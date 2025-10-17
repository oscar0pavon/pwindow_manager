#ifndef INPUT_H
#define INPUT_H

#include <X11/Xlib.h>

extern void (*handler[LASTEvent]) (XEvent *);

#endif
