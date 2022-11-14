#include "modules/api.h"
#include "modules/localsize.h"
#include "core/half.h"
#include "../rt/quat.h"
#undef CLAMP // ouch. careful! quake redefines this but with (m, x, M) param order!

#include "quakedef.h"
#include "bgmusic.h"

#define MAX_GLTEXTURES 4096 // happens to be MAX_GLTEXTURES in quakespasm, a #define in gl_texmgr.c
#define MAX_IDX_CNT 2000000 // global bounds for dynamic geometry, because lazy programmer
#define MAX_VTX_CNT 2000000

typedef struct qs_data_t
{
  double       oldtime;
  quakeparms_t parms;

  uint32_t    *tex[MAX_GLTEXTURES];       // cpu side cache of quake textures
  uint32_t     tex_dim[2*MAX_GLTEXTURES]; // resolutions of textures for modify_roi_out
  uint8_t      tex_req[MAX_GLTEXTURES];   // dynamic texture request flags: 1:new 2:upload 4:free
  int32_t      tex_cnt;                   // current number of textures
  uint32_t     tex_maxw, tex_maxh;        // maximum texture size, to allocate staging upload buffer
  uint32_t     first_skybox;              // first of 6 cubemap skybox texture ids

  uint32_t     move;                      // for our own debug camera
  double       mx, my;                    // mouse coordinates

  uint32_t     initing;                   // for synchronous initing
  uint32_t     worldspawn;                // need to re-create everything?
}
qs_data_t;

// put quakeparams and ref to this module in static global memory :(
// this is because quake does it too and thus we can't have two instances
// also the texture manager will call into us
static qs_data_t qs_data = {0};
extern float r_avertexnormals[162][3]; // from r_alias.c
// mspriteframe_t *R_GetSpriteFrame (entity_t *currentent); // from r_sprite.c

int init(dt_module_t *mod)
{
  mod->data = &qs_data;
  mod->flags = s_module_request_read_source;

  uint32_t initing = 0;
  do
  {
    initing = __sync_val_compare_and_swap(&qs_data.initing, 0, 1);
    if(initing == 2) // someone else is already done initing
      return 0;
    sched_yield(); // let the other thread continue with this
  } while(initing == 1); // someone else is already initing this


  // quake has already been globally inited. it's so full of static global stuff,
  // we can't init it for thumbnails again. give up:
  if(host_initialized) return 0;

  qs_data_t *d = mod->data;
  d->tex_maxw = 1024; // as it turns out, we sometimes adaptively load more textures and thus the max detection isn't robust.
  d->tex_maxh = 1024; // we would need to re-create nodes every time a new max is detected in QS_load_texture.

  // these host parameters are just a pointer with defined linkage in host.c.
  // we'll need to point it to an actual struct (that belongs to us):
  host_parms = &d->parms;
  d->parms.basedir = "/usr/share/games/quake"; // does not work

  char *argv[] = {"quakespasm",
    "-basedir", "/usr/share/games/quake",
    "+skill", "2",
    "-game", "ad",
    "+map", "e1m8",
    "+map", "e3m1",
    "+map", "ad_azad",
    "+map", "start",
    "-game", "SlayerTest",
    "+map", "e1m2b",
    "+map", "e1m1",
    "+map", "ep1m1",
    "+map", "e1m1b",
    "+map", "start",
    "+map", "st1m1",
    "+map", "start",
  };
  int argc =  9;

  d->worldspawn = 0;
  d->parms.argc = argc;
  d->parms.argv = argv;
  d->parms.errstate = 0;

  COM_InitArgv(d->parms.argc, d->parms.argv);

  Sys_Init();

  d->parms.memsize = 256 * 1024 * 1024; // qs default in 0.94.3
#if 0
  if (COM_CheckParm("-heapsize"))
  {
    int t = COM_CheckParm("-heapsize") + 1;
    if (t < com_argc)
      d->parms.memsize = Q_atoi(com_argv[t]) * 1024;
  }
#endif

  d->parms.membase = malloc (d->parms.memsize);

  if (!d->parms.membase)
    Sys_Error ("Not enough memory free; check disk space\n");

  Sys_Printf("Quake %1.2f (c) id Software\n", VERSION);
  Sys_Printf("GLQuake %1.2f (c) id Software\n", GLQUAKE_VERSION);
  Sys_Printf("FitzQuake %1.2f (c) John Fitzgibbons\n", FITZQUAKE_VERSION);
  Sys_Printf("FitzQuake SDL port (c) SleepwalkR, Baker\n");
  Sys_Printf("QuakeSpasm " QUAKESPASM_VER_STRING " (c) Ozkan Sezer, Eric Wasylishen & others\n");

  Sys_Printf("Host_Init\n");
  Host_Init(); // this one has a lot of meat! we'll need to cut it short i suppose

  // S_BlockSound(); // only start when grabbing
  // close menu because we don't have an esc key:
  key_dest = key_game;
  m_state = m_none;
  IN_Activate();

  initing = __sync_val_compare_and_swap(&qs_data.initing, 1, 2);
  return 0;
}

void
input(
    dt_module_t             *mod,
    dt_module_input_event_t *p)
{
  qs_data_t *dat = mod->data;

  if(p->type == 0)
  { // activate event: store mouse zero and clear all movement flags we might still have
    dat->move = 0;
    dat->mx = dat->my = -666.0;
    S_UnblockSound();
    return;
  }

  if(p->type == -1)
  { // deactivate event
    dat->mx = dat->my = -666.0;
    S_BlockSound();
  }

#if 0 // custom camera:
  float *p_cam = (float *)dt_module_param_float(mod, dt_module_get_param(mod->so, dt_token("cam")));
  if(p->type == 1)
  { // mouse button
  }

  if(p->type == 2)
  { // mouse position: rotate camera based on mouse coordinate
    if(dat->mx == -666.0 && dat->my == -666.0) { dat->mx = p->x; dat->my = p->y; }
    const float avel = 0.001f; // angular velocity
    quat_t rotx, roty, tmp;
    float rgt[3], top[] = {0,0,1};
    cross(top, p_cam+4, rgt);
    quat_init_angle(&rotx, (p->x-dat->mx)*avel, 0, 0, -1);
    quat_init_angle(&roty, (p->y-dat->my)*avel, rgt[0], rgt[1], rgt[2]);
    quat_mul(&rotx, &roty, &tmp);
    quat_transform(&tmp, p_cam+4);
    dat->mx = p->x;
    dat->my = p->y;
  }

  if(p->type == 3)
  { // mouse scrolled, .dx .dy
  }

  if(p->type == 4)
  { // keyboard (key, scancode)
    if(p->action <= 1) // ignore key repeat
    switch(p->key)
    {
      case 'E': dat->move = (dat->move & ~(1<<0)) | (p->action<<0); break;
      case 'D': dat->move = (dat->move & ~(1<<1)) | (p->action<<1); break;
      case 'S': dat->move = (dat->move & ~(1<<2)) | (p->action<<2); break;
      case 'F': dat->move = (dat->move & ~(1<<3)) | (p->action<<3); break;
      case ' ': dat->move = (dat->move & ~(1<<4)) | (p->action<<4); break;
      case 'V': dat->move = (dat->move & ~(1<<5)) | (p->action<<5); break;
      case 'R': // reset camera
                dat->move = 0;
                memset(p_cam, 0, sizeof(float)*8);
                p_cam[4] = 1;
                dt_module_input_event_t p = { 0 };
                mod->so->input(mod, &p);
                return;
    }
  }
#else
  if(p->type == 1)
  { // mouse button
    const int remap[] = {K_MOUSE1, K_MOUSE2, K_MOUSE3, K_MOUSE4, K_MOUSE5};
    if(p->mbutton < 0 || p->mbutton > 4) return;
    Key_Event(remap[p->mbutton], p->action == 1);
  }
  else if(p->type == 2)
  { // mouse position
    dat->mx = p->x; dat->my = p->y;
  }
  else if(p->type == 3)
  { // mouse wheel
    if (p->dy > 0)
    {
      Key_Event(K_MWHEELUP, true);
      Key_Event(K_MWHEELUP, false);
    }
    else if (p->dy < 0)
    {
      Key_Event(K_MWHEELDOWN, true);
      Key_Event(K_MWHEELDOWN, false);
    }
  }
  else if(p->type == 4)
  { // key
    int down = p->action == 1; // GLFW_PRESS
    // we interpret the keyboard as the US layout, so keybindings
    // are based on key position, not the label on the key cap.
    // case SDL_SCANCODE_TAB: return K_TAB;
    // case SDL_SCANCODE_RETURN: return K_ENTER;
    // case SDL_SCANCODE_RETURN2: return K_ENTER;
    // case SDL_SCANCODE_ESCAPE: return K_ESCAPE;
    // case SDL_SCANCODE_SPACE: return K_SPACE;
    // the rest seems sane
    int key = p->key;//p->scancode;
    if(key >= 65 && key <= 90) key += 32; // convert lower to upper case
    if(p->action <= 1) // ignore key repeat
      Key_Event (key, down);
  }
#endif
}

void cleanup(dt_module_t *mod)
{
  // Host_Shutdown(); // doesn't work
  // XXX also can't clean up the params on there! another module/pipeline might need it!
  // qs_data_t *d = mod->data;
  // free(d);
  // i don't think quake does any cleanup
  // TODO: make sure at least our params struct is cleaned up
  mod->data = 0;
}

// called from within qs, pretty much a copy from in_sdl.c:
extern cvar_t cl_maxpitch; //johnfitz -- variable pitch clamping
extern cvar_t cl_minpitch; //johnfitz -- variable pitch clamping
void IN_Move(usercmd_t *cmd)
{
  static double oldx, oldy;
  if(qs_data.mx == -666.0 && qs_data.my == -666.0)
  {
    oldx = qs_data.mx;
    oldy = qs_data.my;
  }
  if(oldx == -666.0 && oldy == -666.0)
  {
    oldx = qs_data.mx;
    oldy = qs_data.my;
  }
  int dmx = (qs_data.mx - oldx) * sensitivity.value;
  int dmy = (qs_data.my - oldy) * sensitivity.value;
  oldx = qs_data.mx;
  oldy = qs_data.my;

  if ( (in_strafe.state & 1) || (lookstrafe.value && (in_mlook.state & 1) ))
    cmd->sidemove += m_side.value * dmx;
  else
    cl.viewangles[YAW] -= m_yaw.value * dmx;

  if (in_mlook.state & 1)
  {
    if (dmx || dmy)
      V_StopPitchDrift ();
  }

  if ( (in_mlook.state & 1) && !(in_strafe.state & 1))
  {
    cl.viewangles[PITCH] += m_pitch.value * dmy;
    /* johnfitz -- variable pitch clamping */
    if (cl.viewangles[PITCH] > cl_maxpitch.value)
      cl.viewangles[PITCH] = cl_maxpitch.value;
    if (cl.viewangles[PITCH] < cl_minpitch.value)
      cl.viewangles[PITCH] = cl_minpitch.value;
  }
  else
  {
    if ((in_strafe.state & 1) && noclip_anglehack)
      cmd->upmove -= m_forward.value * dmy;
    else
      cmd->forwardmove -= m_forward.value * dmy;
  }
}

// called from within quakespasm each time a new map is (re)loaded
void QS_worldspawn()
{
  qs_data.worldspawn = 1;
}

// called from within quakespasm, we provide the function:
void QS_texture_load(gltexture_t *glt, uint32_t *data)
{
  // TODO: maybe use our string pool to do name->id mapping? also it seems quake has a crc
  if(qs_data.initing < 1) return;
  // mirror the data locally. the copy sucks, but whatever.
  // fprintf(stderr, "[load tex] %u %ux%u %s\n", glt->texnum, glt->width, glt->height, glt->name);
  qs_data.tex_cnt = MIN(MAX_GLTEXTURES, MAX(qs_data.tex_cnt, glt->texnum+1));
  if(glt->texnum >= qs_data.tex_cnt)
  {
    fprintf(stderr, "[load tex] no more free slots for %s!\n", glt->name);
    return;
  }
  qs_data.tex_dim[2*glt->texnum+0] = glt->width;
  qs_data.tex_dim[2*glt->texnum+1] = glt->height;
  if(qs_data.tex[glt->texnum]) free(qs_data.tex[glt->texnum]);
  qs_data.tex[glt->texnum] = malloc(sizeof(uint32_t)*glt->width*glt->height);
  memcpy(qs_data.tex[glt->texnum], data, sizeof(uint32_t)*glt->width*glt->height);
  qs_data.tex_maxw = MAX(qs_data.tex_maxw, glt->width);
  qs_data.tex_maxh = MAX(qs_data.tex_maxh, glt->height);
  qs_data.tex_req[glt->texnum] = 7; // free, new and upload

  if(!strncmp(glt->name, "gfx/env/", 8))
    qs_data.first_skybox = glt->texnum-5;

  // TODO: think about cleanup later, maybe
}

#if 0
// could wire this, but we'll notice when quake overwrites the textures anyways
void QS_texture_free(gltexture_t *texture)
{
  if(qs_data.initing < 1) return;
  qs_data.tex_req[glt->texnum] = 4; // request free
  free(qs_data.tex[glt->texnum]);
  qs_data.tex[glt->texnum] = 0;
}
#endif

static inline void
encode_normal(
    int16_t *enc,
    float   *vec)
{
  const float invL1Norm = 1.0f / (fabsf(vec[0]) + fabsf(vec[1]) + fabsf(vec[2]));
  // first find floating point values of octahedral map in [-1,1]:
  float enc0, enc1;
  if (vec[2] < 0.0f)
  {
    enc0 = (1.0f - fabsf(vec[1] * invL1Norm)) * ((vec[0] < 0.0f) ? -1.0f : 1.0f);
    enc1 = (1.0f - fabsf(vec[0] * invL1Norm)) * ((vec[1] < 0.0f) ? -1.0f : 1.0f);
  }
  else
  {
    enc0 = vec[0] * invL1Norm;
    enc1 = vec[1] * invL1Norm;
  }
  enc[0] = roundf(CLAMP(-32768.0f, enc0 * 32768.0f, 32767.0f));
  enc[1] = roundf(CLAMP(-32768.0f, enc1 * 32768.0f, 32767.0f));
}

static void
add_geo(
    entity_t *ent,
    float    *vtx,   // vertex data, 3f
    uint32_t *idx,   // triangle indices, 3 per tri
    // oh fuckit. we just store all the following rubbish once per primitive:
    // 1x mat, 3x n, 3x st = 1+3*2+3*2 = 13 int16 + 1 pad
    int16_t  *ext,
    uint32_t *vtx_cnt,
    uint32_t *idx_cnt)
{
  if(!ent) return;
  // count all verts in all models
  uint32_t nvtx = 0, nidx = 0;
  qmodel_t *m = ent->model;
  // fprintf(stderr, "[add_geo] %s\n", m->name);
  // if (!m || m->name[0] == '*') return; // '*' is the moving brush models such as doors
  if (!m) return;

  // TODO: lerp between lerpdata.pose1 and pose2 using blend
  // TODO: apply transformation matrix cpu side, whatever.
  // TODO: later: put into rt animation kernel
  if(m->type == mod_alias)
  { // alias model:
    // TODO: e->model->flags & MF_HOLEY <= enable alpha test
    // fprintf(stderr, "alias origin and angles %g %g %g -- %g %g %g\n",
    //     ent->origin[0], ent->origin[1], ent->origin[2],
    //     ent->angles[0], ent->angles[1], ent->angles[2]);
    aliashdr_t *hdr = (aliashdr_t *)Mod_Extradata(ent->model);
    aliasmesh_t *desc = (aliasmesh_t *) ((uint8_t *)hdr + hdr->meshdesc);
    // the plural here really hurts but it's from quakespasm code:
    int16_t *indexes = (int16_t *) ((uint8_t *) hdr + hdr->indexes);
    trivertx_t *trivertexes = (trivertx_t *) ((uint8_t *)hdr + hdr->vertexes);

    if(*vtx_cnt + nvtx + hdr->numverts_vbo >= MAX_VTX_CNT) return;
    if(*idx_cnt + nidx + hdr->numindexes >= MAX_IDX_CNT) return;
    nvtx += hdr->numverts_vbo;
    nidx += hdr->numindexes;

    // lerpdata_t  lerpdata;
    // R_SetupAliasFrame(hdr, ent->frame, &lerpdata);
    // R_SetupEntityTransform(ent, &lerpdata);
    // angles: pitch yaw roll. axes: right fwd up
    float angles[3] = {-ent->angles[0], ent->angles[1], ent->angles[2]};
    float fwd[3], rgt[3], top[3], pos[3];
    AngleVectors (angles, fwd, rgt, top);
    for(int k=0;k<3;k++) rgt[k] = -rgt[k]; // seems these come in flipped

    // for(int f = 0; f < hdr->numposes; f++)
    // TODO: upload all vertices so we can just alter the indices on gpu
    int f = ent->frame;
    if(f < 0 || f >= hdr->numposes) return;
    if(vtx) for(int v = 0; v < hdr->numverts_vbo; v++)
    {
      int i = hdr->numverts * f + desc[v].vertindex;
      for(int k=0;k<3;k++)
        pos[k] = trivertexes[i].v[k] * hdr->scale[k] + hdr->scale_origin[k];
      for(int k=0;k<3;k++)
        vtx[3*v+k] = ent->origin[k] + rgt[k] * pos[1] + top[k] * pos[2] + fwd[k] * pos[0];
    }
#if 1 // both options fail to extract correct creases/vertex normals for health/shells
    int16_t *tmpn = alloca(2*sizeof(int16_t)*hdr->numverts_vbo);
    if(ext) for(int v = 0; v < hdr->numverts_vbo; v++)
    {
      int i = hdr->numverts * f + desc[v].vertindex;
      float nm[3], nw[3];
      memcpy(nm, r_avertexnormals[trivertexes[i].lightnormalindex], sizeof(float)*3);
      for(int k=0;k<3;k++) nw[k] = nm[0] * fwd[k] + nm[1] * rgt[k] + nm[2] * top[k];
      encode_normal(tmpn+2*v, nw);
    }
#endif
    if(idx) for(int i = 0; i < hdr->numindexes; i++)
      idx[i] = *vtx_cnt + indexes[i];

    if(ext) for(int i = 0; i < hdr->numindexes/3; i++)
    {
#if 0
      float nm[3], nw[3];
      int off = hdr->numverts * f;
  fprintf(stderr, "[add_geo] %s normal index %d numposes %d/%d\n", m->name, trivertexes[off+desc[indexes[3*i+0]].vertindex].lightnormalindex, f, hdr->numposes);
      memcpy(nm, r_avertexnormals[trivertexes[off+desc[indexes[3*i+0]].vertindex].lightnormalindex], sizeof(float)*3);
      for(int k=0;k<3;k++) nw[k] = nm[0] * fwd[k] + nm[1] * rgt[k] + nm[2] * top[k];
      encode_normal(ext+14*i+0, nw);
      memcpy(nm, r_avertexnormals[trivertexes[off+desc[indexes[3*i+1]].vertindex].lightnormalindex], sizeof(float)*3);
      for(int k=0;k<3;k++) nw[k] = nm[0] * fwd[k] + nm[1] * rgt[k] + nm[2] * top[k];
      encode_normal(ext+14*i+2, nw);
      memcpy(nm, r_avertexnormals[trivertexes[off+desc[indexes[3*i+2]].vertindex].lightnormalindex], sizeof(float)*3);
      for(int k=0;k<3;k++) nw[k] = nm[0] * fwd[k] + nm[1] * rgt[k] + nm[2] * top[k];
      encode_normal(ext+14*i+4, nw);
#else
      memcpy(ext+14*i+0, tmpn+2*indexes[3*i+0], sizeof(int16_t)*2);
      memcpy(ext+14*i+2, tmpn+2*indexes[3*i+1], sizeof(int16_t)*2);
      memcpy(ext+14*i+4, tmpn+2*indexes[3*i+2], sizeof(int16_t)*2);
#endif
      ext[14*i+ 6] = float_to_half((desc[indexes[3*i+0]].st[0]+0.5)/(float)hdr->skinwidth);
      ext[14*i+ 7] = float_to_half((desc[indexes[3*i+0]].st[1]+0.5)/(float)hdr->skinheight);
      ext[14*i+ 8] = float_to_half((desc[indexes[3*i+1]].st[0]+0.5)/(float)hdr->skinwidth);
      ext[14*i+ 9] = float_to_half((desc[indexes[3*i+1]].st[1]+0.5)/(float)hdr->skinheight);
      ext[14*i+10] = float_to_half((desc[indexes[3*i+2]].st[0]+0.5)/(float)hdr->skinwidth);
      ext[14*i+11] = float_to_half((desc[indexes[3*i+2]].st[1]+0.5)/(float)hdr->skinheight);
      ext[14*i+12] = hdr->gltextures[CLAMP(0, ent->skinnum, hdr->numskins-1)][((int)(cl.time*10))&3]->texnum;
      ext[14*i+13] =
        hdr->fbtextures[CLAMP(0, ent->skinnum, hdr->numskins-1)][((int)(cl.time*10))&3] ?
        hdr->fbtextures[CLAMP(0, ent->skinnum, hdr->numskins-1)][((int)(cl.time*10))&3]->texnum : 0;
    }
  }
  else if(m->type == mod_brush)
  { // brush model:
    // fprintf(stderr, "brush origin and angles %g %g %g -- %g %g %g with %d surfs\n",
    //     ent->origin[0], ent->origin[1], ent->origin[2],
    //     ent->angles[0], ent->angles[1], ent->angles[2],
    //     m->nummodelsurfaces);
    float angles[3] = {-ent->angles[0], ent->angles[1], ent->angles[2]};
    float fwd[3], rgt[3], top[3];
    AngleVectors (angles, fwd, rgt, top);
    for (int i=0; i<m->nummodelsurfaces; i++)
    {
      msurface_t *surf = &m->surfaces[m->firstmodelsurface + i];
      // TODO: 
      // if(!strcmp(surf->texinfo->texture->filename, "skip")) skip this surface
      glpoly_t *p = surf->polys;
      while(p)
      {
        if(*vtx_cnt + nvtx + p->numverts >= MAX_VTX_CNT ||
           *idx_cnt + nidx + 3*(p->numverts-2) >= MAX_IDX_CNT)
          break;
        if(vtx) for(int k=0;k<p->numverts;k++)
          for(int l=0;l<3;l++)
            vtx[3*(nvtx+k)+l] =
              p->verts[k][0] * fwd[l] -
              p->verts[k][1] * rgt[l] +
              p->verts[k][2] * top[l]
              + ent->origin[l];
        if(idx) for(int k=2;k<p->numverts;k++)
        {
          idx[nidx+3*(k-2)+0] = *vtx_cnt + nvtx;
          idx[nidx+3*(k-2)+1] = *vtx_cnt + nvtx+k-1;
          idx[nidx+3*(k-2)+2] = *vtx_cnt + nvtx+k;
        }
        if(ext) for(int k=2;k<p->numverts;k++)
        {
          int pi = (nidx+3*(k-2))/3;
          ext[14*pi+0] = surf->texinfo->texture->gloss ? surf->texinfo->texture->gloss->texnum : 0;
          ext[14*pi+1] = surf->texinfo->texture->norm  ? surf->texinfo->texture->norm->texnum  : 0;
          ext[14*pi+2] = 0xffff; // mark as brush model
          ext[14*pi+3] = 0xffff;
          if(surf->texinfo->texture->gltexture)
          {
            ext[14*pi+ 6] = float_to_half(p->verts[0  ][3]);
            ext[14*pi+ 7] = float_to_half(p->verts[0  ][4]);
            ext[14*pi+ 8] = float_to_half(p->verts[k-1][3]);
            ext[14*pi+ 9] = float_to_half(p->verts[k-1][4]);
            ext[14*pi+10] = float_to_half(p->verts[k-0][3]);
            ext[14*pi+11] = float_to_half(p->verts[k-0][4]);
            ext[14*pi+12] = surf->texinfo->texture->gltexture->texnum;
            ext[14*pi+13] = surf->texinfo->texture->fullbright ? surf->texinfo->texture->fullbright->texnum : 0;
            // max textures is 4096 (12 bit) and we have 16. so we can put 4 bits worth of flags here:
            uint32_t flags = 0;
            if(surf->flags & SURF_DRAWLAVA)  flags = 1;
            if(surf->flags & SURF_DRAWSLIME) flags = 2;
            if(surf->flags & SURF_DRAWTELE)  flags = 3;
            if(surf->flags & SURF_DRAWWATER) flags = 4;
            // if(surf->flags & SURF_DRAWSKY)   flags = 5; // could do this too
            ext[14*pi+13] |= flags << 12;
          }
          if(surf->flags & SURF_DRAWSKY) ext[14*pi+12] = 0xffff;
        }
        nvtx += p->numverts;
        nidx += 3*(p->numverts-2);
        // p = p->next;
        p = 0; // XXX
      }
    }
  }
  else if(m->type == mod_sprite)
  { // explosions, decals, etc, this is R_DrawSpriteModel
    vec3_t      point, v_forward, v_right, v_up;
    msprite_t   *psprite;
    mspriteframe_t  *frame;
    float     *s_up, *s_right;
    float     angle, sr, cr;
    float     scale = 1.0f;// XXX newer quakespasm has this: ENTSCALE_DECODE(ent->scale);

    vec3_t vpn, vright, vup, r_origin;
    VectorCopy (r_refdef.vieworg, r_origin);
    AngleVectors (r_refdef.viewangles, vpn, vright, vup);

    frame = R_GetSpriteFrame(ent);
    psprite = (msprite_t *) ent->model->cache.data;

    switch(psprite->type)
    {
      case SPR_VP_PARALLEL_UPRIGHT: //faces view plane, up is towards the heavens
        v_up[0] = 0;
        v_up[1] = 0;
        v_up[2] = 1;
        s_up = v_up;
        s_right = vright;
        break;
      case SPR_FACING_UPRIGHT: //faces camera origin, up is towards the heavens
        VectorSubtract(ent->origin, r_origin, v_forward);
        v_forward[2] = 0;
        VectorNormalizeFast(v_forward);
        v_right[0] = v_forward[1];
        v_right[1] = -v_forward[0];
        v_right[2] = 0;
        v_up[0] = 0;
        v_up[1] = 0;
        v_up[2] = 1;
        s_up = v_up;
        s_right = v_right;
        break;
      case SPR_VP_PARALLEL: //faces view plane, up is towards the top of the screen
        s_up = vup;
        s_right = vright;
        break;
      case SPR_ORIENTED: //pitch yaw roll are independent of camera
        AngleVectors (ent->angles, v_forward, v_right, v_up);
        s_up = v_up;
        s_right = v_right;
        break;
      case SPR_VP_PARALLEL_ORIENTED: //faces view plane, but obeys roll value
        angle = ent->angles[ROLL] * M_PI_DIV_180;
        sr = sin(angle);
        cr = cos(angle);
        v_right[0] = vright[0] * cr + vup[0] * sr;
        v_right[1] = vright[1] * cr + vup[1] * sr;
        v_right[2] = vright[2] * cr + vup[2] * sr;
        v_up[0] = vright[0] * -sr + vup[0] * cr;
        v_up[1] = vright[1] * -sr + vup[1] * cr;
        v_up[2] = vright[2] * -sr + vup[2] * cr;
        s_up = v_up;
        s_right = v_right;
        break;
      default:
        return;
    }

    int numv = 4; // add a quad
    if(*vtx_cnt + nvtx + numv >= MAX_VTX_CNT ||
       *idx_cnt + nidx + 3*(numv-2) >= MAX_IDX_CNT)
          return;
    float vert[4][3];
    if(vtx || ext)
    {
      VectorMA (ent->origin, frame->down * scale, s_up, point);
      VectorMA (point, frame->left * scale, s_right, point);
      for(int l=0;l<3;l++) vert[0][l] = point[l];

      VectorMA (ent->origin, frame->up * scale, s_up, point);
      VectorMA (point, frame->left * scale, s_right, point);
      for(int l=0;l<3;l++) vert[1][l] = point[l];

      VectorMA (ent->origin, frame->up * scale, s_up, point);
      VectorMA (point, frame->right * scale, s_right, point);
      for(int l=0;l<3;l++) vert[2][l] = point[l];

      VectorMA (ent->origin, frame->down * scale, s_up, point);
      VectorMA (point, frame->right * scale, s_right, point);
      for(int l=0;l<3;l++) vert[3][l] = point[l];
    }
    if(vtx)
    {
      for(int l=0;l<3;l++) vtx[3*(nvtx+0)+l] = vert[0][l];
      for(int l=0;l<3;l++) vtx[3*(nvtx+1)+l] = vert[1][l];
      for(int l=0;l<3;l++) vtx[3*(nvtx+2)+l] = vert[2][l];
      for(int l=0;l<3;l++) vtx[3*(nvtx+3)+l] = vert[3][l];
    }
    if(idx)
    {
      idx[nidx+3*0+0] = *vtx_cnt + nvtx;
      idx[nidx+3*0+1] = *vtx_cnt + nvtx+2-1;
      idx[nidx+3*0+2] = *vtx_cnt + nvtx+2;

      idx[nidx+3*1+0] = *vtx_cnt + nvtx;
      idx[nidx+3*1+1] = *vtx_cnt + nvtx+3-1;
      idx[nidx+3*1+2] = *vtx_cnt + nvtx+3;
    }
    if(ext)
    {
      int pi = nidx/3; // start of the two triangles
      float n[3], e0[] = {
        vert[2][0] - vert[0][0],
        vert[2][1] - vert[0][1],
        vert[2][2] - vert[0][2]}, e1[] = {
        vert[1][0] - vert[0][0],
        vert[1][1] - vert[0][1],
        vert[1][2] - vert[0][2]};
      cross(e0, e1, n);
      encode_normal(ext+14*pi+0, n);
      encode_normal(ext+14*pi+2, n);
      encode_normal(ext+14*pi+4, n);
      encode_normal(ext+14*(pi+1)+0, n);
      encode_normal(ext+14*(pi+1)+2, n);
      encode_normal(ext+14*(pi+1)+4, n);
      if(frame->gltexture)
      {
        ext[14*pi+ 6] = float_to_half(0);
        ext[14*pi+ 7] = float_to_half(1);
        ext[14*pi+ 8] = float_to_half(0);
        ext[14*pi+ 9] = float_to_half(0);
        ext[14*pi+10] = float_to_half(1);
        ext[14*pi+11] = float_to_half(0);

        ext[14*(pi+1)+ 6] = float_to_half(0);
        ext[14*(pi+1)+ 7] = float_to_half(1);
        ext[14*(pi+1)+ 8] = float_to_half(1);
        ext[14*(pi+1)+ 9] = float_to_half(0);
        ext[14*(pi+1)+10] = float_to_half(1);
        ext[14*(pi+1)+11] = float_to_half(1);

        ext[14*pi+12]     = frame->gltexture->texnum;
        ext[14*pi+13]     = frame->gltexture->texnum; // fullbright XXX where to get this?
        ext[14*(pi+1)+12] = frame->gltexture->texnum;
        ext[14*(pi+1)+13] = frame->gltexture->texnum; // fullbright XXX where to get this?
      }
    }
    nvtx += 4;
    nidx += 6;
  } // end sprite model
  *vtx_cnt += nvtx;
  *idx_cnt += nidx;
}

void modify_roi_out(
    dt_graph_t  *graph,
    dt_module_t *mod)
{
  int wd = 1920, ht = 1080;
  mod->connector[0].roi.scale   = 1.0;
  mod->connector[0].roi.full_wd = wd;
  mod->connector[0].roi.full_ht = ht;
  mod->connector[2].roi.scale   = 1.0;
  mod->connector[2].roi.full_wd = wd;
  mod->connector[2].roi.full_ht = ht;
  mod->img_param = (dt_image_params_t) {
    .black          = {0, 0, 0, 0},
    .white          = {65535,65535,65535,65535},
    .whitebalance   = {1.0, 1.0, 1.0, 1.0},
    .filters        = 0, // anything not 0 or 9 will be bayer starting at R
    .crop_aabb      = {0, 0, wd, ht},
    .cam_to_rec2020 = {1, 0, 0, 0, 1, 0, 0, 0, 1},
    .noise_a        = 1.0,
    .noise_b        = 0.0,
    .orientation    = 0,
  };
}

void commit_params(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  qs_data_t *d = module->data;
  // float *f = (float *)module->committed_param;
  double newtime = Sys_DoubleTime();
  double time = newtime - d->oldtime;
  Host_Frame(time);
  d->oldtime = newtime;

  // in quake, run `record <demo name>` until `stop`
  // XXX for performance/testing, enable this playdemo and do
  // ./vkdt-cli -g examples/quake.cfg --format o-ffmpeg --filename qu.vid --audio qu.aud --output main --format o-ffmpeg --filename mv.vid --output hist --config frames:3000 fps:24
  // ffmpeg -i qu.vid_0002.h264 -f s16le -sample_rate 44100 -channels 2  -i qu.aud -c:v copy quake.mp4
  // (add -r 60 to resample for different frame rate)
  // (replace '-c:v copy' by '-vcodec libx264 -crf 27 -preset veryfast' for compression)
  // if(graph->frame == 0) Cmd_ExecuteString("playdemo mydemo2", src_command); // 3000 frames
  // if(graph->frame == 0) Cmd_ExecuteString("playdemo rotatingarmour", src_command); // 400 frames
  // if(graph->frame == 0) Cmd_ExecuteString("playdemo mlt-noise", src_command); // 3000 frames
  // to test rocket illumination etc:
  if(graph->frame == 10)
  {
    // Cmd_ExecuteString("developer 1", src_command);
    // Cmd_ExecuteString("bind \"q\" \"impulse 9 ; wait ; impulse 255\"", src_command);
    Cmd_ExecuteString("bind \"q\" \"impulse 9\"", src_command);
    Cmd_ExecuteString("god", src_command);
    // Cmd_ExecuteString("notarget", src_command);
  }

#if 1 // does not work with demo replay
  if(sv_player->v.weapon == 1) // axe has torch built in:
    ((int *)dt_module_param_int(module, dt_module_get_param(module->so, dt_token("torch"))))[0] = 1;
  else
    ((int *)dt_module_param_int(module, dt_module_get_param(module->so, dt_token("torch"))))[0] = 0;
#endif

  float *p_cam = (float *)dt_module_param_float(module, dt_module_get_param(module->so, dt_token("cam")));
#if 0 // our camera
  // put back to params:
  float fwd[] = {p_cam[4], p_cam[5], p_cam[6]};
  float top[] = {0, 0, 1};
  float rgt[3]; cross(top, fwd, rgt);
  float vel = 3.0f;
  if(d->move & (1<<0)) for(int k=0;k<3;k++) p_cam[k] += vel * fwd[k];
  if(d->move & (1<<1)) for(int k=0;k<3;k++) p_cam[k] -= vel * fwd[k];
  if(d->move & (1<<2)) for(int k=0;k<3;k++) p_cam[k] += vel * rgt[k];
  if(d->move & (1<<3)) for(int k=0;k<3;k++) p_cam[k] -= vel * rgt[k];
  if(d->move & (1<<4)) for(int k=0;k<3;k++) p_cam[k] += vel * top[k];
  if(d->move & (1<<5)) for(int k=0;k<3;k++) p_cam[k] -= vel * top[k];
#else // quake camera
  float fwd[3], rgt[3], top[3];
  AngleVectors (r_refdef.viewangles, fwd, rgt, top);
  for(int k=0;k<3;k++) p_cam[k]   = r_refdef.vieworg[k];
  for(int k=0;k<3;k++) p_cam[4+k] = fwd[k];
#endif
}

int read_source(
    dt_module_t             *mod,
    void                    *mapped,
    dt_read_source_params_t *p)
{
  qs_data_t *d = mod->data;

  // fprintf(stderr, "[read_source '%"PRItkn"'] uploading source %d\n", dt_token_str(p->node->kernel), p->a);

  if(p->node->kernel == dt_token("tex") && p->a < d->tex_cnt)
  { // upload texture array
    memcpy(mapped, d->tex[p->a], sizeof(uint32_t)*d->tex_dim[2*p->a]*d->tex_dim[2*p->a+1]);
    p->node->flags &= ~s_module_request_read_source; // done uploading textures
    d->tex_req[p->a] = 0;
    // p->node->connector[0].array_length = d->tex_cnt; // truncate to our current texture count
  }
  else if(p->node->kernel == dt_token("dyngeo"))
  {
    // FIXME: uploading this stuff is like 3x as expensive as uploading the geo for it!
    uint32_t vtx_cnt = 0, idx_cnt = 0;
    // XXX TODO
    // ent is the player model (visible when out of body)
    // ent = &cl_entities[cl.viewentity];
    // view is the weapon model (only visible from inside body)
    // view = &cl.viewent;
    // add_geo(cl_entities+cl.viewentity, 0, 0, mapped, &vtx_cnt, &idx_cnt);
    add_geo(&cl.viewent, 0, 0, mapped, &vtx_cnt, &idx_cnt);
    for(int i=0;i<cl_numvisedicts;i++)
      add_geo(cl_visedicts[i], 0, 0, ((int16_t*)mapped) + 14*(idx_cnt/3), &vtx_cnt, &idx_cnt);
    // for (int i=1 ; i<MAX_MODELS ; i++)
      // add_geo(cl.model_precache+i, 0, 0, ((int16_t*)mapped) + 14*(idx_cnt/3), &vtx_cnt, &idx_cnt);
    // decoration such as flames:
    for (int i=0; i<cl.num_statics; i++)
      add_geo(cl_static_entities+i, 0, 0, ((int16_t*)mapped) + 14*(idx_cnt/3), &vtx_cnt, &idx_cnt);
    // temp entities are in visedicts already
    // for (int i=0; i<cl_max_edicts; i++)
      // add_geo(cl_entities+i, 0, 0, ((int16_t*)mapped) + 14*(idx_cnt/3), &vtx_cnt, &idx_cnt);
    p->node->flags |= s_module_request_read_source; // request again
  }
  else if(p->node->kernel == dt_token("stcgeo"))
  {
    uint32_t vtx_cnt = 0, idx_cnt = 0;
    add_geo(cl_entities+0, 0, 0, mapped, &vtx_cnt, &idx_cnt);
    p->node->flags &= ~s_module_request_read_source; // done uploading static extra data
  }

  return 0;
}

int read_geo(
    dt_module_t *mod,
    dt_read_geo_params_t *p)
{
  // this is only called for our "geo" node because it has an output connector with format "geo".
  uint32_t vtx_cnt = 0, idx_cnt = 0;
  if(p->node->kernel == dt_token("dyngeo"))
  {
    // add_geo(cl_entities+cl.viewentity, p->vtx + 3*vtx_cnt, p->idx + idx_cnt, 0, &vtx_cnt, &idx_cnt);
    add_geo(&cl.viewent, p->vtx + 3*vtx_cnt, p->idx + idx_cnt, 0, &vtx_cnt, &idx_cnt);
    for(int i=0;i<cl_numvisedicts;i++)
      add_geo(cl_visedicts[i], p->vtx + 3*vtx_cnt, p->idx + idx_cnt, 0, &vtx_cnt, &idx_cnt);
    // for (int i=1 ; i<MAX_MODELS ; i++)
      // add_geo(cl.model_precache+i, p->vtx + 3*vtx_cnt, p->idx + idx_cnt, 0, &vtx_cnt, &idx_cnt);
    for (int i=0; i<cl.num_statics; i++)
      add_geo(cl_static_entities+i, p->vtx + 3*vtx_cnt, p->idx + idx_cnt, 0, &vtx_cnt, &idx_cnt);
    // for (int i=0; i<cl_max_edicts; i++)
      // add_geo(cl_entities+i, p->vtx + 3*vtx_cnt, p->idx + idx_cnt, 0, &vtx_cnt, &idx_cnt);
    p->node->rt.vtx_cnt = vtx_cnt;
    p->node->rt.tri_cnt = idx_cnt / 3;
  }
  else if(p->node->kernel == dt_token("stcgeo"))
  {
    add_geo(cl_entities+0, p->vtx + 3*vtx_cnt, p->idx + idx_cnt, 0, &vtx_cnt, &idx_cnt);
    p->node->rt.vtx_cnt = vtx_cnt;
    p->node->rt.tri_cnt = idx_cnt / 3;
    p->node->flags &= ~s_module_request_read_geo; // done uploading static geo for now
  }
  // fprintf(stderr, "[read_geo '%"PRItkn"']: vertex count %u index count %u\n", dt_token_str(p->node->kernel), vtx_cnt, idx_cnt);
  return 0;
}

void
create_nodes(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  qs_data_t *d = module->data;

  // ray tracing kernel:
  assert(graph->num_nodes < graph->max_nodes);
  const uint32_t id_rt = graph->num_nodes++;
  graph->node[id_rt] = (dt_node_t) {
    .name   = dt_token("quake"),
    .kernel = dt_token("main"),
    .module = module,
    .wd     = module->connector[0].roi.wd,
    .ht     = module->connector[0].roi.ht,
    .dp     = 1,
    .num_connectors = 9,
    .connector = {{ // 0
      .name   = dt_token("output"),
      .type   = dt_token("write"),
      .chan   = dt_token("rgba"),
      .format = dt_token("f16"),
      .roi    = module->connector[0].roi,
    },{ // 1
      .name   = dt_token("stcgeo"),
      .type   = dt_token("read"),
      .chan   = dt_token("ssbo"),
      .format = dt_token("geo"),
      .connected_mi = -1,
    },{ // 2
      .name   = dt_token("dyngeo"),
      .type   = dt_token("read"),
      .chan   = dt_token("ssbo"),
      .format = dt_token("geo"),
      .connected_mi = -1,
    },{ // 3
      .name   = dt_token("tex"),
      .type   = dt_token("read"),
      .chan   = dt_token("*"),
      .format = dt_token("*"),
      .connected_mi = -1,
    },{ // 4
      .name   = dt_token("blue"),
      .type   = dt_token("read"),
      .chan   = dt_token("*"),
      .format = dt_token("*"),
      .connected_mi = -1,
    },{ // 5
      .name   = dt_token("aov"),
      .type   = dt_token("write"),
      .chan   = dt_token("rgba"),
      .format = dt_token("f16"),
      .roi    = module->connector[0].roi,
    },{ // 6
      .name   = dt_token("nee_in"),
      .type   = dt_token("read"),
      .chan   = dt_token("rgba"),
      .format = dt_token("ui32"),
      .connected_mi = -1,
    },{ // 7
      .name   = dt_token("nee_out"),
      .type   = dt_token("write"),
      .chan   = dt_token("rgba"),
      .format = dt_token("ui32"),
      .roi    = module->connector[0].roi,
      .flags  = s_conn_clear, // init with zero weights/counts
    },{ // 8
      .name   = dt_token("mv"),
      .type   = dt_token("read"),
      .chan   = dt_token("rg"),
      .format = dt_token("f16"),
      .connected_mi = -1,
    }},
    .push_constant = { d->first_skybox },
    .push_constant_size = sizeof(uint32_t),
  };
  assert(graph->num_nodes < graph->max_nodes);
  const uint32_t id_tex = graph->num_nodes++;
  graph->node[id_tex] = (dt_node_t) {
    .name   = dt_token("quake"),
    .kernel = dt_token("tex"),
    .module = module,
    .wd     = 1,
    .ht     = 1,
    .dp     = 1,
    .flags  = s_module_request_read_source,
    .num_connectors = 1,
    .connector = {{
      .name   = dt_token("tex"),
      .type   = dt_token("source"),
      .chan   = dt_token("rgba"),
      .format = dt_token("ui8"),
      .roi    = {
        .scale   = 1.0,
        .wd      = d->tex_maxw,
        .ht      = d->tex_maxh,
        .full_wd = d->tex_maxw,
        .full_ht = d->tex_maxh,
      },
      .array_length = MAX_GLTEXTURES, // d->tex_cnt,
      .array_dim    = d->tex_dim,
      .array_req    = d->tex_req,
      .flags        = s_conn_dynamic_array,
      .array_alloc_size = 700<<20, // something enough for quake
    }},
  };
  // in case quake was already inited but we are called again to create nodes,
  // we'll also need to re-upload all textures. flag them here:
  for(int i=0;i<d->tex_cnt;i++)
    if(d->tex[i]) d->tex_req[i] = 7;

  uint32_t vtx_cnt = 0, idx_cnt = 0;
#if 0
  // add_geo(cl_entities+cl.viewentity, 0, 0, 0, &vtx_cnt, &idx_cnt);
  add_geo(&cl.viewent, 0, 0, 0, &vtx_cnt, &idx_cnt);
  for(int i=0;i<cl_numvisedicts;i++)
    add_geo(cl_visedicts[i], 0, 0, 0, &vtx_cnt, &idx_cnt);
  fprintf(stderr, "[create_nodes] dynamic vertex count %u index count %u\n", vtx_cnt, idx_cnt);
#endif
  // we'll statically assign a global buffer size here because we want to avoid a fresh
  // node creation/memory allocation pass. reallocation usually invalidates *all* buffers
  // requiring fresh data upload for everything.
  // i suppose the core allocator might need support for incremental additions otherwise.
  vtx_cnt = MAX_VTX_CNT;
  idx_cnt = MAX_IDX_CNT;

  assert(graph->num_nodes < graph->max_nodes);
  const uint32_t id_dyngeo = graph->num_nodes++;
  graph->node[id_dyngeo] = (dt_node_t) {
    .name   = dt_token("quake"),
    .kernel = dt_token("dyngeo"),
    .module = module,
    .wd     = 1,
    .ht     = 1,
    .dp     = 1,
    .flags  = s_module_request_read_geo,
    .num_connectors = 1,
    .connector = {{
      .name   = dt_token("dyngeo"),
      .type   = dt_token("source"),
      .chan   = dt_token("ssbo"),
      .format = dt_token("geo"),
      .roi    = {
        .scale   = 1.0,
        .wd      = vtx_cnt,
        .ht      = idx_cnt,
        .full_wd = vtx_cnt,
        .full_ht = idx_cnt,
      },
    }},
  };

  // the static geometry we count. this means that we'll need to re-create nodes on map change.
  vtx_cnt = idx_cnt = 0;
  add_geo(cl_entities+0, 0, 0, 0, &vtx_cnt, &idx_cnt);
  fprintf(stderr, "[create_nodes] static vertex count %u index count %u\n", vtx_cnt, idx_cnt);
  vtx_cnt = MAX(3, vtx_cnt); // avoid crash for not initialised model
  idx_cnt = MAX(3, idx_cnt);

  assert(graph->num_nodes < graph->max_nodes);
  const uint32_t id_stcgeo = graph->num_nodes++;
  graph->node[id_stcgeo] = (dt_node_t) {
    .name   = dt_token("quake"),
    .kernel = dt_token("stcgeo"),
    .module = module,
    .wd     = 1,
    .ht     = 1,
    .dp     = 1,
    .flags  = s_module_request_read_geo,
    .num_connectors = 1,
    .connector = {{
      .name   = dt_token("stcgeo"),
      .type   = dt_token("source"),
      .chan   = dt_token("ssbo"),
      .format = dt_token("geo"),
      .roi    = {
        .scale   = 1.0,
        .wd      = vtx_cnt,
        .ht      = idx_cnt,
        .full_wd = vtx_cnt,
        .full_ht = idx_cnt,
      },
    }},
  };

  CONN(dt_node_connect(graph, id_tex, 0, id_rt, 3));
  CONN(dt_node_connect(graph, id_stcgeo, 0, id_rt, 1));
  CONN(dt_node_connect(graph, id_dyngeo, 0, id_rt, 2));
  CONN(dt_node_feedback(graph, id_rt, 7, id_rt, 6)); // nee cache
  dt_connector_copy(graph, module, 0, id_rt, 0); // wire output buffer
  dt_connector_copy(graph, module, 1, id_rt, 4); // wire blue noise input
  dt_connector_copy(graph, module, 2, id_rt, 5); // output aov image
  dt_connector_copy(graph, module, 3, id_rt, 8); // motion vectors from outside

  // propagate up so things will start to move at all at the node level:
  module->flags = s_module_request_read_geo | s_module_request_read_source;
}

#if 1
int audio(
    dt_module_t  *mod,
    const int     frame,
    uint8_t     **samples)
{
  qs_data_t *dat = mod->data;
  if(dat->worldspawn)
  {
    mod->flags = s_module_request_all;
    dat->worldspawn = 0;
    key_dest = key_game;
    m_state = m_none;
    IN_Activate();
  }
  int buffersize = shm->samples * (shm->samplebits / 8);
  int pos = (shm->samplepos * (shm->samplebits / 8));
  int tobufend = buffersize - pos; // bytes to buffer end
  *samples = shm->buffer + pos; // start position

  // sync: keep old buffer pos, check times, submit only what we need
  static double oldtime = 0.0;
  double time = Sys_DoubleTime();
  double newtime = time;
  if(oldtime == 0.0)
    oldtime = newtime - 1.0/60.0; // assume some fps
  // compute number of samples and then bytes required for this timeslot
  int len = ((int)(shm->speed * (time - oldtime))) * (shm->samplebits / 8) * shm->channels;
  if(len > tobufend)
  { // clipped at buffer end?
    shm->samplepos = 0;    // wrap around
    len = 4*(tobufend/4);  // clip number of samples we send to multiple of bytes/sample
    newtime = ((double)len) / (shm->samplebits / 8) / shm->speed + oldtime; // also clip time
  }
  else shm->samplepos += len / (shm->samplebits / 8);
  oldtime = newtime;
  return len/4; // return number of samples (compute from byte size /4: stereo and 16 bit)
}
#endif
