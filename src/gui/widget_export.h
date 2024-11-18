#pragma once

// this is almost the same as the widgets in darkroom mode.
// however, this runs on the abstract mso, without a concrete graph instantiated.
// as such, the parameters are written to pdata and not the params on the graph.
// also, we don't have to trigger a graph re-run etc when widgets are changed.
// export supports only a small subset, so we just duplicate these code paths:
static inline void
export_render_widget(
    const dt_module_so_t *mso,
    int                   parid,
    uint8_t              *pdata)
{
  const dt_ui_param_t *param = mso->param[parid];
  if(!param) return;

  // skip if group mode does not match:
  if(param->widget.grpid != -1)
    if(*(const int32_t *)(pdata + mso->param[param->widget.grpid]->offset) != param->widget.mode)
      return;

  // TODO: bring back somehow
  // static int gamepad_reset = 0;
  // if(ImGui::IsKeyPressed(ImGuiKey_GamepadR3)) gamepad_reset = 1;
  // XXX in fact, ctx->delta_time_seconds needs to be set by us from the outside!

  const float ratio[] = {0.7f, 0.3f};
  const float row_height = vkdt.ctx.style.font->height + 2 * vkdt.ctx.style.tab.padding.y;
  nk_layout_row(&vkdt.ctx, NK_DYNAMIC, row_height, 2, ratio);

  char str[10] = {0};
  memcpy(str, &param->name, 8);
  const dt_token_t widget = param->widget.type;
  if(widget == dt_token("slider"))
  { // distinguish by type:
    if(param->type == dt_token("float"))
    {
      float *val = (float*)(pdata + param->offset);
      dt_tooltip(param->tooltip);
      nk_slider_float(&vkdt.ctx, param->widget.min, val, param->widget.max, 0.01);
      if(nk_widget_is_mouse_clicked(&vkdt.ctx, NK_BUTTON_DOUBLE))
        memcpy(pdata + param->offset, param->val, dt_ui_param_size(param->type, param->cnt));
      nk_label(&vkdt.ctx, str, NK_TEXT_LEFT);
    }
    else if(param->type == dt_token("int"))
    {
      int32_t *val = (int32_t*)(pdata + param->offset);
      dt_tooltip(param->tooltip);
      nk_slider_int(&vkdt.ctx, param->widget.min, val, param->widget.max, 1);
      if(nk_widget_is_mouse_clicked(&vkdt.ctx, NK_BUTTON_DOUBLE))
        memcpy(pdata + param->offset, param->val, dt_ui_param_size(param->type, param->cnt));
      nk_label(&vkdt.ctx, str, NK_TEXT_LEFT);
    }
  }
  else if(widget == dt_token("combo"))
  { // combo box
    if(param->type == dt_token("int"))
    {
      int32_t *val = (int32_t*)(pdata + param->offset);
      struct nk_vec2 size = { ratio[0]*vkdt.state.panel_wd, ratio[0]*vkdt.state.panel_wd };
      nk_combobox_string(&vkdt.ctx, (const char *)param->widget.data, val, 0xffff, row_height, size);
      if(nk_widget_is_mouse_clicked(&vkdt.ctx, NK_BUTTON_DOUBLE))
        memcpy(pdata + param->offset, param->val, dt_ui_param_size(param->type, param->cnt));
      dt_tooltip(param->tooltip); // combo boxes can't have tooltips, or else the popup doesn't work any more
      nk_label(&vkdt.ctx, str, NK_TEXT_LEFT);
    }
  }
  else if(widget == dt_token("filename"))
  {
    char *v = (char *)(pdata + param->offset);
    dt_tooltip(param->tooltip);
    nk_edit_string_zero_terminated(&vkdt.ctx, NK_EDIT_FIELD, v, param->cnt, nk_filter_default);
    if(nk_widget_is_mouse_clicked(&vkdt.ctx, NK_BUTTON_DOUBLE))
      memcpy(pdata + param->offset, param->val, dt_ui_param_size(param->type, param->cnt));
    nk_label(&vkdt.ctx, str, NK_TEXT_LEFT);
  }
}

typedef struct dt_export_widget_t
{ // this memory belongs to the gui and export threads need to copy it.
  int      modid_cnt;   // number of output modules found
  int     *modid;       // points into dt_pipe.module, lists all output modules
  char    *format_text; // output module selection text for imgui combobox
  uint8_t *pdata_buf;   // allocation for all the parameters
  uint8_t **pdata;      // pointer to parameter data for all output modules

  // stuff that is selected in the gui:
  char    basename[240];
  int     wd, ht;
  int     format;
  float   quality;
  int     overwrite;
  int     last_frame_only;
  int     colour_prim;
  int     colour_trc;
} dt_export_widget_t;

static inline void
dt_export_init(
    dt_export_widget_t *w)
{
  size_t pdata_size = 0;
  w->modid = (int *)malloc(sizeof(int)*dt_pipe.num_modules);
  w->modid_cnt = 0;
  for(uint32_t i=0;i<dt_pipe.num_modules;i++)
  {
    if(!strncmp(dt_token_str(dt_pipe.module[i].name), "o-", 2))
    {
      w->modid[w->modid_cnt++] = i;
      pdata_size += dt_module_total_param_size(i);
    }
  }

  w->pdata = (uint8_t **)malloc(sizeof(uint8_t *)*w->modid_cnt);
  w->pdata_buf = (uint8_t *)malloc(sizeof(uint8_t)*pdata_size);
  w->format_text = (char *)calloc(sizeof(char), (w->modid_cnt*9 + 1));
  char *c = w->format_text;
  pdata_size = 0;
  for(int i=0;i<w->modid_cnt;i++)
  {
    const dt_module_so_t *mso = dt_pipe.module + w->modid[i];
    w->pdata[i] = w->pdata_buf + pdata_size;
    for(int p=0;p<mso->num_params;p++)
    { // init default params
      dt_ui_param_t *pp = mso->param[p];
      memcpy(w->pdata[i] + pp->offset, pp->val, dt_ui_param_size(pp->type, pp->cnt));
    }
    pdata_size += dt_module_total_param_size(w->modid[i]);
    int num = snprintf(c, 9, "%" PRItkn, dt_token_str(mso->name));
    c += num + 1;
  }

  w->wd          = dt_rc_get_int(&vkdt.rc, "gui/export/wd", 0);
  w->ht          = dt_rc_get_int(&vkdt.rc, "gui/export/ht", 0);
  w->format      = dt_rc_get_int(&vkdt.rc, "gui/export/format", 4);
  w->quality     = dt_rc_get_float(&vkdt.rc, "gui/export/quality", 90);
  w->colour_prim = dt_rc_get_int(&vkdt.rc, "gui/export/primaries", s_colour_primaries_srgb);
  w->colour_trc  = dt_rc_get_int(&vkdt.rc, "gui/export/trc", s_colour_trc_srgb);
  strncpy(w->basename,
        dt_rc_get(&vkdt.rc, "gui/export/basename", "${home}/img_${seq}"),
        sizeof(w->basename)-1);
  w->last_frame_only = 0;
}

static inline void
dt_export_cleanup(
    dt_export_widget_t *w)
{
  free(w->pdata);
  free(w->pdata_buf);
  free(w->format_text);
  memset(w, 0, sizeof(*w));
}

static inline void
dt_export(
    dt_export_widget_t *w)
{
  if(!w->modid_cnt) dt_export_init(w);

  struct nk_context *ctx = &vkdt.ctx;
  const float row_height = ctx->style.font->height + 2 * ctx->style.tab.padding.y;
  const float ratio[] = {0.7f, 0.3f};
  nk_layout_row(&vkdt.ctx, NK_DYNAMIC, row_height, 2, ratio);
  int resi;
  float resf;

#if 0
  // TODO: wrap into FOCUS_LIST_FIRST and FOCUS_LIST_LAST and pre/post in between macros
  // focus_group_first
  static int focus_id_next = -1; // target to jump to with focus
  int focus_id_curr = 0; // counter which focus-receiver is next
  static double time_tab;
  double time_now = glfwGetTime();
  int tab_keypress = 0;
  if(glfwGetKey(vkdt.win.window, GLFW_KEY_TAB) == GLFW_PRESS && (time_now - time_tab > 0.2))
  {
    time_tab = time_now;
    tab_keypress = 1;
  }
  // property here
  // end
#endif

  resi = w->wd;
#define focus_group_property_first(TYPE, CTX, NAME, m, V, M, I1, I2)\
  static int focus_id_next = -1;\
  int focus_id_curr = 0; \
  static double time_tab; \
  double time_now = glfwGetTime(); \
  int tab_keypress = 0; \
  if(glfwGetKey(vkdt.win.window, GLFW_KEY_TAB) == GLFW_PRESS && (time_now - time_tab > 0.2)) \
  { \
    time_tab = time_now; \
    tab_keypress = 1; \
  } \
  do {\
    if(focus_id_curr++ == focus_id_next) { nk_property_focus(CTX); focus_id_next = -1; }\
    nk_property_ ## TYPE(CTX, NAME, m, V, M, I1, I2);\
    int adv = nk_property_## TYPE ##_unfocus(CTX, NAME, m, V, M, I1, tab_keypress);\
    if(adv) { tab_keypress = adv = 0; focus_id_next = focus_id_curr; }\
  } while(0)
#define focus_group_property(TYPE, CTX, NAME, m, V, M, I1, I2)\
  do {\
    if(focus_id_curr++ == focus_id_next) { nk_property_focus(CTX); focus_id_next = -1; }\
    nk_property_ ## TYPE(CTX, NAME, m, V, M, I1, I2);\
    int adv = nk_property_## TYPE ##_unfocus(CTX, NAME, m, V, M, I1, tab_keypress);\
    if(adv) { tab_keypress = adv = 0; focus_id_next = focus_id_curr; }\
  } while(0)
#define focus_group_property_last(TYPE, CTX, NAME, m, V, M, I1, I2)\
  do {\
    if(focus_id_curr++ == focus_id_next) { nk_property_focus(CTX); focus_id_next = -1; }\
    nk_property_ ## TYPE(CTX, NAME, m, V, M, I1, I2);\
    int adv = nk_property_## TYPE ##_unfocus(CTX, NAME, m, V, M, I1, tab_keypress);\
    if(adv) { tab_keypress = adv = 0; focus_id_next = 0; }\
  } while(0)

#if 0
  if(focus_id_curr++ == focus_id_next) { nk_property_focus(ctx); focus_id_next = -1; }
  nk_property_int(ctx, "#", 0, &resi, 65535, 1, 1);
  int adv = nk_property_int_unfocus(ctx, "#", 0, &resi, 65535, 1, tab_keypress);
  if(adv) { tab_keypress = adv = 0; focus_id_next = focus_id_curr; }
#endif
  focus_group_property_first(int, ctx, "#", 0, &resi, 65535, 1, 1);

  if(resi != w->wd) dt_rc_set_int(&vkdt.rc, "gui/export/wd", (w->wd = resi));
  nk_label(ctx, "width", NK_TEXT_LEFT);

  resi = w->ht;
#if 0
  if(focus_id_curr++ == focus_id_next) { nk_property_focus(ctx); focus_id_next = -1; }
  nk_property_int(ctx, "#", 0, &resi, 65535, 1, 1);
  adv = nk_property_int_unfocus(ctx, "#", 0, &resi, 65535, 1, tab_keypress);
  if(adv) { tab_keypress = adv = 0; focus_id_next = 0; } // circle back to top
#endif
  focus_group_property_last(int, ctx, "#", 0, &resi, 65535, 1, 1);

  if(resi != w->ht) dt_rc_set_int(&vkdt.rc, "gui/export/ht", (w->ht = resi));
  nk_label(ctx, "height", NK_TEXT_LEFT);

  dt_tooltip(
      "basename of exported files. the following will be replaced:\n"
      "${home} -- home directory\n"
      "${yyyy} -- current year\n"
      "${date} -- today's date\n"
      "${seq} -- sequence number\n"
      "${fdir} -- directory of input file\n"
      "${fbase} -- basename of input file");
  if(nk_edit_string_zero_terminated(ctx, NK_EDIT_FIELD, w->basename, sizeof(w->basename), nk_filter_default))
    dt_rc_set(&vkdt.rc, "gui/export/basename", w->basename);
  nk_label(ctx, "filename", NK_TEXT_LEFT);

  resf = w->quality;
  nk_property_float(ctx, "#", 1, &resf, 100, 1, 0.1);
  if(resf != w->quality) dt_rc_set_float(&vkdt.rc, "gui/export/quality", (w->quality = resf));
  nk_label(ctx, "quality", NK_TEXT_LEFT);

  struct nk_vec2 size = { ratio[0]*vkdt.state.panel_wd, ratio[0]*vkdt.state.panel_wd };
  const char *colour_prim_text = "custom\0sRGB\0rec2020\0AdobeRGB\0P3\0XYZ\0\0";
  const char *colour_trc_text  = "linear\0bt709\0sRGB\0PQ\0DCI\0HLG\0gamma\0mclog\0\0";
  int new_prim = nk_combo_string(ctx, colour_prim_text, w->colour_prim, 0xffff, row_height, size);
  if(new_prim != w->colour_prim)
  {
    w->colour_prim = new_prim;
    dt_rc_set_int(&vkdt.rc, "gui/export/primaries", w->colour_prim);
  }
  nk_label(ctx, "primaries", NK_TEXT_LEFT);
  int new_trc  = nk_combo_string(ctx, colour_trc_text, w->colour_trc, 0xffff, row_height, size);
  if(new_trc != w->colour_trc)
  {
    w->colour_trc = new_trc;
    dt_rc_set_int(&vkdt.rc, "gui/export/trc", w->colour_trc);
  }
  nk_label(ctx, "trc", NK_TEXT_LEFT);

  w->last_frame_only = nk_combo_string(ctx, "export every frame\0export last frame only\0\0", w->last_frame_only, 0xffff, row_height, size);
  nk_label(ctx, "animations", NK_TEXT_LEFT);

  // TODO: this is not wired in the backend so hidden from gui for now too:
  // const char *overwrite_mode_str = "keep\0overwrite\0\0";
  // combo "existing files", &overwrite_mode, overwrite_mode_str;

  int new_format = nk_combo_string(ctx, w->format_text, w->format, 0xffff, row_height, size);
  if(new_format != w->format) dt_rc_set_int(&vkdt.rc, "gui/export/format", (w->format = new_format));
  nk_label(ctx, "format", NK_TEXT_LEFT);

  w->format = CLAMP(w->format, 0, w->modid_cnt-1);
  const int id = CLAMP(w->modid[w->format], 0l, dt_pipe.num_modules-1l);
  const dt_module_so_t *mso = dt_pipe.module+id;
  for(int i=0;i<mso->num_params;i++)
  {
    if(mso->param[i]->name == dt_token("filename") || // skip, these are handled globally
       mso->param[i]->name == dt_token("quality")) continue;
    export_render_widget(mso, i, w->pdata[w->format]);
  }
}
