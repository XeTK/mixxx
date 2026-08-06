# DDJ-400 Emulator

A software **Pioneer DDJ-400 emulator** for developing and testing
DDJ-400-driven accessibility features in Mixxx **without physical hardware**.

It creates a **virtual MIDI port** that Mixxx (which uses PortMidi for MIDI
input) sees as an input device, and sends the exact MIDI messages a real
DDJ-400 sends. It provides both an interactive keyboard front end and a
scripted mode for automated regression tests.

> This is a dev/test tool. It is **not** shipped code and does not modify any
> part of Mixxx. All files live under `tools/ddj400-emulator/`.

## Why

- No physical DDJ-400 is guaranteed on hand during development.
- Iterating on the script bindings and the spoken menu needs a way to send
  the exact MIDI messages the hardware sends.
- The emulator doubles as a test harness for CI/regression.

## How it works

Mixxx uses **PortMidi** for MIDI input (`src/controllers/midi/portmidicontroller.cpp`).
On macOS, PortMidi can open **virtual MIDI ports** (CoreMIDI virtual sources).
This emulator opens a virtual MIDI **output** port; Mixxx sees it as an
**input** device and can be assigned the DDJ-400 mapping.

The MIDI numbers sent are verified against
`res/controllers/Pioneer-DDJ-400.midi.xml`:

| Control | MIDI | Value semantics |
|---|---|---|
| BROWSE knob rotate | CC `0xB6` / `0x40` | `0x41` = up, `0x3F` = down (relative) |
| BROWSE press | Note `0x96` / `0x41` | `0x7F` down, `0x00` up |
| BROWSE + SHIFT press | Note `0x96` / `0x42` | `0x7F` down, `0x00` up |
| LOAD Deck1 | Note `0x96` / `0x46` | `0x7F` down, `0x00` up |
| LOAD Deck2 | Note `0x96` / `0x47` | `0x7F` down, `0x00` up |
| SHIFT Deck1 | Note `0x90` / `0x3F` | `0x7F` down, `0x00` up |
| SHIFT Deck2 | Note `0x91` / `0x3F` | `0x7F` down, `0x00` up |

## Connecting to Mixxx

Mixxx matches a controller to its mapping by **device name / product info**, so
a virtual MIDI port will **not** be automatically recognized as a
"Pioneer DDJ-400". You must assign the mapping manually:

1. Start Mixxx and open **Options → Preferences → Controllers**.
2. The virtual port appears in the controller list as `DDJ-400 Emulator` (or
   whatever `--port` name you used), but it is **not** auto-matched to the
   DDJ-400 mapping.
3. Select the virtual port and manually load/assign the **Pioneer DDJ-400**
   mapping (`res/controllers/Pioneer-DDJ-400.midi.xml`).

> The DDJ-400 mapping has an optional **`accessibilityPads`** setting that can
> be enabled for the spoken-feedback pads.

## Install dependencies

```sh
pip install mido python-rtmidi
```

`mido` is a thin wrapper; `python-rtmidi` provides the actual virtual-port
support. The script imports them lazily, so it still parses, compiles, and
runs in `--dry-run` mode without them installed.

## Enable the virtual MIDI driver (macOS)

On macOS, CoreMIDI virtual ports are provided by the built-in **IAC Driver**:

1. Open **Audio MIDI Setup** (Applications → Utilities).
2. If the **IAC Driver** is not visible, choose **Window → Show MIDI Studio**.
3. Double-click **IAC Driver** (or open its properties).
4. Tick **"Device is online"** to enable it.
5. (Optional) Add a bus and name it, e.g. `DDJ-400 Emulator`.

The emulator opens a CoreMIDI virtual output port directly via
`python-rtmidi`; the IAC driver is the standard way to expose a software
port that other apps (Mixxx) can open. If you prefer, you can instead point
the emulator at an IAC bus by naming the port to match.

## Run it

### Interactive mode

```sh
python3 tools/ddj400-emulator/ddj400_emulator.py
```

The emulator opens a virtual MIDI port named **`DDJ-400 Emulator`** and reads
keystrokes from stdin. In Mixxx, go to **Options → Preferences → Controllers**,
add a new MIDI controller, and select the `DDJ-400 Emulator` input device.
Assign it the Pioneer DDJ-400 mapping (or the accessibility fork's mapping).

### Key mapping

| Key | Action |
|---|---|
| `Up` / `Down` | BROWSE rotate (up / down) |
| `Enter` | BROWSE press (short click) |
| `Shift`+`Enter` | BROWSE + SHIFT press (back) |
| `L` | LOAD Deck1 |
| `R` | LOAD Deck2 |
| `S` | SHIFT (hold while held) |
| `H` | show help |
| `Q` / `Ctrl+C` | quit |

**Hold semantics:** press-and-hold a key (e.g. `Enter`) sends the note-down
immediately, keeps it held, and sends note-up when you release. Holding
`Enter` for more than **0.4s** is treated as the "hold to open the menu"
gesture.

The interactive front end uses a simple line protocol so it works in any
terminal without curses. A line ending in `+` means "key held down"; a bare
line means "release". For example, to hold BROWSE open the menu:

```
enter
enter          <- release after >0.4s -> menu opens
down
down
enter
```

### Scripted mode

```sh
python3 tools/ddj400-emulator/ddj400_emulator.py --script example_sequence.txt
```

The sequence file format is documented at the top of `example_sequence.txt`.
It supports explicit hold durations, so the hold-to-open gesture can be
replayed deterministically for regression tests.

### Dry-run (no MIDI device)

```sh
python3 tools/ddj400-emulator/ddj400_emulator.py --dry-run --script example_sequence.txt
```

Prints the MIDI messages instead of sending them. Useful for verifying the
sequence logic without a virtual driver or the Python MIDI libraries.

## Smoke-test sequence

1. Start Mixxx.
2. In **Preferences → Controllers**, add a MIDI controller and select the
   `DDJ-400 Emulator` input device; assign the DDJ-400 mapping.
3. Run the emulator in interactive mode.
4. **Hold `Enter`** for >0.4s — the spoken library menu should open.
5. Press `Down` / `Up` to scroll through items.
6. Press `Enter` (short) to activate the focused item.
7. Press `Shift`+`Enter` to go back.

Or run the whole flow automatically:

```sh
python3 tools/ddj400-emulator/ddj400_emulator.py --script example_sequence.txt
```

## Files

- `ddj400_emulator.py` — the emulator (virtual MIDI port, interactive + scripted modes).
- `example_sequence.txt` — sample scripted sequence (hold-to-open + navigate + activate).
- `README.md` — this file.
