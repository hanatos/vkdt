#pragma once

#include "token.h"
#include "alloc.h"
#include "geo.h"

#include <stdint.h>
#include <vulkan/vulkan.h>

// info about a region of interest.
typedef struct dt_roi_t
{
  uint32_t full_wd, full_ht; // full input size
  uint32_t wd, ht;           // dimensions of scaled region
  float scale;               // scale: wd * scale is on input scale
}
dt_roi_t;

// identifying a connector
typedef struct dt_cid_t
{
  int16_t i; // module or node index on graph
  int16_t c; // connector index on node or module
}
dt_cid_t;
static const dt_cid_t dt_cid_unset = {-1, -1};

typedef enum dt_connector_flags_t
{
  s_conn_none          = 0,
  s_conn_smooth        = 1,  // access this with a bilinear sampler during read
  s_conn_clear         = 2,  // clear this to zero before writing
  s_conn_feedback      = 4,  // this connection is only in between frames (written frame 1 and read frame 2)
  s_conn_dynamic_array = 8,  // dynamically allocated array connector, contents can change during animation
  s_conn_protected     = 16, // flag this connector for rewrites/accumulation/don't overwrite with other buffers
  s_conn_double_buffer = 32, // this connector is double-buffered for async compute/display
  s_conn_mipmap        = 64, // signifies we will build mipmaps
  s_conn_clear_once    = 128,// clear only on frame 0
}
dt_connector_flags_t;

#if 0
// TODO: gcc can't do this either.
// TODO: so we probably want to generate these in a makefile,
// TODO: something like = 0x$(echo -n "ui16" | xxd -p -e | cut -f2 -d" ")ul
// this seems to work:
//  echo "cat << EOF" > test2.sh
//  cat test.h | sed 's/dt_token("\(.*\)")/0x$(echo -n "\1" | xxd -p -e | tr -s " " | cut -f2 -d" ")ul/g' >> test2.sh
//  echo "EOF" >> test2.sh
//  . test2.sh
//  rm test2.sh
typedef enum dt_connector_format_t
{
  s_format_f32  = dt_token("f32"),
  s_format_f16  = dt_token("f16"),
  s_format_ui32 = dt_token("ui32"),
  s_format_ui16 = dt_token("ui16"),
  s_format_ui8  = dt_token("ui8"),
}
dt_connector_format_t;
#endif

// shared property of nodes and modules: how many connectors do we allocate at
// max for each one of them:
#define DT_MAX_CONNECTORS 30

// connectors are used for modules as well as for nodes.
// modules:
// these have to be setup very quickly using tokens from a file
// these are only meta connections, the finest layer DAG will connect nodes
// with memory allocations for vulkan etc.
typedef struct dt_connector_t
{
  dt_token_t name;   // connector name
  dt_token_t type;   // read write source sink modify
  dt_token_t chan;   // rgb yuv.. or ssbo for storage buffers instead of images
  dt_token_t format; // f32 ui16

  dt_connector_flags_t flags;

  // outputs (write buffers) can be connected to multiple inputs
  // inputs (read buffers) can only be connected to exactly one output
  // we only keep track of where inputs come from. this is also
  // how we'll access it in the DAG during DFS from sinks.
  // XXX is a reference count for "write"|"source" buffers (these allocate/own a buffer)
  // XXX yet it should not. put ref count on conn_image?
  dt_cid_t connected;  // for inputs ("read"|"sink"|"modify"): pointing to allocator/owner/beginning of "modify" chain
  dt_cid_t associated; // for *nodes*, points back to module layer if repointing is needed (dt_connector_copy interface)
  dt_cid_t bypass;     // set on input and output which form a tunnel, bypassing *modules*

  // information about buffer dimensions transported here:
  dt_roi_t roi;
  int max_wd, max_ht; // if > 0 will be used to clamp roi of sinks which don't implement their own roi callbacks.

  // if the output/write connector holds an array and the entries have different size:
  uint32_t     *array_dim;        // or 0 if all have the same size of the roi
  dt_vkalloc_t *array_alloc;      // or 0 if not needed. if the connector is flags & s_conn_dynamic_array use this.
  size_t        array_alloc_size; // size of the pool allocated for this dynamic texture array
  dt_vkmem_t   *array_mem;        // memory allocated in outer mem pool, will be split for array dynamically
  uint8_t      *array_req;        // points to flags what do do: 0 - nothing, 1 - alloc, 2 - upload, 4 - free
  int           array_resync;     // means we need to sync a dynamic array for multi-frame descriptor sets with one frame delay (internal use)

  // buffer associated with this in case it connects nodes:
  uint64_t offset_staging[2], size_staging;
  // mem object for allocator:
  // while this may seem duplicate with offset/size, it may be freed already
  // and the offset and size are still valid for successive runs through the
  // pipeline once it has been setup.
  dt_vkmem_t *mem_staging;

  // connectors can hold arrays of images, to facilitate more parallel
  // processing for similar compute on small buffers.
  // arrays are unsupported for: clearing, drawing, sources, and sinks.
  int array_length;
  // connectors can write to double-buffered output depending on animation frames.
  // in the simplest case this is useful for sinks to double-buffer their data
  // so drawing and computing can take place asynchronously.
  int frames;
  // a node's connector consumes array_length * frames buffers in the graph's
  // conn_image[] pool. the location can be determined by dt_graph_connector_image()
  // and makes use of the node->conn_image index list.

  VkBuffer      staging[2];  // for sources and sinks, potentially double buffered
  uint64_t      ssbo_offset; // usually zero, offset into the ssbo backing when binding descriptors

  const char   *tooltip;     // tooltip extracted from docs
}
dt_connector_t;

// "templatised" connection functions for both modules and nodes
typedef struct dt_graph_t dt_graph_t; // fwd declare
// connect source|write (m0,c0) -> sink|read (m1,c1)
#ifndef VKDT_DSO_BUILD
VKDT_API int dt_module_connect (dt_graph_t *graph, int m0, int c0, int m1, int c1);
VKDT_API int dt_module_feedback(dt_graph_t *graph, int m0, int c0, int m1, int c1);
VKDT_API int dt_node_connect (dt_graph_t *graph, int n0, int c0, int n1, int c1);
VKDT_API int dt_node_feedback(dt_graph_t *graph, int n0, int c0, int n1, int c1);
#endif

static inline int
dt_connected(const dt_connector_t *c)
{
  if(c->type == dt_token("read") || c->type == dt_token("sink"))
    return c->connected_mi >= 0 && c->connected_mc >= 0; // input have specific id
  else
    return c->connected_mi > 0; // outputs hold reference counts
}

static inline const char*
dt_connector_error_str(const int err)
{
  switch(err)
  {
    case  1: return "no such destination node";
    case  2: return "no such destination connector";
    case  3: return "destination does not read";
    case  4: return "destination inconsistent";
    case  5: return "destination inconsistent";
    case  6: return "destination inconsistent";
    case  7: return "no such source node";
    case  8: return "no such source connector";
    case  9: return "source does not write";
    case 10: return "channels do not match";
    case 11: return "format does not match";
    case 12: return "connection would be cyclic";
    case -100: return "named source connector does not exist";
    case -101: return "named destination connector does not exist";
    default: return "";
  }
}

#ifdef NDEBUG
#define CONN(A) (A)
#else
#define CONN(A) \
do { \
  int err = (A); \
  if(err) fprintf(stderr, "%s:%d connection failed: %s\n", __FILE__, __LINE__, dt_connector_error_str(err)); \
} while(0)
#endif

static inline size_t
dt_connector_bytes_per_channel(const dt_connector_t *c)
{
  if(c->format == dt_token("ui32")) return 4;
  if(c->format == dt_token("u32"))  return 4;
  if(c->format == dt_token("f32"))  return 4;
  if(c->format == dt_token("atom")) return 4; // evaluates to ui32 if no float atomics are supported, f32 otherwise
  if(c->format == dt_token("dspy")) return 4;
  if(c->format == dt_token("ui16")) return 2;
  if(c->format == dt_token("u16"))  return 2;
  if(c->format == dt_token("f16"))  return 2;
  if(c->format == dt_token("ui8"))  return 1;
  if(c->format == dt_token("u8"))   return 1;
  if(c->format == dt_token("tri"))  return sizeof(geo_tri_t);
  return 0;
}

static inline int
dt_connector_channels(const dt_connector_t *c)
{
  if(c->chan == dt_token("ssbo")) return 1; // shader storage buffers have no channels
  // bayer or x-trans?
  if(c->chan == dt_token("rggb") || c->chan == dt_token("rgbx")) return 1;
  return c->chan <=     0xff ? 1 :
        (c->chan <=   0xffff ? 2 :
         4);
        // (c->chan <= 0xffffff ? 3 : 4)); // this is mostly padded (intel)
}

static inline size_t
dt_connector_bufsize(const dt_connector_t *c, uint32_t wd, uint32_t ht)
{
  if(c->format == dt_token("bc1")) return wd/4*(uint64_t)ht/4 * (uint64_t)8;
  if(c->format == dt_token("yuv")) return wd*(uint64_t)ht * (uint64_t)2;
  const int numc = dt_connector_channels(c);
  const size_t bpp = dt_connector_bytes_per_channel(c);
  return numc * bpp * wd * ht > 0 ? numc * bpp * wd * ht : 16;
}
