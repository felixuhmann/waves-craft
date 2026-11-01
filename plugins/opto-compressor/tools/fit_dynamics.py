import csv,ctypes,numpy as np,sys,json,collections,os
from scipy.optimize import least_squares
H=os.path.dirname(os.path.abspath(__file__))
T=sys.argv[1]
lib=ctypes.CDLL(H+'/sim.so')
st=collections.defaultdict(dict)
for r in csv.DictReader(open(T+'/static_gr.csv')): st[r['mode']+r['pr']][float(r['level_dBFS_sine_peak'])]=float(r['gain_dB'])
def G(s,l): return -st[s.replace('_','')][float(l)]
at=collections.OrderedDict();re=collections.OrderedDict()
for r in csv.DictReader(open(T+'/attack.csv')): at.setdefault((r['setting'],float(r['burst_dBFS']),float(r['burst_ms'])),[]).append((float(r['t_ms']),-float(r['gr_dB'])))
for r in csv.DictReader(open(T+'/release.csv')): re.setdefault((r['setting'],float(r['burst_dBFS']),float(r['burst_ms'])),[]).append((float(r['t_after_ms']),-float(r['gr_dB'])))
rt={(r['setting'],float(r['burst_dBFS']),float(r['burst_ms'])):(float(r['t63_s']),float(r['t90_s'])) for r in csv.DictReader(open(T+'/release_times.csv'))}
cases=list(dict.fromkeys(list(at)+list(re)))
dt=.1
def sim(pv,c):
    s,l,b=c; tot=b+20000 ; n=int(tot/dt); g=np.empty(n)
    lib.simulate(pv.ctypes.data_as(ctypes.c_void_p),ctypes.c_double(G(s,l)),ctypes.c_double(b),ctypes.c_double(tot),ctypes.c_double(dt),g.ctypes.data_as(ctypes.c_void_p),None,None,ctypes.c_int(n))
    return g
def win(g,c,t):
    b=c[2];i0=max(0,int((t-2)/dt));i1=int((t+2)/dt);idx=np.arange(i0,i1);tt=idx*dt
    w=np.where(tt<b,10**((c[1]+40)/10),1.0)
    pre=max(0,int((2-t)/dt))
    num=(w*10**(-g[idx]/10)).sum()+pre;den=w.sum()+pre
    return -10*np.log10(num/den)
names=['t%d'%i for i in range(8)]+['tau0','F1','p1','e0','p2','kmax','Smax','F0','a','b','m','taus']
x0=np.array([190,150,120,110,85,60,50,45, 6,15,1.5,1.5,1.5,2.0,2.2,6,.002,1.0,3,3900.])
if len(sys.argv)>3: x0=np.array([json.load(open(sys.argv[3]))[n] for n in names])
def resid(lx,full=False):
    p=np.exp(lx);pv=p.astype(np.float64)
    r=[];ex=[]
    for c in cases:
        g=sim(pv,c)
        for t,v in at.get(c,[]):
            m=win(g,c,t);tol=3 if t<=5 else 1.5;r.append((m-v)/tol);ex.append(('A',c,t,v,m))
        b=c[2]
        for t,v in re.get(c,[]):
            m=win(g,c,b+t);tol=3 if t<=5 else 1.5
            r.append((m-v)/tol);ex.append(('R',c,t,v,m))
            if t>=200: r.append((m-v)/max(.1,.2*v)*.25)
        if c in rt:
            ge=win(g,c,b+5);ib=int(b/dt)
            for th,tm,nm in((.368,rt[c][0],'t63'),(.1,rt[c][1],'t90')):
                k=np.argmax(g[ib+50:]<th*ge);tm2=(k+50)*dt/1000
                r.append(np.log(tm2/tm)/np.log(1.25));ex.append((nm,c,0,tm,tm2))
    return (np.array(r),ex) if full else np.array(r)
lb=np.full(len(x0),-np.inf);lb[7]=np.log(25.);x0[7]=max(x0[7],26.);s=least_squares(resid,np.log(x0),bounds=(lb,np.inf),method='trf',max_nfev=int(sys.argv[2]),diff_step=.01)
p=np.exp(s.x);json.dump(dict(zip(names,p)),open(H+'/fit.json','w'),indent=0);print(dict(zip(names,np.round(p,4))))
r,ex=resid(s.x,True);print('n',len(r),'rms',np.sqrt((r**2).mean()),'max',abs(r).max())
for e in ex:
    d=e[4]-e[3]
    bad=(e[0] in'AR' and abs(d)>(3 if e[2]<=5 else 1.5)*.6) or (e[0][0]=='t' and abs(np.log(e[4]/e[3]))>np.log(1.25)*.7)
    if bad: print(e[0],e[1],e[2],round(e[3],3),round(float(e[4]),3))
