#ifndef jddcnn_nxtrn_DENOX_MODULE_H
#define jddcnn_nxtrn_DENOX_MODULE_H
#include "modules/api.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int denox_read_source_nxtrn(dt_module_t* mod, void* mapped, dt_read_source_params_t* p) {
  if (p->node->kernel == dt_token("weights")) {
    FILE* f = dt_graph_open_resource(mod->graph, 0, "data/jddcnn-nxtrn.dat", "rb");
    if (!f) {
      snprintf(mod->graph->gui_msg_buf, sizeof(mod->graph->gui_msg_buf),
            "jddcnn: could not find \"data/jddcnn-nxtrn.dat\"");
      return 1;
    }
    fseek(f, 0, SEEK_END);
    const size_t size = ftell(f);
    const size_t expected_size = 4329184;
    if (size != expected_size) {
      snprintf(mod->graph->gui_msg_buf, sizeof(mod->graph->gui_msg_buf),
            "jddcnn: weight file \"data/jddcnn-nxtrn.dat\" has unexpected size!");
      fclose(f);
      return 1;
    }
    fseek(f, 0, SEEK_SET);
    fread(mapped, size, 1, f);
    fclose(f);
  }
  return 0;
}

static void denox_create_nodes_nxtrn(dt_graph_t* graph, dt_module_t* module,
      uint64_t H, uint64_t W,
      int input_id, const char* input_connector,
      int output_id, const char* output_connector) {
  const int64_t r2 = W + 47;
  const int64_t r3 = r2 / 48;
  const int64_t r4 = ((W % 16) + 16) % 16;
  const int64_t r5 = r4 * -1;
  const int64_t r6 = r5 + 16;
  const int64_t r7 = ((r6 % 16) + 16) % 16;
  const int64_t r8 = W + r7;
  const int64_t r9 = r8 + 111;
  const int64_t r10 = r9 / 112;
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
  const int64_t r25 = r24 - 2;
  const int64_t r26 = r25 / 2;
  const int64_t r27 = r26 + 1;
  const int64_t r28 = r8 - 2;
  const int64_t r29 = r28 / 2;
  const int64_t r30 = r29 + 1;
  const int64_t r31 = r30 - 2;
  const int64_t r32 = r31 / 2;
  const int64_t r33 = r32 + 1;
  const int64_t r34 = r33 - 2;
  const int64_t r35 = r34 / 2;
  const int64_t r36 = r35 + 1;
  const int64_t r37 = r36 - 2;
  const int64_t r38 = r37 / 2;
  const int64_t r39 = r38 + 1;
  const int64_t r40 = r39 * r27;
  const int64_t r41 = r40 * 512;
  const int64_t r43 = r36 * r24;
  const int64_t r44 = r43 * 256;
  const int64_t r46 = r33 * r21;
  const int64_t r47 = r46 * 128;
  const int64_t r49 = r30 * r18;
  const int64_t r50 = r49 * 64;
  const int64_t r55 = r27 + 3;
  const int64_t r56 = r55 / 4;
  const int64_t r57 = r39 + 15;
  const int64_t r58 = r57 / 16;
  const int64_t r63 = r24 + 6;
  const int64_t r64 = r63 / 7;
  const int64_t r67 = r24 + 3;
  const int64_t r68 = r67 / 4;
  const int64_t r69 = r36 + 15;
  const int64_t r70 = r69 / 16;
  const int64_t r75 = r21 + 7;
  const int64_t r76 = r75 / 8;
  const int64_t r79 = r33 + 15;
  const int64_t r80 = r79 / 16;
  const int64_t r81 = r8 * r15;
  const int64_t r83 = r81 * 64;
  const int64_t r88 = r18 + 7;
  const int64_t r89 = r88 / 8;
  const int64_t r90 = r18 + 15;
  const int64_t r91 = r90 / 16;
  const int64_t r94 = r30 + 15;
  const int64_t r95 = r94 / 16;
  const int64_t r96 = r81 * 32;
  const int64_t r100 = r15 + 15;
  const int64_t r101 = r100 / 16;
  const int64_t r102 = r15 + 7;
  const int64_t r103 = r102 / 8;
  const int64_t r106 = r15 + 3;
  const int64_t r107 = r106 / 4;
  const int64_t r108 = r8 + 15;
  const int64_t r109 = r108 / 16;
  const int64_t r110 = H * W;
  const int64_t r111 = r110 * 32;
  const int64_t r112 = r81 * 24;
  const int64_t r113 = r110 * 24;
  dt_roi_t roi0 = {.wd = 4329184, .ht = 1};
  dt_roi_t roi1 = {.wd = (uint32_t)(r111 / 2), .ht = 1};
  dt_roi_t roi2 = {.wd = (uint32_t)(r96), .ht = 1};
  dt_roi_t roi3 = {.wd = (uint32_t)(r83), .ht = 1};
  dt_roi_t roi4 = {.wd = (uint32_t)(r50), .ht = 1};
  dt_roi_t roi5 = {.wd = (uint32_t)(r50), .ht = 1};
  dt_roi_t roi6 = {.wd = (uint32_t)(r47), .ht = 1};
  dt_roi_t roi7 = {.wd = (uint32_t)(r47), .ht = 1};
  dt_roi_t roi8 = {.wd = (uint32_t)(r44), .ht = 1};
  dt_roi_t roi9 = {.wd = (uint32_t)(r44), .ht = 1};
  dt_roi_t roi10 = {.wd = (uint32_t)(r41), .ht = 1};
  dt_roi_t roi11 = {.wd = (uint32_t)(r41), .ht = 1};
  dt_roi_t roi12 = {.wd = (uint32_t)(r44), .ht = 1};
  dt_roi_t roi13 = {.wd = (uint32_t)(r44), .ht = 1};
  dt_roi_t roi14 = {.wd = (uint32_t)(r44), .ht = 1};
  dt_roi_t roi15 = {.wd = (uint32_t)(r47), .ht = 1};
  dt_roi_t roi16 = {.wd = (uint32_t)(r47), .ht = 1};
  dt_roi_t roi17 = {.wd = (uint32_t)(r47), .ht = 1};
  dt_roi_t roi18 = {.wd = (uint32_t)(r50), .ht = 1};
  dt_roi_t roi19 = {.wd = (uint32_t)(r50), .ht = 1};
  dt_roi_t roi20 = {.wd = (uint32_t)(r50), .ht = 1};
  dt_roi_t roi21 = {.wd = (uint32_t)(r96), .ht = 1};
  dt_roi_t roi22 = {.wd = (uint32_t)(r96), .ht = 1};
  dt_roi_t roi23 = {.wd = (uint32_t)(r112), .ht = 1};
  dt_roi_t roi24 = {.wd = (uint32_t)(r113 / 2), .ht = 1};
  int weights_id = dt_node_add(graph, module, "jddcnn", "weights",
      1, 1, 1, 0, NULL, 1, 
      "w", "source", "ssbo", "u8", &roi0);
  // memory_pad (memory_pad.comp)
  const uint32_t memory_pad_pc[6] = {(uint32_t)(r8), (uint32_t)(r15), 0, (uint32_t)(r7), 0, (uint32_t)(r14)};
  const int memory_pad_id = dt_node_add(graph, module, "jddcnn", "nxtrn7",
      1 * DT_LOCAL_SIZE_X, r10 * DT_LOCAL_SIZE_Y, r15,
      24, (const int*)memory_pad_pc, 2, //
      "a", "read", "ssbo", "f16", &roi1, // HWC[16] : f16
      "b", "write", "ssbo", "u8", &roi2); // HWC[16] : f16
  // direct_conv_cm (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_pc[2] = {(uint32_t)(r8), (uint32_t)(r15)};
  const int direct_conv_cm_id = dt_node_add(graph, module, "jddcnn", "nxtrn9",
      1 * DT_LOCAL_SIZE_X, r109 * DT_LOCAL_SIZE_Y, r107,
      8, (const int*)direct_conv_cm_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi2, // HWC[16] : f16
      "d", "write", "ssbo", "u8", &roi3); // CHWC8[32] : f16
  // direct_conv_cm_1 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_1_pc[2] = {(uint32_t)(r8), (uint32_t)(r15)};
  const int direct_conv_cm_1_id = dt_node_add(graph, module, "jddcnn", "nxtrn10",
      1 * DT_LOCAL_SIZE_X, r109 * DT_LOCAL_SIZE_Y, r103,
      8, (const int*)direct_conv_cm_1_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi3, // CHWC8[32] : f16
      "d", "write", "ssbo", "u8", &roi4); // HWC[32] : f16
  // direct_conv_cm_2 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_2_pc[2] = {(uint32_t)(r30), (uint32_t)(r18)};
  const int direct_conv_cm_2_id = dt_node_add(graph, module, "jddcnn", "nxtrn19",
      1 * DT_LOCAL_SIZE_X, r95 * DT_LOCAL_SIZE_Y, r89,
      8, (const int*)direct_conv_cm_2_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi4, // HWC[32] : f16
      "d", "write", "ssbo", "u8", &roi5); // CHWC8[32] : f16
  // direct_conv_cm_3 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_3_pc[2] = {(uint32_t)(r30), (uint32_t)(r18)};
  const int direct_conv_cm_3_id = dt_node_add(graph, module, "jddcnn", "nxtrn11",
      1 * DT_LOCAL_SIZE_X, r95 * DT_LOCAL_SIZE_Y, r89,
      8, (const int*)direct_conv_cm_3_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi5, // CHWC8[32] : f16
      "d", "write", "ssbo", "u8", &roi6); // HWC[64] : f16
  // direct_conv_cm_4 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_4_pc[2] = {(uint32_t)(r33), (uint32_t)(r21)};
  const int direct_conv_cm_4_id = dt_node_add(graph, module, "jddcnn", "nxtrn13",
      1 * DT_LOCAL_SIZE_X, r80 * DT_LOCAL_SIZE_Y, r76,
      8, (const int*)direct_conv_cm_4_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi6, // HWC[64] : f16
      "d", "write", "ssbo", "u8", &roi7); // CHWC8[64] : f16
  // direct_conv_cm_5 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_5_pc[2] = {(uint32_t)(r33), (uint32_t)(r21)};
  const int direct_conv_cm_5_id = dt_node_add(graph, module, "jddcnn", "nxtrn15",
      1 * DT_LOCAL_SIZE_X, r80 * DT_LOCAL_SIZE_Y, r76,
      8, (const int*)direct_conv_cm_5_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi7, // CHWC8[64] : f16
      "d", "write", "ssbo", "u8", &roi8); // HWC[128] : f16
  // direct_conv_cm_6 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_6_pc[2] = {(uint32_t)(r36), (uint32_t)(r24)};
  const int direct_conv_cm_6_id = dt_node_add(graph, module, "jddcnn", "nxtrn14",
      1 * DT_LOCAL_SIZE_X, r70 * DT_LOCAL_SIZE_Y, r64,
      8, (const int*)direct_conv_cm_6_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi8, // HWC[128] : f16
      "d", "write", "ssbo", "u8", &roi9); // CHWC8[128] : f16
  // direct_conv_cm_7 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_7_pc[2] = {(uint32_t)(r36), (uint32_t)(r24)};
  const int direct_conv_cm_7_id = dt_node_add(graph, module, "jddcnn", "nxtrn16",
      1 * DT_LOCAL_SIZE_X, r70 * DT_LOCAL_SIZE_Y, r68,
      8, (const int*)direct_conv_cm_7_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi9, // CHWC8[128] : f16
      "d", "write", "ssbo", "u8", &roi10); // CHWC8[256] : f16
  // direct_conv_cm_8 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_8_pc[2] = {(uint32_t)(r39), (uint32_t)(r27)};
  const int direct_conv_cm_8_id = dt_node_add(graph, module, "jddcnn", "nxtrn12",
      1 * DT_LOCAL_SIZE_X, r58 * DT_LOCAL_SIZE_Y, r56,
      8, (const int*)direct_conv_cm_8_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi10, // CHWC8[256] : f16
      "d", "write", "ssbo", "u8", &roi11); // CHWC8[256] : f16
  // direct_conv_cm_9 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_9_pc[2] = {(uint32_t)(r36), (uint32_t)(r24)};
  const int direct_conv_cm_9_id = dt_node_add(graph, module, "jddcnn", "nxtrn18",
      1 * DT_LOCAL_SIZE_X, r70 * DT_LOCAL_SIZE_Y, r64,
      8, (const int*)direct_conv_cm_9_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi11, // CHWC8[256] : f16
      "d", "write", "ssbo", "u8", &roi12); // HWC[128] : f16
  // concat_conv_cm (concat_conv_cm.comp)
  const uint32_t concat_conv_cm_pc[2] = {(uint32_t)(r36), (uint32_t)(r24)};
  const int concat_conv_cm_id = dt_node_add(graph, module, "jddcnn", "nxtrn20",
      1 * DT_LOCAL_SIZE_X, r70 * DT_LOCAL_SIZE_Y, r68,
      8, (const int*)concat_conv_cm_pc, 6, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi0,// unknown
      "d", "read", "ssbo", "u8", &roi12, // HWC[128] : f16
      "e", "read", "ssbo", "u8", &roi8, // HWC[128] : f16
      "f", "write", "ssbo", "u8", &roi13); // HWC[128] : f16
  // direct_conv_cm_10 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_10_pc[2] = {(uint32_t)(r36), (uint32_t)(r24)};
  const int direct_conv_cm_10_id = dt_node_add(graph, module, "jddcnn", "nxtrn14",
      1 * DT_LOCAL_SIZE_X, r70 * DT_LOCAL_SIZE_Y, r64,
      8, (const int*)direct_conv_cm_10_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi13, // HWC[128] : f16
      "d", "write", "ssbo", "u8", &roi14); // CHWC8[128] : f16
  // direct_conv_cm_11 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_11_pc[2] = {(uint32_t)(r33), (uint32_t)(r21)};
  const int direct_conv_cm_11_id = dt_node_add(graph, module, "jddcnn", "nxtrn8",
      1 * DT_LOCAL_SIZE_X, r80 * DT_LOCAL_SIZE_Y, r76,
      8, (const int*)direct_conv_cm_11_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi14, // CHWC8[128] : f16
      "d", "write", "ssbo", "u8", &roi15); // CHWC8[64] : f16
  // concat_conv_cm_1 (concat_conv_cm.comp)
  const uint32_t concat_conv_cm_1_pc[2] = {(uint32_t)(r33), (uint32_t)(r21)};
  const int concat_conv_cm_1_id = dt_node_add(graph, module, "jddcnn", "nxtrn5",
      1 * DT_LOCAL_SIZE_X, r80 * DT_LOCAL_SIZE_Y, r76,
      8, (const int*)concat_conv_cm_1_pc, 6, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi0,// unknown
      "d", "read", "ssbo", "u8", &roi15, // CHWC8[64] : f16
      "e", "read", "ssbo", "u8", &roi6, // HWC[64] : f16
      "f", "write", "ssbo", "u8", &roi16); // CHWC8[64] : f16
  // direct_conv_cm_12 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_12_pc[2] = {(uint32_t)(r33), (uint32_t)(r21)};
  const int direct_conv_cm_12_id = dt_node_add(graph, module, "jddcnn", "nxtrn4",
      1 * DT_LOCAL_SIZE_X, r80 * DT_LOCAL_SIZE_Y, r76,
      8, (const int*)direct_conv_cm_12_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi16, // CHWC8[64] : f16
      "d", "write", "ssbo", "u8", &roi17); // CHWC8[64] : f16
  // direct_conv_cm_13 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_13_pc[2] = {(uint32_t)(r30), (uint32_t)(r18)};
  const int direct_conv_cm_13_id = dt_node_add(graph, module, "jddcnn", "nxtrn17",
      1 * DT_LOCAL_SIZE_X, r95 * DT_LOCAL_SIZE_Y, r91,
      8, (const int*)direct_conv_cm_13_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi17, // CHWC8[64] : f16
      "d", "write", "ssbo", "u8", &roi18); // CHWC8[32] : f16
  // concat_conv_cm_2 (concat_conv_cm.comp)
  const uint32_t concat_conv_cm_2_pc[2] = {(uint32_t)(r30), (uint32_t)(r18)};
  const int concat_conv_cm_2_id = dt_node_add(graph, module, "jddcnn", "nxtrn3",
      1 * DT_LOCAL_SIZE_X, r95 * DT_LOCAL_SIZE_Y, r89,
      8, (const int*)concat_conv_cm_2_pc, 6, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi0,// unknown
      "d", "read", "ssbo", "u8", &roi18, // CHWC8[32] : f16
      "e", "read", "ssbo", "u8", &roi4, // HWC[32] : f16
      "f", "write", "ssbo", "u8", &roi19); // HWC[32] : f16
  // direct_conv_cm_14 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_14_pc[2] = {(uint32_t)(r30), (uint32_t)(r18)};
  const int direct_conv_cm_14_id = dt_node_add(graph, module, "jddcnn", "nxtrn19",
      1 * DT_LOCAL_SIZE_X, r95 * DT_LOCAL_SIZE_Y, r89,
      8, (const int*)direct_conv_cm_14_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi19, // HWC[32] : f16
      "d", "write", "ssbo", "u8", &roi20); // CHWC8[32] : f16
  // direct_conv_cm_15 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_15_pc[2] = {(uint32_t)(r8), (uint32_t)(r15)};
  const int direct_conv_cm_15_id = dt_node_add(graph, module, "jddcnn", "nxtrn2",
      1 * DT_LOCAL_SIZE_X, r109 * DT_LOCAL_SIZE_Y, r101,
      8, (const int*)direct_conv_cm_15_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi20, // CHWC8[32] : f16
      "d", "write", "ssbo", "u8", &roi21); // CHWC8[16] : f16
  // concat_conv_cm_3 (concat_conv_cm.comp)
  const uint32_t concat_conv_cm_3_pc[2] = {(uint32_t)(r8), (uint32_t)(r15)};
  const int concat_conv_cm_3_id = dt_node_add(graph, module, "jddcnn", "nxtrn1",
      1 * DT_LOCAL_SIZE_X, r109 * DT_LOCAL_SIZE_Y, r103,
      8, (const int*)concat_conv_cm_3_pc, 6, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi0,// unknown
      "d", "read", "ssbo", "u8", &roi21, // CHWC8[16] : f16
      "e", "read", "ssbo", "u8", &roi2, // HWC[16] : f16
      "f", "write", "ssbo", "u8", &roi22); // CHWC8[16] : f16
  // direct_conv_cm_16 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_16_pc[2] = {(uint32_t)(r8), (uint32_t)(r15)};
  const int direct_conv_cm_16_id = dt_node_add(graph, module, "jddcnn", "nxtrn0",
      1 * DT_LOCAL_SIZE_X, r109 * DT_LOCAL_SIZE_Y, r101,
      8, (const int*)direct_conv_cm_16_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi22, // CHWC8[16] : f16
      "d", "write", "ssbo", "u8", &roi23); // HWC[12] : f16
  // memory_slice (memory_slice.comp)
  const uint32_t memory_slice_pc[6] = {0, 0, (uint32_t)(r8), (uint32_t)(r15), (uint32_t)(W), (uint32_t)(H)};
  const int memory_slice_id = dt_node_add(graph, module, "jddcnn", "nxtrn6",
      1 * DT_LOCAL_SIZE_X, r3 * DT_LOCAL_SIZE_Y, H,
      24, (const int*)memory_slice_pc, 2, //
      "a", "write", "ssbo", "f16", &roi24, // HWC[12] : f16
      "b", "read", "ssbo", "u8", &roi23); // HWC[12] : f16
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
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_7_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_7_id, "b");
  dt_node_connect_named(graph, direct_conv_cm_6_id, "d", direct_conv_cm_7_id, "c");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_8_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_8_id, "b");
  dt_node_connect_named(graph, direct_conv_cm_7_id, "d", direct_conv_cm_8_id, "c");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_9_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_9_id, "b");
  dt_node_connect_named(graph, direct_conv_cm_8_id, "d", direct_conv_cm_9_id, "c");
  dt_node_connect_named(graph, weights_id, "w", concat_conv_cm_id, "a");
  dt_node_connect_named(graph, weights_id, "w", concat_conv_cm_id, "b");
  dt_node_connect_named(graph, weights_id, "w", concat_conv_cm_id, "c");
  dt_node_connect_named(graph, direct_conv_cm_9_id, "d", concat_conv_cm_id, "d");
  dt_node_connect_named(graph, direct_conv_cm_5_id, "d", concat_conv_cm_id, "e");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_10_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_10_id, "b");
  dt_node_connect_named(graph, concat_conv_cm_id, "f", direct_conv_cm_10_id, "c");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_11_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_11_id, "b");
  dt_node_connect_named(graph, direct_conv_cm_10_id, "d", direct_conv_cm_11_id, "c");
  dt_node_connect_named(graph, weights_id, "w", concat_conv_cm_1_id, "a");
  dt_node_connect_named(graph, weights_id, "w", concat_conv_cm_1_id, "b");
  dt_node_connect_named(graph, weights_id, "w", concat_conv_cm_1_id, "c");
  dt_node_connect_named(graph, direct_conv_cm_11_id, "d", concat_conv_cm_1_id, "d");
  dt_node_connect_named(graph, direct_conv_cm_3_id, "d", concat_conv_cm_1_id, "e");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_12_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_12_id, "b");
  dt_node_connect_named(graph, concat_conv_cm_1_id, "f", direct_conv_cm_12_id, "c");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_13_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_13_id, "b");
  dt_node_connect_named(graph, direct_conv_cm_12_id, "d", direct_conv_cm_13_id, "c");
  dt_node_connect_named(graph, weights_id, "w", concat_conv_cm_2_id, "a");
  dt_node_connect_named(graph, weights_id, "w", concat_conv_cm_2_id, "b");
  dt_node_connect_named(graph, weights_id, "w", concat_conv_cm_2_id, "c");
  dt_node_connect_named(graph, direct_conv_cm_13_id, "d", concat_conv_cm_2_id, "d");
  dt_node_connect_named(graph, direct_conv_cm_1_id, "d", concat_conv_cm_2_id, "e");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_14_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_14_id, "b");
  dt_node_connect_named(graph, concat_conv_cm_2_id, "f", direct_conv_cm_14_id, "c");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_15_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_15_id, "b");
  dt_node_connect_named(graph, direct_conv_cm_14_id, "d", direct_conv_cm_15_id, "c");
  dt_node_connect_named(graph, weights_id, "w", concat_conv_cm_3_id, "a");
  dt_node_connect_named(graph, weights_id, "w", concat_conv_cm_3_id, "b");
  dt_node_connect_named(graph, weights_id, "w", concat_conv_cm_3_id, "c");
  dt_node_connect_named(graph, direct_conv_cm_15_id, "d", concat_conv_cm_3_id, "d");
  dt_node_connect_named(graph, memory_pad_id, "b", concat_conv_cm_3_id, "e");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_16_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_16_id, "b");
  dt_node_connect_named(graph, concat_conv_cm_3_id, "f", direct_conv_cm_16_id, "c");
  dt_node_connect_named(graph, direct_conv_cm_16_id, "d", memory_slice_id, "b");
  if (output_connector == NULL) {
    dt_connector_copy(graph, module, output_id, memory_slice_id, 0);
  } else {
    dt_node_connect_named(graph, memory_slice_id, "a", output_id, output_connector);
  }
  graph->node[direct_conv_cm_id].connector[1].ssbo_offset = 9216;
  graph->node[direct_conv_cm_1_id].connector[0].ssbo_offset = 9280;
  graph->node[direct_conv_cm_1_id].connector[1].ssbo_offset = 27712;
  graph->node[direct_conv_cm_2_id].connector[0].ssbo_offset = 27776;
  graph->node[direct_conv_cm_2_id].connector[1].ssbo_offset = 46208;
  graph->node[direct_conv_cm_3_id].connector[0].ssbo_offset = 46272;
  graph->node[direct_conv_cm_3_id].connector[1].ssbo_offset = 83136;
  graph->node[direct_conv_cm_4_id].connector[0].ssbo_offset = 83264;
  graph->node[direct_conv_cm_4_id].connector[1].ssbo_offset = 156992;
  graph->node[direct_conv_cm_5_id].connector[0].ssbo_offset = 157120;
  graph->node[direct_conv_cm_5_id].connector[1].ssbo_offset = 304576;
  graph->node[direct_conv_cm_6_id].connector[0].ssbo_offset = 304832;
  graph->node[direct_conv_cm_6_id].connector[1].ssbo_offset = 599744;
  graph->node[direct_conv_cm_7_id].connector[0].ssbo_offset = 600000;
  graph->node[direct_conv_cm_7_id].connector[1].ssbo_offset = 1189824;
  graph->node[direct_conv_cm_8_id].connector[0].ssbo_offset = 1190336;
  graph->node[direct_conv_cm_8_id].connector[1].ssbo_offset = 2369984;
  graph->node[direct_conv_cm_9_id].connector[0].ssbo_offset = 2370496;
  graph->node[direct_conv_cm_9_id].connector[1].ssbo_offset = 2960320;
  graph->node[concat_conv_cm_id].connector[0].ssbo_offset = 2960576;
  graph->node[concat_conv_cm_id].connector[1].ssbo_offset = 3255488;
  graph->node[concat_conv_cm_id].connector[2].ssbo_offset = 3550400;
  graph->node[direct_conv_cm_10_id].connector[0].ssbo_offset = 3550656;
  graph->node[direct_conv_cm_10_id].connector[1].ssbo_offset = 3845568;
  graph->node[direct_conv_cm_11_id].connector[0].ssbo_offset = 3845824;
  graph->node[direct_conv_cm_11_id].connector[1].ssbo_offset = 3993280;
  graph->node[concat_conv_cm_1_id].connector[0].ssbo_offset = 3993408;
  graph->node[concat_conv_cm_1_id].connector[1].ssbo_offset = 4067136;
  graph->node[concat_conv_cm_1_id].connector[2].ssbo_offset = 4140864;
  graph->node[direct_conv_cm_12_id].connector[0].ssbo_offset = 4140992;
  graph->node[direct_conv_cm_12_id].connector[1].ssbo_offset = 4214720;
  graph->node[direct_conv_cm_13_id].connector[0].ssbo_offset = 4214848;
  graph->node[direct_conv_cm_13_id].connector[1].ssbo_offset = 4251712;
  graph->node[concat_conv_cm_2_id].connector[0].ssbo_offset = 4251776;
  graph->node[concat_conv_cm_2_id].connector[1].ssbo_offset = 4270208;
  graph->node[concat_conv_cm_2_id].connector[2].ssbo_offset = 4288640;
  graph->node[direct_conv_cm_14_id].connector[0].ssbo_offset = 4288704;
  graph->node[direct_conv_cm_14_id].connector[1].ssbo_offset = 4307136;
  graph->node[direct_conv_cm_15_id].connector[0].ssbo_offset = 4307200;
  graph->node[direct_conv_cm_15_id].connector[1].ssbo_offset = 4316416;
  graph->node[concat_conv_cm_3_id].connector[0].ssbo_offset = 4316448;
  graph->node[concat_conv_cm_3_id].connector[1].ssbo_offset = 4321056;
  graph->node[concat_conv_cm_3_id].connector[2].ssbo_offset = 4325664;
  graph->node[direct_conv_cm_16_id].connector[0].ssbo_offset = 4325696;
  graph->node[direct_conv_cm_16_id].connector[1].ssbo_offset = 4329152;
}

#endif
