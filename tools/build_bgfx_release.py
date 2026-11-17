"""Builds the Release (/MT) bx and bimg libraries the Release configuration of ff8_demaster links.

The repository carries only Debug builds of them (vendor/lib/bgfx/*Debug.lib). Their sources are the upstream
repositories at the commits whose headers are, byte for byte, the ones in vendor/include:
    git clone https://github.com/bkaradzic/bx.git   .build/bx-src   && git -C .build/bx-src checkout 0455026c42c9ed47d44c098a939bb7001a3ff631
    git clone https://github.com/bkaradzic/bimg.git .build/bimg-src && git -C .build/bimg-src checkout 5ee42f6
(bx: the 64 headers match from b1a707f to 0455026, 2020-05-06 to 2020-05-13; bimg: the 3 headers match from 2020-03-04 to
2021-01-11, and 5ee42f6 is its last commit before bx's next one, 2020-05-15.)
The source lists and the options are those of their own GENie scripts (bx/scripts/toolchain.lua for Visual Studio,
Release: /MT /O2 /Ob2 /GS- /arch:SSE2, no RTTI, no exceptions; bx.lua, bimg.lua, bimg_decode.lua, bimg_encode.lua). No
downloaded executable is run: the sources are compiled with this machine's cl.

Usage: python tools/build_bgfx_release.py   (writes vendor/lib/bgfx/{bx,bimg,bimg_decode,bimg_encode}Release.lib)
"""
import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BX = ROOT / ".build" / "bx-src"
BIMG = ROOT / ".build" / "bimg-src"
OUT = ROOT / "ff8_demaster" / "vendor" / "lib" / "bgfx"
OBJ = ROOT / ".build" / "bgfx-release-obj"
EXPECTED = {BX: "0455026c42c9ed47d44c098a939bb7001a3ff631", BIMG: "5ee42f6"}

for repository, commit in EXPECTED.items():
    head = subprocess.run(["git", "-C", str(repository), "rev-parse", "HEAD"], capture_output=True, text=True).stdout.strip()
    if not head.startswith(commit):
        sys.exit(f"{repository} is at {head or 'nothing'}, not {commit} (see the docstring)")

# The x86 compiler environment of the installed Visual Studio.
vswhere = Path(os.environ["ProgramFiles(x86)"]) / "Microsoft Visual Studio" / "Installer" / "vswhere.exe"
install = subprocess.run([str(vswhere), "-latest", "-products", "*", "-property", "installationPath"],
                         capture_output=True, text=True).stdout.strip()
vcvars = Path(install) / "VC" / "Auxiliary" / "Build" / "vcvars32.bat"
environment = subprocess.run(f'cmd /c ""{vcvars}" >nul && set"', capture_output=True, text=True, shell=True).stdout
env = dict(line.split("=", 1) for line in environment.splitlines() if "=" in line)
# Windows looks a program up in this process's PATH, not the one given to the child: name the tools by full path.
import shutil
SEARCH = env.get("PATH") or env.get("Path") or ""
CL, LIB = shutil.which("cl", path=SEARCH), shutil.which("lib", path=SEARCH)
if not CL or not LIB:
    sys.exit(f"cl or lib not found through {vcvars}")

COMMON = ["/nologo", "/c", "/MT", "/O2", "/Ob2", "/GS-", "/arch:SSE2", "/GR-", "/Oy", "/Z7", "/W3", "/wd4201", "/wd4324",
          "/D__STDC_LIMIT_MACROS", "/D__STDC_FORMAT_MACROS", "/D__STDC_CONSTANT_MACROS", "/DNDEBUG", "/DWIN32", "/D_WIN32",
          "/D_HAS_EXCEPTIONS=0", "/D_SCL_SECURE=0", "/D_SECURE_SCL=0", "/D_SCL_SECURE_NO_WARNINGS",
          "/D_CRT_SECURE_NO_WARNINGS", "/D_CRT_SECURE_NO_DEPRECATE", f"/I{BX / 'include' / 'compat' / 'msvc'}"]


def build(name, sources, includes):
    folder = OBJ / name
    folder.mkdir(parents=True, exist_ok=True)
    objects = []
    for index, source in enumerate(sources):
        target = folder / f"{index:03d}_{source.stem}.obj"
        language = ["/TC"] if source.suffix == ".c" else ["/TP", "/std:c++14"]
        command = [CL, *COMMON, *language, *[f"/I{path}" for path in includes], f"/Fo{target}", str(source)]
        result = subprocess.run(command, env=env, capture_output=True, text=True)
        if result.returncode:
            sys.exit(f"{name}: {source}\n{result.stdout}\n{result.stderr}")
        objects.append(str(target))
    library = OUT / f"{name}Release.lib"
    result = subprocess.run([LIB, "/nologo", f"/OUT:{library}", *objects], env=env, capture_output=True, text=True)
    if result.returncode:
        sys.exit(f"{name}: lib\n{result.stdout}\n{result.stderr}")
    print(f"{library.name}: {len(sources)} sources")


def glob(folder, *patterns):
    found = []
    for pattern in patterns:
        found.extend(sorted(folder.glob(pattern)))
    return found


ASTC_CODEC = BIMG / "3rdparty" / "astc-codec"
build("bx", [BX / "src" / "amalgamated.cpp"], [BX / "include", BX / "3rdparty"])
build("bimg",
      [BIMG / "src" / "image.cpp", BIMG / "src" / "image_gnf.cpp"]
      + [ASTC_CODEC / "src" / "decoder" / f"{stem}.cc" for stem in (
          "astc_file", "codec", "endpoint_codec", "footprint", "integer_sequence_codec", "intermediate_astc_block",
          "logical_astc_block", "partition", "physical_astc_block", "quantization", "weight_infill")],
      [BX / "include", BIMG / "include", ASTC_CODEC, ASTC_CODEC / "include"])
build("bimg_decode", [BIMG / "src" / "image_decode.cpp"], [BX / "include", BIMG / "include", BIMG / "3rdparty"])
third = BIMG / "3rdparty"
build("bimg_encode",
      [BIMG / "src" / "image_encode.cpp", BIMG / "src" / "image_cubemap_filter.cpp"]
      + glob(third, "libsquish/**/*.cpp", "edtaa3/**/*.cpp", "etc1/**/*.cpp", "etc2/**/*.cpp", "nvtt/**/*.cpp",
             "pvrtc/**/*.cpp", "astc/**/*.cpp", "iqa/source/**/*.c"),
      [BX / "include", BIMG / "include", third, third / "nvtt", third / "iqa" / "include"])
