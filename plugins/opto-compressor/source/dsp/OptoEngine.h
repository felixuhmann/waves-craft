// Craft Opto Compressor - host-independent DSP core (spec r1).
//
//   input -> gain cell (shared detector) -> colouration -> Gain -> Analog noise/hum -> Mix with dry -> Trim -> output
//
// Detector: weighting filter (spec 5) -> quadrature all-pass pair -> sqrt(a^2+b^2), which reads the sine peak at any
// frequency without look-ahead -> static curve lookup (spec 4). Two states follow the static target: a fast state and a
// slow memory state, whose sum is the reduction (spec 7). A zero-mean term makes the gain follow the waveform (spec 6).
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "OptoTables.h"

namespace craft::opto::dsp
{

inline constexpr double pi = 3.14159265358979323846;

struct Settings
{
    double gain = 4.08;       // 0..10
    double peakReduction = 0; // 0..10
    bool limit = false;
    double hiFreq = 100;      // 0..100
    int analog = 0;           // 0 off, 1 = 50 Hz, 2 = 60 Hz
    double mix = 1;           // 0..1
    double trimDb = 0;
    bool bypass = false;
};

// Spec 3 gain law.
inline double gainLawDb (double gain) noexcept { return 5.0 * (gain - 4.08) + 0.015; }

// Spec 4: gain (dB, <= 0) of the static table; bilinear, continuing with the last 4 dB slope above 0 dBFS.
inline double staticRowDb (int mode, int row, double levelDb) noexcept
{
    const auto& g = tables::staticGainDb[mode][row];
    if (levelDb <= -60.0)
        return g[0];
    if (levelDb >= 0.0)
    {
        const int last = tables::numLevels - 1;
        return g[last] + levelDb * (g[last] - g[last - 2]) / 4.0;
    }
    const double pos = (levelDb + 60.0) * 0.5;
    const int i = std::min (int (pos), tables::numLevels - 2);
    return g[i] + (pos - i) * (g[i + 1] - g[i]);
}

inline double staticGainDb (int mode, double pr, double levelDb) noexcept
{
    pr = std::clamp (pr, 0.0, 10.0);
    int r = 0;
    while (r < tables::numPrRows - 2 && pr >= tables::prRows[r + 1])
        ++r;
    const double u = (pr - tables::prRows[r]) / (tables::prRows[r + 1] - tables::prRows[r]);
    const double a = staticRowDb (mode, r, levelDb), b = staticRowDb (mode, r + 1, levelDb);
    return a + u * (b - a);
}

// Spec 8: editor-only Auto Makeup linkage, in Gain units.
inline double autoMakeupGainDelta (double prOld, double prNew) noexcept
{
    return (staticGainDb (0, prOld, -18.0) - staticGainDb (0, prNew, -18.0)) / 5.0;
}

struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, s1 = 0, s2 = 0;

    double process (double x) noexcept
    {
        const double y = b0 * x + s1;
        s1 = b1 * x - a1 * y + s2;
        s2 = b2 * x - a2 * y;
        return y;
    }
    void reset() noexcept { s1 = s2 = 0; }
    void set (double nb0, double nb1, double nb2, double na0, double na1, double na2) noexcept
    {
        b0 = nb0 / na0; b1 = nb1 / na0; b2 = nb2 / na0; a1 = na1 / na0; a2 = na2 / na0;
    }
    double magnitudeAt (double f, double fs) const noexcept
    {
        const double w = 2.0 * pi * f / fs, c1 = std::cos (w), s1w = std::sin (w), c2 = std::cos (2 * w), s2w = std::sin (2 * w);
        const double nr = b0 + b1 * c1 + b2 * c2, ni = b1 * s1w + b2 * s2w;
        const double dr = 1 + a1 * c1 + a2 * c2, di = a1 * s1w + a2 * s2w;
        return std::sqrt ((nr * nr + ni * ni) / (dr * dr + di * di));
    }
    void setBell (double f0, double q, double db, double fs) noexcept
    {
        const double A = std::pow (10.0, db / 40.0), w = 2 * pi * f0 / fs, al = std::sin (w) / (2 * q), c = std::cos (w);
        set (1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
    }
    void setLowShelf (double f0, double db, double fs) noexcept
    {
        const double A = std::pow (10.0, db / 40.0), w = 2 * pi * f0 / fs, al = std::sin (w) / std::sqrt (2.0), c = std::cos (w);
        const double r = 2 * std::sqrt (A) * al;
        set (A * ((A + 1) - (A - 1) * c + r), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - r),
             (A + 1) + (A - 1) * c + r, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - r);
    }
    // Second-order HF section: matched poles, numerator matched in magnitude at three frequencies, so that the
    // response holds up to Nyquist at every sample rate (M. Vicanek, "Matched Second Order Digital Filters").
    void setMatchedHf (double fz, double qz, double fp, double qp, double fs) noexcept
    {
        const double wp = 2 * pi * fp / fs, rp = std::exp (-wp / (2 * qp));
        const double a1n = -2 * rp * std::cos (wp * std::sqrt (std::max (1 - 1 / (4 * qp * qp), 0.0))), a2n = rp * rp;
        auto analog2 = [&] (double f) {
            const double zr = fz * fz - f * f, zi = f * fz / qz, pr = fp * fp - f * f, pi2 = f * fp / qp;
            const double k = (fp * fp) / (fz * fz);
            return (zr * zr + zi * zi) / (pr * pr + pi2 * pi2) * k * k;
        };
        const double d0 = (1 + a1n + a2n) * (1 + a1n + a2n), d1 = (1 - a1n + a2n) * (1 - a1n + a2n), d2 = -4 * a2n;
        const double f1 = (fs < 60000.0 ? 0.25 : 0.11) * fs, f2 = (fs < 60000.0 ? 0.47 : 0.22) * fs;
        auto rhs = [&] (double f, double& p0, double& p1, double& p2) {
            const double s = std::pow (std::sin (pi * f / fs), 2.0);
            p0 = 1 - s; p1 = s; p2 = 4 * (1 - s) * s;
            return analog2 (f) * (p0 * d0 + p1 * d1 + p2 * d2);
        };
        double p10, p11, p12, p20, p21, p22;
        const double A0 = d0; // |H(0)| = 1
        const double r1 = rhs (f1, p10, p11, p12) - p10 * A0, r2 = rhs (f2, p20, p21, p22) - p20 * A0;
        const double det = p11 * p22 - p12 * p21;
        const double A1 = (r1 * p22 - p12 * r2) / det, A2 = (p11 * r2 - r1 * p21) / det;
        const double s0 = std::sqrt (std::max (A0, 0.0)), s1n = std::sqrt (std::max (A1, 0.0));
        const double W = 0.5 * (s0 + s1n), b0n = 0.5 * (W + std::sqrt (W * W + A2));
        set (b0n, 0.5 * (s0 - s1n), -A2 / (4 * b0n), 1.0, a1n, a2n);
    }
};

class OptoEngine
{
public:
    static constexpr double smoothTauMs = 4.0; // settles within 20 ms (D2)

    void setSettings (const Settings& s) noexcept { target = s; }

    void prepare (double sampleRate) noexcept
    {
        fs = sampleRate;
        dtMs = 1000.0 / fs;
        smoothK = 1.0 - std::exp (-dtMs / smoothTauMs);
        rippleLeak = std::exp (-dtMs / 15.0); // short, so a tiny asymmetry of the zero-mean term cannot build a level offset

        using namespace tables;
        for (int c = 0; c < 2; ++c)
        {
            auto& d = det[c];
            d.shelf.setLowShelf (weighting[0], weighting[1], fs);
            d.bell.setBell (weighting[2], weighting[3], weighting[4], fs);
            const double w0 = 2 * pi * 1732.0 / fs, al = std::sin (w0) / (2 * weighting[10]);
            d.band.set (al, 0, -al, 1 + al, -2 * std::cos (w0), 1 - al);
            auto& col = chan[c];
            col.bell1.setBell (colour[0], colour[1], colour[2], fs);
            col.bell2.setBell (colour[3], colour[4], colour[5], fs);
            col.hf.setMatchedHf (colour[6], colour[7], colour[8], colour[9], fs);
        }
        const double k = std::tan (pi * std::min (weighting[5], 0.45 * fs) / fs);
        bandNorm = 1.0 / det[0].band.magnitudeAt (1000.0, fs);
        bandK = 1.0 - std::exp (-dtMs / 20.0);
        targetK = 1.0 - std::exp (-dtMs / 1.0);
        hpG = 1.0 / (1.0 + k);
        hpA = (k - 1.0) / (k + 1.0);
        detNorm = 1.0 / (det[0].shelf.magnitudeAt (1000.0, fs) * det[0].bell.magnitudeAt (1000.0, fs));
        colourNorm = 1.0 / (chan[0].bell1.magnitudeAt (1000.0, fs) * chan[0].bell2.magnitudeAt (1000.0, fs) * chan[0].hf.magnitudeAt (1000.0, fs));

        const double w1 = std::tan (pi * 12.0 / fs), w2 = std::tan (pi * 0.47);
        for (int i = 0; i < numQuadraturePoles; ++i)
        {
            const double p = w1 * std::pow (w2 / w1, quadraturePos[i]);
            quadC[i] = (p - 1.0) / (p + 1.0);
        }
        // nonlinearity weighting filters (fixed corners; only shape the colouration of spec 6)
        lfK = 1.0 - std::exp (-2.0 * pi * lfCornerHz / fs);
        hfK = 1.0 - std::exp (-2.0 * pi * hfCornerHz / fs);
        dcK = 1.0 - std::exp (-2.0 * pi * 5.0 / fs);

        for (int i = 0; i <= numFastLut; ++i)
        {
            // static reduction G = F + S_inf(F), inverted once: fast-state target for a total reduction
            const double G = i * fastLutStep;
            double lo = 0, hi = G;
            for (int n = 0; n < 40; ++n)
            {
                const double m = 0.5 * (lo + hi);
                (m + slowTarget (m) < G ? lo : hi) = m;
            }
            fastLut[i] = 0.5 * (lo + hi);
        }
        reset();
    }

    void reset() noexcept
    {
        for (int c = 0; c < 2; ++c)
        {
            det[c].hpX = det[c].hpY = det[c].avgX = det[c].avgB = det[c].avgW = det[c].avgBoost = det[c].avgHp = 0;
            std::fill (std::begin (det[c].ap), std::end (det[c].ap), 0.0);
            det[c].shelf.reset();
            det[c].bell.reset();
            det[c].band.reset();
            chan[c].bell1.reset();
            chan[c].bell2.reset();
            chan[c].hf.reset();
            chan[c].lf = chan[c].hfLp = chan[c].dc = 0;
        }
        fast = slow = ripple = targetS = 0;
        humPhase = 0;
        rng = 0x9E3779B97F4A7C15ull;
        snapSmoothers();
    }

    int getLatencySamples() const noexcept { return 0; }
    double getReductionDb() const noexcept { return lastReductionDb; } // <= 0

    // Test aid: put the detector states at the steady values for a given detector level with the current settings.
    void primeSteady (double levelDb) noexcept
    {
        snapSmoothers();
        fast = targetS = fastTargetForLevel (levelDb);
        slow = slowTarget (fast);
        ripple = 0;
    }

    // Test aid: complete the slow memory state for the present fast state instead of waiting tens of seconds.
    void settleMemory() noexcept { slow = slowTarget (fast); }

    template <typename Sample>
    void process (Sample* left, Sample* right, int n) noexcept
    {
        const bool stereo = right != nullptr;
        const double tGain = target.gain, tPr = target.peakReduction, tMode = target.limit ? 1.0 : 0.0, tHf = target.hiFreq;
        const double tMix = target.mix, tTrim = target.trimDb, tByp = target.bypass ? 1.0 : 0.0;
        const auto hum = humAmounts (target.analog);
        const double humHz = target.analog == 2 ? 60.0 : 50.0;

        for (int i = 0; i < n; ++i)
        {
            double xl = sanitize (double (left[i]));
            double xr = stereo ? sanitize (double (right[i])) : 0.0;

            smooth (gainS, tGain);
            smooth (prS, tPr);
            smooth (modeS, tMode);
            smooth (hfS, tHf);
            smooth (mixS, tMix);
            smooth (trimS, tTrim);
            smooth (bypS, tByp);
            smooth (noiseS, hum.noise);
            smooth (hum1S, hum.h1);
            smooth (hum2S, hum.h2);
            smooth (hum3S, hum.h3);

            // ---- detector (one shared for the channels; behaves as max(|L|, |R|))
            const double alpha = hiFreqAmount (hfS);
            double aPick = 0, env2 = quadrature (0, xl, alpha, aPick);
            int loud = 0;
            if (stereo)
            {
                double aR = 0;
                const double e2 = quadrature (1, xr, alpha, aR);
                if (e2 > env2)
                {
                    env2 = e2;
                    aPick = aR;
                    loud = 1;
                }
            }
            loudChannel = loud;
            const double reduction = stepDetector (env2, aPick, bandDrop (loud));
            const double cell = std::pow (10.0, -reduction / 20.0);
            lastReductionDb = -reduction;

            const double makeup = std::pow (10.0, (gainLawDb (gainS)) / 20.0);
            const double trim = std::pow (10.0, trimS / 20.0);
            const double nl = noiseS > 0.0 ? noiseS : 0.0;
            humPhase += 2.0 * pi * humHz / fs;
            if (humPhase > 2.0 * pi)
                humPhase -= 2.0 * pi;
            const double h = (hum1S == 0.0 && hum2S == 0.0 && hum3S == 0.0)
                                 ? 0.0
                                 : hum1S * std::sin (humPhase) + hum2S * std::sin (2 * humPhase) + hum3S * std::sin (3 * humPhase);

            left[i] = Sample (channelOut (0, xl, cell, makeup, trim, nl, h));
            if (stereo)
                right[i] = Sample (channelOut (1, xr, cell, makeup, trim, nl, h));
        }
    }

private:
    static constexpr double lfCornerHz = 200.0, hfCornerHz = 5000.0;
    static constexpr double lfH3 = 0.0125, lfH2 = 0.00265, hfH3 = 0.00185, hfH2 = 0.0006;
    static constexpr double rippleFourth = 0.133, bandDeadZone = 0.03;
    static constexpr int numFastLut = 240;
    static constexpr double fastLutStep = 0.25;

    struct DetChan
    {
        Biquad shelf, bell, band;
        double hpX = 0, hpY = 0, avgX = 0, avgB = 0, avgW = 0, avgBoost = 0, avgHp = 0;
        double ap[tables::numQuadraturePoles] {};
    };
    struct ColourChan
    {
        Biquad bell1, bell2, hf;
        double lf = 0, hfLp = 0, dc = 0;
    };
    struct HumLevels { double noise, h1, h2, h3; };

    static double sanitize (double x) noexcept { return std::isfinite (x) ? std::clamp (x, -1.0e6, 1.0e6) : 0.0; }

    void smooth (double& v, double t) noexcept
    {
        v += smoothK * (t - v);
        if (std::abs (t - v) < 1.0e-9)
            v = t;
    }

    void snapSmoothers() noexcept
    {
        gainS = target.gain; prS = target.peakReduction; modeS = target.limit ? 1.0 : 0.0; hfS = target.hiFreq;
        mixS = target.mix; trimS = target.trimDb; bypS = target.bypass ? 1.0 : 0.0;
        const auto hum = humAmounts (target.analog);
        noiseS = hum.noise; hum1S = hum.h1; hum2S = hum.h2; hum3S = hum.h3;
    }

    // Spec 8: noise and hum are added after Gain and before Mix (channelOut adds the small pre-Gain noise part).
    // Hum amplitudes are sine peak levels (assumption, see notes).
    static HumLevels humAmounts (int analog) noexcept
    {
        auto db = [] (double d) { return std::pow (10.0, d / 20.0); };
        if (analog == 1) return { db (-87.7), db (-95.6), db (-104.7), db (-105.4) };
        if (analog == 2) return { db (-87.7), db (-95.6), db (-104.8), db (-105.8) };
        return { 0.0, 0.0, 0.0, 0.0 };
    }

    double hiFreqAmount (double hf) const noexcept
    {
        using namespace tables;
        const double nodes[5] = { weighting[6], weighting[7], weighting[8], weighting[9], 0.0 };
        const double p = std::clamp (hf, 0.0, 100.0) / 25.0;
        const int i = std::min (int (p), 3);
        return nodes[i] + (p - i) * (nodes[i + 1] - nodes[i]);
    }

    // Spec 5 table: above about -20 dBFS the weighting narrows towards both band edges, not between 1 and 3 kHz.
    double bandDrop (int c) const noexcept
    {
        using namespace tables;
        const auto& d = det[c];
        if (d.avgX < 1.0e-9)
            return 0.0;
        const double shortfall = 1.0 - d.avgB / d.avgX * bandNorm - bandDeadZone;
        const double edge = shortfall > 0.0 ? weighting[11] * std::pow (shortfall, weighting[12]) : 0.0;
        const double boost = d.avgBoost > d.avgW && d.avgW > 1.0e-9 ? 20.0 * std::log10 (d.avgBoost / d.avgW) : 0.0;
        return edge + weighting[14] * boost;
    }

    double quadrature (int c, double x, double alpha, double& branchA) noexcept
    {
        auto& d = det[c];
        d.avgX += bandK * (std::abs (x) - d.avgX);
        d.avgB += bandK * (std::abs (d.band.process (x)) - d.avgB);
        double w = d.bell.process (d.shelf.process (x * detNorm));
        // parallel one-pole high-pass, weighted by the Hi Freq amount
        const double hp = hpG * (w - d.hpX) - hpA * d.hpY;
        d.hpX = w;
        d.hpY = hp;
        d.avgW += bandK * (std::abs (w) - d.avgW);
        d.avgHp += bandK * (std::abs (hp) - d.avgHp);
        w += alpha * hp;
        d.avgBoost += bandK * (std::abs (w) - d.avgBoost);

        double a = w, b = w;
        for (int k = 0; k < tables::numQuadraturePoles; k += 2)
        {
            const double ya = quadC[k] * a + d.ap[k];
            d.ap[k] = a - quadC[k] * ya;
            a = ya;
            const double yb = quadC[k + 1] * b + d.ap[k + 1];
            d.ap[k + 1] = b - quadC[k + 1] * yb;
            b = yb;
        }
        branchA = a;
        return a * a + b * b;
    }

    static double releaseTau (double F) noexcept
    {
        using namespace tables;
        if (F <= releaseNodeDb[0])
            return releaseTauMs[0];
        for (int i = 1; i < numReleaseNodes; ++i)
            if (F < releaseNodeDb[i])
                return releaseTauMs[i - 1] + (F - releaseNodeDb[i - 1]) / (releaseNodeDb[i] - releaseNodeDb[i - 1]) * (releaseTauMs[i] - releaseTauMs[i - 1]);
        return releaseTauMs[numReleaseNodes - 1];
    }
    static double releaseRate (double F) noexcept { return F > 0.0 ? F / releaseTau (F) : 0.0; }
    static double slowTarget (double F) noexcept { return tables::slowMaxDb * (1.0 - std::exp (-F / tables::slowDepthDb)); }

    double fastTargetForLevel (double levelDb) const noexcept
    {
        const double gC = staticGainDb (0, prS, levelDb), gL = staticGainDb (1, prS, levelDb);
        const double G = std::max (0.0, -(gC + modeS * (gL - gC)));
        const double pos = std::min (G / fastLutStep, double (numFastLut) - 1.0e-6);
        const int i = int (pos);
        return fastLut[i] + (pos - i) * (fastLut[i + 1] - fastLut[i]);
    }

    double stepDetector (double env2, double branchA, double drop) noexcept
    {
        double levelDb = env2 > 1.0e-14 ? 10.0 * std::log10 (env2) : -140.0;
        levelDb -= drop * std::clamp ((levelDb + tables::weighting[13]) / tables::weighting[13], 0.0, 1.0);
        // a short target smoother keeps the detector's residual ripple from biasing the asymmetric ballistics
        targetS += targetK * ((levelDb > -60.0 ? fastTargetForLevel (levelDb) : 0.0) - targetS);
        if (targetS < 1.0e-9)
            targetS = 0.0;
        const double T = targetS;
        if (T == 0.0 && fast == 0.0 && slow == 0.0 && ripple == 0.0)
            return 0.0;

        using namespace tables;
        const double e = T - fast;
        double dF;
        if (e > 0.0)
        {
            const double tau = attackTau0Ms * (1.0 + std::pow (fast / attackDepthDb, attackDepthExp))
                               * (1.0 + std::pow (attackErrDb / (e + 1.0e-6), attackErrExp));
            const double r = e / tau;
            dF = r / (1.0 + r / attackMaxDbPerMs);
        }
        else
        {
            dF = std::min (0.0, releaseRate (T) - releaseRate (fast));
        }

        const double si = slowTarget (fast);
        double dS;
        if (slow < si)
        {
            const double q = 1.0 - slow / si;
            dS = std::min ((si - slow) / dtMs, si * slowChargeA * std::pow (fast + 1.0e-9, slowChargeExpDepth) * std::pow (q, slowChargeExpFill));
        }
        else
        {
            dS = (si - slow) / slowDecayMs;
        }

        // gain-cell following of the waveform: zero mean on a sine, second and fourth harmonic of the phase (spec 6)
        double w = 0.0, hfShare = 0.0;
        if (env2 > 1.0e-14 && T > 0.0)
        {
            const double c2 = 1.0 - 2.0 * branchA * branchA / env2; // cos 2 phase of the detector branch
            const double gate = fast > 0.0 ? std::min (1.0, T / fast) : 0.0;
            // tables/harmonics.csv: the fourth-harmonic part keeps a constant ratio at low frequency; towards 5 kHz it
            // grows more slowly with depth than the second-harmonic part.
            hfShare = det[loudChannel].avgX > 1.0e-9 ? std::min (1.0, std::pow (det[loudChannel].avgHp / det[loudChannel].avgX / 0.47, 2.0)) : 0.0;
            const double fourth = rippleFourth * (1.0 + 1.3 * hfShare * (std::pow (std::clamp (releaseRate (fast), 0.03, 0.7) / 0.7, -0.55) - 1.0));
            w = (-4.0 / 3.0 * c2 + fourth * (2.0 * c2 * c2 - 1.0)) * gate * gate;
        }
        ripple = ripple * rippleLeak + dtMs * releaseRate (fast) * w * (1.0 + hfShare * (std::pow (std::clamp (releaseRate (fast), 0.03, 0.7) / 0.7, -0.17) - 1.0));
        if (std::abs (ripple) < 1.0e-12)
            ripple = 0.0;

        fast = std::clamp (fast + dtMs * dF, 0.0, 100.0);
        slow = std::clamp (slow + dtMs * dS, 0.0, 100.0);
        if (fast < 1.0e-9) fast = 0.0;
        if (slow < 1.0e-9) slow = 0.0;

        return std::max (0.0, fast + slow + ripple);
    }

    double channelOut (int c, double x, double cell, double makeup, double trim, double noiseAmp, double hum) noexcept
    {
        auto& col = chan[c];
        const double v = x * cell;
        // spec 6 harmonics: weighted x|x| (odd) and x^2 (even, DC removed) terms, finite for any level
        col.lf += lfK * (v - col.lf);
        col.hfLp += hfK * (v - col.hfLp);
        const double vh = v - col.hfLp;
        const double l = std::clamp (col.lf, -4.0, 4.0), u = std::clamp (vh, -4.0, 4.0);
        const double even = lfH2 * l * l + hfH2 * u * u;
        col.dc += dcK * (even - col.dc);
        const double nl = v + lfH3 * l * std::abs (l) + hfH3 * u * std::abs (u) + (even - col.dc);
        double wet = col.hf.process (col.bell2.process (col.bell1.process (nl * colourNorm))) * makeup;

        if (noiseAmp > 0.0)
        {
            // spec 8: -87.7 dBFS at unity and -82.7 dBFS at Gain 10: mostly output-referred plus a small pre-Gain part
            const double post = std::sqrt (noiseAmp * noiseAmp * (1.0 + makeup * makeup * preNoiseShare));
            wet += nextNoise() * post;
        }
        wet += hum;

        const double mixed = mixS * wet + (1.0 - mixS) * x;
        const double out = mixed * trim;
        const double y = bypS * x + (1.0 - bypS) * out;
        return std::isfinite (y) ? y : resetAfterFault();
    }

    double resetAfterFault() noexcept
    {
        reset();
        return 0.0;
    }

    double nextNoise() noexcept
    {
        rng ^= rng >> 12; rng ^= rng << 25; rng ^= rng >> 27;
        const uint64_t r = rng * 0x2545F4914F6CDD1Dull;
        return (double (r >> 11) * (1.0 / 9007199254740992.0) * 2.0 - 1.0) * 1.7320508075688772;
    }

    Settings target;
    double fs = 48000, dtMs = 1000.0 / 48000, smoothK = 0.005, rippleLeak = 0.9998;
    double gainS = 4.08, prS = 0, modeS = 0, hfS = 100, mixS = 1, trimS = 0, bypS = 0, noiseS = 0, hum1S = 0, hum2S = 0, hum3S = 0;

    DetChan det[2];
    ColourChan chan[2];
    double detNorm = 1, colourNorm = 1, bandNorm = 1, bandK = 0, hpG = 1, hpA = 0, lfK = 0, hfK = 0, dcK = 0;
    double quadC[tables::numQuadraturePoles] {};
    double fastLut[numFastLut + 1] {};

    double fast = 0, slow = 0, ripple = 0, lastReductionDb = 0, targetS = 0, targetK = 0;
    int loudChannel = 0;
    double humPhase = 0;
    uint64_t rng = 0x9E3779B97F4A7C15ull;

    // (10^(5/10) - 1) / (makeup at Gain 10)^2: the pre-Gain share of the noise power (spec 8, table noise.csv)
    static constexpr double preNoiseShare = 2.162277660 / 918.0;
};

} // namespace craft::opto::dsp
