# Fit of the processed-path dynamics (control-rate model in dynsim.c) to band_steps.csv for one Punch value.
# Usage: python3 dyn_fit2.py <punch> [-v] [-fresh]
import numpy as np, os, sys, json, ctypes
from scipy.optimize import least_squares
HERE=os.path.dirname(os.path.abspath(__file__))
T=os.path.join(HERE,'../../../../spec/enhancer/r1/tables/')
SCR=HERE+'/'
lib=ctypes.CDLL(SCR+'dynsim.so')
dp=ctypes.POINTER(ctypes.c_double)
lib.simulate.argtypes=[dp,dp,dp,ctypes.c_int,dp,dp,ctypes.c_int,ctypes.c_double,dp,ctypes.c_int,dp,dp,ctypes.c_int]
BANDS='ABCDE'
OFFS=np.array([-1.563,-1.499,-1.781,-2.165,-0.697])
LV=np.array([-70,-50,-40,-30,-20,-10,-3.])
def arr(a): a=np.ascontiguousarray(a,dtype=np.float64); return a,a.ctypes.data_as(dp)

rows=[l.strip().split(',') for l in open(T+'settled_curves.csv') if l[0] in BANDS]
SET={}
for b,f,p,L,g in rows: SET.setdefault((b,int(p)),{})[float(L)]=float(g)
SET={k:np.array([v[l] for l in LV]) for k,v in SET.items()}
rows=[l.strip().split(',') for l in open(T+'band_steps.csv') if l[0] in BANDS]
STEPS={}
for b,f,p,si,L,t,g in rows: STEPS.setdefault((b,int(p)),[]).append((int(si),float(t),float(g)))
STARTS=[0.,2.,5.]
DT=0.00025

def sim(dyn,gsil,ceil,b,curve,times):
    p,pp=arr(list(dyn)+[gsil,ceil,OFFS[b]])
    lv,lvp=arr(LV); c,cp=arr(curve)
    sl,slp=arr([-10,-40,-10.]); sd,sdp=arr([2,3,2.])
    tm,tmp=arr(times); out=np.zeros(len(times)); o,op=arr(out)
    lib.simulate(pp,lvp,cp,7,slp,sdp,3,DT,tmp,len(times),op,None,0)
    return o

NDYN=11
def unpack(v):
    v=np.asarray(v); return np.abs(v[:NDYN]),v[NDYN:NDYN+5],v[NDYN+5:NDYN+10]

def resid(v,P,detail=False):
    dyn,gsil,ceil=unpack(v)
    r=[];det=[]
    for b in range(5):
        d=STEPS[(BANDS[b],P)]
        times=np.array([STARTS[si]+tm/1000 for si,tm,g in d])
        g=sim(dyn,gsil[b],ceil[b],b,SET[(BANDS[b],P)],times)
        for (si,tm,gm),gg in zip(d,g):
            tol=4. if tm<=50 else 2.
            if b==0 and tm<50: tol=8.   # 45 Hz: gain evaluated over one 22 ms period
            r.append((gg-gm)/tol); det.append((b,si,tm,gm,gg))
    return (np.array(r),det) if detail else np.array(r)

if __name__=='__main__':
    P=int(sys.argv[1])
    fn=SCR+f'dyn_params_{P}.json'
    if os.path.exists(fn) and '-fresh' not in sys.argv:
        v0=np.array(json.load(open(fn)))
    else:
        on=[np.mean([g for si,tm,g in STEPS[(b,P)] if si==0 and tm<=20]) for b in BANDS]
        pk=[max(g for si,tm,g in STEPS[(b,P)] if si==1) for b in BANDS]
        v0=np.array([0.002,0.01,0.15,0.45,0.7,2.0,2.0,0.03,0.08,0.005,0.03]+on+[p+1 for p in pk])
    best=None
    starts=[v0]
    if '-multi' in sys.argv:
        rng=np.random.default_rng(1)
        for k in range(int(os.environ.get('NSTART','2'))):
            v=v0.copy(); v[:NDYN]*=np.exp(rng.normal(0,0.7,NDYN)); starts.append(v)
    for v in starts:
        s=least_squares(lambda x:resid(x,P),v,diff_step=1e-4,max_nfev=250)
        c=np.sum(s.fun**2)
        if best is None or c<best[0]: best=(c,s.x)
    v=best[1]
    json.dump(list(map(float,v)),open(fn,'w'))
    r,det=resid(v,P,True)
    print('punch',P,'max %.2f rms %.3f'%(np.max(np.abs(r)),np.sqrt(np.mean(r**2))))
    dyn,gsil,ceil=unpack(v)
    print('dyn tfa tfr tsa tsr beta kappa width tgu tgd tgu2 tgd2\n',np.round(dyn,4)); print('gsil',np.round(gsil,2),'ceil',np.round(ceil,2))
    if '-v' in sys.argv:
        for b in range(5):
            print(BANDS[b]+': '+' '.join('%d/%g:%.1f(%+.1f)'%(si,tm,gm,gg-gm) for bb,si,tm,gm,gg in det if bb==b))
