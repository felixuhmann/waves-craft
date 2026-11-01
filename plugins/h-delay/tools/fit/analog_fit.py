import numpy as np, sys
from scipy.optimize import least_squares
FS=48000.0
RF=[20,50,100,200,500,1000,2000,5000,10000,20000]
RESP={0:[0]*10,1:[-3.4,-3.0,-1.7,-0.8,-0.4,-0.3,-0.3,-0.3,-0.4,-0.5],
      2:[-0.7,-0.7,-0.7,-0.7,-0.8,-1.0,-1.4,-1.5,-1.7,-1.7],
      3:[-2.1,-6.1,-13.3,-7.5,-2.3,-1.5,-1.0,-0.6,-0.5,-0.5],
      4:[-0.4,-0.3,-0.4,-0.5,-0.5,-0.5,0.0,-0.2,-0.2,0.0]}
DIST={0:[(-0.8,-54,-43),(-2.1,-66,-34),(-5.3,-81,-25)],
      1:[(-2.2,-34,-35),(-2.9,-29,-30),(-5.5,-28,-24)],
      2:[(-1.4,-51,-45),(-2.5,-52,-35),(-5.5,-50,-26)],
      3:[(-3.6,-19,-20),(-2.9,-23,-34),(-5.5,-31,-28)],
      4:[(-0.8,-54,-41),(-2.1,-66,-33),(-5.3,-81,-25)]}
AMPS=[0.1,0.3,0.95]

def rbj(kind,f,Q,gdb,fs=FS):
    w0=2*np.pi*f/fs; A=10**(gdb/40); al=np.sin(w0)/(2*Q); c=np.cos(w0)
    if kind=='ls':
        sA=2*np.sqrt(A)*al
        b=[A*((A+1)-(A-1)*c+sA),2*A*((A-1)-(A+1)*c),A*((A+1)-(A-1)*c-sA)]
        a=[(A+1)+(A-1)*c+sA,-2*((A-1)+(A+1)*c),(A+1)+(A-1)*c-sA]
    elif kind=='hs':
        sA=2*np.sqrt(A)*al
        b=[A*((A+1)+(A-1)*c+sA),-2*A*((A-1)+(A+1)*c),A*((A+1)+(A-1)*c-sA)]
        a=[(A+1)-(A-1)*c+sA,2*((A-1)-(A+1)*c),(A+1)-(A-1)*c-sA]
    elif kind=='bp':  # constant 0 dB peak gain
        b=[al,0,-al]; a=[1+al,-2*c,1-al]
    elif kind=='pk':
        b=[1+al*A,-2*c,1-al*A]; a=[1+al/A,-2*c,1-al/A]
    b=np.array(b)/a[0]; a=np.array(a)/a[0]
    return b,a

def filt(ba,x):
    b,a=ba; y=np.empty_like(x); z1=z2=0.0
    for i in range(len(x)):
        o=b[0]*x[i]+z1; z1=b[1]*x[i]-a[1]*o+z2; z2=b[2]*x[i]-a[2]*o; y[i]=o
    return y
def filtv(ba,x):
    from scipy.signal import lfilter
    return lfilter(ba[0],ba[1],x)

sat=lambda v: v/(1+np.abs(v))

def post(u,c3,kc):
    from scipy.signal import lfilter
    u=u+c3*u*np.abs(u)
    pp=np.exp(-1/(0.02*FS))
    env=lfilter([1-pp],[1,-pp],np.abs(u))
    u=u/(1+kc*env)
    c=np.exp(-2*np.pi*5.0/FS)
    return lfilter([(1+c)/2,-(1+c)/2],[1,-c],u)

def model(mode,p,x):
    if mode==1:
        g,lsf,lsg,c2,dl,hsg,c3,kc=p
        u=filtv(rbj('ls',lsf,0.6,lsg),x)*10**(g/20)
        u=filtv(rbj('hs',8000,0.5,hsg),u)
        u=u+c2*u*u/np.sqrt(u*u+dl*dl)
        return post(u,c3,kc)
    if mode==2:
        g,hsf,hsg,eps,c3,kc=p
        u=filtv(rbj('hs',hsf,0.6,hsg),x)*10**(g/20)
        u=u+eps*u*u/np.sqrt(u*u+1e-6)
        return post(u,c3,kc)
    if mode==3:
        g,bpf,bpq,kb,tp,tn,hsg,c3,kc,fpk,gpk,c2,dl=p
        u=x*10**(g/20)
        u=filtv(rbj('hs',3000,0.5,hsg),u)
        u=filtv(rbj('pk',fpk,0.7,gpk),u)
        b=filtv(rbj('bp',bpf,bpq,0),u)*kb
        t=np.where(b>0,tp,tn)
        dz=b-t*np.tanh(b/t)
        u=u-b+dz
        u=u+c2*u*u/np.sqrt(u*u+dl*dl)
        return post(u,c3,kc)
    if mode==4:
        g,c3,kc=p
        u=x*10**(g/20)
        return post(u,c3,kc)
    return x

def analyse(y,f,n0):
    seg=y[n0:]; N=len(seg); t=np.arange(N)
    out=[]
    for h in (1,2,3):
        e=np.exp(-2j*np.pi*f*h*t/FS)
        out.append(2*abs(np.dot(seg,e))/N)
    return out

def tone(f,a,cyc=40):
    n=int(round(FS/f*cyc)); per=FS/f
    tot=int(n+FS*0.25)
    t=np.arange(tot)
    return a*np.sin(2*np.pi*f*t/FS), tot-n

def evaluate(mode,p):
    r=[]
    # response at -30 dBFS rel Off
    for f,tg in zip(RF,RESP[mode]):
        cyc=max(4,int(f*0.05))
        x,n0=tone(f,10**(-30/20),cyc)
        # make n0 an integer number of periods
        y=sat(model(mode,p,x)); y0=sat(x)
        a=analyse(y,f,n0)[0]; a0=analyse(y0,f,n0)[0]
        r.append(20*np.log10(a/a0)-tg)
    for (a,(fd,h2,h3)) in zip(AMPS,DIST[mode]):
        x,n0=tone(100,a,8)
        y=sat(model(mode,p,x))
        F,H2,H3=analyse(y,100,n0)
        r.append(20*np.log10(F/a)-fd)
        for hv,ht in ((H2,h2),(H3,h3)):
            d=20*np.log10(hv/F+1e-12)-ht
            r.append(max(0.0,d) if ht<=-53 else d)
    if mode==3:
        x,n0=tone(100,0.03,8)
        y=sat(model(mode,p,x))
        F,H2,H3=analyse(y,100,n0)
        r.append(20*np.log10(F/0.03)+10.0)
        r.append(20*np.log10(H2/F)+13.0)
    return np.array(r)

TOL=np.array([1.0]*10+[0.5,4,4]*3)
mode=int(sys.argv[1])
inits={1:[-0.3,100,-3.3,0.05,0.1,-0.2,0.5,0.5],2:[-0.6,1500,-1.0,0.006,0.5,0.5],3:[2.0,105.25,0.2012,0.9484,0.02784,0.09409,-2.6046,0.41813,0.78594,1334.6,-2.62,0.02,0.1],4:[-0.3,0.5,0.5]}
bounds={1:([-3,30,-12,0,0.001,-3,0,0],[1,1000,0,0.5,1,1,1,3]),2:([-3,200,-6,0,0,0],[1,8000,0,0.1,1,3]),
        3:([-3,40,0.1,0.1,0.001,0.001,-4,0,0,300,-5,-0.3,0.001],[3,300,4,1.0,0.5,0.5,1,1,3,4000,0,0.3,1]),4:([-2,0,0],[1,1,3])}
if mode==3: TOL=np.concatenate([TOL,[3.0,4.0]])
def res(p):
    e=evaluate(mode,p)/TOL
    return e
if mode==0:
    print(np.round(evaluate(0,[]),2)); sys.exit()
best=None
rng=np.random.default_rng(1)
for trial in range(int(sys.argv[2]) if len(sys.argv)>2 else 6):
    p0=np.array(inits[mode],float)
    if trial>0:
        lo,hi=map(np.array,bounds[mode]); p0=lo+(hi-lo)*rng.uniform(0,1,len(p0))
        if mode==3: p0=np.clip(np.array(inits[3])*rng.uniform(0.8,1.2,len(p0)),lo+1e-6,hi-1e-6)
    try:
        sol=least_squares(res,p0,bounds=bounds[mode],max_nfev=int(sys.argv[3]) if len(sys.argv)>3 else 300,diff_step=1e-3)
    except Exception as ex:
        print(ex); continue
    c=np.sum(sol.fun**2)
    if best is None or c<best[0]:
        best=(c,sol.x); print(trial,c,list(np.round(sol.x,5)),flush=True)
c,p=best
e=evaluate(mode,p)
print("params",repr(list(p)))
print("resp err",np.round(e[:10],2))
print("dist err",np.round(e[10:],2))
print("worst/tol",np.round(np.max(np.abs(e)/TOL),2))
print("rel",np.round(e/TOL,2))
