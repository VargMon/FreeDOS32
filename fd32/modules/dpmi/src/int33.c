/* DPMI Driver for FD32: int 0x33 services
 * by Hanzac Chen
 *
 * This is free software; see GPL.txt
 */
 
#include <ll/i386/hw-data.h>
#include <ll/i386/hw-instr.h>
#include <ll/i386/hw-func.h>
#include <ll/i386/mem.h>
#include <ll/i386/string.h>
#include <ll/i386/error.h>
#include <ll/i386/cons.h>
#include <devices.h>
#include <logger.h>
#include "rmint.h"


#define __INT33_DEBUG__


#ifdef __INT33_DEBUG__
#define LOG_PRINTF(s) fd32_log_printf("[MOUSE BIOS] "s)
#else
static int log;
#define LOG_PRINTF(s) do {log = 0;} while(0)
#endif


#define FD32_MOUSE_HIDE		0x00
#define FD32_MOUSE_SHOW		0x01
#define FD32_MOUSE_SHAPE	0x02
#define FD32_MOUSE_GETXY	0x03
#define FD32_MOUSE_GETBTN	0x04  /* The low 3 bits of retrieved data is the button status */


static fd32_request_t *request;
static int hdev;


int mousedev_get(void)
{
  int got = 1;

  if (hdev == 0)
    if ((hdev = fd32_dev_search("mouse")) < 0)
    {
      LOG_PRINTF("no mouse driver\n");
      got = 0;
    }

  if (request == 0)
    if (fd32_dev_get(hdev, &request, NULL, NULL, 0) < 0)
    {
      LOG_PRINTF("no mouse request calls\n");
      got = 0;
    }

  return got;
}


int mousebios_int(union rmregs *r)
{
  int res = 0;

  if(r->h.ah == 0x00)
  {
    switch(r->h.al)
    {
      case 0x00:
#ifdef __INT33_DEBUG__
        LOG_PRINTF("Reset mouse driver\n");
#endif
        r->x.ax = 0xffff;
        r->x.bx = 0x0002;
        break;
      case 0x01:
#ifdef __INT33_DEBUG__
        LOG_PRINTF("Show Mouse cursor\n");
#endif
        if(mousedev_get())
        {
          res = request(FD32_MOUSE_SHOW, 0);
          if(res < 0)
            LOG_PRINTF("no FD32_MOUSE_SHOW request call\n");
          else
            res = 0;
        }
        break;
      case 0x02:
#ifdef __INT33_DEBUG__
        LOG_PRINTF("Hide mouse cursor\n");
#endif
        if(mousedev_get())
        {
          res = request(FD32_MOUSE_HIDE, 0);
          if(res < 0)
            LOG_PRINTF("no FD32_MOUSE_HIDE request call\n");
          else
            res = 0;
        }
        break;
      /* MS MOUSE v1.0+ - RETURN POSITION AND BUTTON STATUS */
      case 0x03:
        if(mousedev_get())
        {
          DWORD raw, pos[2];
          res = request(FD32_MOUSE_GETBTN, (void *)&raw);
          if(res < 0) { LOG_PRINTF("no FD32_MOUSE_GETBTN request call\n"); break; }
          res = request(FD32_MOUSE_GETXY, (void *)pos);
          if(res < 0) { LOG_PRINTF("no FD32_MOUSE_GETXY request call\n"); break; }
          r->x.bx = raw&0x00000007;
          r->x.cx = pos[0];
          r->x.dx = pos[1];
          res = 0;
          #ifdef __INT33_DEBUG__
          if(r->x.bx != 0)
            fd32_log_printf("[MOUSE BIOS] Button clicked, cx: %x\tdx: %x\tbx: %x\n", r->x.cx, r->x.dx, r->x.bx);
          #endif
        }
        break;
      /* MS MOUSE v1.0+ - DEFINE HORIZONTAL CURSOR RANGE */
      case 0x07:
        /* r->x.cx = 0; minimum column */
        /* r->x.dx = 639; maximum column */
        res = 1; /* TODO: malfunction */
        break;
      /* MS MOUSE v1.0+ - DEFINE VERTICAL CURSOR RANGE */
      case 0x08:
        /* r->x.cx = 0; minimum row */
        /* r->x.dx = 479; maximum row */
        res = 1; /* TODO: malfunction */
        break;
      /* MS MOUSE v3.0+ - DEFINE TEXT CURSOR */
      case 0x0a:
        if(mousedev_get())
        {
          if (r->x.bx == 0)
          {
            WORD mask[2] = {r->x.cx, r->x.dx};
            res = request(FD32_MOUSE_SHAPE, (void *)mask);
          } else {
            res = request(FD32_MOUSE_SHAPE, 0);
          }
          if (res < 0)
            LOG_PRINTF("no FD32_MOUSE_SHAPE request call\n");
          else
            res = 0;
        }
        break;
      /* MS MOUSE v6.0+ - RETURN DRIVER STORAGE REQUIREMENTS */
      case 0x15:
#ifdef __INT33_DEBUG__
        LOG_PRINTF("RETURN DRIVER STORAGE REQUIREMENTS NOT USED, SIZE SET TO ZERO\n");
#endif
        r->x.bx = 0;
        break;
      case 0x21:
#ifdef __INT33_DEBUG__
        LOG_PRINTF("Software reset mouse driver\n");
#endif
        r->x.ax = 0xffff;
        r->x.bx = 0x0002;
        break;
      default:
        message("[MOUSE BIOS] Unimplemeted INT 33H AL=0x%x!!!\n", r->h.al);
        res = 1;
        break;
    }
  } else {
    res = 1;
  }
  
  return res;
}