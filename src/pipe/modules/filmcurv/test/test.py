import numpy as np
import matplotlib.pyplot as plt

pos = np.linspace(-0.5, 1.5, 100)
x=np.array([0.0,0.20,0.8,1.0])
y=np.array([0.0,0.20,0.9,1.0])
# M * c = y
# or, because python seems to have it upside down:
# c * M = y
# c = y * M.I
# sanity check:
# c * M = ?
# we also want monotonicity, poly'(x) >= 0
# 3*x^2 * c[3] + 2*x c[2] + c[1] >= 0
M=np.matrix([[1,1,1,1],x,x*x,x*x*x])
c = np.dot(y, M.I).A[0]

# https://ws680.nist.gov/publication/get_pdf.cfm?pub_id=17206
# cubic polynomial on [0,1] is monotonic iff
# c0 (c0 + 2c1 + 3c2) >= 0   and
# sqrt( (c1^2 + 3c1c2)^2 + (2c0 + 2c1 + 3c2)^2 (3c0c2^2 - c1^2c2)^2 ) +
#       (c1^2 + 3c1c2)   + (2c0 + 2c1 + 3c2)   (3c0c2^2 - c1^2c2)    >= 0
n=4;
m=np.array([0.0,0.0,0.0,0.0,0.0])
d=np.array([0.0,0.0,0.0,0.0,0.0])
for i in range(0,n-1):
  d[i]=(y[i+1] - y[i])/(x[i+1] - x[i])
d[n-1] = d[n-2];
m[0] = d[0];
m[n-1] = d[n-1];
for i in range(1,n-1):
  m[i] = (d[i-1] + d[i])*.5
# XXX 
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
    # h = x[1] - x[0]
    return x[0] + (v - x[0]) * m[0]
  if v > x[3]:
    h = x[3] - x[2]
    t = 1.0
    t2 = t * t;
    h00 =  6.0 * t2 - 6.0 * t;
    h10 =  3.0 * t2 - 4.0 * t + 1.0;
    h01 = -6.0 * t2 + 6.0 * t;
    h11 =  3.0 * t2 - 2.0 * t;
    # == h * m[3]
    dv = h00 * y[2] + h10 * h * m[2] + h01 * y[3] + h11 * h * m[3];
    return y[3] + (v - x[3]) * m[3] #dv
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

# h00 = (1.0+2.0*t)*(1.0-t)*(1.0-t)
#  h10 = t*t*(3.0-2.0*t)
#  h01 = t*(1.0-t)*(1.0-t) 
#  h11 = t*t*(t-1.0)
#  return y[i]*h00 + h*m[i]*h10 + y[i+1]*h01 + h*m[i+1]*h11


# plt.plot(pos, np.dot(np.array([np.power(pos, 0),pos,pos*pos,pos*pos*pos]),c))
# plt.plot(pos, np.polyval(c, pos))

fig, ax = plt.subplots(figsize=(5,3))
# plot = ax.plot(pos)[0]
# plot.set_ydata(np.polyval(c, pos))
plt.plot(pos, np.power(pos, 0)*c[0]+ pos*c[1] + pos*pos*c[2] + pos*pos*pos*c[3])
plt.plot(pos, np.array([hermite(t) for t in pos]))
fig.canvas.draw()
fig.savefig('test.pdf')
