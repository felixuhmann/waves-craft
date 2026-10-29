// Development aid (not shipped): exposes the gain-cell dynamics of dsp/Dynamics.h to tools/fit_dynamics.py.
// Build (from plugins/fet-limiter): g++ -O2 -shared -fPIC -Isource tools/simwrap.cpp -o libfetsim.so
#include "dsp/Dynamics.h"

#include <cmath>

using namespace craft::fet_limiter::dsp;

static double interp (const double* xs, const double* ys, int n, double x)
{
    if (x <= xs[0]) return ys[0];
    if (x >= xs[n - 1]) return ys[n - 1];
    int i = 0;
    while (xs[i + 1] < x) ++i;
    const double t = (x - xs[i]) / (xs[i + 1] - xs[i]);
    return ys[i] + t * (ys[i + 1] - ys[i]);
}

// p: tauAttack, beta, tauC, tauHCharge, tauKCharge, tau1, tau2, tau3, offset
// Simulates: preMs at xBurst-30, burstMs at xBurst, postMs at xBurst-30 (peak envelope of a carrier). Writes the
// gain reduction (dB, >= 0) for every sample from the burst start on (burst + post) into out.
extern "C" int fet_sim (const double* p, double fs, const double* sx, const double* sr, int ns, double xBurst,
                        double burstMs, double preMs, double postMs, double* out, int outLen)
{
    DynParams d;
    d.tauAttackMs = p[0]; d.beta = p[1]; d.tauCMs = p[2]; d.tauHChargeMs = p[3]; d.tauKChargeMs = p[4];
    d.tau1Ms = p[5]; d.tau2Ms = p[6]; d.tau3Ms = p[7]; d.releaseOffsetDb = p[8];
    GainCell cell;
    cell.prepare (fs);
    cell.setParams (d);
    PeakDetector det;
    det.prepare (fs);
    const double aHi = std::pow (10.0, xBurst / 20.0), aLo = std::pow (10.0, (xBurst - 30.0) / 20.0);
    const int nPre = int (preMs * 0.001 * fs), nBurst = int (burstMs * 0.001 * fs), nPost = int (postMs * 0.001 * fs);
    int w = 0;
    double lastEnv = -1.0, target = 0.0;
    for (int i = 0; i < nPre + nBurst + nPost; ++i)
    {
        const double a = (i >= nPre && i < nPre + nBurst) ? aHi : aLo;
        const double env = det.process (a);
        if (env != lastEnv)
        {
            lastEnv = env;
            target = std::max (0.0, interp (sx, sr, ns, 20.0 * std::log10 (std::max (env, 1e-12))));
        }
        const double g = cell.process (target);
        if (i >= nPre && w < outLen)
            out[w++] = g;
    }
    return w;
}
