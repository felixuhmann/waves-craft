# Crossover fit (see IMPLEMENTATION_NOTES.md, "Crossover").
# Topology: split at crossover 2 first; low side split at crossover 1; high side split at crossover 3, its high side
# split at crossover 4. Each split k is a zero-phase pair P_k, 1-P_k with P_k = |H_k(j x)|^2, x = f/fc_k (bilinear,
# prewarped), H_k = N_k/D_k min-phase, realised as N_k(s)N_k(-s)/D_k(s)^2 and A_k - that, A_k = D_k(-s)/D_k(s).
# All bands carry the common phase prod A_k (allpass compensation), so the bands sum to an allpass.
# Fits the normalised prototypes (one per split) to both band tables and the direct-path group delay table.
import numpy as np, os, sys, json
from scipy.optimize import least_squares
T=os.path.join(os.path.dirname(__file__),'../../../../spec/enhancer/r1/tables/')
fs=48000.
sets=[]
for name,fcs in [('default',[88,494,2576,9285]),('set2',[197,990,3978,11977])]:
    t=np.loadtxt(T+f'xover_bands_{name}.csv',delimiter=',',skiprows=2)
    sets.append((t[:,0],t[:,1:],fcs))
gdt=np.loadtxt(T+'direct_group_delay.csv',delimiter=',',skiprows=2)
tr=(2,(1,0,1),(3,2,(4,3,4)))
ND=int(os.environ.get("ND","4"))
NN=int(os.environ.get("NN","1"))
WARP=os.environ.get("WARP","1")=="1"

def unpack(p):
    p=np.abs(np.asarray(p)).reshape(4,ND+NN)
    return p[:,:ND],p[:,ND:]

def Pmag(p,k,x):
    den,num=unpack(p); x2=x*x
    d=1+sum(den[k,i]*x2**(i+1) for i in range(ND))
    n=1+sum(num[k,i]*x2**(i+1) for i in range(NN))
    return n/d

def bands(p,f,fcs):
    out=[None]*5
    def rec(node,S):
        if isinstance(node,int): out[node]=S; return
        k,L,R=node
        x=(np.tan(np.pi*f/fs)/np.tan(np.pi*fcs[k-1]/fs)) if WARP else f/fcs[k-1]
        Pk=Pmag(p,k-1,x)
        rec(L,S*Pk); rec(R,S*(1-Pk))
    rec(tr,np.ones_like(f)); return np.array(out).T

def spectral_factor(coefs):
    # coefs c_i of 1 + sum c_i x^(2i): return min-phase poly in s (descending), value 1 at s=0
    n=len(coefs)
    poly=np.zeros(2*n+1)
    # P(-s^2): x^2 -> -s^2 ; term c_i x^(2i) -> c_i (-1)^i s^(2i)
    asc=np.zeros(2*n+1); asc[0]=1
    for i,c in enumerate(coefs): asc[2*(i+1)]=c*(-1)**(i+1)
    r=np.roots(asc[::-1])
    lhp=r[r.real<0]
    if len(lhp)!=n:  # numerical: take the n roots with smallest real part
        lhp=r[np.argsort(r.real)][:n]
    q=np.real(np.poly(lhp)); return q/q[-1]

def group_delay(p,f,fcs):
    den,num=unpack(p)
    tot=np.zeros_like(f)
    w=2*np.pi*f/fs
    for k in range(4):
        D=spectral_factor(den[k])   # analog, in x
        wd=2*fs*np.tan(np.pi*fcs[k]/fs)
        # digital allpass from bilinear of D(-s)/D(s): GD = 2 * GD of 1/D_dig
        a=D/(wd**np.arange(len(D)-1,-1,-1))
        from scipy import signal
        bz,az=signal.bilinear([1.0],a,fs)
        _,g=signal.group_delay((bz,az),w=w)
        tot+=2*g-  (len(D)-1)*0  # bilinear zeros at Nyquist add a constant? handled below
    return tot

def gd_allpass(p,f,fcs):
    from scipy import signal
    den,_=unpack(p)
    w=2*np.pi*f/fs; tot=np.zeros_like(f)
    for k in range(4):
        D=spectral_factor(den[k]); wd=2*fs*np.tan(np.pi*fcs[k]/fs)
        a=D/(wd**np.arange(len(D)-1,-1,-1))
        b=a*((-1.0)**np.arange(len(D)-1,-1,-1))  # D(-s)
        bz,az=signal.bilinear(b,a,fs)
        _,g=signal.group_delay((bz,az),w=w)
        tot+=g
    return tot

def resid(p,wgd=1.0):
    r=[]
    for f,ref,fcs in sets:
        b=20*np.log10(np.maximum(bands(p,f,fcs),1e-7))
        w=np.where(ref>-20,1/0.5,np.where(ref>-40,1/1.5,np.where(ref>-60,1/4,0.0)))
        r.append(((b-ref)*w).ravel())
    if wgd>0:
        g=gd_allpass(p,gdt[:,0],sets[0][2])
        tol=np.where(gdt[:,0]>5000,np.maximum(1.0,0.1*gdt[:,1]),0.1*gdt[:,1])
        r.append(wgd*(g-gdt[:,1])/tol)
    return np.concatenate(r)

if __name__=='__main__':
    P4=np.tile([0.6,0.6,1e-3,1e-5],(4,1))
    p0=np.concatenate([P4[:,:ND],np.full((4,NN),1e-3)],1).ravel()
    wgd=float(sys.argv[1]) if len(sys.argv)>1 else 1.0
    g0=gd_allpass(p0,gdt[:,0],sets[0][2])
    print('initial gd ratio',np.round(g0[::6]/gdt[::6,1],3))
    s=least_squares(lambda p:resid(p,wgd),p0,x_scale='jac',max_nfev=4000)
    r=resid(s.x,0); print('bands max %.2f rms %.3f'%(np.max(np.abs(r)),np.sqrt(np.mean(r**2))))
    g=gd_allpass(s.x,gdt[:,0],sets[0][2])
    print('gd ratio',np.round(g/gdt[:,1],3))
    den,num=unpack(s.x)
    print('den',den.tolist()); print('num',num.tolist())
    json.dump({'den':den.tolist(),'num':num.tolist()},open(os.path.join(os.path.dirname(os.path.abspath(__file__)),'xover_params.json'),'w'))
    for (f,ref,fcs) in sets:
        b=20*np.log10(np.maximum(bands(s.x,f,fcs),1e-7))
        e=b-ref
        tol=np.where(ref>-20,0.5,np.where(ref>-40,1.5,np.where(ref>-60,4,99)))
        bad=(np.abs(e)>tol)|((ref<=-60)&(b>-55))
        for i,k in zip(*np.nonzero(bad)): print('FAIL f=%.1f band %d ref %.2f got %.2f'%(f[i],k,ref[i,k],b[i,k]))
