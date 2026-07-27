#!/usr/bin/env python3
"""Fail if Upstream differs from a specified Git reference."""

from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path


def git(repo: Path, *arguments: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        ["git", "-C", str(repo), *arguments],
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )


def find_repository() -> tuple[Path | None, str | None]:
    try:
        result = subprocess.run(
            ["git", "rev-parse", "--show-toplevel"],
            check=False,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )
    except FileNotFoundError:
        return None, "Git is unavailable; ensure 'git' is installed and on PATH."
    if result.returncode:
        detail = result.stderr.strip() or "the current directory is not in a Git repository"
        return None, f"Unable to locate a Git repository: {detail}"
    return Path(result.stdout.strip()), None


def changed_paths(repo: Path, reference: str) -> tuple[list[str] | None, str | None]:
    verified = git(repo, "rev-parse", "--verify", f"{reference}^{{commit}}")
    if verified.returncode:
        detail = verified.stderr.strip() or "not a commit"
        return None, f"Git reference '{reference}' is unavailable or invalid: {detail}"

    differences = git(repo, "diff", "--name-status", "--find-renames", reference, "--", "Upstream")
    if differences.returncode:
        return None, f"Unable to compare Upstream/: {differences.stderr.strip()}"

    paths: set[str] = set()
    for line in differences.stdout.splitlines():
        fields = line.split("\t")
        paths.update(field for field in fields[1:] if field.startswith("Upstream/"))

    untracked = git(repo, "ls-files", "--others", "--exclude-standard", "--", "Upstream")
    if untracked.returncode:
        return None, f"Unable to inspect untracked files: {untracked.stderr.strip()}"
    paths.update(path for path in untracked.stdout.splitlines() if path)
    return sorted(paths), None


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("git_reference", help="commit, tag, or branch to compare against")
    args = parser.parse_args()

    repo, error = find_repository()
    if error:
        print(f"error: {error}", file=sys.stderr)
        return 2

    paths, error = changed_paths(repo, args.git_reference)
    if error:
        print(f"error: {error}", file=sys.stderr)
        return 2
    if paths:
        print(f"Upstream/ differs from {args.git_reference}:", file=sys.stderr)
        for path in paths:
            print(f"  {path}", file=sys.stderr)
        return 1

    print(f"Upstream/ is unchanged from {args.git_reference}.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
