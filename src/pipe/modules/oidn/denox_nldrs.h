#ifndef oidn_nldrs_DENOX_MODULE_H
#define oidn_nldrs_DENOX_MODULE_H
#include "modules/api.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int denox_read_source_nldrs(dt_module_t* mod, void* mapped, dt_read_source_params_t* p) {
  if (p->node->kernel == dt_token("weights")) {
    FILE* f = dt_graph_open_resource(mod->graph, 0, "data/oidn-nldrs.dat", "rb");
    if (!f) {
      snprintf(mod->graph->gui_msg_buf, sizeof(mod->graph->gui_msg_buf),
            "oidn: could not find \"data/oidn-nldrs.dat\"");
      return 1;
    }
    fseek(f, 0, SEEK_END);
    const size_t size = ftell(f);
    const size_t expected_size = 633168;
    if (size != expected_size) {
      snprintf(mod->graph->gui_msg_buf, sizeof(mod->graph->gui_msg_buf),
            "oidn: weight file \"data/oidn-nldrs.dat\" has unexpected size!");
      fclose(f);
      return 1;
    }
    fseek(f, 0, SEEK_SET);
    fread(mapped, size, 1, f);
    fclose(f);
  }
  return 0;
}

static void denox_create_nodes_nldrs(dt_graph_t* graph, dt_module_t* module,
      uint64_t H, uint64_t W,
      int input_id, const char* input_connector,
      int output_id, const char* output_connector) {
  const int64_t r2 = W + 127;
  const int64_t r3 = r2 / 128;
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
  const int64_t r27 = r15 - 2;
  const int64_t r28 = r27 / 2;
  const int64_t r29 = r28 + 1;
  const int64_t r30 = r29 - 2;
  const int64_t r31 = r30 / 2;
  const int64_t r32 = r31 + 1;
  const int64_t r33 = r32 - 2;
  const int64_t r34 = r33 / 2;
  const int64_t r35 = r34 + 1;
  const int64_t r36 = r35 * r26;
  const int64_t r37 = r36 * 64;
  const int64_t r38 = r26 - 2;
  const int64_t r39 = r38 / 2;
  const int64_t r40 = r39 + 1;
  const int64_t r41 = r35 - 2;
  const int64_t r42 = r41 / 2;
  const int64_t r43 = r42 + 1;
  const int64_t r44 = r43 * r40;
  const int64_t r45 = r44 * 64;
  const int64_t r48 = r32 * r23;
  const int64_t r49 = r48 * 64;
  const int64_t r50 = r36 * 128;
  const int64_t r53 = r29 * r20;
  const int64_t r54 = r53 * 64;
  const int64_t r55 = r48 * 128;
  const int64_t r58 = r15 * r8;
  const int64_t r59 = r58 * 6;
  const int64_t r62 = r40 + 3;
  const int64_t r63 = r62 / 4;
  const int64_t r66 = r43 + 15;
  const int64_t r67 = r66 / 16;
  const int64_t r70 = r26 + 4;
  const int64_t r71 = r70 / 5;
  const int64_t r72 = r26 + 7;
  const int64_t r73 = r72 / 8;
  const int64_t r76 = r35 + 15;
  const int64_t r77 = r76 / 16;
  const int64_t r81 = r23 + 11;
  const int64_t r82 = r81 / 12;
  const int64_t r83 = r23 + 5;
  const int64_t r84 = r83 / 6;
  const int64_t r87 = r32 + 15;
  const int64_t r88 = r87 / 16;
  const int64_t r92 = r20 + 7;
  const int64_t r93 = r92 / 8;
  const int64_t r95 = r53 * 128;
  const int64_t r97 = r29 + 15;
  const int64_t r98 = r97 / 16;
  const int64_t r102 = r58 * 64;
  const int64_t r104 = r8 + 7;
  const int64_t r105 = r104 / 8;
  const int64_t r108 = r8 + 3;
  const int64_t r109 = r108 / 4;
  const int64_t r110 = r15 + 15;
  const int64_t r111 = r110 / 16;
  const int64_t r112 = H * W;
  const int64_t r113 = r112 * 6;
  dt_roi_t roi0 = {.wd = 633168, .ht = 1};
  dt_roi_t roi1 = {.wd = (uint32_t)(r113 / 2), .ht = 1};
  dt_roi_t roi2 = {.wd = (uint32_t)(r59), .ht = 1};
  dt_roi_t roi3 = {.wd = (uint32_t)(r102), .ht = 1};
  dt_roi_t roi4 = {.wd = (uint32_t)(r54), .ht = 1};
  dt_roi_t roi5 = {.wd = (uint32_t)(r49), .ht = 1};
  dt_roi_t roi6 = {.wd = (uint32_t)(r37), .ht = 1};
  dt_roi_t roi7 = {.wd = (uint32_t)(r45), .ht = 1};
  dt_roi_t roi8 = {.wd = (uint32_t)(r45), .ht = 1};
  dt_roi_t roi9 = {.wd = (uint32_t)(r45), .ht = 1};
  dt_roi_t roi10 = {.wd = (uint32_t)(r50), .ht = 1};
  dt_roi_t roi11 = {.wd = (uint32_t)(r50), .ht = 1};
  dt_roi_t roi12 = {.wd = (uint32_t)(r55), .ht = 1};
  dt_roi_t roi13 = {.wd = (uint32_t)(r55), .ht = 1};
  dt_roi_t roi14 = {.wd = (uint32_t)(r95), .ht = 1};
  dt_roi_t roi15 = {.wd = (uint32_t)(r54), .ht = 1};
  dt_roi_t roi16 = {.wd = (uint32_t)(r102), .ht = 1};
  dt_roi_t roi17 = {.wd = (uint32_t)(r102), .ht = 1};
  dt_roi_t roi18 = {.wd = (uint32_t)(r59), .ht = 1};
  dt_roi_t roi19 = {.wd = (uint32_t)(r113 / 2), .ht = 1};
  int weights_id = dt_node_add(graph, module, "oidn", "weights",
      1, 1, 1, 0, NULL, 1, 
      "w", "source", "ssbo", "u8", &roi0);
  // memory_pad (memory_pad.comp)
  const uint32_t memory_pad_pc[6] = {(uint32_t)(r15), (uint32_t)(r8), 0, (uint32_t)(r14), 0, (uint32_t)(r7)};
  const int memory_pad_id = dt_node_add(graph, module, "oidn", "nldrs2",
      1 * DT_LOCAL_SIZE_X, r17 * DT_LOCAL_SIZE_Y, r10,
      24, (const int*)memory_pad_pc, 2, //
      "a", "read", "ssbo", "f16", &roi1, // HWC[3] : f16
      "b", "write", "ssbo", "u8", &roi2); // HWC[3] : f16
  // direct_conv_cm (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_pc[2] = {(uint32_t)(r15), (uint32_t)(r8)};
  const int direct_conv_cm_id = dt_node_add(graph, module, "oidn", "nldrs5",
      1 * DT_LOCAL_SIZE_X, r111 * DT_LOCAL_SIZE_Y, r109,
      8, (const int*)direct_conv_cm_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi2, // HWC[3] : f16
      "d", "write", "ssbo", "u8", &roi3); // CHWC8[32] : f16
  // direct_conv_cm_1 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_1_pc[2] = {(uint32_t)(r15), (uint32_t)(r8)};
  const int direct_conv_cm_1_id = dt_node_add(graph, module, "oidn", "nldrs7",
      1 * DT_LOCAL_SIZE_X, r111 * DT_LOCAL_SIZE_Y, r105,
      8, (const int*)direct_conv_cm_1_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi3, // CHWC8[32] : f16
      "d", "write", "ssbo", "u8", &roi4); // CHWC8[32] : f16
  // direct_conv_cm_2 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_2_pc[2] = {(uint32_t)(r29), (uint32_t)(r20)};
  const int direct_conv_cm_2_id = dt_node_add(graph, module, "oidn", "nldrs6",
      1 * DT_LOCAL_SIZE_X, r98 * DT_LOCAL_SIZE_Y, r93,
      8, (const int*)direct_conv_cm_2_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi4, // CHWC8[32] : f16
      "d", "write", "ssbo", "u8", &roi5); // HWC[32] : f16
  // direct_conv_cm_3 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_3_pc[2] = {(uint32_t)(r32), (uint32_t)(r23)};
  const int direct_conv_cm_3_id = dt_node_add(graph, module, "oidn", "nldrs4",
      1 * DT_LOCAL_SIZE_X, r88 * DT_LOCAL_SIZE_Y, r82,
      8, (const int*)direct_conv_cm_3_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi5, // HWC[32] : f16
      "d", "write", "ssbo", "u8", &roi6); // CHWC8[32] : f16
  // direct_conv_cm_4 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_4_pc[2] = {(uint32_t)(r35), (uint32_t)(r26)};
  const int direct_conv_cm_4_id = dt_node_add(graph, module, "oidn", "nldrs6",
      1 * DT_LOCAL_SIZE_X, r77 * DT_LOCAL_SIZE_Y, r73,
      8, (const int*)direct_conv_cm_4_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi6, // CHWC8[32] : f16
      "d", "write", "ssbo", "u8", &roi7); // HWC[32] : f16
  // direct_conv_cm_5 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_5_pc[2] = {(uint32_t)(r43), (uint32_t)(r40)};
  const int direct_conv_cm_5_id = dt_node_add(graph, module, "oidn", "nldrs12",
      1 * DT_LOCAL_SIZE_X, r67 * DT_LOCAL_SIZE_Y, r63,
      8, (const int*)direct_conv_cm_5_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi7, // HWC[32] : f16
      "d", "write", "ssbo", "u8", &roi8); // HWC[32] : f16
  // direct_conv_cm_6 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_6_pc[2] = {(uint32_t)(r43), (uint32_t)(r40)};
  const int direct_conv_cm_6_id = dt_node_add(graph, module, "oidn", "nldrs12",
      1 * DT_LOCAL_SIZE_X, r67 * DT_LOCAL_SIZE_Y, r63,
      8, (const int*)direct_conv_cm_6_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi8, // HWC[32] : f16
      "d", "write", "ssbo", "u8", &roi9); // HWC[32] : f16
  // concat_conv_cm (concat_conv_cm.comp)
  const uint32_t concat_conv_cm_pc[2] = {(uint32_t)(r35), (uint32_t)(r26)};
  const int concat_conv_cm_id = dt_node_add(graph, module, "oidn", "nldrs13",
      1 * DT_LOCAL_SIZE_X, r77 * DT_LOCAL_SIZE_Y, r73,
      8, (const int*)concat_conv_cm_pc, 6, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi0,// unknown
      "d", "read", "ssbo", "u8", &roi9, // HWC[32] : f16
      "e", "read", "ssbo", "u8", &roi6, // CHWC8[32] : f16
      "f", "write", "ssbo", "u8", &roi10); // CHWC8[64] : f16
  // direct_conv_cm_7 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_7_pc[2] = {(uint32_t)(r35), (uint32_t)(r26)};
  const int direct_conv_cm_7_id = dt_node_add(graph, module, "oidn", "nldrs9",
      1 * DT_LOCAL_SIZE_X, r77 * DT_LOCAL_SIZE_Y, r71,
      8, (const int*)direct_conv_cm_7_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi10, // CHWC8[64] : f16
      "d", "write", "ssbo", "u8", &roi11); // HWC[64] : f16
  // concat_conv_cm_1 (concat_conv_cm.comp)
  const uint32_t concat_conv_cm_1_pc[2] = {(uint32_t)(r32), (uint32_t)(r23)};
  const int concat_conv_cm_1_id = dt_node_add(graph, module, "oidn", "nldrs8",
      1 * DT_LOCAL_SIZE_X, r88 * DT_LOCAL_SIZE_Y, r84,
      8, (const int*)concat_conv_cm_1_pc, 6, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi0,// unknown
      "d", "read", "ssbo", "u8", &roi11, // HWC[64] : f16
      "e", "read", "ssbo", "u8", &roi5, // HWC[32] : f16
      "f", "write", "ssbo", "u8", &roi12); // CHWC8[64] : f16
  // direct_conv_cm_8 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_8_pc[2] = {(uint32_t)(r32), (uint32_t)(r23)};
  const int direct_conv_cm_8_id = dt_node_add(graph, module, "oidn", "nldrs11",
      1 * DT_LOCAL_SIZE_X, r88 * DT_LOCAL_SIZE_Y, r84,
      8, (const int*)direct_conv_cm_8_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi12, // CHWC8[64] : f16
      "d", "write", "ssbo", "u8", &roi13); // CHWC8[64] : f16
  // concat_conv_cm_2 (concat_conv_cm.comp)
  const uint32_t concat_conv_cm_2_pc[2] = {(uint32_t)(r29), (uint32_t)(r20)};
  const int concat_conv_cm_2_id = dt_node_add(graph, module, "oidn", "nldrs10",
      1 * DT_LOCAL_SIZE_X, r98 * DT_LOCAL_SIZE_Y, r93,
      8, (const int*)concat_conv_cm_2_pc, 6, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi0,// unknown
      "d", "read", "ssbo", "u8", &roi13, // CHWC8[64] : f16
      "e", "read", "ssbo", "u8", &roi4, // CHWC8[32] : f16
      "f", "write", "ssbo", "u8", &roi14); // CHWC8[64] : f16
  // direct_conv_cm_9 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_9_pc[2] = {(uint32_t)(r29), (uint32_t)(r20)};
  const int direct_conv_cm_9_id = dt_node_add(graph, module, "oidn", "nldrs14",
      1 * DT_LOCAL_SIZE_X, r98 * DT_LOCAL_SIZE_Y, r93,
      8, (const int*)direct_conv_cm_9_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi14, // CHWC8[64] : f16
      "d", "write", "ssbo", "u8", &roi15); // CHWC8[32] : f16
  // concat_conv_cm_3 (concat_conv_cm.comp)
  const uint32_t concat_conv_cm_3_pc[2] = {(uint32_t)(r15), (uint32_t)(r8)};
  const int concat_conv_cm_3_id = dt_node_add(graph, module, "oidn", "nldrs15",
      1 * DT_LOCAL_SIZE_X, r111 * DT_LOCAL_SIZE_Y, r105,
      8, (const int*)concat_conv_cm_3_pc, 6, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi0,// unknown
      "d", "read", "ssbo", "u8", &roi15, // CHWC8[32] : f16
      "e", "read", "ssbo", "u8", &roi2, // HWC[3] : f16
      "f", "write", "ssbo", "u8", &roi16); // CHWC8[32] : f16
  // direct_conv_cm_10 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_10_pc[2] = {(uint32_t)(r15), (uint32_t)(r8)};
  const int direct_conv_cm_10_id = dt_node_add(graph, module, "oidn", "nldrs1",
      1 * DT_LOCAL_SIZE_X, r111 * DT_LOCAL_SIZE_Y, r105,
      8, (const int*)direct_conv_cm_10_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi16, // CHWC8[32] : f16
      "d", "write", "ssbo", "u8", &roi17); // CHWC8[32] : f16
  // direct_conv_cm_11 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_11_pc[2] = {(uint32_t)(r15), (uint32_t)(r8)};
  const int direct_conv_cm_11_id = dt_node_add(graph, module, "oidn", "nldrs0",
      1 * DT_LOCAL_SIZE_X, r111 * DT_LOCAL_SIZE_Y, r105,
      8, (const int*)direct_conv_cm_11_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi17, // CHWC8[32] : f16
      "d", "write", "ssbo", "u8", &roi18); // HWC[3] : f16
  // memory_slice (memory_slice.comp)
  const uint32_t memory_slice_pc[6] = {0, 0, (uint32_t)(r15), (uint32_t)(r8), (uint32_t)(W), (uint32_t)(H)};
  const int memory_slice_id = dt_node_add(graph, module, "oidn", "nldrs3",
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
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_11_id, "b");
  dt_node_connect_named(graph, direct_conv_cm_10_id, "d", direct_conv_cm_11_id, "c");
  dt_node_connect_named(graph, direct_conv_cm_11_id, "d", memory_slice_id, "a");
  if (output_connector == NULL) {
    dt_connector_copy(graph, module, output_id, memory_slice_id, 1);
  } else {
    dt_node_connect_named(graph, memory_slice_id, "b", output_id, output_connector);
  }
  graph->node[direct_conv_cm_id].connector[1].ssbo_offset = 1728;
  graph->node[direct_conv_cm_1_id].connector[0].ssbo_offset = 1792;
  graph->node[direct_conv_cm_1_id].connector[1].ssbo_offset = 20224;
  graph->node[direct_conv_cm_2_id].connector[0].ssbo_offset = 20288;
  graph->node[direct_conv_cm_2_id].connector[1].ssbo_offset = 38720;
  graph->node[direct_conv_cm_3_id].connector[0].ssbo_offset = 38784;
  graph->node[direct_conv_cm_3_id].connector[1].ssbo_offset = 57216;
  graph->node[direct_conv_cm_4_id].connector[0].ssbo_offset = 57280;
  graph->node[direct_conv_cm_4_id].connector[1].ssbo_offset = 75712;
  graph->node[direct_conv_cm_5_id].connector[0].ssbo_offset = 75776;
  graph->node[direct_conv_cm_5_id].connector[1].ssbo_offset = 94208;
  graph->node[direct_conv_cm_6_id].connector[0].ssbo_offset = 94272;
  graph->node[direct_conv_cm_6_id].connector[1].ssbo_offset = 112704;
  graph->node[concat_conv_cm_id].connector[0].ssbo_offset = 112768;
  graph->node[concat_conv_cm_id].connector[1].ssbo_offset = 149632;
  graph->node[concat_conv_cm_id].connector[2].ssbo_offset = 186496;
  graph->node[direct_conv_cm_7_id].connector[0].ssbo_offset = 186624;
  graph->node[direct_conv_cm_7_id].connector[1].ssbo_offset = 260352;
  graph->node[concat_conv_cm_1_id].connector[0].ssbo_offset = 260480;
  graph->node[concat_conv_cm_1_id].connector[1].ssbo_offset = 334208;
  graph->node[concat_conv_cm_1_id].connector[2].ssbo_offset = 371072;
  graph->node[direct_conv_cm_8_id].connector[0].ssbo_offset = 371200;
  graph->node[direct_conv_cm_8_id].connector[1].ssbo_offset = 444928;
  graph->node[concat_conv_cm_2_id].connector[0].ssbo_offset = 445056;
  graph->node[concat_conv_cm_2_id].connector[1].ssbo_offset = 518784;
  graph->node[concat_conv_cm_2_id].connector[2].ssbo_offset = 555648;
  graph->node[direct_conv_cm_9_id].connector[0].ssbo_offset = 555776;
  graph->node[direct_conv_cm_9_id].connector[1].ssbo_offset = 592640;
  graph->node[concat_conv_cm_3_id].connector[0].ssbo_offset = 592704;
  graph->node[concat_conv_cm_3_id].connector[1].ssbo_offset = 611136;
  graph->node[concat_conv_cm_3_id].connector[2].ssbo_offset = 612864;
  graph->node[direct_conv_cm_10_id].connector[0].ssbo_offset = 612928;
  graph->node[direct_conv_cm_10_id].connector[1].ssbo_offset = 631360;
  graph->node[direct_conv_cm_11_id].connector[0].ssbo_offset = 631424;
  graph->node[direct_conv_cm_11_id].connector[1].ssbo_offset = 633152;
}

#endif
