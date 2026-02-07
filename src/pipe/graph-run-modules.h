#pragma once
#include "core/fs.h"
#include "db/hash.h"

static inline VkResult
dt_graph_run_modules_upload_uniforms(
    dt_graph_t          *graph,
    const dt_graph_run_t run)
{ // module traversal
  dt_module_t *const arr = graph->module;
  const int arr_cnt = graph->num_modules;
  uint8_t *uniform_mem = 0;
  QVKR(vkMapMemory(qvk.device, graph->vkmem_uniform, ((uint64_t)graph->double_buffer) * graph->uniform_size,
        graph->uniform_size, 0, (void**)&uniform_mem));
  ((uint32_t *)uniform_mem)[0] = graph->frame;
  ((uint32_t *)uniform_mem)[1] = graph->frame_cnt;
  ((uint32_t *)uniform_mem)[2] = graph->main_img_hash;
  ((uint32_t *)uniform_mem)[3] = 0; // pad
  memcpy(((uint32_t*)uniform_mem)+4, graph->gui_colour, sizeof(float)*8);
#define TRAVERSE_POST \
  dt_module_t *mod = arr+curr;\
  if(mod->so->commit_params)\
    mod->so->commit_params(graph, mod);\
  if(mod->committed_param_size)\
    memcpy(uniform_mem + mod->uniform_offset, mod->committed_param, mod->committed_param_size);\
  else if(mod->param_size)\
    memcpy(uniform_mem + mod->uniform_offset, mod->param, mod->param_size);
#include "graph-traverse.inc"
  vkUnmapMemory(qvk.device, graph->vkmem_uniform);
  return VK_SUCCESS;
}

// default callback for create nodes: pretty much copy the module.
// does no vulkan work, just graph connections. shall not fail.
static inline void
create_nodes(dt_graph_t *graph, dt_module_t *module, uint64_t *uniform_offset)
{
  for(int i=0;i<module->num_connectors;i++)
    module->connector[i].bypass = s_cid_unset;
  const uint64_t u_offset = *uniform_offset;
  uint64_t u_size = module->committed_param_size ?
    module->committed_param_size :
    module->param_size;
  u_size = (u_size + qvk.uniform_alignment-1) & -qvk.uniform_alignment;
  *uniform_offset += u_size;
  // TODO: if roi size/scale does not match, insert resample node!
  // TODO: where? inside create_nodes? or we fix it afterwards?
  if(module->disabled)
  {
    int mc_in = -1, mc_out = -1;
    for(int i=0;i<module->num_connectors;i++)
    {
      if(dt_connector_output(module->connector+i) && module->connector[i].name == dt_token("output"))
        mc_out = i;
      if(dt_connector_input(module->connector+i) && module->connector[i].name == dt_token("input"))
        mc_in = i;
    }
    // TODO error handling?
    dt_connector_bypass(graph, module, mc_in, mc_out);
  }
  else if(module->so->create_nodes)
  {
    module->so->create_nodes(graph, module);
  }
  else
  {
    assert(graph->num_nodes < graph->max_nodes);
    const int nodeid = graph->num_nodes++;
    dt_node_t *node = graph->node + nodeid;

    *node = (dt_node_t) {
      .name           = module->name,
      .kernel         = dt_token("main"), // file name
      .num_connectors = module->num_connectors,
      .module         = module,
      .flags          = module->flags,    // propagate sink/source copy requests
      // make sure we don't follow garbage pointers. pure sink or source nodes
      // don't run a pipeline. and hence have no descriptor sets to construct. they
      // do, however, have input or output images allocated for them.
      .pipeline       = 0,
      .dset_layout    = 0,
    };

    // compute shader or graphics pipeline?
    char filename[PATH_MAX+100] = {0};
    snprintf(filename, sizeof(filename), "modules/%"PRItkn"/main.vert.spv",
        dt_token_str(module->name));
    FILE *test = dt_graph_open_resource(0, 0, filename, "rb");
    if(!test) node->type = s_node_compute;
    else node->type = s_node_graphics;
    if(test) fclose(test);

    // determine kernel dimensions:
    int output = dt_module_get_connector(module, dt_token("output"));
    // output == -1 means we must be a sink, anyways there won't be any more glsl
    // to be run here!
    if(output >= 0)
    {
      dt_roi_t *roi = &module->connector[output].roi;
      node->wd = roi->wd;
      node->ht = roi->ht;
      node->dp = 1;
    }

    for(int i=0;i<module->num_connectors;i++)
      dt_connector_copy(graph, module, i, nodeid, i);
  }
  module->uniform_offset = u_offset;
  module->uniform_size   = u_size;
}

static inline void
init_display_flags(dt_graph_t *graph, dt_module_t *module)
{
  if(module->name == dt_token("display"))
  { // if this is a display module, walk our input connector and make the connection double buffered
    dt_connector_t *c = module->connector; // display has connector 0 as the input
    if(!dt_cid_unset(c->connected))
    {
      int extra_flag = s_conn_double_buffer;
      if(qvk.blit_supported && module->inst == dt_token("main"))
        extra_flag |= s_conn_mipmap;
      c->flags |= extra_flag;

      // find node corresponding to our display module
      int ni = c->associated.i;
      int nc = c->associated.c;
      c = graph->node[ni].connector + nc;
      int ni1 = c->connected.i;
      int nc1 = c->connected.c;
      graph->node[ni1].connector[nc1].flags |= extra_flag;
      c->frames = graph->node[ni1].connector[nc1].frames = 2;
      // also every input connected to the output we're referring to here needs to be updated!
      // this is different from feedback connectors who can happily access both buffers at their dispatch stage.
      // since we're adding the flag so late, it's our responsibility to propagate it. on the bright side
      // the graph topology is fixed now, so we can safely do this:
      for(int n=0;n<graph->num_nodes;n++)
      {
        for(int i=0;i<graph->node[n].num_connectors;i++)
        {
          if(dt_connector_input(graph->node[n].connector+i) &&
              graph->node[n].connector[i].connected.i == ni1 &&
              graph->node[n].connector[i].connected.c == nc1)
          {
            graph->node[n].connector[i].flags |= extra_flag;
            graph->node[n].connector[i].frames = 2;
          }
        }
      }
    }
  }
}

static inline void
init_connector_images(dt_graph_t *graph)
{ // now init frame count, repoint connection image storage
  for(int n=0;n<graph->num_nodes;n++)
  {
    for(int i=0;i<graph->node[n].num_connectors;i++)
    {
      if((graph->node[n].flags & s_module_request_read_source) &&
         (graph->node[n].connector[i].type == dt_token("source")))
        graph->node[n].connector[i].frames = 2;
      if(graph->node[n].connector[i].frames == 0)
      {
        if(graph->node[n].connector[i].flags & s_conn_feedback)
          graph->node[n].connector[i].frames = 2;
        else
          graph->node[n].connector[i].frames = 1;
      }
      if(dt_connector_owner(graph->node[n].connector+i))
      { // only allocate memory for output connectors that own their buffer
        graph->node[n].conn_image[i] = graph->conn_image_end;
        graph->conn_image_end += MAX(1,graph->node[n].connector[i].array_length)
          * graph->node[n].connector[i].frames;
        assert(graph->conn_image_end <= graph->conn_image_max);
      }
      else graph->node[n].conn_image[i] = -1;
    }
  }
}

// propagate full buffer size from source to sink
static inline void
modify_roi_out(dt_graph_t *graph, dt_module_t *module)
{
  // =========================================================
  // manage img_param metadata propagated along with the graph
  int input = dt_module_get_connector(module, dt_token("input"));
  dt_connector_t *c = 0;
  if(input >= 0 &&
     graph->module[module->connector[input].connected.i].img_param.input_name != dt_token("main"))
  for(int i=0;i<module->num_connectors;i++)
  { // the default main input is wherever "main" came in,
    // but preferrably named "input"
    if(!dt_connector_input(module->connector+i)) continue;
    int mid = module->connector[i].connected.i;
    if(mid < 0) continue;
    if(graph->module[mid].img_param.input_name == dt_token("main"))
      input = i;
  }
  // main input modules have to init their params completely. we will not zero it.
  // this is because they run a modify_roi_out prepass to determine the main input
  // dimensions/types and we would destroy it here the second time around.
  if(module->inst != dt_token("main") || strncmp(dt_token_str(module->name), "i-", 2))
    module->img_param = (dt_image_params_t){{0}};
  if(input >= 0)
  { // first copy image metadata if we have a unique "input" connector
    c = module->connector + input;
    if(c->connected.i != -1)
      module->img_param = graph->module[c->connected.i].img_param;
  }
  // =========================================================
  // now handle &input style channel and format references
  // these are copied before the roi_out callback because this often
  // allocates resources based on input formats.
  for(int i=0;i<module->num_connectors;i++)
  {
    dt_connector_t *c = module->connector+i;
    if(dt_token_str(c->chan)[0] == '&')
    {
      dt_token_t tmp = c->chan >> 8;
      for(int j=0;j<module->num_connectors;j++)
      {
        if(module->connector[j].name == tmp)
        {
          c->chan = module->connector[j].chan;
          break;
        }
      }
    }
  }
  // =========================================================
  // copy extents of module inputs from the connected output connectors
  for(int i=0;i<module->num_connectors;i++)
  { // keep incoming roi in sync:
    dt_connector_t *c = module->connector+i;
    if(dt_connector_input(c) && c->connected.i >= 0 && c->connected.c >= 0)
      c->roi = graph->module[c->connected.i].connector[c->connected.c].roi;
  }
  // =========================================================
  // execute callback if present, or run default
  if(!module->disabled && module->so->modify_roi_out)
  {
    module->so->modify_roi_out(graph, module);
    // mark roi in of all outputs as uninitialised:
    for(int i=0;i<module->num_connectors;i++)
      if(dt_connector_owner(module->connector+i))
        module->connector[i].roi.scale = -1.0f;

    // if this is the main input of the graph, remember the img_param
    // globally, so others can pick up maker/model:
    if(module->inst == dt_token("main") && dt_connector_owner(module->connector))
    {
      graph->main_img_param = module->img_param;
      const char *str = dt_module_param_string(module, dt_module_get_param(module->so, dt_token("filename")));
      graph->main_img_hash = str ? (uint32_t)hash64(str) : 0;
    }
    // remember source name to pass on further
    if(module->connector[0].type == dt_token("source"))
      module->img_param.input_name = module->inst;
  }
  else
  { // default implementation:
    // mark roi in of all outputs as uninitialised:
    for(int i=0;i<module->num_connectors;i++)
      if(dt_connector_owner(module->connector+i))
        module->connector[i].roi.scale = -1.0f;
    // remember instance name if [0] is a source connector
    if(module->connector[0].type == dt_token("source"))
      module->img_param.input_name = module->inst;
    // copy over roi from connector named "input" to all outputs ("write")
    dt_roi_t roi = {0};
    if(input < 0)
    {
      // minimal default output size (for generator nodes with no callback)
      // might warn about this.
      roi.full_wd = 1024;
      roi.full_ht = 1024;
    }
    else
    {
      if(c->connected.i != -1)
      {
        roi = graph->module[c->connected.i].connector[c->connected.c].roi;
        c->roi = roi; // also keep incoming roi in sync
      }
    }
    for(int i=0;i<module->num_connectors;i++)
    {
      if(module->connector[i].type == dt_token("write"))
      {
        module->connector[i].roi.full_wd = roi.full_wd;
        module->connector[i].roi.full_ht = roi.full_ht;
      }
    }
  }
  if(module->name == dt_token("bvh")) module->flags |= s_module_request_build_bvh;
  // this does not work if the *node* has a bvh name. in this case create_nodes has to take care of the correct flags manually.
}

static inline void
propagate_roi_in(dt_graph_t *graph, dt_module_t *module)
{
  // after running modify_roi_out, all owners/allocators know their full_wd/full_ht
  // after running modify_roi_in, the size request of the displays has been propagated back through
  // all modules who actually care for it. some modules, however, have inputs such as luts, colour pickers etc
  // that are not really connected to the output size of the module. these are marked as "don't care" during
  // modify_roi_in and we will init them now, simply copying whatever the connector says.
  // we can't do that inline in modify_roi_in, since the whole chain might go uninitialised.
  // we only propagate *forward* here.
  for(int i=0;i<module->num_connectors;i++)
  {
    dt_connector_t *c = module->connector+i;
    if(dt_connector_input(c))
    { // make sure roi is good on the outgoing connector
      if(!dt_cid_unset(c->connected))
      {
        dt_roi_t *roi = &graph->module[c->connected.i].connector[c->connected.c].roi;
        if(roi->wd == 0)
        { // uninited, set to something:
          if(roi->full_wd == 0) roi->full_wd = 2;
          if(roi->full_ht == 0) roi->full_ht = 2;
          roi->wd = roi->full_wd;
          roi->ht = roi->full_ht;
          roi->scale = 1.0f;
        }

        if(graph->module[c->connected.i].connector[c->connected.c].type == dt_token("source"))
        { // sources don't negotiate their size, they just give what they have
          roi->wd = roi->full_wd;
          roi->ht = roi->full_ht;
          if(roi->scale <= 0) roi->scale = 1.0;
          c->roi = *roi;
        }
        else if(c->roi.scale < 0.0f && roi->scale > 0.0f)
          c->roi = *roi;
        // make sure we use the same array size as the data source. this is when the array_length depends on roi_out
        c->array_length = graph->module[c->connected.i].connector[c->connected.c].array_length;
        // now the output is sure about the exact type of data it wants to allocate. so we
        // propagate that the other direction output->our input
        c->format = graph->module[c->connected.i].connector[c->connected.c].format;
        c->chan   = graph->module[c->connected.i].connector[c->connected.c].chan;
      }
    }
  }
  for(int i=0;i<module->num_connectors;i++)
  { // grab from inputs
    dt_connector_t *c = module->connector+i;
    if(dt_connector_input(c) && !dt_cid_unset(c->connected))
      if(c->roi.scale < 0.0f) // copy over only if not inited?
        c->roi = graph->module[c->connected.i].connector[c->connected.c].roi;
  }

  if(!module->disabled && module->so->modify_roi_out)
  { // have custom callback
    dt_module_t tmp = *module;
    for(int i=0;i<module->num_connectors;i++)
    {
      tmp.connector[i].roi.full_wd = tmp.connector[i].roi.wd;
      tmp.connector[i].roi.full_ht = tmp.connector[i].roi.ht;
    }
    module->so->modify_roi_out(graph, &tmp);
    for(int i=0;i<module->num_connectors;i++)
    {
      if(dt_connector_owner(module->connector+i))
      {
        module->connector[i].roi.wd = tmp.connector[i].roi.full_wd;
        module->connector[i].roi.ht = tmp.connector[i].roi.full_ht;
      }
    }
  }
  else
  { // copy over roi from connector named "input" to all outputs ("write")
    int input = dt_module_get_connector(module, dt_token("input"));
    dt_roi_t roi = {0};
    if(input < 0)
    {
      roi.wd = roi.full_wd;
      roi.ht = roi.full_ht;
    }
    else roi = module->connector[input].roi;
    for(int i=0;i<module->num_connectors;i++)
    {
      if(module->connector[i].type == dt_token("write"))
      {
        module->connector[i].roi.wd = roi.wd;
        module->connector[i].roi.ht = roi.ht;
      }
    }
  }
}

// request input region of interest from sink to source
static inline void
modify_roi_in(dt_graph_t *graph, dt_module_t *module)
{
  if(!module->disabled && module->so->modify_roi_in)
  {
    module->so->modify_roi_in(graph, module);
  }
  else
  { // propagate roi request on output module to our inputs ("read")
    int output = dt_module_get_connector(module, dt_token("output"));
    if(output == -1 && module->connector[0].type == dt_token("sink"))
    { // by default ask for it all:
      output = 0;
      dt_roi_t *r = &module->connector[0].roi;
      r->scale = 1.0f;
      // this is the performance/LOD switch for faster processing
      // on low end computers. needs to be wired somehow in gui/config.
      // scale to fit into requested roi
      float max_wd = module->connector[0].max_wd, max_ht = module->connector[0].max_ht;
      float scalex = max_wd > 0 ? r->full_wd / (float) max_wd : 1.0f;
      float scaley = max_ht > 0 ? r->full_ht / (float) max_ht : 1.0f;
      r->scale = MAX(scalex, scaley);
      r->wd = (r->full_wd/r->scale + 0.5f);
      r->ht = (r->full_ht/r->scale + 0.5f);
    }
    if(output < 0)
    {
      dt_log(s_log_pipe|s_log_err, "default modify_roi_in: input roi on %"PRItkn"_%"PRItkn" uninitialised!",
          dt_token_str(module->name), dt_token_str(module->inst));
      return;
    }
    dt_roi_t *roi = &module->connector[output].roi;

    // all input connectors get the same roi as our output.
    for(int i=0;i<module->num_connectors;i++)
    {
      dt_connector_t *c = module->connector+i;
      if(dt_connector_input(c)) c->roi = *roi;
    }
  }

  // in any case copy over to output roi of connected modules:
  for(int i=0;i<module->num_connectors;i++)
  {
    dt_connector_t *c = module->connector+i;
    if(dt_connector_input(c))
    {
      // make sure roi is good on the outgoing connector
      if(!dt_cid_unset(c->connected))
      {
        dt_roi_t *roi = &graph->module[c->connected.i].connector[c->connected.c].roi;
        if(graph->module[c->connected.i].connector[c->connected.c].type == dt_token("source"))
        { // sources don't negotiate their size, they just give what they have
          roi->wd = roi->full_wd;
          roi->ht = roi->full_ht;
          if(roi->scale <= 0) roi->scale = 1.0; // mark as initialised, we force the resolution now
          c->roi = *roi;
        }
        else if(c->roi.scale < 0.0f) {} // this says it doesn't want to propagate
        // if roi->scale > 0 it has been inited before and we're late to the party!
        // in this case, reverse the process:
        else if(roi->scale > 0.0f)
          c->roi = *roi;
        else
          *roi = c->roi;
        // propagate flags:
        graph->module[c->connected.i].connector[c->connected.c].flags |= c->flags;
        // make sure we use the same array size as the data source. this is when the array_length depends on roi_out
        c->array_length = graph->module[c->connected.i].connector[c->connected.c].array_length;
        // now the output is sure about the exact type of data it wants to allocate. so we
        // propagate that the other direction output->our input
        c->format = graph->module[c->connected.i].connector[c->connected.c].format;
        c->chan   = graph->module[c->connected.i].connector[c->connected.c].chan;
      }
    }
  }
}

// run the graph.
// this is the module layer portion of it, factored out of graph.c for improved readability.
static inline VkResult
dt_graph_run_modules(
    dt_graph_t        *graph,
    dt_graph_run_t    *run,
    uint32_t          *modid,
    dt_module_flags_t *module_flags)
{
  int cnt = 0;
  dt_module_t *const arr = graph->module;
  const int arr_cnt = graph->num_modules;
#define TRAVERSE_POST \
  modid[cnt++] = curr;
#include "graph-traverse.inc"

  // run animation callbacks, if any. these come first, so that commit_params
  // can upload the current state that is actually uploaded/processed through
  // the graph. also animations might trigger module flags to upload new
  // things/rebuild all (quake level change)
  for(int i=0;i<cnt;i++)
    if(graph->module[modid[i]].so->animate)
      graph->module[modid[i]].so->animate(graph, graph->module+modid[i]);

  int main_input_module = -1;
  // find extra module flags
  for(int i=0;i<cnt;i++)
  {
    if(!strncmp(dt_token_str(graph->module[modid[i]].name), "i-", 2) &&
       (main_input_module == -1 || graph->module[modid[i]].inst == dt_token("main")))
      main_input_module = modid[i];
    *module_flags |= graph->module[modid[i]].flags;
  }

  // at least one module requested a full rebuild:
  if(*module_flags & s_module_request_all) *run |= s_graph_run_all;

  // if synchronous upload/download is required, we can't interleave frames:
  if((*run & (s_graph_run_upload_source | s_graph_run_download_sink)) ||
     (*module_flags & (s_module_request_read_source | s_module_request_write_sink)))
    *run |= s_graph_run_wait_done;

  // XXX currenty we only run animations/interleaved from gui, so this kinda catches
  // XXX the anim case too. or else wait for fp fence above, too, in case we're allocing!
  // only waiting for the gui thread to draw our output, and only
  // if we intend to clean it up behind their back
  if(graph->gui_attached &&
    (*run & (s_graph_run_roi | s_graph_run_alloc | s_graph_run_create_nodes)))
    QVKL(&qvk.queue[qvk.qid[graph->queue_name]].mutex, vkQueueWaitIdle(qvk.queue[qvk.qid[graph->queue_name]].queue)); // probably enough to wait on gui queue fence

  if(*run & s_graph_run_alloc)
  {
    vkDestroyDescriptorSetLayout(qvk.device, graph->uniform_dset_layout, 0);
    graph->uniform_dset_layout = 0;
    if(!graph->uniform_dset_layout)
    { // init layout of uniform descriptor set:
      VkDescriptorSetLayoutBinding bindings[] = {{
        .binding = 0, // global uniform, frame number etc
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_ALL,
      },{
        .binding = 1, // module local uniform, params struct
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_ALL,
      }};
      VkDescriptorSetLayoutCreateInfo dset_layout_info = {
        .flags        = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
        .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 2,
        .pBindings    = bindings,
      };
      QVKR(vkCreateDescriptorSetLayout(qvk.device, &dset_layout_info, 0, &graph->uniform_dset_layout));
    }
  }

  // ==============================================
  // first pass: find output rois
  // ==============================================
  // execute after all inputs have been traversed:
  // "int curr" will be the current node
  // walk all inputs and determine roi on all outputs
  if(*run & s_graph_run_roi)
  {
    if(main_input_module >= 0) // may set metadata required by others (such as find the right lut)
      modify_roi_out(graph, graph->module + main_input_module);
    for(int i=0;i<cnt;i++)
      modify_roi_out(graph, graph->module + modid[i]);
    for(int i=0;i<cnt;i++) // potentially init remaining feedback rois:
      if(graph->module[modid[i]].connector[0].roi.full_wd == 0)
        modify_roi_out(graph, graph->module + modid[i]);
  }

  // we want to make sure the last output/display is initialised.
  // if the roi out is 0, probably reading some sources went wrong and we need
  // to abort right here!
  for(int i=cnt-1;i>=0;i--)
  {
    if(graph->module[modid[i]].connector[0].type == dt_token("sink"))
    {
      if(graph->module[modid[i]].connector[0].roi.full_wd == 0)
      {
        dt_log(s_log_pipe|s_log_err, "module %"PRItkn" %"PRItkn" connector %"PRItkn" has uninited size!",
            dt_token_str(graph->module[modid[i]].name),
            dt_token_str(graph->module[modid[i]].inst),
            dt_token_str(graph->module[modid[i]].connector[0].name));
        return VK_INCOMPLETE;
      }
      break; // we're good
    }
  }


  // now we don't always want the full size buffer but are interested in a
  // scaled or cropped sub-region. actually this step is performed
  // transparently in the sink module's modify_roi_in first thing in the
  // second pass.

  // ==============================================
  // 2nd pass: request input rois
  // and create nodes for all modules
  // ==============================================
  if(*run & (s_graph_run_roi | s_graph_run_create_nodes))
  { // delete all previous nodes
    for(int i=0;i<graph->num_nodes;i++) for(int j=0;j<graph->node[i].num_connectors;j++)
      if(graph->node[i].connector[j].flags & s_conn_dynamic_array)
      { // free potential double images for multi-frame dsets
        for(int k=0;k<MAX(1,graph->node[i].connector[j].array_length);k++)
        { // avoid error message in case image does not exist:
          dt_connector_image_t *img0 = (graph->node[i].conn_image[j] != -1) ? dt_graph_connector_image(graph, i, j, k, 0) : 0;
          dt_connector_image_t *img1 = (graph->node[i].conn_image[j] != -1) ? dt_graph_connector_image(graph, i, j, k, 1) : 0;
          if(!img0 || !img1) continue;
          if(img0->buffer == img1->buffer)
          { // it's enough to clean up one replicant, the rest will shut down cleanly later:
            *img0 = (dt_connector_image_t){0};
          }
        }
      }
    const uint64_t wait_value[] = {
      MAX(graph->display_dbuffer[0], graph->display_dbuffer[1]),
      MAX(graph->process_dbuffer[0], graph->process_dbuffer[1]),
    };
    VkSemaphore sem[] = {
      graph->semaphore_display,
      graph->semaphore_process,
    };
    VkSemaphoreWaitInfo wait_info = {
      .sType          = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
      .semaphoreCount = 2,
      .pSemaphores    = sem,
      .pValues        = wait_value,
    };
    QVKR(vkWaitSemaphores(qvk.device, &wait_info, UINT64_MAX));
    for(int i=0;i<graph->conn_image_end;i++)
    {
      if(graph->conn_image_pool[i].buffer)     vkDestroyBuffer(qvk.device,    graph->conn_image_pool[i].buffer, VK_NULL_HANDLE);
      if(graph->conn_image_pool[i].image)      vkDestroyImage(qvk.device,     graph->conn_image_pool[i].image, VK_NULL_HANDLE);
      if(graph->conn_image_pool[i].image_view) vkDestroyImageView(qvk.device, graph->conn_image_pool[i].image_view, VK_NULL_HANDLE);
      graph->conn_image_pool[i].buffer = 0;
      graph->conn_image_pool[i].image = 0;
      graph->conn_image_pool[i].image_view = 0;
    }
    graph->conn_image_end = 0;
    for(int i=0;i<cnt;i++)
      for(int j=0;j<graph->module[modid[i]].num_connectors;j++)
        graph->module[modid[i]].connector[j].associated = s_cid_unset;
    for(int i=0;i<graph->num_nodes;i++)
    {
      for(int j=0;j<graph->node[i].num_connectors;j++)
      {
        dt_connector_t *c = graph->node[i].connector+j;
        c->associated = s_cid_unset;
        if(c->staging[0]) vkDestroyBuffer(qvk.device, c->staging[0], VK_NULL_HANDLE);
        if(c->staging[1]) vkDestroyBuffer(qvk.device, c->staging[1], VK_NULL_HANDLE);
        c->staging[0] = c->staging[1] = 0;
      }
      vkDestroyPipelineLayout     (qvk.device, graph->node[i].pipeline_layout,  0);
      vkDestroyPipeline           (qvk.device, graph->node[i].pipeline,         0);
      vkDestroyDescriptorSetLayout(qvk.device, graph->node[i].dset_layout,      0);
      vkDestroyFramebuffer        (qvk.device, graph->node[i].draw_framebuffer, 0);
      vkDestroyRenderPass         (qvk.device, graph->node[i].draw_render_pass, 0);
      graph->node[i].pipeline_layout = 0;
      graph->node[i].pipeline = 0;
      graph->node[i].dset_layout = 0;
      graph->node[i].draw_framebuffer = 0;
      graph->node[i].draw_render_pass = 0;
      dt_raytrace_node_cleanup    (graph->node + i);
    }
    dt_raytrace_graph_cleanup(graph);
    graph->num_nodes = 0;
    // we need three uint32 + 8 floats, alignment is 64 bytes
    // frame, frame_cnt, hash, pad, vec4 col vec4 col
    graph->uniform_global_size = (sizeof(float)*12 + qvk.uniform_alignment) & -qvk.uniform_alignment; // global data, aligned
    uint64_t uniform_offset = graph->uniform_global_size;
    // skip modules with uninited roi! (these are disconnected/dead code elimination cases)
    for(int i=cnt-1;i>=0;i--)
      if(graph->module[modid[i]].connector[0].roi.full_wd > 0)
        modify_roi_in(graph, graph->module+modid[i]);
    for(int i=0;i<cnt;i++) // initialise the input chains that don't actually care about scaling buffers:
      propagate_roi_in(graph, graph->module+modid[i]);
    for(int i=0;i<cnt;i++)
      if(graph->module[modid[i]].connector[0].roi.full_wd > 0)
        create_nodes(graph, graph->module+modid[i], &uniform_offset);
    // make sure connectors are zero inited:
    memset(graph->conn_image_pool, 0, sizeof(dt_connector_image_t)*graph->conn_image_end);
    graph->uniform_size = uniform_offset;

    // these feedback connectors really cause a lot of trouble.
    // we need to initialise the input connectors now after full graph traversal.
    // or else the connection performed during node creation will copy some node id
    // from the connected module that has not yet been initialised.
    // we work around this by storing the *module* id and connector in
    // dt_connector_copy(), and repoint it here.
    for(int ni=0;ni<graph->num_nodes;ni++)
    {
      dt_node_t *n = graph->node + ni;
      for(int i=0;i<n->num_connectors;i++)
      {
        if(!dt_cid_unset(n->connector[i].associated))
        { // needs repointing
          dt_cid_t id;
          dt_cid_t m0 = n->connector[i].associated;
          if(dt_connector_input(n->connector+i))
          { // walk node->module->module->node
            dt_cid_t m1 = graph->module[m0.i].connector[m0.c].connected;
            if(dt_cid_unset(m1)) continue;
            // check for a bypass chain
            // m3(out) -> m2(in) -> bypass m1(out) -> m0(in)
            if(!dt_cid_unset(graph->module[m1.i].connector[m1.c].bypass))
            { // now go from mi1/mc1(out) -> m2 = bypass(in) -> m3 = conn(out)
              // mi0:mc0 is the module connector (input) corresponding to our node
              // mi1:mc1 is an output connector on a different module connected to us
              // now we need to find the input connector in the same module, if there is bypass
              // mi2:mc2
              // and then find the module connected to this input connector, i.e.
              // mi3:mc3 will be an output connector on a different module
              // if mi3:mc3 has bypass, do the same again
              dt_cid_t m3;
              while(1)
              { // find input connector mc2, probably mi1==mi2 bypassing the module
                dt_cid_t m2 = graph->module[m1.i].connector[m1.c].bypass;
                if(dt_cid_unset(m2)) continue;
                // now find previous module mi3 with output connector mc3
                m3 = graph->module[m2.i].connector[m2.c].connected;
                if(dt_cid_unset(m3)) continue;
                // now if this module is again a bypass thing, continue the dance!
                if(dt_cid_unset(graph->module[m3.i].connector[m3.c].bypass)) break;
                if(m1.i == m3.i && m1.c == m3.c) break; // emergency exit
                m1 = m3;
              }
              id = graph->module[m3.i].connector[m3.c].associated;
            }
            else id = graph->module[m1.i].connector[m1.c].associated;
          }
          else // if(dt_connector_owner(n->connector+i))
            id = graph->module[m0.i].connector[m0.c].associated; // walk node->module->node
          n->connector[i].connected = id;
        }
      }
    }
    for(int i=0;i<cnt;i++)
      if(graph->module[modid[i]].connector[0].roi.full_wd > 0)
        init_display_flags(graph, graph->module+modid[i]);
    init_connector_images(graph);
  }
  // one last check:
  if(main_input_module >= 0 && graph->module[main_input_module].connector[0].roi.wd == 0) return VK_INCOMPLETE;
  return VK_SUCCESS;
}

