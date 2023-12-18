#include "modules/api.h"
#include "config.h"

void
modify_roi_out(
    dt_graph_t  *graph,
    dt_module_t *module)
{
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
#ifdef DEBUG_DERIV
  graph->frame_cnt = (N_HIDDEN_LAYERS+1)*WIDTH * WIDTH - WIDTH*16 + 1;
#endif

  // loss:
  int wd = roi_input.wd;
  int ht = roi_input.ht;
  dt_roi_t roi = (dt_roi_t){ .wd = (wd*ht+31)/32, .ht = 1 };
  const int id_loss = dt_node_add(
      graph, module, "dnmlp", "loss",
      wd, ht, 1, 0, 0, 5,
      "O",    "read",  "rgba", "*",   -1ul,
      "R",    "read",  "rgba", "*",   -1ul,
      "dEdO", "write", "rgba", "f16", &roi_input,
      "Eo",   "write", "ssbo", "f32", &roi,
      "dbg",  "write", "rgba", "f16", &roi_dbg);

  wd = roi.wd;
  int id_in = id_loss;
  do
  {
    int cwd = (wd + 31)/32;
    dt_roi_t croi = (dt_roi_t){ .wd = cwd, .ht = 1 };
#ifdef DEBUG_DERIV
    int pc[] = { wd, (N_HIDDEN_LAYERS+1)*WIDTH*WIDTH+1 }; // enough for all weights/dw (+1 frame zero)
#else
    int pc[] = { wd, module->connector[4].roi.wd };
#endif
    const int id_down = dt_node_add(
        graph, module, "cnntrain", "down",
        (cwd+31)/32 * DT_LOCAL_SIZE_X, 1, 1, sizeof(pc), pc, 2,
        "Ei", "read",  "ssbo", "f32", -1ul,
        "Eo", "write", "ssbo", "f32", &croi);
    CONN(dt_node_connect_named(graph, id_in, "Eo", id_down, "Ei"));
    id_in = id_down;
    wd = cwd;
  }
  while(wd > 1);
  // last iteration maps to temporal buffer
  graph->node[id_in].connector[1].roi.wd = module->connector[4].roi.wd; // frames now
  // graph->node[id_down].connector[1].flags = s_conn_protected; // protect memory
  graph->node[id_in].connector[1].type = dt_token("source");// ???
  int pcm[] = { module->connector[4].roi.wd };
  const int id_map = dt_node_add(
      graph, module, "cnntrain", "map",
      module->connector[4].roi.wd, module->connector[4].roi.ht, 1, sizeof(pcm), pcm, 2,
      "Ei",   "read",  "ssbo", "f32", -1ul,
      "dspy", "write", "rgba", "f16", &module->connector[4].roi);
  CONN(dt_node_connect_named(graph, id_in, "Eo", id_map, "Ei"));

  // mip map the input:
  dt_roi_t roi_M[4] = { roi_input };
  for(int i=1;i<4;i++)
  {
    roi_M[i].wd = (roi_M[i-1].wd + 1)/2;
    roi_M[i].ht = (roi_M[i-1].ht + 1)/2;
  }
  const int id_mip = dt_node_add(
      graph, module, "dnmlp", "mip", // XXX channels?
      (roi_input.wd + 7)/8 * DT_LOCAL_SIZE_X, (roi_input.ht + 7)/8 * DT_LOCAL_SIZE_Y, 1, 0, 0, 4,
      "M0", "read",  "rgba", "*",  -1ul,
      "M1", "write", "rgba", "f16", roi_M+1,
      "M2", "write", "rgba", "f16", roi_M+2,
      "M3", "write", "rgba", "f16", roi_M+3);
  dt_connector_copy(graph, module, 0, id_mip, 0); // input image

  dt_roi_t roi_weights = (dt_roi_t){ .wd = WIDTH, .ht = (N_HIDDEN_LAYERS + 1) * WIDTH}; // weights
  int pc_adam[] = { roi_weights.wd*roi_weights.ht };
#ifndef DEBUG_DERIV
  const int id_sum = dt_node_add( // sum dw for adam optimiser
      graph, module, "dnmlp", "sum", (roi_weights.wd*roi_weights.ht+31)/32 * DT_LOCAL_SIZE_X, 1 * DT_LOCAL_SIZE_Y, 1,
      sizeof(pc_adam), pc_adam, 5,
      "dw0", "read",  "ssbo", "f32", -1ul,
      "dw1", "read",  "ssbo", "f32", -1ul,
      "dw2", "read",  "ssbo", "f32", -1ul,
      "dw3", "read",  "ssbo", "f32", -1ul,
      "dw",  "write", "ssbo", "f32", &roi_weights);
#endif
  const int id_adam = dt_node_add( // adam optimiser
      graph, module, "dnmlp", "adam", (roi_weights.wd*roi_weights.ht+31)/32 * DT_LOCAL_SIZE_X, 1 * DT_LOCAL_SIZE_Y, 1,
      sizeof(pc_adam), pc_adam, 8,
      "wi",  "read",  "ssbo", "f16", -1ul,
      "dw",  "read",  "ssbo", "f32", -1ul,
      "m1i", "read",  "ssbo", "f32", -1ul,
      "m2i", "read",  "ssbo", "f32", -1ul,
      "w",   "write", "ssbo", "f16", &roi_weights,
      "m1",  "write", "ssbo", "f32", &roi_weights,
      "m2",  "write", "ssbo", "f32", &roi_weights,
      "wcp", "read",  "ssbo", "f16", -1ul); // weights from checkpoint
  CONN(dt_node_feedback_named(graph, id_adam, "w",  id_adam, "wi"));
  CONN(dt_node_feedback_named(graph, id_adam, "m1", id_adam, "m1i"));
  CONN(dt_node_feedback_named(graph, id_adam, "m2", id_adam, "m2i"));
#ifdef DEBUG_DERIV
  CONN(dt_node_feedback_named(graph, id_adam, "m1", id_adam, "dw"));
#else
  CONN(dt_node_feedback_named(graph, id_sum,  "dw", id_adam, "dw"));
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
    const int pc_fwd[] = { 32, batch_size, 16 };
    // uint32_t in_width;         = WIDTH, multiple of 16
    // uint32_t output_stride;    = 16, last layer only
    const int blocks[] = { pc_fwd[1]/128, 1u, 1u }; // num pixels / (16 * N_ITERS)
    
    // we are passing the number of threads assuming DT_LOCAL_SIZE_X and DT_LOCAL_SIZE_Y
    const int id_fwd = dt_node_add( // infer kernel and store intermediate activations too
        graph, module, "dnmlp", "fwd", blocks[0] * DT_LOCAL_SIZE_X, blocks[1] * DT_LOCAL_SIZE_Y, 1, sizeof(pc_fwd), pc_fwd, 4,
        "M", "read",  "rgba", "*",   -1ul,    // input image mipmap level
        "w", "read",  "ssbo", "f16", -1ul,    // MLP weights
        "K", "write", "ssbo", "f16", &roi_K,  // network output, 15 kernel weights + 1 alpha per px 
        "A", "write", "ssbo", "f16", &roi_A); // intermediate layer activations, 32 per layer per px
    graph->node[id_fwd].connector[3].flags = s_conn_clear; // make sure padded entries are 0

    const int id_apply = dt_node_add( // apply convolution
        graph, module, "dnmlp", "apply", roi_M[i].wd, roi_M[i].ht, 1, 0, 0, 3,
        "M", "read",  "rgba", "*",   -1ul,
        "K", "read",  "ssbo", "f16", -1ul,
        "I", "write", "rgba", "f16", &roi_M[i]);  // output convolved image

    int id_up = -1, id_dup = -1;
    if(i < 3)
    {
      id_up = dt_node_add( // upsample coarse and blend the result to fine with the fine alpha
          graph, module, "dnmlp", "up", roi_M[i].wd, roi_M[i].ht, 1, 0, 0, 3,
          "I",  "read",  "rgba", "*",   -1ul,   // the convolved image on this (fine) scale (with alpha channel)
          "Oc", "read",  "rgba", "*",   -1ul,   // output of coarse level: connect to id_apply->out on coarsest i+1==3, or else to id_up i+1
          "O",  "write", "rgba", "f16", &roi_M[i]);
      CONN(dt_node_connect_named(graph, id_apply, "I", id_up, "I"));
      if(i > 0) CONN(dt_node_connect_named(graph, id_up, "O", id_up_prev, "Oc")); // plug our (coarser) into "Oc" input on finer level
      else      CONN(dt_node_connect_named(graph, id_up, "O", id_loss,    "O"));  // plug finest level output into loss
    }
    else
    { // i == 3, the coarsest layer (does not upscale yet another even coarser layer)
      CONN(dt_node_connect_named(graph, id_apply, "I", id_up_prev, "Oc"));
    }
    id_up_prev = id_up;

#ifndef DEBUG_DERIV
    const int push_dapply[] = { i };
    const int id_dapply = dt_node_add( // route loss derivative through convolution backwards
        graph, module, "dnmlp", "dapply", roi_M[i].wd, roi_M[i].ht, 16, sizeof(push_dapply), push_dapply, 6,
        "M",    "read",  "rgba", "*",   -1ul,    // connect plain input image/mipmap
        "I",    "read",  "rgba", "*",   -1ul,    // connect convolved image (for alpha derivative)
        "dEdI", "read",  "rgba", "f16", -1ul,    // connect dup->dEdI from above
        "K",    "read",  "ssbo", "f16", -1ul,    // kernel from fwd
        "dEdK", "write", "ssbo", "f16", &roi_K,
        "dbg",  "write", "rgba", "f16", &roi_dbg);
    CONN(dt_node_connect_named(graph, id_apply, "I", id_dapply, "I"));
    CONN(dt_node_connect_named(graph, id_fwd,   "K", id_dapply, "K"));

    if(i < 3)
    {
      id_dup = dt_node_add(
          graph, module, "dnmlp", "dup", roi_M[i+1].wd, roi_M[i+1].ht, 1, 0, 0, 5,
          "dEdO",  "read",  "rgba", "*",   -1ul,   // loss
          "I",     "read",  "rgba", "*",   -1ul,   // the convolved image on this (fine) scale
          "Oc",    "read",  "rgba", "*",   -1ul,   // output of coarse level: connect to id_apply->out on coarsest i+1==3, or else to id_up i+1
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
    id_dup_prev = id_dup;

    // out_width = rows = 16;  batch_size = columns; output_stride = 16
    int pc_bck[] = { 16, pc_fwd[1], 16 };
    const int id_bck = dt_node_add( // backpropagation: compute dE/dA
        graph, module, "dnmlp", "bck", blocks[0] * DT_LOCAL_SIZE_X, blocks[1] * DT_LOCAL_SIZE_Y, 1, sizeof(pc_bck), pc_bck, 4,
        "w",    "read",  "ssbo", "f16", -1ul,       // weights
        "dEdA", "write", "ssbo", "f16", &roi_A,     // write gradients (intermediate + last layer) for activations A
        "A",    "read",  "ssbo", "f16", -1ul,       // intermediate layer activations, written as fwd -> A
        "dEdK", "read",  "ssbo", "f16", -1ul);      // input from loss -> dapply
    graph->node[id_bck].connector[1].flags = s_conn_clear; // make sure padded entries are 0

    int pc_mulw[] = { batch_size };
    const int id_dw = dt_node_add( // backpropagation: compute dE/dw
        graph, module, "dnmlp", "mulw", WIDTH * DT_LOCAL_SIZE_X, WIDTH * DT_LOCAL_SIZE_Y, 1, sizeof(pc_mulw), pc_mulw, 5,
        "M",    "read",  "rgba", "*",   -1ul,          // input image
        "dEdK", "read",  "ssbo", "f16", -1ul,          // last layer dE/dK from dapply
        "A",    "read",  "ssbo", "f16", -1ul,          // intermediate fwd activations
        "dEdA", "read",  "ssbo", "f16", -1ul,          // intermediate bck activations
        "dEdw", "write", "ssbo", "f32", &roi_weights); // weight gradients dE/dw

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
          graph, module, "dnmlp", "vis", roi_vis.wd, roi_vis.ht, 1, 0, 0, 4,
          "w",   "read",  "ssbo", "f16", -1ul,
          "dw",  "read",  "ssbo", "f32", -1ul,
          "E",   "read",  "ssbo", "f32", -1ul, // from downsampling/reduction after loss
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

    const char *mipstr = i==1 ? "M1" : (i==2 ? "M2" : "M3");
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
  }
  dt_connector_copy(graph, module, 3, id_adam, 4); // output weights
  dt_connector_copy(graph, module, 6, id_loss, 4); // DEBUG: output per pixel loss
  dt_connector_copy(graph, module, 1, id_loss, 1); // reference image
  dt_connector_copy(graph, module, 4, id_map,  1); // dspy out, loss graph
}
