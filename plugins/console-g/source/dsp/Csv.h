// Craft Console G - minimal CSV reader for the spec tables embedded at build time (generated SpecTables.h).
#pragma once

#include <cctype>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace craft::consoleg::dsp
{

struct CsvTable
{
    std::vector<std::string> header;
    std::vector<std::vector<std::string>> rows;

    int column (const std::string& name) const
    {
        for (size_t i = 0; i < header.size(); ++i)
            if (header[i] == name)
                return int (i);
        throw std::runtime_error ("console-g: missing table column " + name);
    }
};

inline std::vector<std::string> splitCsvLine (const std::string& line)
{
    std::vector<std::string> out;
    std::string cur;
    bool quoted = false;
    for (char ch : line)
    {
        if (ch == '"')
            quoted = ! quoted;
        else if (ch == ',' && ! quoted)
        {
            out.push_back (cur);
            cur.clear();
        }
        else if (ch != '\r')
            cur.push_back (ch);
    }
    out.push_back (cur);
    return out;
}

inline CsvTable parseCsv (const char* const* lines, int count)
{
    CsvTable t;
    if (count <= 0)
        return t;
    t.header = splitCsvLine (lines[0]);
    for (int i = 1; i < count; ++i)
    {
        const std::string l (lines[i]);
        if (! l.empty())
            t.rows.push_back (splitCsvLine (l));
    }
    return t;
}

// Parses a table cell: plain numbers, "+Inf"/"inf", "Out" (= NaN, filter off) and values with a unit suffix
// ("10.0 dB", "0.40 s", "16 Hz", "20.8 KHz" -> 20800).
inline double parseCell (const std::string& cell)
{
    std::string s;
    for (char c : cell)
        if (c != ' ')
            s.push_back (c);
    if (s.empty())
        return std::numeric_limits<double>::quiet_NaN();
    std::string lower;
    for (char c : s)
        lower.push_back (char (std::tolower ((unsigned char) c)));
    if (lower.find ("inf") != std::string::npos)
        return lower[0] == '-' ? -std::numeric_limits<double>::infinity() : std::numeric_limits<double>::infinity();
    if (lower.rfind ("out", 0) == 0)
        return std::numeric_limits<double>::quiet_NaN();
    char* end = nullptr;
    const double v = std::strtod (s.c_str(), &end);
    if (end == s.c_str())
        return std::numeric_limits<double>::quiet_NaN();
    const std::string unit (end);
    std::string unitLower;
    for (char c : unit)
        unitLower.push_back (char (std::tolower ((unsigned char) c)));
    if (unitLower == "khz")
        return v * 1000.0;
    return v;
}

// Number of decimals written in a cell ("2.50" -> 2, "16 Hz" -> 0).
inline int cellDecimals (const std::string& cell)
{
    const auto dot = cell.find ('.');
    if (dot == std::string::npos)
        return 0;
    int n = 0;
    for (size_t i = dot + 1; i < cell.size() && std::isdigit ((unsigned char) cell[i]); ++i)
        ++n;
    return n;
}

} // namespace craft::consoleg::dsp
