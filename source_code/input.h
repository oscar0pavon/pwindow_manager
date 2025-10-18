#ifndef INPUT_H
#define INPUT_H

#include <X11/Xlib.h>
#include "types.h"
#include "draw.h"

extern void (*handler[LASTEvent]) (XEvent *);

extern Cur *cursor[CurLast];

void move_mouse(const Arg *arg);

void resize_with_mouse(const Arg *arg);

#endif
