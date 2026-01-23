#ifndef oidn_ildr_DENOX_MODULE_H
#define oidn_ildr_DENOX_MODULE_H
#include "modules/api.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int denox_read_source_ildr(dt_module_t* mod, void* mapped, dt_read_source_params_t* p) {
  if (p->node->kernel == dt_token("weights")) {
    FILE* f = dt_graph_open_resource(mod->graph, 0, "data/oidn-ildr.dat", "rb");
    if (!f) {
      snprintf(mod->graph->gui_msg_buf, sizeof(mod->graph->gui_msg_buf),
            "oidn: could not find \"data/oidn-ildr.dat\"");
      return 1;
    }
    fseek(f, 0, SEEK_END);
    const size_t size = ftell(f);
    const size_t expected_size = 1835008;
    if (size != expected_size) {
      snprintf(mod->graph->gui_msg_buf, sizeof(mod->graph->gui_msg_buf),
            "oidn: weight file \"data/oidn-ildr.dat\" has unexpected size!");
      fclose(f);
      return 1;
    }
    fseek(f, 0, SEEK_SET);
    fread(mapped, size, 1, f);
    fclose(f);
  }
  return 0;
}

static void denox_create_nodes_ildr(dt_graph_t* graph, dt_module_t* module,
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
  const int64_t r17 = r16 + 767;
  const int64_t r18 = r17 / 768;
  const int64_t r19 = r16 - 2;
  const int64_t r20 = r19 / 2;
  const int64_t r21 = r20 + 1;
  const int64_t r22 = r21 + 255;
  const int64_t r23 = r22 / 256;
  const int64_t r24 = r10 + 31;
  const int64_t r25 = r24 / 32;
  const int64_t r26 = ((H % 16) + 16) % 16;
  const int64_t r27 = r26 * -1;
  const int64_t r28 = r27 + 16;
  const int64_t r29 = ((r28 % 16) + 16) % 16;
  const int64_t r30 = H + r29;
  const int64_t r31 = r30 + 12;
  const int64_t r32 = r31 / 13;
  const int64_t r33 = r30 - 2;
  const int64_t r34 = r33 / 2;
  const int64_t r35 = r34 + 1;
  const int64_t r36 = r35 - 2;
  const int64_t r37 = r36 / 2;
  const int64_t r38 = r37 + 1;
  const int64_t r39 = r38 - 2;
  const int64_t r40 = r39 / 2;
  const int64_t r41 = r40 + 1;
  const int64_t r42 = r21 * r41;
  const int64_t r43 = r42 * 128;
  const int64_t r44 = r41 - 2;
  const int64_t r45 = r44 / 2;
  const int64_t r46 = r45 + 1;
  const int64_t r47 = r21 - 2;
  const int64_t r48 = r47 / 2;
  const int64_t r49 = r48 + 1;
  const int64_t r50 = r49 * r46;
  const int64_t r51 = r50 * 192;
  const int64_t r54 = r16 * r38;
  const int64_t r55 = r54 * 96;
  const int64_t r56 = r42 * 224;
  const int64_t r59 = r13 * r35;
  const int64_t r60 = r59 * 64;
  const int64_t r61 = r54 * 192;
  const int64_t r64 = r10 * r30;
  const int64_t r65 = r64 * 6;
  const int64_t r66 = r59 * 128;
  const int64_t r70 = r50 * 160;
  const int64_t r72 = r46 + 5;
  const int64_t r73 = r72 / 6;
  const int64_t r76 = r49 + 15;
  const int64_t r77 = r76 / 16;
  const int64_t r81 = r41 + 8;
  const int64_t r82 = r81 / 9;
  const int64_t r83 = r41 + 3;
  const int64_t r84 = r83 / 4;
  const int64_t r85 = r41 + 1;
  const int64_t r86 = r85 / 2;
  const int64_t r89 = r21 + 15;
  const int64_t r90 = r89 / 16;
  const int64_t r94 = r38 + 5;
  const int64_t r95 = r94 / 6;
  const int64_t r96 = r38 + 4;
  const int64_t r97 = r96 / 5;
  const int64_t r98 = r38 + 1;
  const int64_t r99 = r98 / 2;
  const int64_t r102 = r16 + 15;
  const int64_t r103 = r102 / 16;
  const int64_t r105 = r64 * 64;
  const int64_t r110 = r35 + 11;
  const int64_t r111 = r110 / 12;
  const int64_t r112 = r35 + 9;
  const int64_t r113 = r112 / 10;
  const int64_t r114 = r35 + 1;
  const int64_t r115 = r114 / 2;
  const int64_t r118 = r13 + 15;
  const int64_t r119 = r118 / 16;
  const int64_t r123 = r64 * 128;
  const int64_t r125 = r30 + 5;
  const int64_t r126 = r125 / 6;
  const int64_t r127 = r30 + 7;
  const int64_t r128 = r127 / 8;
  const int64_t r131 = r30 + 3;
  const int64_t r132 = r131 / 4;
  const int64_t r133 = r10 + 15;
  const int64_t r134 = r133 / 16;
  const int64_t r135 = r42 * 160;
  const int64_t r136 = r54 * 128;
  const int64_t r137 = r59 * 96;
  const int64_t r138 = H * W;
  const int64_t r139 = r138 * 6;
  dt_roi_t roi0 = {.wd = 1835008, .ht = 1};
  dt_roi_t roi1 = {.wd = (uint32_t)(r139 / 2), .ht = 1};
  dt_roi_t roi2 = {.wd = (uint32_t)(r65), .ht = 1};
  dt_roi_t roi3 = {.wd = (uint32_t)(r105), .ht = 1};
  dt_roi_t roi4 = {.wd = (uint32_t)(r60), .ht = 1};
  dt_roi_t roi5 = {.wd = (uint32_t)(r137), .ht = 1};
  dt_roi_t roi6 = {.wd = (uint32_t)(r55), .ht = 1};
  dt_roi_t roi7 = {.wd = (uint32_t)(r136), .ht = 1};
  dt_roi_t roi8 = {.wd = (uint32_t)(r43), .ht = 1};
  dt_roi_t roi9 = {.wd = (uint32_t)(r135), .ht = 1};
  dt_roi_t roi10 = {.wd = (uint32_t)(r70), .ht = 1};
  dt_roi_t roi11 = {.wd = (uint32_t)(r51), .ht = 1};
  dt_roi_t roi12 = {.wd = (uint32_t)(r51), .ht = 1};
  dt_roi_t roi13 = {.wd = (uint32_t)(r56), .ht = 1};
  dt_roi_t roi14 = {.wd = (uint32_t)(r56), .ht = 1};
  dt_roi_t roi15 = {.wd = (uint32_t)(r61), .ht = 1};
  dt_roi_t roi16 = {.wd = (uint32_t)(r61), .ht = 1};
  dt_roi_t roi17 = {.wd = (uint32_t)(r66), .ht = 1};
  dt_roi_t roi18 = {.wd = (uint32_t)(r66), .ht = 1};
  dt_roi_t roi19 = {.wd = (uint32_t)(r123), .ht = 1};
  dt_roi_t roi20 = {.wd = (uint32_t)(r105), .ht = 1};
  dt_roi_t roi21 = {.wd = (uint32_t)(r65), .ht = 1};
  dt_roi_t roi22 = {.wd = (uint32_t)(r139 / 2), .ht = 1};
  int weights_id = dt_node_add(graph, module, "oidn", "weights",
      1, 1, 1, 0, NULL, 1, 
      "w", "source", "ssbo", "u8", &roi0);
  // memory_pad (memory_pad.comp)
  const uint32_t memory_pad_pc[6] = {(uint32_t)(r10), (uint32_t)(r30), 0, (uint32_t)(r9), 0, (uint32_t)(r29)};
  const int memory_pad_id = dt_node_add(graph, module, "oidn", "ildr9",
      1 * DT_LOCAL_SIZE_X, r25 * DT_LOCAL_SIZE_Y, r128,
      24, (const int*)memory_pad_pc, 2, //
      "a", "read", "ssbo", "f16", &roi1, // HWC[3] : f16
      "b", "write", "ssbo", "u8", &roi2); // HWC[3] : f16
  // direct_conv (direct_conv.comp)
  const uint32_t direct_conv_pc[2] = {(uint32_t)(r10), (uint32_t)(r30)};
  const int direct_conv_id = dt_node_add(graph, module, "oidn", "ildr6",
      1 * DT_LOCAL_SIZE_X, r134 * DT_LOCAL_SIZE_Y, r132,
      8, (const int*)direct_conv_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi2, // HWC[3] : f16
      "d", "write", "ssbo", "u8", &roi3); // HWC[32] : f16
  // direct_conv_cm (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_pc[2] = {(uint32_t)(r10), (uint32_t)(r30)};
  const int direct_conv_cm_id = dt_node_add(graph, module, "oidn", "ildr7",
      1 * DT_LOCAL_SIZE_X, r134 * DT_LOCAL_SIZE_Y, r128,
      8, (const int*)direct_conv_cm_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi3, // HWC[32] : f16
      "d", "write", "ssbo", "u8", &roi4); // CHWC8[32] : f16
  // direct_conv_1 (direct_conv.comp)
  const uint32_t direct_conv_1_pc[2] = {(uint32_t)(r13), (uint32_t)(r35)};
  const int direct_conv_1_id = dt_node_add(graph, module, "oidn", "ildr8",
      1 * DT_LOCAL_SIZE_X, r119 * DT_LOCAL_SIZE_Y, r115,
      8, (const int*)direct_conv_1_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi4, // CHWC8[32] : f16
      "d", "write", "ssbo", "u8", &roi5); // CHWC8[48] : f16
  // basic_pool (basic_pool.comp)
  const uint32_t basic_pool_pc[2] = {(uint32_t)(r13), (uint32_t)(r35)};
  const int basic_pool_id = dt_node_add(graph, module, "oidn", "ildr17",
      6 * DT_LOCAL_SIZE_X, r18 * DT_LOCAL_SIZE_Y, r38,
      8, (const int*)basic_pool_pc, 2, //
      "a", "read", "ssbo", "u8", &roi5, // CHWC8[48] : f16
      "b", "write", "ssbo", "u8", &roi6); // CHWC8[48] : f16
  // direct_conv_2 (direct_conv.comp)
  const uint32_t direct_conv_2_pc[2] = {(uint32_t)(r16), (uint32_t)(r38)};
  const int direct_conv_2_id = dt_node_add(graph, module, "oidn", "ildr5",
      1 * DT_LOCAL_SIZE_X, r103 * DT_LOCAL_SIZE_Y, r99,
      8, (const int*)direct_conv_2_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi6, // CHWC8[48] : f16
      "d", "write", "ssbo", "u8", &roi7); // CHWC8[64] : f16
  // basic_pool_1 (basic_pool.comp)
  const uint32_t basic_pool_1_pc[2] = {(uint32_t)(r16), (uint32_t)(r38)};
  const int basic_pool_1_id = dt_node_add(graph, module, "oidn", "ildr11",
      8 * DT_LOCAL_SIZE_X, r23 * DT_LOCAL_SIZE_Y, r86,
      8, (const int*)basic_pool_1_pc, 2, //
      "a", "read", "ssbo", "u8", &roi7, // CHWC8[64] : f16
      "b", "write", "ssbo", "u8", &roi8); // CHWC8[64] : f16
  // direct_conv_3 (direct_conv.comp)
  const uint32_t direct_conv_3_pc[2] = {(uint32_t)(r21), (uint32_t)(r41)};
  const int direct_conv_3_id = dt_node_add(graph, module, "oidn", "ildr13",
      1 * DT_LOCAL_SIZE_X, r90 * DT_LOCAL_SIZE_Y, r84,
      8, (const int*)direct_conv_3_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi8, // CHWC8[64] : f16
      "d", "write", "ssbo", "u8", &roi9); // HWC[80] : f16
  // basic_pool_2 (basic_pool.comp)
  const uint32_t basic_pool_2_pc[2] = {(uint32_t)(r21), (uint32_t)(r41)};
  const int basic_pool_2_id = dt_node_add(graph, module, "oidn", "ildr10",
      1 * DT_LOCAL_SIZE_X, r77 * DT_LOCAL_SIZE_Y, r46,
      8, (const int*)basic_pool_2_pc, 2, //
      "a", "read", "ssbo", "u8", &roi9, // HWC[80] : f16
      "b", "write", "ssbo", "u8", &roi10); // HWC[80] : f16
  // direct_conv_cm_1 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_1_pc[2] = {(uint32_t)(r49), (uint32_t)(r46)};
  const int direct_conv_cm_1_id = dt_node_add(graph, module, "oidn", "ildr18",
      1 * DT_LOCAL_SIZE_X, r77 * DT_LOCAL_SIZE_Y, r73,
      8, (const int*)direct_conv_cm_1_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi10, // HWC[80] : f16
      "d", "write", "ssbo", "u8", &roi11); // HWC[96] : f16
  // direct_conv_cm_2 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_2_pc[2] = {(uint32_t)(r49), (uint32_t)(r46)};
  const int direct_conv_cm_2_id = dt_node_add(graph, module, "oidn", "ildr19",
      1 * DT_LOCAL_SIZE_X, r77 * DT_LOCAL_SIZE_Y, r73,
      8, (const int*)direct_conv_cm_2_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi11, // HWC[96] : f16
      "d", "write", "ssbo", "u8", &roi12); // HWC[96] : f16
  // concat_conv_cm (concat_conv_cm.comp)
  const uint32_t concat_conv_cm_pc[2] = {(uint32_t)(r21), (uint32_t)(r41)};
  const int concat_conv_cm_id = dt_node_add(graph, module, "oidn", "ildr15",
      1 * DT_LOCAL_SIZE_X, r90 * DT_LOCAL_SIZE_Y, r86,
      8, (const int*)concat_conv_cm_pc, 6, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi0,// unknown
      "d", "read", "ssbo", "u8", &roi12, // HWC[96] : f16
      "e", "read", "ssbo", "u8", &roi8, // CHWC8[64] : f16
      "f", "write", "ssbo", "u8", &roi13); // CHWC8[112] : f16
  // direct_conv_4 (direct_conv.comp)
  const uint32_t direct_conv_4_pc[2] = {(uint32_t)(r21), (uint32_t)(r41)};
  const int direct_conv_4_id = dt_node_add(graph, module, "oidn", "ildr12",
      1 * DT_LOCAL_SIZE_X, r90 * DT_LOCAL_SIZE_Y, r82,
      8, (const int*)direct_conv_4_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi13, // CHWC8[112] : f16
      "d", "write", "ssbo", "u8", &roi14); // HWC[112] : f16
  // concat_conv_cm_1 (concat_conv_cm.comp)
  const uint32_t concat_conv_cm_1_pc[2] = {(uint32_t)(r16), (uint32_t)(r38)};
  const int concat_conv_cm_1_id = dt_node_add(graph, module, "oidn", "ildr2",
      1 * DT_LOCAL_SIZE_X, r103 * DT_LOCAL_SIZE_Y, r97,
      8, (const int*)concat_conv_cm_1_pc, 6, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi0,// unknown
      "d", "read", "ssbo", "u8", &roi14, // HWC[112] : f16
      "e", "read", "ssbo", "u8", &roi6, // CHWC8[48] : f16
      "f", "write", "ssbo", "u8", &roi15); // HWC[96] : f16
  // direct_conv_cm_3 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_3_pc[2] = {(uint32_t)(r16), (uint32_t)(r38)};
  const int direct_conv_cm_3_id = dt_node_add(graph, module, "oidn", "ildr19",
      1 * DT_LOCAL_SIZE_X, r103 * DT_LOCAL_SIZE_Y, r95,
      8, (const int*)direct_conv_cm_3_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi15, // HWC[96] : f16
      "d", "write", "ssbo", "u8", &roi16); // HWC[96] : f16
  // concat_conv_cm_2 (concat_conv_cm.comp)
  const uint32_t concat_conv_cm_2_pc[2] = {(uint32_t)(r13), (uint32_t)(r35)};
  const int concat_conv_cm_2_id = dt_node_add(graph, module, "oidn", "ildr0",
      1 * DT_LOCAL_SIZE_X, r119 * DT_LOCAL_SIZE_Y, r113,
      8, (const int*)concat_conv_cm_2_pc, 6, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi0,// unknown
      "d", "read", "ssbo", "u8", &roi16, // HWC[96] : f16
      "e", "read", "ssbo", "u8", &roi4, // CHWC8[32] : f16
      "f", "write", "ssbo", "u8", &roi17); // HWC[64] : f16
  // direct_conv_cm_4 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_4_pc[2] = {(uint32_t)(r13), (uint32_t)(r35)};
  const int direct_conv_cm_4_id = dt_node_add(graph, module, "oidn", "ildr16",
      1 * DT_LOCAL_SIZE_X, r119 * DT_LOCAL_SIZE_Y, r111,
      8, (const int*)direct_conv_cm_4_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi17, // HWC[64] : f16
      "d", "write", "ssbo", "u8", &roi18); // CHWC8[64] : f16
  // concat_conv_cm_3 (concat_conv_cm.comp)
  const uint32_t concat_conv_cm_3_pc[2] = {(uint32_t)(r10), (uint32_t)(r30)};
  const int concat_conv_cm_3_id = dt_node_add(graph, module, "oidn", "ildr1",
      1 * DT_LOCAL_SIZE_X, r134 * DT_LOCAL_SIZE_Y, r132,
      8, (const int*)concat_conv_cm_3_pc, 6, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi0,// unknown
      "d", "read", "ssbo", "u8", &roi18, // CHWC8[64] : f16
      "e", "read", "ssbo", "u8", &roi2, // HWC[3] : f16
      "f", "write", "ssbo", "u8", &roi19); // CHWC8[64] : f16
  // direct_conv_5 (direct_conv.comp)
  const uint32_t direct_conv_5_pc[2] = {(uint32_t)(r10), (uint32_t)(r30)};
  const int direct_conv_5_id = dt_node_add(graph, module, "oidn", "ildr4",
      1 * DT_LOCAL_SIZE_X, r134 * DT_LOCAL_SIZE_Y, r126,
      8, (const int*)direct_conv_5_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi19, // CHWC8[64] : f16
      "d", "write", "ssbo", "u8", &roi20); // CHWC8[32] : f16
  // direct_conv_6 (direct_conv.comp)
  const uint32_t direct_conv_6_pc[2] = {(uint32_t)(r10), (uint32_t)(r30)};
  const int direct_conv_6_id = dt_node_add(graph, module, "oidn", "ildr3",
      1 * DT_LOCAL_SIZE_X, r134 * DT_LOCAL_SIZE_Y, r32,
      8, (const int*)direct_conv_6_pc, 3, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi20, // CHWC8[32] : f16
      "c", "write", "ssbo", "u8", &roi21); // HWC[3] : f16
  // memory_slice (memory_slice.comp)
  const uint32_t memory_slice_pc[6] = {0, 0, (uint32_t)(r10), (uint32_t)(r30), (uint32_t)(W), (uint32_t)(H)};
  const int memory_slice_id = dt_node_add(graph, module, "oidn", "ildr14",
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
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_1_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_1_id, "b");
  dt_node_connect_named(graph, basic_pool_2_id, "b", direct_conv_cm_1_id, "c");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_2_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_2_id, "b");
  dt_node_connect_named(graph, direct_conv_cm_1_id, "d", direct_conv_cm_2_id, "c");
  dt_node_connect_named(graph, weights_id, "w", concat_conv_cm_id, "a");
  dt_node_connect_named(graph, weights_id, "w", concat_conv_cm_id, "b");
  dt_node_connect_named(graph, weights_id, "w", concat_conv_cm_id, "c");
  dt_node_connect_named(graph, direct_conv_cm_2_id, "d", concat_conv_cm_id, "d");
  dt_node_connect_named(graph, basic_pool_1_id, "b", concat_conv_cm_id, "e");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_4_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_4_id, "b");
  dt_node_connect_named(graph, concat_conv_cm_id, "f", direct_conv_4_id, "c");
  dt_node_connect_named(graph, weights_id, "w", concat_conv_cm_1_id, "a");
  dt_node_connect_named(graph, weights_id, "w", concat_conv_cm_1_id, "b");
  dt_node_connect_named(graph, weights_id, "w", concat_conv_cm_1_id, "c");
  dt_node_connect_named(graph, direct_conv_4_id, "d", concat_conv_cm_1_id, "d");
  dt_node_connect_named(graph, basic_pool_id, "b", concat_conv_cm_1_id, "e");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_3_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_3_id, "b");
  dt_node_connect_named(graph, concat_conv_cm_1_id, "f", direct_conv_cm_3_id, "c");
  dt_node_connect_named(graph, weights_id, "w", concat_conv_cm_2_id, "a");
  dt_node_connect_named(graph, weights_id, "w", concat_conv_cm_2_id, "b");
  dt_node_connect_named(graph, weights_id, "w", concat_conv_cm_2_id, "c");
  dt_node_connect_named(graph, direct_conv_cm_3_id, "d", concat_conv_cm_2_id, "d");
  dt_node_connect_named(graph, direct_conv_cm_id, "d", concat_conv_cm_2_id, "e");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_4_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_4_id, "b");
  dt_node_connect_named(graph, concat_conv_cm_2_id, "f", direct_conv_cm_4_id, "c");
  dt_node_connect_named(graph, weights_id, "w", concat_conv_cm_3_id, "a");
  dt_node_connect_named(graph, weights_id, "w", concat_conv_cm_3_id, "b");
  dt_node_connect_named(graph, weights_id, "w", concat_conv_cm_3_id, "c");
  dt_node_connect_named(graph, direct_conv_cm_4_id, "d", concat_conv_cm_3_id, "d");
  dt_node_connect_named(graph, memory_pad_id, "b", concat_conv_cm_3_id, "e");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_5_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_5_id, "b");
  dt_node_connect_named(graph, concat_conv_cm_3_id, "f", direct_conv_5_id, "c");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_6_id, "a");
  dt_node_connect_named(graph, direct_conv_5_id, "d", direct_conv_6_id, "b");
  dt_node_connect_named(graph, direct_conv_6_id, "c", memory_slice_id, "a");
  if (output_connector == NULL) {
    dt_connector_copy(graph, module, output_id, memory_slice_id, 1);
  } else {
    dt_node_connect_named(graph, memory_slice_id, "b", output_id, output_connector);
  }
  graph->node[direct_conv_id].connector[1].ssbo_offset = 4608;
  graph->node[direct_conv_cm_id].connector[0].ssbo_offset = 4672;
  graph->node[direct_conv_cm_id].connector[1].ssbo_offset = 23104;
  graph->node[direct_conv_1_id].connector[0].ssbo_offset = 23168;
  graph->node[direct_conv_1_id].connector[1].ssbo_offset = 50816;
  graph->node[direct_conv_2_id].connector[0].ssbo_offset = 50912;
  graph->node[direct_conv_2_id].connector[1].ssbo_offset = 106208;
  graph->node[direct_conv_3_id].connector[0].ssbo_offset = 106336;
  graph->node[direct_conv_3_id].connector[1].ssbo_offset = 198496;
  graph->node[direct_conv_cm_1_id].connector[0].ssbo_offset = 198656;
  graph->node[direct_conv_cm_1_id].connector[1].ssbo_offset = 336896;
  graph->node[direct_conv_cm_2_id].connector[0].ssbo_offset = 337088;
  graph->node[direct_conv_cm_2_id].connector[1].ssbo_offset = 502976;
  graph->node[concat_conv_cm_id].connector[0].ssbo_offset = 503168;
  graph->node[concat_conv_cm_id].connector[1].ssbo_offset = 696704;
  graph->node[concat_conv_cm_id].connector[2].ssbo_offset = 825728;
  graph->node[direct_conv_4_id].connector[0].ssbo_offset = 825952;
  graph->node[direct_conv_4_id].connector[1].ssbo_offset = 1051744;
  graph->node[concat_conv_cm_1_id].connector[0].ssbo_offset = 1051968;
  graph->node[concat_conv_cm_1_id].connector[1].ssbo_offset = 1245504;
  graph->node[concat_conv_cm_1_id].connector[2].ssbo_offset = 1328448;
  graph->node[direct_conv_cm_3_id].connector[0].ssbo_offset = 1328640;
  graph->node[direct_conv_cm_3_id].connector[1].ssbo_offset = 1494528;
  graph->node[concat_conv_cm_2_id].connector[0].ssbo_offset = 1494720;
  graph->node[concat_conv_cm_2_id].connector[1].ssbo_offset = 1605312;
  graph->node[concat_conv_cm_2_id].connector[2].ssbo_offset = 1642176;
  graph->node[direct_conv_cm_4_id].connector[0].ssbo_offset = 1642304;
  graph->node[direct_conv_cm_4_id].connector[1].ssbo_offset = 1716032;
  graph->node[concat_conv_cm_3_id].connector[0].ssbo_offset = 1716160;
  graph->node[concat_conv_cm_3_id].connector[1].ssbo_offset = 1789888;
  graph->node[concat_conv_cm_3_id].connector[2].ssbo_offset = 1793344;
  graph->node[direct_conv_5_id].connector[0].ssbo_offset = 1793472;
  graph->node[direct_conv_5_id].connector[1].ssbo_offset = 1830336;
  graph->node[direct_conv_6_id].connector[0].ssbo_offset = 1830400;
}

#endif
