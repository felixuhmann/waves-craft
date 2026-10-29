"""Python reference of the static curve / makeup (sections 4-5) used by the fitting scripts (first prototype)."""
import numpy as np, math
import numpy.lib.stride_tricks as st
fs=48000
dgrid=[-2,-1,0,1,2,3,6,9,12,18,24]
D24_4=[0,0,0.01,-0.26,-0.35,-0.41,-0.48,-0.52,-0.54,-0.56,-0.57]
def mk(T,R,Rel):
    u=1-1/R
    ku=[0,.2,1/3,.5,2/3,.75,.875,.95,.98]; kv=[0,.0108,.1056,.2166,.3191,.3670,.4351,.4736,.4886]
    k=np.interp(u,ku,kv)
    lr=[math.log10(x) for x in [3,10,30,100,300,1000,3000]]; sv=[.2476,.6596,.8730,.9628,.9902,1.0,1.0029]
    s=np.interp(math.log10(Rel),lr,sv)
    return k*max(0,-T-9)*s
def grv(D,T,R):
    d=D-T
    dl=np.interp(np.minimum(d,24),dgrid,D24_4)
    dl[d<-2]=0
    g=np.maximum(d,0)*(1-1/R)+dl
    return np.maximum(g,0)
def run(x, T=-24,R=4,A=7,Rel=100,P=0,H=64,law='zk',k=0.7,k2=None,retg=False):
    n=len(x); ax=np.abs(x)
    pad=np.concatenate([ax,np.zeros(H)])
    w=st.sliding_window_view(pad,H+1)[:n]; p=w.max(axis=1)
    D=20*np.log10(np.maximum(p,1e-30))
    Pd=int(round(P*fs/1000))
    if Pd>0: D=np.concatenate([np.full(Pd,D[0]),D[:-Pd]])
    M=mk(T,R,Rel)
    ca=math.exp(-1/(A*fs/1000)); cr=math.exp(-1/(k*Rel*fs/1000))
    g=1.0; out=np.empty(n)
    gam,aa=relpar(Rel); KK=2.0; crz=math.exp(-1/(aa*Rel*fs/1000))
    tgt=10**(-grv(D,T,R)/20)
    if law=='expdb':
        tg=np.log(tgt)
        lg=0.0
        lca=None
        for i in range(n):
            t=tgt[i]
            if t<g:
                g=t+(g-t)*ca; lg=math.log(g)
            else:
                lg=tg[i]+(lg-tg[i])*cr; g=math.exp(lg)
            out[i]=g
    else:
        for i in range(n):
            t=tgt[i]
            if t<g: g=t+(g-t)*ca
            elif law=='lin':
                g=t+(g-t)*cr
            elif law=='zk':
                zt=(t*KK)**gam; z=g**gam; z=zt+(z-zt)*crz; g=min(z**(1/gam),t)
            elif law=='ramp':
                g=min(t,g+1/(k*Rel*fs/1000))
            out[i]=g
    if retg: return out
    return x*out*10**(M/20)
def traj(**kw):
    t=np.arange(int(2.6*fs))/fs
    amp=np.where((t>=0.6)&(t<1.2),10**(-6/20),10**(-40/20))
    x=amp*np.sin(2*np.pi*1000*t)
    y=run(x,**kw)
    def g(s,ms):
        a=s+int(ms*fs/1000); b=a+48
        return 20*np.log10(np.sqrt(np.mean(y[a:b]**2)/np.mean(x[a:b]**2)))
    up=int(0.6*fs); dn=int(1.2*fs)
    att=[g(up,m) for m in [0,1,2,3,5,8,12,20,30,50,80,150]]
    rel=[g(dn,m) for m in [0,2,5,10,20,50,100,200,400,800,1300]]
    return att,rel

RA=[3,30,100,250,300,500,1000,3000]
RG=[.787,1.009,.817,.346,.452,.432,.573,1.113]
RAa=[4.429,4.140,3.117,1.837,2.007,1.963,2.372,4.513]
def relpar(Rel):
    l=math.log10(Rel); L=[math.log10(v) for v in RA]
    return float(np.interp(l,L,RG)), float(np.interp(l,L,RAa))
