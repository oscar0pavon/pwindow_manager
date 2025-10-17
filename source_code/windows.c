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

void focus(Client *client) {
 
  if (!client || !ISVISIBLE(client)){

    for (client = selected_monitor->stack; client && !ISVISIBLE(client); client = client->snext)
      ;

  }
 
  if (selected_monitor->selected_client && selected_monitor->selected_client != client)
    unfocus(selected_monitor->selected_client, 0);
  
  if (client) {
  
    if (client->mon != selected_monitor)
      selected_monitor = client->mon;

    if (client->isurgent)
      seturgent(client, 0);

    detachstack(client);
    attachstack(client);
    grabbuttons(client, 1);

    XSetWindowBorder(display, client->win, color_scheme[SchemeSelected][ColBorder].pixel);

    setfocus(client);

  } else {

    XSetInputFocus(display, root, RevertToPointerRoot, CurrentTime);
    XDeleteProperty(display, root, netatom[NetActiveWindow]);

  }

  selected_monitor->selected_client = client;
  draw_bars();
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

void setup_window(Window w, XWindowAttributes *window_attributes) {
  Client *client, *t = NULL;
  Window transient_window = None;
  XWindowChanges window_changes;

  client = ecalloc(1, sizeof(Client));
  client->win = w;
  /* geometry */
  client->x = client->oldx = window_attributes->x;
  client->y = client->oldy = window_attributes->y;
  client->w = client->oldw = window_attributes->width;
  client->h = client->oldh = window_attributes->height;
  client->oldbw = window_attributes->border_width;

  updatetitle(client);
  if (XGetTransientForHint(display, w, &transient_window) &&
      (t = get_client_from_window(transient_window))) {
    client->mon = t->mon;
    client->tags = t->tags;
  } else {
    client->mon = selected_monitor;
    applyrules(client);
  }

  if (client->x + WIDTH(client) >
      client->mon->window_area_x + client->mon->window_area_width)
    client->x = client->mon->window_area_x + client->mon->window_area_width -
                WIDTH(client);
  if (client->y + HEIGHT(client) >
      client->mon->window_area_y + client->mon->window_area_height)
    client->y = client->mon->window_area_y + client->mon->window_area_height -
                HEIGHT(client);
  client->x = MAX(client->x, client->mon->window_area_x);
  client->y = MAX(client->y, client->mon->window_area_y);
  client->border_width = borderpx;

  window_changes.border_width = client->border_width;
  XConfigureWindow(display, w, CWBorderWidth, &window_changes);
  XSetWindowBorder(display, w, color_scheme[SchemeNormal][ColBorder].pixel);
  configure(client); /* propagates border_width, if size doesn't change */
  updatewindowtype(client);
  updatesizehints(client);
  updatewmhints(client);
  XSelectInput(display, w,
               EnterWindowMask | FocusChangeMask | PropertyChangeMask |
                   StructureNotifyMask);
  grabbuttons(client, 0);
  if (!client->isfloating)
    client->isfloating = client->oldstate =
        transient_window != None || client->isfixed;
  if (client->isfloating)
    XRaiseWindow(display, client->win);
  attach(client);
  attachstack(client);
  XChangeProperty(display, root, netatom[NetClientList], XA_WINDOW, 32,
                  PropModeAppend, (unsigned char *)&(client->win), 1);
  XMoveResizeWindow(display, client->win, client->x + 2 * display_width,
                    client->y, client->w,
                    client->h); /* some windows require this */
  setclientstate(client, NormalState);
  if (client->mon == selected_monitor)
    unfocus(selected_monitor->selected_client, 0);
  client->mon->selected_client = client;
  arrange(client->mon);
  XMapWindow(display, client->win);
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

void scan_windows(void) {
  unsigned int i, number_of_windows;
  Window parent_return, children_return, *windows = NULL;
  XWindowAttributes window_attributes;

  if (XQueryTree(display, root, &parent_return, &children_return, &windows,
                 &number_of_windows)) {
    for (i = 0; i < number_of_windows; i++) {

      if (!XGetWindowAttributes(display, windows[i], &window_attributes) ||
          window_attributes.override_redirect ||
          XGetTransientForHint(display, windows[i], &parent_return)) {

        continue;
      }
      if (window_attributes.map_state == IsViewable ||
          getstate(windows[i]) == IconicState) {

        setup_window(windows[i], &window_attributes);
      }
    }
    for (i = 0; i < number_of_windows; i++) { /* now the transients */

      if (!XGetWindowAttributes(display, windows[i], &window_attributes))
        continue;

      if (XGetTransientForHint(display, windows[i], &parent_return) &&
          (window_attributes.map_state == IsViewable ||
           getstate(windows[i]) == IconicState)) {

        setup_window(windows[i], &window_attributes);
      }
    }

    if (windows)
      XFree(windows);
  }
}

int applysizehints(Client *c, int *x, int *y, int *w, int *h, int interact) {
  int baseismin;
  Monitor *m = c->mon;

  /* set minimum possible */
  *w = MAX(1, *w);
  *h = MAX(1, *h);
  if (interact) {
    if (*x > display_width)
      *x = display_width - WIDTH(c);
    if (*y > display_height)
      *y = display_height - HEIGHT(c);
    if (*x + *w + 2 * c->border_width < 0)
      *x = 0;
    if (*y + *h + 2 * c->border_width < 0)
      *y = 0;
  } else {
    if (*x >= m->window_area_x + m->window_area_width)
      *x = m->window_area_x + m->window_area_width - WIDTH(c);
    if (*y >= m->window_area_y + m->window_area_height)
      *y = m->window_area_y + m->window_area_height - HEIGHT(c);
    if (*x + *w + 2 * c->border_width <= m->window_area_x)
      *x = m->window_area_x;
    if (*y + *h + 2 * c->border_width <= m->window_area_y)
      *y = m->window_area_y;
  }
  if (*h < bar_height)
    *h = bar_height;
  if (*w < bar_height)
    *w = bar_height;
  if (resizehints || c->isfloating || !c->mon->lt[c->mon->sellt]->arrange) {
    if (!c->hintsvalid)
      updatesizehints(c);
    /* see last two sentences in ICCCM 4.1.2.3 */
    baseismin = c->basew == c->minw && c->baseh == c->minh;
    if (!baseismin) { /* temporarily remove base dimensions */
      *w -= c->basew;
      *h -= c->baseh;
    }
    /* adjust for aspect limits */
    if (c->mina > 0 && c->maxa > 0) {
      if (c->maxa < (float)*w / *h)
        *w = *h * c->maxa + 0.5;
      else if (c->mina < (float)*h / *w)
        *h = *w * c->mina + 0.5;
    }
    if (baseismin) { /* increment calculation requires this */
      *w -= c->basew;
      *h -= c->baseh;
    }
    /* adjust for increment value */
    if (c->incw)
      *w -= *w % c->incw;
    if (c->inch)
      *h -= *h % c->inch;
    /* restore base dimensions */
    *w = MAX(*w + c->basew, c->minw);
    *h = MAX(*h + c->baseh, c->minh);
    if (c->maxw)
      *w = MIN(*w, c->maxw);
    if (c->maxh)
      *h = MIN(*h, c->maxh);
  }
  return *x != c->x || *y != c->y || *w != c->w || *h != c->h;
}

void movetoedge(const Arg *arg) {

  // only floating windows can be moved/

  Client *c;
  c = selected_monitor->selected_client;
  int x, y, nx, ny;

  if (!c || !arg)
    return;
  if (selected_monitor->lt[selected_monitor->sellt]->arrange && !c->isfloating)
    return;
  if (sscanf((char *)arg->v, "%d %d", &x, &y) != 2)
    return;

  if (x == 0)
    nx = (selected_monitor->screen_width - c->w) / 2;
  else if (x == -1)
    nx = borderpx;
  else if (x == 1)
    nx = selected_monitor->screen_width - (c->w + 2 * borderpx);
  else
    nx = c->x;

  if (y == 0)
    ny = (selected_monitor->screen_height - (c->h + bar_height)) / 2;
  else if (y == -1)
    ny = bar_height + borderpx;
  else if (y == 1)
    ny = selected_monitor->screen_height - (c->h + 2 * borderpx);
  else
    ny = c->y;

  XRaiseWindow(display, c->win);
  resize(c, nx, ny, c->w, c->h, True);
}

void moveresizewebcam(const Arg *arg) {
  XEvent ev;
  Monitor *m = selected_monitor;

  if (!(m->selected_client && arg && arg->v && m->selected_client->isfloating))
    return;

  resize(m->selected_client, m->selected_client->x + ((int *)arg->v)[0], m->selected_client->y + ((int *)arg->v)[1],
         m->selected_client->w + ((int *)arg->v)[2], m->selected_client->h + ((int *)arg->v)[3], True);

  while (XCheckMaskEvent(display, EnterWindowMask, &ev))
    ;
  Arg new_arg;
  new_arg.v = "1 1";
  movetoedge(&new_arg);
}

void moveresize(const Arg *arg) {
  XEvent ev;
  Monitor *m = selected_monitor;

  if (!(m->selected_client && arg && arg->v && m->selected_client->isfloating))
    return;

  resize(m->selected_client, m->selected_client->x + ((int *)arg->v)[0], m->selected_client->y + ((int *)arg->v)[1],
         m->selected_client->w + ((int *)arg->v)[2], m->selected_client->h + ((int *)arg->v)[3], True);

  while (XCheckMaskEvent(display, EnterWindowMask, &ev))
    ;
}

Atom getatomprop(Client *c, Atom prop) {
  int di;
  unsigned long dl;
  unsigned char *p = NULL;
  Atom da, atom = None;

  if (XGetWindowProperty(display, c->win, prop, 0L, sizeof atom, False, XA_ATOM,
                         &da, &di, &dl, &dl, &p) == Success &&
      p) {
    atom = *(Atom *)p;
    XFree(p);
  }
  return atom;
}

void killclient(const Arg *arg) {
  if (!selected_monitor->selected_client)
    return;
  if (!sendevent(selected_monitor->selected_client, wmatom[WMDelete])) {
    XGrabServer(display);
    XSetErrorHandler(xerrordummy);
    XSetCloseDownMode(display, DestroyAll);
    XKillClient(display, selected_monitor->selected_client->win);
    XSync(display, False);
    XSetErrorHandler(xerror);
    XUngrabServer(display);
  }
}

void setclientstate(Client *c, long state) {
  long data[] = {state, None};

  XChangeProperty(display, c->win, wmatom[WMState], wmatom[WMState], 32,
                  PropModeReplace, (unsigned char *)data, 2);
}

void updatesizehints(Client *c) {
  long msize;
  XSizeHints size;

  if (!XGetWMNormalHints(display, c->win, &size, &msize))
    /* size is uninitialized, ensure that size.flags aren't used */
    size.flags = PSize;
  if (size.flags & PBaseSize) {
    c->basew = size.base_width;
    c->baseh = size.base_height;
  } else if (size.flags & PMinSize) {
    c->basew = size.min_width;
    c->baseh = size.min_height;
  } else
    c->basew = c->baseh = 0;
  if (size.flags & PResizeInc) {
    c->incw = size.width_inc;
    c->inch = size.height_inc;
  } else
    c->incw = c->inch = 0;
  if (size.flags & PMaxSize) {
    c->maxw = size.max_width;
    c->maxh = size.max_height;
  } else
    c->maxw = c->maxh = 0;
  if (size.flags & PMinSize) {
    c->minw = size.min_width;
    c->minh = size.min_height;
  } else if (size.flags & PBaseSize) {
    c->minw = size.base_width;
    c->minh = size.base_height;
  } else
    c->minw = c->minh = 0;
  if (size.flags & PAspect) {
    c->mina = (float)size.min_aspect.y / size.min_aspect.x;
    c->maxa = (float)size.max_aspect.x / size.max_aspect.y;
  } else
    c->maxa = c->mina = 0.0;
  c->isfixed = (c->maxw && c->maxh && c->maxw == c->minw && c->maxh == c->minh);
  c->hintsvalid = 1;
}
