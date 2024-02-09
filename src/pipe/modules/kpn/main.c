#include "modules/api.h"
#include "../kpn-t/config.h"
// #define FUSED_INF // currently broken unfortunately. avoids a roundtrip to global memory and should be better.

void
create_nodes(dt_graph_t *graph, dt_module_t *module)
{
  dt_roi_t roi_input = module->connector[0].roi;
  // ^ these wd/ht are arbitrary, just multiplied for allocation. for real we'd want
  // input.col = output.col = aux.col = batch_size
  // but we don't have input (assemble it on demand from image)

  const int have_coopmat = qvk.coopmat_supported;

  const int Mcnt = 7; // tempting, but apparently doesn't do much for us (need to train for it?)
  // const int Mcnt = 4;
  // mip map the input:
  dt_roi_t roi_M[7] = { roi_input };
  for(int i=1;i<Mcnt;i++)
  {
    roi_M[i].wd = (roi_M[i-1].wd + 1)/2;
    roi_M[i].ht = (roi_M[i-1].ht + 1)/2;
  }
#ifdef NO_ALIAS
  int id_sub [6]; // one subsampling pass per mipmap level
  int id_diff[6];
  for(int i=0;i<6;i++)
  { // insert 4 diff kernels to compute detail coefficients
    id_sub[i] = dt_node_add(
        graph, module, "kpn-t", "sub",
        roi_M[i].wd, roi_M[i].ht, 1, 0, 0, 2,
        "M0", "read",  "rgba", "*", dt_no_roi,
        "M1", "write", "rgba", "f16", roi_M+i+1);
    id_diff[i] = dt_node_add(
        graph, module, "kpn-t", "diff",
        roi_M[i].wd, roi_M[i].ht, 1, 0, 0, 3,
        "Mf", "read",  "rgba", "*", dt_no_roi,
        "Mc", "read",  "rgba", "*", dt_no_roi,
        "D",  "write", "rgba", "f16", roi_M+i);
    if(i == 0)
    {
      dt_connector_copy(graph, module, 0, id_sub [i], 0);
      dt_connector_copy(graph, module, 0, id_diff[i], 0);
    }
    else
    {
      CONN(dt_node_connect_named(graph, id_sub[i-1], "M1", id_sub[i],  "M0"));
      CONN(dt_node_connect_named(graph, id_sub[i-1], "M1", id_diff[i], "Mf"));
    }
    CONN(dt_node_connect_named(graph, id_sub[i],   "M1", id_diff[i], "Mc"));
  }
#else
  const int id_mip0 = dt_node_add(
      graph, module, "kpn-t", "mip", // XXX channels?
      (roi_M[0].wd + 7)/8 * DT_LOCAL_SIZE_X, (roi_M[0].ht + 7)/8 * DT_LOCAL_SIZE_Y, 1, 0, 0, 4,
      "M0", "read",  "rgba", "*",  dt_no_roi,
      "M1", "write", "rgba", "f16", roi_M+1,
      "M2", "write", "rgba", "f16", roi_M+2,
      "M3", "write", "rgba", "f16", roi_M+3);
  dt_connector_copy(graph, module, 0, id_mip0, 0); // input image

#if 1//def PRE_MLP_DIFF // needed for Mcnt = 7 too
  const int id_mip1 = dt_node_add( // second level mip maps:
      graph, module, "kpn-t", "mip",
      (roi_M[3].wd + 7)/8 * DT_LOCAL_SIZE_X, (roi_M[3].ht + 7)/8 * DT_LOCAL_SIZE_Y, 1, 0, 0, 4,
      "M3", "read",  "rgba", "*",  dt_no_roi,
      "M4", "write", "rgba", "f16", roi_M+4,
      "M5", "write", "rgba", "f16", roi_M+5,
      "M6", "write", "rgba", "f16", roi_M+6);
  CONN(dt_node_connect_named(graph, id_mip0, "M3", id_mip1, "M3"));
#ifdef PRE_MLP_DIFF
  int id_diff[6];
  for(int i=0;i<6;i++)
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
    else CONN(dt_node_connect_named(graph, i < 4 ? id_mip0 : id_mip1, Mf, id_diff[i], "Mf"));
    CONN(dt_node_connect_named(graph, i < 3 ? id_mip0 : id_mip1, Mc, id_diff[i], "Mc"));
  }
#endif
#else
  const int id_mip1 = id_mip0;
#endif
#endif

  // multi-level mlp kernel prediction pipeline
  int id_up_prev = -1;
  for(int i=0;i<Mcnt-1;i++)
  { // from fine M0 to coarse M2 or on to M5
    const uint32_t unit = N_ITERS * 16;
    const uint32_t batch_size = unit * ((unit - 1 + roi_M[i].wd*roi_M[i].ht)/unit); // #px rounded up to 128 (=N_ITERS*16)
    const int pc_inf[] = { 32, batch_size, 16, i };
    // uint32_t in_width;         = WIDTH, multiple of 16
    // uint32_t output_stride;    = 16, last layer only
    const int blocks[] = { pc_inf[1]/unit, 1u, 1u }; // num pixels / (16 * N_ITERS)
    
#ifdef FUSED_INF // fused kernel without global memory intermediats.
    const int id_fused = dt_node_add( // infer kernel and store intermediate activations too
        graph, module, "kpn-t", "infapply", blocks[0] * DT_LOCAL_SIZE_X, blocks[1] * DT_LOCAL_SIZE_Y, 1, sizeof(pc_inf), pc_inf, 3,
        "M", "read",  "rgba", "*",   dt_no_roi,   // input image mipmap level
        "w", "read",  "ssbo", "f16", dt_no_roi,   // MLP weights
        "I", "write", "rgba", "f16", &roi_M[i]);  // output convolved image
    dt_connector_copy(graph, module, 2, id_fused, 1); // wire weights
    const int id_apply = id_fused; // for connections below
#else
    dt_roi_t roi_K = (dt_roi_t){ .wd = batch_size, .ht = 16 }; // output: 15-tap kernel (+1 alpha) per pixel
    // we are passing the number of threads assuming DT_LOCAL_SIZE_X and DT_LOCAL_SIZE_Y
    const int id_inf = dt_node_add( // infer kernel and store intermediate activations too
        graph, module, "kpn-t", have_coopmat ? "inf" : "inf-", blocks[0] * DT_LOCAL_SIZE_X, blocks[1] * DT_LOCAL_SIZE_Y, 1, sizeof(pc_inf), pc_inf, 3,
        "M", "read",  "rgba", "*",   dt_no_roi, // input image mipmap level
        "w", "read",  "ssbo", "f16", dt_no_roi, // MLP weights
        "K", "write", "ssbo", "f16", &roi_K);   // network output, 15 kernel weights + 1 alpha per px 

    const int id_apply = dt_node_add( // apply convolution
        graph, module, "kpn-t", "apply", roi_M[i].wd, roi_M[i].ht, 1, 0, 0, 3,
        "M", "read",  "rgba", "*",   dt_no_roi,
        "K", "read",  "ssbo", "f16", dt_no_roi,
        "I", "write", "rgba", "f16", &roi_M[i]);  // output convolved image
    dt_connector_copy(graph, module, 2, id_inf, 1); // wire weights
    CONN(dt_node_connect_named(graph, id_inf, "K", id_apply, "K"));
#endif

    int id_up = -1;
#ifdef PRE_MLP_DIFF
    id_up = dt_node_add( // upsample coarse and blend the result to fine with the fine alpha
        graph, module, "kpn-t", "up", roi_M[i].wd, roi_M[i].ht, 1, 0, 0, 3,
        "I",  "read",  "rgba", "*",   dt_no_roi,   // the convolved image on this (fine) scale (with alpha channel)
        "Oc", "read",  "rgba", "*",   dt_no_roi,   // output of coarse level: connect to id_apply->out on coarsest i+1==3, or else to id_up i+1
        "O",  "write", "rgba", "f16", &roi_M[i]);
    CONN(dt_node_connect_named(graph, id_apply, "I", id_up, "I"));
    if(i > 0) CONN(dt_node_connect_named(graph, id_up, "O", id_up_prev, "Oc")); // plug our (coarser) into "Oc" input on finer level
    if(i == Mcnt-2)
    { // i is the coarsest layer we have detail coefs for (does not upscale yet another even coarser layer)
#ifdef NO_ALIAS
      CONN(dt_node_connect_named(graph, id_sub[Mcnt-2], "M1", id_up, "Oc"));
#else
      char M[3] = "MX"; M[1] = '0' + i + 1;
      CONN(dt_node_connect_named(graph, id_mip1, M, id_up, "Oc"));
#endif
    }
#else
    if(i < Mcnt-2)
    {
      id_up = dt_node_add( // upsample coarse and blend the result to fine with the fine alpha
          graph, module, "kpn-t", "up", roi_M[i].wd, roi_M[i].ht, 1, 0, 0, 3,
          "I",  "read",  "rgba", "*",   dt_no_roi,   // the convolved image on this (fine) scale (with alpha channel)
          "Oc", "read",  "rgba", "*",   dt_no_roi,   // output of coarse level: connect to id_apply->out on coarsest i+1==3, or else to id_up i+1
          "O",  "write", "rgba", "f16", &roi_M[i]);
      CONN(dt_node_connect_named(graph, id_apply, "I", id_up, "I"));
      if(i > 0) CONN(dt_node_connect_named(graph, id_up, "O", id_up_prev, "Oc")); // plug our (coarser) into "Oc" input on finer level
    }
    else
    { // i is the coarsest layer (does not upscale yet another even coarser layer)
      CONN(dt_node_connect_named(graph, id_apply, "I", id_up_prev, "Oc"));
    }
#endif
    id_up_prev = id_up;

#ifdef PRE_MLP_DIFF
    if(i==0) dt_connector_copy(graph, module, 1, id_up, 2); // output image
    CONN(dt_node_connect_named(graph, id_diff[i], "D", id_apply, "M"));
    CONN(dt_node_connect_named(graph, id_diff[i], "D", id_inf,   "M"));
#else
    char mipstr[3] = "MX";
    mipstr[1] = '0' + i;
    const int id_mip = i > 3 ? id_mip1 : id_mip0;
#ifndef FUSED_INF
    if(i==0)  dt_connector_copy    (graph, module, 0,      id_inf,  0); // input image
    else CONN(dt_node_connect_named(graph, id_mip, mipstr, id_inf, "M"));
#endif
    if(i==0)
    {
      dt_connector_copy(graph, module, 1, id_up,     2); // output image
      dt_connector_copy(graph, module, 0, id_apply,  0); // input image
    }
    else
      CONN(dt_node_connect_named(graph, id_mip, mipstr, id_apply,  "M"));
#endif
  }
}
