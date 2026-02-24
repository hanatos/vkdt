layout(std140, set = 0, binding = 1) uniform params_t
{
  int i_gamut;
  int i_oetf;
  float tn_Lp;
  float tn_gb;
  float pt_hdr;
  float tn_Lg;

  int crv_en;
  float tn_con;
  float tn_sh;
  float tn_toe;
  float tn_off;

  int tn_hc_en;
  float tn_hc;
  float tn_hc_pv;
  float tn_hc_st;

  int tn_lc_en;
  float tn_lc;
  float tn_lc_w;

  int cwp;
  float cwp_lm;
  float rs_sa;
  float rs_rw;
  float rs_bw;

  int pt_en;
  float pt_lml;
  float pt_lml_r;
  float pt_lml_g;
  float pt_lml_b;
  float pt_lmh;
  float pt_lmh_r;
  float pt_lmh_b;

  int ptl_en;
  float ptl_c;
  float ptl_m;
  float ptl_y;

  int ptm_en;
  float ptm_lo;
  float ptm_lo_r;
  float ptm_lo_s;
  float ptm_hi;
  float ptm_hi_r;
  float ptm_hi_s;

  int brl_en;
  float brl;
  float brl_r;
  float brl_g;
  float brl_b;
  float brl_rng;
  float brl_st;

  int brlp_en;
  float brlp;
  float brlp_r;
  float brlp_g;
  float brlp_b;

  int hc_en;
  float hc_r;
  float hc_r_r;

  int hsrgb_en;
  float hs_r;
  float hs_r_r;
  float hs_g;
  float hs_g_r;
  float hs_b;
  float hs_b_r;

  int hscmy_en;
  float hs_c;
  float hs_c_r;
  float hs_m;
  float hs_m_r;
  float hs_y;
  float hs_y_r;
  int clmp;
  int tn_su;
  int o_gamut;
  int eotf;
} params;

