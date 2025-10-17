#include "windows.h"
#include "pwindow_manager.h"
#include "types.h"

#include "config.h"
#include "util.h"
#include <X11/Xatom.h>

const char broken[] = "broken";

void showhide(Client *c) {
  if (!c)
    return;
  if (ISVISIBLE(c)) {
    /* show clients top down */
    XMoveWindow(display, c->win, c->x, c->y);
    if ((!c->mon->lt[c->mon->sellt]->arrange || c->isfloating) &&
        !c->isfullscreen)
      resize(c, c->x, c->y, c->w, c->h, 0);
    showhide(c->snext);
  } else {
    /* hide clients bottom up */
    showhide(c->snext);
    XMoveWindow(display, c->win, WIDTH(c) * -2, c->y);
  }
}

void focus(Client *c) {
  if (!c || !ISVISIBLE(c))
    for (c = selected_monitor->stack; c && !ISVISIBLE(c); c = c->snext)
      ;
  if (selected_monitor->selected_client && selected_monitor->selected_client != c)
    unfocus(selected_monitor->selected_client, 0);
  if (c) {
    if (c->mon != selected_monitor)
      selected_monitor = c->mon;
    if (c->isurgent)
      seturgent(c, 0);
    detachstack(c);
    attachstack(c);
    grabbuttons(c, 1);
    XSetWindowBorder(display, c->win, color_scheme[SchemeSelected][ColBorder].pixel);
    setfocus(c);
  } else {
    XSetInputFocus(display, root, RevertToPointerRoot, CurrentTime);
    XDeleteProperty(display, root, netatom[NetActiveWindow]);
  }
  selected_monitor->selected_client = c;
  drawbars();
}

void detach(Client *c) {
  Client **tc;

  for (tc = &c->mon->clients; *tc && *tc != c; tc = &(*tc)->next)
    ;
  *tc = c->next;
}

void detachstack(Client *c) {
  Client **tc, *t;

  for (tc = &c->mon->stack; *tc && *tc != c; tc = &(*tc)->snext)
    ;
  *tc = c->snext;

  if (c == c->mon->selected_client) {
    for (t = c->mon->stack; t && !ISVISIBLE(t); t = t->snext)
      ;
    c->mon->selected_client = t;
  }
}

void attach(Client *c) {
  c->next = c->mon->clients;
  c->mon->clients = c;
}

void attachstack(Client *c) {
  c->snext = c->mon->stack;
  c->mon->stack = c;
}

void setfocus(Client *c) {
  if (!c->neverfocus) {
    XSetInputFocus(display, c->win, RevertToPointerRoot, CurrentTime);
    XChangeProperty(display, root, netatom[NetActiveWindow], XA_WINDOW, 32,
                    PropModeReplace, (unsigned char *)&(c->win), 1);
  }
  sendevent(c, wmatom[WMTakeFocus]);
}

void setfullscreen(Client *c, int fullscreen) {
  if (fullscreen && !c->isfullscreen) {
    XChangeProperty(display, c->win, netatom[NetWMState], XA_ATOM, 32,
                    PropModeReplace, (unsigned char *)&netatom[NetWMFullscreen],
                    1);
    c->isfullscreen = 1;
    c->oldstate = c->isfloating;
    c->oldbw = c->border_width;
    c->border_width = 0;
    c->isfloating = 1;
    resizeclient(c, c->mon->screen_x, c->mon->screen_y, c->mon->screen_width,
                 c->mon->screen_height);
    XRaiseWindow(display, c->win);
  } else if (!fullscreen && c->isfullscreen) {
    XChangeProperty(display, c->win, netatom[NetWMState], XA_ATOM, 32,
                    PropModeReplace, (unsigned char *)0, 0);
    c->isfullscreen = 0;
    c->isfloating = c->oldstate;
    c->border_width = c->oldbw;
    c->x = c->oldx;
    c->y = c->oldy;
    c->w = c->oldw;
    c->h = c->oldh;
    resizeclient(c, c->x, c->y, c->w, c->h);
    arrange(c->mon);
  }
}

void updatetitle(Client *c) {
  if (!gettextprop(c->win, netatom[NetWMName], c->name, sizeof c->name))
    gettextprop(c->win, XA_WM_NAME, c->name, sizeof c->name);
  if (c->name[0] == '\0') /* hack to mark broken clients */
    strcpy(c->name, broken);
}

long getstate(Window w) {
  int format;
  long result = -1;
  unsigned char *p = NULL;
  unsigned long n, extra;
  Atom real;

  if (XGetWindowProperty(display, w, wmatom[WMState], 0L, 2L, False,
                         wmatom[WMState], &real, &format, &n, &extra,
                         (unsigned char **)&p) != Success)
    return -1;
  if (n != 0)
    result = *p;
  XFree(p);
  return result;
}

void manage(Window w, XWindowAttributes *wa) {
  Client *c, *t = NULL;
  Window trans = None;
  XWindowChanges wc;

  c = ecalloc(1, sizeof(Client));
  c->win = w;
  /* geometry */
  c->x = c->oldx = wa->x;
  c->y = c->oldy = wa->y;
  c->w = c->oldw = wa->width;
  c->h = c->oldh = wa->height;
  c->oldbw = wa->border_width;

  updatetitle(c);
  if (XGetTransientForHint(display, w, &trans) && (t = get_client_from_window(trans))) {
    c->mon = t->mon;
    c->tags = t->tags;
  } else {
    c->mon = selected_monitor;
    applyrules(c);
  }

  if (c->x + WIDTH(c) > c->mon->window_area_x + c->mon->window_area_width)
    c->x = c->mon->window_area_x + c->mon->window_area_width - WIDTH(c);
  if (c->y + HEIGHT(c) > c->mon->window_area_y + c->mon->window_area_height)
    c->y = c->mon->window_area_y + c->mon->window_area_height - HEIGHT(c);
  c->x = MAX(c->x, c->mon->window_area_x);
  c->y = MAX(c->y, c->mon->window_area_y);
  c->border_width = borderpx;

  wc.border_width = c->border_width;
  XConfigureWindow(display, w, CWBorderWidth, &wc);
  XSetWindowBorder(display, w, color_scheme[SchemeNormal][ColBorder].pixel);
  configure(c); /* propagates border_width, if size doesn't change */
  updatewindowtype(c);
  updatesizehints(c);
  updatewmhints(c);
  XSelectInput(display, w,
               EnterWindowMask | FocusChangeMask | PropertyChangeMask |
                   StructureNotifyMask);
  grabbuttons(c, 0);
  if (!c->isfloating)
    c->isfloating = c->oldstate = trans != None || c->isfixed;
  if (c->isfloating)
    XRaiseWindow(display, c->win);
  attach(c);
  attachstack(c);
  XChangeProperty(display, root, netatom[NetClientList], XA_WINDOW, 32,
                  PropModeAppend, (unsigned char *)&(c->win), 1);
  XMoveResizeWindow(display, c->win, c->x + 2 * display_width, c->y, c->w,
                    c->h); /* some windows require this */
  setclientstate(c, NormalState);
  if (c->mon == selected_monitor)
    unfocus(selected_monitor->selected_client, 0);
  c->mon->selected_client = c;
  arrange(c->mon);
  XMapWindow(display, c->win);
  focus(NULL);
}

void resize(Client *c, int x, int y, int w, int h, int interact) {
  if (applysizehints(c, &x, &y, &w, &h, interact))
    resizeclient(c, x, y, w, h);
}

void resizeclient(Client *c, int x, int y, int w, int h) {
  XWindowChanges wc;

  c->oldx = c->x;
  c->x = wc.x = x;
  c->oldy = c->y;
  c->y = wc.y = y;
  c->oldw = c->w;
  c->w = wc.width = w;
  c->oldh = c->h;
  c->h = wc.height = h;
  wc.border_width = c->border_width;
  XConfigureWindow(display, c->win, CWX | CWY | CWWidth | CWHeight | CWBorderWidth,
                   &wc);
  configure(c);
  XSync(display, False);
}

void window_to_monitor(const Arg *arg) {
  if (!selected_monitor->selected_client || !monitors->next)
    return;
  sendmon(selected_monitor->selected_client, numtomon(arg->i));
}

void full_screen_floating_window(const Arg *arg) {
  set_window_dimention(selected_monitor->selected_client, selected_monitor, 1917, 1077);
}

void minimal_screen_floating_window(const Arg *arg) {
  set_window_dimention(selected_monitor->selected_client, selected_monitor, 1280, 720);
}

void window_to_monitor_and_focus(const Arg *arg) {
  Monitor *target_monitor = numtomon(arg->i);
  Monitor *monitor = target_monitor;

  Monitor *previus_monitor = selected_monitor;

  printf("The monitor number is %i\n", arg->i);

  if (!selected_monitor->selected_client)
    return;

  Client *window = selected_monitor->selected_client;

  if (target_monitor == selected_monitor)
    return;

  if (window->mon == monitor)
    return;
  detach(window);
  detachstack(window);
  window->mon = monitor;
  window->tags =
      monitor->tagset[monitor->seltags]; /* assign tags of target monitor */
  attach(window);
  attachstack(window);

  focus(window);

  unfocus(selected_monitor->selected_client, 0);
  selected_monitor = target_monitor;
  arrange(monitor);
  arrange(previus_monitor);
  focus(NULL);
}

void set_window_floating(Client *window, Monitor *monitor) {
  if (window->isfullscreen) /* no support for fullscreen windows */
    return;
  // window->isfloating = !window->isfloating || window->isfixed;
  if (window->isfloating)
    resize(window, window->x, window->y, window->w, window->h, 0);
  arrange(monitor);
}

void set_window_dimention(Client *window, Monitor *monitor, int width,
                          int height) {
  if (window->isfullscreen) /* no support for fullscreen windows */
    return;
  // window->isfloating = !window->isfloating || window->isfixed;
  if (window->isfloating)
    resize(window, 0, 0, width, height, 0);
  arrange(monitor);
}

void unfocus(Client *c, int setfocus) {
  if (!c)
    return;
  grabbuttons(c, 0);
  XSetWindowBorder(display, c->win, color_scheme[SchemeNormal][ColBorder].pixel);
  if (setfocus) {
    XSetInputFocus(display, root, RevertToPointerRoot, CurrentTime);
    XDeleteProperty(display, root, netatom[NetActiveWindow]);
  }
}

void togglefloating(const Arg *arg) {
  if (!selected_monitor->selected_client)
    return;
  if (selected_monitor->selected_client
          ->isfullscreen) /* no support for fullscreen windows */
    return;
  selected_monitor->selected_client->isfloating =
      !selected_monitor->selected_client->isfloating || selected_monitor->selected_client->isfixed;
  if (selected_monitor->selected_client->isfloating)
    resize(selected_monitor->selected_client, selected_monitor->selected_client->x,
           selected_monitor->selected_client->y, selected_monitor->selected_client->w,
           selected_monitor->selected_client->h, 0);
  arrange(selected_monitor);
}

Client *get_client_from_window(Window w) {
  Client *c;
  Monitor *m;

  for (m = monitors; m; m = m->next)
    for (c = m->clients; c; c = c->next)
      if (c->win == w)
        return c;
  return NULL;
}

void zoom(const Arg *arg) {
  Client *c = selected_monitor->selected_client;

  if (!selected_monitor->lt[selected_monitor->sellt]->arrange || !c ||
      c->isfloating)
    return;
  if (c == nexttiled(selected_monitor->clients) && !(c = nexttiled(c->next)))
    return;
  pop(c);
}
