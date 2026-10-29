# Evaluate one parameter set of analog_fit_r2.py. Usage: eval_r2.py <mode> name=value ...
import sys
mode=int(sys.argv[1]); kv=dict(a.split('=') for a in sys.argv[2:])
sys.argv=[sys.argv[0],'0']
import analog_fit_r2 as F
import numpy as np
P=dict(F.NEUTRAL); P.update({k:float(v) for k,v in kv.items()})
e,t=F.evaluate(mode,P)
print("rel",np.round(e/t,2)); print("worst",round(float(np.abs(e/t).max()),3))
