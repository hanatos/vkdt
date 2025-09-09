#pragma once
static inline void
dt_tooltip(const char *fmt, ...)
{
  char text[512];
  if(fmt && fmt[0] && nk_widget_is_hovered(&vkdt.ctx))
  {
    nk_style_push_font(&vkdt.ctx, nk_glfw3_font(0));
    va_list args;
    va_start(args, fmt);
    vsnprintf(text, sizeof(text), fmt, args);
    va_end(args);
    text[sizeof(text)-1]=0;
    char *c = text;
    int len = strlen(text);
    struct nk_user_font *font = nk_glfw3_font(0);
    const float pad = vkdt.ctx.style.window.tooltip_padding.x*2;
    float w = font->width(font->userdata, font->height, text, len) + 1.5*pad;
    if(nk_tooltip_begin(&vkdt.ctx, MIN(w, vkdt.state.panel_wd)))
    {
      nk_layout_row_static(&vkdt.ctx, font->height, MIN(w, vkdt.state.panel_wd)-pad, 1);
      while(c < text + len)
      {
        char *bp = c;
        for(char *cc=c;*cc!='\n'&&cc<text+len;cc++)
        {
          if(*cc==' ')
          {
            float w = font->width(font->userdata, font->height, c, cc-c);
            if(w > vkdt.state.panel_wd-pad)
            {
              if(bp > text) *bp = '\n';
              break;
            }
            bp = cc;
          }
          else if(cc==text+len-1) bp = cc;
          else if(cc[1]=='\n') bp = cc+1;
        }
        if(*bp == '\n') *bp = 0;
        nk_label(&vkdt.ctx, c, NK_TEXT_LEFT);
        if(bp == c) break; // fuckit, no spaces found
        c = bp+1;
      }
      nk_tooltip_end(&vkdt.ctx);
    }
    nk_style_pop_font(&vkdt.ctx);
  }
}
