#pragma once

  // if(ImGui::BeginPopupModal("apply preset", NULL, ImGuiWindowFlags_AlwaysAutoResize))
  // {
    // uint32_t cid = dt_db_current_imgid(&vkdt.db);
    // if(cid != -1u) dt_db_image_path(&vkdt.db, cid, filename, sizeof(filename));
    // if(!strstr(vkdt.db.dirname, "examples") && !strstr(filename, "examples"))
    //   dt_graph_write_config_ascii(&vkdt.graph_dev, filename);

//
static inline int  // TODO returns uh what? accept reject codes?
filteredlist(
    const char *dir,         // has to be !=0 and exist as directory. %s will be replaced as global basedir
    const char *dir_local)   // optional. if exists will be shown first. %s will be replaced by local basedir
{
    int ok = 0;
    char filename[1024] = {0};
    int pick = -1, local = 0;
#define FREE_ENT do {\
    for(int i=0;i<ent_cnt;i++) free(ent[i]);\
    for(int i=0;i<ent_local_cnt;i++) free(ent_local[i]);\
    free(ent_local); ent_local = 0; ent_local_cnt = 0;\
    free(ent); ent = 0; ent_cnt = 0; } while(0)
    static struct dirent **ent = 0, **ent_local = 0;
    static int ent_cnt = 0, ent_local_cnt = 0;
    static char filter[256] = "";
    if(ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
    if(ImGui::InputText("##edit", filter, IM_ARRAYSIZE(filter), ImGuiInputTextFlags_EnterReturnsTrue))
      ok = 1;
    if(ImGui::IsItemHovered())
      ImGui::SetTooltip(
          "type to filter the list\n"
          "press enter to apply top item\n"
          "press escape to close");
    if(dt_gui_imgui_nav_input(ImGuiNavInput_Cancel) > 0.0f)
    { FREE_ENT; ImGui::CloseCurrentPopup(); }

    if(!ent_cnt)
    { // open preset directory
      char dirname[PATH_MAX+20];
      // snprintf(dirname, sizeof(dirname), "%s/data/presets", dt_pipe.basedir);
      snprintf(dirname, sizeof(dirname), dir, dt_pipe.basedir);
      ent_cnt = scandir(dirname, &ent, 0, alphasort);
      // snprintf(dirname, sizeof(dirname), "%s/presets", vkdt.db.basedir);
      if(dir_local)
      {
        snprintf(dirname, sizeof(dirname), dir_local, vkdt.db.basedir);
        ent_local_cnt = scandir(dirname, &ent_local, 0, alphasort);
        if(ent_local_cnt == -1) ent_local_cnt = 0; // fine, you don't have user directory
      }
      else ent_local_cnt = 0;
    }
#define LIST(E, L) do { \
    for(int i=0;i<E##_cnt;i++)\
      if(strstr(E[i]->d_name, filter) && strstr(E[i]->d_name, ".pst")) {\
        if(pick < 0) { local = L; pick = i; } \
        if(ImGui::Button(E[i]->d_name)) {\
          ok = 1; pick = i; local = L;\
        } } } while(0)
    LIST(ent_local, 1);
    LIST(ent, 0);
#undef LIST

    if (ImGui::Button("cancel", ImVec2(120, 0))) {FREE_ENT; ImGui::CloseCurrentPopup();}
    ImGui::SameLine();
    if (ImGui::Button("ok", ImVec2(120, 0))) ok = 1;
    if(ok)
    {
      char filename[PATH_MAX+300];
      // XXX dir_local and dir!
      if(local) snprintf(filename, sizeof(filename), "%s/presets/%s", vkdt.db.basedir, ent_local[pick]->d_name);
      else      snprintf(filename, sizeof(filename), "%s/data/presets/%s", dt_pipe.basedir,  ent[pick]->d_name);
      // XXX TODO: pass ok on to the outside instead, along with d_name!
      FILE *f = fopen(filename, "rb");
      uint32_t lno = 0;
      if(f)
      {
        char line[300000];
        while(!feof(f))
        {
          fscanf(f, "%299999[^\n]", line);
          if(fgetc(f) == EOF) break; // read \n
          lno++;
          // > 0 are warnings, < 0 are fatal, 0 is success
          if(dt_graph_read_config_line(&vkdt.graph_dev, line) < 0) goto error;
          dt_graph_history_line(&vkdt.graph_dev, line);
        }
        fclose(f);
        vkdt.graph_dev.runflags = static_cast<dt_graph_run_t>(s_graph_run_all);
        darkroom_reset_zoom();
      }
      else
      {
error:
        if(f) fclose(f);
        dt_gui_notification("failed to read %s line %d", filename, lno);
      }
      FREE_ENT;
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
#undef FREE_ENT
