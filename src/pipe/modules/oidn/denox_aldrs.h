#ifndef oidn_aldrs_DENOX_MODULE_H
#define oidn_aldrs_DENOX_MODULE_H
#include "modules/api.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int denox_read_source_aldrs(dt_module_t* mod, void* mapped, dt_read_source_params_t* p) {
  if (p->node->kernel == dt_token("weights")) {
    FILE* f = dt_graph_open_resource(mod->graph, 0, "data/oidn-aldrs.dat", "rb");
    if (!f) {
      snprintf(mod->graph->gui_msg_buf, sizeof(mod->graph->gui_msg_buf),
            "oidn: could not find \"data/oidn-aldrs.dat\"");
      return 1;
    }
    fseek(f, 0, SEEK_END);
    const size_t size = ftell(f);
    const size_t expected_size = 638944;
    if (size != expected_size) {
      snprintf(mod->graph->gui_msg_buf, sizeof(mod->graph->gui_msg_buf),
            "oidn: weight file \"data/oidn-aldrs.dat\" has unexpected size!");
      fclose(f);
      return 1;
    }
    fseek(f, 0, SEEK_SET);
    fread(mapped, size, 1, f);
    fclose(f);
  }
  return 0;
}

static void denox_create_nodes_aldrs(dt_graph_t* graph, dt_module_t* module,
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
  const int64_t r13 = ((H % 16) + 16) % 16;
  const int64_t r14 = r13 * -1;
  const int64_t r15 = r14 + 16;
  const int64_t r16 = ((r15 % 16) + 16) % 16;
  const int64_t r17 = H + r16;
  const int64_t r18 = r17 + 12;
  const int64_t r19 = r18 / 13;
  const int64_t r20 = r17 - 2;
  const int64_t r21 = r20 / 2;
  const int64_t r22 = r21 + 1;
  const int64_t r23 = r22 - 2;
  const int64_t r24 = r23 / 2;
  const int64_t r25 = r24 + 1;
  const int64_t r26 = r25 - 2;
  const int64_t r27 = r26 / 2;
  const int64_t r28 = r27 + 1;
  const int64_t r29 = r10 - 2;
  const int64_t r30 = r29 / 2;
  const int64_t r31 = r30 + 1;
  const int64_t r32 = r31 - 2;
  const int64_t r33 = r32 / 2;
  const int64_t r34 = r33 + 1;
  const int64_t r35 = r34 - 2;
  const int64_t r36 = r35 / 2;
  const int64_t r37 = r36 + 1;
  const int64_t r38 = r37 * r28;
  const int64_t r39 = r38 * 64;
  const int64_t r40 = r28 - 2;
  const int64_t r41 = r40 / 2;
  const int64_t r42 = r41 + 1;
  const int64_t r43 = r37 - 2;
  const int64_t r44 = r43 / 2;
  const int64_t r45 = r44 + 1;
  const int64_t r46 = r45 * r42;
  const int64_t r47 = r46 * 64;
  const int64_t r50 = r34 * r25;
  const int64_t r51 = r50 * 64;
  const int64_t r52 = r38 * 128;
  const int64_t r55 = r31 * r22;
  const int64_t r56 = r55 * 64;
  const int64_t r57 = r50 * 128;
  const int64_t r60 = r10 * r17;
  const int64_t r61 = r60 * 6;
  const int64_t r64 = r42 + 5;
  const int64_t r65 = r64 / 6;
  const int64_t r68 = r45 + 15;
  const int64_t r69 = r68 / 16;
  const int64_t r72 = r28 + 11;
  const int64_t r73 = r72 / 12;
  const int64_t r74 = r28 + 7;
  const int64_t r75 = r74 / 8;
  const int64_t r76 = r28 + 3;
  const int64_t r77 = r76 / 4;
  const int64_t r80 = r37 + 15;
  const int64_t r81 = r80 / 16;
  const int64_t r85 = r25 + 7;
  const int64_t r86 = r85 / 8;
  const int64_t r87 = r25 + 3;
  const int64_t r88 = r87 / 4;
  const int64_t r91 = r34 + 15;
  const int64_t r92 = r91 / 16;
  const int64_t r96 = r22 + 11;
  const int64_t r97 = r96 / 12;
  const int64_t r98 = r22 + 3;
  const int64_t r99 = r98 / 4;
  const int64_t r101 = r55 * 128;
  const int64_t r103 = r31 + 15;
  const int64_t r104 = r103 / 16;
  const int64_t r107 = r17 + 20;
  const int64_t r108 = r107 / 21;
  const int64_t r109 = r17 + 2;
  const int64_t r110 = r109 / 3;
  const int64_t r111 = r17 + 13;
  const int64_t r112 = r111 / 14;
  const int64_t r114 = r60 * 64;
  const int64_t r116 = r17 + 5;
  const int64_t r117 = r116 / 6;
  const int64_t r118 = r17 + 11;
  const int64_t r119 = r118 / 12;
  const int64_t r122 = r10 + 15;
  const int64_t r123 = r122 / 16;
  const int64_t r124 = H * W;
  const int64_t r125 = r124 * 6;
  dt_roi_t roi0 = {.wd = 638944, .ht = 1};
  dt_roi_t roi1 = {.wd = (uint32_t)(r125 / 2), .ht = 1};
  dt_roi_t roi2 = {.wd = (uint32_t)(r61), .ht = 1};
  dt_roi_t roi3 = {.wd = (uint32_t)(r114), .ht = 1};
  dt_roi_t roi4 = {.wd = (uint32_t)(r56), .ht = 1};
  dt_roi_t roi5 = {.wd = (uint32_t)(r51), .ht = 1};
  dt_roi_t roi6 = {.wd = (uint32_t)(r39), .ht = 1};
  dt_roi_t roi7 = {.wd = (uint32_t)(r47), .ht = 1};
  dt_roi_t roi8 = {.wd = (uint32_t)(r47), .ht = 1};
  dt_roi_t roi9 = {.wd = (uint32_t)(r47), .ht = 1};
  dt_roi_t roi10 = {.wd = (uint32_t)(r52), .ht = 1};
  dt_roi_t roi11 = {.wd = (uint32_t)(r52), .ht = 1};
  dt_roi_t roi12 = {.wd = (uint32_t)(r57), .ht = 1};
  dt_roi_t roi13 = {.wd = (uint32_t)(r57), .ht = 1};
  dt_roi_t roi14 = {.wd = (uint32_t)(r101), .ht = 1};
  dt_roi_t roi15 = {.wd = (uint32_t)(r56), .ht = 1};
  dt_roi_t roi16 = {.wd = (uint32_t)(r114), .ht = 1};
  dt_roi_t roi17 = {.wd = (uint32_t)(r114), .ht = 1};
  dt_roi_t roi18 = {.wd = (uint32_t)(r61), .ht = 1};
  dt_roi_t roi19 = {.wd = (uint32_t)(r125 / 2), .ht = 1};
  int weights_id = dt_node_add(graph, module, "oidn", "weights",
      1, 1, 1, 0, NULL, 1, 
      "w", "source", "ssbo", "u8", &roi0);
  // memory_pad (memory_pad.comp)
  const uint32_t memory_pad_pc[6] = {(uint32_t)(r10), (uint32_t)(r17), 0, (uint32_t)(r9), 0, (uint32_t)(r16)};
  const int memory_pad_id = dt_node_add(graph, module, "oidn", "aldrs1",
      1 * DT_LOCAL_SIZE_X, r12 * DT_LOCAL_SIZE_Y, r110,
      24, (const int*)memory_pad_pc, 2, //
      "a", "read", "ssbo", "f16", &roi1, // HWC[3] : f16
      "b", "write", "ssbo", "u8", &roi2); // HWC[3] : f16
  // direct_conv (direct_conv.comp)
  const uint32_t direct_conv_pc[2] = {(uint32_t)(r10), (uint32_t)(r17)};
  const int direct_conv_id = dt_node_add(graph, module, "oidn", "aldrs4",
      1 * DT_LOCAL_SIZE_X, r123 * DT_LOCAL_SIZE_Y, r108,
      8, (const int*)direct_conv_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi2, // HWC[3] : f16
      "d", "write", "ssbo", "u8", &roi3); // HWC[32] : f16
  // direct_conv_cm (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_pc[2] = {(uint32_t)(r10), (uint32_t)(r17)};
  const int direct_conv_cm_id = dt_node_add(graph, module, "oidn", "aldrs8",
      1 * DT_LOCAL_SIZE_X, r123 * DT_LOCAL_SIZE_Y, r119,
      8, (const int*)direct_conv_cm_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi3, // HWC[32] : f16
      "d", "write", "ssbo", "u8", &roi4); // CHWC8[32] : f16
  // direct_conv_cm_1 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_1_pc[2] = {(uint32_t)(r31), (uint32_t)(r22)};
  const int direct_conv_cm_1_id = dt_node_add(graph, module, "oidn", "aldrs5",
      1 * DT_LOCAL_SIZE_X, r104 * DT_LOCAL_SIZE_Y, r99,
      8, (const int*)direct_conv_cm_1_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi4, // CHWC8[32] : f16
      "d", "write", "ssbo", "u8", &roi5); // CHWC8[32] : f16
  // direct_conv_cm_2 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_2_pc[2] = {(uint32_t)(r34), (uint32_t)(r25)};
  const int direct_conv_cm_2_id = dt_node_add(graph, module, "oidn", "aldrs5",
      1 * DT_LOCAL_SIZE_X, r92 * DT_LOCAL_SIZE_Y, r88,
      8, (const int*)direct_conv_cm_2_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi5, // CHWC8[32] : f16
      "d", "write", "ssbo", "u8", &roi6); // CHWC8[32] : f16
  // direct_conv_cm_3 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_3_pc[2] = {(uint32_t)(r37), (uint32_t)(r28)};
  const int direct_conv_cm_3_id = dt_node_add(graph, module, "oidn", "aldrs5",
      1 * DT_LOCAL_SIZE_X, r81 * DT_LOCAL_SIZE_Y, r77,
      8, (const int*)direct_conv_cm_3_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi6, // CHWC8[32] : f16
      "d", "write", "ssbo", "u8", &roi7); // CHWC8[32] : f16
  // direct_conv_1 (direct_conv.comp)
  const uint32_t direct_conv_1_pc[2] = {(uint32_t)(r45), (uint32_t)(r42)};
  const int direct_conv_1_id = dt_node_add(graph, module, "oidn", "aldrs10",
      1 * DT_LOCAL_SIZE_X, r69 * DT_LOCAL_SIZE_Y, r65,
      8, (const int*)direct_conv_1_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi7, // CHWC8[32] : f16
      "d", "write", "ssbo", "u8", &roi8); // CHWC8[32] : f16
  // direct_conv_2 (direct_conv.comp)
  const uint32_t direct_conv_2_pc[2] = {(uint32_t)(r45), (uint32_t)(r42)};
  const int direct_conv_2_id = dt_node_add(graph, module, "oidn", "aldrs12",
      1 * DT_LOCAL_SIZE_X, r69 * DT_LOCAL_SIZE_Y, r65,
      8, (const int*)direct_conv_2_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi8, // CHWC8[32] : f16
      "d", "write", "ssbo", "u8", &roi9); // HWC[32] : f16
  // concat_conv_cm (concat_conv_cm.comp)
  const uint32_t concat_conv_cm_pc[2] = {(uint32_t)(r37), (uint32_t)(r28)};
  const int concat_conv_cm_id = dt_node_add(graph, module, "oidn", "aldrs6",
      1 * DT_LOCAL_SIZE_X, r81 * DT_LOCAL_SIZE_Y, r73,
      8, (const int*)concat_conv_cm_pc, 6, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi0,// unknown
      "d", "read", "ssbo", "u8", &roi9, // HWC[32] : f16
      "e", "read", "ssbo", "u8", &roi6, // CHWC8[32] : f16
      "f", "write", "ssbo", "u8", &roi10); // HWC[64] : f16
  // direct_conv_cm_4 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_4_pc[2] = {(uint32_t)(r37), (uint32_t)(r28)};
  const int direct_conv_cm_4_id = dt_node_add(graph, module, "oidn", "aldrs9",
      1 * DT_LOCAL_SIZE_X, r81 * DT_LOCAL_SIZE_Y, r75,
      8, (const int*)direct_conv_cm_4_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi10, // HWC[64] : f16
      "d", "write", "ssbo", "u8", &roi11); // HWC[64] : f16
  // concat_conv_cm_1 (concat_conv_cm.comp)
  const uint32_t concat_conv_cm_1_pc[2] = {(uint32_t)(r34), (uint32_t)(r25)};
  const int concat_conv_cm_1_id = dt_node_add(graph, module, "oidn", "aldrs2",
      1 * DT_LOCAL_SIZE_X, r92 * DT_LOCAL_SIZE_Y, r88,
      8, (const int*)concat_conv_cm_1_pc, 6, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi0,// unknown
      "d", "read", "ssbo", "u8", &roi11, // HWC[64] : f16
      "e", "read", "ssbo", "u8", &roi5, // CHWC8[32] : f16
      "f", "write", "ssbo", "u8", &roi12); // HWC[64] : f16
  // direct_conv_cm_5 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_5_pc[2] = {(uint32_t)(r34), (uint32_t)(r25)};
  const int direct_conv_cm_5_id = dt_node_add(graph, module, "oidn", "aldrs9",
      1 * DT_LOCAL_SIZE_X, r92 * DT_LOCAL_SIZE_Y, r86,
      8, (const int*)direct_conv_cm_5_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi12, // HWC[64] : f16
      "d", "write", "ssbo", "u8", &roi13); // HWC[64] : f16
  // concat_conv_cm_2 (concat_conv_cm.comp)
  const uint32_t concat_conv_cm_2_pc[2] = {(uint32_t)(r31), (uint32_t)(r22)};
  const int concat_conv_cm_2_id = dt_node_add(graph, module, "oidn", "aldrs7",
      1 * DT_LOCAL_SIZE_X, r104 * DT_LOCAL_SIZE_Y, r99,
      8, (const int*)concat_conv_cm_2_pc, 6, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi0,// unknown
      "d", "read", "ssbo", "u8", &roi13, // HWC[64] : f16
      "e", "read", "ssbo", "u8", &roi4, // CHWC8[32] : f16
      "f", "write", "ssbo", "u8", &roi14); // CHWC8[64] : f16
  // direct_conv_3 (direct_conv.comp)
  const uint32_t direct_conv_3_pc[2] = {(uint32_t)(r31), (uint32_t)(r22)};
  const int direct_conv_3_id = dt_node_add(graph, module, "oidn", "aldrs11",
      1 * DT_LOCAL_SIZE_X, r104 * DT_LOCAL_SIZE_Y, r97,
      8, (const int*)direct_conv_3_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi14, // CHWC8[64] : f16
      "d", "write", "ssbo", "u8", &roi15); // CHWC8[32] : f16
  // concat_conv_cm_3 (concat_conv_cm.comp)
  const uint32_t concat_conv_cm_3_pc[2] = {(uint32_t)(r10), (uint32_t)(r17)};
  const int concat_conv_cm_3_id = dt_node_add(graph, module, "oidn", "aldrs3",
      1 * DT_LOCAL_SIZE_X, r123 * DT_LOCAL_SIZE_Y, r112,
      8, (const int*)concat_conv_cm_3_pc, 6, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi0,// unknown
      "d", "read", "ssbo", "u8", &roi15, // CHWC8[32] : f16
      "e", "read", "ssbo", "u8", &roi2, // HWC[3] : f16
      "f", "write", "ssbo", "u8", &roi16); // CHWC8[32] : f16
  // direct_conv_4 (direct_conv.comp)
  const uint32_t direct_conv_4_pc[2] = {(uint32_t)(r10), (uint32_t)(r17)};
  const int direct_conv_4_id = dt_node_add(graph, module, "oidn", "aldrs10",
      1 * DT_LOCAL_SIZE_X, r123 * DT_LOCAL_SIZE_Y, r117,
      8, (const int*)direct_conv_4_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi16, // CHWC8[32] : f16
      "d", "write", "ssbo", "u8", &roi17); // CHWC8[32] : f16
  // direct_conv_5 (direct_conv.comp)
  const uint32_t direct_conv_5_pc[2] = {(uint32_t)(r10), (uint32_t)(r17)};
  const int direct_conv_5_id = dt_node_add(graph, module, "oidn", "aldrs13",
      1 * DT_LOCAL_SIZE_X, r123 * DT_LOCAL_SIZE_Y, r19,
      8, (const int*)direct_conv_5_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi17, // CHWC8[32] : f16
      "d", "write", "ssbo", "u8", &roi18); // HWC[3] : f16
  // memory_slice (memory_slice.comp)
  const uint32_t memory_slice_pc[6] = {0, 0, (uint32_t)(r10), (uint32_t)(r17), (uint32_t)(W), (uint32_t)(H)};
  const int memory_slice_id = dt_node_add(graph, module, "oidn", "aldrs0",
      1 * DT_LOCAL_SIZE_X, r5 * DT_LOCAL_SIZE_Y, r3,
      24, (const int*)memory_slice_pc, 2, //
      "a", "read", "ssbo", "u8", &roi18, // HWC[3] : f16
      "b", "write", "ssbo", "f16", &roi19); // HWC[3] : f16
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
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_3_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_3_id, "b");
  dt_node_connect_named(graph, direct_conv_cm_2_id, "d", direct_conv_cm_3_id, "c");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_1_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_1_id, "b");
  dt_node_connect_named(graph, direct_conv_cm_3_id, "d", direct_conv_1_id, "c");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_2_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_2_id, "b");
  dt_node_connect_named(graph, direct_conv_1_id, "d", direct_conv_2_id, "c");
  dt_node_connect_named(graph, weights_id, "w", concat_conv_cm_id, "a");
  dt_node_connect_named(graph, weights_id, "w", concat_conv_cm_id, "b");
  dt_node_connect_named(graph, weights_id, "w", concat_conv_cm_id, "c");
  dt_node_connect_named(graph, direct_conv_2_id, "d", concat_conv_cm_id, "d");
  dt_node_connect_named(graph, direct_conv_cm_2_id, "d", concat_conv_cm_id, "e");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_4_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_4_id, "b");
  dt_node_connect_named(graph, concat_conv_cm_id, "f", direct_conv_cm_4_id, "c");
  dt_node_connect_named(graph, weights_id, "w", concat_conv_cm_1_id, "a");
  dt_node_connect_named(graph, weights_id, "w", concat_conv_cm_1_id, "b");
  dt_node_connect_named(graph, weights_id, "w", concat_conv_cm_1_id, "c");
  dt_node_connect_named(graph, direct_conv_cm_4_id, "d", concat_conv_cm_1_id, "d");
  dt_node_connect_named(graph, direct_conv_cm_1_id, "d", concat_conv_cm_1_id, "e");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_5_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_5_id, "b");
  dt_node_connect_named(graph, concat_conv_cm_1_id, "f", direct_conv_cm_5_id, "c");
  dt_node_connect_named(graph, weights_id, "w", concat_conv_cm_2_id, "a");
  dt_node_connect_named(graph, weights_id, "w", concat_conv_cm_2_id, "b");
  dt_node_connect_named(graph, weights_id, "w", concat_conv_cm_2_id, "c");
  dt_node_connect_named(graph, direct_conv_cm_5_id, "d", concat_conv_cm_2_id, "d");
  dt_node_connect_named(graph, direct_conv_cm_id, "d", concat_conv_cm_2_id, "e");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_3_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_3_id, "b");
  dt_node_connect_named(graph, concat_conv_cm_2_id, "f", direct_conv_3_id, "c");
  dt_node_connect_named(graph, weights_id, "w", concat_conv_cm_3_id, "a");
  dt_node_connect_named(graph, weights_id, "w", concat_conv_cm_3_id, "b");
  dt_node_connect_named(graph, weights_id, "w", concat_conv_cm_3_id, "c");
  dt_node_connect_named(graph, direct_conv_3_id, "d", concat_conv_cm_3_id, "d");
  dt_node_connect_named(graph, memory_pad_id, "b", concat_conv_cm_3_id, "e");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_4_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_4_id, "b");
  dt_node_connect_named(graph, concat_conv_cm_3_id, "f", direct_conv_4_id, "c");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_5_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_5_id, "b");
  dt_node_connect_named(graph, direct_conv_4_id, "d", direct_conv_5_id, "c");
  dt_node_connect_named(graph, direct_conv_5_id, "d", memory_slice_id, "a");
  if (output_connector == NULL) {
    dt_connector_copy(graph, module, output_id, memory_slice_id, 1);
  } else {
    dt_node_connect_named(graph, memory_slice_id, "b", output_id, output_connector);
  }
  graph->node[direct_conv_id].connector[1].ssbo_offset = 4608;
  graph->node[direct_conv_cm_id].connector[0].ssbo_offset = 4672;
  graph->node[direct_conv_cm_id].connector[1].ssbo_offset = 23104;
  graph->node[direct_conv_cm_1_id].connector[0].ssbo_offset = 23168;
  graph->node[direct_conv_cm_1_id].connector[1].ssbo_offset = 41600;
  graph->node[direct_conv_cm_2_id].connector[0].ssbo_offset = 41664;
  graph->node[direct_conv_cm_2_id].connector[1].ssbo_offset = 60096;
  graph->node[direct_conv_cm_3_id].connector[0].ssbo_offset = 60160;
  graph->node[direct_conv_cm_3_id].connector[1].ssbo_offset = 78592;
  graph->node[direct_conv_1_id].connector[0].ssbo_offset = 78656;
  graph->node[direct_conv_1_id].connector[1].ssbo_offset = 97088;
  graph->node[direct_conv_2_id].connector[0].ssbo_offset = 97152;
  graph->node[direct_conv_2_id].connector[1].ssbo_offset = 115584;
  graph->node[concat_conv_cm_id].connector[0].ssbo_offset = 115648;
  graph->node[concat_conv_cm_id].connector[1].ssbo_offset = 152512;
  graph->node[concat_conv_cm_id].connector[2].ssbo_offset = 189376;
  graph->node[direct_conv_cm_4_id].connector[0].ssbo_offset = 189504;
  graph->node[direct_conv_cm_4_id].connector[1].ssbo_offset = 263232;
  graph->node[concat_conv_cm_1_id].connector[0].ssbo_offset = 263360;
  graph->node[concat_conv_cm_1_id].connector[1].ssbo_offset = 337088;
  graph->node[concat_conv_cm_1_id].connector[2].ssbo_offset = 373952;
  graph->node[direct_conv_cm_5_id].connector[0].ssbo_offset = 374080;
  graph->node[direct_conv_cm_5_id].connector[1].ssbo_offset = 447808;
  graph->node[concat_conv_cm_2_id].connector[0].ssbo_offset = 447936;
  graph->node[concat_conv_cm_2_id].connector[1].ssbo_offset = 521664;
  graph->node[concat_conv_cm_2_id].connector[2].ssbo_offset = 558528;
  graph->node[direct_conv_3_id].connector[0].ssbo_offset = 558656;
  graph->node[direct_conv_3_id].connector[1].ssbo_offset = 595520;
  graph->node[concat_conv_cm_3_id].connector[0].ssbo_offset = 595584;
  graph->node[concat_conv_cm_3_id].connector[1].ssbo_offset = 614016;
  graph->node[concat_conv_cm_3_id].connector[2].ssbo_offset = 615744;
  graph->node[direct_conv_4_id].connector[0].ssbo_offset = 615808;
  graph->node[direct_conv_4_id].connector[1].ssbo_offset = 634240;
  graph->node[direct_conv_5_id].connector[0].ssbo_offset = 634304;
  graph->node[direct_conv_5_id].connector[1].ssbo_offset = 638912;
}

#endif
