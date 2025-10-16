#include "monitors.h"

#include "windows.h"
#include <stddef.h>

void
focusmon(const Arg *arg)
{
	Monitor *m;

	if (!mons->next)
		return;
	if ((m = dirtomon(arg->i)) == selmon)
		return;
	unfocus(selmon->sel, 0);
	selmon = m;
	focus(NULL);
}

Monitor *dirtomon(int dir) {
  Monitor *m = NULL;

  if (dir > 0) {
    if (!(m = selmon->next))
      m = mons;
  } else if (selmon == mons)
    for (m = mons; m->next; m = m->next)
      ;
  else
    for (m = mons; m->next != selmon; m = m->next)
      ;
  return m;
}


Monitor *numtomon(int num)
{
	Monitor *m = NULL;
	int i = 0;

	for(m = mons, i=0; m->next && i < num; m = m->next){
		i++;
	}
	return m;
}



void
focus_monitor(const Arg *arg)
{
	Monitor *m;

	if (!mons->next)
		return;

	if ((m = dirtomon(arg->i)) == selmon)
		return;
	unfocus(selmon->sel, 0);
	selmon = m;
	focus(NULL);
}

