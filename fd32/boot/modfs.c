/* FD/32 Module PseudoFS
 * by Luca Abeni
 * 
 * This is free software; see GPL.txt
 */

#include <ll/i386/hw-data.h>
#include <ll/i386/mem.h>
#include <ll/i386/error.h>

#include "dev/char.h"
#include "mods.h"

DWORD start[10];
DWORD end[10];
DWORD pointer[10];
int n = 0;

void driver_dummy(void)
{
  error("FD/32 Panic: an unimplemented function has been called!!\n");
}

static int modfs_read(void *f, DWORD size, BYTE *buff)
{
  int file = (int)f;
  int len;

  if (file > n) {
    return 0;
  }

  len = size;
  if (pointer[file] + len > end[file]) {
    len = end[file] - pointer[file];
  }

  memcpy(buff, (void *)pointer[file], len);

  pointer[file] += len;

  return len;
}

static int modfs_seek(void *f, int pos, int wence)
{
  int file = (int)f;
  int res;

  if (file > n) {
    return 0;
  }

  if (wence == 0) {
    res = pos;
  } else if (wence == 1) {
    res = pointer[file] + pos - start[file];
  } else if (wence == 2) {
    if (end[file] - start[file] < pos) {
      res = 0;
    } else {
      res = end[file] - start[file] - pos;
    }
  } else {
    res = pointer[file] - start[file];
  }

  if (start[file] + res > end[file]) {
    res = end[file] - start[file];
  }
  pointer[file] = start[file] + res;
  
  return res;
}

static int modfs_size(int file)
{
  int res;

  if (file > n) {
    return 0;
  }

  res = end[file] - start[file];
  return res;
}

void modfs_init(struct fd32_dev_char *pseudodev, DWORD addr, int count)
{
  int i;
  DWORD modstart, modend;
 
  n = count;

  for (i = 0 ; i < count; i++) {
    module_address(addr, i, &modstart, &modend);
    start[i] = modstart;
    end[i] = modend;
    pointer[i] = modstart;
  }

  pseudodev->open  = (void *)driver_dummy;
  pseudodev->close = (void *)driver_dummy;
  pseudodev->seek = modfs_seek;
  pseudodev->read = modfs_read;
  pseudodev->write = (void *)driver_dummy;
  pseudodev->ioctl = (void *)driver_dummy;
}
