# Spec r2 section 4.1-4.4: checks the envelope saturation S(v) = v / (1 + w e + (1 - w) |v|) (release tau)
# against the compression / harmonic / short-delay tables. Usage: sat_check_r2.py <w> <tau_ms> [hold_ms]
# The envelope holds its peak for hold_ms after each attack, then releases exponentially.
import sys, numpy as np
FS=48000.0
w=float(sys.argv[1]); tau=float(sys.argv[2])/1000
HOLD=int(round(float(sys.argv[3])/1000*FS)) if len(sys.argv)>3 else 0
rho=np.exp(-1/(tau*FS))
class Env:
    def __init__(s): s.e=0.0; s.h=0
    def __call__(s,a):
        if a>s.e: s.e=a; s.h=HOLD
        elif s.h>0: s.h-=1
        else: s.e=s.e*rho+a*(1-rho)
        return s.e
def S(x):
    env=Env(); y=np.empty_like(x)
    for i,v in enumerate(x):
        a=abs(v); e=env(a); y[i]=v/(1+w*e+(1-w)*a)
    return y
def harm(f,a):
    N=int(round(FS/f*40)); n0=int(0.3*FS); t=np.arange(n0+N); y=S(a*np.sin(2*np.pi*f*t/FS))[n0:]
    tt=np.arange(N); return [2*abs(np.dot(y,np.exp(-2j*np.pi*f*h*tt/FS)))/N for h in (1,3)]
worst=0
for f,t30,t12 in zip([20,50,100,200,500,1000,2000,5000],[-.23,-.24,-.24,-.25,-.26,-.27,-.27,-.27],[-1.66,-1.71,-1.74,-1.82,-1.88,-1.91,-1.92,-1.92]):
    g30=20*np.log10(harm(f,0.0316)[0]/0.0316); g12=20*np.log10(harm(f,0.251)[0]/0.251)
    worst=max(worst,abs(g30-t30)/0.15,abs(g12-t12)/0.15)
    print("4.2 %5d Hz  %.2f (%.2f)  %.2f (%.2f)"%(f,g30,t30,g12,t12))
for a,fd,h3,tl in zip([0.03,0.1,0.3,0.95],[-0.2,-0.8,-2.1,-5.3],[-52.9,-42.6,-33.5,-24.7],[2.5,2.5,2.5,4]):
    F,H3=harm(100,a); g=20*np.log10(F/a); h=20*np.log10(H3/F)
    worst=max(worst,abs(g-fd)/0.2,abs(h-h3)/tl)
    print("4.3 a=%.2f fund %.2f (%.1f) H3 %.1f (%.1f)"%(a,g,fd,h,h3))
tab={1:[0.419,0.437,0.452,0.467],2:[0.436,0.466,0.491,0.511],3:[0.452,0.490,0.519,0.540],5:[0.479,0.526,0.555,0.573],10:[0.5,0.546,0.569,0.582]}
for d,t in tab.items():
    D=int(round(d*FS/1000)); n=6*(D+1)+5; buf=np.zeros(n); x=np.zeros(n); x[0]=0.5
    # loop: v[n] = x[n] + 0.6 w[n-D-1]; out echo k at D + k(D+1)
    env=Env(); wv=np.zeros(n)
    for i in range(n):
        v=x[i]+(0.6*wv[i-D-1] if i-D-1>=0 else 0)
        a=abs(v); e=env(a); wv[i]=v/(1+w*e+(1-w)*a)
    pk=[abs(wv[k*(D+1)]) for k in range(5)]
    r=[pk[k+1]/pk[k] for k in range(4)]
    err=[20*np.log10(r[k]/t[k]) for k in range(4)]
    worst=max(worst,max(abs(q) for q in err)/0.3)
    print("4.4 %2d ms"%d, np.round(r,3), np.round(err,2))
print("WORST/TOL", round(worst,2))
