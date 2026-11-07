#!/bin/bash
set -eu
cd "$(dirname "$0")"

# --- Unpack Arguments --------------------------------------------------------
auto_compile_flags=""
for arg in "$@"; do declare $arg='1'; done
if [[ "$#" == "0" ]]; then raid='1'; fi
if [[ "$#" == "1" && "${release:-0}" == "1" ]]; then raid='1'; fi
if [[ "${app:-0}" == "1" ]]; then raid='1'; fi
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

# --- Package The macOS App (`./build.sh raid release app`) -------------------
# rad: Raid.app (build/release/Raid.app for a release build, else
# build/Raid.app): the binary, the icon (icons/raid.icns), the non-system
# libraries it links (Homebrew's FreeType and what that links) copied into
# Contents/Frameworks with their paths rewritten, then signed ad hoc.
if [[ "${app:-0}" == "1" && "$os_name" == "Darwin" ]]; then
  if [[ "${release:-0}" == "1" ]]; then app_dir="build/release/Raid.app"; else app_dir="build/Raid.app"; fi
  echo "[packaging $app_dir]"
  rm -rf "$app_dir"
  mkdir -p "$app_dir/Contents/MacOS" "$app_dir/Contents/Resources" "$app_dir/Contents/Frameworks"
  cp build/raid "$app_dir/Contents/MacOS/raid"
  cp icons/raid.icns "$app_dir/Contents/Resources/raid.icns"
  queue=("$app_dir/Contents/MacOS/raid")
  min_os="11.0"
  while [[ "${#queue[@]}" != "0" ]]; do
    bin="${queue[0]}"; queue=("${queue[@]:1}")
    bin_min_os="$(otool -l "$bin" | awk '/LC_BUILD_VERSION/{f=1} f && $1=="minos"{print $2; exit}')"
    if [[ -n "$bin_min_os" ]]; then min_os="$(printf '%s\n%s\n' "$min_os" "$bin_min_os" | sort -V | tail -1)"; fi
    for dep in $(otool -L "$bin" | tail -n +2 | awk '{print $1}' | grep -E '^(/opt/homebrew|/usr/local)/' || true); do
      name="$(basename "$dep")"
      if [[ ! -f "$app_dir/Contents/Frameworks/$name" ]]; then
        cp -L "$dep" "$app_dir/Contents/Frameworks/$name"
        chmod u+w "$app_dir/Contents/Frameworks/$name"
        install_name_tool -id "@executable_path/../Frameworks/$name" "$app_dir/Contents/Frameworks/$name" 2>/dev/null
        queue+=("$app_dir/Contents/Frameworks/$name")
      fi
      install_name_tool -change "$dep" "@executable_path/../Frameworks/$name" "$bin" 2>/dev/null
    done
  done
  version="$(awk '/# define BUILD_VERSION_(MAJOR|MINOR|PATCH) /{printf "%s%s", sep, $4; sep="."}' src/base/base_context_cracking.h)"
  cat > "$app_dir/Contents/Info.plist" <<PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>CFBundleName</key>                 <string>Raid</string>
  <key>CFBundleDisplayName</key>          <string>Raid</string>
  <key>CFBundleIdentifier</key>           <string>dev.raid.ide</string>
  <key>CFBundleExecutable</key>           <string>raid</string>
  <key>CFBundleIconFile</key>             <string>raid</string>
  <key>CFBundlePackageType</key>          <string>APPL</string>
  <key>CFBundleInfoDictionaryVersion</key><string>6.0</string>
  <key>CFBundleShortVersionString</key>   <string>$version</string>
  <key>CFBundleVersion</key>              <string>$version</string>
  <key>LSMinimumSystemVersion</key>       <string>$min_os</string>
  <key>LSApplicationCategoryType</key>    <string>public.app-category.developer-tools</string>
  <key>NSPrincipalClass</key>             <string>NSApplication</string>
  <key>NSHighResolutionCapable</key>      <true/>
</dict>
</plist>
PLIST
  for lib in "$app_dir"/Contents/Frameworks/*.dylib; do codesign --force --sign - "$lib" >/dev/null 2>&1; done
  codesign --force --sign - "$app_dir" >/dev/null 2>&1
  touch "$app_dir"
  echo "[packaged $app_dir: Raid $version, macOS $min_os or later]"
fi

# --- Warn On No Builds -------------------------------------------------------
if [[ "${didbuild:-0}" == "0" ]]; then
  echo "[WARNING] no valid build target specified; must use build target names as arguments to this script, like \`./build.sh raid\` or \`./build.sh rad_tests\`."
  exit 1
fi

# --- Warn On Debug Builds (if debug not explicitly specified) ----------------
if [[ -z "${debug:-}" && -z "${release:-}" ]]; then
  echo "[INFO] Debug build complete. For a faster build, call this script with the \`release\` argument (this will take significantly longer than a debug build)."
fi
