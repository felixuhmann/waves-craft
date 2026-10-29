# Spec r3 section 9.2 / 9.2.1: refit of the Analog wet chains with the click (transient) vectors added.
# Changes vs analog_fit_r2.py: DC blocker at 1 Hz, no slow envelope compressor (kc = 0: a slow compressor lets
# instantaneous expansion through on clicks), click residuals per §9.2.1.
# Usage: analog_fit_r3.py <mode> [restarts] [seed]     (starts from the r2 constants in START)
import sys, numpy as np
from scipy.signal import lfilter
from scipy.optimize import least_squares
_argv=sys.argv; sys.argv=[_argv[0],'0']
import analog_fit_r2 as F
sys.argv=_argv
FS=F.FS
DCHZ=1.0
def rb_lp(f,Q):
    w0=2*np.pi*f/FS; al=np.sin(w0)/(2*Q); c=np.cos(w0)
    b=np.array([(1-c)/2,1-c,(1-c)/2]); a=np.array([1+al,-2*c,1-al]); return b/a[0],a/a[0]
def A(P,u):
    rb=F.rbj
    u=u*10**(P['g']/20)
    if P['lsg']!=0: u=lfilter(*rb('ls',P['lsf'],0.6,P['lsg']),u)
    if P['hsg']!=0: u=lfilter(*rb('hs',P['hsf'],0.6,P['hsg']),u)
    if P['pkg']!=0: u=lfilter(*rb('pk',P['pkf'],0.7,P['pkg']),u)
    if P['kb']!=0:
        b=P['kb']*lfilter(*rb('bp',P['bpf'],P['bpq'],0),u)
        t=np.where(b>0,P['tp'],P['tn']); u=u-t*np.tanh(b/t)
    # memoryless terms; with nlf > 0 they act only on a low-passed band b (added back as a difference),
    # like low-frequency transformer saturation: broadband clicks are hardly affected
    b=lfilter(*rb_lp(P['nlf'],0.707),u) if P.get('nlf',0)>0 else u
    y=b
    if P['xo']!=0: y=y-P['xo']*np.tanh(y/P['xt'])
    if P['c2']!=0: y=y+P['c2']*y*y/np.sqrt(y*y+P['dl']**2)
    y=y+P['k3']*y**3
    y=y+P['c3']*y*np.abs(y)
    u=u+(y-b)
    c=np.exp(-2*np.pi*DCHZ/FS)
    return lfilter([(1+c)/2,-(1+c)/2],[1,-c],u)
F.A=A   # evaluate() of the r2 module uses F.A
F.BOUNDS['xo']=(-0.01,0.05); F.BOUNDS['nlf']=(50,3000); F.NEUTRAL['nlf']=0   # negative: low-level expansion (adds odd harmonics in phase with the saturation)
D=4800
CLICKS={1:[(0.3,0.8,[-0.34,-0.29,-0.27,-0.24,-0.22,-0.22]),(0.5,0.6,[-0.28,-0.24,-0.21,-0.21,-0.21,-0.21])],
        2:[(0.3,0.8,[-0.92,-0.79,-0.71,-0.66,-0.61,-0.57])],
        3:[(0.3,0.8,[-0.42,-0.35,-0.30,-0.27,-0.24,-0.22]),(0.5,0.6,[-0.29,-0.23,-0.21,-0.17,-0.15,-0.16])],
        4:[(0.3,0.8,[-0.02,-0.02,-0.02,-0.01,-0.02,-0.02])]}
def click_train(a,g):
    w=[]; v=a
    for k in range(6):
        wk=v/(1+abs(v)); w.append(wk); v=g*wk
    x=np.zeros(7*D)
    for k in range(6): x[D+k*(D+1)]=w[k]
    return x,w
def evaluate(mode,P):
    r,t=F.evaluate(mode,P)
    r=list(r); t=list(t)
    for a,g,tg in CLICKS[mode]:
        x,w=click_train(a,g); y=A(P,x)
        for k in range(6):
            n=D+k*(D+1); pk=np.abs(y[n-3:n+4]).max()
            d=20*np.log10(pk/w[k])
            r.append(d-tg[k]); t.append(0.5)
            r.append(max(d,0.0)); t.append(0.05)   # never louder than Off
    return np.array(r),np.array(t)
START={1:dict(g=0.35,lsf=101.364,lsg=-3.08066,hsf=8654.51,hsg=-0.241443,nlf=300,xo=0.0035,xt=0.037,c2=0.066,dl=0.1,k3=0.0,c3=-0.5),
       2:dict(g=-0.555854,hsf=1122.48,hsg=-1.09002,c2=0.00827611,dl=0.347758,k3=0.0,c3=0.0775933),
       3:dict(g=0.880936,hsf=3897.44,hsg=-1.40896,pkf=1240.78,pkg=-2.41597,bpf=96.9773,bpq=0.229789,kb=0.999321,tp=0.019767,tn=0.0343756,c2=0.10833,dl=0.020187,k3=0.0,c3=-0.167766),
       4:dict(g=0.0,xo=-0.0009,xt=0.002,k3=0.0,c3=0.0)}
FREE={1:['g','lsf','lsg','hsf','hsg','nlf','xo','xt','c2','dl','k3','c3'],
      2:['g','hsf','hsg','c2','dl','k3','c3'],
      3:['g','hsf','hsg','pkf','pkg','bpf','bpq','kb','tp','tn','c2','dl','k3','c3'],
      4:['g','xo','xt','k3','c3']}
if __name__=="__main__":
    mode=int(sys.argv[1]); restarts=int(sys.argv[2]) if len(sys.argv)>2 else 2
    rng=np.random.default_rng(int(sys.argv[3]) if len(sys.argv)>3 else mode)
    free=FREE[mode]
    lo=np.array([F.BOUNDS[n][0] for n in free]); hi=np.array([F.BOUNDS[n][1] for n in free])
    def P_of(x):
        P=dict(F.NEUTRAL); P.update(dict(zip(free,x))); return P
    def res(x):
        e,t=evaluate(mode,P_of(x)); return e/t
    x0=np.clip(np.array([START[mode][n] for n in free]),lo+1e-9,hi-1e-9)
    best=None
    for tr in range(restarts):
        xs=x0 if (tr==0 and len(sys.argv)<=4) else np.clip(x0*rng.uniform(0.7,1.3,len(x0)),lo+1e-9,hi-1e-9)
        sol=least_squares(res,xs,bounds=(lo,hi),max_nfev=250,diff_step=1e-3)
        w=np.abs(sol.fun).max()
        print(tr,round(w,3),"PARAMS",{n:float('%.6g'%v) for n,v in zip(free,sol.x)},flush=True)
        if best is None or w<best[0]: best=(w,sol.x)
    print("worst/tol",round(best[0],3))
    print("rel",np.round(res(best[1]),2))
