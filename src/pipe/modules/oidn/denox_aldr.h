#ifndef oidn_aldr_DENOX_MODULE_H
#define oidn_aldr_DENOX_MODULE_H
#include "modules/api.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int denox_read_source_aldr(dt_module_t* mod, void* mapped, dt_read_source_params_t* p) {
  if (p->node->kernel == dt_token("weights")) {
    FILE* f = dt_graph_open_resource(mod->graph, 0, "data/oidn-aldr.dat", "rb");
    if (!f) {
      snprintf(mod->graph->gui_msg_buf, sizeof(mod->graph->gui_msg_buf),
            "oidn: could not find \"data/oidn-aldr.dat\"");
      return 1;
    }
    fseek(f, 0, SEEK_END);
    const size_t size = ftell(f);
    const size_t expected_size = 1835008;
    if (size != expected_size) {
      snprintf(mod->graph->gui_msg_buf, sizeof(mod->graph->gui_msg_buf),
            "oidn: weight file \"data/oidn-aldr.dat\" has unexpected size!");
      fclose(f);
      return 1;
    }
    fseek(f, 0, SEEK_SET);
    fread(mapped, size, 1, f);
    fclose(f);
  }
  return 0;
}

static void denox_create_nodes_aldr(dt_graph_t* graph, dt_module_t* module,
      uint64_t H, uint64_t W,
      int input_id, const char* input_connector,
      int output_id, const char* output_connector) {
  const int64_t r2 = H + 4;
  const int64_t r3 = r2 / 5;
  const int64_t r4 = W + 127;
  const int64_t r5 = r4 / 128;
  const int64_t r6 = ((W % 16) + 16) % 16;
  const int64_t r7 = r6 * -1;
  const int64_t r8 = r7 + 16;
  const int64_t r9 = ((r8 % 16) + 16) % 16;
  const int64_t r10 = W + r9;
  const int64_t r11 = r10 + 191;
  const int64_t r12 = r11 / 192;
  const int64_t r13 = r10 - 2;
  const int64_t r14 = r13 / 2;
  const int64_t r15 = r14 + 1;
  const int64_t r16 = r15 - 2;
  const int64_t r17 = r16 / 2;
  const int64_t r18 = r17 + 1;
  const int64_t r19 = r18 - 2;
  const int64_t r20 = r19 / 2;
  const int64_t r21 = r20 + 1;
  const int64_t r22 = r21 - 2;
  const int64_t r23 = r22 / 2;
  const int64_t r24 = r23 + 1;
  const int64_t r25 = r24 + 13;
  const int64_t r26 = r25 / 14;
  const int64_t r27 = ((H % 16) + 16) % 16;
  const int64_t r28 = r27 * -1;
  const int64_t r29 = r28 + 16;
  const int64_t r30 = ((r29 % 16) + 16) % 16;
  const int64_t r31 = H + r30;
  const int64_t r32 = r31 + 12;
  const int64_t r33 = r32 / 13;
  const int64_t r34 = r31 - 2;
  const int64_t r35 = r34 / 2;
  const int64_t r36 = r35 + 1;
  const int64_t r37 = r36 - 2;
  const int64_t r38 = r37 / 2;
  const int64_t r39 = r38 + 1;
  const int64_t r40 = r39 - 2;
  const int64_t r41 = r40 / 2;
  const int64_t r42 = r41 + 1;
  const int64_t r43 = r21 * r42;
  const int64_t r44 = r43 * 128;
  const int64_t r45 = r42 - 2;
  const int64_t r46 = r45 / 2;
  const int64_t r47 = r46 + 1;
  const int64_t r48 = r24 * r47;
  const int64_t r49 = r48 * 192;
  const int64_t r52 = r18 * r39;
  const int64_t r53 = r52 * 96;
  const int64_t r54 = r43 * 224;
  const int64_t r57 = r15 * r36;
  const int64_t r58 = r57 * 64;
  const int64_t r59 = r52 * 192;
  const int64_t r62 = r10 * r31;
  const int64_t r63 = r62 * 6;
  const int64_t r64 = r57 * 128;
  const int64_t r68 = r48 * 160;
  const int64_t r70 = r47 + 7;
  const int64_t r71 = r70 / 8;
  const int64_t r74 = r24 + 15;
  const int64_t r75 = r74 / 16;
  const int64_t r79 = r42 + 7;
  const int64_t r80 = r79 / 8;
  const int64_t r81 = r42 + 3;
  const int64_t r82 = r81 / 4;
  const int64_t r83 = r42 + 1;
  const int64_t r84 = r83 / 2;
  const int64_t r87 = r21 + 15;
  const int64_t r88 = r87 / 16;
  const int64_t r92 = r39 + 7;
  const int64_t r93 = r92 / 8;
  const int64_t r94 = r39 + 1;
  const int64_t r95 = r94 / 2;
  const int64_t r98 = r18 + 15;
  const int64_t r99 = r98 / 16;
  const int64_t r101 = r62 * 64;
  const int64_t r106 = r36 + 7;
  const int64_t r107 = r106 / 8;
  const int64_t r108 = r36 + 3;
  const int64_t r109 = r108 / 4;
  const int64_t r112 = r15 + 15;
  const int64_t r113 = r112 / 16;
  const int64_t r116 = r31 + 20;
  const int64_t r117 = r116 / 21;
  const int64_t r118 = r31 + 2;
  const int64_t r119 = r118 / 3;
  const int64_t r121 = r62 * 128;
  const int64_t r123 = r31 + 11;
  const int64_t r124 = r123 / 12;
  const int64_t r127 = r31 + 3;
  const int64_t r128 = r127 / 4;
  const int64_t r129 = r10 + 15;
  const int64_t r130 = r129 / 16;
  const int64_t r131 = r43 * 160;
  const int64_t r132 = H * W;
  const int64_t r133 = r132 * 6;
  dt_roi_t roi0 = {.wd = 1835008, .ht = 1};
  dt_roi_t roi1 = {.wd = (uint32_t)(r133 / 2), .ht = 1};
  dt_roi_t roi2 = {.wd = (uint32_t)(r63), .ht = 1};
  dt_roi_t roi3 = {.wd = (uint32_t)(r101), .ht = 1};
  dt_roi_t roi4 = {.wd = (uint32_t)(r58), .ht = 1};
  dt_roi_t roi5 = {.wd = (uint32_t)(r53), .ht = 1};
  dt_roi_t roi6 = {.wd = (uint32_t)(r44), .ht = 1};
  dt_roi_t roi7 = {.wd = (uint32_t)(r131), .ht = 1};
  dt_roi_t roi8 = {.wd = (uint32_t)(r68), .ht = 1};
  dt_roi_t roi9 = {.wd = (uint32_t)(r49), .ht = 1};
  dt_roi_t roi10 = {.wd = (uint32_t)(r49), .ht = 1};
  dt_roi_t roi11 = {.wd = (uint32_t)(r54), .ht = 1};
  dt_roi_t roi12 = {.wd = (uint32_t)(r54), .ht = 1};
  dt_roi_t roi13 = {.wd = (uint32_t)(r59), .ht = 1};
  dt_roi_t roi14 = {.wd = (uint32_t)(r59), .ht = 1};
  dt_roi_t roi15 = {.wd = (uint32_t)(r64), .ht = 1};
  dt_roi_t roi16 = {.wd = (uint32_t)(r64), .ht = 1};
  dt_roi_t roi17 = {.wd = (uint32_t)(r121), .ht = 1};
  dt_roi_t roi18 = {.wd = (uint32_t)(r101), .ht = 1};
  dt_roi_t roi19 = {.wd = (uint32_t)(r63), .ht = 1};
  dt_roi_t roi20 = {.wd = (uint32_t)(r133 / 2), .ht = 1};
  int weights_id = dt_node_add(graph, module, "oidn", "weights",
      1, 1, 1, 0, NULL, 1, 
      "w", "source", "ssbo", "u8", &roi0);
  // memory_pad (memory_pad.comp)
  const uint32_t memory_pad_pc[6] = {(uint32_t)(r10), (uint32_t)(r31), 0, (uint32_t)(r9), 0, (uint32_t)(r30)};
  const int memory_pad_id = dt_node_add(graph, module, "oidn", "aldr4",
      1 * DT_LOCAL_SIZE_X, r12 * DT_LOCAL_SIZE_Y, r119,
      24, (const int*)memory_pad_pc, 2, //
      "a", "read", "ssbo", "f16", &roi1, // HWC[3] : f16
      "b", "write", "ssbo", "u8", &roi2); // HWC[3] : f16
  // direct_conv (direct_conv.comp)
  const uint32_t direct_conv_pc[2] = {(uint32_t)(r10), (uint32_t)(r31)};
  const int direct_conv_id = dt_node_add(graph, module, "oidn", "aldr7",
      1 * DT_LOCAL_SIZE_X, r130 * DT_LOCAL_SIZE_Y, r117,
      8, (const int*)direct_conv_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi2, // HWC[3] : f16
      "d", "write", "ssbo", "u8", &roi3); // HWC[32] : f16
  // direct_conv_cm (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_pc[2] = {(uint32_t)(r10), (uint32_t)(r31)};
  const int direct_conv_cm_id = dt_node_add(graph, module, "oidn", "aldr8",
      1 * DT_LOCAL_SIZE_X, r130 * DT_LOCAL_SIZE_Y, r124,
      8, (const int*)direct_conv_cm_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi3, // HWC[32] : f16
      "d", "write", "ssbo", "u8", &roi4); // CHWC8[32] : f16
  // direct_conv_cm_1 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_1_pc[2] = {(uint32_t)(r15), (uint32_t)(r36)};
  const int direct_conv_cm_1_id = dt_node_add(graph, module, "oidn", "aldr9",
      1 * DT_LOCAL_SIZE_X, r113 * DT_LOCAL_SIZE_Y, r109,
      8, (const int*)direct_conv_cm_1_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi4, // CHWC8[32] : f16
      "d", "write", "ssbo", "u8", &roi5); // CHWC8[48] : f16
  // direct_conv_cm_2 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_2_pc[2] = {(uint32_t)(r18), (uint32_t)(r39)};
  const int direct_conv_cm_2_id = dt_node_add(graph, module, "oidn", "aldr10",
      1 * DT_LOCAL_SIZE_X, r99 * DT_LOCAL_SIZE_Y, r95,
      8, (const int*)direct_conv_cm_2_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi5, // CHWC8[48] : f16
      "d", "write", "ssbo", "u8", &roi6); // CHWC8[64] : f16
  // direct_conv_1 (direct_conv.comp)
  const uint32_t direct_conv_1_pc[2] = {(uint32_t)(r21), (uint32_t)(r42)};
  const int direct_conv_1_id = dt_node_add(graph, module, "oidn", "aldr13",
      1 * DT_LOCAL_SIZE_X, r88 * DT_LOCAL_SIZE_Y, r82,
      8, (const int*)direct_conv_1_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi6, // CHWC8[64] : f16
      "d", "write", "ssbo", "u8", &roi7); // HWC[80] : f16
  // basic_pool (basic_pool.comp)
  const uint32_t basic_pool_pc[2] = {(uint32_t)(r21), (uint32_t)(r42)};
  const int basic_pool_id = dt_node_add(graph, module, "oidn", "aldr5",
      1 * DT_LOCAL_SIZE_X, r26 * DT_LOCAL_SIZE_Y, r47,
      8, (const int*)basic_pool_pc, 2, //
      "a", "read", "ssbo", "u8", &roi7, // HWC[80] : f16
      "b", "write", "ssbo", "u8", &roi8); // HWC[80] : f16
  // direct_conv_cm_3 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_3_pc[2] = {(uint32_t)(r24), (uint32_t)(r47)};
  const int direct_conv_cm_3_id = dt_node_add(graph, module, "oidn", "aldr14",
      1 * DT_LOCAL_SIZE_X, r75 * DT_LOCAL_SIZE_Y, r71,
      8, (const int*)direct_conv_cm_3_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi8, // HWC[80] : f16
      "d", "write", "ssbo", "u8", &roi9); // HWC[96] : f16
  // direct_conv_cm_4 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_4_pc[2] = {(uint32_t)(r24), (uint32_t)(r47)};
  const int direct_conv_cm_4_id = dt_node_add(graph, module, "oidn", "aldr11",
      1 * DT_LOCAL_SIZE_X, r75 * DT_LOCAL_SIZE_Y, r71,
      8, (const int*)direct_conv_cm_4_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi9, // HWC[96] : f16
      "d", "write", "ssbo", "u8", &roi10); // HWC[96] : f16
  // concat_conv_cm (concat_conv_cm.comp)
  const uint32_t concat_conv_cm_pc[2] = {(uint32_t)(r21), (uint32_t)(r42)};
  const int concat_conv_cm_id = dt_node_add(graph, module, "oidn", "aldr15",
      1 * DT_LOCAL_SIZE_X, r88 * DT_LOCAL_SIZE_Y, r84,
      8, (const int*)concat_conv_cm_pc, 6, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi0,// unknown
      "d", "read", "ssbo", "u8", &roi10, // HWC[96] : f16
      "e", "read", "ssbo", "u8", &roi6, // CHWC8[64] : f16
      "f", "write", "ssbo", "u8", &roi11); // HWC[112] : f16
  // direct_conv_cm_5 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_5_pc[2] = {(uint32_t)(r21), (uint32_t)(r42)};
  const int direct_conv_cm_5_id = dt_node_add(graph, module, "oidn", "aldr12",
      1 * DT_LOCAL_SIZE_X, r88 * DT_LOCAL_SIZE_Y, r80,
      8, (const int*)direct_conv_cm_5_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi11, // HWC[112] : f16
      "d", "write", "ssbo", "u8", &roi12); // HWC[112] : f16
  // concat_conv_cm_1 (concat_conv_cm.comp)
  const uint32_t concat_conv_cm_1_pc[2] = {(uint32_t)(r18), (uint32_t)(r39)};
  const int concat_conv_cm_1_id = dt_node_add(graph, module, "oidn", "aldr17",
      1 * DT_LOCAL_SIZE_X, r99 * DT_LOCAL_SIZE_Y, r39,
      8, (const int*)concat_conv_cm_1_pc, 6, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi0,// unknown
      "d", "read", "ssbo", "u8", &roi12, // HWC[112] : f16
      "e", "read", "ssbo", "u8", &roi5, // CHWC8[48] : f16
      "f", "write", "ssbo", "u8", &roi13); // HWC[96] : f16
  // direct_conv_cm_6 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_6_pc[2] = {(uint32_t)(r18), (uint32_t)(r39)};
  const int direct_conv_cm_6_id = dt_node_add(graph, module, "oidn", "aldr11",
      1 * DT_LOCAL_SIZE_X, r99 * DT_LOCAL_SIZE_Y, r93,
      8, (const int*)direct_conv_cm_6_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi13, // HWC[96] : f16
      "d", "write", "ssbo", "u8", &roi14); // HWC[96] : f16
  // concat_conv_cm_2 (concat_conv_cm.comp)
  const uint32_t concat_conv_cm_2_pc[2] = {(uint32_t)(r15), (uint32_t)(r36)};
  const int concat_conv_cm_2_id = dt_node_add(graph, module, "oidn", "aldr6",
      1 * DT_LOCAL_SIZE_X, r113 * DT_LOCAL_SIZE_Y, r109,
      8, (const int*)concat_conv_cm_2_pc, 6, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi0,// unknown
      "d", "read", "ssbo", "u8", &roi14, // HWC[96] : f16
      "e", "read", "ssbo", "u8", &roi4, // CHWC8[32] : f16
      "f", "write", "ssbo", "u8", &roi15); // HWC[64] : f16
  // direct_conv_cm_7 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_7_pc[2] = {(uint32_t)(r15), (uint32_t)(r36)};
  const int direct_conv_cm_7_id = dt_node_add(graph, module, "oidn", "aldr16",
      1 * DT_LOCAL_SIZE_X, r113 * DT_LOCAL_SIZE_Y, r107,
      8, (const int*)direct_conv_cm_7_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi15, // HWC[64] : f16
      "d", "write", "ssbo", "u8", &roi16); // CHWC8[64] : f16
  // concat_conv_cm_3 (concat_conv_cm.comp)
  const uint32_t concat_conv_cm_3_pc[2] = {(uint32_t)(r10), (uint32_t)(r31)};
  const int concat_conv_cm_3_id = dt_node_add(graph, module, "oidn", "aldr3",
      1 * DT_LOCAL_SIZE_X, r130 * DT_LOCAL_SIZE_Y, r128,
      8, (const int*)concat_conv_cm_3_pc, 6, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi0,// unknown
      "d", "read", "ssbo", "u8", &roi16, // CHWC8[64] : f16
      "e", "read", "ssbo", "u8", &roi2, // HWC[3] : f16
      "f", "write", "ssbo", "u8", &roi17); // CHWC8[64] : f16
  // direct_conv_2 (direct_conv.comp)
  const uint32_t direct_conv_2_pc[2] = {(uint32_t)(r10), (uint32_t)(r31)};
  const int direct_conv_2_id = dt_node_add(graph, module, "oidn", "aldr1",
      1 * DT_LOCAL_SIZE_X, r130 * DT_LOCAL_SIZE_Y, r124,
      8, (const int*)direct_conv_2_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi17, // CHWC8[64] : f16
      "d", "write", "ssbo", "u8", &roi18); // CHWC8[32] : f16
  // direct_conv_3 (direct_conv.comp)
  const uint32_t direct_conv_3_pc[2] = {(uint32_t)(r10), (uint32_t)(r31)};
  const int direct_conv_3_id = dt_node_add(graph, module, "oidn", "aldr0",
      1 * DT_LOCAL_SIZE_X, r130 * DT_LOCAL_SIZE_Y, r33,
      8, (const int*)direct_conv_3_pc, 3, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi18, // CHWC8[32] : f16
      "c", "write", "ssbo", "u8", &roi19); // HWC[3] : f16
  // memory_slice (memory_slice.comp)
  const uint32_t memory_slice_pc[6] = {0, 0, (uint32_t)(r10), (uint32_t)(r31), (uint32_t)(W), (uint32_t)(H)};
  const int memory_slice_id = dt_node_add(graph, module, "oidn", "aldr2",
      1 * DT_LOCAL_SIZE_X, r5 * DT_LOCAL_SIZE_Y, r3,
      24, (const int*)memory_slice_pc, 2, //
      "a", "read", "ssbo", "u8", &roi19, // HWC[3] : f16
      "b", "write", "ssbo", "f16", &roi20); // HWC[3] : f16
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
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_1_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_1_id, "b");
  dt_node_connect_named(graph, direct_conv_cm_id, "d", direct_conv_cm_1_id, "c");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_2_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_2_id, "b");
  dt_node_connect_named(graph, direct_conv_cm_1_id, "d", direct_conv_cm_2_id, "c");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_1_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_1_id, "b");
  dt_node_connect_named(graph, direct_conv_cm_2_id, "d", direct_conv_1_id, "c");
  dt_node_connect_named(graph, direct_conv_1_id, "d", basic_pool_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_3_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_3_id, "b");
  dt_node_connect_named(graph, basic_pool_id, "b", direct_conv_cm_3_id, "c");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_4_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_4_id, "b");
  dt_node_connect_named(graph, direct_conv_cm_3_id, "d", direct_conv_cm_4_id, "c");
  dt_node_connect_named(graph, weights_id, "w", concat_conv_cm_id, "a");
  dt_node_connect_named(graph, weights_id, "w", concat_conv_cm_id, "b");
  dt_node_connect_named(graph, weights_id, "w", concat_conv_cm_id, "c");
  dt_node_connect_named(graph, direct_conv_cm_4_id, "d", concat_conv_cm_id, "d");
  dt_node_connect_named(graph, direct_conv_cm_2_id, "d", concat_conv_cm_id, "e");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_5_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_5_id, "b");
  dt_node_connect_named(graph, concat_conv_cm_id, "f", direct_conv_cm_5_id, "c");
  dt_node_connect_named(graph, weights_id, "w", concat_conv_cm_1_id, "a");
  dt_node_connect_named(graph, weights_id, "w", concat_conv_cm_1_id, "b");
  dt_node_connect_named(graph, weights_id, "w", concat_conv_cm_1_id, "c");
  dt_node_connect_named(graph, direct_conv_cm_5_id, "d", concat_conv_cm_1_id, "d");
  dt_node_connect_named(graph, direct_conv_cm_1_id, "d", concat_conv_cm_1_id, "e");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_6_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_6_id, "b");
  dt_node_connect_named(graph, concat_conv_cm_1_id, "f", direct_conv_cm_6_id, "c");
  dt_node_connect_named(graph, weights_id, "w", concat_conv_cm_2_id, "a");
  dt_node_connect_named(graph, weights_id, "w", concat_conv_cm_2_id, "b");
  dt_node_connect_named(graph, weights_id, "w", concat_conv_cm_2_id, "c");
  dt_node_connect_named(graph, direct_conv_cm_6_id, "d", concat_conv_cm_2_id, "d");
  dt_node_connect_named(graph, direct_conv_cm_id, "d", concat_conv_cm_2_id, "e");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_7_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_7_id, "b");
  dt_node_connect_named(graph, concat_conv_cm_2_id, "f", direct_conv_cm_7_id, "c");
  dt_node_connect_named(graph, weights_id, "w", concat_conv_cm_3_id, "a");
  dt_node_connect_named(graph, weights_id, "w", concat_conv_cm_3_id, "b");
  dt_node_connect_named(graph, weights_id, "w", concat_conv_cm_3_id, "c");
  dt_node_connect_named(graph, direct_conv_cm_7_id, "d", concat_conv_cm_3_id, "d");
  dt_node_connect_named(graph, memory_pad_id, "b", concat_conv_cm_3_id, "e");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_2_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_2_id, "b");
  dt_node_connect_named(graph, concat_conv_cm_3_id, "f", direct_conv_2_id, "c");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_3_id, "a");
  dt_node_connect_named(graph, direct_conv_2_id, "d", direct_conv_3_id, "b");
  dt_node_connect_named(graph, direct_conv_3_id, "c", memory_slice_id, "a");
  if (output_connector == NULL) {
    dt_connector_copy(graph, module, output_id, memory_slice_id, 1);
  } else {
    dt_node_connect_named(graph, memory_slice_id, "b", output_id, output_connector);
  }
  graph->node[direct_conv_id].connector[1].ssbo_offset = 4608;
  graph->node[direct_conv_cm_id].connector[0].ssbo_offset = 4672;
  graph->node[direct_conv_cm_id].connector[1].ssbo_offset = 23104;
  graph->node[direct_conv_cm_1_id].connector[0].ssbo_offset = 23168;
  graph->node[direct_conv_cm_1_id].connector[1].ssbo_offset = 50816;
  graph->node[direct_conv_cm_2_id].connector[0].ssbo_offset = 50912;
  graph->node[direct_conv_cm_2_id].connector[1].ssbo_offset = 106208;
  graph->node[direct_conv_1_id].connector[0].ssbo_offset = 106336;
  graph->node[direct_conv_1_id].connector[1].ssbo_offset = 198496;
  graph->node[direct_conv_cm_3_id].connector[0].ssbo_offset = 198656;
  graph->node[direct_conv_cm_3_id].connector[1].ssbo_offset = 336896;
  graph->node[direct_conv_cm_4_id].connector[0].ssbo_offset = 337088;
  graph->node[direct_conv_cm_4_id].connector[1].ssbo_offset = 502976;
  graph->node[concat_conv_cm_id].connector[0].ssbo_offset = 503168;
  graph->node[concat_conv_cm_id].connector[1].ssbo_offset = 696704;
  graph->node[concat_conv_cm_id].connector[2].ssbo_offset = 825728;
  graph->node[direct_conv_cm_5_id].connector[0].ssbo_offset = 825952;
  graph->node[direct_conv_cm_5_id].connector[1].ssbo_offset = 1051744;
  graph->node[concat_conv_cm_1_id].connector[0].ssbo_offset = 1051968;
  graph->node[concat_conv_cm_1_id].connector[1].ssbo_offset = 1245504;
  graph->node[concat_conv_cm_1_id].connector[2].ssbo_offset = 1328448;
  graph->node[direct_conv_cm_6_id].connector[0].ssbo_offset = 1328640;
  graph->node[direct_conv_cm_6_id].connector[1].ssbo_offset = 1494528;
  graph->node[concat_conv_cm_2_id].connector[0].ssbo_offset = 1494720;
  graph->node[concat_conv_cm_2_id].connector[1].ssbo_offset = 1605312;
  graph->node[concat_conv_cm_2_id].connector[2].ssbo_offset = 1642176;
  graph->node[direct_conv_cm_7_id].connector[0].ssbo_offset = 1642304;
  graph->node[direct_conv_cm_7_id].connector[1].ssbo_offset = 1716032;
  graph->node[concat_conv_cm_3_id].connector[0].ssbo_offset = 1716160;
  graph->node[concat_conv_cm_3_id].connector[1].ssbo_offset = 1789888;
  graph->node[concat_conv_cm_3_id].connector[2].ssbo_offset = 1793344;
  graph->node[direct_conv_2_id].connector[0].ssbo_offset = 1793472;
  graph->node[direct_conv_2_id].connector[1].ssbo_offset = 1830336;
  graph->node[direct_conv_3_id].connector[0].ssbo_offset = 1830400;
}

#endif
