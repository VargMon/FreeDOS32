/* FD32 Interrupt reflection functionality
 * by Luca Abeni
 * 
 * This is free software; see GPL.txt
 */

#include <ll/i386/hw-data.h>
#include <ll/i386/hw-func.h>
#include <ll/i386/x-bios.h>
#include <ll/i386/pic.h>
#include <ll/i386/error.h>
#include <logger.h>
#include <../drivers/dpmi/include/dpmi.h>

extern DWORD rm_irq_table[256];

/* This is implemented in oslib\xlib\ctx.s */
extern CONTEXT /*__cdecl*/ context_save(void);

void biosdisk_reflect(unsigned intnum, union regs r)
{
  WORD ctx;
  TSS *vm86_tss;
  DWORD *bos;
  DWORD isr_cs, isr_eip;
  WORD *old_esp;
  unsigned rmint;

  vm86_tss = (TSS *)vm86_get_tss();
  bos = (DWORD *)vm86_tss->esp0;

  #ifdef __REFLECT_DEBUG__
  fd32_log_printf("[BIOSDISK] INT %xh (%u) catched...\n", intnum, intnum);
  fd32_log_printf("[BIOSDISK] AX=%04x BX=%04x CX=%04x DX=%04x SI=%04x DI=%04x DS=%04x ES=%04x\n",
                  r.x.ax, r.x.bx, r.x.cx, r.x.dx, r.x.si, r.x.di, r.x.ds, r.x.es);
  #endif
  ctx = context_save();

  #if 0
  if (ctx == X_VM86_TSS) {
    #ifdef __REFLECT_DEBUG__
    fd32_log_printf("[BIOSDISK] To be reflected in VM86 mode...\n");
    #endif
  #else	/* Does this fix the bochs problem ? */
  {
  #endif

    #if 0
    /* The default handler for INT 15h AH=90h or 91h seems to merely clear */
    /* AH and the carry flag. Let's do it faster without reflecting.       */
    if (intnum == 0x15)
      if ((r.h.ah == 0x90) || (r.h.ah == 0x91)) {
        DWORD ef;
        #ifdef __REFLECT_DEBUG__
        fd32_log_printf("[BIOSDISK] Quick serving INT 15h AH=%02xh\n". r.h.ah);
        #endif
        ef = *(bos - 7);
        ef &= 0xFFFFFFFE;
        *(bos - 7) = ef;
        return;
      }
    #endif

    if ((intnum >= PIC1_BASE) && (intnum < PIC1_BASE + 8)) {
      rmint = intnum - PIC1_BASE + 8;
    } else {
      if ((intnum >= PIC2_BASE) && (intnum < PIC2_BASE + 8)) {
        rmint = intnum;
      } else {
        if ((intnum == 0x40) || (intnum == 0x15)) {
          rmint = intnum;
        } else {
          #ifdef __REFLECT_DEBUG__
          fd32_log_printf("[BIOSDISK] Strange interrupt %02xh to be reflected!!!\n", intnum);
          #endif
          /* Error!!! Should we panic? */
          return;
        }
      }
    }

#ifdef __VM86_REFLECT_DEBUG__
    /* TODO: This debugging code is broken... No tos any longer. */
    fd32_log_printf("[BIOSDISK] Entering ESP: %lx (= 0x%lx)\n",
	    (DWORD)(tos + 9), vm86_tss->esp0);
    fd32_log_printf("[BIOSDISK] Old EIP: 0x%lx    0x%lx\n", *(tos + 9), *(bos - 9));
    fd32_log_printf("[BIOSDISK] Old CS: 0x%x\n", (WORD)(*(bos - 8)));
    fd32_log_printf("[BIOSDISK] Old EFlags: 0x%lx    0x%lx\n", *(tos + 11), *(bos - 7));
    fd32_log_printf("[BIOSDISK] Old ESP: 0x%lx    0x%lx\n", *(tos + 12), *(bos - 6));
    fd32_log_printf("[BIOSDISK] Emulate, please!!!\n");
#endif
    old_esp = (WORD *)(*(bos - 6) + (*(bos - 5) << 4));
#ifdef __VM86_REFLECT_DEBUG__
    fd32_log_printf("[BIOSDISK] Old stack (Linear): 0x%lx\n", (DWORD)old_esp);
#endif
    *(old_esp - 1) = (WORD)(*(bos - 7)) /*CPU_FLAG_VM | CPU_FLAG_IOPL*/;
    *(old_esp - 2) = (WORD)(*(bos - 8));
    *(old_esp - 3) = (WORD)(*(bos - 9));
    *(bos - 6) -= 6;
    
    isr_cs= ((rm_irq_table[rmint]) & 0xFFFF0000) >> 16;
    isr_eip = ((rm_irq_table[rmint]) & 0x0000FFFF);
#ifdef __VM86_REFLECT_DEBUG__
    fd32_log_printf("[BIOSDISK] I have to call 0x%lx:0x%lx\n", isr_cs, isr_eip);
#endif
    *(bos - 8) = isr_cs;
    *(bos - 9) = isr_eip;
  }
}
