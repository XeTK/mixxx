#pragma once

#include <QList>
#include <QString>
#include <memory>

class EngineTts;

/// Synthesizes text to speech and renders it as PCM into an EngineTts sink,
/// which mixes it into Mixxx's own audio output (with ducking) rather than
/// playing it on a separate device. This lets the DJ hear announcements over
/// the music, ducked for intelligibility, and routed to the headphone or main
/// output like any other engine signal.
///
/// Synthesis is platform specific (SAPI on Windows, AVSpeechSynthesizer on
/// macOS, Qt TextToSpeech elsewhere when available); a silent no-op engine is
/// used when no backend is present.
class TtsEngine {
  public:
    // Only meaningful on macOS, where AVSpeechSynthesisVoice reports a real
    // quality tier per installed voice (Enhanced/Premium are downloaded
    // separately via System Settings; most voices are Default until then).
    // Other backends have no equivalent concept and always report Default.
    enum class VoiceQuality {
        Default,
        Enhanced,
        Premium,
    };

    struct Voice {
        QString id;
        QString displayName;
        VoiceQuality quality = VoiceQuality::Default;
    };

    virtual ~TtsEngine() = default;

    // Synthesize text and render it into the sink asynchronously, interrupting
    // any speech already in progress.
    virtual void say(const QString& text) = 0;

    virtual void setVoice(const QString& voiceId) {
        Q_UNUSED(voiceId);
    }

    // Rate in the range [-10, 10]; 0 is normal speed.
    virtual void setRate(int rate) {
        Q_UNUSED(rate);
    }

    // The engine sink that synthesized audio is rendered into. Must be set
    // before say() produces audible output.
    void setSink(EngineTts* pSink) {
        m_pSink = pSink;
    }

    // The engine sample rate to render at, so no resampling is needed before
    // feeding the sink.
    void setSampleRate(int sampleRate) {
        m_sampleRate = sampleRate;
    }

    static std::unique_ptr<TtsEngine> create();
    static QList<Voice> enumerateVoices();

    // False when this build has no speech backend (create() returns a silent
    // no-op engine). Lets the UI warn the user instead of failing silently.
    static bool isAvailable();

  protected:
    EngineTts* m_pSink = nullptr;
    int m_sampleRate = 44100;
};
