#include "PluginEditor.h"

namespace
{
    const juce::Colour kPanelBlack (0xff141414);
    const juce::Colour kStripeBlue (0xff2f6db5);
    const juce::Colour kSilver     (0xffc9ccd1);
}

Acoustic260Editor::Acoustic260Editor (Acoustic260Processor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    // Row 1: preamp. Row 2: effects. Row 3: power amp / output
    addKnob ("trim",    "INPUT TRIM", true);
    addKnob ("volume",  "VOLUME");
    addKnob ("bass",    "BASS");
    addKnob ("treble",  "TREBLE");
    addKnob ("veffect", "VARIAMP");
    addKnob ("level",   "CH LEVEL");

    addKnob ("fattack", "FUZZ ATTACK");
    addKnob ("fgain",   "FUZZ GAIN");
    addKnob ("reverb",  "REVERB");
    addKnob ("tspeed",  "TREM SPEED");
    addKnob ("tint",    "TREM INTENSITY");

    addKnob ("drive",   "POWER DRIVE", true);
    addKnob ("headroom","CLIPPING ADJ");
    addKnob ("horn",    "HORN");
    addKnob ("out",     "OUTPUT", true);

    addSwitch ("bright", "BRIGHT");
    addSwitch ("fuzz",   "FUZZ ON");
    addSwitch ("cab",    "261 CAB");

    variampRange.addItemList ({ "Off", "306 Hz", "599 Hz", "1.2 kHz", "2.5 kHz" }, 1);
    addAndMakeVisible (variampRange);
    variampAtt = std::make_unique<ComboAtt> (processor.apvts, "vrange", variampRange);
    variampLabel.setText ("VARIAMP RANGE", juce::dontSendNotification);
    variampLabel.setJustificationType (juce::Justification::centred);
    variampLabel.setColour (juce::Label::textColourId, kSilver);
    variampLabel.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    addAndMakeVisible (variampLabel);

    setSize (720, 470);
}

void Acoustic260Editor::addKnob (const char* id, const char* text, bool dB)
{
    auto k = std::make_unique<Knob>();
    k->slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 64, 16);
    k->slider.setColour (juce::Slider::rotarySliderFillColourId, kStripeBlue);
    k->slider.setColour (juce::Slider::thumbColourId, kSilver);
    k->slider.setColour (juce::Slider::textBoxTextColourId, kSilver);
    k->slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    if (dB) k->slider.setTextValueSuffix (" dB");
    k->label.setText (text, juce::dontSendNotification);
    k->label.setJustificationType (juce::Justification::centred);
    k->label.setColour (juce::Label::textColourId, kSilver);
    k->label.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    addAndMakeVisible (k->slider);
    addAndMakeVisible (k->label);
    k->att = std::make_unique<SliderAtt> (processor.apvts, id, k->slider);
    knobs.push_back (std::move (k));
}

void Acoustic260Editor::addSwitch (const char* id, const char* text)
{
    auto s = std::make_unique<Switch>();
    s->button.setButtonText (text);
    s->button.setColour (juce::ToggleButton::textColourId, kSilver);
    s->button.setColour (juce::ToggleButton::tickColourId, kStripeBlue);
    addAndMakeVisible (s->button);
    s->att = std::make_unique<ButtonAtt> (processor.apvts, id, s->button);
    switches.push_back (std::move (s));
}

void Acoustic260Editor::paint (juce::Graphics& g)
{
    g.fillAll (kPanelBlack);
    g.setColour (kStripeBlue);
    g.fillRect (0, 52, getWidth(), 6);
    g.setColour (kSilver);
    g.setFont (juce::FontOptions (24.0f, juce::Font::bold));
    g.drawText ("ACOUSTIC  260  V2", 20, 8, 300, 36, juce::Justification::centredLeft);
    g.setFont (juce::FontOptions (11.0f));
    g.drawText ("V2 - behavioural model of the 260/160 schematic", getWidth() - 340, 12, 320, 24,
                juce::Justification::centredRight);
}

void Acoustic260Editor::resized()
{
    auto area = getLocalBounds().withTrimmedTop (66).reduced (14, 6);
    const int rowH = area.getHeight() / 3;

    auto placeRow = [&] (juce::Rectangle<int> row, int firstKnob, int count)
    {
        const int w = row.getWidth() / 6;
        for (int i = 0; i < count; ++i)
        {
            auto cell = row.removeFromLeft (w);
            auto& k = *knobs[(size_t) (firstKnob + i)];
            k.label.setBounds (cell.removeFromTop (16));
            k.slider.setBounds (cell);
        }
    };

    auto r1 = area.removeFromTop (rowH);
    placeRow (r1, 0, 6);                                    // trim, volume, bass, treble, variamp, level

    auto r2 = area.removeFromTop (rowH);
    placeRow (r2, 6, 5);                                    // fuzz atk, fuzz gain, reverb, trem speed, trem int
    r2.removeFromLeft (r2.getWidth() / 6 * 5);

    auto r3 = area;
    placeRow (r3, 11, 4);                                   // drive, clipping, horn, out
    {
        auto sw = r3;
        sw.removeFromLeft (r3.getWidth() / 6 * 4);
        auto col = sw;
        const int h = col.getHeight() / 3;
        switches[0]->button.setBounds (col.removeFromTop (h).reduced (4));
        switches[1]->button.setBounds (col.removeFromTop (h).reduced (4));
        switches[2]->button.setBounds (col.removeFromTop (h).reduced (4));
    }
    // Variamp range selector sits in the free column of row 2
    {
        const int w = (getWidth() - 28) / 6;
        const int x = 14 + w * 5;
        const int y = 66 + rowH + 6;
        variampLabel.setBounds (x, y + 14, w, 14);
        variampRange.setBounds (x + 8, y + 36, w - 16, 24);
    }
}
