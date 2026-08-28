#pragma once

#include <QString>
#include <QtGlobal>

/// Structured test-hook log for spoken announcements (`--tts-log`).
///
/// The original hook simply appended every string passed to
/// AnnouncementManager::speak() to a file. That records what Mixxx *intended*
/// to say, never what was actually audible: an utterance suppressed by the TTS
/// toggle, one destroyed by barge-in before it finished rendering, or a build
/// whose audio path is severed entirely all produce an identical, perfect
/// looking log. A real regression (the track-load announcement being eaten by
/// the Smart Cue pfl change) hid for weeks behind exactly that.
///
/// This module keeps the record of intent but makes the *outcome* explicit, so
/// automated accessibility tests can assert that a string was actually handed
/// to the synthesizer and mixed into the engine output rather than merely
/// requested.
///
/// Record format — one line per event, newline terminated:
///
///     <utc-iso8601-ms> <EVENT> id=<n> [<key>=<value> ...] text="<escaped>"
///
/// e.g.
///
///     2026-08-25T11:34:56.789Z REQUESTED id=17 text="Deck 1. No track loaded."
///     2026-08-25T11:34:56.790Z SPOKEN id=17 text="Deck 1. No track loaded."
///     2026-08-25T11:34:58.912Z COMPLETED id=17 text="Deck 1. No track loaded."
///
/// The text is always the last field, double quoted, with `\` `"` newline and
/// tab backslash-escaped, so a line is both greppable for the raw string and
/// unambiguously parseable. Every event for one utterance carries the same
/// `id`, so scripts can correlate without matching on text.
///
/// Events:
///   REQUESTED  - speak() was called. Always emitted first; this is all the
///                old hook recorded.
///   SUPPRESSED - returned early, never reached the synthesizer.
///                reason=tts-disabled  the user toggle is off
///                reason=sink-destroyed  the engine sink is gone (shutdown)
///                reason=no-audio  the backend synthesized no PCM at all
///   SPOKEN     - handed to the TtsEngine backend for synthesis.
///   SUPERSEDED - discarded by barge-in (a newer utterance bumped the
///                generation counter) before its audio reached the sink.
///                stage=queued   never picked up by the synthesizer thread
///                stage=render   dropped during/after synthesis
///   FLUSHED    - audio reached the EngineTts FIFO but was discarded before
///                being fully mixed into the output (barge-in flush, or the
///                user toggling TTS off mid-utterance).
///   COMPLETED  - every sample of the utterance was mixed into the engine
///                output buffers. The strongest available "it was audible"
///                signal short of capturing the device.
///
/// Thread safety: every function here is safe to call from any *non-realtime*
/// thread (GUI, synthesizer worker, macOS buffer callback). None of them may
/// be called from the audio callback -- they take a mutex and do file I/O.
/// EngineTts hands audio-thread outcomes over a lock-free FIFO and logs them
/// from a timer instead; see EngineTts::pollAudibilityEvents().
namespace mixxx {
namespace ttslog {

// Values for the `reason=` field of SUPPRESSED.
extern const char* const kReasonTtsDisabled;
extern const char* const kReasonSinkDestroyed;
extern const char* const kReasonNoAudio;

// Values for the `stage=` field of SUPERSEDED.
extern const char* const kStageQueued;
extern const char* const kStageRender;

/// True when `--tts-log <path>` was given (or setLogPath() was called).
/// Cheap enough for startup checks; do not call from the audio thread.
bool isEnabled();

/// Override the log path. Used by tests; also the seam that lets the log be
/// pointed somewhere other than the command line argument. Passing an empty
/// string disables logging and clears remembered utterance text.
void setLogPath(const QString& path);

/// Record REQUESTED and allocate the id that every later event for this
/// utterance must carry. Returns 0 when logging is disabled, in which case
/// every other function here is a no-op for that id.
quint64 logRequested(const QString& text);

/// Record SUPPRESSED with the given reason. `text` is passed explicitly
/// because the caller still has it in hand.
void logSuppressed(quint64 id, const QString& text, const char* reason);

/// Record SPOKEN: the text has been handed to the synthesizer backend.
void logSpoken(quint64 id, const QString& text);

/// Record SUPERSEDED. The text is recovered from the id.
void logSuperseded(quint64 id, const char* stage);

/// Record SUPPRESSED for an utterance that produced no audio at all. The text
/// is recovered from the id.
void logSuppressedById(quint64 id, const char* reason);

/// Record FLUSHED / COMPLETED. Called from EngineTts's polling timer, never
/// from the audio callback. The text is recovered from the id.
void logFlushed(quint64 id);
void logCompleted(quint64 id);

} // namespace ttslog
} // namespace mixxx
