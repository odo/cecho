// Copyright (c) 2010, Mazen Harake
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
// 
//  * Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimer.
//  * Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//  * Neither the name of the <ORGANIZATION> nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.x 

// Includes
#include <stdlib.h>
#include <string.h>
#include "cecho.h"
#include "cecho_commands.h"
#include "erl_driver.h"
#include "erl_interface.h"
#include "ei.h"
#include "ncurses.h"
#include "assert.h"

// State structure
typedef struct {
  WINDOW *win[_MAXWINDOWS+1];
  ei_x_buff eixb;
  char *args;
  int argslen;
  int index;
  int version;
  ErlDrvPort drv_port;
} state;

void init_state(state *st, char *args, int argslen);
void ok(state *st);
void error_tuple(state *st, int code);
void boolean(state *st, int code);

void tuple(ei_x_buff *eixb, int size);
void atom(ei_x_buff *eixb, const char *str, int size);
void integer(ei_x_buff *eixb, int integer);
void string(ei_x_buff *eixb, const char *str);
void encode_ok_reply(state *st, int code);
int findfreewindowslot(state *st);

void do_endwin(state *st);
void do_initscr(state *st);
void do_refresh(state *st);
void do_cbreak(state *st);
void do_nocbreak(state *st);
void do_echo(state *st);
void do_noecho(state *st);
void do_addch(state *st);
void do_addstr(state *st);
void do_move(state *st);
void do_getyx(state *st);
void do_getmaxyx(state *st);
void do_curs_set(state *st);
void do_erase(state *st);
void do_has_colors(state *st);
void do_start_color(state *st);
void do_init_pair(state *st);
void do_curs_set(state *st);
void do_init_pair(state *st);
void do_attron(state *st);
void do_attroff(state *st);
void do_nl(state *st);
void do_nonl(state *st);
void do_scrollok(state *st);
void do_mvaddch(state *st);
void do_mvaddstr(state *st);
void do_newwin(state *st);
void do_delwin(state *st);
void do_wmove(state *st);
void do_waddstr(state *st);
void do_waddch(state *st);
void do_mvwaddstr(state *st);
void do_mvwaddch(state *st);
void do_wrefresh(state *st);

// =============================================================================
// Erlang Callbacks
// =============================================================================
static ErlDrvData start(ErlDrvPort port, char *command) {
  state *drvstate = (state *)driver_alloc(sizeof(state));
  drvstate->drv_port = port;
  set_port_control_flags(port, PORT_CONTROL_FLAG_BINARY);
  int i;
  for (i = 0; i < _MAXWINDOWS; i++)
    drvstate->win[i] = NULL;
  return (ErlDrvData)drvstate;
}

static void stop(ErlDrvData drvstate) {
  driver_free(drvstate);
}

static int ctrl(ErlDrvData drvstate, unsigned int command, char *args, 
		int argslen, char **rbuf, int rbuflen) {
  state *st = (state *)drvstate;
  init_state(st, args, argslen);

  switch (command) {
  case ENDWIN: do_endwin(st); break;
  case INITSCR: do_initscr(st); break;
  case REFRESH: do_refresh(st); break;
  case CBREAK: do_cbreak(st); break;
  case NOCBREAK: do_nocbreak(st); break;
  case ECHO: do_echo(st); break;
  case NOECHO: do_noecho(st); break;
  case ADDCH: do_addch(st); break;
  case ADDSTR: do_addstr(st); break;
  case MOVE: do_move(st); break;
  case GETYX: do_getyx(st); break;
  case GETMAXYX: do_getmaxyx(st); break;
  case CURS_SET: do_curs_set(st); break;
  case ERASE: do_erase(st); break;
  case HAS_COLORS: do_has_colors(st); break;
  case START_COLOR: do_start_color(st); break;
  case INIT_PAIR: do_init_pair(st); break;
  case ATTRON: do_attron(st); break;
  case ATTROFF: do_attroff(st); break;
  case NL: do_nl(st); break;
  case NONL: do_nonl(st); break;
  case SCROLLOK: do_scrollok(st); break;
  case MVADDCH: do_mvaddch(st); break;
  case MVADDSTR: do_mvaddstr(st); break;
  case NEWWIN: do_newwin(st); break;
  case DELWIN: do_delwin(st); break;
  case WMOVE: do_wmove(st); break;
  case WADDSTR: do_waddstr(st); break;
  case WADDCH: do_waddch(st); break;
  case MVWADDSTR: do_mvwaddstr(st); break;
  case MVWADDCH: do_mvwaddch(st); break;
  case WREFRESH: do_wrefresh(st); break;
  default: break;
  }
  
  int rlen = st->eixb.index;
  ErlDrvBinary *response = driver_alloc_binary(rlen);
  memcpy(response->orig_bytes, st->eixb.buff, rlen);
  ei_x_free(&(st->eixb));
  *rbuf = (char *)response;
  return rlen;
}

// =============================================================================
// NCurses function wrappers
// ===========================================================================
void do_endwin(state *st) {
  encode_ok_reply(st, endwin());
}

void do_initscr(state *st) {
  st->win[0] = (WINDOW *)initscr();
  if (st->win[0] == NULL) {
    encode_ok_reply(st, -1);
  } else {
    encode_ok_reply(st, 0);
  }
}

void do_refresh(state *st) {
  encode_ok_reply(st, refresh());
}

void do_cbreak(state *st) {
  encode_ok_reply(st, cbreak());
}

void do_nocbreak(state *st) {
  encode_ok_reply(st, nocbreak());
}

void do_echo(state *st) {
  encode_ok_reply(st, echo());
}

void do_noecho(state *st) {
  encode_ok_reply(st, noecho());
}

void do_addch(state *st) {
  char ch = 0;
  ei_decode_char(st->args, &(st->index), &ch);
  encode_ok_reply(st, addch(ch));
}

void do_addstr(state *st) {
  int arity;
  long strlen;
  ei_decode_tuple_header(st->args, &(st->index), &arity);
  ei_decode_long(st->args, &(st->index), &strlen);
  char str[strlen];
  ei_decode_string(st->args, &(st->index), str);
  encode_ok_reply(st, addnstr(str, strlen));
}

void do_move(state *st) {
  int arity;
  long y, x;
  ei_decode_tuple_header(st->args, &(st->index), &arity);
  ei_decode_long(st->args, &(st->index), &y);
  ei_decode_long(st->args, &(st->index), &x);
  encode_ok_reply(st, move((int)y, (int)x));
}

void do_getyx(state *st) {
  int x, y;
  getyx(st->win[0], y, x);
  tuple(&(st->eixb), 2);
  integer(&(st->eixb), y);
  integer(&(st->eixb), x);
}

void do_getmaxyx(state *st) {
  int x, y;
  getmaxyx(st->win[0], y, x);
  tuple(&(st->eixb), 2);
  integer(&(st->eixb), y);
  integer(&(st->eixb), x);
}

void do_curs_set(state *st) {
  long flag;
  ei_decode_long(st->args, &(st->index), &flag);
  curs_set((int)flag);
  ok(st);
}

void do_erase(state *st) {
  encode_ok_reply(st, erase());
}

void do_has_colors(state *st) {
  boolean(st, has_colors());
}

void do_start_color(state *st) {
  encode_ok_reply(st, start_color());
}

void do_init_pair(state *st) {
  int arity;
  long pairnum, fcolor, bcolor;
  ei_decode_tuple_header(st->args, &(st->index), &arity);
  ei_decode_long(st->args, &(st->index), &pairnum);
  ei_decode_long(st->args, &(st->index), &fcolor);
  ei_decode_long(st->args, &(st->index), &bcolor);
  encode_ok_reply(st, init_pair((int)pairnum, (int)fcolor, (int)bcolor));
}

void do_attron(state *st) {
  long attrs;
  ei_decode_long(st->args, &(st->index), &attrs);
  encode_ok_reply(st, attron((int)attrs));
}

void do_attroff(state *st) {
  long attrs;
  ei_decode_long(st->args, &(st->index), &attrs);
  encode_ok_reply(st, attroff((int)attrs));
}

void do_nl(state *st) {
  encode_ok_reply(st, nl());
}

void do_nonl(state *st) {
  encode_ok_reply(st, nonl());
}

void do_scrollok(state *st) {
  int arity;
  int bf;
  long slot;
  ei_decode_tuple_header(st->args, &(st->index), &arity);
  ei_decode_long(st->args, &(st->index), &slot);
  ei_decode_boolean(st->args, &(st->index), &bf);
  encode_ok_reply(st, scrollok(st->win[slot], bf));
}

void do_mvaddch(state *st) {
  char ch = 0;
  int arity;
  long y, x;
  ei_decode_tuple_header(st->args, &(st->index), &arity);
  ei_decode_long(st->args, &(st->index), &y);
  ei_decode_long(st->args, &(st->index), &x);
  ei_decode_char(st->args, &(st->index), &ch);
  encode_ok_reply(st, mvaddch((int)y, (int)x, ch));
}

void do_mvaddstr(state *st) {
  int arity;
  long strlen, y, x;
  ei_decode_tuple_header(st->args, &(st->index), &arity);
  ei_decode_long(st->args, &(st->index), &y);
  ei_decode_long(st->args, &(st->index), &x);
  ei_decode_long(st->args, &(st->index), &strlen);
  char str[strlen];
  ei_decode_string(st->args, &(st->index), str);
  encode_ok_reply(st, mvaddnstr((int)y, (int)x, str, (int)strlen));
}
   
void do_newwin(state *st) {
  int slot = findfreewindowslot(st);
  if (slot > 0) {
    int arity;
    long height, width, starty, startx;
    ei_decode_tuple_header(st->args, &(st->index), &arity);
    ei_decode_long(st->args, &(st->index), &height);
    ei_decode_long(st->args, &(st->index), &width);
    ei_decode_long(st->args, &(st->index), &starty);
    ei_decode_long(st->args, &(st->index), &startx);
    st->win[slot] = newwin(height, width, starty, startx);
    integer(&(st->eixb), slot);
  } else {
    integer(&(st->eixb), -1);
  }
}

void do_delwin(state *st) {
  long slot;
  ei_decode_long(st->args, &(st->index), &slot);
  if (slot == 0) {
    boolean(st, FALSE);
  } else if (st->win[slot] == NULL) {
    boolean(st, FALSE);
  } else if (st->win[slot] != NULL) {
    delwin(st->win[slot]);
    st->win[slot] = NULL;
    boolean(st, TRUE);
  }
}

void do_wmove(state *st) {
  int arity;
  long slot, y, x;
  ei_decode_tuple_header(st->args, &(st->index), &arity);
  ei_decode_long(st->args, &(st->index), &slot);
  ei_decode_long(st->args, &(st->index), &y);
  ei_decode_long(st->args, &(st->index), &x);
  encode_ok_reply(st, wmove(st->win[slot], (int)y, (int)x));
}

void do_waddstr(state *st) {
  int arity;
  long slot, strlen;
  ei_decode_tuple_header(st->args, &(st->index), &arity);
  ei_decode_long(st->args, &(st->index), &slot);
  ei_decode_long(st->args, &(st->index), &strlen);
  char str[strlen];
  ei_decode_string(st->args, &(st->index), str);
  encode_ok_reply(st, waddnstr(st->win[slot], str, strlen));
}

void do_waddch(state *st) {
  int arity;
  long slot;
  char ch = 0;
  ei_decode_tuple_header(st->args, &(st->index), &arity);
  ei_decode_long(st->args, &(st->index), &slot);
  ei_decode_char(st->args, &(st->index), &ch);
  encode_ok_reply(st, waddch(st->win[slot], ch));
}

void do_mvwaddstr(state *st) {
  int arity;
  long slot, y, x, strlen;
  ei_decode_tuple_header(st->args, &(st->index), &arity);
  ei_decode_long(st->args, &(st->index), &slot); 
  ei_decode_long(st->args, &(st->index), &y);
  ei_decode_long(st->args, &(st->index), &x);
  ei_decode_long(st->args, &(st->index), &strlen);
  char str[strlen];
  ei_decode_string(st->args, &(st->index), str);
  encode_ok_reply(st, mvwaddnstr(st->win[slot], (int)y, (int)x, str, strlen));
}

void do_mvwaddch(state *st) {
  int arity;
  long slot, y, x;
  char ch = 0;
  ei_decode_tuple_header(st->args, &(st->index), &arity);
  ei_decode_long(st->args, &(st->index), &slot);
  ei_decode_long(st->args, &(st->index), &y);
  ei_decode_long(st->args, &(st->index), &x);
  ei_decode_char(st->args, &(st->index), &ch);
  encode_ok_reply(st, mvwaddch(st->win[slot], (int)y, (int)x, ch));
}

void do_wrefresh(state *st) {
  long slot;
  ei_decode_long(st->args, &(st->index), &slot);
  encode_ok_reply(st, wrefresh(st->win[slot]));
}

// =============================================================================
// Utility functions
// ===========================================================================
void init_state(state *st, char *args, int argslen) {
  st->index = 0;
  st->version = 0;
  st->args = args;
  st->argslen = argslen;
  ei_decode_version(st->args, &(st->index), &(st->version));
  assert(st->version != 0);
  assert(st->index != 0);
  ei_x_new_with_version(&(st->eixb));
}

void ok(state *st) {
  atom(&(st->eixb), "ok", 2);
}

void error_tuple(state *st, int code) {
  tuple(&(st->eixb), 2);
  atom(&(st->eixb), "error", 5);
  integer(&(st->eixb), code);
}

void boolean(state *st, int code) {
  if (code == TRUE)
    atom(&(st->eixb),"true",4);
  else
    atom(&(st->eixb),"false",5);
}

void tuple(ei_x_buff *eixb, int size) {
  ei_x_encode_tuple_header(eixb, size);
}

void atom(ei_x_buff *eixb, const char *str, int size) {
  ei_x_encode_atom_len(eixb, str, size);
}

void integer(ei_x_buff *eixb, int integer) {
  ei_x_encode_long(eixb, (long)integer);
}

void string(ei_x_buff *eixb, const char *str) {
  ei_x_encode_string(eixb, str);
}

void encode_ok_reply(state *st, int code) {
  if (code == OK) {
    ok(st);
  } else {
    error_tuple(st, code);
  }
}

int findfreewindowslot(state *st) {
  int i;
  for (i = 0; i < _MAXWINDOWS; i++)
    if (st->win[i] == NULL) return i;
  return -1;
}

// =============================================================================
// Erlang driver_entry Specification
// ===========================================================================
ErlDrvEntry driver_entry = {
  NULL,
  start,
  stop,
  NULL,
  NULL,
  NULL,
  "cecho",
  NULL,
  NULL,
  ctrl,
  NULL
};
  
// =============================================================================
// Erlang Driver Name
// =============================================================================
DRIVER_INIT(cecho) {
  return &driver_entry;
}
