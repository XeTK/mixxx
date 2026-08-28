#!/usr/bin/env python3
"""Shared library-seeding fixture for the destructive-actions E2E scenarios
(``d*_destructive_*.py``), covering part of ``features/destructive_actions.feature``
(issue #104 / #74).

Those scenarios need a throwaway library with a known, small number of real
audio files in it *before* Mixxx's window appears, so the scenario can select
"all tracks" deterministically (see ``select_all_tracks`` in each scenario)
without depending on default sort order or the analyser having finished.

Rather than drive the "Add directory to library" GUI flow (slow, and it
finishes on the analyser's own schedule) this writes directly into a fresh
``mixxxdb.sqlite``, the same file Mixxx itself reads on startup. Getting an
up-to-date schema without hand-tracking every migration in ``res/schema.xml``
is the one tricky part: rather than guessing, ``bootstrap_settings_dir``
launches the real ``mixxx`` binary once against an empty settings dir just
long enough for it to create/migrate its own schema, then reuses that file
(cached, since the schema does not change between scenario runs of the same
build) as a template for every scenario's own settings dir.

Usage from a scenario module::

    from library_fixture import bootstrap_settings_dir, make_wav_files, seed_tracks

    def prepare(settings_dir, mixxx_bin):
        bootstrap_settings_dir(mixxx_bin, settings_dir)
        paths = make_wav_files(os.path.join(settings_dir, "..", "music"), count=1)
        seed_tracks(settings_dir, paths)
        global _SEEDED
        _SEEDED = paths
"""
import os
import shutil
import sqlite3
import struct
import subprocess
import tempfile
import time
import wave

DB_FILENAME = "mixxxdb.sqlite"

# Cached "empty but fully migrated" mixxxdb.sqlite, shared across scenario
# processes in one `run_e2e.py` invocation batch so each scenario does not
# have to pay for a whole extra Mixxx boot just to get an up-to-date schema.
_SCHEMA_CACHE_DIR = os.path.join(tempfile.gettempdir(), "mixxx-e2e-schema-cache")


def make_wav_files(music_dir, count, seconds=1, samplerate=44100):
    """Write `count` tiny, valid, silent mono WAV files and return their paths.

    Real files on disk (not just DB rows) so the "file still on disk" /
    "file no longer at that path" assertions in destructive_actions.feature
    are checking an actual filesystem, not a fiction.

    `music_dir` is resolved to its real path first. Found the hard way
    running this against a live build on macOS: both ``/tmp`` and the
    ``$TMPDIR``/``tempfile.gettempdir()`` tree used by `run_e2e.py`'s
    default workdir (``/var/folders/...``) are themselves symlinks
    (``/tmp`` -> ``private/tmp``, ``/var`` -> ``private/var``). Mixxx's own
    library scanner resolves the real, non-symlinked path when it verifies
    a track's file still exists at its recorded `location`. Seed the DB
    with a `/tmp/...` or `/var/folders/...` path and the scanner's
    resolved path never matches it, so every seeded track gets silently
    marked `fs_deleted=1` on the very first scan and vanishes from the
    Tracks view entirely -- not hidden, not purged, just never shown, which
    made every AX interaction downstream (select, open context menu, ...)
    look broken when the tracks simply were not there to select.
    """
    music_dir = os.path.realpath(music_dir)
    os.makedirs(music_dir, exist_ok=True)
    n_frames = int(seconds * samplerate)
    silence = struct.pack("<h", 0) * n_frames
    paths = []
    for i in range(count):
        path = os.path.join(music_dir, f"e2e_fixture_track_{i + 1}.wav")
        with wave.open(path, "wb") as wf:
            wf.setnchannels(1)
            wf.setsampwidth(2)
            wf.setframerate(samplerate)
            wf.writeframes(silence)
        paths.append(path)
    return paths


def _build_schema_template(mixxx_bin, timeout):
    os.makedirs(_SCHEMA_CACHE_DIR, exist_ok=True)
    template_settings = os.path.join(_SCHEMA_CACHE_DIR, "settings")
    template_db = os.path.join(template_settings, DB_FILENAME)
    if os.path.exists(template_db):
        return template_db

    os.makedirs(template_settings, exist_ok=True)
    proc = subprocess.Popen([mixxx_bin, "--settings-path", template_settings])
    try:
        deadline = time.time() + timeout
        while time.time() < deadline and not os.path.exists(template_db):
            time.sleep(0.5)
        if not os.path.exists(template_db):
            raise RuntimeError(
                f"{template_db} was never created within {timeout}s -- "
                "mixxx_bin may not be a working Mixxx binary"
            )
        # Give schema migrations (run on the first-ever open) a moment to
        # finish writing before we terminate the process.
        time.sleep(2.0)
    finally:
        if proc.poll() is None:
            proc.terminate()
            try:
                proc.wait(timeout=10)
            except subprocess.TimeoutExpired:
                proc.kill()
                proc.wait(timeout=10)
    return template_db


def bootstrap_settings_dir(mixxx_bin, settings_dir, timeout=45.0):
    """Populate `settings_dir` with a freshly-migrated, empty mixxxdb.sqlite.

    Builds (or reuses) the shared schema template above, then copies it in --
    so only the *first* scenario in a batch pays for the extra boot.
    """
    template_db = _build_schema_template(mixxx_bin, timeout)
    os.makedirs(settings_dir, exist_ok=True)
    shutil.copyfile(template_db, os.path.join(settings_dir, DB_FILENAME))


def register_root_directory(settings_dir, directory):
    """Register `directory` in the `directories` table (schema.xml revision
    23: ``directories(directory TEXT UNIQUE)``).

    This is required before Mixxx launches, not just cosmetic: CoreServices::
    initialize() (src/coreservices.cpp) checks whether
    TrackCollection::loadRootDirs() -- backed by exactly this table -- is
    empty, and if so opens a *synchronous, blocking* native
    ``QFileDialog::getExistingDirectory`` ("Choose music library directory")
    before the main window or any TTS speaks a word. On a throwaway
    E2E settings dir the table is always empty, so every scenario hit this
    dialog (discovered by running M1 live for the first time): the process
    just hangs with no window, no "Mixxx ready", nothing in --tts-log, until
    the dialog is dismissed one way or another. On a real desktop session
    with other mounted volumes, an accidentally-dismissed instance of this
    dialog can point Mixxx's library scanner at a real, unrelated directory
    on disk -- exactly what a throwaway settings dir is supposed to avoid.
    Registering any directory here (it does not need to contain fixture
    tracks) makes ``loadRootDirs()`` non-empty and skips the dialog entirely.
    """
    os.makedirs(directory, exist_ok=True)
    db_path = os.path.join(settings_dir, DB_FILENAME)
    conn = sqlite3.connect(db_path)
    try:
        conn.execute(
            "INSERT OR IGNORE INTO directories (directory) VALUES (?)",
            (directory,),
        )
        conn.commit()
    finally:
        conn.close()


def seed_tracks(settings_dir, wav_paths, artist="E2E Fixture", album="Destructive Actions"):
    """Insert `wav_paths` directly into track_locations/library.

    Only touches columns present since the base schema (schema.xml revision
    1) plus later columns that all carry safe defaults (header_parsed,
    mixxx_deleted, played) -- see res/schema.xml. That means this does not
    need updating when an unrelated migration adds a new column.

    IMPORTANT, found the hard way by actually running this against a live
    build (it took a screenshot of a permanently-empty Tracks view to spot):
    despite schema.xml revision 1's comment-adjacent column declaration
    ``location varchar(512) REFERENCES track_locations(location)``, by the
    schema version this fork actually migrates to, ``library.location`` is
    a foreign key to ``track_locations.id`` (an autoincrement integer), NOT
    the literal path string -- see ``LibraryTableModel::setTableModel()``'s
    ``INNER JOIN track_locations ON library.location = track_locations.id``.
    An older version of this function inserted the path string into both
    columns, which happened to make `track_row()`'s old path-keyed lookups
    "work" while silently breaking the one thing that actually matters: the
    real ``INNER JOIN`` Mixxx's own Tracks view runs never matched a text
    path against an integer id, so every seeded track was invisible in the
    UI even though the row genuinely existed. Both sides now agree:
    `track_locations.id` is the value stored in `library.location`.

    Also registers each wav's parent directory in the `directories` table.
    Found the hard way running this against a live build: CoreServices::
    initialize() only shows the "choose your music library directory"
    picker (and, on this machine, silently resolves it to a real, huge
    external volume and scans it) when
    ``TrackCollection::loadRootDirs()`` is empty. A settings dir that has
    never had a directory added to it -- exactly what a freshly bootstrapped
    throwaway settings dir looks like -- hits that path. Pre-seeding
    `directories` with the fixture's own music dir means Mixxx already
    considers a root directory configured and skips the picker/scan
    entirely, which matters a lot more than it looks: without this, a
    scenario run against a machine with a large real library configured
    system-wide can silently scan gigabytes of unrelated files before the
    scenario itself ever gets to run.
    """
    db_path = os.path.join(settings_dir, DB_FILENAME)
    conn = sqlite3.connect(db_path)
    try:
        cur = conn.cursor()
        directories = set()
        location_ids = {}
        for path in wav_paths:
            directory = os.path.dirname(path)
            directories.add(directory)
            filename = os.path.basename(path)
            filesize = os.path.getsize(path)
            cur.execute(
                "INSERT INTO track_locations "
                "(location, filename, directory, filesize, fs_deleted, needs_verification) "
                "VALUES (?, ?, ?, ?, 0, 0)",
                (path, filename, directory, filesize),
            )
            location_ids[path] = cur.lastrowid
        for i, path in enumerate(wav_paths):
            title = f"E2E Fixture Track {i + 1}"
            cur.execute(
                "INSERT INTO library "
                "(artist, title, album, location, duration, bitrate, samplerate, "
                " channels, mixxx_deleted, played, header_parsed) "
                "VALUES (?, ?, ?, ?, ?, ?, ?, ?, 0, 0, 1)",
                (artist, title, album, location_ids[path], float(1), 128, 44100, 1),
            )
        for directory in directories:
            cur.execute(
                "INSERT OR IGNORE INTO directories (directory) VALUES (?)",
                (directory,),
            )
        conn.commit()
    finally:
        conn.close()
    return list(wav_paths)


def track_row(settings_dir, wav_path):
    """Return the `library` row for `wav_path` as a dict, or None if absent.

    Ground truth for "is this track still (visibly) in the library" --
    queried straight from the DB rather than by trying to enumerate rows in
    the AX tree, which Qt's table accessibility does not expose reliably
    enough to count on (see COVERAGE.md's notes on the accessibility bridge
    varying by platform/version).

    Joins through `track_locations` to resolve `wav_path` to the integer id
    `library.location` actually stores -- see `seed_tracks()`'s docstring.
    """
    db_path = os.path.join(settings_dir, DB_FILENAME)
    conn = sqlite3.connect(db_path)
    conn.row_factory = sqlite3.Row
    try:
        row = conn.execute(
            "SELECT library.id, library.mixxx_deleted FROM library "
            "INNER JOIN track_locations ON library.location = track_locations.id "
            "WHERE track_locations.location = ?",
            (wav_path,),
        ).fetchone()
    finally:
        conn.close()
    return dict(row) if row is not None else None


def is_visible(settings_dir, wav_path):
    """True if the track is present and not hidden/purged (mixxx_deleted=0)."""
    row = track_row(settings_dir, wav_path)
    return row is not None and not row["mixxx_deleted"]


def _library_id_for(cur, wav_path):
    row = cur.execute(
        "SELECT library.id FROM library "
        "INNER JOIN track_locations ON library.location = track_locations.id "
        "WHERE track_locations.location = ?",
        (wav_path,),
    ).fetchone()
    if row is None:
        raise RuntimeError(f"{wav_path!r} was not seeded into library first")
    return row[0]


def seed_playlist(settings_dir, name, wav_paths, hidden=0):
    """Create a playlist (Playlists/PlaylistTracks) containing `wav_paths`.

    `wav_paths` must already have been seeded via `seed_tracks`. `hidden=1`
    with `name="Auto DJ"` (PlaylistDAO::PLHT_AUTO_DJ, trackschema.h's
    AUTODJ_TABLE) pre-creates the AutoDJ queue with the given tracks already
    in it -- AutoDJFeature's own findOrCrateAutoDjPlaylistId() looks the
    playlist up by that exact name and reuses it rather than creating a
    second one, so seeding it here before Mixxx ever launches is safe.

    Returns the new playlist's id.
    """
    db_path = os.path.join(settings_dir, DB_FILENAME)
    conn = sqlite3.connect(db_path)
    try:
        cur = conn.cursor()
        cur.execute(
            "INSERT INTO Playlists (name, position, hidden, date_created, date_modified) "
            "VALUES (?, 0, ?, datetime('now'), datetime('now'))",
            (name, hidden),
        )
        playlist_id = cur.lastrowid
        for position, path in enumerate(wav_paths):
            track_id = _library_id_for(cur, path)
            cur.execute(
                "INSERT INTO PlaylistTracks (playlist_id, track_id, position) VALUES (?, ?, ?)",
                (playlist_id, track_id, position),
            )
        conn.commit()
    finally:
        conn.close()
    return playlist_id


def seed_crate(settings_dir, name, wav_paths):
    """Create a crate (crates/crate_tracks) containing `wav_paths`.

    Same real-DB-row approach as `seed_playlist` -- `crates`/`crate_tracks`
    are populated directly rather than driving the "New crate" GUI flow.
    Returns the new crate's id.
    """
    db_path = os.path.join(settings_dir, DB_FILENAME)
    conn = sqlite3.connect(db_path)
    try:
        cur = conn.cursor()
        cur.execute(
            "INSERT INTO crates (name, count, show) VALUES (?, ?, 1)",
            (name, len(wav_paths)),
        )
        crate_id = cur.lastrowid
        for path in wav_paths:
            track_id = _library_id_for(cur, path)
            cur.execute(
                "INSERT INTO crate_tracks (crate_id, track_id) VALUES (?, ?)",
                (crate_id, track_id),
            )
        conn.commit()
    finally:
        conn.close()
    return crate_id


def playlist_track_ids(settings_dir, playlist_name):
    """Return the list of track_ids currently in the named playlist."""
    db_path = os.path.join(settings_dir, DB_FILENAME)
    conn = sqlite3.connect(db_path)
    try:
        cur = conn.cursor()
        row = cur.execute("SELECT id FROM Playlists WHERE name = ?", (playlist_name,)).fetchone()
        if row is None:
            return []
        playlist_id = row[0]
        return [
            r[0]
            for r in cur.execute(
                "SELECT track_id FROM PlaylistTracks WHERE playlist_id = ?", (playlist_id,)
            ).fetchall()
        ]
    finally:
        conn.close()


def crate_track_ids(settings_dir, crate_name):
    """Return the list of track_ids currently in the named crate."""
    db_path = os.path.join(settings_dir, DB_FILENAME)
    conn = sqlite3.connect(db_path)
    try:
        cur = conn.cursor()
        row = cur.execute("SELECT id FROM crates WHERE name = ?", (crate_name,)).fetchone()
        if row is None:
            return []
        crate_id = row[0]
        return [
            r[0]
            for r in cur.execute(
                "SELECT track_id FROM crate_tracks WHERE crate_id = ?", (crate_id,)
            ).fetchall()
        ]
    finally:
        conn.close()
