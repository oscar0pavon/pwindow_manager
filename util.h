/* See LICENSE file for copyright and license details. */

#ifndef UTIL_H
#define UTIL_H

#include "pwindow_manager.h"

void die(const char *fmt, ...);
void *ecalloc(size_t nmemb, size_t size);

extern int (*xerrorxlib)(Display *, XErrorEvent *);

#endif
