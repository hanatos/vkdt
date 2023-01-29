#pragma once
// #include "corona_common.h"
// #include "pathspace.h"
#include <stdint.h>
#include <stdio.h>
// #include <xmmintrin.h>

typedef struct primid_t
{
  uint32_t extra : 3;     // extra per-primitive bits (e.g. used to store meshing info for shells)
  uint32_t shapeid : 29;
  uint32_t vi : 28;       // points into vtxidx, which is used to find the vertex location and normals and such
  uint32_t mb : 1;        // has motion blur?
  uint32_t vcnt : 3;      // wraps the type of primitive (tri, quad, or..)
}
primid_t;

// assign meaning to the three-bit primid_t.vcnt variable.
typedef enum prim_type_t
{
  s_prim_none = 0,
  s_prim_sphere = 1,
  s_prim_line = 2,
  s_prim_tri = 3,
  s_prim_quad = 4,
  s_prim_shell = 5,
}
prim_type_t;

typedef struct prims_vtxidx_t
{
  uint32_t v, uv;
}
prims_vtxidx_t;

typedef struct prims_header_t
{
  int32_t magic;
  int32_t version;

  uint64_t num_prims;
  uint64_t vtxidx_offset;
  uint64_t vertex_offset;
}
prims_header_t;

// typedef union prims_vtx_t
// {
//   __m128 m;
//   struct
typedef struct prims_vtx_t
{
  //{
    float v[3]; // vertex coordinates
    uint32_t n; // compressed normal
  //};
}
__attribute__((aligned(16))) // sse alignment for fast motion blur
prims_vtx_t;

typedef struct prims_shape_t
{
  int64_t material;
  uint64_t num_prims;

  char name[1024];  // basename of mapped file
  char tex[512];    // texture name
  int fd;           // file descriptor to possibly out-of-core geometry
  void *data;       // points to mmapped data array, stored so we can unmap it.
  size_t data_size; // size of mmapped buffer

  prims_vtxidx_t *vtxidx; // points into the actual data arrays below
  prims_vtx_t *vtx; // list of vertices, strided 2x if motion blur is enabled

  primid_t *primid; // list of prims sorted by shape. if you don't access this, mmap will forget it :)
}
prims_shape_t;

typedef struct prims_t
{
  // list of shapes:
  uint32_t num_shapes;
  prims_shape_t *shape;

  // loading time counters:
  uint64_t num_loaded_prims;
  uint32_t num_loaded_shapes;

  // interface for bvh building
  uint64_t num_prims;  // total number of primitives for all shapes
  primid_t *primid;    // index array, storing 32-bit shapeid and 32-bit primid

  float ghost_aabb[6]; // axis aligned bounding box of ghost objects, to be considered for scene bounds but invisible/discarded
}
prims_t;

static inline uint64_t
prims_get_shape_vtx_cnt(const prims_t *p, uint32_t shapeid)
{
  const int mb = p->shape[shapeid].primid[0].mb+1; // the whole shape either has motion blur or not
  return (p->shape[shapeid].data_size - ((uint8_t *)p->shape[shapeid].vtx - (uint8_t *)p->shape[shapeid].data))/
    (sizeof(prims_vtx_t) * mb);
}

#if 0
// bounding boxes
void prims_get_bounds_shutter_open(const prims_t *p, primid_t pi, int dim, float *min, float *max);
void prims_get_bounds_shutter_close(const prims_t *p, primid_t pi, const int dim, float *min, float *max);
void prims_get_bounds(const prims_t *p, const primid_t pi, int dim, float *min, float *max);
void prims_get_aabb(const prims_t *p, primid_t pi, float *aabb);
void prims_get_aabb_shutter_close(const prims_t *p, primid_t pi, float *aabb);
void prims_get_shape_aabb(const prims_t *p, uint32_t shapeid, float *aabb);

// area and area sampling
float prims_get_area(const prims_t *p, primid_t pi);
float prims_get_area_time(const prims_t *p, primid_t pi, const float time);
// sample position in area measure on given primitive, p(hit->x) will be the area at this time
void prims_sample(const prims_t *p, primid_t pi, float r1, float r2, hit_t *hit, const float time);
// move the hit position in time. reads hit->{u,v} and writes hit->x and hit->primid.
void prims_retime(const prims_t *p, primid_t pi, hit_t *hit, const float new_time);

// normal and geometric derivatives
void prims_get_normal_time(const prims_t *p, primid_t pi, hit_t *hit, float time);
void prims_get_normal(const prims_t *p, primid_t pi, hit_t *hit);
void prims_init_derivatives(const prims_t *p, primid_t pi, const hit_t *hit, const float time, vertex_manifold_t *mf);

// ray bias things
int prims_offset_ray(const hit_t *hit, ray_t *ray);
float prims_get_ray(const hit_t *hit1, const hit_t *hit2, ray_t *ray);

// ray intersection
void prims_intersect(const prims_t *p, primid_t pi, const ray_t *ray, hit_t *hit);
int prims_intersect_visible(const prims_t *p, primid_t pi, const ray_t *ray, const float max_dist);
#endif

// data management and initialisation
static inline int prims_shader(const prims_t *p, primid_t pi)
{
  return p->shape[pi.shapeid].material;
}
void prims_init(prims_t *p);
void prims_cleanup(prims_t *p);
void prims_allocate(prims_t *p, const uint32_t num_shapes);
void prims_discard_shape(prims_t *p, uint32_t shape);
void prims_allocate_index(prims_t *p);
int prims_load_with_flags(prims_t *p, const char *filename, const char *texture, const int shader, char flags, const char *searchpath);

#if 0
void prims_extend_ghost_aabb(prims_t *p, const float *aabb);

static inline int prims_load(prims_t *p, const char *filename, const char *texture, const int shader)
{
  return prims_load_with_flags(p, filename, texture, shader, 'r', rt.searchpath);
}

static inline void prims_print_info(FILE *fd)
{
  fprintf(fd, "primitive: %ld indexed primitives with motion blur support\n", rt.prims->num_prims);
}
#endif
