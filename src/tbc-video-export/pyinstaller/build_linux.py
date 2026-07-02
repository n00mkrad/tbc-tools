from __future__ import annotations

import os

if os.name != "posix":
    raise SystemExit("Must be run on Linux")

import dunamai
import PyInstaller.__main__
from tbc_video_export.common import consts

# use .99 as the 4th integer if non-final release
is_final = dunamai.Version.parse(consts.PROJECT_VERSION).stage == ""
version = (
    f"{dunamai.Version.parse(consts.PROJECT_VERSION).base}{'' if is_final else '.99'}"
)

print(f"Building Linux binary version {version}")

# Build a single self-contained ELF binary (matches the Windows --onefile flow).
# The Linux release previously shipped a bash wrapper that exec'd a *host* Python
# with host site-packages, which fails on clean machines without scipy/etc. This
# frozen binary bundles the interpreter and all required deps.
PyInstaller.__main__.run(
    [
        "src/tbc_video_export/__main__.py",
        "--clean",
        "--collect-submodules",
        "application",
        "--onefile",
        "--name",
        "tbc-video-export",
    ]
)
