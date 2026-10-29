"""Fit of one analog mode (section 10 tables). Usage: python3 fitm.py <mode> [start.json] [max evaluations]"""
from analog import *
import json, sys
mode=int(sys.argv[1])
if mode==1:
    names=['g','lsf','lsdb','hsf','hsdb','b2','b3']
    P0=dict(g=-0.26,lsf=60,lsdb=0.2,hsf=15000.0,hsdb=-0.1,b2=0.004,b3=0.007)
    lo=[-1,10,-1,2000,-2,-0.1,-0.1]; hi=[0,500,1,23000,2,0.1,0.1]
elif mode==2:
    names=['g','pkf','pkq','pkdb','hsf','hsdb','b2','bf','bq','g3','g2']
    P0=dict(g=-0.02,pkf=1800,pkq=0.5,pkdb=-0.78,hsf=12000,hsdb=0.2,b2=2.3e-4,bf=1500,bq=0.7,g3=0.05,g2=0.0)
    lo=[-1,300,0.1,-3,2000,-2,-0.01,200,0.1,-1,-1]; hi=[1,8000,3,0,23000,2,0.01,10000,5,1,1]
elif mode==3:
    names=['g','lsf','lsdb','lsq','pkf','pkq','pkdb','bf','bq','ap','tp','an','tn','g2','g3','b2','b3','hsf','hsdb']
    P0=dict(g=0.0,lsf=120,lsdb=0.3,pkf=2500,pkq=0.6,pkdb=-1.0,bf=2000,bq=0.5,ap=0.5,tp=0.05,an=0.3,tn=0.03,g2=0.0,g3=0.0,b2=0.0)
    P0.update(lsq=0.7071,b3=0.0,hsf=15000.0,hsdb=0.0)
    lo=[-3,20,-3,0.2,200,0.1,-6,200,0.1,0,1e-4,0,1e-4,-2,-2,-0.1,-0.1,2000,-3]; hi=[3,1000,3,3,15000,4,3,10000,4,1,1,1,1,2,2,0.1,0.1,23000,3]
elif mode==4:
    names=['g','hsf','hsdb','hsq','pkf','pkq','pkdb','xo','xk','b3']
    P0=dict(g=-0.03,hsf=3000,hsdb=0.25,hsq=0.7,pkf=18000.0,pkq=0.7,pkdb=-0.2,xo=9e-4,xk=1e-3,b3=0.0)
    lo=[-1,500,-2,0.2,3000,0.1,-2,0,1e-5,-0.1]; hi=[1,20000,2,3,14000,5,1,0.01,0.05,0.1]
    import analog; analog.MULTI=True; P0.update(json.load(open('mode4_hard.json'))); P0['xk']=0.001; names.remove('xk'); lo.pop(8); hi.pop(8)
if len(sys.argv)>2:
    P0.update(json.load(open(sys.argv[2])))
P,c=fit(mode,names,P0,lo,hi,nfev=int(sys.argv[3]) if len(sys.argv)>3 else 300)
print('cost',c)
print(json.dumps(P))
json.dump(P,open('mode%d.json'%mode,'w'))
report(mode,P)
