"""Model of the analog-mode chain (mirrors source/dsp/Analog.h) and the section 10 measurement emulation."""
import numpy as np, math, sys, json
from scipy.signal import lfilter
from scipy.optimize import least_squares
FS=48000
MULTI=False
def rbj(kind,f,fs,q=0.7071,db=0.0):
    A=10**(db/40); w=2*math.pi*f/fs; c=math.cos(w); s=math.sin(w); al=s/(2*q)
    if kind=='pk':
        b=[1+al*A,-2*c,1-al*A]; a=[1+al/A,-2*c,1-al/A]
    elif kind=='ls':
        sq=2*math.sqrt(A)*al
        b=[A*((A+1)-(A-1)*c+sq),2*A*((A-1)-(A+1)*c),A*((A+1)-(A-1)*c-sq)]
        a=[(A+1)+(A-1)*c+sq,-2*((A-1)+(A+1)*c),(A+1)+(A-1)*c-sq]
    elif kind=='hs':
        sq=2*math.sqrt(A)*al
        b=[A*((A+1)+(A-1)*c+sq),-2*A*((A-1)+(A+1)*c),A*((A+1)+(A-1)*c-sq)]
        a=[(A+1)-(A-1)*c+sq,2*((A-1)-(A+1)*c),(A+1)-(A-1)*c-sq]
    elif kind=='bp':
        b=[al,0,-al]; a=[1+al,-2*c,1-al]
    elif kind=='lp1':
        k=math.tan(math.pi*f/fs); b=[k/(1+k),k/(1+k)]; a=[1,(k-1)/(k+1)]
        return np.array(b),np.array(a)
    elif kind=='hp1':
        k=math.tan(math.pi*f/fs); b=[1/(1+k),-1/(1+k)]; a=[1,(k-1)/(k+1)]
        return np.array(b),np.array(a)
    b=np.array(b)/a[0]; a=np.array(a)/a[0]
    return b,a
DEF=dict(g=0.0,lsf=100.0,lsdb=0.0,lsq=0.7071,pkf=1000.0,pkq=0.7,pkdb=0.0,hsf=5000.0,hsdb=0.0,hsq=0.7071,lpf=0.0,
         b2=0.0,b3=0.0,xo=0.0,xk=0.001,
         bf=2000.0,bq=0.5,ap=0.0,tp=0.01,an=0.0,tn=0.01,g3=0.0,g2=0.0,dc=2.0)
def proc(x,P,fs=FS):
    p=dict(DEF); p.update(P)
    u=x*10**(p['g']/20)
    if p['lsdb']!=0: u=lfilter(*rbj('ls',p['lsf'],fs,p['lsq'],p['lsdb']),u)
    if p['pkdb']!=0: u=lfilter(*rbj('pk',p['pkf'],fs,p['pkq'],p['pkdb']),u)
    if p['hsdb']!=0: u=lfilter(*rbj('hs',p['hsf'],fs,p['hsq'],p['hsdb']),u)
    if p['lpf']>0: u=lfilter(*rbj('lp1',p['lpf'],fs),u)
    y=u
    if p['ap']!=0 or p['an']!=0 or p['g3']!=0 or p['g2']!=0:
        b=lfilter(*rbj('bp',p['bf'],fs,p['bq']),u)
        tp=abs(p['tp'])+1e-9; tn=abs(p['tn'])+1e-9
        phi=np.where(b>=0, b-p['ap']*tp*np.tanh(b/tp), b-p['an']*tn*np.tanh(b/tn))
        phi=phi+p['g3']*b*np.abs(b)+p['g2']*b*b
        y=u-b+phi
    y=y+p['b2']*y*y+p['b3']*y*np.abs(y)
    if p['xo']!=0: y=y-p['xo']*np.tanh(y/(abs(p['xk'])+1e-9))
    if p['dc']>0: y=lfilter(*rbj('hp1',p['dc'],fs),y)
    return y
FR=[20,50,100,200,500,1000,2000,5000,10000,20000]
MAG={1:[-0.11,-0.17,-0.21,-0.24,-0.26,-0.26,-0.27,-0.27,-0.28,-0.32],
     2:[0.01,0.01,-0.05,-0.26,-0.63,-0.76,-0.78,-0.68,-0.42,-0.08],
     3:[0.02,0.17,0.30,-0.28,-2.78,-4.27,-4.65,-3.62,-1.79,-0.37],
     4:[-0.36,-0.34,-0.29,-0.34,-0.34,-0.31,-0.18,-0.08,-0.07,-0.22]}
TONES=[(100,0.03),(100,0.1),(100,0.3),(100,0.95),(1000,0.03),(1000,0.3),(1000,0.95)]
TT={1:[(-0.21,-82.9,-87.7),(-0.21,-74.2,-77.6),(-0.2,-65.2,-68.8),(-0.17,-56.6,-60.8),(-0.26,-81.2,-84.6),(-0.24,-62.3,-65.9),(-0.2,-54.0,-58.3)],
    2:[(-0.05,-97.8,-94.1),(-0.05,-94.5,-83.4),(-0.05,-89.3,-74.2),(-0.05,-79.1,-64.7),(-0.76,-92.0,-75.5),(-0.7,-70.8,-56.4),(-0.57,-61.8,-47.8)],
    3:[(0.31,-61.3,-62.3),(0.28,-51.4,-52.9),(0.22,-43.6,-46.0),(0.13,-37.8,-42.4),(-4.3,-33.1,-34.7),(-1.64,-23.0,-28.4),(-0.49,-21.9,-32.2)],
    4:[(-0.33,-109.3,-36.8),(-0.1,-125.2,-47.2),(-0.03,-132.8,-56.7),(-0.01,-141.0,-66.7),(-0.31,-116.7,-36.6),(-0.03,-147.0,-56.6),(-0.0,-147.9,-66.6)]}
CLK={1:(0.2906,0.8743),2:(0.2882,0.8679),3:(0.2547,0.7982),4:(0.299,0.899)}
def tone(P,f,a,fs=FS):
    per=fs/f
    n=int(round(fs*0.3)); x=a*np.sin(2*np.pi*f*np.arange(n)/fs)
    y=proc(x,P,fs)
    m=int(round(per*round(0.1*f)))
    seg=y[-m:]; k=np.arange(m)
    def h(j):
        e=np.exp(-2j*np.pi*j*f*k/fs); return abs(np.dot(seg,e))*2/m
    h1=h(1)
    return 20*np.log10(h1/a), 20*np.log10(h(2)/h1+1e-30), 20*np.log10(h(3)/h1+1e-30)
def click(P,a,fs=FS):
    x=np.zeros(4096); x[2048]=a
    return np.abs(proc(x,P,fs)).max()
def evaluate(mode,P,fs=FS):
    out={}
    out['mag']=[tone(P,f,10**(-30/20),fs)[0] for f in FR]
    out['tones']=[tone(P,f,a,fs) for f,a in TONES]
    out['clk']=[click(P,0.3,fs),click(P,0.9,fs)]
    if mode==3:
        out['lvl']=[tone(P,1000,10**(-62/20),fs)[0],tone(P,1000,10**(-38/20),fs)[0]]
    return out
def resid(mode,P,w=None,fs=FS):
    e=evaluate(mode,P,fs); r=[]
    r+=list((np.array(e['mag'])-MAG[mode])/0.1)
    for (g,h2,h3),(rg,rh2,rh3),(f,a) in zip(e['tones'],TT[mode],TONES):
        r.append((g-rg)/0.1)
        floor = (mode==2 and (rh2<-90 or rh3<-90)) or (mode==4)
        wh2 = 0.0 if (mode==2 and rh2<-90) or (mode==4 and rh2<-100) else 1/1.5
        wh3 = 0.0 if (mode==2 and rh3<-90) else 1/1.5
        e2=(max(min(h2,-10),-160)-rh2) if wh2 else 0.0
        e3=(max(min(h3,-10),-160)-rh3) if wh3 else 0.0
        r.append(e2/2.0); r.append(e3/2.0)
        r.append(max(abs(e2)-3.0,0)*3); r.append(max(abs(e3)-3.0,0)*3)
    r+=[(e['clk'][0]-CLK[mode][0])/0.003,(e['clk'][1]-CLK[mode][1])/0.01]
    r+=[max(0,e['clk'][0]-0.2995)*1000,max(0,e['clk'][1]-0.8985)*1000]
    if mode==3: r+=[(e['lvl'][0]+5.1)/0.1,(e['lvl'][1]+4.75)/0.1]
    return np.array(r)
def report(mode,P):
    e=evaluate(mode,P)
    print('mag ',' '.join('%+.2f'%(v-r) for v,r in zip(e['mag'],MAG[mode])))
    for (g,h2,h3),(rg,rh2,rh3) in zip(e['tones'],TT[mode]):
        print('  %+.2f/%+.1f/%+.1f   (%.2f %.1f %.1f)'%(g-rg,h2-rh2,h3-rh3,g,h2,h3))
    print('clk',e['clk'],CLK[mode])
    if 'lvl' in e: print('lvl',e['lvl'])
def fit(mode,names,P0,lo,hi,nfev=300):
    def f(v):
        P=dict(P0); P.update(dict(zip(names,v)))
        if MULTI: return np.concatenate([resid(mode,P,fs=q) for q in (44100,48000,96000)])
        return resid(mode,P)
    x0=[P0[n] for n in names]
    r=least_squares(f,x0,bounds=(lo,hi),max_nfev=nfev,x_scale='jac')
    P=dict(P0); P.update(dict(zip(names,r.x)))
    return P,r.cost
