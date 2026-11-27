#ifndef select_gclk_DENOX_MODULE_H
#define select_gclk_DENOX_MODULE_H
#include "modules/api.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int denox_read_source_gclk(dt_module_t* mod, void* mapped, dt_read_source_params_t* p) {
  if (!p || p->node->kernel == dt_token("weights")) {
    FILE* f = dt_graph_open_resource(mod->graph, 0, "data/select-gclk.dat", "rb");
    if (!f) {
      snprintf(mod->graph->gui_msg_buf, sizeof(mod->graph->gui_msg_buf),
            "select: could not find \"data/select-gclk.dat\"");
      return 1;
    }
    fseek(f, 0, SEEK_END);
    const size_t size = ftell(f);
    const size_t expected_size = 25382496;
    if (size != expected_size) {
      snprintf(mod->graph->gui_msg_buf, sizeof(mod->graph->gui_msg_buf),
            "select: weight file \"data/select-gclk.dat\" has unexpected size!");
      fclose(f);
      return 1;
    }
    fseek(f, 0, SEEK_SET);
    if(mapped) fread(mapped, size, 1, f);
    fclose(f);
  }
  return 0;
}

static void denox_create_nodes_gclk(dt_graph_t* graph, dt_module_t* module,
      int input_id, const char* input_connector,
      int output_id, const char* output_connector) {
  dt_roi_t roi0 = {.wd = 25382496, .ht = 1};
  dt_roi_t roi1 = {.wd = (uint32_t)(4194304 / 2), .ht = 1};
  dt_roi_t roi2 = {.wd = (uint32_t)(4194304), .ht = 1};
  dt_roi_t roi3 = {.wd = (uint32_t)(12582912), .ht = 1};
  dt_roi_t roi4 = {.wd = (uint32_t)(1048576), .ht = 1};
  dt_roi_t roi5 = {.wd = (uint32_t)(2097152), .ht = 1};
  dt_roi_t roi6 = {.wd = (uint32_t)(2097152), .ht = 1};
  dt_roi_t roi7 = {.wd = (uint32_t)(2097152), .ht = 1};
  dt_roi_t roi8 = {.wd = (uint32_t)(2097152), .ht = 1};
  dt_roi_t roi9 = {.wd = (uint32_t)(2097152), .ht = 1};
  dt_roi_t roi10 = {.wd = (uint32_t)(2097152), .ht = 1};
  dt_roi_t roi11 = {.wd = (uint32_t)(5242880), .ht = 1};
  dt_roi_t roi12 = {.wd = (uint32_t)(524288), .ht = 1};
  dt_roi_t roi13 = {.wd = (uint32_t)(1048576), .ht = 1};
  dt_roi_t roi14 = {.wd = (uint32_t)(1048576), .ht = 1};
  dt_roi_t roi15 = {.wd = (uint32_t)(1048576), .ht = 1};
  dt_roi_t roi16 = {.wd = (uint32_t)(1048576), .ht = 1};
  dt_roi_t roi17 = {.wd = (uint32_t)(1048576), .ht = 1};
  dt_roi_t roi18 = {.wd = (uint32_t)(1048576), .ht = 1};
  dt_roi_t roi19 = {.wd = (uint32_t)(2097152), .ht = 1};
  dt_roi_t roi20 = {.wd = (uint32_t)(262144), .ht = 1};
  dt_roi_t roi21 = {.wd = (uint32_t)(524288), .ht = 1};
  dt_roi_t roi22 = {.wd = (uint32_t)(524288), .ht = 1};
  dt_roi_t roi23 = {.wd = (uint32_t)(524288), .ht = 1};
  dt_roi_t roi24 = {.wd = (uint32_t)(524288), .ht = 1};
  dt_roi_t roi25 = {.wd = (uint32_t)(524288), .ht = 1};
  dt_roi_t roi26 = {.wd = (uint32_t)(524288), .ht = 1};
  dt_roi_t roi27 = {.wd = (uint32_t)(786432), .ht = 1};
  dt_roi_t roi28 = {.wd = (uint32_t)(131072), .ht = 1};
  dt_roi_t roi29 = {.wd = (uint32_t)(262144), .ht = 1};
  dt_roi_t roi30 = {.wd = (uint32_t)(262144), .ht = 1};
  dt_roi_t roi31 = {.wd = (uint32_t)(262144), .ht = 1};
  dt_roi_t roi32 = {.wd = (uint32_t)(262144), .ht = 1};
  dt_roi_t roi33 = {.wd = (uint32_t)(262144), .ht = 1};
  dt_roi_t roi34 = {.wd = (uint32_t)(262144), .ht = 1};
  dt_roi_t roi35 = {.wd = (uint32_t)(262144), .ht = 1};
  dt_roi_t roi36 = {.wd = (uint32_t)(1024), .ht = 1};
  dt_roi_t roi37 = {.wd = (uint32_t)(64), .ht = 1};
  dt_roi_t roi38 = {.wd = (uint32_t)(16384), .ht = 1};
  dt_roi_t roi39 = {.wd = (uint32_t)(65536), .ht = 1};
  dt_roi_t roi40 = {.wd = (uint32_t)(4096), .ht = 1};
  dt_roi_t roi41 = {.wd = (uint32_t)(256), .ht = 1};
  dt_roi_t roi42 = {.wd = (uint32_t)(16384), .ht = 1};
  dt_roi_t roi43 = {.wd = (uint32_t)(65536), .ht = 1};
  dt_roi_t roi44 = {.wd = (uint32_t)(16384), .ht = 1};
  dt_roi_t roi45 = {.wd = (uint32_t)(1024), .ht = 1};
  dt_roi_t roi46 = {.wd = (uint32_t)(16384), .ht = 1};
  dt_roi_t roi47 = {.wd = (uint32_t)(65536), .ht = 1};
  dt_roi_t roi48 = {.wd = (uint32_t)(65536), .ht = 1};
  dt_roi_t roi49 = {.wd = (uint32_t)(65536), .ht = 1};
  dt_roi_t roi50 = {.wd = (uint32_t)(65536), .ht = 1};
  dt_roi_t roi51 = {.wd = (uint32_t)(65536), .ht = 1};
  dt_roi_t roi52 = {.wd = 1, .ht = 1};
  dt_roi_t roi53 = {.wd = (uint32_t)(262144), .ht = 1};
  dt_roi_t roi54 = {.wd = (uint32_t)(786432), .ht = 1};
  dt_roi_t roi55 = {.wd = (uint32_t)(2097152), .ht = 1};
  dt_roi_t roi56 = {.wd = (uint32_t)(4194304), .ht = 1};
  dt_roi_t roi57 = {.wd = (uint32_t)(131072 / 2), .ht = 1};
  int weights_id = dt_node_add(graph, module, "select", "weights",
      1, 1, 1, 0, NULL, 1, 
      "w", "source", "ssbo", "u8", &roi0);
  // direct_conv (direct_conv.comp)
  const uint32_t direct_conv_pc[4] = {512, 512, 256, 256};
  const int direct_conv_id = dt_node_add(graph, module, "select", "gclk27",
      1 * DT_LOCAL_SIZE_X, 16 * DT_LOCAL_SIZE_Y, 64,
      16, (const int*)direct_conv_pc, 4, //
      "a", "read", "ssbo", "f16", &roi1, // HWC[8] : f16
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi0,// unknown
      "d", "write", "ssbo", "u8", &roi2); // CHWC8[32] : f16
  // direct_conv_1 (direct_conv.comp)
  const uint32_t direct_conv_1_pc[4] = {256, 256, 256, 256};
  const int direct_conv_1_id = dt_node_add(graph, module, "select", "gclk24",
      1 * DT_LOCAL_SIZE_X, 16 * DT_LOCAL_SIZE_Y, 64,
      16, (const int*)direct_conv_1_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi2, // CHWC8[32] : f16
      "d", "write", "ssbo", "u8", &roi3); // CHWC8[32] : f16
  // basic_pool (basic_pool.comp)
  const uint32_t basic_pool_pc[2] = {256, 256};
  const int basic_pool_id = dt_node_add(graph, module, "select", "gclk23",
      4 * DT_LOCAL_SIZE_X, 1 * DT_LOCAL_SIZE_Y, 128,
      8, (const int*)basic_pool_pc, 2, //
      "a", "read", "ssbo", "u8", &roi3, // CHWC8[32] : f16
      "b", "write", "ssbo", "u8", &roi4); // CHWC8[32] : f16
  // direct_conv_2 (direct_conv.comp)
  const uint32_t direct_conv_2_pc[4] = {128, 128, 128, 128};
  const int direct_conv_2_id = dt_node_add(graph, module, "select", "gclk21",
      1 * DT_LOCAL_SIZE_X, 8 * DT_LOCAL_SIZE_Y, 16,
      16, (const int*)direct_conv_2_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi4, // CHWC8[32] : f16
      "d", "write", "ssbo", "u8", &roi5); // HWC[64] : f16
  // direct_conv_3 (direct_conv.comp)
  const uint32_t direct_conv_3_pc[4] = {128, 128, 128, 128};
  const int direct_conv_3_id = dt_node_add(graph, module, "select", "gclk22",
      1 * DT_LOCAL_SIZE_X, 8 * DT_LOCAL_SIZE_Y, 64,
      16, (const int*)direct_conv_3_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi5, // HWC[64] : f16
      "d", "write", "ssbo", "u8", &roi6); // CHWC8[64] : f16
  // direct_conv_4 (direct_conv.comp)
  const uint32_t direct_conv_4_pc[4] = {128, 128, 128, 128};
  const int direct_conv_4_id = dt_node_add(graph, module, "select", "gclk36",
      1 * DT_LOCAL_SIZE_X, 8 * DT_LOCAL_SIZE_Y, 22,
      16, (const int*)direct_conv_4_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi4, // CHWC8[32] : f16
      "d", "write", "ssbo", "u8", &roi7); // CHWC8[64] : f16
  // basic_add (basic_add.comp)
  const uint32_t basic_add_pc[2] = {128, 128};
  const int basic_add_id = dt_node_add(graph, module, "select", "gclk32",
      8 * DT_LOCAL_SIZE_X, 1 * DT_LOCAL_SIZE_Y, 128,
      8, (const int*)basic_add_pc, 3, //
      "a", "read", "ssbo", "u8", &roi6, // CHWC8[64] : f16
      "b", "read", "ssbo", "u8", &roi7, // CHWC8[64] : f16
      "c", "write", "ssbo", "u8", &roi8); // CHWC8[64] : f16
  // direct_conv_5 (direct_conv.comp)
  const uint32_t direct_conv_5_pc[4] = {128, 128, 128, 128};
  const int direct_conv_5_id = dt_node_add(graph, module, "select", "gclk18",
      1 * DT_LOCAL_SIZE_X, 8 * DT_LOCAL_SIZE_Y, 32,
      16, (const int*)direct_conv_5_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi8, // CHWC8[64] : f16
      "d", "write", "ssbo", "u8", &roi9); // HWC[64] : f16
  // direct_conv_6 (direct_conv.comp)
  const uint32_t direct_conv_6_pc[4] = {128, 128, 128, 128};
  const int direct_conv_6_id = dt_node_add(graph, module, "select", "gclk22",
      1 * DT_LOCAL_SIZE_X, 8 * DT_LOCAL_SIZE_Y, 64,
      16, (const int*)direct_conv_6_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi9, // HWC[64] : f16
      "d", "write", "ssbo", "u8", &roi10); // CHWC8[64] : f16
  // basic_add_1 (basic_add.comp)
  const uint32_t basic_add_1_pc[2] = {128, 128};
  const int basic_add_1_id = dt_node_add(graph, module, "select", "gclk32",
      8 * DT_LOCAL_SIZE_X, 1 * DT_LOCAL_SIZE_Y, 128,
      8, (const int*)basic_add_1_pc, 3, //
      "a", "read", "ssbo", "u8", &roi10, // CHWC8[64] : f16
      "b", "read", "ssbo", "u8", &roi8, // CHWC8[64] : f16
      "c", "write", "ssbo", "u8", &roi11); // CHWC8[64] : f16
  // basic_pool_1 (basic_pool.comp)
  const uint32_t basic_pool_1_pc[2] = {128, 128};
  const int basic_pool_1_id = dt_node_add(graph, module, "select", "gclk16",
      8 * DT_LOCAL_SIZE_X, 1 * DT_LOCAL_SIZE_Y, 64,
      8, (const int*)basic_pool_1_pc, 2, //
      "a", "read", "ssbo", "u8", &roi11, // CHWC8[64] : f16
      "b", "write", "ssbo", "u8", &roi12); // CHWC8[64] : f16
  // direct_conv_7 (direct_conv.comp)
  const uint32_t direct_conv_7_pc[4] = {64, 64, 64, 64};
  const int direct_conv_7_id = dt_node_add(graph, module, "select", "gclk48",
      1 * DT_LOCAL_SIZE_X, 4 * DT_LOCAL_SIZE_Y, 22,
      16, (const int*)direct_conv_7_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi12, // CHWC8[64] : f16
      "d", "write", "ssbo", "u8", &roi13); // CHWC8[128] : f16
  // direct_conv_8 (direct_conv.comp)
  const uint32_t direct_conv_8_pc[4] = {128, 128, 64, 64};
  const int direct_conv_8_id = dt_node_add(graph, module, "select", "gclk28",
      1 * DT_LOCAL_SIZE_X, 4 * DT_LOCAL_SIZE_Y, 64,
      16, (const int*)direct_conv_8_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi11, // CHWC8[64] : f16
      "d", "write", "ssbo", "u8", &roi14); // HWC[128] : f16
  // direct_conv_9 (direct_conv.comp)
  const uint32_t direct_conv_9_pc[4] = {64, 64, 64, 64};
  const int direct_conv_9_id = dt_node_add(graph, module, "select", "gclk20",
      1 * DT_LOCAL_SIZE_X, 4 * DT_LOCAL_SIZE_Y, 64,
      16, (const int*)direct_conv_9_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi14, // HWC[128] : f16
      "d", "write", "ssbo", "u8", &roi15); // CHWC8[128] : f16
  // basic_add_2 (basic_add.comp)
  const uint32_t basic_add_2_pc[2] = {64, 64};
  const int basic_add_2_id = dt_node_add(graph, module, "select", "gclk15",
      16 * DT_LOCAL_SIZE_X, 1 * DT_LOCAL_SIZE_Y, 32,
      8, (const int*)basic_add_2_pc, 3, //
      "a", "read", "ssbo", "u8", &roi15, // CHWC8[128] : f16
      "b", "read", "ssbo", "u8", &roi13, // CHWC8[128] : f16
      "c", "write", "ssbo", "u8", &roi16); // CHWC8[128] : f16
  // direct_conv_10 (direct_conv.comp)
  const uint32_t direct_conv_10_pc[4] = {64, 64, 64, 64};
  const int direct_conv_10_id = dt_node_add(graph, module, "select", "gclk29",
      1 * DT_LOCAL_SIZE_X, 4 * DT_LOCAL_SIZE_Y, 13,
      16, (const int*)direct_conv_10_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi16, // CHWC8[128] : f16
      "d", "write", "ssbo", "u8", &roi17); // HWC[128] : f16
  // direct_conv_11 (direct_conv.comp)
  const uint32_t direct_conv_11_pc[4] = {64, 64, 64, 64};
  const int direct_conv_11_id = dt_node_add(graph, module, "select", "gclk20",
      1 * DT_LOCAL_SIZE_X, 4 * DT_LOCAL_SIZE_Y, 64,
      16, (const int*)direct_conv_11_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi17, // HWC[128] : f16
      "d", "write", "ssbo", "u8", &roi18); // CHWC8[128] : f16
  // basic_add_3 (basic_add.comp)
  const uint32_t basic_add_3_pc[2] = {64, 64};
  const int basic_add_3_id = dt_node_add(graph, module, "select", "gclk15",
      16 * DT_LOCAL_SIZE_X, 1 * DT_LOCAL_SIZE_Y, 32,
      8, (const int*)basic_add_3_pc, 3, //
      "a", "read", "ssbo", "u8", &roi18, // CHWC8[128] : f16
      "b", "read", "ssbo", "u8", &roi16, // CHWC8[128] : f16
      "c", "write", "ssbo", "u8", &roi19); // CHWC8[128] : f16
  // basic_pool_2 (basic_pool.comp)
  const uint32_t basic_pool_2_pc[2] = {64, 64};
  const int basic_pool_2_id = dt_node_add(graph, module, "select", "gclk33",
      16 * DT_LOCAL_SIZE_X, 1 * DT_LOCAL_SIZE_Y, 32,
      8, (const int*)basic_pool_2_pc, 2, //
      "a", "read", "ssbo", "u8", &roi19, // CHWC8[128] : f16
      "b", "write", "ssbo", "u8", &roi20); // CHWC8[128] : f16
  // direct_conv_12 (direct_conv.comp)
  const uint32_t direct_conv_12_pc[4] = {32, 32, 32, 32};
  const int direct_conv_12_id = dt_node_add(graph, module, "select", "gclk38",
      1 * DT_LOCAL_SIZE_X, 2 * DT_LOCAL_SIZE_Y, 16,
      16, (const int*)direct_conv_12_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi20, // CHWC8[128] : f16
      "d", "write", "ssbo", "u8", &roi21); // CHWC8[256] : f16
  // direct_conv_13 (direct_conv.comp)
  const uint32_t direct_conv_13_pc[4] = {64, 64, 32, 32};
  const int direct_conv_13_id = dt_node_add(graph, module, "select", "gclk39",
      1 * DT_LOCAL_SIZE_X, 2 * DT_LOCAL_SIZE_Y, 11,
      16, (const int*)direct_conv_13_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi19, // CHWC8[128] : f16
      "d", "write", "ssbo", "u8", &roi22); // HWC[256] : f16
  // direct_conv_14 (direct_conv.comp)
  const uint32_t direct_conv_14_pc[4] = {32, 32, 32, 32};
  const int direct_conv_14_id = dt_node_add(graph, module, "select", "gclk41",
      1 * DT_LOCAL_SIZE_X, 2 * DT_LOCAL_SIZE_Y, 16,
      16, (const int*)direct_conv_14_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi22, // HWC[256] : f16
      "d", "write", "ssbo", "u8", &roi23); // CHWC8[256] : f16
  // basic_add_4 (basic_add.comp)
  const uint32_t basic_add_4_pc[2] = {32, 32};
  const int basic_add_4_id = dt_node_add(graph, module, "select", "gclk49",
      32 * DT_LOCAL_SIZE_X, 1 * DT_LOCAL_SIZE_Y, 32,
      8, (const int*)basic_add_4_pc, 3, //
      "a", "read", "ssbo", "u8", &roi23, // CHWC8[256] : f16
      "b", "read", "ssbo", "u8", &roi21, // CHWC8[256] : f16
      "c", "write", "ssbo", "u8", &roi24); // CHWC8[256] : f16
  // direct_conv_15 (direct_conv.comp)
  const uint32_t direct_conv_15_pc[4] = {32, 32, 32, 32};
  const int direct_conv_15_id = dt_node_add(graph, module, "select", "gclk45",
      1 * DT_LOCAL_SIZE_X, 2 * DT_LOCAL_SIZE_Y, 16,
      16, (const int*)direct_conv_15_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi24, // CHWC8[256] : f16
      "d", "write", "ssbo", "u8", &roi25); // CHWC8[256] : f16
  // direct_conv_16 (direct_conv.comp)
  const uint32_t direct_conv_16_pc[4] = {32, 32, 32, 32};
  const int direct_conv_16_id = dt_node_add(graph, module, "select", "gclk35",
      1 * DT_LOCAL_SIZE_X, 2 * DT_LOCAL_SIZE_Y, 16,
      16, (const int*)direct_conv_16_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi25, // CHWC8[256] : f16
      "d", "write", "ssbo", "u8", &roi26); // CHWC8[256] : f16
  // basic_add_5 (basic_add.comp)
  const uint32_t basic_add_5_pc[2] = {32, 32};
  const int basic_add_5_id = dt_node_add(graph, module, "select", "gclk49",
      32 * DT_LOCAL_SIZE_X, 1 * DT_LOCAL_SIZE_Y, 32,
      8, (const int*)basic_add_5_pc, 3, //
      "a", "read", "ssbo", "u8", &roi26, // CHWC8[256] : f16
      "b", "read", "ssbo", "u8", &roi24, // CHWC8[256] : f16
      "c", "write", "ssbo", "u8", &roi27); // CHWC8[256] : f16
  // basic_pool_3 (basic_pool.comp)
  const uint32_t basic_pool_3_pc[2] = {32, 32};
  const int basic_pool_3_id = dt_node_add(graph, module, "select", "gclk31",
      32 * DT_LOCAL_SIZE_X, 1 * DT_LOCAL_SIZE_Y, 8,
      8, (const int*)basic_pool_3_pc, 2, //
      "a", "read", "ssbo", "u8", &roi27, // CHWC8[256] : f16
      "b", "write", "ssbo", "u8", &roi28); // CHWC8[256] : f16
  // direct_conv_17 (direct_conv.comp)
  const uint32_t direct_conv_17_pc[4] = {16, 16, 16, 16};
  const int direct_conv_17_id = dt_node_add(graph, module, "select", "gclk40",
      16 * DT_LOCAL_SIZE_X, 1 * DT_LOCAL_SIZE_Y, 2,
      16, (const int*)direct_conv_17_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi28, // CHWC8[256] : f16
      "d", "write", "ssbo", "u8", &roi29); // HWC[512] : f16
  // direct_conv_18 (direct_conv.comp)
  const uint32_t direct_conv_18_pc[4] = {32, 32, 16, 16};
  const int direct_conv_18_id = dt_node_add(graph, module, "select", "gclk46",
      32 * DT_LOCAL_SIZE_X, 1 * DT_LOCAL_SIZE_Y, 2,
      16, (const int*)direct_conv_18_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi27, // CHWC8[256] : f16
      "d", "write", "ssbo", "u8", &roi30); // HWC[512] : f16
  // direct_conv_19 (direct_conv.comp)
  const uint32_t direct_conv_19_pc[4] = {16, 16, 16, 16};
  const int direct_conv_19_id = dt_node_add(graph, module, "select", "gclk50",
      16 * DT_LOCAL_SIZE_X, 1 * DT_LOCAL_SIZE_Y, 2,
      16, (const int*)direct_conv_19_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi30, // HWC[512] : f16
      "d", "write", "ssbo", "u8", &roi31); // HWC[512] : f16
  // basic_add_6 (basic_add.comp)
  const uint32_t basic_add_6_pc[2] = {16, 16};
  const int basic_add_6_id = dt_node_add(graph, module, "select", "gclk17",
      32 * DT_LOCAL_SIZE_X, 1 * DT_LOCAL_SIZE_Y, 8,
      8, (const int*)basic_add_6_pc, 3, //
      "a", "read", "ssbo", "u8", &roi31, // HWC[512] : f16
      "b", "read", "ssbo", "u8", &roi29, // HWC[512] : f16
      "c", "write", "ssbo", "u8", &roi32); // HWC[512] : f16
  // direct_conv_20 (direct_conv.comp)
  const uint32_t direct_conv_20_pc[4] = {16, 16, 16, 16};
  const int direct_conv_20_id = dt_node_add(graph, module, "select", "gclk19",
      32 * DT_LOCAL_SIZE_X, 1 * DT_LOCAL_SIZE_Y, 1,
      16, (const int*)direct_conv_20_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi32, // HWC[512] : f16
      "d", "write", "ssbo", "u8", &roi33); // CHWC8[512] : f16
  // direct_conv_21 (direct_conv.comp)
  const uint32_t direct_conv_21_pc[4] = {16, 16, 16, 16};
  const int direct_conv_21_id = dt_node_add(graph, module, "select", "gclk44",
      16 * DT_LOCAL_SIZE_X, 1 * DT_LOCAL_SIZE_Y, 4,
      16, (const int*)direct_conv_21_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi33, // CHWC8[512] : f16
      "d", "write", "ssbo", "u8", &roi34); // HWC[512] : f16
  // basic_add_7 (basic_add.comp)
  const uint32_t basic_add_7_pc[2] = {16, 16};
  const int basic_add_7_id = dt_node_add(graph, module, "select", "gclk17",
      32 * DT_LOCAL_SIZE_X, 1 * DT_LOCAL_SIZE_Y, 8,
      8, (const int*)basic_add_7_pc, 3, //
      "a", "read", "ssbo", "u8", &roi34, // HWC[512] : f16
      "b", "read", "ssbo", "u8", &roi32, // HWC[512] : f16
      "c", "write", "ssbo", "u8", &roi35); // HWC[512] : f16
  // basic_pool_4 (basic_pool.comp)
  const uint32_t basic_pool_4_pc[2] = {16, 16};
  const int basic_pool_4_id = dt_node_add(graph, module, "select", "gclk37",
      4 * DT_LOCAL_SIZE_X, 1 * DT_LOCAL_SIZE_Y, 1,
      8, (const int*)basic_pool_4_pc, 2, //
      "a", "read", "ssbo", "u8", &roi35, // HWC[512] : f16
      "b", "write", "ssbo", "u8", &roi36); // HWC[512] : f16
  // direct_conv_22 (direct_conv.comp)
  const uint32_t direct_conv_22_pc[4] = {1, 1, 1, 1};
  const int direct_conv_22_id = dt_node_add(graph, module, "select", "gclk47",
      1 * DT_LOCAL_SIZE_X, 1 * DT_LOCAL_SIZE_Y, 1,
      16, (const int*)direct_conv_22_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi36, // HWC[512] : f16
      "d", "write", "ssbo", "u8", &roi37); // CHWC8[32] : f16
  // basic_upsample (basic_upsample.comp)
  const uint32_t basic_upsample_pc[2] = {1, 1};
  const int basic_upsample_id = dt_node_add(graph, module, "select", "gclk14",
      4 * DT_LOCAL_SIZE_X, 1 * DT_LOCAL_SIZE_Y, 8,
      8, (const int*)basic_upsample_pc, 2, //
      "a", "read", "ssbo", "u8", &roi37, // CHWC8[32] : f16
      "b", "write", "ssbo", "u8", &roi38); // CHWC8[32] : f16
  // direct_conv_23 (direct_conv.comp)
  const uint32_t direct_conv_23_pc[4] = {16, 16, 16, 16};
  const int direct_conv_23_id = dt_node_add(graph, module, "select", "gclk13",
      1 * DT_LOCAL_SIZE_X, 1 * DT_LOCAL_SIZE_Y, 2,
      16, (const int*)direct_conv_23_pc, 3, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi38, // CHWC8[32] : f16
      "c", "write", "ssbo", "u8", &roi39); // CHWC8[128] : f16
  // basic_pool_5 (basic_pool.comp)
  const uint32_t basic_pool_5_pc[2] = {16, 16};
  const int basic_pool_5_id = dt_node_add(graph, module, "select", "gclk12",
      64 * DT_LOCAL_SIZE_X, 1 * DT_LOCAL_SIZE_Y, 2,
      8, (const int*)basic_pool_5_pc, 2, //
      "a", "read", "ssbo", "u8", &roi35, // HWC[512] : f16
      "b", "write", "ssbo", "u8", &roi40); // HWC[512] : f16
  // direct_conv_24 (direct_conv.comp)
  const uint32_t direct_conv_24_pc[4] = {2, 2, 2, 2};
  const int direct_conv_24_id = dt_node_add(graph, module, "select", "gclk11",
      1 * DT_LOCAL_SIZE_X, 1 * DT_LOCAL_SIZE_Y, 1,
      16, (const int*)direct_conv_24_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi40, // HWC[512] : f16
      "d", "write", "ssbo", "u8", &roi41); // HWC[32] : f16
  // basic_upsample_1 (basic_upsample.comp)
  const uint32_t basic_upsample_1_pc[2] = {2, 2};
  const int basic_upsample_1_id = dt_node_add(graph, module, "select", "gclk10",
      1 * DT_LOCAL_SIZE_X, 1 * DT_LOCAL_SIZE_Y, 8,
      8, (const int*)basic_upsample_1_pc, 2, //
      "a", "read", "ssbo", "u8", &roi41, // HWC[32] : f16
      "b", "write", "ssbo", "u8", &roi42); // HWC[32] : f16
  // direct_conv_25 (direct_conv.comp)
  const uint32_t direct_conv_25_pc[4] = {16, 16, 16, 16};
  const int direct_conv_25_id = dt_node_add(graph, module, "select", "gclk8",
      1 * DT_LOCAL_SIZE_X, 1 * DT_LOCAL_SIZE_Y, 6,
      16, (const int*)direct_conv_25_pc, 3, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi42, // HWC[32] : f16
      "c", "write", "ssbo", "u8", &roi43); // CHWC8[128] : f16
  // basic_pool_6 (basic_pool.comp)
  const uint32_t basic_pool_6_pc[2] = {16, 16};
  const int basic_pool_6_id = dt_node_add(graph, module, "select", "gclk7",
      16 * DT_LOCAL_SIZE_X, 1 * DT_LOCAL_SIZE_Y, 4,
      8, (const int*)basic_pool_6_pc, 2, //
      "a", "read", "ssbo", "u8", &roi35, // HWC[512] : f16
      "b", "write", "ssbo", "u8", &roi44); // HWC[512] : f16
  // direct_conv_26 (direct_conv.comp)
  const uint32_t direct_conv_26_pc[4] = {4, 4, 4, 4};
  const int direct_conv_26_id = dt_node_add(graph, module, "select", "gclk43",
      1 * DT_LOCAL_SIZE_X, 1 * DT_LOCAL_SIZE_Y, 1,
      16, (const int*)direct_conv_26_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi44, // HWC[512] : f16
      "d", "write", "ssbo", "u8", &roi45); // HWC[32] : f16
  // basic_upsample_2 (basic_upsample.comp)
  const uint32_t basic_upsample_2_pc[2] = {4, 4};
  const int basic_upsample_2_id = dt_node_add(graph, module, "select", "gclk9",
      4 * DT_LOCAL_SIZE_X, 1 * DT_LOCAL_SIZE_Y, 16,
      8, (const int*)basic_upsample_2_pc, 2, //
      "a", "read", "ssbo", "u8", &roi45, // HWC[32] : f16
      "b", "write", "ssbo", "u8", &roi46); // HWC[32] : f16
  // direct_conv_27 (direct_conv.comp)
  const uint32_t direct_conv_27_pc[4] = {16, 16, 16, 16};
  const int direct_conv_27_id = dt_node_add(graph, module, "select", "gclk8",
      1 * DT_LOCAL_SIZE_X, 1 * DT_LOCAL_SIZE_Y, 6,
      16, (const int*)direct_conv_27_pc, 3, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi46, // HWC[32] : f16
      "c", "write", "ssbo", "u8", &roi47); // CHWC8[128] : f16
  // direct_conv_28 (direct_conv.comp)
  const uint32_t direct_conv_28_pc[4] = {16, 16, 16, 16};
  const int direct_conv_28_id = dt_node_add(graph, module, "select", "gclk6",
      1 * DT_LOCAL_SIZE_X, 1 * DT_LOCAL_SIZE_Y, 6,
      16, (const int*)direct_conv_28_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi35, // HWC[512] : f16
      "d", "write", "ssbo", "u8", &roi48); // CHWC8[128] : f16
  // basic_add_8 (basic_add.comp)
  const uint32_t basic_add_8_pc[2] = {16, 16};
  const int basic_add_8_id = dt_node_add(graph, module, "select", "gclk42",
      16 * DT_LOCAL_SIZE_X, 1 * DT_LOCAL_SIZE_Y, 16,
      8, (const int*)basic_add_8_pc, 3, //
      "a", "read", "ssbo", "u8", &roi48, // CHWC8[128] : f16
      "b", "read", "ssbo", "u8", &roi39, // CHWC8[128] : f16
      "c", "write", "ssbo", "u8", &roi49); // CHWC8[128] : f16
  // basic_add_9 (basic_add.comp)
  const uint32_t basic_add_9_pc[2] = {16, 16};
  const int basic_add_9_id = dt_node_add(graph, module, "select", "gclk42",
      16 * DT_LOCAL_SIZE_X, 1 * DT_LOCAL_SIZE_Y, 16,
      8, (const int*)basic_add_9_pc, 3, //
      "a", "read", "ssbo", "u8", &roi49, // CHWC8[128] : f16
      "b", "read", "ssbo", "u8", &roi43, // CHWC8[128] : f16
      "c", "write", "ssbo", "u8", &roi50); // CHWC8[128] : f16
  // basic_add_10 (basic_add.comp)
  const uint32_t basic_add_10_pc[2] = {16, 16};
  const int basic_add_10_id = dt_node_add(graph, module, "select", "gclk30",
      16 * DT_LOCAL_SIZE_X, 1 * DT_LOCAL_SIZE_Y, 16,
      8, (const int*)basic_add_10_pc, 3, //
      "a", "read", "ssbo", "u8", &roi50, // CHWC8[128] : f16
      "b", "read", "ssbo", "u8", &roi47, // CHWC8[128] : f16
      "c", "write", "ssbo", "u8", &roi51); // CHWC8[128] : f16
  // basic_upsample_3 (basic_upsample.comp)
  const uint32_t basic_upsample_3_pc[2] = {16, 16};
  const int basic_upsample_3_id = dt_node_add(graph, module, "select", "gclk5",
      16 * DT_LOCAL_SIZE_X, 1 * DT_LOCAL_SIZE_Y, 16,
      8, (const int*)basic_upsample_3_pc, 3, //
      "a", "read", "ssbo", "u8", &roi51, // CHWC8[128] : f16
      "b", "read", "ssbo", "u8", &roi27, // CHWC8[128] : f16
      "dummy", "write", "ssbo", "u8", &roi52);//
  // direct_conv_29 (direct_conv.comp)
  const uint32_t direct_conv_29_pc[4] = {32, 32, 32, 32};
  const int direct_conv_29_id = dt_node_add(graph, module, "select", "gclk26",
      1 * DT_LOCAL_SIZE_X, 2 * DT_LOCAL_SIZE_Y, 16,
      16, (const int*)direct_conv_29_pc, 5, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi27, // CHWC8[384] : f16
      "d", "write", "ssbo", "u8", &roi53, // CHWC8[128] : f16
      "z0", "read", "ssbo", "u8", &roi52);//
  // basic_upsample_4 (basic_upsample.comp)
  const uint32_t basic_upsample_4_pc[2] = {32, 32};
  const int basic_upsample_4_id = dt_node_add(graph, module, "select", "gclk25",
      16 * DT_LOCAL_SIZE_X, 1 * DT_LOCAL_SIZE_Y, 64,
      8, (const int*)basic_upsample_4_pc, 3, //
      "a", "read", "ssbo", "u8", &roi53, // CHWC8[128] : f16
      "b", "read", "ssbo", "u8", &roi19, // CHWC8[128] : f16
      "dummy", "write", "ssbo", "u8", &roi52);//
  // direct_conv_30 (direct_conv.comp)
  const uint32_t direct_conv_30_pc[4] = {64, 64, 64, 64};
  const int direct_conv_30_id = dt_node_add(graph, module, "select", "gclk4",
      1 * DT_LOCAL_SIZE_X, 4 * DT_LOCAL_SIZE_Y, 16,
      16, (const int*)direct_conv_30_pc, 5, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi19, // CHWC8[256] : f16
      "d", "write", "ssbo", "u8", &roi54, // CHWC8[96] : f16
      "z0", "read", "ssbo", "u8", &roi52);//
  // basic_upsample_5 (basic_upsample.comp)
  const uint32_t basic_upsample_5_pc[2] = {64, 64};
  const int basic_upsample_5_id = dt_node_add(graph, module, "select", "gclk34",
      12 * DT_LOCAL_SIZE_X, 1 * DT_LOCAL_SIZE_Y, 64,
      8, (const int*)basic_upsample_5_pc, 3, //
      "a", "read", "ssbo", "u8", &roi54, // CHWC8[96] : f16
      "b", "read", "ssbo", "u8", &roi11, // CHWC8[96] : f16
      "dummy", "write", "ssbo", "u8", &roi52);//
  // direct_conv_31 (direct_conv.comp)
  const uint32_t direct_conv_31_pc[4] = {128, 128, 128, 128};
  const int direct_conv_31_id = dt_node_add(graph, module, "select", "gclk3",
      1 * DT_LOCAL_SIZE_X, 8 * DT_LOCAL_SIZE_Y, 32,
      16, (const int*)direct_conv_31_pc, 5, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi11, // CHWC8[160] : f16
      "d", "write", "ssbo", "u8", &roi55, // CHWC8[64] : f16
      "z0", "read", "ssbo", "u8", &roi52);//
  // basic_upsample_6 (basic_upsample.comp)
  const uint32_t basic_upsample_6_pc[2] = {128, 128};
  const int basic_upsample_6_id = dt_node_add(graph, module, "select", "gclk2",
      8 * DT_LOCAL_SIZE_X, 2 * DT_LOCAL_SIZE_Y, 128,
      8, (const int*)basic_upsample_6_pc, 3, //
      "a", "read", "ssbo", "u8", &roi55, // CHWC8[64] : f16
      "b", "read", "ssbo", "u8", &roi3, // CHWC8[64] : f16
      "dummy", "write", "ssbo", "u8", &roi52);//
  // direct_conv_32 (direct_conv.comp)
  const uint32_t direct_conv_32_pc[4] = {256, 256, 256, 256};
  const int direct_conv_32_id = dt_node_add(graph, module, "select", "gclk1",
      1 * DT_LOCAL_SIZE_X, 16 * DT_LOCAL_SIZE_Y, 64,
      16, (const int*)direct_conv_32_pc, 5, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi3, // CHWC8[96] : f16
      "d", "write", "ssbo", "u8", &roi56, // HWC[32] : f16
      "z0", "read", "ssbo", "u8", &roi52);//
  // direct_conv_33 (direct_conv.comp)
  const uint32_t direct_conv_33_pc[4] = {256, 256, 256, 256};
  const int direct_conv_33_id = dt_node_add(graph, module, "select", "gclk0",
      1 * DT_LOCAL_SIZE_X, 16 * DT_LOCAL_SIZE_Y, 32,
      16, (const int*)direct_conv_33_pc, 4, //
      "a", "write", "ssbo", "f16", &roi57, // HWC[1] : f16
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi0,// unknown
      "d", "read", "ssbo", "u8", &roi56); // HWC[32] : f16
  if (input_connector == NULL) {
    dt_connector_copy(graph, module, input_id, direct_conv_id, 0);
  } else {
    dt_node_connect_named(graph, input_id, input_connector, direct_conv_id, "a");
  }
  dt_node_connect_named(graph, weights_id, "w", direct_conv_id, "b");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_id, "c");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_1_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_1_id, "b");
  dt_node_connect_named(graph, direct_conv_id, "d", direct_conv_1_id, "c");
  dt_node_connect_named(graph, direct_conv_1_id, "d", basic_pool_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_2_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_2_id, "b");
  dt_node_connect_named(graph, basic_pool_id, "b", direct_conv_2_id, "c");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_3_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_3_id, "b");
  dt_node_connect_named(graph, direct_conv_2_id, "d", direct_conv_3_id, "c");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_4_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_4_id, "b");
  dt_node_connect_named(graph, basic_pool_id, "b", direct_conv_4_id, "c");
  dt_node_connect_named(graph, direct_conv_3_id, "d", basic_add_id, "a");
  dt_node_connect_named(graph, direct_conv_4_id, "d", basic_add_id, "b");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_5_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_5_id, "b");
  dt_node_connect_named(graph, basic_add_id, "c", direct_conv_5_id, "c");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_6_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_6_id, "b");
  dt_node_connect_named(graph, direct_conv_5_id, "d", direct_conv_6_id, "c");
  dt_node_connect_named(graph, direct_conv_6_id, "d", basic_add_1_id, "a");
  dt_node_connect_named(graph, basic_add_id, "c", basic_add_1_id, "b");
  dt_node_connect_named(graph, basic_add_1_id, "c", basic_pool_1_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_7_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_7_id, "b");
  dt_node_connect_named(graph, basic_pool_1_id, "b", direct_conv_7_id, "c");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_8_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_8_id, "b");
  dt_node_connect_named(graph, basic_add_1_id, "c", direct_conv_8_id, "c");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_9_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_9_id, "b");
  dt_node_connect_named(graph, direct_conv_8_id, "d", direct_conv_9_id, "c");
  dt_node_connect_named(graph, direct_conv_9_id, "d", basic_add_2_id, "a");
  dt_node_connect_named(graph, direct_conv_7_id, "d", basic_add_2_id, "b");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_10_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_10_id, "b");
  dt_node_connect_named(graph, basic_add_2_id, "c", direct_conv_10_id, "c");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_11_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_11_id, "b");
  dt_node_connect_named(graph, direct_conv_10_id, "d", direct_conv_11_id, "c");
  dt_node_connect_named(graph, direct_conv_11_id, "d", basic_add_3_id, "a");
  dt_node_connect_named(graph, basic_add_2_id, "c", basic_add_3_id, "b");
  dt_node_connect_named(graph, basic_add_3_id, "c", basic_pool_2_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_12_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_12_id, "b");
  dt_node_connect_named(graph, basic_pool_2_id, "b", direct_conv_12_id, "c");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_13_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_13_id, "b");
  dt_node_connect_named(graph, basic_add_3_id, "c", direct_conv_13_id, "c");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_14_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_14_id, "b");
  dt_node_connect_named(graph, direct_conv_13_id, "d", direct_conv_14_id, "c");
  dt_node_connect_named(graph, direct_conv_14_id, "d", basic_add_4_id, "a");
  dt_node_connect_named(graph, direct_conv_12_id, "d", basic_add_4_id, "b");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_15_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_15_id, "b");
  dt_node_connect_named(graph, basic_add_4_id, "c", direct_conv_15_id, "c");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_16_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_16_id, "b");
  dt_node_connect_named(graph, direct_conv_15_id, "d", direct_conv_16_id, "c");
  dt_node_connect_named(graph, direct_conv_16_id, "d", basic_add_5_id, "a");
  dt_node_connect_named(graph, basic_add_4_id, "c", basic_add_5_id, "b");
  dt_node_connect_named(graph, basic_add_5_id, "c", basic_pool_3_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_17_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_17_id, "b");
  dt_node_connect_named(graph, basic_pool_3_id, "b", direct_conv_17_id, "c");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_18_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_18_id, "b");
  dt_node_connect_named(graph, basic_add_5_id, "c", direct_conv_18_id, "c");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_19_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_19_id, "b");
  dt_node_connect_named(graph, direct_conv_18_id, "d", direct_conv_19_id, "c");
  dt_node_connect_named(graph, direct_conv_19_id, "d", basic_add_6_id, "a");
  dt_node_connect_named(graph, direct_conv_17_id, "d", basic_add_6_id, "b");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_20_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_20_id, "b");
  dt_node_connect_named(graph, basic_add_6_id, "c", direct_conv_20_id, "c");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_21_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_21_id, "b");
  dt_node_connect_named(graph, direct_conv_20_id, "d", direct_conv_21_id, "c");
  dt_node_connect_named(graph, direct_conv_21_id, "d", basic_add_7_id, "a");
  dt_node_connect_named(graph, basic_add_6_id, "c", basic_add_7_id, "b");
  dt_node_connect_named(graph, basic_add_7_id, "c", basic_pool_4_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_22_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_22_id, "b");
  dt_node_connect_named(graph, basic_pool_4_id, "b", direct_conv_22_id, "c");
  dt_node_connect_named(graph, direct_conv_22_id, "d", basic_upsample_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_23_id, "a");
  dt_node_connect_named(graph, basic_upsample_id, "b", direct_conv_23_id, "b");
  dt_node_connect_named(graph, basic_add_7_id, "c", basic_pool_5_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_24_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_24_id, "b");
  dt_node_connect_named(graph, basic_pool_5_id, "b", direct_conv_24_id, "c");
  dt_node_connect_named(graph, direct_conv_24_id, "d", basic_upsample_1_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_25_id, "a");
  dt_node_connect_named(graph, basic_upsample_1_id, "b", direct_conv_25_id, "b");
  dt_node_connect_named(graph, basic_add_7_id, "c", basic_pool_6_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_26_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_26_id, "b");
  dt_node_connect_named(graph, basic_pool_6_id, "b", direct_conv_26_id, "c");
  dt_node_connect_named(graph, direct_conv_26_id, "d", basic_upsample_2_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_27_id, "a");
  dt_node_connect_named(graph, basic_upsample_2_id, "b", direct_conv_27_id, "b");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_28_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_28_id, "b");
  dt_node_connect_named(graph, basic_add_7_id, "c", direct_conv_28_id, "c");
  dt_node_connect_named(graph, direct_conv_28_id, "d", basic_add_8_id, "a");
  dt_node_connect_named(graph, direct_conv_23_id, "c", basic_add_8_id, "b");
  dt_node_connect_named(graph, basic_add_8_id, "c", basic_add_9_id, "a");
  dt_node_connect_named(graph, direct_conv_25_id, "c", basic_add_9_id, "b");
  dt_node_connect_named(graph, basic_add_9_id, "c", basic_add_10_id, "a");
  dt_node_connect_named(graph, direct_conv_27_id, "c", basic_add_10_id, "b");
  dt_node_connect_named(graph, basic_add_10_id, "c", basic_upsample_3_id, "a");
  dt_node_connect_named(graph, basic_add_5_id, "c", basic_upsample_3_id, "b");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_29_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_29_id, "b");
  dt_node_connect_named(graph, basic_add_5_id, "c", direct_conv_29_id, "c");
  dt_node_connect_named(graph, basic_upsample_3_id, "dummy", direct_conv_29_id, "z0");
  dt_node_connect_named(graph, direct_conv_29_id, "d", basic_upsample_4_id, "a");
  dt_node_connect_named(graph, basic_add_3_id, "c", basic_upsample_4_id, "b");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_30_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_30_id, "b");
  dt_node_connect_named(graph, basic_add_3_id, "c", direct_conv_30_id, "c");
  dt_node_connect_named(graph, basic_upsample_4_id, "dummy", direct_conv_30_id, "z0");
  dt_node_connect_named(graph, direct_conv_30_id, "d", basic_upsample_5_id, "a");
  dt_node_connect_named(graph, basic_add_1_id, "c", basic_upsample_5_id, "b");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_31_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_31_id, "b");
  dt_node_connect_named(graph, basic_add_1_id, "c", direct_conv_31_id, "c");
  dt_node_connect_named(graph, basic_upsample_5_id, "dummy", direct_conv_31_id, "z0");
  dt_node_connect_named(graph, direct_conv_31_id, "d", basic_upsample_6_id, "a");
  dt_node_connect_named(graph, direct_conv_1_id, "d", basic_upsample_6_id, "b");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_32_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_32_id, "b");
  dt_node_connect_named(graph, direct_conv_1_id, "d", direct_conv_32_id, "c");
  dt_node_connect_named(graph, basic_upsample_6_id, "dummy", direct_conv_32_id, "z0");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_33_id, "b");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_33_id, "c");
  dt_node_connect_named(graph, direct_conv_32_id, "d", direct_conv_33_id, "d");
  if (output_connector == NULL) {
    dt_connector_copy(graph, module, output_id, direct_conv_33_id, 0);
  } else {
    dt_node_connect_named(graph, direct_conv_33_id, "a", output_id, output_connector);
  }
  graph->node[direct_conv_id].connector[2].ssbo_offset = 4608;
  graph->node[direct_conv_1_id].connector[0].ssbo_offset = 4672;
  graph->node[direct_conv_1_id].connector[1].ssbo_offset = 23104;
  graph->node[direct_conv_1_id].connector[3].ssbo_offset = 0;
  graph->node[basic_pool_id].connector[0].ssbo_offset = 0;
  graph->node[direct_conv_2_id].connector[0].ssbo_offset = 23168;
  graph->node[direct_conv_2_id].connector[1].ssbo_offset = 60032;
  graph->node[direct_conv_3_id].connector[0].ssbo_offset = 60160;
  graph->node[direct_conv_3_id].connector[1].ssbo_offset = 133888;
  graph->node[direct_conv_4_id].connector[0].ssbo_offset = 134016;
  graph->node[direct_conv_4_id].connector[1].ssbo_offset = 138112;
  graph->node[direct_conv_5_id].connector[0].ssbo_offset = 138240;
  graph->node[direct_conv_5_id].connector[1].ssbo_offset = 211968;
  graph->node[direct_conv_6_id].connector[0].ssbo_offset = 212096;
  graph->node[direct_conv_6_id].connector[1].ssbo_offset = 285824;
  graph->node[basic_add_1_id].connector[2].ssbo_offset = 0;
  graph->node[basic_pool_1_id].connector[0].ssbo_offset = 0;
  graph->node[direct_conv_7_id].connector[0].ssbo_offset = 285952;
  graph->node[direct_conv_7_id].connector[1].ssbo_offset = 302336;
  graph->node[direct_conv_8_id].connector[0].ssbo_offset = 302592;
  graph->node[direct_conv_8_id].connector[1].ssbo_offset = 450048;
  graph->node[direct_conv_8_id].connector[2].ssbo_offset = 0;
  graph->node[direct_conv_9_id].connector[0].ssbo_offset = 450304;
  graph->node[direct_conv_9_id].connector[1].ssbo_offset = 745216;
  graph->node[direct_conv_10_id].connector[0].ssbo_offset = 745472;
  graph->node[direct_conv_10_id].connector[1].ssbo_offset = 1040384;
  graph->node[direct_conv_11_id].connector[0].ssbo_offset = 1040640;
  graph->node[direct_conv_11_id].connector[1].ssbo_offset = 1335552;
  graph->node[basic_add_3_id].connector[2].ssbo_offset = 0;
  graph->node[basic_pool_2_id].connector[0].ssbo_offset = 0;
  graph->node[direct_conv_12_id].connector[0].ssbo_offset = 1335808;
  graph->node[direct_conv_12_id].connector[1].ssbo_offset = 1401344;
  graph->node[direct_conv_13_id].connector[0].ssbo_offset = 1401856;
  graph->node[direct_conv_13_id].connector[1].ssbo_offset = 1991680;
  graph->node[direct_conv_13_id].connector[2].ssbo_offset = 0;
  graph->node[direct_conv_14_id].connector[0].ssbo_offset = 1992192;
  graph->node[direct_conv_14_id].connector[1].ssbo_offset = 3171840;
  graph->node[direct_conv_15_id].connector[0].ssbo_offset = 3172352;
  graph->node[direct_conv_15_id].connector[1].ssbo_offset = 4352000;
  graph->node[direct_conv_16_id].connector[0].ssbo_offset = 4352512;
  graph->node[direct_conv_16_id].connector[1].ssbo_offset = 5532160;
  graph->node[basic_add_5_id].connector[2].ssbo_offset = 0;
  graph->node[basic_pool_3_id].connector[0].ssbo_offset = 0;
  graph->node[direct_conv_17_id].connector[0].ssbo_offset = 5532672;
  graph->node[direct_conv_17_id].connector[1].ssbo_offset = 5794816;
  graph->node[direct_conv_18_id].connector[0].ssbo_offset = 5795840;
  graph->node[direct_conv_18_id].connector[1].ssbo_offset = 8155136;
  graph->node[direct_conv_18_id].connector[2].ssbo_offset = 0;
  graph->node[direct_conv_19_id].connector[0].ssbo_offset = 8156160;
  graph->node[direct_conv_19_id].connector[1].ssbo_offset = 12874752;
  graph->node[direct_conv_20_id].connector[0].ssbo_offset = 12875776;
  graph->node[direct_conv_20_id].connector[1].ssbo_offset = 17594368;
  graph->node[direct_conv_21_id].connector[0].ssbo_offset = 17595392;
  graph->node[direct_conv_21_id].connector[1].ssbo_offset = 22313984;
  graph->node[direct_conv_22_id].connector[0].ssbo_offset = 22315008;
  graph->node[direct_conv_22_id].connector[1].ssbo_offset = 22347776;
  graph->node[direct_conv_23_id].connector[0].ssbo_offset = 22347840;
  graph->node[direct_conv_24_id].connector[0].ssbo_offset = 22421568;
  graph->node[direct_conv_24_id].connector[1].ssbo_offset = 22454336;
  graph->node[direct_conv_25_id].connector[0].ssbo_offset = 22454400;
  graph->node[direct_conv_26_id].connector[0].ssbo_offset = 22528128;
  graph->node[direct_conv_26_id].connector[1].ssbo_offset = 22560896;
  graph->node[direct_conv_27_id].connector[0].ssbo_offset = 22560960;
  graph->node[direct_conv_28_id].connector[0].ssbo_offset = 22634688;
  graph->node[direct_conv_28_id].connector[1].ssbo_offset = 23814336;
  graph->node[direct_conv_29_id].connector[0].ssbo_offset = 23814592;
  graph->node[direct_conv_29_id].connector[1].ssbo_offset = 24699328;
  graph->node[direct_conv_30_id].connector[0].ssbo_offset = 24699584;
  graph->node[direct_conv_30_id].connector[1].ssbo_offset = 25141952;
  graph->node[direct_conv_31_id].connector[0].ssbo_offset = 25142144;
  graph->node[direct_conv_31_id].connector[1].ssbo_offset = 25326464;
  graph->node[direct_conv_32_id].connector[0].ssbo_offset = 25326592;
  graph->node[direct_conv_32_id].connector[1].ssbo_offset = 25381888;
  graph->node[direct_conv_33_id].connector[1].ssbo_offset = 25381952;
  graph->node[direct_conv_33_id].connector[2].ssbo_offset = 25382464;
}

#endif
