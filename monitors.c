#include "monitors.h"

#include "pwindow_manager.h"
#include "util.h"
#include "windows.h"
#include <X11/Xlib.h>
#include <X11/extensions/Xinerama.h>
#include <stddef.h>
#include <string.h>
#include "config.h"

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

void focus_monitor(const Arg *arg) {
  Monitor *m;

  if (!monitors->next)
    return;
  if ((m = dirtomon(arg->i)) == selected_monitor)
    return;
  unfocus(selected_monitor->sel, 0);
  selected_monitor = m;
  focus(NULL);
}


Monitor *dirtomon(int dir) {
  Monitor *m = NULL;

  if (dir > 0) {
    if (!(m = selected_monitor->next))
      m = monitors;
  } else if (selected_monitor == monitors)
    for (m = monitors; m->next; m = m->next)
      ;
  else
    for (m = monitors; m->next != selected_monitor; m = m->next)
      ;
  return m;
}

Monitor *numtomon(int num) {
  Monitor *m = NULL;
  int i = 0;

  for (m = monitors, i = 0; m->next && i < num; m = m->next) {
    i++;
  }
  return m;
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
    for (m = monitors; m; m = m->next)
      showhide(m->stack);
  if (m) {
    arrangemon(m);
    restack(m);
  } else
    for (m = monitors; m; m = m->next)
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

    for (n = 0, m = monitors; m; m = m->next, n++)
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
      for (m = monitors; m && m->next; m = m->next)
        ;
      if (m)
        m->next = createmon();
      else
        monitors = createmon();
    }
    for (i = 0, m = monitors; i < nn && m; m = m->next, i++)
      if (i >= n || unique[i].x_org != m->screen_x || unique[i].y_org != m->screen_y ||
          unique[i].width != m->screen_width || unique[i].height != m->screen_height) {
        dirty = 1;
        m->num = i;
        /* this is ugly, but it is a race condition otherwise */
        snprintf(m->monmark, sizeof(m->monmark), "(%d)", m->num);
        m->screen_x = m->window_area_x = unique[i].x_org;
        m->screen_y = m->window_area_y = unique[i].y_org;
        m->screen_width = m->window_area_width = unique[i].width;
        m->screen_height = m->window_area_height = unique[i].height;
        updatebarpos(m);
      }
    /* removed monitors if n > nn */
    for (i = nn; i < n; i++) {
      for (m = monitors; m && m->next; m = m->next)
        ;
      while ((c = m->clients)) {
        dirty = 1;
        m->clients = c->next;
        detachstack(c);
        c->mon = monitors;
        attach(c);
        attachstack(c);
      }
      if (m == selected_monitor)
        selected_monitor = monitors;
      cleanupmon(m);
    }
    free(unique);
  } else

  { /* default monitor setup */
    if (!monitors)
      monitors = createmon();
    if (monitors->screen_width != display_width || monitors->screen_height != display_height) {
      dirty = 1;
      monitors->screen_width = monitors->window_area_width = display_width;
      monitors->screen_height = monitors->window_area_height = display_height;
      updatebarpos(monitors);
    }
  }
  if (dirty) {
    selected_monitor = monitors;
    selected_monitor = wintomon(root);
  }
  return dirty;
}

Monitor *wintomon(Window w)
{
	int x, y;
	Client *c;
	Monitor *m;

	if (w == root && getrootptr(&x, &y))
		return recttomon(x, y, 1, 1);
	for (m = monitors; m; m = m->next)
		if (w == m->barwin)
			return m;
	if ((c = wintoclient(w)))
		return c->mon;
	return selected_monitor;
}


Monitor *recttomon(int x, int y, int w, int h)
{
	Monitor *m, *r = selected_monitor;
	int a, area = 0;

	for (m = monitors; m; m = m->next)
		if ((a = INTERSECT(x, y, w, h, m)) > area) {
			area = a;
			r = m;
		}
	return r;
}

Monitor *createmon(void) {
  Monitor *m;

  m = ecalloc(1, sizeof(Monitor));
  m->tagset[0] = m->tagset[1] = 1;
  m->mfact = mfact;
  m->nmaster = nmaster;
  m->showbar = showbar;
  m->topbar = topbar;
  m->lt[0] = &layouts[0];
  m->lt[1] = &layouts[1 % LENGTH(layouts)];
  strncpy(m->ltsymbol, layouts[0].symbol, sizeof m->ltsymbol);
  /* this is actually set in updategeom, to avoid a race condition */
  //	snprintf(m->monmark, sizeof(m->monmark), "(%d)", m->num);
  return m;
}
