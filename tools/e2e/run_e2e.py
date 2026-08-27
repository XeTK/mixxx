#!/usr/bin/env python3
"""E2E orchestrator: launch Mixxx, run a scenario, assert, tear down.

Drives the REAL running Mixxx via the OS accessibility tree and the ``--tts-log``
hook (Spec 04). Each scenario is a Python module under ``tools/e2e/`` exposing a
``run(driver, tts)`` function (or a ``main()`` that accepts the driver/tts).

Usage::

    python3 tools/e2e/run_e2e.py --scenario m1_boot_speech
    python3 tools/e2e/run_e2e.py --scenario m2_ddj400_menu --emulator-port "DDJ-400 Emulator"

The orchestrator:
  1. Creates a throwaway settings dir + a fresh ``--tts-log`` file.
  2. Launches Mixxx with ``--settings-path`` and ``--tts-log``.
  3. Waits for the main window to appear in the AX tree.
  4. Runs the scenario's ``run(driver, tts)``.
  5. Asserts the scenario did not raise, then tears down.
"""
import argparse
import importlib.util
import os
import shutil
import sys
import tempfile
import time

from ax_driver import AxDriver, MixxxProcess, TtsLog, create_backend

HERE = os.path.dirname(os.path.abspath(__file__))


def _scenario_path(name):
    return os.path.join(HERE, name + ".py")


def _load_scenario(name):
    path = _scenario_path(name)
    if not os.path.exists(path):
        sys.exit(f"FAIL: no scenario module {name!r} at {path}")
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    if not hasattr(module, "run"):
        sys.exit(f"FAIL: scenario {name!r} must define run(driver, tts)")
    return module


def _default_mixxx_bin():
    # Allow an override via env; otherwise assume `mixxx` is on PATH.
    return os.environ.get("MIXXX_BIN", "mixxx")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--scenario", required=True,
        help="Scenario module name (e.g. m1_boot_speech, m2_ddj400_menu)",
    )
    parser.add_argument(
        "--mixxx", default=_default_mixxx_bin(),
        help="Path to the Mixxx binary (default: $MIXXX_BIN or 'mixxx')",
    )
    parser.add_argument(
        "--workdir", default=None,
        help="Directory for the throwaway settings + tts log. A temp dir is "
             "used if omitted.",
    )
    parser.add_argument(
        "--keep", action="store_true",
        help="Keep the workdir after the run (useful for debugging).",
    )
    parser.add_argument(
        "--timeout", type=float, default=60.0,
        help="Seconds to wait for the Mixxx window to appear.",
    )
    parser.add_argument(
        "--no-launch", action="store_true",
        help="Attach to an already-running Mixxx instead of launching one. "
             "Requires --tts-log to point at the running app's log.",
    )
    parser.add_argument(
        "--tts-log", default=None,
        help="TTS log path (used with --no-launch).",
    )
    args, extra = parser.parse_known_args(argv)

    workdir = args.workdir or tempfile.mkdtemp(prefix="mixxx-e2e-")
    os.makedirs(workdir, exist_ok=True)
    tts_log = args.tts_log or os.path.join(workdir, "tts.log")

    scenario = _load_scenario(args.scenario)

    proc = None
    if args.no_launch:
        driver = AxDriver(create_backend()).connect()
    else:
        proc = MixxxProcess(
            mixxx_bin=args.mixxx,
            settings_dir=os.path.join(workdir, "settings"),
            tts_log=tts_log,
            extra_args=["--controller-navigation-without-focus"],
        )
        proc.launch()
        print(f"Launched Mixxx (pid={proc.proc.pid}), tts-log={tts_log}")
        driver = AxDriver(create_backend()).connect(proc.proc.pid)

    tts = TtsLog(tts_log)

    try:
        if not args.no_launch:
            print(f"Waiting for Mixxx window (timeout={args.timeout}s)...")
            if driver.wait_for_window(timeout=args.timeout) is None:
                print("FAIL: Mixxx window never appeared in the AX tree")
                return 1
            print("Window appeared.")

        # Let the boot speech ("Mixxx ready") settle before the scenario runs.
        time.sleep(2.0)

        print(f"Running scenario {args.scenario}...")
        scenario.run(driver, tts)
        print(f"PASS: scenario {args.scenario}")
        return 0
    except SystemExit as exc:
        # Scenario called sys.exit(n) to report failure.
        return exc.code if isinstance(exc.code, int) else 1
    except Exception as exc:  # noqa: BLE001 - report and fail
        print(f"FAIL: scenario {args.scenario} raised: {exc!r}")
        return 1
    finally:
        if proc is not None:
            proc.terminate()
        if not args.keep:
            shutil.rmtree(workdir, ignore_errors=True)


if __name__ == "__main__":
    sys.exit(main())
