#pragma once

#include "mlv.h"

/* Struct of index of video and audio frames for quick access */
typedef struct
{
  uint16_t frame_type;     /* VIDF = 1, AUDF = 2, VERS = 3 */
  uint16_t chunk_num;      /* MLV chunk number */
  uint32_t frame_number;   /* Unique frame number */
  uint32_t frame_size;     /* Size of frame data */
  uint64_t frame_offset;   /* Offset to the start of frame data */
  uint64_t frame_time;     /* Time of frame from the start of recording in microseconds */
  uint64_t block_offset;   /* Offset to the start of the block header */
}
mlv_frame_index_t;

/* Struct for MLV handling */
typedef struct
{
  /* Amount of MLV chunks (.MLV, .M00, .M01, ...) */
  int filenum;
  uint64_t block_num; /* How many file blocks in MLV file */

  /* 0=no, 1=yes, mlv file open */
  int is_active;

  /* MLV/Lite file(s) */
  FILE ** file;

  /* For access to MLV headers */
  mlv_file_hdr_t    MLVI;
  mlv_rawi_hdr_t    RAWI;
  mlv_rawc_hdr_t    RAWC;
  mlv_idnt_hdr_t    IDNT;
  mlv_expo_hdr_t    EXPO;
  mlv_lens_hdr_t    LENS;
  mlv_elns_hdr_t    ELNS;
  mlv_rtci_hdr_t    RTCI;
  mlv_wbal_hdr_t    WBAL;
  mlv_wavi_hdr_t    WAVI;
  mlv_diso_hdr_t    DISO;
  mlv_info_hdr_t    INFO;
  mlv_styl_hdr_t    STYL;
  mlv_vers_hdr_t    VERS;
  mlv_dark_hdr_t    DARK;
  mlv_vidf_hdr_t    VIDF; /* One of many VIDFs(don't know if they're different) */
  mlv_audf_hdr_t    AUDF; /* Last AUDF header read */

  char INFO_STRING[256]; /* String stored in INFO block */

  /* Dark frame info */
  uint64_t dark_frame_offset;

  /* Black and white level copy for resseting to the original */
  uint16_t original_black_level;
  uint16_t original_white_level;

  /* Video info */
  double      real_frame_rate; /* ...Because framerate is not explicitly stored in the file */
  double      frame_rate;      /* User may want to override it */
  uint32_t    frames;          /* Number of frames */
  uint32_t    frame_size;      /* NOT counting compression factor */
  mlv_frame_index_t *video_index;

  /* Audio info */
  uint32_t    audios;          /* Number of audio blocks */
  mlv_frame_index_t * audio_index;

  /* Audio buffer pointer and size */
  uint8_t * audio_data;        /* Audio buffer pointer */
  uint64_t  audio_size;        /* Aligned usable audio size */
  uint64_t  audio_buffer_size; /* Full audio buffer size to be freed */

  /* Version info */
  uint32_t    vers_blocks;
  mlv_frame_index_t * vers_index;

  /* Restricted lossless raw data bit depth */
  int lossless_bpp;
}
mlv_header_t;

enum mlv_err { MLV_ERR_NONE, MLV_ERR_OPEN, MLV_ERR_IO, MLV_ERR_CORRUPTED, MLV_ERR_INVALID };
enum mlv_open_mode { MLV_OPEN_FULL, MLV_OPEN_MAPP, MLV_OPEN_PREVIEW };

int mlv_open_clip(
    mlv_header_t *video,
    const char   *filename,
    int           open_mode);

void mlv_header_init(mlv_header_t *video);
void mlv_header_cleanup(mlv_header_t *video);
int mlv_get_frame(
    mlv_header_t *video,
    uint64_t      frame_index,
    uint16_t     *unpackedFrame);
