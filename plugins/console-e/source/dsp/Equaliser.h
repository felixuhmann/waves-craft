// Craft Console E - filter section (high-pass / low-pass) and 4-band equaliser (spec r1 sections 4 and 5).
#pragma once

#include <array>

#include "EqDesign.h"
#include "Parameters.h"
#include "Smoothing.h"

namespace craft::consolee::dsp
{

struct EqSettings
{
    double hp = 0.0, lp = 0.0; // normalised; 0 = Off
    double lfGain = 0.5, lfFreq = param::defaults::lfFreq;
    double lmfGain = 0.5, lmfFreq = param::defaults::lmfFreq, lmfQ = param::defaults::lmfQ;
    double hmfGain = 0.5, hmfFreq = param::defaults::hmfFreq, hmfQ = param::defaults::hmfQ;
    double hfGain = 0.5, hfFreq = param::defaults::hfFreq;
    bool lfBell = false, hfBell = false;
};

// Coefficient sets for a complete EQ setting (also used by the tests to evaluate the response directly).
struct EqCoefficients
{
    BandCoeffs hp, lp, lf, lmf, hmf, hf;
};

inline EqCoefficients designEq (const EqSettings& s, double fs) noexcept
{
    EqCoefficients c;
    c.hp = designHighPass (s.hp, fs);
    c.lp = designLowPass (s.lp, fs);
    c.lf = designEqBand (Band::lf, s.lfBell, s.lfFreq, 0.0, param::bandGain (s.lfGain), fs);
    c.lmf = designEqBand (Band::lmf, true, s.lmfFreq, s.lmfQ, param::bandGain (s.lmfGain), fs);
    c.hmf = designEqBand (Band::hmf, true, s.hmfFreq, s.hmfQ, param::bandGain (s.hmfGain), fs);
    c.hf = designEqBand (Band::hf, s.hfBell, s.hfFreq, 0.0, param::bandGain (s.hfGain), fs);
    return c;
}

// Up to two cascaded sections per channel.
template <int Channels>
struct SectionPair
{
    BandCoeffs coeffs;
    std::array<std::array<Biquad, 2>, Channels> st {};

    void set (const BandCoeffs& c) noexcept
    {
        coeffs = c;
        for (auto& ch : st)
            for (int i = 0; i < 2; ++i)
                ch[size_t (i)].c = c.s[size_t (i)];
    }
    void reset() noexcept
    {
        for (auto& ch : st)
            for (auto& b : ch)
                b.reset();
    }
    double process (int ch, double x) noexcept
    {
        // both sections always run (unused ones hold identity coefficients) so their states stay consistent
        x = st[size_t (ch)][0].process (x);
        return st[size_t (ch)][1].process (x);
    }
};

class Equaliser
{
public:
    static constexpr int maxChannels = 2;
    static constexpr int updateInterval = 16; // samples between coefficient updates while a control moves

    void prepare (double sampleRate, const EqSettings& initial) noexcept
    {
        fs = sampleRate;
        for (auto* r : ramps())
            r->setRampTime (0.012, fs);
        snapTo (initial);
        reset();
    }

    void reset() noexcept
    {
        for (auto* s : sections())
            s->reset();
    }

    // Structural parts (filter on/off, shapes) change immediately (the engine fades around such changes);
    // continuous controls glide.
    void setTargets (const EqSettings& s) noexcept
    {
        target = s;
        auto r = ramps();
        const double v[] = { s.hp, s.lp, s.lfGain, s.lfFreq, s.lmfGain, s.lmfFreq, s.lmfQ, s.hmfGain, s.hmfFreq, s.hmfQ, s.hfGain, s.hfFreq };
        for (size_t i = 0; i < r.size(); ++i)
            r[i]->setTarget (v[i]);
        const bool structural = (s.hp > 0) != (current.hp > 0) || (s.lp > 0) != (current.lp > 0) || s.lfBell != current.lfBell
                             || s.hfBell != current.hfBell;
        if (structural)
            snapTo (s);
    }

    void snapTo (const EqSettings& s) noexcept
    {
        target = s;
        auto r = ramps();
        const double v[] = { s.hp, s.lp, s.lfGain, s.lfFreq, s.lmfGain, s.lmfFreq, s.lmfQ, s.hmfGain, s.hmfFreq, s.hmfQ, s.hfGain, s.hfFreq };
        for (size_t i = 0; i < r.size(); ++i)
            r[i]->snap (v[i]);
        current = s;
        redesign();
        countdown = 0;
    }

    // Advance the control glides by one sample; redesigns every updateInterval samples while moving.
    void tick() noexcept
    {
        if (countdown > 0)
        {
            --countdown;
            return;
        }
        bool moving = false;
        for (auto* r : ramps())
            moving |= ! r->isSettled();
        if (! moving)
            return;
        for (auto* r : ramps())
            r->skip (updateInterval);
        auto r = ramps();
        current.hp = r[0]->getCurrent();
        current.lp = r[1]->getCurrent();
        current.lfGain = r[2]->getCurrent();
        current.lfFreq = r[3]->getCurrent();
        current.lmfGain = r[4]->getCurrent();
        current.lmfFreq = r[5]->getCurrent();
        current.lmfQ = r[6]->getCurrent();
        current.hmfGain = r[7]->getCurrent();
        current.hmfFreq = r[8]->getCurrent();
        current.hmfQ = r[9]->getCurrent();
        current.hfGain = r[10]->getCurrent();
        current.hfFreq = r[11]->getCurrent();
        current.lfBell = target.lfBell;
        current.hfBell = target.hfBell;
        redesign();
        countdown = updateInterval - 1;
    }

    double processFilters (int ch, double x) noexcept { return lpSec.process (ch, hpSec.process (ch, x)); }

    double processBands (int ch, double x) noexcept
    {
        x = lfSec.process (ch, x);
        x = lmfSec.process (ch, x);
        x = hmfSec.process (ch, x);
        return hfSec.process (ch, x);
    }

    const EqSettings& getCurrent() const noexcept { return current; }

private:
    void redesign() noexcept
    {
        const auto c = designEq (current, fs);
        hpSec.set (c.hp);
        lpSec.set (c.lp);
        lfSec.set (c.lf);
        lmfSec.set (c.lmf);
        hmfSec.set (c.hmf);
        hfSec.set (c.hf);
    }

    std::array<LinearRamp*, 12> ramps() noexcept
    {
        return { &rHp, &rLp, &rLfG, &rLfF, &rLmfG, &rLmfF, &rLmfQ, &rHmfG, &rHmfF, &rHmfQ, &rHfG, &rHfF };
    }
    std::array<SectionPair<maxChannels>*, 6> sections() noexcept { return { &hpSec, &lpSec, &lfSec, &lmfSec, &hmfSec, &hfSec }; }

    double fs = 48000.0;
    EqSettings target, current;
    LinearRamp rHp, rLp, rLfG, rLfF, rLmfG, rLmfF, rLmfQ, rHmfG, rHmfF, rHmfQ, rHfG, rHfF;
    SectionPair<maxChannels> hpSec, lpSec, lfSec, lmfSec, hmfSec, hfSec;
    int countdown = 0;
};

} // namespace craft::consolee::dsp
