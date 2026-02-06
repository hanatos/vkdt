#include "modules/api.h"
#include <math.h>

void
modify_roi_out(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  module->connector[2].roi.full_wd = 512;
  module->connector[2].roi.full_ht = 256;
  module->connector[3].roi.full_wd =
  module->connector[3].roi.full_ht = 1;
  module->connector[4].roi = module->connector[0].roi;
}

void
modify_roi_in(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  module->connector[0].roi.wd = module->connector[0].roi.full_wd;
  module->connector[0].roi.ht = module->connector[0].roi.full_ht;
  module->connector[0].roi.scale = 1;
  module->connector[1].roi.wd = module->connector[1].roi.full_wd;
  module->connector[1].roi.ht = module->connector[1].roi.full_ht;
  module->connector[1].roi.scale = 1;
}

void
create_nodes(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  // one node to collect, and one to read back the sink
  dt_roi_t rbuf = {
    .wd = ((module->connector[0].roi.wd + DT_LOCAL_SIZE_X - 1)/DT_LOCAL_SIZE_X) * ((module->connector[0].roi.ht + DT_LOCAL_SIZE_Y - 1)/DT_LOCAL_SIZE_Y),
    .ht = 2,
  };
  const int id_mse = dt_node_add(graph, module, "loss", "main",
      module->connector[0].roi.wd, module->connector[0].roi.ht, 1, 0, 0, 4,
      "input", "read", "*", "*", dt_no_roi,
      "orig",  "read", "*", "*", dt_no_roi,
      "loss",  "write", "ssbo", "f32", &rbuf,
      "err",   "write", "rgba", "f16", &module->connector[0].roi);
  dt_connector_copy(graph, module, 0, id_mse, 0);
  dt_connector_copy(graph, module, 1, id_mse, 1);
  dt_connector_copy(graph, module, 4, id_mse, 3);

  // remember mse as entry point
  int node = id_mse;
  int conn = 2;

  const int sz = 64;
  while(rbuf.wd > 1)
  {
    const int cwd = (rbuf.wd + sz - 1)/sz;
    const int pc[] = { rbuf.wd };
    const int wd = cwd * DT_LOCAL_SIZE_X; // cwd is number of work groups.
    rbuf.wd = cwd;
    const int id_down = dt_node_add(graph, module, "loss", "down", wd, 1, 1, sizeof(pc), pc, 2,
        "input",  "read",  "ssbo", "f32", dt_no_roi,
        "output", "write", "ssbo", "f32", &rbuf);
    CONN(dt_node_connect(graph, node, conn, id_down, 0));
    node = id_down;
    conn = 1;
  }

  dt_roi_t rtime = (dt_roi_t){.wd = 256, .ht = 1};
  dt_roi_t tiny  = (dt_roi_t){.wd = 1,   .ht = 1};
  const int pc[] = { 256 };
  const int id_dspy = dt_node_add(graph, module, "loss", "map",
      MAX(1,module->connector[2].roi.wd), MAX(1,module->connector[2].roi.ht), 1, sizeof(pc), pc, 4,
      "time", "write", "ssbo", "f32", &rtime,
      "loss", "read",  "ssbo", "f32", dt_no_roi,
      "tiny", "write", "y",    "f32", &tiny,
      "dspy", "write", "rgba", "f16", &module->connector[2].roi);
  dt_connector_copy(graph, module, 2, id_dspy, 3);
  CONN(dt_node_connect(graph, node, conn, id_dspy, 1));

  const int id_sink = dt_node_add(graph, module, "loss", "sink", 1, 1, 1, 0, 0, 1,
      "input", "sink", "*", "f32", dt_no_roi);
  CONN(dt_node_connect_named(graph, id_dspy, "tiny", id_sink, "input")); // stupid dance via image because we don't have the buffer in host visible memory
  dt_connector_copy(graph, module, 3, id_dspy, 2);
  graph->node[id_dspy].connector[0].flags |= s_conn_protected; // protect memory, will update the timeline
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
    dt_module_t            *module,
    void                   *buf,
    dt_write_sink_params_t *p)
{
  float *loss = 0;
  for(int p=0;p<module->so->num_params;p++)
    if(module->so->param[p]->name == dt_token("loss"))
      loss = (float *)(module->param + module->so->param[p]->offset);
  if(!loss) return;

  float *b = buf;
  loss[0] = b[0];
}
