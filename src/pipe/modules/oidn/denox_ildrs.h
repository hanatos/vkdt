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
    const size_t expected_size = 633184;
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
  const int64_t r2 = W + 191;
  const int64_t r3 = r2 / 192;
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
  const int64_t r16 = r15 + 1;
  const int64_t r17 = r16 / 2;
  const int64_t r18 = r8 + 63;
  const int64_t r19 = r18 / 64;
  const int64_t r20 = r15 - 2;
  const int64_t r21 = r20 / 2;
  const int64_t r22 = r21 + 1;
  const int64_t r23 = r22 - 2;
  const int64_t r24 = r23 / 2;
  const int64_t r25 = r24 + 1;
  const int64_t r26 = r25 - 2;
  const int64_t r27 = r26 / 2;
  const int64_t r28 = r27 + 1;
  const int64_t r29 = r8 - 2;
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
  const int64_t r62 = r42 + 3;
  const int64_t r63 = r62 / 4;
  const int64_t r64 = r45 + 7;
  const int64_t r65 = r64 / 8;
  const int64_t r68 = r28 + 11;
  const int64_t r69 = r68 / 12;
  const int64_t r70 = r28 + 15;
  const int64_t r71 = r70 / 16;
  const int64_t r74 = r28 + 7;
  const int64_t r75 = r74 / 8;
  const int64_t r76 = r37 + 7;
  const int64_t r77 = r76 / 8;
  const int64_t r81 = r25 + 15;
  const int64_t r82 = r81 / 16;
  const int64_t r85 = r34 + 7;
  const int64_t r86 = r85 / 8;
  const int64_t r90 = r22 + 15;
  const int64_t r91 = r90 / 16;
  const int64_t r92 = r22 + 7;
  const int64_t r93 = r92 / 8;
  const int64_t r95 = r55 * 128;
  const int64_t r97 = r22 + 3;
  const int64_t r98 = r97 / 4;
  const int64_t r99 = r31 + 7;
  const int64_t r100 = r99 / 8;
  const int64_t r101 = r8 * r15;
  const int64_t r102 = r101 * 6;
  const int64_t r105 = r101 * 70;
  const int64_t r107 = r15 + 31;
  const int64_t r108 = r107 / 32;
  const int64_t r110 = r101 * 64;
  const int64_t r112 = r15 + 3;
  const int64_t r113 = r112 / 4;
  const int64_t r114 = r15 + 11;
  const int64_t r115 = r114 / 12;
  const int64_t r116 = r15 + 15;
  const int64_t r117 = r116 / 16;
  const int64_t r120 = r15 + 7;
  const int64_t r121 = r120 / 8;
  const int64_t r122 = r8 + 7;
  const int64_t r123 = r122 / 8;
  const int64_t r124 = H * W;
  const int64_t r125 = r124 * 6;
  dt_roi_t roi0 = {.wd = 633184, .ht = 1};
  dt_roi_t roi1 = {.wd = (uint32_t)(r125 / 2), .ht = 1};
  dt_roi_t roi2 = {.wd = (uint32_t)(r102), .ht = 1};
  dt_roi_t roi3 = {.wd = (uint32_t)(r110), .ht = 1};
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
  dt_roi_t roi14 = {.wd = (uint32_t)(r95), .ht = 1};
  dt_roi_t roi15 = {.wd = (uint32_t)(r56), .ht = 1};
  dt_roi_t roi16 = {.wd = (uint32_t)(r110), .ht = 1};
  dt_roi_t roi17 = {.wd = (uint32_t)(r105), .ht = 1};
  dt_roi_t roi18 = {.wd = 1, .ht = 1};
  dt_roi_t roi19 = {.wd = (uint32_t)(r110), .ht = 1};
  dt_roi_t roi20 = {.wd = (uint32_t)(r110), .ht = 1};
  dt_roi_t roi21 = {.wd = (uint32_t)(r102), .ht = 1};
  dt_roi_t roi22 = {.wd = (uint32_t)(r125 / 2), .ht = 1};
  int weights_id = dt_node_add(graph, module, "oidn", "weights",
      1, 1, 1, 0, NULL, 1, 
      "w", "source", "ssbo", "u8", &roi0);
  // memory_pad (memory_pad.comp)
  const uint32_t memory_pad_pc[6] = {(uint32_t)(r8), (uint32_t)(r15), 0, (uint32_t)(r7), 0, (uint32_t)(r14)};
  const int memory_pad_id = dt_node_add(graph, module, "oidn", "ildrs3",
      1 * DT_LOCAL_SIZE_X, r10 * DT_LOCAL_SIZE_Y, r113,
      24, (const int*)memory_pad_pc, 2, //
      "a", "read", "ssbo", "f16", &roi1, // HWC[3] : f16
      "b", "write", "ssbo", "u8", &roi2); // HWC[3] : f16
  // direct_conv_cm (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_pc[2] = {(uint32_t)(r8), (uint32_t)(r15)};
  const int direct_conv_cm_id = dt_node_add(graph, module, "oidn", "ildrs7",
      1 * DT_LOCAL_SIZE_X, r123 * DT_LOCAL_SIZE_Y, r115,
      8, (const int*)direct_conv_cm_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi2, // HWC[3] : f16
      "d", "write", "ssbo", "u8", &roi3); // HWC[32] : f16
  // direct_conv_cm_1 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_1_pc[2] = {(uint32_t)(r8), (uint32_t)(r15)};
  const int direct_conv_cm_1_id = dt_node_add(graph, module, "oidn", "ildrs8",
      1 * DT_LOCAL_SIZE_X, r123 * DT_LOCAL_SIZE_Y, r108,
      8, (const int*)direct_conv_cm_1_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi3, // HWC[32] : f16
      "d", "write", "ssbo", "u8", &roi4); // HWC[32] : f16
  // direct_conv_cm_2 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_2_pc[2] = {(uint32_t)(r31), (uint32_t)(r22)};
  const int direct_conv_cm_2_id = dt_node_add(graph, module, "oidn", "ildrs10",
      1 * DT_LOCAL_SIZE_X, r100 * DT_LOCAL_SIZE_Y, r98,
      8, (const int*)direct_conv_cm_2_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi4, // HWC[32] : f16
      "d", "write", "ssbo", "u8", &roi5); // CHWC8[32] : f16
  // direct_conv_cm_3 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_3_pc[2] = {(uint32_t)(r34), (uint32_t)(r25)};
  const int direct_conv_cm_3_id = dt_node_add(graph, module, "oidn", "ildrs13",
      1 * DT_LOCAL_SIZE_X, r86 * DT_LOCAL_SIZE_Y, r82,
      8, (const int*)direct_conv_cm_3_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi5, // CHWC8[32] : f16
      "d", "write", "ssbo", "u8", &roi6); // CHWC8[32] : f16
  // direct_conv_cm_4 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_4_pc[2] = {(uint32_t)(r37), (uint32_t)(r28)};
  const int direct_conv_cm_4_id = dt_node_add(graph, module, "oidn", "ildrs12",
      1 * DT_LOCAL_SIZE_X, r77 * DT_LOCAL_SIZE_Y, r69,
      8, (const int*)direct_conv_cm_4_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi6, // CHWC8[32] : f16
      "d", "write", "ssbo", "u8", &roi7); // CHWC8[32] : f16
  // direct_conv_cm_5 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_5_pc[2] = {(uint32_t)(r45), (uint32_t)(r42)};
  const int direct_conv_cm_5_id = dt_node_add(graph, module, "oidn", "ildrs14",
      1 * DT_LOCAL_SIZE_X, r65 * DT_LOCAL_SIZE_Y, r63,
      8, (const int*)direct_conv_cm_5_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi7, // CHWC8[32] : f16
      "d", "write", "ssbo", "u8", &roi8); // HWC[32] : f16
  // direct_conv_cm_6 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_6_pc[2] = {(uint32_t)(r45), (uint32_t)(r42)};
  const int direct_conv_cm_6_id = dt_node_add(graph, module, "oidn", "ildrs5",
      1 * DT_LOCAL_SIZE_X, r65 * DT_LOCAL_SIZE_Y, r63,
      8, (const int*)direct_conv_cm_6_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi8, // HWC[32] : f16
      "d", "write", "ssbo", "u8", &roi9); // HWC[32] : f16
  // concat_conv_cm (concat_conv_cm.comp)
  const uint32_t concat_conv_cm_pc[2] = {(uint32_t)(r37), (uint32_t)(r28)};
  const int concat_conv_cm_id = dt_node_add(graph, module, "oidn", "ildrs16",
      1 * DT_LOCAL_SIZE_X, r77 * DT_LOCAL_SIZE_Y, r71,
      8, (const int*)concat_conv_cm_pc, 6, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi0,// unknown
      "d", "read", "ssbo", "u8", &roi9, // HWC[32] : f16
      "e", "read", "ssbo", "u8", &roi6, // CHWC8[32] : f16
      "f", "write", "ssbo", "u8", &roi10); // CHWC8[64] : f16
  // direct_conv_cm_7 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_7_pc[2] = {(uint32_t)(r37), (uint32_t)(r28)};
  const int direct_conv_cm_7_id = dt_node_add(graph, module, "oidn", "ildrs17",
      1 * DT_LOCAL_SIZE_X, r77 * DT_LOCAL_SIZE_Y, r75,
      8, (const int*)direct_conv_cm_7_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi10, // CHWC8[64] : f16
      "d", "write", "ssbo", "u8", &roi11); // HWC[64] : f16
  // concat_conv_cm_1 (concat_conv_cm.comp)
  const uint32_t concat_conv_cm_1_pc[2] = {(uint32_t)(r34), (uint32_t)(r25)};
  const int concat_conv_cm_1_id = dt_node_add(graph, module, "oidn", "ildrs15",
      1 * DT_LOCAL_SIZE_X, r86 * DT_LOCAL_SIZE_Y, r82,
      8, (const int*)concat_conv_cm_1_pc, 6, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi0,// unknown
      "d", "read", "ssbo", "u8", &roi11, // HWC[64] : f16
      "e", "read", "ssbo", "u8", &roi5, // CHWC8[32] : f16
      "f", "write", "ssbo", "u8", &roi12); // HWC[64] : f16
  // direct_conv_cm_8 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_8_pc[2] = {(uint32_t)(r34), (uint32_t)(r25)};
  const int direct_conv_cm_8_id = dt_node_add(graph, module, "oidn", "ildrs18",
      1 * DT_LOCAL_SIZE_X, r86 * DT_LOCAL_SIZE_Y, r82,
      8, (const int*)direct_conv_cm_8_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi12, // HWC[64] : f16
      "d", "write", "ssbo", "u8", &roi13); // CHWC8[64] : f16
  // concat_conv_cm_2 (concat_conv_cm.comp)
  const uint32_t concat_conv_cm_2_pc[2] = {(uint32_t)(r31), (uint32_t)(r22)};
  const int concat_conv_cm_2_id = dt_node_add(graph, module, "oidn", "ildrs20",
      1 * DT_LOCAL_SIZE_X, r100 * DT_LOCAL_SIZE_Y, r91,
      8, (const int*)concat_conv_cm_2_pc, 6, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi0,// unknown
      "d", "read", "ssbo", "u8", &roi13, // CHWC8[64] : f16
      "e", "read", "ssbo", "u8", &roi4, // HWC[32] : f16
      "f", "write", "ssbo", "u8", &roi14); // HWC[64] : f16
  // direct_conv_cm_9 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_9_pc[2] = {(uint32_t)(r31), (uint32_t)(r22)};
  const int direct_conv_cm_9_id = dt_node_add(graph, module, "oidn", "ildrs9",
      1 * DT_LOCAL_SIZE_X, r100 * DT_LOCAL_SIZE_Y, r93,
      8, (const int*)direct_conv_cm_9_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi14, // HWC[64] : f16
      "d", "write", "ssbo", "u8", &roi15); // HWC[32] : f16
  // basic_upsample (basic_upsample.comp)
  const uint32_t basic_upsample_pc[2] = {(uint32_t)(r31), (uint32_t)(r22)};
  const int basic_upsample_id = dt_node_add(graph, module, "oidn", "ildrs6",
      1 * DT_LOCAL_SIZE_X, r19 * DT_LOCAL_SIZE_Y, r17,
      8, (const int*)basic_upsample_pc, 2, //
      "a", "read", "ssbo", "u8", &roi15, // HWC[32] : f16
      "b", "write", "ssbo", "u8", &roi16); // HWC[32] : f16
  // copy_transform (copy_transform.comp)
  const uint32_t copy_transform_pc[2] = {(uint32_t)(r8), (uint32_t)(r15)};
  const int copy_transform_id = dt_node_add(graph, module, "oidn", "ildrs4",
      1 * DT_LOCAL_SIZE_X, r19 * DT_LOCAL_SIZE_Y, r15,
      8, (const int*)copy_transform_pc, 2, //
      "a", "read", "ssbo", "u8", &roi16, // HWC[32] : f16
      "b", "write", "ssbo", "u8", &roi17); // HWC[35] : f16
  // copy_transform_1 (copy_transform.comp)
  const uint32_t copy_transform_1_pc[2] = {(uint32_t)(r8), (uint32_t)(r15)};
  const int copy_transform_1_id = dt_node_add(graph, module, "oidn", "ildrs2",
      1 * DT_LOCAL_SIZE_X, r19 * DT_LOCAL_SIZE_Y, r17,
      8, (const int*)copy_transform_1_pc, 3, //
      "a", "read", "ssbo", "u8", &roi2, // HWC[3] : f16
      "b", "read", "ssbo", "u8", &roi17, // HWC[35] : f16
      "dummy", "write", "ssbo", "u8", &roi18);//
  // direct_conv_cm_10 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_10_pc[2] = {(uint32_t)(r8), (uint32_t)(r15)};
  const int direct_conv_cm_10_id = dt_node_add(graph, module, "oidn", "ildrs19",
      1 * DT_LOCAL_SIZE_X, r123 * DT_LOCAL_SIZE_Y, r121,
      8, (const int*)direct_conv_cm_10_pc, 5, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi17, // HWC[35] : f16
      "d", "write", "ssbo", "u8", &roi19, // HWC[32] : f16
      "z0", "read", "ssbo", "u8", &roi18);//
  // direct_conv_cm_11 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_11_pc[2] = {(uint32_t)(r8), (uint32_t)(r15)};
  const int direct_conv_cm_11_id = dt_node_add(graph, module, "oidn", "ildrs1",
      1 * DT_LOCAL_SIZE_X, r123 * DT_LOCAL_SIZE_Y, r117,
      8, (const int*)direct_conv_cm_11_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi19, // HWC[32] : f16
      "d", "write", "ssbo", "u8", &roi20); // CHWC8[32] : f16
  // direct_conv_cm_12 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_12_pc[2] = {(uint32_t)(r8), (uint32_t)(r15)};
  const int direct_conv_cm_12_id = dt_node_add(graph, module, "oidn", "ildrs0",
      1 * DT_LOCAL_SIZE_X, r123 * DT_LOCAL_SIZE_Y, r117,
      8, (const int*)direct_conv_cm_12_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi20, // CHWC8[32] : f16
      "d", "write", "ssbo", "u8", &roi21); // HWC[3] : f16
  // memory_slice (memory_slice.comp)
  const uint32_t memory_slice_pc[6] = {0, 0, (uint32_t)(r8), (uint32_t)(r15), (uint32_t)(W), (uint32_t)(H)};
  const int memory_slice_id = dt_node_add(graph, module, "oidn", "ildrs11",
      1 * DT_LOCAL_SIZE_X, r3 * DT_LOCAL_SIZE_Y, H,
      24, (const int*)memory_slice_pc, 2, //
      "a", "write", "ssbo", "f16", &roi22, // HWC[3] : f16
      "b", "read", "ssbo", "u8", &roi21); // HWC[3] : f16
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
  dt_node_connect_named(graph, direct_conv_cm_9_id, "d", basic_upsample_id, "a");
  dt_node_connect_named(graph, basic_upsample_id, "b", copy_transform_id, "a");
  dt_node_connect_named(graph, memory_pad_id, "b", copy_transform_1_id, "a");
  dt_node_connect_named(graph, copy_transform_id, "b", copy_transform_1_id, "b");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_10_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_10_id, "b");
  dt_node_connect_named(graph, copy_transform_id, "b", direct_conv_cm_10_id, "c");
  dt_node_connect_named(graph, copy_transform_1_id, "dummy", direct_conv_cm_10_id, "z0");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_11_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_11_id, "b");
  dt_node_connect_named(graph, direct_conv_cm_10_id, "d", direct_conv_cm_11_id, "c");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_12_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_12_id, "b");
  dt_node_connect_named(graph, direct_conv_cm_11_id, "d", direct_conv_cm_12_id, "c");
  dt_node_connect_named(graph, direct_conv_cm_12_id, "d", memory_slice_id, "b");
  if (output_connector == NULL) {
    dt_connector_copy(graph, module, output_id, memory_slice_id, 0);
  } else {
    dt_node_connect_named(graph, memory_slice_id, "a", output_id, output_connector);
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
  graph->node[direct_conv_cm_10_id].connector[0].ssbo_offset = 592704;
  graph->node[direct_conv_cm_10_id].connector[1].ssbo_offset = 612864;
  graph->node[direct_conv_cm_11_id].connector[0].ssbo_offset = 612928;
  graph->node[direct_conv_cm_11_id].connector[1].ssbo_offset = 631360;
  graph->node[direct_conv_cm_12_id].connector[0].ssbo_offset = 631424;
  graph->node[direct_conv_cm_12_id].connector[1].ssbo_offset = 633152;
}

#endif
