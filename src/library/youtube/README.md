# YouTube (CC) library feature

Search Creative Commons–licensed music on YouTube and load it into Mixxx as a
normal, fully analyzable track (waveform, beatgrid, BPM, key, cue, loop, sync).

Only Creative Commons content is offered. The license is enforced by `yt-dlp`
itself via a `--match-filter` on the license field, for both search and
download, so commercial / standard-licensed videos never appear and cannot be
fetched. **No YouTube Data API key or Google account is required** — the only
dependency is `yt-dlp`.

## How the flow works

1. **Search** — `YouTubeCcSearchTask` runs `yt-dlp` against YouTube's own
   Creative Commons results filter
   (`https://www.youtube.com/results?search_query=<q>&sp=EgIwAQ%3D%3D`) with
   `--flat-playlist --print ...` and parses one tab-separated line per video.
   The `sp=` code is YouTube's server-side CC filter, so results are CC without
   needing to extract each video (yt-dlp does not populate the per-video
   `license` field during a search). Metadata only, and fast.
2. **Download** — on double-click, `YouTubeCcDownloader` runs `yt-dlp` (with the
   same CC match-filter as a hard gate) to fetch the audio-only stream — no
   re-encode, so no ffmpeg needed — into a per-user cache directory, keyed by
   videoId (re-fetch is skipped if already cached).
3. **Load** — the downloaded file is added to the collection via
   `TrackCollectionManager::resolveTrackIdsFromLocations` (which analyzes it like
   any local file) and loaded. Attribution (source URL, uploader, CC BY) is
   written into the track comment.

## UI structure

The feature follows the standard Mixxx layout instead of a bespoke panel:

- The **main library search bar** drives search — there is no search box inside
  the view. Typing while the "YouTube (CC)" root is selected runs a CC search
  (`DlgYouTubeCc::onSearch`).
- The sidebar has a **"Downloaded" child node**. It shows a native track table
  (`YouTubeCcTrackModel`, a `BaseSqlTableModel` filtered to the cache directory)
  — sortable columns, right-click actions, drag-to-deck, and the main search bar
  filters it, exactly like the main **Tracks** view. Downloaded tracks are also
  in your main library, so they appear under **Tracks** too.

Search results themselves stay a lightweight custom list, because they are
remote videos with no local file / analysis until downloaded, so they can't be
a native track table. The standard **load-to-deck shortcuts still work on
them** (e.g. Shift+Left / Shift+Right): `DlgYouTubeCc` observes the
`[ChannelN],LoadSelectedTrack(AndPlay)` controls and, when its view is active,
downloads the selected result and loads it to that deck.

## One-time setup

- **yt-dlp** must be installed and on your `PATH` (or set an explicit path in
  the config key below). See https://github.com/yt-dlp/yt-dlp. That's it.

## Config keys (`mixxx.cfg`, group `[youtube_cc]`)

| Key           | Default   | Meaning                                  |
|---------------|-----------|------------------------------------------|
| `ytdlp_path`  | `yt-dlp`  | Path to the yt-dlp executable            |

The cache directory is `<settings>/youtube_cc_cache`. The feature itself can be
hidden via `[library] ShowYouTubeCcLibrary = 0`.

## Scope / limitations

- Creative Commons only, by design. This is not a route to commercial-catalog
  audio; see the project discussion for why that path is a non-starter.
- Attribution is recorded in the comment; CC BY also requires attribution in any
  public performance/redistribution — that remains the user's responsibility.
- Search relies on YouTube's `sp=` Creative Commons filter for the result list;
  the download step independently hard-gates on the license (a full extraction,
  where yt-dlp's `license` field is populated), so a non-CC video can never be
  fetched even if the filter code were to change.
