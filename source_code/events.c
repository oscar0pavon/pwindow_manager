#include "events.h"

#include "pwindow_manager.h"
#include "windows.h"
#include <X11/Xatom.h>

#include "bar.h"


void clientmessage(XEvent *e) {
  XClientMessageEvent *cme = &e->xclient;
  Client *c = get_client_from_window(cme->window);

  if (!c)
    return;
  if (cme->message_type == netatom[NetWMState]) {
    if (cme->data.l[1] == netatom[NetWMFullscreen] ||
        cme->data.l[2] == netatom[NetWMFullscreen])
      setfullscreen(c, (cme->data.l[0] == 1 /* _NET_WM_STATE_ADD    */
                        || (cme->data.l[0] == 2 /* _NET_WM_STATE_TOGGLE */ &&
                            !c->isfullscreen)));
  } else if (cme->message_type == netatom[NetActiveWindow]) {
    if (c != selected_monitor->selected_client && !c->isurgent)
      seturgent(c, 1);
  }
}

void configure(Client *c) {
  XConfigureEvent ce;

  ce.type = ConfigureNotify;
  ce.display = display;
  ce.event = c->win;
  ce.window = c->win;
  ce.x = c->x;
  ce.y = c->y;
  ce.width = c->w;
  ce.height = c->h;
  ce.border_width = c->border_width;
  ce.above = None;
  ce.override_redirect = False;
  XSendEvent(display, c->win, False, StructureNotifyMask, (XEvent *)&ce);
}

void configurenotify(XEvent *e) {
  Monitor *m;
  Client *c;
  XConfigureEvent *ev = &e->xconfigure;
  int dirty;

  /* TODO: updategeom handling sucks, needs to be simplified */
  if (ev->window == root) {
    dirty = (display_width != ev->width || display_height != ev->height);
    display_width = ev->width;
    display_height = ev->height;
    if (updategeom() || dirty) {
      drw_resize(drw, display_width, bar_height);
      create_bars();
      for (m = monitors; m; m = m->next) {
        for (c = m->clients; c; c = c->next)
          if (c->isfullscreen)
            resizeclient(c, m->screen_x, m->screen_y, m->screen_width,
                         m->screen_height);
        XMoveResizeWindow(display, m->bar_window, m->window_area_x, m->bar_geometry,
                          m->window_area_width, bar_height);
      }
      focus(NULL);
      arrange(NULL);
    }
  }
}

void configurerequest(XEvent *e) {
  Client *c;
  Monitor *m;
  XConfigureRequestEvent *ev = &e->xconfigurerequest;
  XWindowChanges wc;

  if ((c = get_client_from_window(ev->window))) {
    if (ev->value_mask & CWBorderWidth)
      c->border_width = ev->border_width;
    else if (c->isfloating ||
             !selected_monitor->lt[selected_monitor->sellt]->arrange) {
      m = c->mon;
      if (ev->value_mask & CWX) {
        c->oldx = c->x;
        c->x = m->screen_x + ev->x;
      }
      if (ev->value_mask & CWY) {
        c->oldy = c->y;
        c->y = m->screen_y + ev->y;
      }
      if (ev->value_mask & CWWidth) {
        c->oldw = c->w;
        c->w = ev->width;
      }
      if (ev->value_mask & CWHeight) {
        c->oldh = c->h;
        c->h = ev->height;
      }
      if ((c->x + c->w) > m->screen_x + m->screen_width && c->isfloating)
        c->x = m->screen_x +
               (m->screen_width / 2 - WIDTH(c) / 2); /* center in x direction */
      if ((c->y + c->h) > m->screen_y + m->screen_height && c->isfloating)
        c->y = m->screen_y + (m->screen_height / 2 -
                              HEIGHT(c) / 2); /* center in y direction */
      if ((ev->value_mask & (CWX | CWY)) &&
          !(ev->value_mask & (CWWidth | CWHeight)))
        configure(c);
      if (ISVISIBLE(c))
        XMoveResizeWindow(display, c->win, c->x, c->y, c->w, c->h);
    } else
      configure(c);
  } else {
    wc.x = ev->x;
    wc.y = ev->y;
    wc.width = ev->width;
    wc.height = ev->height;
    wc.border_width = ev->border_width;
    wc.sibling = ev->above;
    wc.stack_mode = ev->detail;
    XConfigureWindow(display, ev->window, ev->value_mask, &wc);
  }
  XSync(display, False);
}

void destroynotify(XEvent *e) {
  Client *c;
  XDestroyWindowEvent *ev = &e->xdestroywindow;

  if ((c = get_client_from_window(ev->window)))
    unmanage(c, 1);
}

void enternotify(XEvent *e) {
  Client *c;
  Monitor *m;
  XCrossingEvent *ev = &e->xcrossing;

  if ((ev->mode != NotifyNormal || ev->detail == NotifyInferior) &&
      ev->window != root)
    return;
  c = get_client_from_window(ev->window);
  m = c ? c->mon : wintomon(ev->window);
  if (m != selected_monitor) {
    unfocus(selected_monitor->selected_client, 1);
    selected_monitor = m;
  } else if (!c || c == selected_monitor->selected_client)
    return;
  focus(c);
}

void expose(XEvent *e) {
  Monitor *m;
  XExposeEvent *ev = &e->xexpose;

  if (ev->count == 0 && (m = wintomon(ev->window)))
    draw_bar(m);
}

/* there are some broken focus acquiring clients needing extra handling */
void focusin(XEvent *e) {
  XFocusChangeEvent *ev = &e->xfocus;

  if (selected_monitor->selected_client && ev->window != selected_monitor->selected_client->win)
    setfocus(selected_monitor->selected_client);
}

void mappingnotify(XEvent *e) {
  XMappingEvent *ev = &e->xmapping;

  XRefreshKeyboardMapping(ev);
  if (ev->request == MappingKeyboard)
    grabkeys();
}

void maprequest(XEvent *e) {
  static XWindowAttributes wa;
  XMapRequestEvent *ev = &e->xmaprequest;

  if (!XGetWindowAttributes(display, ev->window, &wa) || wa.override_redirect)
    return;
  if (!get_client_from_window(ev->window))
    manage(ev->window, &wa);
}

void motionnotify(XEvent *e) {
  static Monitor *mon = NULL;
  Monitor *m;
  XMotionEvent *ev = &e->xmotion;

  if (ev->window != root)
    return;
  if ((m = recttomon(ev->x_root, ev->y_root, 1, 1)) != mon && mon) {
    unfocus(selected_monitor->selected_client, 1);
    selected_monitor = m;
    focus(NULL);
  }
  mon = m;
}

void propertynotify(XEvent *event) {
  Client *client;
  Window trans;
  XPropertyEvent *property_event = &event->xproperty;

  if ((property_event->window == root) && (property_event->atom == XA_WM_NAME))
    updatestatus();
  else if (property_event->state == PropertyDelete)
    return; /* ignore */
  else if ((client = get_client_from_window(property_event->window))) {
    switch (property_event->atom) {
    default:
      break;
    case XA_WM_TRANSIENT_FOR:
      if (!client->isfloating && (XGetTransientForHint(display, client->win, &trans)) &&
          (client->isfloating = (get_client_from_window(trans)) != NULL))
        arrange(client->mon);
      break;
    case XA_WM_NORMAL_HINTS:
      client->hintsvalid = 0;
      break;
    case XA_WM_HINTS:
      updatewmhints(client);
      draw_bars();
      break;
    }
    if (property_event->atom == XA_WM_NAME || property_event->atom == netatom[NetWMName]) {
      updatetitle(client);
      if (client == client->mon->selected_client)
        draw_bar(client->mon);
    }
    if (property_event->atom == netatom[NetWMWindowType])
      updatewindowtype(client);
  }
}

int sendevent(Client *c, Atom proto) {
  int n;
  Atom *protocols;
  int exists = 0;
  XEvent ev;

  if (XGetWMProtocols(display, c->win, &protocols, &n)) {
    while (!exists && n--)
      exists = protocols[n] == proto;
    XFree(protocols);
  }
  if (exists) {
    ev.type = ClientMessage;
    ev.xclient.window = c->win;
    ev.xclient.message_type = wmatom[WMProtocols];
    ev.xclient.format = 32;
    ev.xclient.data.l[0] = proto;
    ev.xclient.data.l[1] = CurrentTime;
    XSendEvent(display, c->win, False, NoEventMask, &ev);
  }
  return exists;
}

void unmapnotify(XEvent *e) {
  Client *c;
  XUnmapEvent *ev = &e->xunmap;

  if ((c = get_client_from_window(ev->window))) {
    if (ev->send_event)
      setclientstate(c, WithdrawnState);
    else
      unmanage(c, 0);
  }
}
