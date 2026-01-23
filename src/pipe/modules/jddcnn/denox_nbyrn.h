#ifndef jddcnn_nbyrn_DENOX_MODULE_H
#define jddcnn_nbyrn_DENOX_MODULE_H
#include "modules/api.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int denox_read_source_nbyrn(dt_module_t* mod, void* mapped, dt_read_source_params_t* p) {
  if (p->node->kernel == dt_token("weights")) {
    FILE* f = dt_graph_open_resource(mod->graph, 0, "data/jddcnn-nbyrn.dat", "rb");
    if (!f) {
      snprintf(mod->graph->gui_msg_buf, sizeof(mod->graph->gui_msg_buf),
            "jddcnn: could not find \"data/jddcnn-nbyrn.dat\"");
      return 1;
    }
    fseek(f, 0, SEEK_END);
    const size_t size = ftell(f);
    const size_t expected_size = 4308416;
    if (size != expected_size) {
      snprintf(mod->graph->gui_msg_buf, sizeof(mod->graph->gui_msg_buf),
            "jddcnn: weight file \"data/jddcnn-nbyrn.dat\" has unexpected size!");
      fclose(f);
      return 1;
    }
    fseek(f, 0, SEEK_SET);
    fread(mapped, size, 1, f);
    fclose(f);
  }
  return 0;
}

static void denox_create_nodes_nbyrn(dt_graph_t* graph, dt_module_t* module,
      uint64_t H, uint64_t W,
      int input_id, const char* input_connector,
      int output_id, const char* output_connector) {
  const int64_t r2 = W + 47;
  const int64_t r3 = r2 / 48;
  const int64_t r4 = ((H % 16) + 16) % 16;
  const int64_t r5 = r4 * -1;
  const int64_t r6 = r5 + 16;
  const int64_t r7 = ((r6 % 16) + 16) % 16;
  const int64_t r8 = H + r7;
  const int64_t r9 = r8 + 6;
  const int64_t r10 = r9 / 7;
  const int64_t r11 = ((W % 16) + 16) % 16;
  const int64_t r12 = r11 * -1;
  const int64_t r13 = r12 + 16;
  const int64_t r14 = ((r13 % 16) + 16) % 16;
  const int64_t r15 = W + r14;
  const int64_t r16 = r15 + 95;
  const int64_t r17 = r16 / 96;
  const int64_t r18 = r8 - 2;
  const int64_t r19 = r18 / 2;
  const int64_t r20 = r19 + 1;
  const int64_t r21 = r20 - 2;
  const int64_t r22 = r21 / 2;
  const int64_t r23 = r22 + 1;
  const int64_t r24 = r23 - 2;
  const int64_t r25 = r24 / 2;
  const int64_t r26 = r25 + 1;
  const int64_t r27 = r26 - 2;
  const int64_t r28 = r27 / 2;
  const int64_t r29 = r28 + 1;
  const int64_t r30 = r15 - 2;
  const int64_t r31 = r30 / 2;
  const int64_t r32 = r31 + 1;
  const int64_t r33 = r32 - 2;
  const int64_t r34 = r33 / 2;
  const int64_t r35 = r34 + 1;
  const int64_t r36 = r35 - 2;
  const int64_t r37 = r36 / 2;
  const int64_t r38 = r37 + 1;
  const int64_t r39 = r38 - 2;
  const int64_t r40 = r39 / 2;
  const int64_t r41 = r40 + 1;
  const int64_t r42 = r41 * r29;
  const int64_t r43 = r42 * 512;
  const int64_t r45 = r38 * r26;
  const int64_t r46 = r45 * 256;
  const int64_t r48 = r35 * r23;
  const int64_t r49 = r48 * 128;
  const int64_t r51 = r32 * r20;
  const int64_t r52 = r51 * 64;
  const int64_t r57 = r29 + 3;
  const int64_t r58 = r57 / 4;
  const int64_t r59 = r41 + 15;
  const int64_t r60 = r59 / 16;
  const int64_t r65 = r26 + 6;
  const int64_t r66 = r65 / 7;
  const int64_t r69 = r26 + 3;
  const int64_t r70 = r69 / 4;
  const int64_t r71 = r38 + 15;
  const int64_t r72 = r71 / 16;
  const int64_t r77 = r23 + 7;
  const int64_t r78 = r77 / 8;
  const int64_t r81 = r35 + 15;
  const int64_t r82 = r81 / 16;
  const int64_t r83 = r15 * r8;
  const int64_t r84 = r83 * 32;
  const int64_t r89 = r20 + 7;
  const int64_t r90 = r89 / 8;
  const int64_t r91 = r20 + 15;
  const int64_t r92 = r91 / 16;
  const int64_t r95 = r32 + 15;
  const int64_t r96 = r95 / 16;
  const int64_t r99 = r83 * 8;
  const int64_t r104 = r8 + 23;
  const int64_t r105 = r104 / 24;
  const int64_t r106 = r8 + 15;
  const int64_t r107 = r106 / 16;
  const int64_t r108 = r8 + 7;
  const int64_t r109 = r108 / 8;
  const int64_t r112 = r15 + 15;
  const int64_t r113 = r112 / 16;
  const int64_t r114 = H * W;
  const int64_t r115 = r114 * 8;
  const int64_t r116 = r83 * 24;
  const int64_t r117 = r114 * 24;
  dt_roi_t roi0 = {.wd = 4308416, .ht = 1};
  dt_roi_t roi1 = {.wd = (uint32_t)(r115 / 2), .ht = 1};
  dt_roi_t roi2 = {.wd = (uint32_t)(r99), .ht = 1};
  dt_roi_t roi3 = {.wd = (uint32_t)(r84), .ht = 1};
  dt_roi_t roi4 = {.wd = (uint32_t)(r52), .ht = 1};
  dt_roi_t roi5 = {.wd = (uint32_t)(r52), .ht = 1};
  dt_roi_t roi6 = {.wd = (uint32_t)(r49), .ht = 1};
  dt_roi_t roi7 = {.wd = (uint32_t)(r49), .ht = 1};
  dt_roi_t roi8 = {.wd = (uint32_t)(r46), .ht = 1};
  dt_roi_t roi9 = {.wd = (uint32_t)(r46), .ht = 1};
  dt_roi_t roi10 = {.wd = (uint32_t)(r43), .ht = 1};
  dt_roi_t roi11 = {.wd = (uint32_t)(r43), .ht = 1};
  dt_roi_t roi12 = {.wd = (uint32_t)(r46), .ht = 1};
  dt_roi_t roi13 = {.wd = (uint32_t)(r46), .ht = 1};
  dt_roi_t roi14 = {.wd = (uint32_t)(r46), .ht = 1};
  dt_roi_t roi15 = {.wd = (uint32_t)(r49), .ht = 1};
  dt_roi_t roi16 = {.wd = (uint32_t)(r49), .ht = 1};
  dt_roi_t roi17 = {.wd = (uint32_t)(r49), .ht = 1};
  dt_roi_t roi18 = {.wd = (uint32_t)(r52), .ht = 1};
  dt_roi_t roi19 = {.wd = (uint32_t)(r52), .ht = 1};
  dt_roi_t roi20 = {.wd = (uint32_t)(r52), .ht = 1};
  dt_roi_t roi21 = {.wd = (uint32_t)(r84), .ht = 1};
  dt_roi_t roi22 = {.wd = (uint32_t)(r84), .ht = 1};
  dt_roi_t roi23 = {.wd = (uint32_t)(r116), .ht = 1};
  dt_roi_t roi24 = {.wd = (uint32_t)(r117 / 2), .ht = 1};
  int weights_id = dt_node_add(graph, module, "jddcnn", "weights",
      1, 1, 1, 0, NULL, 1, 
      "w", "source", "ssbo", "u8", &roi0);
  // memory_pad (memory_pad.comp)
  const uint32_t memory_pad_pc[6] = {(uint32_t)(r15), (uint32_t)(r8), 0, (uint32_t)(r14), 0, (uint32_t)(r7)};
  const int memory_pad_id = dt_node_add(graph, module, "jddcnn", "nbyrn4",
      1 * DT_LOCAL_SIZE_X, r17 * DT_LOCAL_SIZE_Y, r10,
      24, (const int*)memory_pad_pc, 2, //
      "a", "read", "ssbo", "f16", &roi1, // HWC[4] : f16
      "b", "write", "ssbo", "u8", &roi2); // HWC[4] : f16
  // direct_conv_cm (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_pc[2] = {(uint32_t)(r15), (uint32_t)(r8)};
  const int direct_conv_cm_id = dt_node_add(graph, module, "jddcnn", "nbyrn7",
      1 * DT_LOCAL_SIZE_X, r113 * DT_LOCAL_SIZE_Y, r109,
      8, (const int*)direct_conv_cm_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi2, // HWC[4] : f16
      "d", "write", "ssbo", "u8", &roi3); // HWC[16] : f16
  // direct_conv_cm_1 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_1_pc[2] = {(uint32_t)(r15), (uint32_t)(r8)};
  const int direct_conv_cm_1_id = dt_node_add(graph, module, "jddcnn", "nbyrn5",
      1 * DT_LOCAL_SIZE_X, r113 * DT_LOCAL_SIZE_Y, r107,
      8, (const int*)direct_conv_cm_1_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi3, // HWC[16] : f16
      "d", "write", "ssbo", "u8", &roi4); // HWC[32] : f16
  // direct_conv_cm_2 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_2_pc[2] = {(uint32_t)(r32), (uint32_t)(r20)};
  const int direct_conv_cm_2_id = dt_node_add(graph, module, "jddcnn", "nbyrn17",
      1 * DT_LOCAL_SIZE_X, r96 * DT_LOCAL_SIZE_Y, r90,
      8, (const int*)direct_conv_cm_2_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi4, // HWC[32] : f16
      "d", "write", "ssbo", "u8", &roi5); // CHWC8[32] : f16
  // direct_conv_cm_3 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_3_pc[2] = {(uint32_t)(r32), (uint32_t)(r20)};
  const int direct_conv_cm_3_id = dt_node_add(graph, module, "jddcnn", "nbyrn9",
      1 * DT_LOCAL_SIZE_X, r96 * DT_LOCAL_SIZE_Y, r90,
      8, (const int*)direct_conv_cm_3_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi5, // CHWC8[32] : f16
      "d", "write", "ssbo", "u8", &roi6); // HWC[64] : f16
  // direct_conv_cm_4 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_4_pc[2] = {(uint32_t)(r35), (uint32_t)(r23)};
  const int direct_conv_cm_4_id = dt_node_add(graph, module, "jddcnn", "nbyrn10",
      1 * DT_LOCAL_SIZE_X, r82 * DT_LOCAL_SIZE_Y, r78,
      8, (const int*)direct_conv_cm_4_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi6, // HWC[64] : f16
      "d", "write", "ssbo", "u8", &roi7); // CHWC8[64] : f16
  // direct_conv_cm_5 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_5_pc[2] = {(uint32_t)(r35), (uint32_t)(r23)};
  const int direct_conv_cm_5_id = dt_node_add(graph, module, "jddcnn", "nbyrn12",
      1 * DT_LOCAL_SIZE_X, r82 * DT_LOCAL_SIZE_Y, r78,
      8, (const int*)direct_conv_cm_5_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi7, // CHWC8[64] : f16
      "d", "write", "ssbo", "u8", &roi8); // HWC[128] : f16
  // direct_conv_cm_6 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_6_pc[2] = {(uint32_t)(r38), (uint32_t)(r26)};
  const int direct_conv_cm_6_id = dt_node_add(graph, module, "jddcnn", "nbyrn11",
      1 * DT_LOCAL_SIZE_X, r72 * DT_LOCAL_SIZE_Y, r66,
      8, (const int*)direct_conv_cm_6_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi8, // HWC[128] : f16
      "d", "write", "ssbo", "u8", &roi9); // CHWC8[128] : f16
  // direct_conv_cm_7 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_7_pc[2] = {(uint32_t)(r38), (uint32_t)(r26)};
  const int direct_conv_cm_7_id = dt_node_add(graph, module, "jddcnn", "nbyrn13",
      1 * DT_LOCAL_SIZE_X, r72 * DT_LOCAL_SIZE_Y, r70,
      8, (const int*)direct_conv_cm_7_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi9, // CHWC8[128] : f16
      "d", "write", "ssbo", "u8", &roi10); // CHWC8[256] : f16
  // direct_conv_cm_8 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_8_pc[2] = {(uint32_t)(r41), (uint32_t)(r29)};
  const int direct_conv_cm_8_id = dt_node_add(graph, module, "jddcnn", "nbyrn8",
      1 * DT_LOCAL_SIZE_X, r60 * DT_LOCAL_SIZE_Y, r58,
      8, (const int*)direct_conv_cm_8_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi10, // CHWC8[256] : f16
      "d", "write", "ssbo", "u8", &roi11); // CHWC8[256] : f16
  // direct_conv_cm_9 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_9_pc[2] = {(uint32_t)(r38), (uint32_t)(r26)};
  const int direct_conv_cm_9_id = dt_node_add(graph, module, "jddcnn", "nbyrn15",
      1 * DT_LOCAL_SIZE_X, r72 * DT_LOCAL_SIZE_Y, r66,
      8, (const int*)direct_conv_cm_9_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi11, // CHWC8[256] : f16
      "d", "write", "ssbo", "u8", &roi12); // HWC[128] : f16
  // concat_conv_cm (concat_conv_cm.comp)
  const uint32_t concat_conv_cm_pc[2] = {(uint32_t)(r38), (uint32_t)(r26)};
  const int concat_conv_cm_id = dt_node_add(graph, module, "jddcnn", "nbyrn18",
      1 * DT_LOCAL_SIZE_X, r72 * DT_LOCAL_SIZE_Y, r70,
      8, (const int*)concat_conv_cm_pc, 6, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi0,// unknown
      "d", "read", "ssbo", "u8", &roi12, // HWC[128] : f16
      "e", "read", "ssbo", "u8", &roi8, // HWC[128] : f16
      "f", "write", "ssbo", "u8", &roi13); // HWC[128] : f16
  // direct_conv_cm_10 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_10_pc[2] = {(uint32_t)(r38), (uint32_t)(r26)};
  const int direct_conv_cm_10_id = dt_node_add(graph, module, "jddcnn", "nbyrn11",
      1 * DT_LOCAL_SIZE_X, r72 * DT_LOCAL_SIZE_Y, r66,
      8, (const int*)direct_conv_cm_10_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi13, // HWC[128] : f16
      "d", "write", "ssbo", "u8", &roi14); // CHWC8[128] : f16
  // direct_conv_cm_11 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_11_pc[2] = {(uint32_t)(r35), (uint32_t)(r23)};
  const int direct_conv_cm_11_id = dt_node_add(graph, module, "jddcnn", "nbyrn20",
      1 * DT_LOCAL_SIZE_X, r82 * DT_LOCAL_SIZE_Y, r78,
      8, (const int*)direct_conv_cm_11_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi14, // CHWC8[128] : f16
      "d", "write", "ssbo", "u8", &roi15); // CHWC8[64] : f16
  // concat_conv_cm_1 (concat_conv_cm.comp)
  const uint32_t concat_conv_cm_1_pc[2] = {(uint32_t)(r35), (uint32_t)(r23)};
  const int concat_conv_cm_1_id = dt_node_add(graph, module, "jddcnn", "nbyrn3",
      1 * DT_LOCAL_SIZE_X, r82 * DT_LOCAL_SIZE_Y, r78,
      8, (const int*)concat_conv_cm_1_pc, 6, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi0,// unknown
      "d", "read", "ssbo", "u8", &roi15, // CHWC8[64] : f16
      "e", "read", "ssbo", "u8", &roi6, // HWC[64] : f16
      "f", "write", "ssbo", "u8", &roi16); // CHWC8[64] : f16
  // direct_conv_cm_12 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_12_pc[2] = {(uint32_t)(r35), (uint32_t)(r23)};
  const int direct_conv_cm_12_id = dt_node_add(graph, module, "jddcnn", "nbyrn2",
      1 * DT_LOCAL_SIZE_X, r82 * DT_LOCAL_SIZE_Y, r78,
      8, (const int*)direct_conv_cm_12_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi16, // CHWC8[64] : f16
      "d", "write", "ssbo", "u8", &roi17); // CHWC8[64] : f16
  // direct_conv_cm_13 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_13_pc[2] = {(uint32_t)(r32), (uint32_t)(r20)};
  const int direct_conv_cm_13_id = dt_node_add(graph, module, "jddcnn", "nbyrn14",
      1 * DT_LOCAL_SIZE_X, r96 * DT_LOCAL_SIZE_Y, r92,
      8, (const int*)direct_conv_cm_13_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi17, // CHWC8[64] : f16
      "d", "write", "ssbo", "u8", &roi18); // CHWC8[32] : f16
  // concat_conv_cm_2 (concat_conv_cm.comp)
  const uint32_t concat_conv_cm_2_pc[2] = {(uint32_t)(r32), (uint32_t)(r20)};
  const int concat_conv_cm_2_id = dt_node_add(graph, module, "jddcnn", "nbyrn1",
      1 * DT_LOCAL_SIZE_X, r96 * DT_LOCAL_SIZE_Y, r90,
      8, (const int*)concat_conv_cm_2_pc, 6, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi0,// unknown
      "d", "read", "ssbo", "u8", &roi18, // CHWC8[32] : f16
      "e", "read", "ssbo", "u8", &roi4, // HWC[32] : f16
      "f", "write", "ssbo", "u8", &roi19); // HWC[32] : f16
  // direct_conv_cm_14 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_14_pc[2] = {(uint32_t)(r32), (uint32_t)(r20)};
  const int direct_conv_cm_14_id = dt_node_add(graph, module, "jddcnn", "nbyrn17",
      1 * DT_LOCAL_SIZE_X, r96 * DT_LOCAL_SIZE_Y, r90,
      8, (const int*)direct_conv_cm_14_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi19, // HWC[32] : f16
      "d", "write", "ssbo", "u8", &roi20); // CHWC8[32] : f16
  // direct_conv_cm_15 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_15_pc[2] = {(uint32_t)(r15), (uint32_t)(r8)};
  const int direct_conv_cm_15_id = dt_node_add(graph, module, "jddcnn", "nbyrn6",
      1 * DT_LOCAL_SIZE_X, r113 * DT_LOCAL_SIZE_Y, r105,
      8, (const int*)direct_conv_cm_15_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi20, // CHWC8[32] : f16
      "d", "write", "ssbo", "u8", &roi21); // HWC[16] : f16
  // concat_conv_cm_3 (concat_conv_cm.comp)
  const uint32_t concat_conv_cm_3_pc[2] = {(uint32_t)(r15), (uint32_t)(r8)};
  const int concat_conv_cm_3_id = dt_node_add(graph, module, "jddcnn", "nbyrn19",
      1 * DT_LOCAL_SIZE_X, r113 * DT_LOCAL_SIZE_Y, r109,
      8, (const int*)concat_conv_cm_3_pc, 6, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi0,// unknown
      "d", "read", "ssbo", "u8", &roi21, // HWC[16] : f16
      "e", "read", "ssbo", "u8", &roi2, // HWC[4] : f16
      "f", "write", "ssbo", "u8", &roi22); // HWC[16] : f16
  // direct_conv_cm_16 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_16_pc[2] = {(uint32_t)(r15), (uint32_t)(r8)};
  const int direct_conv_cm_16_id = dt_node_add(graph, module, "jddcnn", "nbyrn16",
      1 * DT_LOCAL_SIZE_X, r113 * DT_LOCAL_SIZE_Y, r107,
      8, (const int*)direct_conv_cm_16_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi22, // HWC[16] : f16
      "d", "write", "ssbo", "u8", &roi23); // HWC[12] : f16
  // memory_slice (memory_slice.comp)
  const uint32_t memory_slice_pc[6] = {0, 0, (uint32_t)(r15), (uint32_t)(r8), (uint32_t)(W), (uint32_t)(H)};
  const int memory_slice_id = dt_node_add(graph, module, "jddcnn", "nbyrn0",
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
  graph->node[direct_conv_cm_id].connector[1].ssbo_offset = 1152;
  graph->node[direct_conv_cm_1_id].connector[0].ssbo_offset = 1184;
  graph->node[direct_conv_cm_1_id].connector[1].ssbo_offset = 10400;
  graph->node[direct_conv_cm_2_id].connector[0].ssbo_offset = 10464;
  graph->node[direct_conv_cm_2_id].connector[1].ssbo_offset = 28896;
  graph->node[direct_conv_cm_3_id].connector[0].ssbo_offset = 28960;
  graph->node[direct_conv_cm_3_id].connector[1].ssbo_offset = 65824;
  graph->node[direct_conv_cm_4_id].connector[0].ssbo_offset = 65952;
  graph->node[direct_conv_cm_4_id].connector[1].ssbo_offset = 139680;
  graph->node[direct_conv_cm_5_id].connector[0].ssbo_offset = 139808;
  graph->node[direct_conv_cm_5_id].connector[1].ssbo_offset = 287264;
  graph->node[direct_conv_cm_6_id].connector[0].ssbo_offset = 287520;
  graph->node[direct_conv_cm_6_id].connector[1].ssbo_offset = 582432;
  graph->node[direct_conv_cm_7_id].connector[0].ssbo_offset = 582688;
  graph->node[direct_conv_cm_7_id].connector[1].ssbo_offset = 1172512;
  graph->node[direct_conv_cm_8_id].connector[0].ssbo_offset = 1173024;
  graph->node[direct_conv_cm_8_id].connector[1].ssbo_offset = 2352672;
  graph->node[direct_conv_cm_9_id].connector[0].ssbo_offset = 2353184;
  graph->node[direct_conv_cm_9_id].connector[1].ssbo_offset = 2943008;
  graph->node[concat_conv_cm_id].connector[0].ssbo_offset = 2943264;
  graph->node[concat_conv_cm_id].connector[1].ssbo_offset = 3238176;
  graph->node[concat_conv_cm_id].connector[2].ssbo_offset = 3533088;
  graph->node[direct_conv_cm_10_id].connector[0].ssbo_offset = 3533344;
  graph->node[direct_conv_cm_10_id].connector[1].ssbo_offset = 3828256;
  graph->node[direct_conv_cm_11_id].connector[0].ssbo_offset = 3828512;
  graph->node[direct_conv_cm_11_id].connector[1].ssbo_offset = 3975968;
  graph->node[concat_conv_cm_1_id].connector[0].ssbo_offset = 3976096;
  graph->node[concat_conv_cm_1_id].connector[1].ssbo_offset = 4049824;
  graph->node[concat_conv_cm_1_id].connector[2].ssbo_offset = 4123552;
  graph->node[direct_conv_cm_12_id].connector[0].ssbo_offset = 4123680;
  graph->node[direct_conv_cm_12_id].connector[1].ssbo_offset = 4197408;
  graph->node[direct_conv_cm_13_id].connector[0].ssbo_offset = 4197536;
  graph->node[direct_conv_cm_13_id].connector[1].ssbo_offset = 4234400;
  graph->node[concat_conv_cm_2_id].connector[0].ssbo_offset = 4234464;
  graph->node[concat_conv_cm_2_id].connector[1].ssbo_offset = 4252896;
  graph->node[concat_conv_cm_2_id].connector[2].ssbo_offset = 4271328;
  graph->node[direct_conv_cm_14_id].connector[0].ssbo_offset = 4271392;
  graph->node[direct_conv_cm_14_id].connector[1].ssbo_offset = 4289824;
  graph->node[direct_conv_cm_15_id].connector[0].ssbo_offset = 4289888;
  graph->node[direct_conv_cm_15_id].connector[1].ssbo_offset = 4299104;
  graph->node[concat_conv_cm_3_id].connector[0].ssbo_offset = 4299136;
  graph->node[concat_conv_cm_3_id].connector[1].ssbo_offset = 4303744;
  graph->node[concat_conv_cm_3_id].connector[2].ssbo_offset = 4304896;
  graph->node[direct_conv_cm_16_id].connector[0].ssbo_offset = 4304928;
  graph->node[direct_conv_cm_16_id].connector[1].ssbo_offset = 4308384;
}

#endif
