#include "PluginProcessor.h"
#include "PluginEditor.h"
using namespace juce;

const StringArray& GraphProcessor::keyNames()   { static const StringArray k { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }; return k; }
const StringArray& GraphProcessor::scaleNames() { static const StringArray s { "Major", "Minor", "Chromatic" }; return s; }
const StringArray& GraphProcessor::gridNames()  { static const StringArray s { "1/4", "1/8", "1/16", "Off" }; return s; }

AudioProcessorValueTreeState::ParameterLayout GraphProcessor::createLayout()
{
    std::vector<std::unique_ptr<RangedAudioParameter>> p;
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "key", 1 },   "Key",   keyNames(),   0));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "scale", 1 }, "Scale", scaleNames(), 0));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "grid", 1 },  "Grid",  gridNames(),  1));
    p.push_back (std::make_unique<AudioParameterBool>   (ParameterID { "snap", 1 },  "Snap to Scale", true));
    p.push_back (std::make_unique<AudioParameterBool>   (ParameterID { "wave", 1 },  "Show Waveform", true));
    return { p.begin(), p.end() };
}

GraphProcessor::GraphProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
}

bool GraphProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto out = l.getMainOutputChannelSet();
    return (out == AudioChannelSet::mono() || out == AudioChannelSet::stereo()) && out == l.getMainInputChannelSet();
}

void GraphProcessor::prepareToPlay (double sr, int)
{
    const ScopedLock sl (captureLock);
    capRate = sr;
    const size_t need = (size_t) (sr * kMaxCaptureSeconds);
    if (capBuf.size() < need) { capBuf.assign (need, 0.0f); capLen = 0; }
}

void GraphProcessor::startCapture()
{
    capLen.store (0, std::memory_order_release);
    capturing = true;
}

void GraphProcessor::processBlock (AudioBuffer<float>& buf, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;
    // Stage 1: audio passes through untouched; while capturing we also copy a mono mix into the capture buffer.
    if (capturing.load())
    {
        const int n = buf.getNumSamples(), ch = buf.getNumChannels(), len = capLen.load (std::memory_order_relaxed);
        const int room = (int) capBuf.size() - len;
        const int todo = jmin (n, room);
        for (int i = 0; i < todo; ++i)
        {
            float s = 0.0f;
            for (int c = 0; c < ch; ++c) s += buf.getReadPointer (c)[i];
            capBuf[(size_t) (len + i)] = s / (float) jmax (1, ch);
        }
        capLen.store (len + todo, std::memory_order_release);
        if (todo < n) capturing = false;           // buffer full
    }
}

AudioProcessorEditor* GraphProcessor::createEditor() { return new GraphEditor (*this); }

void GraphProcessor::getStateInformation (MemoryBlock& dest)
{
    auto state = apvts.copyState();
    ValueTree nodes ("NOTES");
    for (auto& n : notes)
    {
        ValueTree v ("N");
        v.setProperty ("start", n.start, nullptr).setProperty ("len", n.length, nullptr).setProperty ("pitch", n.pitch, nullptr)
         .setProperty ("tr", n.transition, nullptr).setProperty ("vib", n.vibrato, nullptr).setProperty ("fmt", n.formant, nullptr)
         .setProperty ("fine", n.fine, nullptr).setProperty ("flat", n.flatten, nullptr);
        nodes.appendChild (v, nullptr);
    }
    state.appendChild (nodes, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void GraphProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
    {
        auto state = ValueTree::fromXml (*xml);
        if (! state.isValid()) return;
        notes.clear();
        if (auto nodes = state.getChildWithName ("NOTES"); nodes.isValid())
        {
            for (auto v : nodes)
            {
                GraphNote n;
                n.start = v["start"]; n.length = v["len"]; n.pitch = v["pitch"]; n.transition = v["tr"]; n.vibrato = v["vib"];
                n.formant = v["fmt"]; n.fine = v["fine"]; n.flatten = v["flat"];
                notes.push_back (n);
            }
            state.removeChild (nodes, nullptr);
        }
        apvts.replaceState (state);
    }
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new GraphProcessor(); }
