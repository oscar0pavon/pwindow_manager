#ifndef INPUT_H
#define INPUT_H

#include <X11/Xlib.h>
#include "types.h"
#include "draw.h"

extern void (*handler[LASTEvent]) (XEvent *);

extern Cur *cursor[CurLast];

#endif
