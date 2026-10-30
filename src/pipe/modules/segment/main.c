#include "modules/api.h"
#include "denox_nseg.h"
#include "denox_nseg_size.h"

#define NET_WD 384
#define NET_HT 384
#define NET_OUT 192 // logits grid
#define NET_CH 10   // people, sky, vegetation, water, mountain, ground, building, subject,
                    // artificial ground, clothing

static const char *weights = "data/segment-nseg.dat";

int read_source(dt_module_t *mod, void *mapped, dt_read_source_params_t *p)
{
  return denox_read_source_nseg(mod, mapped, p);
}

// empty mask, blending leaves the image unchanged
static void
create_nodes_empty(dt_graph_t *graph, dt_module_t *module)
{
  const dt_roi_t *roi = &module->connector[0].roi;
  const int id = dt_node_add(
      graph, module, "segment", "zero", roi->wd, roi->ht, 1, 0, 0, 2,
      "input",  "read",  "rgba", "*",   dt_no_roi,
      "output", "write", "y",    "f16", roi);
  dt_connector_copy(graph, module, 0, id, 0);
  dt_connector_copy(graph, module, 1, id, 1);
}

void create_nodes(dt_graph_t *graph, dt_module_t *module)
{
  if(qvk.vendorID != 0x10de) // nvidia
  {
    snprintf(graph->gui_msg_buf, sizeof(graph->gui_msg_buf),
        "segment: no network for this gpu vendor (0x%04x), mask is empty", qvk.vendorID);
    graph->gui_msg = graph->gui_msg_buf;
    return create_nodes_empty(graph, module);
  }
  // read_source can't fail gracefully, so check the weights here
  FILE *f = dt_graph_open_resource(graph, 0, weights, "rb");
  const int found = f != 0;
  size_t size = 0;
  if(found)
  {
    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fclose(f);
  }
  if(size != DENOX_NSEG_WEIGHTS_SIZE)
  {
    snprintf(graph->gui_msg_buf, sizeof(graph->gui_msg_buf),
        found ? "segment: \"%s\" has unexpected size, mask is empty" :
            "segment: could not find \"%s\", mask is empty", weights);
    graph->gui_msg = graph->gui_msg_buf;
    return create_nodes_empty(graph, module);
  }

  const dt_roi_t input_roi = module->connector[0].roi;
  const dt_roi_t input_ssbo_roi = { .wd = NET_WD * NET_HT * 3, .ht = 1 };

  // resample to the network resolution, write a contiguous hwc ssbo
  const int pc_in[] = { NET_WD, NET_HT }, pc_out[] = { NET_OUT, NET_OUT, NET_CH };
  const int input = dt_node_add(
      graph, module, "segment", "input", NET_WD, NET_HT, 1, sizeof(pc_in), pc_in, 2,
      "input",  "read",  "rgba", "*",   dt_no_roi,
      "output", "write", "ssbo", "f16", &input_ssbo_roi);

  // convert the logits of the chosen channel to a mask at input resolution
  const int output = dt_node_add(
      graph, module, "segment", "output", input_roi.wd, input_roi.ht, 1, sizeof(pc_out), pc_out, 2,
      "input",  "read",  "ssbo", "f16", dt_no_roi,
      "output", "write", "y",    "f16", &input_roi);

  denox_create_nodes_nseg(graph, module, input, "output", output, "input");

  dt_connector_copy(graph, module, 0, input,  0);
  dt_connector_copy(graph, module, 1, output, 1);
}
