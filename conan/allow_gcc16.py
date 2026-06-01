#!/usr/bin/env python3
import sys
from pathlib import Path


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: allow_gcc16.py SETTINGS_YML", file=sys.stderr)
        return 2

    settings_path = Path(sys.argv[1])
    text = settings_path.read_text()
    old = '"15", "15.1", "15.2"]'
    new = '"15", "15.1", "15.2", "16"]'

    if new in text:
        return 0
    if old not in text:
        print(f"error: cannot find GCC version list in {settings_path}", file=sys.stderr)
        return 1

    settings_path.write_text(text.replace(old, new, 1))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
