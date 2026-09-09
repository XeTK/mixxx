# YouTube (CC) library feature

Search Creative Commons–licensed music on YouTube and load it into Mixxx as a
normal, fully analyzable track (waveform, beatgrid, BPM, key, cue, loop, sync).

Only Creative Commons content is offered: the search is filtered to
`videoLicense=creativeCommon` at the YouTube Data API, and each result's license
is re-verified via `videos.list` before it can be downloaded. Commercial /
standard-licensed videos never appear and cannot be fetched.

## How the flow works

1. **Search** — `YouTubeCcSearchTask` calls the YouTube Data API v3
   (`search.list` + `videos.list`) using a locally stored API key. Metadata only.
2. **Download** — on double-click, `YouTubeCcDownloader` runs `yt-dlp` to fetch
   the audio-only stream (no re-encode, so no ffmpeg needed) into a per-user
   cache directory, keyed by videoId (re-fetch is skipped if already cached).
3. **Load** — the downloaded file is added to the collection via
   `TrackCollectionManager::resolveTrackIdsFromLocations` (which analyzes it like
   any local file) and loaded. Attribution (source URL, uploader, CC BY) is
   written into the track comment.

## One-time setup

- **yt-dlp** must be installed and on your `PATH` (or set an explicit path in
  the config key below). See https://github.com/yt-dlp/yt-dlp.
- **YouTube Data API key** (free): Google Cloud console → APIs & Services →
  Enable "YouTube Data API v3" → Credentials → create an API key. The feature
  prompts for it on the first search and stores it locally.

## Config keys (`mixxx.cfg`, group `[youtube_cc]`)

| Key           | Default   | Meaning                                  |
|---------------|-----------|------------------------------------------|
| `api_key`     | *(empty)* | YouTube Data API v3 key                  |
| `ytdlp_path`  | `yt-dlp`  | Path to the yt-dlp executable            |

The cache directory is `<settings>/youtube_cc_cache`. The feature itself can be
hidden via `[library] ShowYouTubeCcLibrary = 0`.

## Scope / limitations

- Creative Commons only, by design. This is not a route to commercial-catalog
  audio; see the project discussion for why that path is a non-starter.
- Attribution is recorded in the comment; CC BY also requires attribution in any
  public performance/redistribution — that remains the user's responsibility.
- Search costs YouTube Data API quota (~100 units per search; 10k/day default).
