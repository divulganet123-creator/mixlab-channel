#include "PluginProcessor.h"

using Coeff = juce::dsp::IIR::Coefficients<float>;

juce::AudioProcessorValueTreeState::ParameterLayout MixLabProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    auto add = [&] (const char* id, const char* name, float mn, float mx, float step, float def, const char* unit)
    {
        l.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { id, 1 }, name,
            juce::NormalisableRange<float> (mn, mx, step), def,
            juce::AudioParameterFloatAttributes().withLabel (unit)));
    };

    add ("in",  "Ganho de entrada",  -24.f, 24.f, 0.1f,  0.f,   "dB");
    add ("hpf", "Passa-altas",        20.f, 400.f, 1.f,  20.f,  "Hz");
    add ("lo",  "Graves",            -12.f, 12.f, 0.1f,  0.f,   "dB");
    add ("mid", "Medios",            -12.f, 12.f, 0.1f,  0.f,   "dB");
    add ("hi",  "Agudos",            -12.f, 12.f, 0.1f,  0.f,   "dB");
    add ("th",  "Comp limiar",       -50.f, 0.f,  0.5f, -18.f,  "dB");
    add ("ra",  "Comp razao",          1.f, 12.f, 0.1f,  1.f,   ":1");
    add ("att", "Comp ataque",         1.f, 100.f, 1.f, 10.f,   "ms");
    add ("rel", "Comp liberacao",     20.f, 1000.f, 1.f, 150.f, "ms");
    add ("mk",  "Comp ganho final",    0.f, 18.f, 0.1f,  0.f,   "dB");
    add ("ds",  "De-esser",            0.f, 100.f, 1.f,  0.f,   "%");
    add ("dt",  "Delay tempo",        20.f, 1000.f, 1.f, 250.f, "ms");
    add ("fb",  "Delay repeticoes",    0.f, 90.f, 1.f,  30.f,   "%");
    add ("dm",  "Delay mix",           0.f, 100.f, 1.f,  0.f,   "%");
    add ("rs",  "Reverb tamanho",      0.f, 100.f, 1.f, 50.f,   "%");
    add ("rm",  "Reverb mix",          0.f, 100.f, 1.f,  0.f,   "%");
    add ("out", "Volume de saida",   -30.f, 12.f, 0.1f,  0.f,   "dB");
    l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "lim", 1 }, "Limitador", true));
    return l;
}

MixLabProcessor::MixLabProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
}

bool MixLabProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo())
           && out == layouts.getMainInputChannelSet();
}

void MixLabProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock,
                                  (juce::uint32) getTotalNumOutputChannels() };

    for (auto* f : { &hpf, &lo, &mid, &hi }) { f->prepare (spec); f->reset(); }
    lastEq[0] = lastEq[1] = lastEq[2] = lastEq[3] = -1000.f;

    splitter.prepare (spec);
    splitter.setCutoffFrequency (6000.0f);
    splitter.reset();

    comp.prepare (spec);
    deesser.prepare (spec);
    deesser.setRatio (10.0f);
    deesser.setAttack (2.0f);
    deesser.setRelease (60.0f);

    delay.prepare (spec);
    delay.reset();

    limiter.prepare (spec);
    limiter.setThreshold (-1.0f);
    limiter.setRelease (100.0f);

    reverb.setSampleRate (sampleRate);
    reverb.reset();

    highBand.setSize ((int) spec.numChannels, samplesPerBlock);
}

void MixLabProcessor::processBlock (juce::AudioBuffer<float>& buf, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buf.getNumSamples();
    const int nch = buf.getNumChannels();
    auto dB = [] (float x) { return juce::Decibels::decibelsToGain (x); };

    // entrada
    buf.applyGain (dB (param ("in")));

    juce::dsp::AudioBlock<float> block (buf);
    juce::dsp::ProcessContextReplacing<float> ctx (block);

    // equalizador (coeficientes so mudam quando o parametro muda)
    const float f0 = param ("hpf"), g1 = param ("lo"), g2 = param ("mid"), g3 = param ("hi");
    if (f0 != lastEq[0] || g1 != lastEq[1] || g2 != lastEq[2] || g3 != lastEq[3])
    {
        *hpf.state = *Coeff::makeHighPass (sr, f0, 0.707f);
        *lo.state  = *Coeff::makeLowShelf (sr, 200.0f, 0.707f, dB (g1));
        *mid.state = *Coeff::makePeakFilter (sr, 1500.0f, 0.8f, dB (g2));
        *hi.state  = *Coeff::makeHighShelf (sr, 6000.0f, 0.707f, dB (g3));
        lastEq[0] = f0; lastEq[1] = g1; lastEq[2] = g2; lastEq[3] = g3;
    }
    hpf.process (ctx);
    lo.process (ctx);
    mid.process (ctx);
    hi.process (ctx);

    // de-esser: comprime so a banda acima de 6 kHz
    const float ds = param ("ds");
    if (ds > 0.5f)
    {
        highBand.setSize (nch, n, false, false, true);
        for (int ch = 0; ch < nch; ++ch)
        {
            auto* x = buf.getWritePointer (ch);
            auto* h = highBand.getWritePointer (ch);
            for (int i = 0; i < n; ++i)
            {
                float low = 0.f, high = 0.f;
                splitter.processSample (ch, x[i], low, high);
                x[i] = low;
                h[i] = high;
            }
        }
        deesser.setThreshold (-12.0f - ds * 0.3f);
        juce::dsp::AudioBlock<float> hb (highBand);
        juce::dsp::ProcessContextReplacing<float> hc (hb);
        deesser.process (hc);
        for (int ch = 0; ch < nch; ++ch)
            buf.addFrom (ch, 0, highBand, ch, 0, n);
    }

    // compressor
    const float ra = param ("ra");
    if (ra > 1.05f)
    {
        comp.setThreshold (param ("th"));
        comp.setRatio (ra);
        comp.setAttack (param ("att"));
        comp.setRelease (param ("rel"));
        comp.process (ctx);
        buf.applyGain (dB (param ("mk")));
    }

    // delay
    const float dm = param ("dm") / 100.0f;
    if (dm > 0.001f)
    {
        delay.setDelay ((float) (param ("dt") * 0.001 * sr));
        const float fb = param ("fb") / 100.0f;
        for (int ch = 0; ch < nch; ++ch)
        {
            auto* x = buf.getWritePointer (ch);
            for (int i = 0; i < n; ++i)
            {
                const float d = delay.popSample (ch);
                delay.pushSample (ch, x[i] + d * fb);
                x[i] += d * dm;
            }
        }
    }

    // reverb
    const float rm = param ("rm") / 100.0f;
    if (rm > 0.001f)
    {
        juce::Reverb::Parameters p;
        p.roomSize = param ("rs") / 100.0f;
        p.damping  = 0.5f;
        p.wetLevel = rm * 0.33f;
        p.dryLevel = 1.0f;
        p.width    = 1.0f;
        reverb.setParameters (p);
        if (nch >= 2) reverb.processStereo (buf.getWritePointer (0), buf.getWritePointer (1), n);
        else          reverb.processMono (buf.getWritePointer (0), n);
    }

    // saida e limitador
    buf.applyGain (dB (param ("out")));
    if (param ("lim") > 0.5f)
        limiter.process (ctx);
}

juce::AudioProcessorEditor* MixLabProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor (*this);
}

void MixLabProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void MixLabProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MixLabProcessor();
}
