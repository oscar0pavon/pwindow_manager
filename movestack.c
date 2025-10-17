void
movestack(const Arg *arg) {
	Client *c = NULL, *p = NULL, *pc = NULL, *i;

	if(arg->i > 0) {
		/* find the client after selmon->sel */
		for(c = selected_monitor->sel->next; c && (!ISVISIBLE(c) || c->isfloating); c = c->next);
		if(!c)
			for(c = selected_monitor->clients; c && (!ISVISIBLE(c) || c->isfloating); c = c->next);

	}
	else {
		/* find the client before selmon->sel */
		for(i = selected_monitor->clients; i != selected_monitor->sel; i = i->next)
			if(ISVISIBLE(i) && !i->isfloating)
				c = i;
		if(!c)
			for(; i; i = i->next)
				if(ISVISIBLE(i) && !i->isfloating)
					c = i;
	}
	/* find the client before selmon->sel and c */
	for(i = selected_monitor->clients; i && (!p || !pc); i = i->next) {
		if(i->next == selected_monitor->sel)
			p = i;
		if(i->next == c)
			pc = i;
	}

	/* swap c and selmon->sel selmon->clients in the selmon->clients list */
	if(c && c != selected_monitor->sel) {
		Client *temp = selected_monitor->sel->next==c?selected_monitor->sel:selected_monitor->sel->next;
		selected_monitor->sel->next = c->next==selected_monitor->sel?c:c->next;
		c->next = temp;

		if(p && p != c)
			p->next = c;
		if(pc && pc != selected_monitor->sel)
			pc->next = selected_monitor->sel;

		if(selected_monitor->sel == selected_monitor->clients)
			selected_monitor->clients = c;
		else if(c == selected_monitor->clients)
			selected_monitor->clients = selected_monitor->sel;

		arrange(selected_monitor);
	}
}
