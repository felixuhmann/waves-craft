# Phase difference of the public-domain 90-degree allpass pair (O. Niemitalo) at several sample rates.
import numpy as np
from scipy import signal
A1=[0.6923878,0.9360654322959,0.9882295226860,0.9987488452737]
A2=[0.4021921162426,0.8561710882420,0.9722909545651,0.9952884791278]
def resp(coefs,w,delay=0):
    z=np.exp(1j*w)
    h=np.ones_like(z)
    for a in coefs:
        c=a*a
        h*= (c - z**-2)/(1 - c*z**-2)
    return h*z**(-delay)
for fs in [44100,48000,96000,192000]:
    f=np.array([10,15,20,30,45,60,100,1000,10000,15000,20000])
    f=f[f<fs/2]
    w=2*np.pi*f/fs
    d=np.angle(resp(A1,w,1)/resp(A2,w))*180/np.pi
    print(fs,' '.join('%g:%.2f'%(a,b) for a,b in zip(f,d)))
