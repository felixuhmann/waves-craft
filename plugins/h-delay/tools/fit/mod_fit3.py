import numpy as np, itertools, sys
FSIM=4000.0
cases=[ (0.5,10,0.1,1.03),(2,10,0.1,1.02),(5,10,0.1,0.98),(10,10,0.1,0.70),(20,10,0.1,0.50),(50,10,0.1,0.03),
 (0.5,100,0.1,0.95),(0.5,200,0.1,0.69),(0.5,500,0.1,0.43),(0.5,1000,0.1,0.25),(0.5,3500,0.1,0.07)]
order=sys.argv[1] if len(sys.argv)>1 else "lpfirst"
Ss=np.array([19,20,21,22,23,24,26])
fcs=np.array([14,16,18,20,22,25,28,32,36])
Qs=np.array([0.5,0.6,0.7071,0.8,0.9,1.0,1.2])
tls=np.array([2.0,5.0,10.0])
G=np.array(list(itertools.product(Ss,fcs,Qs,tls)),float)
S,fc,Q,tl=G.T
w0=2*np.pi*fc/FSIM; al=np.sin(w0)/(2*Q); c=np.cos(w0)
b0=(1-c)/2; b1=1-c; b2=b0; a0=1+al; a1=-2*c; a2=1-al
b0,b1,b2,a1,a2=b0/a0,b1/a0,b2/a0,a1/a0,a2/a0
Sd=S/FSIM
lk=1/(tl*FSIM)
def tri(ph): return 4*np.abs(ph-np.floor(ph+0.5))-1

def run(rate,D,depth):
    A=D*depth/2
    T=max(3.0, 3/rate)+12
    n=int(T*FSIM)
    z1=np.zeros(len(G)); z2=np.zeros(len(G)); y=np.zeros(len(G)); xp=np.zeros(len(G))
    mx=np.full(len(G),-1e9); mn=np.full(len(G),1e9)
    start=n-int(FSIM/rate)-2
    for i in range(n):
        x=A*tri(rate*i/FSIM)
        if order=="lpfirst":
            o=b0*x+z1; z1=b1*x-a1*o+z2; z2=b2*x-a2*o
            y=y+Sd*np.tanh((o-xp)/Sd)+(o-y)*lk; xp=o
            out=y
        else:
            y=y+Sd*np.tanh((x-xp)/Sd)+(x-y)*lk; xp=x
            o=b0*y+z1; z1=b1*y-a1*o+z2; z2=b2*y-a2*o
            out=o
        if i>=start:
            mx=np.maximum(mx,out); mn=np.minimum(mn,out)
    return (mx-mn)/2/A

R=np.array([run(r,D,d) for r,D,d,_ in cases])
T=np.array([c[3] for c in cases])[:,None]
err=np.abs(R/T-1)
worst=err.max(0)
o=np.argsort(worst)
for kk in o[:12]:
    print(G[kk], round(worst[kk],3), np.round(R[:,kk],3))
