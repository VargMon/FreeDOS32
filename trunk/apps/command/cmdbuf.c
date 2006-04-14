/* Command-line buffer handling for FreeDOS-32's COMMAND
 *
 * Copyright (C) 2005 by Hanzac Chen
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the
 * Free Software Foundation, Inc.,
 * 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include <conio.h>
#include <string.h>
#include "cmdbuf.h"
#include "command.h"

#define KEYB_FLAG_INSERT    0x0080
#define KEY_ASCII(k)    (k & 0x00FF)
#define KEY_BACKSPACE    KEY_ASCII(0x0E08)

static unsigned int tail = 0;
static unsigned int cur = 0;

#define MAX_CMDQUEUE_LEN 0x10
static char cmdqueue[MAX_CMDQUEUE_LEN][MAX_CMD_BUFLEN];
static unsigned int cmdqueue_count = 0;
static unsigned int cmdqueue_index = 0;

unsigned int cmdbuf_get_tail(void)
{
	return tail;
}

static void _cmdbuf_clr_line(char *cmd_buf)
{
	unsigned int i, n;
	/* Clear the original command */
	for (i = 0, n = tail; i < n; i++) {
		putch(KEY_ASCII(KEY_BACKSPACE));
		cur--;
		cmdbuf_delch(cmd_buf);
	}
}

void cmdbuf_move(char *cmd_buf, int direction)
{
  if (direction == UP || direction == DOWN) {
    if (cmdqueue[cmdqueue_index][0]) {
      _cmdbuf_clr_line(cmd_buf);
      /* Reinput the command from the queue */
      cputs(cmdqueue[cmdqueue_index]);
      strcpy(cmd_buf, cmdqueue[cmdqueue_index]);
      cur = tail = strlen(cmdqueue[cmdqueue_index]);
    }
  }

  switch (direction)
  {
    case UP:
      cmdqueue_index --;
      cmdqueue_index = cmdqueue_index%MAX_CMDQUEUE_LEN;
      if (cmdqueue[cmdqueue_index][0] == '\0') {
        cmdqueue_index ++;
        cmdqueue_index = cmdqueue_index%MAX_CMDQUEUE_LEN;
      }
      break;
    case LEFT:
      if (cur != 0) {
        putch(KEY_ASCII(KEY_BACKSPACE));
        cur--;
      }
      break;
    case RIGHT:
      if (cur < tail) {
        putch(cmd_buf[cur]);
        cur++;
      }
      break;
    case DOWN:
      cmdqueue_index ++;
      cmdqueue_index = cmdqueue_index%MAX_CMDQUEUE_LEN;
      if (cmdqueue[cmdqueue_index][0] == '\0') {
        cmdqueue_index --;
        cmdqueue_index = cmdqueue_index%MAX_CMDQUEUE_LEN;
      }
      break;
    default:
      break;
  }
}

void cmdbuf_delch(char *cmd_buf)
{
  if (cur < tail) {
    unsigned int i;
    cmd_buf[cur] = 0;
    cmd_buf[tail] = 0;

    /* Move the left string to the current position */
    for (i = cur; i < tail; i++)
    {
      putch(cmd_buf[i+1]);
      cmd_buf[i] = cmd_buf[i+1];
    }

    /* Put cursor back to the current position */
    for (i = cur; i < tail; i++)
      putch(KEY_ASCII(KEY_BACKSPACE));

    /* Subtract the string 1 */
    tail--;
  }
}


void cmdbuf_putch(char *cmd_buf, unsigned int buf_size, char ch, unsigned short flag)
{
  unsigned int i;

  if (cur < buf_size) {
    /* Reflect the insert method */
    if (flag&KEYB_FLAG_INSERT) {
      for (i = tail; i > cur; i--)
        cmd_buf[i] = cmd_buf[i-1];
    }
    /* Put into cmdline buffer */
    cmd_buf[cur++] = ch;
    if ((flag&KEYB_FLAG_INSERT && tail < buf_size) || cur > tail)
      tail++;
    /* Update the string on screen */
    for (i = cur-1; i < tail; i++)
      putch(cmd_buf[i]);

    /* Put cursor back to the current position */
    for (i = cur; i < tail; i++)
      putch(KEY_ASCII(KEY_BACKSPACE));
  }
}


char *cmdbuf_gets(char *cmd_buf)
{
  cmd_buf[tail] = 0;
  /* Reset the cmdbuf */
  cur = tail = 0;

  /* Enqueue the cmdbuf and save the current index */
  strcpy(cmdqueue[cmdqueue_count], cmd_buf);
  cmdqueue_index = cmdqueue_count;
  cmdqueue_count++;
  cmdqueue_count = cmdqueue_count%MAX_CMDQUEUE_LEN;

  return cmd_buf;
}
