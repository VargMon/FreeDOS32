/* ATA Driver for FD32
 * by Luca Abeni & Nils Labugt
 *
 * This is free software; see GPL.txt
 */

#ifndef __ATA_DETECT_H__
#define __ATA_DETECT_H__

int ata_detect(struct ide_interface *i, void (*disk_add)(struct ata_device *d, char *name));

#endif
