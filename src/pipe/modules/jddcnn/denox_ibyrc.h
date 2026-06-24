#ifndef jddcnn_ibyrc_DENOX_MODULE_H
#define jddcnn_ibyrc_DENOX_MODULE_H
#include "modules/api.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int denox_read_source_ibyrc(dt_module_t* mod, void* mapped, dt_read_source_params_t* p) {
  if (p->node->kernel == dt_token("weights")) {
    FILE* f = dt_graph_open_resource(mod->graph, 0, "data/jddcnn-ibyrc.dat", "rb");
    if (!f) {
      snprintf(mod->graph->gui_msg_buf, sizeof(mod->graph->gui_msg_buf),
            "jddcnn: could not find \"data/jddcnn-ibyrc.dat\"");
      return 1;
    }
    fseek(f, 0, SEEK_END);
    const size_t size = ftell(f);
    const size_t expected_size = 4308416;
    if (size != expected_size) {
      snprintf(mod->graph->gui_msg_buf, sizeof(mod->graph->gui_msg_buf),
            "jddcnn: weight file \"data/jddcnn-ibyrc.dat\" has unexpected size!");
      fclose(f);
      return 1;
    }
    fseek(f, 0, SEEK_SET);
    fread(mapped, size, 1, f);
    fclose(f);
  }
  return 0;
}

static void denox_create_nodes_ibyrc(dt_graph_t* graph, dt_module_t* module,
      uint64_t H, uint64_t W,
      int input_id, const char* input_connector,
      int output_id, const char* output_connector) {
  const int64_t r2 = W + 7;
  const int64_t r3 = r2 / 8;
  const int64_t r4 = ((W % 16) + 16) % 16;
  const int64_t r5 = r4 * -1;
  const int64_t r6 = r5 + 16;
  const int64_t r7 = ((r6 % 16) + 16) % 16;
  const int64_t r8 = W + r7;
  const int64_t r9 = r8 + 15;
  const int64_t r10 = r9 / 16;
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
  const int64_t r53 = r24 + 7;
  const int64_t r54 = r53 / 8;
  const int64_t r57 = r27 + 7;
  const int64_t r58 = r57 / 8;
  const int64_t r59 = r39 + 7;
  const int64_t r60 = r59 / 8;
  const int64_t r67 = r24 + 15;
  const int64_t r68 = r67 / 16;
  const int64_t r69 = r36 + 7;
  const int64_t r70 = r69 / 8;
  const int64_t r75 = r21 + 15;
  const int64_t r76 = r75 / 16;
  const int64_t r79 = r21 + 7;
  const int64_t r80 = r79 / 8;
  const int64_t r81 = r33 + 7;
  const int64_t r82 = r81 / 8;
  const int64_t r83 = r8 * r15;
  const int64_t r84 = r83 * 32;
  const int64_t r86 = r15 + 3;
  const int64_t r87 = r86 / 4;
  const int64_t r89 = r49 * 128;
  const int64_t r91 = r18 + 15;
  const int64_t r92 = r91 / 16;
  const int64_t r95 = r18 + 3;
  const int64_t r96 = r95 / 4;
  const int64_t r97 = r30 + 7;
  const int64_t r98 = r97 / 8;
  const int64_t r101 = r83 * 8;
  const int64_t r106 = r15 + 15;
  const int64_t r107 = r106 / 16;
  const int64_t r110 = r15 + 7;
  const int64_t r111 = r110 / 8;
  const int64_t r112 = r8 + 7;
  const int64_t r113 = r112 / 8;
  const int64_t r114 = H * W;
  const int64_t r115 = r114 * 8;
  const int64_t r116 = r83 * 24;
  const int64_t r117 = r114 * 24;
  dt_roi_t roi0 = {.wd = 4308416, .ht = 1};
  dt_roi_t roi1 = {.wd = (uint32_t)(r115 / 2), .ht = 1};
  dt_roi_t roi2 = {.wd = (uint32_t)(r101), .ht = 1};
  dt_roi_t roi3 = {.wd = (uint32_t)(r84), .ht = 1};
  dt_roi_t roi4 = {.wd = (uint32_t)(r89), .ht = 1};
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
  dt_roi_t roi18 = {.wd = 1, .ht = 1};
  dt_roi_t roi19 = {.wd = (uint32_t)(r50), .ht = 1};
  dt_roi_t roi20 = {.wd = (uint32_t)(r50), .ht = 1};
  dt_roi_t roi21 = {.wd = (uint32_t)(r84), .ht = 1};
  dt_roi_t roi22 = {.wd = (uint32_t)(r84), .ht = 1};
  dt_roi_t roi23 = {.wd = (uint32_t)(r116), .ht = 1};
  dt_roi_t roi24 = {.wd = (uint32_t)(r117 / 2), .ht = 1};
  int weights_id = dt_node_add(graph, module, "jddcnn", "weights",
      1, 1, 1, 0, NULL, 1, 
      "w", "source", "ssbo", "u8", &roi0);
  // memory_pad (memory_pad.comp)
  const uint32_t memory_pad_pc[6] = {(uint32_t)(r8), (uint32_t)(r15), 0, (uint32_t)(r7), 0, (uint32_t)(r14)};
  const int memory_pad_id = dt_node_add(graph, module, "jddcnn", "ibyrc6",
      1 * DT_LOCAL_SIZE_X, r10 * DT_LOCAL_SIZE_Y, r87,
      24, (const int*)memory_pad_pc, 2, //
      "a", "read", "ssbo", "f16", &roi1, // HWC[4] : f16
      "b", "write", "ssbo", "u8", &roi2); // HWC[4] : f16
  // direct_conv_cm (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_pc[2] = {(uint32_t)(r8), (uint32_t)(r15)};
  const int direct_conv_cm_id = dt_node_add(graph, module, "jddcnn", "ibyrc7",
      1 * DT_LOCAL_SIZE_X, r113 * DT_LOCAL_SIZE_Y, r111,
      8, (const int*)direct_conv_cm_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi2, // HWC[4] : f16
      "d", "write", "ssbo", "u8", &roi3); // CHWC8[16] : f16
  // direct_conv_cm_1 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_1_pc[2] = {(uint32_t)(r8), (uint32_t)(r15)};
  const int direct_conv_cm_1_id = dt_node_add(graph, module, "jddcnn", "ibyrc9",
      1 * DT_LOCAL_SIZE_X, r113 * DT_LOCAL_SIZE_Y, r107,
      8, (const int*)direct_conv_cm_1_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi3, // CHWC8[16] : f16
      "d", "write", "ssbo", "u8", &roi4); // CHWC8[32] : f16
  // direct_conv_cm_2 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_2_pc[2] = {(uint32_t)(r30), (uint32_t)(r18)};
  const int direct_conv_cm_2_id = dt_node_add(graph, module, "jddcnn", "ibyrc10",
      1 * DT_LOCAL_SIZE_X, r98 * DT_LOCAL_SIZE_Y, r96,
      8, (const int*)direct_conv_cm_2_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi4, // CHWC8[32] : f16
      "d", "write", "ssbo", "u8", &roi5); // HWC[32] : f16
  // direct_conv_cm_3 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_3_pc[2] = {(uint32_t)(r30), (uint32_t)(r18)};
  const int direct_conv_cm_3_id = dt_node_add(graph, module, "jddcnn", "ibyrc8",
      1 * DT_LOCAL_SIZE_X, r98 * DT_LOCAL_SIZE_Y, r92,
      8, (const int*)direct_conv_cm_3_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi5, // HWC[32] : f16
      "d", "write", "ssbo", "u8", &roi6); // CHWC8[64] : f16
  // direct_conv_cm_4 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_4_pc[2] = {(uint32_t)(r33), (uint32_t)(r21)};
  const int direct_conv_cm_4_id = dt_node_add(graph, module, "jddcnn", "ibyrc12",
      1 * DT_LOCAL_SIZE_X, r82 * DT_LOCAL_SIZE_Y, r80,
      8, (const int*)direct_conv_cm_4_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi6, // CHWC8[64] : f16
      "d", "write", "ssbo", "u8", &roi7); // HWC[64] : f16
  // direct_conv_cm_5 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_5_pc[2] = {(uint32_t)(r33), (uint32_t)(r21)};
  const int direct_conv_cm_5_id = dt_node_add(graph, module, "jddcnn", "ibyrc14",
      1 * DT_LOCAL_SIZE_X, r82 * DT_LOCAL_SIZE_Y, r76,
      8, (const int*)direct_conv_cm_5_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi7, // HWC[64] : f16
      "d", "write", "ssbo", "u8", &roi8); // HWC[128] : f16
  // direct_conv_cm_6 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_6_pc[2] = {(uint32_t)(r36), (uint32_t)(r24)};
  const int direct_conv_cm_6_id = dt_node_add(graph, module, "jddcnn", "ibyrc17",
      1 * DT_LOCAL_SIZE_X, r70 * DT_LOCAL_SIZE_Y, r68,
      8, (const int*)direct_conv_cm_6_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi8, // HWC[128] : f16
      "d", "write", "ssbo", "u8", &roi9); // CHWC8[128] : f16
  // direct_conv_cm_7 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_7_pc[2] = {(uint32_t)(r36), (uint32_t)(r24)};
  const int direct_conv_cm_7_id = dt_node_add(graph, module, "jddcnn", "ibyrc11",
      1 * DT_LOCAL_SIZE_X, r70 * DT_LOCAL_SIZE_Y, r54,
      8, (const int*)direct_conv_cm_7_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi9, // CHWC8[128] : f16
      "d", "write", "ssbo", "u8", &roi10); // HWC[256] : f16
  // direct_conv_cm_8 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_8_pc[2] = {(uint32_t)(r39), (uint32_t)(r27)};
  const int direct_conv_cm_8_id = dt_node_add(graph, module, "jddcnn", "ibyrc18",
      1 * DT_LOCAL_SIZE_X, r60 * DT_LOCAL_SIZE_Y, r58,
      8, (const int*)direct_conv_cm_8_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi10, // HWC[256] : f16
      "d", "write", "ssbo", "u8", &roi11); // CHWC8[256] : f16
  // direct_conv_cm_9 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_9_pc[2] = {(uint32_t)(r36), (uint32_t)(r24)};
  const int direct_conv_cm_9_id = dt_node_add(graph, module, "jddcnn", "ibyrc20",
      1 * DT_LOCAL_SIZE_X, r70 * DT_LOCAL_SIZE_Y, r68,
      8, (const int*)direct_conv_cm_9_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi11, // CHWC8[256] : f16
      "d", "write", "ssbo", "u8", &roi12); // HWC[128] : f16
  // concat_conv_cm (concat_conv_cm.comp)
  const uint32_t concat_conv_cm_pc[2] = {(uint32_t)(r36), (uint32_t)(r24)};
  const int concat_conv_cm_id = dt_node_add(graph, module, "jddcnn", "ibyrc16",
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
  const int direct_conv_cm_10_id = dt_node_add(graph, module, "jddcnn", "ibyrc17",
      1 * DT_LOCAL_SIZE_X, r70 * DT_LOCAL_SIZE_Y, r68,
      8, (const int*)direct_conv_cm_10_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi13, // HWC[128] : f16
      "d", "write", "ssbo", "u8", &roi14); // CHWC8[128] : f16
  // direct_conv_cm_11 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_11_pc[2] = {(uint32_t)(r33), (uint32_t)(r21)};
  const int direct_conv_cm_11_id = dt_node_add(graph, module, "jddcnn", "ibyrc19",
      1 * DT_LOCAL_SIZE_X, r82 * DT_LOCAL_SIZE_Y, r80,
      8, (const int*)direct_conv_cm_11_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi14, // CHWC8[128] : f16
      "d", "write", "ssbo", "u8", &roi15); // CHWC8[64] : f16
  // concat_conv_cm_1 (concat_conv_cm.comp)
  const uint32_t concat_conv_cm_1_pc[2] = {(uint32_t)(r33), (uint32_t)(r21)};
  const int concat_conv_cm_1_id = dt_node_add(graph, module, "jddcnn", "ibyrc15",
      1 * DT_LOCAL_SIZE_X, r82 * DT_LOCAL_SIZE_Y, r76,
      8, (const int*)concat_conv_cm_1_pc, 6, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi0,// unknown
      "d", "read", "ssbo", "u8", &roi15, // CHWC8[64] : f16
      "e", "read", "ssbo", "u8", &roi6, // CHWC8[64] : f16
      "f", "write", "ssbo", "u8", &roi16); // HWC[64] : f16
  // direct_conv_cm_12 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_12_pc[2] = {(uint32_t)(r33), (uint32_t)(r21)};
  const int direct_conv_cm_12_id = dt_node_add(graph, module, "jddcnn", "ibyrc4",
      1 * DT_LOCAL_SIZE_X, r82 * DT_LOCAL_SIZE_Y, r76,
      8, (const int*)direct_conv_cm_12_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi16, // HWC[64] : f16
      "d", "write", "ssbo", "u8", &roi17); // CHWC8[64] : f16
  // direct_conv_cm_13 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_13_pc[2] = {(uint32_t)(r30), (uint32_t)(r18)};
  const int direct_conv_cm_13_id = dt_node_add(graph, module, "jddcnn", "ibyrc5",
      1 * DT_LOCAL_SIZE_X, r98 * DT_LOCAL_SIZE_Y, r96,
      8, (const int*)direct_conv_cm_13_pc, 5, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi17, // CHWC8[64] : f16
      "d", "read", "ssbo", "u8", &roi4, // CHWC8[32] : f16
      "dummy", "write", "ssbo", "u8", &roi18);//
  // direct_conv_cm_14 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_14_pc[2] = {(uint32_t)(r30), (uint32_t)(r18)};
  const int direct_conv_cm_14_id = dt_node_add(graph, module, "jddcnn", "ibyrc3",
      1 * DT_LOCAL_SIZE_X, r98 * DT_LOCAL_SIZE_Y, r96,
      8, (const int*)direct_conv_cm_14_pc, 5, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi4, // CHWC8[64] : f16
      "d", "write", "ssbo", "u8", &roi19, // CHWC8[32] : f16
      "z0", "read", "ssbo", "u8", &roi18);//
  // direct_conv_cm_15 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_15_pc[2] = {(uint32_t)(r30), (uint32_t)(r18)};
  const int direct_conv_cm_15_id = dt_node_add(graph, module, "jddcnn", "ibyrc10",
      1 * DT_LOCAL_SIZE_X, r98 * DT_LOCAL_SIZE_Y, r96,
      8, (const int*)direct_conv_cm_15_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi19, // CHWC8[32] : f16
      "d", "write", "ssbo", "u8", &roi20); // HWC[32] : f16
  // direct_conv_cm_16 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_16_pc[2] = {(uint32_t)(r8), (uint32_t)(r15)};
  const int direct_conv_cm_16_id = dt_node_add(graph, module, "jddcnn", "ibyrc2",
      1 * DT_LOCAL_SIZE_X, r113 * DT_LOCAL_SIZE_Y, r111,
      8, (const int*)direct_conv_cm_16_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi20, // HWC[32] : f16
      "d", "write", "ssbo", "u8", &roi21); // HWC[16] : f16
  // concat_conv_cm_2 (concat_conv_cm.comp)
  const uint32_t concat_conv_cm_2_pc[2] = {(uint32_t)(r8), (uint32_t)(r15)};
  const int concat_conv_cm_2_id = dt_node_add(graph, module, "jddcnn", "ibyrc1",
      1 * DT_LOCAL_SIZE_X, r113 * DT_LOCAL_SIZE_Y, r111,
      8, (const int*)concat_conv_cm_2_pc, 6, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi0,// unknown
      "d", "read", "ssbo", "u8", &roi21, // HWC[16] : f16
      "e", "read", "ssbo", "u8", &roi2, // HWC[4] : f16
      "f", "write", "ssbo", "u8", &roi22); // CHWC8[16] : f16
  // direct_conv_cm_17 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_17_pc[2] = {(uint32_t)(r8), (uint32_t)(r15)};
  const int direct_conv_cm_17_id = dt_node_add(graph, module, "jddcnn", "ibyrc13",
      1 * DT_LOCAL_SIZE_X, r113 * DT_LOCAL_SIZE_Y, r111,
      8, (const int*)direct_conv_cm_17_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi22, // CHWC8[16] : f16
      "d", "write", "ssbo", "u8", &roi23); // HWC[12] : f16
  // memory_slice (memory_slice.comp)
  const uint32_t memory_slice_pc[6] = {0, 0, (uint32_t)(r8), (uint32_t)(r15), (uint32_t)(W), (uint32_t)(H)};
  const int memory_slice_id = dt_node_add(graph, module, "jddcnn", "ibyrc0",
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
  dt_node_connect_named(graph, direct_conv_cm_1_id, "d", direct_conv_cm_13_id, "d");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_14_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_14_id, "b");
  dt_node_connect_named(graph, direct_conv_cm_1_id, "d", direct_conv_cm_14_id, "c");
  dt_node_connect_named(graph, direct_conv_cm_13_id, "dummy", direct_conv_cm_14_id, "z0");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_15_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_15_id, "b");
  dt_node_connect_named(graph, direct_conv_cm_14_id, "d", direct_conv_cm_15_id, "c");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_16_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_16_id, "b");
  dt_node_connect_named(graph, direct_conv_cm_15_id, "d", direct_conv_cm_16_id, "c");
  dt_node_connect_named(graph, weights_id, "w", concat_conv_cm_2_id, "a");
  dt_node_connect_named(graph, weights_id, "w", concat_conv_cm_2_id, "b");
  dt_node_connect_named(graph, weights_id, "w", concat_conv_cm_2_id, "c");
  dt_node_connect_named(graph, direct_conv_cm_16_id, "d", concat_conv_cm_2_id, "d");
  dt_node_connect_named(graph, memory_pad_id, "b", concat_conv_cm_2_id, "e");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_17_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_17_id, "b");
  dt_node_connect_named(graph, concat_conv_cm_2_id, "f", direct_conv_cm_17_id, "c");
  dt_node_connect_named(graph, direct_conv_cm_17_id, "d", memory_slice_id, "b");
  if (output_connector == NULL) {
    dt_connector_copy(graph, module, output_id, memory_slice_id, 0);
  } else {
    dt_node_connect_named(graph, memory_slice_id, "a", output_id, output_connector);
  }
  graph->node[direct_conv_cm_id].connector[1].ssbo_offset = 1152;
  graph->node[direct_conv_cm_1_id].connector[0].ssbo_offset = 1184;
  graph->node[direct_conv_cm_1_id].connector[1].ssbo_offset = 10400;
  graph->node[direct_conv_cm_1_id].connector[3].ssbo_offset = r50;
  graph->node[direct_conv_cm_2_id].connector[0].ssbo_offset = 10464;
  graph->node[direct_conv_cm_2_id].connector[1].ssbo_offset = 28896;
  graph->node[direct_conv_cm_2_id].connector[2].ssbo_offset = r50;
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
  graph->node[direct_conv_cm_14_id].connector[0].ssbo_offset = 4234464;
  graph->node[direct_conv_cm_14_id].connector[1].ssbo_offset = 4271328;
  graph->node[direct_conv_cm_15_id].connector[0].ssbo_offset = 4271392;
  graph->node[direct_conv_cm_15_id].connector[1].ssbo_offset = 4289824;
  graph->node[direct_conv_cm_16_id].connector[0].ssbo_offset = 4289888;
  graph->node[direct_conv_cm_16_id].connector[1].ssbo_offset = 4299104;
  graph->node[concat_conv_cm_2_id].connector[0].ssbo_offset = 4299136;
  graph->node[concat_conv_cm_2_id].connector[1].ssbo_offset = 4303744;
  graph->node[concat_conv_cm_2_id].connector[2].ssbo_offset = 4304896;
  graph->node[direct_conv_cm_17_id].connector[0].ssbo_offset = 4304928;
  graph->node[direct_conv_cm_17_id].connector[1].ssbo_offset = 4308384;
}

#endif
