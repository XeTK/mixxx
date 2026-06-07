#include "util/ttsengine.h"

#ifdef Q_OS_WIN

// initguid.h must come before sapi.h and WASAPI headers to define CLSID/IID
// constants inline, avoiding a link-time dependency on sapi.lib / uuid.lib.
#include <audioclient.h>
#include <initguid.h>
#include <mmdeviceapi.h>
#include <sapi.h>

#include <atomic>
#include <fstream>
#include <thread>
#include <vector>

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

// Raw PCM data read from a rendered WAV file.
struct WavPcm {
    std::vector<int16_t> samples; // interleaved, nChannels wide
    DWORD sampleRate{0};
    WORD nChannels{0};
};

// Parse a RIFF/WAVE PCM file and return its samples. Returns empty on failure.
WavPcm readWavFile(const std::wstring& path) {
    WavPcm result;
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        return result;
    }

    auto readBytes = [&](char* dst, std::streamsize n) { f.read(dst, n); };
    auto readU16 = [&]() -> uint16_t {
        uint16_t v = 0;
        f.read(reinterpret_cast<char*>(&v), 2);
        return v;
    };
    auto readU32 = [&]() -> uint32_t {
        uint32_t v = 0;
        f.read(reinterpret_cast<char*>(&v), 4);
        return v;
    };

    char id[4];
    readBytes(id, 4);
    if (memcmp(id, "RIFF", 4) != 0) {
        return result;
    }
    readU32(); // file size
    readBytes(id, 4);
    if (memcmp(id, "WAVE", 4) != 0) {
        return result;
    }

    bool foundData = false;
    while (f && !foundData) {
        char chunkId[4];
        readBytes(chunkId, 4);
        const uint32_t chunkSize = readU32();
        if (!f) {
            break;
        }

        if (memcmp(chunkId, "fmt ", 4) == 0 && chunkSize >= 16) {
            readU16(); // format tag
            result.nChannels = readU16();
            result.sampleRate = readU32();
            readU32(); // byte rate
            readU16(); // block align
            readU16(); // bits per sample
            const auto extra = static_cast<std::streamoff>(chunkSize - 16);
            if (extra > 0) {
                f.seekg(extra, std::ios::cur);
            }
        } else if (memcmp(chunkId, "data", 4) == 0) {
            result.samples.resize(chunkSize / sizeof(int16_t));
            readBytes(reinterpret_cast<char*>(result.samples.data()), chunkSize);
            foundData = true;
        } else {
            f.seekg(static_cast<std::streamoff>(chunkSize), std::ios::cur);
        }
    }

    if (!foundData || result.nChannels == 0 || result.sampleRate == 0) {
        result.samples.clear();
    }
    return result;
}

// Play mono PCM samples on a WASAPI device, routing only to the given channel
// pair (0-based: 0 = ch 1-2, 1 = ch 3-4, …). Uses AUTOCONVERTPCM so Windows
// handles sample-rate conversion from the WAV's native rate to the device rate.
// Checks stop on each iteration so a subsequent say() can cancel playback.
void wasapiPlayOnChannelPair(
        const WavPcm& pcm,
        const QString& endpointId,
        int channelPair,
        std::atomic<bool>& stop) {
    if (pcm.samples.empty() || pcm.sampleRate == 0) {
        return;
    }

    IMMDeviceEnumerator* pEnum = nullptr;
    if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator),
                nullptr,
                CLSCTX_ALL,
                __uuidof(IMMDeviceEnumerator),
                reinterpret_cast<void**>(&pEnum)))) {
        return;
    }

    IMMDevice* pDevice = nullptr;
    if (!endpointId.isEmpty()) {
        pEnum->GetDevice(endpointId.toStdWString().c_str(), &pDevice);
    }
    if (!pDevice) {
        pEnum->GetDefaultAudioEndpoint(eRender, eMultimedia, &pDevice);
    }
    pEnum->Release();
    if (!pDevice) {
        return;
    }

    IAudioClient* pClient = nullptr;
    HRESULT hr = pDevice->Activate(__uuidof(IAudioClient),
            CLSCTX_ALL,
            nullptr,
            reinterpret_cast<void**>(&pClient));
    pDevice->Release();
    if (FAILED(hr) || !pClient) {
        return;
    }

    // Find how many channels the device mix format has.
    WAVEFORMATEX* pMixFmt = nullptr;
    pClient->GetMixFormat(&pMixFmt);
    const WORD nDevCh = pMixFmt ? pMixFmt->nChannels : 2;
    CoTaskMemFree(pMixFmt);

    // Determine the two target channels (0-based). Clamp if out of range.
    const WORD targetCh1 = static_cast<WORD>(std::min<int>(channelPair * 2, nDevCh - 1));
    const WORD targetCh2 = static_cast<WORD>(std::min<int>(channelPair * 2 + 1, nDevCh - 1));

    // Initialize the client with the device's channel count but the WAV's
    // sample rate; AUTOCONVERTPCM lets Windows handle the rate conversion.
    WAVEFORMATEX clientFmt{};
    clientFmt.wFormatTag = WAVE_FORMAT_PCM;
    clientFmt.nChannels = nDevCh;
    clientFmt.nSamplesPerSec = pcm.sampleRate;
    clientFmt.wBitsPerSample = 16;
    clientFmt.nBlockAlign = static_cast<WORD>(nDevCh * 2);
    clientFmt.nAvgBytesPerSec = pcm.sampleRate * clientFmt.nBlockAlign;
    clientFmt.cbSize = 0;

    hr = pClient->Initialize(AUDCLNT_SHAREMODE_SHARED,
            AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY,
            10000000LL, // 1-second buffer
            0,
            &clientFmt,
            nullptr);
    if (FAILED(hr)) {
        pClient->Release();
        return;
    }

    UINT32 bufFrames = 0;
    pClient->GetBufferSize(&bufFrames);

    IAudioRenderClient* pRender = nullptr;
    if (FAILED(pClient->GetService(
                __uuidof(IAudioRenderClient), reinterpret_cast<void**>(&pRender)))) {
        pClient->Release();
        return;
    }

    pClient->Start();

    // Each entry in pcm.samples is one mono sample; write it to the two target
    // channels, with silence in all other channels.
    size_t pos = 0;
    const size_t total = pcm.samples.size();

    while (pos < total && !stop.load(std::memory_order_relaxed)) {
        UINT32 padding = 0;
        pClient->GetCurrentPadding(&padding);
        const UINT32 available = bufFrames - padding;
        if (available == 0) {
            Sleep(5);
            continue;
        }

        const UINT32 frames = static_cast<UINT32>(
                std::min<size_t>(available, total - pos));

        BYTE* pData = nullptr;
        if (FAILED(pRender->GetBuffer(frames, &pData))) {
            break;
        }

        auto* out = reinterpret_cast<int16_t*>(pData);
        for (UINT32 f = 0; f < frames; ++f) {
            const int16_t sample = pcm.samples[pos + f];
            for (WORD ch = 0; ch < nDevCh; ++ch) {
                out[f * nDevCh + ch] = (ch == targetCh1 || ch == targetCh2) ? sample : 0;
            }
        }

        pRender->ReleaseBuffer(frames, 0);
        pos += frames;
    }

    // Let any remaining buffered audio drain before stopping.
    if (!stop.load(std::memory_order_relaxed)) {
        UINT32 padding = 1;
        while (padding > 0 && !stop.load(std::memory_order_relaxed)) {
            Sleep(5);
            pClient->GetCurrentPadding(&padding);
        }
    }

    pClient->Stop();
    pClient->Reset();
    pRender->Release();
    pClient->Release();
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
        m_stopPlayback.store(true);
        if (m_pCurrentOutputToken) {
            m_pCurrentOutputToken->Release();
        }
        if (m_pVoice) {
            m_pVoice->Release();
        }
    }

    void say(const QString& text) override {
        if (!m_pVoice) {
            return;
        }

        // Cancel any ongoing WASAPI channel-routed playback.
        m_stopPlayback.store(true);

        if (m_channelPair > 0) {
            // Render speech to PCM via SAPI (synchronous, fast), then play back
            // via WASAPI in a background thread so say() returns promptly.
            WavPcm pcm = renderToWav(text);
            const QString endpointId = m_wapiEndpointId;
            const int channelPair = m_channelPair;
            m_stopPlayback.store(false);
            std::thread([this, pcm = std::move(pcm), endpointId, channelPair]() {
                CoInitializeEx(nullptr, COINIT_MULTITHREADED);
                wasapiPlayOnChannelPair(pcm, endpointId, channelPair, m_stopPlayback);
                CoUninitialize();
            }).detach();
        } else {
            m_stopPlayback.store(false);
            m_pVoice->Speak(
                    text.toStdWString().c_str(), SPF_ASYNC | SPF_PURGEBEFORESPEAK, nullptr);
        }
    }

    void setOutputDevice(const QString& deviceId) override {
        if (!m_pVoice) {
            return;
        }
        m_pVoice->Speak(nullptr, SPF_PURGEBEFORESPEAK, nullptr);

        // Extract the WASAPI endpoint ID from the SAPI token ID.
        // SAPI token IDs look like:
        //   HKEY_LOCAL_MACHINE\...\MMAudioOut\{0.0.0.00000000}.{guid}
        // The trailing component is the WASAPI endpoint ID.
        const int lastSlash = deviceId.lastIndexOf(QLatin1Char('\\'));
        m_wapiEndpointId = (lastSlash >= 0) ? deviceId.mid(lastSlash + 1) : deviceId;

        if (deviceId.isEmpty()) {
            if (m_pCurrentOutputToken) {
                m_pCurrentOutputToken->Release();
                m_pCurrentOutputToken = nullptr;
            }
            m_pVoice->SetOutput(nullptr, FALSE);
            m_wapiEndpointId.clear();
            return;
        }

        IEnumSpObjectTokens* pEnum = createAudioOutEnumerator();
        if (!pEnum) {
            return;
        }
        setTokenById(pEnum, deviceId, [this](ISpObjectToken* pToken) {
            if (m_pCurrentOutputToken) {
                m_pCurrentOutputToken->Release();
            }
            pToken->AddRef();
            m_pCurrentOutputToken = pToken;
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

    void setOutputChannel(int channelPair) override {
        m_stopPlayback.store(true); // cancel any in-flight WASAPI playback
        m_channelPair = channelPair;
        m_stopPlayback.store(false);
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

    // Render text to a mono 16 kHz PCM buffer via SAPI by writing to a temp
    // WAV file, then reading it back. Called on the main thread (synchronous).
    WavPcm renderToWav(const QString& text) {
        wchar_t tempDir[MAX_PATH]{};
        wchar_t tempFile[MAX_PATH]{};
        GetTempPathW(MAX_PATH, tempDir);
        GetTempFileNameW(tempDir, L"tts", 0, tempFile);
        DeleteFileW(tempFile); // GetTempFileName creates the placeholder; SAPI recreates it
        const std::wstring wavPath = std::wstring(tempFile) + L".wav";

        WavPcm result;

        ISpStream* pStream = nullptr;
        if (FAILED(CoCreateInstance(CLSID_SpStream,
                    nullptr,
                    CLSCTX_ALL,
                    IID_ISpStream,
                    reinterpret_cast<void**>(&pStream)))) {
            return result;
        }

        WAVEFORMATEX wfex{};
        wfex.wFormatTag = WAVE_FORMAT_PCM;
        wfex.nChannels = 1;
        wfex.nSamplesPerSec = 16000;
        wfex.wBitsPerSample = 16;
        wfex.nBlockAlign = 2;
        wfex.nAvgBytesPerSec = 32000;

        if (FAILED(pStream->BindToFile(
                    wavPath.c_str(), SPFM_CREATE_ALWAYS, &SPDFID_WaveFormatEx, &wfex, 0))) {
            pStream->Release();
            return result;
        }

        m_pVoice->SetOutput(pStream, FALSE);
        m_pVoice->Speak(text.toStdWString().c_str(), SPF_PURGEBEFORESPEAK, nullptr);
        m_pVoice->WaitUntilDone(INFINITE);
        pStream->Close();
        pStream->Release();
        // Restore the SAPI voice's audio output device.
        m_pVoice->SetOutput(m_pCurrentOutputToken, FALSE);

        result = readWavFile(wavPath);
        DeleteFileW(wavPath.c_str());
        return result;
    }

    ISpVoice* m_pVoice = nullptr;
    ISpObjectToken* m_pCurrentOutputToken = nullptr; // retained reference; released on change
    QString m_wapiEndpointId;
    int m_channelPair{0};
    std::atomic<bool> m_stopPlayback{false};
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
