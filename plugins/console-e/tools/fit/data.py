"""Appendix A parser: column frequencies F, magnitude rows and the A.17 peak table."""
import os
import numpy as np
SPEC = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '..', '..', 'spec', 'console-e', 'r1', 'APPENDIX.md')
F = np.array([20, 31.5, 50, 80, 125, 200, 315, 500, 800, 1250, 2000, 3150, 5000, 8000, 12500, 16000, 20000.])
rows = {}
peaks = {}
sec = None
for line in open(SPEC):
    if line.startswith('###'):
        sec = line.split()[1]
        continue
    if not line.startswith('|') or line.startswith('|---'):
        continue
    c = [x.strip() for x in line.strip().strip('|').split('|')]
    if sec.startswith('A.') and sec != 'A.17' and len(c) == 18 and c[1] != '20':
        rows[c[0]] = np.array([float(x) for x in c[1:]])
    elif sec == 'A.17' and c[0] != 'id':
        peaks[c[0]] = [float(x) for x in c[1:]]
