/* DOS16/32 execution for FD32
 * by Luca Abeni & Hanzac Chen
 *
 * This is free software; see GPL.txt
 */

#include <ll/i386/hw-data.h>
#include <ll/i386/hw-func.h>
#include <ll/i386/error.h>
#include <ll/i386/x-bios.h>
#include <ll/string.h>
#include <devices.h>
#include <filesys.h>
#include <format.h>
#include <fcntl.h>
#include <kmem.h>
#include <exec.h>
#include <kernel.h>
#include <logger.h>
#include "dos_exec.h"

#define __DOS_EXEC_DEBUG__

#define VM86_STACK_SIZE 0x2000
/* TSS optional section */
static BYTE  vm86_stack0[VM86_STACK_SIZE];
extern DWORD current_SP;

static int vm86_call(WORD ip, WORD sp, WORD vm86_stack_size, LIN_ADDR vm86_stack, X_REGS16 *in, X_REGS16 *out, X_SREGS16 *s)
{
  DWORD vm86_flags, vm86_cs, vm86_ip;
  LIN_ADDR vm86_stack_ptr;

  struct tss * p_vm86_tss = vm86_get_tss();
  vm86_init(vm86_stack, vm86_stack_size);

  /* if (service < 0x10 || in == NULL) return -1; */
  if (p_vm86_tss->back_link != 0) {
    message("WTF? VM86 called with CS = 0x%x, IP = 0x%x\n", p_vm86_tss->cs, ip);
    fd32_abort();
  }
  /* Setup the stack frame */
  p_vm86_tss->ss = s->ss;
  p_vm86_tss->ebp = 0x91E; /* this is more or less random but some programs expect 0x9 in the high byte of BP!! */
  p_vm86_tss->esp = sp;
  /* Build an iret stack frame which returns to vm86_iretAddress */
  vm86_cs = 0xcd;
  vm86_ip = 0x48;
  vm86_flags = 0; /* CPU_FLAG_VM | CPU_FLAG_IOPL; */
  vm86_stack_ptr = vm86_stack + vm86_stack_size;
  lmempokew(vm86_stack_ptr - 6, vm86_ip);
  lmempokew(vm86_stack_ptr - 4, vm86_cs);
  lmempokew(vm86_stack_ptr - 2, vm86_flags);

  /* Wanted VM86 mode + IOPL = 3! */
  p_vm86_tss->eflags = CPU_FLAG_VM | CPU_FLAG_IOPL;
  /* Preload some standard values into the registers */
  p_vm86_tss->ss0 = X_FLATDATA_SEL;
  p_vm86_tss->esp0 = (DWORD)&(vm86_stack0[VM86_STACK_SIZE - 1]);

  /* Copy the parms from the X_*REGS structures in the vm86 TSS */
  p_vm86_tss->eax = (DWORD)in->x.ax;
  p_vm86_tss->ebx = (DWORD)in->x.bx;
  p_vm86_tss->ecx = (DWORD)in->x.cx;
  p_vm86_tss->edx = (DWORD)in->x.dx;
  p_vm86_tss->esi = (DWORD)in->x.si;
  p_vm86_tss->edi = (DWORD)in->x.di;
  /* IF Segment registers are required, copy them... */
  if (s != NULL) {
      p_vm86_tss->es = (WORD)s->es;
      p_vm86_tss->ds = (WORD)s->ds;
  } else {
      p_vm86_tss->ds = p_vm86_tss->ss;
      p_vm86_tss->es = p_vm86_tss->ss;
  }
  p_vm86_tss->gs = p_vm86_tss->ss;
  p_vm86_tss->fs = p_vm86_tss->ss;
  /* Execute the BIOS call, fetching the CS:IP of the real interrupt
   * handler from 0:0 (DOS irq table!)
   */
  p_vm86_tss->cs = s->cs;
  p_vm86_tss->eip = ip;

  /* Let's use the ll standard call... */
  p_vm86_tss->back_link = ll_context_save();
  
  /* TODO: Remove this line */
  /* __asm__ __volatile__ ("movl %%esp, %0" : "=m" (current_SP)); */
  if (out != NULL) {
    ll_context_load(X_VM86_TSS);
  } else {
    ll_context_to(X_VM86_TSS);
  }
  /* Back from the APP, after the stub switches VM86 to protected mode */

  /* Send back in the X_*REGS structure the value obtained with
   * the real-mode interrupt call
   */
  if (out != NULL) {
    if (p_vm86_tss->back_link != 0) {message("Panic!\n"); fd32_abort();}

    /* out->x.ax = global_regs->eax;
    out->x.bx = global_regs->ebx;
    out->x.cx = global_regs->ecx;
    out->x.dx = global_regs->edx;
    out->x.si = global_regs->esi;
    out->x.di = global_regs->edi;
    out->x.cflag = global_regs->flags; */
  }
  if (s != NULL) {
    s->es = p_vm86_tss->es;
    s->ds = p_vm86_tss->ds;
  }
  return 1;
}

/* File handling */
struct funky_file {
  fd32_request_t *request;
  void *file_id;
};

static int my_read(int id, void *b, int len)
{
  struct funky_file *f;
  fd32_read_t r;
  int res;

  f = (struct funky_file *)id;
  r.Size = sizeof(fd32_read_t);
  r.DeviceId = f->file_id;
  r.Buffer = b;
  r.BufferBytes = len;
  res = f->request(FD32_READ, &r);
#ifdef __DEBUG__
  if (res < 0) {
    fd32_log_printf("WTF!!!\n");
  }
#endif

  return res;
}

static int my_seek(int id, int pos, int w)
{
  int error;
  struct funky_file *f;
  fd32_lseek_t ls;

  f = (struct funky_file *)id;
  ls.Size = sizeof(fd32_lseek_t);
  ls.DeviceId = f->file_id;
  ls.Offset = (long long int) pos;
  ls.Origin = (DWORD) w;
  error = f->request(FD32_LSEEK, &ls);
  return (int) ls.Offset;
}

/* MZ format handling for VM86 */
static int isMZ(struct kern_funcs *kf, int f, struct read_funcs *rf)
{
  WORD magic;

  kf->file_read(f, &magic, 2);
  kf->file_seek(f, kf->file_offset, kf->seek_set);

  if (magic != 0x5A4D) { /* "MZ" */
    return 0;
  } else {
    return 1;
  }
}

/* MZ format handling for direct execution */
static int isMZ2(struct kern_funcs *kf, int f, struct read_funcs *rf)
{
  WORD magic;
  DWORD i, nt_sgn;
  struct dos_header hdr;
  DWORD dj_header_start;
  struct bin_format *binfmt = fd32_get_binfmt();

  kf->file_read(f, &magic, 2);
  kf->file_seek(f, kf->file_offset, kf->seek_set);

  if (magic != 0x5A4D) { /* "MZ" */
    return 0;
  }

#ifdef __MOD_DEBUG__
  kf->log("    Seems to be a DOS file...\n");
  kf->log("    Perhaps a PE? Only them are supported...\n");
#endif

  kf->file_read(f, &hdr, sizeof(struct dos_header));

  dj_header_start = hdr.e_cp * 512L;
  if (hdr.e_cblp) {
    dj_header_start -= (512L - hdr.e_cblp);
  }
  kf->file_seek(f, dj_header_start, kf->seek_set);
  
  for (i = 0; binfmt[i].name != NULL; i++)
  {
    if (strcmp(binfmt[i].name, "coff") == 0) {
      if (binfmt[i].check(kf, f, rf)) {
        kf->file_offset = dj_header_start;
      #ifdef __MOD_DEBUG__
        fd32_log_printf("    DJGPP COFF File\n");
      #endif
        return 1;
      }
    }
  }

  kf->file_seek(f, hdr.e_lfanew, kf->seek_set);
  kf->file_read(f, &nt_sgn, 4);
  
  kf->message("The magic : %lx\n", nt_sgn);
  if (nt_sgn == 0x00004550) {
    for (i = 0; binfmt[i].name != NULL; i++)
    {
      if (strcmp(binfmt[i].name, "pei") == 0) {
        if (binfmt[i].check(kf, f, rf)) {
        #ifdef __MOD_DEBUG__
          kf->log("It seems to be an NT PE\n");
        #endif
          return 1;
        }
      }
    }
  }

  return 0;
}


/* TODO: Re-consider the fcb1 and fcb2 to support multi-tasking */
static DWORD g_fcb1 = 0, g_fcb2 = 0, g_env_segment, g_env_segtmp;
static int vm86_exec_process(struct kern_funcs *kf, int f, struct read_funcs *rf,
		char *cmdline, char *args)
{
  struct dos_header hdr;
  struct psp *ppsp;
  X_REGS16 in, out;
  X_SREGS16 s;
  DWORD mem;
  DWORD stack;
  DWORD stack_size;
  BYTE *exec_text;
  BYTE *env_data;
  DWORD exec;
  DWORD exec_size;
  char truefilename[FD32_LFNPMAX];

  if (fd32_truename(truefilename, cmdline, FD32_TNSUBST) < 0) {
#ifdef __DOS_EXEC_DEBUG__
    fd32_log_printf("Cannot canonicalize the file name!!!\n");
#endif
    return -1;
  } else {
    cmdline = truefilename;
  }

  kf->file_read(f, &hdr, sizeof(struct dos_header));

  exec_size = hdr.e_cp*0x20-hdr.e_cparhdr+hdr.e_minalloc;
  stack_size = 0x4000;

  mem = dosmem_get(sizeof(struct psp)+exec_size*0x10+stack_size);
  /* NOTE: Align exec text at 0x10 boundary */
  if ((mem&0x0F) != 0) {
    message("[EXEC] Dos memory not at 0x10 boundary!\n");
  }

  ppsp = (struct psp *)mem;
  exec = mem+sizeof(struct psp);
  /* TODO: Stack is for VM86 init not the program's stack */
  stack = mem+sizeof(struct psp)+exec_size*0x10;

  exec_text = (BYTE *)exec;

  kf->file_seek(f, hdr.e_cparhdr*0x10, kf->seek_set);
  kf->file_read(f, exec_text, exec_size*0x10);

#ifdef __DOS_EXEC_DEBUG__
  fd32_log_printf("[DPMI] Exec at 0x%x: %x %x %x ... %x %x ...\n", (int)exec_text, exec_text[0], exec_text[1], exec_text[2], exec_text[0x100], exec_text[0x101]);
#endif
  env_data = (BYTE *)(g_env_segment<<4);
  env_data[0] = 0;
  env_data[1] = 1;
  env_data[2] = 0;
  /* NOTE: cmdline is only the filepath */
  strcpy(env_data + 3, cmdline);
#ifdef __DOS_EXEC_DEBUG__
  fd32_log_printf("[DPMI] ENVSEG: %x - %x %x %x %s\n", (int)g_env_segment, env_data[0], env_data[1], env_data[2], env_data+3);
#endif
  /* Init PSP */
  ppsp->ps_size = (exec+exec_size*0x10)>>4;
  ppsp->ps_parent = 0;
  ppsp->ps_environ = g_env_segment;
  ppsp->ps_maxfiles = 0x20;
  ppsp->ps_filetab = fd32_init_jft(0x20);

  if (g_fcb1 != NULL) {
    g_fcb1 = (g_fcb1>>12)+(g_fcb1&0x0000FFFF);
    memcpy(ppsp->def_fcb_1, (void *)g_fcb1, 16);
  }
  if (g_fcb2 != NULL) {
    g_fcb2 = (g_fcb2>>12)+(g_fcb2&0x0000FFFF);
    memcpy(ppsp->def_fcb_2, (void *)g_fcb2, 20);
  }
#ifdef __DOS_EXEC_DEBUG__
  fd32_log_printf("[DPMI] FCB: %x %x content: %x %x\n", (int)g_fcb1, (int)g_fcb2, *((BYTE *)g_fcb1), *((BYTE *)g_fcb2));
#endif
  
  s.ds = s.cs = (exec>>4)+hdr.e_cs;
  /* TODO: Set the ss and sp corretly, after fixing the INT stack */
  s.ss = stack>>4; // s.cs+hdr.e_ss;
  s.es = (DWORD)ppsp>>4;
  in.x.ax = 0;
  in.x.bx = 0;
  in.x.dx = s.ds;
  in.x.di = hdr.e_sp;
  in.x.si = hdr.e_ip;

  /* NOTE: Set the current_psp */
  extern struct psp *current_psp;
  current_psp = ppsp;
  /* Call in VM86 mode */
  vm86_call(hdr.e_ip, hdr.e_sp+0x3000, stack_size, (LIN_ADDR)stack, &in, &out, &s);
  dosmem_free(mem, sizeof(struct psp)+exec_size*0x10+stack_size);
  
  return 0;
}

int dos_exec_switch(int option)
{
  int res;

  switch(option)
  {
    case DOS_VM86_EXEC:
      g_env_segtmp = dosmem_get(0x100);
      g_env_segment = g_env_segtmp>>4;
      fd32_set_binfmt("mz", isMZ, vm86_exec_process);
      res = 1;
      break;
    case DOS_DIRECT_EXEC:
      fd32_set_binfmt("mz", isMZ2, fd32_exec_process);
      res = 1;
      break;
    default:
      res = 0;
      break;
  }
  
  return res;
}

int dos_exec(char *filename, DWORD env_segment, char *args,
		DWORD fcb1, DWORD fcb2, WORD *return_val)
{
  struct kern_funcs kf;
  struct read_funcs rf;
  struct bin_format *binfmt;
  struct funky_file f;
  void *fs_device;
  char *pathname;
  fd32_openfile_t of;
  DWORD i;

/* TODO: filename must be canonicalized with fd32_truename, but fd32_truename
         resolve the current directory, that is per process.
         Have we a valid current_psp at this point?
         Next, truefilename shall be used instead of filename.
  char truefilename[FD32_LFNPMAX];
  if (fd32_truename(truefilename, filename, FD32_TNSUBST) < 0) {
#ifdef __DEBUG__
    fd32_log_printf("Cannot canonicalize the file name!!!\n");
#endif
    return -1;
  } */
  if (fd32_get_drive(/*true*/filename, &f.request, &fs_device, &pathname) < 0) {
#ifdef __DOS_EXEC_DEBUG__
    fd32_log_printf("Cannot find the drive!!!\n");
#endif
    return -1;
  }
#ifdef __DOS_EXEC_DEBUG__
  fd32_log_printf("Opening %s\n", /*true*/filename);
#endif
  of.Size = sizeof(fd32_openfile_t);
  of.DeviceId = fs_device;
  of.FileName = pathname;
  of.Mode = O_RDONLY;
  if (f.request(FD32_OPENFILE, &of) < 0) {
#ifdef __DOS_EXEC_DEBUG__
    fd32_log_printf("File not found!!\n");
#endif
    return -1;
  }
  f.file_id = of.FileId;

#ifdef __DOS_EXEC_DEBUG__
  fd32_log_printf("FileId = 0x%lx (0x%lx)\n", (DWORD)f.file_id, (DWORD)&f);
#endif
  kf.file_read = my_read;
  kf.file_seek = my_seek;
  kf.mem_alloc = mem_get;
  kf.mem_alloc_region = mem_get_region;
  kf.mem_free = mem_free;
  kf.message = message;
  kf.log = fd32_log_printf;
  kf.error = message;
  kf.get_dll_table = get_dll_table;
  kf.add_dll_table = add_dll_table;
  kf.seek_set = FD32_SEEKSET;
  kf.seek_cur = FD32_SEEKCUR;
  kf.file_offset = 0;

  /* Get the binary format object table, ending with NULL name */
  binfmt = fd32_get_binfmt();
  
  /* Load different modules in various binary format */
  for (i = 0; binfmt[i].name != NULL; i++)
  {
    if (binfmt[i].check(&kf, (int)(&f), &rf)) {
      g_env_segment = env_segment;
      g_fcb1 = fcb1;
      g_fcb2 = fcb2;
      *return_val = binfmt[i].exec(&kf, (int)(&f), &rf, filename, args);
      break;
    }
#ifdef __DOS_EXEC_DEBUG__
    else {
      fd32_log_printf("[MOD] Not '%s' format\n", binfmt[i].name);
    }
#endif
    /* p->file_seek(file, p->file_offset, p->seek_set); */
  }

  /* TODO: Close file */
  return 1;
}