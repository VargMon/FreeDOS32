#ifndef __FD32_FD32_DRENV_H
#define __FD32_FD32_DRENV_H

#include <ll/i386/hw-data.h>
#include <ll/i386/hw-func.h>
#include <ll/i386/hw-instr.h>
#include <ll/i386/pic.h>
#include <ll/i386/stdio.h>
#include <ll/i386/error.h>
#include <ll/i386/stdlib.h>
#include <ll/i386/string.h>
#include <ll/ctype.h>
#include <ll/getopt.h>
#include <ll/sys/ll/event.h>

#include <dr-env/bios.h>
#include <dr-env/mem.h>
#include <dr-env/datetime.h>
/* FIXME: This is a temporary hack while waiting for standard headers */
#include <dr-env/stdint.h>

#include <kmem.h>
#include <exec.h>
#include <kernel.h>
#include <logger.h>

#define itoa(value, string, radix) dcvt(value, string, radix, 10, 0)
#define fd32_outb(port, data)      outp(port, data)
#define fd32_outw(port, data)      outpw(port, data)
#define fd32_outd(port, data)      outpd(port, data)
#define fd32_inb                   inp
#define fd32_inw                   inpw
#define fd32_ind                   inpd
#define fd32_message               message
#define fd32_log_printf            fd32_log_printf
#define fd32_error                 error
#define fd32_kmem_get              (void *)mem_get
#define fd32_kmem_get_region       mem_get_region
#define fd32_kmem_free(m, size)    mem_free((DWORD)m, size)

/* Interrupt management */
typedef void Fd32IntHandler(int);
#define FD32_INT_HANDLER(name)     void name(int n)
#define fd32_cli                   cli
#define fd32_sti                   sti
#define fd32_master_eoi()
#define fd32_irq_bind(n, h)        l1_irq_bind(n, h); irq_unmask(n);

/* Timer-driven events */
typedef void Fd32EventCallback(void *params);
extern inline int fd32_event_post(unsigned us, Fd32EventCallback *handler, void *params)
{
  struct timespec ts;
  int res;
  DWORD f;
  
  f = ll_fsave();
#ifdef __TIME_NEW_IS_OK__
  ll_gettime(TIME_NEW, &ts);
#else
  ll_gettime(TIME_EXACT, &ts);
#endif
  ts.tv_sec += us / 1000000;
  ts.tv_nsec += (us % 1000000) * 1000;
  ts.tv_sec += ts.tv_nsec / 1000000000;
  ts.tv_nsec %= 1000000000;
  res = event_post(ts, handler, params);
  ll_frestore(f);

  return res;
}
extern inline int fd32_event_delete(int index)
{
  int res;
  DWORD f;

  f = ll_fsave();
  res = event_delete(index);
  ll_frestore(f);

  return res;
}

/* A inlined cpu idle for save the consumption */
extern inline void fd32_cpu_idle(void)
{
  __asm__ __volatile__ ("hlt");
}

#define WFC(c) while (c) fd32_cpu_idle()

#define assert(x) /* always as with NDEBUG defined */
typedef int wchar_t;
typedef int wint_t;

#endif /* #ifndef __FD32_FD32_DRENV_H */
