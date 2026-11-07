# Raid

A fast, native IDE for Rust, written in C.

- Editor with panels, tabs, undo/redo, Find, Go to Line, folding
- File explorer, Output and Problems panels
- Cargo: Build, Check, Run, Test (Debug / Release), clickable errors
- rust-analyzer: diagnostics, hover, go to definition, completion,
  signature help, quick fixes, inlay hints

Status: alpha. macOS (Apple Silicon) and Windows x64.

## Requirements

`cargo` and `rust-analyzer` (`rustup component add rust-analyzer`).

## Build

macOS (Xcode command-line tools, `brew install freetype pkg-config`):

```
bash build.sh raid release app   # build/release/Raid.app
bash build.sh raid               # build/raid (debug)
```

Windows (Visual Studio 2022 or Build Tools 2022 with the "Desktop
development with C++" workload, Windows SDK 10.0.26100):

```
tests\shell_build.bat raid release   :: build\raid.exe (optimized)
tests\shell_build.bat raid           :: build\raid.exe (debug)
```

`shell_build.bat` works from any prompt: it loads the VS 2022 x64
environment itself. From an "x64 Native Tools Command Prompt for VS 2022"
you can also run `build raid release` directly. If Visual Studio 18 (2026)
is installed without the C++ workload, use `shell_build.bat`: a VS 18
developer environment otherwise hides the VS 2022 compiler.

If `build\raid.exe` is running (link error LNK1104), build to a copy:
`tests\shell_build_copy.bat release`.

## Tests

```
bash build.sh rad_tests && build/rad_tests
tests/run_scripts.sh
```

## License

MIT, see [LICENSE](LICENSE).
