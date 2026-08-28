#!/usr/bin/env python3
"""M3 Scenario 3: verify accessible names are present in the AX tree.

Uses a running Mixxx, opens Preferences via the AX tree, and asserts that the
expected preference pages (Sound Hardware, Controllers, etc.) are exposed as
accessible nodes.

Run via the orchestrator::

    python3 tools/e2e/run_e2e.py --scenario m3_accessible_names
"""
import os
import sys

from ax_driver import AxDriver, create_backend

EXPECTED_PAGES = [
    "Sound Hardware",
    "Controllers",
    "Accessibility",
    "Interface",
    "Decks",
    "Effects",
    "Library",
    "Recording",
    "Mixer",
    "Waveforms",
    "Vinyl Control",
]


def prepare(settings_dir, mixxx_bin):
    # See m1_boot_speech.prepare(): an empty `directories` table makes Mixxx
    # block on a native file-choose dialog before any window appears.
    from library_fixture import bootstrap_settings_dir, register_root_directory

    bootstrap_settings_dir(mixxx_bin, settings_dir)
    music_dir = os.path.normpath(os.path.join(settings_dir, "..", "music"))
    register_root_directory(settings_dir, music_dir)


def run(driver, tts):
    # Open the Options menu, then Preferences, via the AX tree.
    options = driver.find_node(role="AXMenuBarItem", name="Options")
    if options is None:
        print("FAIL: could not find Options menu")
        sys.exit(1)
    driver.activate(options)
    prefs = driver.wait_for(name="Preferences", timeout=5.0)
    if prefs is None:
        print("FAIL: could not find Preferences item")
        sys.exit(1)
    driver.activate(prefs)

    # Collect every accessible label in the (now-open) Preferences tree.
    all_labels = set()
    for _, _, text in driver.walk():
        if text:
            all_labels.add(text)

    print("\n=== Accessible names check ===")
    missing = []
    for name in EXPECTED_PAGES:
        found = any(name.lower() in label.lower() for label in all_labels)
        print(f"  [{'OK' if found else 'MISSING'}] {name}")
        if not found:
            missing.append(name)

    print(f"\nTotal distinct accessible labels: {len(all_labels)}")
    if missing:
        print(f"FAIL: missing {len(missing)} expected pages: {missing}")
        sys.exit(1)
    print("PASS: all expected preference pages are accessible")


if __name__ == "__main__":
    driver = AxDriver(create_backend()).connect()
    run(driver, None)
