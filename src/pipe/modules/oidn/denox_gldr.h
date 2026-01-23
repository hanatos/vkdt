#ifndef oidn_gldr_DENOX_MODULE_H
#define oidn_gldr_DENOX_MODULE_H
#include "modules/api.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int denox_read_source_gldr(dt_module_t* mod, void* mapped, dt_read_source_params_t* p) {
  if (p->node->kernel == dt_token("weights")) {
    FILE* f = dt_graph_open_resource(mod->graph, 0, "data/oidn-gldr.dat", "rb");
    if (!f) {
      snprintf(mod->graph->gui_msg_buf, sizeof(mod->graph->gui_msg_buf),
            "oidn: could not find \"data/oidn-gldr.dat\"");
      return 1;
    }
    fseek(f, 0, SEEK_END);
    const size_t size = ftell(f);
    const size_t expected_size = 1840768;
    if (size != expected_size) {
      snprintf(mod->graph->gui_msg_buf, sizeof(mod->graph->gui_msg_buf),
            "oidn: weight file \"data/oidn-gldr.dat\" has unexpected size!");
      fclose(f);
      return 1;
    }
    fseek(f, 0, SEEK_SET);
    fread(mapped, size, 1, f);
    fclose(f);
  }
  return 0;
}

static void denox_create_nodes_gldr(dt_graph_t* graph, dt_module_t* module,
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
  const int64_t r9 = r8 - 2;
  const int64_t r10 = r9 / 2;
  const int64_t r11 = r10 + 1;
  const int64_t r12 = r11 - 2;
  const int64_t r13 = r12 / 2;
  const int64_t r14 = r13 + 1;
  const int64_t r15 = r14 - 2;
  const int64_t r16 = r15 / 2;
  const int64_t r17 = r16 + 1;
  const int64_t r18 = r17 + 511;
  const int64_t r19 = r18 / 512;
  const int64_t r20 = r14 + 511;
  const int64_t r21 = r20 / 512;
  const int64_t r22 = r8 + 141;
  const int64_t r23 = r22 / 142;
  const int64_t r24 = r8 + 95;
  const int64_t r25 = r24 / 96;
  const int64_t r26 = r17 - 2;
  const int64_t r27 = r26 / 2;
  const int64_t r28 = r27 + 1;
  const int64_t r29 = r28 + 127;
  const int64_t r30 = r29 / 128;
  const int64_t r31 = r14 + 1151;
  const int64_t r32 = r31 / 1152;
  const int64_t r33 = r11 + 159;
  const int64_t r34 = r33 / 160;
  const int64_t r35 = r17 + 255;
  const int64_t r36 = r35 / 256;
  const int64_t r37 = r11 + 127;
  const int64_t r38 = r37 / 128;
  const int64_t r39 = r8 + 63;
  const int64_t r40 = r39 / 64;
  const int64_t r41 = ((H % 16) + 16) % 16;
  const int64_t r42 = r41 * -1;
  const int64_t r43 = r42 + 16;
  const int64_t r44 = ((r43 % 16) + 16) % 16;
  const int64_t r45 = H + r44;
  const int64_t r46 = r45 - 2;
  const int64_t r47 = r46 / 2;
  const int64_t r48 = r47 + 1;
  const int64_t r49 = r48 - 2;
  const int64_t r50 = r49 / 2;
  const int64_t r51 = r50 + 1;
  const int64_t r52 = r51 - 2;
  const int64_t r53 = r52 / 2;
  const int64_t r54 = r53 + 1;
  const int64_t r55 = r54 - 2;
  const int64_t r56 = r55 / 2;
  const int64_t r57 = r56 + 1;
  const int64_t r58 = r28 * r57;
  const int64_t r60 = r58 * 160;
  const int64_t r62 = r57 + 3;
  const int64_t r63 = r62 / 4;
  const int64_t r64 = r57 + 7;
  const int64_t r65 = r64 / 8;
  const int64_t r67 = r58 * 192;
  const int64_t r69 = r57 + 1;
  const int64_t r70 = r69 / 2;
  const int64_t r71 = r28 + 15;
  const int64_t r72 = r71 / 16;
  const int64_t r73 = r54 + 6;
  const int64_t r74 = r73 / 7;
  const int64_t r75 = r17 * r54;
  const int64_t r80 = r75 * 320;
  const int64_t r82 = r54 + 1;
  const int64_t r83 = r82 / 2;
  const int64_t r84 = r54 + 5;
  const int64_t r85 = r84 / 6;
  const int64_t r87 = r75 * 224;
  const int64_t r89 = r17 + 15;
  const int64_t r90 = r89 / 16;
  const int64_t r91 = r14 * r51;
  const int64_t r96 = r91 * 320;
  const int64_t r98 = r51 + 6;
  const int64_t r99 = r98 / 7;
  const int64_t r100 = r51 + 7;
  const int64_t r101 = r100 / 8;
  const int64_t r103 = r91 * 192;
  const int64_t r105 = r51 + 1;
  const int64_t r106 = r105 / 2;
  const int64_t r107 = r14 + 15;
  const int64_t r108 = r107 / 16;
  const int64_t r109 = r8 * r45;
  const int64_t r111 = r109 * 64;
  const int64_t r113 = r11 * r48;
  const int64_t r118 = r113 * 256;
  const int64_t r120 = r48 + 7;
  const int64_t r121 = r120 / 8;
  const int64_t r123 = r113 * 128;
  const int64_t r125 = r48 + 1;
  const int64_t r126 = r125 / 2;
  const int64_t r127 = r11 + 15;
  const int64_t r128 = r127 / 16;
  const int64_t r129 = r109 * 6;
  const int64_t r132 = r109 * 134;
  const int64_t r134 = r45 + 1;
  const int64_t r135 = r134 / 2;
  const int64_t r137 = r109 * 128;
  const int64_t r139 = r45 + 6;
  const int64_t r140 = r139 / 7;
  const int64_t r141 = r45 + 7;
  const int64_t r142 = r141 / 8;
  const int64_t r145 = r45 + 3;
  const int64_t r146 = r145 / 4;
  const int64_t r147 = r8 + 15;
  const int64_t r148 = r147 / 16;
  const int64_t r149 = r75 * 160;
  const int64_t r150 = r91 * 128;
  const int64_t r151 = r75 * 192;
  const int64_t r152 = r113 * 96;
  const int64_t r153 = r91 * 224;
  const int64_t r154 = r113 * 192;
  const int64_t r155 = H * W;
  const int64_t r156 = r155 * 6;
  dt_roi_t roi0 = {.wd = 1840768, .ht = 1};
  dt_roi_t roi1 = {.wd = (uint32_t)(r156 / 2), .ht = 1};
  dt_roi_t roi2 = {.wd = (uint32_t)(r129), .ht = 1};
  dt_roi_t roi3 = {.wd = (uint32_t)(r111), .ht = 1};
  dt_roi_t roi4 = {.wd = (uint32_t)(r111), .ht = 1};
  dt_roi_t roi5 = {.wd = (uint32_t)(r118), .ht = 1};
  dt_roi_t roi6 = {.wd = (uint32_t)(r152), .ht = 1};
  dt_roi_t roi7 = {.wd = (uint32_t)(r96), .ht = 1};
  dt_roi_t roi8 = {.wd = (uint32_t)(r150), .ht = 1};
  dt_roi_t roi9 = {.wd = (uint32_t)(r80), .ht = 1};
  dt_roi_t roi10 = {.wd = (uint32_t)(r149), .ht = 1};
  dt_roi_t roi11 = {.wd = (uint32_t)(r60), .ht = 1};
  dt_roi_t roi12 = {.wd = (uint32_t)(r67), .ht = 1};
  dt_roi_t roi13 = {.wd = (uint32_t)(r67), .ht = 1};
  dt_roi_t roi14 = {.wd = 1, .ht = 1};
  dt_roi_t roi15 = {.wd = (uint32_t)(r87), .ht = 1};
  dt_roi_t roi16 = {.wd = (uint32_t)(r87), .ht = 1};
  dt_roi_t roi17 = {.wd = (uint32_t)(r103), .ht = 1};
  dt_roi_t roi18 = {.wd = (uint32_t)(r103), .ht = 1};
  dt_roi_t roi19 = {.wd = (uint32_t)(r123), .ht = 1};
  dt_roi_t roi20 = {.wd = (uint32_t)(r123), .ht = 1};
  dt_roi_t roi21 = {.wd = (uint32_t)(r137), .ht = 1};
  dt_roi_t roi22 = {.wd = (uint32_t)(r132), .ht = 1};
  dt_roi_t roi23 = {.wd = (uint32_t)(r137), .ht = 1};
  dt_roi_t roi24 = {.wd = (uint32_t)(r111), .ht = 1};
  dt_roi_t roi25 = {.wd = (uint32_t)(r129), .ht = 1};
  dt_roi_t roi26 = {.wd = (uint32_t)(r156 / 2), .ht = 1};
  int weights_id = dt_node_add(graph, module, "oidn", "weights",
      1, 1, 1, 0, NULL, 1, 
      "w", "source", "ssbo", "u8", &roi0);
  // memory_pad (memory_pad.comp)
  const uint32_t memory_pad_pc[6] = {(uint32_t)(r8), (uint32_t)(r45), 0, (uint32_t)(r7), 0, (uint32_t)(r44)};
  const int memory_pad_id = dt_node_add(graph, module, "oidn", "gldr14",
      1 * DT_LOCAL_SIZE_X, r25 * DT_LOCAL_SIZE_Y, r140,
      24, (const int*)memory_pad_pc, 2, //
      "a", "read", "ssbo", "f16", &roi1, // HWC[3] : f16
      "b", "write", "ssbo", "u8", &roi2); // HWC[3] : f16
  // direct_conv (direct_conv.comp)
  const uint32_t direct_conv_pc[2] = {(uint32_t)(r8), (uint32_t)(r45)};
  const int direct_conv_id = dt_node_add(graph, module, "oidn", "gldr13",
      1 * DT_LOCAL_SIZE_X, r148 * DT_LOCAL_SIZE_Y, r146,
      8, (const int*)direct_conv_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi2, // HWC[3] : f16
      "d", "write", "ssbo", "u8", &roi3); // CHWC8[32] : f16
  // direct_conv_1 (direct_conv.comp)
  const uint32_t direct_conv_1_pc[2] = {(uint32_t)(r8), (uint32_t)(r45)};
  const int direct_conv_1_id = dt_node_add(graph, module, "oidn", "gldr24",
      1 * DT_LOCAL_SIZE_X, r148 * DT_LOCAL_SIZE_Y, r146,
      8, (const int*)direct_conv_1_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi3, // CHWC8[32] : f16
      "d", "write", "ssbo", "u8", &roi4); // CHWC8[32] : f16
  // basic_pool (basic_pool.comp)
  const uint32_t basic_pool_pc[2] = {(uint32_t)(r8), (uint32_t)(r45)};
  const int basic_pool_id = dt_node_add(graph, module, "oidn", "gldr26",
      4 * DT_LOCAL_SIZE_X, r34 * DT_LOCAL_SIZE_Y, r48,
      8, (const int*)basic_pool_pc, 2, //
      "a", "read", "ssbo", "u8", &roi4, // CHWC8[32] : f16
      "b", "write", "ssbo", "u8", &roi5); // CHWC8[32] : f16
  // direct_conv_2 (direct_conv.comp)
  const uint32_t direct_conv_2_pc[2] = {(uint32_t)(r11), (uint32_t)(r48)};
  const int direct_conv_2_id = dt_node_add(graph, module, "oidn", "gldr15",
      1 * DT_LOCAL_SIZE_X, r128 * DT_LOCAL_SIZE_Y, r121,
      8, (const int*)direct_conv_2_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi5, // CHWC8[32] : f16
      "d", "write", "ssbo", "u8", &roi6); // CHWC8[48] : f16
  // basic_pool_1 (basic_pool.comp)
  const uint32_t basic_pool_1_pc[2] = {(uint32_t)(r11), (uint32_t)(r48)};
  const int basic_pool_1_id = dt_node_add(graph, module, "oidn", "gldr18",
      6 * DT_LOCAL_SIZE_X, r32 * DT_LOCAL_SIZE_Y, r51,
      8, (const int*)basic_pool_1_pc, 2, //
      "a", "read", "ssbo", "u8", &roi6, // CHWC8[48] : f16
      "b", "write", "ssbo", "u8", &roi7); // CHWC8[48] : f16
  // direct_conv_3 (direct_conv.comp)
  const uint32_t direct_conv_3_pc[2] = {(uint32_t)(r14), (uint32_t)(r51)};
  const int direct_conv_3_id = dt_node_add(graph, module, "oidn", "gldr8",
      1 * DT_LOCAL_SIZE_X, r108 * DT_LOCAL_SIZE_Y, r99,
      8, (const int*)direct_conv_3_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi7, // CHWC8[48] : f16
      "d", "write", "ssbo", "u8", &roi8); // CHWC8[64] : f16
  // basic_pool_2 (basic_pool.comp)
  const uint32_t basic_pool_2_pc[2] = {(uint32_t)(r14), (uint32_t)(r51)};
  const int basic_pool_2_id = dt_node_add(graph, module, "oidn", "gldr20",
      8 * DT_LOCAL_SIZE_X, r36 * DT_LOCAL_SIZE_Y, r54,
      8, (const int*)basic_pool_2_pc, 2, //
      "a", "read", "ssbo", "u8", &roi8, // CHWC8[64] : f16
      "b", "write", "ssbo", "u8", &roi9); // CHWC8[64] : f16
  // direct_conv_4 (direct_conv.comp)
  const uint32_t direct_conv_4_pc[2] = {(uint32_t)(r17), (uint32_t)(r54)};
  const int direct_conv_4_id = dt_node_add(graph, module, "oidn", "gldr9",
      1 * DT_LOCAL_SIZE_X, r90 * DT_LOCAL_SIZE_Y, r74,
      8, (const int*)direct_conv_4_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi9, // CHWC8[64] : f16
      "d", "write", "ssbo", "u8", &roi10); // CHWC8[80] : f16
  // basic_pool_3 (basic_pool.comp)
  const uint32_t basic_pool_3_pc[2] = {(uint32_t)(r17), (uint32_t)(r54)};
  const int basic_pool_3_id = dt_node_add(graph, module, "oidn", "gldr21",
      10 * DT_LOCAL_SIZE_X, r30 * DT_LOCAL_SIZE_Y, r70,
      8, (const int*)basic_pool_3_pc, 2, //
      "a", "read", "ssbo", "u8", &roi10, // CHWC8[80] : f16
      "b", "write", "ssbo", "u8", &roi11); // CHWC8[80] : f16
  // direct_conv_5 (direct_conv.comp)
  const uint32_t direct_conv_5_pc[2] = {(uint32_t)(r28), (uint32_t)(r57)};
  const int direct_conv_5_id = dt_node_add(graph, module, "oidn", "gldr25",
      1 * DT_LOCAL_SIZE_X, r72 * DT_LOCAL_SIZE_Y, r63,
      8, (const int*)direct_conv_5_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi11, // CHWC8[80] : f16
      "d", "write", "ssbo", "u8", &roi12); // CHWC8[96] : f16
  // direct_conv_6 (direct_conv.comp)
  const uint32_t direct_conv_6_pc[2] = {(uint32_t)(r28), (uint32_t)(r57)};
  const int direct_conv_6_id = dt_node_add(graph, module, "oidn", "gldr11",
      1 * DT_LOCAL_SIZE_X, r72 * DT_LOCAL_SIZE_Y, r65,
      8, (const int*)direct_conv_6_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi12, // CHWC8[96] : f16
      "d", "write", "ssbo", "u8", &roi13); // CHWC8[96] : f16
  // basic_upsample (basic_upsample.comp)
  const uint32_t basic_upsample_pc[2] = {(uint32_t)(r28), (uint32_t)(r57)};
  const int basic_upsample_id = dt_node_add(graph, module, "oidn", "gldr27",
      12 * DT_LOCAL_SIZE_X, r19 * DT_LOCAL_SIZE_Y, r83,
      8, (const int*)basic_upsample_pc, 3, //
      "a", "read", "ssbo", "u8", &roi13, // CHWC8[96] : f16
      "b", "read", "ssbo", "u8", &roi9, // CHWC8[96] : f16
      "dummy", "write", "ssbo", "u8", &roi14);//
  // direct_conv_7 (direct_conv.comp)
  const uint32_t direct_conv_7_pc[2] = {(uint32_t)(r17), (uint32_t)(r54)};
  const int direct_conv_7_id = dt_node_add(graph, module, "oidn", "gldr7",
      1 * DT_LOCAL_SIZE_X, r90 * DT_LOCAL_SIZE_Y, r85,
      8, (const int*)direct_conv_7_pc, 5, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi9, // CHWC8[160] : f16
      "d", "write", "ssbo", "u8", &roi15, // CHWC8[112] : f16
      "z0", "read", "ssbo", "u8", &roi14);//
  // direct_conv_8 (direct_conv.comp)
  const uint32_t direct_conv_8_pc[2] = {(uint32_t)(r17), (uint32_t)(r54)};
  const int direct_conv_8_id = dt_node_add(graph, module, "oidn", "gldr6",
      1 * DT_LOCAL_SIZE_X, r90 * DT_LOCAL_SIZE_Y, r85,
      8, (const int*)direct_conv_8_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi15, // CHWC8[112] : f16
      "d", "write", "ssbo", "u8", &roi16); // CHWC8[112] : f16
  // basic_upsample_1 (basic_upsample.comp)
  const uint32_t basic_upsample_1_pc[2] = {(uint32_t)(r17), (uint32_t)(r54)};
  const int basic_upsample_1_id = dt_node_add(graph, module, "oidn", "gldr10",
      14 * DT_LOCAL_SIZE_X, r21 * DT_LOCAL_SIZE_Y, r106,
      8, (const int*)basic_upsample_1_pc, 3, //
      "a", "read", "ssbo", "u8", &roi16, // CHWC8[112] : f16
      "b", "read", "ssbo", "u8", &roi7, // CHWC8[112] : f16
      "dummy", "write", "ssbo", "u8", &roi14);//
  // direct_conv_9 (direct_conv.comp)
  const uint32_t direct_conv_9_pc[2] = {(uint32_t)(r14), (uint32_t)(r51)};
  const int direct_conv_9_id = dt_node_add(graph, module, "oidn", "gldr17",
      1 * DT_LOCAL_SIZE_X, r108 * DT_LOCAL_SIZE_Y, r101,
      8, (const int*)direct_conv_9_pc, 5, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi7, // CHWC8[160] : f16
      "d", "write", "ssbo", "u8", &roi17, // CHWC8[96] : f16
      "z0", "read", "ssbo", "u8", &roi14);//
  // direct_conv_10 (direct_conv.comp)
  const uint32_t direct_conv_10_pc[2] = {(uint32_t)(r14), (uint32_t)(r51)};
  const int direct_conv_10_id = dt_node_add(graph, module, "oidn", "gldr5",
      1 * DT_LOCAL_SIZE_X, r108 * DT_LOCAL_SIZE_Y, r101,
      8, (const int*)direct_conv_10_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi17, // CHWC8[96] : f16
      "d", "write", "ssbo", "u8", &roi18); // CHWC8[96] : f16
  // basic_upsample_2 (basic_upsample.comp)
  const uint32_t basic_upsample_2_pc[2] = {(uint32_t)(r14), (uint32_t)(r51)};
  const int basic_upsample_2_id = dt_node_add(graph, module, "oidn", "gldr16",
      12 * DT_LOCAL_SIZE_X, r38 * DT_LOCAL_SIZE_Y, r126,
      8, (const int*)basic_upsample_2_pc, 3, //
      "a", "read", "ssbo", "u8", &roi18, // CHWC8[96] : f16
      "b", "read", "ssbo", "u8", &roi5, // CHWC8[96] : f16
      "dummy", "write", "ssbo", "u8", &roi14);//
  // direct_conv_11 (direct_conv.comp)
  const uint32_t direct_conv_11_pc[2] = {(uint32_t)(r11), (uint32_t)(r48)};
  const int direct_conv_11_id = dt_node_add(graph, module, "oidn", "gldr3",
      1 * DT_LOCAL_SIZE_X, r128 * DT_LOCAL_SIZE_Y, r121,
      8, (const int*)direct_conv_11_pc, 5, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi5, // CHWC8[128] : f16
      "d", "write", "ssbo", "u8", &roi19, // CHWC8[64] : f16
      "z0", "read", "ssbo", "u8", &roi14);//
  // direct_conv_12 (direct_conv.comp)
  const uint32_t direct_conv_12_pc[2] = {(uint32_t)(r11), (uint32_t)(r48)};
  const int direct_conv_12_id = dt_node_add(graph, module, "oidn", "gldr19",
      1 * DT_LOCAL_SIZE_X, r128 * DT_LOCAL_SIZE_Y, r121,
      8, (const int*)direct_conv_12_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi19, // CHWC8[64] : f16
      "d", "write", "ssbo", "u8", &roi20); // HWC[64] : f16
  // basic_upsample_3 (basic_upsample.comp)
  const uint32_t basic_upsample_3_pc[2] = {(uint32_t)(r11), (uint32_t)(r48)};
  const int basic_upsample_3_id = dt_node_add(graph, module, "oidn", "gldr2",
      1 * DT_LOCAL_SIZE_X, r23 * DT_LOCAL_SIZE_Y, r135,
      8, (const int*)basic_upsample_3_pc, 2, //
      "a", "read", "ssbo", "u8", &roi20, // HWC[64] : f16
      "b", "write", "ssbo", "u8", &roi21); // HWC[64] : f16
  // copy_transform (copy_transform.comp)
  const uint32_t copy_transform_pc[2] = {(uint32_t)(r8), (uint32_t)(r45)};
  const int copy_transform_id = dt_node_add(graph, module, "oidn", "gldr23",
      2 * DT_LOCAL_SIZE_X, r40 * DT_LOCAL_SIZE_Y, r45,
      8, (const int*)copy_transform_pc, 2, //
      "a", "read", "ssbo", "u8", &roi21, // HWC[64] : f16
      "b", "write", "ssbo", "u8", &roi22); // HWC[67] : f16
  // copy_transform_1 (copy_transform.comp)
  const uint32_t copy_transform_1_pc[2] = {(uint32_t)(r8), (uint32_t)(r45)};
  const int copy_transform_1_id = dt_node_add(graph, module, "oidn", "gldr1",
      1 * DT_LOCAL_SIZE_X, r40 * DT_LOCAL_SIZE_Y, r45,
      8, (const int*)copy_transform_1_pc, 3, //
      "a", "read", "ssbo", "u8", &roi2, // HWC[3] : f16
      "b", "read", "ssbo", "u8", &roi22, // HWC[67] : f16
      "dummy", "write", "ssbo", "u8", &roi14);//
  // direct_conv_13 (direct_conv.comp)
  const uint32_t direct_conv_13_pc[2] = {(uint32_t)(r8), (uint32_t)(r45)};
  const int direct_conv_13_id = dt_node_add(graph, module, "oidn", "gldr0",
      1 * DT_LOCAL_SIZE_X, r148 * DT_LOCAL_SIZE_Y, r142,
      8, (const int*)direct_conv_13_pc, 5, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi22, // HWC[67] : f16
      "d", "write", "ssbo", "u8", &roi23, // CHWC8[64] : f16
      "z0", "read", "ssbo", "u8", &roi14);//
  // direct_conv_14 (direct_conv.comp)
  const uint32_t direct_conv_14_pc[2] = {(uint32_t)(r8), (uint32_t)(r45)};
  const int direct_conv_14_id = dt_node_add(graph, module, "oidn", "gldr4",
      1 * DT_LOCAL_SIZE_X, r148 * DT_LOCAL_SIZE_Y, r146,
      8, (const int*)direct_conv_14_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi23, // CHWC8[64] : f16
      "d", "write", "ssbo", "u8", &roi24); // CHWC8[32] : f16
  // direct_conv_15 (direct_conv.comp)
  const uint32_t direct_conv_15_pc[2] = {(uint32_t)(r8), (uint32_t)(r45)};
  const int direct_conv_15_id = dt_node_add(graph, module, "oidn", "gldr22",
      1 * DT_LOCAL_SIZE_X, r148 * DT_LOCAL_SIZE_Y, r146,
      8, (const int*)direct_conv_15_pc, 3, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi24, // CHWC8[32] : f16
      "c", "write", "ssbo", "u8", &roi25); // HWC[3] : f16
  // memory_slice (memory_slice.comp)
  const uint32_t memory_slice_pc[6] = {0, 0, (uint32_t)(r8), (uint32_t)(r45), (uint32_t)(W), (uint32_t)(H)};
  const int memory_slice_id = dt_node_add(graph, module, "oidn", "gldr12",
      1 * DT_LOCAL_SIZE_X, r3 * DT_LOCAL_SIZE_Y, H,
      24, (const int*)memory_slice_pc, 2, //
      "a", "read", "ssbo", "u8", &roi25, // HWC[3] : f16
      "b", "write", "ssbo", "f16", &roi26); // HWC[3] : f16
  if (input_connector == NULL) {
    dt_connector_copy(graph, module, input_id, memory_pad_id, 0);
  } else {
    dt_node_connect_named(graph, input_id, input_connector, memory_pad_id, "a");
  }
  dt_node_connect_named(graph, weights_id, "w", direct_conv_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_id, "b");
  dt_node_connect_named(graph, memory_pad_id, "b", direct_conv_id, "c");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_1_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_1_id, "b");
  dt_node_connect_named(graph, direct_conv_id, "d", direct_conv_1_id, "c");
  dt_node_connect_named(graph, direct_conv_1_id, "d", basic_pool_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_2_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_2_id, "b");
  dt_node_connect_named(graph, basic_pool_id, "b", direct_conv_2_id, "c");
  dt_node_connect_named(graph, direct_conv_2_id, "d", basic_pool_1_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_3_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_3_id, "b");
  dt_node_connect_named(graph, basic_pool_1_id, "b", direct_conv_3_id, "c");
  dt_node_connect_named(graph, direct_conv_3_id, "d", basic_pool_2_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_4_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_4_id, "b");
  dt_node_connect_named(graph, basic_pool_2_id, "b", direct_conv_4_id, "c");
  dt_node_connect_named(graph, direct_conv_4_id, "d", basic_pool_3_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_5_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_5_id, "b");
  dt_node_connect_named(graph, basic_pool_3_id, "b", direct_conv_5_id, "c");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_6_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_6_id, "b");
  dt_node_connect_named(graph, direct_conv_5_id, "d", direct_conv_6_id, "c");
  dt_node_connect_named(graph, direct_conv_6_id, "d", basic_upsample_id, "a");
  dt_node_connect_named(graph, basic_pool_2_id, "b", basic_upsample_id, "b");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_7_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_7_id, "b");
  dt_node_connect_named(graph, basic_pool_2_id, "b", direct_conv_7_id, "c");
  dt_node_connect_named(graph, basic_upsample_id, "dummy", direct_conv_7_id, "z0");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_8_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_8_id, "b");
  dt_node_connect_named(graph, direct_conv_7_id, "d", direct_conv_8_id, "c");
  dt_node_connect_named(graph, direct_conv_8_id, "d", basic_upsample_1_id, "a");
  dt_node_connect_named(graph, basic_pool_1_id, "b", basic_upsample_1_id, "b");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_9_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_9_id, "b");
  dt_node_connect_named(graph, basic_pool_1_id, "b", direct_conv_9_id, "c");
  dt_node_connect_named(graph, basic_upsample_1_id, "dummy", direct_conv_9_id, "z0");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_10_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_10_id, "b");
  dt_node_connect_named(graph, direct_conv_9_id, "d", direct_conv_10_id, "c");
  dt_node_connect_named(graph, direct_conv_10_id, "d", basic_upsample_2_id, "a");
  dt_node_connect_named(graph, basic_pool_id, "b", basic_upsample_2_id, "b");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_11_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_11_id, "b");
  dt_node_connect_named(graph, basic_pool_id, "b", direct_conv_11_id, "c");
  dt_node_connect_named(graph, basic_upsample_2_id, "dummy", direct_conv_11_id, "z0");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_12_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_12_id, "b");
  dt_node_connect_named(graph, direct_conv_11_id, "d", direct_conv_12_id, "c");
  dt_node_connect_named(graph, direct_conv_12_id, "d", basic_upsample_3_id, "a");
  dt_node_connect_named(graph, basic_upsample_3_id, "b", copy_transform_id, "a");
  dt_node_connect_named(graph, memory_pad_id, "b", copy_transform_1_id, "a");
  dt_node_connect_named(graph, copy_transform_id, "b", copy_transform_1_id, "b");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_13_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_13_id, "b");
  dt_node_connect_named(graph, copy_transform_id, "b", direct_conv_13_id, "c");
  dt_node_connect_named(graph, copy_transform_1_id, "dummy", direct_conv_13_id, "z0");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_14_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_14_id, "b");
  dt_node_connect_named(graph, direct_conv_13_id, "d", direct_conv_14_id, "c");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_15_id, "a");
  dt_node_connect_named(graph, direct_conv_14_id, "d", direct_conv_15_id, "b");
  dt_node_connect_named(graph, direct_conv_15_id, "c", memory_slice_id, "a");
  if (output_connector == NULL) {
    dt_connector_copy(graph, module, output_id, memory_slice_id, 1);
  } else {
    dt_node_connect_named(graph, memory_slice_id, "b", output_id, output_connector);
  }
  graph->node[direct_conv_id].connector[1].ssbo_offset = 4608;
  graph->node[direct_conv_1_id].connector[0].ssbo_offset = 4672;
  graph->node[direct_conv_1_id].connector[1].ssbo_offset = 23104;
  graph->node[basic_pool_id].connector[1].ssbo_offset = r154;
  graph->node[direct_conv_2_id].connector[0].ssbo_offset = 23168;
  graph->node[direct_conv_2_id].connector[1].ssbo_offset = 50816;
  graph->node[direct_conv_2_id].connector[2].ssbo_offset = r154;
  graph->node[basic_pool_1_id].connector[1].ssbo_offset = r153;
  graph->node[direct_conv_3_id].connector[0].ssbo_offset = 50912;
  graph->node[direct_conv_3_id].connector[1].ssbo_offset = 106208;
  graph->node[direct_conv_3_id].connector[2].ssbo_offset = r153;
  graph->node[basic_pool_2_id].connector[1].ssbo_offset = r151;
  graph->node[direct_conv_4_id].connector[0].ssbo_offset = 106336;
  graph->node[direct_conv_4_id].connector[1].ssbo_offset = 198496;
  graph->node[direct_conv_4_id].connector[2].ssbo_offset = r151;
  graph->node[direct_conv_5_id].connector[0].ssbo_offset = 198656;
  graph->node[direct_conv_5_id].connector[1].ssbo_offset = 336896;
  graph->node[direct_conv_6_id].connector[0].ssbo_offset = 337088;
  graph->node[direct_conv_6_id].connector[1].ssbo_offset = 502976;
  graph->node[direct_conv_7_id].connector[0].ssbo_offset = 503168;
  graph->node[direct_conv_7_id].connector[1].ssbo_offset = 825728;
  graph->node[direct_conv_8_id].connector[0].ssbo_offset = 825952;
  graph->node[direct_conv_8_id].connector[1].ssbo_offset = 1051744;
  graph->node[direct_conv_9_id].connector[0].ssbo_offset = 1051968;
  graph->node[direct_conv_9_id].connector[1].ssbo_offset = 1328448;
  graph->node[direct_conv_10_id].connector[0].ssbo_offset = 1328640;
  graph->node[direct_conv_10_id].connector[1].ssbo_offset = 1494528;
  graph->node[direct_conv_11_id].connector[0].ssbo_offset = 1494720;
  graph->node[direct_conv_11_id].connector[1].ssbo_offset = 1642176;
  graph->node[direct_conv_12_id].connector[0].ssbo_offset = 1642304;
  graph->node[direct_conv_12_id].connector[1].ssbo_offset = 1716032;
  graph->node[direct_conv_13_id].connector[0].ssbo_offset = 1716160;
  graph->node[direct_conv_13_id].connector[1].ssbo_offset = 1799104;
  graph->node[direct_conv_14_id].connector[0].ssbo_offset = 1799232;
  graph->node[direct_conv_14_id].connector[1].ssbo_offset = 1836096;
  graph->node[direct_conv_15_id].connector[0].ssbo_offset = 1836160;
}

#endif
