#ifndef MONITORS_H
#define MONITORS_H

#include "types.h"

typedef struct Monitor Monitor;

struct Monitor {
	char ltsymbol[16];
	char monmark[16];
	float mfact;
	int nmaster;
	int num;
	int bar_geometry;               /* bar geometry */
	int screen_x, screen_y, screen_width, screen_height;   /* screen size */
	int window_area_x, window_area_y, window_area_width, window_area_height;   /* window area  */
	unsigned int seltags;
	unsigned int sellt;
	unsigned int tagset[2];
	int showbar;
	int topbar;
	Client *clients;
	Client *selected_client;
	Client *stack;
	Monitor *next;
	Window bar_window;
	const Layout *lt[2];
};

Monitor *createmon(void);
Monitor *dirtomon(int dir);
Monitor *numtomon(int num);
void focusnthmon(const Arg *arg);
void tagnthmon(const Arg *arg);
void draw_bar(Monitor *m);

void sendmon(Client *c, Monitor *m);

void focus_monitor(const Arg *arg);
int updategeom(void);



extern Monitor *monitors, *selected_monitor;


#endif
