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
#include "types.h"
#include "util.h"

#include "input.h"
#include "pwindow_manager.h"
#include "windows.h"
#include "events.h"
#include "bar.h"

char stext[256];

unsigned int numlockmask = 0;

/* variables */
int screen;
int display_width, display_height; /* X display screen geometry width, height */

static int running = 1;

Atom wmatom[WMLast], netatom[NetLast];
Monitor *monitors, *selected_monitor;
Display *display;
Window root, wmcheckwin;
Color **color_scheme;

/* configuration, allows nested code to access above variables */
#include "config.h"


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
  XGetClassHint(display, c->win, &ch);
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


void check_other_window_manager(void) {
  xerrorxlib = XSetErrorHandler(xerrorstart);
  /* this causes an error if some other window manager is running */
  XSelectInput(display, DefaultRootWindow(display), SubstructureRedirectMask);
  XSync(display, False);
  XSetErrorHandler(xerror);
  XSync(display, False);
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
  XUngrabKey(display, AnyKey, AnyModifier, root);
  while (monitors)
    clean_up_monitors(monitors);
  for (i = 0; i < CurLast; i++)
    drw_cur_free(drw, cursor[i]);
  for (i = 0; i < LENGTH(colors); i++)
    free(color_scheme[i]);
  free(color_scheme);
  XDestroyWindow(display, wmcheckwin);
  drw_free(drw);
  XSync(display, False);
  XSetInputFocus(display, PointerRoot, RevertToPointerRoot, CurrentTime);
  XDeleteProperty(display, root, netatom[NetActiveWindow]);
}


void focusstack(const Arg *arg) {
  Client *c = NULL, *i;

  if (!selected_monitor->selected_client ||
      (selected_monitor->selected_client->isfullscreen && lockfullscreen))
    return;
  if (arg->i > 0) {
    for (c = selected_monitor->selected_client->next; c && !ISVISIBLE(c); c = c->next)
      ;
    if (!c)
      for (c = selected_monitor->clients; c && !ISVISIBLE(c); c = c->next)
        ;
  } else {
    for (i = selected_monitor->clients; i != selected_monitor->selected_client; i = i->next)
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


int getrootptr(int *x, int *y) {
  int di;
  unsigned int dui;
  Window dummy;

  return XQueryPointer(display, root, &dummy, &dummy, x, y, &di, &di, &dui);
}

int gettextprop(Window w, Atom atom, char *text, unsigned int size) {
  char **list = NULL;
  int n;
  XTextProperty name;

  if (!text || size == 0)
    return 0;
  text[0] = '\0';
  if (!XGetTextProperty(display, w, &name, atom) || !name.nitems)
    return 0;
  if (name.encoding == XA_STRING) {
    strncpy(text, (char *)name.value, size - 1);
  } else if (XmbTextPropertyToTextList(display, &name, &list, &n) >= Success &&
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
  Client *sel = selected_monitor->selected_client, *c;

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
  Client *sel = selected_monitor->selected_client, *c;

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


void setlayout(const Arg *arg) {
  if (!arg || !arg->v ||
      arg->v != selected_monitor->lt[selected_monitor->sellt])
    selected_monitor->sellt ^= 1;
  if (arg && arg->v)
    selected_monitor->lt[selected_monitor->sellt] = (Layout *)arg->v;
  strncpy(selected_monitor->ltsymbol,
          selected_monitor->lt[selected_monitor->sellt]->symbol,
          sizeof selected_monitor->ltsymbol);
  if (selected_monitor->selected_client)
    arrange(selected_monitor);
  else
    draw_bar(selected_monitor);
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
  screen = DefaultScreen(display);
  display_width = DisplayWidth(display, screen);
  display_height = DisplayHeight(display, screen);
  root = RootWindow(display, screen);
  drw = drw_create(display, screen, root, display_width, display_height);
  if (!drw_fontset_create(drw, fonts, LENGTH(fonts)))
    die("no fonts could be loaded.");
  lrpad = drw->fonts->h;
  bar_height = drw->fonts->h + 2;
  updategeom();
  /* init atoms */
  utf8string = XInternAtom(display, "UTF8_STRING", False);
  wmatom[WMProtocols] = XInternAtom(display, "WM_PROTOCOLS", False);
  wmatom[WMDelete] = XInternAtom(display, "WM_DELETE_WINDOW", False);
  wmatom[WMState] = XInternAtom(display, "WM_STATE", False);
  wmatom[WMTakeFocus] = XInternAtom(display, "WM_TAKE_FOCUS", False);
  netatom[NetActiveWindow] = XInternAtom(display, "_NET_ACTIVE_WINDOW", False);
  netatom[NetSupported] = XInternAtom(display, "_NET_SUPPORTED", False);
  netatom[NetWMName] = XInternAtom(display, "_NET_WM_NAME", False);
  netatom[NetWMState] = XInternAtom(display, "_NET_WM_STATE", False);
  netatom[NetWMCheck] = XInternAtom(display, "_NET_SUPPORTING_WM_CHECK", False);
  netatom[NetWMFullscreen] =
      XInternAtom(display, "_NET_WM_STATE_FULLSCREEN", False);
  netatom[NetWMWindowType] = XInternAtom(display, "_NET_WM_WINDOW_TYPE", False);
  netatom[NetWMWindowTypeDialog] =
      XInternAtom(display, "_NET_WM_WINDOW_TYPE_DIALOG", False);
  netatom[NetClientList] = XInternAtom(display, "_NET_CLIENT_LIST", False);
  /* init cursors */
  cursor[CurNormal] = drw_cur_create(drw, XC_left_ptr);
  cursor[CurResize] = drw_cur_create(drw, XC_sizing);
  cursor[CurMove] = drw_cur_create(drw, XC_fleur);
  /* init appearance */
  color_scheme = ecalloc(LENGTH(colors), sizeof(Color *));
  for (i = 0; i < LENGTH(colors); i++)
    color_scheme[i] = drw_scm_create(drw, colors[i], 3);
  /* init bars */
  create_bars();
  updatestatus();
  /* supporting window for NetWMCheck */
  wmcheckwin = XCreateSimpleWindow(display, root, 0, 0, 1, 1, 0, 0, 0);
  XChangeProperty(display, wmcheckwin, netatom[NetWMCheck], XA_WINDOW, 32,
                  PropModeReplace, (unsigned char *)&wmcheckwin, 1);
  XChangeProperty(display, wmcheckwin, netatom[NetWMName], utf8string, 8,
                  PropModeReplace, (unsigned char *)"pwindow_manager", 16);
  XChangeProperty(display, root, netatom[NetWMCheck], XA_WINDOW, 32,
                  PropModeReplace, (unsigned char *)&wmcheckwin, 1);
  /* EWMH support per view */
  XChangeProperty(display, root, netatom[NetSupported], XA_ATOM, 32,
                  PropModeReplace, (unsigned char *)netatom, NetLast);
  XDeleteProperty(display, root, netatom[NetClientList]);
  /* select events */
  wa.cursor = cursor[CurNormal]->cursor;
  wa.event_mask = SubstructureRedirectMask | SubstructureNotifyMask |
                  ButtonPressMask | PointerMotionMask | EnterWindowMask |
                  LeaveWindowMask | StructureNotifyMask | PropertyChangeMask;
  XChangeWindowAttributes(display, root, CWEventMask | CWCursor, &wa);
  XSelectInput(display, root, wa.event_mask);
  grabkeys();
  focus(NULL);
}

void seturgent(Client *c, int urg) {
  XWMHints *wmh;

  c->isurgent = urg;
  if (!(wmh = XGetWMHints(display, c->win)))
    return;
  wmh->flags = urg ? (wmh->flags | XUrgencyHint) : (wmh->flags & ~XUrgencyHint);
  XSetWMHints(display, c->win, wmh);
  XFree(wmh);
}

void spawn(const Arg *arg) {
  struct sigaction sa;

  if (arg->v == dmenucmd)
    dmenumon[0] = '0' + selected_monitor->num;
  if (fork() == 0) {
    if (display)
      close(ConnectionNumber(display));
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
  if (selected_monitor->selected_client && arg->ui & TAGMASK) {
    selected_monitor->selected_client->tags = arg->ui & TAGMASK;
    focus(NULL);
    arrange(selected_monitor);
  }
}

void tagmon(const Arg *arg) {
  if (!selected_monitor->selected_client || !monitors->next)
    return;
  sendmon(selected_monitor->selected_client, dirtomon(arg->i));
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



void toggletag(const Arg *arg) {
  unsigned int newtags;

  if (!selected_monitor->selected_client)
    return;
  newtags = selected_monitor->selected_client->tags ^ (arg->ui & TAGMASK);
  if (newtags) {
    selected_monitor->selected_client->tags = newtags;
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
    XGrabServer(display); /* avoid race conditions */
    XSetErrorHandler(xerrordummy);
    XSelectInput(display, c->win, NoEventMask);
    XConfigureWindow(display, c->win, CWBorderWidth, &wc); /* restore border */
    XUngrabButton(display, AnyButton, AnyModifier, c->win);
    setclientstate(c, WithdrawnState);
    XSync(display, False);
    XSetErrorHandler(xerror);
    XUngrabServer(display);
  }
  free(c);
  focus(NULL);
  updateclientlist();
  arrange(m);
}



void updateclientlist(void) {
  Client *c;
  Monitor *m;

  XDeleteProperty(display, root, netatom[NetClientList]);
  for (m = monitors; m; m = m->next)
    for (c = m->clients; c; c = c->next)
      XChangeProperty(display, root, netatom[NetClientList], XA_WINDOW, 32,
                      PropModeAppend, (unsigned char *)&(c->win), 1);
}

void updatenumlockmask(void) {
  unsigned int i, j;
  XModifierKeymap *modmap;

  numlockmask = 0;
  modmap = XGetModifierMapping(display);
  for (i = 0; i < 8; i++)
    for (j = 0; j < modmap->max_keypermod; j++)
      if (modmap->modifiermap[i * modmap->max_keypermod + j] ==
          XKeysymToKeycode(display, XK_Num_Lock))
        numlockmask = (1 << i);
  XFreeModifiermap(modmap);
}


void updatestatus(void) {
  if (!gettextprop(root, XA_WM_NAME, stext, sizeof(stext)))
    strcpy(stext, "pwindow_manager");
  draw_bar(selected_monitor);
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

  if ((wmh = XGetWMHints(display, c->win))) {
    if (c == selected_monitor->selected_client && wmh->flags & XUrgencyHint) {
      wmh->flags &= ~XUrgencyHint;
      XSetWMHints(display, c->win, wmh);
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
    for (c = selected_monitor->selected_client->next; c && (!ISVISIBLE(c) || c->isfloating);
         c = c->next)
      ;
    if (!c)
      for (c = selected_monitor->clients; c && (!ISVISIBLE(c) || c->isfloating);
           c = c->next)
        ;

  } else {
    /* find the client before selmon->sel */
    for (i = selected_monitor->clients; i != selected_monitor->selected_client; i = i->next)
      if (ISVISIBLE(i) && !i->isfloating)
        c = i;
    if (!c)
      for (; i; i = i->next)
        if (ISVISIBLE(i) && !i->isfloating)
          c = i;
  }
  /* find the client before selmon->sel and c */
  for (i = selected_monitor->clients; i && (!p || !pc); i = i->next) {
    if (i->next == selected_monitor->selected_client)
      p = i;
    if (i->next == c)
      pc = i;
  }

  /* swap c and selmon->sel selmon->clients in the selmon->clients list */
  if (c && c != selected_monitor->selected_client) {
    Client *temp = selected_monitor->selected_client->next == c
                       ? selected_monitor->selected_client
                       : selected_monitor->selected_client->next;
    selected_monitor->selected_client->next =
        c->next == selected_monitor->selected_client ? c : c->next;
    c->next = temp;

    if (p && p != c)
      p->next = c;
    if (pc && pc != selected_monitor->selected_client)
      pc->next = selected_monitor->selected_client;

    if (selected_monitor->selected_client == selected_monitor->clients)
      selected_monitor->clients = c;
    else if (c == selected_monitor->clients)
      selected_monitor->clients = selected_monitor->selected_client;

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

  if (!(display = XOpenDisplay(NULL)))
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
  XSync(display, False);
  while (running && !XNextEvent(display, &ev)) {
    if (handler[ev.type]) {
      handler[ev.type](&ev); /* call handler */
    }
    //move_godot_to_monitor(0);
  }

  cleanup();
  XCloseDisplay(display);
  return EXIT_SUCCESS;
}
