
#include "windows.h"
#include "pwindow_manager.h"
#include "types.h"


void
showhide(Client *c)
{
	if (!c)
		return;
	if (ISVISIBLE(c)) {
		/* show clients top down */
		XMoveWindow(dpy, c->win, c->x, c->y);
		if ((!c->mon->lt[c->mon->sellt]->arrange || c->isfloating) && !c->isfullscreen)
			resize(c, c->x, c->y, c->w, c->h, 0);
		showhide(c->snext);
	} else {
		/* hide clients bottom up */
		showhide(c->snext);
		XMoveWindow(dpy, c->win, WIDTH(c) * -2, c->y);
	}
}

void focus(Client *c) {
  if (!c || !ISVISIBLE(c))
    for (c = selmon->stack; c && !ISVISIBLE(c); c = c->snext)
      ;
  if (selmon->sel && selmon->sel != c)
    unfocus(selmon->sel, 0);
  if (c) {
    if (c->mon != selmon)
      selmon = c->mon;
    if (c->isurgent)
      seturgent(c, 0);
    detachstack(c);
    attachstack(c);
    grabbuttons(c, 1);
    XSetWindowBorder(dpy, c->win, scheme[SchemeSel][ColBorder].pixel);
    setfocus(c);
  } else {
    XSetInputFocus(dpy, root, RevertToPointerRoot, CurrentTime);
    XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
  }
  selmon->sel = c;
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

  if (c == c->mon->sel) {
    for (t = c->mon->stack; t && !ISVISIBLE(t); t = t->snext)
      ;
    c->mon->sel = t;
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
