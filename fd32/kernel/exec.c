/* FD32 Executable file parser
 * It uses Dynalink to load generic executables or object files.
 * The object files are drivers or native applications
 * and need dynamic linking
 * by Luca Abeni
 * 
 * This is free software; see GPL.txt
 */

#include <ll/i386/hw-data.h>
#include <ll/i386/hw-instr.h>
#include <ll/i386/stdlib.h>
#include <ll/i386/cons.h>
#include <ll/i386/error.h>
#include <ll/i386/mem.h>
#include <ll/ctype.h>

#include "kmem.h"
#include "kernel.h"
#include "format.h"
#include "exec.h"
#include "logger.h"

void exec_process(struct kern_funcs *p, int file, struct read_funcs *parser, char *cmdline)
{
  //  int res;
  void (*fun)(void);
  DWORD offset;
  int retval;
  int size;
  DWORD exec_space;
  //  DWORD entry;
  int bss_sect, relocate, i;
  DWORD dyn_entry;
  int symsize;
  struct section_info *sections;
  struct symbol_info *symbols;
  struct table_info tables;
  int run(DWORD address, WORD psp_sel);


  dyn_entry = parser->read_headers(p, file, &tables);

  sections = (struct section_info *)mem_get(sizeof(struct section_info) * tables.num_sections);
  if (sections == 0) {
    error("Error allocating sections array\n");
    
    /* Should provide some error code... */
    return;
  }

  bss_sect = parser->read_section_headers(p, file, &tables, sections);
  symbols = NULL;
  if (tables.num_symbols != 0) {
    symsize = tables.num_symbols * sizeof (struct symbol_info);
    symbols = (struct symbol_info *)mem_get(symsize);
    if (symbols == 0) {
      error("Error allocating symbols table\n");
      
      /* Should provide some error code... */
      return;
    }
    parser->read_symbols(p, file, &tables, symbols);
  }

  relocate = 0;
  for (i = 0 ; i < tables.num_sections; i++) {
    if (sections[i].num_reloc != 0) {
      relocate = 1;
    }
  }
  
  if (dyn_entry == 0) {
    int init_sect;
    DWORD uninitspace;
    int res;
    void *kernel_symbols;

    exec_space = parser->load_relocatable(p, file, tables.num_sections, sections, &size);
    if (relocate == 0) {
      error("Warning: file is not an executable, but it has not any reloc info!!!\n");
    }
    if ((bss_sect < 0) || (bss_sect > tables.num_sections)) {
      error("Error: strange file --- no BSS section\n");
      /* TODO: Return code; free allocated resources... */
      return;
    }
    uninitspace = size;
#ifdef __EXEC_DEBUG__
    fd32_log_printf("[EXEC] Start of local BSS: 0x%lx\n", uninitspace);
#endif
    kernel_symbols = get_syscall_table();
    for (i = 0 ; i < tables.num_sections; i++) {
      res = parser->relocate_section(p, exec_space, uninitspace, sections,
				     i, symbols, kernel_symbols);
      if (res < 0) {
	error("Error: relocation failed!!!\n");
	/* TODO: Return code; free allocated resources... */
	return;
      }
    }
#ifdef __EXEC_DEBUG__
    fd32_log_printf("[EXEC] Searcing for the initialization function...");
#endif
    if (symbols == 0) {
      error("Error: no symbols!!!\n");
      /* TODO: Return code; free allocated resources... */
      return;
    }
    dyn_entry = (DWORD)parser->import_symbol(p, tables.num_symbols, symbols,
					     "_init", &init_sect);
    if (init_sect != -1) {
#ifdef __EXEC_DEBUG__
      fd32_log_printf("Found: (%d =  0x%x) 0x%lx\n", init_sect,
	      init_sect, dyn_entry);
      fd32_log_printf("       Section Base: 0x%lx       Exec Space: 0x%lx\n",
	      sections[init_sect].base, exec_space);
#endif
      dyn_entry += sections[init_sect].base + exec_space;
#ifdef __EXEC_DEBUG__
      fd32_log_printf("       Entry point: 0x%lx\n", dyn_entry);
      fd32_log_printf("       Going to run...\n");
#endif
      run(dyn_entry, 0);
#ifdef __EXEC_DEBUG__
      fd32_log_printf("       Returned\n");
#endif
    } else {
      message("Not found\n");
    }
  } else {
    exec_space = parser->load_executable(p, file, tables.num_sections, sections, &size);
    if (exec_space == 0) {
#ifdef __EXEC_DEBUG__
      message("Error decoding the Executable data\n");
#endif
      return;
    }
    offset = exec_space - sections[0].base;
    fun = (void *)(dyn_entry + offset);
#ifdef __EXEC_DEBUG__
    fd32_log_printf("[EXEC] Before calling 0x%lx  = 0x%lx + 0x%lx...\n",
	    (DWORD)fun, dyn_entry, offset);
#endif
    /*  retval = run(entry_offset + offset); */
    retval = create_process(dyn_entry + offset, exec_space, size, cmdline);
    message("Returned: %d!!!\n", retval);
    mem_free(exec_space, size);
  }
}