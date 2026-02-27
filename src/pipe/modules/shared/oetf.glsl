// from Jed Smith's OpenDRT, GPLv3:
vec3 oetf_davinci_intermediate(vec3 x) {
    return mix(x/10.44426855, exp2(x/0.07329248 - 7.0) - 0.0075, greaterThan(x, vec3(0.02740668)));
}
// histogram uses this for single channel:
float oetf_filmlight_tlog(float x) {
  return mix(exp((x - 0.5520126568606655)/0.09232902596577353) - 0.0057048244042473785, (x-0.075)/16.184376489665897, x < 0.075);
}
vec3 oetf_filmlight_tlog(vec3 x) {
  return mix(exp((x - 0.5520126568606655)/0.09232902596577353) - 0.0057048244042473785, (x-0.075)/16.184376489665897, lessThan(x, vec3(0.075)));
}
vec3 oetf_acescct(vec3 x) {
  return mix(exp2(x*17.52 - 9.72), (x - 0.0729055341958355)/10.5402377416545, lessThanEqual(x, vec3(0.155251141552511)));
}
vec3 oetf_arri_logc3(vec3 x) {
  return mix((pow(vec3(10.0), (x - 0.385537)/0.247190) - 0.052272)/5.555556, (x - 0.092809)/5.367655, lessThan(x, vec3(5.367655*0.010591 + 0.092809)));
}
vec3 oetf_arri_logc4(vec3 x) {
  return mix((exp2(14.0*(x - 0.09286412512218964)/0.9071358748778103 + 6.0) - 64.0)/2231.8263090676883, x*0.3033266726886969 - 0.7774983977293537, lessThan(x, vec3(-0.7774983977293537)));
}
vec3 oetf_red_log3g10(vec3 x) {
  return mix((pow(vec3(10.0), x/0.224282) - 1.0)/155.975327 - 0.01, (x/15.1927) - 0.01, lessThan(x, vec3(0.0)));
}
vec3 oetf_panasonic_vlog(vec3 x) {
  return mix(pow(vec3(10.0), (x - 0.598206)/0.241514) - 0.00873, (x - 0.125)/5.6, lessThan(x, vec3(0.181)));
}
vec3 oetf_sony_slog3(vec3 x) {
  return mix((pow(vec3(10.0), ((x*1023.0 - 420.0)/261.5))*(0.18 + 0.01) - 0.01), (x*1023.0 - 95.0)*0.01125/(171.2102946929 - 95.0), lessThan(x, vec3(171.2102946929/1023.0)));
}
vec3 oetf_fujifilm_flog2(vec3 x) {
  return mix(pow(vec3(10.0), ((x - 0.384316)/0.245281))/5.555556 - 0.064829/5.555556, (x - 0.092864)/8.799461, lessThan(x, vec3(0.100686685370811)));
}
