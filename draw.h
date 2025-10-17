/* See LICENSE file for copyright and license details. */
#ifndef DRW_H
#define DRW_H

#include <X11/Xlib.h>
#include <X11/X.h>
#include <X11/Xft/Xft.h>

typedef struct {
	Cursor cursor;
} Cur;

typedef struct Fnt {
	Display *dpy;
	unsigned int h;
	XftFont *xfont;
	FcPattern *pattern;
	struct Fnt *next;
} Fnt;

enum { ColFg, ColBg, ColBorder }; /* Clr scheme index */
typedef XftColor Color;

typedef struct {
	unsigned int w, h;
	Display *dpy;
	int screen;
	Window root;
	Drawable drawable;
	GC gc;
	Color *scheme;
	Fnt *fonts;
} Draw;

extern Draw *drw;

/* Drawable abstraction */
Draw *drw_create(Display *dpy, int screen, Window win, unsigned int w, unsigned int h);
void drw_resize(Draw *drw, unsigned int w, unsigned int h);
void drw_free(Draw *drw);

/* Fnt abstraction */
Fnt *drw_fontset_create(Draw* drw, const char *fonts[], size_t fontcount);
void drw_fontset_free(Fnt* set);
unsigned int drw_fontset_getwidth(Draw *drw, const char *text);
unsigned int drw_fontset_getwidth_clamp(Draw *drw, const char *text, unsigned int n);
void drw_font_getexts(Fnt *font, const char *text, unsigned int len, unsigned int *w, unsigned int *h);

/* Colorscheme abstraction */
void drw_clr_create(Draw *drw, Color *dest, const char *clrname);
Color *drw_scm_create(Draw *drw, const char *clrnames[], size_t clrcount);

/* Cursor abstraction */
Cur *drw_cur_create(Draw *drw, int shape);
void drw_cur_free(Draw *drw, Cur *cursor);

/* Drawing context manipulation */
void drw_setfontset(Draw *drw, Fnt *set);
void drw_setscheme(Draw *drw, Color *scm);

/* Drawing functions */
void drw_rect(Draw *drw, int x, int y, unsigned int w, unsigned int h, int filled, int invert);
int drw_text(Draw *drw, int x, int y, unsigned int w, unsigned int h, unsigned int lpad, const char *text, int invert);

/* Map functions */
void drw_map(Draw *drw, Window win, int x, int y, unsigned int w, unsigned int h);

#endif
