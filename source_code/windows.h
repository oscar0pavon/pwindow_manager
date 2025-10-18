#ifndef WINDOWS_H
#define WINDOWS_H

#include "types.h"

void setfocus(Client *c);

void unfocus(Client *c, int setfocus);

void focus(Client *c);

void detach(Client *c);

void detachstack(Client *c);

void attach(Client *c);

void attachstack(Client *c);

void showhide(Client *c);

void scan_windows(void);

void window_to_monitor_and_focus(const Arg *arg);

void window_to_monitor(const Arg *arg);

extern const char broken[];

#endif
