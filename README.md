# Raid

Raid is a native IDE for **Rust**, written in C. It is built on the codebase
of the [RAD Debugger](https://github.com/EpicGames/raddebugger) (MIT license)
and keeps its UI: the same layout, panels, tabs, theming, fonts and feel.
The debugger itself has been removed; Raid is an editor.

Status: alpha, Windows x64 only for now. macOS comes at the end; see
`CLAUDE.md` (goals, decisions, plan) and `docs/PROGRESS.md` (where things
stand).

## Features

- Editable text editor: multiple panels and tabs, one shared buffer per
  file, undo/redo, Find, Go to Line, code folding, Save All, drag and drop.
- File explorer and tool windows (Output, Problems).
- Rust syntax highlighting, plus semantic colors from rust-analyzer.
- Cargo: Build, Check, Run, Test, Stop (Debug/Release); errors in the
  Problems list and in the editor, with clickable locations.
- rust-analyzer (LSP): diagnostics, hover, go to definition, completion as
  you type, signature help, code actions, navigation bar.
- Settings tab, themes, sessions restored on restart.

Raid needs `cargo` and `rust-analyzer` on the `PATH`
(`rustup component add rust-analyzer`).

## Building (Windows x64)

Needs Visual Studio 2022 (or its Build Tools) with the C++ workload and a
Windows SDK. In an "x64 Native Tools Command Prompt for VS 2022", from the
repository root:

```
build raid              :: build\raid.exe (debug)
build raid release      :: build\raid.exe (optimized)
build rad_tests         :: build\rad_tests.exe (console tests)
```

`build` alone builds `raid`. Options: `clang`, `asan`, `ubsan`, `opengl`,
`telemetry`, `spall` (see the top of `build.bat`). Every build first runs
`metagen`, which generates tables from the `.mdesk` files into
`generated/` folders.

Run: `build\raid.exe [folder or file ...]`. Settings and the session are
stored in `%APPDATA%\raid\` (`--appdata:<folder>` uses another folder).

## Tests

- `build\rad_tests.exe`: JSON, LSP framing, Rust lexing and text buffer
  checks.
- `powershell -ExecutionPolicy Bypass -File tests\run_scripts.ps1`: UI
  scripts (`tests\scripts\*.radtest`) that drive Raid with simulated input,
  without taking focus. The script language is described at the top of
  `src\rad\rad_script.h`.

## Code

The code is organized in layers, one folder per layer under `src/`, each
with a short prefix (`UI_`, `DR_`, ...). The IDE itself is the `rad` layer
(`src/rad/`, prefix `RAD_`; the code keeps that name). It is built on the
RAD Debugger's `base`, `ui`, `draw`, `render`, `font_provider`,
`font_cache`, `window_manager`, `shell`, `config`, `text`, `content` and
`file_stream` layers, plus Raid's own `text_buffer`, `json` and `lsp`. See
`CLAUDE.md` section 3 and `docs/notes/`.

The RAD Debugger's frontend, engine and tools were removed in milestone 10;
git tag `raddbg-reference` is the last commit that has them.

## License

MIT, see `LICENSE`. The RAD Debugger codebase is Copyright (c) Epic Games
Tools; its notice is kept in `LICENSE` and in the source files.
