#include "util/ttsengine.h"

#ifdef Q_OS_WIN

// initguid.h must come before sapi.h to define CLSID/IID constants inline,
// avoiding a link-time dependency on sapi.lib.
#include <initguid.h>
#include <sapi.h>

// Enumerate SAPI audio output tokens using ISpObjectTokenCategory directly,
// avoiding sphelper.h which requires ATL headers not present in BuildTools.
namespace {

IEnumSpObjectTokens* createTokenEnumerator(LPCWSTR category) {
    ISpObjectTokenCategory* pCategory = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_SpObjectTokenCategory,
            nullptr,
            CLSCTX_ALL,
            IID_ISpObjectTokenCategory,
            reinterpret_cast<void**>(&pCategory));
    if (FAILED(hr) || !pCategory) {
        return nullptr;
    }
    pCategory->SetId(category, FALSE);
    IEnumSpObjectTokens* pEnum = nullptr;
    pCategory->EnumTokens(nullptr, nullptr, &pEnum);
    pCategory->Release();
    return pEnum;
}

IEnumSpObjectTokens* createAudioOutEnumerator() {
    return createTokenEnumerator(SPCAT_AUDIOOUT);
}

IEnumSpObjectTokens* createVoiceEnumerator() {
    return createTokenEnumerator(SPCAT_VOICES);
}

// Returns the display name for a SAPI token. Tries the token's default registry
// value first, then falls back to the "DeviceName" attribute.
QString tokenDisplayName(ISpObjectToken* pToken) {
    WCHAR* pDesc = nullptr;
    if (SUCCEEDED(pToken->GetStringValue(nullptr, &pDesc)) && pDesc && *pDesc) {
        QString name = QString::fromWCharArray(pDesc);
        CoTaskMemFree(pDesc);
        return name;
    }
    CoTaskMemFree(pDesc);

    // Fallback: read DeviceName from the Attributes sub-key.
    ISpDataKey* pAttrKey = nullptr;
    if (SUCCEEDED(pToken->OpenKey(L"Attributes", &pAttrKey))) {
        WCHAR* pAttrDesc = nullptr;
        if (SUCCEEDED(pAttrKey->GetStringValue(L"DeviceName", &pAttrDesc)) &&
                pAttrDesc && *pAttrDesc) {
            QString name = QString::fromWCharArray(pAttrDesc);
            CoTaskMemFree(pAttrDesc);
            pAttrKey->Release();
            return name;
        }
        CoTaskMemFree(pAttrDesc);
        pAttrKey->Release();
    }
    return {};
}

} // namespace

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

    void setOutputDevice(const QString& deviceId) override {
        if (!m_pVoice) {
            return;
        }
        m_pVoice->Speak(nullptr, SPF_PURGEBEFORESPEAK, nullptr);

        if (deviceId.isEmpty()) {
            m_pVoice->SetOutput(nullptr, FALSE);
            return;
        }

        IEnumSpObjectTokens* pEnum = createAudioOutEnumerator();
        if (!pEnum) {
            return;
        }
        setTokenById(pEnum, deviceId, [this](ISpObjectToken* pToken) {
            m_pVoice->SetOutput(pToken, FALSE);
        });
        pEnum->Release();
    }

    void setVoice(const QString& voiceId) override {
        if (!m_pVoice) {
            return;
        }
        m_pVoice->Speak(nullptr, SPF_PURGEBEFORESPEAK, nullptr);

        if (voiceId.isEmpty()) {
            m_pVoice->SetVoice(nullptr);
            return;
        }

        IEnumSpObjectTokens* pEnum = createVoiceEnumerator();
        if (!pEnum) {
            return;
        }
        setTokenById(pEnum, voiceId, [this](ISpObjectToken* pToken) {
            m_pVoice->SetVoice(pToken);
        });
        pEnum->Release();
    }

    void setRate(int rate) override {
        if (m_pVoice) {
            m_pVoice->SetRate(rate);
        }
    }

  private:
    // Walk pEnum looking for the token whose ID matches targetId, then call
    // apply(pToken) on the first match. Releases each token; caller releases pEnum.
    template<typename F>
    static void setTokenById(IEnumSpObjectTokens* pEnum,
            const QString& targetId,
            F apply) {
        ISpObjectToken* pToken = nullptr;
        while (pEnum->Next(1, &pToken, nullptr) == S_OK) {
            WCHAR* pId = nullptr;
            if (SUCCEEDED(pToken->GetId(&pId))) {
                const bool match = (targetId == QString::fromWCharArray(pId));
                CoTaskMemFree(pId);
                if (match) {
                    apply(pToken);
                    pToken->Release();
                    return;
                }
            }
            pToken->Release();
        }
    }

    ISpVoice* m_pVoice = nullptr;
};

template<typename T>
static QList<T> enumerateTokens(IEnumSpObjectTokens* pEnum) {
    QList<T> result;
    if (!pEnum) {
        return result;
    }
    ISpObjectToken* pToken = nullptr;
    while (pEnum->Next(1, &pToken, nullptr) == S_OK) {
        WCHAR* pId = nullptr;
        if (SUCCEEDED(pToken->GetId(&pId))) {
            const QString displayName = tokenDisplayName(pToken);
            if (!displayName.isEmpty()) {
                result << T{QString::fromWCharArray(pId), displayName};
            }
            CoTaskMemFree(pId);
        }
        pToken->Release();
    }
    pEnum->Release();
    return result;
}

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

QList<TtsEngine::AudioOutputDevice> TtsEngine::enumerateOutputDevices() {
#ifdef Q_OS_WIN
    return enumerateTokens<AudioOutputDevice>(createAudioOutEnumerator());
#else
    return {};
#endif
}

QList<TtsEngine::Voice> TtsEngine::enumerateVoices() {
#ifdef Q_OS_WIN
    return enumerateTokens<Voice>(createVoiceEnumerator());
#else
    return {};
#endif
}
