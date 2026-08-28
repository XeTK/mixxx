#include "util/ttslog.h"

#include <QDateTime>
#include <QFile>
#include <QHash>
#include <QList>
#include <QMutex>
#include <QMutexLocker>
#include <QTextStream>

#include "util/cmdlineargs.h"

namespace mixxx {
namespace ttslog {

const char* const kReasonTtsDisabled = "tts-disabled";
const char* const kReasonSinkDestroyed = "sink-destroyed";
const char* const kReasonNoAudio = "no-audio";

const char* const kStageQueued = "queued";
const char* const kStageRender = "render";

namespace {

// How many utterances' text to keep so outcome events raised later (possibly
// on another thread, seconds after the fact) can still name their utterance.
// Announcements are short and this is a test hook, so a generous ring costs
// nothing and guarantees the E2E scripts never see a text-less record.
constexpr int kRememberedUtterances = 512;

QString escapeText(const QString& text) {
    QString out;
    out.reserve(text.size() + 8);
    for (const QChar c : text) {
        switch (c.unicode()) {
        case '\\':
            out += QLatin1String("\\\\");
            break;
        case '"':
            out += QLatin1String("\\\"");
            break;
        case '\n':
            out += QLatin1String("\\n");
            break;
        case '\r':
            out += QLatin1String("\\r");
            break;
        case '\t':
            out += QLatin1String("\\t");
            break;
        default:
            out += c;
            break;
        }
    }
    return out;
}

/// Serializes every writer (GUI thread, synthesizer worker, macOS buffer
/// callback, EngineTts poll timer) behind one mutex and appends whole lines,
/// so records never interleave.
class TtsLog {
  public:
    static TtsLog& instance() {
        static TtsLog s;
        return s;
    }

    void setPath(const QString& path) {
        QMutexLocker locker(&m_mutex);
        m_path = path;
        m_initialized = true;
        m_texts.clear();
        m_order.clear();
    }

    bool enabled() {
        QMutexLocker locker(&m_mutex);
        ensureInitialized();
        return !m_path.isEmpty();
    }

    quint64 requested(const QString& text) {
        QMutexLocker locker(&m_mutex);
        ensureInitialized();
        if (m_path.isEmpty()) {
            return 0;
        }
        const quint64 id = ++m_lastId;
        remember(id, text);
        write(QLatin1String("REQUESTED"), id, text, QString());
        return id;
    }

    void event(const char* name,
            quint64 id,
            const QString& text,
            const QString& detail) {
        if (id == 0) {
            return;
        }
        QMutexLocker locker(&m_mutex);
        ensureInitialized();
        if (m_path.isEmpty()) {
            return;
        }
        write(QLatin1String(name), id, text, detail);
    }

    void eventById(const char* name, quint64 id, const QString& detail) {
        if (id == 0) {
            return;
        }
        QMutexLocker locker(&m_mutex);
        ensureInitialized();
        if (m_path.isEmpty()) {
            return;
        }
        write(QLatin1String(name), id, m_texts.value(id), detail);
    }

  private:
    // Callers hold m_mutex.
    void ensureInitialized() {
        if (m_initialized) {
            return;
        }
        m_initialized = true;
        m_path = CmdlineArgs::Instance().getTtsLogPath();
    }

    void remember(quint64 id, const QString& text) {
        m_texts.insert(id, text);
        m_order.append(id);
        while (m_order.size() > kRememberedUtterances) {
            m_texts.remove(m_order.takeFirst());
        }
    }

    void write(const QLatin1String& name,
            quint64 id,
            const QString& text,
            const QString& detail) {
        QFile file(m_path);
        if (!file.open(QIODevice::Append | QIODevice::Text)) {
            return;
        }
        QTextStream out(&file);
        out << QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)
            << ' ' << name << " id=" << id;
        if (!detail.isEmpty()) {
            out << ' ' << detail;
        }
        out << " text=\"" << escapeText(text) << "\"\n";
    }

    QMutex m_mutex;
    bool m_initialized{false};
    QString m_path;
    quint64 m_lastId{0};
    QHash<quint64, QString> m_texts;
    QList<quint64> m_order;
};

} // namespace

bool isEnabled() {
    return TtsLog::instance().enabled();
}

void setLogPath(const QString& path) {
    TtsLog::instance().setPath(path);
}

quint64 logRequested(const QString& text) {
    return TtsLog::instance().requested(text);
}

void logSuppressed(quint64 id, const QString& text, const char* reason) {
    TtsLog::instance().event("SUPPRESSED",
            id,
            text,
            QStringLiteral("reason=%1").arg(QLatin1String(reason)));
}

void logSpoken(quint64 id, const QString& text) {
    TtsLog::instance().event("SPOKEN", id, text, QString());
}

void logSuperseded(quint64 id, const char* stage) {
    TtsLog::instance().eventById("SUPERSEDED",
            id,
            QStringLiteral("stage=%1").arg(QLatin1String(stage)));
}

void logSuppressedById(quint64 id, const char* reason) {
    TtsLog::instance().eventById("SUPPRESSED",
            id,
            QStringLiteral("reason=%1").arg(QLatin1String(reason)));
}

void logFlushed(quint64 id) {
    TtsLog::instance().eventById("FLUSHED", id, QString());
}

void logCompleted(quint64 id) {
    TtsLog::instance().eventById("COMPLETED", id, QString());
}

} // namespace ttslog
} // namespace mixxx
