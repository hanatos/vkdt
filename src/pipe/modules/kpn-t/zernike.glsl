// normalised to integrate to pi over the unit disk, i.e.:
// for an orthonormal basis need to multiply these by 1/pi.
float
zernike(
    uint i, 
    float rho,
    float phi)
{
  if(rho > 1.0) return 0.0;
  switch(i)
  { // osa/ansi index
  case 0:
    return 1.0;
  case 1:
    return 2.0 * rho * sin(phi);
  case 2:
    return 2.0 * rho * cos(phi);
  case 3:
    return sqrt(6.0) * rho*rho * sin(2.0*phi);
  case 4:
    return sqrt(3.0) * (2.0*rho*rho - 1.0);
  case 5:
    return sqrt(6.0) * rho*rho * cos(2.0*phi);
  case 6:
    return sqrt(8.0) * rho*rho*rho * sin(3.0*phi);
  case 7:
    return sqrt(8.0) * (3.0 * rho*rho*rho - 2.0*rho) * sin(phi);
  case 8:
    return sqrt(8.0) * (3.0 * rho*rho*rho - 2.0*rho) * cos(phi);
  case 9:
    return sqrt(8.0) * rho*rho*rho * cos(3.0*phi);
  case 10:
    return sqrt(10.0) * rho*rho*rho*rho * sin(4.0*phi);
  case 11:
    return sqrt(10.0) * (4.0*rho*rho*rho*rho - 3.0*rho*rho) * sin(2.0*phi);
  case 12:
    return sqrt(5.0) * (6.0*rho*rho*rho*rho - 6.0*rho*rho + 1.0);
  case 13:
    return sqrt(10.0) * (4.0*rho*rho*rho*rho - 3.0*rho*rho) * cos(2.0*phi);
  case 14:
    return sqrt(10.0) * rho*rho*rho*rho * cos(4.0*phi);
  default:
    return 0.0;
  }
}

// TODO: now we have means to measure zernike moments as a vector:
// m = {<px, z_i>}_i
// where px (and accordingly z_i) are discretised at sampling locations j as px_j
// so
// m = A . px
// where A_ij = z_ij
// for j in 21 sampling locations around the development point in a 5x5 grid and
// for i in 0..5 or 9 or 14 zernike polynomials (likely I << J)
//
//       |           |
// m_j = |           | px
//       |           |
//
// i.e. there is no left inverse because A has more columns than rows.
// how do we now compute the expected value in the center?
// 1) straight zernike expansion using the moments as coefficients:
//    sum all coeff * corresponding zernike poly * 1/pi
// 2) compute moore/penrose pseudo inverse of A and px from that?
//    may not be useful in this underdetermined setting

#if 0
int main(int argc, char *argv[])
{
  const int R = 2;
  const int wd = 2*R+1, ht = 2*R+1;

  float *buf = malloc(sizeof(float)*3*wd*ht);
  for(int i=0;i<15;i++)
  {
    int k = 0;
    for(int r0=-R;r0<=R;r0++)
      for(int r1=-R;r1<=R;r1++)
      {
        double rho = sqrt(r0*r0+r1*r1)/(R+0.5);
        double phi = atan2(r0, r1);
        for(int c=0;c<3;c++) buf[3*k+c] = 0.5 + 0.5 * zernike(i, rho, phi);
        k++;
      }
    char filename[256];
    snprintf(filename, sizeof(filename), "zernike_%02d.pfm", i);
    FILE *f = fopen(filename, "wb");
    if(f)
    {
      fprintf(f, "PF\n%d %d\n-1.0\n", wd, ht);
      fwrite(buf, sizeof(float)*3, wd*ht, f);
      fclose(f);
    }
  }
  exit(0);
}
#endif
