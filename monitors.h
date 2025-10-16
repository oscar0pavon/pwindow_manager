#ifndef MONITORS_H
#define MONITORS_H

#include "types.h"

struct Monitor {
	char ltsymbol[16];
	char monmark[16];
	float mfact;
	int nmaster;
	int num;
	int by;               /* bar geometry */
	int mx, my, mw, mh;   /* screen size */
	int wx, wy, ww, wh;   /* window area  */
	unsigned int seltags;
	unsigned int sellt;
	unsigned int tagset[2];
	int showbar;
	int topbar;
	Client *clients;
	Client *sel;
	Client *stack;
	Monitor *next;
	Window barwin;
	const Layout *lt[2];
};

//Monitors
static Monitor *createmon(void);
Monitor *dirtomon(int dir);
static Monitor *numtomon(int num);
static void focusnthmon(const Arg *arg);
static void tagnthmon(const Arg *arg);
static void drawbar(Monitor *m);

static Monitor *mons, *selmon;

#endif
