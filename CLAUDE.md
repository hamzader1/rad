# CLAUDE.md — Rust IDE built on the RAD Debugger codebase

## 1. Goal

Build a native IDE for **Rust** (C/C++ later) by forking the RAD Debugger
(https://github.com/EpicGames/raddebugger, MIT license).

The thing I care about most is the **UI**. It must look and behave **identically** to
the RAD Debugger UI (layout, panels, tabs, theming, fonts, animations, feel).
No generic-looking replacement. If a choice makes the UI look different, the answer is no.

Required features, in priority order:

1. Editable text editor (the RAD Debugger code view is read-only today).
2. File explorer panel on the left.
3. Rust support through LSP (rust-analyzer): diagnostics, hover, go-to-definition,
   completion-as-you-type.
4. Cargo build/run integration with an output panel and clickable error locations.
5. Debugging Rust programs (reuse the RAD Debugger's own debugger on Windows).
6. macOS port at the very end.

## 2. Decisions already made (do not re-litigate without a strong reason)

- **Language: C**, same as the RAD Debugger codebase, so the UI stays identical.
- **Fork and extend, do not strip first.** Add the editor next to the existing views,
  hide the debugger panels, and prune debugger-only code LAST and only in pieces that
  nothing depends on. The `raddbg` (RD_) layer is heavily coupled to the debugger.
- **macOS port comes at the end**, after pruning (see section 7).
- Do not use DAP/LLDB for the first debugging milestone. On Windows, the Rust MSVC
  target produces PDB files, which is the RAD Debugger's strongest debugging path.

## 3. Facts about the RAD Debugger codebase (from its README)

- Native, user-mode, multi-process debugger. **x64 only**, Windows and Linux.
  Linux support is early. Project status is alpha. License: MIT (keep the notice).
- Code is organized in **layers**, forming a directed acyclic graph. Each layer lives in
  a folder under `src/` and uses a short namespace prefix (e.g. `UI_`, `DR_`, `OS_`).
  Layers must not form cycles.
- Layers that matter for the UI and editor:
  - `base` (no prefix): strings, math, arenas, helper macros.
  - `ui` (`UI_`): immediate-mode, hierarchical UI building API plus some widgets.
  - `draw` (`DR_`): high-level draw API with batching, built on `render`.
  - `render` (`R_`): thin abstraction over GPU APIs (D3D11 on Windows).
  - `font_provider` (`FP_`) and `font_cache` (`FNT_`): rasterization and glyph atlas
    (DirectWrite on Windows, FreeType on Linux).
  - OS abstraction (corrected 2026-11-02 after reading the code; the README's
    `os/core`/`os/gfx` layout no longer exists in this tree):
    - `base` itself declares the core OS API (`base_files`, `base_processes`,
      `base_threads`, `base_system`, ...), marked `@per_os_impl`.
    - `window_manager` (`WM_`): windows, input events, clipboard, cursor (was `os/gfx`).
    - `shell` (`SH_`): native dialogs, message boxes, file browser.
    - Per-platform implementations live in `src/win32/` and `src/linux/`
      (subfolders `base`, `window_manager`, `shell`, ...).
  - `rad` (`src/rad/`): the IDE build target (`build rad` -> `rad.exe`). Its own
    unity-build entry point; reuses the `raddbg` UI unmodified.
  - `text` (`TXT_`): text processing, lexing and parsing of source code, async.
  - `content` (`C_`), `file_stream` (`FS_`), `artifact_cache` (`AC_`): content-addressed,
    immutable data caches with async fill and hot-reload on file change.
  - `mutable_text` (`MTX_`): mutable text cache (used for the Output log).
  - `raddbg` (`RD_`): the debugger frontend that ties everything together.
- Debugger-only layers (candidates for pruning at the end): `demon`, `ctrl`,
  `dbg_engine`, `dbg_info`, `eval`, `eval_visualization`, `disasm`, `rdi_*`, `pdb`,
  `coff`, `pe`, `dwarf`, `elf`, `codeview`, `regs`, `msf`, `msvc_crt`, `linker`, `radbin`.
- Build: `build.bat` on Windows (needs MSVC/Windows SDK or Clang), `build.sh` on Linux.
  Metagen (`.mdesk` files) generates tables and enums; do not edit generated output.

## 4. Why editing is not a flag flip

```
RAD Debugger today:
  file -> 128-bit hash -> content cache -> async lex -> read-only view
                  ^ hot-reload when the file changes on disk

An editor needs:
  keystroke -> mutate buffer -> new version -> incremental re-lex -> redraw
            -> didChange(version N) to rust-analyzer
            <- diagnostics arrive LATER, tagged with an older version
```

The existing text pipeline assumes immutable, content-addressed data. The editor needs:
a mutable buffer (rope, piece table or gap buffer), cursors and selections, undo/redo,
save, a **version counter on every change**, and a way to map positions from an older
version to the current one (language-server answers are asynchronous).

## 5. LSP design

```
 Editor  <---- JSON-RPC over stdin/stdout ---->  rust-analyzer
   didOpen / didChange(version) / didSave
   request(id): completion, hover, definition
   <- response(id)
   <- publishDiagnostics (async, may refer to an older version)
```

Needs: process spawn with pipes (through the `base` process API, `process_launch`), a JSON parser/writer, a message
framing layer (Content-Length headers), a request/response table keyed by id,
a debounced change sender, and handling of position encoding (LSP defaults to UTF-16
offsets; negotiate UTF-8 if the server supports it).

## 6. Plan (milestones, each with a "done when")

1. **Build and read (week 1).** Fork, build with `build.bat`, run it.
   Read in order: `base`, `ui`, `draw`, then the code-view part of `raddbg`.
   Write notes in `docs/notes/` on: how a frame is built, how the code view draws text.
   Done when: the build runs and the notes exist.
2. **Editor view without a debugger.** Open a file with no target running and no
   debugger panels visible. Done when: I can browse a Rust project as a plain viewer.
3. **File explorer.** Left panel tree built with the existing UI building blocks.
   Done when: it looks like it belongs in the debugger.
4. **Editable text core.** Buffer, cursor, selection, undo, save, version counter.
   Done when: I can edit and save a file, with undo/redo, no corruption on large files.
5. **Syntax highlighting.** The existing code lexes C; write a Rust lexer in the same style.
6. **LSP client.** Diagnostics first, then hover, go-to-definition, then completion popup.
7. **Cargo integration.** Build/run, output panel, clickable error locations.
8. **Debugging.** Reuse the RAD Debugger on Windows with the MSVC Rust target (PDB).
   Test early how Rust enums, `Option`, `Vec`, trait objects display; custom visualizers
   will probably be needed.
9. **Prune and rebrand.** Remove debugger-only layers last. Keep the MIT notice.
10. **macOS port** (section 7).

## 7. Portability rules (follow from day one)

1. Never call Win32 directly from new code. Everything touching the OS (spawning
   rust-analyzer with pipes, clipboard, file watching, file dialogs) goes through
   the `base` OS API, `window_manager` or `shell`, adding new functions to those APIs
   when needed (with a `src/win32/` and `src/linux/` implementation).
2. Keep new editor code out of the debugger layers. Buffer, LSP client and explorer
   depend only on `base`, `ui`, `draw`, `text` (and `window_manager`/`shell` through their APIs).
3. Avoid x64-only assumptions in new code (Apple Silicon is ARM64).

macOS port later means implementing, per platform: the `base` OS API (adapt from Linux, both
Unix-like), `window_manager` (Cocoa: window, input, clipboard, cursor), `render` (Metal, or GL
which macOS stops at 4.1 and has deprecated), and `font_provider` (CoreText or FreeType).
Hardest part: Cocoa wants to own the main thread and run loop, while the frame loop is
written as "poll events, build UI, draw". Glyph pixels will differ slightly from
DirectWrite output; that cannot be made bit-identical.

### Feasibility spike (do after milestone 1, about one day)

Answer and write down:
- Which OpenGL version does the Linux render backend require? (macOS max is 4.1.)
- Does `base` use x64 CPU intrinsics outside debugger layers?
- How does `window_manager` structure its event loop and frame pacing?
These are UNVERIFIED assumptions. Do not assume answers.

## 8. How we work together

- **UI fidelity is the top rule.** Reuse the existing `ui`/`draw` building blocks and
  the existing theme/palette/fonts. Do not introduce new visual styles.
- **Match the codebase style exactly:** layer boundaries, namespace prefixes, arena
  allocation, metagen tables, formatting, naming. Read neighboring code before writing.
- **You write 100% of the code.** I do not write any code. That includes the text buffer,
  version tracking, LSP client, UI, build scripts, tests, everything. You also build,
  run, test and fix it yourself until each milestone's "done when" is met.
- **Read before you write.** Before changing anything, read the neighboring code and
  state the plan briefly (which files, why there). Then implement it. Keep explanations
  short, concrete and causal; use ASCII diagrams when they help.
- **Be honest.** Push back when something is a bad idea. Do not agree just to be agreeable.
  Say clearly when something is unverified or you are guessing.
- **Small steps.** One layer or one milestone at a time, builds green after each step,
  small commits. Do not modify generated code. Do not delete debugger layers early.
