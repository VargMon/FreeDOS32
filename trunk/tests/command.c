/*
*   FILE NAME:
*       COMMAND.C
*
*   PROGRAMMER(S):
*       Allen S. Cheung (allencheung@fastmail.ca)
*
*   UPDATE:
*       15-Apr-2002
*
*   LICENSE:
*       GNU General Public License
*
*   HOW TO COMPILE AND USE WITH FREEDOS:
*       1) Install DJGPP on your development computer.
*       2) Install FreeDos on your target FreeDos computer.
*       3a) Compile the program with the accompanying make file.
*           type:          make -f command.mak
*       3b) OR, use rhide to build the program with the accompanying
*           project file (command.gpr).
*       4) Rename the resultant program from COMMAND.EXE to COMMAND.COM.
*       5) Copy COMMAND.COM to the root directory
*          of the boot drive of your target FreeDos computer.
*       6) Copy CWSDPMI.EXE (from DJGPP distro) to the root directory
*          of the boot drive of your target FreeDos computer.
*       7) Reboot said computer.
*
*   BUGS, PECULIARITIES, AND/OR UNIMPLEMENTED FEATURES
*       - Piping between commands is not yet implemented, although
*         you can pipe output to a file or pipe input from a file.
*       - Some commands are recognized but are unimplemented.
*         They are: MORE, and SET (when invoked withou args)
*       - Some commands are incomplete: TIME, DATE
*       - Some built-in commands should really be separate executables
*         and not built-in. They are: CHOICE, MORE, PAUSE, XCOPY.
*       - Some commands are not even recognized but should be.
*         They are: FOR, SHIFT
*
*   COPYRIGHT (C) 1997  CENTROID CORPORATION, HOWARD, PA 16841
*
***/

/* WARNING: This is not the original version.     */
/* Slightly modified for FreeDOS32 by Salvo Isaja */

#include <values.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <conio.h>
#include <crt0.h>
#include <unistd.h>
#include <dos.h>
#include <dir.h>
#include <go32.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <ctype.h>
#include <float.h>
#include <sys/exceptn.h>
#include <dpmi.h>

/*
*   These declarations/definitions turn off some unwanted DJGPP features
*/
#define UNUSED __attribute__((unused))
int _crt0_startup_flags =
       _CRT0_FLAG_USE_DOS_SLASHES |          // keep the backslashes
       _CRT0_FLAG_DISALLOW_RESPONSE_FILES |  // no response files (i.e. `@gcc.rf')
       _CRT0_FLAG_NO_LFN |                   // disable long file names
       _CRT0_FLAG_LOCK_MEMORY |              // disable virtual memory
       _CRT0_FLAG_PRESERVE_FILENAME_CASE;    // keep DOS names uppercase
char **__crt0_glob_function(char *_argument UNUSED) {return NULL;} // prevent wildcard expansion of arguments of main()
void __crt0_load_environment_file(char *_app_name UNUSED) {} // prevent loading of environment file

/*
*   Define true and false
*/
#define FALSE  (0)
#define TRUE   (!FALSE)

/*
*   Command.com shell modes
*/
#define SHELL_NORMAL           0  // interactive mode, user can exit
#define SHELL_PERMANENT        1  // interactive mode, user cannot exit
#define SHELL_SINGLE_CMD       2  // non-interactive, run one command, then exit
#define SHELL_STARTUP_WITH_CMD 3  // run one command on startup, interactive thereafter, user can exit
static int shell_mode = SHELL_NORMAL;

/*
*   Command parser defines/variables
*/
#define MAX_CMD_BUFLEN 128        // Define max command length
static char cmd_line[MAX_CMD_BUFLEN] = ""; // when this string is not "" it triggers command execution
static char cmd[MAX_CMD_BUFLEN] = "";
static char cmd_arg[MAX_CMD_BUFLEN] = "";
static char cmd_switch[MAX_CMD_BUFLEN] = "";
static char cmd_args[MAX_CMD_BUFLEN] = "";
static char goto_label[MAX_CMD_BUFLEN] = "";

/*
*   Command interpreter/executor defines/variables
*/
#define MAX_STACK_LEVEL        20 // Max number of batch file call stack levels
#define MAX_BAT_ARGS           9  // Max number of batch file arguments
static int need_to_crlf_at_next_prompt = TRUE;
static int stack_level = 0;
static int echo_on[MAX_STACK_LEVEL];
static char bat_file_path[MAX_STACK_LEVEL][FILENAME_MAX];  // when this string is not "" it triggers batch file execution
static char bat_arg[MAX_STACK_LEVEL][MAX_BAT_ARGS][MAX_CMD_BUFLEN];
static int bat_file_line_number[MAX_STACK_LEVEL];
static unsigned error_level = 0;  // Program execution return code

/*
*   Pipe variables and defines
*/
#define STDIN_INDEX  0
#define STDOUT_INDEX 1
static char pipe_file[2][MAX_CMD_BUFLEN] = {"",""};
static int pipe_file_redir_count[2];
static char pipe_to_cmd[MAX_CMD_BUFLEN] = "";
static int pipe_to_cmd_redir_count;

/*
*   Max subdirectory level, used by /S switch within XCOPY, ATTRIB and DELTREE
*/
#define MAX_SUBDIR_LEVEL     15

/*
*   File transfer modes
*/
#define FILE_XFER_COPY         0
#define FILE_XFER_XCOPY        1
#define FILE_XFER_MOVE         2

/*
*   Count of the number of valid commands
*/
#define CMD_TABLE_COUNT        (sizeof(cmd_table) / sizeof(cmd_table[0]))

/*
*   File attribute constants
*/
static const char attrib_letters[4] = {'R', 'A', 'S', 'H'};
static const unsigned attrib_values[4] = {_A_RDONLY, _A_ARCH, _A_SYSTEM, _A_HIDDEN};

/*
*   Some private prototypes
*/
static void parse_cmd_line(void);
static void perform_external_cmd(int call);
static void perform_set(void);
static void perform_unimplemented_cmd(void);

/***
*
*   FUNCTION:    conv_unix_path_to_ms_dos
*
*   PROGRAMMER:  Allen S. Cheung
*
*   UPDATE:      04-Jan-2001
*
*   PURPOSE:     Force the given filepath into MS-DOS style
*
*   CALL:        void conv_unix_path_to_ms_dos(char *path)
*
*   WHERE:       *path is a file path either in Unix or MS-DOS style,
*                    which will be converted to MS-dos style if
*                    it was in Unix format.  (I/R)
*
*   RETURN:       none
*
*   NOTES:
*
***/
static void conv_unix_path_to_ms_dos(char *path)
  {
  char *p = path;
  if (p != NULL)
    {
    while (*p != '\0')
      {
      if (*p == '/') *p = '\\'; // change slashes to backslashes
      *p = toupper(*p);         // change to uppercase
      p++;
      }
    }
  }

static int is_drive_spec(char *s)    // check for form "A:"
  {
  if (!isalpha(s[0]))
    return FALSE;
  if (s[1] != ':')
    return FALSE;
  if (s[2] != '\0')
    return FALSE;
  return TRUE;
  }

static int is_drive_spec_with_slash(char *s)  // check for form "C:\"
  {
  if (!isalpha(s[0]))
    return FALSE;
  if (s[1] != ':')
    return FALSE;
  if (s[2] != '\\')
    return FALSE;
  if (s[3] != '\0')
    return FALSE;
  return TRUE;
  }

static int has_trailing_slash(char *s)
  {
  if (*s == '\0')
    return FALSE;
  s = strchr(s,'\0')-1;
  if (*s == '\\' || *s == '/')
    return TRUE;
  return FALSE;
  }

static int has_wildcard(char *s)
  {
  if (strchr(s, '*') != NULL)
    return TRUE;
  if (strchr(s, '?') != NULL)
    return TRUE;
  return FALSE;
  }

static void reset_batfile_call_stack(void)
  {
  static int first_time = TRUE;
  int ba;

  if (!first_time)
    {
    if (bat_file_path[stack_level][0] != '\0')
      {
      cprintf("Batch file aborted - %s, line %d\r\n",
        bat_file_path[stack_level], bat_file_line_number[stack_level]);
      }
    }
  first_time = FALSE;
  // initialize stack
  for (stack_level = 0; stack_level < MAX_STACK_LEVEL; stack_level++)
    {
    bat_file_path[stack_level][0] = '\0';
    for (ba = 0; ba < MAX_BAT_ARGS; ba++)
      bat_arg[stack_level][ba][0] = '\0';
    bat_file_line_number[stack_level] = 0;
    echo_on[stack_level] = TRUE;
    }
  stack_level = 0;
  }

static void output_prompt(void)
  {
  char cur_drive_and_path[MAXPATH];
  char *promptvar = getenv("PROMPT");

  if (need_to_crlf_at_next_prompt)
    {
    cputs("\r\n");
    need_to_crlf_at_next_prompt = FALSE;
    }

  if (promptvar == NULL)
    promptvar = "$p$g";
  getcwd(cur_drive_and_path, MAXPATH);
  conv_unix_path_to_ms_dos(cur_drive_and_path);
  while (*promptvar != '\0')
    {
    if (*promptvar == '$')
      {
      promptvar++;
      switch (toupper(*promptvar))
        {
        case '\0':
          promptvar--;
          break;
        case 'Q': //    = (equal sign)
          putch('=');
          break;
        case '$': //    $ (dollar sign)
          putch('$');
          break;
        case 'T': //    Current time
          {
          struct time t;
          gettime(&t);
          cprintf("%2d:%02d:%02d.%02d", t.ti_hour, t.ti_min, t.ti_sec, t.ti_hund);
          break;
          }
        case 'D': //    Current date
          {
          struct date d;
          getdate(&d);
          cprintf("%02d-%02d-%04d", d.da_mon, d.da_day, d.da_year);
          break;
          }
        case 'P': //   Current drive and path
          {
          cputs(cur_drive_and_path);
          break;
          }
        case 'N': //    Current drive
          {
          putch(*cur_drive_and_path);
          putch(':');
          break;
          }
        case 'G': //    > (greater-than sign)
          putch('>');
          break;
        case 'L': //    < (less-than sign)
          putch('<');
          break;
        case 'B': //    | (pipe)
          putch('|');
          break;
        case '_': //    ENTER-LINEFEED
          cputs("\r\n");
          break;
        case 'E': //    ASCII escape code
          putch(27);
          break;
        case 'H': //    Backspace
          putch(8);
          break;
        default:
          putch('$');
          putch(*promptvar);
          break;
        }
      }
    else
      putch(*promptvar);
    promptvar++;
    }
  }

static void extract_args(char *src)
  {
  char *dest, *saved_src = src;

  // scout ahead to see if there are really any arguments
  while (*src == ' ' || *src == '\t')
    src++;
  if (*src == '\0')
    {
    cmd_arg[0] = '\0';
    cmd_switch[0] = '\0';
    cmd_args[0] = '\0';
    return;
    }

  // extract combined arguments
  src = saved_src;
  if (*src == ' ' || *src == '\t')
    src++;
  memmove(cmd_args, src, strlen(src)+1);

  // extract first occurring single argument
  src = cmd_args;
  while (*src == ' ' || *src == '\t')
    src++;
  dest = cmd_arg;
  while (*src != ' ' && *src != '\t' && *src != '\0')
    {
    *dest = *src;
    dest++;
    src++;
    if (*src == '/')
      break;
    }
  *dest = '\0';

  // copy the single argument to cmd_switch if it qualifies as a switch
  if (cmd_arg[0] == '/')
    strcpy(cmd_switch, cmd_arg);
  else
    cmd_switch[0] = '\0';
  return;
  }

static void advance_cmd_arg(void)
  {
  char *extr;

  extr = cmd_args;

  // skip over first argument
  while (*extr == ' ' || *extr == '\t')
    extr++;
  if (*extr == '\0')
    goto NoArgs;

  while (*extr != ' ' && *extr != '\t' && *extr != '\0')
    {
    extr++;
    if (*extr == '/')
      break;
    }
  if (*extr == '\0')
    goto NoArgs;

  // extract the rest
  extract_args(extr);
  return;

NoArgs:
  cmd_arg[0] = '\0';
  cmd_switch[0] = '\0';
  cmd_args[0] = '\0';
  return;
  }

static void prompt_for_and_get_cmd(void)
  {
  char conbuf[MAX_CMD_BUFLEN+1];

  output_prompt();
  conbuf[0] = MAX_CMD_BUFLEN-1;
  //-Salvo: was while(kbhit()) getch();
  while(kbhit())
  {
    int c = getch();
    printf("while(kbhit()) getch(): '%02x'\n", c);
  }
  cgets(conbuf);

  strncpy(cmd_line,&(conbuf[2]),conbuf[1]);
  cmd_line[(int)(conbuf[1])] = '\0';
  parse_cmd_line();
  cputs("\r\n");
  }

static int get_choice(char *choices)
  {
  int choice, key;
  strupr(choices);
  do
    {
    key = getkey();
    if (key == 0)
      continue;
    } while (strchr(choices, toupper(key)) == NULL);
  choice = toupper(key);
  cprintf("%c", choice);
  cputs("\r\n");
  return choice;
  }

static void get_cmd_from_bat_file(void)
  {
  FILE *cmd_file;
  int line_num, c, ba;
  char *s;

  if (bat_file_line_number[stack_level] != MAXINT)
    bat_file_line_number[stack_level]++;

  cmd_file = fopen(bat_file_path[stack_level], "rt");
  if (cmd_file == NULL)
    {
    cprintf("Cannot open %s\r\n", bat_file_path[stack_level]);
    goto ErrorDone;
    }

  for (line_num = 0; line_num < bat_file_line_number[stack_level]; line_num++)
    {
    /* input as much of the line as the buffer can hold */
    s = fgets(cmd_line, MAX_CMD_BUFLEN, cmd_file);
  
    /* if s is null, investigate why */
    if (s == NULL)
      {
      /* check for error */
      if (ferror(cmd_file))
        {
        cprintf("Read error: %s, line %d\r\n", bat_file_path[stack_level], line_num+1);
        goto ErrorDone;
        }
      /* line is unavailable because of end of file */
      if (goto_label[0] != '\0')
        {
        cprintf("Label not found - %s\r\n", goto_label);
        goto ErrorDone;
        }
      goto FileDone;
      }

    /*
    *   Check for newline character;
    *   If present, we have successfully reached end of line
    *   If not present, continue getting line until newline or eof encountered
    */
    s = strchr(cmd_line, '\n');
    if (s != NULL)
      *s = '\0';
    else
      {
      do
        {
        c = fgetc(cmd_file);
        } while (c != '\n' && c != EOF);  // if eof occurs here, it needs
                                         // to be caught on the next iteration
                                         // but not now, and not here.
      if (ferror(cmd_file))
        {
        cprintf("Read error: %s, line %d\r\n", bat_file_path[stack_level], line_num+1);
        goto ErrorDone;
        }
      }

    // check for goto arrival at labeled destination
    if (goto_label[0] != '\0')
      {
      s = cmd_line;
      while (*s == ' ' || *s == '\t')
        s++;
      if (*s == ':')
        {
        s++;
        if (strnicmp(goto_label, s, strlen(goto_label)) == 0)
          {
          s += strlen(goto_label);
          if (*s == ' ' || *s == '\t' || *s == '\0')
            {
            // we have arrived... set line number, erase goto label
            bat_file_line_number[stack_level] = line_num + 1;
            goto_label[0] = '\0';
            break;
            }
          }
        }
      }
    }

  // parse command
  parse_cmd_line();

  // deal with echo on/off and '@' at the beginning of the command line
  if (cmd[0] == '@')
    memmove(cmd, cmd+1, strlen(cmd));
  else
    {
    if (echo_on[stack_level])
      {
      output_prompt();
      cputs(cmd_line);
      cputs("\r\n");
      }
    }

  goto RoutineDone;

ErrorDone:
  reset_batfile_call_stack();
FileDone:
  cmd_line[0] = '\0';
  parse_cmd_line();  // this clears cmd[], cmd_arg[], cmd_switch[], and cmd_args[]
  bat_file_path[stack_level][0] = '\0';
  for (ba = 0; ba < MAX_BAT_ARGS; ba++)
    bat_arg[stack_level][ba][0] = '\0';
  bat_file_line_number[stack_level] = 0;
  echo_on[stack_level] = TRUE;
  if (stack_level > 0)
    stack_level--;
RoutineDone:
  if (cmd_file != NULL)
    fclose(cmd_file);
  }

static int ensure_dir_existence(char *dir)
  {
  char *c;
  char dir_path[MAXPATH];

  strcpy(dir_path, dir);
  if (*(strchr(dir_path, '\0')-1) == '\\')     // take away ending backslash
    *(strchr(dir_path, '\0')-1) = '\0';

  if (access(dir_path, D_OK) != 0)
    {
    c = strchr(dir_path, '\\');
    while (c != NULL)
      {
      c = strchr(c+1, '\\');
      if (c == NULL)
        printf("Creating directory - %s\\\n", dir_path);
      else
        *c = '\0';
      if (mkdir(dir_path, S_IWUSR) != 0 && c == NULL)
        {
        cprintf("Unable to create directory - %s\\\r\n", dir_path);
        return -1;
        }
      if (c != NULL)
        *c = '\\';
      }
    }
  return 0;
  }

static int copy_single_file(char *source_file, char *dest_file, int transfer_type)
  {
  FILE *source_stream;
  FILE *dest_stream;
  char transfer_buffer[32768];
  size_t byte_count;
  struct ftime file_time;

  if (stricmp(source_file, dest_file) == 0)
    {
    cprintf("Source and destination cannot match - %s\r\n", source_file);
    return -1;
    }

  /* Open file for copy */
  source_stream = fopen(source_file, "rb");
  if (source_stream == NULL)
    {
    cprintf("Unable to open source file - %s\r\n", source_file);
    return -1;
    }
  dest_stream = fopen(dest_file, "wb");
  if (dest_stream == NULL)
    {
    cprintf("Unable to open destination file - %s\r\n", dest_file);
    fclose(source_stream);
    return -1;
    }

  /* Copy file contents*/
  do
    {
    byte_count = fread(transfer_buffer, 1, 32768, source_stream);
    if (byte_count > 0)
      {
      if (fwrite(transfer_buffer, 1, byte_count, dest_stream) != byte_count)
        goto copy_error_close;
      }
    }
  while (byte_count > 0);

  /* Copy date and time */
  if (getftime(fileno(source_stream), &file_time) != 0)
    goto copy_error_close;
  fflush(dest_stream);
  if (setftime(fileno(dest_stream), &file_time) != 0)
    goto copy_error_close;

  /* Close source and dest files */
  fclose(source_stream);
  if (fclose(dest_stream) != 0)
    goto copy_error;
  return 0;

/*
*    Error routine
*/
copy_error_close:
  fclose(source_stream);
  fclose(dest_stream);

copy_error:
  remove(dest_file);          // erase the unfinished file
  if (transfer_type == FILE_XFER_MOVE)
    cprintf("Error occurred while moving file - %s\r\n", source_file);
  else
    cprintf("Error occurred while copying to file - %s\r\n", dest_file);
  return -1;
  }  /* copy_single_file */

static int verify_file(char *master_file, char *verify_file)
  {
  FILE *mstream;
  FILE *vstream;
  char mtransfer_buffer[32768];
  char vtransfer_buffer[32768];
  int b;
  size_t mbyte_count, vbyte_count;
  struct ftime mfile_time, vfile_time;


  /* Open files */
  mstream = fopen(master_file, "rb");
  if (mstream == NULL)
    goto verify_error;
  vstream = fopen(verify_file, "rb");
  if (vstream == NULL)
    {
    fclose(mstream);
    goto verify_error;
    }

  /* Verify file contents*/
  do
    {
    mbyte_count = fread(mtransfer_buffer, 1, 32768, mstream);
    vbyte_count = fread(vtransfer_buffer, 1, 32768, vstream);
    if (mbyte_count != vbyte_count)
      goto verify_error_close;
    if (mbyte_count > 0)
      {
      for (b = 0; b < mbyte_count; b++)
        {
        if (mtransfer_buffer[b] != vtransfer_buffer[b])
          goto verify_error_close;
        }
      }
    }
  while (mbyte_count > 0);

  /* verify date and time */
  if (getftime(fileno(mstream), &mfile_time) != 0)
    goto verify_error_close;
  if (getftime(fileno(vstream), &vfile_time) != 0)
    goto verify_error_close;
  if (memcmp(&mfile_time, &vfile_time, sizeof(struct ftime)) != 0)
    goto verify_error_close;

  /* Close source and dest files */
  fclose(mstream);
  fclose(vstream);
  return 0;

/*
*    Error routine
*/
verify_error_close:
  fclose(mstream);
  fclose(vstream);

verify_error:
  cprintf("Verify failed - %s\r\n", verify_file);
  return -1;
  }

static void general_file_transfer(int transfer_type)
  {
  int xfer_count = 0;
  int ffrc;
  int traverse_subdirs = FALSE;
  int copy_empty_subdirs = FALSE;
  int xcopy_dont_ask_fd_question = FALSE;
  int do_file_verify;
  int s, subdir_level = 0;
  struct ffblk ff[MAX_SUBDIR_LEVEL];
  char dir_name[MAX_SUBDIR_LEVEL][MAXPATH];
  int visitation_mode[MAX_SUBDIR_LEVEL]; // 4 = findfirst source_filespec for files;
                                         // 3 = findnext source_filespec for files;
                                         // 2 = findfirst *.* for subdirs;
                                         // 1 = findnext *.* for subdirs;
                                         // 0 = done
  unsigned attrib;
  char drivespec[MAXDRIVE], dirspec[MAXDIR], filespec[MAXFILE], extspec[MAXEXT];
  char temp_path[MAXPATH];
  char source_path[MAXPATH] = "", source_filespec[MAXPATH];
  char dest_path[MAXPATH] = "", dest_filespec[MAXPATH];
  char full_source_filespec[MAXPATH];
  char full_dest_filespec[MAXPATH];
  char full_dest_dirspec[MAXPATH];

  if (transfer_type == FILE_XFER_MOVE)
    do_file_verify = TRUE;
  else
    do_file_verify = FALSE;

  while (*cmd_arg != '\0')
    {
    if (*cmd_switch == '\0') // if not a command switch ...
      {
      if (*source_path == '\0')
        {
        strncpy(source_path, cmd_arg, MAXPATH);
        source_path[MAXPATH-1] = '\0';
        conv_unix_path_to_ms_dos(source_path);
        }
      else if (*dest_path == '\0')
        {
        strncpy(dest_path, cmd_arg, MAXPATH);
        dest_path[MAXPATH-1] = '\0';
        conv_unix_path_to_ms_dos(dest_path);
        }
      else
        {
        cprintf("Too many parameters - %s\r\n", cmd_args);
        reset_batfile_call_stack();
        return;
        }
      }
    else
      {
      if (stricmp(cmd_switch,"/v") == 0)
        {
        if (transfer_type == FILE_XFER_COPY ||
            transfer_type == FILE_XFER_XCOPY)
          do_file_verify = TRUE;
        else
          goto InvalidSwitch;
        }
      else
        {
        if (transfer_type == FILE_XFER_XCOPY)
          {
          if (stricmp(cmd_switch,"/s") == 0)
            traverse_subdirs = TRUE;
          else if (stricmp(cmd_switch,"/e") == 0)
            copy_empty_subdirs = TRUE;
          else if (stricmp(cmd_switch,"/i") == 0)
            xcopy_dont_ask_fd_question = TRUE;
          else
            goto InvalidSwitch;
          }
        else
          goto InvalidSwitch;
        }
      }
    advance_cmd_arg();
    }

  if (*source_path == '\0' ||
      (transfer_type == FILE_XFER_MOVE && *dest_path == '\0'))
    {
    cputs("Required parameter missing\r\n");
    reset_batfile_call_stack();
    return;
    }

  if (*dest_path == '\0')
    strcpy(dest_path, ".");

  // prepare source for fnsplit() -
  // attach a file specification if specified source doesn't have one
  if (is_drive_spec(source_path) || has_trailing_slash(source_path))
    strcat(source_path, "*.*");
  else
    {
    // see if source exists and is a directory; if so, attach a file spec
    if (access(source_path, D_OK) == 0)
      strcat(source_path, "\\*.*");
    }

  // parse source - create full source path and split into 2 components: path + file spec
  _fixpath(source_path, temp_path);
  fnsplit(temp_path, drivespec, dirspec, filespec, extspec);
  strcpy(source_path, drivespec);
  strcat(source_path, dirspec);
  conv_unix_path_to_ms_dos(source_path);
  strcpy(source_filespec, filespec);
  strcat(source_filespec, extspec);
  conv_unix_path_to_ms_dos(source_filespec);

  // prepare dest for fnsplit() -
  // attach a file specification if specified dest doesn't have one
  if (is_drive_spec(dest_path) || has_trailing_slash(dest_path))
    strcat(dest_path, "*.*");
  else
    {
    // see if dest exists and is a directory; if so, attach a file spec
    if (access(dest_path, D_OK) == 0)
      strcat(dest_path, "\\*.*");
    else  // else -- if dest does not exist or is not a directory...
      {
      if ((transfer_type == FILE_XFER_XCOPY && xcopy_dont_ask_fd_question) ||
          transfer_type == FILE_XFER_MOVE)
        {
        // if source has a wildcard and dest does not, then treat dest as a dir ...
        if (has_wildcard(source_filespec) && !has_wildcard(dest_path))
          strcat(dest_path, "\\*.*");     // dest is a directory; attach a file spec
        }
      else
        {
        if (transfer_type == FILE_XFER_XCOPY)  // if we are doing xcopy, ask if target is a dir or a file
          {
          fnsplit(dest_path, NULL, NULL, filespec, extspec);
          cprintf("Does %s%s specify a file name\r\n", filespec, extspec);
          cputs("or directory name on the target\r\n");
          cputs("(F = file, D = directory)?");
          if (get_choice("FD") == 'D')
            strcat(dest_path, "\\*.*");
          }
        }
      }
    }

  // parse dest - create full dest path and split into 2 components: path + file spec
  _fixpath(dest_path, temp_path);
  fnsplit(temp_path, drivespec, dirspec, filespec, extspec);
  strcpy(dest_path, drivespec);
  strcat(dest_path, dirspec);
  conv_unix_path_to_ms_dos(dest_path);
  strcpy(dest_filespec, filespec);
  strcat(dest_filespec, extspec);
  conv_unix_path_to_ms_dos(dest_filespec);

  // don't allow wildcard on the destination, except for *.*
  if ((has_wildcard(dest_filespec) &&
       strcmp(dest_filespec, "*.*") != 0) ||
      has_wildcard(dest_path))
    {
    cputs("Illegal wildcard on destination\r\n");
    reset_batfile_call_stack();
    return;
    }

  // Stuff for the move command only
  if (transfer_type == FILE_XFER_MOVE)
    {
    // if source and dest are both full directories in the same
    // tree and on the same drive, and the dest directory does not exist,
    // then just rename source directory to dest
    if (strcmp(source_filespec, "*.*") == 0 &&
        !is_drive_spec_with_slash(source_path) &&
        strcmp(dest_filespec, "*.*") == 0 &&
        !is_drive_spec_with_slash(dest_path))
      {
      char source_dirspec[MAXPATH];
      char dest_dirspec[MAXPATH];
      int dest_dir_exists;
      char *sbs, *dbs;  // backslash ptrs

      // check for both dirs to be at same tree and on the same drive
      strcpy(source_dirspec, source_path);
      strcpy(dest_dirspec, dest_path);
      *(strrchr(source_dirspec, '\\')) = '\0'; // get rid of trailing backslash
      *(strrchr(dest_dirspec, '\\')) = '\0'; // get rid of trailing backslash
      dest_dir_exists = (access(dest_dirspec, D_OK) == 0);
      sbs = strrchr(source_dirspec, '\\');
      *sbs = '\0'; // chop off source dir name, leaving source tree
      dbs = strrchr(dest_dirspec, '\\');
      *dbs = '\0'; // chop off dest dir name, leaving dest tree

      if (stricmp(source_dirspec, dest_dirspec) == 0) // if source tree == dest tree
        {
        if (!dest_dir_exists) // if dest dir does not exist..
          {
          *sbs = '\\'; // put the backslash back
          *dbs = '\\'; // put the backslash back
          if (_rename(source_dirspec, dest_dirspec) == 0)
            {
            printf("%s renamed to %s\n", source_dirspec, dbs+1);
            return;
            }
          }
        }
      }
    }

  // visit each directory; perform transfer
  visitation_mode[0] = 4;
  dir_name[0][0] = '\0';
  while (subdir_level >= 0)
    {
    if (visitation_mode[subdir_level] == 4 || visitation_mode[subdir_level] == 2)
      {
      strcpy(full_source_filespec, source_path);
      for (s = 0; s <= subdir_level; s++)
        strcat(full_source_filespec, dir_name[s]);
      if (visitation_mode[subdir_level] == 4)
        {
        strcat(full_source_filespec, source_filespec);
        attrib = 0+FA_HIDDEN+FA_SYSTEM;
        }
      else
        {
        strcat(full_source_filespec, "*.*");
        attrib = 0+FA_DIREC+FA_HIDDEN+FA_SYSTEM;
        }
      ffrc = findfirst(full_source_filespec, &(ff[subdir_level]), attrib);
      visitation_mode[subdir_level]--;
      }
    else
      ffrc = findnext(&(ff[subdir_level]));
    if (ffrc == 0)
      {
      conv_unix_path_to_ms_dos(ff[subdir_level].ff_name);
      strcpy(full_source_filespec, source_path);
      strcpy(full_dest_filespec, dest_path);
      strcpy(full_dest_dirspec, dest_path);
      for (s = 0; s <= subdir_level; s++)
        {
        strcat(full_source_filespec, dir_name[s]);
        strcat(full_dest_filespec, dir_name[s]);
        strcat(full_dest_dirspec, dir_name[s]);
        }
      strcat(full_source_filespec, ff[subdir_level].ff_name);
      if (strcmp(dest_filespec, "*.*") == 0)
        strcat(full_dest_filespec, ff[subdir_level].ff_name);
      else
        strcat(full_dest_filespec, dest_filespec);
  
      if ((ff[subdir_level].ff_attrib&FA_DIREC) != 0)
        {
        if (visitation_mode[subdir_level] <= 2 &&
            traverse_subdirs &&
            strcmp(ff[subdir_level].ff_name,".") != 0 &&
            strcmp(ff[subdir_level].ff_name,"..") != 0)
          {
          subdir_level++;
          if (subdir_level >= MAX_SUBDIR_LEVEL)
            {
            cputs("Directory tree is too deep\r\n");
            reset_batfile_call_stack();
            return;
            }
          if (copy_empty_subdirs)
            {
            if (ensure_dir_existence(full_dest_filespec) != 0)
              {
              reset_batfile_call_stack();
              return;
              }
            }
          visitation_mode[subdir_level] = 4;
          strcpy(dir_name[subdir_level], ff[subdir_level-1].ff_name);
          strcat(dir_name[subdir_level], "\\");
          }
        }
      else
        {
        if (visitation_mode[subdir_level] > 2)
          {
          if (transfer_type == FILE_XFER_XCOPY ||
              transfer_type == FILE_XFER_MOVE)
            {
            if (ensure_dir_existence(full_dest_dirspec) != 0)
              {
              reset_batfile_call_stack();
              return;
              }
            }
          if (copy_single_file(full_source_filespec,
                               full_dest_filespec, transfer_type) != 0)
            {
            reset_batfile_call_stack();
            return;
            }
          if (do_file_verify)
            {
            if (verify_file(full_source_filespec, full_dest_filespec) != 0)
              {
              reset_batfile_call_stack();
              return;
              }
            }
          if (transfer_type == FILE_XFER_MOVE)
            {
            if (remove(full_source_filespec) != 0)
              {
              remove(full_dest_filespec);
              cprintf("Unable to move file - %s\r\n", full_source_filespec);
              reset_batfile_call_stack();
              return;
              }
            }
          printf("%s %s to %s\n",
            ff[subdir_level].ff_name,
            transfer_type == FILE_XFER_MOVE?"moved":"copied",
            strcmp(dest_filespec, "*.*")==0?full_dest_dirspec:full_dest_filespec);
          xfer_count++;
          }
        }
      }
    else
      {
      if (traverse_subdirs)
        visitation_mode[subdir_level]--;
      else
        visitation_mode[subdir_level] = 0;
      if (visitation_mode[subdir_level] <= 0)
        subdir_level--;
      }
    }
  if (xfer_count == 0)
    printf("File(s) not found - %s%s\n", source_path, source_filespec);
  else
    {
    if (transfer_type == FILE_XFER_MOVE)
      printf("%9d file(s) moved\n", xfer_count);
    else
      printf("%9d file(s) copied\n", xfer_count);
    }
  return;

InvalidSwitch:
  cprintf("Invalid switch - %s\r\n", cmd_switch);
  reset_batfile_call_stack();
  return;
  }

static int get_set_file_attribute(char *full_path_filespec, unsigned req_attrib, unsigned attrib_mask)
  {
  int a;
  unsigned actual_attrib;

  if (attrib_mask == 0)
    {
    if (_dos_getfileattr(full_path_filespec, &actual_attrib) != 0)
      {
      cprintf("Cannot read attribute - %s\r\n", full_path_filespec);
      return -1;
      }
    }
  else
    {
    if (_dos_getfileattr(full_path_filespec, &actual_attrib) != 0)
      actual_attrib = 0;
    actual_attrib &= (~attrib_mask);
    actual_attrib |= (req_attrib & attrib_mask);
    if (_dos_setfileattr(full_path_filespec, actual_attrib) != 0)
      goto CantSetAttr;
    printf("Attribute set to ");
    }

  for (a = 0; a < 4; a++)
    {
    if ((actual_attrib&attrib_values[a]) == 0)
      printf(" -%c",tolower(attrib_letters[a]));
    else
      printf(" +%c",toupper(attrib_letters[a]));
    }
  printf("  - %s\n", full_path_filespec);
  return 0;

CantSetAttr:
  cprintf("Cannot set attribute - %s\r\n", full_path_filespec);
  return -1;
  }

///////////////////////////////////////////////////////////////////////////////////

static void perform_attrib(void)
  {
  int file_count = 0;
  int ffrc;
  int traverse_subdirs = FALSE;
  int s, subdir_level = 0;
  struct ffblk ff[MAX_SUBDIR_LEVEL];
  char dir_name[MAX_SUBDIR_LEVEL][MAXPATH];
  int visitation_mode[MAX_SUBDIR_LEVEL]; // 4 = findfirst source_filespec for files;
                                         // 3 = findnext source_filespec for files;
                                         // 2 = findfirst *.* for subdirs;
                                         // 1 = findnext *.* for subdirs;
                                         // 0 = done
  char drivespec[MAXDRIVE], dirspec[MAXDIR], filename[MAXFILE], extspec[MAXEXT];
  char temp_path[MAXPATH];
  char path[MAXPATH] = "", filespec[MAXPATH];
  char full_path_filespec[MAXPATH];

  int a;
  unsigned req_attrib = 0, attrib_mask = 0;
  unsigned search_attrib;

  while (*cmd_arg != '\0')
    {
    if (*cmd_switch == '\0') // if not a command switch ...
      {
      if (strlen(cmd_arg) == 2 && (cmd_arg[0] == '+' || cmd_arg[0] == '-'))
        {
        for (a = 0; a < 4; a++)
          {
          if (toupper(cmd_arg[1]) == toupper(attrib_letters[a]))
            {
            attrib_mask |= attrib_values[a];
            if (cmd_arg[0] == '+')
              req_attrib |= attrib_values[a];
            else
              req_attrib &= (~(attrib_values[a]));
            }
          }
        }
      else if (*path == '\0')
        {
        strncpy(path, cmd_arg, MAXPATH);
        path[MAXPATH-1] = '\0';
        conv_unix_path_to_ms_dos(path);
        }
      else
        {
        cprintf("Too many parameters - %s\r\n", cmd_args);
        reset_batfile_call_stack();
        return;
        }
      }
    else
      {
      if (stricmp(cmd_switch,"/s") == 0)
        traverse_subdirs = TRUE;
      else
        {
        cprintf("Invalid switch - %s\r\n", cmd_switch);
        reset_batfile_call_stack();
        return;
        }
      }
    advance_cmd_arg();
    }

  if (*path == '\0')
    strcpy(path, "*.*");

  // prepare path for fnsplit() -
  // attach a file specification if specified path doesn't have one
  if (is_drive_spec(path) || has_trailing_slash(path))
    strcat(path, "*.*");
  else
    {
    // see if path exists and is a directory; if so, attach a file spec
    if (access(path, D_OK) == 0)
      strcat(path, "\\*.*");
    }

  // parse path - create full path and split into 2 components: path + file spec
  _fixpath(path, temp_path);
  fnsplit(temp_path, drivespec, dirspec, filename, extspec);
  strcpy(path, drivespec);
  strcat(path, dirspec);
  conv_unix_path_to_ms_dos(path);
  strcpy(filespec, filename);
  strcat(filespec, extspec);
  conv_unix_path_to_ms_dos(filespec);

  // visit each directory; perform attrib get/set
  visitation_mode[0] = 4;
  dir_name[0][0] = '\0';
  while (subdir_level >= 0)
    {
    if (visitation_mode[subdir_level] == 4 || visitation_mode[subdir_level] == 2)
      {
      strcpy(full_path_filespec, path);
      for (s = 0; s <= subdir_level; s++)
        strcat(full_path_filespec, dir_name[s]);
      if (visitation_mode[subdir_level] == 4)
        {
        strcat(full_path_filespec, filespec);
        search_attrib = FA_RDONLY+FA_ARCH+FA_SYSTEM+FA_HIDDEN;
        }
      else
        {
        strcat(full_path_filespec, "*.*");
        search_attrib = FA_DIREC+FA_RDONLY+FA_ARCH+FA_SYSTEM+FA_HIDDEN;
        }
      ffrc = findfirst(full_path_filespec, &(ff[subdir_level]), search_attrib);
      visitation_mode[subdir_level]--;
      }
    else
      ffrc = findnext(&(ff[subdir_level]));
    if (ffrc == 0)
      {
      conv_unix_path_to_ms_dos(ff[subdir_level].ff_name);
      strcpy(full_path_filespec, path);
      for (s = 0; s <= subdir_level; s++)
        strcat(full_path_filespec, dir_name[s]);
      strcat(full_path_filespec, ff[subdir_level].ff_name);
  
      if ((ff[subdir_level].ff_attrib&FA_DIREC) != 0)
        {
        if (visitation_mode[subdir_level] <= 2 &&
            traverse_subdirs &&
            strcmp(ff[subdir_level].ff_name,".") != 0 &&
            strcmp(ff[subdir_level].ff_name,"..") != 0)
          {
          subdir_level++;
          if (subdir_level >= MAX_SUBDIR_LEVEL)
            {
            cputs("Directory tree is too deep\r\n");
            reset_batfile_call_stack();
            return;
            }
          visitation_mode[subdir_level] = 4;
          strcpy(dir_name[subdir_level], ff[subdir_level-1].ff_name);
          strcat(dir_name[subdir_level], "\\");
          }
        }
      else
        {
        if (visitation_mode[subdir_level] > 2)
          {
          if (get_set_file_attribute(full_path_filespec, req_attrib, attrib_mask) != 0)
            {
            reset_batfile_call_stack();
            return;
            }
          file_count++;
          }
        }
      }
    else
      {
      if (traverse_subdirs)
        visitation_mode[subdir_level]--;
      else
        visitation_mode[subdir_level] = 0;
      if (visitation_mode[subdir_level] <= 0)
        subdir_level--;
      }
    }
  if (file_count == 0)
    printf("File(s) not found - %s%s\n", path, filespec);
  }

static void perform_call(void)
  {
  while (*cmd_switch)  // skip switches
    advance_cmd_arg();
  strcpy(cmd, cmd_arg);
  advance_cmd_arg();
  perform_external_cmd(TRUE);
  }

static void perform_cd(void)
  {
  while (*cmd_switch)  // skip switches
    advance_cmd_arg();
  if (*cmd_arg)
    {
    if (chdir(cmd_arg) != 0)
      {
      cprintf("Directory does not exist - %s\r\n",cmd_arg);
      reset_batfile_call_stack();
      return;
      }
    }
  else
    {
    char cur_drive_and_path[MAXPATH];
    getcwd(cur_drive_and_path, MAXPATH);
    conv_unix_path_to_ms_dos(cur_drive_and_path);
    puts(cur_drive_and_path);
    }
  }

static void perform_change_drive(void)
  {
  int drive_set, cur_drive, dummy;
  drive_set = toupper(cmd[0])-'A'+1;
  _dos_setdrive(drive_set, &dummy);
  _dos_getdrive(&cur_drive);
  if (cur_drive != drive_set)
    {
    cprintf("Invalid drive specification - %s\r\n", cmd);
    reset_batfile_call_stack();
    }
  }

static void perform_choice(void)
  {
  char *choices = "YN";  // Y,N are the default choices
  char *text = "";
  int supress_prompt = FALSE;
  int choice;

  while (*cmd_arg != '\0')
    {
    if (*cmd_switch == '\0') // if not a command switch ...
      {
      text = cmd_args;
      break;
      }
    else
      {
      if (strnicmp(cmd_switch,"/c:", 3) == 0 && strlen(cmd_switch) > 3)
        choices = cmd_switch+3;
      else if (stricmp(cmd_switch,"/n") == 0)
        supress_prompt = TRUE;
      else
        {
        cprintf("Invalid switch - %s\r\n", cmd_switch);
        reset_batfile_call_stack();
        return;
        }
      }
    advance_cmd_arg();
    }

  cputs(text);
  if (!supress_prompt)
    {
    int first = TRUE;
    char *c;

    putch('[');
    c = choices;
    while (*c != '\0')
      {
      if (first)
        first = FALSE;
      else
        putch(',');
      putch(toupper(*c));
      c++;
      }
    cputs("]?");
    }
  choice = get_choice(choices);
  error_level = strchr(choices, choice) - choices + 1;
  return;
  }

static void perform_copy(void)
  {
  general_file_transfer(FILE_XFER_COPY);
  }

static void perform_xcopy(void)
  {
  general_file_transfer(FILE_XFER_XCOPY);
  }

static void perform_date(void)
  {
  struct dosdate_t date;
  const char *day_of_week[7] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  _dos_getdate(&date);
  printf("Current date is %s %02d-%02d-%04d\n", day_of_week[(int)date.dayofweek],
                                               (int)date.month,
                                               (int)date.day,
                                               (int)date.year);
  }

static void perform_delete(void)
  {
  struct ffblk ff;
  char filespec[MAXPATH] = "";
  char full_filespec[MAXPATH] = "";
  char drive[MAXDRIVE], dir[MAXDIR];

  while (*cmd_arg != '\0')
    {
    if (*cmd_switch == '\0') // if not a command switch ...
      {
      if (*filespec == '\0')
        {
        strncpy(filespec, cmd_arg, MAXPATH);
        filespec[MAXPATH-1] = '\0';
        }
      else
        {
        cprintf("Too many parameters - %s\r\n", cmd_args);
        reset_batfile_call_stack();
        return;
        }
      }
    advance_cmd_arg();
    }

  if (findfirst(filespec,&ff,0) != 0)
    {
    printf("File(s) not found - %s\n", filespec);  // informational msg; not an error
    return;
    }

  _fixpath(filespec, full_filespec);
  fnsplit(full_filespec, drive, dir, NULL, NULL);
  conv_unix_path_to_ms_dos(drive);
  conv_unix_path_to_ms_dos(dir);

  while (findfirst(full_filespec,&ff,0)==0)
    {
    char individual_filespec[MAXPATH];

    conv_unix_path_to_ms_dos(ff.ff_name);
    strcpy(individual_filespec, drive);
    strcat(individual_filespec, dir);
    strcat(individual_filespec, ff.ff_name);

    if (remove(individual_filespec) == 0)
      printf("%s erased\n", individual_filespec);
    else
      {
      cprintf("Access denied - %s\r\n", individual_filespec);
      reset_batfile_call_stack();
      return;
      }
    }
  }

static void perform_deltree(void)
  {
  int file_count = 0, dir_count = 0;
  int ffrc;
  int s, subdir_level = 0;
  int confirm_before_delete = TRUE;
  struct ffblk ff[MAX_SUBDIR_LEVEL];
  char dir_name[MAX_SUBDIR_LEVEL][MAXPATH];
  int remove_level1_dir;
  int visitation_mode[MAX_SUBDIR_LEVEL]; // 4 = findfirst source_filespec for files;
                                         // 3 = findnext source_filespec for files;
                                         // 2 = findfirst *.* for subdirs;
                                         // 1 = findnext *.* for subdirs;
                                         // 0 = done
  char drivespec[MAXDRIVE], dirspec[MAXDIR], filename[MAXFILE], extspec[MAXEXT];
  char temp_path[MAXPATH];
  char path[MAXPATH] = "", filespec[MAXPATH];
  char full_path_filespec[MAXPATH];
  int choice;
  unsigned search_attrib;

  while (*cmd_arg != '\0')
    {
    if (*cmd_switch == '\0') // if not a command switch ...
      {
      if (*path == '\0')
        {
        strncpy(path, cmd_arg, MAXPATH);
        path[MAXPATH-1] = '\0';
        conv_unix_path_to_ms_dos(path);
        }
      else
        {
        cprintf("Too many parameters - %s\r\n", cmd_args);
        reset_batfile_call_stack();
        return;
        }
      }
    else
      {
      if (stricmp(cmd_switch,"/Y") == 0)
        confirm_before_delete = FALSE;
      else
        {
        cprintf("Invalid switch - %s\r\n", cmd_switch);
        reset_batfile_call_stack();
        return;
        }
      }
    advance_cmd_arg();
    }

  // prepare path for fnsplit() -
  // attach a file specification if specified path doesn't have one
  if (is_drive_spec(path) || has_trailing_slash(path))
    strcat(path, "*.*");

  // parse path - create full path and split into 2 components: path + file spec
  _fixpath(path, temp_path);
  fnsplit(temp_path, drivespec, dirspec, filename, extspec);
  strcpy(path, drivespec);
  strcat(path, dirspec);
  conv_unix_path_to_ms_dos(path);
  strcpy(filespec, filename);
  strcat(filespec, extspec);
  conv_unix_path_to_ms_dos(filespec);

  // visit each directory; delete files and subdirs
  visitation_mode[0] = 4;
  dir_name[0][0] = '\0';
  remove_level1_dir = FALSE;
  while (subdir_level >= 0)
    {
    if (visitation_mode[subdir_level] == 4 || visitation_mode[subdir_level] == 2)
      {
      strcpy(full_path_filespec, path);
      for (s = 0; s <= subdir_level; s++)
        strcat(full_path_filespec, dir_name[s]);
      if (subdir_level == 0)
        strcat(full_path_filespec, filespec);
      else
        strcat(full_path_filespec, "*.*");
      if (visitation_mode[subdir_level] == 4)
        search_attrib = FA_RDONLY+FA_ARCH+FA_SYSTEM+FA_HIDDEN;
      else
        search_attrib = FA_DIREC+FA_RDONLY+FA_ARCH+FA_SYSTEM+FA_HIDDEN;
      ffrc = findfirst(full_path_filespec, &(ff[subdir_level]), search_attrib);
      visitation_mode[subdir_level]--;
      }
    else
      ffrc = findnext(&(ff[subdir_level]));
    if (ffrc == 0)
      {
      conv_unix_path_to_ms_dos(ff[subdir_level].ff_name);
      strcpy(full_path_filespec, path);
      for (s = 0; s <= subdir_level; s++)
        strcat(full_path_filespec, dir_name[s]);
      strcat(full_path_filespec, ff[subdir_level].ff_name);
  
      if ((ff[subdir_level].ff_attrib&FA_DIREC) != 0)
        {
        if (visitation_mode[subdir_level] <= 2 &&
            strcmp(ff[subdir_level].ff_name,".") != 0 &&
            strcmp(ff[subdir_level].ff_name,"..") != 0)
          {
          if (subdir_level == 0)
            {
            if (confirm_before_delete)
              {
              cprintf("Delete directory %s\\ and all its subdirectories? [Y/N] ", full_path_filespec);
              remove_level1_dir = (get_choice("YN") == 'Y');
              }
            else
              remove_level1_dir = TRUE;
            }
          if (subdir_level > 0 || remove_level1_dir)
            {
            subdir_level++;
            if (subdir_level >= MAX_SUBDIR_LEVEL)
              {
              cputs("Directory tree is too deep\r\n");
              reset_batfile_call_stack();
              return;
              }
            visitation_mode[subdir_level] = 4;
            strcpy(dir_name[subdir_level], ff[subdir_level-1].ff_name);
            strcat(dir_name[subdir_level], "\\");
            }
          }
        }
      else
        {
        if (visitation_mode[subdir_level] > 2)
          {
          if (confirm_before_delete && subdir_level == 0)
            {
            cprintf("Delete file %s ? [Y/N] ", full_path_filespec);
            choice = get_choice("YN");
            }
          else
            choice = 'Y';
          if (choice == 'Y')
            {
            if (remove(full_path_filespec) != 0)
              {
              cprintf("Unable to delete file - %s\r\n", full_path_filespec);
              reset_batfile_call_stack();
              return;
              }
            if (subdir_level == 0)
              printf("%s deleted\n", full_path_filespec);
            file_count++;
            }
          }
        }
      }
    else
      {
      visitation_mode[subdir_level]--;
      if (visitation_mode[subdir_level] <= 0)
        {
        if (subdir_level > 0)
          {
          strcpy(full_path_filespec, path);
          for (s = 0; s <= subdir_level; s++)
            strcat(full_path_filespec, dir_name[s]);
          *(strrchr(full_path_filespec,'\\')) = '\0';
          if (subdir_level > 1 || remove_level1_dir)
            {
            if (rmdir(full_path_filespec) != 0)
              {
              cprintf("Unable to remove directory - %s\\\r\n", full_path_filespec);
              reset_batfile_call_stack();
              return;
              }
            if (subdir_level == 1)
              printf("%s removed\n", full_path_filespec);
            dir_count++;
            }
          }
        subdir_level--;
        }
      }
    }
  printf("%9d file(s) deleted, ", file_count);
  if (dir_count == 1)
    printf("%9d directory removed\n", dir_count);
  else
    printf("%9d (sub)directories removed\n", dir_count);
  }

static void perform_dir(void)
  {
  int wide_column_countdown = -1;
  long long avail; //was double avail; --Salvo
  struct ffblk ff;
  unsigned attrib = FA_DIREC+FA_RDONLY+FA_ARCH+FA_SYSTEM+FA_HIDDEN, first, hour;
  struct dfree df;
  unsigned long filecount = 0, dircount = 0, bytecount = 0;
  char dirspec[MAXPATH], *ft, ampm;
  char volspec[7] = "X:\\*.*";
  char full_filespec[MAXPATH];
  char filespec[MAXPATH] = "";

  while (*cmd_arg != '\0')
    {
    if (*cmd_switch == '\0') // if not a command switch ...
      {
      if (*filespec == '\0')
        {
        strncpy(filespec, cmd_arg, MAXPATH);
        filespec[MAXPATH-1] = '\0';
        conv_unix_path_to_ms_dos(filespec);
        }
      else
        {
        cprintf("Too many parameters - %s\r\n", cmd_args);
        reset_batfile_call_stack();
        return;
        }
      }
    else
      {
      if (stricmp(cmd_switch,"/w")==0)
        wide_column_countdown = 5;
      }
    advance_cmd_arg();
    }

  if (*filespec == '\0' || is_drive_spec(filespec) || has_trailing_slash(filespec))
    strcat(filespec, "*.*");
  _fixpath(filespec, full_filespec);
  conv_unix_path_to_ms_dos(full_filespec);

  volspec[0] = full_filespec[0];
  if (findfirst(volspec, &ff, FA_LABEL) == 0)
    printf(" Volume in drive %c is %s\n", volspec[0], ff.ff_name);
  else
    puts(" Volume has no label");

  fnsplit (full_filespec, NULL, dirspec, NULL, NULL);
  printf(" Directory of %c:%s\n\n", full_filespec[0], dirspec);

  first = TRUE;
  for (;;)
    {
    if (first)
      {
      if (findfirst(full_filespec, &ff, attrib) != 0)
        {
        puts("File not found");  // informational message -- not an error
        return;
        }
      first = FALSE;
      }
    else
      {
      if (findnext(&ff) != 0)
        break;
      }
    conv_unix_path_to_ms_dos(ff.ff_name);
    if (wide_column_countdown < 0)
      {
      ft = strchr(ff.ff_name, '.');
      if (ft == NULL || stricmp(ff.ff_name,".") == 0 || stricmp(ff.ff_name,"..") == 0)
        ft = "";
      else
        {
        *ft = '\0';
        ft++;
        }
      printf("%-8s %-3s ", ff.ff_name, ft);
      if ((ff.ff_attrib&FA_DIREC) == 0)
        printf("%13lu  ", ff.ff_fsize);
      else
        printf("  <DIR>        ");
//--Salvo was:
//      printf("%02d-%02d-%04d ", (int)(ff.ff_fdate>>5)&0xF,   // month
//                                (int)(ff.ff_fdate)&0x1F,    // day
//                                (int)((ff.ff_fdate>>9)&0x7F)+1980); // year
      printf("%04d-%02d-%02d ", (int)((ff.ff_fdate>>9)&0x7F)+1980, // year
                                (int)(ff.ff_fdate>>5)&0xF,   // month
                                (int)(ff.ff_fdate)&0x1F);    // day

      hour = (int)(ff.ff_ftime>>11)&0x1F;
//--Salvo was:
//      if (hour < 12)
//        ampm = 'a';
//      else
//        ampm = 'p';
//      if (hour > 12)
//        hour -= 12;
//      if (hour == 0)
//        hour = 12;
//      printf("%2d:%02d%c\n",hour,(int)(ff.ff_ftime>>5)&0x3F,ampm);
      printf("%02d:%02d\n",hour,(int)(ff.ff_ftime>>5)&0x3F);
      }
    else
      {
      if ((ff.ff_attrib&FA_DIREC) == 0)
        printf("%-14s", ff.ff_name);
      else
        {
        int len = strlen(ff.ff_name) + 2;
        printf("[%s]", ff.ff_name);
        while (len < 14)
          {
          printf(" ");
          len++;
          }
        }
      wide_column_countdown--;
      if (wide_column_countdown == 0)
        {
        puts("");
        wide_column_countdown = 5;
        }
      else
        printf("  ");
      }

    if ((ff.ff_attrib&FA_DIREC) == 0)
      {
      filecount++;
      bytecount += ff.ff_fsize;
      }
    else
      dircount++;
    }
  if (wide_column_countdown >= 0 && wide_column_countdown < 5)
    puts("");
  printf("%10lu file(s) %14lu bytes\n", filecount, bytecount);
  printf("%10lu dir(s) ", dircount);

  getdfree(full_filespec[0]-'A'+1, &df);
  //--Salvo: was avail = (double)((unsigned)df.df_avail) * (double)((unsigned)df.df_bsec) * (double)((unsigned)df.df_sclus);
  avail = (long long) df.df_avail * (long long) df.df_bsec * (long long) df.df_sclus;

// Original code below: --Salvo
//    puts("      2 GB or more free");
//  else if (avail < 1000000.0)
//    printf("%15.0lf byte(s) free\n", avail);
//  else if (avail < 1000000000.0)
//    printf("%15.0lf KB free\n", avail / 1000.0);
//  else
//    printf("%15.2lf MB free\n", avail / 1000000.0);
  if (avail == 2147155968)
    puts("      2 GiB or more free");
  else if (avail < 1048576)
    printf("%15lli byte(s) free\n", avail);
  else if (avail < 1073741824)
    printf("%15lli KiB free\n", avail / 1024);
  else
    printf("%15lli MiB free\n", avail / 1048576);
  }

static void perform_echo(void)
  {
  if (stricmp(cmd_arg, "off") == 0)
    echo_on[stack_level] = FALSE;
  else if (stricmp(cmd_arg, "on") == 0)
    echo_on[stack_level] = TRUE;
  else if (cmd_arg[0] == '\0')
    {
    if (echo_on[stack_level])
      puts("ECHO is on");
    else
      puts("ECHO is off");
    }
  else
    puts(cmd_args);
  }

static void perform_exit(void)
  {
  int ba;
  bat_file_path[stack_level][0] = '\0';
  for (ba = 0; ba < MAX_BAT_ARGS; ba++)
    bat_arg[stack_level][ba][0] = '\0';
  bat_file_line_number[stack_level] = 0;
  echo_on[stack_level] = TRUE;
  if (stack_level > 0)
    stack_level--;
  else
    {
    if (shell_mode != SHELL_PERMANENT)
      exit(0);
    }
  }

void perform_external_cmd(int call)
  {
  struct ffblk ff;
  char cmd_name[MAX_CMD_BUFLEN];
  char *pathvar, pathlist[200];
  char full_cmd[MAXPATH+MAX_CMD_BUFLEN] = "";
  char temp_cmd[MAXPATH+MAX_CMD_BUFLEN];

  int exec_type, e, ba;
  static char *exec_ext[3] = {".COM",".EXE",".BAT"};
  char *s;

  // No wildcards allowed -- reject them
  if (has_wildcard(cmd))
    goto BadCommand;

  // Assemble a path list, and also extract the command name without the path.
  // For commands that already specify a path, the path list will consist of
  // only that path.
  s = strrchr(cmd, '\\');
  if (s != NULL)
    {
    s++;
    strncpy(pathlist, cmd, s-cmd);
    pathlist[s-cmd] = '\0';
    }
  else
    {
    s = strchr(cmd, ':');
    if (s != NULL)
      {
      s++;
      strncpy(pathlist, cmd, s-cmd);
      pathlist[s-cmd] = '\0';
      }
    else
      {
      strcpy(pathlist, ".\\;");
      s = cmd;
      pathvar = getenv("PATH");
      if(pathvar != NULL)
        {
        strncat(pathlist, pathvar, 200 - strlen(pathlist) - 1);
        pathlist[sizeof(pathlist)-1] = '\0';
        }
      }
    }
  strcpy(cmd_name, s);

  // Search for the command
  exec_type = -1;
  pathvar = strtok(pathlist, "; ");
  while (pathvar != NULL)
    {
    // start to build full command name (sort of, because it still could be missing .exe, .com, or .bat)
    strcpy(full_cmd, pathvar);
    s = strchr(full_cmd, '\0');
    if (strlen(full_cmd) > 0)
      {
      if(*(s-1)!=':'&&*(s-1)!='\\')
        {
        *s = '\\';
        s++;
        *s = '\0';
        }
      }
    if (stricmp(full_cmd,".\\") == 0)
      full_cmd[0] = '\0';
    strcat(full_cmd, cmd_name);
    _fixpath(full_cmd, temp_cmd);
    strcpy(full_cmd, temp_cmd);
    conv_unix_path_to_ms_dos(full_cmd);

    // check validity for each executable type
    for (e = 0; e < 3; e++)
      {
      s = strchr(cmd_name, '.');
      if (s == NULL)  // no file type mentioned
        {
        s = strchr(full_cmd, '\0');  // save position of the nul terminator
        strcat(full_cmd, exec_ext[e]);
        if (findfirst(full_cmd,&ff,0)==0)
          {
          exec_type = e;
          break;
          }
        *s = '\0'; // restore nul terminator
        }
      else
        {
        if (stricmp(s, exec_ext[e]) == 0)
          {
          if (findfirst(full_cmd,&ff,0)==0)
            {
            exec_type = e;
            break;
            }
          }
        }
      }

    if (exec_type < 0)  // if exec file not found yet...
      pathvar = strtok(NULL, "; ");  // try next path in path list
    else
      pathvar = NULL;   // command found...
    }

  if (exec_type < 0)
    goto BadCommand;

  strupr(full_cmd);

  if (exec_type == 2)  // if command is a batch file
    {
    if (call)
      {
      stack_level++;
      if (stack_level >= MAX_STACK_LEVEL)
        goto StackOverflow;
      }
    else
      bat_file_line_number[stack_level] = 0;
    strcpy(bat_file_path[stack_level], full_cmd);
    ba = 0;
    while (ba < MAX_BAT_ARGS && *cmd_arg != '\0')
      {
      strcpy(bat_arg[stack_level][ba], cmd_arg);
      advance_cmd_arg();
      ba++;
      }
    }
  else
    {
    if (*cmd_args != ' ' && *cmd_args != '\t')
      strcat(full_cmd, " ");
    strcat(full_cmd, cmd_args);
    _control87(0x033f, 0xffff);
    __djgpp_exception_toggle();
    error_level = system(full_cmd) & 0xff;
    __djgpp_exception_toggle();
    _control87(0x033f, 0xffff);
    _clear87();
    _fpreset();
    }
  return;

BadCommand:
  cprintf("Bad command or file name - %s\r\n", cmd);
  //reset_batfile_call_stack();  -- even if error becuase the command is not found, don't clean up
  return;

StackOverflow:
  cputs("Call stack overflow\r\n");
  reset_batfile_call_stack();
  return;
  }

static void perform_goto(void)
  {
  if (bat_file_path[stack_level][0] != '\0')
    {
    strcpy(goto_label, cmd_arg);
    bat_file_line_number[stack_level] = MAXINT;
    }
  else
    cputs("Goto not valid in immediate mode.\r\n");
  }

static void perform_if(void)
  {
  int not_flag = FALSE;
  int condition_fulfilled = FALSE;

  if (cmd_arg[0] == '\0')
    goto SyntaxError;

  if (stricmp(cmd_arg, "not") == 0)
    {
    not_flag = TRUE;
    advance_cmd_arg();
    }

  if (stricmp(cmd_arg, "exist") == 0)  // conditional is "exist <filename>"
    {
    char *s;
    struct ffblk ff;

    advance_cmd_arg();
    s = strrchr(cmd_arg, '\\');
    if (s != NULL)
      {
      if (stricmp(s,"\\nul") == 0)
        *s = '\0';
      }
    if (access(cmd_arg, F_OK) == 0 || access(cmd_arg, D_OK) == 0 ||
        findfirst(cmd_arg, &ff, 0) == 0)
      condition_fulfilled = TRUE;
    }
  else if (strnicmp(cmd_args, "errorlevel", 10) == 0) //conditional is "errolevel x"
    {
    char *s;
    unsigned ecomp;

    s = cmd_args+10;
    if (*s != ' ' && *s != '\t' && *s != '=')
      goto SyntaxError;
    while (*s == ' ' || *s == '\t' || *s == '=')
      {
      *s = 'x';
      s++;
      }
    if (sscanf(s, "%u", &ecomp) == 1)
      {
      if (ecomp == error_level)
        {
        while (*s != ' ' && *s != '\t')
          {
          *s = 'x';
          s++;
          }
        condition_fulfilled = TRUE;
        }
      }
    }
  else // assume the conditional is in the form " xxxxxxxx  ==  yyyyy "
    {  //                                      op1^ op1end^ ^eq ^op2 ^op2end
    int len;
    char *op1, *op1end, *eq, *op2, *op2end;

    op1 = cmd_args;
    while (*op1 == ' ' || *op1 == '\t')
      op1++;

    op1end = op1;
    while (*op1end != ' ' && *op1end != '\t' && *op1end != '\0' && *op1end != '=')
      op1end++;
    if (*op1end == '\0')
      goto SyntaxError;
    len = op1end - op1;
    if (len == 0)
      goto SyntaxError;

    eq = op1end;
    while (*eq != '\0' && *eq != '=')
      eq++;
    if (*eq != '=')
      goto SyntaxError;
    eq++;
    if (*eq != '=')
      goto SyntaxError;
    while (*eq == '=')
      eq++;

    op2 = eq;
    while (*op2 == ' ' || *op2 == '\t')
      op2++;

    op2end = op2;
    while (*op2end != ' ' && *op2end != '\t' && *op2end != '\0')
      op2end++;
    if (op2 == op2end)
      goto SyntaxError;

    if (len == (op2end - op2))
      {
      if (strnicmp(op1, op2, len) == 0)
        condition_fulfilled = TRUE;
      }

    while (op1 != op2end)
      {
      *op1 = 'x';
      op1++;
      }

    }

  advance_cmd_arg();

  if ((not_flag && !condition_fulfilled) ||
      (!not_flag && condition_fulfilled))
    {
    strcpy(cmd, cmd_arg);
    advance_cmd_arg();
    }
  else
    cmd[0] = '\0';

  return;

SyntaxError:
  cputs("Syntax error\r\n");
  reset_batfile_call_stack();
  cmd_line[0] = '\0';
  parse_cmd_line();  // this clears cmd[], cmd_arg[], cmd_switch[], and cmd_args[]
  return;
  }

static void perform_md(void)
  {
  while (*cmd_switch)  // skip switches
    advance_cmd_arg();
  if (*cmd_arg)
    {
    if (mkdir(cmd_arg, S_IWUSR) != 0)
      {
      cprintf("Could not create directory - %s\r\n", cmd_arg);
      reset_batfile_call_stack();
      }
    }
  else
    {
    cputs("Required parameter missing");
    reset_batfile_call_stack();
    }
  }

static void perform_more(void)
  {
  perform_unimplemented_cmd();
  }

static void perform_move(void)
  {
  general_file_transfer(FILE_XFER_MOVE);
  }

static void perform_null_cmd(void)
  {
  }

static void perform_path(void)
  {
  if (*cmd_args == '\0')
    {
    char *pathvar = getenv("PATH");
    printf("PATH=");
    if (pathvar == NULL)
      puts("");
    else
      puts(pathvar);
    }
  else
    {
    memmove(cmd_args+5, cmd_args, strlen(cmd_args)+1);
    strncpy(cmd_args, "PATH=", 5);
    perform_set();
    }
  }

static void perform_pause(void)
  {
  cputs("Press any key to continue . . .\r\n");
  getkey();
  }

static void perform_prompt(void)
  {
  memmove(cmd_args+7, cmd_args, strlen(cmd_args)+1);
  strncpy(cmd_args, "PROMPT=", 7);
  perform_set();
  }

static void perform_rd(void)
  {
  while (*cmd_switch)  // skip switches
    advance_cmd_arg();
  if (*cmd_arg)
    {
    if (rmdir(cmd_arg) != 0)
      {
      cprintf("Could not remove directory - %s\r\n", cmd_arg);
      reset_batfile_call_stack();
      }
    }
  else
    {
    cputs("Required parameter missing");
    reset_batfile_call_stack();
    }
  }

static void perform_rename(void)
  {
  int first, zfill;
  struct ffblk ff;
  unsigned attrib = FA_RDONLY+FA_ARCH+FA_SYSTEM;
  char from_path[MAXPATH] = "", to_path[MAXPATH] = "";
  char from_drive[MAXDRIVE], to_drive[MAXDRIVE];
  char from_dir[MAXDIR], to_dir[MAXDIR];
  char from_name[MAXFILE], to_name[MAXFILE], new_to_name[MAXFILE];
  char from_ext[MAXEXT], to_ext[MAXEXT], new_to_ext[MAXEXT];
  char full_from_filespec[MAXPATH], full_to_filespec[MAXPATH];
  char *w;

  while (*cmd_arg != '\0')
    {
    if (*cmd_switch == '\0') // if not a command switch ...
      {
      if (*from_path == '\0')
        {
        strncpy(from_path, cmd_arg, MAXPATH);
        from_path[MAXPATH-1] = '\0';
        conv_unix_path_to_ms_dos(from_path);
        }
      else if (*to_path == '\0')
        {
        strncpy(to_path, cmd_arg, MAXPATH);
        to_path[MAXPATH-1] = '\0';
        conv_unix_path_to_ms_dos(to_path);
        }
      else
        {
        cprintf("Too many parameters - %s\r\n", cmd_args);
        reset_batfile_call_stack();
        return;
        }
      }
    else
      {
      cprintf("Invalid switch - %s\r\n", cmd_args);
      reset_batfile_call_stack();
      return;
      }
    advance_cmd_arg();
    }

  _fixpath(from_path, full_from_filespec);
  conv_unix_path_to_ms_dos(full_from_filespec);
  fnsplit(full_from_filespec, from_drive, from_dir, from_name, from_ext);
  if (has_wildcard(from_dir) || has_trailing_slash(from_path))
    {
    cprintf("Invalid parameter - %s\r\n", from_path);
    reset_batfile_call_stack();
    return;
    }

  _fixpath(to_path, full_to_filespec);
  conv_unix_path_to_ms_dos(full_to_filespec);
  fnsplit(full_to_filespec, to_drive, to_dir, to_name, to_ext);
  if (stricmp(from_drive, to_drive) != 0 ||
      stricmp(from_dir, to_dir) != 0 ||
      has_trailing_slash(to_path))
    {
    cprintf("Invalid parameter - %s\r\n", to_path);
    reset_batfile_call_stack();
    return;
    }

  first = TRUE;
  for (;;)
    {
    if (first)
      {
      if (findfirst(full_from_filespec, &ff, attrib) != 0)
        {
        cprintf("File not found - %s", from_path);
        reset_batfile_call_stack();
        return;
        }
      first = FALSE;
      }
    else
      {
      if (findnext(&ff) != 0)
        break;
      }

    strcpy(full_from_filespec, from_drive);
    strcat(full_from_filespec, from_dir);
    strcat(full_from_filespec, ff.ff_name);
    conv_unix_path_to_ms_dos(full_from_filespec);
    fnsplit(full_from_filespec, NULL, NULL, from_name, from_ext);
    for (zfill = strlen(from_name); zfill < MAXFILE; zfill++)
      from_name[zfill] = '\0';
    for (zfill = strlen(from_ext); zfill < MAXEXT; zfill++)
      from_ext[zfill] = '\0';

    strcpy(full_to_filespec, from_drive);
    strcat(full_to_filespec, from_dir);
    strcpy(new_to_name, to_name);
    strcpy(new_to_ext, to_ext);
    while ((w = strchr(new_to_name, '?')) != NULL)
      *w = from_name[w-new_to_name];
    if ((w = strchr(new_to_name, '*')) != NULL)
      strcpy(w, &(from_name[w-new_to_name]));
    while ((w = strchr(new_to_ext, '?')) != NULL)
      *w = from_ext[w-new_to_ext];
    if ((w = strchr(new_to_ext, '*')) != NULL)
      strcpy(w, &(from_ext[w-new_to_ext]));
    fnmerge(full_to_filespec, to_drive, to_dir, new_to_name, new_to_ext);
    conv_unix_path_to_ms_dos(full_to_filespec);
    if (stricmp(full_from_filespec, full_to_filespec) != 0)
      {
      conv_unix_path_to_ms_dos(new_to_name);
      conv_unix_path_to_ms_dos(new_to_ext);
      if (_rename(full_from_filespec, full_to_filespec) == 0)
        printf("%s renamed to %s%s\n", full_from_filespec, new_to_name, new_to_ext);
      else
        {
        cprintf("Unable to rename %s to %s%s\n", full_from_filespec, new_to_name, new_to_ext);
        reset_batfile_call_stack();
        return;
        }
      }
    }
  }

void perform_set(void)
  {
  char *var_name, *var_value, *eq;
  if (*cmd_args == '\0')
    {
    perform_unimplemented_cmd();
    //int i;
    //while (environ[i])
    //  printf("%s\n",environ[i++]);
    }
  else
    {
    var_name = cmd_args;
    eq = strchr(cmd_args, '=');
    if (eq == NULL)
      {
      cputs("Syntax error\r\n");
      reset_batfile_call_stack();
      return;
      }
    var_value = eq+1;
    *eq = '\0';
    while (eq > var_name)
      {
      eq--;
      if (*eq != ' ' && *eq != '\t')
        break;
      *eq = '\0';
      }
    if (strlen(var_name) == 0)
      {
      cputs("Syntax error\r\n");
      reset_batfile_call_stack();
      return;
      }
    strupr(var_name);
    if (setenv(var_name, var_value, TRUE) != 0)
      {
      cprintf("Error setting environment variable - %s\r\n", var_name);
      reset_batfile_call_stack();
      return;
      }
    }
  }

static void perform_time(void)
  {
  struct dostime_t dtime;
  char ampm;
  int hour;

  _dos_gettime(&dtime);
  hour = dtime.hour;
  if (hour < 12)
    ampm = 'a';
  else
    ampm = 'p';
  if (hour > 12)
    hour -= 12;
  if (hour == 0)
    hour = 12;

  printf("Current time is %2d:%02d:%02d.%02d%c\n",
         hour, dtime.minute, dtime.second, dtime.hsecond, ampm);

  }

static void perform_type(void)
  {
  FILE *textfile;
  char filespec[MAXPATH] = "";
  int c;

  while (*cmd_arg != '\0')
    {
    if (*cmd_switch == '\0') // if not a command switch ...
      {
      if (*filespec == '\0')
        {
        strncpy(filespec, cmd_arg, MAXPATH);
        filespec[MAXPATH-1] = '\0';
        conv_unix_path_to_ms_dos(filespec);
        }
      else
        {
        cprintf("Too many parameters - %s\r\n", cmd_args);
        reset_batfile_call_stack();
        return;
        }
      }
    else
      {
      }
    advance_cmd_arg();
    }

  textfile = fopen(filespec,"rt");
  if (textfile != NULL)
    {
    while ((c=fgetc(textfile)) != EOF)
      putchar(c);
    fclose(textfile);
    }
  else
    {
    cprintf("Unable to open file - %s\r\n", filespec);
    reset_batfile_call_stack();
    }
  }

void perform_unimplemented_cmd(void)
  {
  cputs("Command not implemented\r\n");
  reset_batfile_call_stack();
  }

//////////////////////////////////////////////////////////////////////////////////

struct built_in_cmd
  {
  char *cmd_name;
  void (*cmd_fn)(void);
  };

struct built_in_cmd cmd_table[] =
  {
    {"attrib", perform_attrib},
    {"call", perform_call},
    {"cd", perform_cd},
    {"chdir", perform_cd},
    {"choice", perform_choice},
    {"cls", clrscr},
    {"copy", perform_copy},
    {"date", perform_date},
    {"del", perform_delete},
    {"deltree", perform_deltree},
    {"erase", perform_delete},
    {"dir", perform_dir},
    {"echo", perform_echo},
    {"exit", perform_exit},
    {"goto", perform_goto},
    {"md", perform_md},
    {"mkdir", perform_md},
    {"move", perform_move},
    {"more", perform_more},
    {"path", perform_path},
    {"pause", perform_pause},
    {"prompt", perform_prompt},
    {"rd", perform_rd},
    {"rmdir", perform_rd},
    {"rename", perform_rename},
    {"ren", perform_rename},
    {"set", perform_set},
    {"time", perform_time},
    {"type", perform_type},
    {"xcopy", perform_xcopy}
  };

void parse_cmd_line(void)
  {
  int c, cmd_len, *pipe_count_addr;
  char *extr, *dest, *saved_extr, *delim;
  char new_cmd_line[MAX_CMD_BUFLEN], *v, *end;

  // substitute in variable values before parsing
  extr = strchr(cmd_line, '%');
  if (extr != NULL)
    {
    dest = new_cmd_line;
    extr = cmd_line;
    v = NULL;
    while ((v != NULL || *extr != '\0') && dest < new_cmd_line+MAX_CMD_BUFLEN-1)
      {
      if (v == NULL)
        {
        if (*extr == '%')
          {
          extr++;
          if (*extr == '0')                   //  '%0'
            {
            extr++;
            v = bat_file_path[stack_level];
            continue;
            }
          if (*extr >= '1' && *extr <= '9')       //  '%1' to '%9'
            {
            v = bat_arg[stack_level][(*extr)-'1'];
            extr++;
            continue;
            }
          end = strchr(extr, '%');             // find ending '%'
          if (end == NULL)                  // if '%' found, but no ending '%' ...
            {
            *dest = '%';
            dest++;
            continue;
            }
          else                              // else ending '%' is found too
            {
            if (extr == end)                   //   if "%%", replace with single '%'
              {
              extr++;
              *dest = '%';
              dest++;
              continue;
              }
            else
              {
              *end = '\0';
              strupr(extr);
              v = getenv(extr);
              extr = end + 1;
              }
            }
          }
        else
          {
          *dest = *extr;
          dest++;
          extr++;
          }
        }
      else
        {
        if (*v != '\0')
          {
          *dest = *v;
          dest++;
          v++;
          }
        if (*v == '\0')
          v = NULL;
        }
      }
    *dest = '\0';
    strcpy(cmd_line, new_cmd_line);
    }

  // extract pipe specs....
  pipe_file[STDIN_INDEX][0] = '\0';   //  <
  pipe_file_redir_count[STDIN_INDEX] = 0;   // count of '<' characters

  pipe_file[STDOUT_INDEX][0] = '\0';  //  > or >>
  pipe_file_redir_count[STDOUT_INDEX] = 0;   // count of '>' characters

  pipe_to_cmd[0] = '\0';      // |
  pipe_to_cmd_redir_count = 0; // count of '|' characters

  extr = cmd_line;
  while (*extr != '\0')
    {
    c = *extr;
    switch (*extr)
      {
      case '<':
        dest =  pipe_file[STDIN_INDEX];
        pipe_count_addr = &(pipe_file_redir_count[STDIN_INDEX]);
        break;
      case '>':
        dest = pipe_file[STDOUT_INDEX];
        pipe_count_addr = &(pipe_file_redir_count[STDOUT_INDEX]);
        break;
      case '|':
        dest = pipe_to_cmd;
        pipe_count_addr = &pipe_to_cmd_redir_count;
        break;
      default:
        c = 0;
        break;
      }

    if (c == 0)
      extr++;
    else
      {
      // count redirection characters
      saved_extr = extr;
      while (*extr == c)
        {
        (*pipe_count_addr)++;
        extr++;
        }

      // skip over spaces
      while (*extr == ' ' || *extr == '\t')
        extr++;

      // extract pipe destinations
      if (c == '|')     // "pipe to" command
        {
        while (*extr != '\0')
          {
          *dest = *extr;
          dest++;
          extr++;
          }
        }
      else             // pipe in or out file
        {
        while (*extr != ' ' && *extr != '\t' && *extr != '\0')
          {
          *dest = *extr;
          dest++;
          extr++;
          }
        }
      *dest = '\0';

      // snip out pipe spec from the cmd_line[] string
      memmove(saved_extr, extr, strlen(extr)+1);
      extr = saved_extr;
      }
    }
  conv_unix_path_to_ms_dos(pipe_file[STDIN_INDEX]);
  conv_unix_path_to_ms_dos(pipe_file[STDOUT_INDEX]);

  // done with variables and pipes -- now, skip leading spaces
  extr = cmd_line;
  while (*extr == ' ' || *extr == '\t')
    extr++;
  if (*extr == '\0')
    {
    cmd[0] = '\0';
    cmd_arg[0] = '\0';
    cmd_switch[0] = '\0';
    cmd_args[0] = '\0';
    return;
    }

  // extract built-in command if command line contains one
  for (c = 0; c < CMD_TABLE_COUNT; c++)
    {
    cmd_len = strlen(cmd_table[c].cmd_name);
    if (strnicmp(extr, cmd_table[c].cmd_name, cmd_len) == 0)
      {
      delim = extr+cmd_len;
      if (!isalnum(*delim))
        {
        // ok, we have a built-in command; extract it
        strcpy(cmd, cmd_table[c].cmd_name);
        extr = delim;
        break;
        }
      }
    }

  // if not built-in command, extract as an external command
  if (c >= CMD_TABLE_COUNT)
    {
    dest = cmd;
    while (*extr != ' ' && *extr != '\t' && *extr != '/' && *extr != '\0')
      {
      *dest = *extr;
      dest++;
      extr++;
      }
    *dest = '\0';
    }

  // extract the rest as arguments
  extract_args(extr);
  return;
  }

static void exec_cmd(void)
  {
  int c;
  int pipe_index, pipe_fno[2], old_std_fno[2], redir_result[2];

  for (pipe_index = 0; pipe_index < 2; pipe_index++)
    {
    pipe_fno[pipe_index] = -1;
    old_std_fno[pipe_index] = -1;
    redir_result[pipe_index] = -1;
    }

  for (pipe_index = 0; pipe_index < 2; pipe_index++)
    {
    // open the pipe file
    if (pipe_file_redir_count[pipe_index] > 0)
      {
      if (pipe_index == STDIN_INDEX)
        pipe_fno[pipe_index] = open(pipe_file[pipe_index], O_TEXT|O_RDONLY, 0666);  // open for read
      else // else pipe_index is STDOUT_INDEX
        {
        if (pipe_file_redir_count[STDOUT_INDEX] > 1)
          pipe_fno[pipe_index] = open(pipe_file[pipe_index], O_TEXT|O_WRONLY|O_APPEND|O_CREAT, 0666); // open for append
        else
          pipe_fno[pipe_index] = open(pipe_file[pipe_index], O_TEXT|O_WRONLY|O_TRUNC|O_CREAT, 0666);  // open as new file
        }

      // save a reference to the old standard in/out file handle
      if (pipe_fno[pipe_index] >= 0)
        old_std_fno[pipe_index] = dup(pipe_index);

      // redirect pipe file to standard in/out
      if (pipe_fno[pipe_index] >= 0 && old_std_fno[pipe_index] != -1)
        redir_result[pipe_index] = dup2(pipe_fno[pipe_index], pipe_index);

      // check for error
      if (pipe_fno[pipe_index] < 0 ||
          old_std_fno[pipe_index] == -1 ||
          redir_result[pipe_index] == -1)
        {
        if (pipe_index == STDIN_INDEX)
          cprintf("Unable to pipe standard input from file - %s\r\n", pipe_file[pipe_index]);
        else
          cprintf("Unable to pipe standard output to file - %s\r\n", pipe_file[pipe_index]);
        reset_batfile_call_stack();
        goto Exit;
        }

      // close pipe file handle
      if (pipe_fno[pipe_index] >= 0)
        {
        close(pipe_fno[pipe_index]);
        pipe_fno[pipe_index] = -1;
        }

      }
    }

  if (pipe_to_cmd_redir_count != 0)
    {
    cputs("Piping between 2 commands is not supported\r\n");
    reset_batfile_call_stack();
    goto Exit;
    }

  while (cmd[0] != '\0')
    {
    need_to_crlf_at_next_prompt = TRUE;
    if (stricmp(cmd, "if") == 0)
      {
      perform_if();
      continue;
      }
    else if (strnicmp(cmd, "rem", 3) == 0)     // rem statement
      perform_null_cmd();
    else if (cmd[0] == ':')                   // goto label
      perform_null_cmd();
    else if (is_drive_spec(cmd) ||
             is_drive_spec_with_slash(cmd))  // drive letter
      perform_change_drive();
    else
      {
      for (c = 0; c < CMD_TABLE_COUNT; c++)
        {
        if (stricmp(cmd, cmd_table[c].cmd_name) == 0)
          {
          (*cmd_table[c].cmd_fn)();
          break;
          }
        }
      if (c >= CMD_TABLE_COUNT)
        perform_external_cmd(FALSE);
      }
    cmd[0] = '\0';
    }
Exit:
  cmd_line[0] = '\0';
  for (pipe_index = 0; pipe_index < 2; pipe_index++)
    {
    if (redir_result[pipe_index] != -1)
      dup2(old_std_fno[pipe_index], pipe_index);
    if (pipe_fno[pipe_index] >= 0)
      close(pipe_fno[pipe_index]);
    if (old_std_fno[pipe_index] != -1)
      close(old_std_fno[pipe_index]);
    if (redir_result[pipe_index] != -1)
      {
      if (pipe_index == STDIN_INDEX)
        setbuf(stdin, NULL);
      else
        setbuf(stdout, NULL);
      }
    }
  }


int main(int argc, char **argv)
  {
  int a;

  // reset fpu
  _clear87();
  _fpreset();

  // unbuffer stdin and stdout
  setbuf(stdin, NULL);
  setbuf(stdout, NULL);

  // init bat file stack
  reset_batfile_call_stack();


  // process arguments
  for (a = 1; a < argc; a++)
    {
    // check for permanent shell
    if (stricmp(argv[a], "/P") == 0)
      {
      int drive;
      shell_mode = SHELL_PERMANENT;
      strcpy(bat_file_path[0], "X:\\AUTOEXEC.BAT");  // trigger execution of autoexec.bat
      _dos_getdrive(&drive);
      drive += ('A' - 1);
      bat_file_path[0][0] = drive;
      // no arguments for batch file
      }

    // check for command in arguments
    if (stricmp(argv[a], "/C") == 0 || stricmp(argv[a], "/K") == 0)
      {
      int cmd_buf_remaining;

      if (stricmp(argv[a], "/C") == 0)
        shell_mode = SHELL_SINGLE_CMD;
      else
        shell_mode = SHELL_STARTUP_WITH_CMD;

      // build command from rest of the arguments
      a++;
      cmd_buf_remaining = MAX_CMD_BUFLEN-1;
      while (a < argc)
        {
        strncat(cmd_line, argv[a], cmd_buf_remaining);
        cmd_buf_remaining -= strlen(argv[a]);
        if (cmd_buf_remaining < 0)
          cmd_buf_remaining = 0;
        strncat(cmd_line, " ", cmd_buf_remaining);
        cmd_buf_remaining--;
        if (cmd_buf_remaining < 0)
          cmd_buf_remaining = 0;
        a++;
        }
      parse_cmd_line();
      }
    }

  // greet the user with a required message, due to a legality with DJGPP and CWSDPMI.
  if (shell_mode == SHELL_PERMANENT)
    {
    delay(1000); // delay so that the user can see the FreeDOS greeting
    clrscr();
    cputs("          ����������������������������������������������������������Ŀ\r\n"
          "          �    The program COMMAND.COM was created using the DJGPP   �\r\n"   //  This message is required only if
          "          � software development tools and relies on CWSDPMI to run. �\r\n"   //  you want to distribute the binary
          "          �       You have the right to obtain source code and       �\r\n"   //  COMMAND.COM without sources, and
          "          �        binary updates for both DJGPP and CWSDPMI.        �\r\n"   //  if you want to distribute
          "          �    These may be freely downloaded from www.delorie.com   �\r\n"   //  CWSDPMI.EXE without sources.
          "          ������������������������������������������������������������\r\n");
    delay(1500); // delay so that the user can see the command.com greeting on
                 // permanent shell mode before autoexec.bat takes over
    }

  // Main command parsing/interpretation/execution loop
  for (;;)
    {
    if (cmd_line[0] == '\0')
      {
      if (bat_file_path[stack_level][0] == '\0')
        {
        if (shell_mode == SHELL_SINGLE_CMD)
          perform_exit();
        prompt_for_and_get_cmd();
        }
      else
        get_cmd_from_bat_file();
      }
    exec_cmd();
    }
  }
