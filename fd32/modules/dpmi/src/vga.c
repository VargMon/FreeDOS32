/* DPMI Driver for FD32: VGA calls
 *
 * Copyright (C) 2001,2002 the LGPL VGABios developers Team
 */

#include <dr-env.h>
#include "vga.h"

static VGAMODE vga_modes[MODE_MAX+1]=
{//mode  vesa   class  model   pg bits sw   sh  tw  th  cw ch  sstart  slength misc  pelm  crtc  actl  gdc   sequ  dac
 {0x00, 0xFFFF, TEXT,  CTEXT,   8, 4, 360, 400, 40, 25, 9, 16, 0xB800, 0x0800, 0x67, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x02},
 {0x01, 0xFFFF, TEXT,  CTEXT,   8, 4, 360, 400, 40, 25, 9, 16, 0xB800, 0x0800, 0x67, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x02},
 {0x02, 0xFFFF, TEXT,  CTEXT,   4, 4, 720, 400, 80, 25, 9, 16, 0xB800, 0x1000, 0x67, 0xFF, 0x01, 0x00, 0x00, 0x01, 0x02},
 {0x03, 0xFFFF, TEXT,  CTEXT,   4, 4, 720, 400, 80, 25, 9, 16, 0xB800, 0x1000, 0x67, 0xFF, 0x01, 0x00, 0x00, 0x01, 0x02},
 {0x04, 0xFFFF, GRAPH, CGA,     4, 2, 320, 200, 40, 25, 8, 8,  0xB800, 0x0800, 0x63, 0xFF, 0x02, 0x01, 0x01, 0x02, 0x01},
 {0x05, 0xFFFF, GRAPH, CGA,     1, 2, 320, 200, 40, 25, 8, 8,  0xB800, 0x0800, 0x63, 0xFF, 0x02, 0x01, 0x01, 0x02, 0x01},
 {0x06, 0xFFFF, GRAPH, CGA,     1, 1, 640, 200, 80, 25, 8, 8,  0xB800, 0x1000, 0x63, 0xFF, 0x03, 0x02, 0x02, 0x03, 0x01},
 {0x07, 0xFFFF, TEXT,  MTEXT,   4, 4, 720, 400, 80, 25, 9, 16, 0xB000, 0x1000, 0x66, 0xFF, 0x04, 0x03, 0x03, 0x01, 0x00},
 {0x0D, 0xFFFF, GRAPH, PLANAR4, 8, 4, 320, 200, 40, 25, 8, 8,  0xA000, 0x2000, 0x63, 0xFF, 0x05, 0x04, 0x04, 0x04, 0x01},
 {0x0E, 0xFFFF, GRAPH, PLANAR4, 4, 4, 640, 200, 80, 25, 8, 8,  0xA000, 0x4000, 0x63, 0xFF, 0x06, 0x04, 0x04, 0x05, 0x01},
 {0x0F, 0xFFFF, GRAPH, PLANAR1, 2, 1, 640, 350, 80, 25, 8, 14, 0xA000, 0x8000, 0xa3, 0xFF, 0x07, 0x05, 0x04, 0x05, 0x00},
 {0x10, 0xFFFF, GRAPH, PLANAR4, 2, 4, 640, 350, 80, 25, 8, 14, 0xA000, 0x8000, 0xa3, 0xFF, 0x07, 0x06, 0x04, 0x05, 0x02},
 {0x11, 0xFFFF, GRAPH, PLANAR1, 1, 1, 640, 480, 80, 30, 8, 16, 0xA000, 0x0000, 0xe3, 0xFF, 0x08, 0x07, 0x04, 0x05, 0x02},
 {0x12, 0xFFFF, GRAPH, PLANAR4, 1, 4, 640, 480, 80, 30, 8, 16, 0xA000, 0x0000, 0xe3, 0xFF, 0x08, 0x06, 0x04, 0x05, 0x02},
 {0x13, 0xFFFF, GRAPH, LINEAR8, 1, 8, 320, 200, 40, 25, 8, 8,  0xA000, 0x0000, 0x63, 0xFF, 0x09, 0x08, 0x05, 0x06, 0x03},
 {0x6A, 0xFFFF, GRAPH, PLANAR4, 1, 4, 800, 600,100, 37, 8, 16, 0xA000, 0x0000, 0xe3, 0xFF, 0x0A, 0x06, 0x04, 0x05, 0x02}
};

/* CRTC */
#define CRTC_MAX_REG   0x18
#define CRTC_MAX_MODEL 0x0A

static uint8_t crtc_regs[CRTC_MAX_MODEL+1][CRTC_MAX_REG+1]=
{/* Model   00   01   02   03   04   05   06   07   08   09   0A   0B   0C   0D   0E   0F   10   11   12   13   14   15   16   17   18 */
 /* 00 */ {0x2d,0x27,0x28,0x90,0x2b,0xa0,0xbf,0x1f,0x00,0x4f,0x0d,0x0e,0x00,0x00,0x00,0x00,0x9c,0x8e,0x8f,0x14,0x1f,0x96,0xb9,0xa3,0xff},
 /* 01 */ {0x5f,0x4f,0x50,0x82,0x55,0x81,0xbf,0x1f,0x00,0x4f,0x0d,0x0e,0x00,0x00,0x00,0x00,0x9c,0x8e,0x8f,0x28,0x1f,0x96,0xb9,0xa3,0xff},
 /* 02 */ {0x2d,0x27,0x28,0x90,0x2b,0x80,0xbf,0x1f,0x00,0xc1,0x00,0x00,0x00,0x00,0x00,0x00,0x9c,0x8e,0x8f,0x14,0x00,0x96,0xb9,0xa2,0xff},
 /* 03 */ {0x5f,0x4f,0x50,0x82,0x54,0x80,0xbf,0x1f,0x00,0xc1,0x00,0x00,0x00,0x00,0x00,0x00,0x9c,0x8e,0x8f,0x28,0x00,0x96,0xb9,0xc2,0xff},
 /* 04 */ {0x5f,0x4f,0x50,0x82,0x55,0x81,0xbf,0x1f,0x00,0x4f,0x0d,0x0e,0x00,0x00,0x00,0x00,0x9c,0x8e,0x8f,0x28,0x0f,0x96,0xb9,0xa3,0xff},
 /* 05 */ {0x2d,0x27,0x28,0x90,0x2b,0x80,0xbf,0x1f,0x00,0xc0,0x00,0x00,0x00,0x00,0x00,0x00,0x9c,0x8e,0x8f,0x14,0x00,0x96,0xb9,0xe3,0xff},
 /* 06 */ {0x5f,0x4f,0x50,0x82,0x54,0x80,0xbf,0x1f,0x00,0xc0,0x00,0x00,0x00,0x00,0x00,0x00,0x9c,0x8e,0x8f,0x28,0x00,0x96,0xb9,0xe3,0xff},
 /* 07 */ {0x5f,0x4f,0x50,0x82,0x54,0x80,0xbf,0x1f,0x00,0x40,0x00,0x00,0x00,0x00,0x00,0x00,0x83,0x85,0x5d,0x28,0x0f,0x63,0xba,0xe3,0xff},
 /* 08 */ {0x5f,0x4f,0x50,0x82,0x54,0x80,0x0b,0x3e,0x00,0x40,0x00,0x00,0x00,0x00,0x00,0x00,0xea,0x8c,0xdf,0x28,0x00,0xe7,0x04,0xe3,0xff},
 /* 09 */ {0x5f,0x4f,0x50,0x82,0x54,0x80,0xbf,0x1f,0x00,0x41,0x00,0x00,0x00,0x00,0x00,0x00,0x9c,0x8e,0x8f,0x28,0x40,0x96,0xb9,0xa3,0xff},
 /* 0A */ {0x7f,0x63,0x63,0x83,0x6b,0x1b,0x72,0xf0,0x00,0x60,0x00,0x00,0x00,0x00,0x00,0x00,0x59,0x8d,0x57,0x32,0x00,0x57,0x73,0xe3,0xff}
};

/* Attribute Controler 0x3c0 */
#define ACTL_MAX_REG   0x14
#define ACTL_MAX_MODEL 0x08

static uint8_t actl_regs[ACTL_MAX_MODEL+1][ACTL_MAX_REG+1]=
{/* Model   00   01   02   03   04   05   06   07   08   09   0A   0B   OC   OD   OE   OF   10   11   12   13   14 */
 /* 00 */ {0x00,0x01,0x02,0x03,0x04,0x05,0x14,0x07,0x38,0x39,0x3a,0x3b,0x3c,0x3d,0x3e,0x3f,0x0c,0x00,0x0f,0x08,0x00},
 /* 01 */ {0x00,0x13,0x15,0x17,0x02,0x04,0x06,0x07,0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x01,0x00,0x03,0x00,0x00},
 /* 02 */ {0x00,0x17,0x17,0x17,0x17,0x17,0x17,0x17,0x17,0x17,0x17,0x17,0x17,0x17,0x17,0x17,0x01,0x00,0x01,0x00,0x00},
 /* 03 */ {0x00,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x10,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x0e,0x00,0x0f,0x08,0x00},
 /* 04 */ {0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x01,0x00,0x0f,0x00,0x00},
 /* 05 */ {0x00,0x08,0x00,0x00,0x18,0x18,0x00,0x00,0x00,0x08,0x00,0x00,0x00,0x18,0x00,0x00,0x01,0x00,0x01,0x00,0x00},
 /* 06 */ {0x00,0x01,0x02,0x03,0x04,0x05,0x14,0x07,0x38,0x39,0x3a,0x3b,0x3c,0x3d,0x3e,0x3f,0x01,0x00,0x0f,0x00,0x00},
 /* 07 */ {0x00,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x01,0x00,0x01,0x00,0x00},
 /* 08 */ {0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,0x41,0x00,0x0f,0x00,0x00}
};

/* Sequencer 0x3c4 */
#define SEQU_MAX_REG   0x04
#define SEQU_MAX_MODEL 0x06

static uint8_t sequ_regs[SEQU_MAX_MODEL+1][SEQU_MAX_REG+1]=
{/* Model   00   01   02   03   04 */
 /* 00 */ {0x03,0x08,0x03,0x00,0x02},
 /* 01 */ {0x03,0x00,0x03,0x00,0x02},
 /* 02 */ {0x03,0x09,0x03,0x00,0x02},
 /* 03 */ {0x03,0x01,0x01,0x00,0x06},
 /* 04 */ {0x03,0x09,0x0f,0x00,0x06},
 /* 05 */ {0x03,0x01,0x0f,0x00,0x06},
 /* 06 */ {0x03,0x01,0x0f,0x00,0x0e}
};

/* Graphic ctl 0x3ce */
#define GRDC_MAX_REG   0x08
#define GRDC_MAX_MODEL 0x05

static uint8_t grdc_regs[GRDC_MAX_MODEL+1][GRDC_MAX_REG+1]=
{/* Model   00   01   02   03   04   05   06   07   08 */
 /* 00 */ {0x00,0x00,0x00,0x00,0x00,0x10,0x0e,0x0f,0xff},
 /* 01 */ {0x00,0x00,0x00,0x00,0x00,0x30,0x0f,0x0f,0xff},
 /* 02 */ {0x00,0x00,0x00,0x00,0x00,0x00,0x0d,0x0f,0xff},
 /* 03 */ {0x00,0x00,0x00,0x00,0x00,0x10,0x0a,0x0f,0xff},
 /* 04 */ {0x00,0x00,0x00,0x00,0x00,0x00,0x05,0x0f,0xff},
 /* 05 */ {0x00,0x00,0x00,0x00,0x00,0x40,0x05,0x0f,0xff}
};

/* Default Palette */
#define DAC_MAX_MODEL 3

static uint8_t dac_regs[DAC_MAX_MODEL+1]=
{0x3f,0x3f,0x3f,0xff};

/* Mono */
static uint8_t palette0[63+1][3]=
{
  {0x00,0x00,0x00}, {0x00,0x00,0x00}, {0x00,0x00,0x00}, {0x00,0x00,0x00}, {0x00,0x00,0x00}, {0x00,0x00,0x00}, {0x00,0x00,0x00}, {0x00,0x00,0x00},
  {0x2a,0x2a,0x2a}, {0x2a,0x2a,0x2a}, {0x2a,0x2a,0x2a}, {0x2a,0x2a,0x2a}, {0x2a,0x2a,0x2a}, {0x2a,0x2a,0x2a}, {0x2a,0x2a,0x2a}, {0x2a,0x2a,0x2a},
  {0x2a,0x2a,0x2a}, {0x2a,0x2a,0x2a}, {0x2a,0x2a,0x2a}, {0x2a,0x2a,0x2a}, {0x2a,0x2a,0x2a}, {0x2a,0x2a,0x2a}, {0x2a,0x2a,0x2a}, {0x2a,0x2a,0x2a},
  {0x3f,0x3f,0x3f}, {0x3f,0x3f,0x3f}, {0x3f,0x3f,0x3f}, {0x3f,0x3f,0x3f}, {0x3f,0x3f,0x3f}, {0x3f,0x3f,0x3f}, {0x3f,0x3f,0x3f}, {0x3f,0x3f,0x3f},
  {0x00,0x00,0x00}, {0x00,0x00,0x00}, {0x00,0x00,0x00}, {0x00,0x00,0x00}, {0x00,0x00,0x00}, {0x00,0x00,0x00}, {0x00,0x00,0x00}, {0x00,0x00,0x00},
  {0x2a,0x2a,0x2a}, {0x2a,0x2a,0x2a}, {0x2a,0x2a,0x2a}, {0x2a,0x2a,0x2a}, {0x2a,0x2a,0x2a}, {0x2a,0x2a,0x2a}, {0x2a,0x2a,0x2a}, {0x2a,0x2a,0x2a},
  {0x2a,0x2a,0x2a}, {0x2a,0x2a,0x2a}, {0x2a,0x2a,0x2a}, {0x2a,0x2a,0x2a}, {0x2a,0x2a,0x2a}, {0x2a,0x2a,0x2a}, {0x2a,0x2a,0x2a}, {0x2a,0x2a,0x2a},
  {0x3f,0x3f,0x3f}, {0x3f,0x3f,0x3f}, {0x3f,0x3f,0x3f}, {0x3f,0x3f,0x3f}, {0x3f,0x3f,0x3f}, {0x3f,0x3f,0x3f}, {0x3f,0x3f,0x3f}, {0x3f,0x3f,0x3f} 
};

static uint8_t palette1[63+1][3]=
{
  {0x00,0x00,0x00}, {0x00,0x00,0x2a}, {0x00,0x2a,0x00}, {0x00,0x2a,0x2a}, {0x2a,0x00,0x00}, {0x2a,0x00,0x2a}, {0x2a,0x15,0x00}, {0x2a,0x2a,0x2a},
  {0x00,0x00,0x00}, {0x00,0x00,0x2a}, {0x00,0x2a,0x00}, {0x00,0x2a,0x2a}, {0x2a,0x00,0x00}, {0x2a,0x00,0x2a}, {0x2a,0x15,0x00}, {0x2a,0x2a,0x2a},
  {0x15,0x15,0x15}, {0x15,0x15,0x3f}, {0x15,0x3f,0x15}, {0x15,0x3f,0x3f}, {0x3f,0x15,0x15}, {0x3f,0x15,0x3f}, {0x3f,0x3f,0x15}, {0x3f,0x3f,0x3f},
  {0x15,0x15,0x15}, {0x15,0x15,0x3f}, {0x15,0x3f,0x15}, {0x15,0x3f,0x3f}, {0x3f,0x15,0x15}, {0x3f,0x15,0x3f}, {0x3f,0x3f,0x15}, {0x3f,0x3f,0x3f},
  {0x00,0x00,0x00}, {0x00,0x00,0x2a}, {0x00,0x2a,0x00}, {0x00,0x2a,0x2a}, {0x2a,0x00,0x00}, {0x2a,0x00,0x2a}, {0x2a,0x15,0x00}, {0x2a,0x2a,0x2a},
  {0x00,0x00,0x00}, {0x00,0x00,0x2a}, {0x00,0x2a,0x00}, {0x00,0x2a,0x2a}, {0x2a,0x00,0x00}, {0x2a,0x00,0x2a}, {0x2a,0x15,0x00}, {0x2a,0x2a,0x2a},
  {0x15,0x15,0x15}, {0x15,0x15,0x3f}, {0x15,0x3f,0x15}, {0x15,0x3f,0x3f}, {0x3f,0x15,0x15}, {0x3f,0x15,0x3f}, {0x3f,0x3f,0x15}, {0x3f,0x3f,0x3f},
  {0x15,0x15,0x15}, {0x15,0x15,0x3f}, {0x15,0x3f,0x15}, {0x15,0x3f,0x3f}, {0x3f,0x15,0x15}, {0x3f,0x15,0x3f}, {0x3f,0x3f,0x15}, {0x3f,0x3f,0x3f} 
};

static uint8_t palette2[63+1][3]=
{
  {0x00,0x00,0x00}, {0x00,0x00,0x2a}, {0x00,0x2a,0x00}, {0x00,0x2a,0x2a}, {0x2a,0x00,0x00}, {0x2a,0x00,0x2a}, {0x2a,0x2a,0x00}, {0x2a,0x2a,0x2a},
  {0x00,0x00,0x15}, {0x00,0x00,0x3f}, {0x00,0x2a,0x15}, {0x00,0x2a,0x3f}, {0x2a,0x00,0x15}, {0x2a,0x00,0x3f}, {0x2a,0x2a,0x15}, {0x2a,0x2a,0x3f},
  {0x00,0x15,0x00}, {0x00,0x15,0x2a}, {0x00,0x3f,0x00}, {0x00,0x3f,0x2a}, {0x2a,0x15,0x00}, {0x2a,0x15,0x2a}, {0x2a,0x3f,0x00}, {0x2a,0x3f,0x2a},
  {0x00,0x15,0x15}, {0x00,0x15,0x3f}, {0x00,0x3f,0x15}, {0x00,0x3f,0x3f}, {0x2a,0x15,0x15}, {0x2a,0x15,0x3f}, {0x2a,0x3f,0x15}, {0x2a,0x3f,0x3f},
  {0x15,0x00,0x00}, {0x15,0x00,0x2a}, {0x15,0x2a,0x00}, {0x15,0x2a,0x2a}, {0x3f,0x00,0x00}, {0x3f,0x00,0x2a}, {0x3f,0x2a,0x00}, {0x3f,0x2a,0x2a},
  {0x15,0x00,0x15}, {0x15,0x00,0x3f}, {0x15,0x2a,0x15}, {0x15,0x2a,0x3f}, {0x3f,0x00,0x15}, {0x3f,0x00,0x3f}, {0x3f,0x2a,0x15}, {0x3f,0x2a,0x3f},
  {0x15,0x15,0x00}, {0x15,0x15,0x2a}, {0x15,0x3f,0x00}, {0x15,0x3f,0x2a}, {0x3f,0x15,0x00}, {0x3f,0x15,0x2a}, {0x3f,0x3f,0x00}, {0x3f,0x3f,0x2a},
  {0x15,0x15,0x15}, {0x15,0x15,0x3f}, {0x15,0x3f,0x15}, {0x15,0x3f,0x3f}, {0x3f,0x15,0x15}, {0x3f,0x15,0x3f}, {0x3f,0x3f,0x15}, {0x3f,0x3f,0x3f} 
};

static uint8_t palette3[256][3]=
{
  {0x00,0x00,0x00}, {0x00,0x00,0x2a}, {0x00,0x2a,0x00}, {0x00,0x2a,0x2a}, {0x2a,0x00,0x00}, {0x2a,0x00,0x2a}, {0x2a,0x15,0x00}, {0x2a,0x2a,0x2a},
  {0x15,0x15,0x15}, {0x15,0x15,0x3f}, {0x15,0x3f,0x15}, {0x15,0x3f,0x3f}, {0x3f,0x15,0x15}, {0x3f,0x15,0x3f}, {0x3f,0x3f,0x15}, {0x3f,0x3f,0x3f},
  {0x00,0x00,0x00}, {0x05,0x05,0x05}, {0x08,0x08,0x08}, {0x0b,0x0b,0x0b}, {0x0e,0x0e,0x0e}, {0x11,0x11,0x11}, {0x14,0x14,0x14}, {0x18,0x18,0x18},
  {0x1c,0x1c,0x1c}, {0x20,0x20,0x20}, {0x24,0x24,0x24}, {0x28,0x28,0x28}, {0x2d,0x2d,0x2d}, {0x32,0x32,0x32}, {0x38,0x38,0x38}, {0x3f,0x3f,0x3f},
  {0x00,0x00,0x3f}, {0x10,0x00,0x3f}, {0x1f,0x00,0x3f}, {0x2f,0x00,0x3f}, {0x3f,0x00,0x3f}, {0x3f,0x00,0x2f}, {0x3f,0x00,0x1f}, {0x3f,0x00,0x10},
  {0x3f,0x00,0x00}, {0x3f,0x10,0x00}, {0x3f,0x1f,0x00}, {0x3f,0x2f,0x00}, {0x3f,0x3f,0x00}, {0x2f,0x3f,0x00}, {0x1f,0x3f,0x00}, {0x10,0x3f,0x00},
  {0x00,0x3f,0x00}, {0x00,0x3f,0x10}, {0x00,0x3f,0x1f}, {0x00,0x3f,0x2f}, {0x00,0x3f,0x3f}, {0x00,0x2f,0x3f}, {0x00,0x1f,0x3f}, {0x00,0x10,0x3f},
  {0x1f,0x1f,0x3f}, {0x27,0x1f,0x3f}, {0x2f,0x1f,0x3f}, {0x37,0x1f,0x3f}, {0x3f,0x1f,0x3f}, {0x3f,0x1f,0x37}, {0x3f,0x1f,0x2f}, {0x3f,0x1f,0x27},
                                                                                                                                                
  {0x3f,0x1f,0x1f}, {0x3f,0x27,0x1f}, {0x3f,0x2f,0x1f}, {0x3f,0x37,0x1f}, {0x3f,0x3f,0x1f}, {0x37,0x3f,0x1f}, {0x2f,0x3f,0x1f}, {0x27,0x3f,0x1f},
  {0x1f,0x3f,0x1f}, {0x1f,0x3f,0x27}, {0x1f,0x3f,0x2f}, {0x1f,0x3f,0x37}, {0x1f,0x3f,0x3f}, {0x1f,0x37,0x3f}, {0x1f,0x2f,0x3f}, {0x1f,0x27,0x3f},
  {0x2d,0x2d,0x3f}, {0x31,0x2d,0x3f}, {0x36,0x2d,0x3f}, {0x3a,0x2d,0x3f}, {0x3f,0x2d,0x3f}, {0x3f,0x2d,0x3a}, {0x3f,0x2d,0x36}, {0x3f,0x2d,0x31},
  {0x3f,0x2d,0x2d}, {0x3f,0x31,0x2d}, {0x3f,0x36,0x2d}, {0x3f,0x3a,0x2d}, {0x3f,0x3f,0x2d}, {0x3a,0x3f,0x2d}, {0x36,0x3f,0x2d}, {0x31,0x3f,0x2d},
  {0x2d,0x3f,0x2d}, {0x2d,0x3f,0x31}, {0x2d,0x3f,0x36}, {0x2d,0x3f,0x3a}, {0x2d,0x3f,0x3f}, {0x2d,0x3a,0x3f}, {0x2d,0x36,0x3f}, {0x2d,0x31,0x3f},
  {0x00,0x00,0x1c}, {0x07,0x00,0x1c}, {0x0e,0x00,0x1c}, {0x15,0x00,0x1c}, {0x1c,0x00,0x1c}, {0x1c,0x00,0x15}, {0x1c,0x00,0x0e}, {0x1c,0x00,0x07},
  {0x1c,0x00,0x00}, {0x1c,0x07,0x00}, {0x1c,0x0e,0x00}, {0x1c,0x15,0x00}, {0x1c,0x1c,0x00}, {0x15,0x1c,0x00}, {0x0e,0x1c,0x00}, {0x07,0x1c,0x00},
  {0x00,0x1c,0x00}, {0x00,0x1c,0x07}, {0x00,0x1c,0x0e}, {0x00,0x1c,0x15}, {0x00,0x1c,0x1c}, {0x00,0x15,0x1c}, {0x00,0x0e,0x1c}, {0x00,0x07,0x1c},
                                                                                                                                                
  {0x0e,0x0e,0x1c}, {0x11,0x0e,0x1c}, {0x15,0x0e,0x1c}, {0x18,0x0e,0x1c}, {0x1c,0x0e,0x1c}, {0x1c,0x0e,0x18}, {0x1c,0x0e,0x15}, {0x1c,0x0e,0x11},
  {0x1c,0x0e,0x0e}, {0x1c,0x11,0x0e}, {0x1c,0x15,0x0e}, {0x1c,0x18,0x0e}, {0x1c,0x1c,0x0e}, {0x18,0x1c,0x0e}, {0x15,0x1c,0x0e}, {0x11,0x1c,0x0e},
  {0x0e,0x1c,0x0e}, {0x0e,0x1c,0x11}, {0x0e,0x1c,0x15}, {0x0e,0x1c,0x18}, {0x0e,0x1c,0x1c}, {0x0e,0x18,0x1c}, {0x0e,0x15,0x1c}, {0x0e,0x11,0x1c},
  {0x14,0x14,0x1c}, {0x16,0x14,0x1c}, {0x18,0x14,0x1c}, {0x1a,0x14,0x1c}, {0x1c,0x14,0x1c}, {0x1c,0x14,0x1a}, {0x1c,0x14,0x18}, {0x1c,0x14,0x16},
  {0x1c,0x14,0x14}, {0x1c,0x16,0x14}, {0x1c,0x18,0x14}, {0x1c,0x1a,0x14}, {0x1c,0x1c,0x14}, {0x1a,0x1c,0x14}, {0x18,0x1c,0x14}, {0x16,0x1c,0x14},
  {0x14,0x1c,0x14}, {0x14,0x1c,0x16}, {0x14,0x1c,0x18}, {0x14,0x1c,0x1a}, {0x14,0x1c,0x1c}, {0x14,0x1a,0x1c}, {0x14,0x18,0x1c}, {0x14,0x16,0x1c},
  {0x00,0x00,0x10}, {0x04,0x00,0x10}, {0x08,0x00,0x10}, {0x0c,0x00,0x10}, {0x10,0x00,0x10}, {0x10,0x00,0x0c}, {0x10,0x00,0x08}, {0x10,0x00,0x04},
  {0x10,0x00,0x00}, {0x10,0x04,0x00}, {0x10,0x08,0x00}, {0x10,0x0c,0x00}, {0x10,0x10,0x00}, {0x0c,0x10,0x00}, {0x08,0x10,0x00}, {0x04,0x10,0x00},
                                                                                                                                                
  {0x00,0x10,0x00}, {0x00,0x10,0x04}, {0x00,0x10,0x08}, {0x00,0x10,0x0c}, {0x00,0x10,0x10}, {0x00,0x0c,0x10}, {0x00,0x08,0x10}, {0x00,0x04,0x10},
  {0x08,0x08,0x10}, {0x0a,0x08,0x10}, {0x0c,0x08,0x10}, {0x0e,0x08,0x10}, {0x10,0x08,0x10}, {0x10,0x08,0x0e}, {0x10,0x08,0x0c}, {0x10,0x08,0x0a},
  {0x10,0x08,0x08}, {0x10,0x0a,0x08}, {0x10,0x0c,0x08}, {0x10,0x0e,0x08}, {0x10,0x10,0x08}, {0x0e,0x10,0x08}, {0x0c,0x10,0x08}, {0x0a,0x10,0x08},
  {0x08,0x10,0x08}, {0x08,0x10,0x0a}, {0x08,0x10,0x0c}, {0x08,0x10,0x0e}, {0x08,0x10,0x10}, {0x08,0x0e,0x10}, {0x08,0x0c,0x10}, {0x08,0x0a,0x10},
  {0x0b,0x0b,0x10}, {0x0c,0x0b,0x10}, {0x0d,0x0b,0x10}, {0x0f,0x0b,0x10}, {0x10,0x0b,0x10}, {0x10,0x0b,0x0f}, {0x10,0x0b,0x0d}, {0x10,0x0b,0x0c},
  {0x10,0x0b,0x0b}, {0x10,0x0c,0x0b}, {0x10,0x0d,0x0b}, {0x10,0x0f,0x0b}, {0x10,0x10,0x0b}, {0x0f,0x10,0x0b}, {0x0d,0x10,0x0b}, {0x0c,0x10,0x0b},
  {0x0b,0x10,0x0b}, {0x0b,0x10,0x0c}, {0x0b,0x10,0x0d}, {0x0b,0x10,0x0f}, {0x0b,0x10,0x10}, {0x0b,0x0f,0x10}, {0x0b,0x0d,0x10}, {0x0b,0x0c,0x10},
  {0x00,0x00,0x00}, {0x00,0x00,0x00}, {0x00,0x00,0x00}, {0x00,0x00,0x00}, {0x00,0x00,0x00}, {0x00,0x00,0x00}, {0x00,0x00,0x00}, {0x00,0x00,0x00}
};

/* Fonts pointers ... */ 
static uint8_t *vgafont8;
static uint8_t *vgafont16;

static void biosfn_get_font_info(uint8_t bh, uint16_t *es, uint16_t *bp, uint16_t *cx, uint16_t *dx)
{
  uint32_t f;
  X_REGS16 regs_r;
  X_SREGS16 selectors_r;
  f = ll_fsave();
  regs_r.x.ax = 0x1130;
  regs_r.h.bh = bh;
  vm86_callBIOS(0x10, &regs_r, &regs_r, &selectors_r);
  *cx = regs_r.x.cx;
  *dx = regs_r.x.dx;
  *bp = regs_r.x.bp;
  *es = selectors_r.es;
  ll_frestore(f);
}

void _vga_init(void)
{
  uint16_t seg, off, cx, dx;

  biosfn_get_font_info (0x03, &seg, &off, &cx, &dx);
  /* seg = 0xC000; */
  fd32_log_printf("[VGA] font 8, %x:%x\n", seg, off);
  vgafont8 = (uint8_t *)(seg<<4)+off;
  biosfn_get_font_info (0x06, &seg, &off, &cx, &dx);
  /* seg = 0xC000; */
  fd32_log_printf("[VGA] font 16, %x:%x\n", seg, off);
  vgafont16 = (uint8_t *)(seg<<4)+off;
}

/* Utils */
static void * memsetw(void *s, uint16_t pattern, size_t count)
{
  uint16_t *p = s;
  for ( ; count > 0; count --)
    *p++ = pattern;

  return s;
}

static uint8_t find_vga_entry(uint8_t mode) 
{
  uint32_t i;
  uint8_t line=0xFF;
  for(i = 0; i <= MODE_MAX; i++)
    if(vga_modes[i].svgamode == mode) {
      line = i;
      break;
    }
  return line;
}

static void get_font_access()
{
  outpw( VGAREG_SEQU_ADDRESS, 0x0100 );
  outpw( VGAREG_SEQU_ADDRESS, 0x0402 );
  outpw( VGAREG_SEQU_ADDRESS, 0x0704 );
  outpw( VGAREG_SEQU_ADDRESS, 0x0300 );
  outpw( VGAREG_GRDC_ADDRESS, 0x0204 );
  outpw( VGAREG_GRDC_ADDRESS, 0x0005 );
  outpw( VGAREG_GRDC_ADDRESS, 0x0406 );
}

static void release_font_access()
{
  outpw( VGAREG_SEQU_ADDRESS, 0x0100 );
  outpw( VGAREG_SEQU_ADDRESS, 0x0302 );
  outpw( VGAREG_SEQU_ADDRESS, 0x0304 );
  outpw( VGAREG_SEQU_ADDRESS, 0x0300 );
  outpw( VGAREG_GRDC_ADDRESS, 0x0004 );
  outpw( VGAREG_GRDC_ADDRESS, 0x1005 );
  outpw( VGAREG_GRDC_ADDRESS, 0x0e06 );
}

static void set_scan_lines(uint8_t lines)
{
  uint16_t crtc_addr, cols, vde;
  uint8_t crtc_r9, ovl, rows;

  crtc_addr = bios_da.video_crtc_port;
  outp(crtc_addr, 0x09);
  crtc_r9 = inp(crtc_addr+1);
  crtc_r9 = (crtc_r9 & 0xe0) | (lines - 1);
  outp(crtc_addr+1, crtc_r9);
  if(lines==8)
    vga_set_cursor_shape(0x06, 0x07);
  else
    vga_set_cursor_shape(lines-4,lines-3);
  bios_da.video_font_height = lines;
  outp(crtc_addr, 0x12);
  vde = inp(crtc_addr+1);
  outp(crtc_addr, 0x07);
  ovl = inp(crtc_addr+1);
  vde += (((ovl & 0x02) << 7) + ((ovl & 0x40) << 3) + 1);
  rows = vde / lines;
  bios_da.video_row_size = rows-1;
  cols = bios_da.video_column_size;
  bios_da.video_membuf_size = rows * cols * 2;
}

/* Functions */
void vga_set_cursor_pos(uint8_t page, uint16_t cursor)
{
  uint8_t xcurs, ycurs, current;
  uint16_t nbcols, nbrows, address, crtc_addr;

  /* Should not happen...*/
  if(page>7) return;

  /* Bios cursor pos */
  bios_da.video_cursor_pos[page] = cursor;

  /* Set the hardware cursor */
  current= bios_da.video_active_page;
  if(page == current) {
    /* Get the dimensions */
    nbcols = bios_da.video_column_size;
    nbrows = bios_da.video_row_size+1;

    xcurs=cursor&0x00ff, ycurs=(cursor&0xff00)>>8;

    /* Calculate the address knowing nbcols nbrows and page num */
    address = (((nbcols*nbrows)|0x00FF)+1)*page+xcurs+ycurs*nbcols;
   
    /* CRTC regs 0x0e and 0x0f */
    crtc_addr = bios_da.video_crtc_port;
    outp(crtc_addr,0x0e);
    outp(crtc_addr+1,(address&0xff00)>>8);
    outp(crtc_addr,0x0f);
    outp(crtc_addr+1,address&0x00ff);
  }
}

void vga_get_cursor_pos(uint8_t page, uint16_t *shape, uint16_t *pos)
{
  if(page < 8) {
    /* FIXME should handle VGA 14/16 lines */
    *shape = bios_da.video_cursor_shape;
    *pos = bios_da.video_cursor_pos[page];
  } else {
    /* Default */
    *shape = 0;
    *pos = 0;
  }
}

void vga_set_active_page(uint8_t page)
{
  uint16_t cursor, dummy, crtc_addr;
  uint16_t nbcols, nbrows, address;
  uint8_t mode, line;

  if(page>7) return;

  /* Get the mode */
  mode = bios_da.video_mode;
  line = find_vga_entry(mode);
  if(line==0xFF) return;

  /* Get pos curs pos for the right page */
  vga_get_cursor_pos(page, &dummy, &cursor);

  if(vga_modes[line].class == TEXT) {
    /* Get the dimensions */
    nbcols = bios_da.video_column_size;
    nbrows = bios_da.video_row_size+1;
  
    /* Calculate the address knowing nbcols nbrows and page num */
    bios_da.video_page_offset = (((nbcols*nbrows*2)|0x00FF)+1)*page;

    /* Start address */
    address = (((nbcols*nbrows)|0x00FF)+1)*page;
  } else {
    address = page*vga_modes[line].slength;
  }

  /* CRTC regs 0x0c and 0x0d */
  crtc_addr = bios_da.video_crtc_port;
  outp(crtc_addr,0x0c);
  outp(crtc_addr+1,(address&0xff00)>>8);
  outp(crtc_addr,0x0d);
  outp(crtc_addr+1,address&0x00ff);

  /* And change the BIOS page */
  bios_da.video_active_page = page;

#ifdef __DEBUG__
  fd32_log_printf("[VGA] Set active page %02x address %04x\n", page, address);
#endif

  /* Display the cursor, now the page is active */
  vga_set_cursor_pos(page, cursor);
}

void vga_set_cursor_shape(uint8_t ch, uint8_t cl) 
{
  uint16_t cheight,crtc_addr;

  ch &= 0x3f;
  cl &= 0x1f;

  bios_da.video_cursor_shape = (ch<<8)+cl;

  cheight = bios_da.video_font_height;
  if((cheight>8) && (cl<8) && (ch<0x20))
  {
    if(cl!=(ch+1))
      ch = ((ch+1) * cheight / 8) -1;
    else
      ch = ((cl+1) * cheight / 8) - 2;
    cl = ((cl+1) * cheight / 8) - 1;
  }

  /* CTRC regs 0x0a and 0x0b */
  crtc_addr = bios_da.video_crtc_port;
  outp(crtc_addr,0x0a);
  outp(crtc_addr+1,ch);
  outp(crtc_addr,0x0b);
  outp(crtc_addr+1,cl);
}

void vga_set_overscan_border_color(uint8_t value)
{
  inp(VGAREG_ACTL_RESET);
  outp(VGAREG_ACTL_ADDRESS, 0x11);
  outp(VGAREG_ACTL_WRITE_DATA, value);
  outp(VGAREG_ACTL_ADDRESS, 0x20);
}

void vga_set_all_palette_reg(uint8_t *pal)
{
  inp(VGAREG_ACTL_RESET);
  /* First the colors */
  #define VGAREG_WRITE_COLOR(c) outp(VGAREG_ACTL_ADDRESS, c); outp(VGAREG_ACTL_WRITE_DATA, pal[0]); pal++
  VGAREG_WRITE_COLOR(0x00); VGAREG_WRITE_COLOR(0x01);
  VGAREG_WRITE_COLOR(0x02); VGAREG_WRITE_COLOR(0x03);
  VGAREG_WRITE_COLOR(0x04); VGAREG_WRITE_COLOR(0x05);
  VGAREG_WRITE_COLOR(0x06); VGAREG_WRITE_COLOR(0x07);
  VGAREG_WRITE_COLOR(0x08); VGAREG_WRITE_COLOR(0x09);
  VGAREG_WRITE_COLOR(0x0A); VGAREG_WRITE_COLOR(0x0B);
  VGAREG_WRITE_COLOR(0x0C); VGAREG_WRITE_COLOR(0x0D);
  VGAREG_WRITE_COLOR(0x0E); VGAREG_WRITE_COLOR(0x0F);
  #undef VGAREG_WRITE_COLOR

  /* Then the border */
  outp(VGAREG_ACTL_ADDRESS, 0x11);
  outp(VGAREG_ACTL_WRITE_DATA, pal[0]);
  outp(VGAREG_ACTL_ADDRESS, 0x20);
}

void vga_get_all_palette_reg(uint8_t *pal)
{
  /* First the colors */
  #define VGAREG_READ_COLOR(c) inp(VGAREG_ACTL_RESET); outp(VGAREG_ACTL_ADDRESS, c); pal[0] = inp(VGAREG_ACTL_READ_DATA); pal++
  VGAREG_READ_COLOR(0x00); VGAREG_READ_COLOR(0x01);
  VGAREG_READ_COLOR(0x02); VGAREG_READ_COLOR(0x03);
  VGAREG_READ_COLOR(0x04); VGAREG_READ_COLOR(0x05);
  VGAREG_READ_COLOR(0x06); VGAREG_READ_COLOR(0x07);
  VGAREG_READ_COLOR(0x08); VGAREG_READ_COLOR(0x09);
  VGAREG_READ_COLOR(0x0A); VGAREG_READ_COLOR(0x0B);
  VGAREG_READ_COLOR(0x0C); VGAREG_READ_COLOR(0x0D);
  VGAREG_READ_COLOR(0x0E); VGAREG_READ_COLOR(0x0F);
  #undef VGAREG_READ_COLOR

  /* Then the border */
  inp(VGAREG_ACTL_RESET);
  outp(VGAREG_ACTL_ADDRESS, 0x11);
  pal[0] = inp(VGAREG_ACTL_READ_DATA);
  outp(VGAREG_ACTL_ADDRESS, 0x20);
}

void vga_toggle_intensity(uint8_t state) 
{
  uint32_t value;

  inp(VGAREG_ACTL_RESET);
  outp(VGAREG_ACTL_ADDRESS, 0x10);
  value = inp(VGAREG_ACTL_READ_DATA);
  value &= 0xF7;
  value |= (state&0x01)<<3;
  outp(VGAREG_ACTL_WRITE_DATA, value);
  outp(VGAREG_ACTL_ADDRESS, 0x20);
}

uint8_t vga_get_single_palette_reg(uint8_t reg)
{
  uint8_t value = 0;

  if(reg <= ACTL_MAX_REG)
  {
    inp(VGAREG_ACTL_RESET);
    outp(VGAREG_ACTL_ADDRESS, reg);
    value = inp(VGAREG_ACTL_READ_DATA);
    inp(VGAREG_ACTL_RESET);
    outp(VGAREG_ACTL_ADDRESS, 0x20);
  }
  
  return value;
}

void vga_set_single_palette_reg(uint8_t reg, uint8_t value)
{
  if(reg <= ACTL_MAX_REG)
  {
    inp(VGAREG_ACTL_RESET);
    outp(VGAREG_ACTL_ADDRESS, reg);
    outp(VGAREG_ACTL_WRITE_DATA, value);
    outp(VGAREG_ACTL_ADDRESS, 0x20);
  }
}

void vga_set_all_dac_reg(uint16_t start, uint16_t count, uint8_t *table)
{
  uint16_t i;
  outp(VGAREG_DAC_WRITE_ADDRESS, start);
  for(i=0; i<count; i++)
  {
    outp(VGAREG_DAC_DATA, *table++);
    outp(VGAREG_DAC_DATA, *table++);
    outp(VGAREG_DAC_DATA, *table++);
  }
}

void vga_set_single_dac_reg(uint16_t reg, uint8_t r, uint8_t g, uint8_t b)
{
  outp(VGAREG_DAC_WRITE_ADDRESS, reg);
  outp(VGAREG_DAC_DATA, r);
  outp(VGAREG_DAC_DATA, g);
  outp(VGAREG_DAC_DATA, b);
}

void vga_read_single_dac_reg(uint16_t reg, uint8_t *r, uint8_t *g, uint8_t *b)
{
  outp(VGAREG_DAC_READ_ADDRESS, reg);
  *r = inp(VGAREG_DAC_DATA); /* RED   */
  *g = inp(VGAREG_DAC_DATA); /* GREEN */
  *b = inp(VGAREG_DAC_DATA); /* BLUE  */
}

void vga_scroll(uint8_t nblines, uint8_t attr, uint8_t rul, uint8_t cul, uint8_t rlr, uint8_t clr, uint8_t page, uint8_t dir)
{
  /* page == 0xFF if current */
  uint8_t mode,line;
  uint16_t nbcols,nbrows,i;
  uint16_t address;

  if(rul>rlr)return;
  if(cul>clr)return;

  /* Get the mode */
  mode = bios_da.video_mode;
  line = find_vga_entry(mode);
  if(line==0xFF)return;

  /* Get the dimensions */
  nbrows = bios_da.video_row_size+1;
  nbcols = bios_da.video_column_size;

  /* Get the current page */
  if(page==0xFF)
    page = bios_da.video_active_page;

  if(vga_modes[line].class==TEXT)
  {
    /* Compute the address */
    address = ((((nbcols*nbrows*2)|0x00ff)+1)*page);
#ifdef __DEBUG__
    fd32_log_printf("Scroll, address %04x (%04x %04x %02x)\n", address, nbrows, nbcols, page);
#endif

    if(rlr >= nbrows) rlr = nbrows-1;
    if(clr >= nbcols) clr = nbcols-1;
    if(nblines > nbrows) nblines = 0;

    if(nblines==0&&rul==0&&cul==0&&rlr==nbrows-1&&clr==nbcols-1)
    {
      memsetw((uint8_t *)(vga_modes[line].sstart<<4)+address, (uint16_t)attr*0x100+' ', nbrows*nbcols);
    } else {/* if Scroll up */
      if(dir==SCROLL_UP) {
        for(i=rul;i<=rlr;i++)
        {
          if((i+nblines>rlr)||(nblines==0))
            memsetw((uint8_t *)(vga_modes[line].sstart<<4)+address+(i*nbcols+cul)*2, (uint16_t)attr*0x100+' ', clr-cul+1);
          else
            memcpy((uint8_t *)(vga_modes[line].sstart<<4)+address+(i*nbcols+cul)*2, (uint8_t *)(vga_modes[line].sstart<<4)+((i+nblines)*nbcols+cul)*2, (clr-cul+1)*2);
        }
      } else {
        for(i=rlr;i>=rul;i--)
        {
          if((i<rul+nblines)||(nblines==0))
            memsetw((uint8_t *)(vga_modes[line].sstart<<4)+address+(i*nbcols+cul)*2, (uint16_t)attr*0x100+' ', clr-cul+1);
          else
            memcpy((uint8_t *)(vga_modes[line].sstart<<4)+address+(i*nbcols+cul)*2, (uint8_t *)(vga_modes[line].sstart<<4)+((i-nblines)*nbcols+cul)*2, (clr-cul+1)*2);
        }
      }
    }
  } else {
    /* FIXME gfx mode */
#ifdef __DEBUG__
    fd32_log_printf("Scroll in graphics mode !\n");
#endif
  }
}

void vga_write_char_attr(uint8_t car, uint8_t page, uint8_t attr, uint16_t count)
{
  uint8_t xcurs,ycurs,mode,line;
  uint16_t nbcols,nbrows,address;
  uint16_t cursor,dummy;

  /* Get the mode */
  mode = bios_da.video_mode;
  line = find_vga_entry(mode);
  if(line==0xFF)return;

  /* Get the cursor pos for the page */
  vga_get_cursor_pos(page,&dummy,&cursor);
  xcurs=cursor&0x00ff;ycurs=(cursor&0xff00)>>8;

  /* Get the dimensions */
  nbrows = bios_da.video_row_size+1;
  nbcols = bios_da.video_column_size;

  if(vga_modes[line].class==TEXT)
  {
    /* Compute the address */
    address=((((nbcols*nbrows*2)|0x00ff)+1)*page)+(xcurs+ycurs*nbcols)*2;

    dummy=((uint16_t)attr<<8)+car;
    memsetw((uint8_t *)(vga_modes[line].sstart<<4)+address,dummy,count);
  }
}

void vga_write_teletype(uint8_t car, uint8_t page, uint8_t attr, uint8_t flag)
{/* flag = WITH_ATTR / NO_ATTR */
  uint8_t xcurs,ycurs,mode,line;
  uint16_t nbcols,nbrows,address;
  uint16_t cursor,dummy;

  /* special case if page is 0xff, use current page */
  if(page==0xff)
    page=bios_da.video_active_page;

  /* FIXME gfx mode */

  /* Get the mode */
  mode = bios_da.video_mode;
  line = find_vga_entry(mode);
  if(line==0xFF)return;

  /* Get the cursor pos for the page */
  vga_get_cursor_pos(page,&dummy,&cursor);
  xcurs=cursor&0x00ff;ycurs=(cursor&0xff00)>>8;

  /* Get the dimensions */
  nbrows=bios_da.video_mode+1;
  nbcols=bios_da.video_column_size;

  switch(car)
  {
    case 7:
    /* FIXME should beep */
    break;

    case 8:
    if(xcurs>0)xcurs--;
    break;

    case '\r':
    xcurs=0;
    break;

    case '\n':
    xcurs=0;
    ycurs++;
    break;

    case '\t':
    do {
      vga_write_teletype(' ',page,attr,flag);
      vga_get_cursor_pos(page,&dummy,&cursor);
      xcurs=cursor&0x00ff;ycurs=(cursor&0xff00)>>8;
    } while(xcurs%8==0);
    break;

    default:
    if(vga_modes[line].class==TEXT) {
      /* Compute the address */
      address=((((nbcols*nbrows*2)|0x00ff)+1)*page)+(xcurs+ycurs*nbcols)*2;

      /* Write the char */
      *(uint8_t *)((vga_modes[line].sstart<<4)+address) = car;

      if(flag==WITH_ATTR)
       *(uint8_t *)((vga_modes[line].sstart<<4)+address+1) = attr;
    }
    xcurs++;
  }

  /* Do we need to wrap ? */
  if(xcurs==nbcols) {
    xcurs=0;
    ycurs++;
  }

  /* Do we need to scroll ? */
  if(ycurs==nbrows) {
    vga_scroll(0x01,0x07,0,0,nbrows-1,nbcols-1,page,SCROLL_UP);
    ycurs-=1;
  }
 
  /* Set the cursor for the page */
  cursor=ycurs; cursor<<=8; cursor+=xcurs;
  vga_set_cursor_pos(page,cursor);
}

uint8_t vga_get_video_mode(uint8_t *colsnum, uint8_t *activepage)
{
  colsnum[0] = bios_da.video_column_size;
  activepage[0] = bios_da.video_active_page;
  return (bios_da.video_mode_options & 0x80) | bios_da.video_mode;
}

static void vga_perform_gray_scale_summing (uint16_t start, uint16_t count) 
{
  uint8_t r,g,b;
  uint16_t i;
  uint16_t index;

  inp(VGAREG_ACTL_RESET);
  outp(VGAREG_ACTL_ADDRESS,0x00);

  for( index = 0; index < count; index++ ) 
  {
    /* set read address and switch to read mode */
    outp(VGAREG_DAC_READ_ADDRESS,start);
    /* get 6-bit wide RGB data values */
    r=inp(VGAREG_DAC_DATA);
    g=inp(VGAREG_DAC_DATA);
    b=inp(VGAREG_DAC_DATA);

    /* intensity = ( 0.3 * Red ) + ( 0.59 * Green ) + ( 0.11 * Blue ) */
    i = ( ( 77*r + 151*g + 28*b ) + 0x80 ) >> 8;

    if(i>0x3f)i=0x3f;
 
    /* set write address and switch to write mode */
    outp(VGAREG_DAC_WRITE_ADDRESS,start);
    /* write new intensity value */
    outp(VGAREG_DAC_DATA, i&0xff);
    outp(VGAREG_DAC_DATA, i&0xff);
    outp(VGAREG_DAC_DATA, i&0xff);
    start++;
  }  
  inp(VGAREG_ACTL_RESET);
  outp(VGAREG_ACTL_ADDRESS,0x20);
}

uint8_t vga_set_video_mode(uint8_t mode)
{ /* modenum: Bit 7 is 1 if no clear screen */
  uint16_t i, ret = 0x20;
  uint16_t crtc_addr;
  uint8_t line, *palette;
  uint8_t modenum = mode&0x7F;
  uint8_t modeset_ctl, video_ctl, vga_switches;
  
  /* find the entry in the video modes */
  line = find_vga_entry(modenum);
  fd32_log_printf("[INT10] Set VGA mode to %x (at line %x)\n", modenum, line);

  /* Read the bios vga control */
  video_ctl = bios_da.video_mode_options;
  /* Read the bios vga switches */
  vga_switches = bios_da.video_feat_bit;
  /* Read the bios mode set control */
  modeset_ctl = bios_da.video_display_data;
  
  /* if palette loading (bit 3 of modeset ctl = 0) */
  if((modeset_ctl&0x08) == 0)
  { /* Set the PEL mask */
    outp(VGAREG_PEL_MASK,vga_modes[line].pelmask);

    /* Set the whole dac always, from 0 */
    outp(VGAREG_DAC_WRITE_ADDRESS,0x00);

    /* From which palette */
    switch(vga_modes[line].dacmodel)
    {
      case 0:
        palette=(uint8_t *)palette0;
        break;
      case 1:
        palette=(uint8_t *)palette1;
        break;
      case 2:
        palette=(uint8_t *)palette2;
        break;
      case 3:
        palette=(uint8_t *)palette3;
        break;
      default: /* WARNING: unverified */
        palette=(uint8_t *)palette0;
        break;
     }
    /* Always 256*3 values */
    for(i=0;i<0x0100;i++)
    {
      if(i<=dac_regs[vga_modes[line].dacmodel]) {
        outp(VGAREG_DAC_DATA,palette[(i*3)+0]);
        outp(VGAREG_DAC_DATA,palette[(i*3)+1]);
        outp(VGAREG_DAC_DATA,palette[(i*3)+2]);
      } else {
        outp(VGAREG_DAC_DATA,0);
        outp(VGAREG_DAC_DATA,0);
        outp(VGAREG_DAC_DATA,0);
      }
    }
    if((modeset_ctl&0x02)==0x02)
      vga_perform_gray_scale_summing(0x00, 0x100);
  }
  /* Reset Attribute Ctl flip-flop */
  inp(VGAREG_ACTL_RESET);

  /* Set Attribute Ctl */
  for(i=0;i<=ACTL_MAX_REG;i++) {
    outp(VGAREG_ACTL_ADDRESS, i);
    outp(VGAREG_ACTL_WRITE_DATA, actl_regs[vga_modes[line].actlmodel][i]);
  }

  /* Set Sequencer Ctl */
  for(i=0;i<=SEQU_MAX_REG;i++) {
    outp(VGAREG_SEQU_ADDRESS, i);
    outp(VGAREG_SEQU_DATA, sequ_regs[vga_modes[line].sequmodel][i]);
  }

  /* Set Grafx Ctl */
  for(i=0;i<=GRDC_MAX_REG;i++) {
    outp(VGAREG_GRDC_ADDRESS, i);
    outp(VGAREG_GRDC_DATA, grdc_regs[vga_modes[line].grdcmodel][i]);
  }

  /* Set CRTC address VGA or MDA */
  crtc_addr = vga_modes[line].memmodel == MTEXT ? VGAREG_MDA_CRTC_ADDRESS : VGAREG_VGA_CRTC_ADDRESS;

  /* Set CRTC regs */
  for(i=0;i<=CRTC_MAX_REG;i++) {
    outp(crtc_addr, i);
    outp(crtc_addr+1, crtc_regs[vga_modes[line].crtcmodel][i]);
  }

  /* Set the misc register */
  outp(VGAREG_WRITE_MISC_OUTPUT, vga_modes[line].miscreg);

  /* Enable video */
  outp(VGAREG_ACTL_ADDRESS, 0x20);
  inp(VGAREG_ACTL_RESET);

  if((mode&0x80) == 0x00) /* noclear equals zero? */
  {
    if(vga_modes[line].class == TEXT) {
      uint16_t *p = (uint16_t *)(vga_modes[line].sstart*0x10);
      for (i = 0; i < 0x4000; i++) /* 32k */
        p[i] = 0x0720;
    } else {
      if(mode<0x0d) {
       memset((uint8_t *)(vga_modes[line].sstart*0x10), 0x00, 0x8000); /* 32k */
      } else {
        uint8_t mmask;
       outp( VGAREG_SEQU_ADDRESS, 0x02 );
       mmask = inp( VGAREG_SEQU_DATA );
       outp( VGAREG_SEQU_DATA, 0x0f ); /* all planes */
       memset((uint8_t *)(vga_modes[line].sstart*0x10), 0x00, 0x10000); /* 64k */
       outp( VGAREG_SEQU_DATA, mmask );
      }
    }
  }

  /* Set the BIOS mem */
  bios_da.video_mode = mode;
  bios_da.video_column_size = vga_modes[line].twidth;
  bios_da.video_membuf_size = vga_modes[line].slength;
  bios_da.video_crtc_port = crtc_addr;
  bios_da.video_row_size = vga_modes[line].theight-1;
  bios_da.video_font_height = vga_modes[line].cheight;
  bios_da.video_mode_options = 0x60|(mode&0x80);
  bios_da.video_feat_bit = 0xF9;
  bios_da.video_display_data = bios_da.video_display_data&0x7F;

  /* FIXME We nearly have the good tables. to be reworked */
  bios_da.video_dcc_index = 0x08; /* 8 is VGA should be ok for now */  
  bios_da.video_saveptr = 0x0;

  /* FIXME */
  bios_da.vdu_ctrl_reg = 0x0; /* Unavailable on vanilla vga, but... */
  bios_da.vdu_color_reg = 0x0;
 
  /* Set cursor shape */
  if(vga_modes[line].class==TEXT) {
    vga_set_cursor_shape(0x06, 0x07);
  }

  /* Set cursor pos for page 0..7 */
  for(i=0;i<8;i++)
    vga_set_cursor_pos(i,0x0000);

  /* Write the fonts in memory */
  if(vga_modes[line].class==TEXT) {
    vga_load_text_8_16_pat(0x04, 0x00);
    vga_set_text_block_specifier(0x00);
  }
  
  return ret;
}

uint8_t vga_read_display_code(void)
{
  return bios_da.video_dcc_index;
}

void vga_set_display_code(uint8_t active_code)
{
  bios_da.video_dcc_index = active_code;
}

void vga_set_text_block_specifier(uint8_t bl)
{
  outp( VGAREG_SEQU_ADDRESS, 0x03 );
  outp( VGAREG_SEQU_DATA, bl );
}

void vga_load_text_8_16_pat(uint8_t al, uint8_t bl)
{
  uint16_t blockaddr, dest, i, src;

  get_font_access();
  blockaddr = bl << 13;
  for(i=0; i < 0x100; i++)
  {
    src = i * 16;
    dest = blockaddr + i * 32;
    memcpy((uint8_t *)0xA0000+dest, vgafont16+src, 16);
  }
  release_font_access();
  if(al >= 0x10)
   set_scan_lines(16);
}

void vga_load_text_8_8_pat(uint8_t al, uint8_t bl)
{
  uint16_t blockaddr, dest, i, src;

  get_font_access();
  blockaddr = bl << 13;
  for(i=0; i < 0x100; i++)
  {
    src = i * 8;
    dest = blockaddr + i * 32;
    memcpy((uint8_t *)0xA0000+dest, vgafont8+src, 8);
  }
  release_font_access();
  if(al >= 0x10)
    set_scan_lines(8);
}
