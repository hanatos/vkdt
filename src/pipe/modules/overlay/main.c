#include "modules/api.h"
#include "core/half.h"
#include "core/lut.h"
#include "gui/font_metrics.h"

#if 1 // nuklear.h, just the utf8 decoding
#define NK_LEN(a) (sizeof(a)/sizeof(a)[0])
#define NK_BETWEEN(x, a, b) ((a) <= (x) && (x) < (b))
#define NK_UTF_INVALID 0xFFFD
#define NK_UTF_SIZE 4
const uint8_t  nk_utfbyte[NK_UTF_SIZE+1] = {0x80, 0, 0xC0, 0xE0, 0xF0};
const uint8_t  nk_utfmask[NK_UTF_SIZE+1] = {0xC0, 0x80, 0xE0, 0xF0, 0xF8};
const uint32_t nk_utfmin [NK_UTF_SIZE+1] = {0, 0, 0x80, 0x800, 0x10000};
const uint32_t nk_utfmax [NK_UTF_SIZE+1] = {0x10FFFF, 0x7F, 0x7FF, 0xFFFF, 0x10FFFF};
static inline int
nk_utf_validate(uint32_t *u, int i)
{
  if (!u) return 0;
  if (!NK_BETWEEN(*u, nk_utfmin[i], nk_utfmax[i]) ||
      NK_BETWEEN(*u, 0xD800, 0xDFFF))
    *u = NK_UTF_INVALID;
  for (i = 1; *u > nk_utfmax[i]; ++i);
  return i;
}
static inline uint32_t
nk_utf_decode_byte(char c, int *i)
{
  if (!i) return 0;
  for(*i = 0; *i < (int)NK_LEN(nk_utfmask); ++(*i)) {
    if (((uint8_t)c & nk_utfmask[*i]) == nk_utfbyte[*i])
      return (uint8_t)(c & ~nk_utfmask[*i]);
  }
  return 0;
}
static inline int
nk_utf_decode(const char *c, uint32_t *u, int clen)
{
  int i, j, len, type=0;
  uint32_t udecoded;

  if (!c || !u) return 0;
  if (!clen) return 0;
  *u = NK_UTF_INVALID;

  udecoded = nk_utf_decode_byte(c[0], &len);
  if (!NK_BETWEEN(len, 1, NK_UTF_SIZE))
    return 1;

  for (i = 1, j = 1; i < clen && j < len; ++i, ++j) {
    udecoded = (udecoded << 6) | nk_utf_decode_byte(c[i], &type);
    if (type != 0)
      return j;
  }
  if (j < len)
    return 0;
  *u = udecoded;
  nk_utf_validate(u, len);
  return len;
}
int
nk_utf_len(const char *str, int len)
{
    const char *text;
    int glyphs = 0;
    int text_len;
    int glyph_len;
    int src_len = 0;
    uint32_t unicode;

    if (!str || !len) return 0;

    text = str;
    text_len = len;
    glyph_len = nk_utf_decode(text, &unicode, text_len);
    while (glyph_len && src_len < len) {
        glyphs++;
        src_len = src_len + glyph_len;
        glyph_len = nk_utf_decode(text + src_len, &unicode, text_len - src_len);
    }
    return glyphs;
}
#endif

typedef struct overlay_t
{
  dt_font_t       font;
  dt_lut_header_t font_hdr;
  uint8_t        *font_img;
  char            filename[PATH_MAX];

  char textl[256], textr[256];
  int  textl_len,  textr_len;
  // TODO position
} overlay_t;

int init(dt_module_t *mod)
{
  overlay_t *ov = malloc(sizeof(overlay_t));
  memset(ov, 0, sizeof(*ov));
  dt_font_init(&ov->font);
  mod->data = ov;
  return 0;
}

void cleanup(dt_module_t *mod)
{
  overlay_t *ov = mod->data;
  if(ov)
  {
    dt_font_cleanup(&ov->font);
    free(ov->font_img);
  }
  free(mod->data);
  mod->data = 0;
}

#if 0
dt_graph_run_t
check_params(
    dt_module_t *module,
    uint32_t     parid,
    uint32_t     num,
    void        *oldval)
{
}
#endif

void commit_params(dt_graph_t *graph, dt_module_t *module)
{
  overlay_t *ov = module->data;
  // TODO replace text! overwrite text_len with utf-8 length!
  const int pid_txtl = dt_module_get_param(module->so, dt_token("text l"));
  const char *p_txtl = dt_module_param_string(module, pid_txtl);
  const int pid_txtr = dt_module_get_param(module->so, dt_token("text r"));
  const char *p_txtr = dt_module_param_string(module, pid_txtr);
  snprintf(ov->textl, sizeof(ov->textl), "%s", p_txtl);
  snprintf(ov->textr, sizeof(ov->textr), "%s", p_txtr);
  ov->textl_len = nk_utf_len(ov->textl, strlen(ov->textl));
  ov->textr_len = nk_utf_len(ov->textr, strlen(ov->textr));
  for(int n=0;n<graph->num_nodes;n++)
  {
    if(graph->node[n].name == dt_token("overlay") &&
       graph->node[n].kernel == dt_token("main") &&
       graph->node[n].module->inst == module->inst)
    {
      graph->node[n].vtx_cnt = 6*(ov->textl_len + ov->textr_len);
      break;
    }
  }
}

void modify_roi_out(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  module->flags |= s_module_request_read_source;
  module->connector[0].roi = (dt_roi_t){ .full_wd = 1024, .full_ht = 1024, .marker = s_roi_mark_dontcare };
  overlay_t *ov = module->data;
  const int pid_font = dt_module_get_param(module->so, dt_token("font"));
  const char *p_font = dt_module_param_string(module, pid_font);

  // check cache hash/filename on ov->filename!
  if(!strcmp(p_font, ov->filename)) return; // already loaded
  if(ov->filename[0])
  { // something else loaded: cleanup allocated image and font file stuff
    dt_font_cleanup(&ov->font);
    free(ov->font_img);
    memset(ov, 0, sizeof(*ov));
  }
  snprintf(ov->filename, sizeof(ov->filename), "%s", p_font);

  char tmp[PATH_MAX] = {0};
  snprintf(tmp, sizeof(tmp), "%s_msdf.lut", p_font);
  FILE *f = dt_graph_open_resource(graph, 0, tmp, "rb");
  if(!f)
  {
    // TODO error message
    fprintf(stderr, "[overlay] could not find msdf font lut `%s'!\n", tmp);
    return;
  }
  else
  {
    if(fread(&ov->font_hdr, sizeof(dt_lut_header_t), 1, f) != 1 ||
        ov->font_hdr.version < dt_lut_header_version ||
        ov->font_hdr.channels != 3 ||
        ov->font_hdr.datatype != dt_lut_header_ui8)
    {
      // TODO error message
      fprintf(stderr, "[overlay] could not load msdf font lut from `%s'!\n", tmp);
      fclose(f);
    }
    else
    {
      ov->font_img = malloc(sizeof(uint8_t)*4*ov->font_hdr.wd*ov->font_hdr.ht);
      for(int k=0;k<ov->font_hdr.wd*ov->font_hdr.ht;k++)
      {
        fread(ov->font_img+4*k, sizeof(uint8_t), 3, f);
        ov->font_img[4*k+3] = 0xff;
      }
      fclose(f);
    }
  }

  snprintf(tmp, sizeof(tmp), "%s_metrics.lut", p_font);
  f = dt_graph_open_resource(graph, 0, tmp, "rb");
  if(!f)
  {
    // TODO error gui message
    fprintf(stderr, "[overlay] could not find font metrics lut `%s'!\n", tmp);
    return;
  }
  dt_font_read(&ov->font, f);
  ov->font.default_glyph = dt_font_glyph(&ov->font, '*');
  if(f) fclose(f);
}

typedef struct vtx_t
{
  uint16_t x, y;
  uint16_t u, v;
  uint8_t r,g,b,a;
}
vtx_t;

static inline vtx_t genv(
    float gx, float gy, float u, float v, uint8_t col[4])
{
  return (vtx_t) {
        .x = float_to_half(gx),
        .y = float_to_half(gy),
        .u = float_to_half(u),
        .v = float_to_half(v),
        .r = col[0], .g = col[1], .b = col[2], .a = col[3],
  };
}

int read_source(
    dt_module_t             *mod,
    void                    *mapped,
    dt_read_source_params_t *p)
{
  overlay_t *ov = mod->data;
  if(p->node->kernel == dt_token("font"))
  { // copy over msdf font texture
    memcpy(mapped, ov->font_img, sizeof(uint8_t)*4*ov->font_hdr.wd*ov->font_hdr.ht);
  }
  else if(p->node->kernel == dt_token("vtx"))
  { // create vtx draw list
    vtx_t *vert = mapped;
    uint8_t col[] = {0xff, 0xff, 0xff, 0xff}; // TODO
    int vidx = 0;
    const float font_height = 0.1f;
    for(int i=0;i<2;i++)
    {
      uint32_t unicode, next;
      int text_len = 0, len = strlen(i ? ov->textr : ov->textl);
      len = MIN(len, i ? ov->textr_len : ov->textl_len);
      len = MIN(len, i ? sizeof(ov->textr) : sizeof(ov->textl));
      float x = i ? 0.9f-len*font_height*0.333 : -0.9f; // positioning: 0,0 is center of image
      float y = -0.8f;
      int glyph_len = nk_utf_decode(i ? ov->textr : ov->textl, &unicode, len);
      while(text_len < len && glyph_len)
      {
        if (unicode == NK_UTF_INVALID) break;
        int next_glyph_len = nk_utf_decode(
            (i ? ov->textr : ov->textl) + text_len + glyph_len, &next, len - text_len);
        dt_font_glyph_t *gg = ov->font.glyph + dt_font_glyph(&ov->font, unicode);
        float scale = gg->height_to_em * font_height;

        float gx = x + scale * gg->pbox_x;
        float gy = y + scale + scale * gg->pbox_y;
        float gw = scale * gg->pbox_w, gh = scale * gg->pbox_h;
        float char_width = scale * gg->advance;
        float u0 = gg->tbox_x, v0 = gg->tbox_y, u1 = u0 + gg->tbox_w, v1 = v0 + gg->tbox_h;
        // push 6 vertices/2 triangles
        vert[vidx++] = genv(gx,    gy,    u0, v0, col);
        vert[vidx++] = genv(gx+gw, gy,    u1, v0, col);
        vert[vidx++] = genv(gx+gw, gy+gh, u1, v1, col);
        vert[vidx++] = genv(gx,    gy,    u0, v0, col);
        vert[vidx++] = genv(gx+gw, gy+gh, u1, v1, col);
        vert[vidx++] = genv(gx   , gy+gh, u0, v1, col);
        if(vidx >= p->node->connector[0].roi.wd) break;

        text_len += glyph_len;
        x += char_width;
        glyph_len = next_glyph_len;
        unicode = next;
      }
    }
  }
  return 0;
}

void
create_nodes(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  overlay_t *ov = module->data;
  const int wd = module->connector[0].roi.wd;
  const int ht = module->connector[0].roi.ht;
  dt_roi_t roi_vtx  = { .wd = 2*sizeof(ov->textl), .ht = 6*sizeof(vtx_t) };
  dt_roi_t roi_font = { .wd = ov->font_hdr.wd,     .ht = ov->font_hdr.ht };
  const int id_font = dt_node_add(graph, module, "overlay", "font", 1, 1, 1, 0, 0, 1,
      "font", "source", "rgba", "ui8", &roi_font);
  const int id_vtx  = dt_node_add(graph, module, "overlay", "vtx", 1, 1, 1, 0, 0, 1,
      "vtx", "source", "ssbo", "ui8", &roi_vtx);
  const int id_overlay = dt_node_add(graph, module, "overlay", "main", wd, ht, 1,
      0, 0, 3,
      "vtx",    "read",  "ssbo", "*",    dt_no_roi,
      "font",   "read",  "*",    "*",    dt_no_roi,
      "output", "write", "rgba", "f16", &module->connector[0].roi);
  graph->node[id_overlay].type = s_node_graphics; // mark for rasterisation via vert/frag shaders
  CONN(dt_node_connect_named(graph, id_vtx,  "vtx",  id_overlay, "vtx"));
  CONN(dt_node_connect_named(graph, id_font, "font", id_overlay, "font"));
  dt_connector_copy(graph, module, 0, id_overlay, 2);
}
