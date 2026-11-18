#pragma once
#include "PluginProcessor.h"

class Acoustic260Editor : public juce::AudioProcessorEditor
{
public:
    explicit Acoustic260Editor (Acoustic260Processor&);
    ~Acoustic260Editor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboAtt  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<SliderAtt> att;
    };
    struct Switch
    {
        juce::ToggleButton button;
        std::unique_ptr<ButtonAtt> att;
    };

    void addKnob (const char* id, const char* text, bool dB = false);
    void addSwitch (const char* id, const char* text);

    Acoustic260Processor& processor;
    std::vector<std::unique_ptr<Knob>> knobs;
    std::vector<std::unique_ptr<Switch>> switches;
    juce::ComboBox variampRange;
    juce::Label variampLabel;
    std::unique_ptr<ComboAtt> variampAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Acoustic260Editor)
};
