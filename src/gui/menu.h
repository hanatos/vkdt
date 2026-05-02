#pragma once
// chord menu system: sequential hotkeys with popup overlays.
// press a leader key to open a submenu, then press a child key to
// execute an action or descend deeper.  menus are defined in config
// files in bin/menus/ and ~/.config/vkdt/menus/.
//
// config format (indentation = 2 spaces per level):
//   # comment
//   presets p
//     filmsim f
//     export e
//     *scan_presets
//   tools t
//     crop c
//     draw d
//
// *name = dynamic provider (filled at runtime)
// !keyaccel_name key = leaf referencing a keyaccel file

#include "core/log.h"
#include "gui/gui.h"
#include "gui/render.h"
#include "gui/hotkey.h"
#include "gui/dragkey.h"
#include "db/stringpool.h"
#include "pipe/res.h"
#include "pipe/modules/api.h"
#include "nk.h"

#include <string.h>
#include <ctype.h>
#include <limits.h>

#define DT_MENU_MAX_ENTRIES   256
#define DT_MENU_MAX_DYNAMIC   128 
#define DT_MENU_MAX_DEPTH       4
typedef enum { DT_MENU_SK_ENTRY=0, DT_MENU_SK_DYN_PARENT=1, DT_MENU_SK_GAMEPAD_ROOT=2 } dt_menu_sk_t;
typedef struct { int16_t idx; uint8_t kind; } dt_menu_stack_entry_t;
typedef struct dt_menu_entry_t
{
  uint16_t    key;            // GLFW key code for this entry
  int16_t     parent;         // index of parent, -1 = root level
  int16_t     first_child;    // index of first child, -1 = leaf
  int16_t     next_sibling;   // next entry at same level, -1 = last
  const char *label;          // display text (null-terminated); if NULL, check label_tkn
  dt_token_t  label_tkn;     // non-zero: display token as label
  const char *tooltip;        // hover tooltip, or NULL (from stringpool)
  int         hotkey_index;   // for leaves: index into view's hk[] array, -1 = provider action
  char        action_buf[64]; // for provider leaves: action string copy (stable storage)
  int       (*provider)(      // non-NULL: fills dynamic children into buf, returns count
    struct dt_menu_entry_t *buf, int max, void *data);
  void       *provider_data;
  void      (*on_open)(void *data); // called once when this entry's submenu is descended into
}
dt_menu_entry_t;

typedef struct dt_menu_t
{
  dt_menu_entry_t entries[DT_MENU_MAX_ENTRIES];
  dt_menu_entry_t dynamic[DT_MENU_MAX_DYNAMIC]; // buffer for provider results
  dt_menu_entry_t children[DT_MENU_MAX_DYNAMIC + DT_MENU_MAX_ENTRIES]; // scratch buffer for get_children
  dt_stringpool_t sp;
  int             cnt;        // number of static entries
  dt_menu_stack_entry_t stack[DT_MENU_MAX_DEPTH];
  int             depth;      // 0 = closed
  int             cursor;     // highlighted entry for gamepad/keyboard nav
  int             clicked;    // index of clicked entry this frame, -1 = none
  int             scroll_off; // scroll offset for long lists
  int             child_cnt;  // cached child count (valid when depth > 0)
  int             children_dirty; // set to re-scan providers
  dt_menu_entry_t dyn_parent; // copy of a dynamic entry used as submenu parent
  void          (*action_cb)(const char *action); // per-view action handler
  const char     *view_name;  // for logging
  int8_t          gamepad_button; // GLFW_GAMEPAD_BUTTON_* leader, or -1
}
dt_menu_t;


// all depth changes go through here to keep wstate in sync
static inline void
dt_menu_set_depth(dt_menu_t *m, int d)
{
  m->depth = d;
  m->children_dirty = 1;
  vkdt.wstate.chord_menu_active = d > 0;
  vkdt.wstate.chord_menu_scroll = 0.0f;
  if(d == 0) { vkdt.wstate.pending_modid = -1; m->dyn_parent.provider = 0; }
}

static inline int
dt_menu_ensure_children(dt_menu_t *m);

static inline const char*
dt_menu_glfw_to_name(uint16_t key)
{
  return hk_get_key_for_scancode(key)->lib;
}

static inline int
dt_menu_indent_level(const char *line)
{
  int n = 0;
  while(line[n] == ' ') n++;
  return n / 2;
}

static inline void
dt_menu_link_child(dt_menu_t *m, int parent_idx, int child_idx)
{
  m->entries[child_idx].parent = parent_idx;
  if(parent_idx < 0)
  {
    int prev = -1;
    for(int i = 0; i < m->cnt; i++)
      if(i != child_idx && m->entries[i].parent == -1) prev = i;
    if(prev >= 0)
      m->entries[prev].next_sibling = child_idx;
  }
  else
  {
    if(m->entries[parent_idx].first_child < 0)
      m->entries[parent_idx].first_child = child_idx;
    else
    {
      int s = m->entries[parent_idx].first_child;
      while(m->entries[s].next_sibling >= 0) s = m->entries[s].next_sibling;
      m->entries[s].next_sibling = child_idx;
    }
  }
}

// forward declarations for providers
static int dt_menu_prov_presets(dt_menu_entry_t *buf, int max, void *data);
static int dt_menu_prov_keyaccel(dt_menu_entry_t *buf, int max, void *data);
static int dt_menu_prov_modules(dt_menu_entry_t *buf, int max, void *data);
static int dt_menu_prov_params(dt_menu_entry_t *buf, int max, void *data);
static int dt_menu_prov_combo_opts(dt_menu_entry_t *buf, int max, void *data);

static inline void
dt_menu_resolve_provider(dt_menu_entry_t *e, const char *name)
{
  e->provider = 0;
  e->provider_data = 0;
  if(!strcmp(name, "scan_presets"))       e->provider = dt_menu_prov_presets;
  else if(!strcmp(name, "scan_keyaccel")) e->provider = dt_menu_prov_keyaccel;
  else if(!strcmp(name, "scan_modules"))  e->provider = dt_menu_prov_modules;
  else dt_log(s_log_gui, "[menu] unknown provider '%s'", name);
}

static inline int
dt_menu_load(dt_menu_t *m, const char *viewname)
{
  memset(m->entries, 0, sizeof(m->entries));
  memset(m->dynamic, 0, sizeof(m->dynamic));
  m->cnt = 0;

  m->cursor = 0;
  m->clicked = -1;
  m->view_name = viewname;
  m->gamepad_button = -1;
  dt_menu_set_depth(m, 0);
  for(int i = 0; i < DT_MENU_MAX_ENTRIES; i++)
  {
    m->entries[i].parent = -1;
    m->entries[i].first_child = -1;
    m->entries[i].next_sibling = -1;
    m->entries[i].hotkey_index = -1;
  }

  dt_stringpool_init(&m->sp, 2 * DT_MENU_MAX_ENTRIES, 30);

  char filename[PATH_MAX];
  snprintf(filename, sizeof(filename), "menus/%s.menu", viewname);
  FILE *f = dt_graph_open_resource(0, 0, filename, "rb");
  if(!f)
  {
    dt_log(s_log_gui, "[menu] no menu config for '%s'", viewname);
    return 1;
  }

  int parent_stack[DT_MENU_MAX_DEPTH + 1];
  for(int i = 0; i <= DT_MENU_MAX_DEPTH; i++) parent_stack[i] = -1;

  char line[256];
  while(fgets(line, sizeof(line), f))
  {
    char *nl = strchr(line, '\n');
    if(nl) *nl = 0;
    if(line[0] == '#' || line[0] == 0) continue;

    int level = dt_menu_indent_level(line);
    const char *p = line;
    while(*p == ' ') p++;
    if(!*p) continue;

    if(!strncmp(p, "gamepad:", 8))
    {
      m->gamepad_button = (int8_t)hk_gamepad_name_to_button(p + 8);
      continue;
    }

    if(m->cnt >= DT_MENU_MAX_ENTRIES) break;

    dt_menu_entry_t *e = m->entries + m->cnt;
    e->parent = -1;
    e->first_child = -1;
    e->next_sibling = -1;
    e->hotkey_index = -1;
    e->action_buf[0] = 0;
    e->provider = 0;
    e->provider_data = 0;
    e->on_open = 0;

    if(p[0] == '*')
    {
      p++;
      const char *dedup = 0;
      dt_stringpool_get(&m->sp, p, strlen(p), m->cnt + 1, &dedup);
      e->label = dedup;
      e->key = 0;
      dt_menu_resolve_provider(e, p);
      dt_menu_link_child(m, level > 0 ? parent_stack[level - 1] : -1, m->cnt);
      m->cnt++;
    }
    else if(p[0] == '!')
    { // !name: keyaccel file or preset leaf
      p++;
      char acname[64] = {0}, keyname[32] = {0};
      const char *last_space = strrchr(p, ' ');
      if(last_space && last_space[1])
      {
        snprintf(keyname, sizeof(keyname), "%s", last_space + 1);
        int len = (int)(last_space - p);
        if(len > 0 && (size_t)len < sizeof(acname))
        { memcpy(acname, p, len); acname[len] = 0; }
      }
      else snprintf(acname, sizeof(acname), "%s", p);

      char fname[PATH_MAX], comment[128] = {0};
      snprintf(fname, sizeof(fname), "keyaccel/%s", acname);
      FILE *kf = dt_graph_open_resource(0, 0, fname, "rb");
      int is_preset = 0;
      if(kf) { fscanf(kf, "# %127[^\n]", comment); fclose(kf);
               snprintf(e->action_buf, sizeof(e->action_buf), "%s", acname); }
      else
      { // no keyaccel: treat as preset, action_cb routes on .pst suffix
        char pst_fname[PATH_MAX];
        snprintf(pst_fname, sizeof(pst_fname), "presets/%s.pst", acname);
        FILE *pf = dt_graph_open_resource(0, 0, pst_fname, "rb");
        if(pf) { fscanf(pf, "# %127[^\n]", comment); fclose(pf); }
        snprintf(e->action_buf, sizeof(e->action_buf), "%s.pst", acname);
        is_preset = 1;
      }

      const char *dedup = 0;
      if(is_preset)
      { // label = short filename; tooltip = first-line comment if any
        dt_stringpool_get(&m->sp, acname, strlen(acname), m->cnt + 1, &dedup);
        if(comment[0])
        {
          const char *tip = 0;
          dt_stringpool_get(&m->sp, comment, strlen(comment), m->cnt, &tip);
          e->tooltip = tip;
        }
      }
      else
      { // keyaccel: label = comment (or acname fallback), no tooltip
        if(!comment[0]) snprintf(comment, sizeof(comment), "%s", acname);
        dt_stringpool_get(&m->sp, comment, strlen(comment), m->cnt + 1, &dedup);
      }
      e->label = dedup;
      e->key = keyname[0] ? hk_name_to_glfw(keyname) : 0;

      int parent = level > 0 ? parent_stack[level - 1] : -1;
      dt_menu_link_child(m, parent, m->cnt);
      parent_stack[level] = m->cnt;
      for(int i = level + 1; i <= DT_MENU_MAX_DEPTH; i++) parent_stack[i] = -1;
      m->cnt++;
    }
    else
    {
      char label[128] = {0}, keyname[32] = {0};
      const char *last_space = strrchr(p, ' ');
      if(last_space && last_space[1] && strlen(last_space + 1) <= 8)
      {
        snprintf(keyname, sizeof(keyname), "%s", last_space + 1);
        int len = (int)(last_space - p);
        if(len > 0 && (size_t)len < sizeof(label))
        {
          memcpy(label, p, len);
          label[len] = 0;
        }
      }
      else
      {
        snprintf(label, sizeof(label), "%s", p);
      }

      const char *dedup = 0;
      dt_stringpool_get(&m->sp, label, strlen(label), m->cnt + 1, &dedup);
      e->label = dedup;
      e->key = keyname[0] ? hk_name_to_glfw(keyname) : 0;

      int parent = level > 0 ? parent_stack[level - 1] : -1;
      dt_menu_link_child(m, parent, m->cnt);
      parent_stack[level] = m->cnt;
      for(int i = level + 1; i <= DT_MENU_MAX_DEPTH; i++) parent_stack[i] = -1;

      m->cnt++;
    }
  }
  fclose(f);
  dt_log(s_log_gui, "[menu] loaded %d entries for '%s'", m->cnt, viewname);
  return 0;
}

static inline void
dt_menu_cleanup(dt_menu_t *m)
{
  dt_stringpool_cleanup(&m->sp);
  m->cnt = 0;
  dt_menu_set_depth(m, 0);
}

// returns label if set, else null-terminates label_tkn into buf[9]
static inline const char *
dt_menu_entry_label(const dt_menu_entry_t *e, char buf[9])
{
  if(e->label) return e->label;
  if(e->label_tkn) { memcpy(buf, dt_token_str(e->label_tkn), 8); buf[8] = 0; return buf; }
  return NULL;
}

// set label from string (if non-NULL) or token fallback
static inline void
dt_menu_set_label(dt_menu_entry_t *e, const char *str, dt_token_t tkn)
{
  if(str) e->label = str;
  else    e->label_tkn = tkn;
}

// try label characters first, then first free slot; *used bitmask: a-z=0-25, 0-9=26-35
static inline void
dt_menu_assign_key(dt_menu_entry_t *e, uint64_t *used)
{
  if(e->key) return;
  char buf[9];
  const char *l = dt_menu_entry_label(e, buf);
  if(!l || !l[0]) return;
  int assigned = 0;
  for(int j = 0; l[j] && !assigned; j++)
  {
    char c = tolower((unsigned char)l[j]);
    int bit = -1;
    if(c >= 'a' && c <= 'z') bit = c - 'a';
    else if(c >= '0' && c <= '9') bit = 26 + (c - '0');
    if(bit >= 0 && !(*used & (1ull << bit)))
    {
      e->key = bit < 26 ? GLFW_KEY_A + bit : GLFW_KEY_0 + (bit - 26);
      *used |= (1ull << bit);
      assigned = 1;
    }
  }
  if(!assigned)
  {
    for(int b = 0; b < 36 && !assigned; b++)
    {
      if(!(*used & (1ull << b)))
      {
        e->key = b < 26 ? GLFW_KEY_A + b : GLFW_KEY_0 + (b - 26);
        *used |= (1ull << b);
        assigned = 1;
      }
    }
  }
}

static inline void
dt_menu_assign_dynamic_keys(dt_menu_entry_t *entries, int cnt)
{
  uint64_t used = 0;
  for(int i = 0; i < cnt; i++)
    dt_menu_assign_key(entries + i, &used);
}

// sort by label first so key assignment is stable across provider re-scans
static inline void
dt_menu_assign_stable_keys(dt_menu_entry_t *entries, int cnt, uint64_t *used)
{
  int idx[DT_MENU_MAX_DYNAMIC];
  for(int i = 0; i < cnt && i < DT_MENU_MAX_DYNAMIC; i++) idx[i] = i;
  for(int i = 1; i < cnt && i < DT_MENU_MAX_DYNAMIC; i++)
  {
    int tmp = idx[i];
    char la_buf[9], lb_buf[9];
    const char *la = dt_menu_entry_label(entries + tmp, la_buf);
    if(!la) la = "";
    int j = i - 1;
    for(; j >= 0; j--)
    {
      const char *lb = dt_menu_entry_label(entries + idx[j], lb_buf);
      if(!lb) lb = "";
      if(strcmp(lb, la) <= 0) break;
      idx[j + 1] = idx[j];
    }
    idx[j + 1] = tmp;
  }
  for(int si = 0; si < cnt && si < DT_MENU_MAX_DYNAMIC; si++)
    dt_menu_assign_key(entries + idx[si], used);
}

static inline void
dt_menu_entry_init(dt_menu_entry_t *e)
{
  memset(e, 0, sizeof(*e));
  e->parent = -1;
  e->first_child = -1;
  e->next_sibling = -1;
  e->hotkey_index = -1;
}

#define DT_MENU_SCANDIR_CACHE_SLOTS 4

typedef struct dt_menu_scandir_slot_t
{
  char            dirname[64];
  char            ext[16];
  time_t          mtime[2]; // [0]=homedir [1]=basedir; 0=absent
  dt_menu_entry_t buf[DT_MENU_MAX_DYNAMIC];
  int             cnt;
}
dt_menu_scandir_slot_t;

static dt_menu_scandir_slot_t g_menu_scandir_cache[DT_MENU_SCANDIR_CACHE_SLOTS];

static inline time_t
dt_menu_dir_mtime(const char *dirname, int inbase)
{
  char path[PATH_MAX];
  snprintf(path, sizeof(path), "%s/%s",
      inbase ? dt_pipe.basedir : dt_pipe.homedir, dirname);
  struct stat st;
  return stat(path, &st) ? 0 : st.st_mtime;
}

static inline int
dt_menu_prov_scandir(dt_menu_entry_t *buf, int max, const char *dirname, const char *ext)
{
  time_t mtime[2] = { dt_menu_dir_mtime(dirname, 0), dt_menu_dir_mtime(dirname, 1) };
  const char *ext_key = ext ? ext : "";

  // find matching cache slot
  dt_menu_scandir_slot_t *slot = 0;
  int free_slot = -1;
  for(int i = 0; i < DT_MENU_SCANDIR_CACHE_SLOTS; i++)
  {
    dt_menu_scandir_slot_t *s = g_menu_scandir_cache + i;
    if(s->dirname[0] && !strcmp(s->dirname, dirname) && !strcmp(s->ext, ext_key))
    { slot = s; break; }
    if(!s->dirname[0] && free_slot < 0) free_slot = i;
  }

  // cache hit: directories unchanged
  if(slot && slot->mtime[0] == mtime[0] && slot->mtime[1] == mtime[1])
  {
    int cnt = slot->cnt < max ? slot->cnt : max;
    for(int i = 0; i < cnt; i++)
    {
      buf[i] = slot->buf[i];
      buf[i].label = buf[i].action_buf; // re-point into output buffer
    }
    return cnt;
  }

  // cache miss: pick slot, evict slot 0 if all full
  if(!slot)
  {
    slot = g_menu_scandir_cache + (free_slot >= 0 ? free_slot : 0);
    snprintf(slot->dirname, sizeof(slot->dirname), "%s", dirname);
    snprintf(slot->ext,     sizeof(slot->ext),     "%s", ext_key);
  }

  // full directory scan into slot
  int cnt = 0;
  for(int inbase = 0; inbase < 2 && cnt < DT_MENU_MAX_DYNAMIC; inbase++)
  {
    void *dirp = dt_res_opendir(dirname, inbase);
    if(!dirp) continue;
    const char *basename = 0;
    while((basename = dt_res_next_basename(dirp, inbase)) && cnt < DT_MENU_MAX_DYNAMIC)
    {
      if(basename[0] == '.') continue;
      if(ext)
      {
        size_t len = strlen(basename), elen = strlen(ext);
        if(len <= elen || strcmp(basename + len - elen, ext)) continue;
      }
      int dup = 0;
      for(int i = 0; i < cnt; i++)
        if(!strcmp(slot->buf[i].action_buf, basename)) { dup = 1; break; }
      if(dup) continue;
      dt_menu_entry_init(slot->buf + cnt);
      snprintf(slot->buf[cnt].action_buf, sizeof(slot->buf[cnt].action_buf), "%s", basename);
      slot->buf[cnt].label = slot->buf[cnt].action_buf;
      if(cnt > 0) slot->buf[cnt - 1].next_sibling = cnt;
      cnt++;
    }
    dt_res_closedir(dirp, inbase);
  }
  dt_menu_assign_dynamic_keys(slot->buf, cnt);
  slot->cnt      = cnt;
  slot->mtime[0] = mtime[0];
  slot->mtime[1] = mtime[1];

  // copy to output
  int out_cnt = cnt < max ? cnt : max;
  for(int i = 0; i < out_cnt; i++)
  {
    buf[i] = slot->buf[i];
    buf[i].label = buf[i].action_buf;
  }
  return out_cnt;
}

static int
dt_menu_prov_presets(dt_menu_entry_t *buf, int max, void *data)
{
  (void)data;
  return dt_menu_prov_scandir(buf, max, "presets", ".pst");
}

static int
dt_menu_prov_keyaccel(dt_menu_entry_t *buf, int max, void *data)
{
  (void)data;
  return dt_menu_prov_scandir(buf, max, "keyaccel", 0);
}


static inline void
dt_menu_format_combo(char *buf, int bufsz,
    dt_token_t mod, dt_token_t inst, dt_token_t param, int value)
{
  snprintf(buf, bufsz, "combo:%.8s:%.8s:%.8s:%d",
      dt_token_str(mod), dt_token_str(inst),
      dt_token_str(param), value);
}

static inline int
dt_menu_parse_combo(const char *action, dt_token_t *mod, dt_token_t *inst, dt_token_t *param, int *value)
{
  char m[9] = {0}, i[9] = {0}, p[9] = {0};
  if(sscanf(action + 6, "%8[^:]:%8[^:]:%8[^:]:%d", m, i, p, value) < 4)
    return 1;
  *mod = dt_token(m); *inst = dt_token(i); *param = dt_token(p);
  return 0;
}

static inline void
dt_menu_format_activate(char *buf, int bufsz, dt_token_t mod, dt_token_t inst)
{
  snprintf(buf, bufsz, "activate:%.8s:%.8s",
      dt_token_str(mod), dt_token_str(inst));
}

static inline int
dt_menu_parse_activate(const char *action, dt_token_t *mod, dt_token_t *inst)
{
  char m[9] = {0}, i[9] = {0};
  if(sscanf(action + 9, "%8[^:]:%8s", m, i) < 2)
    return 1;
  *mod = dt_token(m); *inst = dt_token(i);
  return 0;
}

static inline void
dt_menu_format_widget(char *buf, int bufsz, dt_token_t mod, dt_token_t inst, dt_token_t param)
{
  snprintf(buf, bufsz, "widget:%.8s:%.8s:%.8s",
      dt_token_str(mod), dt_token_str(inst), dt_token_str(param));
}

static inline int
dt_menu_parse_widget(const char *action, dt_token_t *mod, dt_token_t *inst, dt_token_t *param)
{
  char m[9] = {0}, i[9] = {0}, p[9] = {0};
  if(sscanf(action + 7, "%8[^:]:%8[^:]:%8s", m, i, p) < 3) return 1;
  *mod = dt_token(m); *inst = dt_token(i); *param = dt_token(p);
  return 0;
}

static inline void
dt_menu_format_callback_action(char *buf, int bufsz, dt_token_t mod, dt_token_t inst, dt_token_t param)
{
  snprintf(buf, bufsz, "callback:%.8s:%.8s:%.8s",
      dt_token_str(mod), dt_token_str(inst), dt_token_str(param));
}

static inline int
dt_menu_parse_callback_action(const char *action, dt_token_t *mod, dt_token_t *inst, dt_token_t *param)
{
  char m[9] = {0}, i[9] = {0}, p[9] = {0};
  if(sscanf(action + 9, "%8[^:]:%8[^:]:%8s", m, i, p) < 3) return 1;
  *mod = dt_token(m); *inst = dt_token(i); *param = dt_token(p);
  return 0;
}

static inline void
dt_menu_prov_on_open_module(void *data)
{
  vkdt.wstate.pending_modid = (int)(intptr_t)data;
}

// enumerate graph modules (same order as tweak-all view)
static int
dt_menu_prov_modules(dt_menu_entry_t *buf, int max, void *data)
{
  (void)data;
  dt_graph_t *graph = &vkdt.graph_dev;
  dt_module_t *const arr = graph->module;
  const int arr_cnt = graph->num_modules;
  uint32_t modid[100], mcnt = 0;
#define TRAVERSE_POST \
  if(mcnt < sizeof(modid)/sizeof(modid[0])) modid[mcnt++] = curr;
#include "pipe/graph-traverse.inc"
  char exclude[16][20];
  struct { char label[20]; char key[8]; } overrides[16];
  int nexclude = 0, noverrides = 0;
  {
    FILE *cf = dt_graph_open_resource(0, 0, "menus/module-keys", "rb");
    if(cf)
    {
      char line[128];
      while(fgets(line, sizeof(line), cf))
      {
        char *nl = strchr(line, '\n');
        if(nl) *nl = 0;
        if(line[0] == '#' || line[0] == 0) continue;
        if(line[0] == '!' && line[1] && nexclude < 16)
        { snprintf(exclude[nexclude++], sizeof(exclude[0]), "%s", line + 1); continue; }
        char *sp = strrchr(line, ' ');
        if(sp && sp[1] && noverrides < 16)
        {
          *sp = 0;
          snprintf(overrides[noverrides].label, sizeof(overrides[0].label), "%s", line);
          snprintf(overrides[noverrides].key, sizeof(overrides[0].key), "%s", sp + 1);
          noverrides++;
        }
      }
      fclose(cf);
    }
  }
  int cnt = 0;
  for(int m = mcnt - 1; m >= 0 && cnt < max; m--)
  {
    dt_module_t *mod = arr + modid[m];
    if(!mod->so->num_params) continue;
    if(!mod->so->has_inout_chain) continue;
    int skip = 0;
    const char *mname = dt_token_str(mod->name);
    for(int e = 0; e < nexclude; e++)
    {
      if(!strcmp(exclude[e], mname)) { skip = 1; break; }
      char lbl[20];
      snprintf(lbl, sizeof(lbl), "%.8s:%.8s", mname, dt_token_str(mod->inst));
      if(!strcmp(exclude[e], lbl)) { skip = 1; break; }
    }
    if(skip) continue;
    dt_menu_entry_init(buf + cnt);
    snprintf(buf[cnt].action_buf, sizeof(buf[cnt].action_buf),
        "%.8s:%.8s", dt_token_str(mod->name), dt_token_str(mod->inst));
    buf[cnt].label = buf[cnt].action_buf;
    buf[cnt].key = 0;
    buf[cnt].provider = dt_menu_prov_params;
    buf[cnt].provider_data = (void *)(intptr_t)modid[m];
    buf[cnt].on_open = dt_menu_prov_on_open_module;
    if(cnt > 0) buf[cnt - 1].next_sibling = cnt;
    cnt++;
  }
  uint64_t used = 0;
  for(int o = 0; o < noverrides; o++)
  {
    int glfw = hk_name_to_glfw(overrides[o].key);
    if(!glfw) continue;
    for(int i = 0; i < cnt; i++)
    {
      if(!buf[i].label || strcmp(buf[i].label, overrides[o].label)) continue;
      buf[i].key = glfw;
      char c = tolower((unsigned char)overrides[o].key[0]);
      int bit = -1;
      if(c >= 'a' && c <= 'z') bit = c - 'a';
      else if(c >= '0' && c <= '9') bit = 26 + (c - '0');
      if(bit >= 0) used |= (1ull << bit);
      break;
    }
  }
  dt_menu_assign_stable_keys(buf, cnt, &used);
  return cnt;
}

static inline int
dt_menu_param_visible(dt_module_t *mod, const dt_ui_param_t *param)
{
  if(param->widget.grpid == -1) return 1;
  int grpval = dt_module_param_int(mod, param->widget.grpid)[0];
  if(param->widget.mode < 100 && grpval != param->widget.mode) return 0;
  if(param->widget.mode >= 100 && grpval == param->widget.mode - 100) return 0;
  return 1;
}

static int
dt_menu_prov_params(dt_menu_entry_t *buf, int max, void *data)
{
  int modid = (int)(intptr_t)data;
  dt_graph_t *graph = &vkdt.graph_dev;
  if(modid < 0 || modid >= (int)graph->num_modules) return 0;
  dt_module_t *mod = graph->module + modid;
  int cnt = 0;
  for(int p = 0; p < mod->so->num_params && cnt < max; p++)
  {
    const dt_ui_param_t *param = mod->so->param[p];
    if(!param) continue;
    if(!dt_menu_param_visible(mod, param)) continue;
    dt_token_t wtype = param->widget.type;
    if(wtype == dt_token("hidden") || wtype == dt_token("filename")) continue;
    int is_slider = (param->type == dt_token("float") && wtype == dt_token("slider"));
    int is_rgb    = (param->type == dt_token("float") && wtype == dt_token("rgb"));
    int is_combo  = (param->type == dt_token("int")   && wtype == dt_token("combo"));

    if(is_slider)
    {
      const char *base = param->long_name;
      for(int c = 0; c < param->cnt && cnt < max; c++)
      {
        dt_menu_entry_init(buf + cnt);
        dt_dragkey_format_action(buf[cnt].action_buf, sizeof(buf[cnt].action_buf),
            mod->name, mod->inst, param->name, c);
        if(param->cnt > 1)
        { // pack "name N" label after action string for multi-component sliders
          int alen = strlen(buf[cnt].action_buf);
          if(base)
            snprintf(buf[cnt].action_buf + alen + 1,
                sizeof(buf[cnt].action_buf) - alen - 1, "%s %d", base, c);
          else
            snprintf(buf[cnt].action_buf + alen + 1,
                sizeof(buf[cnt].action_buf) - alen - 1, "%.8s %d", dt_token_str(param->name), c);
          buf[cnt].label = buf[cnt].action_buf + alen + 1;
        }
        else if(base)
          buf[cnt].label = base;
        else
          buf[cnt].label_tkn = param->name;
        buf[cnt].key = 0;
        if(cnt > 0) buf[cnt - 1].next_sibling = cnt;
        cnt++;
      }
    }
    else if(is_rgb)
    {
      static const char *suffix[3] = {" r", " g", " b"};
      const char *base = param->long_name;
      for(int c = 0; c < 3 && cnt < max; c++)
      {
        dt_menu_entry_init(buf + cnt);
        dt_dragkey_format_action(buf[cnt].action_buf, sizeof(buf[cnt].action_buf),
            mod->name, mod->inst, param->name, c);
        int alen = strlen(buf[cnt].action_buf);
        if(base)
          snprintf(buf[cnt].action_buf + alen + 1,
              sizeof(buf[cnt].action_buf) - alen - 1, "%s%s", base, suffix[c]);
        else
          snprintf(buf[cnt].action_buf + alen + 1,
              sizeof(buf[cnt].action_buf) - alen - 1, "%.8s%s", dt_token_str(param->name), suffix[c]);
        buf[cnt].label = buf[cnt].action_buf + alen + 1;
        buf[cnt].key = 0;
        if(cnt > 0) buf[cnt - 1].next_sibling = cnt;
        cnt++;
      }
    }
    else if(is_combo)
    {
      dt_menu_entry_init(buf + cnt);
      dt_menu_set_label(buf + cnt, param->long_name, param->name);
      buf[cnt].key = 0;
      buf[cnt].provider = dt_menu_prov_combo_opts;
      if(modid >= 0x10000 || p >= 0x10000) { dt_log(s_log_err, "[menu] modid/param index exceeds encoding limit, skipping"); continue; }
      buf[cnt].provider_data = (void *)(intptr_t)(((unsigned)modid << 16) | (unsigned)p);
      if(cnt > 0) buf[cnt - 1].next_sibling = cnt;
      cnt++;
    }
    else if(wtype == dt_token("callback"))
    { // button that calls ui_callback
      dt_menu_entry_init(buf + cnt);
      dt_menu_format_callback_action(buf[cnt].action_buf, sizeof(buf[cnt].action_buf),
          mod->name, mod->inst, param->name);
      dt_menu_set_label(buf + cnt, param->long_name, param->name);
      buf[cnt].key = 0;
      if(cnt > 0) buf[cnt - 1].next_sibling = cnt;
      cnt++;
    }
    else if(wtype == dt_token("pers") || wtype == dt_token("crop"))
    { // tool widget: start interactive overlay
      dt_menu_entry_init(buf + cnt);
      dt_menu_format_widget(buf[cnt].action_buf, sizeof(buf[cnt].action_buf),
          mod->name, mod->inst, param->name);
      dt_menu_set_label(buf + cnt, param->long_name, param->name);
      buf[cnt].key = 0;
      if(cnt > 0) buf[cnt - 1].next_sibling = cnt;
      cnt++;
    }
    else if(wtype == dt_token("straight"))
    { // tool widget with embedded float slider: emit both dragkey and tool-start entries
      dt_menu_entry_init(buf + cnt);
      dt_dragkey_format_action(buf[cnt].action_buf, sizeof(buf[cnt].action_buf),
          mod->name, mod->inst, param->name, 0);
      dt_menu_set_label(buf + cnt, param->long_name, param->name);
      buf[cnt].key = 0;
      if(cnt > 0) buf[cnt - 1].next_sibling = cnt;
      cnt++;
      if(cnt < max)
      {
        dt_menu_entry_init(buf + cnt);
        dt_menu_format_widget(buf[cnt].action_buf, sizeof(buf[cnt].action_buf),
            mod->name, mod->inst, param->name);
        // pack label "paramname tool" after action string
        int alen = strlen(buf[cnt].action_buf);
        if(param->long_name)
          snprintf(buf[cnt].action_buf + alen + 1,
              sizeof(buf[cnt].action_buf) - alen - 1, "%s tool", param->long_name);
        else
          snprintf(buf[cnt].action_buf + alen + 1,
              sizeof(buf[cnt].action_buf) - alen - 1, "%.8s tool", dt_token_str(param->name));
        buf[cnt].label = buf[cnt].action_buf + alen + 1;
        buf[cnt].key = 0;
        if(cnt > 0) buf[cnt - 1].next_sibling = cnt;
        cnt++;
      }
    }
    else if(wtype == dt_token("colwheel"))
    {
      const char *base = param->long_name ? param->long_name : dt_token_str(param->name);
      // 2D colour entry (components 0+1 on x+y axes)
      if(cnt < max)
      {
        dt_menu_entry_init(buf + cnt);
        snprintf(buf[cnt].action_buf, sizeof(buf[cnt].action_buf),
            "dragkey2d:%.8s:%.8s:%.8s",
            dt_token_str(mod->name), dt_token_str(mod->inst), dt_token_str(param->name));
        dt_menu_set_label(buf + cnt, param->long_name, param->name);
        buf[cnt].key = 0;
        if(cnt > 0) buf[cnt - 1].next_sibling = cnt;
        cnt++;
      }
      // brightness entry (component 3, master slider)
      if(cnt < max && param->cnt > 3)
      {
        dt_menu_entry_init(buf + cnt);
        dt_dragkey_format_action(buf[cnt].action_buf, sizeof(buf[cnt].action_buf),
            mod->name, mod->inst, param->name, 3);
        int alen = strlen(buf[cnt].action_buf);
        snprintf(buf[cnt].action_buf + alen + 1,
            sizeof(buf[cnt].action_buf) - alen - 1, "%s brightness", base);
        buf[cnt].label = buf[cnt].action_buf + alen + 1;
        buf[cnt].key = 0;
        if(cnt > 0) buf[cnt - 1].next_sibling = cnt;
        cnt++;
      }
    }
    else if(wtype == dt_token("colour") || wtype == dt_token("print"))
      continue; // display-only, no useful menu action
    else // tool/interactive widget: activate module
    {
      dt_menu_entry_init(buf + cnt);
      dt_menu_format_activate(buf[cnt].action_buf, sizeof(buf[cnt].action_buf),
          mod->name, mod->inst);
      dt_menu_set_label(buf + cnt, param->long_name, param->name);
      buf[cnt].key = 0;
      if(cnt > 0) buf[cnt - 1].next_sibling = cnt;
      cnt++;
    }
  }
  dt_menu_assign_dynamic_keys(buf, cnt);
  return cnt;
}

// enumerate options for a combo parameter (provider_data encodes modid<<16|parid)
static int
dt_menu_prov_combo_opts(dt_menu_entry_t *buf, int max, void *data)
{
  unsigned encoded = (unsigned)(intptr_t)data;
  int modid = (int)(encoded >> 16);
  int parid = (int)(encoded & 0xffff);
  dt_graph_t *graph = &vkdt.graph_dev;
  if(modid < 0 || modid >= (int)graph->num_modules) return 0;
  dt_module_t *mod = graph->module + modid;
  if(parid < 0 || parid >= mod->so->num_params) return 0;
  const dt_ui_param_t *param = mod->so->param[parid];
  if(!param || !param->widget.data) return 0;

  const char *opts = (const char *)param->widget.data;
  int cnt = 0;
  int idx = 0;
  while(*opts && cnt < max)
  {
    dt_menu_entry_init(buf + cnt);
    dt_menu_format_combo(buf[cnt].action_buf, sizeof(buf[cnt].action_buf),
        mod->name, mod->inst, param->name, idx);
    buf[cnt].label = opts; // stable: widget.data lifetime = module SO
    buf[cnt].key = 0;
    if(cnt > 0) buf[cnt - 1].next_sibling = cnt;
    cnt++;
    idx++;
    opts += strlen(opts) + 1; // skip past \0 separator
  }
  dt_menu_assign_dynamic_keys(buf, cnt);
  return cnt;
}

static inline void
dt_menu_exec_action(dt_menu_t *m, dt_menu_entry_t *e)
{
  if(!e->action_buf[0]) return;
  if(m->action_cb)
    m->action_cb(e->action_buf);
  else
    dt_log(s_log_gui, "[menu] no action callback set for '%s'", e->action_buf);
}

static inline int
dt_menu_get_children(
    dt_menu_t       *m,
    int              parent_idx,
    dt_menu_entry_t *out,
    int              max)
{
  int cnt = 0;
  if(parent_idx >= 0)
  {
    int c = m->entries[parent_idx].first_child;
    while(c >= 0 && cnt < max)
    {
      if(m->entries[c].provider)
      {
        int pcnt = m->entries[c].provider(
            m->dynamic, DT_MENU_MAX_DYNAMIC, m->entries[c].provider_data);

        for(int i = 0; i < pcnt && cnt < max; i++)
          out[cnt++] = m->dynamic[i];
      }
      else
      {
        out[cnt++] = m->entries[c];
      }
      c = m->entries[c].next_sibling;
    }
  }
  else
  {
    for(int i = 0; i < m->cnt && cnt < max; i++)
    {
      if(m->entries[i].parent != -1) continue;
      if(m->entries[i].provider)
      {
        int pcnt = m->entries[i].provider(
            m->dynamic, DT_MENU_MAX_DYNAMIC, m->entries[i].provider_data);

        for(int j = 0; j < pcnt && cnt < max; j++)
          out[cnt++] = m->dynamic[j];
      }
      else
      {
        out[cnt++] = m->entries[i];
      }
    }
  }
  return cnt;
}

// avoids re-scanning providers every frame; call before reading m->children
static inline int
dt_menu_ensure_children(dt_menu_t *m)
{
  if(m->depth <= 0) return 0;
  if(!m->children_dirty) return m->child_cnt;
  dt_menu_stack_entry_t se = m->stack[m->depth - 1];
  double t_scan_beg = glfwGetTime();
  if(se.kind == DT_MENU_SK_DYN_PARENT)
  {
    if(m->dyn_parent.provider)
    {
      m->child_cnt = m->dyn_parent.provider(
          m->dynamic, DT_MENU_MAX_DYNAMIC, m->dyn_parent.provider_data);
      for(int i = 0; i < m->child_cnt; i++)
        m->children[i] = m->dynamic[i];
      dt_menu_assign_dynamic_keys(m->children, m->child_cnt);
    }
    else m->child_cnt = 0;
  }
  else if(se.kind == DT_MENU_SK_GAMEPAD_ROOT)
  {
    m->child_cnt = dt_menu_get_children(m, -1,
        m->children, DT_MENU_MAX_DYNAMIC + DT_MENU_MAX_ENTRIES);
  }
  else
  {
    m->child_cnt = dt_menu_get_children(m, se.idx,
        m->children, DT_MENU_MAX_DYNAMIC + DT_MENU_MAX_ENTRIES);
  }
  dt_log(s_log_perf, "[menu] ensure_children (%s depth=%d):\t%8.3f ms",
      m->view_name ? m->view_name : "?", m->depth,
      1000.0*(glfwGetTime() - t_scan_beg));
  m->children_dirty = 0;
  return m->child_cnt;
}

// returns 1 = consumed, -(hotkey_index+2) = caller should dispatch hotkey
static inline int
dt_menu_select(dt_menu_t *m, dt_menu_entry_t *children, int child_cnt,
    int idx, hk_t *hk, int hk_cnt)
{
  if(idx < 0 || idx >= child_cnt) return 1;
  dt_menu_entry_t *e = &children[idx];

  if(e->first_child >= 0 || e->provider)
  {
    if(m->depth >= DT_MENU_MAX_DEPTH) { dt_gui_notification("menu too deep"); return 1; }
    if(e->provider)
    {
      if(e->on_open) e->on_open(e->provider_data);
      m->dyn_parent = *e;
      m->stack[m->depth] = (dt_menu_stack_entry_t){.kind = DT_MENU_SK_DYN_PARENT};
      dt_menu_set_depth(m, m->depth + 1);
      m->cursor = 0;
      m->scroll_off = 0;
    }
    else
    {
      dt_menu_stack_entry_t pse = m->stack[m->depth - 1];
      int match_parent = pse.kind == DT_MENU_SK_ENTRY ? pse.idx : -1;
      int found = 0;
      for(int j = 0; j < m->cnt; j++)
      {
        if(m->entries[j].key == e->key && m->entries[j].parent == match_parent)
        {
          m->stack[m->depth] = (dt_menu_stack_entry_t){.idx = j};
          dt_menu_set_depth(m, m->depth + 1);
          m->cursor = 0;
          m->scroll_off = 0;
          found = 1;
          break;
        }
      }
      if(!found) { dt_menu_set_depth(m, 0); return 1; }
    }
    // auto-select if submenu has exactly one child
    int ccnt = dt_menu_ensure_children(m);
    if(ccnt == 1)
      return dt_menu_select(m, m->children, ccnt, 0, hk, hk_cnt);
    return 1;
  }
  else
  {
    dt_menu_set_depth(m, 0);
    if(e->hotkey_index >= 0 && e->hotkey_index < hk_cnt)
      return -(e->hotkey_index + 2);
    else if(e->action_buf[0])
    {
      dt_menu_exec_action(m, e);
      return 1;
    }
    return 1;
  }
}

// returns 0 = not consumed, 1 = consumed, -(hotkey_index+2) = caller dispatches
static inline int
dt_menu_keyboard(
    dt_menu_t *m,
    hk_t      *hk,
    int        hk_cnt,
    int        key,
    int        action,
    int        mods)
{
  if(action != GLFW_PRESS && action != GLFW_REPEAT) return m->depth > 0 ? 1 : 0;
  // REPEAT only for navigation keys, not for selection/leader keys
  int is_repeat = (action == GLFW_REPEAT);

  if(m->depth == 0)
  {
    if(is_repeat) return 0; // don't open menus on repeat
    if(mods) return 0; // don't steal modified keys (e.g. ctrl+p)
    if(dt_gui_input_blocked()) return 0;
    for(int i = 0; i < m->cnt; i++)
    {
      if(m->entries[i].parent != -1) continue;
      if(m->entries[i].key != key) continue;
      if(m->entries[i].first_child >= 0 || m->entries[i].provider)
      {
        m->stack[0] = (dt_menu_stack_entry_t){.idx = i};
        dt_menu_set_depth(m, 1);
        m->cursor = 0;
        m->scroll_off = 0;
        return 1;
      }
      else if(m->entries[i].hotkey_index >= 0)
        return 0; // let hk_get_hotkey handle it
    }
    return 0;
  }

  if(key == GLFW_KEY_ESCAPE)
  {
    if(m->depth <= 1) dt_menu_set_depth(m, 0);
    else { dt_menu_set_depth(m, m->depth - 1); m->cursor = 0; m->scroll_off = 0; }
    return 1;
  }

  int child_cnt = dt_menu_ensure_children(m);

  if(key == GLFW_KEY_UP)   { if(m->cursor > 0) m->cursor--; return 1; }
  if(key == GLFW_KEY_DOWN) { if(m->cursor < child_cnt - 1) m->cursor++; return 1; }
  if(key == GLFW_KEY_ENTER || key == GLFW_KEY_RIGHT)
  {
    if(m->cursor >= 0 && m->cursor < child_cnt)
      return dt_menu_select(m, m->children, child_cnt, m->cursor, hk, hk_cnt);
    return 1;
  }
  if(key == GLFW_KEY_LEFT)
  {
    if(m->depth <= 1) dt_menu_set_depth(m, 0);
    else { dt_menu_set_depth(m, m->depth - 1); m->cursor = 0; m->scroll_off = 0; }
    return 1;
  }

  if(!is_repeat)
  { // only on press: match key, close on no match (modifier keys are transparent)
    if(key == GLFW_KEY_LEFT_SHIFT   || key == GLFW_KEY_RIGHT_SHIFT  ||
       key == GLFW_KEY_LEFT_CONTROL || key == GLFW_KEY_RIGHT_CONTROL ||
       key == GLFW_KEY_LEFT_ALT     || key == GLFW_KEY_RIGHT_ALT    ||
       key == GLFW_KEY_LEFT_SUPER   || key == GLFW_KEY_RIGHT_SUPER  ||
       key == GLFW_KEY_CAPS_LOCK)
      return 1;
    for(int i = 0; i < child_cnt; i++)
    {
      if(m->children[i].key != key) continue;
      m->cursor = i;
      return dt_menu_select(m, m->children, child_cnt, i, hk, hk_cnt);
    }
    dt_menu_set_depth(m, 0);
  }
  return 1;
}

// call at the top of each per-view gamepad handler; return early when this returns 1
static inline int
dt_menu_gamepad(dt_menu_t *m, hk_t *hk, int hk_cnt,
    GLFWgamepadstate *last, GLFWgamepadstate *curr)
{
  if(m->gamepad_button < 0) return 0;
#define GP_PRESSED(A) (curr->buttons[A] && !last->buttons[A])

  if(m->depth == 0)
  {
    if(GP_PRESSED(m->gamepad_button))
    {
      m->stack[0] = (dt_menu_stack_entry_t){.kind = DT_MENU_SK_GAMEPAD_ROOT};
      dt_menu_set_depth(m, 1);
      m->cursor = 0;
      m->scroll_off = 0;
      return 1;
    }
    return 0;
  }

  int child_cnt = dt_menu_ensure_children(m);

  if(GP_PRESSED(GLFW_GAMEPAD_BUTTON_DPAD_UP))
  { if(m->cursor > 0) m->cursor--; return 1; }
  if(GP_PRESSED(GLFW_GAMEPAD_BUTTON_DPAD_DOWN))
  { if(m->cursor < child_cnt - 1) m->cursor++; return 1; }

  if(GP_PRESSED(GLFW_GAMEPAD_BUTTON_B) || GP_PRESSED(m->gamepad_button))
  {
    if(m->depth <= 1) dt_menu_set_depth(m, 0);
    else { dt_menu_set_depth(m, m->depth - 1); m->cursor = 0; }
    return 1;
  }

  if(GP_PRESSED(GLFW_GAMEPAD_BUTTON_A) && child_cnt > 0)
    return dt_menu_select(m, m->children, child_cnt, m->cursor, hk, hk_cnt);

#undef GP_PRESSED
  return 1; // consume all other buttons while menu is open
}

static inline void
dt_menu_render(dt_menu_t *m, struct nk_context *ctx)
{
  if(m->depth <= 0) return;

  int child_cnt = dt_menu_ensure_children(m);
  if(child_cnt == 0) { dt_menu_set_depth(m, 0); return; }

  const int max_visible = 16;

  char breadcrumb[256] = {0};
  int boff = 0;
  for(int d = 0; d < m->depth && d < DT_MENU_MAX_DEPTH; d++)
  {
    if(m->stack[d].kind != DT_MENU_SK_ENTRY) continue;
    const char *l = m->entries[m->stack[d].idx].label;
    if(l) boff += snprintf(breadcrumb + boff, sizeof(breadcrumb) - boff, "%s%s", d ? " > " : "", l);
  }
  if(child_cnt > max_visible)
    boff += snprintf(breadcrumb + boff, sizeof(breadcrumb) - boff, " [%d/%d]", m->cursor + 1, child_cnt);

  struct nk_color bg        = vkdt.style.colour[NK_COLOR_WINDOW];
  struct nk_color hl        = vkdt.style.colour[NK_COLOR_DT_ACCENT];
  struct nk_color label_col = vkdt.style.colour[NK_COLOR_TEXT];
  struct nk_color btn_normal = vkdt.style.colour[NK_COLOR_BUTTON];
  struct nk_color btn_hover = vkdt.style.colour[NK_COLOR_BUTTON_HOVER];

  nk_style_push_color(ctx, &ctx->style.window.background, bg);
  nk_style_push_vec2(ctx, &ctx->style.window.padding, nk_vec2(8, 6));
  nk_style_push_color(ctx, &ctx->style.button.normal.data.color, btn_normal);
  nk_style_push_color(ctx, &ctx->style.button.hover.data.color, btn_hover);
  nk_style_push_color(ctx, &ctx->style.button.active.data.color, hl);
  nk_style_push_color(ctx, &ctx->style.button.text_normal, label_col);
  nk_style_push_color(ctx, &ctx->style.button.text_hover, label_col);
  nk_style_push_color(ctx, &ctx->style.button.text_active, label_col);
  nk_style_push_float(ctx, &ctx->style.button.rounding, 2);
  nk_style_push_vec2(ctx, &ctx->style.button.padding, nk_vec2(4, 2));
  nk_style_push_flags(ctx, &ctx->style.button.text_alignment, NK_TEXT_LEFT);

  const float row_h = ctx->style.font->height + 2 * ctx->style.button.padding.y + 4;
  const float row_spacing = ctx->style.window.spacing.y;
  const float header_h = row_h + 8;
  const float key_col_w = 50.0f;
  const int display_rows = child_cnt < max_visible ? child_cnt : max_visible;
  const float content_h = header_h + display_rows * (row_h + row_spacing) + 2 * ctx->style.window.padding.y;
  const float menu_w_raw = vkdt.state.center_wd > 600 ? 550 : vkdt.state.center_wd - 50;
  const float menu_w = menu_w_raw > 100 ? menu_w_raw : 100;
  const float menu_x = vkdt.state.center_x + (vkdt.state.center_wd - menu_w) * 0.5f;
  const float menu_y = vkdt.state.center_y + (vkdt.state.center_ht - content_h) * 0.5f - 30;

  if(nk_begin(ctx, "chord menu",
        nk_rect(menu_x, menu_y, menu_w, content_h),
        NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_BORDER))
  {
    nk_layout_row_dynamic(ctx, header_h, 1);
    nk_label_colored(ctx, breadcrumb, NK_TEXT_LEFT, label_col);

    m->clicked = -1;

    if(vkdt.wstate.chord_menu_scroll != 0.0f)
    {
      const float step = 1.5f;
      while(vkdt.wstate.chord_menu_scroll >= step && m->cursor > 0)
      { m->cursor--; vkdt.wstate.chord_menu_scroll -= step; }
      while(vkdt.wstate.chord_menu_scroll <= -step && m->cursor < child_cnt - 1)
      { m->cursor++; vkdt.wstate.chord_menu_scroll += step; }
      // at edges: discard remaining scroll to prevent flicker
      if(m->cursor <= 0 || m->cursor >= child_cnt - 1)
        vkdt.wstate.chord_menu_scroll = 0.0f;
    }

    // clamp AFTER all cursor changes this frame (scroll, keyboard, gamepad)
    if(m->cursor < 0) m->cursor = 0;
    if(m->cursor >= child_cnt) m->cursor = child_cnt - 1;
    if(m->cursor < m->scroll_off) m->scroll_off = m->cursor;
    if(m->cursor >= m->scroll_off + max_visible)
      m->scroll_off = m->cursor - max_visible + 1;
    if(m->scroll_off < 0) m->scroll_off = 0;
    int max_off = child_cnt > max_visible ? child_cnt - max_visible : 0;
    if(m->scroll_off > max_off) m->scroll_off = max_off;
    int visible = child_cnt < max_visible ? child_cnt : max_visible;

    nk_style_push_vec2(ctx, &ctx->style.window.spacing, nk_vec2(0, row_spacing));
    for(int vi = 0; vi < visible; vi++)
    {
      int i = vi + m->scroll_off;
      if(i >= child_cnt) break;
      int is_cursor = (i == m->cursor);
      int has_sub = m->children[i].first_child >= 0 || m->children[i].provider;

      char label_display[192], clean_lbl[128];
      const char *kname = m->children[i].key ? dt_menu_glfw_to_name(m->children[i].key) : "";
      char lbl_tkn_buf[9];
      const char *lbl = dt_menu_entry_label(m->children + i, lbl_tkn_buf);
      if(!lbl) lbl = "?";
      snprintf(clean_lbl, sizeof(clean_lbl), "%s", lbl);
      size_t ll = strlen(clean_lbl);
      if(ll > 4 && !strcmp(clean_lbl + ll - 4, ".pst")) clean_lbl[ll - 4] = 0; // strip .pst
      snprintf(label_display, sizeof(label_display), "%s%s", clean_lbl, has_sub ? "  >" : "");

      if(is_cursor)
      {
        nk_style_push_color(ctx, &ctx->style.button.normal.data.color, hl);
        nk_style_push_color(ctx, &ctx->style.button.hover.data.color, hl);
        nk_style_push_color(ctx, &ctx->style.button.text_normal, vkdt.style.colour[NK_COLOR_DT_ACCENT_TEXT]);
        nk_style_push_color(ctx, &ctx->style.button.text_hover,  vkdt.style.colour[NK_COLOR_DT_ACCENT_TEXT]);
      }

      nk_layout_row_template_begin(ctx, row_h);
      nk_layout_row_template_push_static(ctx, key_col_w);
      nk_layout_row_template_push_dynamic(ctx);
      nk_layout_row_template_end(ctx);
      nk_style_push_flags(ctx, &ctx->style.button.text_alignment, NK_TEXT_CENTERED);
      if(nk_button_label(ctx, kname)) m->clicked = i;      // key column
      nk_style_pop_flags(ctx);
      if(nk_button_label(ctx, label_display)) m->clicked = i; // label column
      if(nk_widget_is_hovered(ctx) && m->children[i].tooltip) nk_tooltip(ctx, m->children[i].tooltip);

      if(is_cursor)
      {
        nk_style_pop_color(ctx);
        nk_style_pop_color(ctx);
        nk_style_pop_color(ctx);
        nk_style_pop_color(ctx);
      }
    }
    nk_style_pop_vec2(ctx);

  }
  nk_end(ctx);

  if(nk_input_is_mouse_pressed(&ctx->input, NK_BUTTON_LEFT) &&
     !nk_input_is_mouse_hovering_rect(&ctx->input,
       nk_rect(menu_x, menu_y, menu_w, content_h)))
    dt_menu_set_depth(m, 0);

  vkdt.wstate.nk_active_next = 0; // don't steal keyboard from hotkeys

  // 11 pushes above (spacing and cursor colours are balanced inside the loop)
  nk_style_pop_flags(ctx);
  nk_style_pop_vec2(ctx);
  nk_style_pop_float(ctx);
  nk_style_pop_color(ctx);
  nk_style_pop_color(ctx);
  nk_style_pop_color(ctx);
  nk_style_pop_color(ctx);
  nk_style_pop_color(ctx);
  nk_style_pop_color(ctx);
  nk_style_pop_vec2(ctx);
  nk_style_pop_color(ctx);
}


// execute a combo: action (set an integer parameter from the menu).
// returns 1 if the action string was handled (regardless of success).
static inline int
dt_menu_exec_combo(const char *action)
{
  if(strncmp(action, "combo:", 6)) return 0;
  dt_token_t mod, inst, par;
  int val = 0;
  if(!dt_menu_parse_combo(action, &mod, &inst, &par, &val))
  {
    int modid = dt_module_get(&vkdt.graph_dev, mod, inst);
    if(modid < 0) { dt_gui_notification("module not found"); return 1; }
    int parid = dt_module_get_param(vkdt.graph_dev.module[modid].so, par);
    if(parid < 0) { dt_gui_notification("param not found"); return 1; }
    int32_t *p = (int32_t *)(vkdt.graph_dev.module[modid].param
        + vkdt.graph_dev.module[modid].so->param[parid]->offset);
    int32_t oldval = *p;
    *p = val;
    dt_graph_run_t flags = s_graph_run_none;
    if(vkdt.graph_dev.module[modid].so->check_params)
      flags = vkdt.graph_dev.module[modid].so->check_params(
          vkdt.graph_dev.module + modid, parid, 0, &oldval);
    vkdt.graph_dev.runflags = flags | s_graph_run_record_cmd_buf;
    vkdt.graph_dev.active_module = modid;
    dt_graph_history_append(&vkdt.graph_dev, modid, parid, 0.0);
    vkdt.wstate.busy += 2;
  }
  return 1;
}

// returns 0 = nothing, 1 = consumed, -(hotkey_index+2) = caller dispatches
static inline int
dt_menu_process_clicks(dt_menu_t *m, hk_t *hk, int hk_cnt)
{
  if(m->depth <= 0 || m->clicked < 0) return 0;

  int child_cnt = dt_menu_ensure_children(m);
  int idx = m->clicked;
  m->clicked = -1;
  m->cursor = idx;
  return dt_menu_select(m, m->children, child_cnt, idx, hk, hk_cnt);
}
