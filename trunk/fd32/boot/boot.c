/* FD/32 Initialization Code
 * by Luca Abeni
 * 
 * This is free software; see GPL.txt
 */

#include <ll/i386/stdlib.h>
#include <ll/i386/cons.h>
#include <ll/i386/error.h>
#include <ll/i386/mem.h>
#include <ll/i386/mb-info.h>
#include <ll/i386/hw-instr.h>
#include <ll/i386/hw-func.h>

/* #define STATIC_DPMI */
/* #define STATIC_BIOSDISK */

#define DISK_TEST 1

#include "logger.h"
#include "init.h"
#include "mods.h"
#include "mmap.h"
#include "filesys.h"	/* For fd32_set_default_drive() */

#ifdef STATIC_DPMI
extern void DPMI_init(void);
#endif
#ifdef STATIC_BIOSDISK
extern void biosdisk_init(void);
#endif

int main (int argc, char *argv[])
{
#ifdef __BOOT_DEBUG__
  DWORD sp1, sp2;
#endif
  struct multiboot_info *mbi;
  DWORD lbase = 0, hbase = 0;
  DWORD lsize = 0, hsize = 0;
  extern WORD kern_CS, kern_DS;
  
#ifdef __BOOT_DEBUG__
  sp1 = get_SP();
#endif

  cli();
  mbi = x_init();

  /* Set the kern_CS and kern_DS for DPMI... */
  kern_CS = get_CS();
  kern_DS = get_DS();

  if (mbi == NULL) {
	message("Error in LowLevel initialization code...\n");
	sti();
	exit(-1);
  }
  sti();

  message("FreeDOS/32 Kernel booting...\n");
  message("System information from multiboot (flags 0x%lx):\n",
	  mbi->flags);

  if (mbi->flags & MB_INFO_MEMORY) {
#ifdef __BOOT_DEBUG__
    message("[FD32] Memory information OK\n");
#endif
    lsize = mbi->mem_lower * 1024;
    hsize = mbi->mem_upper * 1024;
#ifdef __BOOT_DEBUG__
    message("    Mem Lower: %lx %lu\n", lsize, lsize);
    message("    Mem Upper: %lx %lu\n", hsize, hsize);
#endif
    if (mbi->flags & MB_INFO_USEGDT) {
	  lbase = mbi->mem_lowbase;
	  hbase = mbi->mem_upbase;
    } else {
      lbase = 0x0;
      hbase = 0x100000;
    }
#ifdef __BOOT_DEBUG__
    message("        Low Memory: %ld - %ld (%lx - %lx) \n", 
	    lbase, lbase + lsize,
	    lbase, lbase + lsize);
    message("        High Memory: %ld - %ld (%lx - %lx)\n", 
	    hbase, hbase + hsize,
	    hbase, hbase + hsize);
#endif
  }
  
  system_init(mbi);
  
  if (mbi->flags & MB_INFO_BOOTDEV) {
    unsigned char *b;

    b = &mbi->boot_device;
    if (b[3] == 0) {
      fd32_set_default_drive('A');
    }
    if (b[3] == 1) {
      fd32_set_default_drive('B');
    }
#ifdef __BOOT_DEBUG__
    message("    Boot device set: %u %u %u %u\n", b[0], b[1], b[2], b[3]);
#endif
  }
  
  dos_init(lbase, lsize);

  fd32_log_init();
#ifdef STATIC_BIOSDISK
  biosdisk_init();
#endif
#ifdef STATIC_DPMI
  DPMI_init();
#endif
  
  if (mbi->flags & MB_INFO_MODS) {
#ifdef __BOOT_DEBUG__
    fd32_log_printf("[FD32] Attempting to load the shell...\n");
#endif
    process_modules(mbi->mods_count, mbi->mods_addr);
  } else {
    message("Sorry, no modules provided... I cannot load the shell!!!\n");
  }

#ifdef STATIC_BIOSDISK
#if (DISK_TEST == 1)
  bios_test();
#endif

#if (DISK_TEST == 2)
  disk_test();
#endif

#if (DISK_TEST == 3)
  fat_test();
#endif
#endif
  cli();
  x_end();
#ifdef __BOOT_DEBUG__
  sp2 = get_SP();
  fd32_log_printf("[FD32] End reached!\n");
  fd32_log_printf("    Actual stack : %lx - ", sp2);
  fd32_log_printf("    Begin stack : %lx\n", sp1);
  fd32_log_printf("    Check if same : %s\n",sp1 == sp2 ? "Ok :-)" : "No :-(");
#endif    
  fd32_log_stats();
  return 1;
}
