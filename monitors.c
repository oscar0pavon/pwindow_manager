#include "monitors.h"

#include "pwindow_manager.h"
#include "util.h"
#include "windows.h"
#include <X11/Xlib.h>
#include <X11/extensions/Xinerama.h>
#include <stddef.h>
#include <string.h>

void restack(Monitor *m) {
  Client *c;
  XEvent ev;
  XWindowChanges wc;

  drawbar(m);
  if (!m->sel)
    return;
  if (m->sel->isfloating || !m->lt[m->sellt]->arrange)
    XRaiseWindow(dpy, m->sel->win);
  if (m->lt[m->sellt]->arrange) {
    wc.stack_mode = Below;
    wc.sibling = m->barwin;
    for (c = m->stack; c; c = c->snext)
      if (!c->isfloating && ISVISIBLE(c)) {
        XConfigureWindow(dpy, c->win, CWSibling | CWStackMode, &wc);
        wc.sibling = c->win;
      }
  }
  XSync(dpy, False);
  while (XCheckMaskEvent(dpy, EnterWindowMask, &ev))
    ;
}

void focusmon(const Arg *arg) {
  Monitor *m;

  if (!mons->next)
    return;
  if ((m = dirtomon(arg->i)) == selmon)
    return;
  unfocus(selmon->sel, 0);
  selmon = m;
  focus(NULL);
}

Monitor *dirtomon(int dir) {
  Monitor *m = NULL;

  if (dir > 0) {
    if (!(m = selmon->next))
      m = mons;
  } else if (selmon == mons)
    for (m = mons; m->next; m = m->next)
      ;
  else
    for (m = mons; m->next != selmon; m = m->next)
      ;
  return m;
}

Monitor *numtomon(int num) {
  Monitor *m = NULL;
  int i = 0;

  for (m = mons, i = 0; m->next && i < num; m = m->next) {
    i++;
  }
  return m;
}

void focus_monitor(const Arg *arg) {
  Monitor *m;

  if (!mons->next)
    return;

  if ((m = dirtomon(arg->i)) == selmon)
    return;
  unfocus(selmon->sel, 0);
  selmon = m;
  focus(NULL);
}

void arrangemon(Monitor *m) {
  strncpy(m->ltsymbol, m->lt[m->sellt]->symbol, sizeof m->ltsymbol);
  if (m->lt[m->sellt]->arrange)
    m->lt[m->sellt]->arrange(m);
}

void arrange(Monitor *m) {
  if (m)
    showhide(m->stack);
  else
    for (m = mons; m; m = m->next)
      showhide(m->stack);
  if (m) {
    arrangemon(m);
    restack(m);
  } else
    for (m = mons; m; m = m->next)
      arrangemon(m);
}

void sendmon(Client *c, Monitor *m) {
  if (c->mon == m)
    return;
  unfocus(c, 1);
  detach(c);
  detachstack(c);
  c->mon = m;
  c->tags = m->tagset[m->seltags]; /* assign tags of target monitor */
  attach(c);
  attachstack(c);
  focus(NULL);
  arrange(NULL);
}

void send_to_monitor(Client *window, Monitor *monitor) {
  if (window->mon == monitor)
    return;
  unfocus(window, 1);
  detach(window);
  detachstack(window);
  window->mon = monitor;
  window->tags =
      monitor->tagset[monitor->seltags]; /* assign tags of target monitor */
  attach(window);
  attachstack(window);
  focus(NULL);
  arrange(NULL);
}

int isuniquegeom(XineramaScreenInfo *unique, size_t n,
                 XineramaScreenInfo *info) {
  while (n--)
    if (unique[n].x_org == info->x_org && unique[n].y_org == info->y_org &&
        unique[n].width == info->width && unique[n].height == info->height)
      return 0;
  return 1;
}

int updategeom(void) {
  int dirty = 0;

  if (XineramaIsActive(dpy)) {
    int i, j, n, nn;
    Client *c;
    Monitor *m;
    XineramaScreenInfo *info = XineramaQueryScreens(dpy, &nn);
    XineramaScreenInfo *unique = NULL;

    for (n = 0, m = mons; m; m = m->next, n++)
      ;
    /* only consider unique geometries as separate screens */
    unique = ecalloc(nn, sizeof(XineramaScreenInfo));
    for (i = 0, j = 0; i < nn; i++)
      if (isuniquegeom(unique, j, &info[i]))
        memcpy(&unique[j++], &info[i], sizeof(XineramaScreenInfo));
    XFree(info);
    nn = j;

    /* new monitors if nn > n */
    for (i = n; i < nn; i++) {
      for (m = mons; m && m->next; m = m->next)
        ;
      if (m)
        m->next = createmon();
      else
        mons = createmon();
    }
    for (i = 0, m = mons; i < nn && m; m = m->next, i++)
      if (i >= n || unique[i].x_org != m->mx || unique[i].y_org != m->my ||
          unique[i].width != m->mw || unique[i].height != m->mh) {
        dirty = 1;
        m->num = i;
        /* this is ugly, but it is a race condition otherwise */
        snprintf(m->monmark, sizeof(m->monmark), "(%d)", m->num);
        m->mx = m->wx = unique[i].x_org;
        m->my = m->wy = unique[i].y_org;
        m->mw = m->ww = unique[i].width;
        m->mh = m->wh = unique[i].height;
        updatebarpos(m);
      }
    /* removed monitors if n > nn */
    for (i = nn; i < n; i++) {
      for (m = mons; m && m->next; m = m->next)
        ;
      while ((c = m->clients)) {
        dirty = 1;
        m->clients = c->next;
        detachstack(c);
        c->mon = mons;
        attach(c);
        attachstack(c);
      }
      if (m == selmon)
        selmon = mons;
      cleanupmon(m);
    }
    free(unique);
  } else

  { /* default monitor setup */
    if (!mons)
      mons = createmon();
    if (mons->mw != sw || mons->mh != sh) {
      dirty = 1;
      mons->mw = mons->ww = sw;
      mons->mh = mons->wh = sh;
      updatebarpos(mons);
    }
  }
  if (dirty) {
    selmon = mons;
    selmon = wintomon(root);
  }
  return dirty;
}
