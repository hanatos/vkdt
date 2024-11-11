// see http://scottburns.us/fast-rgb-to-spectrum-conversion-for-reflectances/
// only works within srgb. slightly steppy spectra, but good enough for quake, maybe.
// goes above 1.0 for white even within srgb. scott burns says this works and has nice
// tables with fitted spectra. to make it extra cheap i just include sigmoid fits here
// which are generally worse than the tabulated data.
vec4 rgb_to_spectrum(vec3 srgb, vec4 lambda)
{
  const vec3 cr = vec3( 58.0425, -12.5469, -7.82178);
  const vec3 cg = vec3(-202.823,  153.935, -26.6818);
  const vec3 cb = vec3( 32.7857, -61.4796,  14.1278);
  vec4 ln = (lambda - vec4(360.0))/vec4(830.0-360.0);
  vec4 rho_r = (cr.x * ln + cr.y)*ln + cr.z;
  vec4 rho_g = (cg.x * ln + cg.y)*ln + cg.z;
  vec4 rho_b = (cb.x * ln + cb.y)*ln + cb.z;
  rho_r = 0.5 * rho_r / sqrt(1.0 + rho_r*rho_r) + 0.5;
  rho_g = 0.5 * rho_g / sqrt(1.0 + rho_g*rho_g) + 0.5;
  rho_b = 0.5 * rho_b / sqrt(1.0 + rho_b*rho_b) + 0.5;
  return rho_r * srgb.r + rho_g * srgb.g + rho_b * srgb.b;
}
