@echo off
:: Runs build.bat from a shell that is not a VS developer prompt (e.g. Claude
:: Code's shell): clears NoDefaultCurrentDirectoryInExePath (with it set,
:: build.bat's bare `metagen.exe` call fails), loads the VS 2022 Build Tools
:: x64 environment, then calls build.bat with the same arguments.
::
::   tests\shell_build.bat raid             -> build\raid.exe (debug)
::   tests\shell_build.bat raid release     -> build\raid.exe (optimized)
::   tests\shell_build.bat rad_tests
::
:: If build\raid.exe is running, linking fails (LNK1104): use
:: tests\shell_build_copy.bat instead.
set NoDefaultCurrentDirectoryInExePath=
:: A VS 18 developer environment inherited from the parent shell makes
:: vcvars point at VS 18 (no C++ tools there): clear it first.
set VCINSTALLDIR=
set VSINSTALLDIR=
set VCIDEInstallDir=
set DevEnvDir=
set VCToolsInstallDir=
set VisualStudioVersion=
set "PATH=C:\Program Files (x86)\Microsoft Visual Studio\Installer;%PATH%"
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
cd /d "%~dp0.."
call build.bat %* 2>&1
