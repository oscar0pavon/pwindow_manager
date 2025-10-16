#ifndef WINDOWS_H
#define WINDOWS_H

#include "types.h"

void setfocus(Client *c);

void unfocus(Client *c, int setfocus);

void focus(Client *c);

#endif
