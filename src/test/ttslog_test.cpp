#include "util/ttslog.h"

#include <gtest/gtest.h>

#include <QFile>
#include <QStringList>
#include <QTemporaryDir>

namespace {

QStringList readLines(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    return QString::fromUtf8(file.readAll()).split(QChar('\n'), Qt::SkipEmptyParts);
}

} // namespace

// Direct, non-realtime unit tests for util/ttslog.h's record format. The
// integration-shaped call sites (AnnouncementManager, EngineTts, the TtsEngine
// backends) are covered in announcementmanager_test.cpp and enginetts_test.cpp;
// this file pins the module's own behavior in isolation, including the
// SUPERSEDED event, which the barge-in path in ttsengine.cpp/ttsenginemac.mm
// raises but which requires an OS TTS backend to reach through those callers.
class TtsLogTest : public ::testing::Test {
  protected:
    void SetUp() override {
        ASSERT_TRUE(m_tempDir.isValid());
        m_logPath = m_tempDir.filePath(QStringLiteral("tts.log"));
    }

    void TearDown() override {
        // Leave the global hook off for every other test in the binary.
        mixxx::ttslog::setLogPath(QString());
    }

    QTemporaryDir m_tempDir;
    QString m_logPath;
};

TEST_F(TtsLogTest, DisabledByDefault) {
    EXPECT_FALSE(mixxx::ttslog::isEnabled());
    EXPECT_EQ(static_cast<quint64>(0),
            mixxx::ttslog::logRequested(QStringLiteral("hello")));
}

TEST_F(TtsLogTest, SetLogPath_EnablesAndAssignsIncreasingIds) {
    mixxx::ttslog::setLogPath(m_logPath);
    ASSERT_TRUE(mixxx::ttslog::isEnabled());

    const quint64 id1 = mixxx::ttslog::logRequested(QStringLiteral("first"));
    const quint64 id2 = mixxx::ttslog::logRequested(QStringLiteral("second"));
    EXPECT_GT(id1, static_cast<quint64>(0));
    EXPECT_GT(id2, id1);
}

TEST_F(TtsLogTest, EmptyPath_Disables) {
    mixxx::ttslog::setLogPath(m_logPath);
    ASSERT_TRUE(mixxx::ttslog::isEnabled());
    mixxx::ttslog::setLogPath(QString());
    EXPECT_FALSE(mixxx::ttslog::isEnabled());
}

TEST_F(TtsLogTest, Requested_WritesOneLine) {
    mixxx::ttslog::setLogPath(m_logPath);
    const quint64 id = mixxx::ttslog::logRequested(QStringLiteral("Deck 1 ready"));

    const QStringList lines = readLines(m_logPath);
    ASSERT_EQ(1, lines.size());
    EXPECT_TRUE(lines.first().contains(QStringLiteral("REQUESTED")));
    EXPECT_TRUE(lines.first().contains(QStringLiteral("id=%1").arg(id)));
    EXPECT_TRUE(lines.first().endsWith(QStringLiteral("text=\"Deck 1 ready\"")));
}

TEST_F(TtsLogTest, Spoken_ReachedTheSynthesizer) {
    mixxx::ttslog::setLogPath(m_logPath);
    const quint64 id = mixxx::ttslog::logRequested(QStringLiteral("hi"));
    mixxx::ttslog::logSpoken(id, QStringLiteral("hi"));

    const QStringList lines = readLines(m_logPath);
    ASSERT_EQ(2, lines.size());
    EXPECT_TRUE(lines.at(1).contains(QStringLiteral("SPOKEN")));
    EXPECT_TRUE(lines.at(1).contains(QStringLiteral("id=%1").arg(id)));
}

TEST_F(TtsLogTest, Suppressed_RecordsReason) {
    mixxx::ttslog::setLogPath(m_logPath);
    const quint64 id = mixxx::ttslog::logRequested(QStringLiteral("muted"));
    mixxx::ttslog::logSuppressed(id, QStringLiteral("muted"), mixxx::ttslog::kReasonTtsDisabled);

    const QStringList lines = readLines(m_logPath);
    ASSERT_EQ(2, lines.size());
    EXPECT_TRUE(lines.at(1).contains(QStringLiteral("SUPPRESSED")));
    EXPECT_TRUE(lines.at(1).contains(QStringLiteral("reason=tts-disabled")));
    // A suppressed utterance must never also claim to have been SPOKEN.
    EXPECT_FALSE(lines.at(1).contains(QStringLiteral("SPOKEN")));
}

TEST_F(TtsLogTest, SuppressedById_RecoversTextFromId) {
    mixxx::ttslog::setLogPath(m_logPath);
    const quint64 id = mixxx::ttslog::logRequested(QStringLiteral("no backend"));
    // Callers past the AnnouncementManager (e.g. NullTtsEngine, the FIFO
    // drain finding a zero-length span) don't have the text in hand; the
    // module must recover it from the id it remembered at REQUESTED time.
    mixxx::ttslog::logSuppressedById(id, mixxx::ttslog::kReasonNoAudio);

    const QStringList lines = readLines(m_logPath);
    ASSERT_EQ(2, lines.size());
    EXPECT_TRUE(lines.at(1).endsWith(QStringLiteral("text=\"no backend\"")));
    EXPECT_TRUE(lines.at(1).contains(QStringLiteral("reason=no-audio")));
}

TEST_F(TtsLogTest, Superseded_RecordsStageAndRecoversText) {
    mixxx::ttslog::setLogPath(m_logPath);
    const quint64 firstId = mixxx::ttslog::logRequested(QStringLiteral("first message"));
    // A second utterance barges in before the first was ever picked up by
    // the synthesizer worker.
    mixxx::ttslog::logSuperseded(firstId, mixxx::ttslog::kStageQueued);
    const quint64 secondId = mixxx::ttslog::logRequested(QStringLiteral("second message"));
    // ...and a third barges in mid-render of the second.
    mixxx::ttslog::logSuperseded(secondId, mixxx::ttslog::kStageRender);

    const QStringList lines = readLines(m_logPath);
    ASSERT_EQ(4, lines.size());
    EXPECT_TRUE(lines.at(1).contains(QStringLiteral("SUPERSEDED")));
    EXPECT_TRUE(lines.at(1).contains(QStringLiteral("stage=queued")));
    EXPECT_TRUE(lines.at(1).endsWith(QStringLiteral("text=\"first message\"")));

    EXPECT_TRUE(lines.at(3).contains(QStringLiteral("SUPERSEDED")));
    EXPECT_TRUE(lines.at(3).contains(QStringLiteral("stage=render")));
    EXPECT_TRUE(lines.at(3).endsWith(QStringLiteral("text=\"second message\"")));
}

TEST_F(TtsLogTest, FlushedAndCompleted_AreDistinctOutcomes) {
    mixxx::ttslog::setLogPath(m_logPath);
    const quint64 flushedId = mixxx::ttslog::logRequested(QStringLiteral("cut off"));
    mixxx::ttslog::logFlushed(flushedId);
    const quint64 completedId = mixxx::ttslog::logRequested(QStringLiteral("heard in full"));
    mixxx::ttslog::logCompleted(completedId);

    const QStringList lines = readLines(m_logPath);
    ASSERT_EQ(4, lines.size());
    EXPECT_TRUE(lines.at(1).contains(QStringLiteral("FLUSHED")));
    EXPECT_TRUE(lines.at(3).contains(QStringLiteral("COMPLETED")));
    EXPECT_FALSE(lines.at(1).contains(QStringLiteral("COMPLETED")));
}

TEST_F(TtsLogTest, ZeroId_IsANoOpForEveryOutcomeFunction) {
    mixxx::ttslog::setLogPath(m_logPath);
    // Zero is the sentinel for "logging was disabled when this id would have
    // been allocated" (see logRequested()'s doc comment). Every other
    // function must silently ignore it rather than writing a garbage record.
    mixxx::ttslog::logSuppressed(0, QStringLiteral("x"), mixxx::ttslog::kReasonTtsDisabled);
    mixxx::ttslog::logSpoken(0, QStringLiteral("x"));
    mixxx::ttslog::logSuperseded(0, mixxx::ttslog::kStageQueued);
    mixxx::ttslog::logSuppressedById(0, mixxx::ttslog::kReasonNoAudio);
    mixxx::ttslog::logFlushed(0);
    mixxx::ttslog::logCompleted(0);

    EXPECT_TRUE(readLines(m_logPath).isEmpty());
}

TEST_F(TtsLogTest, TextField_EscapesQuotesBackslashesAndControlChars) {
    mixxx::ttslog::setLogPath(m_logPath);
    mixxx::ttslog::logRequested(QStringLiteral("say \"hi\"\\now\tindeed\nplease"));

    const QStringList lines = readLines(m_logPath);
    ASSERT_EQ(1, lines.size());
    EXPECT_TRUE(lines.first().endsWith(
            QStringLiteral("text=\"say \\\"hi\\\"\\\\now\\tindeed\\nplease\"")))
            << lines.first().toStdString();
}

TEST_F(TtsLogTest, SetLogPath_ClearsRememberedTextForOldIds) {
    mixxx::ttslog::setLogPath(m_logPath);
    const quint64 id = mixxx::ttslog::logRequested(QStringLiteral("before reset"));

    QTemporaryDir otherDir;
    ASSERT_TRUE(otherDir.isValid());
    const QString otherPath = otherDir.filePath(QStringLiteral("other.log"));
    mixxx::ttslog::setLogPath(otherPath);

    // The id from before the reset is stale; recovering its text must not
    // resurrect the old utterance under the new path.
    mixxx::ttslog::logSuppressedById(id, mixxx::ttslog::kReasonNoAudio);
    const QStringList lines = readLines(otherPath);
    ASSERT_EQ(1, lines.size());
    EXPECT_TRUE(lines.first().endsWith(QStringLiteral("text=\"\"")))
            << lines.first().toStdString();
}
