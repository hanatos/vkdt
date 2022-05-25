#pragma once

// a module is a user facing macroscopic block of compute
// that potentially consists of several nodes. a wavelet transform
// would for instance iterate on a couple of analysis nodes and then
// resynthesize the data with different nodes.
// a module has one set of user parameters which map to the gui
// and one set that maps to the pipeline/uniform buffers of the node kernels.

// modules and their connections need to be setup quickly from config files
// whereas nodes will be constructed by the module which also manages their
// connections based on the size and roi of the buffers to process.

// none of these functions are thread safe, a graph should only ever be owned
// by one thread.

#include "global.h"
#include "connector.h"

// bare essential meta data that travels
// with a (raw) input buffer along the graph.
// since this data may be tampered with as it is processed
// through the pipeline, every module holds its own instance.
// in the worst case we wastefully copy it around.
typedef struct dt_image_params_t
{
  // from rawspeed:
  float    black[4];          // black point
  float    white[4];          // clipping threshold
  float    whitebalance[4];   // camera white balance coefficients
  uint32_t filters;           // 0-full 9-xtrans else: bayer bits
  uint32_t crop_aabb[4];      // crops away black borders of raw

  // from exiv2:
  float    cam_to_rec2020[9]; // adobe dng matrix or maybe from exif
  uint32_t orientation;       // flip/rotate bits from exif
  char     datetime[20];      // date time string
  char     maker[32];         // camera maker string
  char     model[32];         // camera model string
  float    exposure;          // exposure value as shot
  float    aperture;          // f-stop as shot
  float    iso;               // iso value as shot
  float    focal_length;      // focal length of lens

  // from us:
  float noise_a;              // raw noise estimate, gaussian part
  float noise_b;              // raw noise estimate, poissonian part
}
dt_image_params_t;

// a keyframe param change
typedef struct dt_keyframe_t
{
  dt_token_t param;     // the parameter name
  int        frame;     // the frame to apply this
  uint32_t   beg, end;  // the begin and end byte offsets in the params array
  uint8_t   *data;      // the data to slap over. points into the graph's param pool.
}
dt_keyframe_t;

// this is an instance of a module.
typedef struct dt_module_t
{
  dt_module_so_t *so; // class of module
  dt_token_t name;    // mirrored class name
  dt_token_t inst;    // instance name
  dt_graph_t *graph;  // pointing back to containing graph

  // a list of publicly visible connectors
  dt_connector_t connector[DT_MAX_CONNECTORS];
  int num_connectors;

  // this block of data identifies a few metadata properties of the
  // image, see above for the definition. strictly speaking we'd need one of
  // these per connector. only most connectors aren't in fact related to input
  // image files any more. one important use case, however, is that we'd like
  // to be able to merge multiple raw input files, and change the metadata
  // (such as clipping threshold or black level) on the way. this is why this
  // block here is valid for the *output* of the module, i.e. if the module
  // changes things, it'll write here. to grab the input for a connector, there
  // is an api call to follow the connection and grab the params from the
  // earlier module: dt_module_get_input_img_param()
  dt_image_params_t img_param;

  // parameters:
  // human facing parameters for gui + serialisation
  // compute facing parameters for uniform upload
  // simple code path for when both are equivalent
  uint32_t version;     // module version affects param semantics
  uint8_t *param;       // points into pool stored with graph
  int      param_size;

  uint32_t       keyframe_cnt;  // number of keyframes
  uint64_t       keyframe_size; // allocation size
  dt_keyframe_t *keyframe;      // dynamically allocated keyframe array

  // these stay 0 unless inited by the module in init().
  // if the module implements commit_params(), it shall be used
  // to fill this block, which will then be uploaded as uniform
  // to the kernels.
  uint8_t *committed_param;
  int      committed_param_size;

  uint32_t uniform_offset; // offset into global uniform buffer
  uint32_t uniform_size;   // size of module params padded to 16 byte multiples

  dt_module_flags_t flags; // flags to signal special requests during graph processing

  // this is useful for instance for a cpu caching of
  // input data decoded from disk inside a module:
  void *data; // if you indeed must store your own data.
}
dt_module_t;

typedef struct dt_graph_t dt_graph_t; // fwd declare

// add a module to the graph, also init the dso class. returns the module id or -1.
int dt_module_add(dt_graph_t *graph, dt_token_t name, dt_token_t inst);

int dt_module_get_connector(const dt_module_t *m, dt_token_t conn);

// remove module from the graph
int dt_module_remove(dt_graph_t *graph, const int modid);

// convenience functions for simple cases where the graph degenerates to
// a simple linear chain of "output" being plugged into "input" connectors.
// will return -1 if that's not the case or there's no such connection.
int dt_module_get_module_before(const dt_graph_t *graph, const dt_module_t *us, int *conn);

// note that the get_module_after() case is much more expensive, as it needs to exhaustively
// search the list of modules and will return all connected modules.
// returns the count of filled elements in the given m_out and c_out arrays.
int dt_module_get_module_after(
    const dt_graph_t  *graph,      // the associated graph
    const dt_module_t *us,         // your module
    int               *m_out,      // buffer to store module ids
    int               *c_out,      // buffer to store input connector ids
    int                max_cnt);   // size of the argument buffers
