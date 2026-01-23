#ifndef oidn_nldr_DENOX_MODULE_H
#define oidn_nldr_DENOX_MODULE_H
#include "modules/api.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int denox_read_source_nldr(dt_module_t* mod, void* mapped, dt_read_source_params_t* p) {
  if (p->node->kernel == dt_token("weights")) {
    FILE* f = dt_graph_open_resource(mod->graph, 0, "data/oidn-nldr.dat", "rb");
    if (!f) {
      snprintf(mod->graph->gui_msg_buf, sizeof(mod->graph->gui_msg_buf),
            "oidn: could not find \"data/oidn-nldr.dat\"");
      return 1;
    }
    fseek(f, 0, SEEK_END);
    const size_t size = ftell(f);
    const size_t expected_size = 1829248;
    if (size != expected_size) {
      snprintf(mod->graph->gui_msg_buf, sizeof(mod->graph->gui_msg_buf),
            "oidn: weight file \"data/oidn-nldr.dat\" has unexpected size!");
      fclose(f);
      return 1;
    }
    fseek(f, 0, SEEK_SET);
    fread(mapped, size, 1, f);
    fclose(f);
  }
  return 0;
}

static void denox_create_nodes_nldr(dt_graph_t* graph, dt_module_t* module,
      uint64_t H, uint64_t W,
      int input_id, const char* input_connector,
      int output_id, const char* output_connector) {
  const int64_t r2 = W + 127;
  const int64_t r3 = r2 / 128;
  const int64_t r4 = ((W % 16) + 16) % 16;
  const int64_t r5 = r4 * -1;
  const int64_t r6 = r5 + 16;
  const int64_t r7 = ((r6 % 16) + 16) % 16;
  const int64_t r8 = W + r7;
  const int64_t r9 = r8 + 95;
  const int64_t r10 = r9 / 96;
  const int64_t r11 = ((H % 16) + 16) % 16;
  const int64_t r12 = r11 * -1;
  const int64_t r13 = r12 + 16;
  const int64_t r14 = ((r13 % 16) + 16) % 16;
  const int64_t r15 = H + r14;
  const int64_t r16 = r15 - 2;
  const int64_t r17 = r16 / 2;
  const int64_t r18 = r17 + 1;
  const int64_t r19 = r18 - 2;
  const int64_t r20 = r19 / 2;
  const int64_t r21 = r20 + 1;
  const int64_t r22 = r21 - 2;
  const int64_t r23 = r22 / 2;
  const int64_t r24 = r23 + 1;
  const int64_t r25 = r8 - 2;
  const int64_t r26 = r25 / 2;
  const int64_t r27 = r26 + 1;
  const int64_t r28 = r27 - 2;
  const int64_t r29 = r28 / 2;
  const int64_t r30 = r29 + 1;
  const int64_t r31 = r30 - 2;
  const int64_t r32 = r31 / 2;
  const int64_t r33 = r32 + 1;
  const int64_t r34 = r33 * r24;
  const int64_t r35 = r34 * 128;
  const int64_t r36 = r24 - 2;
  const int64_t r37 = r36 / 2;
  const int64_t r38 = r37 + 1;
  const int64_t r39 = r33 - 2;
  const int64_t r40 = r39 / 2;
  const int64_t r41 = r40 + 1;
  const int64_t r42 = r41 * r38;
  const int64_t r43 = r42 * 192;
  const int64_t r46 = r30 * r21;
  const int64_t r47 = r46 * 96;
  const int64_t r48 = r34 * 224;
  const int64_t r51 = r27 * r18;
  const int64_t r52 = r51 * 64;
  const int64_t r53 = r46 * 192;
  const int64_t r56 = r8 * r15;
  const int64_t r57 = r56 * 6;
  const int64_t r58 = r51 * 128;
  const int64_t r62 = r42 * 160;
  const int64_t r66 = r38 + 3;
  const int64_t r67 = r66 / 4;
  const int64_t r68 = r41 + 15;
  const int64_t r69 = r68 / 16;
  const int64_t r70 = r24 + 9;
  const int64_t r71 = r70 / 10;
  const int64_t r75 = r24 + 5;
  const int64_t r76 = r75 / 6;
  const int64_t r77 = r24 + 7;
  const int64_t r78 = r77 / 8;
  const int64_t r81 = r33 + 15;
  const int64_t r82 = r81 / 16;
  const int64_t r83 = r21 + 15;
  const int64_t r84 = r83 / 16;
  const int64_t r88 = r21 + 7;
  const int64_t r89 = r88 / 8;
  const int64_t r92 = r30 + 15;
  const int64_t r93 = r92 / 16;
  const int64_t r95 = r56 * 64;
  const int64_t r100 = r18 + 11;
  const int64_t r101 = r100 / 12;
  const int64_t r102 = r18 + 5;
  const int64_t r103 = r102 / 6;
  const int64_t r104 = r18 + 7;
  const int64_t r105 = r104 / 8;
  const int64_t r108 = r27 + 15;
  const int64_t r109 = r108 / 16;
  const int64_t r111 = r15 + 6;
  const int64_t r112 = r111 / 7;
  const int64_t r115 = r56 * 128;
  const int64_t r117 = r15 + 11;
  const int64_t r118 = r117 / 12;
  const int64_t r119 = r15 + 7;
  const int64_t r120 = r119 / 8;
  const int64_t r123 = r15 + 3;
  const int64_t r124 = r123 / 4;
  const int64_t r125 = r8 + 15;
  const int64_t r126 = r125 / 16;
  const int64_t r127 = H * W;
  const int64_t r128 = r127 * 6;
  dt_roi_t roi0 = {.wd = 1829248, .ht = 1};
  dt_roi_t roi1 = {.wd = (uint32_t)(r128 / 2), .ht = 1};
  dt_roi_t roi2 = {.wd = (uint32_t)(r57), .ht = 1};
  dt_roi_t roi3 = {.wd = (uint32_t)(r95), .ht = 1};
  dt_roi_t roi4 = {.wd = (uint32_t)(r52), .ht = 1};
  dt_roi_t roi5 = {.wd = (uint32_t)(r47), .ht = 1};
  dt_roi_t roi6 = {.wd = (uint32_t)(r35), .ht = 1};
  dt_roi_t roi7 = {.wd = (uint32_t)(r62), .ht = 1};
  dt_roi_t roi8 = {.wd = (uint32_t)(r43), .ht = 1};
  dt_roi_t roi9 = {.wd = (uint32_t)(r43), .ht = 1};
  dt_roi_t roi10 = {.wd = (uint32_t)(r48), .ht = 1};
  dt_roi_t roi11 = {.wd = (uint32_t)(r48), .ht = 1};
  dt_roi_t roi12 = {.wd = (uint32_t)(r53), .ht = 1};
  dt_roi_t roi13 = {.wd = (uint32_t)(r53), .ht = 1};
  dt_roi_t roi14 = {.wd = (uint32_t)(r58), .ht = 1};
  dt_roi_t roi15 = {.wd = (uint32_t)(r58), .ht = 1};
  dt_roi_t roi16 = {.wd = (uint32_t)(r115), .ht = 1};
  dt_roi_t roi17 = {.wd = (uint32_t)(r95), .ht = 1};
  dt_roi_t roi18 = {.wd = (uint32_t)(r57), .ht = 1};
  dt_roi_t roi19 = {.wd = (uint32_t)(r128 / 2), .ht = 1};
  int weights_id = dt_node_add(graph, module, "oidn", "weights",
      1, 1, 1, 0, NULL, 1, 
      "w", "source", "ssbo", "u8", &roi0);
  // memory_pad (memory_pad.comp)
  const uint32_t memory_pad_pc[6] = {(uint32_t)(r8), (uint32_t)(r15), 0, (uint32_t)(r7), 0, (uint32_t)(r14)};
  const int memory_pad_id = dt_node_add(graph, module, "oidn", "nldr2",
      1 * DT_LOCAL_SIZE_X, r10 * DT_LOCAL_SIZE_Y, r112,
      24, (const int*)memory_pad_pc, 2, //
      "a", "read", "ssbo", "f16", &roi1, // HWC[3] : f16
      "b", "write", "ssbo", "u8", &roi2); // HWC[3] : f16
  // direct_conv_cm (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_pc[2] = {(uint32_t)(r8), (uint32_t)(r15)};
  const int direct_conv_cm_id = dt_node_add(graph, module, "oidn", "nldr7",
      1 * DT_LOCAL_SIZE_X, r126 * DT_LOCAL_SIZE_Y, r124,
      8, (const int*)direct_conv_cm_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi2, // HWC[3] : f16
      "d", "write", "ssbo", "u8", &roi3); // CHWC8[32] : f16
  // direct_conv_cm_1 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_1_pc[2] = {(uint32_t)(r8), (uint32_t)(r15)};
  const int direct_conv_cm_1_id = dt_node_add(graph, module, "oidn", "nldr9",
      1 * DT_LOCAL_SIZE_X, r126 * DT_LOCAL_SIZE_Y, r120,
      8, (const int*)direct_conv_cm_1_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi3, // CHWC8[32] : f16
      "d", "write", "ssbo", "u8", &roi4); // CHWC8[32] : f16
  // direct_conv_cm_2 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_2_pc[2] = {(uint32_t)(r27), (uint32_t)(r18)};
  const int direct_conv_cm_2_id = dt_node_add(graph, module, "oidn", "nldr3",
      1 * DT_LOCAL_SIZE_X, r109 * DT_LOCAL_SIZE_Y, r101,
      8, (const int*)direct_conv_cm_2_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi4, // CHWC8[32] : f16
      "d", "write", "ssbo", "u8", &roi5); // CHWC8[48] : f16
  // direct_conv_cm_3 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_3_pc[2] = {(uint32_t)(r30), (uint32_t)(r21)};
  const int direct_conv_cm_3_id = dt_node_add(graph, module, "oidn", "nldr8",
      1 * DT_LOCAL_SIZE_X, r93 * DT_LOCAL_SIZE_Y, r84,
      8, (const int*)direct_conv_cm_3_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi5, // CHWC8[48] : f16
      "d", "write", "ssbo", "u8", &roi6); // CHWC8[64] : f16
  // direct_conv_cm_4 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_4_pc[2] = {(uint32_t)(r33), (uint32_t)(r24)};
  const int direct_conv_cm_4_id = dt_node_add(graph, module, "oidn", "nldr10",
      1 * DT_LOCAL_SIZE_X, r82 * DT_LOCAL_SIZE_Y, r71,
      8, (const int*)direct_conv_cm_4_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi6, // CHWC8[64] : f16
      "d", "write", "ssbo", "u8", &roi7); // HWC[80] : f16
  // direct_conv_cm_5 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_5_pc[2] = {(uint32_t)(r41), (uint32_t)(r38)};
  const int direct_conv_cm_5_id = dt_node_add(graph, module, "oidn", "nldr11",
      1 * DT_LOCAL_SIZE_X, r69 * DT_LOCAL_SIZE_Y, r67,
      8, (const int*)direct_conv_cm_5_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi7, // HWC[80] : f16
      "d", "write", "ssbo", "u8", &roi8); // HWC[96] : f16
  // direct_conv_cm_6 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_6_pc[2] = {(uint32_t)(r41), (uint32_t)(r38)};
  const int direct_conv_cm_6_id = dt_node_add(graph, module, "oidn", "nldr16",
      1 * DT_LOCAL_SIZE_X, r69 * DT_LOCAL_SIZE_Y, r67,
      8, (const int*)direct_conv_cm_6_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi8, // HWC[96] : f16
      "d", "write", "ssbo", "u8", &roi9); // CHWC8[96] : f16
  // concat_conv_cm (concat_conv_cm.comp)
  const uint32_t concat_conv_cm_pc[2] = {(uint32_t)(r33), (uint32_t)(r24)};
  const int concat_conv_cm_id = dt_node_add(graph, module, "oidn", "nldr12",
      1 * DT_LOCAL_SIZE_X, r82 * DT_LOCAL_SIZE_Y, r76,
      8, (const int*)concat_conv_cm_pc, 6, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi0,// unknown
      "d", "read", "ssbo", "u8", &roi9, // CHWC8[96] : f16
      "e", "read", "ssbo", "u8", &roi6, // CHWC8[64] : f16
      "f", "write", "ssbo", "u8", &roi10); // CHWC8[112] : f16
  // direct_conv_cm_7 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_7_pc[2] = {(uint32_t)(r33), (uint32_t)(r24)};
  const int direct_conv_cm_7_id = dt_node_add(graph, module, "oidn", "nldr4",
      1 * DT_LOCAL_SIZE_X, r82 * DT_LOCAL_SIZE_Y, r78,
      8, (const int*)direct_conv_cm_7_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi10, // CHWC8[112] : f16
      "d", "write", "ssbo", "u8", &roi11); // CHWC8[112] : f16
  // concat_conv_cm_1 (concat_conv_cm.comp)
  const uint32_t concat_conv_cm_1_pc[2] = {(uint32_t)(r30), (uint32_t)(r21)};
  const int concat_conv_cm_1_id = dt_node_add(graph, module, "oidn", "nldr13",
      1 * DT_LOCAL_SIZE_X, r93 * DT_LOCAL_SIZE_Y, r89,
      8, (const int*)concat_conv_cm_1_pc, 6, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi0,// unknown
      "d", "read", "ssbo", "u8", &roi11, // CHWC8[112] : f16
      "e", "read", "ssbo", "u8", &roi5, // CHWC8[48] : f16
      "f", "write", "ssbo", "u8", &roi12); // HWC[96] : f16
  // direct_conv_cm_8 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_8_pc[2] = {(uint32_t)(r30), (uint32_t)(r21)};
  const int direct_conv_cm_8_id = dt_node_add(graph, module, "oidn", "nldr14",
      1 * DT_LOCAL_SIZE_X, r93 * DT_LOCAL_SIZE_Y, r89,
      8, (const int*)direct_conv_cm_8_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi12, // HWC[96] : f16
      "d", "write", "ssbo", "u8", &roi13); // HWC[96] : f16
  // concat_conv_cm_2 (concat_conv_cm.comp)
  const uint32_t concat_conv_cm_2_pc[2] = {(uint32_t)(r27), (uint32_t)(r18)};
  const int concat_conv_cm_2_id = dt_node_add(graph, module, "oidn", "nldr17",
      1 * DT_LOCAL_SIZE_X, r109 * DT_LOCAL_SIZE_Y, r105,
      8, (const int*)concat_conv_cm_2_pc, 6, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi0,// unknown
      "d", "read", "ssbo", "u8", &roi13, // HWC[96] : f16
      "e", "read", "ssbo", "u8", &roi4, // CHWC8[32] : f16
      "f", "write", "ssbo", "u8", &roi14); // CHWC8[64] : f16
  // direct_conv_cm_9 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_9_pc[2] = {(uint32_t)(r27), (uint32_t)(r18)};
  const int direct_conv_cm_9_id = dt_node_add(graph, module, "oidn", "nldr1",
      1 * DT_LOCAL_SIZE_X, r109 * DT_LOCAL_SIZE_Y, r103,
      8, (const int*)direct_conv_cm_9_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi14, // CHWC8[64] : f16
      "d", "write", "ssbo", "u8", &roi15); // CHWC8[64] : f16
  // concat_conv_cm_3 (concat_conv_cm.comp)
  const uint32_t concat_conv_cm_3_pc[2] = {(uint32_t)(r8), (uint32_t)(r15)};
  const int concat_conv_cm_3_id = dt_node_add(graph, module, "oidn", "nldr15",
      1 * DT_LOCAL_SIZE_X, r126 * DT_LOCAL_SIZE_Y, r120,
      8, (const int*)concat_conv_cm_3_pc, 6, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi0,// unknown
      "d", "read", "ssbo", "u8", &roi15, // CHWC8[64] : f16
      "e", "read", "ssbo", "u8", &roi2, // HWC[3] : f16
      "f", "write", "ssbo", "u8", &roi16); // CHWC8[64] : f16
  // direct_conv_cm_10 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_10_pc[2] = {(uint32_t)(r8), (uint32_t)(r15)};
  const int direct_conv_cm_10_id = dt_node_add(graph, module, "oidn", "nldr0",
      1 * DT_LOCAL_SIZE_X, r126 * DT_LOCAL_SIZE_Y, r118,
      8, (const int*)direct_conv_cm_10_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi16, // CHWC8[64] : f16
      "d", "write", "ssbo", "u8", &roi17); // CHWC8[32] : f16
  // direct_conv_cm_11 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_11_pc[2] = {(uint32_t)(r8), (uint32_t)(r15)};
  const int direct_conv_cm_11_id = dt_node_add(graph, module, "oidn", "nldr6",
      1 * DT_LOCAL_SIZE_X, r126 * DT_LOCAL_SIZE_Y, r120,
      8, (const int*)direct_conv_cm_11_pc, 3, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi17, // CHWC8[32] : f16
      "c", "write", "ssbo", "u8", &roi18); // HWC[3] : f16
  // memory_slice (memory_slice.comp)
  const uint32_t memory_slice_pc[6] = {0, 0, (uint32_t)(r8), (uint32_t)(r15), (uint32_t)(W), (uint32_t)(H)};
  const int memory_slice_id = dt_node_add(graph, module, "oidn", "nldr5",
      1 * DT_LOCAL_SIZE_X, r3 * DT_LOCAL_SIZE_Y, H,
      24, (const int*)memory_slice_pc, 2, //
      "a", "read", "ssbo", "u8", &roi18, // HWC[3] : f16
      "b", "write", "ssbo", "f16", &roi19); // HWC[3] : f16
  if (input_connector == NULL) {
    dt_connector_copy(graph, module, input_id, memory_pad_id, 0);
  } else {
    dt_node_connect_named(graph, input_id, input_connector, memory_pad_id, "a");
  }
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_id, "b");
  dt_node_connect_named(graph, memory_pad_id, "b", direct_conv_cm_id, "c");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_1_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_1_id, "b");
  dt_node_connect_named(graph, direct_conv_cm_id, "d", direct_conv_cm_1_id, "c");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_2_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_2_id, "b");
  dt_node_connect_named(graph, direct_conv_cm_1_id, "d", direct_conv_cm_2_id, "c");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_3_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_3_id, "b");
  dt_node_connect_named(graph, direct_conv_cm_2_id, "d", direct_conv_cm_3_id, "c");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_4_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_4_id, "b");
  dt_node_connect_named(graph, direct_conv_cm_3_id, "d", direct_conv_cm_4_id, "c");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_5_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_5_id, "b");
  dt_node_connect_named(graph, direct_conv_cm_4_id, "d", direct_conv_cm_5_id, "c");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_6_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_6_id, "b");
  dt_node_connect_named(graph, direct_conv_cm_5_id, "d", direct_conv_cm_6_id, "c");
  dt_node_connect_named(graph, weights_id, "w", concat_conv_cm_id, "a");
  dt_node_connect_named(graph, weights_id, "w", concat_conv_cm_id, "b");
  dt_node_connect_named(graph, weights_id, "w", concat_conv_cm_id, "c");
  dt_node_connect_named(graph, direct_conv_cm_6_id, "d", concat_conv_cm_id, "d");
  dt_node_connect_named(graph, direct_conv_cm_3_id, "d", concat_conv_cm_id, "e");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_7_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_7_id, "b");
  dt_node_connect_named(graph, concat_conv_cm_id, "f", direct_conv_cm_7_id, "c");
  dt_node_connect_named(graph, weights_id, "w", concat_conv_cm_1_id, "a");
  dt_node_connect_named(graph, weights_id, "w", concat_conv_cm_1_id, "b");
  dt_node_connect_named(graph, weights_id, "w", concat_conv_cm_1_id, "c");
  dt_node_connect_named(graph, direct_conv_cm_7_id, "d", concat_conv_cm_1_id, "d");
  dt_node_connect_named(graph, direct_conv_cm_2_id, "d", concat_conv_cm_1_id, "e");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_8_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_8_id, "b");
  dt_node_connect_named(graph, concat_conv_cm_1_id, "f", direct_conv_cm_8_id, "c");
  dt_node_connect_named(graph, weights_id, "w", concat_conv_cm_2_id, "a");
  dt_node_connect_named(graph, weights_id, "w", concat_conv_cm_2_id, "b");
  dt_node_connect_named(graph, weights_id, "w", concat_conv_cm_2_id, "c");
  dt_node_connect_named(graph, direct_conv_cm_8_id, "d", concat_conv_cm_2_id, "d");
  dt_node_connect_named(graph, direct_conv_cm_1_id, "d", concat_conv_cm_2_id, "e");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_9_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_9_id, "b");
  dt_node_connect_named(graph, concat_conv_cm_2_id, "f", direct_conv_cm_9_id, "c");
  dt_node_connect_named(graph, weights_id, "w", concat_conv_cm_3_id, "a");
  dt_node_connect_named(graph, weights_id, "w", concat_conv_cm_3_id, "b");
  dt_node_connect_named(graph, weights_id, "w", concat_conv_cm_3_id, "c");
  dt_node_connect_named(graph, direct_conv_cm_9_id, "d", concat_conv_cm_3_id, "d");
  dt_node_connect_named(graph, memory_pad_id, "b", concat_conv_cm_3_id, "e");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_10_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_10_id, "b");
  dt_node_connect_named(graph, concat_conv_cm_3_id, "f", direct_conv_cm_10_id, "c");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_11_id, "a");
  dt_node_connect_named(graph, direct_conv_cm_10_id, "d", direct_conv_cm_11_id, "b");
  dt_node_connect_named(graph, direct_conv_cm_11_id, "c", memory_slice_id, "a");
  if (output_connector == NULL) {
    dt_connector_copy(graph, module, output_id, memory_slice_id, 1);
  } else {
    dt_node_connect_named(graph, memory_slice_id, "b", output_id, output_connector);
  }
  graph->node[direct_conv_cm_id].connector[1].ssbo_offset = 1728;
  graph->node[direct_conv_cm_1_id].connector[0].ssbo_offset = 1792;
  graph->node[direct_conv_cm_1_id].connector[1].ssbo_offset = 20224;
  graph->node[direct_conv_cm_2_id].connector[0].ssbo_offset = 20288;
  graph->node[direct_conv_cm_2_id].connector[1].ssbo_offset = 47936;
  graph->node[direct_conv_cm_3_id].connector[0].ssbo_offset = 48032;
  graph->node[direct_conv_cm_3_id].connector[1].ssbo_offset = 103328;
  graph->node[direct_conv_cm_4_id].connector[0].ssbo_offset = 103456;
  graph->node[direct_conv_cm_4_id].connector[1].ssbo_offset = 195616;
  graph->node[direct_conv_cm_5_id].connector[0].ssbo_offset = 195776;
  graph->node[direct_conv_cm_5_id].connector[1].ssbo_offset = 334016;
  graph->node[direct_conv_cm_6_id].connector[0].ssbo_offset = 334208;
  graph->node[direct_conv_cm_6_id].connector[1].ssbo_offset = 500096;
  graph->node[concat_conv_cm_id].connector[0].ssbo_offset = 500288;
  graph->node[concat_conv_cm_id].connector[1].ssbo_offset = 693824;
  graph->node[concat_conv_cm_id].connector[2].ssbo_offset = 822848;
  graph->node[direct_conv_cm_7_id].connector[0].ssbo_offset = 823072;
  graph->node[direct_conv_cm_7_id].connector[1].ssbo_offset = 1048864;
  graph->node[concat_conv_cm_1_id].connector[0].ssbo_offset = 1049088;
  graph->node[concat_conv_cm_1_id].connector[1].ssbo_offset = 1242624;
  graph->node[concat_conv_cm_1_id].connector[2].ssbo_offset = 1325568;
  graph->node[direct_conv_cm_8_id].connector[0].ssbo_offset = 1325760;
  graph->node[direct_conv_cm_8_id].connector[1].ssbo_offset = 1491648;
  graph->node[concat_conv_cm_2_id].connector[0].ssbo_offset = 1491840;
  graph->node[concat_conv_cm_2_id].connector[1].ssbo_offset = 1602432;
  graph->node[concat_conv_cm_2_id].connector[2].ssbo_offset = 1639296;
  graph->node[direct_conv_cm_9_id].connector[0].ssbo_offset = 1639424;
  graph->node[direct_conv_cm_9_id].connector[1].ssbo_offset = 1713152;
  graph->node[concat_conv_cm_3_id].connector[0].ssbo_offset = 1713280;
  graph->node[concat_conv_cm_3_id].connector[1].ssbo_offset = 1787008;
  graph->node[concat_conv_cm_3_id].connector[2].ssbo_offset = 1790464;
  graph->node[direct_conv_cm_10_id].connector[0].ssbo_offset = 1790592;
  graph->node[direct_conv_cm_10_id].connector[1].ssbo_offset = 1827456;
  graph->node[direct_conv_cm_11_id].connector[0].ssbo_offset = 1827520;
}

#endif
