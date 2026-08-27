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
```

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
