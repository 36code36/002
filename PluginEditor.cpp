#include "PluginEditor.h"

ChordForgeEditor::ChordForgeEditor (ChordForgeProcessor& p)
    : juce::AudioProcessorEditor (&p), proc (p)
{
    // chord input
    chordInput.setMultiLine (true, true);
    chordInput.setReturnKeyStartsNewLine (true);
    chordInput.setScrollbarsShown (true);
    chordInput.setFont (juce::Font (juce::FontOptions (18.0f)));
    chordInput.setTextToShowWhenEmpty ("Paste your chords here, e.g.   Dm7  G7  Cmaj7  Am7", juce::Colours::grey);
    chordInput.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff262a31));
    chordInput.setColour (juce::TextEditor::textColourId, juce::Colours::white);
    chordInput.setColour (juce::TextEditor::outlineColourId, juce::Colour (0xff4a505b));
    chordInput.setText (proc.getChordText(), false);
    chordInput.onTextChange = [this] { proc.setChordText (chordInput.getText()); };
    addAndMakeVisible (chordInput);

    // style + beats
    styleBox.addItemList (juce::StringArray { "Soul", "Jazz", "Funk", "R&B" }, 1);
    beatsBox.addItemList (juce::StringArray { "1", "2", "4", "8" }, 1);
    addAndMakeVisible (styleBox);
    addAndMakeVisible (beatsBox);

    styleLabel.setText ("Style", juce::dontSendNotification);
    beatsLabel.setText ("Beats / chord", juce::dontSendNotification);
    addAndMakeVisible (styleLabel);
    addAndMakeVisible (beatsLabel);

    styleAtt = std::make_unique<ComboAtt> (proc.apvts, "style", styleBox);
    beatsAtt = std::make_unique<ComboAtt> (proc.apvts, "beats", beatsBox);

    addAndMakeVisible (bassToggle);
    addAndMakeVisible (soundToggle);
    bassAtt  = std::make_unique<ButtonAtt> (proc.apvts, "bass", bassToggle);
    soundAtt = std::make_unique<ButtonAtt> (proc.apvts, "sound", soundToggle);

    // sliders
    setupSlider (chaosSlider,    chaosLabel,    "Chaos");
    setupSlider (humanizeSlider, humanizeLabel, "Humanize");
    setupSlider (tempoSlider,    tempoLabel,    "Preview tempo");
    chaosAtt    = std::make_unique<SliderAtt> (proc.apvts, "chaos", chaosSlider);
    humanizeAtt = std::make_unique<SliderAtt> (proc.apvts, "humanize", humanizeSlider);
    tempoAtt    = std::make_unique<SliderAtt> (proc.apvts, "tempo", tempoSlider);

    // buttons
    addAndMakeVisible (randomizeButton);
    addAndMakeVisible (playButton);
    addAndMakeVisible (saveButton);
    randomizeButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff6a4cff));

    randomizeButton.onClick = [this]
    {
        proc.stopPreview();
        proc.randomize (chordInput.getText());
        repaint();
    };

    playButton.onClick = [this]
    {
        if (proc.isPreviewing())
        {
            proc.stopPreview();
        }
        else
        {
            if (proc.getResult() == nullptr)
                proc.randomize (chordInput.getText());
            proc.startPreview();
        }
    };

    saveButton.onClick = [this]
    {
        if (proc.getResult() == nullptr)
            proc.randomize (chordInput.getText());

        chooser = std::make_unique<juce::FileChooser> (
            "Save MIDI file",
            juce::File::getSpecialLocation (juce::File::userDesktopDirectory).getChildFile ("ChordForge.mid"),
            "*.mid");

        juce::Component::SafePointer<ChordForgeEditor> safe (this);
        chooser->launchAsync (juce::FileBrowserComponent::saveMode
                                  | juce::FileBrowserComponent::canSelectFiles
                                  | juce::FileBrowserComponent::warnAboutOverwriting,
                              [safe] (const juce::FileChooser& fc)
                              {
                                  if (safe == nullptr)
                                      return;
                                  const auto f = fc.getResult();
                                  if (f == juce::File())
                                      return;
                                  safe->proc.exportMidi (f.withFileExtension ("mid"));
                              });
    };

    addAndMakeVisible (dragChip);
    dragChip.getFile = [this] { return proc.makeMidiFile(); };

    setSize (660, 540);
    startTimerHz (10);
}

ChordForgeEditor::~ChordForgeEditor()
{
    stopTimer();
}

void ChordForgeEditor::setupSlider (juce::Slider& s, juce::Label& l, const juce::String& name)
{
    s.setSliderStyle (juce::Slider::LinearHorizontal);
    s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 70, 22);
    addAndMakeVisible (s);

    l.setText (name, juce::dontSendNotification);
    addAndMakeVisible (l);
}

void ChordForgeEditor::timerCallback()
{
    playButton.setButtonText (proc.isPreviewing() ? "Stop" : "Play preview");

    if (proc.resultVersion.load() != lastVersion)
    {
        lastVersion = proc.resultVersion.load();
        repaint();
    }
}

void ChordForgeEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1b1d22));

    g.setColour (juce::Colours::white);
    g.setFont (juce::Font (juce::FontOptions (24.0f)));
    g.drawText ("Chord Forge", 16, 8, 300, 32, juce::Justification::centredLeft);

    g.setColour (juce::Colour (0xff262a31));
    g.fillRoundedRectangle (resultArea.toFloat(), 8.0f);

    juce::String text;
    juce::Colour col = juce::Colour (0xffffd166);

    if (proc.lastError.isNotEmpty())
    {
        text = proc.lastError;
        col = juce::Colour (0xffff6b6b);
    }
    else if (auto r = proc.getResult())
    {
        text = r->styleName + ":   " + r->names.joinIntoString ("   |   ");
    }
    else
    {
        text = "Paste chords above, then press RANDOMIZE.";
        col = juce::Colours::lightgrey;
    }

    g.setColour (col);
    g.setFont (juce::Font (juce::FontOptions (18.0f)));
    g.drawFittedText (text, resultArea.reduced (12, 8), juce::Justification::topLeft, 4);
}

void ChordForgeEditor::resized()
{
    auto area = getLocalBounds().reduced (16);
    area.removeFromTop (36); // title

    chordInput.setBounds (area.removeFromTop (80));
    area.removeFromTop (10);

    auto row = area.removeFromTop (28);
    styleLabel.setBounds (row.removeFromLeft (50));
    styleBox.setBounds (row.removeFromLeft (110));
    row.removeFromLeft (16);
    beatsLabel.setBounds (row.removeFromLeft (90));
    beatsBox.setBounds (row.removeFromLeft (70));
    row.removeFromLeft (16);
    bassToggle.setBounds (row.removeFromLeft (100));
    soundToggle.setBounds (row.removeFromLeft (120));
    area.removeFromTop (10);

    auto place = [&area] (juce::Label& l, juce::Slider& s)
    {
        auto r = area.removeFromTop (28);
        l.setBounds (r.removeFromLeft (110));
        s.setBounds (r);
    };

    place (chaosLabel, chaosSlider);
    place (humanizeLabel, humanizeSlider);
    place (tempoLabel, tempoSlider);
    area.removeFromTop (10);

    auto buttons = area.removeFromTop (46);
    randomizeButton.setBounds (buttons.removeFromLeft (220));
    buttons.removeFromLeft (10);
    playButton.setBounds (buttons.removeFromLeft (150));
    buttons.removeFromLeft (10);
    saveButton.setBounds (buttons.removeFromLeft (140));
    area.removeFromTop (10);

    dragChip.setBounds (area.removeFromBottom (48));
    area.removeFromBottom (10);

    resultArea = area;
}
