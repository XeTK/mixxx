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
        )
        self.Quartz = Quartz
        self._create_app = AXUIElementCreateApplication
        self._copy_attr = AXUIElementCopyAttributeValue
        self._perform = AXUIElementPerformAction
        self._set_attr = AXUIElementSetAttributeValue

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

    def focus(self, el):
        return self._set_attr(el, "AXFocused", True)

    def send_key(self, keycode, modifiers):
        flags = 0
        for name in modifiers:
            flags |= _MODIFIER_FLAGS[name]
        q = self.Quartz
        down = q.CGEventCreateKeyboardEvent(None, keycode, True)
        q.CGEventSetFlags(down, flags)
        q.CGEventPost(q.kCGHIDEventTap, down)
        up = q.CGEventCreateKeyboardEvent(None, keycode, False)
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

    def send_key(self, keycode, modifiers=()):
        """Post a key press. `modifiers` is an iterable of names like 'alt'."""
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
