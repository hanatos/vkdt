#include "modules/api.h"
#include "jpegexiforient.h"

#include <jpeglib.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <setjmp.h>

typedef struct jpginput_buf_t
{
  char filename[PATH_MAX];
  char errormsg[256];
  uint32_t frame;
  uint32_t width, height;
  struct jpeg_decompress_struct dinfo;
  FILE *f;
}
jpginput_buf_t;

typedef struct jpgerr_t
{
  struct jpeg_error_mgr pub;
  jmp_buf setjmp_buffer;
}
jpgerr_t;

static void
error_exit(j_common_ptr cinfo)
{
  jpgerr_t *myerr = (jpgerr_t *)cinfo->err;
  (*cinfo->err->output_message)(cinfo);
  longjmp(myerr->setjmp_buffer, 1);
}

/*
 * Since an ICC profile can be larger than the maximum size of a JPEG marker
 * (64K), we need provisions to split it into multiple markers.  The format
 * defined by the ICC specifies one or more APP2 markers containing the
 * following data:
 *  Identifying string  ASCII "ICC_PROFILE\0"  (12 bytes)
 *  Marker sequence number  1 for first APP2, 2 for next, etc (1 byte)
 *  Number of markers Total number of APP2's used (1 byte)
 *      Profile data    (remainder of APP2 data)
 * Decoders should use the marker sequence numbers to reassemble the profile,
 * rather than assuming that the APP2 markers appear in the correct sequence.
 */

#define EXIF_MARKER (JPEG_APP0 + 1) /* JPEG marker code for Exif */
#define ICC_MARKER (JPEG_APP0 + 2)  /* JPEG marker code for ICC */
#define ICC_OVERHEAD_LEN 14         /* size of non-profile data in APP2 */
#define MAX_BYTES_IN_MARKER 65533   /* maximum data len of a JPEG marker */
#define MAX_DATA_BYTES_IN_MARKER (MAX_BYTES_IN_MARKER - ICC_OVERHEAD_LEN)

typedef struct icc_tag_t
{
  uint32_t sig;
  uint32_t off;
  uint32_t size;
}
icc_tag_t;

typedef struct icc_header_t
{
  uint32_t size;
  uint32_t type;
  uint32_t version;
  uint32_t class;
  uint32_t colour_space;
  uint32_t pcs;
  uint32_t stuff0[11];    // bytes 24..67 = 44b = 11 ints
  uint32_t illuminant[3]; // bytes 68..79
  uint32_t stuff1[12];    // 80..127
  uint32_t tag_cnt;
  icc_tag_t tag[];
}
icc_header_t;

static uint32_t le(uint32_t be)
{
  be =  (be >> 16) | (be << 16);
  be = ((be >> 8)&0xff00ffu) | ((be << 8)&0xff00ff00u);
  return be;
}

static float read_s15u16(const icc_header_t *h, int beoff, int k)
{
  uint8_t *data = ((uint8_t*)h)+le(beoff)+8+k*4;
  uint32_t val = le(*(uint32_t*)data);
  int16_t  v1 = val>>16;
  uint16_t v0 = val&0xffff;
  return v1 + v0/65536.0;
}

static float read_gamma(const icc_header_t *h, int beoff)
{
  uint8_t *data = ((uint8_t*)h)+le(beoff)+8;
  uint32_t cnt = le(*(uint32_t*)data); //cnt==0 means linear, ==1 means gamma, and the rest we don't.
  if(cnt == 0) return 0.0;
  if(cnt == 1) return data[4] + data[5]/256.0;
  return 0.0; // unsupported, let's pretend it's linear
}

static int marker_is_icc(jpeg_saved_marker_ptr marker)
{
  return marker->marker == ICC_MARKER && marker->data_length >= ICC_OVERHEAD_LEN
         && !strcmp((char*)marker->data, "ICC_PROFILE");
}
/*
 * icc reading stuff stolen from darktable/imageio, so this code is GPLv3
 * See if there was an ICC profile in the JPEG file being read;
 * if so, reassemble and return the profile data.
 *
 * TRUE is returned if an ICC profile was found, FALSE if not.
 * If TRUE is returned, *icc_data_ptr is set to point to the
 * returned data, and *icc_data_len is set to its length.
 *
 * IMPORTANT: the data at **icc_data_ptr has been allocated with malloc()
 * and must be freed by the caller with free() when the caller no longer
 * needs it.  (Alternatively, we could write this routine to use the
 * IJG library's memory allocator, so that the data would be freed implicitly
 * at jpeg_finish_decompress() time.  But it seems likely that many apps
 * will prefer to have the data stick around after decompression finishes.)
 *
 * NOTE: if the file contains invalid ICC APP2 markers, we just silently
 * return FALSE.  You might want to issue an error message instead.
 */
static icc_header_t*
read_icc_profile(
    const j_decompress_ptr dinfo,
    unsigned int *icc_data_len)
{
  jpeg_saved_marker_ptr marker;
  int num_markers = 0;
  int seq_no;
  JOCTET *icc_data;
  unsigned int total_length;
#define MAX_SEQ_NO 255                      /* sufficient since marker numbers are bytes */
  char marker_present[MAX_SEQ_NO + 1];      /* 1 if marker found */
  unsigned int data_length[MAX_SEQ_NO + 1]; /* size of profile data in marker */
  unsigned int data_offset[MAX_SEQ_NO + 1]; /* offset for data in marker */

  *icc_data_len = 0;

  /* This first pass over the saved markers discovers whether there are
   * any ICC markers and verifies the consistency of the marker numbering.
   */

  for(seq_no = 1; seq_no <= MAX_SEQ_NO; seq_no++) marker_present[seq_no] = 0;

  for(marker = dinfo->marker_list; marker != NULL; marker = marker->next)
  {
    if(marker_is_icc(marker))
    {
      if(num_markers == 0)
        num_markers = GETJOCTET(marker->data[13]);
      else if(num_markers != GETJOCTET(marker->data[13]))
        return 0; /* inconsistent num_markers fields */
      seq_no = GETJOCTET(marker->data[12]);
      if(seq_no <= 0 || seq_no > num_markers) return 0; /* bogus sequence number */
      if(marker_present[seq_no]) return 0;              /* duplicate sequence numbers */
      marker_present[seq_no] = 1;
      data_length[seq_no] = marker->data_length - ICC_OVERHEAD_LEN;
    }
  }

  if(num_markers == 0) return 0;

  /* Check for missing markers, count total space needed,
   * compute offset of each marker's part of the data.
   */

  total_length = 0;
  for(seq_no = 1; seq_no <= num_markers; seq_no++)
  {
    if(marker_present[seq_no] == 0) return 0; /* missing sequence number */
    data_offset[seq_no] = total_length;
    total_length += data_length[seq_no];
  }

  if(total_length == 0) return 0; /* found only empty markers? */

  /* Allocate space for assembled data */
  icc_data = (JOCTET *)malloc(total_length * sizeof(JOCTET));

  /* and fill it in */
  for(marker = dinfo->marker_list; marker != NULL; marker = marker->next)
  {
    if(marker_is_icc(marker))
    {
      JOCTET FAR *src_ptr;
      JOCTET *dst_ptr;
      unsigned int length;
      seq_no = GETJOCTET(marker->data[12]);
      dst_ptr = icc_data + data_offset[seq_no];
      src_ptr = marker->data + ICC_OVERHEAD_LEN;
      length = data_length[seq_no];
      while(length--) *dst_ptr++ = *src_ptr++;
    }
  }

  *icc_data_len = total_length;
  return (icc_header_t *)icc_data;
}

static int 
read_header(
    dt_module_t *mod,
    uint32_t    frame,
    const char *filename)
{
  jpginput_buf_t *jpg = mod->data;
  if(jpg && !strcmp(jpg->filename, filename) && jpg->frame == frame)
    return 0; // already loaded
  assert(jpg); // this should be inited in init()

  if(jpg->f)
  {
    fclose(jpg->f);
    jpeg_destroy_decompress(&(jpg->dinfo));
    jpg->f = 0;
  }
  jpg->f = dt_graph_open_resource(mod->graph, frame, filename, "rb");
  if(!jpg->f)
  {
    jpg->filename[0] = 0;
    snprintf(jpg->errormsg, sizeof(jpg->errormsg), "i-jpg:%"PRItkn" could not open file `%s'!", dt_token_str(mod->inst), filename);
    mod->graph->gui_msg = jpg->errormsg;
    return 1;
  }

  jpgerr_t err;
  jpg->dinfo.err = jpeg_std_error(&err.pub);
  err.pub.error_exit = error_exit;
  if(setjmp(err.setjmp_buffer))
  {
    jpeg_destroy_decompress(&(jpg->dinfo));
    fclose(jpg->f);
    jpg->f = 0;
    jpg->filename[0] = 0;
    snprintf(jpg->errormsg, sizeof(jpg->errormsg), "i-jpg:%"PRItkn" failed to decode jpeg `%s'!", dt_token_str(mod->inst), filename);
    mod->graph->gui_msg = jpg->errormsg;
    return 1;
  }
  jpeg_create_decompress(&(jpg->dinfo));
  jpeg_stdio_src(&(jpg->dinfo), jpg->f);
  // setup_read_exif(&(jpg->dinfo));
  jpeg_save_markers(&(jpg->dinfo), ICC_MARKER, 0xFFFF);
  // jpg->dinfo.buffered_image = TRUE;
  jpeg_read_header(&(jpg->dinfo), TRUE);
  jpg->dinfo.out_color_space = JCS_RGB;
  jpg->dinfo.out_color_components = 3;
  jpg->width  = jpg->dinfo.image_width;
  jpg->height = jpg->dinfo.image_height;

  for(int k=0;k<4;k++)
  {
    mod->img_param.black[k]        = 0.0f;
    mod->img_param.white[k]        = 1.0f;
    mod->img_param.whitebalance[k] = 1.0f;
  }
  mod->img_param.filters = 0;
  mod->img_param.colour_primaries = s_colour_primaries_srgb;
  mod->img_param.colour_trc       = s_colour_trc_srgb;

  FILE *f2 = dt_graph_open_resource(mod->graph, frame, filename, "rb");
  mod->img_param.orientation = jpg_read_orientation(f2);

  // read embedded icc if any, and load matrix. also set primaries to custom.
  // we don't really support icc, like at all. the only thing we do is read a
  // matrix + single gamma for all three colours (or linear).
  unsigned int length = 0;
  icc_header_t *h = read_icc_profile(&(jpg->dinfo), &length);
  if(h)
  {
    const int tag_cnt = le(h->tag_cnt);
    float gamma = 0.0, wt[3] = {0.0}, M[9] = {0.0};  // image rgb to xyz
    mod->img_param.colour_primaries = s_colour_primaries_custom;
    for(int i=0;i<tag_cnt;i++)
    {
      icc_tag_t *entry = h->tag + i;
      if     (entry->sig == *((int*)"wtpt")) for(int k=0;k<3;k++) wt[k]    = read_s15u16(h, entry->off, k);
      else if(entry->sig == *((int*)"rXYZ")) for(int k=0;k<3;k++) M[3*k+0] = read_s15u16(h, entry->off, k);
      else if(entry->sig == *((int*)"gXYZ")) for(int k=0;k<3;k++) M[3*k+1] = read_s15u16(h, entry->off, k);
      else if(entry->sig == *((int*)"bXYZ")) for(int k=0;k<3;k++) M[3*k+2] = read_s15u16(h, entry->off, k);
      else if(entry->sig == *((int*)"rTRC")) gamma = read_gamma(h, entry->off);
      else if(entry->sig == *((int*)"gTRC")) gamma = read_gamma(h, entry->off);
      else if(entry->sig == *((int*)"bTRC")) gamma = read_gamma(h, entry->off);
    }
    // stay with srgb in case gamma is zero
    if(gamma > 0.0) mod->img_param.colour_trc = s_colour_trc_gamma;
    // bradford adapt stupid matrix: xyz_to_rec2020 * Bi * S * B * M
    // where B is the bradford matrix and S a diagonal matrix adapting from wtpt to D50 in the icc XYZ
    float B[9]  = { 0.8951000,   0.2664000, -0.1614000,
                   -0.7502000,   1.7135000,  0.0367000,
                    0.0389000,  -0.0685000,  1.0296000};
    float D50[] = {0.99628443, 1.02042736, 0.81864437}; // D50 white in bradford/lms space
    float wtp[3] = {0.0}; // = B * wt
    for(int j=0;j<3;j++) for(int i=0;i<3;i++) wtp[j] += B[3*j+i] * wt[i];
    for(int j=0;j<3;j++) for(int i=0;i<3;i++) B[3*j+i] *= wtp[j]/D50[j]; // diagonal matrix affects rows of matrix to the right
    float B_to_rec2020[9] = { // from bradford lms to rec2020 directly
       1.54272507, -0.44695197,  0.01168675,
       0.04066615,  0.93658984, -0.01169462,
      -0.00911443,  0.01295986,  0.91312784};
    float T[9] = {0.0}; // = S * B * M
    for(int j=0;j<3;j++) for(int i=0;i<3;i++) for(int k=0;k<3;k++) T[3*j+i] += B[3*j+k] * M[3*k+i];
    for(int j=0;j<3;j++) for(int i=0;i<3;i++) mod->img_param.cam_to_rec2020[3*j+i] = 0.0; // = (xyz_to_rec2020 * Bi) * (S * B * M)
    for(int j=0;j<3;j++) for(int i=0;i<3;i++) for(int k=0;k<3;k++) mod->img_param.cam_to_rec2020[3*j+i] += B_to_rec2020[3*j+k] * T[3*k+i];
    free(h);
  }
  else mod->img_param.cam_to_rec2020[0] = 0.0f/0.0f; // mark as uninitialised

  snprintf(jpg->filename, sizeof(jpg->filename), "%s", filename);
  jpg->frame = frame;
  return 0;
}

static int
read_plain(
    jpginput_buf_t *jpg, uint8_t *out)
{
  JSAMPROW row_pointer[1];
  int ac = jpg->dinfo.out_color_components;
  row_pointer[0] = malloc(jpg->dinfo.output_width * (uint64_t)ac);
  uint8_t *tmp = out;
  while(jpg->dinfo.output_scanline < jpg->dinfo.image_height)
  {
    if(jpeg_read_scanlines(&(jpg->dinfo), row_pointer, 1) != 1)
    {
      jpeg_destroy_decompress(&(jpg->dinfo));
      free(row_pointer[0]);
      fclose(jpg->f);
      jpg->f = 0;
      jpg->filename[0] = 0;
      return 1;
    }
    for(unsigned int i = 0; i < jpg->dinfo.image_width; i++)
    {
      for(int k = 0; k < 3; k++) tmp[4 * i + k] = row_pointer[0][ac * i + MIN(k,ac-1)];
      tmp[4*i+3] = 255;
    }
    tmp += 4 * jpg->width;
  }
  free(row_pointer[0]);
  return 0;
}

static int
jpeg_read(
    jpginput_buf_t *jpg, uint8_t *out)
{
  jpgerr_t err;
  jpg->dinfo.err = jpeg_std_error(&err.pub);
  err.pub.error_exit = error_exit;
  if(setjmp(err.setjmp_buffer))
  {
    jpeg_destroy_decompress(&(jpg->dinfo));
    fclose(jpg->f);
    jpg->f = 0;
    jpg->filename[0] = 0;
    return 1;
  }

  (void)jpeg_start_decompress(&(jpg->dinfo));
  read_plain(jpg, out);
  (void)jpeg_finish_decompress(&(jpg->dinfo));
  // i think libjpeg doesn't want us to retain the state, at least not the way
  // by splitting here. so we'll just clean it all up:
  jpeg_destroy_decompress(&(jpg->dinfo));
  fclose(jpg->f);
  jpg->f = 0;
  jpg->filename[0] = 0;
  return 0;
}

int init(dt_module_t *mod)
{
  jpginput_buf_t *dat = malloc(sizeof(*dat));
  memset(dat, 0, sizeof(*dat));
  mod->data = dat;
  return 0;
}

void cleanup(dt_module_t *mod)
{
  if(!mod->data) return;
  jpginput_buf_t *jpg = mod->data;
  jpeg_destroy_decompress(&(jpg->dinfo));
  if(jpg->filename[0])
  {
    if(jpg->f) fclose(jpg->f);
    jpg->filename[0] = 0;
  }
  free(jpg);
  mod->data = 0;
}

dt_graph_run_t
check_params(
    dt_module_t *module,
    uint32_t     parid,
    uint32_t     num,
    void        *oldval)
{
  if(parid == 2 || parid == 3) // colour space
  {
    const int prim = dt_module_param_int(module, 2)[0];
    const int trc  = dt_module_param_int(module, 3)[0];
    module->img_param.colour_primaries = prim;
    module->img_param.colour_trc       = trc;
    return s_graph_run_all; // propagate image params and potentially different buffer sizes downwards
  }
  return s_graph_run_record_cmd_buf;
}

// this callback is responsible to set the full_{wd,ht} dimensions on the
// regions of interest on all "write"|"source" channels
void modify_roi_out(
    dt_graph_t  *graph,
    dt_module_t *mod)
{
  // load image if not happened yet
  const int   id       = dt_module_param_int(mod, 1)[0];
  const char *filename = dt_module_param_string(mod, 0);
  if(strstr(filename, "%")) // reading a sequence of raws as a timelapse animation
    mod->flags = s_module_request_read_source;
  if(read_header(mod, id+graph->frame, filename)) return;
  jpginput_buf_t *jpg = mod->data;
  mod->connector[0].roi.full_wd = jpg->width;
  mod->connector[0].roi.full_ht = jpg->height;

  int *p_prim = (int*)dt_module_param_int(mod, dt_module_get_param(mod->so, dt_token("prim")));
  int *p_trc  = (int*)dt_module_param_int(mod, dt_module_get_param(mod->so, dt_token("trc" )));
  // don't overwrite user choice, but if they don't know/leave at defaults we do:
  if(*p_prim == s_colour_primaries_unknown) *p_prim = mod->img_param.colour_primaries;
  if(*p_trc  == s_colour_trc_unknown)       *p_trc  = mod->img_param.colour_trc;
  mod->img_param.colour_primaries = *p_prim;
  mod->img_param.colour_trc       = *p_trc;
}

int read_source(
    dt_module_t             *mod,
    void                    *mapped,
    dt_read_source_params_t *p)
{
  const int   id       = dt_module_param_int(mod, 1)[0];
  const char *filename = dt_module_param_string(mod, 0);
  if(read_header(mod, id+mod->graph->frame, filename)) return 1;
  jpginput_buf_t *jpg = mod->data;
  jpeg_read(jpg, mapped);
  return 0;
}
#undef ICC_MARKER
#undef ICC_OVERHEAD_LEN
#undef MAX_BYTES_IN_MARKER
#undef MAX_DATA_BYTES_IN_MARKER
#undef MAX_SEQ_NO
