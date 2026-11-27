#ifndef segment_aseg_DENOX_MODULE_H
#define segment_aseg_DENOX_MODULE_H
#include "modules/api.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int denox_read_source_aseg(dt_module_t* mod, void* mapped, dt_read_source_params_t* p) {
  if (!p || p->node->kernel == dt_token("weights")) {
    FILE* f = dt_graph_open_resource(mod->graph, 0, "data/segment-aseg.dat", "rb");
    if (!f) {
      snprintf(mod->graph->gui_msg_buf, sizeof(mod->graph->gui_msg_buf),
            "segment: could not find \"data/segment-aseg.dat\"");
      return 1;
    }
    fseek(f, 0, SEEK_END);
    const size_t size = ftell(f);
    const size_t expected_size = 25398240;
    if (size != expected_size) {
      snprintf(mod->graph->gui_msg_buf, sizeof(mod->graph->gui_msg_buf),
            "segment: weight file \"data/segment-aseg.dat\" has unexpected size!");
      fclose(f);
      return 1;
    }
    fseek(f, 0, SEEK_SET);
    if(mapped) fread(mapped, size, 1, f);
    fclose(f);
  }
  return 0;
}

static void denox_create_nodes_aseg(dt_graph_t* graph, dt_module_t* module,
      int input_id, const char* input_connector,
      int output_id, const char* output_connector) {
  dt_roi_t roi0 = {.wd = 25398240, .ht = 1};
  dt_roi_t roi1 = {.wd = (uint32_t)(884736 / 2), .ht = 1};
  dt_roi_t roi2 = {.wd = (uint32_t)(2359296), .ht = 1};
  dt_roi_t roi3 = {.wd = (uint32_t)(589824), .ht = 1};
  dt_roi_t roi4 = {.wd = (uint32_t)(1179648), .ht = 1};
  dt_roi_t roi5 = {.wd = (uint32_t)(1179648), .ht = 1};
  dt_roi_t roi6 = {.wd = (uint32_t)(1179648), .ht = 1};
  dt_roi_t roi7 = {.wd = (uint32_t)(1179648), .ht = 1};
  dt_roi_t roi8 = {.wd = (uint32_t)(1179648), .ht = 1};
  dt_roi_t roi9 = {.wd = (uint32_t)(1179648), .ht = 1};
  dt_roi_t roi10 = {.wd = (uint32_t)(1179648), .ht = 1};
  dt_roi_t roi11 = {.wd = (uint32_t)(294912), .ht = 1};
  dt_roi_t roi12 = {.wd = (uint32_t)(589824), .ht = 1};
  dt_roi_t roi13 = {.wd = (uint32_t)(589824), .ht = 1};
  dt_roi_t roi14 = {.wd = (uint32_t)(589824), .ht = 1};
  dt_roi_t roi15 = {.wd = (uint32_t)(589824), .ht = 1};
  dt_roi_t roi16 = {.wd = (uint32_t)(589824), .ht = 1};
  dt_roi_t roi17 = {.wd = (uint32_t)(589824), .ht = 1};
  dt_roi_t roi18 = {.wd = (uint32_t)(589824), .ht = 1};
  dt_roi_t roi19 = {.wd = (uint32_t)(147456), .ht = 1};
  dt_roi_t roi20 = {.wd = (uint32_t)(294912), .ht = 1};
  dt_roi_t roi21 = {.wd = (uint32_t)(294912), .ht = 1};
  dt_roi_t roi22 = {.wd = (uint32_t)(294912), .ht = 1};
  dt_roi_t roi23 = {.wd = (uint32_t)(294912), .ht = 1};
  dt_roi_t roi24 = {.wd = (uint32_t)(294912), .ht = 1};
  dt_roi_t roi25 = {.wd = (uint32_t)(294912), .ht = 1};
  dt_roi_t roi26 = {.wd = (uint32_t)(294912), .ht = 1};
  dt_roi_t roi27 = {.wd = (uint32_t)(73728), .ht = 1};
  dt_roi_t roi28 = {.wd = (uint32_t)(147456), .ht = 1};
  dt_roi_t roi29 = {.wd = (uint32_t)(147456), .ht = 1};
  dt_roi_t roi30 = {.wd = (uint32_t)(147456), .ht = 1};
  dt_roi_t roi31 = {.wd = (uint32_t)(147456), .ht = 1};
  dt_roi_t roi32 = {.wd = (uint32_t)(147456), .ht = 1};
  dt_roi_t roi33 = {.wd = (uint32_t)(147456), .ht = 1};
  dt_roi_t roi34 = {.wd = (uint32_t)(147456), .ht = 1};
  dt_roi_t roi35 = {.wd = (uint32_t)(1024), .ht = 1};
  dt_roi_t roi36 = {.wd = (uint32_t)(64), .ht = 1};
  dt_roi_t roi37 = {.wd = (uint32_t)(9216), .ht = 1};
  dt_roi_t roi38 = {.wd = (uint32_t)(36864), .ht = 1};
  dt_roi_t roi39 = {.wd = (uint32_t)(4096), .ht = 1};
  dt_roi_t roi40 = {.wd = (uint32_t)(256), .ht = 1};
  dt_roi_t roi41 = {.wd = (uint32_t)(9216), .ht = 1};
  dt_roi_t roi42 = {.wd = (uint32_t)(36864), .ht = 1};
  dt_roi_t roi43 = {.wd = (uint32_t)(16384), .ht = 1};
  dt_roi_t roi44 = {.wd = (uint32_t)(1024), .ht = 1};
  dt_roi_t roi45 = {.wd = (uint32_t)(9216), .ht = 1};
  dt_roi_t roi46 = {.wd = (uint32_t)(36864), .ht = 1};
  dt_roi_t roi47 = {.wd = (uint32_t)(36864), .ht = 1};
  dt_roi_t roi48 = {.wd = (uint32_t)(36864), .ht = 1};
  dt_roi_t roi49 = {.wd = (uint32_t)(36864), .ht = 1};
  dt_roi_t roi50 = {.wd = (uint32_t)(36864), .ht = 1};
  dt_roi_t roi51 = {.wd = (uint32_t)(147456), .ht = 1};
  dt_roi_t roi52 = {.wd = (uint32_t)(147456), .ht = 1};
  dt_roi_t roi53 = {.wd = (uint32_t)(589824), .ht = 1};
  dt_roi_t roi54 = {.wd = (uint32_t)(442368), .ht = 1};
  dt_roi_t roi55 = {.wd = (uint32_t)(1769472), .ht = 1};
  dt_roi_t roi56 = {.wd = (uint32_t)(1179648), .ht = 1};
  dt_roi_t roi57 = {.wd = (uint32_t)(7077888), .ht = 1};
  dt_roi_t roi58 = {.wd = 1, .ht = 1};
  dt_roi_t roi59 = {.wd = (uint32_t)(2359296), .ht = 1};
  dt_roi_t roi60 = {.wd = (uint32_t)(737280 / 2), .ht = 1};
  int weights_id = dt_node_add(graph, module, "segment", "weights",
      1, 1, 1, 0, NULL, 1, 
      "w", "source", "ssbo", "u8", &roi0);
  // direct_conv_cm (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_pc[4] = {384, 384, 192, 192};
  const int direct_conv_cm_id = dt_node_add(graph, module, "segment", "aseg36",
      1 * DT_LOCAL_SIZE_X, 12 * DT_LOCAL_SIZE_Y, 16,
      16, (const int*)direct_conv_cm_pc, 4, //
      "a", "read", "ssbo", "f16", &roi1, // HWC[3] : f16
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi0,// unknown
      "d", "write", "ssbo", "u8", &roi2); // HWC[32] : f16
  // direct_conv_cm_1 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_1_pc[4] = {192, 192, 192, 192};
  const int direct_conv_cm_1_id = dt_node_add(graph, module, "segment", "aseg32",
      1 * DT_LOCAL_SIZE_X, 12 * DT_LOCAL_SIZE_Y, 48,
      16, (const int*)direct_conv_cm_1_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi2, // HWC[32] : f16
      "d", "write", "ssbo", "u8", &roi3); // CHWC8[32] : f16
  // direct_conv_cm_2 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_2_pc[4] = {96, 96, 96, 96};
  const int direct_conv_cm_2_id = dt_node_add(graph, module, "segment", "aseg29",
      1 * DT_LOCAL_SIZE_X, 6 * DT_LOCAL_SIZE_Y, 12,
      16, (const int*)direct_conv_cm_2_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi3, // CHWC8[32] : f16
      "d", "write", "ssbo", "u8", &roi4); // HWC[64] : f16
  // direct_conv_cm_3 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_3_pc[4] = {96, 96, 96, 96};
  const int direct_conv_cm_3_id = dt_node_add(graph, module, "segment", "aseg26",
      1 * DT_LOCAL_SIZE_X, 6 * DT_LOCAL_SIZE_Y, 16,
      16, (const int*)direct_conv_cm_3_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi4, // HWC[64] : f16
      "d", "write", "ssbo", "u8", &roi5); // HWC[64] : f16
  // direct_conv_cm_4 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_4_pc[4] = {96, 96, 96, 96};
  const int direct_conv_cm_4_id = dt_node_add(graph, module, "segment", "aseg38",
      1 * DT_LOCAL_SIZE_X, 6 * DT_LOCAL_SIZE_Y, 24,
      16, (const int*)direct_conv_cm_4_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi3, // CHWC8[32] : f16
      "d", "write", "ssbo", "u8", &roi6); // HWC[64] : f16
  // basic_add (basic_add.comp)
  const uint32_t basic_add_pc[2] = {96, 96};
  const int basic_add_id = dt_node_add(graph, module, "segment", "aseg46",
      1 * DT_LOCAL_SIZE_X, 1 * DT_LOCAL_SIZE_Y, 48,
      8, (const int*)basic_add_pc, 3, //
      "a", "read", "ssbo", "u8", &roi5, // HWC[64] : f16
      "b", "read", "ssbo", "u8", &roi6, // HWC[64] : f16
      "c", "write", "ssbo", "u8", &roi7); // HWC[64] : f16
  // direct_conv_cm_5 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_5_pc[4] = {96, 96, 96, 96};
  const int direct_conv_cm_5_id = dt_node_add(graph, module, "segment", "aseg23",
      1 * DT_LOCAL_SIZE_X, 6 * DT_LOCAL_SIZE_Y, 8,
      16, (const int*)direct_conv_cm_5_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi7, // HWC[64] : f16
      "d", "write", "ssbo", "u8", &roi8); // HWC[64] : f16
  // direct_conv_cm_6 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_6_pc[4] = {96, 96, 96, 96};
  const int direct_conv_cm_6_id = dt_node_add(graph, module, "segment", "aseg26",
      1 * DT_LOCAL_SIZE_X, 6 * DT_LOCAL_SIZE_Y, 16,
      16, (const int*)direct_conv_cm_6_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi8, // HWC[64] : f16
      "d", "write", "ssbo", "u8", &roi9); // HWC[64] : f16
  // basic_add_1 (basic_add.comp)
  const uint32_t basic_add_1_pc[2] = {96, 96};
  const int basic_add_1_id = dt_node_add(graph, module, "segment", "aseg46",
      1 * DT_LOCAL_SIZE_X, 1 * DT_LOCAL_SIZE_Y, 48,
      8, (const int*)basic_add_1_pc, 3, //
      "a", "read", "ssbo", "u8", &roi9, // HWC[64] : f16
      "b", "read", "ssbo", "u8", &roi7, // HWC[64] : f16
      "c", "write", "ssbo", "u8", &roi10); // HWC[64] : f16
  // basic_pool (basic_pool.comp)
  const uint32_t basic_pool_pc[2] = {96, 96};
  const int basic_pool_id = dt_node_add(graph, module, "segment", "aseg28",
      1 * DT_LOCAL_SIZE_X, 2 * DT_LOCAL_SIZE_Y, 48,
      8, (const int*)basic_pool_pc, 2, //
      "a", "read", "ssbo", "u8", &roi10, // HWC[64] : f16
      "b", "write", "ssbo", "u8", &roi11); // HWC[64] : f16
  // direct_conv_cm_7 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_7_pc[4] = {48, 48, 48, 48};
  const int direct_conv_cm_7_id = dt_node_add(graph, module, "segment", "aseg34",
      1 * DT_LOCAL_SIZE_X, 3 * DT_LOCAL_SIZE_Y, 6,
      16, (const int*)direct_conv_cm_7_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi11, // HWC[64] : f16
      "d", "write", "ssbo", "u8", &roi12); // HWC[128] : f16
  // direct_conv_cm_8 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_8_pc[4] = {96, 96, 48, 48};
  const int direct_conv_cm_8_id = dt_node_add(graph, module, "segment", "aseg19",
      1 * DT_LOCAL_SIZE_X, 3 * DT_LOCAL_SIZE_Y, 24,
      16, (const int*)direct_conv_cm_8_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi10, // HWC[64] : f16
      "d", "write", "ssbo", "u8", &roi13); // CHWC8[128] : f16
  // direct_conv_cm_9 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_9_pc[4] = {48, 48, 48, 48};
  const int direct_conv_cm_9_id = dt_node_add(graph, module, "segment", "aseg17",
      1 * DT_LOCAL_SIZE_X, 3 * DT_LOCAL_SIZE_Y, 16,
      16, (const int*)direct_conv_cm_9_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi13, // CHWC8[128] : f16
      "d", "write", "ssbo", "u8", &roi14); // HWC[128] : f16
  // basic_add_2 (basic_add.comp)
  const uint32_t basic_add_2_pc[2] = {48, 48};
  const int basic_add_2_id = dt_node_add(graph, module, "segment", "aseg15",
      2 * DT_LOCAL_SIZE_X, 1 * DT_LOCAL_SIZE_Y, 24,
      8, (const int*)basic_add_2_pc, 3, //
      "a", "read", "ssbo", "u8", &roi14, // HWC[128] : f16
      "b", "read", "ssbo", "u8", &roi12, // HWC[128] : f16
      "c", "write", "ssbo", "u8", &roi15); // HWC[128] : f16
  // direct_conv_cm_10 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_10_pc[4] = {48, 48, 48, 48};
  const int direct_conv_cm_10_id = dt_node_add(graph, module, "segment", "aseg24",
      1 * DT_LOCAL_SIZE_X, 3 * DT_LOCAL_SIZE_Y, 12,
      16, (const int*)direct_conv_cm_10_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi15, // HWC[128] : f16
      "d", "write", "ssbo", "u8", &roi16); // CHWC8[128] : f16
  // direct_conv_cm_11 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_11_pc[4] = {48, 48, 48, 48};
  const int direct_conv_cm_11_id = dt_node_add(graph, module, "segment", "aseg17",
      1 * DT_LOCAL_SIZE_X, 3 * DT_LOCAL_SIZE_Y, 16,
      16, (const int*)direct_conv_cm_11_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi16, // CHWC8[128] : f16
      "d", "write", "ssbo", "u8", &roi17); // HWC[128] : f16
  // basic_add_3 (basic_add.comp)
  const uint32_t basic_add_3_pc[2] = {48, 48};
  const int basic_add_3_id = dt_node_add(graph, module, "segment", "aseg15",
      2 * DT_LOCAL_SIZE_X, 1 * DT_LOCAL_SIZE_Y, 24,
      8, (const int*)basic_add_3_pc, 3, //
      "a", "read", "ssbo", "u8", &roi17, // HWC[128] : f16
      "b", "read", "ssbo", "u8", &roi15, // HWC[128] : f16
      "c", "write", "ssbo", "u8", &roi18); // HWC[128] : f16
  // basic_pool_1 (basic_pool.comp)
  const uint32_t basic_pool_1_pc[2] = {48, 48};
  const int basic_pool_1_id = dt_node_add(graph, module, "segment", "aseg31",
      2 * DT_LOCAL_SIZE_X, 1 * DT_LOCAL_SIZE_Y, 24,
      8, (const int*)basic_pool_1_pc, 2, //
      "a", "read", "ssbo", "u8", &roi18, // HWC[128] : f16
      "b", "write", "ssbo", "u8", &roi19); // HWC[128] : f16
  // direct_conv_cm_12 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_12_pc[4] = {24, 24, 24, 24};
  const int direct_conv_cm_12_id = dt_node_add(graph, module, "segment", "aseg25",
      1 * DT_LOCAL_SIZE_X, 2 * DT_LOCAL_SIZE_Y, 12,
      16, (const int*)direct_conv_cm_12_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi19, // HWC[128] : f16
      "d", "write", "ssbo", "u8", &roi20); // HWC[256] : f16
  // direct_conv_cm_13 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_13_pc[4] = {48, 48, 24, 24};
  const int direct_conv_cm_13_id = dt_node_add(graph, module, "segment", "aseg40",
      1 * DT_LOCAL_SIZE_X, 2 * DT_LOCAL_SIZE_Y, 12,
      16, (const int*)direct_conv_cm_13_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi18, // HWC[128] : f16
      "d", "write", "ssbo", "u8", &roi21); // CHWC8[256] : f16
  // direct_conv_cm_14 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_14_pc[4] = {24, 24, 24, 24};
  const int direct_conv_cm_14_id = dt_node_add(graph, module, "segment", "aseg42",
      1 * DT_LOCAL_SIZE_X, 2 * DT_LOCAL_SIZE_Y, 8,
      16, (const int*)direct_conv_cm_14_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi21, // CHWC8[256] : f16
      "d", "write", "ssbo", "u8", &roi22); // HWC[256] : f16
  // basic_add_4 (basic_add.comp)
  const uint32_t basic_add_4_pc[2] = {24, 24};
  const int basic_add_4_id = dt_node_add(graph, module, "segment", "aseg41",
      2 * DT_LOCAL_SIZE_X, 1 * DT_LOCAL_SIZE_Y, 24,
      8, (const int*)basic_add_4_pc, 3, //
      "a", "read", "ssbo", "u8", &roi22, // HWC[256] : f16
      "b", "read", "ssbo", "u8", &roi20, // HWC[256] : f16
      "c", "write", "ssbo", "u8", &roi23); // HWC[256] : f16
  // direct_conv_cm_15 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_15_pc[4] = {24, 24, 24, 24};
  const int direct_conv_cm_15_id = dt_node_add(graph, module, "segment", "aseg35",
      1 * DT_LOCAL_SIZE_X, 2 * DT_LOCAL_SIZE_Y, 24,
      16, (const int*)direct_conv_cm_15_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi23, // HWC[256] : f16
      "d", "write", "ssbo", "u8", &roi24); // CHWC8[256] : f16
  // direct_conv_cm_16 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_16_pc[4] = {24, 24, 24, 24};
  const int direct_conv_cm_16_id = dt_node_add(graph, module, "segment", "aseg42",
      1 * DT_LOCAL_SIZE_X, 2 * DT_LOCAL_SIZE_Y, 8,
      16, (const int*)direct_conv_cm_16_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi24, // CHWC8[256] : f16
      "d", "write", "ssbo", "u8", &roi25); // HWC[256] : f16
  // basic_add_5 (basic_add.comp)
  const uint32_t basic_add_5_pc[2] = {24, 24};
  const int basic_add_5_id = dt_node_add(graph, module, "segment", "aseg41",
      2 * DT_LOCAL_SIZE_X, 1 * DT_LOCAL_SIZE_Y, 24,
      8, (const int*)basic_add_5_pc, 3, //
      "a", "read", "ssbo", "u8", &roi25, // HWC[256] : f16
      "b", "read", "ssbo", "u8", &roi23, // HWC[256] : f16
      "c", "write", "ssbo", "u8", &roi26); // HWC[256] : f16
  // basic_pool_2 (basic_pool.comp)
  const uint32_t basic_pool_2_pc[2] = {24, 24};
  const int basic_pool_2_id = dt_node_add(graph, module, "segment", "aseg21",
      6 * DT_LOCAL_SIZE_X, 1 * DT_LOCAL_SIZE_Y, 12,
      8, (const int*)basic_pool_2_pc, 2, //
      "a", "read", "ssbo", "u8", &roi26, // HWC[256] : f16
      "b", "write", "ssbo", "u8", &roi27); // HWC[256] : f16
  // direct_conv_cm_17 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_17_pc[4] = {12, 12, 12, 12};
  const int direct_conv_cm_17_id = dt_node_add(graph, module, "segment", "aseg27",
      8 * DT_LOCAL_SIZE_X, 1 * DT_LOCAL_SIZE_Y, 3,
      16, (const int*)direct_conv_cm_17_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi27, // HWC[256] : f16
      "d", "write", "ssbo", "u8", &roi28); // HWC[512] : f16
  // direct_conv_cm_18 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_18_pc[4] = {24, 24, 12, 12};
  const int direct_conv_cm_18_id = dt_node_add(graph, module, "segment", "aseg43",
      8 * DT_LOCAL_SIZE_X, 1 * DT_LOCAL_SIZE_Y, 6,
      16, (const int*)direct_conv_cm_18_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi26, // HWC[256] : f16
      "d", "write", "ssbo", "u8", &roi29); // HWC[512] : f16
  // direct_conv_cm_19 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_19_pc[4] = {12, 12, 12, 12};
  const int direct_conv_cm_19_id = dt_node_add(graph, module, "segment", "aseg37",
      16 * DT_LOCAL_SIZE_X, 1 * DT_LOCAL_SIZE_Y, 3,
      16, (const int*)direct_conv_cm_19_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi29, // HWC[512] : f16
      "d", "write", "ssbo", "u8", &roi30); // HWC[512] : f16
  // basic_add_6 (basic_add.comp)
  const uint32_t basic_add_6_pc[2] = {12, 12};
  const int basic_add_6_id = dt_node_add(graph, module, "segment", "aseg44",
      16 * DT_LOCAL_SIZE_X, 1 * DT_LOCAL_SIZE_Y, 12,
      8, (const int*)basic_add_6_pc, 3, //
      "a", "read", "ssbo", "u8", &roi30, // HWC[512] : f16
      "b", "read", "ssbo", "u8", &roi28, // HWC[512] : f16
      "c", "write", "ssbo", "u8", &roi31); // HWC[512] : f16
  // direct_conv_cm_20 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_20_pc[4] = {12, 12, 12, 12};
  const int direct_conv_cm_20_id = dt_node_add(graph, module, "segment", "aseg45",
      16 * DT_LOCAL_SIZE_X, 1 * DT_LOCAL_SIZE_Y, 3,
      16, (const int*)direct_conv_cm_20_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi31, // HWC[512] : f16
      "d", "write", "ssbo", "u8", &roi32); // HWC[512] : f16
  // direct_conv_cm_21 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_21_pc[4] = {12, 12, 12, 12};
  const int direct_conv_cm_21_id = dt_node_add(graph, module, "segment", "aseg37",
      16 * DT_LOCAL_SIZE_X, 1 * DT_LOCAL_SIZE_Y, 3,
      16, (const int*)direct_conv_cm_21_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi32, // HWC[512] : f16
      "d", "write", "ssbo", "u8", &roi33); // HWC[512] : f16
  // basic_add_7 (basic_add.comp)
  const uint32_t basic_add_7_pc[2] = {12, 12};
  const int basic_add_7_id = dt_node_add(graph, module, "segment", "aseg44",
      16 * DT_LOCAL_SIZE_X, 1 * DT_LOCAL_SIZE_Y, 12,
      8, (const int*)basic_add_7_pc, 3, //
      "a", "read", "ssbo", "u8", &roi33, // HWC[512] : f16
      "b", "read", "ssbo", "u8", &roi31, // HWC[512] : f16
      "c", "write", "ssbo", "u8", &roi34); // HWC[512] : f16
  // basic_pool_3 (basic_pool.comp)
  const uint32_t basic_pool_3_pc[2] = {12, 12};
  const int basic_pool_3_id = dt_node_add(graph, module, "segment", "aseg30",
      11 * DT_LOCAL_SIZE_X, 1 * DT_LOCAL_SIZE_Y, 1,
      8, (const int*)basic_pool_3_pc, 2, //
      "a", "read", "ssbo", "u8", &roi34, // HWC[512] : f16
      "b", "write", "ssbo", "u8", &roi35); // HWC[512] : f16
  // direct_conv_cm_22 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_22_pc[4] = {1, 1, 1, 1};
  const int direct_conv_cm_22_id = dt_node_add(graph, module, "segment", "aseg47",
      1 * DT_LOCAL_SIZE_X, 1 * DT_LOCAL_SIZE_Y, 1,
      16, (const int*)direct_conv_cm_22_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi35, // HWC[512] : f16
      "d", "write", "ssbo", "u8", &roi36); // HWC[32] : f16
  // basic_upsample (basic_upsample.comp)
  const uint32_t basic_upsample_pc[2] = {1, 1};
  const int basic_upsample_id = dt_node_add(graph, module, "segment", "aseg39",
      1 * DT_LOCAL_SIZE_X, 1 * DT_LOCAL_SIZE_Y, 6,
      8, (const int*)basic_upsample_pc, 2, //
      "a", "read", "ssbo", "u8", &roi36, // HWC[32] : f16
      "b", "write", "ssbo", "u8", &roi37); // HWC[32] : f16
  // direct_conv_cm_23 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_23_pc[4] = {12, 12, 12, 12};
  const int direct_conv_cm_23_id = dt_node_add(graph, module, "segment", "aseg48",
      1 * DT_LOCAL_SIZE_X, 1 * DT_LOCAL_SIZE_Y, 3,
      16, (const int*)direct_conv_cm_23_pc, 3, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi37, // HWC[32] : f16
      "c", "write", "ssbo", "u8", &roi38); // HWC[128] : f16
  // basic_pool_4 (basic_pool.comp)
  const uint32_t basic_pool_4_pc[2] = {12, 12};
  const int basic_pool_4_id = dt_node_add(graph, module, "segment", "aseg16",
      7 * DT_LOCAL_SIZE_X, 1 * DT_LOCAL_SIZE_Y, 2,
      8, (const int*)basic_pool_4_pc, 2, //
      "a", "read", "ssbo", "u8", &roi34, // HWC[512] : f16
      "b", "write", "ssbo", "u8", &roi39); // HWC[512] : f16
  // direct_conv_cm_24 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_24_pc[4] = {2, 2, 2, 2};
  const int direct_conv_cm_24_id = dt_node_add(graph, module, "segment", "aseg14",
      1 * DT_LOCAL_SIZE_X, 1 * DT_LOCAL_SIZE_Y, 1,
      16, (const int*)direct_conv_cm_24_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi39, // HWC[512] : f16
      "d", "write", "ssbo", "u8", &roi40); // HWC[32] : f16
  // basic_upsample_1 (basic_upsample.comp)
  const uint32_t basic_upsample_1_pc[2] = {2, 2};
  const int basic_upsample_1_id = dt_node_add(graph, module, "segment", "aseg20",
      4 * DT_LOCAL_SIZE_X, 1 * DT_LOCAL_SIZE_Y, 12,
      8, (const int*)basic_upsample_1_pc, 2, //
      "a", "read", "ssbo", "u8", &roi40, // HWC[32] : f16
      "b", "write", "ssbo", "u8", &roi41); // HWC[32] : f16
  // direct_conv_cm_25 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_25_pc[4] = {12, 12, 12, 12};
  const int direct_conv_cm_25_id = dt_node_add(graph, module, "segment", "aseg48",
      1 * DT_LOCAL_SIZE_X, 1 * DT_LOCAL_SIZE_Y, 3,
      16, (const int*)direct_conv_cm_25_pc, 3, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi41, // HWC[32] : f16
      "c", "write", "ssbo", "u8", &roi42); // HWC[128] : f16
  // basic_pool_5 (basic_pool.comp)
  const uint32_t basic_pool_5_pc[2] = {12, 12};
  const int basic_pool_5_id = dt_node_add(graph, module, "segment", "aseg33",
      7 * DT_LOCAL_SIZE_X, 1 * DT_LOCAL_SIZE_Y, 4,
      8, (const int*)basic_pool_5_pc, 2, //
      "a", "read", "ssbo", "u8", &roi34, // HWC[512] : f16
      "b", "write", "ssbo", "u8", &roi43); // HWC[512] : f16
  // direct_conv_cm_26 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_26_pc[4] = {4, 4, 4, 4};
  const int direct_conv_cm_26_id = dt_node_add(graph, module, "segment", "aseg12",
      1 * DT_LOCAL_SIZE_X, 1 * DT_LOCAL_SIZE_Y, 1,
      16, (const int*)direct_conv_cm_26_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi43, // HWC[512] : f16
      "d", "write", "ssbo", "u8", &roi44); // CHWC8[32] : f16
  // basic_upsample_2 (basic_upsample.comp)
  const uint32_t basic_upsample_2_pc[2] = {4, 4};
  const int basic_upsample_2_id = dt_node_add(graph, module, "segment", "aseg11",
      4 * DT_LOCAL_SIZE_X, 1 * DT_LOCAL_SIZE_Y, 6,
      8, (const int*)basic_upsample_2_pc, 2, //
      "a", "read", "ssbo", "u8", &roi44, // CHWC8[32] : f16
      "b", "write", "ssbo", "u8", &roi45); // CHWC8[32] : f16
  // direct_conv_cm_27 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_27_pc[4] = {12, 12, 12, 12};
  const int direct_conv_cm_27_id = dt_node_add(graph, module, "segment", "aseg22",
      1 * DT_LOCAL_SIZE_X, 1 * DT_LOCAL_SIZE_Y, 3,
      16, (const int*)direct_conv_cm_27_pc, 3, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi45, // CHWC8[32] : f16
      "c", "write", "ssbo", "u8", &roi46); // HWC[128] : f16
  // direct_conv_cm_28 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_28_pc[4] = {12, 12, 12, 12};
  const int direct_conv_cm_28_id = dt_node_add(graph, module, "segment", "aseg9",
      1 * DT_LOCAL_SIZE_X, 1 * DT_LOCAL_SIZE_Y, 2,
      16, (const int*)direct_conv_cm_28_pc, 4, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi34, // HWC[512] : f16
      "d", "write", "ssbo", "u8", &roi47); // HWC[128] : f16
  // basic_add_8 (basic_add.comp)
  const uint32_t basic_add_8_pc[2] = {12, 12};
  const int basic_add_8_id = dt_node_add(graph, module, "segment", "aseg8",
      4 * DT_LOCAL_SIZE_X, 1 * DT_LOCAL_SIZE_Y, 6,
      8, (const int*)basic_add_8_pc, 3, //
      "a", "read", "ssbo", "u8", &roi47, // HWC[128] : f16
      "b", "read", "ssbo", "u8", &roi38, // HWC[128] : f16
      "c", "write", "ssbo", "u8", &roi48); // HWC[128] : f16
  // basic_add_9 (basic_add.comp)
  const uint32_t basic_add_9_pc[2] = {12, 12};
  const int basic_add_9_id = dt_node_add(graph, module, "segment", "aseg8",
      4 * DT_LOCAL_SIZE_X, 1 * DT_LOCAL_SIZE_Y, 6,
      8, (const int*)basic_add_9_pc, 3, //
      "a", "read", "ssbo", "u8", &roi48, // HWC[128] : f16
      "b", "read", "ssbo", "u8", &roi42, // HWC[128] : f16
      "c", "write", "ssbo", "u8", &roi49); // HWC[128] : f16
  // basic_add_10 (basic_add.comp)
  const uint32_t basic_add_10_pc[2] = {12, 12};
  const int basic_add_10_id = dt_node_add(graph, module, "segment", "aseg13",
      16 * DT_LOCAL_SIZE_X, 1 * DT_LOCAL_SIZE_Y, 12,
      8, (const int*)basic_add_10_pc, 3, //
      "a", "read", "ssbo", "u8", &roi49, // HWC[128] : f16
      "b", "read", "ssbo", "u8", &roi46, // HWC[128] : f16
      "c", "write", "ssbo", "u8", &roi50); // HWC[128] : f16
  // basic_upsample_3 (basic_upsample.comp)
  const uint32_t basic_upsample_3_pc[2] = {12, 12};
  const int basic_upsample_3_id = dt_node_add(graph, module, "segment", "aseg18",
      2 * DT_LOCAL_SIZE_X, 1 * DT_LOCAL_SIZE_Y, 12,
      8, (const int*)basic_upsample_3_pc, 2, //
      "a", "read", "ssbo", "u8", &roi50, // HWC[128] : f16
      "b", "write", "ssbo", "u8", &roi51); // HWC[128] : f16
  // concat_conv_cm (concat_conv_cm.comp)
  const uint32_t concat_conv_cm_pc[2] = {24, 24};
  const int concat_conv_cm_id = dt_node_add(graph, module, "segment", "aseg7",
      1 * DT_LOCAL_SIZE_X, 2 * DT_LOCAL_SIZE_Y, 8,
      8, (const int*)concat_conv_cm_pc, 6, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi0,// unknown
      "d", "read", "ssbo", "u8", &roi51, // HWC[128] : f16
      "e", "read", "ssbo", "u8", &roi26, // HWC[256] : f16
      "f", "write", "ssbo", "u8", &roi52); // HWC[128] : f16
  // basic_upsample_4 (basic_upsample.comp)
  const uint32_t basic_upsample_4_pc[2] = {24, 24};
  const int basic_upsample_4_id = dt_node_add(graph, module, "segment", "aseg6",
      2 * DT_LOCAL_SIZE_X, 1 * DT_LOCAL_SIZE_Y, 48,
      8, (const int*)basic_upsample_4_pc, 2, //
      "a", "read", "ssbo", "u8", &roi52, // HWC[128] : f16
      "b", "write", "ssbo", "u8", &roi53); // HWC[128] : f16
  // concat_conv_cm_1 (concat_conv_cm.comp)
  const uint32_t concat_conv_cm_1_pc[2] = {48, 48};
  const int concat_conv_cm_1_id = dt_node_add(graph, module, "segment", "aseg4",
      1 * DT_LOCAL_SIZE_X, 3 * DT_LOCAL_SIZE_Y, 12,
      8, (const int*)concat_conv_cm_1_pc, 6, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi0,// unknown
      "d", "read", "ssbo", "u8", &roi53, // HWC[128] : f16
      "e", "read", "ssbo", "u8", &roi18, // HWC[128] : f16
      "f", "write", "ssbo", "u8", &roi54); // CHWC8[96] : f16
  // basic_upsample_5 (basic_upsample.comp)
  const uint32_t basic_upsample_5_pc[2] = {48, 48};
  const int basic_upsample_5_id = dt_node_add(graph, module, "segment", "aseg3",
      12 * DT_LOCAL_SIZE_X, 1 * DT_LOCAL_SIZE_Y, 96,
      8, (const int*)basic_upsample_5_pc, 2, //
      "a", "read", "ssbo", "u8", &roi54, // CHWC8[96] : f16
      "b", "write", "ssbo", "u8", &roi55); // CHWC8[96] : f16
  // concat_conv_cm_2 (concat_conv_cm.comp)
  const uint32_t concat_conv_cm_2_pc[2] = {96, 96};
  const int concat_conv_cm_2_id = dt_node_add(graph, module, "segment", "aseg2",
      1 * DT_LOCAL_SIZE_X, 6 * DT_LOCAL_SIZE_Y, 20,
      8, (const int*)concat_conv_cm_2_pc, 6, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi0,// unknown
      "d", "read", "ssbo", "u8", &roi55, // CHWC8[96] : f16
      "e", "read", "ssbo", "u8", &roi10, // HWC[64] : f16
      "f", "write", "ssbo", "u8", &roi56); // CHWC8[64] : f16
  // basic_upsample_6 (basic_upsample.comp)
  const uint32_t basic_upsample_6_pc[2] = {96, 96};
  const int basic_upsample_6_id = dt_node_add(graph, module, "segment", "aseg10",
      8 * DT_LOCAL_SIZE_X, 1 * DT_LOCAL_SIZE_Y, 192,
      8, (const int*)basic_upsample_6_pc, 2, //
      "a", "read", "ssbo", "u8", &roi56, // CHWC8[64] : f16
      "b", "write", "ssbo", "u8", &roi57); // CHWC8[64] : f16
  // direct_conv_cm_29 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_29_pc[4] = {192, 192, 192, 192};
  const int direct_conv_cm_29_id = dt_node_add(graph, module, "segment", "aseg1",
      1 * DT_LOCAL_SIZE_X, 12 * DT_LOCAL_SIZE_Y, 24,
      16, (const int*)direct_conv_cm_29_pc, 5, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi2, // HWC[32] : f16
      "d", "read", "ssbo", "u8", &roi57, // CHWC8[32] : f16
      "dummy", "write", "ssbo", "u8", &roi58);//
  // direct_conv_cm_30 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_30_pc[4] = {192, 192, 192, 192};
  const int direct_conv_cm_30_id = dt_node_add(graph, module, "segment", "aseg5",
      1 * DT_LOCAL_SIZE_X, 12 * DT_LOCAL_SIZE_Y, 48,
      16, (const int*)direct_conv_cm_30_pc, 5, //
      "a", "read", "ssbo", "u8", &roi0,// unknown
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi57, // CHWC8[96] : f16
      "d", "write", "ssbo", "u8", &roi59, // HWC[32] : f16
      "z0", "read", "ssbo", "u8", &roi58);//
  // direct_conv_cm_31 (direct_conv_cm.comp)
  const uint32_t direct_conv_cm_31_pc[4] = {192, 192, 192, 192};
  const int direct_conv_cm_31_id = dt_node_add(graph, module, "segment", "aseg0",
      1 * DT_LOCAL_SIZE_X, 12 * DT_LOCAL_SIZE_Y, 48,
      16, (const int*)direct_conv_cm_31_pc, 4, //
      "a", "write", "ssbo", "f16", &roi60, // HWC[10] : f16
      "b", "read", "ssbo", "u8", &roi0,// unknown
      "c", "read", "ssbo", "u8", &roi0,// unknown
      "d", "read", "ssbo", "u8", &roi59); // HWC[32] : f16
  if (input_connector == NULL) {
    dt_connector_copy(graph, module, input_id, direct_conv_cm_id, 0);
  } else {
    dt_node_connect_named(graph, input_id, input_connector, direct_conv_cm_id, "a");
  }
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_id, "b");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_id, "c");
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
  dt_node_connect_named(graph, direct_conv_cm_1_id, "d", direct_conv_cm_4_id, "c");
  dt_node_connect_named(graph, direct_conv_cm_3_id, "d", basic_add_id, "a");
  dt_node_connect_named(graph, direct_conv_cm_4_id, "d", basic_add_id, "b");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_5_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_5_id, "b");
  dt_node_connect_named(graph, basic_add_id, "c", direct_conv_cm_5_id, "c");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_6_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_6_id, "b");
  dt_node_connect_named(graph, direct_conv_cm_5_id, "d", direct_conv_cm_6_id, "c");
  dt_node_connect_named(graph, direct_conv_cm_6_id, "d", basic_add_1_id, "a");
  dt_node_connect_named(graph, basic_add_id, "c", basic_add_1_id, "b");
  dt_node_connect_named(graph, basic_add_1_id, "c", basic_pool_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_7_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_7_id, "b");
  dt_node_connect_named(graph, basic_pool_id, "b", direct_conv_cm_7_id, "c");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_8_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_8_id, "b");
  dt_node_connect_named(graph, basic_add_1_id, "c", direct_conv_cm_8_id, "c");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_9_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_9_id, "b");
  dt_node_connect_named(graph, direct_conv_cm_8_id, "d", direct_conv_cm_9_id, "c");
  dt_node_connect_named(graph, direct_conv_cm_9_id, "d", basic_add_2_id, "a");
  dt_node_connect_named(graph, direct_conv_cm_7_id, "d", basic_add_2_id, "b");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_10_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_10_id, "b");
  dt_node_connect_named(graph, basic_add_2_id, "c", direct_conv_cm_10_id, "c");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_11_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_11_id, "b");
  dt_node_connect_named(graph, direct_conv_cm_10_id, "d", direct_conv_cm_11_id, "c");
  dt_node_connect_named(graph, direct_conv_cm_11_id, "d", basic_add_3_id, "a");
  dt_node_connect_named(graph, basic_add_2_id, "c", basic_add_3_id, "b");
  dt_node_connect_named(graph, basic_add_3_id, "c", basic_pool_1_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_12_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_12_id, "b");
  dt_node_connect_named(graph, basic_pool_1_id, "b", direct_conv_cm_12_id, "c");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_13_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_13_id, "b");
  dt_node_connect_named(graph, basic_add_3_id, "c", direct_conv_cm_13_id, "c");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_14_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_14_id, "b");
  dt_node_connect_named(graph, direct_conv_cm_13_id, "d", direct_conv_cm_14_id, "c");
  dt_node_connect_named(graph, direct_conv_cm_14_id, "d", basic_add_4_id, "a");
  dt_node_connect_named(graph, direct_conv_cm_12_id, "d", basic_add_4_id, "b");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_15_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_15_id, "b");
  dt_node_connect_named(graph, basic_add_4_id, "c", direct_conv_cm_15_id, "c");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_16_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_16_id, "b");
  dt_node_connect_named(graph, direct_conv_cm_15_id, "d", direct_conv_cm_16_id, "c");
  dt_node_connect_named(graph, direct_conv_cm_16_id, "d", basic_add_5_id, "a");
  dt_node_connect_named(graph, basic_add_4_id, "c", basic_add_5_id, "b");
  dt_node_connect_named(graph, basic_add_5_id, "c", basic_pool_2_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_17_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_17_id, "b");
  dt_node_connect_named(graph, basic_pool_2_id, "b", direct_conv_cm_17_id, "c");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_18_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_18_id, "b");
  dt_node_connect_named(graph, basic_add_5_id, "c", direct_conv_cm_18_id, "c");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_19_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_19_id, "b");
  dt_node_connect_named(graph, direct_conv_cm_18_id, "d", direct_conv_cm_19_id, "c");
  dt_node_connect_named(graph, direct_conv_cm_19_id, "d", basic_add_6_id, "a");
  dt_node_connect_named(graph, direct_conv_cm_17_id, "d", basic_add_6_id, "b");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_20_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_20_id, "b");
  dt_node_connect_named(graph, basic_add_6_id, "c", direct_conv_cm_20_id, "c");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_21_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_21_id, "b");
  dt_node_connect_named(graph, direct_conv_cm_20_id, "d", direct_conv_cm_21_id, "c");
  dt_node_connect_named(graph, direct_conv_cm_21_id, "d", basic_add_7_id, "a");
  dt_node_connect_named(graph, basic_add_6_id, "c", basic_add_7_id, "b");
  dt_node_connect_named(graph, basic_add_7_id, "c", basic_pool_3_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_22_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_22_id, "b");
  dt_node_connect_named(graph, basic_pool_3_id, "b", direct_conv_cm_22_id, "c");
  dt_node_connect_named(graph, direct_conv_cm_22_id, "d", basic_upsample_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_23_id, "a");
  dt_node_connect_named(graph, basic_upsample_id, "b", direct_conv_cm_23_id, "b");
  dt_node_connect_named(graph, basic_add_7_id, "c", basic_pool_4_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_24_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_24_id, "b");
  dt_node_connect_named(graph, basic_pool_4_id, "b", direct_conv_cm_24_id, "c");
  dt_node_connect_named(graph, direct_conv_cm_24_id, "d", basic_upsample_1_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_25_id, "a");
  dt_node_connect_named(graph, basic_upsample_1_id, "b", direct_conv_cm_25_id, "b");
  dt_node_connect_named(graph, basic_add_7_id, "c", basic_pool_5_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_26_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_26_id, "b");
  dt_node_connect_named(graph, basic_pool_5_id, "b", direct_conv_cm_26_id, "c");
  dt_node_connect_named(graph, direct_conv_cm_26_id, "d", basic_upsample_2_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_27_id, "a");
  dt_node_connect_named(graph, basic_upsample_2_id, "b", direct_conv_cm_27_id, "b");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_28_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_28_id, "b");
  dt_node_connect_named(graph, basic_add_7_id, "c", direct_conv_cm_28_id, "c");
  dt_node_connect_named(graph, direct_conv_cm_28_id, "d", basic_add_8_id, "a");
  dt_node_connect_named(graph, direct_conv_cm_23_id, "c", basic_add_8_id, "b");
  dt_node_connect_named(graph, basic_add_8_id, "c", basic_add_9_id, "a");
  dt_node_connect_named(graph, direct_conv_cm_25_id, "c", basic_add_9_id, "b");
  dt_node_connect_named(graph, basic_add_9_id, "c", basic_add_10_id, "a");
  dt_node_connect_named(graph, direct_conv_cm_27_id, "c", basic_add_10_id, "b");
  dt_node_connect_named(graph, basic_add_10_id, "c", basic_upsample_3_id, "a");
  dt_node_connect_named(graph, weights_id, "w", concat_conv_cm_id, "a");
  dt_node_connect_named(graph, weights_id, "w", concat_conv_cm_id, "b");
  dt_node_connect_named(graph, weights_id, "w", concat_conv_cm_id, "c");
  dt_node_connect_named(graph, basic_upsample_3_id, "b", concat_conv_cm_id, "d");
  dt_node_connect_named(graph, basic_add_5_id, "c", concat_conv_cm_id, "e");
  dt_node_connect_named(graph, concat_conv_cm_id, "f", basic_upsample_4_id, "a");
  dt_node_connect_named(graph, weights_id, "w", concat_conv_cm_1_id, "a");
  dt_node_connect_named(graph, weights_id, "w", concat_conv_cm_1_id, "b");
  dt_node_connect_named(graph, weights_id, "w", concat_conv_cm_1_id, "c");
  dt_node_connect_named(graph, basic_upsample_4_id, "b", concat_conv_cm_1_id, "d");
  dt_node_connect_named(graph, basic_add_3_id, "c", concat_conv_cm_1_id, "e");
  dt_node_connect_named(graph, concat_conv_cm_1_id, "f", basic_upsample_5_id, "a");
  dt_node_connect_named(graph, weights_id, "w", concat_conv_cm_2_id, "a");
  dt_node_connect_named(graph, weights_id, "w", concat_conv_cm_2_id, "b");
  dt_node_connect_named(graph, weights_id, "w", concat_conv_cm_2_id, "c");
  dt_node_connect_named(graph, basic_upsample_5_id, "b", concat_conv_cm_2_id, "d");
  dt_node_connect_named(graph, basic_add_1_id, "c", concat_conv_cm_2_id, "e");
  dt_node_connect_named(graph, concat_conv_cm_2_id, "f", basic_upsample_6_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_29_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_29_id, "b");
  dt_node_connect_named(graph, direct_conv_cm_id, "d", direct_conv_cm_29_id, "c");
  dt_node_connect_named(graph, basic_upsample_6_id, "b", direct_conv_cm_29_id, "d");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_30_id, "a");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_30_id, "b");
  dt_node_connect_named(graph, basic_upsample_6_id, "b", direct_conv_cm_30_id, "c");
  dt_node_connect_named(graph, direct_conv_cm_29_id, "dummy", direct_conv_cm_30_id, "z0");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_31_id, "b");
  dt_node_connect_named(graph, weights_id, "w", direct_conv_cm_31_id, "c");
  dt_node_connect_named(graph, direct_conv_cm_30_id, "d", direct_conv_cm_31_id, "d");
  if (output_connector == NULL) {
    dt_connector_copy(graph, module, output_id, direct_conv_cm_31_id, 0);
  } else {
    dt_node_connect_named(graph, direct_conv_cm_31_id, "a", output_id, output_connector);
  }
  graph->node[direct_conv_cm_id].connector[2].ssbo_offset = 1728;
  graph->node[direct_conv_cm_id].required_subgroup_size = 32;
  graph->node[direct_conv_cm_1_id].connector[0].ssbo_offset = 1792;
  graph->node[direct_conv_cm_1_id].connector[1].ssbo_offset = 20224;
  graph->node[direct_conv_cm_1_id].required_subgroup_size = 32;
  graph->node[direct_conv_cm_2_id].connector[0].ssbo_offset = 20288;
  graph->node[direct_conv_cm_2_id].connector[1].ssbo_offset = 57152;
  graph->node[direct_conv_cm_2_id].required_subgroup_size = 32;
  graph->node[direct_conv_cm_3_id].connector[0].ssbo_offset = 57280;
  graph->node[direct_conv_cm_3_id].connector[1].ssbo_offset = 131008;
  graph->node[direct_conv_cm_3_id].required_subgroup_size = 32;
  graph->node[direct_conv_cm_4_id].connector[0].ssbo_offset = 131136;
  graph->node[direct_conv_cm_4_id].connector[1].ssbo_offset = 135232;
  graph->node[direct_conv_cm_4_id].required_subgroup_size = 32;
  graph->node[direct_conv_cm_5_id].connector[0].ssbo_offset = 135360;
  graph->node[direct_conv_cm_5_id].connector[1].ssbo_offset = 209088;
  graph->node[direct_conv_cm_5_id].required_subgroup_size = 32;
  graph->node[direct_conv_cm_6_id].connector[0].ssbo_offset = 209216;
  graph->node[direct_conv_cm_6_id].connector[1].ssbo_offset = 282944;
  graph->node[direct_conv_cm_6_id].required_subgroup_size = 32;
  graph->node[direct_conv_cm_7_id].connector[0].ssbo_offset = 283072;
  graph->node[direct_conv_cm_7_id].connector[1].ssbo_offset = 299456;
  graph->node[direct_conv_cm_7_id].required_subgroup_size = 32;
  graph->node[direct_conv_cm_8_id].connector[0].ssbo_offset = 299712;
  graph->node[direct_conv_cm_8_id].connector[1].ssbo_offset = 447168;
  graph->node[direct_conv_cm_8_id].required_subgroup_size = 32;
  graph->node[direct_conv_cm_9_id].connector[0].ssbo_offset = 447424;
  graph->node[direct_conv_cm_9_id].connector[1].ssbo_offset = 742336;
  graph->node[direct_conv_cm_9_id].required_subgroup_size = 32;
  graph->node[direct_conv_cm_10_id].connector[0].ssbo_offset = 742592;
  graph->node[direct_conv_cm_10_id].connector[1].ssbo_offset = 1037504;
  graph->node[direct_conv_cm_10_id].required_subgroup_size = 32;
  graph->node[direct_conv_cm_11_id].connector[0].ssbo_offset = 1037760;
  graph->node[direct_conv_cm_11_id].connector[1].ssbo_offset = 1332672;
  graph->node[direct_conv_cm_11_id].required_subgroup_size = 32;
  graph->node[direct_conv_cm_12_id].connector[0].ssbo_offset = 1332928;
  graph->node[direct_conv_cm_12_id].connector[1].ssbo_offset = 1398464;
  graph->node[direct_conv_cm_12_id].required_subgroup_size = 32;
  graph->node[direct_conv_cm_13_id].connector[0].ssbo_offset = 1398976;
  graph->node[direct_conv_cm_13_id].connector[1].ssbo_offset = 1988800;
  graph->node[direct_conv_cm_13_id].required_subgroup_size = 32;
  graph->node[direct_conv_cm_14_id].connector[0].ssbo_offset = 1989312;
  graph->node[direct_conv_cm_14_id].connector[1].ssbo_offset = 3168960;
  graph->node[direct_conv_cm_14_id].required_subgroup_size = 32;
  graph->node[direct_conv_cm_15_id].connector[0].ssbo_offset = 3169472;
  graph->node[direct_conv_cm_15_id].connector[1].ssbo_offset = 4349120;
  graph->node[direct_conv_cm_15_id].required_subgroup_size = 32;
  graph->node[direct_conv_cm_16_id].connector[0].ssbo_offset = 4349632;
  graph->node[direct_conv_cm_16_id].connector[1].ssbo_offset = 5529280;
  graph->node[direct_conv_cm_16_id].required_subgroup_size = 32;
  graph->node[direct_conv_cm_17_id].connector[0].ssbo_offset = 5529792;
  graph->node[direct_conv_cm_17_id].connector[1].ssbo_offset = 5791936;
  graph->node[direct_conv_cm_17_id].required_subgroup_size = 32;
  graph->node[direct_conv_cm_18_id].connector[0].ssbo_offset = 5792960;
  graph->node[direct_conv_cm_18_id].connector[1].ssbo_offset = 8152256;
  graph->node[direct_conv_cm_18_id].required_subgroup_size = 32;
  graph->node[direct_conv_cm_19_id].connector[0].ssbo_offset = 8153280;
  graph->node[direct_conv_cm_19_id].connector[1].ssbo_offset = 12871872;
  graph->node[direct_conv_cm_19_id].required_subgroup_size = 32;
  graph->node[direct_conv_cm_20_id].connector[0].ssbo_offset = 12872896;
  graph->node[direct_conv_cm_20_id].connector[1].ssbo_offset = 17591488;
  graph->node[direct_conv_cm_20_id].required_subgroup_size = 32;
  graph->node[direct_conv_cm_21_id].connector[0].ssbo_offset = 17592512;
  graph->node[direct_conv_cm_21_id].connector[1].ssbo_offset = 22311104;
  graph->node[direct_conv_cm_21_id].required_subgroup_size = 32;
  graph->node[direct_conv_cm_22_id].connector[0].ssbo_offset = 22312128;
  graph->node[direct_conv_cm_22_id].connector[1].ssbo_offset = 22344896;
  graph->node[direct_conv_cm_22_id].required_subgroup_size = 32;
  graph->node[direct_conv_cm_23_id].connector[0].ssbo_offset = 22344960;
  graph->node[direct_conv_cm_23_id].required_subgroup_size = 32;
  graph->node[direct_conv_cm_24_id].connector[0].ssbo_offset = 22418688;
  graph->node[direct_conv_cm_24_id].connector[1].ssbo_offset = 22451456;
  graph->node[direct_conv_cm_24_id].required_subgroup_size = 32;
  graph->node[direct_conv_cm_25_id].connector[0].ssbo_offset = 22451520;
  graph->node[direct_conv_cm_25_id].required_subgroup_size = 32;
  graph->node[direct_conv_cm_26_id].connector[0].ssbo_offset = 22525248;
  graph->node[direct_conv_cm_26_id].connector[1].ssbo_offset = 22558016;
  graph->node[direct_conv_cm_26_id].required_subgroup_size = 32;
  graph->node[direct_conv_cm_27_id].connector[0].ssbo_offset = 22558080;
  graph->node[direct_conv_cm_27_id].required_subgroup_size = 32;
  graph->node[direct_conv_cm_28_id].connector[0].ssbo_offset = 22631808;
  graph->node[direct_conv_cm_28_id].connector[1].ssbo_offset = 23811456;
  graph->node[direct_conv_cm_28_id].required_subgroup_size = 32;
  graph->node[concat_conv_cm_id].connector[0].ssbo_offset = 23811712;
  graph->node[concat_conv_cm_id].connector[1].ssbo_offset = 24106624;
  graph->node[concat_conv_cm_id].connector[2].ssbo_offset = 24696448;
  graph->node[concat_conv_cm_id].required_subgroup_size = 32;
  graph->node[concat_conv_cm_1_id].connector[0].ssbo_offset = 24696704;
  graph->node[concat_conv_cm_1_id].connector[1].ssbo_offset = 24917888;
  graph->node[concat_conv_cm_1_id].connector[2].ssbo_offset = 25139072;
  graph->node[concat_conv_cm_1_id].required_subgroup_size = 32;
  graph->node[concat_conv_cm_2_id].connector[0].ssbo_offset = 25139264;
  graph->node[concat_conv_cm_2_id].connector[1].ssbo_offset = 25249856;
  graph->node[concat_conv_cm_2_id].connector[2].ssbo_offset = 25323584;
  graph->node[concat_conv_cm_2_id].required_subgroup_size = 32;
  graph->node[direct_conv_cm_29_id].connector[0].ssbo_offset = 25323712;
  graph->node[direct_conv_cm_29_id].connector[1].ssbo_offset = 25342144;
  graph->node[direct_conv_cm_29_id].connector[3].ssbo_offset = 0;
  graph->node[direct_conv_cm_29_id].required_subgroup_size = 32;
  graph->node[direct_conv_cm_30_id].connector[0].ssbo_offset = 25342208;
  graph->node[direct_conv_cm_30_id].connector[1].ssbo_offset = 25397504;
  graph->node[direct_conv_cm_30_id].required_subgroup_size = 32;
  graph->node[direct_conv_cm_31_id].connector[1].ssbo_offset = 25397568;
  graph->node[direct_conv_cm_31_id].connector[2].ssbo_offset = 25398208;
  graph->node[direct_conv_cm_31_id].required_subgroup_size = 32;
}

#endif
