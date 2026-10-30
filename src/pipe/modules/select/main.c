#include "modules/api.h"
#include "denox_nclk.h"
#include "denox_nclk_size.h"

#define NET_WD 512
#define NET_HT 512
#define NET_OUT 256 // logits grid
#define NET_RUNS 5  // at most this many runs: the first click (or the box), then the last clicks

static const char *weights = "data/select-nclk.dat";

// strokes are params, don't reload the weights
void animate(dt_graph_t *graph, dt_module_t *mod)
{
  mod->flags &= ~s_module_request_read_source;
}

int read_source(dt_module_t *mod, void *mapped, dt_read_source_params_t *p)
{
  mod->flags = 0; // the draw widget requests this for every vertex
  return denox_read_source_nclk(mod, mapped, p);
}

// empty mask, blending leaves the image unchanged
static void
create_nodes_empty(dt_graph_t *graph, dt_module_t *module)
{
  const dt_roi_t *roi = &module->connector[0].roi;
  const int id = dt_node_add(
      graph, module, "select", "zero", roi->wd, roi->ht, 1, 0, 0, 2,
      "input",  "read",  "rgba", "*",   dt_no_roi,
      "output", "write", "y",    "f16", roi);
  dt_connector_copy(graph, module, 0, id, 0);
  dt_connector_copy(graph, module, 1, id, 1);
}

static int
has_box(const float *box)
{
  return box[2] > box[0] && box[3] > box[1];
}

// one run per click, the first one sees the box alone
static int
num_runs(int clicks, const float *box)
{
  return CLAMP(clicks + has_box(box), 1, NET_RUNS);
}

dt_graph_run_t
check_params(dt_module_t *module, uint32_t parid, uint32_t num, void *oldval)
{
  const float *clk = dt_module_param_float(module, 0), *box = dt_module_param_float(module, 1);
  const int runs = num_runs(clk[0], box);
  const dt_graph_run_t nodes = s_graph_run_create_nodes | s_graph_run_alloc | s_graph_run_record_cmd_buf;
  if(parid == 0 && num_runs(((float *)oldval)[0], box) != runs) return nodes;
  if(parid == 1 && num_runs(clk[0], (float *)oldval) != runs) return nodes;
  if(parid == 4) return nodes; // zoom
  return s_graph_run_record_cmd_buf;
}

// rgb, box and strokes on the network grid, for the region in the crop buffer
static int
create_nodes_input(dt_graph_t *graph, dt_module_t *module, int crop)
{
  const dt_roi_t roi = { .wd = NET_WD * NET_HT * 8, .ht = 1 };
  const int pc[] = { NET_WD, NET_HT };
  const int id = dt_node_add(
      graph, module, "select", "input", NET_WD, NET_HT, 1, sizeof(pc), pc, 3,
      "input",  "read",  "rgba", "*",   dt_no_roi,
      "crop",   "read",  "ssbo", "f32", dt_no_roi,
      "output", "write", "ssbo", "f16", &roi);
  dt_connector_copy(graph, module, 0, id, 0);
  dt_node_connect_named(graph, crop, "output", id, "crop");
  return id;
}

// the first few clicks and the mask of the run before on top of the input
static int
create_nodes_prompt(dt_graph_t *graph, dt_module_t *module, int input, int crop, int run, int prev)
{
  const dt_roi_t roi = { .wd = NET_WD * NET_HT * 8, .ht = 1 };
  const int pc[] = { NET_WD, NET_HT, NET_OUT, run, prev };
  const int id = dt_node_add(
      graph, module, "select", "prompt", NET_WD, NET_HT, 1, sizeof(pc), pc, 4,
      "input",  "read",  "ssbo", "f16", dt_no_roi,
      "prev",   "read",  "ssbo", "f16", dt_no_roi,
      "crop",   "read",  "ssbo", "f32", dt_no_roi,
      "output", "write", "ssbo", "f16", &roi);
  dt_node_connect_named(graph, input, "output", id, "input");
  dt_node_connect_named(graph, crop,  "output", id, "crop");
  if(!prev) dt_node_connect_named(graph, input, "output", id, "prev"); // not read
  return id;
}

// the network from the prompt node to connector c of node dst, returns the node writing the logits.
// denox adds a weights node every time, all runs are rewired to the first one
static int
create_nodes_net(dt_graph_t *graph, dt_module_t *module, int prompt, int dst, const char *c, int *weights)
{
  const int w = graph->num_nodes;
  denox_create_nodes_nclk(graph, module, prompt, "output", dst, c);
  if(*weights < 0) *weights = w;
  else for(int n=w+1;n<graph->num_nodes;n++) for(int k=0;k<graph->node[n].num_connectors;k++)
  {
    dt_connector_t *cn = graph->node[n].connector + k;
    if(!dt_connector_input(cn) || cn->connected.i != w) continue;
    const uint64_t offset = cn->ssbo_offset;
    dt_node_connect(graph, *weights, 0, n, k);
    cn->ssbo_offset = offset;
  }
  const dt_token_t ct = dt_token(c);
  for(int k=0;k<graph->node[dst].num_connectors;k++)
    if(graph->node[dst].connector[k].name == ct) return graph->node[dst].connector[k].connected.i;
  return -1;
}

void create_nodes(dt_graph_t *graph, dt_module_t *module)
{
  if(qvk.vendorID != 0x10de) // nvidia
  {
    snprintf(graph->gui_msg_buf, sizeof(graph->gui_msg_buf),
        "select: no network for this gpu vendor (0x%04x), mask is empty", qvk.vendorID);
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
  if(size != DENOX_NCLK_WEIGHTS_SIZE)
  {
    snprintf(graph->gui_msg_buf, sizeof(graph->gui_msg_buf),
        found ? "select: \"%s\" has unexpected size, mask is empty" :
            "select: could not find \"%s\", mask is empty", weights);
    graph->gui_msg = graph->gui_msg_buf;
    return create_nodes_empty(graph, module);
  }

  const float *clk = dt_module_param_float(module, 0);
  const int clicks = CLAMP((int)clk[0], 0, 21);
  const float *box = dt_module_param_float(module, 1);
  const int runs = num_runs(clicks, box);
  const int zoom = dt_module_param_int(module, 4)[0];
  const dt_roi_t roi = module->connector[0].roi, crop_roi = { .wd = 12, .ht = 1 };

  // region the network sees, then the chain of runs, each one fed the mask of the one before
  const int crop = dt_node_add(
      graph, module, "select", "crop", 1, 1, 1, 0, 0, 1,
      "output", "write", "ssbo", "f32", &crop_roi);
  const int input = create_nodes_input(graph, module, crop);
  int weights = -1;
  int prompt = create_nodes_prompt(graph, module, input, crop, 0, 0);
  for(int r=1;r<runs;r++)
  {
    const int next = create_nodes_prompt(graph, module, input, crop, r, 1);
    create_nodes_net(graph, module, prompt, next, "prev", &weights);
    prompt = next;
  }

  // logits to a mask at input resolution
  const int pc_out[] = { NET_OUT };
  const int output = dt_node_add(
      graph, module, "select", "output", roi.wd, roi.ht, 1, sizeof(pc_out), pc_out, 4,
      "input",  "read",  "ssbo", "f16", dt_no_roi,
      "zoom",   "read",  "ssbo", "f16", dt_no_roi,
      "crop",   "read",  "ssbo", "f32", dt_no_roi,
      "output", "write", "y",    "f16", &roi);
  const int logits = create_nodes_net(graph, module, prompt, output, "input", &weights);
  dt_connector_copy(graph, module, 1, output, 3);
  if(!zoom)
  {
    dt_node_connect_named(graph, logits, "a", output, "zoom");
    dt_node_connect_named(graph, crop, "output", output, "crop");
    return;
  }

  // run once more with all clicks on a region around the mask, if it is small
  const int pc_zoom[] = { roi.wd, roi.ht, NET_OUT };
  const int zoom_crop = dt_node_add(
      graph, module, "select", "zoom", DT_LOCAL_SIZE_X, DT_LOCAL_SIZE_Y, 1, sizeof(pc_zoom), pc_zoom, 3,
      "input",  "read",  "ssbo", "f16", dt_no_roi,
      "crop",   "read",  "ssbo", "f32", dt_no_roi,
      "output", "write", "ssbo", "f32", &crop_roi);
  dt_node_connect_named(graph, logits, "a", zoom_crop, "input");
  dt_node_connect_named(graph, crop, "output", zoom_crop, "crop");
  const int zinput  = create_nodes_input(graph, module, zoom_crop);
  const int zprompt = create_nodes_prompt(graph, module, zinput, zoom_crop, NET_RUNS, 1);
  dt_node_connect_named(graph, logits, "a", zprompt, "prev");
  create_nodes_net(graph, module, zprompt, output, "zoom", &weights);
  dt_node_connect_named(graph, zoom_crop, "output", output, "crop");
}
