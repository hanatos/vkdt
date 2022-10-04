#pragma once
// stolen from https://raw.githubusercontent.com/kloper/libjpeg/master/jpegexiforient.c
/*
 * jpegexiforient.c
 *
 * This is a utility program to get and set the Exif Orientation Tag.
 * It can be used together with jpegtran in scripts for automatic
 * orientation correction of digital camera pictures.
 *
 * The Exif orientation value gives the orientation of the camera
 * relative to the scene when the image was captured.  The relation
 * of the '0th row' and '0th column' to visual position is shown as
 * below.
 *
 * Value | 0th Row     | 0th Column
 * ------+-------------+-----------
 *   1   | top         | left side
 *   2   | top         | rigth side
 *   3   | bottom      | rigth side
 *   4   | bottom      | left side
 *   5   | left side   | top
 *   6   | right side  | top
 *   7   | right side  | bottom
 *   8   | left side   | bottom
 *
 * For convenience, here is what the letter F would look like if it were
 * tagged correctly and displayed by a program that ignores the orientation
 * tag:
 *
 *   1        2       3      4         5            6           7          8
 *
 * 888888  888888      88  88      8888888888  88                  88  8888888888
 * 88          88      88  88      88  88      88  88          88  88      88  88
 * 8888      8888    8888  8888    88          8888888888  8888888888          88
 * 88          88      88  88
 * 88          88  888888  888888
 *
 */

#include <stdio.h>
#include <stdlib.h>

/* Return next input byte, or EOF if no more */
#define NEXTBYTE()  getc(myfile)

/* Read one byte, testing for EOF */
static int
read_1_byte (FILE *myfile)
{
  int c;
  c = NEXTBYTE();
  return c;
}

/* Read 2 bytes, convert to unsigned int */
/* All 2-byte quantities in JPEG markers are MSB first */
static unsigned int
read_2_bytes (FILE *myfile)
{
  int c1, c2;

  c1 = NEXTBYTE();
  if (c1 == EOF) return EOF;
  c2 = NEXTBYTE();
  if (c2 == EOF) return EOF;
  return (((unsigned int) c1) << 8) + ((unsigned int) c2);
}

static inline int
jpg_read_orientation(FILE *myfile)
{
  unsigned char exif_data[65536L];

  int set_flag = 0;
  unsigned int length, i;
  int is_motorola; /* Flag for byte order */
  unsigned int offset, number_of_tags, tagnum;

  /* Read File head, check for JPEG SOI + Exif APP1 */
  for (i = 0; i < 4; i++)
    exif_data[i] = (unsigned char) read_1_byte(myfile);
  if (exif_data[0] != 0xFF ||
      exif_data[1] != 0xD8) goto abort; // no JPEG SOI

  i = 0;
  while(i++ < 40 && (exif_data[2] != 0xff || exif_data[3] != 0xe1)) // search for Exif APP1
  {
    exif_data[2] = (unsigned char) read_1_byte(myfile);
    exif_data[3] = (unsigned char) read_1_byte(myfile);
  }
  if(i >= 40) goto abort; // give up

  /* Get the marker parameter length count */
  length = read_2_bytes(myfile);
  /* Length includes itself, so must be at least 2 */
  /* Following Exif data length must be at least 6 */
  if (length < 8) goto abort;
  length -= 8;
  /* Read Exif head, check for "Exif" */
  for (i = 0; i < 6; i++)
    exif_data[i] = (unsigned char) read_1_byte(myfile);
  if (exif_data[0] != 0x45 ||
      exif_data[1] != 0x78 ||
      exif_data[2] != 0x69 ||
      exif_data[3] != 0x66 ||
      exif_data[4] != 0 ||
      exif_data[5] != 0)
    goto abort;
  /* Read Exif body */
  for (i = 0; i < length; i++)
  {
    int res = read_1_byte(myfile);
    if(res == EOF) goto abort;
    exif_data[i] = (unsigned char)res;
  }

  if (length < 12) goto abort; /* Length of an IFD entry */

  /* Discover byte order */
  if (exif_data[0] == 0x49 && exif_data[1] == 0x49)
    is_motorola = 0;
  else if (exif_data[0] == 0x4D && exif_data[1] == 0x4D)
    is_motorola = 1;
  else
    goto abort;

  /* Check Tag Mark */
  if (is_motorola) {
    if (exif_data[2] != 0) goto abort;
    if (exif_data[3] != 0x2A) goto abort;
  } else {
    if (exif_data[3] != 0) goto abort;
    if (exif_data[2] != 0x2A) goto abort;
  }

  /* Get first IFD offset (offset to IFD0) */
  if (is_motorola) {
    if (exif_data[4] != 0) goto abort;
    if (exif_data[5] != 0) goto abort;
    offset = exif_data[6];
    offset <<= 8;
    offset += exif_data[7];
  } else {
    if (exif_data[7] != 0) goto abort;
    if (exif_data[6] != 0) goto abort;
    offset = exif_data[5];
    offset <<= 8;
    offset += exif_data[4];
  }
  if (offset > length - 2) goto abort; /* check end of data segment */

  /* Get the number of directory entries contained in this IFD */
  if (is_motorola) {
    number_of_tags = exif_data[offset];
    number_of_tags <<= 8;
    number_of_tags += exif_data[offset+1];
  } else {
    number_of_tags = exif_data[offset+1];
    number_of_tags <<= 8;
    number_of_tags += exif_data[offset];
  }
  if (number_of_tags == 0) goto abort;
  offset += 2;

  /* Search for Orientation Tag in IFD0 */
  for (;;) {
    if (offset > length - 12) goto abort; /* check end of data segment */
    /* Get Tag number */
    if (is_motorola) {
      tagnum = exif_data[offset];
      tagnum <<= 8;
      tagnum += exif_data[offset+1];
    } else {
      tagnum = exif_data[offset+1];
      tagnum <<= 8;
      tagnum += exif_data[offset];
    }
    if (tagnum == 0x0112) break; /* found Orientation Tag */
    if (--number_of_tags == 0) goto abort;
    offset += 12;
  }

  /* Get the Orientation value */
  if (is_motorola) {
    if (exif_data[offset+8] != 0) goto abort;
    set_flag = exif_data[offset+9];
  } else {
    if (exif_data[offset+9] != 0) goto abort;
    set_flag = exif_data[offset+8];
  }
  if (set_flag > 8) goto abort;

abort:
  fclose(myfile);
  /* Write out Orientation value */
  return set_flag;
}
