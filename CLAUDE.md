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
5. **macOS at the very end** (decided 2026-11-03, replacing "macOS from day one" of
   2026-11-02). Work on Windows only until the IDE is done, then port to the Mac.
6. Debugging Rust programs: later, and NOT with the RAD Debugger engine (it gets
   removed, see section 2). Proposed (not decided): LLDB through `lldb-dap`, which is
   the native debugger on macOS.

## 2. Decisions already made (do not re-litigate without a strong reason)

- **Language: C**, same as the RAD Debugger codebase, so the UI stays identical.
- **The RAD Debugger engine gets removed** (decided 2026-11-02). We build an IDE, not
  a debugger. The debugger-only layers (section 3) must not end up in the shipped IDE.
- **Windows first, macOS at the very end** (decided 2026-11-03; replaces "macOS from
  day one" of 2026-11-02). No Mac CI and no `src/mac/` work until the Windows IDE is
  done. New code still follows the portability rules (section 7): they cost little now
  and keep the final port possible. The Mac window/render/font backends will be
  written and visually tested on the Mac itself; they cannot be tested from Windows.
- **UI chrome: option B** (decided 2026-11-02). The IDE frontend is its own layer,
  `src/rad/` (`RAD_`), built directly on `ui`, `draw`, `render`, `font_*`, `text`,
  `window_manager`, `shell`, `config`. Visual pieces of `raddbg` (window frame, top
  bar, bottom bar, panels, tabs, box renderer, widgets, theme presets, icon font)
  are ported with the same `ui_` calls, sizes, tags and theme values, and checked
  with side-by-side screenshots against `raddbg.exe`. `rad.exe` links no debugger
  layer. (Rejected option A: keep the `raddbg` frontend and stub the debugger.)
  - Facts behind the decision (checked 2026-11-02): RD_ makes about 1,150 calls into
    debugger layers (`e_` 577, `d_` 359, `ev_` 100, `rdi_` 57, `di_` 47, `dasm_` 9,
    `dmn_` 4); RD_ reads every setting through the debugger's expression evaluator.
    `ui`, `draw`, `render`, `font_*`, `text`, `window_manager` include no debugger
    layer; `config` only used `d_hash_from_string` (now replaced by the identical
    `u64_hash_from_seed_str8(5381, ...)` from `base`). `demon` has no stub and
    `#error`s on anything but Windows/Linux (`src/demon/demon_inc.c:10`).

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
  - `rad` (`RAD_`, `src/rad/`): the IDE frontend and build target (`build rad` ->
    `rad.exe`). Unity build of only the non-debugger layers; see section 2 (option B).
    `rad.mdesk` holds copies of raddbg's icon/theme/code-color tables plus the IDE
    command table.
  - `text` (`TXT_`): text processing, lexing and parsing of source code, async.
  - `content` (`C_`), `file_stream` (`FS_`), `artifact_cache` (`AC_`): content-addressed,
    immutable data caches with async fill and hot-reload on file change.
  - `mutable_text` (`MTX_`): mutable text cache (used for the Output log).
  - `text_buffer` (`TB_`, `src/text_buffer/`, ours): the editable buffer. Base blob +
    patch list, compacted in the background into new lexed blobs; undo/redo with
    groups, version counter, save, disk-change tracking. Depends on `base`,
    `content`, `text` only; tested by `rad_tests`.
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

Updated 2026-11-03: Windows first, macOS at the very end (renumbered).
Revised 2026-11-03 (agreed with the user): added milestones 5 (test and measure) and
6 (editor essentials) before any language work; Cargo now comes before LSP (it builds
the shared plumbing: process with pipes, JSON, diagnostics in the editor); removing the
debugger is the last Windows step (raddbg.exe stays the visual reference until then).
Old numbers: syntax 5 -> 7, LSP 6 -> 9, Cargo 7 -> 8, remove debugger 8 -> 10,
Mac 9/10 -> 11/12, debugging 11 -> 13.

1. **Build and read (week 1).** Build with `build.bat`, run it (done: `build rad`).
   Read in order: `base`, `ui`, `draw`, then the code-view part of `raddbg`.
   Write notes in `docs/notes/` on: how a frame is built, how the code view draws text.
   Answer the feasibility spike (section 7) and the open question (section 2).
   Done when: the build runs, the notes exist, option A or B is chosen with evidence.
2. **Editor view without a debugger.** Open a file with no debugger panels visible.
   Done when: I can browse a Rust project as a plain viewer.
3. **Editable text core.** Buffer, cursor, selection, undo, save, version counter.
   Done when: I can edit and save a file, with undo/redo, no corruption on large files.
4. **File explorer.** Left panel tree built with the existing UI building blocks.
   Done when: it looks like it belongs in the debugger.
5. **Test and measure.** A script mode in `rad` (`--script:<file>`): it injects keys,
   text, wheel and mouse input as if they came from the OS, without needing the
   window to have focus, runs checks on editor state (visible lines, cursor, text,
   tabs, explorer rows) and measures frame time and input-to-frame latency, all
   written to a results file. A runner in `tests/` runs every script.
   Done when: scroll, typing, undo/redo, save and explorer checks run as scripts
   while the user keeps working, and report pass/fail plus timings.
6. **Editor essentials.** One buffer per file (tabs share it; LSP needs per-document
   versions), Find (Ctrl+F) and Go to Line ported from raddbg's `Search`/`FindNext`/
   `GoToLine`, session memory (folder, open tabs, layout, window) in raddbg's user-file
   format, Save All, file watching instead of polling, and an audit of the port
   against raddbg for other dropped per-frame steps (like the scroll bug).
   Done when: each item has a passing script, and restarting rad restores the session.
7. **Syntax highlighting.** The existing code lexes C; write a Rust lexer in the same style.
8. **Cargo integration.** Build/run/test, output panel, `cargo check
   --message-format=json` errors listed and shown in the editor, clickable locations.
   Builds the plumbing LSP reuses: process with pipes (`base`), JSON, diagnostics UI.
9. **LSP client.** Diagnostics first, then hover, go-to-definition, then completion popup.
10. **Remove the debugger and rebrand.** Debugger-only layers out of the repo/build.
   Keep the MIT notice. Last Windows step: raddbg.exe is the visual reference until then.
11. **Mac foundation, compile-only (done from Windows).** Make `base` compile on
   macOS ARM64 (x86-only intrinsics, see section 7), add `src/mac/base` adapted from
   `src/linux/base`, add a macOS GitHub Actions workflow.
   Done when: a small console test program on `base` builds and runs on the macOS
   runner, green on every push.
12. **Mac UI, on the Mac.** `window_manager` (Cocoa), `render` (OpenGL 3.3 core first,
    Metal if needed), `font_provider` (FreeType first). Needs Claude Code running on
    the Mac. Biggest single risk of the port.
    Done when: `rad` runs on the Mac and matches the Windows screenshots (glyph
    rasterization aside).
13. **Debugging (later, optional).** Proposed: LLDB via `lldb-dap`. Decide then.

Builds: day to day, build unoptimized (`build rad`, fast to compile). At the end of
every milestone, also build an optimized one for the user to test: `build rad release`,
then copy `build\rad.exe` to `build\release\rad.exe` (the next debug build overwrites
`build\rad.exe`). No Linux build/CI for now (decided 2026-11-03).

## 7. Portability rules (follow from day one, even while the work is Windows-only)

1. Never call Win32 directly from new code. Everything touching the OS (spawning
   rust-analyzer with pipes, clipboard, file watching, file dialogs) goes through
   the `base` OS API, `window_manager` or `shell`, adding new functions to those APIs
   when needed (with a `src/win32/` implementation now, `src/linux/` when cheap, and
   `src/mac/` at the end).
2. Keep new editor code out of the debugger layers. Buffer, LSP client and explorer
   depend only on `base`, `ui`, `draw`, `text` (and `window_manager`/`shell` through their APIs).
3. Avoid x64-only assumptions in new code (Apple Silicon is ARM64).

macOS work means implementing, per platform: the `base` OS API (adapt from Linux, both
Unix-like), `window_manager` (Cocoa: window, input, clipboard, cursor), `render` (Metal, or GL
which macOS stops at 4.1 and has deprecated), and `font_provider` (CoreText or FreeType).
Hardest part: Cocoa wants to own the main thread and run loop, while the frame loop is
written as "poll events, build UI, draw". Glyph pixels will differ slightly from
DirectWrite output; that cannot be made bit-identical.

### Feasibility spike (part of milestone 1)

Answer and write down:
- Which OpenGL version does the Linux render backend require? (macOS max is 4.1.)
- Does `base` use x64 CPU intrinsics outside debugger layers?
- How does `window_manager` structure its event loop and frame pacing?
These are UNVERIFIED assumptions. Do not assume answers.

Partial answers (checked in the code 2026-11-02):
- OpenGL: the GL backend uses `#version 330 core` shaders
  (`src/render/opengl/generated/render_opengl.meta.h:26`) and requests a 3.3 context
  (`src/render/opengl/linux/egl/render_opengl_linux_egl.c:119`). That is below the
  macOS 4.1 core limit. Not run on a Mac yet.
- x64 intrinsics: yes. `src/base/base_core.h:276-277` includes `immintrin.h` and
  `emmintrin.h` for every clang/gcc build, with no arch guard, and
  `src/base/base_core.c:667` uses SSE2 `_mm_*` calls. Both break ARM64 and need
  guards plus a portable fallback. Atomics use `__atomic_*` builtins (portable).
- Linux-only APIs that `src/mac/base` must replace: `futex` syscall, `memfd_create`,
  `/proc/self`, `sys/sysinfo.h` (in `src/linux/base`), `epoll` (socket), `eventfd`
  (window_manager). The Linux window manager is X11 only; nothing to reuse for Cocoa.
- Event loop and frame pacing: not checked yet.

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
- **Keep the progress log.** Read `docs/PROGRESS.md` at the start of a session. Update
  it (status, next steps, dated journal entry) in the same commit as the work, so any
  session on any machine can pick up where the last one stopped.
