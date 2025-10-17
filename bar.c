#include "bar.h"

#include "pwindow_manager.h"
#include "config.h"
#include "input.h"
#include <stdio.h>

int bar_height;                    /* bar height */
int lrpad;                         /* sum of left and right padding for text */

void togglebar(const Arg *arg) {

  selected_monitor->showbar = !selected_monitor->showbar;

  updatebarpos(selected_monitor);

  XMoveResizeWindow(display, selected_monitor->bar_window,
                    selected_monitor->window_area_x,
                    selected_monitor->bar_geometry,
                    selected_monitor->window_area_width, bar_height);

  arrange(selected_monitor);
}

void draw_bar(Monitor *monitor) {
  int x, width, text_width = 0;
  int box_size = drw->fonts->h / 9;
  int box_width = drw->fonts->h / 6 + 2;
  unsigned int i, occ = 0, urg = 0;
  Client *client;

  if (!monitor->showbar)
    return;


  /* draw status first so it can be overdrawn by tags later */
  /* status is only drawn on selected monitor */

  if (monitor == selected_monitor) {

    drw_setscheme(drw, color_scheme[SchemeNormal]);
    text_width = TEXTW(stext) - lrpad + 2; /* 2px right padding */
    drw_text(drw, monitor->window_area_width - text_width, 0, text_width,
             bar_height, 0, stext, 0);
    
    // draw_rectangle(drw,monitor->screen_x,monitor->screen_y,
    //     monitor->screen_width, monitor->screen_height, 1 , 0);
    draw_rectangle(drw,50,50,
        monitor->screen_width, monitor->screen_height, 1 , 0);
  }

  for (client = monitor->clients; client; client = client->next) {
    occ |= client->tags;
    if (client->isurgent)
      urg |= client->tags;
  }

  x = 0;

  for (i = 0; i < LENGTH(tags); i++) {
    width = TEXTW(tags[i]);
    drw_setscheme(drw, color_scheme[monitor->tagset[monitor->seltags] & 1 << i
                                        ? SchemeSelected
                                        : SchemeNormal]);
    drw_text(drw, x, 0, width, bar_height, lrpad / 2, tags[i], urg & 1 << i);
    if (occ & 1 << i)
      draw_rectangle(drw, x + box_size, box_size, box_width, box_width,
               monitor == selected_monitor &&
                   selected_monitor->selected_client &&
                   selected_monitor->selected_client->tags & 1 << i,
               urg & 1 << i);
    x += width;
  }

  width = TEXTW(monitor->ltsymbol);
  drw_setscheme(drw, color_scheme[SchemeNormal]);

  x = drw_text(drw, x, 0, width, bar_height, lrpad / 2, monitor->ltsymbol, 0);
  width = TEXTW(monitor->monmark);

  drw_setscheme(drw, color_scheme[SchemeNormal]);
  x = drw_text(drw, x, 0, width, bar_height, lrpad / 2, monitor->monmark, 0);

  if ((width = monitor->window_area_width - text_width - x) > bar_height) {
    if (monitor->selected_client) {
      drw_setscheme(drw,
                    color_scheme[monitor == selected_monitor ? SchemeSelected
                                                             : SchemeNormal]);

      // draw window name
      drw_text(drw, x, 0, width, bar_height, lrpad / 2,
               monitor->selected_client->name, 0);

      // draw a little squad in the side of the window name
      if (monitor->selected_client->isfloating) {
        draw_rectangle(drw, x + box_size, box_size, box_width, box_width,
                 monitor->selected_client->isfixed, 0);
      }

    } else {
      drw_setscheme(drw, color_scheme[SchemeNormal]);
      draw_rectangle(drw, x, 0, width, bar_height, 1, 1);
    }
  }
  drw_map(drw, monitor->bar_window, 0, 0, monitor->window_area_width, bar_height);
    
}

void drawbars(void) {
  Monitor *m;

  for (m = monitors; m; m = m->next)
    draw_bar(m);
}

void create_bars(void) {
  Monitor *monitor;
  XSetWindowAttributes wa = {.override_redirect = True,
                             .background_pixmap = ParentRelative,
                             .event_mask = ButtonPressMask | ExposureMask};
  XClassHint ch = {"pwindow_manager", "pwindow_manager"};
  for (monitor = monitors; monitor; monitor = monitor->next) {
    if (monitor->bar_window)
      continue;

    monitor->bar_window =
        XCreateWindow(display, root, monitor->window_area_x,
                      monitor->bar_geometry, monitor->window_area_width,
                      bar_height, 0, DefaultDepth(display, screen),
                      CopyFromParent, DefaultVisual(display, screen),
                      CWOverrideRedirect | CWBackPixmap | CWEventMask, &wa);

    XDefineCursor(display, monitor->bar_window, cursor[CurNormal]->cursor);
    XMapRaised(display, monitor->bar_window);
    XSetClassHint(display, monitor->bar_window, &ch);
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
