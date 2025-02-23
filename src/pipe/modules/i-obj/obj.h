#pragma once
#include <float.h>
#include <inttypes.h>
// simple header to parse a wavefront obj file into raw geo triangles

typedef struct geo_obj_face_t
{ // triangulated face

  // XXX do we need this?
  // indices into v vt n
}
geo_obj_face_t;

typedef struct geo_obj_t
{ // struct mirorring the contents of the obj file
  float          *v;  // vertices
  float          *vt; // texture st coordinates
  float          *n;  // vertex normals
  geo_obj_face_t *f;  // triangle referencing the above
}
geo_obj_t;


static inline void
geo_obj_load_lists(
    FILE *f,
    float *v,
    float *n,
    float *vt,
    uint64_t num_verts,
    uint64_t num_normals,
    uint64_t num_vts,
    float   *aabb)
{
  char line[2048];
  int lineno = 1;
  uint64_t ni = 0, vi = 0, ti = 0;
  while(fscanf(f, "%[^\n]", line) != EOF)
  {
    if(!strncmp(line, "vn ", 3))
    {
      if(ni >= num_normals) fprintf(stderr, "obj has fewer normals than expected (%" PRIu64 ")\n", num_normals);
      assert(ni < num_normals);
      const int cnt = sscanf(line, "vn %f %f %f", n + 3*ni, n+3*ni+1, n+3*ni+2);
      if(cnt == 3) ni++;
      else fprintf(stderr, "line %d: weird normal: `%s'\n", lineno, line);
    }
    else if(vt && !strncmp(line, "vt ", 3))
    {
      if(ti >= num_vts) fprintf(stderr, "obj has fewer texture coordinates than expected (%" PRIu64 ")\n", num_vts);
      assert(ti < num_vts);
      const int cnt = sscanf(line, "vt %f %f", vt + 2*ti, vt+2*ti+1);
      if(cnt == 2) ti++;
      else fprintf(stderr, "line %d: weird vts: `%s'\n", lineno, line);
    }
    else if(!strncmp(line, "v ", 2))
    {
      if(vi >= num_verts) fprintf(stderr, "obj has fewer vertices than expected (%" PRIu64 ")\n", num_verts);
      assert(vi < num_verts);
      const int cnt = sscanf(line, "v %f %f %f", v + 3*vi, v+3*vi+1, v+3*vi+2);
      for(int i=0;i<3;i++) aabb[i+0] = MIN(aabb[i+0], v[3*vi+i]);
      for(int i=0;i<3;i++) aabb[i+3] = MAX(aabb[i+3], v[3*vi+i]);
      if(cnt == 3) vi++;
      else fprintf(stderr, "line %d: weird vertex: `%s'\n", lineno, line);
    }
    (void)fgetc(f); // munch newline
    lineno++;
    // invalidate line in case the next line will be a blank:
    line[0] = '\0';
  }
}

static inline int
geo_obj_to_zero_base_idx(const int idx, uint64_t num_indices)
{
  return (idx <  0) ? idx + num_indices : idx - 1;
}

static inline geo_tri_t*
geo_obj_read(const char *filename, uint32_t *num_tris)
{
  *num_tris = 0;
  FILE *f = fopen(filename, "rb");
  if(!f) return 0;
  uint64_t num_verts = 0, num_normals = 0, num_vts = 0, num_faces = 0;
  char line[2048];
  // first pass: count vertices, normals, and vts:
  while(fscanf(f, "%[^\n]", line) != EOF)
  {
    if(!strncmp(line, "vn ", 3)) num_normals++;
    else if(!strncmp(line, "vt ", 3)) num_vts++;
    else if(!strncmp(line, "v ", 2)) num_verts++;
    else if(!strncmp(line, "f ", 2)) num_faces++;
    else if(!strncmp(line, "l ", 2)) num_faces++;
    (void)fgetc(f); // munch newline
    line[0] = '\0'; // invalidate to support blank lines
  }
  fseek(f, 0, SEEK_SET); // rewind

  num_verts   = MAX(4, num_verts);
  num_normals = MAX(4, num_normals);
  num_vts     = MAX(4, num_vts);

  float aabb[6] = {FLT_MAX,FLT_MAX,FLT_MAX,-FLT_MAX,-FLT_MAX,-FLT_MAX};
  float *v  = malloc(sizeof(float)*3*num_verts);
  float *n  = malloc(sizeof(float)*3*num_normals);
  float *vt = malloc(sizeof(float)*2*num_vts);
  geo_tri_t *tri = malloc(sizeof(geo_tri_t)*num_faces);

  memset(v,  0, 3*sizeof(float)*4);
  memset(n,  0, 3*sizeof(float)*4);
  memset(vt, 0, 2*sizeof(float)*4);

  // second pass: load lists of vertices etc
  geo_obj_load_lists(f, v, n, vt, num_verts, num_normals, num_vts, aabb);
  fprintf(stderr, "obj: bounding box %g %g %g -- %g %g %g\n",
      aabb[0], aabb[1], aabb[2], aabb[3], aabb[4], aabb[5]);

  // third pass: load faces and write out
  int lineno = 1, messaged = 0;
  fseek(f, 0, SEEK_SET);
  int mtl = 0; // fake material id
  uint32_t face = 0;
  while(fscanf(f, "%[^\n]", line) != EOF)
  {
    // if(!strncmp(line, "g ", 2) || !strncmp(line, "usemtl ", 7))
    // if(!strncmp(line, "o ", 2) || !strncmp(line, "usemtl ", 7))
    if(!strncmp(line, "o ", 2))
    // if(!strncmp(line, "usemtl ", 7))
    { // new object, increment material id
      mtl ++;
    }
    else if(!strncmp(line, "f ", 2) || !strncmp(line, "l ", 2))
    { // face. parse vert normal texture indices
      int vert[4] = {0}, norm[4] = {0}, uvco[4] = {0};

      // find number of '/' to guess format:
      char *c = line;
      int slashes = 0, doubleslashes = 0;
      for(;*c!=0;c++) if(*c == '/') { slashes++; if(c[1] == '/') doubleslashes = 1;}
      int cnt = 0;
      if(slashes != 8 && slashes != 6 && slashes != 4 && slashes != 3 && slashes != 0)
        fprintf(stderr, "line %d: wrong number of '/' in face definition! `%s'\n", lineno, line);
      // v/t/n and v//n
      if(slashes >= 6)
      {
        if(doubleslashes) // v//n
          cnt = sscanf(line+1, " %d//%d %d//%d %d//%d %d//%d",
              vert+0, norm+0, vert+1, norm+1, vert+2, norm+2, vert+3, norm+3);
        else // v/t/n
          cnt = sscanf(line+1, " %d/%d/%d %d/%d/%d %d/%d/%d %d/%d/%d",
              vert+0, uvco+0, norm+0, vert+1, uvco+1, norm+1, vert+2, uvco+2, norm+2, vert+3, uvco+3, norm+3);
      }
      else if(slashes >= 3) // v/t
        cnt = sscanf(line+1, " %d/%d %d/%d %d/%d %d/%d",
            vert+0, uvco+0, vert+1, uvco+1, vert+2, uvco+2, vert+3, uvco+3);
      else // v
        cnt = sscanf(line+1, " %d %d %d %d",
            vert+0, vert+1, vert+2, vert+3);
      // obj indices are 1 based
      for(int k=0;k<4;k++)
      {
        vert[k] = geo_obj_to_zero_base_idx(vert[k], num_verts);
        uvco[k] = geo_obj_to_zero_base_idx(uvco[k], num_vts);
        norm[k] = geo_obj_to_zero_base_idx(norm[k], num_normals);
      }
      int vcnt = 0;
      // if(cnt == 12 || cnt == 8 || cnt == 4) // quad
      //   vcnt = 4;
      if(cnt == 9 || cnt == 6 || cnt == 3) // tri
        vcnt = 3;
      // else if(cnt == 2) // uv and normal-less lines
      //   vcnt = 2;
      else if(!messaged)
      {
        fprintf(stderr, "obj: only tris supported so far\n");
        messaged = 1;
      }

      geo_vtx_t vtx[4];
      for(int vi=0;vi<vcnt;vi++)
      {
        vtx[vi] = (geo_vtx_t){
          .x    = v[3*vert[vi]+0], .y = v[3*vert[vi]+1], .z = v[3*vert[vi]+2],
          .tex0 = vi ? 0 : mtl,
          .n    = geo_encode_normal(n + 3*norm[vi]),
          .s    = float_to_half(vt[2*uvco[vi]+0]),
          .t    = float_to_half(vt[2*uvco[vi]+1]),
        };
      }
      int ti = face++;
      tri[ti] = (geo_tri_t){vtx[0], vtx[1], vtx[2]};
    }
    (void)fgetc(f); // munch newline
    lineno++;
    line[0] = '\0'; // support blanks
  }
  *num_tris = face;
  fclose(f);
  free(v);
  free(n);
  free(vt);
  return tri;
}
