#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

// One drawn note. Times are seconds from the start of the captured audio.
struct GraphNote
{
    double start = 0.0, length = 0.5;
    int pitch = 60;                       // MIDI note the vocal should sing
    float transition = 35.0f;             // ms glide into the note
    float vibrato = 40.0f;                // % of the original vibrato kept
    float formant = 0.0f;                 // semitones
    float fine = 0.0f;                    // cents
    float flatten = 100.0f;               // % pitch-drift flattening inside the note
};

class GraphProcessor : public juce::AudioProcessor
{
public:
    GraphProcessor();
    ~GraphProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "PitchlyGraph"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    static const juce::StringArray& keyNames();
    static const juce::StringArray& scaleNames();
    static const juce::StringArray& gridNames();

    // ---- capture: records what plays through the plugin so it can be shown and edited ----
    void startCapture();
    void stopCapture()                      { capturing = false; }
    bool isCapturing() const                { return capturing.load(); }
    int  capturedSamples() const            { return capLen.load (std::memory_order_acquire); }
    double captureRate() const              { return capRate; }
    const float* captureData() const        { return capBuf.data(); }
    juce::CriticalSection captureLock;      // held by prepareToPlay (resize) and by readers

    juce::AudioProcessorValueTreeState apvts;
    std::vector<GraphNote> notes;           // message thread only

private:
    static constexpr int kMaxCaptureSeconds = 180;
    std::vector<float> capBuf;
    std::atomic<int> capLen { 0 };
    std::atomic<bool> capturing { false };
    double capRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GraphProcessor)
};
