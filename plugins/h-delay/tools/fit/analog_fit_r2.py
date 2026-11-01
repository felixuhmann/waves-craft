# Spec r2 section 9.2: fit of the Analog colour chain A applied to the wet signal after the r2 envelope
# saturation S (wet = A(S(x)) for Feedback 0). Usage: analog_fit_r2.py <mode> [restarts]
import sys, numpy as np
from scipy.signal import lfilter
from scipy.optimize import least_squares
FS=48000.0
TAU=0.008; HOLD=int(round(0.0011*FS))   # implemented saturation (see Saturation.h / sat_check_r2.py)
def sat(x):
    rho=np.exp(-1/(TAU*FS)); e=0.0; h=0; y=np.empty_like(x)
    for i,v in enumerate(x):
        a=abs(v)
        if a>e: e=a; h=HOLD
        elif h>0: h-=1
        else: e=e*rho+a*(1-rho)
        y[i]=v/(1+0.72*e+0.28*a)
    return y
RF=[20,50,100,200,500,1000,2000,5000,10000,20000]
RESP={1:[-3.4,-3.0,-1.7,-0.8,-0.4,-0.3,-0.3,-0.3,-0.4,-0.5],
      2:[-0.7,-0.7,-0.7,-0.7,-0.8,-1.0,-1.4,-1.5,-1.7,-1.7],
      3:[-2.1,-6.3,-8.9,-5.8,-3.6,-2.1,-1.1,-0.6,-0.5,-0.5],
      4:[-0.4,-0.3,-0.4,-0.5,-0.5,-0.5,0.0,-0.2,-0.2,0.0]}
RTOL={m:[1.0]*10 for m in RESP}; RTOL[3]=[1,3,3,3,3,1,1,1,1,1]
AMPS=[0.03,0.1,0.3,0.95]
NONE=None
DIST={0:[(-0.2,NONE,-52.9),(-0.8,NONE,-42.6),(-2.1,NONE,-33.5),(-5.3,NONE,-24.7)],
      1:[(-2.1,-42,-44),(-2.2,-34,-35.5),(-2.9,-29,-30),(-5.5,-28,-24)],
      2:[(-0.9,-70,-55),(-1.4,-60,-45),(-2.5,-53,-35),(-5.5,-50,-26)],
      3:[(-10.6,-13.6,-9.3),(-3.6,-19.5,-20.2),(-2.9,-23.2,-33.8),(-5.5,-30.6,-27.5)],
      4:[(-0.6,NONE,-36),(-0.8,NONE,-40.6),(-2.1,NONE,-33.4),(-5.3,NONE,-24.7)]}
H3TOL=[4,4,4,4]
def rbj(kind,f,Q,gdb):
    w0=2*np.pi*f/FS; A=10**(gdb/40); al=np.sin(w0)/(2*Q); c=np.cos(w0)
    if kind=='ls':
        s=2*np.sqrt(A)*al
        b=[A*((A+1)-(A-1)*c+s),2*A*((A-1)-(A+1)*c),A*((A+1)-(A-1)*c-s)]; a=[(A+1)+(A-1)*c+s,-2*((A-1)+(A+1)*c),(A+1)+(A-1)*c-s]
    elif kind=='hs':
        s=2*np.sqrt(A)*al
        b=[A*((A+1)+(A-1)*c+s),-2*A*((A-1)+(A+1)*c),A*((A+1)+(A-1)*c-s)]; a=[(A+1)-(A-1)*c+s,2*((A-1)-(A+1)*c),(A+1)-(A-1)*c-s]
    elif kind=='bp':
        b=[al,0,-al]; a=[1+al,-2*c,1-al]
    else:
        b=[1+al*A,-2*c,1-al*A]; a=[1+al/A,-2*c,1-al/A]
    return np.array(b)/a[0],np.array(a)/a[0]
NAMES=['g','lsf','lsg','hsf','hsg','pkf','pkg','bpf','bpq','kb','tp','tn','xo','xt','c2','dl','k3','c3','kc']
NEUTRAL=dict(g=0,lsf=100,lsg=0,hsf=3000,hsg=0,pkf=1000,pkg=0,bpf=100,bpq=0.3,kb=0,tp=0.02,tn=0.05,xo=0,xt=0.01,c2=0,dl=0.1,k3=0,c3=0,kc=0)
BOUNDS=dict(g=(-3,3),lsf=(30,1000),lsg=(-12,0),hsf=(300,10000),hsg=(-6,0),pkf=(300,4000),pkg=(-5,0),bpf=(40,300),bpq=(0.1,4),kb=(0,1),
            tp=(0.001,0.5),tn=(0.001,0.5),xo=(0,0.05),xt=(0.0005,0.2),c2=(-0.3,0.3),dl=(0.001,1),k3=(-0.5,0.5),c3=(-1,2),kc=(0,3))
FREE={1:['g','lsf','lsg','hsf','hsg','c2','dl','c3','kc','xo','xt'],
      2:['g','hsf','hsg','c2','dl','c3','kc'],
      3:['g','hsf','hsg','pkf','pkg','bpf','bpq','kb','tp','tn','c2','dl','c3','kc'],
      4:['g','xo','xt','k3','c3','kc']}
def A(P,u):
    u=u*10**(P['g']/20)
    if P['lsg']!=0: u=lfilter(*rbj('ls',P['lsf'],0.6,P['lsg']),u)
    if P['hsg']!=0: u=lfilter(*rbj('hs',P['hsf'],0.6,P['hsg']),u)
    if P['pkg']!=0: u=lfilter(*rbj('pk',P['pkf'],0.7,P['pkg']),u)
    if P['kb']!=0:
        b=P['kb']*lfilter(*rbj('bp',P['bpf'],P['bpq'],0),u)
        t=np.where(b>0,P['tp'],P['tn']); u=u-t*np.tanh(b/t)
    if P['xo']!=0: u=u-P['xo']*np.tanh(u/P['xt'])
    if P['c2']!=0: u=u+P['c2']*u*u/np.sqrt(u*u+P['dl']**2)
    u=u+P['k3']*u**3
    u=u+P['c3']*u*np.abs(u)
    if P['kc']!=0:
        pp=np.exp(-1/(0.02*FS)); env=lfilter([1-pp],[1,-pp],np.abs(u)); u=u/(1+P['kc']*env)
    c=np.exp(-2*np.pi*5/FS)
    return lfilter([(1+c)/2,-(1+c)/2],[1,-c],u)
def tone(f,a,periods=40,settle=0.6):
    N=int(round(FS/f*periods)); n0=int(settle*FS); t=np.arange(n0+N)
    return a*np.sin(2*np.pi*f*t/FS),n0,N
def harm(y,f,n0,N):
    t=np.arange(N); seg=y[n0:n0+N]
    return [2*abs(np.dot(seg,np.exp(-2j*np.pi*f*h*t/FS)))/N for h in (1,2,3)]
# cache S(x)
RC=[];
for f in RF:
    x,n0,N=tone(f,10**(-1.5)); s=sat(x); RC.append((f,s,n0,N,harm(s,f,n0,N)[0]))
DC=[]
for a in AMPS:
    x,n0,N=tone(100,a); DC.append((a,sat(x),n0,N))
def evaluate(mode,P):
    r=[]; tol=[]
    if mode>0:
        for (f,s,n0,N,ref),tg,tl in zip(RC,RESP[mode],RTOL[mode]):
            r.append(20*np.log10(harm(A(P,s),f,n0,N)[0]/ref)-tg); tol.append(tl)
    for k,(a,s,n0,N) in enumerate(DC):
        y=A(P,s) if mode>0 else s
        F,H2,H3=harm(y,100,n0,N); fd,h2,h3=DIST[mode][k]
        r.append(20*np.log10(F/a)-fd); tol.append(1.5 if (mode==3 and k==0) else 0.5)
        if h2 is not None: r.append(20*np.log10(H2/F+1e-15)-h2); tol.append(4)
        r.append(20*np.log10(H3/F+1e-15)-h3); tol.append(H3TOL[k])
    return np.array(r),np.array(tol)
if __name__=="__main__":
    mode=int(sys.argv[1])
    if mode==0:
        e,t=evaluate(0,NEUTRAL); print(np.round(e,2)); sys.exit()
    free=FREE[mode]; restarts=int(sys.argv[2]) if len(sys.argv)>2 else 4
    lo=np.array([BOUNDS[n][0] for n in free]); hi=np.array([BOUNDS[n][1] for n in free])
    def P_of(x):
        P=dict(NEUTRAL); P.update(dict(zip(free,x))); return P
    def res(x):
        e,t=evaluate(mode,P_of(x)); return e/t
    rng=np.random.default_rng(int(sys.argv[3]) if len(sys.argv)>3 else mode); best=None
    for tr in range(restarts):
        x0=lo+(hi-lo)*rng.uniform(0.2,0.8,len(free))
        if mode==3: x0[free.index('bpf')]=rng.uniform(80,130)
        try: sol=least_squares(res,x0,bounds=(lo,hi),max_nfev=400,diff_step=1e-3)
        except Exception as ex: print(ex); continue
        w=np.abs(sol.fun).max()
        print(tr,round(w,3),flush=True)
        if best is None or w<best[0]: best=(w,sol.x)
    w,x=best
    print("worst/tol",round(w,3))
    print("PARAMS",{n:float('%.6g'%v) for n,v in zip(free,x)})
    print("rel",np.round(res(x),2))
