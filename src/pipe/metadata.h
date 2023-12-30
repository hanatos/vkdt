#pragma once
// metadata to be passed along with the image in the graph.
// this is the diverse part specific to some kind of data,
// extending the generic things stored in dt_image_params_t.
// these structs are allocated and put into the *meta/*next chain
// in the img_param struct in the module by the i-* input
// modules. these also own the memory and are responsible
// to clean up their share in their respective module cleanup
// functions.

typedef struct dt_image_metadata_t
{
  uint32_t type; // type id
  void    *next; // next metadata pointer or null
}
dt_image_metadata_t;

#define s_image_metadata_text 1
typedef struct dt_image_metadata_text_t
{ // generic text attachment
  uint32_t    type; // = s_image_metadata_text
  void       *next;
  const char *text;
}
dt_image_metadata_text_t;

static inline dt_image_metadata_t*
dt_metadata_append(
    dt_image_metadata_t *meta,
    dt_image_metadata_t *add)
{
  add->next = meta;
  return add;
}

static inline void*
dt_metadata_find(
    dt_image_metadata_t *meta,
    uint32_t             type)
{
  while(meta)
  {
    if(meta->type == type) return meta;
    meta = meta->next;
  }
  return 0;
}
