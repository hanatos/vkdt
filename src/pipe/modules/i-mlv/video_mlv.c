#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <math.h>
#include <time.h>
#include <stdint.h>

#include "video_mlv.h"
// #include "audio_mlv.h"

/* Lossless decompression */
#include "liblj92/lj92.h"
#include "liblj92/lj92.c"

#define ROR32(v,a) ((v) >> (a) | (v) << (32-(a)))

static int seek_to_next_known_block(FILE * in_file)
{
  uint64_t read_ahead_size = 128 * 1024 * 1024;
  uint8_t *ahead = malloc(read_ahead_size);

  uint64_t read = fread(ahead, 1, read_ahead_size, in_file);
  fseek(in_file, -read, SEEK_CUR);
  for (uint64_t i = 0; i < read; i++)
  {
    if (memcmp(ahead + i, "VIDF", 4) == 0 ||
        memcmp(ahead + i, "AUDF", 4) == 0 ||
        memcmp(ahead + i, "NULL", 4) == 0 ||
        memcmp(ahead + i, "RTCI", 4) == 0)
    {
      fseek(in_file, i, SEEK_CUR);
      free(ahead);
      return 1;
    }
  }

  free(ahead);
  return 0;
}

/* Spanned multichunk MLV file handling */
static FILE **load_all_chunks(const char *base_filename, int *entries)
{
  int seq_number = 0;
  int max_name_len = strlen(base_filename) + 16;
  char *filename = alloca(max_name_len);

  strncpy(filename, base_filename, max_name_len - 1);
  FILE **files = malloc(sizeof(FILE*));

  files[0] = fopen(filename, "rb");
  if(!files[0])
  {
    free(files);
    return NULL;
  }

  // FIXME i think this test is broken and should probably return 0 anyways
  /* get extension and check if it is a .MLV */
  char *dot = strrchr(filename, '.');
  if(dot && strcasecmp(dot+1, "mlv"))
    seq_number = 100;

  (*entries)++;
  while(seq_number < 99)
  {
    FILE **realloc_files = realloc(files, (*entries + 1) * sizeof(FILE*));

    if(!realloc_files)
    {
      free(files);
      return NULL;
    }

    files = realloc_files;

    /* check for the next file M00, M01 etc */
    char seq_name[8];

    sprintf(seq_name, "%02d", seq_number);
    seq_number++;

    strcpy(&filename[strlen(filename) - 2], seq_name);

    /* try to open */
    files[*entries] = fopen(filename, "rb");
    if(files[*entries]) (*entries)++;
    else break;
  }

  return files;
}

static void close_all_chunks(FILE ** files, int entries)
{
  for(int i = 0; i < entries; i++)
    if(files[i]) fclose(files[i]);
  if(files) free(files);
}

// TODO: replace by qsort
static void frame_index_sort(mlv_frame_index_t *frame_index, uint32_t entries)
{
  if (!entries) return;

  uint32_t n = entries;
  do
  {
    uint32_t new_n = 1;
    for (uint32_t i = 0; i < n-1; ++i)
    {
      if (frame_index[i].frame_time > frame_index[i+1].frame_time)
      {
        mlv_frame_index_t tmp = frame_index[i+1];
        frame_index[i+1] = frame_index[i];
        frame_index[i] = tmp;
        new_n = i + 1;
      }
    }
    n = new_n;
  } while (n > 1);
}

/* Unpack or decompress original raw data */
int mlv_get_frame(
    mlv_header_t *video,
    uint64_t      frame_index,
    uint16_t     *unpackedFrame)
{
  int bitdepth  = video->RAWI.raw_info.bits_per_pixel;
  int width     = video->RAWI.xRes;
  int height    = video->RAWI.yRes;
  int pixel_cnt = width * height;

  int chunk = video->video_index[frame_index].chunk_num;
  uint32_t frame_size = video->video_index[frame_index].frame_size;
  uint64_t frame_offset = video->video_index[frame_index].frame_offset;
  uint64_t frame_header_offset = video->video_index[frame_index].block_offset;

  /* How many bytes is RAW frame */
  int raw_frame_size = (width * height * bitdepth) / 8;
  /* Memory buffer for original RAW data */
  uint8_t *raw_frame = malloc(raw_frame_size + 4); // additional 4 bytes for safety

  FILE *file = video->file[chunk];

  fseek(file, frame_header_offset, SEEK_SET);
  if(fread(&video->VIDF, sizeof(mlv_vidf_hdr_t), 1, file) != 1)
  {
    free(raw_frame);
    return 1;
  }

  fseek(file, frame_offset, SEEK_SET);
  if (video->MLVI.videoClass & MLV_VIDEO_CLASS_FLAG_LJ92)
  {
    if(fread(raw_frame, frame_size, 1, file) != 1)
    { // frame data read error
      free(raw_frame);
      return 1;
    }

    int components = 1;
    lj92 decoder_object;
    int ret = lj92_open(&decoder_object, raw_frame, frame_size, &width, &height, &bitdepth, &components);
    if(ret != LJ92_ERROR_NONE)
    { // lj92 decoding failed
      free(raw_frame);
      return 1;
    }
    else
    {
      ret = lj92_decode(decoder_object, unpackedFrame, width * height * components, 0, NULL, 0);
      if(ret != LJ92_ERROR_NONE)
      { // lj92 failure
        free(raw_frame);
        return 1;
      }
    }
    lj92_close(decoder_object);
  }
  else /* If not compressed just unpack to 16bit */
  {
    if(fread(raw_frame, raw_frame_size, 1, file) != 1)
    { // can't read frame data
      free(raw_frame);
      return 1;
    }

    uint32_t mask = (1 << bitdepth) - 1;
#pragma omp parallel for
    for (int i = 0; i < pixel_cnt; ++i)
    {
      uint32_t bits_offset = i * bitdepth;
      uint32_t bits_address = bits_offset / 16;
      uint32_t bits_shift = bits_offset % 16;
      uint32_t rotate_value = 16 + ((32 - bitdepth) - bits_shift);
      uint32_t uncorrected_data = *((uint32_t *)&((uint16_t *)raw_frame)[bits_address]);
      uint32_t data = ROR32(uncorrected_data, rotate_value);
      unpackedFrame[i] = ((uint16_t)(data & mask));
    }
  }

  free(raw_frame);
  return 0;
}

void mlv_header_init(mlv_header_t *video)
{
  memset(video, 0, sizeof(*video));
}

/* Free all memory and close file */
void mlv_header_cleanup(mlv_header_t *video)
{
  /* Close all MLV file chunks */
  if(video->file) close_all_chunks(video->file, video->filenum);
  /* Free all memory */
  free(video->video_index);
  free(video->audio_index);
  free(video->vers_index);
  free(video->audio_data);
  memset(video, 0, sizeof(*video));
}

int mlv_open_clip(
    mlv_header_t *video,
    const char   *filename,
    int           open_mode)
{
  video->file = load_all_chunks(filename, &video->filenum);
  if(!video->file) return MLV_ERR_OPEN; // can not open file

  uint64_t block_num = 0; /* Number of blocks in file */
  mlv_hdr_t block_header; /* Basic MLV block header */
  uint64_t video_frames = 0; /* Number of frames in video */
  uint64_t audio_frames = 0; /* Number of audio blocks in video */
  uint32_t vers_blocks = 0; /* Number of VERS blocks in MLV */
  uint64_t video_index_max = 0; /* initial size of frame index */
  uint64_t audio_index_max = 0; /* initial size of audio index */
  uint32_t vers_index_max = 0; /* initial size of VERS index */
  int mlvi_read = 0; /* Flips to 1 if 1st chunk MLVI block was read */
  int rtci_read = 0; /* Flips to 1 if 1st RTCI block was read */
  int lens_read = 0; /* Flips to 1 if 1st LENS block was read */
  int elns_read = 0; /* Flips to 1 if 1st ELNS block was read */
  int wbal_read = 0; /* Flips to 1 if 1st WBAL block was read */
  int styl_read = 0; /* Flips to 1 if 1st STYL block was read */
  int fread_err = 1;

  for(int i = 0; i < video->filenum; i++)
  {
    /* Getting size of file in bytes */
    fseek(video->file[i], 0, SEEK_END);
    uint64_t file_size = ftell(video->file[i]);
    if(!file_size)
    {
      --video->filenum;
      return MLV_ERR_INVALID;
    }
    fseek(video->file[i], 0, SEEK_SET); /* Start of file */

    /* Read file header */
    if(fread(&block_header, sizeof(mlv_hdr_t), 1, video->file[i]) != 1)
    {
      --video->filenum;
      return MLV_ERR_INVALID;
    }
    fseek(video->file[i], 0, SEEK_SET); /* Start of file */

    if(memcmp(block_header.blockType, "MLVI", 4) == 0)
    {
      if( !mlvi_read )
      {
        fread_err &= fread(&video->MLVI, sizeof(mlv_file_hdr_t), 1, video->file[i]);
        mlvi_read = 1; // read MLVI only for first chunk
      }
    }
    else
    {
      --video->filenum;
      return MLV_ERR_INVALID;
    }

    while ( ftell(video->file[i]) < file_size ) /* Check if were at end of file yet */
    {
      /* Record position to go back to it later if block is read */
      uint64_t block_start = ftell(video->file[i]);
      /* Read block header */
      fread_err &= fread(&block_header, sizeof(mlv_hdr_t), 1, video->file[i]);
      if(block_header.blockSize < sizeof(mlv_hdr_t))
      { // invalid block size
        --video->filenum;
        return MLV_ERR_INVALID;
      }

      /* Next block location */
      uint64_t next_block = (uint64_t)block_start + (uint64_t)block_header.blockSize;
      /* Go back to start of block for next bit */
      fseek(video->file[i], block_start, SEEK_SET);

      /* Now check what kind of block it is and read it in to the mlv object */
      if ( memcmp(block_header.blockType, "NULL", 4) == 0 || memcmp(block_header.blockType, "BKUP", 4) == 0)
      {
        /* do nothing, skip this block */
      }
      else if ( memcmp(block_header.blockType, "VIDF", 4) == 0 )
      {
        fread_err &= fread(&video->VIDF, sizeof(mlv_vidf_hdr_t), 1, video->file[i]);

        /* Dynamically resize the frame index buffer */
        if(!video_index_max)
        {
          video_index_max = 128;
          video->video_index = calloc(video_index_max, sizeof(mlv_frame_index_t));
        }
        else if(video_frames >= video_index_max - 1)
        {
          uint64_t video_index_new_size = video_index_max * 2;
          mlv_frame_index_t * video_index_new = calloc(video_index_new_size, sizeof(mlv_frame_index_t));
          memcpy(video_index_new, video->video_index, video_index_max * sizeof(mlv_frame_index_t));
          free(video->video_index);
          video->video_index = video_index_new;
          video_index_max = video_index_new_size;
        }

        /* Fill frame index */
        video->video_index[video_frames].frame_type = 1;
        video->video_index[video_frames].chunk_num = i;
        video->video_index[video_frames].frame_size = video->VIDF.blockSize - sizeof(mlv_vidf_hdr_t) - video->VIDF.frameSpace;
        video->video_index[video_frames].frame_offset = ftell(video->file[i]) + video->VIDF.frameSpace;
        video->video_index[video_frames].frame_number = video->VIDF.frameNumber;
        video->video_index[video_frames].frame_time = video->VIDF.timestamp;
        video->video_index[video_frames].block_offset = block_start;

        /* Count actual video frames */
        video_frames++;

        /* In preview mode exit loop after first videf read */
        if(open_mode == MLV_OPEN_PREVIEW)
        {
          video->frames = video_frames;
          video->audios = audio_frames;
          goto preview_out;
        }
      }
      else if ( memcmp(block_header.blockType, "AUDF", 4) == 0 )
      {
        fread_err &= fread(&video->AUDF, sizeof(mlv_audf_hdr_t), 1, video->file[i]);

        /* Dynamically resize the audio index buffer */
        if(!audio_index_max)
        {
          audio_index_max = 32;
          video->audio_index = malloc(sizeof(mlv_frame_index_t) * audio_index_max);
        }
        else if(audio_frames >= audio_index_max - 1)
        {
          uint64_t audio_index_new_size = audio_index_max * 2;
          mlv_frame_index_t * audio_index_new = calloc(audio_index_new_size, sizeof(mlv_frame_index_t));
          memcpy(audio_index_new, video->audio_index, audio_index_max * sizeof(mlv_frame_index_t));
          free(video->audio_index);
          video->audio_index = audio_index_new;
          audio_index_max = audio_index_new_size;
        }

        /* Fill audio index */
        video->audio_index[audio_frames].frame_type = 2;
        video->audio_index[audio_frames].chunk_num = i;
        video->audio_index[audio_frames].frame_size = video->AUDF.blockSize - sizeof(mlv_audf_hdr_t) - video->AUDF.frameSpace;
        video->audio_index[audio_frames].frame_offset = ftell(video->file[i]) + video->AUDF.frameSpace;
        video->audio_index[audio_frames].frame_number = video->AUDF.frameNumber;
        video->audio_index[audio_frames].frame_time = video->AUDF.timestamp;
        video->audio_index[audio_frames].block_offset = block_start;

        /* Count actual audio frames */
        audio_frames++;
      }
      else if ( memcmp(block_header.blockType, "RAWI", 4) == 0 )
      {
        fread_err &= fread(&video->RAWI, sizeof(mlv_rawi_hdr_t), 1, video->file[i]);
      }
      else if ( memcmp(block_header.blockType, "RAWC", 4) == 0 )
      {
        fread_err &= fread(&video->RAWC, sizeof(mlv_rawc_hdr_t), 1, video->file[i]);
      }
      else if ( memcmp(block_header.blockType, "WAVI", 4) == 0 )
      {
        fread_err &= fread(&video->WAVI, sizeof(mlv_wavi_hdr_t), 1, video->file[i]);
      }
      else if ( memcmp(block_header.blockType, "EXPO", 4) == 0 )
      {
        fread_err &= fread(&video->EXPO, sizeof(mlv_expo_hdr_t), 1, video->file[i]);
      }
      else if ( memcmp(block_header.blockType, "LENS", 4) == 0 )
      {
        if( !lens_read )
        {
          fread_err &= fread(&video->LENS, sizeof(mlv_lens_hdr_t), 1, video->file[i]);
          lens_read = 1; //read only first one
          //Terminate string, if it isn't terminated.
          for( int n = 0; n < 32; n++ )
          {
            if( video->LENS.lensName[n] == '\0' ) break;
            if( n == 31 ) video->LENS.lensName[n] = '\0';
          }
        }
      }
      else if ( memcmp(block_header.blockType, "ELNS", 4) == 0 )
      {
        if( !elns_read )
        {
          fread_err &= fread(&video->ELNS, sizeof(mlv_elns_hdr_t), 1, video->file[i]);
          elns_read = 1; //read only first one
        }
      }
      else if ( memcmp(block_header.blockType, "WBAL", 4) == 0 )
      {
        if( !wbal_read )
        {
          fread_err &= fread(&video->WBAL, sizeof(mlv_wbal_hdr_t), 1, video->file[i]);
          wbal_read = 1; //read only first one
        }
      }
      else if ( memcmp(block_header.blockType, "STYL", 4) == 0 )
      {
        if( !styl_read )
        {
          fread_err &= fread(&video->STYL, sizeof(mlv_styl_hdr_t), 1, video->file[i]);
          styl_read = 1; //read only first one
        }
      }
      else if ( memcmp(block_header.blockType, "RTCI", 4) == 0 )
      {
        if( !rtci_read )
        {
          fread_err &= fread(&video->RTCI, sizeof(mlv_rtci_hdr_t), 1, video->file[i]);
          rtci_read = 1; //read only first one
        }
      }
      else if ( memcmp(block_header.blockType, "IDNT", 4) == 0 )
      {
        fread_err &= fread(&video->IDNT, sizeof(mlv_idnt_hdr_t), 1, video->file[i]);
      }
      else if ( memcmp(block_header.blockType, "INFO", 4) == 0 )
      {
        fread_err &= fread(&video->INFO, sizeof(mlv_info_hdr_t), 1, video->file[i]);
        if(video->INFO.blockSize > sizeof(mlv_info_hdr_t))
        {
          fread_err &= fread(&video->INFO_STRING, video->INFO.blockSize - sizeof(mlv_info_hdr_t), 1, video->file[i]);
        }
      }
      else if ( memcmp(block_header.blockType, "DISO", 4) == 0 )
      {
        fread_err &= fread(&video->DISO, sizeof(mlv_diso_hdr_t), 1, video->file[i]);
      }
      else if ( memcmp(block_header.blockType, "MARK", 4) == 0 )
      {
        /* do nothing atm */
        //fread(&video->MARK, sizeof(mlv_mark_hdr_t), 1, video->file[i]);
      }
      else if ( memcmp(block_header.blockType, "ELVL", 4) == 0 )
      {
        /* do nothing atm */
        //fread(&video->ELVL, sizeof(mlv_elvl_hdr_t), 1, video->file[i]);
      }
      else if ( memcmp(block_header.blockType, "DEBG", 4) == 0 )
      {
        /* do nothing atm */
        //fread(&video->DEBG, sizeof(mlv_debg_hdr_t), 1, video->file[i]);
      }
      else if ( memcmp(block_header.blockType, "VERS", 4) == 0 )
      {
        /* Find all VERS blocks and make index for them */
        fread_err &= fread(&video->VERS, sizeof(mlv_vers_hdr_t), 1, video->file[i]);

        /* Dynamically resize the index buffer */
        if(!vers_index_max)
        {
          vers_index_max = 128;
          video->vers_index = calloc(vers_index_max, sizeof(mlv_frame_index_t));
        }
        else if(vers_blocks >= vers_index_max - 1)
        {
          uint64_t vers_index_new_size = vers_index_max * 2;
          mlv_frame_index_t * vers_index_new = calloc(vers_index_new_size, sizeof(mlv_frame_index_t));
          memcpy(vers_index_new, video->vers_index, vers_index_max * sizeof(mlv_frame_index_t));
          free(video->vers_index);
          video->vers_index = vers_index_new;
          vers_index_max = vers_index_new_size;
        }

        /* Fill frame index */
        video->vers_index[vers_blocks].frame_type = 3;
        video->vers_index[vers_blocks].chunk_num = i;
        video->vers_index[vers_blocks].frame_size = video->VERS.blockSize - sizeof(mlv_vers_hdr_t);
        video->vers_index[vers_blocks].frame_offset = ftell(video->file[i]);
        video->vers_index[vers_blocks].frame_number = vers_blocks;
        video->vers_index[vers_blocks].frame_time = video->VERS.timestamp;
        video->vers_index[vers_blocks].block_offset = block_start;

        /* Count actual VERS blocks */
        vers_blocks++;
      }
      else if ( memcmp(block_header.blockType, "DARK", 4) == 0 )
      {
        fread_err &= fread(&video->DARK, sizeof(mlv_dark_hdr_t), 1, video->file[i]);
        video->dark_frame_offset = ftell(video->file[i]);
      }
      else
      { // block name is wrong, so try to brute force the position of next valid block
        if(!seek_to_next_known_block(video->file[i]))
        { // unknown block type
          --video->filenum;
          return MLV_ERR_CORRUPTED;
        }
        continue;
      }

      /* Move to next block */
      fseek(video->file[i], next_block, SEEK_SET);

      block_num++;
    }
  }

  if(!fread_err)
  {
    --video->filenum;
    return MLV_ERR_IO;
  }
  if(!video_frames)
  {
    --video->filenum;
    return MLV_ERR_INVALID;
  }

  /* Set total block amount in mlv */
  video->block_num = block_num;

  /* Sort video and audio frames by time stamp */
  if(video_frames) frame_index_sort(video->video_index, video_frames);
  if(audio_frames) frame_index_sort(video->audio_index, audio_frames);

  /* Set frame count in video object */
  video->frames = video_frames;
  /* Set audio count in video object */
  video->audios = audio_frames;
  /* Set VERS block count in video object */
  video->vers_blocks = vers_blocks;

  /* Reads MLV audio into buffer (video->audio_data) and sync it,
   * set full audio buffer size (video->audio_buffer_size) and
   * aligned usable audio data size (video->audio_size) */
  // readMlvAudioData(video);

preview_out:

  /* NON compressed frame size */
  video->frame_size = video->RAWI.xRes * video->RAWI.yRes * video->RAWI.raw_info.bits_per_pixel / 8;
  video->frame_rate = (double)(video->MLVI.sourceFpsNom / (double)video->MLVI.sourceFpsDenom);

  return MLV_ERR_NONE;
}

#if 0
void printMlvInfo(mlv_header_t *video)
{
  printf("\nMLV Info\n\n");
  printf("      MLV Version: %s\n", video->MLVI.versionString);
  printf("      File Blocks: %lu\n", video->block_num);
  printf("\nLens Info\n\n");
  printf("       Lens Model: %s\n", video->LENS.lensName);
  printf("    Serial Number: %s\n", video->LENS.lensSerial);
  printf("\nCamera Info\n\n");
  printf("     Camera Model: %s\n", video->IDNT.cameraName);
  printf("    Serial Number: %s\n", video->IDNT.cameraSerial);
  printf("\nVideo Info\n\n");
  printf("     X Resolution: %i\n", video->RAWI.xRes);
  printf("     Y Resolution: %i\n", video->RAWI.yRes);
  printf("     Total Frames: %i\n", video->frames);
  printf("       Frame Rate: %.3f\n", video->frame_rate);
  printf("\nExposure Info\n\n");
  printf("          Shutter: 1/%.1f\n", (float)1000000 / (float)video->EXPO.shutterValue);
  printf("      ISO Setting: %i\n", video->EXPO.isoValue);
  printf("     Digital Gain: %i\n", video->EXPO.digitalGain);
  printf("\nRAW Info\n\n");
  printf("      Black Level: %i\n", video->RAWI.raw_info.black_level);
  printf("      White Level: %i\n", video->RAWI.raw_info.white_level);
  printf("     Bits / Pixel: %i\n\n", video->RAWI.raw_info.bits_per_pixel);
}
#endif
