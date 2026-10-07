#!/usr/bin/env python3
"""
Architecture enforcement script for FreeCut-CPP.

Enforces:
1. engine/ must NEVER include any Qt headers (<Q...> or <Qt...>) or link Qt.
2. engine/<module> may only include headers from modules it is explicitly
   allowed to depend on according to the ALLOWED_DEPENDENCIES table.
"""

from __future__ import annotations

import argparse
import os
import re
import sys
from pathlib import Path

# Dependency rules: module -> list of allowed module dependencies
# core -> model -> commands -> keyframes -> storage -> media -> playback ->
# render -> effects -> audio -> export -> plugins -> analysis
ALLOWED_DEPENDENCIES: dict[str, list[str]] = {
    "core": [],
    "model": ["core"],
    "keyframes": ["core"],
    "commands": ["core", "model", "keyframes"],
    "storage": ["core", "model", "keyframes"],
    "media": ["core"],
    "playback": ["core", "model", "media"],
    "render": ["core", "model", "keyframes", "media"],
    "effects": ["core", "keyframes", "render"],
    "audio": ["core", "model", "media"],
    "export": ["core", "model", "media", "render", "audio", "playback"],
    "plugins": ["core", "effects", "render"],
    "analysis": ["core", "media"],
}

# Regex to detect Qt header includes
QT_INCLUDE_REGEX = re.compile(
    r'^\s*#\s*include\s*[<"](?:Q[A-Z0-9][a-zA-Z0-9_]*|Qt[A-Za-z0-9_/]+)[>"]'
)

# Regex to detect Qt CMake link/find_package calls
QT_CMAKE_REGEX = re.compile(
    r'\b(?:find_package\s*\(\s*Qt[56]|Qt[56]::[A-Za-z0-9_]+)\b',
    re.IGNORECASE,
)

# Regex to extract #include <freecut/<module>/...>
FREECUT_INCLUDE_REGEX = re.compile(
    r'^\s*#\s*include\s*[<"]freecut/([a-zA-Z0-9_]+)/[^>"]+[>"]'
)

SOURCE_EXTENSIONS = {".hpp", ".h", ".cpp", ".cc", ".cxx", ".c"}
CMAKE_EXTENSIONS = {".cmake"}


def scan_file_for_qt_links(file_path: Path) -> list[str]:
    violations: list[str] = []
    try:
        content = file_path.read_text(encoding="utf-8", errors="replace")
    except OSError as err:
        return [f"{file_path}: Unable to read file: {err}"]

    for line_idx, line in enumerate(content.splitlines(), start=1):
        if QT_CMAKE_REGEX.search(line):
            violations.append(
                f"{file_path}:{line_idx}: Forbidden Qt reference in engine build file: '{line.strip()}'"
            )
    return violations


def scan_source_file(
    file_path: Path, module_name: str
) -> list[str]:
    violations: list[str] = []
    try:
        content = file_path.read_text(encoding="utf-8", errors="replace")
    except OSError as err:
        return [f"{file_path}: Unable to read file: {err}"]

    allowed = ALLOWED_DEPENDENCIES.get(module_name)
    if allowed is None:
        violations.append(
            f"{file_path}: Module '{module_name}' is not recognized in ALLOWED_DEPENDENCIES table."
        )
        return violations

    for line_idx, line in enumerate(content.splitlines(), start=1):
        # 1. Check forbidden Qt includes
        if QT_INCLUDE_REGEX.search(line):
            violations.append(
                f"{file_path}:{line_idx}: Forbidden Qt header include in engine: '{line.strip()}'"
            )

        # 2. Check engine module includes
        match = FREECUT_INCLUDE_REGEX.search(line)
        if match:
            included_mod = match.group(1)
            # Intra-module include is always allowed
            if included_mod != module_name:
                if included_mod not in ALLOWED_DEPENDENCIES:
                    violations.append(
                        f"{file_path}:{line_idx}: Includes unknown module '{included_mod}': '{line.strip()}'"
                    )
                elif included_mod not in allowed:
                    violations.append(
                        f"{file_path}:{line_idx}: Module '{module_name}' is not allowed to depend on "
                        f"'{included_mod}'. Allowed dependencies: {allowed}. Line: '{line.strip()}'"
                    )

    return violations


def run_checks(repo_root: Path) -> list[str]:
    violations: list[str] = []
    engine_dir = repo_root / "engine"

    if not engine_dir.is_dir():
        return [f"Engine directory not found at '{engine_dir}'"]

    for root, dirs, files in os.walk(engine_dir):
        root_path = Path(root)
        rel_to_engine = root_path.relative_to(engine_dir)

        # Determine module name if inside a module directory
        module_name = rel_to_engine.parts[0] if rel_to_engine.parts else None

        for fname in files:
            file_path = root_path / fname

            # Check CMake files for Qt linking
            if fname == "CMakeLists.txt" or file_path.suffix.lower() in CMAKE_EXTENSIONS:
                violations.extend(scan_file_for_qt_links(file_path))

            # Check C++ source/header files
            if file_path.suffix.lower() in SOURCE_EXTENSIONS:
                if module_name:
                    violations.extend(scan_source_file(file_path, module_name))
                else:
                    # Source file directly in engine/ without a module folder
                    violations.append(
                        f"{file_path}: Source file placed directly in engine/ root; must belong to a module."
                    )

    return violations


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Verify architectural boundaries of FreeCut-CPP engine."
    )
    parser.add_argument(
        "--root",
        type=Path,
        default=Path(__file__).resolve().parent.parent,
        help="Repository root directory (defaults to parent of scripts/)",
    )
    args = parser.parse_args()
    repo_root = args.root.resolve()

    print("======================================================================")
    print("FreeCut-CPP Architecture Enforcement")
    print(f"Scanning repository root: {repo_root}")
    print("======================================================================")

    violations = run_checks(repo_root)

    if violations:
        print(f"\nFAILURE: Found {len(violations)} architecture violation(s):\n")
        for v in violations:
            print(f"  [ERROR] {v}")
        print("\nPlease fix the above violations before committing or merging.")
        return 1

    print("\nSUCCESS: All architecture boundary checks passed without violations.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
