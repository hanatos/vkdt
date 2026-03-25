#pragma once
// drag-to-adjust: hold a key and drag the mouse to change a parameter.
// config files live in bin/dragkeys/ and ~/.config/vkdt/dragkeys/.
// format:
//   # comment
//   key:<keyname>
//   <module>:<instance>:<param>:<component>:<sensitivity>
//   ...
// multiple component lines per file are supported (e.g. for white balance).

#include "core/log.h"
#include "gui/gui.h"
#include "gui/view.h"
#include "gui/hotkey.h"
#include "pipe/graph.h"
#include "pipe/graph-history.h"
#include "pipe/modules/api.h"
#include "pipe/res.h"

#include <dirent.h>
#include <limits.h>

#define DT_DRAGKEY_MAX            16
#define DT_DRAGKEY_MAX_COMP        8
#define DT_DRAGKEY_DBLCLICK_WIN  0.4  // seconds: window for double-click reset after commit

typedef struct dt_dragkey_comp_t
{
	dt_token_t module;
	dt_token_t instance;
	dt_token_t param;
	int        component;    // index into parameter array
	float      sensitivity;  // base parameter change per pixel (linear regime)
	// resolved at drag start:
	int        modid;
	int        parid;
	float      start_val;
	float      min_val;     // slider min (for clamping)
	float      max_val;     // slider max (for clamping)
}
dt_dragkey_comp_t;

typedef struct dt_dragkey_t
{
	int    key;              // GLFW key code
	char   name[64];         // display name (from filename)
	dt_dragkey_comp_t comp[DT_DRAGKEY_MAX_COMP];
	int    comp_cnt;
}
dt_dragkey_t;

typedef struct dt_dragkeys_t
{
	dt_dragkey_t dk[DT_DRAGKEY_MAX];
	dt_dragkey_t menu_dk;    // temporary dragkey for menu-triggered adjustments
	int    cnt;
	int    active;           // -1 = none, DT_DRAGKEY_MAX = latched drag active (menu_dk)
	int    latched;          // 1 = latched drag active, click or any key to finish
	double start_x;          // mouse x when drag began
	double commit_time;      // glfwGetTime() of last commit (for double-click reset)
}
dt_dragkeys_t;

static inline int
dt_dragkey_load_file(dt_dragkey_t *dk, const char *path, const char *basename)
{
	FILE *f = fopen(path, "rb");
	if(!f) return 1;
	dk->key = 0;
	dk->comp_cnt = 0;
	snprintf(dk->name, sizeof(dk->name), "%s", basename);
	char line[256];
	while(fgets(line, sizeof(line), f))
	{
		char *nl = strchr(line, '\n');
		if(nl) *nl = 0;
		if(line[0] == '#' || line[0] == 0) continue;
		if(!strncmp(line, "key:", 4))
		{
			dk->key = hk_name_to_glfw(line + 4);
		}
		else
		{ // module:instance:param:component:sensitivity
			if(dk->comp_cnt >= DT_DRAGKEY_MAX_COMP) continue;
			dt_dragkey_comp_t *c = dk->comp + dk->comp_cnt;
			char mod[9] = {0}, inst[9] = {0}, par[9] = {0};
			int comp = 0;
			float sens = 0.002f;
			if(sscanf(line, "%8[^:]:%8[^:]:%8[^:]:%d:%f", mod, inst, par, &comp, &sens) >= 4)
			{
				c->module    = dt_token(mod);
				c->instance  = dt_token(inst);
				c->param     = dt_token(par);
				c->component = comp;
				c->sensitivity = sens;
				dk->comp_cnt++;
			}
		}
	}
	fclose(f);
	return (dk->key == 0 || dk->comp_cnt == 0) ? 1 : 0;
}

static inline void
dt_dragkeys_init(dt_dragkeys_t *dk)
{
	memset(dk, 0, sizeof(*dk));
	dk->active = -1;
	// load from home dir first (user overrides), then basedir (shipped defaults)
	for(int inbase = 0; inbase < 2; inbase++)
	{
		void *dirp = dt_res_opendir("dragkeys", inbase);
		if(!dirp) continue;
		const char *basename = 0;
		while((basename = dt_res_next_basename(dirp, inbase)))
		{
			if(basename[0] == '.') continue;
			if(dk->cnt >= DT_DRAGKEY_MAX) break;
			// check for duplicates (home overrides base)
			int dup = 0;
			for(int i = 0; i < dk->cnt; i++)
				if(!strcmp(dk->dk[i].name, basename)) { dup = 1; break; }
			if(dup) continue;
			char path[PATH_MAX];
			snprintf(path, sizeof(path), "%s/dragkeys/%s",
					inbase ? dt_pipe.basedir : dt_pipe.homedir, basename);
			if(!dt_dragkey_load_file(dk->dk + dk->cnt, path, basename))
			{
				dt_log(s_log_gui, "[dragkey] loaded '%s' key=%d components=%d",
						basename, dk->dk[dk->cnt].key, dk->dk[dk->cnt].comp_cnt);
				dk->cnt++;
			}
		}
		dt_res_closedir(dirp, inbase);
	}
}

static inline void
dt_dragkeys_cleanup(dt_dragkeys_t *dk)
{
	dk->cnt = 0;
	dk->active = -1;
	dk->latched = 0;
	dk->commit_time = 0.0;
}

static inline void
dt_dragkey_commit(dt_dragkey_t *d)
{
	for(int j = 0; j < d->comp_cnt; j++)
	{
		dt_dragkey_comp_t *c = d->comp + j;
		if(c->modid >= 0 && c->parid >= 0)
			dt_graph_history_append(&vkdt.graph_dev, c->modid, c->parid, 0.0);
	}
}

// restore all params to their start values (cancel)
static inline void
dt_dragkey_restore(dt_dragkey_t *d)
{
	for(int j = 0; j < d->comp_cnt; j++)
	{
		dt_dragkey_comp_t *c = d->comp + j;
		if(c->modid < 0 || c->parid < 0) continue;
		float *val = (float *)(vkdt.graph_dev.module[c->modid].param
				+ vkdt.graph_dev.module[c->modid].so->param[c->parid]->offset);
		val[c->component] = c->start_val;
	}
	vkdt.graph_dev.runflags = s_graph_run_record_cmd_buf;
}

// reset all params to their module default values
static inline void
dt_dragkey_reset(dt_dragkey_t *d)
{
	for(int j = 0; j < d->comp_cnt; j++)
	{
		dt_dragkey_comp_t *c = d->comp + j;
		if(c->modid < 0 || c->parid < 0) continue;
		const dt_ui_param_t *pp = vkdt.graph_dev.module[c->modid].so->param[c->parid];
		float *val = (float *)(vkdt.graph_dev.module[c->modid].param + pp->offset);
		val[c->component] = ((const float *)pp->val)[c->component];
	}
	// append history once per (modid, parid) pair
	for(int j = 0; j < d->comp_cnt; j++)
	{
		dt_dragkey_comp_t *c = d->comp + j;
		if(c->modid < 0 || c->parid < 0) continue;
		int dup = 0;
		for(int k = 0; k < j; k++)
			if(d->comp[k].modid == c->modid && d->comp[k].parid == c->parid) { dup = 1; break; }
		if(!dup) dt_graph_history_append(&vkdt.graph_dev, c->modid, c->parid, 0.0);
	}
	vkdt.graph_dev.runflags = s_graph_run_all;
	vkdt.wstate.busy += 2;
}

// called from darkroom_keyboard. returns 1 if the event was consumed.
static inline int
dt_dragkey_keyboard(dt_dragkeys_t *dk, int key, int action, int mods)
{
	if(action == GLFW_PRESS && dk->active < 0)
	{
		for(int i = 0; i < dk->cnt; i++)
		{
			if(dk->dk[i].key != key) continue;
			dt_dragkey_t *d = dk->dk + i;
			if(mods & GLFW_MOD_SHIFT) { // shift+key: reset to module defaults
				for(int j = 0; j < d->comp_cnt; j++)
				{
					dt_dragkey_comp_t *c = d->comp + j;
					c->modid = dt_module_get(&vkdt.graph_dev, c->module, c->instance);
					if(c->modid < 0) continue;
					c->parid = dt_module_get_param(vkdt.graph_dev.module[c->modid].so, c->param);
				}
				dt_dragkey_reset(d);
				dt_gui_notification("%s: reset to default", d->name);
				return 1;
			}
			// activate latched drag: press once, drag, click or any key to commit
			dk->menu_dk = *d;
			dt_dragkey_t *md = &dk->menu_dk;
			int ok = 0;
			for(int j = 0; j < md->comp_cnt; j++)
			{
				dt_dragkey_comp_t *c = md->comp + j;
				c->modid = dt_module_get(&vkdt.graph_dev, c->module, c->instance);
				if(c->modid < 0) continue;
				c->parid = dt_module_get_param(vkdt.graph_dev.module[c->modid].so, c->param);
				if(c->parid < 0) continue;
				const float *val = dt_module_param_float(vkdt.graph_dev.module + c->modid, c->parid);
				if(!val) continue;
				c->start_val = val[c->component];
				const dt_ui_param_t *pp = vkdt.graph_dev.module[c->modid].so->param[c->parid];
				c->min_val = pp->widget.min;
				c->max_val = pp->widget.max;
				ok = 1;
			}
			if(!ok) return 0;
			vkdt.wstate.pending_modid = md->comp[0].modid;
			double mx, my;
			dt_view_get_cursor_pos(vkdt.win.window, &mx, &my);
			dk->start_x = mx;
			dk->active = DT_DRAGKEY_MAX;
			dk->latched = 1;
			dt_gui_notification("%s: drag to adjust -- click or any key to confirm, esc to cancel", md->name);
			return 1;
		}
	}
	else if(dk->active == DT_DRAGKEY_MAX && dk->latched) { // latched mode: any key commits
		if(action == GLFW_PRESS) {
			if(key == GLFW_KEY_ESCAPE) {
				dt_dragkey_restore(&dk->menu_dk);
				dk->commit_time = 0.0;
			} else {
				dt_dragkey_commit(&dk->menu_dk);
				dk->commit_time = glfwGetTime();
			}
			dk->active = -1;
			dk->latched = 0;
			return 1;
		}
		return 1; // consume all key events while latched
	}
	return 0;
}

// called from darkroom_mouse_position. returns 1 if drag is active.
static inline int
dt_dragkey_mouse_move(dt_dragkeys_t *dk, double x)
{
	if(dk->active < 0) return 0;
	dt_dragkey_t *d = &dk->menu_dk;
	float delta = (float)(x - dk->start_x);
	char msg[128];
	int off = 0;
	off += snprintf(msg + off, sizeof(msg) - off, "%s:", d->name);
	for(int j = 0; j < d->comp_cnt; j++)
	{
		dt_dragkey_comp_t *c = d->comp + j;
		if(c->modid < 0 || c->parid < 0) continue;
		float new_val = c->start_val + delta * c->sensitivity;
		if(c->min_val < c->max_val)
		{ // clamp to slider range
			if(new_val < c->min_val) new_val = c->min_val;
			if(new_val > c->max_val) new_val = c->max_val;
		}
		float *val = (float *)(vkdt.graph_dev.module[c->modid].param
				+ vkdt.graph_dev.module[c->modid].so->param[c->parid]->offset);
		val[c->component] = new_val;
		if(off < (int)sizeof(msg) - 20)
			off += snprintf(msg + off, sizeof(msg) - off, " %.3f", new_val);
	}
	vkdt.graph_dev.runflags = s_graph_run_all;
	dt_gui_notification("%s", msg);
	return 1;
}

// called from darkroom_mouse_button. returns 1 if latched drag was committed.
static inline int
dt_dragkey_mouse_button(dt_dragkeys_t *dk, int button, int action)
{
	if(button == 0 && action == GLFW_PRESS && dk->active < 0
	   && dk->commit_time > 0.0 && (glfwGetTime() - dk->commit_time) < DT_DRAGKEY_DBLCLICK_WIN) { // double-click to reset: second left-click within window after a commit
		dk->commit_time = 0.0;
		dt_dragkey_reset(&dk->menu_dk);
		dt_gui_notification("%s: reset to default", dk->menu_dk.name);
		return 1;
	}
	if(dk->active < 0 || !dk->latched) return 0;
	if(button == 0 && action == GLFW_PRESS)
	{ // left click: commit
		dt_dragkey_commit(&dk->menu_dk);
		dk->active = -1;
		dk->latched = 0;
		dk->commit_time = glfwGetTime();
		return 1;
	}
	if(button == 1 && action == GLFW_PRESS)
	{ // right click: cancel
		dt_dragkey_restore(&dk->menu_dk);
		dk->active = -1;
		dk->latched = 0;
		dk->commit_time = 0.0;
		return 1;
	}
	return 1; // consume all mouse buttons while latched
}

// programmatically activate a drag for a single parameter component.
// the drag is latched: mouse-move adjusts, click commits, escape/right-click cancels.
static inline int
dt_dragkey_activate(
		dt_dragkeys_t *dk,
		dt_token_t     mod,
		dt_token_t     inst,
		dt_token_t     param,
		int            component)
{
	dt_dragkey_t *d = &dk->menu_dk;
	d->comp_cnt = 1;
	d->comp[0].module    = mod;
	d->comp[0].instance  = inst;
	d->comp[0].param     = param;
	d->comp[0].component = component;
	int modid = dt_module_get(&vkdt.graph_dev, mod, inst);
	if(modid < 0) return 1;
	int parid = dt_module_get_param(vkdt.graph_dev.module[modid].so, param);
	if(parid < 0) return 1;
	if(component < 0 || component >= vkdt.graph_dev.module[modid].so->param[parid]->cnt) return 1;
	d->comp[0].modid = modid;
	d->comp[0].parid = parid;
	const float *val = dt_module_param_float(vkdt.graph_dev.module + modid, parid);
	if(!val) return 1;
	d->comp[0].start_val = val[component];
	// compute sensitivity from slider range and image width
	const dt_ui_param_t *pp = vkdt.graph_dev.module[modid].so->param[parid];
	d->comp[0].min_val = pp->widget.min;
	d->comp[0].max_val = pp->widget.max;
	float range = pp->widget.max - pp->widget.min;
	if(range <= 0) range = 1.0f;
	float width = vkdt.state.center_wd > 0 ? vkdt.state.center_wd : 1000.0f;
	d->comp[0].sensitivity = range / width;
	snprintf(d->name, sizeof(d->name), "%.8s:%.8s",
			dt_token_str(mod), dt_token_str(param));
	double mx, my;
	dt_view_get_cursor_pos(vkdt.win.window, &mx, &my);
	dk->start_x = mx;
	vkdt.wstate.pending_modid = modid;
	dk->active = DT_DRAGKEY_MAX;
	dk->latched = 1;
	return 0;
}

// format a dragkey action string (used by menu provider)
static inline void
dt_dragkey_format_action(char *buf, int bufsz,
		dt_token_t mod, dt_token_t inst, dt_token_t param, int component)
{
	snprintf(buf, bufsz, "dragkey:%.8s:%.8s:%.8s:%d",
			dt_token_str(mod), dt_token_str(inst),
			dt_token_str(param), component);
}

// parse a dragkey action string and activate. returns 0 on success.
static inline int
dt_dragkey_parse_and_activate(dt_dragkeys_t *dk, const char *action)
{
	char mod[9] = {0}, inst[9] = {0}, par[9] = {0};
	int comp = 0;
	if(sscanf(action + 8, "%8[^:]:%8[^:]:%8[^:]:%d", mod, inst, par, &comp) < 4)
		return 1;
	return dt_dragkey_activate(dk, dt_token(mod), dt_token(inst), dt_token(par), comp);
}
