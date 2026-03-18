layout(std140, set = 0, binding = 1) uniform params_t
{ 
  vec4 cam_x;
  vec4 cam_w;
  vec4 cam_u;
  vec4 fog;
  float cltime;
  int spp;
  int wd;
  int ht;
  float vis_emit;
} params;


