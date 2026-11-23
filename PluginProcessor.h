#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>
#include <atomic>
#include <vector>
#include "ChordEngine.h"

struct ChordResult
{
    std::vector<ChordEngine::NoteEvent> notes;
    juce::StringArray names;       // the chords after randomising, for display
    juce::String styleName;
    double totalBeats = 0.0;
};

class ChordForgeProcessor : public juce::AudioProcessor
{
public:
    ChordForgeProcessor();
    ~ChordForgeProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Chord Forge"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // Parses the text, randomises it in the chosen style and stores the result.
    bool randomize (const juce::String& chordText);

    std::shared_ptr<const ChordResult> getResult() const
    {
        const juce::SpinLock::ScopedLockType l (resultLock);
        return result;
    }

    juce::File makeMidiFile();                 // writes the current result to a temp .mid
    bool exportMidi (const juce::File& file);  // writes the current result to a chosen file

    void startPreview();
    void stopPreview();
    bool isPreviewing() const { return playing.load(); }

    juce::String getChordText() const;
    void setChordText (const juce::String& text);

    juce::AudioProcessorValueTreeState apvts;
    std::atomic<int> resultVersion { 0 };
    juce::String lastError;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    ChordEngine::Settings currentSettings() const;

    struct Ev { juce::int64 sample; bool on; int note; int vel; };

    juce::Synthesiser synth;
    juce::Random rng;
    double sampleRate = 44100.0;

    mutable juce::SpinLock resultLock;
    std::shared_ptr<const ChordResult> result;

    juce::SpinLock previewLock;
    std::vector<Ev> events;
    int nextEvent = 0;
    juce::int64 playPos = 0;
    std::atomic<bool> playing { false };
    std::atomic<bool> flushRequested { false };

    std::atomic<float> *styleP = nullptr, *beatsP = nullptr, *chaosP = nullptr, *humanizeP = nullptr,
                       *bassP = nullptr, *soundP = nullptr, *tempoP = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChordForgeProcessor)
};
