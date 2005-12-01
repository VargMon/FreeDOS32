/* DPMI Driver for FD32: VGA calls
 *
 * Copyright (C) 2001,2002 the LGPL VGABios developers Team
 */

#include <ll/i386/hw-data.h>
#include <ll/i386/hw-instr.h>
#include <ll/i386/cons.h>
#include <logger.h>
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

static BYTE crtc_regs[CRTC_MAX_MODEL+1][CRTC_MAX_REG+1]=
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

static BYTE actl_regs[ACTL_MAX_MODEL+1][ACTL_MAX_REG+1]=
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

static BYTE sequ_regs[SEQU_MAX_MODEL+1][SEQU_MAX_REG+1]=
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

static BYTE grdc_regs[GRDC_MAX_MODEL+1][GRDC_MAX_REG+1]=
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

static BYTE dac_regs[DAC_MAX_MODEL+1]=
{0x3f,0x3f,0x3f,0xff};

/* Mono */
static BYTE palette0[63+1][3]=
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

static BYTE palette1[63+1][3]=
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

static BYTE palette2[63+1][3]=
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

static BYTE palette3[256][3]=
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

static BYTE *biosmem_p = (BYTE *)0x400;

/* Utils */
static BYTE find_vga_entry(BYTE mode) 
{
  DWORD i;
  BYTE line=0xFF;
  for(i = 0; i <= MODE_MAX; i++)
    if(vga_modes[i].svgamode == mode) {
      line = i;
      break;
    }
  return line;
}

/* Functions */
void vga_set_all_palette_reg(BYTE *pal)
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

void vga_get_all_palette_reg(BYTE *pal)
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

void vga_toggle_intensity(BYTE state) 
{
  DWORD value;

  inp(VGAREG_ACTL_RESET);
  outp(VGAREG_ACTL_ADDRESS, 0x10);
  value = inp(VGAREG_ACTL_READ_DATA);
  value &= 0xF7;
  value |= (state&0x01)<<3;
  outp(VGAREG_ACTL_WRITE_DATA, value);
  outp(VGAREG_ACTL_ADDRESS, 0x20);
}

BYTE vga_get_single_palette_reg(BYTE reg)
{
  BYTE value = 0;

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

void vga_set_single_palette_reg(BYTE reg, BYTE value)
{
  if(reg <= ACTL_MAX_REG)
  {
    inp(VGAREG_ACTL_RESET);
    outp(VGAREG_ACTL_ADDRESS, reg);
    outp(VGAREG_ACTL_WRITE_DATA, value);
    outp(VGAREG_ACTL_ADDRESS, 0x20);
  }
}

void vga_set_all_dac_reg(WORD start, WORD count, BYTE *table)
{
  WORD i;
  outp(VGAREG_DAC_WRITE_ADDRESS, start);
  for(i=0; i<count; i++)
  {
    outp(VGAREG_DAC_DATA, *table++);
    outp(VGAREG_DAC_DATA, *table++);
    outp(VGAREG_DAC_DATA, *table++);
  }
}

void vga_set_single_dac_reg(WORD reg, BYTE r, BYTE g, BYTE b)
{
  outp(VGAREG_DAC_WRITE_ADDRESS, reg);
  outp(VGAREG_DAC_DATA, r);
  outp(VGAREG_DAC_DATA, g);
  outp(VGAREG_DAC_DATA, b);
}

void vga_read_single_dac_reg(WORD reg, BYTE *r, BYTE *g, BYTE *b)
{
  outp(VGAREG_DAC_READ_ADDRESS, reg);
  *r = inp(VGAREG_DAC_DATA); /* RED   */
  *g = inp(VGAREG_DAC_DATA); /* GREEN */
  *b = inp(VGAREG_DAC_DATA); /* BLUE  */
}

BYTE vga_get_video_mode(BYTE *colsnum, BYTE *activepage)
{
  colsnum[0] = biosmem_p[BIOSMEM_NB_COLS];
  activepage[0] = biosmem_p[BIOSMEM_CURRENT_PAGE];
  return (biosmem_p[BIOSMEM_VIDEO_CTL] & 0x80) | biosmem_p[BIOSMEM_CURRENT_MODE];
}

static void vga_perform_gray_scale_summing (WORD start, WORD count) 
{BYTE r,g,b;
 WORD i;
 WORD index;

 inp(VGAREG_ACTL_RESET);
 outp(VGAREG_ACTL_ADDRESS,0x00);

 for( index = 0; index < count; index++ ) 
  {
   // set read address and switch to read mode
   outp(VGAREG_DAC_READ_ADDRESS,start);
   // get 6-bit wide RGB data values
   r=inp( VGAREG_DAC_DATA );
   g=inp( VGAREG_DAC_DATA );
   b=inp( VGAREG_DAC_DATA );

   // intensity = ( 0.3 * Red ) + ( 0.59 * Green ) + ( 0.11 * Blue )
   i = ( ( 77*r + 151*g + 28*b ) + 0x80 ) >> 8;

   if(i>0x3f)i=0x3f;
 
   // set write address and switch to write mode
   outp(VGAREG_DAC_WRITE_ADDRESS,start);
   // write new intensity value
   outp( VGAREG_DAC_DATA, i&0xff );
   outp( VGAREG_DAC_DATA, i&0xff );
   outp( VGAREG_DAC_DATA, i&0xff );
   start++;
  }  
 inp(VGAREG_ACTL_RESET);
 outp(VGAREG_ACTL_ADDRESS,0x20);
}

BYTE vga_set_video_mode(BYTE modenum)
{/* modenum: Bit 7 is 1 if no clear screen */
  WORD i, ret = 0x20;
  WORD crtc_addr;
  BYTE line, *palette;
  BYTE modeset_ctl, video_ctl, vga_switches;
  
  /* find the entry in the video modes */
  line = find_vga_entry(modenum);
  fd32_log_printf("[INT10] Set VGA mode to %x (at line %x)\n", modenum, line);

  /* Read the bios vga control */
  video_ctl = biosmem_p[BIOSMEM_VIDEO_CTL];
  /* Read the bios vga switches */
  vga_switches = biosmem_p[BIOSMEM_SWITCHES];
  /* Read the bios mode set control */
  modeset_ctl = biosmem_p[BIOSMEM_MODESET_CTL];
  
  /* if palette loading (bit 3 of modeset ctl = 0) */
  if((modeset_ctl&0x08) == 0)
  {/* Set the PEL mask */
   outp(VGAREG_PEL_MASK,vga_modes[line].pelmask);

   /* Set the whole dac always, from 0 */
   outp(VGAREG_DAC_WRITE_ADDRESS,0x00);

   /* From which palette */
   switch(vga_modes[line].dacmodel)
    {case 0:
      palette=(BYTE *)palette0;
      break;
     case 1:
      palette=(BYTE *)palette1;
      break;
     case 2:
      palette=(BYTE *)palette2;
      break;
     case 3:
      palette=(BYTE *)palette3;
      break;
     default: /* WARNING: unverified */
      palette=(BYTE *)palette0;
      break;
    }
   /* Always 256*3 values */
   for(i=0;i<0x0100;i++)
    {if(i<=dac_regs[vga_modes[line].dacmodel])
      {outp(VGAREG_DAC_DATA,palette[(i*3)+0]);
       outp(VGAREG_DAC_DATA,palette[(i*3)+1]);
       outp(VGAREG_DAC_DATA,palette[(i*3)+2]);
      }
     else
      {outp(VGAREG_DAC_DATA,0);
       outp(VGAREG_DAC_DATA,0);
       outp(VGAREG_DAC_DATA,0);
      }
    }
   if((modeset_ctl&0x02)==0x02)
    {
     vga_perform_gray_scale_summing(0x00, 0x100);
    }
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

  /* Set cursor shape */
  if(vga_modes[line].class==TEXT) {
    cursor(0x06, 0x07);
  }

  /* Set cursor pos for page 0..7 */
  place(0, 0);
  
  return ret;
}
