/* See LICENSE file for copyright and license details. */

/* appearance */
#include <X11/X.h>
#include <X11/Xutil.h>

#include "pwindow_manager.h"

static const unsigned int borderpx  = 3;        /* border pixel of windows */
static const unsigned int snap      = 32;       /* snap pixel */
static const int showbar            = 1;        /* 0 means no bar */
static const int topbar             = 1;        /* 0 means bottom bar */
static const char *fonts[]          = { "monospace:size=13" };
static const char dmenufont[]       = "monospace:size=13";
static const char col_gray1[]       = "#282C34";
static const char col_gray2[]       = "#444444";
static const char col_gray3[]       = "#bbbbbb";
static const char col_gray4[]       = "#eeeeee";
static const char col_cyan[]        = "#273b6b";
static const char col_bg[]					= "#3e4452";
static const char *colors[][3]      = {
	/*               fg         bg         border   */
	[SchemeNorm] = { col_gray3, col_gray1, col_gray2 },
	[SchemeSel]  = { col_gray4, col_bg,  col_cyan  },
};

/* tagging */
static const char *tags[] = { "1", "2", "3", "4", "5", "6", "7", "8", "9" };

static const Rule rules[] = {
	/* xprop(1):
	 *	WM_CLASS(STRING) = instance, class
	 *	WM_NAME(STRING) = title
	 */
	/* class      instance    title       tags mask     isfloating   monitor */
	{ "Gimp",     NULL,       NULL,       0,            1,           -1 },
	{ NULL,				NULL,       "pengine",       0,            1,           -1 },
	{ "prufus",   NULL,       "prufus",       0,            1,           -1 },
	{ "XVkbd",    NULL,       "xvkbd - Virtual Keyboard",       0,            1,           -1 },
//{ "Firefox",  NULL,       NULL,       1 << 8,       0,           -1 },
	//{ "ffplay",		NULL,       "WebCam",    0,       1,           -1 },
	//{ "st-256color",		"st-256color",       "st",    0,       1,           1 },
	//{ "Qemu",		NULL,       "QEMU",    0,       1,           -1 }
	{ "Godot",     "Godot_Engine",       "basket (DEBUG)",       0,            1,           0 },
	{ "basket",     "Godot_Engine",       "basket (DEBUG)",       0,            1,           0 },
};

/* layout(s) */
static const float mfact     = 0.5; /* factor of master area size [0.05..0.95] */
static const int nmaster     = 1;    /* number of clients in master area */
static const int nviews      = 3;    /* mask of tags highlighted by default (tags 1-4) */
static const int resizehints = 0;    /* 1 means respect size hints in tiled resizals */
static const int lockfullscreen = 1; /* 1 will force focus on the fullscreen window */


static const float facts[1];    //static const float facts[]     = {     0,     0.5 }; // = mfact   // 50%
static const int masters[1];    //static const int masters[]     = {     0,      -1 }; // = nmaster // vertical stacking (for rotated monitor)
static const int views[1];      //static const int views[]       = {     0,      ~0 }; // = nviews  // all tags
/* invert tags after nviews */  /* array dimentions can both be as big as needed */  // final highlighted tags
static const int toggles[1][1]; //static const int toggles[2][2] = { {0,8}, {~0,~0} }; // 2-4+9     // all (leave as views above)
static const int toggles[1][1] = {{~0}};

static const Layout layouts[] = {
	/* symbol     arrange function */
	{ "[]=",      tile },    /* first entry is default */
	{ "><>",      NULL },    /* no layout function means floating behavior */
	{ "[M]",      monocle },
};

/* key definitions */
#define MODKEY Mod4Mask
#define WINKEY Mod1Mask
#define ALTMOD Mod1Mask
#define TAGKEYS(KEY,TAG) \
	{ MODKEY,                       KEY,      view,           {.ui = 1 << TAG} }, \
	{ MODKEY|ControlMask,           KEY,      toggleview,     {.ui = 1 << TAG} }, \
	{ MODKEY|WINKEY,                KEY,      nview,          {.ui = 1 << TAG} }, \
	{ MODKEY|WINKEY|ControlMask,    KEY,      ntoggleview,    {.ui = 1 << TAG} }, \
	{ MODKEY|ShiftMask,             KEY,      tag,            {.ui = 1 << TAG} }, \
	{ MODKEY|ControlMask|ShiftMask, KEY,      toggletag,      {.ui = 1 << TAG} }, 

/* helper for spawning shell commands in the pre dwm-5.0 fashion */
#define SHCMD(cmd) { .v = (const char*[]){ "/bin/sh", "-c", cmd, NULL } }

/* commands */
static char dmenumon[2] = "0"; /* component of dmenucmd, manipulated in spawn() */
static const char *dmenucmd[] = { "dmenu_run", "-m", dmenumon, "-fn", dmenufont, "-nb", col_gray1, "-nf", col_gray3, "-sb", col_cyan, "-sf", col_gray4, NULL };
static const char *terminal[]  = { "pterminal", NULL };
static const char *reboot[]  = { "/usr/sbin/reboot", NULL };
static const char *mute_mic[]  = { "toggle_mic", NULL };
static const char *screenshot[]  = { "scrot", NULL };
static const char *screenshot_select[]  = { "scrot", "-s", NULL };
static const char *volume_up[]  = { "volume_up", NULL };
static const char *volume_down[]  = { "volume_down", NULL };

#include "movestack.c"

static const Key keys[] = {
	/* modifier                     key        function        argument */
	{ MODKEY,                       XK_d,      spawn,          {.v = dmenucmd } },
	{ MODKEY|ShiftMask,			XK_w,	moveresizewebcam,		{.v = (int []){ 0, 0, -320, -240 }}},
	{ MODKEY,												XK_Return, spawn,          {.v = terminal} },
	{ MODKEY,												XK_Delete, spawn,          {.v = reboot } },
	{ MODKEY|ShiftMask,							XK_a,			spawn,          {.v = mute_mic} },
	{ MODKEY|ShiftMask,							XK_s,			spawn,          {.v = screenshot} },
	{ MODKEY|ControlMask,							XK_s,			spawn,          {.v = screenshot_select} },
	{ MODKEY|ShiftMask,							XK_m,			spawn,          {.v = volume_up} },
	{ MODKEY|ShiftMask,							XK_n,			spawn,          {.v = volume_down} },
	{ MODKEY,                       XK_b,      togglebar,      {0} },
	{ MODKEY,                       XK_j,      focusstack,     {.i = +1 } },
	{ MODKEY,                       XK_k,      focusstack,     {.i = -1 } },
	{ MODKEY|ShiftMask,             XK_j,      pushdown,       {0} },
	{ MODKEY|ShiftMask,             XK_k,      pushup,         {0} },
//	{ MODKEY|ShiftMask,             XK_j,      movestack,      {.i = +1 } },
//	{ MODKEY|ShiftMask,             XK_k,      movestack,      {.i = -1 } },
	{ MODKEY,                       XK_i,      incnmaster,     {.i = +1 } },
	{ MODKEY,                       XK_o,      incnmaster,     {.i = -1 } },
	{ MODKEY,                       XK_h,      setmfact,       {.f = -0.05} },
	{ MODKEY,                       XK_l,      setmfact,       {.f = +0.05} },
//{ MODKEY,                       XK_Return, zoom,           {0} },
	{ MODKEY,                       XK_Tab,    view,           {0} },
	{ MODKEY|ShiftMask,             XK_q,      killclient,     {0} },
	{ MODKEY,                       XK_t,      setlayout,      {.v = &layouts[0]} },
	{ MODKEY,                       XK_f,      setlayout,      {.v = &layouts[1]} },
	{ MODKEY,                       XK_m,      setlayout,      {.v = &layouts[2]} },
	{ MODKEY,                       XK_space,  setlayout,      {0} },
	{ MODKEY|ShiftMask,             XK_space,  togglefloating, {0} },
	{ MODKEY,                       XK_0,      view,           {.ui = ~0 } },
	{ MODKEY|ShiftMask,             XK_0,      tag,            {.ui = ~0 } },
	{ MODKEY,                       XK_comma,  focusmon,       {.i = -1 } },
	{ MODKEY,                       XK_period, focusmon,       {.i = +1 } },
	{ MODKEY|ShiftMask,             XK_comma,  tagmon,         {.i = -1 } },
	{ MODKEY|ShiftMask,             XK_period, tagmon,         {.i = +1 } },
	TAGKEYS(                        XK_1,                      0)
	TAGKEYS(                        XK_2,                      1)
	TAGKEYS(                        XK_3,                      2)
	TAGKEYS(                        XK_4,                      3)
	TAGKEYS(                        XK_5,                      4)
	TAGKEYS(                        XK_6,                      5)
	TAGKEYS(                        XK_7,                      6)
	TAGKEYS(                        XK_8,                      7)
	TAGKEYS(                        XK_9,                      8)
	{ MODKEY,                       XK_grave,  reset_view,     {0} },
	{ MODKEY|ShiftMask,             XK_e,      quit,           {0} },
	{ALTMOD,												XK_j,		focus_monitor ,			 {.i = +1}}, 
	{ALTMOD,												XK_k,		focus_monitor , {.i = -1}}, 
	{ALTMOD,												XK_h,		window_to_monitor_and_focus,			 {.i = 0}}, 
	{ALTMOD,												XK_l,		window_to_monitor_and_focus,			 {.i = 1}}, 
	{ALTMOD|ShiftMask,												XK_h,		window_to_monitor,			 {.i = 0}}, 
	{ALTMOD|ShiftMask,												XK_l,		window_to_monitor,			 {.i = 1}}, 
	{MODKEY,             XK_r,      move_godot_to_monitor,           {0} },
};

/* button definitions */
/* click can be ClkTagBar, ClkLtSymbol, ClkStatusText, ClkWinTitle, ClkClientWin, or ClkRootWin */
static const Button buttons[] = {
	/* click                event mask      button          function        argument */
	{ ClkLtSymbol,          0,              Button1,        setlayout,      {0} },
	{ ClkLtSymbol,          0,              Button3,        setlayout,      {.v = &layouts[2]} },
  { ClkMonNum,            0,              Button1,        focusmon,       {.i = +1} },
  { ClkMonNum,            0,              Button3,        focusmon,       {.i = -1} },
	{ ClkMonNum,            0,              Button2,        reset_view,     {0} },
	{ ClkWinTitle,          0,              Button2,        zoom,           {0} },
	{ ClkStatusText,        0,              Button2,        spawn,          {.v = terminal} },
	{ ClkClientWin,         MODKEY,         Button1,        movemouse,      {0} },
	{ ClkClientWin,         MODKEY,					Button2,        togglefloating, {0} },
	{ ClkClientWin,         MODKEY,         Button3,        resizemouse,    {0} },
	{ ClkTagBar,            0,              Button1,        view,           {0} },
	{ ClkTagBar,            0,              Button3,        toggleview,     {0} },
	{ ClkTagBar,            MODKEY|WINKEY,  Button1,        nview,          {0} },
	{ ClkTagBar,            MODKEY|WINKEY,  Button3,        ntoggleview,    {0} },
	{ ClkTagBar,            MODKEY,         Button1,        tag,            {0} },
	{ ClkTagBar,            MODKEY,         Button3,        toggletag,      {0} },
};
static const char *const autostart[] = {
	"pstatus_bar", NULL,
	"set_background",NULL, 
	"pulseaudio",NULL, 
	//"bluetooth_audio",NULL,
	"volume",NULL,
	//"monitors",NULL,
	"pterminal",NULL,
	"pterminal",NULL,
	//"firefox", NULL,
//	"pulse_loop",NULL, 
	//"hsetroot", "-center", "/usr/home/bit6tream/pic/wallapper.png", NULL,
	//"xrdb", "/usr/home/bit6tream/.config/X/Xresources", NULL,
	//"sh", "-c", "while :; do dwmstatus.sh -; sleep 60; done", NULL,
	//"sh", "-c", "while :; do /root/pulse.sh -; sleep 5; done", NULL,
	//"dunst", NULL,
	//"picom", NULL,
	NULL
};
