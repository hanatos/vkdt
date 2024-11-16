// =============================================
//  camera related functions and structs
// =============================================

void camera_setup(float angle, float dist)
{ // camera setup
  float h = 0.2;
  float ca = cos(angle);
  float sa = sin(angle);
  vec3 to = normalize(vec3(-sa, -h, ca));
  vec3 rg = normalize(cross(to, vec3(0,1,0)));
  camera.rot = mat3(rg, cross(to,rg), to);
  camera.o = camera.rot * vec3(0.0, 0.0, -dist) + vec3(0, h, dist/4.0);
}

float // returns W(x0) / (p(x0) * p(w))  (all x in area measure and w in projected solid angle measure)
camera_sample_xw(
    in  vec2 xi, // in [0,1]^2
    out vec4 ro, out vec3 rd) // resulting ray position and direction
{
  ro = vec4(camera.o, -1);
  rd = normalize(vec3(camera.size * (xi - 0.5), camera.fd));
  const float dt4 = rd.z*rd.z*rd.z*rd.z;
  const float A = camera.size.x*camera.size.y;
  rd = camera.rot * rd;
  return A * dt4 / (camera.fd*camera.fd);
}

// connect to camera, return part of the measurement contribution we know:
// geo term, transmittance, phase function.
float // returns 1 or 0 if behind camera.
camera_sample_x(
    in  vec2 xi,  // for sampling lens
    in  vec3 wi,  // usually ignored, incident direction
    in  vec4 p,   // path vertex to connect
    out vec4 x,   // vertex on camera
    out vec2 px)  // pixel coordinate in [0,1]
{
  vec3 cr = camera.rot * vec3(1, 0, 0);
  vec3 cu = camera.rot * vec3(0, 1, 0);
  vec3 cd = camera.rot * vec3(0, 0, 1);
  float z = dot(p.xyz - camera.o, cd);
  float i = dot(p.xyz - camera.o, cr) * camera.fd/z;
  float j = dot(p.xyz - camera.o, cu) * camera.fd/z;
  px = (vec2(i, j)+camera.size/2.0)/camera.size;
  x = vec4(camera.o, -1);
  if(z > 0) return 1.0;
  return 0.0;
}

float // return projected solid angle pdf(w)
camera_pdf_xw(in vec4 x, in vec3 w)
{
  vec3 rd = transpose(camera.rot) * w;
  const float dt4 = rd.z*rd.z*rd.z*rd.z;
  const float A = camera.size.x*camera.size.y;
  return 1.0/A * camera.fd*camera.fd/dt4;
}

float  // return camera responsivity and pixel coordinate
camera_eval(
    in vec4 x,    // point on aperture
    in vec3 w,    // direction from aperture to scene
    out vec2 px)  // output pixel coordinate
{
  vec3 cr = camera.rot * vec3(1, 0, 0);
  vec3 cu = camera.rot * vec3(0, 1, 0);
  vec3 cd = camera.rot * vec3(0, 0, 1);
  float z = dot(w, cd);
  float i = dot(w, cr) * camera.fd/z;
  float j = dot(w, cu) * camera.fd/z;
  px = (vec2(i, j)+camera.size/2.0)/camera.size;
  if(z > 0) return 1.0;
  return 0.0;
}

float camera_pdf_x(
    in vec3 wi,
    in vec4 x0,
    in vec4 x)
{
  return 1.0; // we're pinhole and not sampling x
}
