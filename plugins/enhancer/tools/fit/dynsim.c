/* Control-rate model of the processed-path gain (fit helper for dyn_fit2.py; mirrors source/dsp/BandDynamics.h).
   Build: gcc -O2 -shared -fPIC -o dynsim.so dynsim.c -lm */
#include <math.h>

static double interp_curve (double L, const double* lv, const double* g, int n)
{
    if (L <= lv[0]) return g[0];
    for (int i = 1; i < n; ++i)
        if (L <= lv[i]) return g[i - 1] + (g[i] - g[i - 1]) * (L - lv[i - 1]) / (lv[i] - lv[i - 1]);
    return g[n - 1];
}

/* p: tfa tfr tsa tsr beta kappa width tgu tgd tgu2 tgd2 gsil ceil offs  (14)
   curve: 7 gains at levels lv; seq: nseg levels (dBFS input, -999 = silence) with durations
   times: sample times (s) -> out gains (dB) */
void simulate (const double* p, const double* lv, const double* curve, int ncurve, const double* segLevel,
               const double* segDur, int nseg, double dt, const double* times, int ntimes, double* out, double* trace,
               int ntrace)
{
    const double tfa = p[0], tfr = p[1], tsa = p[2], tsr = p[3], beta = p[4], kappa = p[5], width = p[6];
    const double tgu = p[7], tgd = p[8], tgu2 = p[9], tgd2 = p[10], gsil = p[11], ceil_ = p[12], offs = p[13];
    const double cfa = exp (-dt / fmax (tfa, 1e-6)), cfr = exp (-dt / fmax (tfr, 1e-6));
    const double csa = exp (-dt / fmax (tsa, 1e-6)), csr = exp (-dt / fmax (tsr, 1e-6));
    const double cgu = exp (-dt / fmax (tgu, 1e-6)), cgd = exp (-dt / fmax (tgd, 1e-6));
    const double cgu2 = exp (-dt / fmax (tgu2, 1e-6)), cgd2 = exp (-dt / fmax (tgd2, 1e-6));
    const double floorDb = -120.0;
    double lF = floorDb, S = 1e-6, G1 = -1, G2 = -1;
    double tEnd = 0;
    for (int s = 0; s < nseg; ++s) tEnd += segDur[s];
    const int n = (int) (tEnd / dt + 0.5);
    int seg = 0;
    double segStart = 0;
    int ti = 0;
    const double h0 = width * log1p (exp (-kappa / width));
    for (int i = 0; i <= n; ++i)
    {
        const double t = i * dt;
        while (seg < nseg - 1 && t >= segStart + segDur[seg] - 1e-12) { segStart += segDur[seg]; ++seg; }
        const double levIn = segLevel[seg];
        const double xDb = levIn < -900 ? floorDb : levIn + offs;
        const double x = pow (10.0, xDb / 20.0);
        /* fast detector (dB domain) */
        lF = xDb > lF ? xDb + (lF - xDb) * cfa : xDb + (lF - xDb) * cfr;
        /* slow detector (linear) */
        S = x > S ? x + (S - x) * csa : x + (S - x) * csr;
        const double sDb = 20.0 * log10 (fmax (S, 1e-12));
        double r = sDb - lF;
        if (r < 0) r = 0;
        const double h = beta * (width * log1p (exp ((r - kappa) / width)) - h0);
        double Lin = lF - offs, g;
        if (Lin >= lv[0]) g = interp_curve (Lin, lv, curve, ncurve);
        else
        {
            double a = (lv[0] - Lin) / 20.0;
            if (a > 1) a = 1;
            g = curve[0] + (gsil - curve[0]) * a;
        }
        /* boosted gain: soft minimum with the ceiling (1 dB knee), never below the static gain */
        double gt = -log (exp (-(g + h)) + exp (-ceil_));
        if (gt < g) gt = g;
        const double gl = pow (10.0, gt / 20.0);
        if (G1 < 0) { G1 = gl; G2 = gl; }
        G1 = gl > G1 ? gl + (G1 - gl) * cgu : gl + (G1 - gl) * cgd;
        G2 = G1 > G2 ? G1 + (G2 - G1) * cgu2 : G1 + (G2 - G1) * cgd2;
        if (trace && i < ntrace) trace[i] = 20 * log10 (G2);
        while (ti < ntimes && times[ti] <= t + 0.5 * dt) { out[ti] = 20 * log10 (G2); ++ti; }
    }
    while (ti < ntimes) { out[ti] = 20 * log10 (G2); ++ti; }
}
