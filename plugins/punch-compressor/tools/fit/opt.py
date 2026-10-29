"""Nelder-Mead fit of the fast-stage / charge constants (section 6 + 7.4) using the C++ tune harness."""
import subprocess, math, sys, numpy as np
from scipy.optimize import minimize
import os
D=os.environ.get('TUNE_DIR','.')  # directory holding the compiled tune harness
def evalp(f0,pf,kf,c0,pc):
    out=subprocess.run([D+'/tune',*[str(v) for v in (f0,pf,kf,c0,pc)]],capture_output=True,text=True).stdout
    T={};N=L=None
    for line in out.splitlines():
        p=line.split()
        if p[0]=='T': T[p[1]]=(np.array([float(v) for v in p[2:13]]),float(p[14]))
        elif p[0]=='N': N=np.array([float(v) for v in p[1:]])
        elif p[0]=='L': L=np.array([float(v) for v in p[1:]])
    return T,N,L
def cost(x,verbose=False):
    f0,pf,kf,c0,pc=math.exp(x[0]),x[1],math.exp(x[2]),math.exp(x[3]),x[4]
    T,N,L=evalp(f0,pf,kf,c0,pc)
    c=np.sum((N/0.35)**2)+np.sum((L/2.5)**2)
    for k,(e,r) in T.items():
        c+=(r/0.4)**2+np.sum(np.maximum(np.abs(e[3:])-1.0,0)**2)*20
    if verbose:
        print('f0 %.3f pf %.3f kf %.3f c0 %.3f pc %.3f cost %.2f'%(f0,pf,kf,c0,pc,c))
        print(' N',' '.join('%+.2f'%v for v in N)); print(' L',' '.join('%+.1f'%v for v in L))
        for k,(e,r) in T.items(): print(' ',k,'max %.2f rms %.2f'%(np.abs(e).max(),r))
    sys.stdout.flush()
    return c
x0=np.array([float(v) for v in sys.argv[1:6]]) if len(sys.argv)>5 else np.array([math.log(5),0.3,math.log(0.3),math.log(50),0.0])
print('start'); cost(x0,True)
r=minimize(cost,x0,method='Nelder-Mead',options=dict(maxfev=int(sys.argv[6]) if len(sys.argv)>6 else 80,xatol=0.01,fatol=0.05,initial_simplex=None))
print('result',r.x.tolist()); cost(r.x,True)
