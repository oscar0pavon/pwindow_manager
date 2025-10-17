/* See LICENSE file for copyright and license details.
 *
 * dynamic window manager is designed like any other X client as well. It is
 * driven through handling X events. In contrast to other X clients, a window
 * manager selects for SubstructureRedirectMask on the root window, to receive
 * events about window (dis-)appearance. Only one X connection at a time is
 * allowed to select for this event mask.
 *
 * The event handlers of dwm are organized in an array which is accessed
 * whenever a new event has been fetched. This allows event dispatching
 * in O(1) time.
 *
 * Each child of the root window is called a client, except windows which have
 * set the override_redirect flag. Clients are organized in a linked client
 * list on each monitor, the focus history is remembered through a stack list
 * on each monitor. Each client contains a bit array to indicate the tags of a
 * client.
 *
 * Keys and tagging rules are organized as arrays and defined in config.h.
 *
 * To understand everything else, start reading main().
 */
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>
#include <locale.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <stdbool.h>

#include <X11/Xft/Xft.h>

#include "draw.h"
#include "util.h"

#include "input.h"
#include "pwindow_manager.h"
#include "windows.h"
#include "events.h"

char stext[256];

unsigned int numlockmask = 0;

/* variables */
int screen;
int display_width, display_height; /* X display screen geometry width, height */
int bar_height;                    /* bar height */
int lrpad;                         /* sum of left and right padding for text */

static int running = 1;

Atom wmatom[WMLast], netatom[NetLast];
Monitor *monitors, *selected_monitor;
Display *dpy;
Window root, wmcheckwin;
Color **scheme;

/* configuration, allows nested code to access above variables */
#include "config.h"

void movetoedge(const Arg *arg) {

  // only floating windows can be moved/

  Client *c;
  c = selected_monitor->sel;
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

  XRaiseWindow(dpy, c->win);
  resize(c, nx, ny, c->w, c->h, True);
}

void moveresizewebcam(const Arg *arg) {
  XEvent ev;
  Monitor *m = selected_monitor;

  if (!(m->sel && arg && arg->v && m->sel->isfloating))
    return;

  resize(m->sel, m->sel->x + ((int *)arg->v)[0], m->sel->y + ((int *)arg->v)[1],
         m->sel->w + ((int *)arg->v)[2], m->sel->h + ((int *)arg->v)[3], True);

  while (XCheckMaskEvent(dpy, EnterWindowMask, &ev))
    ;
  Arg new_arg;
  new_arg.v = "1 1";
  movetoedge(&new_arg);
}

void moveresize(const Arg *arg) {
  XEvent ev;
  Monitor *m = selected_monitor;

  if (!(m->sel && arg && arg->v && m->sel->isfloating))
    return;

  resize(m->sel, m->sel->x + ((int *)arg->v)[0], m->sel->y + ((int *)arg->v)[1],
         m->sel->w + ((int *)arg->v)[2], m->sel->h + ((int *)arg->v)[3], True);

  while (XCheckMaskEvent(dpy, EnterWindowMask, &ev))
    ;
}

/* compile-time check if all tags fit into an unsigned int bit array. */
struct NumTags {
  char limitexceeded[LENGTH(tags) > 31 ? -1 : 1];
};

/* dwm will keep pid's of processes from autostart array and kill them at quit
 */
static pid_t *autostart_pids;
static size_t autostart_len;

/* execute command from autostart array */
void execute_auto_start_porgrams() {
  const char *const *p;
  size_t i = 0;

  /* count entries */
  for (p = autostart; *p; autostart_len++, p++)
    while (*++p)
      ;

  autostart_pids = malloc(autostart_len * sizeof(pid_t));
  for (p = autostart; *p; i++, p++) {
    if ((autostart_pids[i] = fork()) == 0) {
      setsid();
      execvp(*p, (char *const *)p);
      fprintf(stderr, "dwm: execvp %s\n", *p);
      perror(" failed");
      _exit(EXIT_FAILURE);
    }
    /* skip arguments */
    while (*++p)
      ;
  }
}

/* function implementations */
void applyrules(Client *c) {
  const char *class, *instance;
  unsigned int i;
  const Rule *r;
  Monitor *m;
  XClassHint ch = {NULL, NULL};

  /* rule matching */
  c->isfloating = 0;
  c->tags = 0;
  XGetClassHint(dpy, c->win, &ch);
  class = ch.res_class ? ch.res_class : broken;
  instance = ch.res_name ? ch.res_name : broken;

  for (i = 0; i < LENGTH(rules); i++) {
    r = &rules[i];
    if ((!r->title || strstr(c->name, r->title)) &&
        (!r->class || strstr(class, r->class)) &&
        (!r->instance || strstr(instance, r->instance))) {
      c->isfloating = r->isfloating;
      c->tags |= r->tags;
      for (m = monitors; m && m->num != r->monitor; m = m->next)
        ;
      if (m)
        c->mon = m;
    }
  }
  if (ch.res_class)
    XFree(ch.res_class);
  if (ch.res_name)
    XFree(ch.res_name);
  c->tags =
      c->tags & TAGMASK ? c->tags & TAGMASK : c->mon->tagset[c->mon->seltags];
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

void check_other_window_manager(void) {
  xerrorxlib = XSetErrorHandler(xerrorstart);
  /* this causes an error if some other window manager is running */
  XSelectInput(dpy, DefaultRootWindow(dpy), SubstructureRedirectMask);
  XSync(dpy, False);
  XSetErrorHandler(xerror);
  XSync(dpy, False);
}

void cleanup(void) {
  Arg a = {.ui = ~0};
  Layout foo = {"", NULL};
  Monitor *m;
  size_t i;

  view(&a);
  selected_monitor->lt[selected_monitor->sellt] = &foo;
  for (m = monitors; m; m = m->next)
    while (m->stack)
      unmanage(m->stack, 0);
  XUngrabKey(dpy, AnyKey, AnyModifier, root);
  while (monitors)
    cleanupmon(monitors);
  for (i = 0; i < CurLast; i++)
    drw_cur_free(drw, cursor[i]);
  for (i = 0; i < LENGTH(colors); i++)
    free(scheme[i]);
  free(scheme);
  XDestroyWindow(dpy, wmcheckwin);
  drw_free(drw);
  XSync(dpy, False);
  XSetInputFocus(dpy, PointerRoot, RevertToPointerRoot, CurrentTime);
  XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
}

void cleanupmon(Monitor *mon) {
  Monitor *m;

  if (mon == monitors)
    monitors = monitors->next;
  else {
    for (m = monitors; m && m->next != mon; m = m->next)
      ;
    m->next = mon->next;
  }
  XUnmapWindow(dpy, mon->barwin);
  XDestroyWindow(dpy, mon->barwin);
  free(mon);
}


void drawbar(Monitor *m) {
  int x, w, tw = 0;
  int boxs = drw->fonts->h / 9;
  int boxw = drw->fonts->h / 6 + 2;
  unsigned int i, occ = 0, urg = 0;
  Client *c;

  if (!m->showbar)
    return;

  /* draw status first so it can be overdrawn by tags later */
  if (m == selected_monitor) { /* status is only drawn on selected monitor */
    drw_setscheme(drw, scheme[SchemeNorm]);
    tw = TEXTW(stext) - lrpad + 2; /* 2px right padding */
    drw_text(drw, m->window_area_width - tw, 0, tw, bar_height, 0, stext, 0);
  }

  for (c = m->clients; c; c = c->next) {
    occ |= c->tags;
    if (c->isurgent)
      urg |= c->tags;
  }
  x = 0;
  for (i = 0; i < LENGTH(tags); i++) {
    w = TEXTW(tags[i]);
    drw_setscheme(
        drw, scheme[m->tagset[m->seltags] & 1 << i ? SchemeSel : SchemeNorm]);
    drw_text(drw, x, 0, w, bar_height, lrpad / 2, tags[i], urg & 1 << i);
    if (occ & 1 << i)
      drw_rect(drw, x + boxs, boxs, boxw, boxw,
               m == selected_monitor && selected_monitor->sel &&
                   selected_monitor->sel->tags & 1 << i,
               urg & 1 << i);
    x += w;
  }
  w = TEXTW(m->ltsymbol);
  drw_setscheme(drw, scheme[SchemeNorm]);
  x = drw_text(drw, x, 0, w, bar_height, lrpad / 2, m->ltsymbol, 0);
  w = TEXTW(m->monmark);
  drw_setscheme(drw, scheme[SchemeNorm]);
  x = drw_text(drw, x, 0, w, bar_height, lrpad / 2, m->monmark, 0);

  if ((w = m->window_area_width - tw - x) > bar_height) {
    if (m->sel) {
      drw_setscheme(drw,
                    scheme[m == selected_monitor ? SchemeSel : SchemeNorm]);
      drw_text(drw, x, 0, w, bar_height, lrpad / 2, m->sel->name, 0);
      if (m->sel->isfloating)
        drw_rect(drw, x + boxs, boxs, boxw, boxw, m->sel->isfixed, 0);
    } else {
      drw_setscheme(drw, scheme[SchemeNorm]);
      drw_rect(drw, x, 0, w, bar_height, 1, 1);
    }
  }
  drw_map(drw, m->barwin, 0, 0, m->window_area_width, bar_height);
}

void drawbars(void) {
  Monitor *m;

  for (m = monitors; m; m = m->next)
    drawbar(m);
}



void focusstack(const Arg *arg) {
  Client *c = NULL, *i;

  if (!selected_monitor->sel ||
      (selected_monitor->sel->isfullscreen && lockfullscreen))
    return;
  if (arg->i > 0) {
    for (c = selected_monitor->sel->next; c && !ISVISIBLE(c); c = c->next)
      ;
    if (!c)
      for (c = selected_monitor->clients; c && !ISVISIBLE(c); c = c->next)
        ;
  } else {
    for (i = selected_monitor->clients; i != selected_monitor->sel; i = i->next)
      if (ISVISIBLE(i))
        c = i;
    if (!c)
      for (; i; i = i->next)
        if (ISVISIBLE(i))
          c = i;
  }
  if (c) {
    focus(c);
    restack(selected_monitor);
  }
}

Atom getatomprop(Client *c, Atom prop) {
  int di;
  unsigned long dl;
  unsigned char *p = NULL;
  Atom da, atom = None;

  if (XGetWindowProperty(dpy, c->win, prop, 0L, sizeof atom, False, XA_ATOM,
                         &da, &di, &dl, &dl, &p) == Success &&
      p) {
    atom = *(Atom *)p;
    XFree(p);
  }
  return atom;
}

int getrootptr(int *x, int *y) {
  int di;
  unsigned int dui;
  Window dummy;

  return XQueryPointer(dpy, root, &dummy, &dummy, x, y, &di, &di, &dui);
}

int gettextprop(Window w, Atom atom, char *text, unsigned int size) {
  char **list = NULL;
  int n;
  XTextProperty name;

  if (!text || size == 0)
    return 0;
  text[0] = '\0';
  if (!XGetTextProperty(dpy, w, &name, atom) || !name.nitems)
    return 0;
  if (name.encoding == XA_STRING) {
    strncpy(text, (char *)name.value, size - 1);
  } else if (XmbTextPropertyToTextList(dpy, &name, &list, &n) >= Success &&
             n > 0 && *list) {
    strncpy(text, *list, size - 1);
    XFreeStringList(list);
  }
  text[size - 1] = '\0';
  XFree(name.value);
  return 1;
}


void incnmaster(const Arg *arg) {
  selected_monitor->nmaster = MAX(selected_monitor->nmaster + arg->i, 0);
  arrange(selected_monitor);
}

void killclient(const Arg *arg) {
  if (!selected_monitor->sel)
    return;
  if (!sendevent(selected_monitor->sel, wmatom[WMDelete])) {
    XGrabServer(dpy);
    XSetErrorHandler(xerrordummy);
    XSetCloseDownMode(dpy, DestroyAll);
    XKillClient(dpy, selected_monitor->sel->win);
    XSync(dpy, False);
    XSetErrorHandler(xerror);
    XUngrabServer(dpy);
  }
}


void monocle(Monitor *m) {
  unsigned int n = 0;
  Client *c;

  for (c = m->clients; c; c = c->next)
    if (ISVISIBLE(c))
      n++;
  if (n > 0) /* override layout symbol */
    snprintf(m->ltsymbol, sizeof m->ltsymbol, "[%d]", n);
  for (c = nexttiled(m->clients); c; c = nexttiled(c->next))
    resize(c, m->window_area_x, m->window_area_y,
           m->window_area_width - 2 * c->border_width, m->window_area_height - 2 * c->border_width,
           0);
}


Client *prevtiled(Client *c) {
  Client *p, *r;

  for (p = selected_monitor->clients, r = NULL; p && p != c; p = p->next)
    if (!p->isfloating && ISVISIBLE(p))
      r = p;
  return r;
}

Client *nexttiled(Client *c) {
  for (; c && (c->isfloating || !ISVISIBLE(c)); c = c->next)
    ;
  return c;
}

void pop(Client *c) {
  detach(c);
  attach(c);
  focus(c);
  arrange(c->mon);
}


void quit(const Arg *arg) {
  size_t i;

  /* kill child processes */
  for (i = 0; i < autostart_len; i++) {
    if (0 < autostart_pids[i]) {
      kill(autostart_pids[i], SIGTERM);
      waitpid(autostart_pids[i], NULL, 0);
    }
  }

  running = 0;
}


void pushdown(const Arg *arg) {
  Client *sel = selected_monitor->sel, *c;

  if (!sel || sel->isfloating)
    return;
  if ((c = nexttiled(sel->next))) {
    detach(sel);
    sel->next = c->next;
    c->next = sel;
  } else {
    detach(sel);
    attach(sel);
  }
  focus(sel);
  arrange(selected_monitor);
}

void pushup(const Arg *arg) {
  Client *sel = selected_monitor->sel, *c;

  if (!sel || sel->isfloating)
    return;
  if ((c = prevtiled(sel))) {
    detach(sel);
    sel->next = c;
    if (selected_monitor->clients == c)
      selected_monitor->clients = sel;
    else {
      for (c = selected_monitor->clients; c->next != sel->next; c = c->next)
        ;
      c->next = sel;
    }
  } else {
    for (c = sel; c->next; c = c->next)
      ;
    detach(sel);
    sel->next = NULL;
    c->next = sel;
  }
  focus(sel);
  arrange(selected_monitor);
}

void scan_windows(void) {
  unsigned int i, num;
  Window d1, d2, *wins = NULL;
  XWindowAttributes wa;

  if (XQueryTree(dpy, root, &d1, &d2, &wins, &num)) {
    for (i = 0; i < num; i++) {
      if (!XGetWindowAttributes(dpy, wins[i], &wa) || wa.override_redirect ||
          XGetTransientForHint(dpy, wins[i], &d1))
        continue;
      if (wa.map_state == IsViewable || getstate(wins[i]) == IconicState)
        manage(wins[i], &wa);
    }
    for (i = 0; i < num; i++) { /* now the transients */
      if (!XGetWindowAttributes(dpy, wins[i], &wa))
        continue;
      if (XGetTransientForHint(dpy, wins[i], &d1) &&
          (wa.map_state == IsViewable || getstate(wins[i]) == IconicState))
        manage(wins[i], &wa);
    }
    if (wins)
      XFree(wins);
  }
}

void setclientstate(Client *c, long state) {
  long data[] = {state, None};

  XChangeProperty(dpy, c->win, wmatom[WMState], wmatom[WMState], 32,
                  PropModeReplace, (unsigned char *)data, 2);
}


void setlayout(const Arg *arg) {
  if (!arg || !arg->v ||
      arg->v != selected_monitor->lt[selected_monitor->sellt])
    selected_monitor->sellt ^= 1;
  if (arg && arg->v)
    selected_monitor->lt[selected_monitor->sellt] = (Layout *)arg->v;
  strncpy(selected_monitor->ltsymbol,
          selected_monitor->lt[selected_monitor->sellt]->symbol,
          sizeof selected_monitor->ltsymbol);
  if (selected_monitor->sel)
    arrange(selected_monitor);
  else
    drawbar(selected_monitor);
}

/* arg > 1.0 will set mfact absolutely */
void setmfact(const Arg *arg) {
  float f;

  if (!arg || !selected_monitor->lt[selected_monitor->sellt]->arrange)
    return;
  f = arg->f < 1.0 ? arg->f + selected_monitor->mfact : arg->f - 1.0;
  if (f < 0.05 || f > 0.95)
    return;
  selected_monitor->mfact = f;
  arrange(selected_monitor);
}

void setup(void) {
  int i;
  pid_t pid;
  XSetWindowAttributes wa;
  Atom utf8string;
  struct sigaction sa;

  /* do not transform children into zombies when they terminate */
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_NOCLDSTOP | SA_NOCLDWAIT | SA_RESTART;
  sa.sa_handler = SIG_IGN;
  sigaction(SIGCHLD, &sa, NULL);

  /* clean up any zombies (inherited from .xinitrc etc) immediately */
  while ((pid = waitpid(-1, NULL, WNOHANG)) > 0) {
    pid_t *p, *lim;

    if (!(p = autostart_pids))
      continue;
    lim = &p[autostart_len];

    for (; p < lim; p++) {
      if (*p == pid) {
        *p = -1;
        break;
      }
    }
  }

  /* init screen */
  screen = DefaultScreen(dpy);
  display_width = DisplayWidth(dpy, screen);
  display_height = DisplayHeight(dpy, screen);
  root = RootWindow(dpy, screen);
  drw = drw_create(dpy, screen, root, display_width, display_height);
  if (!drw_fontset_create(drw, fonts, LENGTH(fonts)))
    die("no fonts could be loaded.");
  lrpad = drw->fonts->h;
  bar_height = drw->fonts->h + 2;
  updategeom();
  /* init atoms */
  utf8string = XInternAtom(dpy, "UTF8_STRING", False);
  wmatom[WMProtocols] = XInternAtom(dpy, "WM_PROTOCOLS", False);
  wmatom[WMDelete] = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
  wmatom[WMState] = XInternAtom(dpy, "WM_STATE", False);
  wmatom[WMTakeFocus] = XInternAtom(dpy, "WM_TAKE_FOCUS", False);
  netatom[NetActiveWindow] = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", False);
  netatom[NetSupported] = XInternAtom(dpy, "_NET_SUPPORTED", False);
  netatom[NetWMName] = XInternAtom(dpy, "_NET_WM_NAME", False);
  netatom[NetWMState] = XInternAtom(dpy, "_NET_WM_STATE", False);
  netatom[NetWMCheck] = XInternAtom(dpy, "_NET_SUPPORTING_WM_CHECK", False);
  netatom[NetWMFullscreen] =
      XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN", False);
  netatom[NetWMWindowType] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
  netatom[NetWMWindowTypeDialog] =
      XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DIALOG", False);
  netatom[NetClientList] = XInternAtom(dpy, "_NET_CLIENT_LIST", False);
  /* init cursors */
  cursor[CurNormal] = drw_cur_create(drw, XC_left_ptr);
  cursor[CurResize] = drw_cur_create(drw, XC_sizing);
  cursor[CurMove] = drw_cur_create(drw, XC_fleur);
  /* init appearance */
  scheme = ecalloc(LENGTH(colors), sizeof(Color *));
  for (i = 0; i < LENGTH(colors); i++)
    scheme[i] = drw_scm_create(drw, colors[i], 3);
  /* init bars */
  updatebars();
  updatestatus();
  /* supporting window for NetWMCheck */
  wmcheckwin = XCreateSimpleWindow(dpy, root, 0, 0, 1, 1, 0, 0, 0);
  XChangeProperty(dpy, wmcheckwin, netatom[NetWMCheck], XA_WINDOW, 32,
                  PropModeReplace, (unsigned char *)&wmcheckwin, 1);
  XChangeProperty(dpy, wmcheckwin, netatom[NetWMName], utf8string, 8,
                  PropModeReplace, (unsigned char *)"pwindow_manager", 16);
  XChangeProperty(dpy, root, netatom[NetWMCheck], XA_WINDOW, 32,
                  PropModeReplace, (unsigned char *)&wmcheckwin, 1);
  /* EWMH support per view */
  XChangeProperty(dpy, root, netatom[NetSupported], XA_ATOM, 32,
                  PropModeReplace, (unsigned char *)netatom, NetLast);
  XDeleteProperty(dpy, root, netatom[NetClientList]);
  /* select events */
  wa.cursor = cursor[CurNormal]->cursor;
  wa.event_mask = SubstructureRedirectMask | SubstructureNotifyMask |
                  ButtonPressMask | PointerMotionMask | EnterWindowMask |
                  LeaveWindowMask | StructureNotifyMask | PropertyChangeMask;
  XChangeWindowAttributes(dpy, root, CWEventMask | CWCursor, &wa);
  XSelectInput(dpy, root, wa.event_mask);
  grabkeys();
  focus(NULL);
}

void seturgent(Client *c, int urg) {
  XWMHints *wmh;

  c->isurgent = urg;
  if (!(wmh = XGetWMHints(dpy, c->win)))
    return;
  wmh->flags = urg ? (wmh->flags | XUrgencyHint) : (wmh->flags & ~XUrgencyHint);
  XSetWMHints(dpy, c->win, wmh);
  XFree(wmh);
}

void spawn(const Arg *arg) {
  struct sigaction sa;

  if (arg->v == dmenucmd)
    dmenumon[0] = '0' + selected_monitor->num;
  if (fork() == 0) {
    if (dpy)
      close(ConnectionNumber(dpy));
    setsid();

    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sa.sa_handler = SIG_DFL;
    sigaction(SIGCHLD, &sa, NULL);

    execvp(((char **)arg->v)[0], (char **)arg->v);
    die("dwm: execvp '%s' failed:", ((char **)arg->v)[0]);
  }
}

void tag(const Arg *arg) {
  if (selected_monitor->sel && arg->ui & TAGMASK) {
    selected_monitor->sel->tags = arg->ui & TAGMASK;
    focus(NULL);
    arrange(selected_monitor);
  }
}

void tagmon(const Arg *arg) {
  if (!selected_monitor->sel || !monitors->next)
    return;
  sendmon(selected_monitor->sel, dirtomon(arg->i));
}

void tile(Monitor *m) {

  unsigned int i, n, h, mw, my, ty;
  Client *c;

  for (n = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), n++)
    ;
  if (n == 0)
    return;

  if (n > m->nmaster)
    mw = m->nmaster ? m->window_area_width * m->mfact : 0;
  else
    mw = m->window_area_width;
  for (i = my = ty = 0, c = nexttiled(m->clients); c;
       c = nexttiled(c->next), i++)
    if (i < m->nmaster) {
      h = (m->window_area_height - my) / (MIN(n, m->nmaster) - i);
      resize(c, m->window_area_x, m->window_area_y + my, mw - (2 * c->border_width),
             h - (2 * c->border_width), 0);
      if (my + HEIGHT(c) < m->window_area_height)
        my += HEIGHT(c);
    } else {
      h = (m->window_area_height - ty) / (n - i);
      resize(c, m->window_area_x + mw, m->window_area_y + ty,
             m->window_area_width - mw - (2 * c->border_width), h - (2 * c->border_width), 0);
      if (ty + HEIGHT(c) < m->window_area_height)
        ty += HEIGHT(c);
    }
}

void togglebar(const Arg *arg) {

  selected_monitor->showbar = !selected_monitor->showbar;

  updatebarpos(selected_monitor);

  XMoveResizeWindow(dpy, selected_monitor->barwin,
                    selected_monitor->window_area_x,
                    selected_monitor->bar_geometry,
                    selected_monitor->window_area_width, bar_height);

  arrange(selected_monitor);
}


void toggletag(const Arg *arg) {
  unsigned int newtags;

  if (!selected_monitor->sel)
    return;
  newtags = selected_monitor->sel->tags ^ (arg->ui & TAGMASK);
  if (newtags) {
    selected_monitor->sel->tags = newtags;
    focus(NULL);
    arrange(selected_monitor);
  }
}

void ntoggleview(const Arg *arg) {
  const Arg n = {.i = +1};
  const int mon = selected_monitor->num;
  do {
    focus_monitor(&n);
    toggleview(arg);
  } while (selected_monitor->num != mon);
}

void toggleview(const Arg *arg) {
  unsigned int newtagset =
      selected_monitor->tagset[selected_monitor->seltags] ^ (arg->ui & TAGMASK);

  if (newtagset) {
    selected_monitor->tagset[selected_monitor->seltags] = newtagset;
    focus(NULL);
    arrange(selected_monitor);
  }
}


void unmanage(Client *c, int destroyed) {
  Monitor *m = c->mon;
  XWindowChanges wc;

  detach(c);
  detachstack(c);
  if (!destroyed) {
    wc.border_width = c->oldbw;
    XGrabServer(dpy); /* avoid race conditions */
    XSetErrorHandler(xerrordummy);
    XSelectInput(dpy, c->win, NoEventMask);
    XConfigureWindow(dpy, c->win, CWBorderWidth, &wc); /* restore border */
    XUngrabButton(dpy, AnyButton, AnyModifier, c->win);
    setclientstate(c, WithdrawnState);
    XSync(dpy, False);
    XSetErrorHandler(xerror);
    XUngrabServer(dpy);
  }
  free(c);
  focus(NULL);
  updateclientlist();
  arrange(m);
}

void unmapnotify(XEvent *e) {
  Client *c;
  XUnmapEvent *ev = &e->xunmap;

  if ((c = wintoclient(ev->window))) {
    if (ev->send_event)
      setclientstate(c, WithdrawnState);
    else
      unmanage(c, 0);
  }
}

void updatebars(void) {
  Monitor *m;
  XSetWindowAttributes wa = {.override_redirect = True,
                             .background_pixmap = ParentRelative,
                             .event_mask = ButtonPressMask | ExposureMask};
  XClassHint ch = {"pwindow_manager", "pwindow_manager"};
  for (m = monitors; m; m = m->next) {
    if (m->barwin)
      continue;
    m->barwin = XCreateWindow(
        dpy, root, m->window_area_x, m->bar_geometry, m->window_area_width,
        bar_height, 0, DefaultDepth(dpy, screen), CopyFromParent,
        DefaultVisual(dpy, screen),
        CWOverrideRedirect | CWBackPixmap | CWEventMask, &wa);
    XDefineCursor(dpy, m->barwin, cursor[CurNormal]->cursor);
    XMapRaised(dpy, m->barwin);
    XSetClassHint(dpy, m->barwin, &ch);
  }
}

void updatebarpos(Monitor *m) {
  m->window_area_y = m->screen_y;
  m->window_area_height = m->screen_height;
  if (m->showbar) {
    m->window_area_height -= bar_height;
    m->bar_geometry =
        m->topbar ? m->window_area_y : m->window_area_y + m->window_area_height;
    m->window_area_y =
        m->topbar ? m->window_area_y + bar_height : m->window_area_y;
  } else
    m->bar_geometry = -bar_height;
}

void updateclientlist(void) {
  Client *c;
  Monitor *m;

  XDeleteProperty(dpy, root, netatom[NetClientList]);
  for (m = monitors; m; m = m->next)
    for (c = m->clients; c; c = c->next)
      XChangeProperty(dpy, root, netatom[NetClientList], XA_WINDOW, 32,
                      PropModeAppend, (unsigned char *)&(c->win), 1);
}

void updatenumlockmask(void) {
  unsigned int i, j;
  XModifierKeymap *modmap;

  numlockmask = 0;
  modmap = XGetModifierMapping(dpy);
  for (i = 0; i < 8; i++)
    for (j = 0; j < modmap->max_keypermod; j++)
      if (modmap->modifiermap[i * modmap->max_keypermod + j] ==
          XKeysymToKeycode(dpy, XK_Num_Lock))
        numlockmask = (1 << i);
  XFreeModifiermap(modmap);
}

void updatesizehints(Client *c) {
  long msize;
  XSizeHints size;

  if (!XGetWMNormalHints(dpy, c->win, &size, &msize))
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

void updatestatus(void) {
  if (!gettextprop(root, XA_WM_NAME, stext, sizeof(stext)))
    strcpy(stext, "pwindow_manager");
  drawbar(selected_monitor);
}

void updatewindowtype(Client *c) {
  Atom state = getatomprop(c, netatom[NetWMState]);
  Atom wtype = getatomprop(c, netatom[NetWMWindowType]);

  if (state == netatom[NetWMFullscreen])
    setfullscreen(c, 1);
  if (wtype == netatom[NetWMWindowTypeDialog])
    c->isfloating = 1;
}

void updatewmhints(Client *c) {
  XWMHints *wmh;

  if ((wmh = XGetWMHints(dpy, c->win))) {
    if (c == selected_monitor->sel && wmh->flags & XUrgencyHint) {
      wmh->flags &= ~XUrgencyHint;
      XSetWMHints(dpy, c->win, wmh);
    } else
      c->isurgent = (wmh->flags & XUrgencyHint) ? 1 : 0;
    if (wmh->flags & InputHint)
      c->neverfocus = !wmh->input;
    else
      c->neverfocus = 0;
    XFree(wmh);
  }
}

void nview(const Arg *arg) {
  const Arg n = {.i = +1};
  const int mon = selected_monitor->num;
  do {
    focus_monitor(&n);
    view(arg);
  } while (selected_monitor->num != mon);
}

void view(const Arg *arg) {
  if ((arg->ui & TAGMASK) ==
      selected_monitor->tagset[selected_monitor->seltags])
    return;
  selected_monitor->seltags ^= 1; /* toggle sel tagset */
  if (arg->ui & TAGMASK)
    selected_monitor->tagset[selected_monitor->seltags] = arg->ui & TAGMASK;
  focus(NULL);
  arrange(selected_monitor);
}



void reset_view(const Arg *arg) {
  const int mon = selected_monitor->num;
  Arg n = {.i = +1}; // focusmon(next monitor)
  Arg m = {.f = 0};  // mfact -> facts[]
  Arg i = {.i = 0};  // nmaster -> masters[]
  Arg v = {.ui = 0}; // nviews -> views[]
  Arg t = {.ui = 0}; // toggles[] -> toggleview()
  unsigned int x;
  do {
    focus_monitor(&n);
    m.f =
        (facts[selected_monitor->num] ? facts[selected_monitor->num] : mfact) +
        1;
    i.i = (masters[selected_monitor->num] ? masters[selected_monitor->num]
                                          : nmaster) -
          selected_monitor->nmaster;
    v.ui = (views[selected_monitor->num] == ~0
                ? ~0
                : ((1 << (views[selected_monitor->num]
                              ? (views[selected_monitor->num] + 1)
                              : (nviews + 1))) -
                   1));
    setmfact(&m);
    incnmaster(&i);
    view(&v);
    for (x = 0; x < LENGTH(toggles[selected_monitor->num]); x++) {
      if ((toggles[selected_monitor->num][x] ||
           toggles[selected_monitor->num][x] == 0) &&
          toggles[selected_monitor->num][x] != ~0) {
        t.ui = (1 << toggles[selected_monitor->num][x]);
        toggleview(&t);
      };
    }
  } while (selected_monitor->num != mon);
}

void movestack(const Arg *arg) {
  Client *c = NULL, *p = NULL, *pc = NULL, *i;

  if (arg->i > 0) {
    /* find the client after selmon->sel */
    for (c = selected_monitor->sel->next; c && (!ISVISIBLE(c) || c->isfloating);
         c = c->next)
      ;
    if (!c)
      for (c = selected_monitor->clients; c && (!ISVISIBLE(c) || c->isfloating);
           c = c->next)
        ;

  } else {
    /* find the client before selmon->sel */
    for (i = selected_monitor->clients; i != selected_monitor->sel; i = i->next)
      if (ISVISIBLE(i) && !i->isfloating)
        c = i;
    if (!c)
      for (; i; i = i->next)
        if (ISVISIBLE(i) && !i->isfloating)
          c = i;
  }
  /* find the client before selmon->sel and c */
  for (i = selected_monitor->clients; i && (!p || !pc); i = i->next) {
    if (i->next == selected_monitor->sel)
      p = i;
    if (i->next == c)
      pc = i;
  }

  /* swap c and selmon->sel selmon->clients in the selmon->clients list */
  if (c && c != selected_monitor->sel) {
    Client *temp = selected_monitor->sel->next == c
                       ? selected_monitor->sel
                       : selected_monitor->sel->next;
    selected_monitor->sel->next =
        c->next == selected_monitor->sel ? c : c->next;
    c->next = temp;

    if (p && p != c)
      p->next = c;
    if (pc && pc != selected_monitor->sel)
      pc->next = selected_monitor->sel;

    if (selected_monitor->sel == selected_monitor->clients)
      selected_monitor->clients = c;
    else if (c == selected_monitor->clients)
      selected_monitor->clients = selected_monitor->sel;

    arrange(selected_monitor);
  }
}

int main(int argc, char *argv[]) {
  if (argc == 2 && !strcmp("-v", argv[1]))
    die("dwm");
  else if (argc != 1)
    die("usage: dwm [-v]");

  if (!setlocale(LC_CTYPE, "") || !XSupportsLocale())
    fputs("warning: no locale support\n", stderr);

  if (!(dpy = XOpenDisplay(NULL)))
    die("dwm: cannot open display");

  check_other_window_manager();

  // call xrandr first for configuring the screens
  system("setup_screens");

  execute_auto_start_porgrams();

  setup();

  scan_windows();

  //	const Arg r = {0};
  //	reset_view(&r);

  XEvent ev;

  Arg arg;
  arg.i = 0;

  focus_monitor(&arg);

  /* main event loop */
  XSync(dpy, False);
  while (running && !XNextEvent(dpy, &ev)) {
    if (handler[ev.type]) {
      handler[ev.type](&ev); /* call handler */
    }
    //move_godot_to_monitor(0);
  }

  cleanup();
  XCloseDisplay(dpy);
  return EXIT_SUCCESS;
}
