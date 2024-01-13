#pragma once

namespace{
inline void
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

  static int gamepad_reset = 0;
  if(ImGui::IsKeyPressed(ImGuiKey_GamepadR3)) gamepad_reset = 1;
  // some state for double click detection for reset functionality
  static int doubleclick = 0;
  static double doubleclick_time = 0;
  double time_now = ImGui::GetTime();
#define RESETBLOCK \
  {\
    if(time_now - doubleclick_time > ImGui::GetIO().MouseDoubleClickTime) doubleclick = 0;\
    if(doubleclick) memcpy(pdata + param->offset, param->val, dt_ui_param_size(param->type, param->cnt));\
    change = 1;\
  }\
  if((ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) || \
     (ImGui::IsItemFocused() && gamepad_reset))\
  {\
    doubleclick_time = time_now;\
    gamepad_reset = 0;\
    doubleclick = 1;\
    memcpy(pdata + param->offset, param->val, dt_ui_param_size(param->type, param->cnt));\
    change = 1;\
  }\
  if(change)

#define TOOLTIP \
  if(param->tooltip && ImGui::IsItemHovered())\
  {\
    ImGui::SetNextWindowViewport(ImGui::GetMainViewport()->ID);\
    ImGui::BeginTooltip();\
    ImGui::PushTextWrapPos(vkdt.state.panel_wd);\
    ImGui::TextUnformatted(param->tooltip);\
    ImGui::PopTextWrapPos();\
    ImGui::EndTooltip();\
  }

  int change = 0;
  ImGui::PushID(parid);
  char str[10] = {0};
  memcpy(str, &param->name, 8);
  switch(param->widget.type)
  { // distinguish by type:
    case dt_token("slider"):
    {
      if(param->type == dt_token("float"))
      {
        float *val = (float*)(pdata + param->offset);
        if(ImGui::SliderFloat(str, val, param->widget.min, param->widget.max, "%2.5f"))
        RESETBLOCK { }
        TOOLTIP
      }
      else if(param->type == dt_token("int"))
      {
        int32_t *val = (int32_t*)(pdata + param->offset);
        if(ImGui::SliderInt(str, val, param->widget.min, param->widget.max, "%d"))
        RESETBLOCK { }
        TOOLTIP
      }
      break;
    }
    case dt_token("combo"):
    { // combo box
      if(param->type == dt_token("int"))
      {
        int32_t *val = (int32_t*)(pdata + param->offset);
        if(ImGui::Combo(str, val, (const char *)param->widget.data))
        RESETBLOCK { }
        TOOLTIP
      }
      break;
    }
    case dt_token("filename"):
    {
      char *v = (char *)(pdata + param->offset);
      ImGui::InputText(str, v, param->cnt);
      TOOLTIP
      break;
    }
    default: break;
  }
  ImGui::PopID();
}
} // end anonymous namespace

struct dt_export_widget_t
{ // this memory belongs to the gui and export threads need to copy it.
  int modid_cnt;       // number of output modules found
  int *modid;          // points into dt_pipe.module, lists all output modules
  char *format_text;   // output module selection text for imgui combobox
  uint8_t *pdata_buf;  // allocation for all the parameters
  uint8_t **pdata;     // pointer to parameter data for all output modules

  // stuff that is selected in the gui:
  char    basename[240];
  int     wd, ht;
  int     format;
  float   quality;
  int     overwrite;
  int     last_frame_only;
};

inline void
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

  w->wd = dt_rc_get_int(&vkdt.rc, "gui/export/wd", 0);
  w->ht = dt_rc_get_int(&vkdt.rc, "gui/export/ht", 0);
  w->format = dt_rc_get_int(&vkdt.rc, "gui/export/format", 2);
  w->quality = dt_rc_get_float(&vkdt.rc, "gui/export/quality", 90);
  strncpy(w->basename,
        dt_rc_get(&vkdt.rc, "gui/export/basename", "${home}/img_${seq}"),
        sizeof(w->basename)-1);
  w->last_frame_only = 0;
}

inline void
dt_export_cleanup(
    dt_export_widget_t *w)
{
  free(w->pdata);
  free(w->pdata_buf);
  free(w->format_text);
  memset(w, 0, sizeof(*w));
}

inline void
dt_export(
    dt_export_widget_t *w)
{
  if(!w->modid_cnt) dt_export_init(w);

  if(ImGui::InputInt("width", &w->wd, 1, 100, 0))
    dt_rc_set_int(&vkdt.rc, "gui/export/wd", w->wd);
  if(ImGui::InputInt("height", &w->ht, 1, 100, 0))
    dt_rc_set_int(&vkdt.rc, "gui/export/ht", w->ht);
  if(ImGui::InputText("filename", w->basename, sizeof(w->basename)))
    dt_rc_set(&vkdt.rc, "gui/export/basename", w->basename);
  if(ImGui::IsItemHovered()) dt_gui_set_tooltip(
      "basename of exported files. the following will be replaced:\n"
      "${home} -- home directory\n"
      "${yyyy} -- current year\n"
      "${date} -- today's date\n"
      "${seq} -- sequence number\n"
      "${fdir} -- directory of input file\n"
      "${fbase} -- basename of input file");
  if(ImGui::InputFloat("quality", &w->quality, 1, 100, 0))
    dt_rc_set_float(&vkdt.rc, "gui/export/quality", w->quality);
  if(ImGui::Combo("format", &w->format, w->format_text))
    dt_rc_set_int(&vkdt.rc, "gui/export/format", w->format);
  ImGui::Combo("animations", &w->last_frame_only, "export every frame\0export last frame only\0\0");
  // TODO: this is not wired in the backend so hidden from gui for now too:
  // const char *overwrite_mode_str = "keep\0overwrite\0\0";
  // ImGui::Combo("existing files", &overwrite_mode, overwrite_mode_str);
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
