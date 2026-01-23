#ifndef jddcnn_gxtrn_DENOX_MODULE_H
#define jddcnn_gxtrn_DENOX_MODULE_H
#include "modules/api.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int denox_read_source_gxtrn(dt_module_t* mod, void* mapped, dt_read_source_params_t* p) {
  if (p->node->kernel == dt_token("weights")) {
    FILE* f = dt_graph_open_resource(mod->graph, 0, "data/jddcnn-gxtrn.dat", "rb");
    if (!f) {
      snprintf(mod->graph->gui_msg_buf, sizeof(mod->graph->gui_msg_buf),
            "jddcnn: could not find \"data/jddcnn-gxtrn.dat\"");
      return 1;
    }
    fseek(f, 0, SEEK_END);
    const size_t size = ftell(f);
    const size_t expected_size = 4330336;
    if (size != expected_size) {
      snprintf(mod->graph->gui_msg_buf, sizeof(mod->graph->gui_msg_buf),
            "jddcnn: weight file \"data/jddcnn-gxtrn.dat\" has unexpected size!");
      fclose(f);
      return 1;
    }
    fseek(f, 0, SEEK_SET);
    fread(mapped, size, 1, f);
    fclose(f);
  }
  return 0;
}

static void denox_create_nodes_gxtrn(dt_graph_t* graph, dt_module_t* module,
      uint64_t H, uint64_t W,
      int input_id, const char* input_connector,
      int output_id, const char* output_connector) {
  const int64_t r2 = ((H % 16) + 16) % 16;
  const int64_t r3 = r2 * -1;
  const int64_t r4 = r3 + 16;
  const int64_t r5 = ((r4 % 16) + 16) % 16;
  const int64_t r6 = H + r5;
  const int64_t r7 = r6 - 2;
  const int64_t r8 = r7 / 2;
  const int64_t r9 = r8 + 1;
  const int64_t r10 = r9 - 2;
  const int64_t r11 = r10 / 2;
  const int64_t r12 = r11 + 1;
  const int64_t r13 = r12 - 2;
  const int64_t r14 = r13 / 2;
  const int64_t r15 = r14 + 1;
  const int64_t r16 = r15 - 2;
  const int64_t r17 = r16 / 2;
  const int64_t r18 = r17 + 1;
  const int64_t r19 = ((W % 16) + 16) % 16;
  const int64_t r20 = r19 * -1;
  const int64_t r21 = r20 + 16;
  const int64_t r22 = ((r21 % 16) + 16) % 16;
  const int64_t r23 = W + r22;
  const int64_t r24 = r23 - 2;
  const int64_t r25 = r24 / 2;
  const int64_t r26 = r25 + 1;
  const int64_t r27 = r26 - 2;
  const int64_t r28 = r27 / 2;
  const int64_t r29 = r28 + 1;
  const int64_t r30 = r29 - 2;
  const int64_t r31 = r30 / 2;
  const int64_t r32 = r31 + 1;
  const int64_t r33 = r32 - 2;
  const int64_t r34 = r33 / 2;
  const int64_t r35 = r34 + 1;
  const int64_t r36 = r35 * r18;
  const int64_t r38 = r32 + 351;
  const int64_t r39 = r38 / 352;
  const int64_t r40 = r32 * r15;
  const int64_t r42 = r29 + 351;
  const int64_t r43 = r42 / 352;
  const int64_t r44 = r29 * r12;
  const int64_t r46 = r26 * r9;
  const int64_t r48 = r23 * r6;
  const int64_t r50 = r23 + 42;
  const int64_t r51 = r50 / 43;
  const int64_t r52 = r23 + 79;
  const int64_t r53 = r52 / 80;
  const int64_t r54 = r23 + 351;
  const int64_t r55 = r54 / 352;
  const int64_t r56 = r23 + 31;
  const int64_t r57 = r56 / 32;
  const int64_t r59 = W + 47;
  const int64_t r60 = r59 / 48;
  const int64_t r61 = r23 + 111;
  const int64_t r62 = r61 / 112;
  const int64_t r63 = r23 + 159;
  const int64_t r64 = r63 / 160;
  const int64_t r65 = r35 + 127;
  const int64_t r66 = r65 / 128;
  const int64_t r67 = r35 + 255;
  const int64_t r68 = r67 / 256;
  const int64_t r69 = r29 + 895;
  const int64_t r70 = r69 / 896;
  const int64_t r71 = r29 + 639;
  const int64_t r72 = r71 / 640;
  const int64_t r73 = r26 + 1535;
  const int64_t r74 = r73 / 1536;
  const int64_t r75 = r26 + 159;
  const int64_t r76 = r75 / 160;
  const int64_t r77 = r32 + 63;
  const int64_t r78 = r77 / 64;
  const int64_t r79 = r32 + 127;
  const int64_t r80 = r79 / 128;
  const int64_t r81 = r29 + 127;
  const int64_t r82 = r81 / 128;
  const int64_t r83 = r26 + 127;
  const int64_t r84 = r83 / 128;
  const int64_t r85 = r23 + 255;
  const int64_t r86 = r85 / 256;
  const int64_t r87 = r40 * 256;
  const int64_t r90 = r36 * 512;
  const int64_t r92 = r18 + 1;
  const int64_t r93 = r92 / 2;
  const int64_t r94 = r35 + 15;
  const int64_t r95 = r94 / 16;
  const int64_t r96 = r44 * 128;
  const int64_t r99 = r40 * 512;
  const int64_t r101 = r15 + 8;
  const int64_t r102 = r101 / 9;
  const int64_t r103 = r15 + 2;
  const int64_t r104 = r103 / 3;
  const int64_t r105 = r15 + 1;
  const int64_t r106 = r105 / 2;
  const int64_t r109 = r32 + 15;
  const int64_t r110 = r109 / 16;
  const int64_t r111 = r46 * 64;
  const int64_t r114 = r44 * 256;
  const int64_t r116 = r12 + 7;
  const int64_t r117 = r116 / 8;
  const int64_t r120 = r12 + 1;
  const int64_t r121 = r120 / 2;
  const int64_t r122 = r29 + 15;
  const int64_t r123 = r122 / 16;
  const int64_t r125 = r48 * 64;
  const int64_t r128 = r46 * 128;
  const int64_t r130 = r9 + 7;
  const int64_t r131 = r130 / 8;
  const int64_t r132 = r9 + 3;
  const int64_t r133 = r132 / 4;
  const int64_t r134 = r9 + 1;
  const int64_t r135 = r134 / 2;
  const int64_t r138 = r26 + 15;
  const int64_t r139 = r138 / 16;
  const int64_t r140 = r6 + 1;
  const int64_t r141 = r140 / 2;
  const int64_t r142 = r48 * 32;
  const int64_t r146 = r6 + 4;
  const int64_t r147 = r146 / 5;
  const int64_t r150 = r6 + 3;
  const int64_t r151 = r150 / 4;
  const int64_t r152 = r23 + 15;
  const int64_t r153 = r152 / 16;
  const int64_t r154 = H * W;
  const int64_t r155 = r154 * 32;
  const int64_t r156 = r48 * 24;
  const int64_t r157 = r154 * 24;
  dt_roi_t roi0 = {.wd = 4330336, .ht = 1};
  dt_roi_t roi1 = {.wd = (uint32_t)(r155 / 2), .ht = 1};
  dt_roi_t roi2 = {.wd = (uint32_t)(r142), .ht = 1};
  dt_roi_t roi3 = {.wd = (uint32_t)(r125), .ht = 1};
  dt_roi_t roi4 = {.wd = (uint32_t)(r125), .ht = 1};
  dt_roi_t roi5 = {.wd = (uint32_t)(r125), .ht = 1};
  dt_roi_t roi6 = {.wd = (uint32_t)(r125), .ht = 1};
  dt_roi_t roi7 = {.wd = (uint32_t)(r128), .ht = 1};
  dt_roi_t roi8 = {.wd = (uint32_t)(r111), .ht = 1};
  dt_roi_t roi9 = {.wd = (uint32_t)(r111), .ht = 1};
  dt_roi_t roi10 = {.wd = (uint32_t)(r128), .ht = 1};
  dt_roi_t roi11 = {.wd = (uint32_t)(r128), .ht = 1};
  dt_roi_t roi12 = {.wd = (uint32_t)(r114), .ht = 1};
  dt_roi_t roi13 = {.wd = (uint32_t)(r96), .ht = 1};
  dt_roi_t roi14 = {.wd = (uint32_t)(r96), .ht = 1};
  dt_roi_t roi15 = {.wd = (uint32_t)(r114), .ht = 1};
  dt_roi_t roi16 = {.wd = (uint32_t)(r114), .ht = 1};
  dt_roi_t roi17 = {.wd = (uint32_t)(r99), .ht = 1};
  dt_roi_t roi18 = {.wd = (uint32_t)(r87), .ht = 1};
  dt_roi_t roi19 = {.wd = (uint32_t)(r87), .ht = 1};
  dt_roi_t roi20 = {.wd = (uint32_t)(r99), .ht = 1};
  dt_roi_t roi21 = {.wd = (uint32_t)(r99), .ht = 1};
  dt_roi_t roi22 = {.wd = (uint32_t)(r90), .ht = 1};
  dt_roi_t roi23 = {.wd = (uint32_t)(r90), .ht = 1};
  dt_roi_t roi24 = {.wd = (uint32_t)(r90), .ht = 1};
  dt_roi_t roi25 = {.wd = (uint32_t)(r99), .ht = 1};
  dt_roi_t roi26 = {.wd = (uint32_t)(r87), .ht = 1};
  dt_roi_t roi27 = {.wd = 1, .ht = 1};
  dt_roi_t roi28 = {.wd = (uint32_t)(r87), .ht = 1};
  dt_roi_t roi29 = {.wd = (uint32_t)(r87), .ht = 1};
  dt_roi_t roi30 = {.wd = (uint32_t)(r87), .ht = 1};
  dt_roi_t roi31 = {.wd = (uint32_t)(r87), .ht = 1};
  dt_roi_t roi32 = {.wd = (uint32_t)(r114), .ht = 1};
  dt_roi_t roi33 = {.wd = (uint32_t)(r96), .ht = 1};
  dt_roi_t roi34 = {.wd = (uint32_t)(r96), .ht = 1};
  dt_roi_t roi35 = {.wd = (uint32_t)(r96), .ht = 1};
  dt_roi_t roi36 = {.wd = (uint32_t)(r96), .ht = 1};
  dt_roi_t roi37 = {.wd = (uint32_t)(r96), .ht = 1};
  dt_roi_t roi38 = {.wd = (uint32_t)(r128), .ht = 1};
  dt_roi_t roi39 = {.wd = (uint32_t)(r111), .ht = 1};
  dt_roi_t roi40 = {.wd = (uint32_t)(r111), .ht = 1};
  dt_roi_t roi41 = {.wd = (uint32_t)(r111), .ht = 1};
  dt_roi_t roi42 = {.wd = (uint32_t)(r111), .ht = 1};
  dt_roi_t roi43 = {.wd = (uint32_t)(r111), .ht = 1};
  dt_roi_t roi44 = {.wd = (uint32_t)(r125), .ht = 1};
  dt_roi_t roi45 = {.wd = (uint32_t)(r142), .ht = 1};
  dt_roi_t roi46 = {.wd = (uint32_t)(r142), .ht = 1};
  dt_roi_t roi47 = {.wd = (uint32_t)(r125), .ht = 1};
  dt_roi_t roi48 = {.wd = (uint32_t)(r142), .ht = 1};
  dt_roi_t roi49 = {.wd = (uint32_t)(r142), .ht = 1};
  dt_roi_t roi50 = {.wd = (uint32_t)(r156), .ht = 1};
  dt_roi_t roi51 = {.wd = (uint32_t)(r156), .ht = 1};
  dt_roi_t roi52 = {.wd = (uint32_t)(r157 / 2), .ht = 1};
  int weights_id = dt_node_add(graph, module, "jddcnn", "weights",
      1, 1, 1, 0, NULL, 1, 
      "w", "source", "ssbo", "u8", &roi0);
  // memory_pad (memory_pad.comp)
  const uint32_t memory_pad_pc[6] = {(uint32_t)(r23), (uint32_t)(r6), 0, (uint32_t)(r22), 0, (uint32_t)(r5)};
  const int memory_pad_id = dt_node_add(graph, module, "jddcnn", "gxtrn29",
      1 * DT_LOCAL_SIZE_X, r62 * DT_LOCAL_SIZE_Y, r6,
      24, (const int*)memory_pad_pc, 2, //
      "a", "read", "ssbo", "f16", &roi1, // HWC[16] : f16
      "b", "write", "ssbo", "u8", &roi2); // HWC[16] : f16
  // direct_conv (direct_conv.comp)
  const uint32_t direct_conv_pc[2] = {(uint32_t)(r23), (uint32_t)(r6)};
  const int direct_conv_id = dt_node_add(graph, module, "jddcnn", "gxtrn35",
      1 * DT_LOCAL_SIZE_X, r153 * DT_LOCAL_SIZE_Y, r151,
      8, (const int*)direct_conv_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi2, // HWC[16] : f16
      "d", "write", "ssbo", "u8", &roi3); // HWC[32] : f16
  // basic_activation (basic_activation.comp)
  const uint32_t basic_activation_pc[2] = {(uint32_t)(r23), (uint32_t)(r6)};
  const int basic_activation_id = dt_node_add(graph, module, "jddcnn", "gxtrn25",
      1 * DT_LOCAL_SIZE_X, r51 * DT_LOCAL_SIZE_Y, r6,
      8, (const int*)basic_activation_pc, 2, //
      "a", "read", "ssbo", "u8", &roi3, // HWC[32] : f16
      "b", "write", "ssbo", "u8", &roi4); // HWC[32] : f16
  // direct_conv_1 (direct_conv.comp)
  const uint32_t direct_conv_1_pc[2] = {(uint32_t)(r23), (uint32_t)(r6)};
  const int direct_conv_1_id = dt_node_add(graph, module, "jddcnn", "gxtrn22",
      1 * DT_LOCAL_SIZE_X, r153 * DT_LOCAL_SIZE_Y, r151,
      8, (const int*)direct_conv_1_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi4, // HWC[32] : f16
      "d", "write", "ssbo", "u8", &roi5); // CHWC8[32] : f16
  // basic_activation_1 (basic_activation.comp)
  const uint32_t basic_activation_1_pc[2] = {(uint32_t)(r23), (uint32_t)(r6)};
  const int basic_activation_1_id = dt_node_add(graph, module, "jddcnn", "gxtrn21",
      4 * DT_LOCAL_SIZE_X, r55 * DT_LOCAL_SIZE_Y, r6,
      8, (const int*)basic_activation_1_pc, 2, //
      "a", "read", "ssbo", "u8", &roi5, // CHWC8[32] : f16
      "b", "write", "ssbo", "u8", &roi6); // CHWC8[32] : f16
  // basic_pool (basic_pool.comp)
  const uint32_t basic_pool_pc[2] = {(uint32_t)(r23), (uint32_t)(r6)};
  const int basic_pool_id = dt_node_add(graph, module, "jddcnn", "gxtrn11",
      4 * DT_LOCAL_SIZE_X, r76 * DT_LOCAL_SIZE_Y, r9,
      8, (const int*)basic_pool_pc, 2, //
      "a", "read", "ssbo", "u8", &roi6, // CHWC8[32] : f16
      "b", "write", "ssbo", "u8", &roi7); // CHWC8[32] : f16
  // direct_conv_2 (direct_conv.comp)
  const uint32_t direct_conv_2_pc[2] = {(uint32_t)(r26), (uint32_t)(r9)};
  const int direct_conv_2_id = dt_node_add(graph, module, "jddcnn", "gxtrn18",
      1 * DT_LOCAL_SIZE_X, r139 * DT_LOCAL_SIZE_Y, r133,
      8, (const int*)direct_conv_2_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi7, // CHWC8[32] : f16
      "d", "write", "ssbo", "u8", &roi8); // CHWC8[32] : f16
  // basic_activation_2 (basic_activation.comp)
  const uint32_t basic_activation_2_pc[2] = {(uint32_t)(r26), (uint32_t)(r9)};
  const int basic_activation_2_id = dt_node_add(graph, module, "jddcnn", "gxtrn16",
      4 * DT_LOCAL_SIZE_X, r84 * DT_LOCAL_SIZE_Y, r9,
      8, (const int*)basic_activation_2_pc, 2, //
      "a", "read", "ssbo", "u8", &roi8, // CHWC8[32] : f16
      "b", "write", "ssbo", "u8", &roi9); // CHWC8[32] : f16
  // direct_conv_3 (direct_conv.comp)
  const uint32_t direct_conv_3_pc[2] = {(uint32_t)(r26), (uint32_t)(r9)};
  const int direct_conv_3_id = dt_node_add(graph, module, "jddcnn", "gxtrn15",
      1 * DT_LOCAL_SIZE_X, r139 * DT_LOCAL_SIZE_Y, r131,
      8, (const int*)direct_conv_3_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi9, // CHWC8[32] : f16
      "d", "write", "ssbo", "u8", &roi10); // CHWC8[64] : f16
  // basic_activation_3 (basic_activation.comp)
  const uint32_t basic_activation_3_pc[2] = {(uint32_t)(r26), (uint32_t)(r9)};
  const int basic_activation_3_id = dt_node_add(graph, module, "jddcnn", "gxtrn33",
      8 * DT_LOCAL_SIZE_X, r74 * DT_LOCAL_SIZE_Y, r135,
      8, (const int*)basic_activation_3_pc, 2, //
      "a", "read", "ssbo", "u8", &roi10, // CHWC8[64] : f16
      "b", "write", "ssbo", "u8", &roi11); // CHWC8[64] : f16
  // basic_pool_1 (basic_pool.comp)
  const uint32_t basic_pool_1_pc[2] = {(uint32_t)(r26), (uint32_t)(r9)};
  const int basic_pool_1_id = dt_node_add(graph, module, "jddcnn", "gxtrn10",
      8 * DT_LOCAL_SIZE_X, r70 * DT_LOCAL_SIZE_Y, r121,
      8, (const int*)basic_pool_1_pc, 2, //
      "a", "read", "ssbo", "u8", &roi11, // CHWC8[64] : f16
      "b", "write", "ssbo", "u8", &roi12); // CHWC8[64] : f16
  // direct_conv_4 (direct_conv.comp)
  const uint32_t direct_conv_4_pc[2] = {(uint32_t)(r29), (uint32_t)(r12)};
  const int direct_conv_4_id = dt_node_add(graph, module, "jddcnn", "gxtrn8",
      1 * DT_LOCAL_SIZE_X, r123 * DT_LOCAL_SIZE_Y, r117,
      8, (const int*)direct_conv_4_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi12, // CHWC8[64] : f16
      "d", "write", "ssbo", "u8", &roi13); // CHWC8[64] : f16
  // basic_activation_4 (basic_activation.comp)
  const uint32_t basic_activation_4_pc[2] = {(uint32_t)(r29), (uint32_t)(r12)};
  const int basic_activation_4_id = dt_node_add(graph, module, "jddcnn", "gxtrn24",
      8 * DT_LOCAL_SIZE_X, r82 * DT_LOCAL_SIZE_Y, r12,
      8, (const int*)basic_activation_4_pc, 2, //
      "a", "read", "ssbo", "u8", &roi13, // CHWC8[64] : f16
      "b", "write", "ssbo", "u8", &roi14); // CHWC8[64] : f16
  // direct_conv_5 (direct_conv.comp)
  const uint32_t direct_conv_5_pc[2] = {(uint32_t)(r29), (uint32_t)(r12)};
  const int direct_conv_5_id = dt_node_add(graph, module, "jddcnn", "gxtrn20",
      1 * DT_LOCAL_SIZE_X, r123 * DT_LOCAL_SIZE_Y, r117,
      8, (const int*)direct_conv_5_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi14, // CHWC8[64] : f16
      "d", "write", "ssbo", "u8", &roi15); // CHWC8[128] : f16
  // basic_activation_5 (basic_activation.comp)
  const uint32_t basic_activation_5_pc[2] = {(uint32_t)(r29), (uint32_t)(r12)};
  const int basic_activation_5_id = dt_node_add(graph, module, "jddcnn", "gxtrn26",
      16 * DT_LOCAL_SIZE_X, r43 * DT_LOCAL_SIZE_Y, r12,
      8, (const int*)basic_activation_5_pc, 2, //
      "a", "read", "ssbo", "u8", &roi15, // CHWC8[128] : f16
      "b", "write", "ssbo", "u8", &roi16); // CHWC8[128] : f16
  // basic_pool_2 (basic_pool.comp)
  const uint32_t basic_pool_2_pc[2] = {(uint32_t)(r29), (uint32_t)(r12)};
  const int basic_pool_2_id = dt_node_add(graph, module, "jddcnn", "gxtrn27",
      16 * DT_LOCAL_SIZE_X, r80 * DT_LOCAL_SIZE_Y, r15,
      8, (const int*)basic_pool_2_pc, 2, //
      "a", "read", "ssbo", "u8", &roi16, // CHWC8[128] : f16
      "b", "write", "ssbo", "u8", &roi17); // CHWC8[128] : f16
  // direct_conv_6 (direct_conv.comp)
  const uint32_t direct_conv_6_pc[2] = {(uint32_t)(r32), (uint32_t)(r15)};
  const int direct_conv_6_id = dt_node_add(graph, module, "jddcnn", "gxtrn17",
      1 * DT_LOCAL_SIZE_X, r110 * DT_LOCAL_SIZE_Y, r102,
      8, (const int*)direct_conv_6_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi17, // CHWC8[128] : f16
      "d", "write", "ssbo", "u8", &roi18); // CHWC8[128] : f16
  // basic_activation_6 (basic_activation.comp)
  const uint32_t basic_activation_6_pc[2] = {(uint32_t)(r32), (uint32_t)(r15)};
  const int basic_activation_6_id = dt_node_add(graph, module, "jddcnn", "gxtrn12",
      16 * DT_LOCAL_SIZE_X, r80 * DT_LOCAL_SIZE_Y, r15,
      8, (const int*)basic_activation_6_pc, 2, //
      "a", "read", "ssbo", "u8", &roi18, // CHWC8[128] : f16
      "b", "write", "ssbo", "u8", &roi19); // CHWC8[128] : f16
  // direct_conv_7 (direct_conv.comp)
  const uint32_t direct_conv_7_pc[2] = {(uint32_t)(r32), (uint32_t)(r15)};
  const int direct_conv_7_id = dt_node_add(graph, module, "jddcnn", "gxtrn30",
      1 * DT_LOCAL_SIZE_X, r110 * DT_LOCAL_SIZE_Y, r104,
      8, (const int*)direct_conv_7_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi19, // CHWC8[128] : f16
      "d", "write", "ssbo", "u8", &roi20); // CHWC8[256] : f16
  // basic_activation_7 (basic_activation.comp)
  const uint32_t basic_activation_7_pc[2] = {(uint32_t)(r32), (uint32_t)(r15)};
  const int basic_activation_7_id = dt_node_add(graph, module, "jddcnn", "gxtrn31",
      32 * DT_LOCAL_SIZE_X, r39 * DT_LOCAL_SIZE_Y, r15,
      8, (const int*)basic_activation_7_pc, 2, //
      "a", "read", "ssbo", "u8", &roi20, // CHWC8[256] : f16
      "b", "write", "ssbo", "u8", &roi21); // CHWC8[256] : f16
  // basic_pool_3 (basic_pool.comp)
  const uint32_t basic_pool_3_pc[2] = {(uint32_t)(r32), (uint32_t)(r15)};
  const int basic_pool_3_id = dt_node_add(graph, module, "jddcnn", "gxtrn23",
      32 * DT_LOCAL_SIZE_X, r68 * DT_LOCAL_SIZE_Y, r18,
      8, (const int*)basic_pool_3_pc, 2, //
      "a", "read", "ssbo", "u8", &roi21, // CHWC8[256] : f16
      "b", "write", "ssbo", "u8", &roi22); // CHWC8[256] : f16
  // direct_conv_8 (direct_conv.comp)
  const uint32_t direct_conv_8_pc[2] = {(uint32_t)(r35), (uint32_t)(r18)};
  const int direct_conv_8_id = dt_node_add(graph, module, "jddcnn", "gxtrn13",
      1 * DT_LOCAL_SIZE_X, r95 * DT_LOCAL_SIZE_Y, r93,
      8, (const int*)direct_conv_8_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi22, // CHWC8[256] : f16
      "d", "write", "ssbo", "u8", &roi23); // CHWC8[256] : f16
  // basic_activation_8 (basic_activation.comp)
  const uint32_t basic_activation_8_pc[2] = {(uint32_t)(r35), (uint32_t)(r18)};
  const int basic_activation_8_id = dt_node_add(graph, module, "jddcnn", "gxtrn32",
      32 * DT_LOCAL_SIZE_X, r66 * DT_LOCAL_SIZE_Y, r18,
      8, (const int*)basic_activation_8_pc, 2, //
      "a", "read", "ssbo", "u8", &roi23, // CHWC8[256] : f16
      "b", "write", "ssbo", "u8", &roi24); // CHWC8[256] : f16
  // basic_upsample (basic_upsample.comp)
  const uint32_t basic_upsample_pc[2] = {(uint32_t)(r35), (uint32_t)(r18)};
  const int basic_upsample_id = dt_node_add(graph, module, "jddcnn", "gxtrn9",
      32 * DT_LOCAL_SIZE_X, r78 * DT_LOCAL_SIZE_Y, r106,
      8, (const int*)basic_upsample_pc, 2, //
      "a", "read", "ssbo", "u8", &roi24, // CHWC8[256] : f16
      "b", "write", "ssbo", "u8", &roi25); // CHWC8[256] : f16
  // direct_conv_9 (direct_conv.comp)
  const uint32_t direct_conv_9_pc[2] = {(uint32_t)(r32), (uint32_t)(r15)};
  const int direct_conv_9_id = dt_node_add(graph, module, "jddcnn", "gxtrn34",
      1 * DT_LOCAL_SIZE_X, r110 * DT_LOCAL_SIZE_Y, r102,
      8, (const int*)direct_conv_9_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi25, // CHWC8[256] : f16
      "d", "write", "ssbo", "u8", &roi26); // CHWC8[128] : f16
  // basic_activation_9 (basic_activation.comp)
  const uint32_t basic_activation_9_pc[2] = {(uint32_t)(r32), (uint32_t)(r15)};
  const int basic_activation_9_id = dt_node_add(graph, module, "jddcnn", "gxtrn12",
      16 * DT_LOCAL_SIZE_X, r80 * DT_LOCAL_SIZE_Y, r15,
      8, (const int*)basic_activation_9_pc, 3, //
      "a", "read", "ssbo", "u8", &roi26, // CHWC8[128] : f16
      "b", "read", "ssbo", "u8", &roi17, // CHWC8[128] : f16
      "dummy", "write", "ssbo", "u8", &roi27);//
  // direct_conv_10 (direct_conv.comp)
  const uint32_t direct_conv_10_pc[2] = {(uint32_t)(r32), (uint32_t)(r15)};
  const int direct_conv_10_id = dt_node_add(graph, module, "jddcnn", "gxtrn34",
      1 * DT_LOCAL_SIZE_X, r110 * DT_LOCAL_SIZE_Y, r102,
      8, (const int*)direct_conv_10_pc, 5, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi17, // CHWC8[256] : f16
      "d", "write", "ssbo", "u8", &roi28, // CHWC8[128] : f16
      "z0", "read", "ssbo", "u8", &roi27);//
  // basic_activation_10 (basic_activation.comp)
  const uint32_t basic_activation_10_pc[2] = {(uint32_t)(r32), (uint32_t)(r15)};
  const int basic_activation_10_id = dt_node_add(graph, module, "jddcnn", "gxtrn12",
      16 * DT_LOCAL_SIZE_X, r80 * DT_LOCAL_SIZE_Y, r15,
      8, (const int*)basic_activation_10_pc, 2, //
      "a", "read", "ssbo", "u8", &roi28, // CHWC8[128] : f16
      "b", "write", "ssbo", "u8", &roi29); // CHWC8[128] : f16
  // direct_conv_11 (direct_conv.comp)
  const uint32_t direct_conv_11_pc[2] = {(uint32_t)(r32), (uint32_t)(r15)};
  const int direct_conv_11_id = dt_node_add(graph, module, "jddcnn", "gxtrn17",
      1 * DT_LOCAL_SIZE_X, r110 * DT_LOCAL_SIZE_Y, r102,
      8, (const int*)direct_conv_11_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi29, // CHWC8[128] : f16
      "d", "write", "ssbo", "u8", &roi30); // CHWC8[128] : f16
  // basic_activation_11 (basic_activation.comp)
  const uint32_t basic_activation_11_pc[2] = {(uint32_t)(r32), (uint32_t)(r15)};
  const int basic_activation_11_id = dt_node_add(graph, module, "jddcnn", "gxtrn12",
      16 * DT_LOCAL_SIZE_X, r80 * DT_LOCAL_SIZE_Y, r15,
      8, (const int*)basic_activation_11_pc, 2, //
      "a", "read", "ssbo", "u8", &roi30, // CHWC8[128] : f16
      "b", "write", "ssbo", "u8", &roi31); // CHWC8[128] : f16
  // basic_upsample_1 (basic_upsample.comp)
  const uint32_t basic_upsample_1_pc[2] = {(uint32_t)(r32), (uint32_t)(r15)};
  const int basic_upsample_1_id = dt_node_add(graph, module, "jddcnn", "gxtrn19",
      16 * DT_LOCAL_SIZE_X, r72 * DT_LOCAL_SIZE_Y, r12,
      8, (const int*)basic_upsample_1_pc, 2, //
      "a", "read", "ssbo", "u8", &roi31, // CHWC8[128] : f16
      "b", "write", "ssbo", "u8", &roi32); // CHWC8[128] : f16
  // direct_conv_12 (direct_conv.comp)
  const uint32_t direct_conv_12_pc[2] = {(uint32_t)(r29), (uint32_t)(r12)};
  const int direct_conv_12_id = dt_node_add(graph, module, "jddcnn", "gxtrn36",
      1 * DT_LOCAL_SIZE_X, r123 * DT_LOCAL_SIZE_Y, r117,
      8, (const int*)direct_conv_12_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi32, // CHWC8[128] : f16
      "d", "write", "ssbo", "u8", &roi33); // CHWC8[64] : f16
  // basic_activation_12 (basic_activation.comp)
  const uint32_t basic_activation_12_pc[2] = {(uint32_t)(r29), (uint32_t)(r12)};
  const int basic_activation_12_id = dt_node_add(graph, module, "jddcnn", "gxtrn24",
      8 * DT_LOCAL_SIZE_X, r82 * DT_LOCAL_SIZE_Y, r12,
      8, (const int*)basic_activation_12_pc, 3, //
      "a", "read", "ssbo", "u8", &roi33, // CHWC8[64] : f16
      "b", "read", "ssbo", "u8", &roi12, // CHWC8[64] : f16
      "dummy", "write", "ssbo", "u8", &roi27);//
  // direct_conv_13 (direct_conv.comp)
  const uint32_t direct_conv_13_pc[2] = {(uint32_t)(r29), (uint32_t)(r12)};
  const int direct_conv_13_id = dt_node_add(graph, module, "jddcnn", "gxtrn36",
      1 * DT_LOCAL_SIZE_X, r123 * DT_LOCAL_SIZE_Y, r117,
      8, (const int*)direct_conv_13_pc, 5, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi12, // CHWC8[128] : f16
      "d", "write", "ssbo", "u8", &roi34, // CHWC8[64] : f16
      "z0", "read", "ssbo", "u8", &roi27);//
  // basic_activation_13 (basic_activation.comp)
  const uint32_t basic_activation_13_pc[2] = {(uint32_t)(r29), (uint32_t)(r12)};
  const int basic_activation_13_id = dt_node_add(graph, module, "jddcnn", "gxtrn24",
      8 * DT_LOCAL_SIZE_X, r82 * DT_LOCAL_SIZE_Y, r12,
      8, (const int*)basic_activation_13_pc, 2, //
      "a", "read", "ssbo", "u8", &roi34, // CHWC8[64] : f16
      "b", "write", "ssbo", "u8", &roi35); // CHWC8[64] : f16
  // direct_conv_14 (direct_conv.comp)
  const uint32_t direct_conv_14_pc[2] = {(uint32_t)(r29), (uint32_t)(r12)};
  const int direct_conv_14_id = dt_node_add(graph, module, "jddcnn", "gxtrn8",
      1 * DT_LOCAL_SIZE_X, r123 * DT_LOCAL_SIZE_Y, r117,
      8, (const int*)direct_conv_14_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi35, // CHWC8[64] : f16
      "d", "write", "ssbo", "u8", &roi36); // CHWC8[64] : f16
  // basic_activation_14 (basic_activation.comp)
  const uint32_t basic_activation_14_pc[2] = {(uint32_t)(r29), (uint32_t)(r12)};
  const int basic_activation_14_id = dt_node_add(graph, module, "jddcnn", "gxtrn24",
      8 * DT_LOCAL_SIZE_X, r82 * DT_LOCAL_SIZE_Y, r12,
      8, (const int*)basic_activation_14_pc, 2, //
      "a", "read", "ssbo", "u8", &roi36, // CHWC8[64] : f16
      "b", "write", "ssbo", "u8", &roi37); // CHWC8[64] : f16
  // basic_upsample_2 (basic_upsample.comp)
  const uint32_t basic_upsample_2_pc[2] = {(uint32_t)(r29), (uint32_t)(r12)};
  const int basic_upsample_2_id = dt_node_add(graph, module, "jddcnn", "gxtrn37",
      8 * DT_LOCAL_SIZE_X, r84 * DT_LOCAL_SIZE_Y, r135,
      8, (const int*)basic_upsample_2_pc, 2, //
      "a", "read", "ssbo", "u8", &roi37, // CHWC8[64] : f16
      "b", "write", "ssbo", "u8", &roi38); // CHWC8[64] : f16
  // direct_conv_15 (direct_conv.comp)
  const uint32_t direct_conv_15_pc[2] = {(uint32_t)(r26), (uint32_t)(r9)};
  const int direct_conv_15_id = dt_node_add(graph, module, "jddcnn", "gxtrn38",
      1 * DT_LOCAL_SIZE_X, r139 * DT_LOCAL_SIZE_Y, r133,
      8, (const int*)direct_conv_15_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi38, // CHWC8[64] : f16
      "d", "write", "ssbo", "u8", &roi39); // CHWC8[32] : f16
  // basic_activation_15 (basic_activation.comp)
  const uint32_t basic_activation_15_pc[2] = {(uint32_t)(r26), (uint32_t)(r9)};
  const int basic_activation_15_id = dt_node_add(graph, module, "jddcnn", "gxtrn16",
      4 * DT_LOCAL_SIZE_X, r84 * DT_LOCAL_SIZE_Y, r9,
      8, (const int*)basic_activation_15_pc, 3, //
      "a", "read", "ssbo", "u8", &roi39, // CHWC8[32] : f16
      "b", "read", "ssbo", "u8", &roi7, // CHWC8[32] : f16
      "dummy", "write", "ssbo", "u8", &roi27);//
  // direct_conv_16 (direct_conv.comp)
  const uint32_t direct_conv_16_pc[2] = {(uint32_t)(r26), (uint32_t)(r9)};
  const int direct_conv_16_id = dt_node_add(graph, module, "jddcnn", "gxtrn38",
      1 * DT_LOCAL_SIZE_X, r139 * DT_LOCAL_SIZE_Y, r133,
      8, (const int*)direct_conv_16_pc, 5, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi7, // CHWC8[64] : f16
      "d", "write", "ssbo", "u8", &roi40, // CHWC8[32] : f16
      "z0", "read", "ssbo", "u8", &roi27);//
  // basic_activation_16 (basic_activation.comp)
  const uint32_t basic_activation_16_pc[2] = {(uint32_t)(r26), (uint32_t)(r9)};
  const int basic_activation_16_id = dt_node_add(graph, module, "jddcnn", "gxtrn16",
      4 * DT_LOCAL_SIZE_X, r84 * DT_LOCAL_SIZE_Y, r9,
      8, (const int*)basic_activation_16_pc, 2, //
      "a", "read", "ssbo", "u8", &roi40, // CHWC8[32] : f16
      "b", "write", "ssbo", "u8", &roi41); // CHWC8[32] : f16
  // direct_conv_17 (direct_conv.comp)
  const uint32_t direct_conv_17_pc[2] = {(uint32_t)(r26), (uint32_t)(r9)};
  const int direct_conv_17_id = dt_node_add(graph, module, "jddcnn", "gxtrn18",
      1 * DT_LOCAL_SIZE_X, r139 * DT_LOCAL_SIZE_Y, r133,
      8, (const int*)direct_conv_17_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi41, // CHWC8[32] : f16
      "d", "write", "ssbo", "u8", &roi42); // CHWC8[32] : f16
  // basic_activation_17 (basic_activation.comp)
  const uint32_t basic_activation_17_pc[2] = {(uint32_t)(r26), (uint32_t)(r9)};
  const int basic_activation_17_id = dt_node_add(graph, module, "jddcnn", "gxtrn16",
      4 * DT_LOCAL_SIZE_X, r84 * DT_LOCAL_SIZE_Y, r9,
      8, (const int*)basic_activation_17_pc, 2, //
      "a", "read", "ssbo", "u8", &roi42, // CHWC8[32] : f16
      "b", "write", "ssbo", "u8", &roi43); // CHWC8[32] : f16
  // basic_upsample_3 (basic_upsample.comp)
  const uint32_t basic_upsample_3_pc[2] = {(uint32_t)(r26), (uint32_t)(r9)};
  const int basic_upsample_3_id = dt_node_add(graph, module, "jddcnn", "gxtrn7",
      4 * DT_LOCAL_SIZE_X, r64 * DT_LOCAL_SIZE_Y, r141,
      8, (const int*)basic_upsample_3_pc, 2, //
      "a", "read", "ssbo", "u8", &roi43, // CHWC8[32] : f16
      "b", "write", "ssbo", "u8", &roi44); // CHWC8[32] : f16
  // direct_conv_18 (direct_conv.comp)
  const uint32_t direct_conv_18_pc[2] = {(uint32_t)(r23), (uint32_t)(r6)};
  const int direct_conv_18_id = dt_node_add(graph, module, "jddcnn", "gxtrn5",
      1 * DT_LOCAL_SIZE_X, r153 * DT_LOCAL_SIZE_Y, r151,
      8, (const int*)direct_conv_18_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi44, // CHWC8[32] : f16
      "d", "write", "ssbo", "u8", &roi45); // HWC[16] : f16
  // basic_activation_18 (basic_activation.comp)
  const uint32_t basic_activation_18_pc[2] = {(uint32_t)(r23), (uint32_t)(r6)};
  const int basic_activation_18_id = dt_node_add(graph, module, "jddcnn", "gxtrn28",
      1 * DT_LOCAL_SIZE_X, r53 * DT_LOCAL_SIZE_Y, r141,
      8, (const int*)basic_activation_18_pc, 2, //
      "a", "read", "ssbo", "u8", &roi45, // HWC[16] : f16
      "b", "write", "ssbo", "u8", &roi46); // HWC[16] : f16
  // copy_transform (copy_transform.comp)
  const uint32_t copy_transform_pc[2] = {(uint32_t)(r23), (uint32_t)(r6)};
  const int copy_transform_id = dt_node_add(graph, module, "jddcnn", "gxtrn4",
      2 * DT_LOCAL_SIZE_X, r86 * DT_LOCAL_SIZE_Y, r6,
      8, (const int*)copy_transform_pc, 2, //
      "a", "read", "ssbo", "u8", &roi46, // HWC[16] : f16
      "b", "write", "ssbo", "u8", &roi47); // HWC[32] : f16
  // copy_transform_1 (copy_transform.comp)
  const uint32_t copy_transform_1_pc[2] = {(uint32_t)(r23), (uint32_t)(r6)};
  const int copy_transform_1_id = dt_node_add(graph, module, "jddcnn", "gxtrn14",
      2 * DT_LOCAL_SIZE_X, r86 * DT_LOCAL_SIZE_Y, r6,
      8, (const int*)copy_transform_1_pc, 3, //
      "a", "read", "ssbo", "u8", &roi2, // HWC[16] : f16
      "b", "read", "ssbo", "u8", &roi47, // HWC[32] : f16
      "dummy", "write", "ssbo", "u8", &roi27);//
  // direct_conv_19 (direct_conv.comp)
  const uint32_t direct_conv_19_pc[2] = {(uint32_t)(r23), (uint32_t)(r6)};
  const int direct_conv_19_id = dt_node_add(graph, module, "jddcnn", "gxtrn3",
      1 * DT_LOCAL_SIZE_X, r153 * DT_LOCAL_SIZE_Y, r151,
      8, (const int*)direct_conv_19_pc, 5, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi47, // HWC[32] : f16
      "d", "write", "ssbo", "u8", &roi48, // CHWC8[16] : f16
      "z0", "read", "ssbo", "u8", &roi27);//
  // basic_activation_19 (basic_activation.comp)
  const uint32_t basic_activation_19_pc[2] = {(uint32_t)(r23), (uint32_t)(r6)};
  const int basic_activation_19_id = dt_node_add(graph, module, "jddcnn", "gxtrn2",
      2 * DT_LOCAL_SIZE_X, r55 * DT_LOCAL_SIZE_Y, r6,
      8, (const int*)basic_activation_19_pc, 2, //
      "a", "read", "ssbo", "u8", &roi48, // CHWC8[16] : f16
      "b", "write", "ssbo", "u8", &roi49); // CHWC8[16] : f16
  // direct_conv_20 (direct_conv.comp)
  const uint32_t direct_conv_20_pc[2] = {(uint32_t)(r23), (uint32_t)(r6)};
  const int direct_conv_20_id = dt_node_add(graph, module, "jddcnn", "gxtrn6",
      1 * DT_LOCAL_SIZE_X, r153 * DT_LOCAL_SIZE_Y, r147,
      8, (const int*)direct_conv_20_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi49, // CHWC8[16] : f16
      "d", "write", "ssbo", "u8", &roi50); // HWC[12] : f16
  // basic_activation_20 (basic_activation.comp)
  const uint32_t basic_activation_20_pc[2] = {(uint32_t)(r23), (uint32_t)(r6)};
  const int basic_activation_20_id = dt_node_add(graph, module, "jddcnn", "gxtrn1",
      1 * DT_LOCAL_SIZE_X, r57 * DT_LOCAL_SIZE_Y, r6,
      8, (const int*)basic_activation_20_pc, 2, //
      "a", "read", "ssbo", "u8", &roi50, // HWC[12] : f16
      "b", "write", "ssbo", "u8", &roi51); // HWC[12] : f16
  // memory_slice (memory_slice.comp)
  const uint32_t memory_slice_pc[6] = {0, 0, (uint32_t)(r23), (uint32_t)(r6), (uint32_t)(W), (uint32_t)(H)};
  const int memory_slice_id = dt_node_add(graph, module, "jddcnn", "gxtrn0",
      1 * DT_LOCAL_SIZE_X, r60 * DT_LOCAL_SIZE_Y, H,
      24, (const int*)memory_slice_pc, 2, //
      "a", "write", "ssbo", "f16", &roi52, // HWC[12] : f16
      "b", "read", "ssbo", "u8", &roi51); // HWC[12] : f16
  if (input_connector == NULL) {
    dt_connector_copy(graph, module, input_id, memory_pad_id, 0);
  } else {
    dt_node_connect_named(graph, input_id, input_connector, memory_pad_id, "a");
  }
  dt_node_connect_named(graph, weights_id, "w", direct_conv_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_id, "b");
  dt_node_connect_named(graph, memory_pad_id, "b", direct_conv_id, "c");
  dt_node_connect_named(graph, direct_conv_id, "d", basic_activation_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_1_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_1_id, "b");
  dt_node_connect_named(graph, basic_activation_id, "b", direct_conv_1_id, "c");
  dt_node_connect_named(graph, direct_conv_1_id, "d", basic_activation_1_id, "a");
  dt_node_connect_named(graph, basic_activation_1_id, "b", basic_pool_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_2_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_2_id, "b");
  dt_node_connect_named(graph, basic_pool_id, "b", direct_conv_2_id, "c");
  dt_node_connect_named(graph, direct_conv_2_id, "d", basic_activation_2_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_3_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_3_id, "b");
  dt_node_connect_named(graph, basic_activation_2_id, "b", direct_conv_3_id, "c");
  dt_node_connect_named(graph, direct_conv_3_id, "d", basic_activation_3_id, "a");
  dt_node_connect_named(graph, basic_activation_3_id, "b", basic_pool_1_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_4_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_4_id, "b");
  dt_node_connect_named(graph, basic_pool_1_id, "b", direct_conv_4_id, "c");
  dt_node_connect_named(graph, direct_conv_4_id, "d", basic_activation_4_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_5_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_5_id, "b");
  dt_node_connect_named(graph, basic_activation_4_id, "b", direct_conv_5_id, "c");
  dt_node_connect_named(graph, direct_conv_5_id, "d", basic_activation_5_id, "a");
  dt_node_connect_named(graph, basic_activation_5_id, "b", basic_pool_2_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_6_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_6_id, "b");
  dt_node_connect_named(graph, basic_pool_2_id, "b", direct_conv_6_id, "c");
  dt_node_connect_named(graph, direct_conv_6_id, "d", basic_activation_6_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_7_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_7_id, "b");
  dt_node_connect_named(graph, basic_activation_6_id, "b", direct_conv_7_id, "c");
  dt_node_connect_named(graph, direct_conv_7_id, "d", basic_activation_7_id, "a");
  dt_node_connect_named(graph, basic_activation_7_id, "b", basic_pool_3_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_8_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_8_id, "b");
  dt_node_connect_named(graph, basic_pool_3_id, "b", direct_conv_8_id, "c");
  dt_node_connect_named(graph, direct_conv_8_id, "d", basic_activation_8_id, "a");
  dt_node_connect_named(graph, basic_activation_8_id, "b", basic_upsample_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_9_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_9_id, "b");
  dt_node_connect_named(graph, basic_upsample_id, "b", direct_conv_9_id, "c");
  dt_node_connect_named(graph, direct_conv_9_id, "d", basic_activation_9_id, "a");
  dt_node_connect_named(graph, basic_pool_2_id, "b", basic_activation_9_id, "b");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_10_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_10_id, "b");
  dt_node_connect_named(graph, basic_pool_2_id, "b", direct_conv_10_id, "c");
  dt_node_connect_named(graph, basic_activation_9_id, "dummy", direct_conv_10_id, "z0");
  dt_node_connect_named(graph, direct_conv_10_id, "d", basic_activation_10_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_11_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_11_id, "b");
  dt_node_connect_named(graph, basic_activation_10_id, "b", direct_conv_11_id, "c");
  dt_node_connect_named(graph, direct_conv_11_id, "d", basic_activation_11_id, "a");
  dt_node_connect_named(graph, basic_activation_11_id, "b", basic_upsample_1_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_12_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_12_id, "b");
  dt_node_connect_named(graph, basic_upsample_1_id, "b", direct_conv_12_id, "c");
  dt_node_connect_named(graph, direct_conv_12_id, "d", basic_activation_12_id, "a");
  dt_node_connect_named(graph, basic_pool_1_id, "b", basic_activation_12_id, "b");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_13_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_13_id, "b");
  dt_node_connect_named(graph, basic_pool_1_id, "b", direct_conv_13_id, "c");
  dt_node_connect_named(graph, basic_activation_12_id, "dummy", direct_conv_13_id, "z0");
  dt_node_connect_named(graph, direct_conv_13_id, "d", basic_activation_13_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_14_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_14_id, "b");
  dt_node_connect_named(graph, basic_activation_13_id, "b", direct_conv_14_id, "c");
  dt_node_connect_named(graph, direct_conv_14_id, "d", basic_activation_14_id, "a");
  dt_node_connect_named(graph, basic_activation_14_id, "b", basic_upsample_2_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_15_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_15_id, "b");
  dt_node_connect_named(graph, basic_upsample_2_id, "b", direct_conv_15_id, "c");
  dt_node_connect_named(graph, direct_conv_15_id, "d", basic_activation_15_id, "a");
  dt_node_connect_named(graph, basic_pool_id, "b", basic_activation_15_id, "b");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_16_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_16_id, "b");
  dt_node_connect_named(graph, basic_pool_id, "b", direct_conv_16_id, "c");
  dt_node_connect_named(graph, basic_activation_15_id, "dummy", direct_conv_16_id, "z0");
  dt_node_connect_named(graph, direct_conv_16_id, "d", basic_activation_16_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_17_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_17_id, "b");
  dt_node_connect_named(graph, basic_activation_16_id, "b", direct_conv_17_id, "c");
  dt_node_connect_named(graph, direct_conv_17_id, "d", basic_activation_17_id, "a");
  dt_node_connect_named(graph, basic_activation_17_id, "b", basic_upsample_3_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_18_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_18_id, "b");
  dt_node_connect_named(graph, basic_upsample_3_id, "b", direct_conv_18_id, "c");
  dt_node_connect_named(graph, direct_conv_18_id, "d", basic_activation_18_id, "a");
  dt_node_connect_named(graph, basic_activation_18_id, "b", copy_transform_id, "a");
  dt_node_connect_named(graph, memory_pad_id, "b", copy_transform_1_id, "a");
  dt_node_connect_named(graph, copy_transform_id, "b", copy_transform_1_id, "b");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_19_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_19_id, "b");
  dt_node_connect_named(graph, copy_transform_id, "b", direct_conv_19_id, "c");
  dt_node_connect_named(graph, copy_transform_1_id, "dummy", direct_conv_19_id, "z0");
  dt_node_connect_named(graph, direct_conv_19_id, "d", basic_activation_19_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_20_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_20_id, "b");
  dt_node_connect_named(graph, basic_activation_19_id, "b", direct_conv_20_id, "c");
  dt_node_connect_named(graph, direct_conv_20_id, "d", basic_activation_20_id, "a");
  dt_node_connect_named(graph, basic_activation_20_id, "b", memory_slice_id, "b");
  if (output_connector == NULL) {
    dt_connector_copy(graph, module, output_id, memory_slice_id, 0);
  } else {
    dt_node_connect_named(graph, memory_slice_id, "a", output_id, output_connector);
  }
  graph->node[direct_conv_id].connector[1].ssbo_offset = 9216;
  graph->node[direct_conv_1_id].connector[0].ssbo_offset = 9280;
  graph->node[direct_conv_1_id].connector[1].ssbo_offset = 27712;
  graph->node[basic_pool_id].connector[1].ssbo_offset = r111;
  graph->node[direct_conv_2_id].connector[0].ssbo_offset = 27776;
  graph->node[direct_conv_2_id].connector[1].ssbo_offset = 46208;
  graph->node[direct_conv_2_id].connector[2].ssbo_offset = r111;
  graph->node[direct_conv_3_id].connector[0].ssbo_offset = 46272;
  graph->node[direct_conv_3_id].connector[1].ssbo_offset = 83136;
  graph->node[basic_pool_1_id].connector[1].ssbo_offset = r96;
  graph->node[direct_conv_4_id].connector[0].ssbo_offset = 83264;
  graph->node[direct_conv_4_id].connector[1].ssbo_offset = 156992;
  graph->node[direct_conv_4_id].connector[2].ssbo_offset = r96;
  graph->node[direct_conv_5_id].connector[0].ssbo_offset = 157120;
  graph->node[direct_conv_5_id].connector[1].ssbo_offset = 304576;
  graph->node[basic_pool_2_id].connector[1].ssbo_offset = r87;
  graph->node[direct_conv_6_id].connector[0].ssbo_offset = 304832;
  graph->node[direct_conv_6_id].connector[1].ssbo_offset = 599744;
  graph->node[direct_conv_6_id].connector[2].ssbo_offset = r87;
  graph->node[direct_conv_7_id].connector[0].ssbo_offset = 600000;
  graph->node[direct_conv_7_id].connector[1].ssbo_offset = 1189824;
  graph->node[direct_conv_8_id].connector[0].ssbo_offset = 1190336;
  graph->node[direct_conv_8_id].connector[1].ssbo_offset = 2369984;
  graph->node[direct_conv_9_id].connector[0].ssbo_offset = 2370496;
  graph->node[direct_conv_9_id].connector[1].ssbo_offset = 2960320;
  graph->node[direct_conv_10_id].connector[0].ssbo_offset = 2960576;
  graph->node[direct_conv_10_id].connector[1].ssbo_offset = 3550400;
  graph->node[direct_conv_11_id].connector[0].ssbo_offset = 3550656;
  graph->node[direct_conv_11_id].connector[1].ssbo_offset = 3845568;
  graph->node[direct_conv_12_id].connector[0].ssbo_offset = 3845824;
  graph->node[direct_conv_12_id].connector[1].ssbo_offset = 3993280;
  graph->node[direct_conv_13_id].connector[0].ssbo_offset = 3993408;
  graph->node[direct_conv_13_id].connector[1].ssbo_offset = 4140864;
  graph->node[direct_conv_14_id].connector[0].ssbo_offset = 4140992;
  graph->node[direct_conv_14_id].connector[1].ssbo_offset = 4214720;
  graph->node[direct_conv_15_id].connector[0].ssbo_offset = 4214848;
  graph->node[direct_conv_15_id].connector[1].ssbo_offset = 4251712;
  graph->node[direct_conv_16_id].connector[0].ssbo_offset = 4251776;
  graph->node[direct_conv_16_id].connector[1].ssbo_offset = 4288640;
  graph->node[direct_conv_17_id].connector[0].ssbo_offset = 4288704;
  graph->node[direct_conv_17_id].connector[1].ssbo_offset = 4307136;
  graph->node[direct_conv_18_id].connector[0].ssbo_offset = 4307200;
  graph->node[direct_conv_18_id].connector[1].ssbo_offset = 4316416;
  graph->node[direct_conv_19_id].connector[0].ssbo_offset = 4316448;
  graph->node[direct_conv_19_id].connector[1].ssbo_offset = 4325664;
  graph->node[direct_conv_20_id].connector[0].ssbo_offset = 4325696;
  graph->node[direct_conv_20_id].connector[1].ssbo_offset = 4330304;
}

#endif
