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

Monitor *createmon(void);
Monitor *dirtomon(int dir);
Monitor *numtomon(int num);
void focusnthmon(const Arg *arg);
void tagnthmon(const Arg *arg);
void drawbar(Monitor *m);

void sendmon(Client *c, Monitor *m);

void focusmon(const Arg *arg);
int updategeom(void);



extern Monitor *monitors, *selected_monitor;


#endif
