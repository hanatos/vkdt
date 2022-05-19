/*
   This file is part of darktable,
   Copyright (C) 2009-2020 darktable developers.

   darktable is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   darktable is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with darktable.  If not, see <http://www.gnu.org/licenses/>.
 */

extern "C" {
#include "module.h"
}

#include <cassert>
#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <exiv2/exiv2.hpp>

namespace {

typedef enum dt_exif_image_orientation_t
{
  EXIF_ORIENTATION_NONE              = 1,
  EXIF_ORIENTATION_FLIP_HORIZONTALLY = 2,
  EXIF_ORIENTATION_FLIP_VERTICALLY   = 4,
  EXIF_ORIENTATION_ROTATE_180_DEG    = 3,
  EXIF_ORIENTATION_TRANSPOSE         = 5,
  EXIF_ORIENTATION_ROTATE_CCW_90_DEG = 8,
  EXIF_ORIENTATION_ROTATE_CW_90_DEG  = 6,
  EXIF_ORIENTATION_TRANSVERSE        = 7
}
dt_exif_image_orientation_t;

int dt_exif_read_exif_data(dt_image_params_t *ip, Exiv2::ExifData &exifData)
{
  try
  {
    /* List of tag names taken from exiv2's printSummary() in actions.cpp */
    Exiv2::ExifData::const_iterator pos;

#define FIND_EXIF_TAG(key) ((pos = exifData.findKey(Exiv2::ExifKey(key))) != exifData.end() && (pos)->size())

    // read shutter time
    if(FIND_EXIF_TAG("Exif.Photo.ExposureTime"))
      ip->exposure = pos->toFloat();
    else if(FIND_EXIF_TAG("Exif.Photo.ShutterSpeedValue"))
      ip->exposure = 1.0f / pos->toFloat();

    // read aperture
    if(FIND_EXIF_TAG("Exif.Photo.FNumber"))
      ip->aperture = pos->toFloat();
    else if(FIND_EXIF_TAG("Exif.Photo.ApertureValue"))
      ip->aperture = pos->toFloat();

    // Read ISO speed - Nikon happens to return a pair for Lo and Hi modes
    if((pos = Exiv2::isoSpeed(exifData)) != exifData.end() && pos->size())
    {
      // if standard exif iso tag, use the old way of interpreting the return value to be more regression-save
      if(strcmp(pos->key().c_str(), "Exif.Photo.ISOSpeedRatings") == 0)
      {
        int isofield = pos->count() > 1 ? 1 : 0;
        ip->iso = pos->toFloat(isofield);
      }
      else
      {
        std::string str = pos->print();
        ip->iso = (float)std::atof(str.c_str());
      }
    }
    // some newer cameras support iso settings that exceed the 16 bit of exif's ISOSpeedRatings
    if(ip->iso == 65535 || ip->iso == 0)
    {
      if(FIND_EXIF_TAG("Exif.PentaxDng.ISO") || FIND_EXIF_TAG("Exif.Pentax.ISO"))
      {
        std::string str = pos->print();
        ip->iso = (float)std::atof(str.c_str());
      }
      else if(FIND_EXIF_TAG("Exif.Photo.RecommendedExposureIndex"))
      {
        ip->iso = pos->toFloat();
      }
    }

    // read focal length
    if((pos = Exiv2::focalLength(exifData)) != exifData.end() && pos->size())
    {
      // This works around a bug in exiv2 the developers refuse to fix
      // For details see http://dev.exiv2.org/issues/1083
      if (pos->key() == "Exif.Canon.FocalLength" && pos->count() == 4)
        ip->focal_length = pos->toFloat(1);
      else
        ip->focal_length = pos->toFloat();
    }

    /*
     * Read image orientation
     */
    if(FIND_EXIF_TAG("Exif.Image.Orientation"))
    {
      ip->orientation = pos->toLong();
    }
    else if(FIND_EXIF_TAG("Exif.PanasonicRaw.Orientation"))
    {
      ip->orientation = pos->toLong();
    }

    if(FIND_EXIF_TAG("Exif.Image.DateTimeOriginal"))
    {
      std::string str = pos->print(&exifData);
      strncpy(ip->datetime, str.c_str(), sizeof(ip->datetime)-1);
    }
    else if(FIND_EXIF_TAG("Exif.Photo.DateTimeOriginal"))
    {
      std::string str = pos->print(&exifData);
      strncpy(ip->datetime, str.c_str(), sizeof(ip->datetime)-1);
    }
    else
    {
      ip->datetime[0] = 0;
    }

    { // read embedded color matrix as used in DNGs
      int illu1 = -1, illu2 = -1, illu = -1; // -1: not found, otherwise the detected CalibrationIlluminant
      float colmatrix[12];
      ip->cam_to_rec2020[0] = 0.0f/0.0f; // mark as uninitialised
      // The correction matrices are taken from
      // http://www.brucelindbloom.com - chromatic Adaption.
      // using Bradford method: found Illuminant -> D65
      const float correctmat[7][9] = {
        { 0.9555766, -0.0230393, 0.0631636, -0.0282895, 1.0099416, 0.0210077, 0.0122982, -0.0204830,
          1.3299098 }, // 23 = D50
        { 0.9726856, -0.0135482, 0.0361731, -0.0167463, 1.0049102, 0.0120598, 0.0070026, -0.0116372,
          1.1869548 }, // 20 = D55
        { 1.0206905, 0.0091588, -0.0228796, 0.0115005, 0.9984917, -0.0076762, -0.0043619, 0.0072053,
          0.8853432 }, // 22 = D75
        { 0.8446965, -0.1179225, 0.3948108, -0.1366303, 1.1041226, 0.1291718, 0.0798489, -0.1348999,
          3.1924009 }, // 17 = Standard light A
        { 0.9415037, -0.0321240, 0.0584672, -0.0428238, 1.0250998, 0.0203309, 0.0101511, -0.0161170,
          1.2847354 }, // 18 = Standard light B
        { 0.9904476, -0.0071683, -0.0116156, -0.0123712, 1.0155950, -0.0029282, -0.0035635, 0.0067697,
          0.9181569 }, // 19 = Standard light C
        { 0.9212269, -0.0449128, 0.1211620, -0.0553723, 1.0277243, 0.0403563, 0.0235086, -0.0391019,
          1.6390644 }  // F2 = cool white
      };

      if(FIND_EXIF_TAG("Exif.Image.CalibrationIlluminant1")) illu1 = pos->toLong();
      if(FIND_EXIF_TAG("Exif.Image.CalibrationIlluminant2")) illu2 = pos->toLong();
      Exiv2::ExifData::const_iterator cm1_pos = exifData.findKey(Exiv2::ExifKey("Exif.Image.ColorMatrix1"));
      Exiv2::ExifData::const_iterator cm2_pos = exifData.findKey(Exiv2::ExifKey("Exif.Image.ColorMatrix2"));

      // Which is the wanted colormatrix?
      // If we have D65 in Illuminant1 we use it; otherwise we prefer Illuminant2 because it's the higher
      // color temperature and thus closer to D65
      if(illu1 == 21 && cm1_pos != exifData.end() && cm1_pos->count() == 9 && cm1_pos->size())
      {
        for(int i = 0; i < 9; i++) colmatrix[i] = cm1_pos->toFloat(i);
        illu = illu1;
      }
      else if(illu2 != -1 && cm2_pos != exifData.end() && cm2_pos->count() == 9 && cm2_pos->size())
      {
        for(int i = 0; i < 9; i++) colmatrix[i] = cm2_pos->toFloat(i);
        illu = illu2;
      }
      else if(illu1 != -1 && cm1_pos != exifData.end() && cm1_pos->count() == 9 && cm1_pos->size())
      {
        for(int i = 0; i < 9; i++) colmatrix[i] = cm1_pos->toFloat(i);
        illu = illu1;
      }
      // In a few cases we only have one color matrix; it should not be corrected
      if(illu == -1 && cm1_pos != exifData.end() && cm1_pos->count() == 9 && cm1_pos->size())
      {
        for(int i = 0; i < 9; i++) colmatrix[i] = cm1_pos->toFloat(i);
        illu = 0;
      }

      // Take the found CalibrationIlluminant / ColorMatrix pair.
      // D65 or default: just copy. Otherwise multiply by the specific correction matrix.
      if(illu != -1)
      {
        float d65_colour_matrix[9];
        // If no supported Illuminant is found it's better NOT to use the found matrix.
        // The colorin module will write an error message and use a fallback matrix
        // instead of showing wrong colors.
        switch(illu)
        {
          case 23:
            mat3mul(d65_colour_matrix, correctmat[0], colmatrix);
            break;
          case 20:
            mat3mul(d65_colour_matrix, correctmat[1], colmatrix);
            break;
          case 22:
            mat3mul(d65_colour_matrix, correctmat[2], colmatrix);
            break;
          case 17:
            mat3mul(d65_colour_matrix, correctmat[3], colmatrix);
            break;
          case 18:
            mat3mul(d65_colour_matrix, correctmat[4], colmatrix);
            break;
          case 19:
            mat3mul(d65_colour_matrix, correctmat[5], colmatrix);
            break;
          case 3:
            mat3mul(d65_colour_matrix, correctmat[3], colmatrix);
            break;
          case 14:
            mat3mul(d65_colour_matrix, correctmat[6], colmatrix);
            break;
          default:
            for(int i = 0; i < 9; i++) d65_colour_matrix[i] = colmatrix[i];
            break;
        }
        // now convert to rec2020
        const float xyz_to_rec2020[] = {
           1.7166511880, -0.3556707838, -0.2533662814,
          -0.6666843518,  1.6164812366,  0.0157685458,
           0.0176398574, -0.0427706133,  0.9421031212
        };
        float cam_to_xyz[9] = {0.0f};
        mat3inv(cam_to_xyz, d65_colour_matrix);
        float cam_to_rec2020[9] = {0.0f};
        for(int j=0;j<3;j++) for(int i=0;i<3;i++) for(int k=0;k<3;k++)
          cam_to_rec2020[3*j+i] +=
            xyz_to_rec2020[3*j+k] * cam_to_xyz[3*k+i];
        for(int k=0;k<9;k++)
          ip->cam_to_rec2020[k] = cam_to_rec2020[k];
      }
    }
    return 0;
  }
  catch(Exiv2::AnyError &e)
  {
    std::string s(e.what());
    std::cerr << "[exiv2 dt_exif_read_exif_data] " << s << std::endl;
    return 1;
  }
#undef FIND_EXIF_TAG
}
} // end anonymous namespace

/** read the metadata of an image.
 * XMP data trumps IPTC data trumps EXIF data
 */
int dt_exif_read(dt_image_params_t *ip, const char *path)
{
  // at least set datetime taken to something useful in case there is no exif
  // data in this file (pfm, png, ...)
  struct stat statbuf;

  if(!stat(path, &statbuf))
  {
    struct tm result;
    strftime(ip->datetime, 20, "%Y:%m:%d %H:%M:%S", localtime_r(&statbuf.st_mtime, &result));
  }

  try
  {
    std::unique_ptr<Exiv2::Image> image(Exiv2::ImageFactory::open(path));
    assert(image.get() != 0);
    image->readMetadata();

    // EXIF metadata
    Exiv2::ExifData &exifData = image->exifData();
    if(!exifData.empty())
      return dt_exif_read_exif_data(ip, exifData);
    return 1;
  }
  catch(Exiv2::AnyError &e)
  {
    std::string s(e.what());
    std::cerr << "[exiv2 dt_exif_read] " << path << ": " << s << std::endl;
    return 1;
  }
}
