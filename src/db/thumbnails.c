#include "core/log.h"
#include "core/fs.h"
#include "db/db.h"
#include "db/thumbnails.h"
#include "db/hash.h"
#include "qvk/qvk.h"
#include "pipe/graph-io.h"
#include "pipe/graph-defaults.h"
#include "pipe/graph-export.h"
#include "pipe/modules/api.h"
#include "pipe/dlist.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include <inttypes.h>

#if 0
void
debug_test_list(
    dt_thumbnails_t *t)
{
  dt_thumbnail_t *l = t->lru;
  assert(!l->prev);
  assert(l->next);
  int len = 1;
  while(l->next)
  {
    dt_thumbnail_t *n = l->next;
    assert(n->prev == l);
    l = n;
    len++;
  }
  assert(t->thumb_max == len);
  assert(t->mru == l);
}
#endif

VkResult
dt_thumbnails_init(
    dt_thumbnails_t *tn,
    const int wd,
    const int ht,
    const int cnt,
    const size_t heap_size)
{
  memset(tn, 0, sizeof(*tn));

#ifdef __ANDROID__
  snprintf(tn->cachedir, sizeof(tn->cachedir), "%s/cache", dt_pipe.app->activity->internalDataPath);
#else
  fs_cachedir(tn->cachedir, sizeof(tn->cachedir));
#endif
  int err = fs_mkdir_p(tn->cachedir, 0755);
  if(err && errno != EEXIST)
  {
    dt_log(s_log_err|s_log_db, "could not create thumbnail cache directory %s!", tn->cachedir);
    return VK_INCOMPLETE;
  }

  tn->thumb_wd = wd,
  tn->thumb_ht = ht,
  tn->thumb_max = cnt;

  dt_graph_init(tn->graph + 0, s_queue_work0);
  dt_graph_init(tn->graph + 1, s_queue_work1);

  threads_mutex_init(tn->graph_lock + 0, 0);
  threads_mutex_init(tn->graph_lock + 1, 0);

  // just creating bc1 files in the background, not actually used to serve
  // any thumbnails:
  if(cnt == 0) return VK_SUCCESS;

  tn->thumb = malloc(sizeof(dt_thumbnail_t)*tn->thumb_max);
  memset(tn->thumb, 0, sizeof(dt_thumbnail_t)*tn->thumb_max);
  // need at least one extra slot to catch free block (if contiguous, else more)
  // seems we sometimes get quite fragmented memory after long runs, be sure we can always split free memory:
  dt_vkalloc_init(&tn->alloc, 3*tn->thumb_max, heap_size);

  // init lru list
  tn->lru = tn->thumb + 1; // [0] is special: busy bee
  tn->mru = tn->thumb + tn->thumb_max-1;
  tn->thumb[1].next = tn->thumb+2;
  tn->thumb[tn->thumb_max-1].prev = tn->thumb+tn->thumb_max-2;
  for(int k=2;k<tn->thumb_max-1;k++)
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
    .memoryTypeIndex = qvk_memory_get_device(),
  };
  QVKR(vkAllocateMemory(qvk.device, &mem_alloc_info, 0, &tn->vkmem));

  // create descriptor pool (keep at least one for each type)
  VkDescriptorPoolSize pool_sizes[] = {{
    .type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
    .descriptorCount = tn->thumb_max,
  }};

  VkDescriptorPoolCreateInfo pool_info = {
    .flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
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
    .flags        = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
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
  for(int i=0;i<DT_THUMBNAILS_THREADS;i++)
  {
    dt_graph_cleanup(tn->graph + i);
    pthread_mutex_destroy(tn->graph_lock + i);
  }
  for(int i=0;i<tn->thumb_max;i++)
  {
    if(tn->thumb[i].image)      vkDestroyImage    (qvk.device, tn->thumb[i].image,      0);
    if(tn->thumb[i].image_view) vkDestroyImageView(qvk.device, tn->thumb[i].image_view, 0);
    tn->thumb[i].image      = 0;
    tn->thumb[i].image_view = 0;
  }
  free(tn->thumb);
  tn->thumb = 0;
  if(tn->dset_layout) vkDestroyDescriptorSetLayout(qvk.device, tn->dset_layout, 0);
  if(tn->dset_pool)   vkDestroyDescriptorPool     (qvk.device, tn->dset_pool,   0);
  if(tn->vkmem)       vkFreeMemory                (qvk.device, tn->vkmem,       0);
  dt_vkalloc_cleanup(&tn->alloc);
}

void
dt_thumbnails_invalidate(
    dt_thumbnails_t *tn,
    const char      *filename)
{
  uint64_t hash = hash64(filename);
  char bc1filename[1040];
  snprintf(bc1filename, sizeof(bc1filename), "%s/%"PRIx64".bc1", tn->cachedir, hash);
  unlink(bc1filename);
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

  // use ~/.cache/vkdt/<hash-of-filename>.bc1 as output file name
  // if that already exists with a newer timestamp than the cfg, bail out

  dt_token_t input_module = dt_graph_default_input_module(filename);
  char cfgfilename[PATH_MAX+100];
  char deffilename[PATH_MAX+100];
  char bc1filename[PATH_MAX+100];
  uint64_t hash = hash64(filename);
  if(snprintf(bc1filename, sizeof(bc1filename), "%s/%"PRIx64".bc1", tn->cachedir, hash) >= sizeof(bc1filename)) return VK_INCOMPLETE;
  if(snprintf(cfgfilename, sizeof(cfgfilename), "%s", filename) >= sizeof(cfgfilename)) return VK_INCOMPLETE;
  if(snprintf(deffilename, sizeof(deffilename), "default.%"PRItkn, dt_token_str(input_module)) >= sizeof(deffilename)) return VK_INCOMPLETE;
  struct stat statbuf = {0};
  time_t tcfg = 0, tbc1 = 0;

  if(!stat(cfgfilename, &statbuf))
#ifdef __APPLE__
    tcfg = statbuf.st_mtimespec.tv_sec;
#else
    tcfg = statbuf.st_mtime;
#endif
  else
  {
    char tmp[PATH_MAX];
    if(snprintf(tmp, sizeof(tmp), "%s/%s", dt_pipe.basedir, deffilename) >= PATH_MAX)
      return VK_INCOMPLETE;
    if(!stat(tmp, &statbuf))
#ifdef __APPLE__
      tcfg = statbuf.st_mtimespec.tv_sec;
#else
      tcfg = statbuf.st_mtime;
#endif
    else return VK_INCOMPLETE;
  }

  if(!stat(bc1filename, &statbuf))
  { // check timestamp
#ifdef __APPLE__
    tbc1 = statbuf.st_mtimespec.tv_sec;
#else
    tbc1 = statbuf.st_mtime;
#endif
    if(tcfg && (tbc1 >= tcfg)) return VK_SUCCESS; // already up to date
  }

  dt_graph_reset(graph);

  char *extrap[] = {
    "frames:1",                   // only render first frame of animation
  };
  dt_graph_export_t param = {
    .extra_param_cnt = 1,
    .p_extra_param   = extrap,
    .p_cfgfile       = cfgfilename,
    .p_defcfg        = deffilename,
    .input_module    = input_module,
    .output_cnt      = 1,
    .output = {{
      .max_width  = tn->thumb_wd,
      .max_height = tn->thumb_ht,
      .mod        = dt_token("o-bc1"),
      .inst       = dt_token("main"),
      .p_filename = bc1filename,
      .colour_primaries = s_colour_primaries_2020, // thumbnails are immediately in working space
      .colour_trc       = s_colour_trc_srgb,       // but have srgb gamma to better fit into 8 bit files
    }},
  };

  clock_t beg = clock();
  if(dt_graph_export(graph, &param) != VK_SUCCESS)
  {
    dt_log(s_log_db, "[thm] running the thumbnail graph failed on image '%s'!", filename);
    // mark as dead
    snprintf(cfgfilename, sizeof(cfgfilename), "%s/data/bomb.bc1", dt_pipe.basedir);
    fs_link(cfgfilename, bc1filename);
    return 4;
  }
  clock_t end = clock();
  dt_log(s_log_perf, "[thm] ran graph in %3.0fms", 1000.0*(end-beg)/CLOCKS_PER_SEC);

  return VK_SUCCESS;
}

typedef struct cache_coll_job_t
{
  uint64_t stamp;
  threads_mutex_t mutex_storage;
  uint32_t gid;
  threads_mutex_t *mutex;
  dt_thumbnails_t *tn;
  dt_db_t *db;
  uint32_t *coll;
  void    (*ufn)(void);
}
cache_coll_job_t;

static void thread_free_coll(void *arg)
{
  // task is done, every thread will call this
  cache_coll_job_t *j = arg;
  // only first thread destroys shared things
  if(j->gid == 0)
  {
    pthread_mutex_destroy(&j->mutex_storage);
    free(j->coll);
  }
  free(j);
}

void
dt_thumbnails_cache_abort(
    dt_thumbnails_t *tn)
{
  tn->job_timestamp++;
  for(int i=0;i<DT_THUMBNAILS_THREADS;i++)
    threads_mutex_lock(tn->graph_lock+i);
  // now we hold all the locks at the same time. anyone picking up a lock after we return from here
  // will definitely see the new timestamp and abort immediately.
  for(int i=0;i<DT_THUMBNAILS_THREADS;i++)
    threads_mutex_unlock(tn->graph_lock+i);
}

static void
thread_work_coll(
    uint32_t item, void *arg)
{
  cache_coll_job_t *j = arg;
  threads_mutex_lock(j->tn->graph_lock+j->gid); // shield against potential overscheduling (call _cache_list() from the gui before the old one is done)
  if(j->stamp != j->tn->job_timestamp) goto abort; // job invalid/stale, will not be able to access db any more!
  j->tn->graph[j->gid].io_mutex = j->mutex;
  char filename[1024];
  dt_db_image_path(j->db, j->coll[item], filename, sizeof(filename));
  (void) dt_thumbnails_cache_one(j->tn->graph + j->gid, j->tn, filename);
  // invalidate what we have in memory to trigger a reload:
  threads_mutex_lock(&j->db->image_mutex);
  j->db->image[j->coll[item]].thumbnail = 0;
  threads_mutex_unlock(&j->db->image_mutex);
  j->tn->graph[j->gid].io_mutex = 0;
  if(j->ufn) j->ufn();
abort:
  threads_mutex_unlock(j->tn->graph_lock+j->gid);
}

VkResult
dt_thumbnails_cache_list(
    dt_thumbnails_t *tn,
    dt_db_t         *db,
    const uint32_t  *imgid,
    uint32_t         imgid_cnt,
    void           (*updatefn)(void))
{
  if(imgid_cnt <= 0)
    return VK_INCOMPLETE;

  uint32_t *collection = malloc(sizeof(uint32_t) * imgid_cnt);
  memcpy(collection, imgid, sizeof(uint32_t) * imgid_cnt); // take copy because this thing changes
  cache_coll_job_t *job0 = 0;
  int taskid = -1;
  for(int k=0;k<DT_THUMBNAILS_THREADS;k++)
  {
    cache_coll_job_t *job = malloc(sizeof(cache_coll_job_t));
    if(k == 0)
    {
      *job = (cache_coll_job_t) {
        .stamp = tn->job_timestamp,
        .coll  = collection,
        .gid   = k,
        .tn    = tn,
        .db    = db,
        .ufn   = updatefn,
      };
      threads_mutex_init(&job->mutex_storage, 0);
      job->mutex = &job->mutex_storage;
      job0 = job;
    }
    else *job = (cache_coll_job_t) {
      .stamp = tn->job_timestamp,
      .mutex = &job0->mutex_storage,
      .coll  = collection,
      .gid   = k,
      .tn    = tn,
      .db    = db,
      .ufn   = updatefn,
    };
    // we only care about internal errors. if we call with stupid values,
    // it just does nothing and returns:
    taskid = threads_task(
        "thumb",
        imgid_cnt,
        taskid,
        job,
        thread_work_coll,
        thread_free_coll);
    if(taskid < 0) return VK_INCOMPLETE;
  }
  return VK_SUCCESS;
}

VkResult
dt_thumbnails_cache_collection(
    dt_thumbnails_t *tn,
    dt_db_t         *db,
    void           (*updatefn)(void))
{
  return dt_thumbnails_cache_list(tn, db, db->collection, db->collection_cnt, updatefn);
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
    const uint32_t  *collection,
    uint32_t         beg,
    uint32_t         end)
{
  for(int k=beg;k<end;k++)
  { // for all images in given collection
    const uint32_t imgid = collection[k];
    if(imgid >= db->image_cnt) break; // safety first. this probably means this job is stale! big danger!
    dt_image_t *img = db->image + imgid;
    uint32_t tid = img->thumbnail;
    if(tid == 0)
    { // not loaded
      char filename[1024];
      dt_db_image_path(db, imgid, filename, sizeof(filename));  
      uint32_t thumb_index = -1u;
      if(dt_thumbnails_load_one(tn, filename, &thumb_index) == VK_SUCCESS)
      {
        threads_mutex_lock(&db->image_mutex);
        img->thumbnail = thumb_index;
        threads_mutex_unlock(&db->image_mutex);
      }
    }
    else if(tid > 0 && tid < tn->thumb_max)
    { // loaded, update lru
      // threads_mutex_lock(&tn->lru_lock);
      dt_thumbnail_t *th = tn->thumb + tid;
      if(th == tn->lru) tn->lru = tn->lru->next; // move head
      tn->lru->prev = 0;
      if(tn->mru == th) tn->mru = th->prev;      // going to remove mru, need to move
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
  char imgfilename[PATH_MAX] = {0};
  if(strncmp(filename, "data/", 5))
  { // only hash images that aren't straight from our resource directory:
    // XXX run through realpath once for windows and / vs \\ confusion?
    uint64_t hash = hash64(filename);
    if(snprintf(imgfilename, sizeof(imgfilename), "%s/%"PRIx64".bc1", tn->cachedir, hash) >= sizeof(imgfilename)) return VK_INCOMPLETE;
  }
  else if(snprintf(imgfilename, sizeof(imgfilename), "%s/%s", dt_pipe.basedir, filename) >= sizeof(imgfilename)) return VK_INCOMPLETE;
  struct stat statbuf = {0};
  if(stat(imgfilename, &statbuf)) return VK_INCOMPLETE;

  dt_graph_reset(graph);
  int m0 = dt_module_add(graph, dt_token("i-bc1"), dt_token("main"));
  int m1 = dt_module_add(graph, dt_token("thumb"), dt_token("main"));
  if(m0 < 0 || m1 < 0)
  { // catching a crash here, but this is worrying
    dt_log(s_log_err, "[thm] failed to add modules to the graph!");
    return VK_INCOMPLETE;
  }
  dt_module_connect(graph, m0, 0, m1, 0);

  dt_thumbnail_t *th = 0;
  if(*thumb_index == -1u)
  { // allocate thumbnail from lru list
    // threads_mutex_lock(&tn->lru_lock);
    th = tn->lru;
    tn->lru = tn->lru->next;             // move head
    if(tn->mru == th) tn->mru = th->prev;// going to remove mru, need to move
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
  dt_module_set_param_string(graph->module + m0, dt_token("filename"), imgfilename);

  // run graph only up to roi computations to get size
  // run all <= create nodes
  dt_graph_run_t run = ~-(s_graph_run_create_nodes<<1);
  if(dt_graph_run(graph, run) != VK_SUCCESS)
  {
    dt_log(s_log_err, "[thm] failed to run first half of graph!");
    return VK_INCOMPLETE;
  }

  // now grab roi size from graph's main output node
  th->wd = graph->module[m1].connector[0].roi.full_wd;
  th->ht = graph->module[m1].connector[0].roi.full_ht;

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
  if(!mem)
  {
    dt_log(s_log_err, "[thm] no more thumbnail gpu memory allocation possible!\n");
    return VK_INCOMPLETE; // probably fragmented memory
  }
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

  clock_t beg = clock();
  // run all the rest we didn't run above
  if(dt_graph_run(graph, ~run) != VK_SUCCESS)
  {
    dt_log(s_log_err, "[thm] running the thumbnail graph failed on image '%s'!", imgfilename);
    return VK_INCOMPLETE;
  }
  clock_t end = clock();
  dt_log(s_log_perf, "[thm] ran graph in %3.0fms", 1000.0*(end-beg)/CLOCKS_PER_SEC);

  // reset here too to make sure cleanup() is called on all modules.
  // this releases file descriptors and webcams etc.
  dt_graph_reset(graph);

  return VK_SUCCESS;
}
