/* Layout support for FD32's Keyborad Driver
 * - Using KEY file pack for FD-KEYB
 * - The file's structure to parse is based on KC of FD-KEYB
 *
 * Copyright (C) 2005 by Hanzac Chen
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the
 * Free Software Foundation, Inc.,
 * 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include "layout.h"
#include <filesys.h>


#define __KEYB_DEBUG__


/* Current selected keyb control block (the layout) */
static KeybCB cb;


/* Decode the scancode based on the current layout
	TODO: Implement user-defined flags
 */
int keyb_layout_decode(BYTE c, keyb_std_status_t stdst, int lock)
{
	DWORD i;
	int ret = -1;

	if (cb.ref != NULL) {
		BYTE *p;
		KeyType *blk;
		BYTE data_size;
		BYTE submap_idx;
		BYTE plane_idx = 0;

		/* Select the plane based on the flags */
		if (stdst.s.shift && !stdst.s.ctrl && !stdst.s.alt) {
			plane_idx = 1;
		} else if (stdst.s.ctrl || stdst.s.alt) {
			/* Additional planes */
			for (i = 0; i < cb.ref->plane_num; i++) {
				if (stdst.data&cb.planes[i].wtStd && !(stdst.data&cb.planes[i].exStd)) {
					plane_idx = 2+i;
					break;
				}
			}
		}

		/* Select the keycode based on the scancode */
		submap_idx = cb.ref->cur_submap;
		p = (BYTE *)cb.ref+cb.submaps[submap_idx].keysofs;
		/* Granularity (scancode-flag) check on the data_size */
		for (blk = (KeyType *)p, data_size = blk->flags.data_maxidx>>blk->flags.s_flag; blk->scancode != 0;
			p += sizeof(KeyType)+data_size,
			blk = (KeyType *)p, data_size = blk->flags.data_maxidx>>blk->flags.s_flag) {
			if (blk->scancode == c) {
                #ifdef __KEYB_DEBUG__
				fd32_log_printf("[KEYB] Scancode: 0x%02x, flags: 0x%02x, cmd: 0x%02x, data: %02x %02x ...\n",
					blk->scancode, *(BYTE *)(&blk->flags), blk->cmdbits, blk->data[0], blk->data[1]);
				#endif
				if (blk->data[plane_idx])
					ret = blk->data[plane_idx];
				/* If keycode is cmd 0 then return -1 */
				break;
			}
		}
	}

	return ret;
}

#ifdef __KEYB_DEBUG__
static int kl_print_id(BYTE *id, DWORD len)
{
	DWORD i, j, code;

    fd32_log_printf("[KEYB] KL id: ");
	for (i = 0; i < len; i = j+1)
	{
	 	for (j = i+2; id[j] != ',' && j < len; j++)
			fd32_log_printf("%c", id[j]);
		code = *((WORD *)(id+i));
		if (code != 0)
			fd32_log_printf("(%ld)", code);
		if (j < len)
			fd32_log_printf(", ");
	}
	fd32_log_printf("\n");
	return 0;
}
#endif

int keyb_layout_choose(const char *filename, char *layout_name)
{
	int ret = -1;
	KHeader kheader;
	KLHeader klheader;
	struct kernel_file f;
	int fileid = (int)&f;

	if (fd32_kernel_open(filename, O_RDONLY, 0, 0, &f) >= 0)
	{
		DWORD keybcb;

		/* The keyboard library header */
		fd32_kernel_read(fileid, &kheader, SIZEOF_KHEADER);
		#ifdef __KEYB_DEBUG__
		fd32_log_printf("[KEYB] KC header:\n\tsig: %c%c%c\n\tver: %x\n\tlength: %x\n", kheader.Sig[0], kheader.Sig[1], kheader.Sig[2], *((WORD *)kheader.version), kheader.length);
		#endif
		/* The keyboard library description */
		kheader.description = (BYTE *)mem_get(kheader.length+1);
		fd32_kernel_read(fileid, kheader.description, kheader.length);
		kheader.description[kheader.length-1] = 0;
		fd32_message("KC description: %s\n", kheader.description);
		mem_free((DWORD)kheader.description, kheader.length+1);

		for ( ; fd32_kernel_read(fileid, &klheader, SIZEOF_KLHEADER) > 0; ) {
			#ifdef __KEYB_DEBUG__
			fd32_log_printf("[KEYB] KL size: %x length: %x\n", klheader.klsize, klheader.length);
			#endif
			klheader.id = (BYTE *)mem_get(klheader.length);
			fd32_kernel_read(fileid, klheader.id, klheader.length);
			/* Match the layout name (or id, not implemented) */
			if (toupper(klheader.id[2]) == toupper(layout_name[0]) &&
				toupper(klheader.id[3]) == toupper(layout_name[1])) {
				cb.size = klheader.klsize-klheader.length;
				keybcb = mem_get(cb.size);
				cb.ref = (void *)keybcb;

				fd32_kernel_read(fileid, cb.ref, cb.size);
				cb.submaps = (SubMapping *)(keybcb+SIZEOF_CB);
				cb.planes = (Plane *)(keybcb+SIZEOF_CB+cb.ref->submap_num*sizeof(SubMapping));
				#ifdef __KEYB_DEBUG__
				/* Key layout debug output */
				DWORD i;
				keyb_std_status_t stdst;
				kl_print_id(klheader.id, klheader.length);
				fd32_log_printf("[KEYB] KeybCB: next %lx:\n\tSubmap_num %d\n\tPlane_num %d\n\tDec_sep %x\n\tCur_submap %d\n\tMCBID %x\n",
					cb.ref->next, cb.ref->submap_num, cb.ref->plane_num, cb.ref->dec_sep, cb.ref->cur_submap, cb.ref->mcbid);
				fd32_log_printf("[KEYB] Codepage: ");
				for (i = 0; i < cb.ref->submap_num; i++)
					fd32_log_printf("%d ", cb.submaps[i].codepage);
				fd32_log_printf("\n");
				for (i = 0; i < 0x100; i++)
				    keyb_layout_decode(i, stdst, 0);
				/* Display additional planes */
				for (i = 0; i < cb.ref->plane_num; i++) {
					fd32_log_printf("[KEYB] Plane %ld: %x %x %x %x\n", i, cb.planes[i].wtStd, cb.planes[i].exStd, cb.planes[i].wtUsr, cb.planes[i].exUsr);
				}
				#endif
				mem_free((DWORD)klheader.id, klheader.length);
				ret = 0;
				break;
			} else {
				/* Seek to the next KL */
				fd32_kernel_seek(fileid, klheader.klsize-klheader.length, FD32_SEEKCUR);
				mem_free((DWORD)klheader.id, klheader.length);
			}
		}

		fd32_kernel_close(fileid);
	}

	return ret;
}

int keyb_layout_free(void)
{
	if (cb.ref != NULL)
		return mem_free((DWORD)cb.ref, cb.size);
	else
		return -1;
}