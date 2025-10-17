
bool game_editor_moved = false;
Client *game_editor_window = NULL;
Monitor *target_monitor = NULL;

bool is_playing_basket(const char *instance, const char *class, char *name) {

  if (strcmp(instance, "Godot_Engine") == 0 && strcmp(class, "basket") == 0 &&
      strcmp(name, "basket (DEBUG)") == 0) {
    return true;
  }

  return false;
}

bool is_playing_race(const char *instance, const char *class, char *name) {

  if (strcmp(instance, "Godot_Engine") == 0 &&
      strcmp(class, "speed_mostsimple") == 0 &&
      strcmp(name, "speed_mostsimple (DEBUG)") == 0) {
    return true;
  }

  return false;
}

void move_godot_to_monitor(const Arg *arg) {
  Client *window;
  const char *class, *instance;
  XClassHint ch = {NULL, NULL};

  if (target_monitor != NULL) {
    Client *target_window = NULL;
    bool found_game_window = false;
    for (target_window = target_monitor->clients;
         target_window && ISVISIBLE(target_window);
         target_window = target_window->next) {
      XGetClassHint(dpy, target_window->win, &ch);
      class = ch.res_class ? ch.res_class : broken;
      instance = ch.res_name ? ch.res_name : broken;
      if (is_playing_race(instance, class, target_window->name)) {
        found_game_window = true;
        return;
      }
    }
    if (!found_game_window) {
      target_monitor = false;
      game_editor_moved = false;
    }
  }

  // move window to the next monitor
  for (window = selected_monitor->clients; window && ISVISIBLE(window);
       window = window->next) {
    XGetClassHint(dpy, window->win, &ch);
    class = ch.res_class ? ch.res_class : broken;
    instance = ch.res_name ? ch.res_name : broken;
    if (is_playing_race(instance, class, window->name)) {
      if (game_editor_moved == false) {
        window->isfloating = true;
        target_monitor = dirtomon(-1);
        // send_to_monitor(window, target_monitor);
        // set_window_floating(window,target_monitor);
        // set_window_dimention(window,target_monitor,1920,1080);
        // set_window_floating(window,selmon);
        // set_window_dimention(window,selmon,1280,720);
        game_editor_window = window;
        game_editor_moved = true;
      }
    }
  }
}
