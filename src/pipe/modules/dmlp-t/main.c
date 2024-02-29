#include "modules/api.h"
#include "metadata.h"
#include "config.h"

int init(dt_module_t *mod)
{
  dt_image_metadata_text_t *meta = malloc(sizeof(*meta));
  char *text = malloc(sizeof(char)*2048);
  *meta = (dt_image_metadata_text_t) {
    .type = s_image_metadata_text,
    .text = text,
  };
  mod->data = meta;
  snprintf(text, 2048,
      "lut contains weights for\n"
      "combined demosaicing and denoising using an mlp\n"
      "%d hidden layers\n"
      "%d network width\n"
      "%s mlp activation\n"
      "%s loss",
      N_HIDDEN_LAYERS,
      WIDTH,
       MLP_ACTIVATION == MLP_ACTIVATION_NONE ? "none" :
      (MLP_ACTIVATION == MLP_ACTIVATION_RELU ? "relu" : "leaky relu"),
       LOSS == LOSS_L2 ? "L2" :
      (LOSS == LOSS_SMAPE ? "SMAPE" : "cosh"));
  return 0;
}

void cleanup(dt_module_t *mod)
{
  if(!mod->data) return;
  // probably not necessary now because a module remove/cleanup will trigger a complete graph rebuild:
  mod->img_param.meta = dt_metadata_remove(mod->img_param.meta, mod->data);
  dt_image_metadata_text_t *meta = mod->data;
  free((char *)meta->text);
  free(mod->data);
  mod->data = 0;
}

void
modify_roi_out(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  module->img_param.meta = dt_metadata_append(module->img_param.meta, module->data);
  module->connector[3].roi = (dt_roi_t){ .full_wd = WIDTH, .full_ht = (N_HIDDEN_LAYERS+1)*WIDTH};  // weights
  module->connector[2].roi = module->connector[0].roi; // output image
  module->connector[4].roi.full_wd = 500; // output graph
  module->connector[4].roi.full_ht = 200;
  module->connector[5].roi = (dt_roi_t){ .full_wd = (N_HIDDEN_LAYERS+2)*WIDTH, .full_ht = 4*WIDTH };
  module->connector[6].roi = (dt_roi_t){ .full_wd = 4*module->connector[0].roi.full_wd, .full_ht = 4*module->connector[0].roi.full_ht };

  // init roi for outputs just in case there is no downstream chain to do it later:
  module->connector[3].roi.wd = module->connector[3].roi.full_wd;
  module->connector[3].roi.ht = module->connector[3].roi.full_ht;
  module->connector[4].roi.wd = module->connector[4].roi.full_wd;
  module->connector[4].roi.ht = module->connector[4].roi.full_ht;
  module->connector[5].roi.wd = module->connector[5].roi.full_wd;
  module->connector[5].roi.ht = module->connector[5].roi.full_ht;
  module->connector[6].roi.wd = module->connector[6].roi.full_wd;
  module->connector[6].roi.ht = module->connector[6].roi.full_ht;
}

void
modify_roi_in(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  for(int i=0;i<2;i++)
  { // request in full
    module->connector[i].roi.wd = module->connector[i].roi.full_wd;
    module->connector[i].roi.ht = module->connector[i].roi.full_ht;
    module->connector[i].roi.scale = 1;
  }
  // sorry, we insist:
  module->connector[2].roi = module->connector[0].roi;
}

void
create_nodes(dt_graph_t *graph, dt_module_t *module)
{
  dt_roi_t roi_input   = module->connector[0].roi;
  dt_roi_t roi_dbg     = module->connector[6].roi;
  // ^ these wd/ht are arbitrary, just multiplied for allocation. for real we'd want
  // input.col = output.col = aux.col = batch_size
  // but we don't have input (assemble it on demand from image)

  const uint32_t batch_size = 128 * ((127 + (roi_input.wd*roi_input.ht+3)/4)/128);      // #2x2 bayer blocks rounded up to 128 (=N_ITERS*16)
  dt_roi_t roi_K = (dt_roi_t){ .wd = batch_size,  .ht = 16 };                           // output: 3 channels for 4 pixels, and some rest to pad to 16
  dt_roi_t roi_A = (dt_roi_t){ .wd = batch_size,  .ht = (N_HIDDEN_LAYERS + 1) *WIDTH};  // intermediates for backprop
  dt_roi_t roi_weights = (dt_roi_t){ .wd = WIDTH, .ht = (N_HIDDEN_LAYERS + 1) *WIDTH};  // weights
  const int id_ass = dt_node_add( // assemble output from network into final image
      graph, module, "dmlp-t", "ass", roi_input.wd, roi_input.ht, 1, 0, 0, 2,
      "K", "read",  "ssbo", "f16", dt_no_roi,
      "O", "write", "rgba", "f16", &roi_input);
  const int id_dass = dt_node_add( // propagate pixel loss back to network order
      graph, module, "dmlp-t", "dass", roi_input.wd, roi_input.ht, 1, 0, 0, 2,
      "dEdO", "read",  "rgba", "f16", dt_no_roi, // from loss
      "dEdK", "write", "ssbo", "f16", &roi_K);   // to backpropagation, this is directly dE/dK
  int pc_mulw[] = { batch_size, 0 };
  const int id_dw = dt_node_add( // backpropagation: compute dE/dw
      graph, module, "dmlp-t", "mulw", WIDTH * DT_LOCAL_SIZE_X, WIDTH * DT_LOCAL_SIZE_Y, 1, sizeof(pc_mulw), pc_mulw, 6,
      "M",    "read",  "rgba", "*",   dt_no_roi,     // input image
      "dEdK", "read",  "ssbo", "f16", dt_no_roi,     // last layer dE/dK from dass
      "A",    "read",  "ssbo", "f16", dt_no_roi,     // intermediate fwd activations
      "dEdA", "read",  "ssbo", "f16", dt_no_roi,     // intermediate bck activations
      "dEdw", "write", "ssbo", "f32", &roi_weights,  // weight gradients dE/dw
      "nab",  "read",  "ssbo", "f32", dt_no_roi);    // noise profile from cnngenin

  int wd = roi_input.wd;
  int ht = roi_input.ht;
  dt_roi_t roi = (dt_roi_t){ .wd = (wd*ht+31)/32, .ht = 1 };
  const int id_loss = dt_node_add( // loss:
      graph, module, "kpn-t", "loss",
      wd, ht, 1, 0, 0, 5,
      "O",    "read",  "rgba", "*",   dt_no_roi,
      "R",    "read",  "rgba", "*",   dt_no_roi,
      "dEdO", "write", "rgba", "f16", &roi_input,
      "Eo",   "write", "ssbo", "f32", &roi,
      "dbg",  "write", "rgba", "f16", &roi_dbg);
  CONN(dt_node_connect_named(graph, id_ass,  "O",    id_loss, "O"));
  CONN(dt_node_connect_named(graph, id_loss, "dEdO", id_dass, "dEdO"));

  wd = roi.wd;
  int id_in = id_loss;
  do
  {
    int cwd = (wd + 31)/32;
    dt_roi_t croi = (dt_roi_t){ .wd = cwd, .ht = 1 };
    int pc[] = { wd, module->connector[4].roi.wd };
    const int id_down = dt_node_add(
        graph, module, "kpn-t", "down",
        (cwd+31)/32 * DT_LOCAL_SIZE_X, 1, 1, sizeof(pc), pc, 2,
        "Ei", "read",  "ssbo", "f32", dt_no_roi,
        "Eo", "write", "ssbo", "f32", &croi);
    CONN(dt_node_connect_named(graph, id_in, "Eo", id_down, "Ei"));
    id_in = id_down;
    wd = cwd;
  }
  while(wd > 1);
  // last iteration maps to temporal buffer
  graph->node[id_in].connector[1].roi.wd = module->connector[4].roi.wd; // frames now
  int pcm[] = { module->connector[4].roi.wd };
  const int id_map = dt_node_add(
      graph, module, "kpn-t", "map",
      module->connector[4].roi.wd, module->connector[4].roi.ht, 1, sizeof(pcm), pcm, 2,
      "Ei",   "read",  "ssbo", "f32", dt_no_roi,
      "dspy", "write", "rgba", "f16", &module->connector[4].roi);
  CONN(dt_node_connect_named(graph, id_in, "Eo", id_map, "Ei"));
  graph->node[id_in].connector[1].flags = s_conn_protected; // protect memory

  int pc_adam[] = { roi_weights.wd*roi_weights.ht };
  int id_grad = -1;
  {
  int cnt = (roi_weights.wd * roi_weights.ht + 31) / 32;
  int it = 0;
  while(cnt > 1)
  {
    int pc_grad[] = { cnt, it };
    int id_next = dt_node_add(
        graph, module, "kpn-t", "norm", cnt * DT_LOCAL_SIZE_X, 1 * DT_LOCAL_SIZE_Y, 1,
        sizeof(pc_grad), pc_grad, 2,
        "dwi", "read",  "ssbo", "f32", dt_no_roi,
        "dwo", "write", "ssbo", "f32", &roi_weights);
    if(id_grad > 0) CONN(dt_node_connect_named(graph, id_grad, "dwo",  id_next, "dwi"));
    else            CONN(dt_node_connect_named(graph, id_dw,   "dEdw", id_next, "dwi"));
    cnt = (cnt + 31)/32;
    id_grad = id_next;
    it++;
  }}
  const int id_adam = dt_node_add( // adam optimiser
      graph, module, "kpn-t", "adam", (roi_weights.wd*roi_weights.ht+31)/32 * DT_LOCAL_SIZE_X, 1 * DT_LOCAL_SIZE_Y, 1,
      sizeof(pc_adam), pc_adam, 9,
      "wi",  "read",  "ssbo", "f16", dt_no_roi,
      "dw",  "read",  "ssbo", "f32", dt_no_roi,
      "m1i", "read",  "ssbo", "f32", dt_no_roi,
      "m2i", "read",  "ssbo", "f32", dt_no_roi,
      "w",   "write", "ssbo", "f16", &roi_weights,
      "m1",  "write", "ssbo", "f32", &roi_weights,
      "m2",  "write", "ssbo", "f32", &roi_weights,
      "wcp", "read",  "ssbo", "f16", dt_no_roi,  // weights from checkpoint
      "dwg", "read",  "ssbo", "f32", dt_no_roi); // gradient norm
  CONN(dt_node_feedback_named(graph, id_adam, "w",   id_adam, "wi"));
  CONN(dt_node_feedback_named(graph, id_adam, "m1",  id_adam, "m1i"));
  CONN(dt_node_feedback_named(graph, id_adam, "m2",  id_adam, "m2i"));
  CONN(dt_node_feedback_named(graph, id_grad, "dwo", id_adam, "dwg"));
  if(dt_connected(module->connector+7)) dt_connector_copy(graph, module, 7, id_adam, 7);
  else CONN(dt_node_feedback_named(graph, id_adam, "w", id_adam, "wcp")); // connect dummy

  // now the actual pipeline
  // fwd eval with our new input and then output a 2x2 block directly
  
  const int pc_fwd[] = { WIDTH, batch_size, 16, 0 };
  // uint32_t in_width;         = WIDTH, multiple of 16
  // uint32_t output_stride;    = 16, last layer only
  const int blocks[] = { pc_fwd[1]/128, 1u, 1u }; // num bayer blocks / (16 * N_ITERS)
    
  // we are passing the number of threads assuming DT_LOCAL_SIZE_X and DT_LOCAL_SIZE_Y
  const int id_fwd = dt_node_add( // infer kernel and store intermediate activations too
      graph, module, "dmlp-t", "fwd", blocks[0] * DT_LOCAL_SIZE_X, blocks[1] * DT_LOCAL_SIZE_Y, 1, sizeof(pc_fwd), pc_fwd, 5,
      "M", "read",  "rgba", "*",   dt_no_roi, // input image, bayer mosaiced
      "w", "read",  "ssbo", "f16", dt_no_roi, // MLP weights
      "K", "write", "ssbo", "f16", &roi_K,    // network output, direct prediction of colour per px block
      "A", "write", "ssbo", "f16", &roi_A,    // intermediate layer activations, WIDTH per layer per px block
      "nab","read", "ssbo", "f32", dt_no_roi);// noise profile from cnngenin
  graph->node[id_fwd].connector[3].flags = s_conn_clear; // make sure padded entries are 0
  dt_connector_copy(graph, module, 8, id_fwd, 4); // wire noise profile ssbo

  // out_width = rows = 16;  batch_size = columns; output_stride = 16
  int pc_bck[] = { 16, pc_fwd[1], 16 };
  const int id_bck = dt_node_add( // backpropagation: compute dE/dA
      graph, module, "dmlp-t", "bck", blocks[0] * DT_LOCAL_SIZE_X, blocks[1] * DT_LOCAL_SIZE_Y, 1, sizeof(pc_bck), pc_bck, 4,
      "w",    "read",  "ssbo", "f16", dt_no_roi,  // weights
      "dEdA", "write", "ssbo", "f16", &roi_A,     // write gradients (intermediate + last layer) for activations A
      "A",    "read",  "ssbo", "f16", dt_no_roi,  // intermediate layer activations, written as fwd -> A
      "dEdK", "read",  "ssbo", "f16", dt_no_roi); // input from loss
  graph->node[id_bck].connector[1].flags = s_conn_clear; // make sure padded entries are 0

  dt_connector_copy(graph, module, 8, id_dw, 5); // wire noise profile ssbo
  CONN(dt_node_feedback_named(graph, id_dw,    "dEdw", id_adam,   "dw"));
  CONN(dt_node_connect_named(graph, id_adam,   "w",    id_bck,    "w"));
  CONN(dt_node_connect_named(graph, id_fwd,    "A",    id_bck,    "A"));
  CONN(dt_node_connect_named(graph, id_dass,   "dEdK", id_bck,    "dEdK"));
  CONN(dt_node_connect_named(graph, id_fwd,    "A",    id_dw,     "A"));
  CONN(dt_node_connect_named(graph, id_bck,    "dEdA", id_dw,     "dEdA"));
  CONN(dt_node_connect_named(graph, id_dass,   "dEdK", id_dw,     "dEdK"));

  CONN(dt_node_connect_named(graph, id_adam,   "w",    id_fwd,    "w"));
  CONN(dt_node_connect_named(graph, id_fwd,    "K",    id_ass,    "K"));

  dt_roi_t roi_vis = { .wd = (N_HIDDEN_LAYERS+2)*WIDTH, .ht = 4*WIDTH };
  const int id_vis = dt_node_add(
      graph, module, "kpn-t", "vis", roi_vis.wd, roi_vis.ht, 1, 0, 0, 4,
      "w",   "read",  "ssbo", "f16", dt_no_roi,
      "dw",  "read",  "ssbo", "f32", dt_no_roi,
      "E",   "read",  "ssbo", "f32", dt_no_roi, // from downsampling/reduction after loss
      "out", "write", "rgba", "f16", &roi_vis);
  CONN(dt_node_connect_named(graph, id_adam, "w",    id_vis, "w"));
  CONN(dt_node_connect_named(graph, id_dw,   "dEdw", id_vis, "dw"));
  CONN(dt_node_connect_named(graph, id_in,   "Eo",   id_vis, "E"));
  dt_connector_copy(graph, module, 5, id_vis,  3); // debug display weights out

  dt_connector_copy(graph, module, 0, id_fwd,    0); // input image
  dt_connector_copy(graph, module, 0, id_dw,     0);
  dt_connector_copy(graph, module, 2, id_ass,    1); // output image

  dt_connector_copy(graph, module, 3, id_adam, 4); // output weights
  dt_connector_copy(graph, module, 6, id_loss, 4); // DEBUG: output per pixel loss
  dt_connector_copy(graph, module, 1, id_loss, 1); // reference image
  dt_connector_copy(graph, module, 4, id_map,  1); // dspy out, loss graph
}
