@echo off
setlocal enabledelayedexpansion
cd /D "%~dp0"

:: --- Usage Notes (2024/1/10, trimmed to Raid's targets 2026-11-05) ----------
::
:: This is the central build script for Raid on Windows. It takes a list of
:: simple alphanumeric-only arguments which control (a) what is built, (b)
:: which compiler & linker are used, and (c) extra high-level build options.
:: By default, if no options are passed, then `raid` (raid.exe) is built.
::
:: Below is a non-exhaustive list of possible ways to use the script:
:: `build raid`
:: `build raid clang`
:: `build raid release`
:: `build raid asan telemetry`
:: `build rad_tests`
::
:: For a full list of possible build targets and their build command lines,
:: search for @build_targets in this file.
::
:: Below is a list of all possible non-target command line options:
::
:: - `asan`: enable address sanitizer
:: - `ubsan`: enable undefined-behavior sanitizer
:: - `telemetry`: enable RAD telemetry profiling support
:: - `spall`: enable spall profiling support
:: - `opengl`: use the OpenGL render backend instead of D3D11
::
:: The RAD Debugger's own targets (raddbg, radlink, radbin, ...) were removed
:: with their sources in milestone 10; they build from the `raddbg-reference`
:: git tag (see docs/PROGRESS.md, "Build, run, test").

:: --- Unpack Arguments -------------------------------------------------------
for %%a in (%*) do set "%%~a=1"
if not "%msvc%"=="1" if not "%clang%"=="1" set msvc=1
if not "%release%"=="1" if not "%debug%"=="1" set debug_implicit=1
if not "%release%"=="1" set debug=1
if "%debug%"=="1"   set release=0 && echo [debug mode]
if "%release%"=="1" set debug=0 && echo [release mode]
if "%msvc%"=="1"    set clang=0 && echo [msvc compile]
if "%clang%"=="1"   set msvc=0 && echo [clang compile]
if "%~1"==""                     echo [default mode, assuming `raid` build] && set raid=1
if "%~1"=="release" if "%~2"=="" echo [default mode, assuming `raid` build] && set raid=1

:: --- Unpack Command Line Build Arguments ------------------------------------
set auto_compile_flags=
if "%telemetry%"=="1"               set auto_compile_flags=%auto_compile_flags% -DPROFILE_TELEMETRY=1 && echo [telemetry profiling enabled]
if "%spall%"=="1"                   set auto_compile_flags=%auto_compile_flags% -DPROFILE_SPALL=1 && echo [spall profiling enabled]
if "%asan%"=="1"                    set auto_compile_flags=%auto_compile_flags% -fsanitize=address && echo [asan enabled]
if "%ubsan%"=="1"                   set auto_compile_flags=%auto_compile_flags% -fsanitize=undefined && echo [ubsan enabled]
if "%opengl%"=="1"                  set auto_compile_flags=%auto_compile_flags% -DR_BACKEND=R_BACKEND_OPENGL && echo [opengl render backend]
if "%dwarf%"=="1" if "%clang%"=="1" set auto_compile_flags=%auto_compile_flags% -gdwarf && echo [dwarf debug info]
if "%dwarf%"==""  if "%clang%"=="1" set auto_compile_flags=%auto_compile_flags% -gcodeview

:: --- Mix Git Commit ID ------------------------------------------------------
:: (only version tags like v1.0 name a build; others, e.g. raddbg-reference,
:: are not versions, so the hash is shown alone)
call git rev-parse --is-inside-work-tree >nul 2>&1 && set in_git_tree=1
if "%in_git_tree%"=="1" (
  for /f %%i in ('call git describe --always --dirty --match "v[0-9]*"') do set auto_compile_flags=!auto_compile_flags! -DBUILD_GIT_HASH=\"%%i\"
  for /f %%i in ('call git rev-parse HEAD')              do set auto_compile_flags=!auto_compile_flags! -DBUILD_GIT_HASH_FULL=\"%%i\"
)

:: --- Compile/Link Line Definitions ------------------------------------------
set cl_common=     /I..\src\ /I..\local\ /nologo /FC /Z7 /Zc:preprocessor
set cl_debug=      call cl /Od /Ob1 /DBUILD_DEBUG=1 %cl_common% %auto_compile_flags%
set cl_release=    call cl /O2 /DBUILD_DEBUG=0 %cl_common% %auto_compile_flags%
set cl_link=       /link /MANIFEST:EMBED /INCREMENTAL:NO /pdbaltpath:%%%%_PDB%%%% /NATVIS:"%~dp0\src\natvis\base.natvis" /noexp /nocoffgrpinfo /opt:ref /opt:icf
set cl_out=        /out:
set clang_common=  -mcx16 -I..\src\ -I..\local\ -fdiagnostics-absolute-paths -Wall -Wno-unknown-warning-option -Wno-missing-braces -Wno-unused-function -Wno-unused-parameter -Wno-writable-strings -Wno-missing-field-initializers -Wno-unused-value -Wno-unused-variable -Wno-unused-local-typedef -Wno-deprecated-register -Wno-deprecated-declarations -Wno-unused-but-set-variable -Wno-single-bit-bitfield-constant-conversion -Wno-compare-distinct-pointer-types -Wno-initializer-overrides -Wno-incompatible-pointer-types-discards-qualifiers -Xclang -flto-visibility-public-std -D_USE_MATH_DEFINES -Dstrdup=_strdup -Dgnu_printf=printf -ferror-limit=10000
set clang_debug=   call clang -g -O0 -DBUILD_DEBUG=1 -D_DEBUG %clang_common% %auto_compile_flags%
set clang_release= call clang -g -O2 -DBUILD_DEBUG=0 -DNDEBUG %clang_common% %auto_compile_flags%
set clang_link=    -fuse-ld=lld -Xlinker /MANIFEST:EMBED -Xlinker /pdbaltpath:%%%%_PDB%%%% -Xlinker /NATVIS:"%~dp0\src\natvis\base.natvis" -Xlinker /opt:ref -Xlinker /opt:noicf
set clang_out=     -o

:: --- Per-Build Settings -----------------------------------------------------
set link_icon=logo.res
if "%msvc%"=="1"    set rc=call rc
if "%clang%"=="1"   set rc=call llvm-rc

:: --- Choose Compile/Link Lines ----------------------------------------------
if "%msvc%"=="1"      set compile_debug=%cl_debug%
if "%msvc%"=="1"      set compile_release=%cl_release%
if "%msvc%"=="1"      set compile_link=%cl_link%
if "%msvc%"=="1"      set out=%cl_out%
if "%clang%"=="1"     set compile_debug=%clang_debug%
if "%clang%"=="1"     set compile_release=%clang_release%
if "%clang%"=="1"     set compile_link=%clang_link%
if "%clang%"=="1"     set out=%clang_out%
if "%debug%"=="1"     set compile=%compile_debug%
if "%release%"=="1"   set compile=%compile_release%

:: --- Prep Directories -------------------------------------------------------
if not exist build mkdir build
if not exist local mkdir local

:: --- Produce Logo Icon File -------------------------------------------------
pushd build
%rc% /nologo /fo logo.res ..\data\logo.rc || exit /b 1
popd

:: --- Build & Run Metaprogram ------------------------------------------------
pushd build
if not exist metagen.exe set meta=1
if "%meta%"=="1" (
  echo [building metagen]
  %compile_debug% ..\src\metagen\metagen_main.c %compile_link% %out%metagen.exe || exit /b 1
)
if "%no_meta%"=="" if exist metagen.exe (
  echo [running metagen]
  metagen.exe || exit /b 1
)
popd

:: --- Build Everything (@build_targets) --------------------------------------
pushd build
if "%raid%"=="1"                       set didbuild=1 && %compile% ..\src\rad\rad_main.c                                     %compile_link% %link_icon% %out%raid.exe || exit /b 1
if "%rad_tests%"=="1"                  set didbuild=1 && %compile% ..\src\rad\tests\rad_tests.c                              %compile_link% %out%rad_tests.exe || exit /b 1
popd

:: --- Warn On No Builds ------------------------------------------------------
if "%didbuild%"=="" (
  echo [WARNING] no valid build target specified; must use build target names as arguments to this script, like `build raid` or `build rad_tests`.
  exit /b 1
)

:: --- Warn On Debug Builds (if debug not explicitly specified) ---------------
if "%debug_implicit%"=="1" (
  echo [INFO] Debug build complete. For a faster build, call this script with the `release` argument; this will take significantly longer than a debug build.
)
