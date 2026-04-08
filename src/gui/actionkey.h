#pragma once
// action keys: simple press-to-act hotkeys with view-dependent behaviour.
// config files live in bin/actionkeys/ and ~/.config/vkdt/actionkeys/.
// format:
//   # comment
//   key:<keyname>
//   darkroom:<action>
//   lighttable:<action>
//   files:<action>
//   nodes:<action>
//
// supported actions:
//   leave  — go to parent view (darkroom→lighttable, lighttable→quit, etc.)
//   quit   — close the application

#include "core/log.h"
#include "gui/gui.h"
#include "gui/view.h"
#include "gui/render.h"
#include "gui/hotkey.h"
#include "pipe/res.h"

#include <GLFW/glfw3.h>
#include <dirent.h>
#include <limits.h>
#include <string.h>

#define DT_ACTIONKEY_MAX 16

typedef enum dt_actionkey_action_t
{
  s_action_none  = 0,
  s_action_leave,
  s_action_quit,
}
dt_actionkey_action_t;

typedef struct dt_actionkey_t
{
  int    key;                                // GLFW key code
  char   name[64];                           // display name (from filename)
  dt_actionkey_action_t action[s_view_cnt];  // per-view action
}
dt_actionkey_t;

typedef struct dt_actionkeys_t
{
  dt_actionkey_t ak[DT_ACTIONKEY_MAX];
  int cnt;
  int inited;
}
dt_actionkeys_t;

static inline dt_actionkey_action_t
dt_actionkey_parse_action(const char *s)
{
  if(!strcmp(s, "leave")) return s_action_leave;
  if(!strcmp(s, "quit"))  return s_action_quit;
  return s_action_none;
}

static inline int
dt_actionkey_parse_view(const char *s)
{
  if(!strncmp(s, "darkroom:",   9)) return s_view_darkroom;
  if(!strncmp(s, "lighttable:", 11)) return s_view_lighttable;
  if(!strncmp(s, "files:",      6)) return s_view_files;
  if(!strncmp(s, "nodes:",      6)) return s_view_nodes;
  return -1;
}

static inline int
dt_actionkey_load_file(dt_actionkey_t *ak, const char *path, const char *basename)
{
  FILE *f = fopen(path, "rb");
  if(!f) return 1;
  memset(ak, 0, sizeof(*ak));
  snprintf(ak->name, sizeof(ak->name), "%s", basename);
  char line[256];
  while(fgets(line, sizeof(line), f))
  {
    char *nl = strchr(line, '\n');
    if(nl) *nl = 0;
    if(line[0] == '#' || line[0] == 0) continue;
    if(!strncmp(line, "key:", 4))
    {
      ak->key = hk_name_to_glfw(line + 4);
    }
    else
    {
      int view = dt_actionkey_parse_view(line);
      if(view < 0) continue;
      const char *colon = strchr(line, ':');
      if(!colon) continue;
      ak->action[view] = dt_actionkey_parse_action(colon + 1);
    }
  }
  fclose(f);
  return ak->key == 0 ? 1 : 0;
}

static inline void
dt_actionkeys_init(dt_actionkeys_t *ak)
{
  memset(ak, 0, sizeof(*ak));
  for(int inbase = 0; inbase < 2; inbase++)
  {
    void *dirp = dt_res_opendir("actionkeys", inbase);
    if(!dirp) continue;
    const char *basename = 0;
    while((basename = dt_res_next_basename(dirp, inbase)))
    {
      if(basename[0] == '.') continue;
      if(ak->cnt >= DT_ACTIONKEY_MAX) break;
      int dup = 0;
      for(int i = 0; i < ak->cnt; i++)
        if(!strcmp(ak->ak[i].name, basename)) { dup = 1; break; }
      if(dup) continue;
      char path[PATH_MAX];
      snprintf(path, sizeof(path), "%s/actionkeys/%s",
          inbase ? dt_pipe.basedir : dt_pipe.homedir, basename);
      if(!dt_actionkey_load_file(ak->ak + ak->cnt, path, basename))
      {
        ak->cnt++;
      }
    }
    dt_res_closedir(dirp, inbase);
  }
}

static inline void
dt_actionkeys_cleanup(dt_actionkeys_t *ak)
{
  ak->cnt = 0;
}

// called from dt_view_keyboard. returns 1 if event was consumed.
static inline int
dt_actionkey_keyboard(dt_actionkeys_t *ak, int key, int action)
{
  if(!ak->inited) { dt_actionkeys_init(ak); ak->inited = 1; }
  if(action != GLFW_PRESS) return 0;
  if(dt_gui_input_blocked()) return 0;
  if(vkdt.wstate.popup) return 0;
  for(int i = 0; i < ak->cnt; i++)
  {
    if(ak->ak[i].key != key) continue;
    dt_actionkey_action_t act = ak->ak[i].action[vkdt.view_mode];
    switch(act)
    {
      case s_action_leave:
        if(vkdt.view_mode == s_view_darkroom)
          dt_view_switch(s_view_lighttable);
        else if(vkdt.view_mode == s_view_lighttable)
          glfwSetWindowShouldClose(vkdt.win.window, GLFW_TRUE);
        else if(vkdt.view_mode == s_view_nodes)
          dt_view_switch(s_view_darkroom);
        else if(vkdt.view_mode == s_view_files)
          dt_view_switch(s_view_lighttable);
        return 1;
      case s_action_quit:
        glfwSetWindowShouldClose(vkdt.win.window, GLFW_TRUE);
        return 1;
      default:
        return 0;
    }
  }
  return 0;
}
