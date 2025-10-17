#ifndef EVENTS_H
#define EVENTS_H

#include <X11/Xlib.h>

void clientmessage(XEvent *e);

void configurenotify(XEvent *e);
void configurerequest(XEvent *e);
void destroynotify(XEvent *e);
void enternotify(XEvent *e);
void expose(XEvent *e);
void focusin(XEvent *e);

#endif
