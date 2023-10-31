// by Nikos Papadopoulos, 4rknova / 2015
// WTFPL

// Based on FlyGuy's shader: https://www.shadertoy.com/view/llSGRm
// #define FONTSC_SZ vec2(2.5, 5)     // Font size
#define FONTSC_SZ vec2(2, 3)     // Font size
// #define SCREEN_SZ vec2(800, 600) // Screen size
#define CHR       vec4(6,7,6.*FONTSC_SZ.x,9.*FONTSC_SZ.y)  // Character size (xy), spacing(zw)
// #define STR_SZ(c) vec2(c*CHR.zw) // String size
// #define DWN_SC    2.             // Downscale

#define C vec2
C c_spc = C(      0,      0), c_exc = C( 276705,  32776), c_quo = C(1797408,      0)
, c_hsh = C(  10738,1134484), c_dol = C( 538883,  19976), c_pct = C(1664033,  68006)
, c_amp = C( 545090, 174362), c_apo = C( 798848,      0), c_lbr = C( 270466,  66568)
, c_rbr = C( 528449,  33296), c_ast = C(  10471,1688832), c_crs = C(   4167,1606144)
, c_per = C(      0,   1560), c_dsh = C(      7,1572864), c_com = C(      0,   1544)
, c_lsl = C(   1057,  67584), c_0   = C( 935221, 731292), c_1   = C( 274497,  33308)
, c_2   = C( 934929,1116222), c_3   = C( 934931,1058972), c_4   = C( 137380,1302788)
, c_5   = C(2048263,1058972), c_6   = C( 401671,1190044), c_7   = C(2032673,  66576)
, c_8   = C( 935187,1190044), c_9   = C( 935187,1581336), c_col = C(    195,   1560)
, c_scl = C(    195,   1544), c_les = C( 135300,  66052), c_equ = C(    496,   3968)
, c_grt = C( 528416, 541200), c_que = C( 934929,1081352), c_ats = C( 935285, 714780)
, c_A   = C( 935188, 780450), c_B   = C(1983767,1190076), c_C   = C( 935172, 133276)
, c_D   = C(1983764, 665788), c_E   = C(2048263,1181758), c_F   = C(2048263,1181728)
, c_G   = C( 935173,1714334), c_H   = C(1131799,1714338), c_I   = C( 921665,  33308)
, c_J   = C(  66576, 665756), c_K   = C(1132870, 166178), c_L   = C(1065220, 133182)
, c_M   = C(1142100, 665762), c_N   = C(1140052,1714338), c_O   = C( 935188, 665756)
, c_P   = C(1983767,1181728), c_Q   = C( 935188, 698650), c_R   = C(1983767,1198242)
, c_S   = C( 935171,1058972), c_T   = C(2035777,  33288), c_U   = C(1131796, 665756)
, c_V   = C(1131796, 664840), c_W   = C(1131861, 699028), c_X   = C(1131681,  84130)
, c_Y   = C(1131794,1081864), c_Z   = C(1968194, 133180), c_lsb = C( 925826,  66588)
, c_rsl = C(  16513,  16512), c_rsb = C( 919584,1065244), c_pow = C( 272656,      0)
, c_usc = C(      0,     62), c_a   = C(    224, 649374), c_b   = C(1065444, 665788)
, c_c   = C(    228, 657564), c_d   = C(  66804, 665758), c_e   = C(    228, 772124)
, c_f   = C( 401543,1115152), c_g   = C(    244, 665474), c_h   = C(1065444, 665762)
, c_i   = C( 262209,  33292), c_j   = C( 131168,1066252), c_k   = C(1065253, 199204)
, c_l   = C( 266305,  33292), c_m   = C(    421, 698530), c_n   = C(    452,1198372)
, c_o   = C(    228, 665756), c_p   = C(    484, 667424), c_q   = C(    244, 665474)
, c_r   = C(    354, 590904), c_s   = C(    228, 114844), c_t   = C(   8674,  66824)
, c_u   = C(    292,1198868), c_v   = C(    276, 664840), c_w   = C(    276, 700308)
, c_x   = C(    292,1149220), c_y   = C(    292,1163824), c_z   = C(    480,1148988)
, c_lpa = C( 401542,  66572), c_bar = C( 266304,  33288), c_rpa = C( 788512,1589528)
, c_tid = C( 675840,      0), c_lar = C(   8387,1147904);

vec2 carret = vec2(0);

// Returns the digit sprite for the given number.
vec2 digit(float d) {    
  vec3 r = vec3(0, 0, floor(d));
  if      (r.z == 0.) r.xy = c_0; else if (r.z == 1.) r.xy = c_1;
  else if (r.z == 2.) r.xy = c_2; else if (r.z == 3.) r.xy = c_3;
  else if (r.z == 4.) r.xy = c_4; else if (r.z == 5.) r.xy = c_5;
  else if (r.z == 6.) r.xy = c_6; else if (r.z == 7.) r.xy = c_7;
  else if (r.z == 8.) r.xy = c_8; else if (r.z == 9.) r.xy = c_9;
  return r.xy;
}
// Extracts bit
float bit(float n, float b) {
  b = clamp(b,-1.,22.);
  return floor(mod(floor(n / pow(2.,floor(b))),2.));
}
// Returns the pixel at uv in the given bit-packed sprite.
float spr(vec2 spr, vec2 size, vec2 uv)
{
  uv = floor(uv/ FONTSC_SZ);
  //Calculate the bit to extract (x + y * width) (flipped on x-axis)
  float b = (size.x-uv.x-1.0) + uv.y * size.x;

  //Clipping bound to remove garbage outside the sprite's boundaries.
  bool bounds = all(greaterThanEqual(uv,vec2(0)));
  bounds = bounds && all(lessThan(uv,size));

  return bounds ? bit(spr.x, b - 21.0) + bit(spr.y, b) : 0.0;
}
// Prints a character and moves the carret forward by 1 character width.
float print_char(vec2 ch, vec2 uv) { 
  float px = spr(ch, CHR.xy, uv - carret);
  carret.x += CHR.z;
  return px;
}
// Prints out the given number starting at pos.
float print_number(float number,vec2 pos, vec2 uv)
{
  vec2 dec_pos = pos;
  float result = 0.;

  [[unroll]] for(int i = 3; i >= -2; --i) {
    //Clip off leading zeros.
    float clip = float(abs(number) > pow(10.0, float(i)) || i <= 0);        
    float d = mod(number / pow(10., float(i)),10.);

    if(i == -1) {
      result += spr(c_per,CHR.xy, uv - dec_pos);
      dec_pos.x += CHR.z;
    }
    result += spr(digit(d),CHR.xy, uv - dec_pos) * clip;
    dec_pos.x += CHR.z * clip;
  }

  return result;
}
