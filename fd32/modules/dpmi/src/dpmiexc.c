#include <ll/i386/hw-data.h>
#include <ll/i386/hw-func.h>
#include <ll/i386/error.h>
#include <ll/i386/x-bios.h>

#include <logger.h>
#include "handler.h"
#include "dpmi.h"
#include "dpmiexc.h"
#include "ldtmanag.h"

extern DWORD ll_exc_table[16];
extern struct handler exc_table[32];
extern struct gate IDT[256];
extern DWORD rm_irq_table[256];

extern void int21_handler(union regs *r);

static void reflect(DWORD intnum, volatile union regs *r)
{
  struct tss *vm86_tss;
  WORD isr_cs, isr_eip;
  CONTEXT c = get_tr();

  if (c == X_VM86_CALLBIOS_TSS)
    vm86_tss = vm86_get_tss(X_VM86_CALLBIOS_TSS);
  else if (c == X_VM86_TSS)
    vm86_tss = vm86_get_tss(X_VM86_TSS);

  if (c == X_VM86_CALLBIOS_TSS || c == X_VM86_TSS) {
#ifdef __DPMIEXC_DEBUG__
    fd32_log_printf("[DPMI] Emulate interrupt %lx ...\n", intnum);
#endif
    /* Allocate a new return frame */
    if (r->d.flags&CPU_FLAG_VM) {
      WORD *app_stack;
      r->d.vm86_esp -= 6;
      app_stack = (void *)(r->d.vm86_ss<<4)+r->d.vm86_esp;
      app_stack[2] = r->d.flags;
      app_stack[1] = r->d.ecs;
      app_stack[0] = r->d.eip;
      /* We are emulating INT intnum */
      if (fd32_get_real_mode_int(intnum, &isr_cs, &isr_eip) >= 0) {
        r->d.ecs = isr_cs;
        r->d.eip = isr_eip;
        r->d.flags = CPU_FLAG_VM | CPU_FLAG_IOPL;
      } else {
        message("[DPMI] Invalid interrupt 0x%lx!\n", intnum);
      }
    } else {
      SET_CARRY;
      message("[DPMI] Direct call BIOS without vm86, not supported!\n");
    }
  } else {
    message("[DPMI] Redirect to the chandler, not done!\n");
  }
}

static void real_mode_int_mgr(DWORD intnum, union regs r)
{
  switch (intnum)
  {
    case 0x21:
      int21_handler (&r);
      break;
    default:
      reflect (intnum, &r);
      break;
  }
}

void fd32_enable_real_mode_int(int intnum)
{
  l1_int_bind(intnum, real_mode_int_mgr);
}

int fd32_get_real_mode_int(int intnum, WORD *segment, WORD *offset)
{
  if ((intnum < 0) || (intnum > 255)) {
    return -1;
  }
/* This should be OK... Check it! */
  *segment = ((rm_irq_table[intnum]) & 0xFFFF0000) >> 16;
  *offset = ((rm_irq_table[intnum]) & 0x0000FFFF);

  return 0;
}

int fd32_set_real_mode_int(int intnum, WORD segment, WORD offset)
{
  if ((intnum < 0) || (intnum > 255)) {
    return -1;
  }
  /* Check this... */
  rm_irq_table[intnum] = ((DWORD)segment << 16) | offset;

  fd32_enable_real_mode_int(intnum);

  return 0;
}

int fd32_get_exception_handler(int excnum, WORD *selector, DWORD *offset)
{
  if (excnum > 0x1F) {
    return ERROR_INVALID_VALUE;
#if 0
    /* Error!!! */
    r->d.eax &= 0xFFFF0000;
    r->d.eax |= 0x8021;
    SET_CARRY;

    return;
#endif
  }
  
  /* CX:EDX = <selector>:<offset> of the exception handler */
  *selector = exc_table[excnum].cs;
  *offset = exc_table[excnum].eip;

  return 0;
}

int fd32_set_exception_handler(int excnum, WORD selector, DWORD offset)
{
  if (excnum > 0x1F) {
    return ERROR_INVALID_VALUE;
  }
  
  /* CX:EDX = <selector>:<offset> of the exception handler */
  /* Set it */
  /* Warn: we have to add a check on the selector value (CX) */
  exc_table[excnum].cs = selector;
  exc_table[excnum].eip = offset;

  return 0;
}

int fd32_get_protect_mode_int(int intnum, WORD *selector, DWORD *offset)
{
  if (intnum > 0xFF) {
    return ERROR_INVALID_VALUE;
  }
  
  /* CX:EDX = <selector>:<offset> of the interrupt handler */
  /* Set it */
  /* Warn: we have to add a check on the selector value (CX) */
  *selector = IDT[intnum].sel;
  *offset = IDT[intnum].offset_hi << 16 | IDT[intnum].offset_lo;
  
  return 0;
}

int fd32_set_protect_mode_int(int intnum, WORD selector, DWORD offset)
{
  if (intnum > 0xFF) {
    return ERROR_INVALID_VALUE;
  }
  
  /* CX:EDX = <selector>:<offset> of the interrupt handler */
  /* Set it */
  /* Warn: we have to add a check on the selector value (CX) */
  IDT[intnum].sel = selector;
  IDT[intnum].offset_hi = (WORD)(offset >> 16);
  IDT[intnum].offset_lo = (WORD)(offset);

  return 0;
}
