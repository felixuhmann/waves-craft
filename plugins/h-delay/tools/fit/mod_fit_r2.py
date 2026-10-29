# Spec r2 section 8: modulation smoother = one-pole with tau = k * D (+ t0), then a 2nd-order low-pass (fc, Q).
# Grid search against the 12-row table (half peak-to-peak excursion / nominal A).
import numpy as np, itertools
FSIM=4000.0
cases=[(0.5,10,10,1.02),(2,10,10,1.02),(5,10,10,0.90),(10,10,10,0.72),(20,10,10,0.50),(50,10,10,0.03),
       (0.5,50,50,0.99),(0.5,100,10,0.95),(0.5,200,10,0.73),(0.5,500,10,0.43),(0.5,1000,10,0.25),(0.5,3500,10,0.07)]
ks=np.array([0.8,0.9,1.0,1.1,1.2,1.3])
fcs=np.array([14,16,18,20,22,25])
Qs=np.array([0.7,0.85,1.0,1.2,1.4])
G=np.array(list(itertools.product(ks,fcs,Qs)),float)
k,fc,Q=G.T
w0=2*np.pi*fc/FSIM; al=np.sin(w0)/(2*Q); c=np.cos(w0)
b0=(1-c)/2; b1=1-c; b2=b0; a0=1+al; a1=-2*c; a2=1-al
b0,b1,b2,a1,a2=b0/a0,b1/a0,b2/a0,a1/a0,a2/a0
def tri(ph): return 4*np.abs(ph-np.floor(ph+0.5))-1
def run(rate,D,depth):
    A=D*depth/100/2
    tau=k*D/1000
    p=np.exp(-1/(tau*FSIM))
    T=max(4.0,3/rate)+6*tau.max()
    n=int(T*FSIM)
    z1=np.zeros(len(G)); z2=np.zeros(len(G)); y=np.zeros(len(G))
    mx=np.full(len(G),-1e9); mn=np.full(len(G),1e9)
    start=n-int(FSIM/rate)-2
    for i in range(n):
        x=A*tri(0.25+rate*i/FSIM)
        y=x+p*(y-x)
        o=b0*y+z1; z1=b1*y-a1*o+z2; z2=b2*y-a2*o
        if i>=start: mx=np.maximum(mx,o); mn=np.minimum(mn,o)
    return (mx-mn)/2/A
R=np.array([run(r,D,d) for r,D,d,_ in cases])
T=np.array([c[3] for c in cases])[:,None]
worst=np.abs(R/T-1).max(0)
for kk in np.argsort(worst)[:8]:
    print(G[kk],round(worst[kk],3),np.round(R[:,kk],3))
