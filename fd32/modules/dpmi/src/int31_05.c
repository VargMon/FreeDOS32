/* DPMI Driver for FD32: handler for INT 0x31, Service 0x05
 * by Luca Abeni
 * 
 * This is free software; see GPL.txt
 */

#include<ll/i386/hw-data.h>
#include<ll/i386/hw-func.h>
#include<ll/i386/mem.h>
#include<ll/i386/error.h>

#include "dpmi.h"
#include "kernel.h"
#include "dpmimem.h"
#include "int31_05.h"
#include <logger.h>

/* #define __DEBUG__ */

void int31_0501(union regs *r)
{
  DWORD blocksize;
  DWORD handle;
  DWORD base;

  blocksize = ((r->d.ebx & 0xFFFF) << 16) | (r->d.ecx & 0xFFFF);
#ifdef __DEBUG__
  fd32_log_printf("[DPMI] Allocate Memory Block\n");
  fd32_log_printf("       Block size: 0x%lx (%ld)\n", blocksize, blocksize);
#endif
  handle = dpmi_alloc(blocksize, &base);
  if (handle == 0) {
    /* Error!!!! */
    SET_CARRY;
    /* For the moment, let's not care about the error code... We have
     * an abort...
     */
    error("Error: No more physical memory!!!\n");
    r->x.ax = 0x8013; /* Physical memory not available */
    fd32_abort();
    return;
  }
#ifdef __DEBUG__
  fd32_log_printf("       Allocated 0x%lx-0x%lx\n", base, base + blocksize);
  fd32_log_printf("       Handle: 0x%lx\n", handle);
#endif

  /* The allocated address is referred to the kernel DS (base = 0)
   * But It need not be translated to the user data segment
   * (remember: we have the user DS in the stack)
   *
   * Because it should be linear address, and DjLibC also compares
   * it with the djgpp base, see DjLibC: src/libc/crt0/crt0.S
   * (from Hanzac)
   */

  CLEAR_CARRY;
  r->x.ax = 0;
  r->x.bx = (base >> 16) & 0x0000FFFF;
  r->x.cx = base & 0x0000FFFF;
  r->x.si = (handle >> 16) & 0x0000FFFF;
  r->x.di = handle & 0x0000FFFF;
#ifdef __DEBUG__
  fd32_log_printf("       EAX: 0x%lx EBX: 0x%lx ECX: 0x%lx\n",
                  r->d.eax, r->d.ebx, r->d.ecx);
  fd32_log_printf("       ESI: 0x%lx EDI: 0x%lx \n", r->d.esi, r->d.edi);
#endif
}

void int31_0502(union regs *r)
{
  DWORD handle;
  int res;
  
  handle = ((r->d.esi & 0xFFFF) << 16) | (r->d.edi & 0xFFFF);
#ifdef __DEBUG__
  fd32_log_printf("[DPMI] Free Memory Block\n");
  fd32_log_printf("       Handle: 0x%lx:0x%lx = 0x%lx\n", r->d.esi, r->d.edi, handle);
#endif
  if (handle == 0) {
    fd32_log_printf("Freeing program memory chunk: kernel will do it...\n");
    res = 1;
  } else {
    res = dpmi_free(handle);
  }
  CLEAR_CARRY;
  r->x.ax = 0x0000;
  if (res != 0) {
    error("Failed\n");
    fd32_abort();
  }
  
  return;
}

void int31_0503(union regs *r)
{
  DWORD blocksize;
  DWORD handle;
  DWORD base;

  handle = (r->d.esi << 16) | r->x.di;
  if (dpmi_free(handle) != 0)
    fd32_abort();

  blocksize = (r->d.ebx << 16) | r->x.cx;
  handle = dpmi_alloc(blocksize, &base);

  CLEAR_CARRY;
  r->x.ax = 0;
  r->x.bx = base >> 16;
  r->x.cx = base;
  r->x.si = handle >> 16;
  r->x.di = handle;
}

void int31_0507(union regs *r)
{
#ifdef __DEBUG__
  fd32_log_printf("[DPMI] Modify Page Attributes: handle 0x%lx, offset 0x%lx, n. 0x%lx attr:0x%lx\n",
                  r->d.esi, r->d.ebx, r->d.ecx, *((DWORD *)r->d.edx));
#endif
  /*Error: set CF... */
  /* CF = 1 */
  SET_CARRY;
}
