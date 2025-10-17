#ifndef _PWINDOW_MANAGER_H
#define _PWINDOW_MANAGER_H


#include <X11/X.h>
#include <X11/Xlib.h>
#include "types.h"
#include "monitors.h"

#include "draw.h"


extern Atom wmatom[WMLast], netatom[NetLast];
extern Display *dpy;
extern Window root, wmcheckwin;
extern Color **scheme;

/* function declarations */
void applyrules(Client *c);
int applysizehints(Client *c, int *x, int *y, int *w, int *h, int interact);
void arrange(Monitor *m);
void arrangemon(Monitor *m);

void buttonpress(XEvent *e);
void checkotherwm(void);
void cleanup(void);
void cleanupmon(Monitor *mon);
void configure(Client *c);


void movestack(const Arg *arg);


void window_to_monitor_and_focus(const Arg *arg);
void window_to_monitor(const Arg *arg);
void focus_monitor(const Arg *arg);

void move_godot_to_monitor(const Arg *arg);
void send_to_monitor(Client *window, Monitor *monitor);
void set_window_floating(Client* window, Monitor* monitor);
void set_window_dimention(Client* window, Monitor* monitor, int height, int width);
void full_screen_floating_window(const Arg* arg);
void minimal_screen_floating_window(const Arg* arg);

void drawbars(void);

void focusstack(const Arg *arg);
Atom getatomprop(Client *c, Atom prop);
int getrootptr(int *x, int *y);
long getstate(Window w);
int gettextprop(Window w, Atom atom, char *text, unsigned int size);

void grabbuttons(Client *c, int focused);
void grabkeys(void);
void incnmaster(const Arg *arg);
void keypress(XEvent *e);
void killclient(const Arg *arg);
void manage(Window w, XWindowAttributes *wa);
void mappingnotify(XEvent *e);
void maprequest(XEvent *e);
void monocle(Monitor *m);
void motionnotify(XEvent *e);
void movemouse(const Arg *arg);

void movetoedge(const Arg *arg);
void moveresize(const Arg *arg);
void moveresizewebcam(const Arg *arg);

Client *prevtiled(Client *c);
Client *nexttiled(Client *c);

void pop(Client *c);
void propertynotify(XEvent *e);
void quit(const Arg *arg);
Monitor *recttomon(int x, int y, int w, int h);

void pushdown(const Arg *arg);
void pushup(const Arg *arg);

void resize(Client *c, int x, int y, int w, int h, int interact);
void resizeclient(Client *c, int x, int y, int w, int h);
void resizemouse(const Arg *arg);
void restack(Monitor *m);
void run(void);
void scan(void);
int sendevent(Client *c, Atom proto);

void setclientstate(Client *c, long state);
void setfullscreen(Client *c, int fullscreen);
void setlayout(const Arg *arg);
void setmfact(const Arg *arg);
void setup(void);


void seturgent(Client *c, int urg);

void spawn(const Arg *arg);
void tag(const Arg *arg);
void tagmon(const Arg *arg);
void tile(Monitor *m);
void togglebar(const Arg *arg);
void togglefloating(const Arg *arg);
void toggletag(const Arg *arg);
void toggleview(const Arg *arg);
void ntoggleview(const Arg *arg);
void nview(const Arg *arg);
void unmanage(Client *c, int destroyed);
void unmapnotify(XEvent *e);
void updatebarpos(Monitor *m);
void updatebars(void);
void updateclientlist(void);

void updatenumlockmask(void);
void updatesizehints(Client *c);
void updatestatus(void);
void updatetitle(Client *c);
void updatewindowtype(Client *c);
void updatewmhints(Client *c);
void view(const Arg *arg);
Client *wintoclient(Window w);
Monitor *wintomon(Window w);
int xerror(Display *dpy, XErrorEvent *ee);
int xerrordummy(Display *dpy, XErrorEvent *ee);
int xerrorstart(Display *dpy, XErrorEvent *ee);
void zoom(const Arg *arg);
void autostart_exec(void);

void reset_view(const Arg *arg);

/* variables */
extern int screen;
extern int display_width, display_height;           /* X display screen geometry width, height */
extern int bar_height;               /* bar height */
extern int lrpad;            /* sum of left and right padding for text */

extern char stext[256];

extern unsigned int numlockmask;

#endif
