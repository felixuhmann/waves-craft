import sys, numpy as np
from scipy.optimize import least_squares
src=open('analog_fit.py').read()
pre=src.split("TOL=np.array")[0]
sys.argv=['x','3']
exec(pre)
TOL=np.array([0.8]*10+[0.4,3,3]*3+[3.0,3.0])
p0=np.array([0.5238,106.20797,0.27041,0.999,0.024,0.04116,-1.07451,0.36711,0.58572,1272.52971,-1.5183,0.07232,0.0319])
lo=np.array([-3,40,0.1,0.1,0.001,0.001,-4,0,0,300,-5,-0.3,0.001]); hi=np.array([3,300,4,1.0,0.5,0.5,1,1,3,4000,0,0.3,1])
def res(p):
    e=evaluate(3,p)/TOL
    return np.sign(e)*np.maximum(np.abs(e),0)**2  # emphasise large errors
sol=least_squares(res,p0,bounds=(lo,hi),max_nfev=400,diff_step=1e-3)
p=sol.x
e=evaluate(3,p)
T2=np.array([1.0]*10+[0.5,4,4]*3+[3.0,4.0])
print("params"," ".join("%.6g"%x for x in p))
print("rel",np.round(e/T2,2))
print("worst",np.abs(e/T2).max())
