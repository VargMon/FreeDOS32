/* DPMI Driver for FD32: VGA calls
 *
 * Copyright (C) 2001,2002 the LGPL VGABios developers Team
 */

#ifndef __VGA_H__
#define __VGA_H__

/* from http://www.nongnu.org/vgabios (vgatables.h) */
#define VGAREG_MDA_CRTC_ADDRESS   0x3B4
#define VGAREG_ACTL_ADDRESS       0x3C0
#define VGAREG_ACTL_WRITE_DATA    0x3C0
#define VGAREG_ACTL_READ_DATA     0x3C1
#define VGAREG_WRITE_MISC_OUTPUT  0x3C2
#define VGAREG_SEQU_ADDRESS       0x3C4
#define VGAREG_SEQU_DATA          0x3C5
#define VGAREG_PEL_MASK           0x3C6
#define VGAREG_DAC_READ_ADDRESS   0x3C7
#define VGAREG_DAC_WRITE_ADDRESS  0x3C8
#define VGAREG_DAC_DATA           0x3C9
#define VGAREG_GRDC_ADDRESS       0x3Ce
#define VGAREG_GRDC_DATA          0x3Cf
#define VGAREG_VGA_CRTC_ADDRESS   0x3D4
#define VGAREG_ACTL_RESET         0x3DA

#define ACTL_MAX_REG              0x14

/*
 * mode definition
 */

#define TEXT       0x00
#define GRAPH      0x01
#define CTEXT      0x00
#define MTEXT      0x01
#define CGA        0x02
#define PLANAR1    0x03
#define PLANAR2    0x04
#define PLANAR4    0x05
#define LINEAR8    0x06
#define MODE_MAX   0x14

typedef struct
{
 BYTE  svgamode;
 WORD  vesamode;
 BYTE  class;    /* TEXT, GRAPH */
 BYTE  memmodel; /* CTEXT,MTEXT,CGA,PL1,PL2,PL4,P8,P15,P16,P24,P32 */
 BYTE  nbpages; 
 BYTE  pixbits;
 WORD  swidth, sheight;
 WORD  twidth, theight;
 WORD  cwidth, cheight;
 WORD  sstart;
 WORD  slength;
 BYTE  miscreg;
 BYTE  pelmask;
 BYTE  crtcmodel;
 BYTE  actlmodel;
 BYTE  grdcmodel;
 BYTE  sequmodel;
 BYTE  dacmodel; /* 0 1 2 3 */
} __attribute__ ((packed)) VGAMODE;


#define SCROLL_DOWN 0
#define SCROLL_UP   1
#define NO_ATTR     2
#define WITH_ATTR   3

void vga_set_active_page(BYTE page);
void vga_set_cursor_shape(BYTE ch, BYTE cl);
void vga_get_cursor_pos(BYTE page, WORD *shape, WORD *pos);
void vga_set_cursor_pos(BYTE page, WORD cursor);
void vga_set_overscan_border_color(BYTE value);
void vga_get_all_palette_reg(BYTE *pal);
void vga_set_all_palette_reg(BYTE *pal);
void vga_toggle_intensity(BYTE state);
BYTE vga_get_single_palette_reg(BYTE reg);
void vga_set_single_palette_reg(BYTE reg, BYTE value);
void vga_set_all_dac_reg(WORD start, WORD count, BYTE *table);
void vga_set_single_dac_reg(WORD reg, BYTE r, BYTE g, BYTE b);
void vga_read_single_dac_reg(WORD reg, BYTE *r, BYTE *g, BYTE *b);
void vga_scroll(BYTE nblines, BYTE attr, BYTE rul, BYTE cul, BYTE rlr, BYTE clr, BYTE page, BYTE dir);
void vga_write_char_attr(BYTE car, BYTE page, BYTE attr, WORD count);
void vga_write_teletype(BYTE car, BYTE page, BYTE attr, BYTE flag);
BYTE vga_get_video_mode(BYTE *colsnum, BYTE *curpage);
BYTE vga_set_video_mode(BYTE modenum);
BYTE vga_read_display_code(void);
void vga_set_display_code(BYTE active_code);
void vga_set_text_block_specifier(BYTE bl);
void vga_load_text_8_16_pat(BYTE al, BYTE bl);
void vga_load_text_8_8_pat(BYTE al, BYTE bl);

#endif
