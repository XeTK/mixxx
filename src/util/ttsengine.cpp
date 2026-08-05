#include "util/ttsengine.h"

#include <vector>

#include "engine/enginetts.h"
#include "util/types.h"
#ifdef Q_OS_MACOS
#include "util/ttsenginemac.h"
#endif

#ifdef Q_OS_WIN

// windows.h must precede the SAPI/COM headers. initguid.h must come before
// sapi.h to define CLSID/IID constants inline, avoiding a link-time dependency
// on sapi.lib / uuid.lib.
#include <windows.h>

#include <initguid.h>
#include <sapi.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <fstream>
#include <mutex>
#include <thread>

#include <QtGlobal>

namespace {

// Enumerate SAPI voice tokens using ISpObjectTokenCategory directly, avoiding
// sphelper.h which requires ATL headers not present in BuildTools.
IEnumSpObjectTokens* createVoiceEnumerator() {
    ISpObjectTokenCategory* pCategory = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_SpObjectTokenCategory,
            nullptr,
            CLSCTX_ALL,
            IID_ISpObjectTokenCategory,
            reinterpret_cast<void**>(&pCategory));
    if (FAILED(hr) || !pCategory) {
        return nullptr;
    }
    pCategory->SetId(SPCAT_VOICES, FALSE);
    IEnumSpObjectTokens* pEnum = nullptr;
    pCategory->EnumTokens(nullptr, nullptr, &pEnum);
    pCategory->Release();
    return pEnum;
}

// Returns the display name for a SAPI token.
QString tokenDisplayName(ISpObjectToken* pToken) {
    WCHAR* pDesc = nullptr;
    if (SUCCEEDED(pToken->GetStringValue(nullptr, &pDesc)) && pDesc && *pDesc) {
        QString name = QString::fromWCharArray(pDesc);
        CoTaskMemFree(pDesc);
        return name;
    }
    CoTaskMemFree(pDesc);
    return {};
}

// Walk pEnum for the token whose ID matches targetId and apply() it.
template<typename F>
void setTokenById(IEnumSpObjectTokens* pEnum, const QString& targetId, F apply) {
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

// Mono 16-bit PCM read back from a rendered WAV file, plus its sample rate.
struct WavPcm {
    std::vector<int16_t> samples;
    uint32_t sampleRate{0};
};

// Parse a RIFF/WAVE PCM file. Returns empty on failure.
WavPcm readWavFile(const std::wstring& path) {
    WavPcm result;
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        return result;
    }
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
    f.read(id, 4);
    if (memcmp(id, "RIFF", 4) != 0) {
        return result;
    }
    readU32(); // file size
    f.read(id, 4);
    if (memcmp(id, "WAVE", 4) != 0) {
        return result;
    }

    uint16_t nChannels = 0;
    bool foundData = false;
    while (f && !foundData) {
        char chunkId[4];
        f.read(chunkId, 4);
        const uint32_t chunkSize = readU32();
        if (!f) {
            break;
        }
        if (memcmp(chunkId, "fmt ", 4) == 0 && chunkSize >= 16) {
            readU16(); // format tag
            nChannels = readU16();
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
            f.read(reinterpret_cast<char*>(result.samples.data()), chunkSize);
            foundData = true;
        } else {
            f.seekg(static_cast<std::streamoff>(chunkSize), std::ios::cur);
        }
    }

    // Only mono is requested from SAPI; bail otherwise.
    if (!foundData || nChannels != 1 || result.sampleRate == 0) {
        result.samples.clear();
    }
    return result;
}

} // namespace

// Renders speech to PCM with SAPI on a dedicated worker thread and feeds it to
// the engine sink. The worker is the FIFO's single producer.
class SapiTtsEngine final : public TtsEngine {
  public:
    SapiTtsEngine() {
        m_worker = std::thread([this] { workerLoop(); });
    }

    ~SapiTtsEngine() override {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_quit = true;
        }
        m_cv.notify_all();
        if (m_worker.joinable()) {
            m_worker.join();
        }
    }

    void say(const QString& text) override {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_pendingText = text;
            m_hasPending = true;
        }
        // Newer utterance supersedes any render in progress and any audio
        // already queued in the sink.
        m_generation.fetch_add(1, std::memory_order_release);
        if (m_pSink) {
            m_pSink->requestFlush();
        }
        m_cv.notify_all();
    }

    void setVoice(const QString& voiceId) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_desiredVoiceId = voiceId;
        m_voiceDirty = true;
    }

    void setRate(int rate) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_desiredRate = rate;
        m_rateDirty = true;
    }

  private:
    void workerLoop() {
        CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        ISpVoice* pVoice = nullptr;
        CoCreateInstance(CLSID_SpVoice,
                nullptr,
                CLSCTX_ALL,
                IID_ISpVoice,
                reinterpret_cast<void**>(&pVoice));

        while (true) {
            QString text;
            QString voiceId;
            int rate = 0;
            bool applyVoice = false;
            bool applyRate = false;
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                m_cv.wait(lock, [this] { return m_hasPending || m_quit; });
                if (m_quit) {
                    break;
                }
                text = m_pendingText;
                m_hasPending = false;
                applyVoice = m_voiceDirty;
                voiceId = m_desiredVoiceId;
                m_voiceDirty = false;
                applyRate = m_rateDirty;
                rate = m_desiredRate;
                m_rateDirty = false;
            }

            if (!pVoice) {
                continue;
            }
            if (applyVoice) {
                applyVoiceToken(pVoice, voiceId);
            }
            if (applyRate) {
                pVoice->SetRate(rate);
            }

            const long generation = m_generation.load(std::memory_order_acquire);
            const WavPcm pcm = renderToPcm(pVoice, text);
            // Abort if a newer utterance arrived while we were rendering.
            if (generation != m_generation.load(std::memory_order_acquire)) {
                continue;
            }
            renderToSink(pcm, generation);
        }

        if (pVoice) {
            pVoice->Release();
        }
        CoUninitialize();
    }

    void applyVoiceToken(ISpVoice* pVoice, const QString& voiceId) {
        if (voiceId.isEmpty()) {
            pVoice->SetVoice(nullptr);
            return;
        }
        IEnumSpObjectTokens* pEnum = createVoiceEnumerator();
        if (!pEnum) {
            return;
        }
        setTokenById(pEnum, voiceId, [pVoice](ISpObjectToken* pToken) {
            pVoice->SetVoice(pToken);
        });
        pEnum->Release();
    }

    // Render text to mono 16-bit PCM at the engine sample rate by writing a
    // temp WAV and reading it back.
    WavPcm renderToPcm(ISpVoice* pVoice, const QString& text) {
        wchar_t tempDir[MAX_PATH]{};
        wchar_t tempFile[MAX_PATH]{};
        GetTempPathW(MAX_PATH, tempDir);
        GetTempFileNameW(tempDir, L"tts", 0, tempFile);
        DeleteFileW(tempFile); // SAPI recreates it
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
        wfex.nSamplesPerSec = static_cast<DWORD>(m_sampleRate);
        wfex.wBitsPerSample = 16;
        wfex.nBlockAlign = 2;
        wfex.nAvgBytesPerSec = wfex.nSamplesPerSec * wfex.nBlockAlign;

        if (FAILED(pStream->BindToFile(
                    wavPath.c_str(), SPFM_CREATE_ALWAYS, &SPDFID_WaveFormatEx, &wfex, 0))) {
            pStream->Release();
            return result;
        }

        pVoice->SetOutput(pStream, FALSE);
        pVoice->Speak(text.toStdWString().c_str(), SPF_PURGEBEFORESPEAK, nullptr);
        pVoice->WaitUntilDone(INFINITE);
        pStream->Close();
        pStream->Release();

        result = readWavFile(wavPath);
        DeleteFileW(wavPath.c_str());
        return result;
    }

    // Convert mono int16 PCM to interleaved stereo float and push it into the
    // sink, waiting for room and aborting if superseded by a newer utterance.
    void renderToSink(const WavPcm& pcm, long generation) {
        if (!m_pSink || pcm.samples.empty()) {
            return;
        }

        // Wait (briefly) for the sink to drain the previous utterance so the
        // barge-in flush has taken effect before we write.
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(250);
        while (!m_pSink->isEmpty() && std::chrono::steady_clock::now() < deadline) {
            if (generation != m_generation.load(std::memory_order_acquire)) {
                return;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }

        constexpr int kChunkFrames = 1024;
        CSAMPLE stereo[kChunkFrames * 2];
        size_t pos = 0;
        const size_t total = pcm.samples.size();
        while (pos < total) {
            if (generation != m_generation.load(std::memory_order_acquire)) {
                return; // superseded
            }
            const int frames = static_cast<int>(
                    std::min<size_t>(kChunkFrames, total - pos));
            for (int i = 0; i < frames; ++i) {
                const CSAMPLE s = static_cast<CSAMPLE>(pcm.samples[pos + i]) / 32768.0f;
                stereo[i * 2] = s;
                stereo[i * 2 + 1] = s;
            }
            int toWrite = frames * 2;
            int offset = 0;
            while (toWrite > 0) {
                if (generation != m_generation.load(std::memory_order_acquire)) {
                    return;
                }
                const int written = m_pSink->writeSamples(stereo + offset, toWrite);
                if (written == 0) {
                    // Sink full; let the audio thread drain it.
                    std::this_thread::sleep_for(std::chrono::milliseconds(2));
                    continue;
                }
                offset += written;
                toWrite -= written;
            }
            pos += frames;
        }
    }

    std::thread m_worker;
    std::mutex m_mutex;
    std::condition_variable m_cv;
    QString m_pendingText;
    QString m_desiredVoiceId;
    int m_desiredRate{0};
    bool m_hasPending{false};
    bool m_voiceDirty{false};
    bool m_rateDirty{false};
    bool m_quit{false};
    std::atomic<long> m_generation{0};
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

#elif defined(Q_OS_MACOS)

// Native backend using AVSpeechSynthesizer lives in ttsenginemac.mm (needs
// Objective-C++); this avoids depending on the Qt6 TextToSpeech module, which
// Mixxx's macOS dependency bundle doesn't ship.

#elif defined(MIXXX_USE_ESPEAK)

// Native Linux backend using eSpeak NG. eSpeak renders speech to mono 16-bit
// PCM via a synthesis callback (espeak_SetSynthCallback) rather than playing it
// on a device, which is exactly the buffer we need to feed the EngineTts sink.
// It is self-contained (no daemon) and thread-safe, unlike Qt's flite plugin
// which crashes when synthesizing on a worker thread.
//
// A dedicated worker thread (mirroring SapiTtsEngine) serializes synthesis and
// pushes PCM into the sink in chunks, supporting barge-in via a generation
// counter.
#include <espeak-ng/speak_lib.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <mutex>
#include <thread>
#include <vector>

#include <QtGlobal>

class EspeakTtsEngine final : public TtsEngine {
  public:
    EspeakTtsEngine() {
        // AUDIO_OUTPUT_SYNCHRONOUS makes espeak_Synth() return only after the
        // whole utterance has been rendered into the callback buffer, and
        // returns the sample rate from espeak_Initialize().
        m_sampleRate = espeak_Initialize(AUDIO_OUTPUT_SYNCHRONOUS, 0, nullptr, 0);
        if (m_sampleRate <= 0) {
            m_sampleRate = 22050; // eSpeak default fallback
        }
        espeak_SetSynthCallback(&EspeakTtsEngine::synthCallback);
        m_worker = std::thread([this] { workerLoop(); });
    }

    ~EspeakTtsEngine() override {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_quit = true;
        }
        m_cv.notify_all();
        if (m_worker.joinable()) {
            m_worker.join();
        }
        espeak_Terminate();
    }

    void say(const QString& text) override {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_pendingText = text;
            m_hasPending = true;
        }
        // Newer utterance supersedes any render in progress and any audio
        // already queued in the sink.
        m_generation.fetch_add(1, std::memory_order_release);
        if (m_pSink) {
            m_pSink->requestFlush();
        }
        m_cv.notify_all();
    }

    void setVoice(const QString& voiceId) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_desiredVoiceId = voiceId;
        m_voiceDirty = true;
    }

    void setRate(int rate) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_desiredRate = rate;
        m_rateDirty = true;
    }

  private:
    // eSpeak synthesis callback: appends mono 16-bit PCM to the current
    // utterance buffer. Called on the eSpeak internal thread.
    static int synthCallback(short* wav, int numsamples, espeak_EVENT* events) {
        Q_UNUSED(events);
        if (wav && numsamples > 0) {
            s_pcm->insert(s_pcm->end(), wav, wav + numsamples);
        }
        return 0; // continue synthesis
    }

    void workerLoop() {
        while (true) {
            QString text;
            QString voiceId;
            int rate = 0;
            bool applyVoice = false;
            bool applyRate = false;
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                m_cv.wait(lock, [this] { return m_hasPending || m_quit; });
                if (m_quit) {
                    break;
                }
                text = m_pendingText;
                m_hasPending = false;
                applyVoice = m_voiceDirty;
                voiceId = m_desiredVoiceId;
                m_voiceDirty = false;
                applyRate = m_rateDirty;
                rate = m_desiredRate;
                m_rateDirty = false;
            }

            if (applyVoice && !voiceId.isEmpty()) {
                espeak_SetVoiceByName(voiceId.toUtf8().constData());
            }
            if (applyRate) {
                // eSpeak rate is words-per-minute; map Mixxx [-10,10] to a
                // reasonable WPM range around the default 175.
                espeak_SetParameter(espeakRATE, 175 + rate * 15, 0);
            }

            const long generation = m_generation.load(std::memory_order_acquire);
            std::vector<short> pcm;
            s_pcm = &pcm;
            const QByteArray utf8 = text.toUtf8();
            espeak_Synth(utf8.constData(),
                    static_cast<size_t>(utf8.size()) + 1,
                    0,
                    POS_CHARACTER,
                    0,
                    espeakCHARS_UTF8,
                    nullptr,
                    nullptr);
            espeak_Synchronize();
            s_pcm = nullptr;

            // Abort if a newer utterance arrived while we were rendering.
            if (generation != m_generation.load(std::memory_order_acquire)) {
                continue;
            }
            renderToSink(pcm, generation);
        }
    }

    // Convert mono int16 PCM to interleaved stereo float and push it into the
    // sink, waiting for room and aborting if superseded by a newer utterance.
    void renderToSink(const std::vector<short>& pcm, long generation) {
        if (!m_pSink || pcm.empty()) {
            return;
        }

        // Wait (briefly) for the sink to drain the previous utterance so the
        // barge-in flush has taken effect before we write.
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(250);
        while (!m_pSink->isEmpty() && std::chrono::steady_clock::now() < deadline) {
            if (generation != m_generation.load(std::memory_order_acquire)) {
                return;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }

        constexpr int kChunkFrames = 1024;
        CSAMPLE stereo[kChunkFrames * 2];
        size_t pos = 0;
        const size_t total = pcm.size();
        while (pos < total) {
            if (generation != m_generation.load(std::memory_order_acquire)) {
                return; // superseded
            }
            const int frames = static_cast<int>(
                    std::min<size_t>(kChunkFrames, total - pos));
            for (int i = 0; i < frames; ++i) {
                const CSAMPLE s = static_cast<CSAMPLE>(pcm[pos + i]) / 32768.0f;
                stereo[i * 2] = s;
                stereo[i * 2 + 1] = s;
            }
            int toWrite = frames * 2;
            int offset = 0;
            while (toWrite > 0) {
                if (generation != m_generation.load(std::memory_order_acquire)) {
                    return;
                }
                const int written = m_pSink->writeSamples(stereo + offset, toWrite);
                if (written == 0) {
                    // Sink full; let the audio thread drain it.
                    std::this_thread::sleep_for(std::chrono::milliseconds(2));
                    continue;
                }
                offset += written;
                toWrite -= written;
            }
            pos += frames;
        }
    }

    // eSpeak is not re-entrant across threads; the callback runs on eSpeak's
    // internal thread while the worker calls espeak_Synth(). We hand the worker's
    // buffer to the callback via this pointer. Synthesis is serialized by the
    // single worker thread, so this is safe.
    static std::vector<short>* s_pcm;

    std::thread m_worker;
    std::mutex m_mutex;
    std::condition_variable m_cv;
    QString m_pendingText;
    QString m_desiredVoiceId;
    int m_desiredRate{0};
    bool m_hasPending{false};
    bool m_voiceDirty{false};
    bool m_rateDirty{false};
    bool m_quit{false};
    std::atomic<long> m_generation{0};
};

std::vector<short>* EspeakTtsEngine::s_pcm = nullptr;

#else

class NullTtsEngine final : public TtsEngine {
  public:
    void say(const QString&) override {
    }
};

#endif

std::unique_ptr<TtsEngine> TtsEngine::create() {
#ifdef Q_OS_WIN
    return std::make_unique<SapiTtsEngine>();
#elif defined(Q_OS_MACOS)
    return createMacTtsEngine();
#elif defined(MIXXX_USE_ESPEAK)
    return std::make_unique<EspeakTtsEngine>();
#else
    return std::make_unique<NullTtsEngine>();
#endif
}

bool TtsEngine::isAvailable() {
#if defined(Q_OS_WIN) || defined(Q_OS_MACOS) || defined(MIXXX_USE_ESPEAK)
    return true;
#else
    return false;
#endif
}

QList<TtsEngine::Voice> TtsEngine::enumerateVoices() {
#ifdef Q_OS_WIN
    return enumerateTokens<Voice>(createVoiceEnumerator());
#elif defined(Q_OS_MACOS)
    return enumerateMacTtsVoices();
#elif defined(MIXXX_USE_ESPEAK)
    QList<TtsEngine::Voice> result;
    const espeak_VOICE** voices = espeak_ListVoices(nullptr);
    if (voices) {
        for (int i = 0; voices[i] != nullptr; ++i) {
            const char* name = voices[i]->name;
            if (name && *name) {
                result << Voice{QString::fromUtf8(name), QString::fromUtf8(name)};
            }
        }
    }
    return result;
#else
    return {};
#endif
}
