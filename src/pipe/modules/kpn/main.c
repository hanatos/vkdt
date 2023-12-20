#include "modules/api.h"
#include "../kpn-t/config.h"
// TODO: re-define N_ITERS

void
create_nodes(dt_graph_t *graph, dt_module_t *module)
{
  dt_roi_t roi_input = module->connector[0].roi;
  // ^ these wd/ht are arbitrary, just multiplied for allocation. for real we'd want
  // input.col = output.col = aux.col = batch_size
  // but we don't have input (assemble it on demand from image)

  // mip map the input:
  dt_roi_t roi_M[4] = { roi_input };
  for(int i=1;i<4;i++)
  {
    roi_M[i].wd = (roi_M[i-1].wd + 1)/2;
    roi_M[i].ht = (roi_M[i-1].ht + 1)/2;
  }
  const int id_mip = dt_node_add(
      graph, module, "kpn-t", "mip", // XXX channels?
      (roi_input.wd + 7)/8 * DT_LOCAL_SIZE_X, (roi_input.ht + 7)/8 * DT_LOCAL_SIZE_Y, 1, 0, 0, 4,
      "M0", "read",  "rgba", "*",  -1ul,
      "M1", "write", "rgba", "f16", roi_M+1,
      "M2", "write", "rgba", "f16", roi_M+2,
      "M3", "write", "rgba", "f16", roi_M+3);
  dt_connector_copy(graph, module, 0, id_mip, 0); // input image

  // multi-level mlp kernel prediction pipeline
  int id_up_prev = -1;
  for(int i=0;i<4;i++)
  { // from fine M0 to coarse M3
    const uint32_t unit = N_ITERS * 16;
    const uint32_t batch_size = unit * ((unit - 1 + roi_M[i].wd*roi_M[i].ht)/unit); // #px rounded up to 128 (=N_ITERS*16)
    const int pc_inf[] = { 32, batch_size, 16 };
    // uint32_t in_width;         = WIDTH, multiple of 16
    // uint32_t output_stride;    = 16, last layer only
    const int blocks[] = { pc_inf[1]/unit, 1u, 1u }; // num pixels / (16 * N_ITERS)
    
#if 1 // fused kernel without global memory intermediats.
    // XXX TODO: double check the wiring below and maybe put behind ifdef too
    const int id_apply = dt_node_add( // infer kernel and store intermediate activations too
        graph, module, "kpn-t", "infapply", blocks[0] * DT_LOCAL_SIZE_X, blocks[1] * DT_LOCAL_SIZE_Y, 1, sizeof(pc_inf), pc_inf, 3,
        "M", "read",  "rgba", "*",   -1ul,    // input image mipmap level
        "w", "read",  "ssbo", "f16", -1ul,    // MLP weights
        "I", "write", "rgba", "f16", &roi_M[i]);  // output convolved image
    const int id_inf = id_apply;
#else
    dt_roi_t roi_K = (dt_roi_t){ .wd = batch_size, .ht = 16 }; // output: 15-tap kernel (+1 alpha) per pixel
    // we are passing the number of threads assuming DT_LOCAL_SIZE_X and DT_LOCAL_SIZE_Y
    const int id_inf = dt_node_add( // infer kernel and store intermediate activations too
        graph, module, "kpn-t", "inf", blocks[0] * DT_LOCAL_SIZE_X, blocks[1] * DT_LOCAL_SIZE_Y, 1, sizeof(pc_inf), pc_inf, 3,
        "M", "read",  "rgba", "*",   -1ul,    // input image mipmap level
        "w", "read",  "ssbo", "f16", -1ul,    // MLP weights
        "K", "write", "ssbo", "f16", &roi_K); // network output, 15 kernel weights + 1 alpha per px 

    const int id_apply = dt_node_add( // apply convolution
        graph, module, "kpn-t", "apply", roi_M[i].wd, roi_M[i].ht, 1, 0, 0, 3,
        "M", "read",  "rgba", "*",   -1ul,
        "K", "read",  "ssbo", "f16", -1ul,
        "I", "write", "rgba", "f16", &roi_M[i]);  // output convolved image
#endif

    int id_up = -1;
    if(i < 3)
    {
      id_up = dt_node_add( // upsample coarse and blend the result to fine with the fine alpha
          graph, module, "kpn-t", "up", roi_M[i].wd, roi_M[i].ht, 1, 0, 0, 3,
          "I",  "read",  "rgba", "*",   -1ul,   // the convolved image on this (fine) scale (with alpha channel)
          "Oc", "read",  "rgba", "*",   -1ul,   // output of coarse level: connect to id_apply->out on coarsest i+1==3, or else to id_up i+1
          "O",  "write", "rgba", "f16", &roi_M[i]);
      CONN(dt_node_connect_named(graph, id_apply, "I", id_up, "I"));
      if(i > 0) CONN(dt_node_connect_named(graph, id_up, "O", id_up_prev, "Oc")); // plug our (coarser) into "Oc" input on finer level
    }
    else
    { // i == 3, the coarsest layer (does not upscale yet another even coarser layer)
      CONN(dt_node_connect_named(graph, id_apply, "I", id_up_prev, "Oc"));
    }
    id_up_prev = id_up;

    dt_connector_copy(graph, module, 2, id_inf, 1); // wire weights
    CONN(dt_node_connect_named(graph, id_inf, "K", id_apply, "K"));

    const char *mipstr = i==1 ? "M1" : (i==2 ? "M2" : "M3");
    if(i==0)
    {
      dt_connector_copy(graph, module, 0, id_inf,    0); // input image
      dt_connector_copy(graph, module, 1, id_up,     2); // output image
      dt_connector_copy(graph, module, 0, id_apply,  0);
    }
    else
    {
      CONN(dt_node_connect_named(graph, id_mip, mipstr, id_inf,    "M"));
      CONN(dt_node_connect_named(graph, id_mip, mipstr, id_apply,  "M"));
    }
  }
}
