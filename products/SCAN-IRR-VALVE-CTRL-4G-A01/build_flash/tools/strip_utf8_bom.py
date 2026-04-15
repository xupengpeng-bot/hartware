"""Remove UTF-8 BOM from *.cmd in build_flash if present (fixes '﻿@echo' is not recognized)."""
from __future__ import annotations

import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BOM = b"\xef\xbb\xbf"


def main() -> None:
    n = 0
    for name in os.listdir(ROOT):
        if not name.endswith(".cmd"):
            continue
        path = os.path.join(ROOT, name)
        if not os.path.isfile(path):
            continue
        with open(path, "rb") as f:
            data = f.read()
        if data.startswith(BOM):
            with open(path, "wb") as f:
                f.write(data[len(BOM) :])
            print("stripped BOM:", name)
            n += 1
    if n == 0:
        print("no BOM found in *.cmd")


if __name__ == "__main__":
    main()
