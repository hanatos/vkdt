/*
 * Copyright (C) 2013 Magic Lantern Team
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the
 * Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#ifndef _mlv_h_
#define _mlv_h_

#include "raw.h"

#define MLV_VERSION_STRING "v2.0"
#define MLV_VIDEO_CLASS_RAW          0x01
#define MLV_VIDEO_CLASS_YUV          0x02
#define MLV_VIDEO_CLASS_JPEG         0x03
#define MLV_VIDEO_CLASS_H264         0x04

#define MLV_VIDEO_CLASS_FLAG_LZMA    0x80
#define MLV_VIDEO_CLASS_FLAG_DELTA   0x40
#define MLV_VIDEO_CLASS_FLAG_LJ92    0x20

#define MLV_AUDIO_CLASS_FLAG_LZMA    0x80

#define MLV_FRAME_UNSPECIFIED 0
#define MLV_FRAME_VIDF        1
#define MLV_FRAME_AUDF        2

#pragma pack(push,1)

typedef struct {
    uint8_t     blockType[4];
    uint32_t    blockSize;
    uint64_t    timestamp;
} mlv_hdr_t;

typedef struct {
    uint8_t     fileMagic[4];    /* Magic Lantern Video file header */
    uint32_t    blockSize;    /* size of the whole header */
    uint8_t     versionString[8];    /* null-terminated C-string of the exact revision of this format */
    uint64_t    fileGuid;    /* UID of the file (group) generated using hw counter, time of day and PRNG */
    uint16_t    fileNum;    /* the ID within fileCount this file has (0 to fileCount-1) */
    uint16_t    fileCount;    /* how many files belong to this group (splitting or parallel) */
    uint32_t    fileFlags;    /* 1=out-of-order data, 2=dropped frames, 4=single image mode, 8=stopped due to error */
    uint16_t    videoClass;    /* 0=none, 1=RAW, 2=YUV, 3=JPEG, 4=H.264 */
    uint16_t    audioClass;    /* 0=none, 1=WAV */
    uint32_t    videoFrameCount;    /* number of video frames in this file. set to 0 on start, updated when finished. */
    uint32_t    audioFrameCount;    /* number of audio frames in this file. set to 0 on start, updated when finished. */
    uint32_t    sourceFpsNom;    /* configured fps in 1/s multiplied by sourceFpsDenom */
    uint32_t    sourceFpsDenom;    /* denominator for fps. usually set to 1000, but may be 1001 for NTSC */
}  mlv_file_hdr_t;

typedef struct {
    uint8_t     blockType[4];    /* this block contains one frame of video data */
    uint32_t    blockSize;    /* total frame size */
    uint64_t    timestamp;    /* hardware counter timestamp for this frame (relative to recording start) */
    uint32_t    frameNumber;    /* unique video frame number */
    uint16_t    cropPosX;    /* specifies from which sensor row/col the video frame was copied (8x2 blocks) */
    uint16_t    cropPosY;    /* (can be used to process dead/hot pixels) */
    uint16_t    panPosX;    /* specifies the panning offset which is cropPos, but with higher resolution (1x1 blocks) */
    uint16_t    panPosY;    /* (it's the frame area from sensor the user wants to see) */
    uint32_t    frameSpace;    /* size of dummy data before frameData starts, necessary for EDMAC alignment */
 /* uint8_t     frameData[variable]; */
}  mlv_vidf_hdr_t;

typedef struct {
    uint8_t     blockType[4];    /* this block contains audio data */
    uint32_t    blockSize;    /* total frame size */
    uint64_t    timestamp;    /* hardware counter timestamp for this frame (relative to recording start) */
    uint32_t    frameNumber;    /* unique audio frame number */
    uint32_t    frameSpace;    /* size of dummy data before frameData starts, necessary for EDMAC alignment */
 /* uint8_t     frameData[variable]; */
}  mlv_audf_hdr_t;

typedef struct {
    uint8_t     blockType[4];    /* when videoClass is RAW, this block will contain detailed format information */
    uint32_t    blockSize;    /* total frame size */
    uint64_t    timestamp;    /* hardware counter timestamp for this frame (relative to recording start) */
    uint16_t    xRes;    /* Configured video resolution, may differ from payload resolution */
    uint16_t    yRes;    /* Configured video resolution, may differ from payload resolution */
    struct raw_info raw_info;    /* the raw_info structure delivered by raw.h of ML Core */
}  mlv_rawi_hdr_t;

typedef struct {
    uint8_t     blockType[4];   /* RAWC - raw image capture information */
    uint32_t    blockSize;      /* sizeof(mlv_rawc_hdr_t) */
    uint64_t    timestamp;      /* hardware counter timestamp */

    /* see struct raw_capture_info from raw.h */

    /* sensor attributes: resolution, crop factor */
    uint16_t sensor_res_x;      /* sensor resolution */
    uint16_t sensor_res_y;      /* 2-3 GPixel cameras anytime soon? (to overflow this) */
    uint16_t sensor_crop;       /* sensor crop factor x100 */
    uint16_t reserved;          /* reserved for future use */

    /* video mode attributes */
    /* (how the sensor is configured for image capture) */
    /* subsampling factor: (binning_x+skipping_x) x (binning_y+skipping_y) */
    uint8_t  binning_x;         /* 3 (1080p and 720p); 1 (crop, zoom) */
    uint8_t  skipping_x;        /* so far, 0 everywhere */
    uint8_t  binning_y;         /* 1 (most cameras in 1080/720p; also all crop modes); 3 (5D3 1080p); 5 (5D3 720p) */
    uint8_t  skipping_y;        /* 2 (most cameras in 1080p); 4 (most cameras in 720p); 0 (5D3) */
    int16_t  offset_x;          /* crop offset (top-left active pixel) - optional (SHRT_MIN if unknown) */
    int16_t  offset_y;          /* relative to top-left active pixel from a full-res image (FRSP or CR2) */

    /* The captured *active* area (raw_info.active_area) will be mapped
     * on a full-res image (which does not use subsampling) as follows:
     *   active_width  = raw_info.active_area.x2 - raw_info.active_area.x1
     *   active_height = raw_info.active_area.y2 - raw_info.active_area.y1
     *   .x1 (left)  : offset_x + full_res.active_area.x1
     *   .y1 (top)   : offset_y + full_res.active_area.y1
     *   .x2 (right) : offset_x + active_width  * (binning_x+skipping_x) + full_res.active_area.x1
     *   .y2 (bottom): offset_y + active_height * (binning_y+skipping_y) + full_res.active_area.y1
     */
}  mlv_rawc_hdr_t;

typedef struct {
    uint8_t     blockType[4];    /* when audioClass is WAV, this block contains format details  compatible to RIFF */
    uint32_t    blockSize;    /* total frame size */
    uint64_t    timestamp;    /* hardware counter timestamp for this frame (relative to recording start) */
    uint16_t    format;    /* 1=Integer PCM, 6=alaw, 7=mulaw */
    uint16_t    channels;    /* audio channel count: 1=mono, 2=stereo */
    uint32_t    samplingRate;    /* audio sampling rate in 1/s */
    uint32_t    bytesPerSecond;    /* audio data rate */
    uint16_t    blockAlign;    /* see RIFF WAV hdr description */
    uint16_t    bitsPerSample;    /* audio ADC resolution */
}  mlv_wavi_hdr_t;

typedef struct {
    uint8_t     blockType[4];
    uint32_t    blockSize;    /* total frame size */
    uint64_t    timestamp;    /* hardware counter timestamp for this frame (relative to recording start) */
    uint32_t    isoMode;    /* 0=manual, 1=auto */
    uint32_t    isoValue;    /* camera delivered ISO value */
    uint32_t    isoAnalog;    /* ISO obtained by hardware amplification (most full-stop ISOs, except extreme values) */
    uint32_t    digitalGain;    /* digital ISO gain (1024 = 1 EV) - it's not baked in the raw data, so you may want to scale it or adjust the white level */
    uint64_t    shutterValue;    /* exposure time in microseconds */
}  mlv_expo_hdr_t;

typedef struct {
    uint8_t     blockType[4];
    uint32_t    blockSize;    /* total frame size */
    uint64_t    timestamp;    /* hardware counter timestamp for this frame (relative to recording start) */
    uint16_t    focalLength;    /* in mm */
    uint16_t    focalDist;    /* in mm (65535 = infinite) */
    uint16_t    aperture;    /* f-number * 100 */
    uint8_t     stabilizerMode;    /* 0=off, 1=on, (is the new L mode relevant) */
    uint8_t     autofocusMode;    /* 0=off, 1=on */
    uint32_t    flags;    /* 1=CA avail, 2=Vign avail, ... */
    uint32_t    lensID;    /* hexadecimal lens ID (delivered by properties?) */
    uint8_t     lensName[32];    /* full lens string */
    uint8_t     lensSerial[32]; /* full lens serial number */
}  mlv_lens_hdr_t;

typedef struct {
    uint8_t     blockType[4];       /* ELNS - Extended LENS block with longer lens name and optional fields, depending on camera */
    uint32_t    blockSize;
    uint64_t    timestamp;
    uint16_t    focalLengthMin;     /* shortest focal length in mm                       */
    uint16_t    focalLengthMax;     /* longest focal length in mm                        */
    uint16_t    apertureMin;        /* lowest f-number * 100                             */
    uint16_t    apertureMax;        /* highest f-number * 100                            */
    uint32_t    version;            /* lens internal version number, if available        */
    uint8_t     extenderInfo;       /* extender information, if provided by camera       */
    uint8_t     capabilities;       /* capability information, if provided by camera     */
    uint8_t     chipped;            /* when not zero, lens is communicating with camera  */
 /* uint8_t     lensName[variable];    full lens string, null terminated                 */
}  mlv_elns_hdr_t;

typedef struct {
    uint8_t     blockType[4];
    uint32_t    blockSize;    /* total frame size */
    uint64_t    timestamp;    /* hardware counter timestamp for this frame (relative to recording start) */
    uint16_t    tm_sec;    /* seconds (0-59) */
    uint16_t    tm_min;    /* minute (0-59) */
    uint16_t    tm_hour;    /* hour (0-23) */
    uint16_t    tm_mday;    /* day of month (1-31) */
    uint16_t    tm_mon;    /* month (0-11) */
    uint16_t    tm_year;    /* year since 1900 */
    uint16_t    tm_wday;    /* day of week */
    uint16_t    tm_yday;    /* day of year */
    uint16_t    tm_isdst;    /* daylight saving */
    uint16_t    tm_gmtoff;    /* GMT offset */
    uint8_t     tm_zone[8];    /* time zone string */
}  mlv_rtci_hdr_t;

typedef struct {
    uint8_t     blockType[4];
    uint32_t    blockSize;    /* total frame size */
    uint64_t    timestamp;    /* hardware counter timestamp for this frame (relative to recording start) */
    uint8_t     cameraName[32];    /* PROP (0x00000002), offset 0, length 32 */
    uint32_t    cameraModel;    /* PROP (0x00000002), offset 32, length 4 */
    uint8_t     cameraSerial[32];    /* Camera serial number (if available) */
}  mlv_idnt_hdr_t;

typedef struct {
    uint16_t    fileNumber;    /* the logical file number as specified in header */
    uint8_t     empty;    /* for future use. set to zero. */
    uint8_t     frameType;    /* MLV_FRAME_VIDF(1) for VIDF, MLV_FRAME_AUDF(2) for AUDF, MLV_FRAME_UNSPECIFIED(0) otherwise */
    uint64_t    frameOffset;    /* the file offset at which the frame is stored (VIDF/AUDF) */
}  mlv_xref_t;

typedef struct {
    uint8_t     blockType[4];    /* can be added in post processing when out of order data is present */
    uint32_t    blockSize;    /* this can also be placed in a separate file with only file header plus this block */
    uint64_t    timestamp;
    uint32_t    frameType;    /* bitmask: 1=video, 2=audio */
    uint32_t    entryCount;    /* number of xrefs that follow here */
    //mlv_xref_t  xrefEntries;    /* this structure refers to the n'th video/audio frame offset in the files */
}  mlv_xref_hdr_t;

typedef struct {
    uint8_t     blockType[4];    /* user definable info string. take number, location, etc. */
    uint32_t    blockSize;
    uint64_t    timestamp;
 /* uint8_t     stringData[variable]; */
}  mlv_info_hdr_t;

typedef struct {
    uint8_t     blockType[4];    /* Dual-ISO information */
    uint32_t    blockSize;
    uint64_t    timestamp;
    uint32_t    dualMode;    /* bitmask: 0=off, 1=odd lines, 2=even lines, upper bits may be defined later */
    uint32_t    isoValue;
}  mlv_diso_hdr_t;

typedef struct {
    uint8_t     blockType[4];    /* markers set by user while recording */
    uint32_t    blockSize;
    uint64_t    timestamp;
    uint32_t    type;    /* value may depend on the button being pressed or counts up (t.b.d) */
}  mlv_mark_hdr_t;

typedef struct {
    uint8_t     blockType[4];
    uint32_t    blockSize;
    uint64_t    timestamp;
    uint32_t    picStyleId;
    int32_t     contrast;
    int32_t     sharpness;
    int32_t     saturation;
    int32_t     colortone;
    uint8_t     picStyleName[16];
}  mlv_styl_hdr_t;

typedef struct {
    uint8_t     blockType[4];    /* Electronic level (orientation) data */
    uint32_t    blockSize;
    uint64_t    timestamp;
    uint32_t    roll;    /* degrees x100 (here, 45.00 degrees) */
    uint32_t    pitch;    /* 10.00 degrees */
}  mlv_elvl_hdr_t;

typedef struct {
    uint8_t     blockType[4];    /* White balance info */
    uint32_t    blockSize;
    uint64_t    timestamp;
    uint32_t    wb_mode;    /* WB_AUTO 0, WB_SUNNY 1, WB_SHADE 8, WB_CLOUDY 2, WB_TUNGSTEN 3, WB_FLUORESCENT 4, WB_FLASH 5, WB_CUSTOM 6, WB_KELVIN 9 */
    uint32_t    kelvin;    /* only when wb_mode is WB_KELVIN */
    uint32_t    wbgain_r;    /* only when wb_mode is WB_CUSTOM */
    uint32_t    wbgain_g;    /* 1024 = 1.0 */
    uint32_t    wbgain_b;    /* note: it's 1/canon_gain (uses dcraw convention) */
    uint32_t    wbs_gm;    /* WBShift (no idea how to use these in post) */
    uint32_t    wbs_ba;    /* range: -9...9 */
}  mlv_wbal_hdr_t;

typedef struct {
    uint8_t     blockType[4];    /* DEBG - debug messages for development use, contains no production data */
    uint32_t    blockSize;
    uint64_t    timestamp;
    uint32_t    type;       /* debug data type, for now 0 - text log */
    uint32_t    length;     /* to allow that data can be of arbitrary length and blocks are padded to 32 bits, so store real length */
 /* uint8_t     stringData[variable]; */
}  mlv_debg_hdr_t;

typedef struct {
    uint8_t     blockType[4];    /* VERS - Version information block, appears once per module */
    uint32_t    blockSize;
    uint64_t    timestamp;
    uint32_t    length;     /* to allow that data can be of arbitrary length and blocks are padded to 32 bits, so store real length */
 /* uint8_t     stringData[variable];  // Version string, e.g. "ml-core 20130912", "mlv_rec v2.1" or "mlv_lite 0d3fbdaf crop_rec_8k"
                                       // general format "<module_name> <version_information>"
                                       // where <module_name> must not contain spaces whereas <version_information> may be of any characters in UTF-8 format
*/
}  mlv_vers_hdr_t;

typedef struct {
    uint8_t     blockType[4];       /* DARK - contains essential settings' been active while recording and actual averaged frame data */
    uint32_t    blockSize;          /* total block size */
    uint64_t    timestamp;          /* for this block it is always max value of uint64_t */
    uint32_t    samplesAveraged;    /* amount of samples (frames) have been averaged */
    uint32_t    cameraModel;        /* PROP (0x00000002), offset 32, length 4 */
    uint16_t    xRes;               /* dark frame width */
    uint16_t    yRes;               /* dark frame height */
    uint32_t    rawWidth;           /* raw buffer width */
    uint32_t    rawHeight;          /* raw buffer height */
    uint32_t    bits_per_pixel;     /* bits per pixel */
    uint32_t    black_level;        /* autodetected */
    uint32_t    white_level;        /* somewhere around 13000 - 16000, varies with camera, settings etc */
    uint32_t    sourceFpsNom;       /* configured fps in 1/s multiplied by sourceFpsDenom */
    uint32_t    sourceFpsDenom;     /* denominator for fps. usually set to 1000, but may be 1001 for NTSC */
    uint32_t    isoMode;            /* 0=manual, 1=auto */
    uint32_t    isoValue;           /* camera delivered ISO value */
    uint32_t    isoAnalog;          /* ISO obtained by hardware amplification (most full-stop ISOs, except extreme values) */
    uint32_t    digitalGain;        /* digital ISO gain (1024 = 1 EV) - it's not baked in the raw data, so you may want to scale it or adjust the white level */
    uint64_t    shutterValue;       /* exposure time in microseconds */
    uint8_t     binning_x;          /* 3 (1080p and 720p); 1 (crop, zoom) */
    uint8_t     skipping_x;         /* so far, 0 everywhere */
    uint8_t     binning_y;          /* 1 (most cameras in 1080/720p; also all crop modes); 3 (5D3 1080p); 5 (5D3 720p) */
    uint8_t     skipping_y;         /* 2 (most cameras in 1080p); 4 (most cameras in 720p); 0 (5D3) */
    /* uint8_t     frameData[variable]; */
}  mlv_dark_hdr_t;

#pragma pack(pop)

#endif
