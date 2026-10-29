"""Closed-form fit of the slow release law (gamma, a, K) to the section 7.4 release rows."""
import numpy as np, math
from scipy.optimize import least_squares
from comp import mk
fs=48000
REL={3:[-10.38,-4.76,-1.41,0.49,1.27,1.36,1.36,1.36,1.36,1.36,1.36],30:[-7.98,-6.92,-5.56,-3.77,-1.24,3.09,4.81,4.81,4.81,4.81,4.81],100:[-7.58,-7.16,-6.66,-5.92,-4.64,-1.84,1.40,5.30,5.30,5.30,5.30],250:[-7.46,-7.21,-6.94,-6.61,-6.00,-4.45,-2.47,0.36,5.44,5.44,5.44],300:[-7.45,-7.21,-6.96,-6.68,-6.17,-4.82,-3.05,-0.46,4.21,5.45,5.45],500:[-7.43,-7.22,-7.01,-6.82,-6.50,-5.61,-4.36,-2.39,0.44,5.48,5.48],1000:[-7.42,-7.35,-7.30,-7.21,-7.04,-6.54,-5.79,-4.50,-2.49,0.37,3.69],3000:[-7.42,-7.41,-7.39,-7.36,-7.30,-7.12,-6.84,-6.31,-5.36,-3.78,-2.21]}
TMS=[0,2,5,10,20,50,100,200,400,800,1300]
G0=10**(-12.94/20)
t=np.arange(int(1.35*fs)+48)/fs*1000
sw=np.sin(2*np.pi*1000*t/1000)**2
def traj(Rel,gam,a,K):
    tau=a*Rel
    e=np.exp(-t/tau)
    if abs(gam)<1e-6:
        z0=math.log(G0); zt=math.log(K)
        z=zt+(z0-zt)*e; g=np.exp(np.minimum(z,0))
    else:
        z0=G0**gam; zt=K**gam
        z=zt+(z0-zt)*e; g=np.minimum(z,1.0)**(1/gam)
    M=mk(-24,4,Rel)
    out=[]
    for m in TMS:
        i=int(m*48); w=sw[i:i+48]
        out.append(10*np.log10(np.sum(w*g[i:i+48]**2)/np.sum(w))+M)
    return np.array(out)
def fit(Rel,x0=(0.5,1.0,1.05)):
    ref=np.array(REL[Rel])
    f=lambda p: traj(Rel,*p)-ref
    r=least_squares(f,x0,bounds=([0.0,0.1,1.0],[1.5,5,3]))
    return r.x, f(r.x)
if __name__=='__main__':
    for Rel in REL:
        p,e=fit(Rel)
        print(Rel,'gam %.3f a %.3f K %.4f'%tuple(p),' '.join('%+.2f'%v for v in e))
