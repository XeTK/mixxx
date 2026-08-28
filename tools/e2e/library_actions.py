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
    """Open the track table's context menu with a real secondary click.

    Originally this sent the cross-platform "open the context menu from the
    keyboard" chord, Shift+F10. Running live against a real build showed
    that never opens anything here (no menu, no error, nothing) -- macOS
    itself binds the bare F10 key to "Application windows" (part of Mission
    Control) by default, and intercepts it before Mixxx's event loop ever
    sees it, chord or no chord. A real right-click, the same physical
    action a mouse user performs, is not subject to that OS-level
    collision and reliably opens the menu (confirmed live, including that
    the resulting AXMenuItem nodes are found the same way the old
    keyboard-driven menu's were).

    Clicks at a fixed small offset from the table's top-left corner rather
    than its centre: with as few as 1-2 fixture rows the table's centre can
    land below the last row, in the table's empty area, which still focuses
    the table but does not open a *track* context menu.
    """
    table = focus_track_table(driver)
    driver.right_click(table, x_offset=100, y_offset=40)
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
    kHideRemoveShortcutModifier), only this direct keypress path shows the
    confirmation dialog. The menu's "Hide from Library" / "Remove" items
    call straight through to TrackDAO with no confirmation at all, so a
    scenario asserting "the confirmation appeared" must go through this
    path, not the menu.

    The modifier is Command, not physical Control, on macOS. util/defs.h
    defines the shortcut as ``Qt::CTRL`` + ``Key_Backspace``, with a comment
    right above it -- "On macOS, CTRL corresponds to the Command key" -- that
    is easy to misread as "use the physical Control key" if you have not
    actually hit the Qt::CTRL/Cmd swap before. An earlier version of this
    function sent physical Control+Backspace and it was a silent no-op
    against a real build: no dialog, no error, nothing selected changed.
    Command+Backspace (confirmed live) is what WTrackTableView actually
    receives as Qt::CTRL+Key_Backspace on macOS.
    """
    focus_track_table(driver)
    driver.press("backspace", ["command"])
    time.sleep(0.5)


def _dialog_buttons(driver):
    """Return every AXButton currently in the AX tree, as
    ``(x, y, width, height, node)`` sorted left to right.

    Includes the window's own traffic-light buttons (always the first three,
    clustered at the top-left of whatever window they belong to) alongside
    any real dialog buttons -- callers that want only the dialog's own
    buttons should look at the *last* N entries, not all of them.
    """
    from ApplicationServices import AXValueGetValue, kAXValueCGPointType, kAXValueCGSizeType

    buttons = []
    for node, role, _text in driver.walk(driver.app, max_depth=8):
        if role != "AXButton":
            continue
        pos_ref = driver.get_attr(node, "AXPosition")
        size_ref = driver.get_attr(node, "AXSize")
        if pos_ref is None or size_ref is None:
            continue
        ok_pos, pos = AXValueGetValue(pos_ref, kAXValueCGPointType, None)
        ok_size, size = AXValueGetValue(size_ref, kAXValueCGSizeType, None)
        if not ok_pos or not ok_size:
            continue
        buttons.append((pos.x, pos.y, size.width, size.height, node))
    buttons.sort(key=lambda b: b[0])
    return buttons


def click_dialog_accept_button(driver, expected_count=2):
    """Click a modal dialog's accept (rightmost) button.

    Every confirmation dialog in this file's scope (Purge's Yes/No, Hide/
    Remove's Ok/Cancel, Delete's Cancel/Okay) is built from a
    QMessageBox/QDialogButtonBox with the safe/default action on the LEFT
    and the accept action on the RIGHT -- matching native macOS button
    order, confirmed live for the Hide dialog (clicking the rightmost
    AXButton's centre hid the tracks; Tab then Return, tried first, did
    not -- Tab does not appear to move focus between this dialog's buttons
    in a way the AX layer or the dialog itself honours, so this clicks
    the button directly instead, the same physical action a mouse user
    performs).

    `expected_count` is how many AXButtons the dialog itself should
    contribute (2 for a two-button Ok/Cancel-style dialog, 1 for a
    single-OK info popup) -- used only to sanity-check that a dialog is
    actually open before clicking, since the window's own three
    traffic-light buttons are always present too and would otherwise make
    "click the rightmost button" silently click the wrong window.
    """
    buttons = _dialog_buttons(driver)
    dialog_buttons = buttons[-expected_count:] if expected_count else buttons
    if len(buttons) < 3 + expected_count:
        raise RuntimeError(
            f"expected at least {3 + expected_count} AXButtons (3 traffic-light + "
            f"{expected_count} dialog) but found {len(buttons)} -- is a dialog open?"
        )
    x, y, w, h, _node = dialog_buttons[-1]
    driver.backend.click(x + w / 2.0, y + h / 2.0)
    time.sleep(0.5)
