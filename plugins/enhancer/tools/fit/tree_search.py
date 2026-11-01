# Search over crossover-tree topologies with zero-phase elementary splits P(f/fc) (|H|^2 of a 4th-order all-pole H,
# realised as H^2 with allpass compensation). Fits P's 4 coefficients to both band tables at 48 kHz (bilinear, prewarped).
import numpy as np, os, itertools
from scipy.optimize import least_squares
T=os.path.join(os.path.dirname(__file__),'../../../../spec/enhancer/r1/tables/')
fs=48000.
sets=[]
for name,fcs in [('default',[88,494,2576,9285]),('set2',[197,990,3978,11977])]:
    t=np.loadtxt(T+f'xover_bands_{name}.csv',delimiter=',',skiprows=2)
    sets.append((t[:,0],t[:,1:],fcs))

def trees(lo,hi):
    # bands lo..hi inclusive; returns list of nested tuples (k, left, right) splitting at crossover k (between band k-1 and k)
    if lo==hi: return [lo]
    out=[]
    for k in range(lo+1,hi+1):
        for L in trees(lo,k-1):
            for R in trees(k,hi):
                out.append((k,L,R))
    return out

def P(p,f,fc):
    # analog magnitude^2, x evaluated with bilinear prewarp
    x=np.tan(np.pi*f/fs)/np.tan(np.pi*fc/fs)
    x2=x*x
    return 1/(1+np.abs(p[0])*x2+np.abs(p[1])*x2**2+np.abs(p[2])*x2**3+np.abs(p[3])*x2**4)

def bands(tree,p,f,fcs):
    out=[None]*5
    def rec(node,S):
        if isinstance(node,int): out[node]=S; return
        k,L,R=node
        Pk=P(p,f,fcs[k-1])
        rec(L,S*Pk); rec(R,S*(1-Pk))
    rec(tree,np.ones_like(f))
    return np.array(out).T

def resid(p,tree):
    r=[]
    for f,ref,fcs in sets:
        b=20*np.log10(np.maximum(bands(tree,p,f,fcs),1e-7))
        w=np.where(ref>-20,1/0.5,np.where(ref>-40,1/1.5,np.where(ref>-60,1/4,0.0)))
        r.append(((b-ref)*w).ravel())
    return np.concatenate(r)

p0=np.array([6.02088791e-01,7.43028091e-01,1.80051452e-02,3.68830483e-04])
res=[]
for tr in trees(0,4):
    s=least_squares(resid,p0,args=(tr,))
    r=resid(s.x,tr)
    res.append((np.max(np.abs(r)),np.sqrt(np.mean(r**2)),tr,np.abs(s.x)))
res.sort(key=lambda a:a[1])
for a in res: print('%.2f %.3f'%(a[0],a[1]),a[2],a[3])
