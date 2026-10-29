// Craft Console E - shared helpers for the unit tests.
#pragma once

#include <cmath>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "dsp/ConsoleEngine.h"

namespace craft::consolee::test
{

using namespace craft::consolee::dsp;

inline double dbToGain (double db) { return std::pow (10.0, db / 20.0); }
inline double gainToDb (double g) { return 20.0 * std::log10 (g); }

// ---- Appendix A / B parser ------------------------------------------------------------------------
struct AppendixRow
{
    std::string section, id;
    std::vector<double> values;
};

inline const std::vector<double>& appendixFrequencies()
{
    static const std::vector<double> f { 20, 31.5, 50, 80, 125, 200, 315, 500, 800, 1250, 2000, 3150, 5000, 8000, 12500, 16000, 20000 };
    return f;
}

inline std::vector<std::string> splitCells (const std::string& line)
{
    std::vector<std::string> cells;
    std::stringstream ss (line);
    std::string cell;
    while (std::getline (ss, cell, '|'))
    {
        const auto a = cell.find_first_not_of (" \t\r");
        const auto b = cell.find_last_not_of (" \t\r");
        cells.push_back (a == std::string::npos ? std::string() : cell.substr (a, b - a + 1));
    }
    if (! cells.empty() && cells.front().empty()) cells.erase (cells.begin());
    if (! cells.empty() && cells.back().empty()) cells.pop_back();
    return cells;
}

inline double parseNumber (const std::string& s)
{
    if (s == "—" || s.empty()) return std::nan ("");
    if (s == "+Inf" || s == "Inf") return INFINITY;
    return std::stod (s);
}

// All table rows of the appendix: section heading ("A.3", "B.1", ...) and numeric cells.
inline const std::vector<AppendixRow>& appendixRows()
{
    static const std::vector<AppendixRow> rows = [] {
        std::vector<AppendixRow> out;
        std::ifstream in (CONSOLE_E_APPENDIX);
        std::string line, section;
        while (std::getline (in, line))
        {
            if (line.rfind ("### ", 0) == 0)
            {
                std::stringstream ss (line.substr (4));
                ss >> section;
                continue;
            }
            if (line.rfind ("|", 0) != 0 || line.rfind ("|---", 0) == 0)
                continue;
            auto cells = splitCells (line);
            if (cells.size() < 2)
                continue;
            const std::string& first = cells[0];
            // skip header rows (first value cell not numeric or a header label)
            if (first == "id" || first == "R \\ u" || first == "R \\ T" || first == "HP" || first == "LP" || first.find (' ') != std::string::npos)
                continue;
            AppendixRow r;
            r.section = section;
            r.id = first;
            for (size_t i = 1; i < cells.size(); ++i)
                r.values.push_back (parseNumber (cells[i]));
            out.push_back (r);
        }
        return out;
    }();
    return rows;
}

// ---- row id -> control settings ------------------------------------------------------------------
enum class RowBand { hp, lp, lfShelf, lfBell, lmf, hmf, hfShelf, hfBell };

struct RowSetting
{
    RowBand band;
    double freqR = 0, qR = 0, gainDb = 0;
};

inline std::vector<std::string> splitId (const std::string& id)
{
    std::vector<std::string> t;
    std::stringstream ss (id);
    std::string x;
    while (std::getline (ss, x, '_'))
        t.push_back (x);
    return t;
}

// Rows labelled Q 0.5 coincide exactly with the Q sweep at r = 0.9 (lmq_0.90 == gm_1k_q0.5_+12.0), see notes.
inline double qRowToNorm (double q)
{
    if (q == 3.5) return 0.0;
    if (q == 1.5) return 0.5;
    if (q == 0.5) return 0.9;
    return 1.0;
}

inline RowSetting classifyRow (const std::string& id)
{
    namespace pm = param;
    const auto t = splitId (id);
    RowSetting s {};
    auto num = [] (const std::string& x) { return std::stod (x); };
    if (t[0] == "hpn") { s.band = RowBand::hp; s.freqR = num (t[1]); return s; }
    if (t[0] == "lpn") { s.band = RowBand::lp; s.freqR = num (t[1]); return s; }
    if (t[0] == "lf") { s.band = t[1] == "shelf" ? RowBand::lfShelf : RowBand::lfBell; s.freqR = pm::lfFreq.toNorm (num (t[2])); s.gainDb = num (t[3]); return s; }
    if (t[0] == "hf") { s.band = t[1] == "shelf" ? RowBand::hfShelf : RowBand::hfBell; s.freqR = pm::hfFreq.toNorm (num (t[2])); s.gainDb = num (t[3]); return s; }
    if (t[0] == "lff") { s.band = RowBand::lfBell; s.freqR = num (t[1]); s.gainDb = 12; return s; }
    if (t[0] == "hff") { s.band = RowBand::hfBell; s.freqR = num (t[1]); s.gainDb = 12; return s; }
    if (t[0] == "lmq") { s.band = RowBand::lmf; s.freqR = 0.5; s.qR = num (t[1]); s.gainDb = 12; return s; }
    if (t[0] == "gl") { s.band = t[1] == "bell60" ? RowBand::lfBell : RowBand::lfShelf; s.freqR = pm::lfFreq.toNorm (60); s.gainDb = num (t[2]); return s; }
    if (t[0] == "gh") { s.band = t[1] == "bell8k" ? RowBand::hfBell : RowBand::hfShelf; s.freqR = 0.5; s.gainDb = num (t[2]); return s; }
    if (t[0] == "gm") { s.band = RowBand::lmf; s.freqR = 0.5; s.qR = qRowToNorm (num (t[2].substr (1))); s.gainDb = num (t[3]); return s; }
    if (t[0] == "lmf" || t[0] == "hmf")
    {
        s.band = t[0] == "lmf" ? RowBand::lmf : RowBand::hmf;
        if (t.size() == 2) { s.freqR = num (t[1]); s.qR = 0.5; s.gainDb = 12; return s; }
        s.freqR = (t[0] == "lmf" ? pm::lmfFreq : pm::hmfFreq).toNorm (num (t[1]));
        s.qR = qRowToNorm (num (t[2]));
        s.gainDb = num (t[3]);
        return s;
    }
    throw std::runtime_error ("unknown row " + id);
}

// EQ settings for a row (all other controls neutral)
inline EqSettings eqForRow (const RowSetting& r)
{
    EqSettings s;
    const double g = param::bandGain.toNorm (r.gainDb);
    switch (r.band)
    {
        case RowBand::hp: s.hp = r.freqR; break;
        case RowBand::lp: s.lp = r.freqR; break;
        case RowBand::lfShelf: case RowBand::lfBell: s.lfBell = r.band == RowBand::lfBell; s.lfFreq = r.freqR; s.lfGain = g; break;
        case RowBand::lmf: s.lmfFreq = r.freqR; s.lmfQ = r.qR; s.lmfGain = g; break;
        case RowBand::hmf: s.hmfFreq = r.freqR; s.hmfQ = r.qR; s.hmfGain = g; break;
        case RowBand::hfShelf: case RowBand::hfBell: s.hfBell = r.band == RowBand::hfBell; s.hfFreq = r.freqR; s.hfGain = g; break;
    }
    return s;
}

inline double eqResponseDb (const EqCoefficients& c, double f, double fs)
{
    return c.hp.magnitudeDb (f, fs) + c.lp.magnitudeDb (f, fs) + c.lf.magnitudeDb (f, fs) + c.lmf.magnitudeDb (f, fs)
         + c.hmf.magnitudeDb (f, fs) + c.hf.magnitudeDb (f, fs);
}

// ---- signals ------------------------------------------------------------------------------------
inline std::vector<double> sine (double freq, double fs, double levelDb, size_t n, double phase = 0.0)
{
    std::vector<double> x (n);
    const double a = dbToGain (levelDb);
    for (size_t i = 0; i < n; ++i)
        x[i] = a * std::sin (2.0 * pi * freq * double (i) / fs + phase);
    return x;
}

// Amplitude of the component at freq over [start, start + len) (single-bin DFT, len = whole cycles ideally).
inline double toneAmplitude (const std::vector<double>& x, double freq, double fs, size_t start, size_t len)
{
    double re = 0, im = 0;
    for (size_t i = start; i < start + len && i < x.size(); ++i)
    {
        const double ph = 2.0 * pi * freq * double (i) / fs;
        re += x[i] * std::cos (ph);
        im += x[i] * std::sin (ph);
    }
    return 2.0 * std::sqrt (re * re + im * im) / double (len);
}

inline size_t wholeCycles (double freq, double fs, double approxSeconds)
{
    const double period = fs / freq;
    const double cycles = std::max (1.0, std::floor (approxSeconds * fs / period));
    return size_t (std::llround (cycles * period));
}

// Run a stereo (or mono) signal through an engine in blocks; channels share the input when `right` is empty.
inline std::vector<std::vector<double>> run (ConsoleEngine& e, const std::vector<double>& left, const std::vector<double>& right = {},
                                             int block = 256)
{
    const int nch = e.getNumChannels();
    std::vector<std::vector<double>> io (static_cast<size_t> (nch));
    io[0] = left;
    if (nch > 1)
        io[1] = right.empty() ? left : right;
    for (size_t pos = 0; pos < left.size(); pos += size_t (block))
    {
        const int n = int (std::min (size_t (block), left.size() - pos));
        double* ptr[2] = { io[0].data() + pos, nch > 1 ? io[1].data() + pos : nullptr };
        e.process (ptr, n);
    }
    return io;
}

inline ConsoleEngine makeEngine (const Settings& s, double fs = 48000.0, int channels = 2)
{
    ConsoleEngine e (channels);
    e.setSettings (s);
    e.prepare (fs);
    return e;
}

// Settings helpers
inline Settings withCompressor (double ratio, double thresholdDb, Settings s = {})
{
    s.compRatio = param::ratioToNorm (ratio);
    s.compThreshold = param::compThreshold.toNorm (thresholdDb);
    return s;
}

} // namespace craft::consolee::test
