#include "input.h"

#include "config.h"
#include "events.h"
#include "pwindow_manager.h"
#include "types.h"
#include "windows.h"
#include <X11/Xlib.h>

#include "draw.h"

Cur *cursor[CurLast];

void (*handler[LASTEvent])(XEvent *) = {[ButtonPress] = mouse_button_press,
                                        [ClientMessage] = clientmessage,
                                        [ConfigureRequest] = configurerequest,
                                        [ConfigureNotify] = configurenotify,
                                        [DestroyNotify] = destroynotify,
                                        [EnterNotify] = enternotify,
                                        [Expose] = expose,
                                        [FocusIn] = focusin,
                                        [KeyPress] = keypress,
                                        [MappingNotify] = mappingnotify,
                                        [MapRequest] = maprequest,
                                        [MotionNotify] = motionnotify,
                                        [PropertyNotify] = propertynotify,
                                        [UnmapNotify] = unmapnotify};

void mouse_button_press(XEvent *event) {
  unsigned int i, x, click;
  Arg arg = {0};
  Client *client;
  Monitor *monitor;
  XButtonPressedEvent *buttons_pressed_event = &event->xbutton;

  click = ClkRootWin;
  /* focus monitor if necessary */
  if ((monitor = wintomon(buttons_pressed_event->window)) &&
      monitor != selected_monitor) {
    unfocus(selected_monitor->selected_client, 1);
    selected_monitor = monitor;
    focus(NULL);
  }
  if (buttons_pressed_event->window == selected_monitor->bar_window) {
    i = x = 0;
    do
      x += TEXTW(tags[i]);
    while (buttons_pressed_event->x >= x && ++i < LENGTH(tags));
    if (i < LENGTH(tags)) {
      click = ClkTagBar;
      arg.ui = 1 << i;
    } else if (buttons_pressed_event->x < x + TEXTW(selected_monitor->ltsymbol))
      click = ClkLtSymbol;
    else if (buttons_pressed_event->x < x + TEXTW(selected_monitor->ltsymbol) +
                                            TEXTW(selected_monitor->monmark))
      click = ClkMonNum;
    else if (buttons_pressed_event->x >
             selected_monitor->window_area_width - (int)TEXTW(stext))
      click = ClkStatusText;
    else
      click = ClkWinTitle;
  } else if ((client = get_client_from_window(buttons_pressed_event->window))) {
    focus(client);
    restack(selected_monitor);
    XAllowEvents(display, ReplayPointer, CurrentTime);
    click = ClkClientWin;
  }
  for (i = 0; i < LENGTH(buttons); i++)
    if (click == buttons[i].click && buttons[i].func &&
        buttons[i].button == buttons_pressed_event->button &&
        CLEANMASK(buttons[i].mask) == CLEANMASK(buttons_pressed_event->state))
      buttons[i].func(
          click == ClkTagBar && buttons[i].arg.i == 0 ? &arg : &buttons[i].arg);
}

void keypress(XEvent *e) {
  unsigned int i;
  KeySym keysym;
  XKeyEvent *ev;

  ev = &e->xkey;
  keysym = XKeycodeToKeysym(display, (KeyCode)ev->keycode, 0);
  for (i = 0; i < LENGTH(keys); i++)
    if (keysym == keys[i].keysym &&
        CLEANMASK(keys[i].mod) == CLEANMASK(ev->state) && keys[i].func)
      keys[i].func(&(keys[i].arg));
}

void move_mouse(const Arg *arg) {
  int x, y, out_client_x, out_client_y, nx, ny;
  Client *client;
  Monitor *monitor;
  XEvent event;
  Time lasttime = 0;

  if (!(client = selected_monitor->selected_client))
    return;
  if (client->isfullscreen) /* no support moving fullscreen windows by mouse */
    return;

  restack(selected_monitor);

  out_client_x = client->x;
  out_client_y = client->y;

  if (XGrabPointer(display, root, False, MOUSEMASK, GrabModeAsync,
                   GrabModeAsync, None, cursor[CursorMove]->cursor,
                   CurrentTime) != GrabSuccess) {

    return;
  }

  if (!getrootptr(&x, &y))
    return;

  do {
    XMaskEvent(display, MOUSEMASK | ExposureMask | SubstructureRedirectMask,
               &event);
    switch (event.type) {
    case ConfigureRequest:
    case Expose:
    case MapRequest:
      handler[event.type](&event);
      break;
    case MotionNotify:
      if ((event.xmotion.time - lasttime) <= (1000 / 60))
        continue;
      lasttime = event.xmotion.time;

      nx = out_client_x + (event.xmotion.x - x);
      ny = out_client_y + (event.xmotion.y - y);
      if (abs(selected_monitor->window_area_x - nx) < snap)
        nx = selected_monitor->window_area_x;
      else if (abs((selected_monitor->window_area_x +
                    selected_monitor->window_area_width) -
                   (nx + WIDTH(client))) < snap)
        nx = selected_monitor->window_area_x +
             selected_monitor->window_area_width - WIDTH(client);
      if (abs(selected_monitor->window_area_y - ny) < snap)
        ny = selected_monitor->window_area_y;
      else if (abs((selected_monitor->window_area_y +
                    selected_monitor->window_area_height) -
                   (ny + HEIGHT(client))) < snap)
        ny = selected_monitor->window_area_y +
             selected_monitor->window_area_height - HEIGHT(client);
      if (!client->isfloating &&
          selected_monitor->lt[selected_monitor->sellt]->arrange &&
          (abs(nx - client->x) > snap || abs(ny - client->y) > snap))
        togglefloating(NULL);
      if (!selected_monitor->lt[selected_monitor->sellt]->arrange ||
          client->isfloating)
        resize(client, nx, ny, client->w, client->h, 1);
      break;
    }
  } while (event.type != ButtonRelease);

  XUngrabPointer(display, CurrentTime);

  if ((monitor = recttomon(client->x, client->y, client->w, client->h)) !=
      selected_monitor) {
    sendmon(client, monitor);
    selected_monitor = monitor;
    focus(NULL);
  }
}

void resize_with_mouse(const Arg *arg) {
  int out_client_x, out_client_y, new_width, new_height;
  Client *client;
  Monitor *monitor;
  XEvent event;
  Time lasttime = 0;

  if (!(client = selected_monitor->selected_client))
    return;
  if (client
          ->isfullscreen) /* no support resizing fullscreen windows by mouse */
    return;

  restack(selected_monitor);
  out_client_x = client->x;
  out_client_y = client->y;

  if (XGrabPointer(display, root, False, MOUSEMASK, GrabModeAsync,
                   GrabModeAsync, None, cursor[CurResize]->cursor,
                   CurrentTime) != GrabSuccess)
    return;
  XWarpPointer(display, None, client->win, 0, 0, 0, 0,
               client->w + client->border_width - 1,
               client->h + client->border_width - 1);
  do {
    XMaskEvent(display, MOUSEMASK | ExposureMask | SubstructureRedirectMask,
               &event);
    switch (event.type) {
    case ConfigureRequest:
    case Expose:
    case MapRequest:
      handler[event.type](&event);
      break;
    case MotionNotify:
      if ((event.xmotion.time - lasttime) <= (1000 / 60))
        continue;
      lasttime = event.xmotion.time;

      new_width =
          MAX(event.xmotion.x - out_client_x - 2 * client->border_width + 1, 1);
      new_height =
          MAX(event.xmotion.y - out_client_y - 2 * client->border_width + 1, 1);
      if (client->mon->window_area_x + new_width >=
              selected_monitor->window_area_x &&
          client->mon->window_area_x + new_width <=
              selected_monitor->window_area_x +
                  selected_monitor->window_area_width &&
          client->mon->window_area_y + new_height >=
              selected_monitor->window_area_y &&
          client->mon->window_area_y + new_height <=
              selected_monitor->window_area_y +
                  selected_monitor->window_area_height) {
        if (!client->isfloating &&
            selected_monitor->lt[selected_monitor->sellt]->arrange &&
            (abs(new_width - client->w) > snap ||
             abs(new_height - client->h) > snap))
          togglefloating(NULL);
      }
      if (!selected_monitor->lt[selected_monitor->sellt]->arrange ||
          client->isfloating)
        resize(client, client->x, client->y, new_width, new_height, 1);
      break;
    }
  } while (event.type != ButtonRelease);
  XWarpPointer(display, None, client->win, 0, 0, 0, 0,
               client->w + client->border_width - 1,
               client->h + client->border_width - 1);
  XUngrabPointer(display, CurrentTime);
  while (XCheckMaskEvent(display, EnterWindowMask, &event))
    ;
  if ((monitor = recttomon(client->x, client->y, client->w, client->h)) !=
      selected_monitor) {
    sendmon(client, monitor);
    selected_monitor = monitor;
    focus(NULL);
  }
}

void grabkeys(void) {
  updatenumlockmask();
  {
    unsigned int i, j, k;
    unsigned int modifiers[] = {0, LockMask, numlockmask,
                                numlockmask | LockMask};
    int start, end, skip;
    KeySym *syms;

    XUngrabKey(display, AnyKey, AnyModifier, root);
    XDisplayKeycodes(display, &start, &end);
    syms = XGetKeyboardMapping(display, start, end - start + 1, &skip);
    if (!syms)
      return;
    for (k = start; k <= end; k++)
      for (i = 0; i < LENGTH(keys); i++)
        /* skip modifier codes, we do that ourselves */
        if (keys[i].keysym == syms[(k - start) * skip])
          for (j = 0; j < LENGTH(modifiers); j++)
            XGrabKey(display, k, keys[i].mod | modifiers[j], root, True,
                     GrabModeAsync, GrabModeAsync);
    XFree(syms);
  }
}

void grabbuttons(Client *c, int focused) {
  updatenumlockmask();
  {
    unsigned int i, j;
    unsigned int modifiers[] = {0, LockMask, numlockmask,
                                numlockmask | LockMask};
    XUngrabButton(display, AnyButton, AnyModifier, c->win);
    if (!focused)
      XGrabButton(display, AnyButton, AnyModifier, c->win, False, BUTTONMASK,
                  GrabModeSync, GrabModeSync, None, None);
    for (i = 0; i < LENGTH(buttons); i++)
      if (buttons[i].click == ClkClientWin)
        for (j = 0; j < LENGTH(modifiers); j++)
          XGrabButton(display, buttons[i].button,
                      buttons[i].mask | modifiers[j], c->win, False, BUTTONMASK,
                      GrabModeAsync, GrabModeSync, None, None);
  }
}
