/* FD/32 Process Creation routines
 * by Luca Abeni & Hanzac Chen
 * 
 * This is free software; see GPL.txt
 */

#include <ll/i386/hw-data.h>
#include <ll/i386/stdlib.h>
#include <ll/i386/string.h>
#include <ll/getopt.h>
#include "kmem.h"
#include "kernel.h"

/* #define __PROCESS_DEBUG__ */

WORD kern_CS, kern_DS;

/* TODO: # Environ variables management
*/

/* extern DWORD current_SP; */
/* Top/kernel process info */
static struct process_info top_P = { NULL, NULL, NULL, NULL, 0, "fd32.bin", NULL, 0 };
static struct process_info *cur_P = &top_P;

/* Gets the Current Directiry List for the current process. */
/* Returns the pointer to the address of the first element. */
void **fd32_get_cdslist()
{
  return &cur_P->cds_list;
}

/* Gets the JFT for the current process.                          */
/* Returns the pointer to the JFT and fills the JftSize parameter */
/* with the number of entries of the JFT. JftSize may be NULL.    */
void *fd32_get_jft(int *JftSize)
{
  if (JftSize) *JftSize = cur_P->jft_size;
  return cur_P->jft;
}

struct process_info *fd32_get_current_pi(void)
{
  return cur_P;
}

void fd32_set_current_pi(struct process_info *ppi)
{
  ppi->prev_P = cur_P; /* TODO: The previous pi will be lost */
  cur_P = ppi;
}

void create_dll(DWORD entry, DWORD base, DWORD size)
{
  struct process_info pi;
  int dll_run(DWORD);

  /* DLL initialization */
#ifdef __EXEC_DEBUG__
  fd32_log_printf("       Entry point: 0x%lx\n", entry);
  fd32_log_printf("       Going to run...\n");
#endif
  pi.memlimit = base + size;
  fd32_set_current_pi(&pi);
  dll_run(entry);
  /* Back to the previous process */
  fd32_set_current_pi(pi.prev_P);
  /* CHECKME: I added these two lines... Are they correct??? Luca. */
  /* current_SP = current_psp->old_stack; */
}

/* TODO: It's probably better to separate create_process and run_process... */

int fd32_create_process(DWORD entry, DWORD base, DWORD size, WORD fs_sel, char *filename, char *args)
{
  int res;
  int run(DWORD address, WORD psp_sel, DWORD parm);

  /* No entry point... We assume that we need dynamic linking */
  cur_P->name = filename;
  cur_P->args = args;
  cur_P->memlimit = base + size;
  cur_P->cds_list = NULL; /* Pointer set by FS */
#ifdef __PROCESS_DEBUG__
  fd32_log_printf("[PROCESS] Going to run 0x%lx, size 0x%lx\n", entry, size);
  message("Mem Limit: 0x%lx = 0x%lx 0x%lx\n", cur_P->memlimit, base, size);
#endif
  res = run(entry, fs_sel, (DWORD)/*args*/cur_P);

#ifdef __PROCESS_DEBUG__
  fd32_log_printf("[PROCESS] Returned 0x%lx\n", res);
#endif

  return res;
}

int fd32_get_argv(char *filename, char *args, char ***_pargv)
{
  DWORD i;
  int _argc = 1, is_newarg;

  /* Get the argc */
  if (args != NULL) {
    for (i = 0; args[i] != 0; i++)
      if (args[i] == ' ')
        _argc++;
    /* Add one at the end */
    _argc++;
  }

  /* Allocate a argv and fill */
  *_pargv = (char **)mem_get(sizeof(char *)*_argc);
  (*_pargv)[0] = filename; /* The first argument is the filename */
  _argc = 1;
  if (args != NULL) {
    for (i = 0, is_newarg = 1; args[i] != 0; i++) {
      if (is_newarg)
        (*_pargv)[_argc] = args+i, is_newarg = 0;
      if (args[i] == ' ')
        args[i] = 0, _argc++, is_newarg = 1;
    }
    /* Add one at the end */
    _argc ++;
  }

  return _argc;
}

int fd32_unget_argv(int _argc, char *_argv[])
{
  DWORD i;

  /* NOTE: Reset getopt (for kernel modules) here */
  optind = 0;

  for (i = 0; i < _argc-1; i++)
    _argv[i][strlen(_argv[i])] = ' ';
  return mem_free((DWORD)_argv, sizeof(char *)*_argc);
}
