#pragma once

#include <QHash>
#include <QObject>
#include <QString>
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
    void slotAnnounceSelectedTrack();
    void slotNewTrackLoaded(TrackPointer pTrack, int deckIndex);
    void slotNumberOfDecksChanged(int decks);
    void slotSkinLoaded();
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
    void init(Library* pLibrary, PlayerManagerInterface* pPlayerManager);
    void speak(const QString& text);

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
    int m_connectedDecks{0};

    // Library focus tracking: updated in slotLibraryFocusChanged.
    FocusWidget m_lastFocusWidget{FocusWidget::None};

    // Deduplication for sidebar announcements.
    QString m_lastAnnouncedSidebarItem;

    // Debounced search announcement.
    QTimer m_searchDebounce;
    QString m_pendingSearch;

    // Debounced track-list sort column/order announcement.
    QTimer m_sortDebounce;

    // Per-deck playback state tracking. Keyed by deck group (e.g. "[Channel1]").
    QHash<QString, bool> m_deckHasTrack;
    QHash<QString, bool> m_deckIsPlaying;
    // True while the deck is playing because the cue button is held (cue
    // preview); the eventual stop is not announced.
    QHash<QString, bool> m_deckCuePreview;

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

    // Debounced announcements for continuously-variable controls.
    QTimer m_controlDebounce;
    QString m_pendingControlText;
    // Pending keyed control announcement (see the keyed
    // announceControlDebounced overload); mutually exclusive with
    // m_pendingControlText.
    QString m_pendingControlKey;
    QString m_pendingControlName;
    QString m_pendingControlValue;
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

    // Effects name lookups; see setEffectNameResolvers().
    std::function<QString(int unit, int slot)> m_effectNameResolver;
    std::function<QString(const QString& deckGroup)> m_quickEffectNameResolver;
};
