#!/bin/bash
set -eu
cd "$(dirname "$0")"

# --- Unpack Arguments --------------------------------------------------------
auto_compile_flags=""
for arg in "$@"; do declare $arg='1'; done
if [[ "$#" == "0" ]]; then raid='1'; fi
if [[ "$#" == "1" && "${release:-0}" == "1" ]]; then raid='1'; fi
if [[ "${asan:-0}" == "1" ]]; then
  echo "[asan enabled]"
  auto_compile_flags="$auto_compile_flags -fsanitize=address"
fi

# --- Mix Git Commit ID -------------------------------------------------------
if git rev-parse --is-inside-work-tree >/dev/null 2>&1; then
  auto_compile_flags="$auto_compile_flags -DBUILD_GIT_HASH=\"$(git describe --always --dirty --match "v[0-9]*")\" -DBUILD_GIT_HASH_FULL=$(git rev-parse HEAD)"
fi

# --- Compile/Link Line Definitions -------------------------------------------
os_name="$(uname -s)"
cc_cflags_gcc=""
if [[ "$os_name" == "Darwin" ]]; then
  # macOS (ARM64 or x86_64): no -msse2 (ARM64), no -mcx16 (ARM64)
  cc_cflags_clang=" -fdiagnostics-absolute-paths -Wno-for-loop-analysis -Wno-incompatible-pointer-types-discards-qualifiers -Wno-initializer-overrides -Wno-compare-distinct-pointer-types -Wno-single-bit-bitfield-constant-conversion -Wno-deprecated-declarations -Wno-writable-strings -Wno-unknown-warning-option -Wno-deprecated-register -Wno-unused-local-typedef -Wno-gnu-folding-constant"
  cc_common=${auto_compile_flags}" -I../src/ -I../local/ -g -Wall -Wno-missing-braces -Wno-unused-function -Wno-unused-variable -Wno-unused-but-set-variable -Wno-unused-value -D_USE_MATH_DEFINES -Dgnu_printf=printf"
  cc_link="-lpthread -lm -ldl -framework CoreFoundation -framework CoreServices"
else
  # Linux
  cc_cflags_clang=" -fdiagnostics-absolute-paths -Wno-for-loop-analysis  -Wno-incompatible-pointer-types-discards-qualifiers -Wno-initializer-overrides -Wno-compare-distinct-pointer-types -Wno-single-bit-bitfield-constant-conversion -Wno-deprecated-declarations -Wno-writable-strings -Wno-unknown-warning-option -Wno-deprecated-register -Wno-unused-local-typedef -msse2"
  cc_common=${auto_compile_flags}"-mcx16 -I../src/ -I../local/ -D_GNU_SOURCE -g -Wall -Wno-missing-braces -Wno-unused-function -Wno-unused-variable -Wno-unused-but-set-variable -Wno-unused-value -D_USE_MATH_DEFINES -Dstrdup=_strdup -Dgnu_printf=printf"
  cc_link="-lpthread -lm -lrt -ldl"
fi
cc_debug="-g -O0 -DBUILD_DEBUG=1 ${cc_common}"
cc_release="-g -O2 -DBUILD_DEBUG=0 ${cc_common}"

# --- Per-Build Settings ------------------------------------------------------
cc_link_dll="-fPIC"

# --- External Libraries ------------------------------------------------------
if [[ "$os_name" == "Darwin" ]]; then
  # macOS: FreeType from Homebrew (brew install freetype pkg-config); Cocoa and
  # Metal from the SDK. The UI backends are Objective-C, so raid compiles as it.
  if [[ -x "$(command -v pkg-config)" ]]; then
    cc_font_provider="$(pkg-config --cflags --libs freetype2)"
  else
    cc_font_provider="-I/opt/homebrew/include/freetype2 -L/opt/homebrew/lib -lfreetype"
  fi
  cc_font_provider="$cc_font_provider -framework CoreText"
  cc_os_gfx="-framework Cocoa"
  # Metal renderer; `opengl` builds the OpenGL one (the Mac's first backend)
  if [[ "${opengl:-0}" == "1" ]]; then
    echo "[opengl renderer]"
    cc_render="-DR_BACKEND=R_BACKEND_OPENGL -framework OpenGL"
  else
    cc_render="-framework Metal -framework QuartzCore"
  fi
  cc_icon=""
  cc_objc="-x objective-c -fno-objc-arc"
else
  # Linux
  # sudo apt install -y pkg-config libfreetype6-dev libx11-dev libxext-dev libxfixes-dev libxrandr-dev libgl-dev libegl-dev
  if [[ -x "$(command -v pkg-config)" ]]; then
    cc_font_provider="$(pkg-config --cflags --libs freetype2)"
    cc_os_gfx="$(pkg-config --cflags --libs x11 xext xfixes xrandr)"
    cc_render="$(pkg-config --cflags --libs gl egl)"
  else
    cc_font_provider="-I/usr/include/freetype2 -lfreetype"
    cc_os_gfx="-lX11 -lXext -lXfixes -lXrandr"
    cc_render="-lGL -lEGL"
  fi
  cc_icon="-DLNX_WM_ICON=1"
  cc_objc=""
fi

# --- Choose Compile/Link Lines -----------------------------------------------
if   [[ "${gcc:-0}"   == "1" ]]; then compiler="${CC:-gcc}   $cc_cflags_gcc";   ar="${AR:-ar}";      echo "[gcc compile]";
elif [[ "${clang:-1}" == "1" ]]; then
  if [[ "$os_name" == "Darwin" ]]; then
    compiler="${CC:-clang} $cc_cflags_clang"; ar="${AR:-ar}"; echo "[clang compile]";
  else
    compiler="${CC:-clang} $cc_cflags_clang"; ar="${AR:-llvm-ar}"; echo "[clang compile]";
  fi
fi
if   [[ "${release:-0}" == "1" ]]; then echo "[release mode]"; compile="$compiler $cc_release";
elif [[ "${debug:-1}"   == "1" ]]; then echo "[debug mode]";   compile="$compiler $cc_debug";
fi

compile() {
  if [[ "$os_name" == "Darwin" ]]; then
    $compile "$@"
  else
    $compile -Wl,--start-group "$@" -latomic -Wl,--end-group
  fi
}

# --- Prep Directories --------------------------------------------------------
mkdir -p build local

# --- Build & Run Metaprogram -------------------------------------------------
cd build
if [[ ! -f metagen ]]; then meta='1'; fi
if [[ "${meta:-0}" == "1" ]]; then
  echo "[building metagen]"
  $compiler $cc_debug ../src/metagen/metagen_main.c $cc_link -o metagen
fi
if [[ -z "${no_meta:-}" ]]; then
  ./metagen
fi
cd ..

# --- Build Everything (@build_targets) ---------------------------------------
cd build
if [[ "${raid:-0}"                 == "1" ]]; then didbuild=1 && compile $cc_objc ../src/rad/rad_main.c $cc_icon $cc_link $cc_os_gfx $cc_render $cc_font_provider -o raid; fi
if [[ "${rad_tests:-0}"            == "1" ]]; then didbuild=1 && compile ../src/rad/tests/rad_tests.c $cc_link -o rad_tests; fi
cd ..

# --- Warn On No Builds -------------------------------------------------------
if [[ "${didbuild:-0}" == "0" ]]; then
  echo "[WARNING] no valid build target specified; must use build target names as arguments to this script, like \`./build.sh raid\` or \`./build.sh rad_tests\`."
  exit 1
fi

# --- Warn On Debug Builds (if debug not explicitly specified) ----------------
if [[ -z "${debug:-}" && -z "${release:-}" ]]; then
  echo "[INFO] Debug build complete. For a faster build, call this script with the \`release\` argument (this will take significantly longer than a debug build)."
fi
