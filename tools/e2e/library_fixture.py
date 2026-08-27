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
    """
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


def seed_tracks(settings_dir, wav_paths, artist="E2E Fixture", album="Destructive Actions"):
    """Insert `wav_paths` directly into track_locations/library.

    Only touches columns present since the base schema (schema.xml revision
    1) plus later columns that all carry safe defaults (header_parsed,
    mixxx_deleted, played) -- see res/schema.xml. That means this does not
    need updating when an unrelated migration adds a new column.
    """
    db_path = os.path.join(settings_dir, DB_FILENAME)
    conn = sqlite3.connect(db_path)
    try:
        cur = conn.cursor()
        for path in wav_paths:
            directory = os.path.dirname(path)
            filename = os.path.basename(path)
            filesize = os.path.getsize(path)
            cur.execute(
                "INSERT INTO track_locations "
                "(location, filename, directory, filesize, fs_deleted, needs_verification) "
                "VALUES (?, ?, ?, ?, 0, 0)",
                (path, filename, directory, filesize),
            )
        for i, path in enumerate(wav_paths):
            title = f"E2E Fixture Track {i + 1}"
            cur.execute(
                "INSERT INTO library "
                "(artist, title, album, location, duration, bitrate, samplerate, "
                " channels, mixxx_deleted, played, header_parsed) "
                "VALUES (?, ?, ?, ?, ?, ?, ?, ?, 0, 0, 1)",
                (artist, title, album, path, float(1), 128, 44100, 1),
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
    """
    db_path = os.path.join(settings_dir, DB_FILENAME)
    conn = sqlite3.connect(db_path)
    conn.row_factory = sqlite3.Row
    try:
        row = conn.execute(
            "SELECT id, mixxx_deleted FROM library WHERE location = ?",
            (wav_path,),
        ).fetchone()
    finally:
        conn.close()
    return dict(row) if row is not None else None


def is_visible(settings_dir, wav_path):
    """True if the track is present and not hidden/purged (mixxx_deleted=0)."""
    row = track_row(settings_dir, wav_path)
    return row is not None and not row["mixxx_deleted"]
