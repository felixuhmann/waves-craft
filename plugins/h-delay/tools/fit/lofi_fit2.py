import numpy as np, sys
from scipy import signal
from scipy.optimize import least_squares
exec(open('lofi_fit.py').read().split("nsec=int")[0])
N=int(sys.argv[1]); rs=float(sys.argv[2]); fsb=float(sys.argv[3])
z,p,k=signal.cheby2(N,rs,2*np.pi*fsb,analog=True,output='zpk')
pp=sorted([q for q in p if q.imag>0],key=lambda q:abs(q))
zz=sorted([q for q in z if q.imag>0],key=lambda q:abs(q))
p0=[]
for i,q in enumerate(pp):
    f0=abs(q)/2/np.pi; Q=abs(q)/(2*abs(q.real)); fz=abs(zz[i])/2/np.pi
    p0+=[np.log(f0),np.log(Q),np.log(fz)]
print("init",res(np.array(p0)).dot(res(np.array(p0))))
sol=least_squares(res,p0,max_nfev=20000,x_scale=0.05)
p=sol.x
print("cost",np.sum(sol.fun**2))
print(repr(list(p)))
for i in range(len(p)//3): print("f0=%.3f Q=%.5f fz=%.3f"%tuple(np.exp(p[3*i:3*i+3])))
for fs in FS:
    f=[1000,3000,4000,4500,5000,6000,7000,8000,9000,10000,12000,15000,20000,30000,40000]
    f=[x for x in f if x<fs/2]
    print(fs,[ "%d:%.2f"%(a,b) for a,b in zip(f,resp(p,fs,f))])
    print("max sb above 10k", resp(p,fs,np.linspace(10000,fs/2*0.999,500)).max())
    print("passband dev", np.abs(resp(p,fs,np.linspace(20,4000,400))).max())
