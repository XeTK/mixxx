#!/usr/bin/env python3
"""Shared AX-tree actions for the destructive-actions E2E scenarios
(``d*_destructive_*.py``). Sits on top of ``ax_driver.py`` the same way
``library_fixture.py`` sits on top of ``MixxxProcess`` -- one shared, small
piece of plumbing rather than seven copies of the same tree-walk.

Everything here assumes the fixture in ``library_fixture.py`` has already
seeded the library, and that Mixxx's main window is showing the default
Library view (the one place the fork gives the track table the accessible
name "Track list" -- see ``WTrackTableView`` constructor, ``setAccessibleName``).
"""
import time

TRACK_TABLE_NAME = "Track list"


def focus_track_table(driver, timeout=15.0):
    """Find and focus the library track table. Returns the AX node."""
    table = driver.wait_for(name=TRACK_TABLE_NAME, timeout=timeout)
    if table is None:
        raise RuntimeError(f"could not find the {TRACK_TABLE_NAME!r} table in the AX tree")
    driver.focus(table)
    time.sleep(0.3)
    return table


def select_all_tracks(driver):
    """Focus the track table and select every row (Cmd+A).

    Scenarios seed exactly as many fixture tracks as the Gherkin scenario
    calls for (one, several, ...) so "select all" is an unambiguous stand-in
    for "select the N tracks the scenario cares about" without depending on
    row order / default sort column.
    """
    focus_track_table(driver)
    driver.press("a", ["command"])
    time.sleep(0.3)


def open_track_context_menu(driver):
    """Open the track table's right-click context menu from the keyboard."""
    focus_track_table(driver)
    driver.press("f10", ["shift"])
    time.sleep(0.5)


def find_menu_item(driver, name_substring, timeout=5.0):
    """Find an open menu's item by (partial) accessible text.

    Tries the AXMenuItem role first (what Qt's Cocoa accessibility bridge is
    documented to expose for QMenu/QAction); falls back to a name-only search
    in case the bridge exposes a different role for this Qt/macOS version --
    matching m3_accessible_names.py's original approach of not hard-depending
    on a specific role for text that should be unique anyway.
    """
    node = driver.wait_for(role="AXMenuItem", name=name_substring, timeout=timeout)
    if node is None:
        node = driver.wait_for(name=name_substring, timeout=1.0)
    return node


def choose_context_menu_item(driver, name_substring):
    """Open the context menu and activate the first item matching `name_substring`."""
    open_track_context_menu(driver)
    item = find_menu_item(driver, name_substring)
    if item is None:
        raise RuntimeError(f"no context menu item matching {name_substring!r} was found")
    driver.activate(item)
    time.sleep(0.5)


def choose_context_menu_item_any(driver, name_candidates):
    """Like choose_context_menu_item, but tries each candidate substring in
    turn. Used for the "Delete Track Files" action, whose label depends on
    the Qt version Mixxx was built against (WTrackMenu::slotRemoveFromDisk):
    "Move Track File(s) to Trash" on Qt >= 5.15, "Delete Files from Disk"
    on older Qt.
    """
    open_track_context_menu(driver)
    for candidate in name_candidates:
        item = find_menu_item(driver, candidate, timeout=2.0)
        if item is not None:
            driver.activate(item)
            time.sleep(0.5)
            return
    raise RuntimeError(f"no context menu item matching any of {name_candidates!r} was found")


def hide_or_remove_selected(driver):
    """Send the Hide/Remove keyboard shortcut directly to the track table.

    This is deliberately NOT "choose Hide from the context menu": per
    WTrackTableView::keyPressEvent (util/defs.h's kHideRemoveShortcutKey /
    kHideRemoveShortcutModifier -- Ctrl+Backspace on macOS, plain Delete
    elsewhere), only this direct keypress path shows the confirmation dialog.
    The menu's "Hide from Library" / "Remove" items call straight through to
    TrackDAO with no confirmation at all, so a scenario asserting "the
    confirmation appeared" must go through this path, not the menu.
    """
    focus_track_table(driver)
    driver.press("backspace", ["control"])
    time.sleep(0.5)
