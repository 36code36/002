#include "ChordEngine.h"
#include <algorithm>
#include <cmath>

namespace ChordEngine
{
namespace
{
    const char* const pcNames[12] = { "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B" };

    bool has (const std::vector<int>& v, int x)
    {
        return std::find (v.begin(), v.end(), x) != v.end();
    }

    void removeValue (std::vector<int>& v, int x)
    {
        v.erase (std::remove (v.begin(), v.end(), x), v.end());
    }

    //==========================================================================
    // parsing

    int parseRoot (const juce::String& s, int& consumed)
    {
        consumed = 0;
        if (s.isEmpty())
            return -1;

        int pc = 0;
        switch ((int) juce::CharacterFunctions::toUpperCase (s[0]))
        {
            case 'C': pc = 0;  break;
            case 'D': pc = 2;  break;
            case 'E': pc = 4;  break;
            case 'F': pc = 5;  break;
            case 'G': pc = 7;  break;
            case 'A': pc = 9;  break;
            case 'B': pc = 11; break;
            default:  return -1;
        }

        consumed = 1;
        if (s.length() > 1)
        {
            if (s[1] == '#')      { pc = (pc + 1) % 12;  consumed = 2; }
            else if (s[1] == 'b') { pc = (pc + 11) % 12; consumed = 2; }
        }
        return pc;
    }

    // Turns a quality string ("m7b5", "maj9", "7#9", "13", "sus4" ...) into intervals.
    bool intervalsFromQuality (const juce::String& qIn, std::vector<int>& out)
    {
        juce::String q = qIn.removeCharacters (" ()");
        q = q.replace ("major", "maj").replace ("minor", "m").replace ("min", "m");

        if (q.startsWith ("Maj") || q.startsWith ("MAJ"))
            q = "maj" + q.substring (3);
        else if (q.startsWith ("M"))
            q = "maj" + q.substring (1);

        if (q.startsWith ("-"))
            q = "m" + q.substring (1);
        q = q.replace ("-", "b");

        if (q.startsWith ("+"))
            q = "aug" + q.substring (1);

        bool minor = false, dim = false, aug = false, majFlag = false, minorMaj = false;
        juce::String rest = q;

        if (rest.startsWith ("dim"))
        {
            dim = true;
            rest = rest.substring (3);
        }
        else if (rest.startsWith ("aug"))
        {
            aug = true;
            rest = rest.substring (3);
        }
        else if (rest.startsWith ("maj"))
        {
            majFlag = true;
            rest = rest.substring (3);
        }
        else if (rest.startsWith ("m"))
        {
            minor = true;
            rest = rest.substring (1);
            if (rest.startsWithIgnoreCase ("maj"))
            {
                minorMaj = true;
                rest = rest.substring (3);
            }
        }

        const bool sus2  = rest.contains ("sus2");
        const bool sus4  = rest.contains ("sus4") || (rest.contains ("sus") && ! sus2);
        const bool alt   = rest.contains ("alt");
        const bool b5    = rest.contains ("b5");
        const bool s5    = rest.contains ("#5");
        const bool b9    = rest.contains ("b9");
        const bool s9    = rest.contains ("#9");
        const bool s11   = rest.contains ("#11");
        const bool b13   = rest.contains ("b13");
        const bool add9  = rest.contains ("add9") || rest.contains ("add2");
        const bool add11 = rest.contains ("add11") || rest.contains ("add4");

        const char* tokens[] = { "add9", "add11", "add2", "add4", "sus2", "sus4", "sus", "alt",
                                 "b13", "#11", "b9", "#9", "b5", "#5" };
        for (auto t : tokens)
            rest = rest.replace (t, "");

        int idx = 0;
        juce::String digits;
        while (idx < rest.length() && juce::CharacterFunctions::isDigit (rest[idx]))
        {
            digits += rest[idx];
            ++idx;
        }
        const int ext = digits.getIntValue();
        rest = rest.substring (idx);

        if (rest.isNotEmpty())
            return false;
        if (ext != 0 && ext != 5 && ext != 6 && ext != 7 && ext != 9 && ext != 11 && ext != 13 && ext != 69)
            return false;

        out.clear();
        out.push_back (0);

        if (ext == 5)
        {
            out.push_back (7);
            return true;
        }

        int third = (minor || dim) ? 3 : 4;
        if (sus2)      third = 2;
        else if (sus4) third = 5;

        int fifth = dim ? 6 : (aug ? 8 : 7);
        if (b5)  fifth = 6;
        if (s5)  fifth = 8;
        if (alt) fifth = 8;

        out.push_back (third);
        out.push_back (fifth);

        const bool wantsSeventh = (ext == 7 || ext == 9 || ext == 11 || ext == 13);
        if (dim && ext == 7)
            out.push_back (9);
        else if ((majFlag || minorMaj) && wantsSeventh)
            out.push_back (11);
        else if (wantsSeventh || alt)
            out.push_back (10);
        else if (ext == 6 || ext == 69)
            out.push_back (9);

        const bool flat9 = b9 || alt;
        if (ext == 9 || ext == 11 || ext == 13 || ext == 69 || add9 || flat9 || s9)
            out.push_back (flat9 ? 13 : (s9 ? 15 : 14));
        if (ext == 11 || add11)
            out.push_back (17);
        if (s11)
            out.push_back (18);
        if (ext == 13)
            out.push_back (b13 ? 20 : 21);
        else if (b13)
            out.push_back (20);

        std::sort (out.begin(), out.end());
        out.erase (std::unique (out.begin(), out.end()), out.end());
        return true;
    }

    juce::String symbolOf (const Chord& c)
    {
        juce::String s = juce::String (pcNames[c.root]) + c.quality;
        if (c.bass >= 0)
            s << "/" << pcNames[c.bass];
        return s;
    }

    void setQuality (Chord& c, const juce::String& q)
    {
        std::vector<int> iv;
        if (intervalsFromQuality (q, iv))
        {
            c.intervals = iv;
            c.quality = q;
        }
    }

    //==========================================================================
    // re-harmonisation

    enum class Kind { Dominant, Minor, Major, Other };

    Kind kindOf (const std::vector<int>& iv)
    {
        if (has (iv, 4) && has (iv, 10))                      return Kind::Dominant;
        if (has (iv, 3) && has (iv, 7))                       return Kind::Minor;
        if (has (iv, 4) && has (iv, 7) && ! has (iv, 10))     return Kind::Major;
        return Kind::Other;
    }

    std::vector<juce::String> options (Style s, Kind k)
    {
        if (k == Kind::Dominant)
        {
            if (s == Style::Soul) return { "7", "9", "13" };
            if (s == Style::Jazz) return { "9", "13", "7b9", "7#9", "7#11", "7alt" };
            if (s == Style::Funk) return { "9", "7#9", "13", "7" };
            return { "9", "13", "7#9", "7" };
        }

        if (k == Kind::Minor)
        {
            if (s == Style::Soul) return { "m7", "m9", "m7", "m11" };
            if (s == Style::Jazz) return { "m7", "m9", "m11", "m6" };
            if (s == Style::Funk) return { "m7", "m9", "m11", "m7" };
            return { "m9", "m11", "m7", "madd9" };
        }

        if (k == Kind::Major)
        {
            if (s == Style::Soul) return { "maj7", "maj9", "6", "add9", "maj7" };
            if (s == Style::Jazz) return { "maj7", "maj9", "6", "69", "maj7#11", "maj13" };
            if (s == Style::Funk) return { "6", "maj7", "maj9", "add9" };
            return { "maj9", "maj7", "add9", "69", "maj13" };
        }

        return {};
    }

    juce::String pick (const std::vector<juce::String>& v, juce::Random& rng)
    {
        return v[(size_t) rng.nextInt ((int) v.size())];
    }

    void reharmonise (Chord& c, const Settings& s, juce::Random& rng)
    {
        Kind k = kindOf (c.intervals);

        // harmonic substitutions: tritone sub, relative major/minor swaps
        if (rng.nextFloat() < 0.3f * s.chaos)
        {
            if (k == Kind::Dominant)
            {
                c.root = (c.root + 6) % 12;
                c.bass = -1;
            }
            else if (k == Kind::Major)
            {
                c.root = (c.root + 9) % 12;
                c.bass = -1;
                setQuality (c, "m7");
                k = Kind::Minor;
            }
            else if (k == Kind::Minor)
            {
                c.root = (c.root + 3) % 12;
                c.bass = -1;
                setQuality (c, "maj7");
                k = Kind::Major;
            }
        }

        // style flavoured extensions / colours
        if (k != Kind::Other && rng.nextFloat() < 0.5f + 0.4f * s.chaos)
        {
            const auto opts = options (s.style, k);
            if (! opts.empty())
                setQuality (c, pick (opts, rng));
        }
    }

    //==========================================================================
    // voicing

    double styleCenter (Style s)
    {
        if (s == Style::Jazz) return 64.0;
        if (s == Style::Funk) return 66.0;
        if (s == Style::RnB)  return 61.0;
        return 62.0;
    }

    std::vector<int> voice (const Chord& c, const Settings& s, juce::Random& rng, double& prevCenter)
    {
        std::vector<int> iv = c.intervals;

        const float dropFifth = (s.style == Style::Soul) ? 0.3f : 0.8f;
        if (iv.size() >= 4 && has (iv, 7) && rng.nextFloat() < dropFifth)
            removeValue (iv, 7);

        const float dropRoot = (s.style == Style::Jazz) ? 0.6f : (s.style == Style::Funk ? 0.4f : 0.0f);
        if (s.addBass && iv.size() >= 4 && rng.nextFloat() < dropRoot)
            removeValue (iv, 0);

        const int n = (int) iv.size();
        const double center = styleCenter (s.style);

        double bestCost = 1.0e9;
        std::vector<int> best;
        double bestMean = center;

        for (int oct = 3; oct <= 5; ++oct)
        {
            for (int inv = 0; inv < n; ++inv)
            {
                for (int drop = 0; drop < 2; ++drop)
                {
                    if (drop == 1 && n < 4)
                        continue;

                    std::vector<int> notes;
                    for (int x : iv)
                        notes.push_back (12 * oct + c.root + x);
                    std::sort (notes.begin(), notes.end());

                    for (int k = 0; k < inv; ++k)
                    {
                        notes.front() += 12;
                        std::sort (notes.begin(), notes.end());
                    }

                    if (drop == 1)
                    {
                        notes[notes.size() - 2] -= 12;
                        std::sort (notes.begin(), notes.end());
                    }

                    const int lo = notes.front();
                    const int hi = notes.back();
                    double sum = 0.0;
                    for (int x : notes)
                        sum += x;
                    const double mean = sum / (double) notes.size();

                    double cost = std::abs (mean - prevCenter) + 0.15 * std::abs (mean - center);
                    if (hi - lo > 15) cost += 3.0;
                    if (lo < 48)      cost += 20.0;
                    if (hi > 84)      cost += 20.0;
                    cost += (double) rng.nextFloat() * (1.0 + 6.0 * (double) s.chaos);

                    if (cost < bestCost)
                    {
                        bestCost = cost;
                        best = notes;
                        bestMean = mean;
                    }
                }
            }
        }

        prevCenter = bestMean;
        return best;
    }

    //==========================================================================
    // rhythms (patterns are written for one 4-beat bar)

    struct Hit { double s; double l; float v; };
    using Pattern = std::vector<Hit>;

    Pattern makePattern (std::initializer_list<Hit> h) { return Pattern (h); }

    const std::vector<Pattern>& patternsFor (Style st)
    {
        static const std::vector<Pattern> soul = {
            makePattern ({ { 0, 4, 1.0f } }),
            makePattern ({ { 0, 2, 1.0f }, { 2, 2, 0.85f } }),
            makePattern ({ { 0, 2.5, 1.0f }, { 2.5, 1.5, 0.9f } }),
            makePattern ({ { 0, 1, 1.0f }, { 1.5, 0.5, 0.7f }, { 2, 2, 0.9f } }),
            makePattern ({ { 0, 1.75, 1.0f }, { 1.75, 0.75, 0.8f }, { 3.5, 0.5, 0.75f } })
        };

        static const std::vector<Pattern> jazz = {
            makePattern ({ { 0, 1.5, 0.95f }, { 1.5, 0.5, 0.8f } }),
            makePattern ({ { 1.5, 1.0, 0.9f }, { 3.5, 0.5, 0.8f } }),
            makePattern ({ { 0, 0.5, 0.8f }, { 2.5, 1.0, 0.95f } }),
            makePattern ({ { 0, 2.0, 0.9f }, { 2.5, 0.5, 0.8f }, { 3.5, 0.5, 0.75f } }),
            makePattern ({ { 0.5, 1.0, 0.85f }, { 2.5, 0.5, 0.8f }, { 3.0, 1.0, 0.9f } })
        };

        static const std::vector<Pattern> funk = {
            makePattern ({ { 0, 0.25, 1.0f }, { 0.75, 0.25, 0.7f }, { 1.5, 0.25, 0.9f },
                           { 2.5, 0.25, 0.7f }, { 3, 0.25, 0.95f }, { 3.5, 0.25, 0.65f } }),
            makePattern ({ { 0, 0.25, 1.0f }, { 0.5, 0.25, 0.6f }, { 1.5, 0.25, 0.9f },
                           { 2, 0.25, 0.95f }, { 3.25, 0.25, 0.7f } }),
            makePattern ({ { 0.5, 0.25, 0.85f }, { 1, 0.25, 0.95f }, { 1.75, 0.25, 0.7f },
                           { 2.5, 0.25, 0.9f }, { 3.5, 0.25, 0.8f } }),
            makePattern ({ { 0, 0.5, 1.0f }, { 1.5, 0.25, 0.8f }, { 2.25, 0.25, 0.7f }, { 3, 0.5, 0.95f } })
        };

        static const std::vector<Pattern> rnb = {
            makePattern ({ { 0, 4, 1.0f } }),
            makePattern ({ { 0, 1.5, 1.0f }, { 1.5, 2.5, 0.85f } }),
            makePattern ({ { 0, 2, 1.0f }, { 2.5, 1.5, 0.85f } }),
            makePattern ({ { 0, 1, 1.0f }, { 1.5, 0.5, 0.7f }, { 2, 1.5, 0.9f }, { 3.5, 0.5, 0.7f } }),
            makePattern ({ { 0, 3.5, 1.0f }, { 3.5, 0.5, 0.8f } })
        };

        if (st == Style::Jazz) return jazz;
        if (st == Style::Funk) return funk;
        if (st == Style::RnB)  return rnb;
        return soul;
    }

    const std::vector<Pattern>& funkBassPatterns()
    {
        static const std::vector<Pattern> p = {
            makePattern ({ { 0, 0.5, 1.0f }, { 1.5, 0.25, 0.8f }, { 2.5, 0.5, 0.9f } }),
            makePattern ({ { 0, 0.75, 1.0f }, { 2, 0.25, 0.8f }, { 3, 0.5, 0.9f } })
        };
        return p;
    }

    void emit (std::vector<NoteEvent>& out, const std::vector<int>& notes, const Pattern& pat,
               double chordStart, int beatsPerChord, int baseVel, const Settings& s,
               juce::Random& rng, bool rollIt)
    {
        for (int rep = 0; rep * 4 < beatsPerChord; ++rep)
        {
            for (const auto& h : pat)
            {
                const double st = (double) (rep * 4) + h.s;
                if (st >= (double) beatsPerChord)
                    continue;

                const double len = juce::jmin (h.l, (double) beatsPerChord - st);
                const double jitter = ((double) rng.nextFloat() - 0.5) * 0.06 * (double) s.humanize;

                int j = 0;
                for (int note : notes)
                {
                    NoteEvent e;
                    e.note = note;

                    const double roll = rollIt ? (double) j * (0.012 + 0.018 * (double) rng.nextFloat()) : 0.0;
                    e.start = juce::jmax (0.0, chordStart + st + jitter + roll);
                    e.length = juce::jmax (0.08, len - 0.03);

                    const float vr = 1.0f + (rng.nextFloat() - 0.5f) * 0.3f * s.humanize;
                    e.velocity = juce::jlimit (1, 127, (int) std::lround ((double) baseVel * (double) h.v * (double) vr));

                    out.push_back (e);
                    ++j;
                }
            }
        }
    }
}

//==============================================================================
bool parseProgression (const juce::String& text, std::vector<Chord>& out, juce::String& error)
{
    out.clear();
    error.clear();

    const juce::String t = text.replaceCharacters ("|,;\n\r\t", "      ");

    juce::StringArray tokens;
    tokens.addTokens (t, " ", "");

    for (auto token : tokens)
    {
        token = token.trim();
        if (token.isEmpty())
            continue;

        if (token == "%" || token == "/")
        {
            if (! out.empty())
                out.push_back (out.back());
            continue;
        }

        if (token.equalsIgnoreCase ("N.C.") || token.equalsIgnoreCase ("NC"))
            continue;

        int consumed = 0;
        const int root = parseRoot (token, consumed);
        if (root < 0)
        {
            error = "Can't read \"" + token + "\"";
            return false;
        }

        juce::String rest = token.substring (consumed);
        int bass = -1;

        const int slash = rest.indexOfChar ('/');
        if (slash >= 0)
        {
            const juce::String before = rest.substring (0, slash);
            const juce::String after = rest.substring (slash + 1);

            if (after.containsOnly ("0123456789"))
            {
                rest = before + after;              // C6/9
            }
            else
            {
                int c2 = 0;
                const int b = parseRoot (after, c2);
                if (b < 0 || c2 != after.length())
                {
                    error = "Can't read \"" + token + "\"";
                    return false;
                }
                bass = b;
                rest = before;
            }
        }

        Chord c;
        c.root = root;
        c.quality = rest;
        c.bass = bass;

        if (! intervalsFromQuality (rest, c.intervals))
        {
            error = "Don't know the chord \"" + token + "\"";
            return false;
        }

        out.push_back (c);
    }

    if (out.empty())
    {
        error = "Paste some chords first, for example:  Dm7  G7  Cmaj7  Am7";
        return false;
    }

    return true;
}

void generate (const std::vector<Chord>& input, const Settings& s, juce::Random& rng,
               std::vector<NoteEvent>& notes, juce::StringArray& names)
{
    notes.clear();
    names.clear();

    double prevCenter = styleCenter (s.style);
    const auto& patterns = patternsFor (s.style);
    const Pattern wholeNote = makePattern ({ { 0, 4, 1.0f } });

    int baseVel = 88;
    if (s.style == Style::Jazz) baseVel = 80;
    if (s.style == Style::Funk) baseVel = 96;
    if (s.style == Style::RnB)  baseVel = 82;

    const bool roll = (s.style == Style::Soul || s.style == Style::RnB);

    for (size_t i = 0; i < input.size(); ++i)
    {
        Chord c = input[i];
        reharmonise (c, s, rng);
        names.add (symbolOf (c));

        const double start = (double) i * (double) s.beatsPerChord;

        const auto v = voice (c, s, rng, prevCenter);
        const Pattern& pat = patterns[(size_t) rng.nextInt ((int) patterns.size())];
        emit (notes, v, pat, start, s.beatsPerChord, baseVel, s, rng, roll);

        if (s.addBass)
        {
            const int bassPc = c.bass >= 0 ? c.bass : c.root;
            const std::vector<int> b { 36 + bassPc };

            if (s.style == Style::Funk)
            {
                const auto& bp = funkBassPatterns();
                emit (notes, b, bp[(size_t) rng.nextInt ((int) bp.size())], start, s.beatsPerChord, 100, s, rng, false);
            }
            else
            {
                emit (notes, b, wholeNote, start, s.beatsPerChord, 100, s, rng, false);
            }
        }
    }
}

bool writeMidiFile (const std::vector<NoteEvent>& notes, const juce::File& file)
{
    const int ppq = 960;

    juce::MidiMessageSequence seq;
    seq.addEvent (juce::MidiMessage::tempoMetaEvent (500000), 0.0);

    for (const auto& n : notes)
    {
        seq.addEvent (juce::MidiMessage::noteOn (1, n.note, (juce::uint8) n.velocity), n.start * ppq);
        seq.addEvent (juce::MidiMessage::noteOff (1, n.note), (n.start + n.length) * ppq);
    }

    seq.sort();
    seq.updateMatchedPairs();

    juce::MidiFile mf;
    mf.setTicksPerQuarterNote (ppq);
    mf.addTrack (seq);

    file.deleteFile();
    juce::FileOutputStream out (file);
    if (! out.openedOk())
        return false;

    return mf.writeTo (out);
}

} // namespace ChordEngine
