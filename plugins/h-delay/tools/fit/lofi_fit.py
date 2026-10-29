import numpy as np, sys
from scipy.optimize import least_squares

FS=[44100.,48000.,96000.]
pts=[(5000,-1.0),(6000,-4.4),(7000,-12),(8000,-27)]
pb=np.linspace(20,4000,40)
sb=np.linspace(10000,48000,80)

def coeffs(f0,Q,fz,fs):
    # analog section prewarped (w0 and wz individually), bilinear with K=2fs
    K=2*fs
    w0=K*np.tan(np.pi*min(f0,0.45*fs)/fs)
    if fz<0.49*fs:
        wz=K*np.tan(np.pi*fz/fs); nb=np.array([w0*w0/(wz*wz),0,w0*w0])
    else:
        nb=np.array([0,0,w0*w0])
    na=np.array([1,w0/Q,w0*w0])
    # bilinear: s=K(1-z^-1)/(1+z^-1)
    def bl(c):
        a2,a1,a0=c  # c[0]s^2+c[1]s+c[2]
        return np.array([a2*K*K+a1*K+a0, -2*a2*K*K+2*a0, a2*K*K-a1*K+a0])
    b=bl(nb); a=bl(na)
    return b/a[0],a/a[0]

def resp(p,fs,f):
    f=np.asarray(f,float); z=np.exp(-1j*2*np.pi*f/fs)
    H=np.ones(len(f),complex)
    n=len(p)//3
    for i in range(n):
        b,a=coeffs(np.exp(p[3*i]),np.exp(p[3*i+1]),np.exp(p[3*i+2]),fs)
        H*=(b[0]+b[1]*z+b[2]*z*z)/(a[0]+a[1]*z+a[2]*z*z)
    return 20*np.log10(np.abs(H)+1e-30)

def res(p):
    r=[]
    for fs in FS:
        r+=list(resp(p,fs,pb)/0.015)
        for f,t in pts:
            r.append((resp(p,fs,[f])[0]-t)/0.25)
        s=resp(p,fs,[9000])[0]; r.append((s+52)/2.0)
        sbf=sb[sb<0.5*fs*0.999]
        s=resp(p,fs,sbf); r+=list(np.maximum(0,s+58)/0.3)
    return np.array(r)

nsec=int(sys.argv[1]) if len(sys.argv)>1 else 4
best=None
for trial in range(60):
    rng=np.random.default_rng(trial)
    p=[]
    for i in range(nsec):
        p+= [np.log(rng.uniform(5000,9000)), np.log(rng.uniform(0.5,10)), np.log(rng.uniform(9000,20000))]
    sol=least_squares(res,p,max_nfev=4000)
    c=np.sum(sol.fun**2)
    if best is None or c<best[0]:
        best=(c,sol.x); print(nsec,trial,c,flush=True)
c,p=best
print(repr(list(p)))
for i in range(nsec): print("f0=%.3f Q=%.5f fz=%.3f"%tuple(np.exp(p[3*i:3*i+3])))
for fs in FS:
    f=[1000,3000,4000,4500,5000,6000,7000,8000,9000,10000,12000,15000,20000,30000,40000]
    f=[x for x in f if x<fs/2]
    print(fs,[ "%d:%.2f"%(a,b) for a,b in zip(f,resp(p,fs,f))])
