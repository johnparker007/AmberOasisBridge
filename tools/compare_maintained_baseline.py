#!/usr/bin/env python3
"""Compare behaviour-relevant files in maintained cores with their upstream baseline."""
from __future__ import annotations

import argparse
from pathlib import Path

SOURCE_SUFFIXES = {".c", ".cpp", ".h", ".hpp", ".inc"}
CORE_PATHS = {
    "Epoch": (Path("Upstream/Epoch/EpochCore/EpochCore"), Path("src/Cores/Epoch")),
    "JPMSystem6": (Path("Upstream/JPMSystem6"), Path("src/Cores/JPMSystem6")),
    "MPU5": (Path("Upstream/MPU5"), Path("src/Cores/MPU5")),
}


def compare(root: Path) -> tuple[list[str], list[str], list[str]]:
    identical, missing, different = [], [], []
    for core, (upstream, maintained) in CORE_PATHS.items():
        source_files = sorted(p for p in (root / upstream).iterdir() if p.suffix.lower() in SOURCE_SUFFIXES)
        for source in source_files:
            destination = root / maintained / source.name
            label = f"{core}/{source.name}"
            if not destination.exists():
                missing.append(label)
            elif source.read_bytes() == destination.read_bytes():
                identical.append(label)
            else:
                different.append(label)
    return identical, missing, different


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    args = parser.parse_args()
    identical, missing, different = compare(args.root)
    for label in identical:
        print(f"IDENTICAL {label}")
    for label in missing:
        print(f"MISSING {label}")
    for label in different:
        print(f"DIFFERENT {label}")
    print(f"Summary: {len(identical)} identical, {len(missing)} missing, {len(different)} different")
    return 1 if missing or different else 0


if __name__ == "__main__":
    raise SystemExit(main())
