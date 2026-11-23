#pragma once
#include "PluginProcessor.h"
#include <functional>

// A small button-like area; dragging from it drags the generated .mid file out of the plugin window.
class DragChip : public juce::Component
{
public:
    DragChip() { setMouseCursor (juce::MouseCursor::DraggingHandCursor); }

    std::function<juce::File()> getFile;

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (juce::Colour (0xff3a2f59));
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour (juce::Colour (0xff8f7cff));
        g.drawRoundedRectangle (r, 8.0f, 1.5f);
        g.setColour (juce::Colours::white);
        g.setFont (juce::Font (juce::FontOptions (16.0f)));
        g.drawText ("Drag this MIDI into the FL Studio piano roll", getLocalBounds(), juce::Justification::centred);
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (e.getDistanceFromDragStart() < 8 || getFile == nullptr)
            return;

        const auto f = getFile();
        if (f.existsAsFile())
            juce::DragAndDropContainer::performExternalDragDropOfFiles (juce::StringArray (f.getFullPathName()), false, this);
    }
};

class ChordForgeEditor : public juce::AudioProcessorEditor,
                         private juce::Timer
{
public:
    explicit ChordForgeEditor (ChordForgeProcessor&);
    ~ChordForgeEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboAtt  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    void timerCallback() override;
    void setupSlider (juce::Slider&, juce::Label&, const juce::String& name);

    ChordForgeProcessor& proc;
    int lastVersion = -1;
    juce::Rectangle<int> resultArea;

    juce::TextEditor chordInput;
    juce::ComboBox styleBox, beatsBox;
    juce::Label styleLabel, beatsLabel;
    juce::ToggleButton bassToggle { "Bass note" }, soundToggle { "Preview sound" };

    juce::Slider chaosSlider, humanizeSlider, tempoSlider;
    juce::Label chaosLabel, humanizeLabel, tempoLabel;

    juce::TextButton randomizeButton { "RANDOMIZE" }, playButton { "Play preview" }, saveButton { "Save .mid..." };
    DragChip dragChip;

    std::unique_ptr<ComboAtt> styleAtt, beatsAtt;
    std::unique_ptr<SliderAtt> chaosAtt, humanizeAtt, tempoAtt;
    std::unique_ptr<ButtonAtt> bassAtt, soundAtt;
    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChordForgeEditor)
};
