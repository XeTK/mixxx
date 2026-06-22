#include "util/ttsengine.h"

#ifdef MIXXX_USE_QT_TTS

#include <QTextToSpeech>
#include <QVoice>

class QtTtsEngine final : public TtsEngine {
  public:
    void say(const QString& text) override {
        m_engine.stop();
        m_engine.say(text);
    }

    void setVoice(const QString& voiceId) override {
        if (voiceId.isEmpty()) {
            return;
        }
        for (const QVoice& v : m_engine.availableVoices()) {
            if (v.name() == voiceId) {
                m_engine.setVoice(v);
                return;
            }
        }
    }

    void setRate(int rate) override {
        m_engine.setRate(rate / 10.0);
    }

  private:
    QTextToSpeech m_engine;
};

#else

class NullTtsEngine final : public TtsEngine {
  public:
    void say(const QString&) override {
    }
};

#endif

std::unique_ptr<TtsEngine> TtsEngine::create() {
#ifdef MIXXX_USE_QT_TTS
    return std::make_unique<QtTtsEngine>();
#else
    return std::make_unique<NullTtsEngine>();
#endif
}

QList<TtsEngine::AudioOutputDevice> TtsEngine::enumerateOutputDevices() {
    return {};
}

QList<TtsEngine::Voice> TtsEngine::enumerateVoices() {
#ifdef MIXXX_USE_QT_TTS
    QTextToSpeech engine;
    QList<TtsEngine::Voice> result;
    for (const QVoice& v : engine.availableVoices()) {
        result << Voice{v.name(), v.name()};
    }
    return result;
#else
    return {};
#endif
}
