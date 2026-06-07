#include "util/ttsengine.h"

#ifdef Q_OS_WIN

// initguid.h must come before sapi.h to define CLSID/IID constants inline,
// avoiding a link-time dependency on sapi.lib.
#include <initguid.h>
#include <sapi.h>

// Enumerate SAPI audio output tokens using ISpObjectTokenCategory directly,
// avoiding sphelper.h which requires ATL headers not present in BuildTools.
namespace {

IEnumSpObjectTokens* createAudioOutEnumerator() {
    ISpObjectTokenCategory* pCategory = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_SpObjectTokenCategory,
            nullptr,
            CLSCTX_ALL,
            IID_ISpObjectTokenCategory,
            reinterpret_cast<void**>(&pCategory));
    if (FAILED(hr) || !pCategory) {
        return nullptr;
    }
    pCategory->SetId(SPCAT_AUDIOOUT, FALSE);
    IEnumSpObjectTokens* pEnum = nullptr;
    pCategory->EnumTokens(nullptr, nullptr, &pEnum);
    pCategory->Release();
    return pEnum;
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
        // Stop any in-progress async speech before switching output device.
        m_pVoice->Speak(nullptr, SPF_PURGEBEFORESPEAK, nullptr);

        if (deviceId.isEmpty()) {
            m_pVoice->SetOutput(nullptr, FALSE);
            return;
        }

        IEnumSpObjectTokens* pEnum = createAudioOutEnumerator();
        if (!pEnum) {
            return;
        }

        ISpObjectToken* pToken = nullptr;
        while (pEnum->Next(1, &pToken, nullptr) == S_OK) {
            WCHAR* pId = nullptr;
            if (SUCCEEDED(pToken->GetId(&pId))) {
                const bool match = (deviceId == QString::fromWCharArray(pId));
                CoTaskMemFree(pId);
                if (match) {
                    m_pVoice->SetOutput(pToken, FALSE);
                    pToken->Release();
                    break;
                }
            }
            pToken->Release();
        }
        pEnum->Release();
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

QList<TtsEngine::AudioOutputDevice> TtsEngine::enumerateOutputDevices() {
    QList<AudioOutputDevice> devices;
#ifdef Q_OS_WIN
    IEnumSpObjectTokens* pEnum = createAudioOutEnumerator();
    if (!pEnum) {
        return devices;
    }

    ISpObjectToken* pToken = nullptr;
    while (pEnum->Next(1, &pToken, nullptr) == S_OK) {
        WCHAR* pId = nullptr;
        if (SUCCEEDED(pToken->GetId(&pId))) {
            const QString displayName = tokenDisplayName(pToken);
            if (!displayName.isEmpty()) {
                devices << AudioOutputDevice{QString::fromWCharArray(pId), displayName};
            }
            CoTaskMemFree(pId);
        }
        pToken->Release();
    }
    pEnum->Release();
#endif
    return devices;
}
