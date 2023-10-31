#include "modules/api.h"
#include "core/half.h"
#include <math.h>

void modify_roi_in(
    dt_graph_t *graph,
    dt_module_t *module)
{
  dt_roi_t *r = &module->connector[0].roi;
  r->wd = r->full_wd;
  r->ht = r->full_ht;
  r->scale = 1.0f;
  r = &module->connector[1].roi;
  r->wd = r->full_wd;
  r->ht = r->full_ht;
  r->scale = 1.0f;
  module->connector[2].roi.wd = module->connector[2].roi.full_wd;
  module->connector[2].roi.ht = module->connector[2].roi.full_ht;
  module->connector[2].roi.scale = 1.0f;
}

void modify_roi_out(
    dt_graph_t *graph,
    dt_module_t *module)
{
  const int sz = 2;
  module->connector[2].roi.full_wd = (module->connector[0].roi.full_wd+sz-1)/sz;
  module->connector[2].roi.full_ht = (module->connector[0].roi.full_ht+sz-1)/sz;
}

void
create_nodes(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  // one node to collect, and one to read back the sink
  dt_roi_t rbuf = (dt_roi_t){.wd = (module->connector[0].roi.wd * module->connector[0].roi.ht+63) / 64, .ht = 1 };
  const int id_mse = dt_node_add(graph, module, "loss", "main",
      module->connector[0].roi.wd, module->connector[1].roi.wd, 1, 0, 0, 3,
      "input", "read", "*", "*", -1ul,
      "orig",  "read", "*", "*", -1ul,
      "loss",  "write", "ssbo", "f32", &rbuf);

  dt_connector_copy(graph, module, 0, id_mse, 0);
  dt_connector_copy(graph, module, 1, id_mse, 1);

  // remember mse as entry point
  int node = id_mse;
  int conn = 2;

  const int sz = 64;
  while(rbuf.wd > 1)
  {
    const int cwd = (rbuf.wd + sz - 1)/sz;
    const int pc[] = { rbuf.wd };
    const int wd = cwd * DT_LOCAL_SIZE_X;
    rbuf.wd = cwd;
    const int id_down = dt_node_add(graph, module, "loss", "down", wd, 1, 1, sizeof(pc), pc, 2,
        "input",  "read",  "ssbo", "f32", -1ul,
        "output", "write", "ssbo", "f32", &rbuf);
    CONN(dt_node_connect(graph, node, conn, id_down, 0));
    node = id_down;
    conn = 1;
  }

  const int id_sink = dt_node_add(graph, module, "loss", "sink", 1, 1, 1, 0, 0, 1,
      "input", "sink", "ssbo", "f32", -1ul);
  CONN(dt_node_connect(graph, node, conn, id_sink, 0));
  module->flags |= s_module_request_write_sink;
}

dt_graph_run_t
check_params(
    dt_module_t *module,
    uint32_t     parid,
    uint32_t     num,
    void        *oldval)
{
  module->flags |= s_module_request_write_sink;
  return s_graph_run_record_cmd_buf; // minimal parameter upload to uniforms
}

// called after pipeline finished up to here.
// our input buffer will come in memory mapped.
void write_sink(
    dt_module_t *module,
    void *buf)
{
  float *loss = 0;
  for(int p=0;p<module->so->num_params;p++)
    if(module->so->param[p]->name == dt_token("loss"))
      loss = (float *)(module->param + module->so->param[p]->offset);
  if(!loss) return;

  float *b = buf;
  loss[0] = b[0];
}
