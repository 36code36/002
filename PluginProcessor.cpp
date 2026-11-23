#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <algorithm>
#include <cmath>

namespace
{
    const char* const styleNames[4] = { "Soul", "Jazz", "Funk", "RnB" };
    const int beatOptions[4] = { 1, 2, 4, 8 };

    class PianoSound : public juce::SynthesiserSound
    {
    public:
        bool appliesToNote (int) override    { return true; }
        bool appliesToChannel (int) override { return true; }
    };

    // very small electric-piano-ish preview voice
    class PianoVoice : public juce::SynthesiserVoice
    {
    public:
        bool canPlaySound (juce::SynthesiserSound* s) override
        {
            return dynamic_cast<PianoSound*> (s) != nullptr;
        }

        void startNote (int midiNote, float velocity, juce::SynthesiserSound*, int) override
        {
            const double sr = getSampleRate();
            const double freq = 440.0 * std::pow (2.0, (midiNote - 69) / 12.0);

            phaseDelta = juce::MathConstants<double>::twoPi * freq / sr;
            phase = 0.0;
            level = velocity * 0.14f;
            env = 1.0f;
            atk = 0.0f;
            atkInc = (float) (1.0 / (0.004 * sr));
            decay = (float) std::exp (-1.0 / (1.4 * sr));
            relDecay = (float) std::exp (-1.0 / (0.12 * sr));
            releasing = false;
            active = true;
        }

        void stopNote (float, bool allowTailOff) override
        {
            if (allowTailOff)
            {
                releasing = true;
            }
            else
            {
                active = false;
                clearCurrentNote();
            }
        }

        void pitchWheelMoved (int) override {}
        void controllerMoved (int, int) override {}

        void renderNextBlock (juce::AudioBuffer<float>& out, int startSample, int numSamples) override
        {
            if (! active)
                return;

            for (int i = 0; i < numSamples; ++i)
            {
                const double p = phase;
                const float s = (float) (std::sin (p) + 0.45 * std::sin (2.0 * p) + 0.2 * std::sin (3.0 * p))
                                * level * env * atk;

                for (int ch = 0; ch < out.getNumChannels(); ++ch)
                    out.addSample (ch, startSample + i, s);

                phase += phaseDelta;
                if (phase > juce::MathConstants<double>::twoPi)
                    phase -= juce::MathConstants<double>::twoPi;

                env *= decay;
                if (releasing)
                    env *= relDecay;
                if (atk < 1.0f)
                    atk = juce::jmin (1.0f, atk + atkInc);

                if (env < 0.0003f)
                {
                    active = false;
                    clearCurrentNote();
                    break;
                }
            }
        }

    private:
        double phase = 0.0, phaseDelta = 0.0;
        float level = 0.0f, env = 0.0f, atk = 1.0f, atkInc = 0.01f, decay = 0.9999f, relDecay = 0.99f;
        bool releasing = false, active = false;
    };
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout ChordForgeProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;

    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "style", 1 }, "Style",
                                                   StringArray { "Soul", "Jazz", "Funk", "R&B" }, 0));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "beats", 1 }, "Beats per chord",
                                                   StringArray { "1", "2", "4", "8" }, 2));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "chaos", 1 }, "Chaos",
                                                  NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "humanize", 1 }, "Humanize",
                                                  NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.3f));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { "bass", 1 }, "Bass note", true));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { "sound", 1 }, "Preview sound", true));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "tempo", 1 }, "Preview tempo",
                                                  NormalisableRange<float> (60.0f, 200.0f, 1.0f), 100.0f));
    return l;
}

ChordForgeProcessor::ChordForgeProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    styleP    = apvts.getRawParameterValue ("style");
    beatsP    = apvts.getRawParameterValue ("beats");
    chaosP    = apvts.getRawParameterValue ("chaos");
    humanizeP = apvts.getRawParameterValue ("humanize");
    bassP     = apvts.getRawParameterValue ("bass");
    soundP    = apvts.getRawParameterValue ("sound");
    tempoP    = apvts.getRawParameterValue ("tempo");

    for (int i = 0; i < 24; ++i)
        synth.addVoice (new PianoVoice());
    synth.addSound (new PianoSound());
}

void ChordForgeProcessor::prepareToPlay (double sr, int)
{
    sampleRate = sr;
    synth.setCurrentPlaybackSampleRate (sr);
}

bool ChordForgeProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void ChordForgeProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    const int num = buffer.getNumSamples();
    juce::MidiBuffer generated;

    if (flushRequested.exchange (false))
    {
        generated.addEvent (juce::MidiMessage::allNotesOff (1), 0);
        synth.allNotesOff (0, true);
    }

    if (playing.load())
    {
        const juce::SpinLock::ScopedTryLockType tl (previewLock);
        if (tl.isLocked())
        {
            while (nextEvent < (int) events.size() && events[(size_t) nextEvent].sample < playPos + num)
            {
                const auto& e = events[(size_t) nextEvent];
                const int offset = (int) juce::jlimit<juce::int64> (0, (juce::int64) num - 1, e.sample - playPos);

                if (e.on)
                    generated.addEvent (juce::MidiMessage::noteOn (1, e.note, (juce::uint8) e.vel), offset);
                else
                    generated.addEvent (juce::MidiMessage::noteOff (1, e.note), offset);

                ++nextEvent;
            }

            playPos += num;
            if (nextEvent >= (int) events.size())
                playing.store (false);
        }
    }

    if (soundP->load() > 0.5f)
        synth.renderNextBlock (buffer, generated, 0, num);

    midiMessages.clear();
    midiMessages.addEvents (generated, 0, num, 0);
}

//==============================================================================
ChordEngine::Settings ChordForgeProcessor::currentSettings() const
{
    ChordEngine::Settings st;
    st.style = (ChordEngine::Style) juce::jlimit (0, 3, (int) styleP->load());
    st.beatsPerChord = beatOptions[juce::jlimit (0, 3, (int) beatsP->load())];
    st.chaos = chaosP->load();
    st.humanize = humanizeP->load();
    st.addBass = bassP->load() > 0.5f;
    return st;
}

bool ChordForgeProcessor::randomize (const juce::String& chordText)
{
    setChordText (chordText);

    std::vector<ChordEngine::Chord> chords;
    juce::String err;

    if (! ChordEngine::parseProgression (chordText, chords, err))
    {
        lastError = err;
        resultVersion++;
        return false;
    }

    const auto st = currentSettings();

    auto r = std::make_shared<ChordResult>();
    ChordEngine::generate (chords, st, rng, r->notes, r->names);
    r->totalBeats = (double) chords.size() * (double) st.beatsPerChord;
    r->styleName = styleNames[(int) st.style];

    {
        const juce::SpinLock::ScopedLockType l (resultLock);
        result = r;
    }

    lastError.clear();
    resultVersion++;
    return true;
}

juce::File ChordForgeProcessor::makeMidiFile()
{
    auto r = getResult();
    if (r == nullptr || r->notes.empty())
        return {};

    auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("ChordForge");
    dir.createDirectory();

    auto f = dir.getChildFile ("ChordForge " + r->styleName + ".mid");
    return ChordEngine::writeMidiFile (r->notes, f) ? f : juce::File();
}

bool ChordForgeProcessor::exportMidi (const juce::File& file)
{
    auto r = getResult();
    if (r == nullptr || r->notes.empty())
        return false;

    return ChordEngine::writeMidiFile (r->notes, file);
}

void ChordForgeProcessor::startPreview()
{
    auto r = getResult();
    if (r == nullptr || r->notes.empty())
        return;

    playing.store (false);

    const double spb = sampleRate * 60.0 / (double) tempoP->load();

    std::vector<Ev> ev;
    ev.reserve (r->notes.size() * 2);

    for (const auto& n : r->notes)
    {
        ev.push_back ({ (juce::int64) std::llround (n.start * spb), true, n.note, n.velocity });
        ev.push_back ({ (juce::int64) std::llround ((n.start + n.length) * spb), false, n.note, 0 });
    }

    std::sort (ev.begin(), ev.end(), [] (const Ev& a, const Ev& b)
    {
        if (a.sample != b.sample)
            return a.sample < b.sample;
        return a.on < b.on;   // note-offs first
    });

    {
        const juce::SpinLock::ScopedLockType l (previewLock);
        events = std::move (ev);
        nextEvent = 0;
        playPos = 0;
    }

    flushRequested.store (true);
    playing.store (true);
}

void ChordForgeProcessor::stopPreview()
{
    playing.store (false);
    flushRequested.store (true);
}

juce::String ChordForgeProcessor::getChordText() const
{
    return apvts.state.getProperty ("chordText").toString();
}

void ChordForgeProcessor::setChordText (const juce::String& text)
{
    apvts.state.setProperty ("chordText", text, nullptr);
}

//==============================================================================
juce::AudioProcessorEditor* ChordForgeProcessor::createEditor()
{
    return new ChordForgeEditor (*this);
}

void ChordForgeProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

void ChordForgeProcessor::setStateInformation (const void* data, int size)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, size));
    if (xml != nullptr && xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ChordForgeProcessor();
}
