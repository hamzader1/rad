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

Windows (Visual Studio 2022, x64 Native Tools prompt):

```
build raid release               :: build\raid.exe
```

## Tests

```
bash build.sh rad_tests && build/rad_tests
tests/run_scripts.sh
```

## License

MIT, see [LICENSE](LICENSE).
