vec4 input_transform(vec4 v)
{ // make 0.5 have unit derivative
  return log(max(vec4(1e-8), v / 0.5));
}

vec4 output_transform(vec4 v)
{
  return exp(v) * 0.5;
}
