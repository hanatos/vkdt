#include "prims.h"
#include "geo.h"

#include <assert.h>
#include <string.h>
#include <float.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>

// slightly bloaty geometry storage backend.
// removed a lot of stuff we don't need if we're not doing cpu ray tracing on it.

#define common_alloc calloc

void prims_init(prims_t *p)
{
  memset(p, 0, sizeof(prims_t));
}

void prims_cleanup(prims_t *p)
{
  for(int k=0;k<p->num_shapes;k++)
  {
    munmap(p->shape[k].data, p->shape[k].data_size);
    if(p->shape[k].fd > 2)
      close(p->shape[k].fd);
  }
  free(p->shape);
  free(p->primid);
  memset(p, 0, sizeof(prims_t));
}

void prims_allocate(prims_t *p, const uint32_t num_shapes)
{
  p->num_shapes = num_shapes;
  p->shape = (prims_shape_t *)common_alloc(16, sizeof(prims_shape_t)*p->num_shapes);
}

void prims_discard_shape(prims_t *p, uint32_t shape)
{
  // only mark as empty to not change shapeids around. prims_allocate_index below
  // will be called right after a sweep over this, so the prims of this shape
  // will just never make it into the global array.
  //
  // i guess we could also deallocate shape resources here.
  p->num_prims -= p->shape[shape].num_prims;
  p->shape[shape].num_prims = 0;
}

// will be called after all shapes have been loaded.
void prims_allocate_index(prims_t *p)
{
  p->primid = (primid_t *)common_alloc(16, sizeof(primid_t)*p->num_prims);
  uint64_t num_loaded_prims = 0;
  for(int shapeid=0;shapeid<p->num_shapes;shapeid++)
  {
    // int shell = !strcmp(p->shape[shapeid].tex, "shell");
    for(int64_t k=0;k<p->shape[shapeid].num_prims;k++)
    {
      // copy over to joint array. mmap will forget that we loaded those pages later on, if needed.
      p->primid[num_loaded_prims + k] = p->shape[shapeid].primid[k];
      p->primid[num_loaded_prims + k].shapeid = shapeid;
      // if(shell) geo_shell_init(p, num_loaded_prims + k);
    }
    num_loaded_prims += p->shape[shapeid].num_prims;
  }
}

int prims_load_with_flags(
    prims_t *p,
    const char *filename,
    const char *texture,
    const int shader,
    char flags,
    const char *searchpath)
{
  int open_flags = (flags == 'r') ? O_RDONLY : O_RDWR;
  int mmap_flags = (flags == 'r') ? PROT_READ : (PROT_READ|PROT_WRITE);

  const int shapeid = p->num_loaded_shapes;
  p->shape[shapeid].material = shader;
  // store texture on shape
  strncpy(p->shape[shapeid].tex, texture, sizeof(p->shape[shapeid].tex)-1);
  char geoname[1024];
  snprintf(geoname, 1024, "%s.geo", filename);
  p->shape[shapeid].fd = open(geoname, open_flags);
  if((p->shape[shapeid].fd == -1) && searchpath)
  {
    char sn[1024];
    snprintf(sn, 1024, "%s/%s.geo", searchpath, filename); 
    p->shape[shapeid].fd = open(sn, open_flags);
  }
  if(p->shape[shapeid].fd == -1)
  {
    p->num_shapes--;
    fprintf(stderr, "[prims_load] could not load geo `%s'! decreasing shape count to %d.\n", filename, p->num_shapes);
    return 1;
  }
  p->shape[shapeid].data_size = lseek(p->shape[shapeid].fd, 0, SEEK_END);
  lseek(p->shape[shapeid].fd, 0, SEEK_SET);
  readahead(p->shape[shapeid].fd, 0, p->shape[shapeid].data_size);
  p->shape[shapeid].data = mmap(0, p->shape[shapeid].data_size, mmap_flags, MAP_SHARED,
                               p->shape[shapeid].fd, 0);
  close(p->shape[shapeid].fd);
  p->shape[shapeid].fd = -1;
  snprintf(p->shape[shapeid].name, 1024, "%s", filename);
  if(p->shape[shapeid].data == (void *)-1)
  {
    perror("[prims_load] mmap");
    p->num_shapes--;
    return 1;
  }

  const prims_header_t *header = (const prims_header_t *)p->shape[shapeid].data;
  if(header->magic != GEO_MAGIC)
  {
    fprintf(stderr, "[prims_load] geo `%s' magic number mismatch!\n", filename);
    p->num_shapes--;
    munmap(p->shape[shapeid].data, p->shape[shapeid].data_size);
    return 1;
  }
  if(header->version != GEO_VERSION)
  {
    fprintf(stderr, "[prims_load] geo `%s' version %d != %d (corona)\n", filename, header->version, GEO_VERSION);
    p->num_shapes--;
    munmap(p->shape[shapeid].data, p->shape[shapeid].data_size);
    return 1;
  }
  p->shape[shapeid].primid = (primid_t *)(header + 1);
  p->shape[shapeid].num_prims = header->num_prims;
  // map our pointers
  p->shape[shapeid].vtxidx = (prims_vtxidx_t *)((uint8_t*)header + header->vtxidx_offset);
  p->shape[shapeid].vtx = (prims_vtx_t *)((uint8_t*)header + header->vertex_offset);

  p->num_prims += header->num_prims;
  p->num_loaded_shapes ++;
  return 0;
}

#undef common_alloc
