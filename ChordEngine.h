#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>

namespace ChordEngine
{
    enum class Style { Soul = 0, Jazz, Funk, RnB };

    struct Chord
    {
        int root = 0;                 // 0 = C ... 11 = B
        juce::String quality;         // as typed, e.g. "m7", "maj9", "7#9"
        std::vector<int> intervals;   // semitones above the root (9th = 14, 13th = 21 ...)
        int bass = -1;                // slash-chord bass pitch class, or -1
    };

    struct NoteEvent
    {
        int note = 60;
        double start = 0.0;    // in beats
        double length = 1.0;   // in beats
        int velocity = 90;
    };

    struct Settings
    {
        Style style = Style::Soul;
        int beatsPerChord = 4;
        float chaos = 0.5f;      // 0..1  how far the chords may wander
        float humanize = 0.3f;   // 0..1  timing / velocity looseness
        bool addBass = true;
    };

    // Reads text like "Dm7 G7 | Cmaj7 Am7/E  C6/9  %". Returns false and fills error on failure.
    bool parseProgression (const juce::String& text, std::vector<Chord>& out, juce::String& error);

    // Randomly re-harmonises, voices and rhythmicises the progression in the chosen style.
    void generate (const std::vector<Chord>& input, const Settings& settings, juce::Random& rng,
                   std::vector<NoteEvent>& notes, juce::StringArray& chordNames);

    // Writes the notes as a standard MIDI file (960 ppq, channel 1).
    bool writeMidiFile (const std::vector<NoteEvent>& notes, const juce::File& file);
}
