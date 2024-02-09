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
      "multi-level kernel predicting mlp with:\n"
      "%d hidden layers\n"
      "%d network width\n"
      "%s kernel weight activation\n"
      "%s alpha blending activation\n"
      "%s mlp activation\n"
      "%s loss",
      N_HIDDEN_LAYERS,
      WIDTH,
       APPLY_ACTIVATION == APPLY_PLAIN ? "plain" :
      (APPLY_ACTIVATION == APPLY_SOFTMAX ? "softmax" : "sigmoid"),
       ALPHA_ACTIVATION == ALPHA_CONST ? "const" :
      (ALPHA_ACTIVATION == ALPHA_PLAIN ? "plain" : "sigmoid"),
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
#ifdef DEBUG_DERIV
  module->connector[4].roi.full_wd = (N_HIDDEN_LAYERS+1)*WIDTH * WIDTH - WIDTH*16 + 1; // enough for all the partial derivatives
  module->connector[4].roi.full_ht = 1;
#else
  module->connector[4].roi.full_wd = 500; // output graph
  module->connector[4].roi.full_ht = 200;
#endif
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
#ifdef DEBUG_DERIV
  graph->frame_cnt = (N_HIDDEN_LAYERS+1)*WIDTH * WIDTH - WIDTH*16 + 1;
#endif

  // loss:
  int wd = roi_input.wd;
  int ht = roi_input.ht;
  dt_roi_t roi = (dt_roi_t){ .wd = (wd*ht+31)/32, .ht = 1 };
  const int id_loss = dt_node_add(
      graph, module, "kpn-t", "loss",
      wd, ht, 1, 0, 0, 5,
      "O",    "read",  "rgba", "*",   dt_no_roi,
      "R",    "read",  "rgba", "*",   dt_no_roi,
      "dEdO", "write", "rgba", "f16", &roi_input,
      "Eo",   "write", "ssbo", "f32", &roi,
      "dbg",  "write", "rgba", "f16", &roi_dbg);

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

  // mip map the input:
  dt_roi_t roi_M[7] = { roi_input };
  for(int i=1;i<7;i++)
  {
    roi_M[i].wd = (roi_M[i-1].wd + 1)/2;
    roi_M[i].ht = (roi_M[i-1].ht + 1)/2;
  }
  const int id_mip = dt_node_add(
      graph, module, "kpn-t", "mip", // XXX channels?
      (roi_input.wd + 7)/8 * DT_LOCAL_SIZE_X, (roi_input.ht + 7)/8 * DT_LOCAL_SIZE_Y, 1, 0, 0, 4,
      "M0", "read",  "rgba", "*",  dt_no_roi,
      "M1", "write", "rgba", "f16", roi_M+1,
      "M2", "write", "rgba", "f16", roi_M+2,
      "M3", "write", "rgba", "f16", roi_M+3);
  dt_connector_copy(graph, module, 0, id_mip, 0); // input image

#ifdef PRE_MLP_DIFF
  const int id_mip1 = dt_node_add(
      graph, module, "kpn-t", "mip",
      (roi_M[3].wd + 7)/8 * DT_LOCAL_SIZE_X, (roi_M[3].ht + 7)/8 * DT_LOCAL_SIZE_Y, 1, 0, 0, 4,
      "M3", "read",  "rgba", "*",  dt_no_roi,
      "M4", "write", "rgba", "f16", roi_M+4,
      "M5", "write", "rgba", "f16", roi_M+5,
      "M6", "write", "rgba", "f16", roi_M+6);
  CONN(dt_node_connect_named(graph, id_mip, "M3", id_mip1, "M3"));
  int id_diff[4]; // XXX increase this number only if the training input support is large enough
  for(int i=0;i<4;i++)
  { // insert 4 diff kernels to compute detail coefficients
    id_diff[i] = dt_node_add(
        graph, module, "kpn-t", "diff",
        roi_M[i].wd, roi_M[i].ht, 1, 0, 0, 3,
        "Mf", "read",  "rgba", "*", dt_no_roi,
        "Mc", "read",  "rgba", "*", dt_no_roi,
        "D",  "write", "rgba", "f16", roi_M+i);
    char Mf[3] = "Mx", Mc[3] = "Mx";
    Mf[1] = '0'+i; Mc[1] = '0'+i+1;
    if(i == 0) dt_connector_copy(graph, module, 0, id_diff[i], 0);
    else CONN(dt_node_connect_named(graph, id_mip, Mf, id_diff[i], "Mf"));
    CONN(dt_node_connect_named(graph, i < 3 ? id_mip : id_mip1, Mc, id_diff[i], "Mc"));
  }
#endif

  dt_roi_t roi_weights = (dt_roi_t){ .wd = WIDTH, .ht = (N_HIDDEN_LAYERS + 1) * WIDTH}; // weights
  int pc_adam[] = { roi_weights.wd*roi_weights.ht };
#ifndef DEBUG_DERIV
  const int id_sum = dt_node_add( // sum dw for adam optimiser
      graph, module, "kpn-t", "sum", (roi_weights.wd*roi_weights.ht+31)/32 * DT_LOCAL_SIZE_X, 1 * DT_LOCAL_SIZE_Y, 1,
      sizeof(pc_adam), pc_adam, 5,
      "dw0", "read",  "ssbo", "f32", dt_no_roi,
      "dw1", "read",  "ssbo", "f32", dt_no_roi,
      "dw2", "read",  "ssbo", "f32", dt_no_roi,
      "dw3", "read",  "ssbo", "f32", dt_no_roi,
      "dw",  "write", "ssbo", "f32", &roi_weights);
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
    else            CONN(dt_node_connect_named(graph, id_sum,   "dw",  id_next, "dwi"));
    cnt = (cnt + 31)/32;
    id_grad = id_next;
    it++;
  }}
#endif
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
  CONN(dt_node_feedback_named(graph, id_adam, "w",  id_adam, "wi"));
  CONN(dt_node_feedback_named(graph, id_adam, "m1", id_adam, "m1i"));
  CONN(dt_node_feedback_named(graph, id_adam, "m2", id_adam, "m2i"));
#ifdef DEBUG_DERIV
  CONN(dt_node_feedback_named(graph, id_adam, "m1", id_adam, "dw"));
  CONN(dt_node_feedback_named(graph, id_adam, "m1", id_adam, "dwg"));
#else
  CONN(dt_node_feedback_named(graph, id_sum,   "dw",  id_adam, "dw"));
  CONN(dt_node_feedback_named(graph, id_grad,  "dwo", id_adam, "dwg"));
#endif
  if(dt_connected(module->connector+7)) dt_connector_copy(graph, module, 7, id_adam, 7);
  else CONN(dt_node_feedback_named(graph, id_adam, "w", id_adam, "wcp")); // connect dummy

  // multi-level mlp kernel prediction pipeline
  int id_up_prev = -1, id_dup_prev = -1;
  for(int i=0;i<4;i++)
  { // from fine M0 to coarse M3
    const uint32_t batch_size = 128 * ((127 + roi_M[i].wd*roi_M[i].ht)/128);            // #px rounded up to 128 (=N_ITERS*16)
    dt_roi_t roi_K = (dt_roi_t){ .wd = batch_size, .ht = 16 };                          // output: 15-tap kernel (+1 alpha) per pixel
    // XXX TODO: check size: seems ht = N_HIDDEN_LAYERS * WIDTH should be enough!
    dt_roi_t roi_A = (dt_roi_t){ .wd = batch_size, .ht = (N_HIDDEN_LAYERS + 1) *WIDTH}; // intermediates for backprop
    const int pc_fwd[] = { 32, batch_size, 16, i };
    // uint32_t in_width;         = WIDTH, multiple of 16
    // uint32_t output_stride;    = 16, last layer only
    const int blocks[] = { pc_fwd[1]/128, 1u, 1u }; // num pixels / (16 * N_ITERS)
    
    // we are passing the number of threads assuming DT_LOCAL_SIZE_X and DT_LOCAL_SIZE_Y
    const int id_fwd = dt_node_add( // infer kernel and store intermediate activations too
        graph, module, "kpn-t", "fwd", blocks[0] * DT_LOCAL_SIZE_X, blocks[1] * DT_LOCAL_SIZE_Y, 1, sizeof(pc_fwd), pc_fwd, 5,
        "M", "read",  "rgba", "*",   dt_no_roi, // input image mipmap level
        "w", "read",  "ssbo", "f16", dt_no_roi, // MLP weights
        "K", "write", "ssbo", "f16", &roi_K,    // network output, 15 kernel weights + 1 alpha per px 
        "A", "write", "ssbo", "f16", &roi_A,    // intermediate layer activations, 32 per layer per px
        "nab","read", "ssbo", "f32", dt_no_roi);// noise profile from cnngenin
    graph->node[id_fwd].connector[3].flags = s_conn_clear; // make sure padded entries are 0
    dt_connector_copy(graph, module, 8, id_fwd, 4); // wire noise profile ssbo

    const int id_apply = dt_node_add( // apply convolution
        graph, module, "kpn-t", "apply", roi_M[i].wd, roi_M[i].ht, 1, 0, 0, 3,
        "M", "read",  "rgba", "*",   dt_no_roi,
        "K", "read",  "ssbo", "f16", dt_no_roi,
        "I", "write", "rgba", "f16", &roi_M[i]);  // output convolved image

    int id_up = -1, id_dup = -1;
#ifdef PRE_MLP_DIFF
    id_up = dt_node_add( // upsample coarse and blend the result to fine with the fine alpha
        graph, module, "kpn-t", "up", roi_M[i].wd, roi_M[i].ht, 1, 0, 0, 3,
        "I",  "read",  "rgba", "*",   dt_no_roi,   // the convolved image on this (fine) scale (with alpha channel)
        "Oc", "read",  "rgba", "*",   dt_no_roi,   // output of coarse level: connect to id_apply->out on coarsest i+1==3, or else to id_up i+1
        "O",  "write", "rgba", "f16", &roi_M[i]);
    CONN(dt_node_connect_named(graph, id_apply, "I", id_up, "I"));
    if(i > 0) CONN(dt_node_connect_named(graph, id_up, "O", id_up_prev, "Oc")); // plug our (coarser) into "Oc" input on finer level
    else      CONN(dt_node_connect_named(graph, id_up, "O", id_loss,    "O"));  // plug finest level output into loss
    if(i == 3) // i == 3, the coarsest layer has no regular upsampled coarse
      CONN(dt_node_connect_named(graph, id_mip1, "M4", id_up, "Oc"));
#else
    if(i < 3)
    {
      id_up = dt_node_add( // upsample coarse and blend the result to fine with the fine alpha
          graph, module, "kpn-t", "up", roi_M[i].wd, roi_M[i].ht, 1, 0, 0, 3,
          "I",  "read",  "rgba", "*",   dt_no_roi,   // the convolved image on this (fine) scale (with alpha channel)
          "Oc", "read",  "rgba", "*",   dt_no_roi,   // output of coarse level: connect to id_apply->out on coarsest i+1==3, or else to id_up i+1
          "O",  "write", "rgba", "f16", &roi_M[i]);
      CONN(dt_node_connect_named(graph, id_apply, "I", id_up, "I"));
      if(i > 0) CONN(dt_node_connect_named(graph, id_up, "O", id_up_prev, "Oc")); // plug our (coarser) into "Oc" input on finer level
      else      CONN(dt_node_connect_named(graph, id_up, "O", id_loss,    "O"));  // plug finest level output into loss
    }
    else
    { // i == 3, the coarsest layer (does not upscale yet another even coarser layer)
      CONN(dt_node_connect_named(graph, id_apply, "I", id_up_prev, "Oc"));
    }
#endif
    id_up_prev = id_up;

#ifndef DEBUG_DERIV
    const int push_dapply[] = { i };
    const int id_dapply = dt_node_add( // route loss derivative through convolution backwards
        graph, module, "kpn-t", "dapply", roi_M[i].wd, roi_M[i].ht, 16, sizeof(push_dapply), push_dapply, 6,
        "M",    "read",  "rgba", "*",   dt_no_roi, // connect plain input image/mipmap
        "I",    "read",  "rgba", "*",   dt_no_roi, // connect convolved image (for alpha derivative)
        "dEdI", "read",  "rgba", "f16", dt_no_roi, // connect dup->dEdI from above
        "K",    "read",  "ssbo", "f16", dt_no_roi, // kernel from fwd
        "dEdK", "write", "ssbo", "f16", &roi_K,
        "dbg",  "write", "rgba", "f16", &roi_dbg);
    CONN(dt_node_connect_named(graph, id_apply, "I", id_dapply, "I"));
    CONN(dt_node_connect_named(graph, id_fwd,   "K", id_dapply, "K"));

#ifdef PRE_MLP_DIFF
    id_dup = dt_node_add(
        graph, module, "kpn-t", "dup", roi_M[i+1].wd, roi_M[i+1].ht, 1, 0, 0, 5,
        "dEdO",  "read",  "rgba", "*",   dt_no_roi,   // loss
        "I",     "read",  "rgba", "*",   dt_no_roi,   // the convolved image on this (fine) scale
        "Oc",    "read",  "rgba", "*",   dt_no_roi,   // output of coarse level: connect to id_apply->out on coarsest i+1==3, or else to id_up i+1
        "dEdI",  "write", "rgba", "f16", &roi_M[i],
        "dEdOc", "write", "rgba", "f16", &roi_M[i+1]);
    if(i == 0) CONN(dt_node_connect_named(graph, id_loss,     "dEdO",  id_dup, "dEdO"));
    else       CONN(dt_node_connect_named(graph, id_dup_prev, "dEdOc", id_dup, "dEdO"));
    if(i != 0) CONN(dt_node_connect_named(graph, id_up,       "O",     id_dup_prev, "Oc"));
    CONN(dt_node_connect_named(graph, id_dup,   "dEdI", id_dapply, "dEdI"));
    CONN(dt_node_connect_named(graph, id_apply, "I",    id_dup,    "I"));
    if(i == 3) CONN(dt_node_connect_named(graph, id_mip1, "M4", id_dup, "Oc"));
#else
    if(i < 3)
    {
      id_dup = dt_node_add(
          graph, module, "kpn-t", "dup", roi_M[i+1].wd, roi_M[i+1].ht, 1, 0, 0, 5,
          "dEdO",  "read",  "rgba", "*",   dt_no_roi,   // loss
          "I",     "read",  "rgba", "*",   dt_no_roi,   // the convolved image on this (fine) scale
          "Oc",    "read",  "rgba", "*",   dt_no_roi,   // output of coarse level: connect to id_apply->out on coarsest i+1==3, or else to id_up i+1
          "dEdI",  "write", "rgba", "f16", &roi_M[i],
          "dEdOc", "write", "rgba", "f16", &roi_M[i+1]);
      if(i == 0) CONN(dt_node_connect_named(graph, id_loss,     "dEdO",  id_dup, "dEdO"));
      else       CONN(dt_node_connect_named(graph, id_dup_prev, "dEdOc", id_dup, "dEdO"));
      if(i != 0) CONN(dt_node_connect_named(graph, id_up,       "O",     id_dup_prev, "Oc"));
      CONN(dt_node_connect_named(graph, id_dup,   "dEdI", id_dapply, "dEdI"));
      CONN(dt_node_connect_named(graph, id_apply, "I",    id_dup,    "I"));
    }
    else
    {
      CONN(dt_node_connect_named(graph, id_dup_prev, "dEdOc", id_dapply,   "dEdI"));
      CONN(dt_node_connect_named(graph, id_apply,    "I",     id_dup_prev, "Oc"));
    }
#endif
    id_dup_prev = id_dup;

    // out_width = rows = 16;  batch_size = columns; output_stride = 16
    int pc_bck[] = { 16, pc_fwd[1], 16 };
    const int id_bck = dt_node_add( // backpropagation: compute dE/dA
        graph, module, "kpn-t", "bck", blocks[0] * DT_LOCAL_SIZE_X, blocks[1] * DT_LOCAL_SIZE_Y, 1, sizeof(pc_bck), pc_bck, 4,
        "w",    "read",  "ssbo", "f16", dt_no_roi,  // weights
        "dEdA", "write", "ssbo", "f16", &roi_A,     // write gradients (intermediate + last layer) for activations A
        "A",    "read",  "ssbo", "f16", dt_no_roi,  // intermediate layer activations, written as fwd -> A
        "dEdK", "read",  "ssbo", "f16", dt_no_roi); // input from loss -> dapply
    graph->node[id_bck].connector[1].flags = s_conn_clear; // make sure padded entries are 0

    int pc_mulw[] = { batch_size, i };
    const int id_dw = dt_node_add( // backpropagation: compute dE/dw
        graph, module, "kpn-t", "mulw", WIDTH * DT_LOCAL_SIZE_X, WIDTH * DT_LOCAL_SIZE_Y, 1, sizeof(pc_mulw), pc_mulw, 6,
        "M",    "read",  "rgba", "*",   dt_no_roi,     // input image
        "dEdK", "read",  "ssbo", "f16", dt_no_roi,     // last layer dE/dK from dapply
        "A",    "read",  "ssbo", "f16", dt_no_roi,     // intermediate fwd activations
        "dEdA", "read",  "ssbo", "f16", dt_no_roi,     // intermediate bck activations
        "dEdw", "write", "ssbo", "f32", &roi_weights,  // weight gradients dE/dw
        "nab",  "read",  "ssbo", "f32", dt_no_roi);    // noise profile from cnngenin

    dt_connector_copy(graph, module, 8, id_dw, 5); // wire noise profile ssbo
    CONN(dt_node_connect(graph, id_dw, 4, id_sum, i));
    CONN(dt_node_connect_named(graph, id_adam,   "w",    id_bck,    "w"));
    CONN(dt_node_connect_named(graph, id_fwd,    "A",    id_bck,    "A"));
    CONN(dt_node_connect_named(graph, id_dapply, "dEdK", id_bck,    "dEdK"));
    CONN(dt_node_connect_named(graph, id_fwd,    "A",    id_dw,     "A"));
    CONN(dt_node_connect_named(graph, id_bck,    "dEdA", id_dw,     "dEdA"));
    CONN(dt_node_connect_named(graph, id_dapply, "dEdK", id_dw,     "dEdK"));
#endif

    CONN(dt_node_connect_named(graph, id_adam,   "w",    id_fwd,    "w"));
    CONN(dt_node_connect_named(graph, id_fwd,    "K",    id_apply,  "K"));

    if(i == 0)
    {
      dt_roi_t roi_vis = { .wd = (N_HIDDEN_LAYERS+2)*WIDTH, .ht = 4*WIDTH };
      const int id_vis = dt_node_add(
          graph, module, "kpn-t", "vis", roi_vis.wd, roi_vis.ht, 1, 0, 0, 4,
          "w",   "read",  "ssbo", "f16", dt_no_roi,
          "dw",  "read",  "ssbo", "f32", dt_no_roi,
          "E",   "read",  "ssbo", "f32", dt_no_roi, // from downsampling/reduction after loss
          "out", "write", "rgba", "f16", &roi_vis);
      CONN(dt_node_connect_named(graph, id_adam, "w",  id_vis, "w"));
#ifdef DEBUG_DERIV
      CONN(dt_node_connect_named(graph, id_in,   "Eo", id_vis, "dw"));
#else
      CONN(dt_node_connect_named(graph, id_sum,  "dw", id_vis, "dw"));
#endif
      CONN(dt_node_connect_named(graph, id_in,   "Eo", id_vis, "E"));
      dt_connector_copy(graph, module, 5, id_vis,  3); // debug display weights out
    }

#ifdef PRE_MLP_DIFF
    if(i == 0) dt_connector_copy(graph, module, 2, id_up, 2);
    CONN(dt_node_connect_named(graph, id_diff[i], "D", id_fwd,    "M"));
    CONN(dt_node_connect_named(graph, id_diff[i], "D", id_apply,  "M"));
#ifndef DEBUG_DERIV
    CONN(dt_node_connect_named(graph, id_diff[i], "D", id_dapply, "M"));
    CONN(dt_node_connect_named(graph, id_diff[i], "D", id_dw,     "M"));
#endif
#else
    const char *mipstr = i==1 ? "M1" : (i==2 ? "M2" : "M3"); // gcc does not understand this codepath (const folding in dt_token) but whatever.
    if(i==0)
    {
      dt_connector_copy(graph, module, 0, id_fwd,    0); // input image
      dt_connector_copy(graph, module, 2, id_up,     2);
      dt_connector_copy(graph, module, 0, id_apply,  0);
#ifndef DEBUG_DERIV
      dt_connector_copy(graph, module, 0, id_dapply, 0);
      dt_connector_copy(graph, module, 0, id_dw,     0);
#endif
    }
    else
    {
      CONN(dt_node_connect_named(graph, id_mip, mipstr, id_fwd,    "M"));
      CONN(dt_node_connect_named(graph, id_mip, mipstr, id_apply,  "M"));
#ifndef DEBUG_DERIV
      CONN(dt_node_connect_named(graph, id_mip, mipstr, id_dapply, "M"));
      CONN(dt_node_connect_named(graph, id_mip, mipstr, id_dw,     "M"));
#endif
    }
#endif
  }
  dt_connector_copy(graph, module, 3, id_adam, 4); // output weights
  // dt_connector_copy(graph, module, 6, id_loss, 4); // DEBUG: output per pixel loss
  dt_connector_copy(graph, module, 6, id_mip, 3); // DEBUG: output per pixel loss
  dt_connector_copy(graph, module, 1, id_loss, 1); // reference image
  dt_connector_copy(graph, module, 4, id_map,  1); // dspy out, loss graph
}
