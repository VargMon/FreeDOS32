/* Project:     OSLib
 * Description: The OS Construction Kit
 * Date:                1.6.2000
 * Idea by:             Luca Abeni & Gerardo Lamastra
 *
 * OSLib is an SO project aimed at developing a common, easy-to-use
 * low-level infrastructure for developing OS kernels and Embedded
 * Applications; it partially derives from the HARTIK project but it
 * currently is independently developed.
 *
 * OSLib is distributed under GPL License, and some of its code has
 * been derived from the Linux kernel source; also some important
 * ideas come from studying the DJGPP go32 extender.
 *
 * We acknowledge the Linux Community, Free Software Foundation,
 * D.J. Delorie and all the other developers who believe in the
 * freedom of software and ideas.
 *
 * For legalese, check out the included GPL license.
 */

/* KL initialization code	*/

#include <ll/i386/stdlib.h>
#include <ll/i386/x-bios.h>
#include <ll/i386/mem.h>
#include <ll/i386/cons.h>
#include <ll/i386/mb-info.h>
#include <ll/i386/error.h>
#include <ll/i386/pit.h>
#include <ll/i386/pic.h>
#include <ll/i386/fpu.h>

#include <ll/i386/tss-ctx.h>
#include <ll/i386/hw-arch.h>
#include <ll/sys/ll/ll-func.h>
#include <ll/sys/ll/ll-mem.h>
#include <ll/sys/ll/event.h>	  /* for irq_bind() & irq_init() */
#include <ll/sys/ll/exc.h>        /* These are the HW exceptions */

FILE(LL-Init);

/* These are declared in llCx32b.C */
struct tss tss_table[TSS_SLOTS];
WORD tss_control[TSS_SLOTS];
BYTE ll_fpu_stdctx[FPU_CONTEXT_SIZE];

void ll_exc_hook(int i)
{
	static char *exc_mess[] = {
	"#Division by 0",
	"#Debug fault",
	"#NMI detected",
	"#Breakpoint trap",
	"#Overflow detected on INTO",
	"#BOUND limit exceeded",
	"*Unvalid opcode",
	"1FPU context switch", /* Handled in the llCtx.Asm/S File */
	"*Double defect",
	"#INTEL reserved",
	"*Unvalid TSS",
	"*Segment not present",
	"*Stack exception",
	"*General protection fault",
	"#Page fault",
	"#INTEL reserved",
	"2Coprocessor error"
	};

	static int exc_code[] = {
	DIV_BY_0, NMI_EXC, DEBUG_EXC, BREAKPOINT_EXC,
	HW_FAULT, HW_FAULT, HW_FAULT,
	0,              /* This is the FPU ctx Switch */
	HW_FAULT, HW_FAULT, HW_FAULT, HW_FAULT,
	HW_FAULT, HW_FAULT, HW_FAULT, HW_FAULT,
	MATH_EXC
	};

	char code = *exc_mess[i];
	#ifdef __LL_DEBUG__
	extern long int ndp_called,ndp_switched;
	extern wu_called;
	extern ai_called;
	extern DWORD *smain;
	#endif

	/* Math error! FPU has to be acknowledgded */
	if (code == '2') outp(0x0F0,0);
	
	message("Exception %d occurred\n", i);
	message("%s\n", &exc_mess[i][1]);
 	
	
#ifdef __LL_DEBUG__
	if (code == '*') {
		/* Dump additional info */
		message("DS:%nx CS:%nx\n", get_ds(), get_cs());
		/* message("WU : %d AI : %d\n",wu_called,ai_called); */
		message("Actual stack : %x\n", get_sp());
		/* message("Main stack : %p\n",smain); */
		dump_tss(get_tr());
	}
#endif
	/* halt(); */
	ll_abort(exc_code[i]);
}

void *ll_init(void)
{
	void *p;
	int i;
	LIN_ADDR b;
/*
	DWORD s;
	BYTE *base;
*/
  struct tss dummy_tss;      /* Very dirty, but we need it, in order to
			 get an initial value for the FPU
			 context...
		      */
  
 
	p = l1_init();
	/* First of all, init the exc and irq tables... */
	irq_init();
	for(i = 0; i < 32; i++) {

/* Warning!!! The hw exceptions should be 32.... Fix it!!! */
		
/*
		ll_irq_table[i] = (DWORD)act_int;
		ll_exc_table[i] = (DWORD)ll_exc_hook;
*/
		l1_exc_bind(i, ll_exc_hook);
	}
	for(i = 0; i < 16; i++) {
		void act_int(int i);
		l1_irq_bind(i, act_int);
	}


  /* Init TSS table & put the corrispondent selectors into GDT */
  tss_control[MAIN_TSS] |= TSS_USED;
  for (i = 0; i < TSS_SLOTS; i++) {
    /* b = appl2linear(&TSS_table[i]); */
    b = (LIN_ADDR)(&tss_table[i]);
    gdt_place(TSSindex2sel(i),(DWORD)b, sizeof(struct tss),
		    FREE_TSS386, GRAN_16);
  }

#if 0
  ll_FPU_save();
  memcpy(ll_FPU_stdctx,ll_FPU_savearea,FPU_CONTEXT_SIZE);
#else
  save_fpu(&dummy_tss); /* OK???*/
  memcpy(ll_fpu_stdctx, dummy_tss.ctx_fpu, FPU_CONTEXT_SIZE);
#endif
  init_fpu();

	
	/* ll_mem_init must be explicitelly called by program... */
#if 0
	/* Get info about extended memory! We suppose that X has loaded */
	/* there the application; if you switch to DOS memory, then you */
	/* have to change the stuff in order it works; check X_...  for */
	/* details.                                                     */
	X_meminfo(&b,&s,NULL,NULL);
	base = (BYTE *)b;
	#ifdef __MEM_DEBUG__
	message("PM Free Mem Base         : %lx\n",b);
	message("PM null addr (0L)        : %lx\n",appl2linear((void *)0L));
	message("PM Free Mem Base (Cnvrtd): %lp\n",base);
	#endif
	ll_mem_init(base,s);
	#ifdef __MEM_DEBUG__
	ll_mem_dump();
	#endif
#endif


	return p;
}

void abort_tail(int code)
{
        message("ABORT %d !!!",code);
        l1_end();
	sti();
	l1_exit(1);
}

void ll_end(void)
{
	l1_end();
}

