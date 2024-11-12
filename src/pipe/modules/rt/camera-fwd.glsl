struct camera_t
{ // camera
  float fd;   // distance film back <-> pinhole
  vec2  size; // size of film back
  vec3  o;
  mat3  rot;
};
camera_t camera = camera_t(1.0, vec2(0.5), vec3(0.0), mat3(1.0));


