#pragma once

#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <functional>
#include <memory>
#include <vector>

#include "library/library_decl.h"
#include "library/trackmodel.h"
#include "preferences/accessibilitysettings.h"
#include "preferences/usersettings.h"
#include "track/track_decl.h"

class Library;
class PlayerManagerInterface;
class TtsEngine;
class EngineTts;
class EngineEarcon;
class ControlObject;
class ControlProxy;

class AnnouncementManager : public QObject {
    Q_OBJECT
  public:
    // Production factory: creates the platform TtsEngine internally.
    static std::unique_ptr<AnnouncementManager> create(Library* pLibrary,
            PlayerManagerInterface* pPlayerManager,
            UserSettingsPointer pConfig,
            EngineTts* pTtsSink,
            EngineEarcon* pEarcon,
            QObject* parent = nullptr);

    // Single constructor. Tests inject a spy TtsEngine; pass nullptr for
    // pTtsSink and pEarcon when no engine-level sinks are needed.
    AnnouncementManager(Library* pLibrary,
            PlayerManagerInterface* pPlayerManager,
            UserSettingsPointer pConfig,
            std::unique_ptr<TtsEngine> pTts,
            EngineTts* pTtsSink,
            EngineEarcon* pEarcon,
            QObject* parent = nullptr);

    ~AnnouncementManager() override;

    // Exposed as public so tests can drive the slots directly without needing
    // a real Library or live signal connections.
  public slots:
    void slotTrackSelected(TrackPointer pTrack);
    /// A selected track-table row's full spoken description with position
    /// (see Library::trackRowSelected). Supersedes slotTrackSelected's
    /// artist/title-only announcement when available - which is always,
    /// except for models that don't override
    /// TrackModel::rowAccessibleText().
    void slotTrackRowSelected(const QString& text, int row, int rowCount);
    void slotAnnounceSelectedTrack();
    void slotNewTrackLoaded(TrackPointer pTrack, int deckIndex);
    void slotNumberOfDecksChanged(int decks);
    // Sampler counterparts of the two slots above: [SamplerN] groups get a
    // lighter-weight subset of deck feedback (load/play/stop/eject) rather
    // than the full connectGroupControls() surface — sync, hotcues, loops,
    // effects routing etc. don't apply to a sampler.
    void slotNewSamplerTrackLoaded(TrackPointer pTrack, int samplerIndex);
    void slotNumberOfSamplersChanged(int samplers);
    void slotSkinLoaded();
    // Connected to SoundManager::devicesSetup(). Marks the engine as
    // confirmed running (a sound device is open and the audio callback is
    // pulling from the EngineTts sink) and, the first time this fires,
    // flushes a "Mixxx ready" announcement queued by slotSkinLoaded() while
    // audio wasn't up yet. On boot the skin loads (and slotSkinLoaded() runs)
    // before setupDevices() ever runs -- speaking immediately at that point
    // would write into EngineTts's FIFO with nothing pulling it yet, and a
    // later boot-dialog utterance (e.g. a sound-device-busy retry) would
    // likely flush it away via barge-in before the engine ever started. See
    // issue #49.
    void slotSoundDevicesReady();
    void slotLibraryFocusChanged(double value);
    void slotSidebarItemActivated(const QString& title,
            int row = -1,
            int siblingCount = 0,
            int childCount = 0,
            bool expanded = false);
    void slotPlaylistTracksEdited(const QString& name, int added, int removed);
    void slotCrateTracksEdited(const QString& name, int added, int removed);
    void slotQuickPickerItemHighlighted(const QString& text, int row, int siblingCount);
    void slotSearchTextChanged(const QString& text);
    void slotSearchResultCount(int count);
    void slotAnnounceSearch();
    // Speaks the current track-list sort column/order after the debounce
    // timer fires. Public so tests can drive it synchronously.
    void slotAnnounceSort();

    // Speaks the pending debounced control announcement (tempo/mixer moves).
    // Public so tests can fire the debounce without waiting for the timer.
    void slotAnnouncePendingControl();

    // Static helpers are public so tests can verify formatting independently.
    static QString formatForBrowsing(TrackPointer pTrack);
    static QString formatForLoad(TrackPointer pTrack, int deckIndex);
    // Sampler load announcement — "Sampler 3 loaded. Artist. Title. …" —
    // mirrors formatForLoad()'s field order and phrasing but names the
    // sampler by number instead of the deck's phonetic letter.
    static QString formatForSamplerLoad(TrackPointer pTrack, int samplerIndex);

    // Spoken summary of a deck's state (playback, time remaining, BPM,
    // pitch), used by the on-demand [ChannelN],tts_status hotkey. Public so
    // tests can verify the formatting.
    QString formatDeckStatus(const QString& group, int deckIndex) const;

    // Single-fact readouts for the granular info hotkeys ([ChannelN],tts_time
    // / tts_bpm / tts_key / tts_bar). A DJ mid-mix wants one number, not the
    // whole status monologue. Public for tests.
    QString formatTimeRemaining(const QString& group, int deckIndex) const;
    QString formatBpm(const QString& group, int deckIndex) const;
    QString formatKey(const QString& group, int deckIndex) const;
    QString formatBarPosition(const QString& group, int deckIndex) const;

    // Re-speaks the loaded track's artist/title on demand ([ChannelN],
    // tts_track), for when the earlier load announcement was missed or
    // forgotten mid-set. Public for tests.
    QString formatTrackName(const QString& group, int deckIndex) const;

    // Test helpers: allow tests to wire up CO observers for a synthetic group
    // without needing a real BaseTrackPlayer.
    void connectGroupControls(const QString& group, int deckIndex = -1);
    void setDeckHasTrack(const QString& group, bool value);
    // Sampler counterpart of connectGroupControls(): wires just play/stop and
    // eject feedback for a synthetic sampler group, without the deck-only
    // machinery (sync, hotcues, loops, EQ, effects routing, vinyl control…)
    // that doesn't apply to a sampler pad.
    void connectSamplerControls(const QString& group, int samplerIndex = -1);

    // Drops the raw engine sink pointer. Called from the sink's destruction
    // signal (see EngineTts::sinkDestroyed) so speak() never dereferences a
    // torn-down EngineTts. Also exposed for tests.
    void onTtsSinkDestroyed();

    // Plain methods below — not slots. (moc chokes on std::function
    // parameters when it generates slot invokers.)
  public:
    // Name lookups for effects announcements, injected from CoreServices
    // where the EffectsManager lives (this class stays decoupled from the
    // effects headers, and tests inject fakes). Null-safe: without resolvers
    // the announcements fall back to numeric descriptions.
    // effectName: display name of the effect loaded in the given unit/slot
    // (1-based), empty when nothing is loaded. quickEffectName: chain preset
    // name of the deck group's QuickEffect (filter knob).
    void setEffectNameResolvers(
            std::function<QString(int unit, int slot)> effectName,
            std::function<QString(const QString& deckGroup)> quickEffectName);

    // Spoken name for a track-table sort column, or empty for columns that
    // are never sorted by (e.g. the internal id). Public for tests.
    static QString sortColumnName(TrackModel::SortColumnId column);

  private:
    void connectDeck(int deckIndex);
    void connectSampler(int samplerIndex);
    void init(Library* pLibrary, PlayerManagerInterface* pPlayerManager);
    void speak(const QString& text);

    // Sends text to the TtsEngine (voice/rate/route sync + say()). This is
    // the tail end of what speak() used to do unconditionally; it is now
    // also the flush point for a speech batch (see beginSpeechBatch below).
    void dispatchSpeech(const QString& text);

    // Speech batching (issue #48): some call sites synchronously trigger a
    // second speak() as a side effect of the first -- e.g. Smart Cue moving
    // the headphone `pfl` control right after the track-load announcement,
    // whose valueChanged observer speaks "headphone cue on" before the load
    // announcement has had any chance to render. TtsEngine's barge-in
    // generation counter then discards the load announcement, which is
    // exactly backwards: barge-in should only interrupt *new* user-driven
    // speech, not a same-event side effect of the utterance already in
    // flight. Wrapping such a call site in beginSpeechBatch()/endSpeechBatch()
    // defers dispatch of every speak() call in between until the batch ends,
    // then joins them into a single utterance so nothing is silently lost.
    // Nestable; only the outermost endSpeechBatch() actually dispatches.
    void beginSpeechBatch();
    void endSpeechBatch();

    // Feedback for an earcon-capable transport event, honoring the
    // FeedbackMode setting: speech only, earcon only (deck-panned), or both.
    // The per-event enable check is the caller's responsibility.
    void emitCue(int earconId, int deckIndex, const QString& speechText);

    // Queue a debounced announcement for a continuously-variable control
    // (pitch fader, volume, EQ, crossfader). Only the newest pending text is
    // spoken, once the control stops moving.
    void announceControlDebounced(const QString& text);

    // Keyed variant for knobs and faders: `key` identifies the physical
    // control (group + control name), `name` is the spoken prefix ("A
    // volume") and `valueText` the position ("a half"). When the same
    // control keeps moving, the name is spoken only once and subsequent
    // announcements are just the new value; an unchanged value (a worn pot
    // jittering around its resting point) is suppressed entirely.
    void announceControlDebounced(
            const QString& key, const QString& name, const QString& valueText);

    // Shared tail of both overloads: speak now (announce-while-moving mode)
    // or arm the debounce timer.
    void startControlDebounce();

    // Spoken deck name for announcements — "Deck, A" or "Deck 1" depending on
    // the naming preference; falls back to the raw group name when the deck
    // index is unknown (tests).
    QString deckName(const QString& group, int deckIndex) const;

    // Deck prefix for the frequent mixer/tempo readouts: just the letter or
    // number in concise mode ("A, volume a half"), the full name otherwise.
    QString mixerDeckName(const QString& group, int deckIndex) const;

    // Spoken sampler name for announcements — "Sampler 3". Samplers are
    // always numbered (no letter-naming preference the way decks have),
    // since there's no established convention for naming them otherwise.
    static QString samplerName(int samplerIndex);

    // True when mixer/fader readouts should be spoken as percentages instead
    // of fractions, per the MixerReadoutStyle preference.
    bool mixerReadoutAsPercent() const;

    // Fraction readout denominator (4, 8, or 16), per the
    // MixerFractionDetail preference.
    int mixerFractionDenominator() const;

    // Suppress hotcue set/cleared announcements briefly after a track load or
    // unload, which rewrites every hotcue status CO.
    void noteTrackChanged(const QString& group);
    bool recentTrackChange(const QString& group) const;

    std::unique_ptr<TtsEngine> m_pTts;
    // Engine sink the synthesized speech is rendered into. Null in unit tests,
    // where a spy TtsEngine is injected instead.
    EngineTts* m_pTtsSink{nullptr};
    // True once the engine sink has been destroyed (see onTtsSinkDestroyed).
    // speak() bails once this is set: there is nowhere to render the speech and
    // the TtsEngine's own sink pointer has been cleared.
    bool m_ttsSinkDestroyed{false};
    // True once slotSoundDevicesReady() has fired at least once, i.e. a sound
    // device is confirmed open and the engine is pulling from the TTS sink.
    // False from construction, matching real boot: the manager is created
    // well before setupDevices() is ever attempted (see issue #49).
    bool m_audioEngineReady{false};
    // Set by slotSkinLoaded() when it wants to announce "Mixxx ready" but
    // m_audioEngineReady is still false; slotSoundDevicesReady() flushes it.
    bool m_pendingReadyAnnouncement{false};
    // Engine earcon player for transport cues. Null in unit tests.
    EngineEarcon* m_pEarcon{nullptr};
    std::unique_ptr<ControlProxy> m_pSampleRate;
    AccessibilitySettings m_settings;
    // Raw config, for settings that are not accessibility-specific (e.g.
    // smart cue, stored under [Controls] and owned by DlgPrefDeck) but that
    // this class still needs to read.
    UserSettingsPointer m_pConfig;
    QString m_currentTtsVoiceId;
    int m_currentTtsRate{0};
    int m_currentTtsRoute{-1};
    PlayerManagerInterface* m_pPlayerManager;
    QTimer m_selectionDebounce;
    TrackPointer m_pendingTrack;
    // Spoken text for a pending row selection (see slotTrackRowSelected).
    // Mutually exclusive with m_pendingTrack; takes priority when set.
    QString m_pendingRowText;
    int m_connectedDecks{0};
    int m_connectedSamplers{0};

    // Library focus tracking: updated in slotLibraryFocusChanged.
    FocusWidget m_lastFocusWidget{FocusWidget::None};

    // Deduplication for sidebar announcements.
    QString m_lastAnnouncedSidebarItem;

    // Debounced search announcement.
    QTimer m_searchDebounce;
    QString m_pendingSearch;

    // Debounced track-list sort column/order announcement.
    QTimer m_sortDebounce;

    // Per-deck (and per-sampler) playback state tracking. Keyed by group
    // (e.g. "[Channel1]", "[Sampler3]").
    QHash<QString, bool> m_deckHasTrack;
    QHash<QString, bool> m_deckIsPlaying;
    // True while the deck is playing because the cue button is held (cue
    // preview); the eventual stop is not announced.
    QHash<QString, bool> m_deckCuePreview;
    // Best-known loop size in beats per deck, seeded whenever loop_enabled or
    // beatloop_size fires. loop_scale (halve/double the active loop, e.g. the
    // DDJ-400's CUE/LOOP CALL buttons) changes the loop length without ever
    // touching beatloop_size, so this is the only way to keep announcing a
    // sane size across repeated scale presses.
    QHash<QString, double> m_deckLoopBeats;

    // On-demand announcement buttons: [ChannelN],tts_status per deck and the
    // global [Tts],repeat. Owned here; mapped from the keyboard like any CO.
    std::vector<std::unique_ptr<ControlObject>> m_pStatusButtons;
    std::unique_ptr<ControlObject> m_pRepeatButton;
    // Controller feedback hooks driven by controller mappings: [Tts],shift
    // (1 while the hardware shift button is held) and [Tts],pad_mode (an
    // enumerated pad-mode id; see the pad-mode table in the .cpp).
    std::unique_ptr<ControlObject> m_pShiftControl;
    std::unique_ptr<ControlObject> m_pPadModeControl;
    QString m_lastSpoken;

    // Speech batch state; see beginSpeechBatch()/endSpeechBatch().
    int m_speechBatchDepth{0};
    QStringList m_batchedSpeech;

    // Debounced announcements for continuously-variable controls.
    QTimer m_controlDebounce;
    // Unkeyed debounced text (loop size, beat-jump size, effect
    // loaded/focused, …): a single slot is correct here — these represent
    // one conceptual readout being stepped through several values in a row
    // (e.g. CUE/LOOP CALL pressed repeatedly), and only the final value
    // should be announced, exactly like the keyed overload collapses
    // several ticks of the same control into one announcement.
    QString m_pendingControlText;
    // Keyed control announcements (see the keyed announceControlDebounced
    // overload) get their own pending slot per control key, instead of a
    // single shared "latest wins" one — otherwise touching a second,
    // different control (e.g. a volume knob) before the first one's
    // debounce timer fires would silently discard the first control's
    // queued value. This was issue #114: the pitch fader's name-on-touch
    // was heard but the debounced value never followed, because a later
    // touch of an unrelated control had overwritten the single shared
    // pending slot before the shared timer fired.
    struct PendingControlAnnouncement {
        QString name;
        QString value;
    };
    // Insertion order of m_pendingControls' keys, so a batch of several
    // controls that settle within the same debounce window is announced in
    // the order they were first touched rather than in unspecified hash
    // order.
    QStringList m_pendingControlOrder;
    QHash<QString, PendingControlAnnouncement> m_pendingControls;
    // The keyed control last spoken (or currently moving), for the
    // name-on-touch and name-once logic. Any unrelated announcement clears
    // the key.
    QString m_lastControlKey;
    qint64 m_lastControlSpokenMs{0};
    // Last spoken value text per control key, session-lifetime: the jitter
    // and no-change guard. A control whose readout hasn't changed makes no
    // announcement at all — neither name nor value.
    QHash<QString, QString> m_lastValueByKey;
    // Result count of the pending library search (-1 = unknown).
    int m_pendingSearchCount{-1};
    // Last immediate utterance in announce-while-moving mode (ms since epoch).
    qint64 m_lastMovingSpeakMs{0};

    // Timestamp (ms since epoch) of the last track load/unload per group, for
    // hotcue announcement suppression.
    QHash<QString, qint64> m_lastTrackChangeMs;

    // Last clipping announcement (ms since epoch), so sustained clipping
    // doesn't repeat the warning on every peak.
    qint64 m_lastClippingAnnounceMs{0};

    // Per-deck equivalent of m_lastClippingAnnounceMs, keyed by deck group,
    // so one channel clipping doesn't suppress another's warning.
    QHash<QString, qint64> m_lastChannelClippingAnnounceMs;

    // Last audio-dropout (xrun) announcement (ms since epoch); reuses the
    // clipping throttle window since [App],audio_latency_overload can pulse
    // just as fast under sustained CPU overload.
    qint64 m_lastXrunAnnounceMs{0};

    // Effects name lookups; see setEffectNameResolvers().
    std::function<QString(int unit, int slot)> m_effectNameResolver;
    std::function<QString(const QString& deckGroup)> m_quickEffectNameResolver;
};
