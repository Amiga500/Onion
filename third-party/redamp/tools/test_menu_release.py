#!/usr/bin/env python3
"""Host UI regression: MENU exits on release but system key chords do not."""
import os
import pathlib
import subprocess
import tempfile

ROOT = pathlib.Path(__file__).resolve().parent.parent
APP = pathlib.Path(os.getenv("REDAMP_APP_DIR", ROOT.parent / "App" / "RedAmp"))
BIN = ROOT / "build" / "redamp-host"

# M2D_HOST input tuples are (frame, evdev code, value).
CASES = {
    "menu-release": ([(3, 1, 1), (12, 1, 0)], False),
    "menu-power": ([(3, 1, 1), (5, 116, 1), (7, 116, 0), (12, 1, 0)], True),
    "power-menu": ([(2, 116, 1), (3, 1, 1), (12, 1, 0), (14, 116, 0)], True),
    "menu-x": ([(3, 1, 1), (5, 42, 1), (7, 42, 0), (12, 1, 0)], True),
    "menu-select": ([(3, 1, 1), (5, 97, 1), (7, 97, 0), (12, 1, 0)], True),
}


def main():
    if not BIN.is_file():
        raise SystemExit("Host player missing: run make host")
    if not (APP / "skins" / "RedOnion.wsz").is_file():
        raise SystemExit(f"RedAmp App assets missing: {APP}")
    with tempfile.TemporaryDirectory(prefix="redamp-menu-test-") as temp:
        tmp = pathlib.Path(temp)
        for name, (events, should_remain_open) in CASES.items():
            keys = tmp / f"{name}.txt"
            keys.write_text("".join(f"{f} {k} {v}\n" for f, k, v in events))
            dump = tmp / f"{name}-frames"
            data = tmp / f"{name}-data"
            dump.mkdir()
            data.mkdir()
            env = dict(os.environ)
            env.update({
                "MA_HOME": str(APP), "MA_ROOT": str(tmp), "MA_DATA": str(data),
                "M2D_INPUT": str(keys), "M2D_DUMP": str(dump),
                "M2D_DUMP_EVERY": "10", "MA_MAX_FRAMES": "24",
            })
            run = subprocess.run([str(BIN)], env=env, capture_output=True, timeout=15)
            if run.returncode:
                raise AssertionError(f"{name}: player exit {run.returncode}: "
                                     f"{run.stderr.decode(errors='replace')[-500:]}")
            at_10 = (dump / "f000010.ppm").is_file()
            at_20 = (dump / "f000020.ppm").is_file()
            if not at_10 or at_20 != should_remain_open:
                raise AssertionError(f"{name}: frames at 10={at_10}, 20={at_20}; "
                                     f"expected open={should_remain_open}")
            print(f"PASS {name}")
    print("MENU release and screenshot chord regression tests passed")


if __name__ == "__main__":
    main()
