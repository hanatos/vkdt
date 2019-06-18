import numpy as np
import matplotlib.pyplot as plt

# define the curve via these control vertices here:
x=np.array([0.3,0.45,0.80,1.00])
y=np.array([0.0,0.62,0.14,0.97])
# M * c = y
# or, because python seems to have it upside down:
# c * M = y
# c = y * M.I
M=np.matrix([[1,1,1,1],x,x*x,x*x*x])
c = np.dot(y, M.I).A[0]

# we also want monotonicity, poly'(x) >= 0
# https://ws680.nist.gov/publication/get_pdf.cfm?pub_id=17206
# cubic polynomial on [0,1] is monotonic iff
# c0 (c0 + 2c1 + 3c2) >= 0   and
# sqrt( (c1^2 + 3c1c2)^2 + (2c0 + 2c1 + 3c2)^2 (3c0c2^2 - c1^2c2)^2 ) +
#       (c1^2 + 3c1c2)   + (2c0 + 2c1 + 3c2)   (3c0c2^2 - c1^2c2)    >= 0

# monotone hermite spline:
n=4;
m=np.array([0.0,0.0,0.0,0.0,0.0])
d=np.array([0.0,0.0,0.0,0.0,0.0])
for i in range(0,n-1):
  d[i]=(y[i+1] - y[i])/(x[i+1] - x[i])
d[n-1] = d[n-2];
m[0] = d[0];
m[n-1] = d[n-1]; # = d[n-2]
for i in range(1,n-1):
  if d[i-1]*d[i] <= 0:
    m[i] = 0.0
  else:
    m[i] = (d[i-1] + d[i])*.5
# extrapolate derivative by using previous curvature:
# m[n-1] = max((1.0 + m[n-2] + d[n-2] - d[n-3])*.5, 0.0)
m[n-1] = max(m[n-2] + d[n-2] - d[n-3], 0.0)
# monotone hermite clamping:
for i in range(0,n):
  if abs(d[i]) <= 0:
    m[i] = 0
    m[i+1] = 0
  else:
    alpha = m[i] / d[i]
    beta = m[i+1]/ d[i]
    tau = alpha * alpha + beta * beta
    if tau > 9 :
      m[i] = 3.0 * alpha * d[i] / np.sqrt(tau)
      m[i+1] = 3.0 * beta * d[i] / np.sqrt(tau)

def hermite(v):
  if v < x[0]:
    return y[0] + (v - x[0]) * m[0]
  if v > x[3]:
    return y[3] + (v - x[3]) * m[3]
  i=0
  if v > x[2]:
    i = 2
  elif v > x[1]:
    i = 1
  h = x[i+1] - x[i]
  t = (v - x[i])/h
  t2 = t * t;
  t3 = t * t2;
  h00 =  2.0 * t3 - 3.0 * t2 + 1.0;
  h10 =  1.0 * t3 - 2.0 * t2 + t;
  h01 = -2.0 * t3 + 3.0 * t2;
  h11 =  1.0 * t3 - 1.0 * t2;

  return h00 * y[i] + h10 * h * m[i] + h01 * y[i+1] + h11 * h * m[i+1];

fig, ax = plt.subplots(figsize=(5,3))
plt.ylim([-0.5,1.5])
plt.xlim([-0.5,1.5])
plt.axvline(x=0.0,ymin=0.0,ymax=1.0)
plt.axvline(x=1.0,ymin=0.0,ymax=1.0)
plt.axvline(x=x[1],ymin=0.0,ymax=1.0)
plt.axvline(x=x[2],ymin=0.0,ymax=1.0)
plt.axhline(y=0.0,xmin=0.0,xmax=1.0)
plt.axhline(y=1.0,xmin=0.0,xmax=1.0)
pos = np.linspace(-0.5, 1.5, 100)
plt.plot(pos, np.power(pos, 0)*c[0]+ pos*c[1] + pos*pos*c[2] + pos*pos*pos*c[3])
plt.plot(pos, np.array([hermite(t) for t in pos]))
fig.canvas.draw()
fig.savefig('test.pdf')
