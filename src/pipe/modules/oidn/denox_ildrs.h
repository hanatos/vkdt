#ifndef oidn_ildrs_DENOX_MODULE_H
#define oidn_ildrs_DENOX_MODULE_H
#include "modules/api.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int denox_read_source_ildrs(dt_module_t* mod, void* mapped, dt_read_source_params_t* p) {
  if (p->node->kernel == dt_token("weights")) {
    FILE* f = dt_graph_open_resource(mod->graph, 0, "data/oidn-ildrs.dat", "rb");
    if (!f) {
      snprintf(mod->graph->gui_msg_buf, sizeof(mod->graph->gui_msg_buf),
            "oidn: could not find \"data/oidn-ildrs.dat\"");
      return 1;
    }
    fseek(f, 0, SEEK_END);
    const size_t size = ftell(f);
    const size_t expected_size = 638944;
    if (size != expected_size) {
      snprintf(mod->graph->gui_msg_buf, sizeof(mod->graph->gui_msg_buf),
            "oidn: weight file \"data/oidn-ildrs.dat\" has unexpected size!");
      fclose(f);
      return 1;
    }
    fseek(f, 0, SEEK_SET);
    fread(mapped, size, 1, f);
    fclose(f);
  }
  return 0;
}

static void denox_create_nodes_ildrs(dt_graph_t* graph, dt_module_t* module,
      uint64_t H, uint64_t W,
      int input_id, const char* input_connector,
      int output_id, const char* output_connector) {
  const int64_t r2 = W + 447;
  const int64_t r3 = r2 / 448;
  const int64_t r4 = H + 6;
  const int64_t r5 = r4 / 7;
  const int64_t r6 = ((W % 16) + 16) % 16;
  const int64_t r7 = r6 * -1;
  const int64_t r8 = r7 + 16;
  const int64_t r9 = ((r8 % 16) + 16) % 16;
  const int64_t r10 = W + r9;
  const int64_t r11 = r10 - 2;
  const int64_t r12 = r11 / 2;
  const int64_t r13 = r12 + 1;
  const int64_t r14 = r13 - 2;
  const int64_t r15 = r14 / 2;
  const int64_t r16 = r15 + 1;
  const int64_t r17 = r16 - 2;
  const int64_t r18 = r17 / 2;
  const int64_t r19 = r18 + 1;
  const int64_t r20 = r19 - 2;
  const int64_t r21 = r20 / 2;
  const int64_t r22 = r21 + 1;
  const int64_t r23 = r22 + 511;
  const int64_t r24 = r23 / 512;
  const int64_t r25 = r19 + 511;
  const int64_t r26 = r25 / 512;
  const int64_t r27 = r16 + 511;
  const int64_t r28 = r27 / 512;
  const int64_t r29 = r10 + 31;
  const int64_t r30 = r29 / 32;
  const int64_t r31 = ((H % 16) + 16) % 16;
  const int64_t r32 = r31 * -1;
  const int64_t r33 = r32 + 16;
  const int64_t r34 = ((r33 % 16) + 16) % 16;
  const int64_t r35 = H + r34;
  const int64_t r36 = r35 + 12;
  const int64_t r37 = r36 / 13;
  const int64_t r38 = r35 - 2;
  const int64_t r39 = r38 / 2;
  const int64_t r40 = r39 + 1;
  const int64_t r41 = r40 - 2;
  const int64_t r42 = r41 / 2;
  const int64_t r43 = r42 + 1;
  const int64_t r44 = r43 - 2;
  const int64_t r45 = r44 / 2;
  const int64_t r46 = r45 + 1;
  const int64_t r47 = r19 * r46;
  const int64_t r48 = r47 * 64;
  const int64_t r49 = r46 - 2;
  const int64_t r50 = r49 / 2;
  const int64_t r51 = r50 + 1;
  const int64_t r52 = r22 * r51;
  const int64_t r53 = r52 * 64;
  const int64_t r56 = r16 * r43;
  const int64_t r57 = r56 * 64;
  const int64_t r58 = r47 * 128;
  const int64_t r61 = r13 * r40;
  const int64_t r62 = r61 * 64;
  const int64_t r63 = r56 * 128;
  const int64_t r66 = r10 * r35;
  const int64_t r67 = r66 * 6;
  const int64_t r70 = r51 + 23;
  const int64_t r71 = r70 / 24;
  const int64_t r72 = r51 + 15;
  const int64_t r73 = r72 / 16;
  const int64_t r76 = r22 + 15;
  const int64_t r77 = r76 / 16;
  const int64_t r78 = r46 + 23;
  const int64_t r79 = r78 / 24;
  const int64_t r82 = r46 + 11;
  const int64_t r83 = r82 / 12;
  const int64_t r84 = r46 + 3;
  const int64_t r85 = r84 / 4;
  const int64_t r88 = r19 + 15;
  const int64_t r89 = r88 / 16;
  const int64_t r90 = r43 + 23;
  const int64_t r91 = r90 / 24;
  const int64_t r95 = r43 + 11;
  const int64_t r96 = r95 / 12;
  const int64_t r97 = r43 + 9;
  const int64_t r98 = r97 / 10;
  const int64_t r101 = r16 + 15;
  const int64_t r102 = r101 / 16;
  const int64_t r106 = r40 + 23;
  const int64_t r107 = r106 / 24;
  const int64_t r108 = r40 + 9;
  const int64_t r109 = r108 / 10;
  const int64_t r110 = r40 + 5;
  const int64_t r111 = r110 / 6;
  const int64_t r113 = r61 * 128;
  const int64_t r115 = r13 + 15;
  const int64_t r116 = r115 / 16;
  const int64_t r120 = r66 * 64;
  const int64_t r122 = r35 + 23;
  const int64_t r123 = r122 / 24;
  const int64_t r124 = r35 + 7;
  const int64_t r125 = r124 / 8;
  const int64_t r128 = r35 + 3;
  const int64_t r129 = r128 / 4;
  const int64_t r130 = r10 + 15;
  const int64_t r131 = r130 / 16;
  const int64_t r132 = H * W;
  const int64_t r133 = r132 * 6;
  dt_roi_t roi0 = {.wd = 638944, .ht = 1};
  dt_roi_t roi1 = {.wd = (uint32_t)(r133 / 2), .ht = 1};
  dt_roi_t roi2 = {.wd = (uint32_t)(r67), .ht = 1};
  dt_roi_t roi3 = {.wd = (uint32_t)(r120), .ht = 1};
  dt_roi_t roi4 = {.wd = (uint32_t)(r62), .ht = 1};
  dt_roi_t roi5 = {.wd = (uint32_t)(r62), .ht = 1};
  dt_roi_t roi6 = {.wd = (uint32_t)(r57), .ht = 1};
  dt_roi_t roi7 = {.wd = (uint32_t)(r57), .ht = 1};
  dt_roi_t roi8 = {.wd = (uint32_t)(r48), .ht = 1};
  dt_roi_t roi9 = {.wd = (uint32_t)(r48), .ht = 1};
  dt_roi_t roi10 = {.wd = (uint32_t)(r53), .ht = 1};
  dt_roi_t roi11 = {.wd = (uint32_t)(r53), .ht = 1};
  dt_roi_t roi12 = {.wd = (uint32_t)(r53), .ht = 1};
  dt_roi_t roi13 = {.wd = (uint32_t)(r58), .ht = 1};
  dt_roi_t roi14 = {.wd = (uint32_t)(r58), .ht = 1};
  dt_roi_t roi15 = {.wd = (uint32_t)(r63), .ht = 1};
  dt_roi_t roi16 = {.wd = (uint32_t)(r63), .ht = 1};
  dt_roi_t roi17 = {.wd = (uint32_t)(r113), .ht = 1};
  dt_roi_t roi18 = {.wd = (uint32_t)(r62), .ht = 1};
  dt_roi_t roi19 = {.wd = (uint32_t)(r120), .ht = 1};
  dt_roi_t roi20 = {.wd = (uint32_t)(r120), .ht = 1};
  dt_roi_t roi21 = {.wd = (uint32_t)(r67), .ht = 1};
  dt_roi_t roi22 = {.wd = (uint32_t)(r133 / 2), .ht = 1};
  int weights_id = dt_node_add(graph, module, "oidn", "weights",
      1, 1, 1, 0, NULL, 1, 
      "w", "source", "ssbo", "u8", &roi0);
  // memory_pad (memory_pad.comp)
  const uint32_t memory_pad_pc[6] = {(uint32_t)(r10), (uint32_t)(r35), 0, (uint32_t)(r9), 0, (uint32_t)(r34)};
  const int memory_pad_id = dt_node_add(graph, module, "oidn", "ildrs3",
      1 * DT_LOCAL_SIZE_X, r30 * DT_LOCAL_SIZE_Y, r125,
      24, (const int*)memory_pad_pc, 2, //
      "a", "read", "ssbo", "f16", &roi1, // HWC[3] : f16
      "b", "write", "ssbo", "u8", &roi2); // HWC[3] : f16
  // direct_conv (direct_conv.comp)
  const uint32_t direct_conv_pc[2] = {(uint32_t)(r10), (uint32_t)(r35)};
  const int direct_conv_id = dt_node_add(graph, module, "oidn", "ildrs1",
      1 * DT_LOCAL_SIZE_X, r131 * DT_LOCAL_SIZE_Y, r129,
      8, (const int*)direct_conv_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi2, // HWC[3] : f16
      "d", "write", "ssbo", "u8", &roi3); // HWC[32] : f16
  // direct_conv_cm (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_pc[2] = {(uint32_t)(r10), (uint32_t)(r35)};
  const int direct_conv_cm_id = dt_node_add(graph, module, "oidn", "ildrs5",
      1 * DT_LOCAL_SIZE_X, r131 * DT_LOCAL_SIZE_Y, r125,
      8, (const int*)direct_conv_cm_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi3, // HWC[32] : f16
      "d", "write", "ssbo", "u8", &roi4); // CHWC8[32] : f16
  // direct_conv_1 (direct_conv.comp)
  const uint32_t direct_conv_1_pc[2] = {(uint32_t)(r13), (uint32_t)(r40)};
  const int direct_conv_1_id = dt_node_add(graph, module, "oidn", "ildrs0",
      1 * DT_LOCAL_SIZE_X, r116 * DT_LOCAL_SIZE_Y, r107,
      8, (const int*)direct_conv_1_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi4, // CHWC8[32] : f16
      "d", "write", "ssbo", "u8", &roi5); // CHWC8[32] : f16
  // basic_pool (basic_pool.comp)
  const uint32_t basic_pool_pc[2] = {(uint32_t)(r13), (uint32_t)(r40)};
  const int basic_pool_id = dt_node_add(graph, module, "oidn", "ildrs6",
      4 * DT_LOCAL_SIZE_X, r28 * DT_LOCAL_SIZE_Y, r43,
      8, (const int*)basic_pool_pc, 2, //
      "a", "read", "ssbo", "u8", &roi5, // CHWC8[32] : f16
      "b", "write", "ssbo", "u8", &roi6); // CHWC8[32] : f16
  // direct_conv_2 (direct_conv.comp)
  const uint32_t direct_conv_2_pc[2] = {(uint32_t)(r16), (uint32_t)(r43)};
  const int direct_conv_2_id = dt_node_add(graph, module, "oidn", "ildrs0",
      1 * DT_LOCAL_SIZE_X, r102 * DT_LOCAL_SIZE_Y, r91,
      8, (const int*)direct_conv_2_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi6, // CHWC8[32] : f16
      "d", "write", "ssbo", "u8", &roi7); // CHWC8[32] : f16
  // basic_pool_1 (basic_pool.comp)
  const uint32_t basic_pool_1_pc[2] = {(uint32_t)(r16), (uint32_t)(r43)};
  const int basic_pool_1_id = dt_node_add(graph, module, "oidn", "ildrs6",
      4 * DT_LOCAL_SIZE_X, r26 * DT_LOCAL_SIZE_Y, r46,
      8, (const int*)basic_pool_1_pc, 2, //
      "a", "read", "ssbo", "u8", &roi7, // CHWC8[32] : f16
      "b", "write", "ssbo", "u8", &roi8); // CHWC8[32] : f16
  // direct_conv_3 (direct_conv.comp)
  const uint32_t direct_conv_3_pc[2] = {(uint32_t)(r19), (uint32_t)(r46)};
  const int direct_conv_3_id = dt_node_add(graph, module, "oidn", "ildrs0",
      1 * DT_LOCAL_SIZE_X, r89 * DT_LOCAL_SIZE_Y, r79,
      8, (const int*)direct_conv_3_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi8, // CHWC8[32] : f16
      "d", "write", "ssbo", "u8", &roi9); // CHWC8[32] : f16
  // basic_pool_2 (basic_pool.comp)
  const uint32_t basic_pool_2_pc[2] = {(uint32_t)(r19), (uint32_t)(r46)};
  const int basic_pool_2_id = dt_node_add(graph, module, "oidn", "ildrs6",
      4 * DT_LOCAL_SIZE_X, r24 * DT_LOCAL_SIZE_Y, r51,
      8, (const int*)basic_pool_2_pc, 2, //
      "a", "read", "ssbo", "u8", &roi9, // CHWC8[32] : f16
      "b", "write", "ssbo", "u8", &roi10); // CHWC8[32] : f16
  // direct_conv_4 (direct_conv.comp)
  const uint32_t direct_conv_4_pc[2] = {(uint32_t)(r22), (uint32_t)(r51)};
  const int direct_conv_4_id = dt_node_add(graph, module, "oidn", "ildrs0",
      1 * DT_LOCAL_SIZE_X, r77 * DT_LOCAL_SIZE_Y, r71,
      8, (const int*)direct_conv_4_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi10, // CHWC8[32] : f16
      "d", "write", "ssbo", "u8", &roi11); // CHWC8[32] : f16
  // direct_conv_5 (direct_conv.comp)
  const uint32_t direct_conv_5_pc[2] = {(uint32_t)(r22), (uint32_t)(r51)};
  const int direct_conv_5_id = dt_node_add(graph, module, "oidn", "ildrs8",
      1 * DT_LOCAL_SIZE_X, r77 * DT_LOCAL_SIZE_Y, r73,
      8, (const int*)direct_conv_5_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi11, // CHWC8[32] : f16
      "d", "write", "ssbo", "u8", &roi12); // HWC[32] : f16
  // concat_conv_cm (concat_conv_cm.comp)
  const uint32_t concat_conv_cm_pc[2] = {(uint32_t)(r19), (uint32_t)(r46)};
  const int concat_conv_cm_id = dt_node_add(graph, module, "oidn", "ildrs10",
      1 * DT_LOCAL_SIZE_X, r89 * DT_LOCAL_SIZE_Y, r85,
      8, (const int*)concat_conv_cm_pc, 6, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi0,// unknown
      "d", "read", "ssbo", "u8", &roi12, // HWC[32] : f16
      "e", "read", "ssbo", "u8", &roi8, // CHWC8[32] : f16
      "f", "write", "ssbo", "u8", &roi13); // HWC[64] : f16
  // direct_conv_cm_1 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_1_pc[2] = {(uint32_t)(r19), (uint32_t)(r46)};
  const int direct_conv_cm_1_id = dt_node_add(graph, module, "oidn", "ildrs2",
      1 * DT_LOCAL_SIZE_X, r89 * DT_LOCAL_SIZE_Y, r83,
      8, (const int*)direct_conv_cm_1_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi13, // HWC[64] : f16
      "d", "write", "ssbo", "u8", &roi14); // HWC[64] : f16
  // concat_conv_cm_1 (concat_conv_cm.comp)
  const uint32_t concat_conv_cm_1_pc[2] = {(uint32_t)(r16), (uint32_t)(r43)};
  const int concat_conv_cm_1_id = dt_node_add(graph, module, "oidn", "ildrs11",
      1 * DT_LOCAL_SIZE_X, r102 * DT_LOCAL_SIZE_Y, r98,
      8, (const int*)concat_conv_cm_1_pc, 6, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi0,// unknown
      "d", "read", "ssbo", "u8", &roi14, // HWC[64] : f16
      "e", "read", "ssbo", "u8", &roi6, // CHWC8[32] : f16
      "f", "write", "ssbo", "u8", &roi15); // HWC[64] : f16
  // direct_conv_cm_2 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_2_pc[2] = {(uint32_t)(r16), (uint32_t)(r43)};
  const int direct_conv_cm_2_id = dt_node_add(graph, module, "oidn", "ildrs2",
      1 * DT_LOCAL_SIZE_X, r102 * DT_LOCAL_SIZE_Y, r96,
      8, (const int*)direct_conv_cm_2_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi15, // HWC[64] : f16
      "d", "write", "ssbo", "u8", &roi16); // HWC[64] : f16
  // concat_conv_cm_2 (concat_conv_cm.comp)
  const uint32_t concat_conv_cm_2_pc[2] = {(uint32_t)(r13), (uint32_t)(r40)};
  const int concat_conv_cm_2_id = dt_node_add(graph, module, "oidn", "ildrs12",
      1 * DT_LOCAL_SIZE_X, r116 * DT_LOCAL_SIZE_Y, r109,
      8, (const int*)concat_conv_cm_2_pc, 6, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi0,// unknown
      "d", "read", "ssbo", "u8", &roi16, // HWC[64] : f16
      "e", "read", "ssbo", "u8", &roi4, // CHWC8[32] : f16
      "f", "write", "ssbo", "u8", &roi17); // CHWC8[64] : f16
  // direct_conv_6 (direct_conv.comp)
  const uint32_t direct_conv_6_pc[2] = {(uint32_t)(r13), (uint32_t)(r40)};
  const int direct_conv_6_id = dt_node_add(graph, module, "oidn", "ildrs9",
      1 * DT_LOCAL_SIZE_X, r116 * DT_LOCAL_SIZE_Y, r111,
      8, (const int*)direct_conv_6_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi17, // CHWC8[64] : f16
      "d", "write", "ssbo", "u8", &roi18); // CHWC8[32] : f16
  // concat_conv_cm_3 (concat_conv_cm.comp)
  const uint32_t concat_conv_cm_3_pc[2] = {(uint32_t)(r10), (uint32_t)(r35)};
  const int concat_conv_cm_3_id = dt_node_add(graph, module, "oidn", "ildrs13",
      1 * DT_LOCAL_SIZE_X, r131 * DT_LOCAL_SIZE_Y, r125,
      8, (const int*)concat_conv_cm_3_pc, 6, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi0,// unknown
      "d", "read", "ssbo", "u8", &roi18, // CHWC8[32] : f16
      "e", "read", "ssbo", "u8", &roi2, // HWC[3] : f16
      "f", "write", "ssbo", "u8", &roi19); // CHWC8[32] : f16
  // direct_conv_7 (direct_conv.comp)
  const uint32_t direct_conv_7_pc[2] = {(uint32_t)(r10), (uint32_t)(r35)};
  const int direct_conv_7_id = dt_node_add(graph, module, "oidn", "ildrs0",
      1 * DT_LOCAL_SIZE_X, r131 * DT_LOCAL_SIZE_Y, r123,
      8, (const int*)direct_conv_7_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi19, // CHWC8[32] : f16
      "d", "write", "ssbo", "u8", &roi20); // CHWC8[32] : f16
  // direct_conv_8 (direct_conv.comp)
  const uint32_t direct_conv_8_pc[2] = {(uint32_t)(r10), (uint32_t)(r35)};
  const int direct_conv_8_id = dt_node_add(graph, module, "oidn", "ildrs4",
      1 * DT_LOCAL_SIZE_X, r131 * DT_LOCAL_SIZE_Y, r37,
      8, (const int*)direct_conv_8_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi20, // CHWC8[32] : f16
      "d", "write", "ssbo", "u8", &roi21); // HWC[3] : f16
  // memory_slice (memory_slice.comp)
  const uint32_t memory_slice_pc[6] = {0, 0, (uint32_t)(r10), (uint32_t)(r35), (uint32_t)(W), (uint32_t)(H)};
  const int memory_slice_id = dt_node_add(graph, module, "oidn", "ildrs7",
      1 * DT_LOCAL_SIZE_X, r3 * DT_LOCAL_SIZE_Y, r5,
      24, (const int*)memory_slice_pc, 2, //
      "a", "read", "ssbo", "u8", &roi21, // HWC[3] : f16
      "b", "write", "ssbo", "f16", &roi22); // HWC[3] : f16
  if (input_connector == NULL) {
    dt_connector_copy(graph, module, input_id, memory_pad_id, 0);
  } else {
    dt_node_connect_named(graph, input_id, input_connector, memory_pad_id, "a");
  }
  dt_node_connect_named(graph, weights_id, "w", direct_conv_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_id, "b");
  dt_node_connect_named(graph, memory_pad_id, "b", direct_conv_id, "c");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_id, "b");
  dt_node_connect_named(graph, direct_conv_id, "d", direct_conv_cm_id, "c");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_1_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_1_id, "b");
  dt_node_connect_named(graph, direct_conv_cm_id, "d", direct_conv_1_id, "c");
  dt_node_connect_named(graph, direct_conv_1_id, "d", basic_pool_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_2_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_2_id, "b");
  dt_node_connect_named(graph, basic_pool_id, "b", direct_conv_2_id, "c");
  dt_node_connect_named(graph, direct_conv_2_id, "d", basic_pool_1_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_3_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_3_id, "b");
  dt_node_connect_named(graph, basic_pool_1_id, "b", direct_conv_3_id, "c");
  dt_node_connect_named(graph, direct_conv_3_id, "d", basic_pool_2_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_4_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_4_id, "b");
  dt_node_connect_named(graph, basic_pool_2_id, "b", direct_conv_4_id, "c");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_5_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_5_id, "b");
  dt_node_connect_named(graph, direct_conv_4_id, "d", direct_conv_5_id, "c");
  dt_node_connect_named(graph, weights_id, "w", concat_conv_cm_id, "a");
  dt_node_connect_named(graph, weights_id, "w", concat_conv_cm_id, "b");
  dt_node_connect_named(graph, weights_id, "w", concat_conv_cm_id, "c");
  dt_node_connect_named(graph, direct_conv_5_id, "d", concat_conv_cm_id, "d");
  dt_node_connect_named(graph, basic_pool_1_id, "b", concat_conv_cm_id, "e");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_1_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_1_id, "b");
  dt_node_connect_named(graph, concat_conv_cm_id, "f", direct_conv_cm_1_id, "c");
  dt_node_connect_named(graph, weights_id, "w", concat_conv_cm_1_id, "a");
  dt_node_connect_named(graph, weights_id, "w", concat_conv_cm_1_id, "b");
  dt_node_connect_named(graph, weights_id, "w", concat_conv_cm_1_id, "c");
  dt_node_connect_named(graph, direct_conv_cm_1_id, "d", concat_conv_cm_1_id, "d");
  dt_node_connect_named(graph, basic_pool_id, "b", concat_conv_cm_1_id, "e");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_2_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_2_id, "b");
  dt_node_connect_named(graph, concat_conv_cm_1_id, "f", direct_conv_cm_2_id, "c");
  dt_node_connect_named(graph, weights_id, "w", concat_conv_cm_2_id, "a");
  dt_node_connect_named(graph, weights_id, "w", concat_conv_cm_2_id, "b");
  dt_node_connect_named(graph, weights_id, "w", concat_conv_cm_2_id, "c");
  dt_node_connect_named(graph, direct_conv_cm_2_id, "d", concat_conv_cm_2_id, "d");
  dt_node_connect_named(graph, direct_conv_cm_id, "d", concat_conv_cm_2_id, "e");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_6_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_6_id, "b");
  dt_node_connect_named(graph, concat_conv_cm_2_id, "f", direct_conv_6_id, "c");
  dt_node_connect_named(graph, weights_id, "w", concat_conv_cm_3_id, "a");
  dt_node_connect_named(graph, weights_id, "w", concat_conv_cm_3_id, "b");
  dt_node_connect_named(graph, weights_id, "w", concat_conv_cm_3_id, "c");
  dt_node_connect_named(graph, direct_conv_6_id, "d", concat_conv_cm_3_id, "d");
  dt_node_connect_named(graph, memory_pad_id, "b", concat_conv_cm_3_id, "e");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_7_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_7_id, "b");
  dt_node_connect_named(graph, concat_conv_cm_3_id, "f", direct_conv_7_id, "c");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_8_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_8_id, "b");
  dt_node_connect_named(graph, direct_conv_7_id, "d", direct_conv_8_id, "c");
  dt_node_connect_named(graph, direct_conv_8_id, "d", memory_slice_id, "a");
  if (output_connector == NULL) {
    dt_connector_copy(graph, module, output_id, memory_slice_id, 1);
  } else {
    dt_node_connect_named(graph, memory_slice_id, "b", output_id, output_connector);
  }
  graph->node[direct_conv_id].connector[1].ssbo_offset = 4608;
  graph->node[direct_conv_cm_id].connector[0].ssbo_offset = 4672;
  graph->node[direct_conv_cm_id].connector[1].ssbo_offset = 23104;
  graph->node[direct_conv_1_id].connector[0].ssbo_offset = 23168;
  graph->node[direct_conv_1_id].connector[1].ssbo_offset = 41600;
  graph->node[direct_conv_2_id].connector[0].ssbo_offset = 41664;
  graph->node[direct_conv_2_id].connector[1].ssbo_offset = 60096;
  graph->node[direct_conv_3_id].connector[0].ssbo_offset = 60160;
  graph->node[direct_conv_3_id].connector[1].ssbo_offset = 78592;
  graph->node[direct_conv_4_id].connector[0].ssbo_offset = 78656;
  graph->node[direct_conv_4_id].connector[1].ssbo_offset = 97088;
  graph->node[direct_conv_5_id].connector[0].ssbo_offset = 97152;
  graph->node[direct_conv_5_id].connector[1].ssbo_offset = 115584;
  graph->node[concat_conv_cm_id].connector[0].ssbo_offset = 115648;
  graph->node[concat_conv_cm_id].connector[1].ssbo_offset = 152512;
  graph->node[concat_conv_cm_id].connector[2].ssbo_offset = 189376;
  graph->node[direct_conv_cm_1_id].connector[0].ssbo_offset = 189504;
  graph->node[direct_conv_cm_1_id].connector[1].ssbo_offset = 263232;
  graph->node[concat_conv_cm_1_id].connector[0].ssbo_offset = 263360;
  graph->node[concat_conv_cm_1_id].connector[1].ssbo_offset = 337088;
  graph->node[concat_conv_cm_1_id].connector[2].ssbo_offset = 373952;
  graph->node[direct_conv_cm_2_id].connector[0].ssbo_offset = 374080;
  graph->node[direct_conv_cm_2_id].connector[1].ssbo_offset = 447808;
  graph->node[concat_conv_cm_2_id].connector[0].ssbo_offset = 447936;
  graph->node[concat_conv_cm_2_id].connector[1].ssbo_offset = 521664;
  graph->node[concat_conv_cm_2_id].connector[2].ssbo_offset = 558528;
  graph->node[direct_conv_6_id].connector[0].ssbo_offset = 558656;
  graph->node[direct_conv_6_id].connector[1].ssbo_offset = 595520;
  graph->node[concat_conv_cm_3_id].connector[0].ssbo_offset = 595584;
  graph->node[concat_conv_cm_3_id].connector[1].ssbo_offset = 614016;
  graph->node[concat_conv_cm_3_id].connector[2].ssbo_offset = 615744;
  graph->node[direct_conv_7_id].connector[0].ssbo_offset = 615808;
  graph->node[direct_conv_7_id].connector[1].ssbo_offset = 634240;
  graph->node[direct_conv_8_id].connector[0].ssbo_offset = 634304;
  graph->node[direct_conv_8_id].connector[1].ssbo_offset = 638912;
}

#endif
