#ifndef _PWINDOW_MANAGER_H
#define _PWINDOW_MANAGER_H


#include <X11/X.h>
#include <X11/Xlib.h>
#include "types.h"
#include "monitors.h"

#include "drw.h"


extern Atom wmatom[WMLast], netatom[NetLast];
extern Display *dpy;
extern Window root, wmcheckwin;
extern Clr **scheme;

/* function declarations */
static void applyrules(Client *c);
static int applysizehints(Client *c, int *x, int *y, int *w, int *h, int interact);
static void arrange(Monitor *m);
static void arrangemon(Monitor *m);
static void attach(Client *c);

void attachstack(Client *c);
static void buttonpress(XEvent *e);
static void checkotherwm(void);
static void cleanup(void);
static void cleanupmon(Monitor *mon);
static void clientmessage(XEvent *e);
static void configure(Client *c);
static void configurenotify(XEvent *e);
static void configurerequest(XEvent *e);
static void destroynotify(XEvent *e);
static void detach(Client *c);

void detachstack(Client *c);



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

static void enternotify(XEvent *e);
static void expose(XEvent *e);
static void focusin(XEvent *e);
static void focusmon(const Arg *arg);
static void focusstack(const Arg *arg);
static Atom getatomprop(Client *c, Atom prop);
static int getrootptr(int *x, int *y);
static long getstate(Window w);
static int gettextprop(Window w, Atom atom, char *text, unsigned int size);

void grabbuttons(Client *c, int focused);
static void grabkeys(void);
static void incnmaster(const Arg *arg);
static void keypress(XEvent *e);
static void killclient(const Arg *arg);
static void manage(Window w, XWindowAttributes *wa);
static void mappingnotify(XEvent *e);
static void maprequest(XEvent *e);
static void monocle(Monitor *m);
static void motionnotify(XEvent *e);
static void movemouse(const Arg *arg);

static void movetoedge(const Arg *arg);
static void moveresize(const Arg *arg);
static void moveresizewebcam(const Arg *arg);

static Client *prevtiled(Client *c);
static Client *nexttiled(Client *c);

static void pop(Client *c);
static void propertynotify(XEvent *e);
static void quit(const Arg *arg);
static Monitor *recttomon(int x, int y, int w, int h);

static void pushdown(const Arg *arg);
static void pushup(const Arg *arg);

static void resize(Client *c, int x, int y, int w, int h, int interact);
static void resizeclient(Client *c, int x, int y, int w, int h);
static void resizemouse(const Arg *arg);
static void restack(Monitor *m);
static void run(void);
static void scan(void);
static int sendevent(Client *c, Atom proto);
static void sendmon(Client *c, Monitor *m);
static void setclientstate(Client *c, long state);
static void setfullscreen(Client *c, int fullscreen);
static void setlayout(const Arg *arg);
static void setmfact(const Arg *arg);
static void setup(void);


void seturgent(Client *c, int urg);

static void showhide(Client *c);
static void spawn(const Arg *arg);
static void tag(const Arg *arg);
static void tagmon(const Arg *arg);
static void tile(Monitor *m);
static void togglebar(const Arg *arg);
static void togglefloating(const Arg *arg);
static void toggletag(const Arg *arg);
static void toggleview(const Arg *arg);
static void ntoggleview(const Arg *arg);
static void nview(const Arg *arg);
static void unmanage(Client *c, int destroyed);
static void unmapnotify(XEvent *e);
static void updatebarpos(Monitor *m);
static void updatebars(void);
static void updateclientlist(void);
static int updategeom(void);
static void updatenumlockmask(void);
static void updatesizehints(Client *c);
static void updatestatus(void);
static void updatetitle(Client *c);
static void updatewindowtype(Client *c);
static void updatewmhints(Client *c);
static void view(const Arg *arg);
static Client *wintoclient(Window w);
static Monitor *wintomon(Window w);
static int xerror(Display *dpy, XErrorEvent *ee);
static int xerrordummy(Display *dpy, XErrorEvent *ee);
static int xerrorstart(Display *dpy, XErrorEvent *ee);
static void zoom(const Arg *arg);
static void autostart_exec(void);

static void reset_view(const Arg *arg);

#endif
