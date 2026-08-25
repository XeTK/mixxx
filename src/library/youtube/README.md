# YouTube library feature

Search Creative Commons–licensed music on YouTube and load it into Mixxx as a
normal, fully analyzable track (waveform, beatgrid, BPM, key, cue, loop, sync).

Only Creative Commons content is offered. The license is enforced by `yt-dlp`
itself via a `--match-filter` on the license field, for both search and
download, so commercial / standard-licensed videos never appear and cannot be
fetched. **No YouTube Data API key or Google account is required** — the only
dependency is `yt-dlp`.

## How the flow works

1. **Search** — `YouTubeSearchTask` runs `yt-dlp` against YouTube's own
   Creative Commons results filter
   (`https://www.youtube.com/results?search_query=<q>&sp=EgIwAQ%3D%3D`) with
   `--flat-playlist --print ...` and parses one tab-separated line per video.
   The `sp=` code is YouTube's server-side CC filter, so results are CC without
   needing to extract each video (yt-dlp does not populate the per-video
   `license` field during a search). Metadata only, and fast.
2. **Download** — on double-click, `YouTubeDownloader` runs `yt-dlp` (with the
   same CC match-filter as a hard gate) to fetch the audio-only stream — no
   re-encode, so no ffmpeg needed — into a per-user cache directory, keyed by
   videoId (re-fetch is skipped if already cached).
3. **Load** — the downloaded file is added to the collection via
   `TrackCollectionManager::resolveTrackIdsFromLocations` (which analyzes it like
   any local file) and loaded. Attribution (source URL, uploader, CC BY) is
   written into the track comment.

## UI structure

The feature follows the standard Mixxx layout, with both nodes rendered by the
shared `WTrackTableView` (`showTrackModel()`) rather than a bespoke panel:

- The **main library search bar** drives search — there is no search box
  inside the view. Typing while the "YouTube" root is selected runs
  `YouTubeSearchModel::search()`, a Creative Commons search.
- Search results are a `YouTubeSearchModel` (`QStandardItemModel` +
  `TrackModel`, following the `BrowseTableModel` pattern for rows that are not
  library entries), so they get the standard track table: sortable columns,
  the normal context menu, and keyboard shortcuts. A result has no local file
  until fetched, so `getTrack()` returns nothing for it; `TrackModel`'s
  `requestDeferredLoad()` hook lets the model take over the load, and
  `rowAccessibleText()` supplies the spoken description a screen reader would
  otherwise get from a `Track` it doesn't have.
- The sidebar has a **"Downloaded" child node**. It shows a native track table
  (`YouTubeTrackModel`, a `BaseSqlTableModel` filtered to the cache directory)
  — sortable columns, right-click actions, drag-to-deck, and the main search
  bar filters it, exactly like the main **Tracks** view. Downloaded tracks are
  also in your main library, so they appear under **Tracks** too.

The standard **load-to-deck shortcuts work on search results** too (e.g.
Shift+Left / Shift+Right): `YouTubeFeature` observes the
`[ChannelN],LoadSelectedTrack(AndPlay)` controls and, when the search results
view is active, downloads the selected result and loads it to that deck via
`requestDeferredLoad()`.

## Accessibility

- Entering the view, the search status ("Searching YouTube for ...", then the
  result count), each result ("Title, by Channel, 3 minutes 42 seconds",
  "downloaded" appended when cached), the start of a download, download
  progress at 25% steps, and download failures are all spoken.
- Fetch progress is also shown on the target deck's waveform overview (a
  `[ChannelN],download_progress` control, 0.0-1.0), so a sighted user sees it
  arriving where the track will play.

## One-time setup

- **yt-dlp** must be installed and on your `PATH` (or set an explicit path in
  the config key below). See https://github.com/yt-dlp/yt-dlp. That's it.

## Config keys (`mixxx.cfg`, group `[youtube]`)

| Key           | Default   | Meaning                                  |
|---------------|-----------|------------------------------------------|
| `ytdlp_path`  | `yt-dlp`  | Path to the yt-dlp executable            |

The cache directory is `<settings>/youtube_cache`. The feature itself can be
hidden via `[library] ShowYouTubeLibrary = 0`.

## Scope / limitations

- Creative Commons only, by design. This is not a route to commercial-catalog
  audio; see the project discussion for why that path is a non-starter.
- Attribution is recorded in the comment; CC BY also requires attribution in any
  public performance/redistribution — that remains the user's responsibility.
- Search relies on YouTube's `sp=` Creative Commons filter for the result list;
  the download step independently hard-gates on the license (a full extraction,
  where yt-dlp's `license` field is populated), so a non-CC video can never be
  fetched even if the filter code were to change.
