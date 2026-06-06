#include "util/ttsengine.h"

#ifdef Q_OS_WIN

// initguid.h must come before sapi.h to define CLSID/IID constants inline,
// avoiding a link-time dependency on sapi.lib.
#include <initguid.h>
#include <sapi.h>

class SapiTtsEngine final : public TtsEngine {
  public:
    SapiTtsEngine() {
        // COM is already initialised by Qt on the main thread.
        CoCreateInstance(CLSID_SpVoice,
                nullptr,
                CLSCTX_ALL,
                IID_ISpVoice,
                reinterpret_cast<void**>(&m_pVoice));
    }

    ~SapiTtsEngine() override {
        if (m_pVoice) {
            m_pVoice->Release();
        }
    }

    void say(const QString& text) override {
        if (!m_pVoice) {
            return;
        }
        m_pVoice->Speak(text.toStdWString().c_str(),
                SPF_ASYNC | SPF_PURGEBEFORESPEAK,
                nullptr);
    }

  private:
    ISpVoice* m_pVoice = nullptr;
};

#else // !Q_OS_WIN

class NullTtsEngine final : public TtsEngine {
  public:
    void say(const QString&) override {}
};

#endif // Q_OS_WIN

std::unique_ptr<TtsEngine> TtsEngine::create() {
#ifdef Q_OS_WIN
    return std::make_unique<SapiTtsEngine>();
#else
    return std::make_unique<NullTtsEngine>();
#endif
}
