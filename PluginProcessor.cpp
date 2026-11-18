#include "PluginProcessor.h"
#include "PluginEditor.h"

using APVTS = juce::AudioProcessorValueTreeState;

APVTS::ParameterLayout Acoustic260Processor::createLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;

    auto knob = [&p] (const char* id, const char* name, float def)
    {
        p.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name,
                     juce::NormalisableRange<float> (0.0f, 10.0f, 0.01f), def));
    };
    auto dbParam = [&p] (const char* id, const char* name, float lo, float hi, float def)
    {
        p.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name,
                     juce::NormalisableRange<float> (lo, hi, 0.1f), def));
    };
    auto sw = [&p] (const char* id, const char* name, bool def)
    {
        p.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id, 1 }, name, def));
    };

    dbParam ("trim",    "Input Trim (dB)", -24.0f, 24.0f, 0.0f);
    knob    ("volume",  "Volume",          5.0f);
    sw      ("bright",  "Bright",          false);
    knob    ("bass",    "Bass",            5.0f);
    knob    ("treble",  "Treble",          5.0f);
    p.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "vrange", 1 }, "Variamp Range",
                 juce::StringArray { "Off", "306 Hz", "599 Hz", "1.2 kHz", "2.5 kHz" }, 0));
    knob    ("veffect", "Variamp Effect",  5.0f);
    knob    ("level",   "Channel Level",   7.0f);
    sw      ("fuzz",    "Fuzz On",         false);
    knob    ("fattack", "Fuzz Attack",     5.0f);
    knob    ("fgain",   "Fuzz Gain",       5.0f);
    knob    ("reverb",  "Reverb",          0.0f);
    knob    ("tspeed",  "Tremolo Speed",   4.0f);
    knob    ("tint",    "Tremolo Intensity", 0.0f);
    dbParam ("drive",   "Power Drive (dB)", 0.0f, 30.0f, 6.0f);
    knob    ("headroom","Clipping Adj",    5.0f);
    sw      ("cab",     "261 Cabinet",     true);
    knob    ("horn",    "Horn",            5.0f);
    dbParam ("out",     "Output (dB)",     -24.0f, 12.0f, 0.0f);

    return { p.begin(), p.end() };
}

Acoustic260Processor::Acoustic260Processor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
}

bool Acoustic260Processor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in  = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;
    return in == out;
}

a260::Params Acoustic260Processor::readParams() const
{
    auto get  = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    auto knob = [&get] (const char* id) { return get (id) / 10.0f; };

    a260::Params p;
    p.inputTrimDb   = get ("trim");
    p.volume        = knob ("volume");
    p.bright        = get ("bright") > 0.5f;
    p.bass          = knob ("bass");
    p.treble        = knob ("treble");
    p.variampRange  = (int) std::lround (get ("vrange"));
    p.variampEffect = knob ("veffect");
    p.level         = knob ("level");
    p.fuzzOn        = get ("fuzz") > 0.5f;
    p.fuzzAttack    = knob ("fattack");
    p.fuzzGain      = knob ("fgain");
    p.reverb        = knob ("reverb");
    p.tremSpeed     = knob ("tspeed");
    p.tremIntensity = knob ("tint");
    p.powerDriveDb  = get ("drive");
    p.headroom      = knob ("headroom");
    p.cabOn         = get ("cab") > 0.5f;
    p.horn          = knob ("horn");
    p.outputDb      = get ("out");
    return p;
}

void Acoustic260Processor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    oversampling.reset();
    oversampling.initProcessing ((size_t) samplesPerBlock);
    setLatencySamples ((int) std::ceil (oversampling.getLatencyInSamples()));

    amp.prepare (sampleRate * (double) (1 << kOversampleOrder));
    amp.reset();
    amp.setParams (readParams());
    cab.prepare (sampleRate);
    mono.setSize (1, samplesPerBlock);
}

void Acoustic260Processor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    if (numSamples == 0 || numChannels == 0) return;

    if (mono.getNumSamples() < numSamples) mono.setSize (1, numSamples, false, false, true);

    const auto params = readParams();
    amp.setParams (params);
    cab.setHorn (params.horn);

    // guitar amp is mono: sum inputs
    float* m = mono.getWritePointer (0);
    for (int i = 0; i < numSamples; ++i) m[i] = 0.0f;
    for (int ch = 0; ch < numChannels; ++ch)
    {
        const float* in = buffer.getReadPointer (ch);
        for (int i = 0; i < numSamples; ++i) m[i] += in[i];
    }
    if (numChannels > 1)
        for (int i = 0; i < numSamples; ++i) m[i] /= (float) numChannels;

    // 4x oversampled preamp -> power amp
    float* chans[] = { m };
    juce::dsp::AudioBlock<float> block (chans, 1, (size_t) numSamples);
    auto up = oversampling.processSamplesUp (block);
    float* u = up.getChannelPointer (0);
    const int nUp = (int) up.getNumSamples();
    for (int i = 0; i < nUp; ++i) u[i] = amp.process (u[i]);
    oversampling.processSamplesDown (block);

    // cabinet at host rate
    if (params.cabOn)
        for (int i = 0; i < numSamples; ++i) m[i] = cab.process (m[i]);

    for (int ch = 0; ch < numChannels; ++ch)
        buffer.copyFrom (ch, 0, m, numSamples);
}

juce::AudioProcessorEditor* Acoustic260Processor::createEditor()
{
    return new Acoustic260Editor (*this);
}

void Acoustic260Processor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void Acoustic260Processor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new Acoustic260Processor();
}
