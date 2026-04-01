#!/usr/bin/env python3

from __future__ import annotations

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
WINDOWS_WORKFLOW = ROOT / ".github/workflows/build_windows_tools.yml"
LINUX_WORKFLOW = ROOT / ".github/workflows/build_linux_tools.yml"
WINDOWS_REQUIREMENTS = ROOT / "src/tbc-video-export/pyinstaller/requirements-build-windows.txt"
BUNDLE_VERIFY_SCRIPT = ROOT / "ci/verify_linux_bundle.sh"

XCB_RUNTIME_LIBS = (
    "libxcb-cursor.so.0",
    "libxcb-icccm.so.4",
    "libxcb-image.so.0",
    "libxcb-keysyms.so.1",
    "libxcb-render-util.so.0",
    "libxkbcommon.so.0",
    "libxkbcommon-x11.so.0",
)

WINDOWS_REQUIRED_SNIPPETS = (
    'VCPKG_COMMIT:',
    'cache: "pip"',
    "requirements-build-windows.txt",
    "import pywintypes, win32file, win32pipe",
    "vcpkg-binary-cache-${{ env.VCPKG_COMMIT }}",
)

LINUX_REQUIRED_SNIPPETS = (
    "gcc-c++ python3",
)

WINDOWS_REQUIRED_PACKAGES = (
    "pyinstaller",
    "pyinstaller-versionfile",
    "dunamai",
    "pywin32",
)


def check_contains(path: Path, snippet: str, errors: list[str]) -> None:
    content = path.read_text(encoding="utf-8")
    if snippet not in content:
        errors.append(f"{path}: missing required snippet: {snippet!r}")


def check_count_at_least(path: Path, snippet: str, minimum: int, errors: list[str]) -> None:
    content = path.read_text(encoding="utf-8")
    count = content.count(snippet)
    if count < minimum:
        errors.append(
            f"{path}: expected snippet {snippet!r} at least {minimum} times, found {count}"
        )


def main() -> int:
    errors: list[str] = []

    for required_file in (
        WINDOWS_WORKFLOW,
        LINUX_WORKFLOW,
        WINDOWS_REQUIREMENTS,
        BUNDLE_VERIFY_SCRIPT,
    ):
        if not required_file.exists():
            errors.append(f"missing required file: {required_file}")

    if errors:
        print("CI contract checks failed:")
        for error in errors:
            print(f" - {error}")
        return 1

    for snippet in WINDOWS_REQUIRED_SNIPPETS:
        check_contains(WINDOWS_WORKFLOW, snippet, errors)

    for snippet in LINUX_REQUIRED_SNIPPETS:
        check_contains(LINUX_WORKFLOW, snippet, errors)

    requirements_content = WINDOWS_REQUIREMENTS.read_text(encoding="utf-8")
    requirements_lines = {
        line.strip()
        for line in requirements_content.splitlines()
        if line.strip() and not line.strip().startswith("#")
    }
    for package in WINDOWS_REQUIRED_PACKAGES:
        if package not in requirements_lines:
            errors.append(
                f"{WINDOWS_REQUIREMENTS}: missing required package line: {package}"
            )

    for runtime_lib in XCB_RUNTIME_LIBS:
        check_count_at_least(LINUX_WORKFLOW, runtime_lib, 2, errors)
        check_contains(BUNDLE_VERIFY_SCRIPT, runtime_lib, errors)

    if errors:
        print("CI contract checks failed:")
        for error in errors:
            print(f" - {error}")
        return 1

    print("CI contract checks passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
