#include "core/log.h"
#include "../ext/pthread-pool/pthread_pool.h"
#include "db/db.h"
#include "db/thumbnails.h"
#include "db/murmur3.h"
#include "qvk/qvk.h"
#include "pipe/graph-io.h"
#include "pipe/modules/api.h"
#include "pipe/dlist.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>


VkResult
dt_thumbnails_init(
    dt_thumbnails_t *tn,
    const int wd,
    const int ht,
    const int cnt,
    const size_t heap_size)
{
  memset(tn, 0, sizeof(*tn));

  // TODO: getenv(XDG_CACHE_HOME)
  const char *home = getenv("HOME");
  snprintf(tn->cachedir, sizeof(tn->cachedir), "%s/.cache/vkdt", home);
  int err = mkdir(tn->cachedir, 0755);
  if(err && errno != EEXIST)
  {
    dt_log(s_log_err|s_log_db, "could not create thumbnail cache directory!");
    return VK_INCOMPLETE;
  }

  tn->thumb_wd = wd,
  tn->thumb_ht = ht,
  tn->thumb_max = cnt;

  dt_graph_init(tn->graph + 0);
  dt_graph_init(tn->graph + 1);
  tn->graph[0].queue     = qvk.queue_work0;
  tn->graph[0].queue_idx = qvk.queue_idx_work0;
  tn->graph[1].queue     = qvk.queue_work1;
  tn->graph[1].queue_idx = qvk.queue_idx_work1;

  // just creating bc1 files in the background, not actually used to serve
  // any thumbnails:
  if(cnt == 0) return VK_SUCCESS;

  tn->thumb = malloc(sizeof(dt_thumbnail_t)*tn->thumb_max);
  memset(tn->thumb, 0, sizeof(dt_thumbnail_t)*tn->thumb_max);
  // need at least one extra slot to catch free block (if contiguous, else more)
  dt_vkalloc_init(&tn->alloc, tn->thumb_max + 10, heap_size);

  // init lru list
  tn->lru = tn->thumb;
  tn->mru = tn->thumb + tn->thumb_max-1;
  tn->thumb[0].next = tn->thumb+1;
  for(int k=1;k<tn->thumb_max-1;k++)
  {
    tn->thumb[k].next = tn->thumb+k+1;
    tn->thumb[k].prev = tn->thumb+k-1;
  }

  dt_log(s_log_db, "allocating %3.1f MB for thumbnails", heap_size/(1024.0*1024.0));

  // alloc dummy image to get memory type bits and something to display
  VkFormat format = VK_FORMAT_BC1_RGB_SRGB_BLOCK;
  VkImageCreateInfo images_create_info = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
    .imageType = VK_IMAGE_TYPE_2D,
    .format = format,
    .extent = {
      .width  = 24,
      .height = 24,
      .depth  = 1
    },
    .mipLevels             = 1,
    .arrayLayers           = 1,
    .samples               = VK_SAMPLE_COUNT_1_BIT,
    .tiling                = VK_IMAGE_TILING_OPTIMAL,
    .usage                 =
        VK_IMAGE_ASPECT_COLOR_BIT
      | VK_IMAGE_USAGE_TRANSFER_DST_BIT
      | VK_IMAGE_USAGE_SAMPLED_BIT,
    .sharingMode           = VK_SHARING_MODE_EXCLUSIVE,
    .queueFamilyIndexCount = 0,
    .pQueueFamilyIndices   = 0,
    .initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED,
  };

  VkImage img;
  QVKR(vkCreateImage(qvk.device, &images_create_info, NULL, &img));
  VkMemoryRequirements mem_req;
  vkGetImageMemoryRequirements(qvk.device, img, &mem_req);
  tn->memory_type_bits = mem_req.memoryTypeBits;
  vkDestroyImage(qvk.device, img, VK_NULL_HANDLE);

  VkMemoryAllocateInfo mem_alloc_info = {
    .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
    .allocationSize  = heap_size,
    .memoryTypeIndex = qvk_get_memory_type(tn->memory_type_bits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
  };
  QVKR(vkAllocateMemory(qvk.device, &mem_alloc_info, 0, &tn->vkmem));

  // create descriptor pool (keep at least one for each type)
  VkDescriptorPoolSize pool_sizes[] = {{
    .type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
    .descriptorCount = tn->thumb_max,
  }};

  VkDescriptorPoolCreateInfo pool_info = {
    .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
    .poolSizeCount = LENGTH(pool_sizes),
    .pPoolSizes    = pool_sizes,
    .maxSets       = tn->thumb_max,
  };
  QVKR(vkCreateDescriptorPool(qvk.device, &pool_info, 0, &tn->dset_pool));

  // create a descriptor set layout
  VkDescriptorSetLayoutBinding binding = {
    .descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
    .descriptorCount    = 1,
    .stageFlags         = VK_SHADER_STAGE_ALL,
    .pImmutableSamplers = 0,
  };
  VkDescriptorSetLayoutCreateInfo dset_layout_info = {
    .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
    .bindingCount = 1,
    .pBindings    = &binding,
  };
  QVKR(vkCreateDescriptorSetLayout(qvk.device, &dset_layout_info, 0, &tn->dset_layout));

  VkDescriptorSetAllocateInfo dset_info = {
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
    .descriptorPool = tn->dset_pool,
    .descriptorSetCount = 1,
    .pSetLayouts = &tn->dset_layout,
  };
  for(int i=0;i<tn->thumb_max;i++)
    QVKR(vkAllocateDescriptorSets(qvk.device, &dset_info, &tn->thumb[i].dset));

  return VK_SUCCESS;
}

void
dt_thumbnails_cleanup(
    dt_thumbnails_t *tn)
{
  dt_graph_cleanup(tn->graph + 0);
  dt_graph_cleanup(tn->graph + 1);
  for(int i=0;i<tn->thumb_max;i++)
  {
    if(tn->thumb[i].image)      vkDestroyImage    (qvk.device, tn->thumb[i].image,      0);
    if(tn->thumb[i].image_view) vkDestroyImageView(qvk.device, tn->thumb[i].image_view, 0);
  }
  free(tn->thumb);
  tn->thumb = 0;
  if(tn->dset_layout) vkDestroyDescriptorSetLayout(qvk.device, tn->dset_layout, 0);
  if(tn->dset_pool)   vkDestroyDescriptorPool     (qvk.device, tn->dset_pool,   0);
  if(tn->vkmem)       vkFreeMemory                (qvk.device, tn->vkmem,       0);
  dt_vkalloc_cleanup(&tn->alloc);
}

// process one image and write a .bc1 thumbnail
// return 0 on success
VkResult
dt_thumbnails_cache_one(
    dt_graph_t      *graph,
    dt_thumbnails_t *tn,
    const char      *filename)  // only accepting .cfg files here (can be non-existent and will be replaced in such case)
{
  int len = strnlen(filename, 2048); // sizeof thumbnail filename
  if(len <= 4) return VK_INCOMPLETE;
  const char *f2 = filename + len - 4;
  if(strcasecmp(f2, ".cfg")) return VK_INCOMPLETE;

  // use ~/.cache/vkdt/<murmur3-of-filename>.bc1 as output file name
  // if that already exists with a newer timestamp than the cfg, bail out

  char cfgfilename[1024];
  char imgfilename[1024];
  char bc1filename[1024];
  uint32_t hash = murmur_hash3(filename, len, 1337);
  snprintf(bc1filename, sizeof(bc1filename), "%s/%x.bc1", tn->cachedir, hash);
  snprintf(cfgfilename, sizeof(cfgfilename), "%s", filename);
  snprintf(imgfilename, sizeof(imgfilename), "%s", filename);
  imgfilename[len-4] = 0; // cut away ".cfg"
  int load_default = 0;
  struct stat statbuf = {0};
  time_t tcfg = 0, tbc1 = 0;

  if(!stat(cfgfilename, &statbuf))
    tcfg = statbuf.st_mtim.tv_sec;
  else
  {
    load_default = 1;
    snprintf(cfgfilename, sizeof(cfgfilename), "default.cfg");
    if(!stat(cfgfilename, &statbuf))
      tcfg = statbuf.st_mtim.tv_sec;
    else return VK_INCOMPLETE;
  }

  if(!stat(bc1filename, &statbuf))
  { // check timestamp
    tbc1 = statbuf.st_mtim.tv_sec;
    if(tcfg && (tbc1 >= tcfg)) return VK_SUCCESS; // already up to date
  }

  // load history stack
  dt_graph_reset(graph);
  if(dt_graph_read_config_ascii(graph, cfgfilename))
  {
    dt_log(s_log_err, "[thm] could not load graph configuration from '%s'!", cfgfilename);
    return 2;
  }

  if(load_default)
  { // set param for rawinput
    int modid = dt_module_get(graph, dt_token("i-raw"), dt_token("01"));
    if(modid < 0 ||
        dt_module_set_param_string(graph->module + modid, dt_token("filename"), imgfilename))
    {
      dt_log(s_log_err, "[thm] config '%s' has no raw input module!", cfgfilename);
      return 3;
    }
  }

  // replace display by o-bc1 in case it's not default.cfg:
  int modid = dt_module_get(graph, dt_token("display"), dt_token("main"));
  if(modid >= 0)
  {
    int cid = dt_module_get_connector(graph->module+modid, dt_token("input"));
    int m0 = graph->module[modid].connector[cid].connected_mi;
    int o0 = graph->module[modid].connector[cid].connected_mc;
    if(m0 < 0)
    {
      dt_log(s_log_err, "[thm] config '%s' has no connected display module!", cfgfilename);
      return 3; // display input not connected
    }
    const int m1 = dt_module_add(graph, dt_token("f2srgb"), dt_token("main"));
    const int i1 = dt_module_get_connector(graph->module+m1, dt_token("input"));
    const int o1 = dt_module_get_connector(graph->module+m1, dt_token("output"));
    const int m2 = dt_module_add(graph, dt_token("o-bc1"), dt_token("main"));
    const int i2 = dt_module_get_connector(graph->module+m2, dt_token("input"));
    graph->module[m1].connector[o1].format = graph->module[m2].connector[i2].format;
    int32_t *usemat = (int32_t *)dt_module_param_int(graph->module+m1, 0);
    usemat[0] = 0; // write as rec2020
    if(dt_module_connect(graph, m0, o0, m1, i1) ||
       dt_module_connect(graph, m1, o1, m2, i2))
    {
      dt_log(s_log_err, "[thm] config '%s' connecting bc1 output failed!", cfgfilename);
      return 3;
    }
    dt_module_remove(graph, modid);
  }

  modid = dt_module_get(graph, dt_token("o-bc1"), dt_token("main"));
  if(modid < 0 ||
     dt_module_set_param_string(graph->module + modid, dt_token("filename"), bc1filename))
  {
    dt_log(s_log_err, "[thm] config '%s' has no bc1 output module!", cfgfilename);
    return 3;
  }

  // ask for reduced resolution in the graph:
  graph->output_wd = tn->thumb_wd;
  graph->output_ht = tn->thumb_ht;

  clock_t beg = clock();
  if(dt_graph_run(graph, s_graph_run_all) != VK_SUCCESS)
  {
    dt_log(s_log_err, "[thm] running the thumbnail graph failed on image '%s'!", imgfilename);
    // mark as dead
    link("data/bomb.bc1", bc1filename);
    return 4;
  }
  clock_t end = clock();
  dt_log(s_log_pipe|s_log_perf, "[thm] ran graph in %3.0fms", 1000.0*(end-beg)/CLOCKS_PER_SEC);

  return VK_SUCCESS;
}

typedef struct cache_coll_job_t
{
  threads_mutex_t *mutex;
  dt_thumbnails_t *tn;
  dt_db_t *db;
  uint32_t *coll;
  uint32_t num;
  uint32_t idx;
  int k;
}
cache_coll_job_t;

static void *thread_work_coll(void *arg)
{
  cache_coll_job_t *j = arg;
  // FIXME: already done this before? for whatever buggy reason the scheduler picks up stale jobs twice.
  // so we assert we don't run twice here. this keeps us from freeing the job below, unfortunately.
  if(j->idx >= j->num) return 0;
  // fprintf(stderr, "[thmb] thread %d working on %d!\n", threads_id(), j->k);
  assert(j->tn);

  j->tn->graph[j->k].io_mutex = &j->mutex;
  for(;j->idx<j->num;j->idx++)
  {
    if((j->idx % DT_THUMBNAILS_THREADS) == j->k)
      (void) dt_thumbnails_cache_one(j->tn->graph + j->k, j->tn,
          j->db->image[j->coll[j->idx]].filename);
    if(threads_shutting_down()) break;
  }
  j->tn->graph[j->k].io_mutex = 0;
  // TODO: cleanup mutex and job here
  // TODO: unfortunately, due to the scheduler bug, we'll potentially need the job memory again (see above)
  // free(j);
  return 0;
}

VkResult
dt_thumbnails_cache_collection(
    dt_thumbnails_t *tn,
    dt_db_t         *db)
{
  if(db->collection_cnt <= 0)
  {
    dt_log(s_log_err, "[thm] no images in collection!");
    return VK_INCOMPLETE;
  }

  uint32_t *collection = db->collection; // TODO: take copy once this thing changes
  threads_mutex_t mutex;
  threads_mutex_init(&mutex, 0); // we'll leak this, too, because we're not waiting for the threads
  cache_coll_job_t *job[DT_THUMBNAILS_THREADS];
  for(int k=0;k<DT_THUMBNAILS_THREADS;k++)
  {
    job[k] = malloc(sizeof(cache_coll_job_t));
    *(job[k]) = (cache_coll_job_t) {
      .mutex = &mutex,
      .num   = db->collection_cnt,
      .coll  = collection,
      .k     = k,
      .tn    = tn,
      .db    = db,
    };
  }
  for(int k=0;k<DT_THUMBNAILS_THREADS;k++)
    threads_task(k, &thread_work_coll, job[k]);
  return VK_SUCCESS;
}

// 1) if db loads a directory, kick off thumbnail creation of directory in bg
//    this step is the only thing in the non-gui thread
// 2) for currently visible collection: batch-update lru and trigger thumbnail loading
//    if necessary (bc1 file exists but not loaded, maybe need "ready" flag)
//    this should be fast enough to run every refresh.
//    start single-thread and maybe interleave with two threads, too
//    (needs lru mutex then)
// this function is 2):
void
dt_thumbnails_load_list(
    dt_thumbnails_t *tn,
    dt_db_t         *db,
    uint32_t        *collection,
    uint32_t         beg,
    uint32_t         end)
{
  // for all images in given collection
  for(int k=beg;k<end;k++)
  {
    const uint32_t imgid = collection[k];
    dt_image_t *img = db->image + imgid;
    if(img->thumbnail == 1) continue; // known broken
    if(img->thumbnail == 0)
    { // not loaded
      if(dt_thumbnails_load_one(tn, img->filename, &img->thumbnail))
        img->thumbnail = 0;
    }
    else if(img->thumbnail > 0 && img->thumbnail < tn->thumb_max)
    { // loaded, update lru
      // threads_mutex_lock(&tn->lru_lock);
      dt_thumbnail_t *th = tn->thumb + img->thumbnail;
      if(th == tn->lru) tn->lru = tn->lru->next; // move head
      DLIST_RM_ELEMENT(th);                      // disconnect old head
      tn->mru = DLIST_APPEND(tn->mru, th);       // append to end and move tail
      // threads_mutex_unlock(&tn->lru_lock);
    }
  }
}

// load a previously cached thumbnail to a VkImage onto the GPU.
// returns VK_SUCCESS on success
VkResult
dt_thumbnails_load_one(
    dt_thumbnails_t *tn,
    const char      *filename,
    uint32_t        *thumb_index)
{
  dt_graph_t *graph = tn->graph;
  char cfgfilename[1024] = {0};
  char imgfilename[1024] = {0};
  snprintf(cfgfilename, sizeof(cfgfilename), "thumb.cfg");
  if(strncmp(filename, "data/", 5))
  { // only hash images that aren't straight from our resource directory:
    // TODO: make sure ./dir/file and dir//file etc turn out to be the same
    uint32_t hash = murmur_hash3(filename, strnlen(filename, 2048), 1337);
    snprintf(imgfilename, sizeof(imgfilename), "%s/%x.bc1", tn->cachedir, hash);
  }
  else snprintf(imgfilename, sizeof(imgfilename), "%s", filename);
  struct stat statbuf = {0};
  if(stat(imgfilename, &statbuf)) return VK_INCOMPLETE;
  if(stat(cfgfilename, &statbuf)) return VK_INCOMPLETE;

  dt_graph_reset(graph);
  if(dt_graph_read_config_ascii(graph, cfgfilename))
  {
    dt_log(s_log_err, "[thm] could not load graph configuration from '%s'!", cfgfilename);
    return VK_INCOMPLETE;
  }

  dt_thumbnail_t *th = 0;
  if(*thumb_index < 2 || *thumb_index == -1u)
  {
    // allocate thumbnail from lru list
    // threads_mutex_lock(&tn->lru_lock);
    th = tn->lru;
    tn->lru = tn->lru->next;             // move head
    DLIST_RM_ELEMENT(th);                // disconnect old head
    tn->mru = DLIST_APPEND(tn->mru, th); // append to end and move tail
    *thumb_index = th - tn->thumb;
    // threads_mutex_unlock(&tn->lru_lock);
  }
  else th = tn->thumb + *thumb_index;
  
  // cache eviction:
  // clean up memory in case there was something here:
  if(th->image)      vkDestroyImage(qvk.device, th->image, VK_NULL_HANDLE);
  if(th->image_view) vkDestroyImageView(qvk.device, th->image_view, VK_NULL_HANDLE);
  th->image      = 0;
  th->image_view = 0;
  th->imgid      = -1u;
  th->offset     = -1u;
  if(th->mem)    dt_vkfree(&tn->alloc, th->mem);
  th->mem        = 0;
  // keep dset and prev/next dlist pointers! (i.e. don't memset th)

  // set param for rawinput
  // get module
  int modid = dt_module_get(graph, dt_token("i-bc1"), dt_token("01"));
  if(modid < 0 ||
     dt_module_set_param_string(graph->module + modid, dt_token("filename"), imgfilename))
  {
    dt_log(s_log_err, "[thm] config '%s' has no bc1 input module!", cfgfilename);
    return VK_INCOMPLETE;
  }

  // run graph only up to roi computations to get size
  // run all <= create nodes
  dt_graph_run_t run = ~-(s_graph_run_create_nodes<<1);
  if(dt_graph_run(graph, run) != VK_SUCCESS)
  {
    dt_log(s_log_err, "[thm] failed to run first half of graph!");
    return VK_INCOMPLETE;
  }

  // now grab roi size from graph's main output node
  modid = dt_module_get(graph, dt_token("thumb"), dt_token("main"));
  th->wd = graph->module[modid].connector[0].roi.full_wd;
  th->ht = graph->module[modid].connector[0].roi.full_ht;

  VkFormat format = VK_FORMAT_BC1_RGB_SRGB_BLOCK;
  VkImageCreateInfo images_create_info = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
    .imageType = VK_IMAGE_TYPE_2D,
    .format = format,
    .extent = {
      .width  = th->wd,
      .height = th->ht,
      .depth  = 1
    },
    .mipLevels             = 1,
    .arrayLayers           = 1,
    .samples               = VK_SAMPLE_COUNT_1_BIT,
    .tiling                = VK_IMAGE_TILING_OPTIMAL,
    .usage                 =
        VK_IMAGE_ASPECT_COLOR_BIT
      | VK_IMAGE_USAGE_TRANSFER_DST_BIT
      | VK_IMAGE_USAGE_SAMPLED_BIT,
    .sharingMode           = VK_SHARING_MODE_EXCLUSIVE,
    .queueFamilyIndexCount = 0,
    .pQueueFamilyIndices   = 0,
    .initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED,
  };

  QVKR(vkCreateImage(qvk.device, &images_create_info, NULL, &th->image));
  VkMemoryRequirements mem_req;
  vkGetImageMemoryRequirements(qvk.device, th->image, &mem_req);
  if(mem_req.memoryTypeBits != tn->memory_type_bits)
    dt_log(s_log_qvk|s_log_err, "[thm] memory type bits don't match!");

  dt_vkmem_t *mem = dt_vkalloc(&tn->alloc, mem_req.size, mem_req.alignment);
  // TODO: if (!mem) we have not enough memory! need to handle this now (more cache eviction?)
  // TODO: could do batch cleanup in case we need memory:
  // walk lru list from front and kill all contents (see above)
  // but leave list as it is

  assert(mem);
  th->mem    = mem;
  th->offset = mem->offset;

  VkImageViewCreateInfo images_view_create_info = {
    .sType      = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
    .viewType   = VK_IMAGE_VIEW_TYPE_2D,
    .format     = format,
    .subresourceRange = {
      .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
      .baseMipLevel   = 0,
      .levelCount     = 1,
      .baseArrayLayer = 0,
      .layerCount     = 1
    },
  };
  VkDescriptorImageInfo img_info = {
    .sampler       = th->wd > 32 ? qvk.tex_sampler : qvk.tex_sampler_nearest,
    .imageLayout   = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
  };
  VkWriteDescriptorSet img_dset = {
    .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
    .dstBinding      = 0,
    .dstArrayElement = 0,
    .descriptorCount = 1,
    .descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
    .pImageInfo      = &img_info,
  };

  // bind image memory, create image view and descriptor set (used to display later on):
  vkBindImageMemory(qvk.device, th->image, tn->vkmem, th->offset);
  images_view_create_info.image = th->image;
  QVKR(vkCreateImageView(qvk.device, &images_view_create_info, NULL, &th->image_view));

  img_dset.dstSet    = th->dset;
  img_info.imageView = th->image_view;
  vkUpdateDescriptorSets(qvk.device, 1, &img_dset, 0, NULL);

  // now run the rest of the graph and copy over VkImage
  // let graph render into our thumbnail:
  graph->thumbnail_image = tn->thumb[*thumb_index].image;
  // these should already match, let's not mess with rounding errors:
  // tn->graph.output_wd = th->wd;
  // tn->graph.output_ht = th->ht;

  clock_t beg = clock();
  // run all the rest we didn't run above
  if(dt_graph_run(graph, ~run) != VK_SUCCESS)
  {
    dt_log(s_log_err, "[thm] running the thumbnail graph failed on image '%s'!", imgfilename);
    return VK_INCOMPLETE;
  }
  clock_t end = clock();
  dt_log(s_log_pipe|s_log_perf, "[thm] ran graph in %3.0fms", 1000.0*(end-beg)/CLOCKS_PER_SEC);

  return VK_SUCCESS;
}
