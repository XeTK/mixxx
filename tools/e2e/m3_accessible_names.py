#!/usr/bin/env python3
"""M3 Scenario 3: verify accessible names are present in the AX tree.

Uses a running Mixxx, opens Preferences via the AX tree, and asserts that the
expected preference pages (Sound Hardware, Controllers, etc.) are exposed as
accessible nodes.
"""
import sys
import time

import Quartz
from ApplicationServices import (
    AXUIElementCreateApplication,
    AXUIElementCopyAttributeValue,
    AXUIElementPerformAction,
)


def find_mixxx():
    for app in Quartz.NSWorkspace.sharedWorkspace().runningApplications():
        p = app.executableURL().path() if app.executableURL() else ""
        if p.endswith("/mixxx"):
            return app
    return None


def get_attr(el, name):
    return AXUIElementCopyAttributeValue(el, name, None)[1]


def find_nodes(el, predicate, results, depth=0, max_depth=12):
    if depth > max_depth:
        return
    role = get_attr(el, "AXRole")
    title = get_attr(el, "AXTitle")
    desc = get_attr(el, "AXDescription")
    if predicate(role, title, desc):
        results.append(el)
    for child in get_attr(el, "AXChildren") or []:
        find_nodes(child, predicate, results, depth + 1, max_depth)


def main():
    mixxx = find_mixxx()
    if mixxx is None:
        print("FAIL: Mixxx not running")
        sys.exit(1)
    pid = mixxx.processIdentifier()
    app = AXUIElementCreateApplication(pid)
    print(f"Mixxx pid={pid}")

    options = []
    find_nodes(app, lambda r, t, d: r == "AXMenuBarItem" and t == "Options", options)
    if not options:
        print("FAIL: could not find Options menu")
        sys.exit(1)
    AXUIElementPerformAction(options[0], "AXPress")
    time.sleep(1.0)

    prefs = []
    find_nodes(app, lambda r, t, d: t and "Preferences" in t, prefs)
    if not prefs:
        print("FAIL: could not find Preferences item")
        sys.exit(1)
    AXUIElementPerformAction(prefs[0], "AXPress")
    time.sleep(2.0)

    all_labels = set()

    def collect(el, depth=0, max_depth=12):
        if depth > max_depth:
            return
        t = get_attr(el, "AXTitle")
        d = get_attr(el, "AXDescription")
        if t:
            all_labels.add(t)
        if d:
            all_labels.add(d)
        for child in get_attr(el, "AXChildren") or []:
            collect(child, depth + 1, max_depth)

    collect(app)

    expected = [
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
    print("\n=== Accessible names check ===")
    missing = []
    for name in expected:
        found = any(name.lower() in label.lower() for label in all_labels)
        print(f"  [{'OK' if found else 'MISSING'}] {name}")
        if not found:
            missing.append(name)

    print(f"\nTotal distinct accessible labels: {len(all_labels)}")
    if missing:
        print(f"FAIL: missing {len(missing)} expected pages: {missing}")
        sys.exit(1)
    print("PASS: all expected preference pages are accessible")
    sys.exit(0)


if __name__ == "__main__":
    main()
