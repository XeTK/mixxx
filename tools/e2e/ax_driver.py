#!/usr/bin/env python3
"""Shared accessibility-tree (AX) driver and TTS-log assertion helpers for E2E.

This module wraps the OS accessibility tree so E2E scenarios can locate nodes,
read their accessible names, trigger actions, and assert on what Mixxx spoke
(via the ``--tts-log`` hook) without touching pixels.

macOS is the primary backend (PyObjC ``Quartz`` / ``ApplicationServices``,
matching the existing m3/m4 scripts). The backend is abstracted behind a small
interface (``AxBackend``) so a Linux AT-SPI (or Windows UIA) backend can be
added later without changing scenario code.

Typical usage from a scenario::

    from ax_driver import AxDriver, TtsLog, create_backend

    driver = AxDriver(create_backend()).connect()   # attach to running Mixxx
    tts = TtsLog("/tmp/mixxx-e2e/tts.log")
    if tts.wait_for("Mixxx ready"):
        ...
"""

import os
import subprocess
import sys
import time

# macOS CGEvent modifier flags (kCGEventFlagMask*). Kept here so the backend
# can translate portable modifier names ("alt", "shift", ...) into platform
# flags without scenarios reaching into Quartz directly.
_MODIFIER_FLAGS = {
    "alt": 0x00080000,      # kCGEventFlagMaskAlternate
    "shift": 0x00020000,    # kCGEventFlagMaskShift
    "control": 0x00040000,  # kCGEventFlagMaskControl
    "command": 0x00100000,  # kCGEventFlagMaskCommand
}

# macOS virtual keycodes (Carbon HIToolbox kVK_* constants) for the handful of
# non-alphanumeric keys scenarios press by name. Kept here, next to the
# modifier flags, so a scenario never has to hardcode a "magic number" keycode
# without a name attached to it.
KEYCODES = {
    "return": 0x24,     # kVK_Return
    "escape": 0x35,     # kVK_Escape
    "backspace": 0x33,  # kVK_Delete (labelled "delete" on a Mac keyboard; this
                         # is the key kHideRemoveShortcutKey/util/defs.h binds
                         # Hide/Remove to on macOS, as Ctrl+Backspace)
    "tab": 0x30,        # kVK_Tab
    "a": 0x00,          # kVK_ANSI_A -- used for Cmd+A (select all)
    "f10": 0x6D,        # kVK_F10 -- Shift+F10 is Qt's cross-platform "open
                         # the context menu from the keyboard" shortcut, for
                         # keyboards (most Mac ones) with no dedicated Menu key
}


class AxBackend:
    """Abstract interface for driving an OS accessibility tree.

    Subclasses implement the platform-specific calls. Scenario code should use
    :class:`AxDriver` (below), not this class directly.
    """

    def find_mixxx_pid(self):
        """Return the PID of a running Mixxx, or None."""
        raise NotImplementedError

    def create_app(self, pid):
        """Return the top-level accessibility element for a process."""
        raise NotImplementedError

    def get_attr(self, el, name):
        """Return an accessibility attribute value (or None)."""
        raise NotImplementedError

    def children(self, el):
        """Return the list of child accessibility elements."""
        raise NotImplementedError

    def perform_action(self, el, action):
        """Perform an accessibility action (e.g. 'AXPress')."""
        raise NotImplementedError

    def set_value(self, el, value):
        """Set an editable element's value (e.g. a search field)."""
        raise NotImplementedError

    def focus(self, el):
        """Move keyboard focus to an element."""
        raise NotImplementedError

    def activate_app(self, pid):
        """Bring the process's application to the foreground (frontmost)."""
        raise NotImplementedError

    def send_key(self, keycode, modifiers):
        """Post a key press (down+up) with the given modifier names."""
        raise NotImplementedError


class MacAxBackend(AxBackend):
    """macOS backend built on PyObjC Quartz / ApplicationServices."""

    def __init__(self):
        import Quartz
        from ApplicationServices import (
            AXUIElementCopyAttributeValue,
            AXUIElementCreateApplication,
            AXUIElementPerformAction,
            AXUIElementSetAttributeValue,
            AXValueGetValue,
            kAXValueCGPointType,
            kAXValueCGSizeType,
        )
        self.Quartz = Quartz
        self._create_app = AXUIElementCreateApplication
        self._copy_attr = AXUIElementCopyAttributeValue
        self._perform = AXUIElementPerformAction
        self._set_attr = AXUIElementSetAttributeValue
        self._value_get = AXValueGetValue
        self._point_type = kAXValueCGPointType
        self._size_type = kAXValueCGSizeType
        # A real CGEventSource, not None. Found the hard way running this
        # against a live build: CGEventCreateMouseEvent/CGEventCreateKeyboardEvent
        # posted with source=None are silently swallowed on this machine --
        # the events land in the HID queue (CGEventGetLocation shows the
        # cursor really moved for a synthesized click) but neither Qt's
        # keyboard focus nor its table selection ever change. A keyboard/mouse
        # event built from an explicit kCGEventSourceStateHIDSystemState
        # source works. Kept as one shared source (not re-created per call)
        # since CGEventSourceCreate is documented to be relatively expensive.
        self._event_source = Quartz.CGEventSourceCreate(
            Quartz.kCGEventSourceStateHIDSystemState)

    def find_mixxx_pid(self):
        for app in self.Quartz.NSWorkspace.sharedWorkspace().runningApplications():
            p = app.executableURL().path() if app.executableURL() else ""
            if p.endswith("/mixxx"):
                return app.processIdentifier()
        return None

    def create_app(self, pid):
        return self._create_app(pid)

    def get_attr(self, el, name):
        return self._copy_attr(el, name, None)[1]

    def children(self, el):
        return self.get_attr(el, "AXChildren") or []

    def perform_action(self, el, action):
        return self._perform(el, action)

    def set_value(self, el, value):
        return self._set_attr(el, "AXValue", value)

    def _center_of(self, el):
        """Return the (x, y) screen-point centre of `el`, or None if `el`
        has no usable geometry (AXPosition/AXSize unset)."""
        pos_ref = self.get_attr(el, "AXPosition")
        size_ref = self.get_attr(el, "AXSize")
        if pos_ref is None or size_ref is None:
            return None
        ok_pos, pos = self._value_get(pos_ref, self._point_type, None)
        ok_size, size = self._value_get(size_ref, self._size_type, None)
        if not ok_pos or not ok_size:
            return None
        return (pos.x + size.width / 2.0, pos.y + size.height / 2.0)

    def click(self, x, y):
        """Synthesize a real left-click at a screen point (see `focus`)."""
        q = self.Quartz
        down = q.CGEventCreateMouseEvent(
            self._event_source, q.kCGEventLeftMouseDown, (x, y), q.kCGMouseButtonLeft)
        q.CGEventPost(q.kCGHIDEventTap, down)
        up = q.CGEventCreateMouseEvent(
            self._event_source, q.kCGEventLeftMouseUp, (x, y), q.kCGMouseButtonLeft)
        q.CGEventPost(q.kCGHIDEventTap, up)

    def right_click(self, x, y):
        """Synthesize a real right-click (secondary click) at a screen point.

        This is how scenarios open the track context menu -- see
        `AxDriver.open_context_menu_on`. An earlier version of this driver
        opened it with the ``Shift+F10`` keyboard shortcut instead; running
        live against a real build revealed that never worked here at all
        (no menu ever appeared, with no error), almost certainly because
        macOS's own default "Application windows" shortcut is bound to the
        bare F10 key and intercepts it before Mixxx ever sees the event. A
        real secondary click is what a mouse user actually does and is not
        subject to that collision.
        """
        q = self.Quartz
        down = q.CGEventCreateMouseEvent(
            self._event_source, q.kCGEventRightMouseDown, (x, y), q.kCGMouseButtonRight)
        q.CGEventPost(q.kCGHIDEventTap, down)
        up = q.CGEventCreateMouseEvent(
            self._event_source, q.kCGEventRightMouseUp, (x, y), q.kCGMouseButtonRight)
        q.CGEventPost(q.kCGHIDEventTap, up)

    def center_of(self, el):
        """Public wrapper around `_center_of` for scenario/action code that
        needs a click point but not a click (e.g. to offset from it)."""
        return self._center_of(el)

    def focus(self, el):
        """Move real keyboard focus to `el`.

        Setting the ``AXFocused`` attribute directly (the "obvious" AX API
        for this) is a silent no-op against this Qt/macOS accessibility
        bridge -- confirmed by running this live: it never raises, but
        AXFocusedUIElement never changes and no Qt widget ever actually
        receives focus. A real synthesized mouse click, the same physical
        action a sighted user performs, reliably does move Qt's keyboard
        focus, so that is the primary mechanism here. Falls back to the
        AXFocused setter for elements with no on-screen geometry (there
        are none among current callers, but better than raising).
        """
        center = self._center_of(el)
        if center is None:
            return self._set_attr(el, "AXFocused", True)
        self.click(*center)
        return None

    def activate_app(self, pid):
        # CGEventPost(kCGHIDEventTap, ...) below is a *system-wide* HID
        # event injection -- it goes to whatever app currently has real OS
        # keyboard focus, not to this pid specifically. AXUIElementSet
        # AttributeValue(el, "AXFocused", True) (see focus() above) only
        # moves focus *within* an already-frontmost app; it does not bring
        # the app itself forward. On a real desktop with other windows (not
        # a dedicated, isolated display), if anything else is frontmost when
        # send_key() fires, the synthetic keys go to the wrong app entirely
        # -- discovered live: Shift+F10 intended for Mixxx's track-table
        # context menu instead landed on the system, popping the Apple menu.
        # Explicitly activating the target app first is the standard fix
        # (same thing every GUI-automation framework does before typing).
        app = self.Quartz.NSRunningApplication.runningApplicationWithProcessIdentifier_(pid)
        if app is not None:
            app.activateWithOptions_(self.Quartz.NSApplicationActivateIgnoringOtherApps)

    def send_key(self, keycode, modifiers):
        flags = 0
        for name in modifiers:
            flags |= _MODIFIER_FLAGS[name]
        q = self.Quartz
        down = q.CGEventCreateKeyboardEvent(self._event_source, keycode, True)
        q.CGEventSetFlags(down, flags)
        q.CGEventPost(q.kCGHIDEventTap, down)
        up = q.CGEventCreateKeyboardEvent(self._event_source, keycode, False)
        q.CGEventSetFlags(up, flags)
        q.CGEventPost(q.kCGHIDEventTap, up)


def create_backend():
    """Instantiate the backend for the current platform."""
    if sys.platform == "darwin":
        return MacAxBackend()
    raise NotImplementedError(
        "No accessibility backend for platform %r yet. Add an AT-SPI backend "
        "for Linux or a UIA backend for Windows." % sys.platform
    )


class AxDriver:
    """High-level, backend-agnostic access to a running app's AX tree."""

    def __init__(self, backend=None):
        self.backend = backend or create_backend()
        self.app = None
        self.pid = None

    def connect(self, pid=None):
        """Attach to a running Mixxx. If pid is None, find it by name."""
        if pid is None:
            pid = self.backend.find_mixxx_pid()
        if pid is None:
            raise RuntimeError("Mixxx is not running")
        self.pid = pid
        self.app = self.backend.create_app(pid)
        # Bring Mixxx to the front now, and again before every synthetic
        # keypress (see send_key()) -- see activate_app()'s docstring for
        # why this isn't optional on a real desktop.
        self.backend.activate_app(pid)
        return self

    # -- low-level passthroughs -------------------------------------------
    def get_attr(self, el, name):
        return self.backend.get_attr(el, name)

    def children(self, el):
        return self.backend.children(el)

    def node_text(self, el):
        """The accessible name/value of a node, as a string ('' if none)."""
        for attr in ("AXTitle", "AXDescription", "AXValue"):
            val = self.backend.get_attr(el, attr)
            if val:
                return str(val)
        return ""

    def activate(self, el):
        """Trigger the default action (click/activate) on a node."""
        return self.backend.perform_action(el, "AXPress")

    def set_value(self, el, value):
        return self.backend.set_value(el, value)

    def focus(self, el):
        return self.backend.focus(el)

    def right_click_point(self, x, y):
        """Right-click a raw screen point. See `MacAxBackend.right_click`."""
        return self.backend.right_click(x, y)

    def right_click(self, el, x_offset=None, y_offset=None):
        """Right-click `el` (its centre, or an offset within it).

        `x_offset`/`y_offset`, if given, are relative to the element's
        top-left corner rather than its centre -- useful for a table, where
        the "centre" of an empty/near-empty table may not land on any row.
        """
        pos_x, pos_y = None, None
        if x_offset is not None or y_offset is not None:
            pos_ref = self.backend.get_attr(el, "AXPosition")
            if pos_ref is not None:
                ok, pos = self.backend._value_get(pos_ref, self.backend._point_type, None)
                if ok:
                    pos_x, pos_y = pos.x, pos.y
        if pos_x is not None:
            x = pos_x + (x_offset or 0)
            y = pos_y + (y_offset or 0)
        else:
            center = self.backend.center_of(el)
            if center is None:
                raise RuntimeError("element has no usable geometry to right-click")
            x, y = center
        self.right_click_point(x, y)

    def send_key(self, keycode, modifiers=()):
        """Post a key press. `modifiers` is an iterable of names like 'alt'."""
        # Re-activate before every keypress: this is a system-wide HID
        # event, delivered to whichever app is frontmost right now, not
        # necessarily Mixxx (another window can steal focus between
        # scenario steps on a real, shared desktop). Cheap and idempotent
        # when Mixxx is already frontmost.
        if self.pid is not None:
            self.backend.activate_app(self.pid)
        return self.backend.send_key(keycode, modifiers)

    def press(self, key_name, modifiers=()):
        """Post a key press by name, e.g. ``press("return")`` or
        ``press("backspace", ["control"])``. See ``KEYCODES`` for the names
        that are defined."""
        keycode = KEYCODES[key_name]
        return self.backend.send_key(keycode, modifiers)

    # -- tree walking ------------------------------------------------------
    def walk(self, el=None, max_depth=12):
        """Yield ``(node, role, text)`` for every node in the subtree."""
        root = el if el is not None else self.app

        def rec(node, depth):
            if depth > max_depth:
                return
            role = self.backend.get_attr(node, "AXRole")
            text = self.node_text(node)
            yield (node, role, text)
            for child in self.backend.children(node):
                yield from rec(child, depth + 1)

        yield from rec(root, 0)

    def find_nodes(self, role=None, name=None, predicate=None, max_depth=12):
        """Return all nodes matching role/name/predicate.

        `name` is a substring match against the node's accessible text.
        `predicate(role, text)` may be supplied for more complex matches.
        """
        results = []
        for node, r, text in self.walk(max_depth=max_depth):
            if role is not None and r != role:
                continue
            if name is not None and name not in text:
                continue
            if predicate is not None and not predicate(r, text):
                continue
            results.append(node)
        return results

    def find_node(self, role=None, name=None, predicate=None, max_depth=12):
        nodes = self.find_nodes(role, name, predicate, max_depth)
        return nodes[0] if nodes else None

    def wait_for(self, role=None, name=None, predicate=None,
                 timeout=30.0, interval=0.5):
        """Poll until a matching node appears, or None on timeout."""
        deadline = time.time() + timeout
        while time.time() < deadline:
            node = self.find_node(role, name, predicate)
            if node is not None:
                return node
            time.sleep(interval)
        return None

    def wait_for_window(self, timeout=60.0):
        """Wait for the app's main window to appear in the AX tree."""
        return self.wait_for(role="AXWindow", timeout=timeout)


class TtsLog:
    """Reads and asserts on the ``--tts-log`` file Mixxx appends to.

    Mixxx appends one spoken string per line (see AnnouncementManager::speak).
    """

    def __init__(self, path):
        self.path = path

    def read(self):
        try:
            with open(self.path, encoding="utf-8") as f:
                return f.read()
        except FileNotFoundError:
            return ""

    def snapshot(self):
        """Return the current contents, for later diffing."""
        return self.read()

    def new_since(self, snapshot):
        """Return the text appended since a previous snapshot."""
        content = self.read()
        return content[len(snapshot):].strip()

    def contains(self, substring):
        return substring in self.read()

    def wait_for(self, substring, timeout=30.0, interval=0.5):
        """Poll until `substring` appears in the log, or False on timeout."""
        deadline = time.time() + timeout
        while time.time() < deadline:
            if substring in self.read():
                return True
            time.sleep(interval)
        return False

    def wait_for_change(self, snapshot, timeout=30.0, interval=0.5):
        """Poll until the log grows past `snapshot`, or False on timeout."""
        deadline = time.time() + timeout
        while time.time() < deadline:
            if self.new_since(snapshot):
                return True
            time.sleep(interval)
        return False


class MixxxProcess:
    """Launches and tears down a real Mixxx with a throwaway settings dir.

    The orchestrator (run_e2e.py) uses this to give each scenario an isolated
    settings dir and a dedicated ``--tts-log`` file.
    """

    def __init__(self, mixxx_bin="mixxx", settings_dir=None, tts_log=None,
                 extra_args=None):
        self.mixxx_bin = mixxx_bin
        self.settings_dir = settings_dir
        self.tts_log = tts_log
        self.extra_args = list(extra_args or [])
        self.proc = None

    def launch(self):
        if self.settings_dir:
            os.makedirs(self.settings_dir, exist_ok=True)
        cmd = [self.mixxx_bin]
        if self.settings_dir:
            cmd += ["--settings-path", self.settings_dir]
        if self.tts_log:
            cmd += ["--tts-log", self.tts_log]
        cmd += self.extra_args
        self.proc = subprocess.Popen(cmd)
        return self

    def terminate(self):
        if self.proc is not None and self.proc.poll() is None:
            self.proc.terminate()
            try:
                self.proc.wait(timeout=10)
            except subprocess.TimeoutExpired:
                self.proc.kill()
                self.proc.wait(timeout=10)
        self.proc = None
