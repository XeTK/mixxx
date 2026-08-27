# Mixxx E2E accessibility testing (`tools/e2e/`)

True end-to-end tests for the accessibility fork (Spec 04). These drive the
**real running Mixxx** via the OS accessibility tree (macOS `AXUIElement`) and
assert on the **spoken output** captured through the `--tts-log` hook — the two
things a blind user actually experiences.

## Architecture

```
run_e2e.py            orchestrator: launch Mixxx, wait for window, run scenario, tear down
ax_driver.py          shared AX-tree driver + TTS-log assertion helpers + MixxxProcess
m1_boot_speech.py     M1: assert "Mixxx ready" is spoken and the window appears
m2_ddj400_menu.py     M2: DDJ-400 menu open/navigate/activate/back via emulator + TTS log
m3_accessible_names.py M3: open Preferences via AX tree, assert pages are exposed
m4_keyboard_readout.py M3: Alt+1 -> assert new speech in the TTS log

library_fixture.py    shared: seed a throwaway mixxxdb.sqlite with fixture tracks
library_actions.py    shared: focus/select the track table, drive its context menu
d1_purge_enter_safe.py               issue #104: Return on the purge dialog purges nothing
d2_purge_escape_safe.py              issue #104: Escape on the purge dialog purges nothing
d3_purge_narrates_before_dialog.py   issue #104: purge announces itself verbatim first
d4_hide_enter_safe.py                issue #104: Return on the hide dialog hides nothing
d5_delete_enter_safe.py              issue #104: Return on the delete dialog deletes nothing
d6_delete_escape_safe.py             issue #104: Escape on the delete dialog deletes nothing
d7_no_destructive_action_fires_immediately.py  issue #104: Purge/Hide/Delete all confirm first
d8_repeated_enter_stack_safe.py      issue #104: 3 rapid Returns on a hide dialog stay safe
```

The `d*` scenarios automate a slice of `features/destructive_actions.feature`
(the manual Gherkin test plan) as real regression tests. See
`features/COVERAGE.md`'s "Automated coverage (issue #104)" section for which
scenarios from that file are covered this way, and which remain manual-only
and why.

`ax_driver.py` abstracts the OS accessibility backend behind `AxBackend`. The
macOS backend (`MacAxBackend`, PyObjC `Quartz`/`ApplicationServices`) is
implemented; a Linux AT-SPI or Windows UIA backend can be added later without
changing scenario code.

## Prerequisites

- macOS with **Accessibility permission** granted to the terminal running the
  tests (System Settings → Privacy & Security → Accessibility) so it can drive
  `AXUIElement`.
- A built `mixxx` binary on `PATH`, or set `MIXXX_BIN` / pass `--mixxx`.
- For M2: `pip install mido python-rtmidi` and the IAC driver enabled (see
  `tools/ddj400-emulator/README.md`), plus the DDJ-400 mapping assigned to the
  emulator's virtual port in Mixxx.

## Run

```sh
# All scenarios
python3 tools/e2e/run_e2e.py --scenario m1_boot_speech
python3 tools/e2e/run_e2e.py --scenario m3_accessible_names
python3 tools/e2e/run_e2e.py --scenario m4_keyboard_readout

# M2 needs the DDJ-400 mapping assigned; point at a custom port if needed
DDJ400_PORT="My Port" python3 tools/e2e/run_e2e.py --scenario m2_ddj400_menu

# Destructive-actions slice (issue #104) -- each seeds its own throwaway
# library via a `prepare(settings_dir, mixxx_bin)` hook before Mixxx launches
python3 tools/e2e/run_e2e.py --scenario d1_purge_enter_safe
python3 tools/e2e/run_e2e.py --scenario d4_hide_enter_safe
python3 tools/e2e/run_e2e.py --scenario d5_delete_enter_safe
```

Each run uses a throwaway settings dir and a fresh `--tts-log`, then tears down.
Use `--keep` to retain the workdir for debugging, or `--no-launch` to attach to
an already-running Mixxx (with `--tts-log` pointing at its log).

## Mapping to Spec 04 milestones

| Milestone | Scenario | Status |
|---|---|---|
| M1 (`--tts-log` + AX driver + boot speech) | `m1_boot_speech` | harness written; needs live run |
| M2 (DDJ-400 menu via emulator + TTS log) | `m2_ddj400_menu` | harness written; needs live run |
| M3 (accessible names + keyboard readout) | `m3_accessible_names`, `m4_keyboard_readout` | refactored onto shared driver; needs live run |
| M4 (full inventory + CI on Linux) | — | not started |
