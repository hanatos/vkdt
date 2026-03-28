#pragma once
// drag-to-adjust: select a parameter via the chord menu (or press its key),
// then click and drag in the image area to change it.  right-click or any key
// confirms; esc restores to the value at arm time.  double-click resets to the
// module default.  config files live in bin/dragkeys/ and ~/.config/vkdt/dragkeys/.
// format:
//   # comment
//   key:<keyname>
//   <module>:<instance>:<param>:<component>:<sensitivity>
//   ...
// multiple component lines per file are supported (e.g. for white balance).
// sensitivity is the parameter change per pixel of horizontal mouse movement.
// negative sensitivity is valid: it inverts the drag direction for that component
// (used in white balance to move red and blue in opposite directions from one drag).
//
// return value conventions used in this file:
//   operation functions (load, resolve, activate_*): 0 = success, 1 = failure
//   event handler functions (keyboard, mouse_*):     1 = event consumed, 0 = not consumed

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
#define DT_DRAGKEY_CLICK_WIN     0.2  // seconds: max press duration to count as a confirm click
#define DT_DRAGKEY_ARROW_STEP   10.0f // pixels equivalent per arrow key press

typedef struct dt_dragkey_comp_t
{
	dt_token_t module;
	dt_token_t instance;
	dt_token_t param;
	int        component;    // index into parameter array
	float      sensitivity;  // base parameter change per pixel (linear regime)
	// resolved at arm time:
	int        modid;
	int        parid;
	float      arm_val;     // value when armed (for esc restore), never updated mid-session
	float      start_val;   // value at start of current click-drag segment (re-snapshotted each mousedown)
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
	int    latched;          // 1 = armed: click-drag adjusts, right-click or any key confirms, esc cancels
	int    dragging;         // 1 = left button currently held (drag segment in progress)
	double start_x;          // mouse x at start of current drag segment (set on each mousedown)
	double mousedown_time;   // glfwGetTime() of last left mousedown (for quick-click confirm)
	double commit_time;      // glfwGetTime() of last commit (for post-confirm double-click reset)
	double mouseup_time;     // glfwGetTime() of last left mouseup (for in-session double-click reset)
	int    key_dec[2];       // keys for step-decrease: [0]=primary (LEFT), [1]=alt (H)
	int    key_inc[2];       // keys for step-increase: [0]=primary (RIGHT), [1]=alt (L)
}
dt_dragkeys_t;

// parse a dragkey config file into dk. returns 0 on success, 1 if unusable (missing key or components).
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
				if(comp < 0 || comp >= DT_DRAGKEY_MAX_COMP)
				{
					dt_log(s_log_err, "[dragkey] component index %d out of range, skipping line", comp);
					continue;
				}
				c->module      = dt_token(mod);
				c->instance    = dt_token(inst);
				c->param       = dt_token(par);
				c->component   = comp;
				c->sensitivity = sens;
				dk->comp_cnt++;
			}
		}
	}
	fclose(f);
	return (dk->key == 0 || dk->comp_cnt == 0) ? 1 : 0;
}

// resolve module and parameter IDs on all components. returns 1 if no component resolved.
static inline int
dt_dragkey_resolve(dt_dragkey_t *d)
{
	int ok = 0;
	for(int j = 0; j < d->comp_cnt; j++)
	{
		dt_dragkey_comp_t *c = d->comp + j;
		c->modid = dt_module_get(&vkdt.graph_dev, c->module, c->instance);
		if(c->modid < 0) { c->parid = -1; continue; }
		c->parid = dt_module_get_param(vkdt.graph_dev.module[c->modid].so, c->param);
		if(c->parid >= 0)
		{
			int cnt = vkdt.graph_dev.module[c->modid].so->param[c->parid]->cnt;
			if(c->component >= cnt)
			{
				dt_log(s_log_err, "[dragkey] component %d >= param count %d, skipping", c->component, cnt);
				c->parid = -1;
				continue;
			}
			ok = 1;
		}
	}
	return ok ? 0 : 1;
}

static inline void
dt_dragkeys_init(dt_dragkeys_t *dk)
{
	memset(dk, 0, sizeof(*dk));
	// default step keys (rebindable via hotkey system, see render_darkroom.c)
	dk->key_dec[0] = GLFW_KEY_LEFT;
	dk->key_dec[1] = GLFW_KEY_H;
	dk->key_inc[0] = GLFW_KEY_RIGHT;
	dk->key_inc[1] = GLFW_KEY_L;
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
dt_dragkey_commit(dt_dragkey_t *d)
{
	for(int j = 0; j < d->comp_cnt; j++)
	{
		dt_dragkey_comp_t *c = d->comp + j;
		if(c->modid >= 0 && c->parid >= 0)
			dt_graph_history_append(&vkdt.graph_dev, c->modid, c->parid, 0.0);
	}
}

// restore all params to arm-time values (esc cancel)
static inline void
dt_dragkey_restore(dt_dragkey_t *d)
{
	for(int j = 0; j < d->comp_cnt; j++)
	{
		dt_dragkey_comp_t *c = d->comp + j;
		if(c->modid < 0 || c->parid < 0) continue;
		float *val = (float *)(vkdt.graph_dev.module[c->modid].param
				+ vkdt.graph_dev.module[c->modid].so->param[c->parid]->offset);
		val[c->component] = c->arm_val;
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

// shared teardown for all latched-mode exit paths.
// commit=1: append history, enable post-confirm double-click reset.
// commit=0: restore arm-time values, disable post-confirm double-click.
static inline void
dt_dragkeys_exit(dt_dragkeys_t *dk, int commit)
{
	if(commit)
	{
		dt_dragkey_commit(&dk->menu_dk);
		dk->commit_time = glfwGetTime();
	}
	else
	{
		dt_dragkey_restore(&dk->menu_dk);
		dk->commit_time = 0.0;
	}
	if(dk->dragging)
		glfwSetInputMode(vkdt.win.window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
	dk->latched        = 0;
	dk->dragging       = 0;
	dk->mousedown_time = 0.0;
	dk->mouseup_time   = 0.0;
	vkdt.wstate.dragkey_latched = 0;
}

static inline void
dt_dragkeys_cleanup(dt_dragkeys_t *dk)
{
	if(dk->latched) dt_dragkey_restore(&dk->menu_dk);
	if(dk->dragging)
		glfwSetInputMode(vkdt.win.window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
	dk->cnt            = 0;
	dk->latched        = 0;
	dk->dragging       = 0;
	dk->mousedown_time = 0.0;
	dk->commit_time    = 0.0;
	dk->mouseup_time   = 0.0;
	vkdt.wstate.dragkey_latched = 0;
}

static inline void
dt_dragkeys_cancel(dt_dragkeys_t *dk)
{
	if(dk->latched) dt_dragkeys_exit(dk, 0);
}

// copy d to menu_dk, resolve module/param IDs, snapshot arm values, and arm latched drag.
// returns 0 on success, 1 if no eligible component found.
static inline int
dt_dragkey_activate_dk(dt_dragkeys_t *dk, dt_dragkey_t *d)
{
	dk->menu_dk = *d;
	dt_dragkey_t *md = &dk->menu_dk;
	if(dt_dragkey_resolve(md)) return 1;
	int ok = 0;
	for(int j = 0; j < md->comp_cnt; j++)
	{
		dt_dragkey_comp_t *c = md->comp + j;
		if(c->modid < 0 || c->parid < 0) continue;
		const float *val = dt_module_param_float(vkdt.graph_dev.module + c->modid, c->parid);
		if(!val) continue;
		c->arm_val   = val[c->component];
		c->start_val = c->arm_val;
		const dt_ui_param_t *pp = vkdt.graph_dev.module[c->modid].so->param[c->parid];
		c->min_val = pp->widget.min;
		c->max_val = pp->widget.max;
		ok = 1;
	}
	if(!ok) return 1;
	vkdt.wstate.pending_modid = -1;
	for(int j = 0; j < md->comp_cnt; j++)
		if(md->comp[j].modid >= 0) { vkdt.wstate.pending_modid = md->comp[j].modid; break; }
	dk->latched      = 1;
	dk->dragging     = 0;
	dk->mouseup_time = 0.0;
	vkdt.wstate.dragkey_latched = 1;
	return 0;
}

// activate a loaded dragkey by name. returns 0 on success, 1 if not found.
static inline int
dt_dragkey_activate_named(dt_dragkeys_t *dk, const char *name)
{
	for(int i = 0; i < dk->cnt; i++)
		if(!strcmp(dk->dk[i].name, name))
			return dt_dragkey_activate_dk(dk, dk->dk + i);
	return 1;
}

// called from darkroom_keyboard. returns 1 if the event was consumed.
static inline int
dt_dragkey_keyboard(dt_dragkeys_t *dk, int key, int action, int mods)
{
	if(action == GLFW_PRESS && !dk->latched)
	{
		for(int i = 0; i < dk->cnt; i++)
		{
			if(dk->dk[i].key != key) continue;
			dt_dragkey_t *d = dk->dk + i;
			if(mods & GLFW_MOD_SHIFT)
			{ // shift+key: reset to module defaults
				dt_dragkey_t tmp = *d;
				if(!dt_dragkey_resolve(&tmp))
				{
					dt_dragkey_reset(&tmp);
					dt_gui_notification("%s: reset to default", d->name);
				}
				return 1;
			}
			// activate: click and drag to adjust, right-click or any key to confirm
			if(!dt_dragkey_activate_dk(dk, d))
			{
				dt_gui_notification("%s: click and drag to adjust -- right-click or any key to confirm, esc to cancel", dk->menu_dk.name);
				return 1;
			}
			return 0;
		}
	}
	else if(dk->latched)
	{ // armed: step keys adjust, any other key-press confirms (esc cancels)
		int is_dec = (key == dk->key_dec[0] || key == dk->key_dec[1]);
		int is_inc = (key == dk->key_inc[0] || key == dk->key_inc[1]);
		if((action == GLFW_PRESS || action == GLFW_REPEAT) && (is_dec || is_inc))
		{ // step the parameter and stay armed (hold for repeat)
			float dir = is_inc ? 1.0f : -1.0f;
			dt_dragkey_t *d = &dk->menu_dk;
			char msg[128];
			int off = snprintf(msg, sizeof(msg), "%s:", d->name);
			for(int j = 0; j < d->comp_cnt; j++)
			{
				dt_dragkey_comp_t *c = d->comp + j;
				if(c->modid < 0 || c->parid < 0) continue;
				float *val = (float *)(vkdt.graph_dev.module[c->modid].param
						+ vkdt.graph_dev.module[c->modid].so->param[c->parid]->offset);
				float new_val = val[c->component] + dir * c->sensitivity * DT_DRAGKEY_ARROW_STEP;
				if(c->min_val < c->max_val)
				{
					if(new_val < c->min_val) new_val = c->min_val;
					if(new_val > c->max_val) new_val = c->max_val;
				}
				val[c->component] = new_val;
				if(off < (int)sizeof(msg) - 20)
					off += snprintf(msg + off, sizeof(msg) - off, " %.3f", new_val);
			}
			vkdt.graph_dev.runflags = s_graph_run_all;
			dt_gui_notification("%s", msg);
			return 1;
		}
		if(action == GLFW_PRESS)
		{ // any other key: confirm (esc = cancel/restore)
			dt_dragkeys_exit(dk, key != GLFW_KEY_ESCAPE);
			return 1;
		}
		return 1; // consume all key events while latched
	}
	return 0;
}

// called from darkroom_mouse_position. returns 1 if armed (consuming mouse events).
static inline int
dt_dragkey_mouse_move(dt_dragkeys_t *dk, double x)
{
	if(!dk->latched) return 0;
	if(!dk->dragging) return 1; // armed but waiting for click: consume, don't adjust
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

// called from darkroom_mouse_button. returns 1 if event was consumed.
static inline int
dt_dragkey_mouse_button(dt_dragkeys_t *dk, int button, int action)
{
	// post-confirm double-click: left-press shortly after exiting armed mode resets to default
	if(button == 0 && action == GLFW_PRESS && !dk->latched
		 && dk->commit_time > 0.0 && (glfwGetTime() - dk->commit_time) < DT_DRAGKEY_DBLCLICK_WIN)
	{
		dk->commit_time = 0.0;
		dt_dragkey_reset(&dk->menu_dk);
		dt_gui_notification("%s: reset to default", dk->menu_dk.name);
		return 1;
	}
	if(!dk->latched) return 0;
	if(button == 1 && action == GLFW_PRESS)
	{ // right click: confirm and exit
		dt_dragkeys_exit(dk, 1);
		return 1;
	}
	if(button == 0 && action == GLFW_PRESS)
	{
		// in-session double-click: reset to module default, stay armed
		if(dk->mouseup_time > 0.0 && (glfwGetTime() - dk->mouseup_time) < DT_DRAGKEY_DBLCLICK_WIN)
		{
			dt_dragkey_reset(&dk->menu_dk);
			// re-snapshot arm_val to the new defaults so esc restores to them
			for(int j = 0; j < dk->menu_dk.comp_cnt; j++)
			{
				dt_dragkey_comp_t *c = dk->menu_dk.comp + j;
				if(c->modid < 0 || c->parid < 0) continue;
				const float *val = dt_module_param_float(vkdt.graph_dev.module + c->modid, c->parid);
				if(val) c->arm_val = val[c->component];
			}
			dk->mouseup_time = 0.0;
			dt_gui_notification("%s: reset to default", dk->menu_dk.name);
			return 1;
		}
		// begin a new drag segment: snapshot current param values as per-drag anchors
		for(int j = 0; j < dk->menu_dk.comp_cnt; j++)
		{
			dt_dragkey_comp_t *c = dk->menu_dk.comp + j;
			if(c->modid < 0 || c->parid < 0) continue;
			const float *val = dt_module_param_float(vkdt.graph_dev.module + c->modid, c->parid);
			if(val) c->start_val = val[c->component];
		}
		double mx, my;
		dt_view_get_cursor_pos(vkdt.win.window, &mx, &my);
		dk->start_x = mx;
		dk->mousedown_time = glfwGetTime();
		dk->dragging = 1;
		glfwSetInputMode(vkdt.win.window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
		return 1;
	}
	if(button == 0 && action == GLFW_RELEASE)
	{
		glfwSetInputMode(vkdt.win.window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
		if((glfwGetTime() - dk->mousedown_time) < DT_DRAGKEY_CLICK_WIN)
		{ // quick click (no real drag): confirm and exit
			dt_dragkeys_exit(dk, 1);
			return 1;
		}
		// slow release = real drag: freeze value, stay armed for another drag
		dk->dragging     = 0;
		dk->mouseup_time = glfwGetTime();
		return 1;
	}
	return 1; // consume all other mouse events while latched (middle click etc.)
}

// programmatically activate a drag for a single parameter component.
// arms the click-drag mode: click and drag adjusts, right-click or any key confirms, esc cancels.
// sensitivity is auto-computed from the slider range and current image width.
static inline int
dt_dragkey_activate(
		dt_dragkeys_t *dk,
		dt_token_t     mod,
		dt_token_t     inst,
		dt_token_t     param,
		int            component)
{
	// validate early before building the temp struct
	int modid = dt_module_get(&vkdt.graph_dev, mod, inst);
	if(modid < 0) return 1;
	int parid = dt_module_get_param(vkdt.graph_dev.module[modid].so, param);
	if(parid < 0) return 1;
	if(component < 0 || component >= vkdt.graph_dev.module[modid].so->param[parid]->cnt) return 1;
	// compute sensitivity from slider range and image width
	const dt_ui_param_t *pp = vkdt.graph_dev.module[modid].so->param[parid];
	float range = pp->widget.max - pp->widget.min;
	if(range <= 0) range = 1.0f;
	float width = vkdt.state.center_wd > 0 ? vkdt.state.center_wd : 1000.0f;
	// build a single-component dragkey and delegate to activate_dk
	dt_dragkey_t d = {0};
	d.comp_cnt           = 1;
	d.comp[0].module     = mod;
	d.comp[0].instance   = inst;
	d.comp[0].param      = param;
	d.comp[0].component  = component;
	d.comp[0].sensitivity = range / width;
	snprintf(d.name, sizeof(d.name), "%.8s:%.8s", dt_token_str(mod), dt_token_str(param));
	return dt_dragkey_activate_dk(dk, &d);
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
