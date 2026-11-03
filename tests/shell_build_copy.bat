@echo off
:: Builds build\<name>.exe (default rad_probe) with build.bat's compile line
:: for `rad`, so tests can run while build\rad.exe is open (then linking
:: build\rad.exe fails with LNK1104). Run tests\shell_build.bat rad once
:: first: it runs metagen, this does not.
::
::   tests\shell_build_copy.bat [debug|release] [name]
::   powershell -ExecutionPolicy Bypass -File tests\run_scripts.ps1 -Exe build\rad_probe.exe
set NoDefaultCurrentDirectoryInExePath=
set "PATH=C:\Program Files (x86)\Microsoft Visual Studio\Installer;%PATH%"
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
cd /d "%~dp0..\build"
set mode_flags=/Od /Ob1 /DBUILD_DEBUG=1
if "%1"=="release" set mode_flags=/O2 /DBUILD_DEBUG=0
set name=rad_probe
if not "%2"=="" set name=%2
cl %mode_flags% /I..\src\ /I..\local\ /nologo /FC /Z7 /Zc:preprocessor /Fo:%name%.obj ..\src\rad\rad_main.c /link /MANIFEST:EMBED /INCREMENTAL:NO /noexp /nocoffgrpinfo /opt:ref /opt:icf logo.res /out:%name%.exe 2>&1
