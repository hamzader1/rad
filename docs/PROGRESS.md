# Progress log

Read this first when starting a session. Update it in the same commit as the
work: change "Status" and "Next steps", and add a dated journal entry (newest
on top). CLAUDE.md holds the goals and decisions; this file holds where we are.

For a new session (any machine): read CLAUDE.md, then "Status", "Next steps"
and "TODO (later)" here, then "Build, run, test". `docs/notes/` explains how a
frame is built and how the code view draws text; the script language for UI
tests is at the top of `src/rad/rad_script.h`. Working rules agreed with the
user, beyond CLAUDE.md:
- Commit and push to `origin/main` after each finished change (standing OK).
  Stage paths explicitly (metagen leaves ~50 unchanged generated files
  marked modified).
- Text with backslashes (Windows paths, C escapes) must not go through a
  Bash heredoc: `\r` and `\n` turn into real control characters. Write such
  edit scripts with the file-writing tool and run `python -I <file>`.

## Status (2026-11-03)

Windows only for now; macOS is milestones 11-12, at the very end (decided 2026-11-03).
Plan revised 2026-11-03 (see CLAUDE.md section 6). Builds: unoptimized day to day; an
optimized build in `build\release\rad.exe` at the end of every milestone.

| Milestone (CLAUDE.md section 6)      | State |
|--------------------------------------|-------|
| 1. Build and read                    | Done. `build rad` works; notes in `docs/notes/`; option B chosen. |
| 2. Editor view without a debugger    | Done. |
| 3. Editable text core                | Mostly done, see below (rest folds into 6). |
| 4. File explorer                     | Done (keyboard, mouse, Open Folder checked). Special panel: "+" = Open Folder, tab not closable, no other tabs in its panel; width adjustable. |
| 5. Test and measure                  | Done. `tests\run_scripts.ps1`: 4 scripts, 60 checks, all pass (debug and release). |
| 6. Editor essentials                 | Done. 13 scripts, 210 checks, all pass (debug and release, `build\release\rad.exe` 2026-11-03). |
| 7. Rust syntax highlighting          | Done. Rust lexing in the `text` layer (see below, `docs/notes/04-syntax-highlighting.md`); 14 scripts, 242 checks, pass (debug and release, `build\release\rad.exe` 2026-11-03). |
| 8. Cargo integration                 | Done. `json` layer; cargo runner (Build, Check, Run, Test, Stop; Debug/Release), Output + Problems tabs, diagnostics in the editor, clickable locations, Run in a terminal window, top-bar run toolbar with the user's icons (see journal). 18 scripts, 368 checks, pass (debug and release, `build\release\rad.exe` 2026-11-03). |
| 9. LSP client                        | In progress: `lsp` layer; rust-analyzer started per Cargo workspace, documents synced, diagnostics in Problems and the editor (see journal). Next: hover, go to definition, completion. |
| 10. Remove debugger, rebrand         | Not started. |
| 11-13. Mac foundation, Mac UI, debugging | Not started. |

Milestone 3 details:
- The buffer is the `text_buffer` layer (`TB_`), used by the code view.
- Checked in the running app: typing, Enter (keeps indent), Backspace,
  Delete, Ctrl+Z (one undo removes a typed word), Ctrl+Y, Ctrl+S (file on
  disk correct, no temp file left, dirty dot and "(modified)" cleared, no
  false "(changed on disk)" after our own save).
- Checked by `rad_tests` (36,270,433 checks, 0 failures, ~37 s): patched
  text after random edits (LF, CRLF); buffer undo/redo/save/dirty against a
  model; undo groups; reload when the file changes while clean, flag while
  dirty; empty text; ~2 MB text with 2,000 edits and compactions.
- NOT checked yet in the app: Ctrl+X / Ctrl+V, "close/exit again to
  discard" guards, panel split/close, external file change.

User confirmed in the app (2026-11-03): cut/paste, redo, Ctrl+W guard.

Milestone 4 details:
- Default layout: explorer panel on the left (20%), editor panel on the
  right (80%, focused). `rad.exe <folder>` roots the explorer there;
  `rad.exe <file>` roots it at the file's folder; no argument: the
  explorer says "No folder open."
- Explorer rows are raddbg's watch-table rows (`rad_cell`, port of
  `rd_cell`): same row height, alternating "alt" rows, bottom border,
  indent guides, expander, focus-hot selection. Compared side by side with
  raddbg's settings table (`watch: expression: "query:user_settings"`).
- Checked in the app: Ctrl+Comma focuses it; Up/Down select; Right
  expands, Left collapses; Enter on a file opens it in the editor panel
  (not next to the explorer) and focuses that panel. File > Open File
  while the explorer is focused also opens into the editor panel.
- Open Folder (File menu, Ctrl+Shift+O = raddbg's "Open Project" key, and
  a button in the "No folder open." state, styled like Getting Started's
  "Open File"): `sh_pick_folder` (new in the shell layer; win32 uses
  IFileOpenDialog + FOS_PICKFOLDERS; linux uses `zenity --directory`,
  NOT compiled or run yet). Checked in the app: the dialog opens, starts
  in the explorer's current folder, "Select Folder" re-roots the explorer
  (tab title follows), Cancel changes nothing. Open text tabs stay open.
- User confirmed the mouse works (2026-11-03).
- The "+" button of a panel holding an explorer runs Open Folder (tooltip
  says so); other panels keep Open File. NOT clicked in the app yet.
- NOT checked in the app yet: a folder argument, refresh after files
  change on disk.

Milestone 5 details:
- `rad --script:<file> [--script_out:<file>]` runs a test script and exits.
  Language, fields and targets: top of `src/rad/rad_script.h`.
- Input is simulated in the `window_manager` layer ("Simulated Input" in
  `window_manager.h`): queued events come out of `wm_get_events`, and the
  focus, mouse, modifier, key-down and clipboard queries answer from the
  simulated state, so the UI code runs exactly as with OS input. Real
  keyboard/mouse events are dropped while a script runs, the window opens
  without activating (`WM_WindowFlag_NoActivate`), the clipboard is private.
- `tests\run_scripts.ps1` runs every `tests\scripts\*.radtest` on a fresh
  copy of `tests\` in `%TEMP%\rad_script_tests`, prints OK/FAIL, failing
  lines and MEASURE lines; exit code = failed scripts. Ran while the user
  worked: no focus taken.
- Scripts: `scroll` (wheel both ways, clamp, Ctrl+End/Home, PageDown, long
  scroll), `edit` (click, typing, Enter, Backspace, mouse selection, copy,
  paste, 20x undo back to clean, 20x redo, save -> exact bytes on disk),
  `explorer` (rows, folders first, expander click, click select, Enter and
  double-click open, Left/Right collapse/expand), `perf` (timings).
- Timings (`perf`, 3,000-line file, 60 Hz display):

  | build   | UI build per frame (avg / p95) | input -> frame presented |
  |---------|--------------------------------|--------------------------|
  | debug   | typing 4.1 / 4.7 ms, wheel 3.9 / 4.6 ms | 16.6 ms (1 frame) |
  | release | typing 2.4 / 3.3 ms, wheel 2.2 / 3.0 ms | 16.6 ms (1 frame) |

  Every input shows up in the next presented frame; the rest of the frame
  is the vsync wait. "Input latency" is app-side only (the OS and the
  compositor add more, not measured).

Milestone 7 details:
- `text` layer: Rust big tokens (strings incl. raw/byte/C strings, chars vs
  lifetimes, nested block comments, attributes) decided by one pair of
  helpers, used by the whole-file token pt scan, the re-lex after edits and
  the visible-range tokenizer (`txt_token_array_from_data__rust`).
  `TXT_TokenPt` gained `aux` (raw-string hashes etc.), `TXT_TextInfo` gained
  `lang`. Primitive types added to the Rust keyword table.
- Colors (raddbg theme slots, nothing new): lifetimes/labels = keyword,
  attributes = meta, strings/chars = string. Screenshot checked
  (`tests/fixtures/syntax.rs`). Not compared with raddbg.exe (raddbg colors
  Rust with the C rules, so it differs on purpose for lifetimes etc.).
- Fixed in the shared edit path (C too), found by the new tests: point
  lookups between map ranges returned "no token" (text after an edit inside a
  multi-line comment/string lost its color); edits across several points
  gave two ranges the same numbers; text inserted right before an opener
  dropped that token's start. The range selection is rewritten (see the note).
- Tests: `rad_tests` (37,593,641 checks: Rust token kinds; per-line resume
  equals whole-text lexing before/after hand-written and 20x50 random
  edits); script `rust_syntax` (32 checks, field `token <line> <col>`).
- Not done: macro names (`println!`) and function names are plain
  identifiers (raddbg colors functions from debug info; LSP semantic tokens
  can do it in milestone 9). Resuming inside a nested comment level > 1 or
  inside nested attribute brackets assumes depth 1. While a token is left
  open by an edit, colors after it are approximate until the buffer compacts.

Milestone 6 details (each item with its script):
- [x] One document per file (`RAD_Document`, script `shared_document`).
  Tabs on the same file share text, undo, dirty, save; each keeps its own
  cursor, mapped through the buffer's change log (`tb_buffer_map_offset`,
  also for LSP later) when another tab edits. Documents tick every frame,
  visible or not. Closing a dirty file's tab asks only if it is the last
  tab on that file; Exit counts dirty files, not tabs.
- [x] Save All (Ctrl+Shift+S, File menu; script `save_all`): every dirty
  document, hidden tabs too. raddbg has no such command (new table row).
- [x] Go to Line (Ctrl+G) and Find (Ctrl+F; Ctrl+R backwards; F3 / Shift+F3
  next/previous; Edit menu), script `find` (32 checks). Ports of raddbg's
  query bar (`rd_view_ui`), `CompleteQuery`, the code view's search and
  goto, and `rd_cell`'s line editing (`rad_cell` now edits when given an
  `edit_buffer`). Query columns added to rad's command table.
  Differences from raddbg, all because rad's code view is an editor: Return/
  Esc presses carry the Accept/Cancel slots directly (raddbg binds them to
  `accept`/`cancel` commands; Space is not bound, it types); Enter's "
"
  text is eaten while the bar has the keyboard; clicking into the code gives
  it the keyboard back (Enter then edits instead of searching). Also fixed
  in the port: raddbg's backward search wraps its start offset around when
  a range ends at 0 (would walk ~2^52 chunks). NOT compared side by side
  with raddbg's bar yet (needs raddbg focused; the code is a line-by-line
  port with the same tags and sizes).
- [x] Session memory (scripts `session_1` + `session_2`: restart on the same
  user file restores folder, panels, tabs, focused panel/tab). The `user`
  cfg tree is written like raddbg's user file (`// rad <version> user file`
  header, `cfg_string_from_tree`, temp/old file dance) to
  `%APPDATA%/rad/default.rad_user` (or `--user:<path>`), when it changes
  (checked every 2 s) and at exit; loaded at startup instead of the default
  layout. Window pos/size/monitor/maximized/fullscreen committed to cfg
  every frame (port of raddbg's). Arguments go into the restored session
  (folder -> Open Folder, files -> Open File). Script runs use no user file
  unless `#! args: --user:...` is the script's first line. Not saved:
  cursor/scroll positions (view state, not cfg).
- [x] File watching (script `file_watch`, 12 checks). `base`: new
  `file_watch_alloc/check/release` (win32: FindFirstChangeNotification;
  linux: NOT implemented, reports a change every check = polling). rad: a
  watcher thread (every 100 ms) checks the explorer's folder watch and
  file_stream's change gen, and wakes the UI (`wm_send_wakeup_event`). The
  explorer re-lists only when its folder changed (was: every second, on
  the UI thread). Checked: an idle rad (blocked on input) wakes and shows
  a file written 1 s later (`sleep_until_woken`).
- [x] Audit of the port against raddbg's per-frame steps. Compared section
  by section: `rd_frame`, `rd_window_frame` (incl. panel leaf UI),
  `rd_code_view_build`. Fixed so far (scripts `tab_titles`, `find`):
  - error border: raddbg flashes a red rounded border around the window on
    user errors (`error_t`); rad faded the bottom-bar message but drew no
    border.
  - same-named files: tab titles get raddbg's qualifiers (`<src> main.rs`),
    from the per-frame "ambiguous paths" table.
  - a selection on one line becomes the search text while the query bar
    is closed (F3 / Ctrl+F search for the selected text).
  - panel zoom (script `zoom`, 12 checks): the button at the right end of
    the focused panel's tab bar, Zoom/Unzoom/Toggle Panel Zoom (Panel
    menu), the hoist animation, splits and panel cycling inside a zoom.
    Deviation: the second panel pass skips the zoomed panel's whole
    subtree (raddbg would build a zoomed split's children twice).
  - loading overlay (raddbg's animated bar + progress layer over a blurred
    panel while a file streams in; fades out after). Script field
    `loading`; `scroll` checks it clears. Seen by screenshot on a 429 MB
    file (a 56 MB one loads in ~0.3 s, too fast to catch).
  Left as a feature gap (see Known limits): tab drag/drop.
  Not needed: debugger steps (debug info, ctrl, eval, hover eval, run to
  line, find thread), key rebinding, bitmap cache, developer menu/lag
  simulation, window titles (rad's never changes), raddbg's dead code
  (`drifted_for_search` is never set; the wrap cache is never read). Not
  ported: `SetProcessWorkingSetSize` at frame 15 (Win32 call in frontend
  code; memory trim only).

## Next steps

1. Milestone 9 (LSP client: rust-analyzer over stdin/stdout pipes, JSON-RPC
   with Content-Length framing; diagnostics first, then hover, go to
   definition, completion). Reuses milestone 8's `json` layer, process
   pipes (`rad_cargo.c`) and the diagnostics UI (`RAD_Diagnostic`, line
   colors, underlines, annotations, Problems tab).
2. Speed: the user will come back to what feels slow (2026-11-03: "ignore,
   we will come back later"). Measured so far: 1 frame from input to
   screen (release: UI build ~2 ms/frame, input shows in the next frame).
3. Not yet compared side by side with raddbg.exe: the query bar, the zoom
   button, the loading overlay (line-by-line ports; needs raddbg focused).
4. For milestone 11 (Mac, at the end), already known: `base_core.h:276-277`
   and `base_core.c:667` need ARM64 guards; drop `-mcx16` on ARM64.

## TODO (later)

- Tab drag/drop between panels (port raddbg's: drop sites on tab bars and
  panel edges, the drag visualization, `MoveTab`). Asked for by the user
  2026-11-03, after milestone 6.

## Known limits

- Scope lines and end-of-scope annotations are off while patches exist,
  i.e. while typing; they come back ~100 ms after typing stops (compaction).
- Compaction re-lexes the whole file. Fine for normal files; on very large
  files typed into very fast, patches pile up while a lex is in flight.
- A tab whose cursor fell more than 1,024 changes behind (hidden while
  another tab edits a lot), or across a reload, gets its cursor clamped
  instead of mapped.
- Undo history grows without limit while a file is open.
- Open File uses the native dialog (`sh_pick_file`), not raddbg's lister.
  Open Folder likewise (`sh_pick_folder`).
- No tab drag/drop between panels (raddbg's was not ported). If it is
  ported, it must keep tabs out of the explorer's panel and the explorer
  in it.
- The explorer re-lists all expanded folders on the UI thread when its
  folder watch fires (any change below the root, e.g. a build writing to
  `target/`); trees are capped at 100,000 rows.
- Open files are still checked by file_stream's timestamp poll (async
  threads, about once a second), not by OS notifications.
- Settings are hardcoded raddbg defaults (`rad_default_settings`). Window
  position and panel layout are not saved between runs.
- Tab inserts 4 spaces. No smart indent after `{`.
- Script mode injects input after the OS layer (as `WM_Event`s), so the
  win32 message translation (WM_KEYDOWN/WM_CHAR -> events) is not covered;
  typed text comes from `wm_codepoint_from_modifiers_and_key` (US layout).
  The linux simulated-input hooks are written but NOT compiled or run.
- Script timings are reported, not checked (no pass/fail thresholds yet).
- Cargo: the program started by Run gets no arguments yet. Arguments
  passed through the terminal wrapper must not contain `"`, `%` or `!`
  (cmd.exe). A run's job object keeps the program's terminal window alive
  until a key is pressed in it; quitting rad closes such windows.
- Cargo: diagnostics are anchored when they arrive; edits made while a
  build runs, before its diagnostics arrive, can shift them by those edits.
- Cargo: processes inherit rad's inheritable handles (`process_launch`
  passes inherit_handles=TRUE whenever it redirects output; a fix is
  PROC_THREAD_ATTRIBUTE_HANDLE_LIST in the win32 base layer).
- Cargo: Output locations with spaces in the path are not detected; the
  Output text is capped at 32 MB.
- Run in a terminal window: Linux is a stub; macOS (milestone 12) needs
  Terminal.app with the same wrapper, plus a bridge for exit code and Stop.
- `cargo_run.radtest` failed 2 times in ~90 runs (none in the last 50,
  30 of them as two parallel loops): right
  after `cargo test`, both code views showed no layout for a frame
  ("visible 0-0", "editor text not loaded"). Not reproduced on purpose yet;
  suspect: view states freed by `rad_view_state_gc` after 3 frames that
  did not build the views.
- The first launch of a freshly built exe took 15-25 s longer (seen 3
  times, debug and release); the same exe then starts in ~2 s. Cause not
  verified (suspect: antivirus scanning a new exe).

## Build, run, test (Windows)

- Open "x64 Native Tools Command Prompt for VS 2022" (VS 2022 Build Tools,
  MSVC 14.44, Windows SDK 10.0.26100), `cd` to the repo, then:
  - `build rad` -> `build\rad.exe`
  - `build rad_tests` -> `build\rad_tests.exe` (prints the check count)
  - `build raddbg` -> reference build for side-by-side UI screenshots
- Run: `build\rad.exe [file ...]`. With no file it shows the start page.
- Log: `%APPDATA%\rad\logs\ui_thread.rad_log` (written by `log_infof`).
- Every build reruns metagen, which writes `generated/*.meta.*` (and
  `src/lib_rdi/rdi.c/.h`) with CRLF. `git status` then shows ~50 modified
  files with no real change. Never stage those; stage paths explicitly.
- Building from Claude Code's shell (no developer prompt):
  `cmd /c tests\shell_build.bat rad [release]` (any build.bat target). It
  unsets `NoDefaultCurrentDirectoryInExePath` (which breaks build.bat's bare
  `metagen.exe` call), loads `vcvars64.bat`, then calls `build.bat`.
- UI tests: `powershell -ExecutionPolicy Bypass -File tests\run_scripts.ps1
  [-Exe build\rad.exe] [-Filter scroll]`. They take no focus, so they can
  run while you work. A new check = a line in a `tests\scripts\*.radtest`
  (language at the top of `src/rad/rad_script.h`).
- If `build\rad.exe` is open, linking it fails (LNK1104): build a copy with
  `cmd /c tests\shell_build_copy.bat [debug|release] [name]` (default
  `build\rad_probe.exe`) and pass it to the runner with `-Exe`.
- Screenshots: `tests\screenshot.ps1 -Script <file> -Out <png>` runs a
  script (ending in `wait <ms>` + `quit`) and captures the window with
  `PrintWindow` after `-AfterMs`, without focus. Works because rad renders
  every frame while a script runs. Use it for side-by-side UI checks.
- Old way, avoid: driving the real window with `PostMessage`/`keybd_event`
  needs the foreground, interrupts the user, and their keystrokes then land
  in rad (happened once). `PrintWindow` on an idle background rad window
  (not scripted) can block for a minute or return blank pixels.

## Code map (`src/rad/`)

- `rad_main.c`: entry point and unity build (which layers get linked).
- `rad.mdesk`: icon, theme and code-color tables copied from `raddbg.mdesk`
  (RD_ -> RAD_), plus the command table (names, icons, default keys).
- `rad_core.h/.c`: state, settings, commands (`rad_exec_cmd`), fonts, the
  window frame (port of `rd_window_frame`: top bar, menus, bottom bar,
  panels, tabs), the box renderer (verbatim), the per-frame loop.
- `rad_widgets.h/.c`: menu bar button, icon buttons, command buttons, the
  code slice (port of `rd_code_slice`).
- `rad_views.h/.c`: view states; documents (`RAD_Document`: one
  `TB_Buffer` per open file, fed from the file stream and ticked every
  frame in `rad_documents_tick`); the code view (port of
  `rd_code_view_build`) with editing; the text view (tab `text`, child
  `path`). The explorer view (tab `explorer`, child `folder`) at the end.
- `rad_widgets.h/.c` also hold `rad_cell` (port of raddbg's `rd_cell`,
  read-only), the row widget of the explorer.
- `rad_cargo.h/.c`: cargo commands: the runner thread (process with a
  pipe, job object for Stop, `--message-format=json` messages ->
  `RAD_Diagnostic`s), Run's terminal phase (`sh_launch_in_terminal`), the
  Output text (one block, submitted to a content key every 100 ms), the
  Debug/Release choice (user file: `cargo_profile`).
- `src/json/` (`JSON_`, base only): JSON parser and writer.
- `icons/`: the user's SVG icons; `make_rad_icons.py` puts them in
  `data/rad_icons.ttf` (raddbg's `icons.ttf` + these glyphs), which rad
  embeds.
- `rad_script.h/.c`: script mode (`--script:<file>`): parser, input
  queue (one step per frame), state fields, targets from the last frame's
  layout (`layout_*` fields in the code view and explorer states), timing.
- `tests/rad_tests.c`: console tests on base/content/text/text_buffer.
- Repo root `tests/` (not `src/rad/tests/`): `run_scripts.ps1`, `screenshot.ps1`,
  `scripts/*.radtest`, `fixtures/` (stored byte-exact: `-text` in
  `.gitattributes`): app-level UI tests.

`src/text_buffer/` (`TB_`): the editable buffer, see the notes at the top
of `text_buffer.h`.

## Journal

### 2026-11-04 (milestone 9, step 3: hover)
- Hover (Quick Info): the mouse resting 500 ms on a token (the code slice
  now reports the character under the mouse, not the nearest boundary)
  sends textDocument/hover (pending edits are sent first); the answer shows
  in a tooltip while the mouse stays on that token. Markdown: fenced code
  in the code font, colored by the editor's lexers; paragraphs joined and
  wrapped at 45 em; links/backticks/bold dropped; at most 24 lines.
- Found: `txt_token_array_from_string__rust` is compiled out (`#if 0`);
  `rad_fstrs_from_code_string_lang` uses `txt_token_array_from_data__rust`.
  Equipped fstrs must live in the UI build arena (not scratch).
- Script `lsp_hover` (7); fields hover_text, hover_has. 20 scripts pass.

### 2026-11-04 (milestone 9, steps 1-2: lsp layer, rust-analyzer diagnostics)
- Installed rust-analyzer (`rustup component add rust-analyzer`; the
  toolchain is aarch64-pc-windows-msvc). Probed it from Python first: it
  takes UTF-8 positions, sends the first diagnostics ~20-35 s after start
  (it indexes std on every start), tags them with the document version.
- New layer `lsp` (`LSP_`, `src/lsp/`, base + json): Content-Length
  framing (incremental parser), JSON-RPC messages, a client (server in a
  job object; reader thread stdout -> inbox; writer thread outbox -> stdin).
  rad_tests: framing in every chunk size, 3 MB bodies, broken headers.
- `rad_lsp.c/.h`: one rust-analyzer per Cargo workspace with an open .rs
  file (root = nearest Cargo.toml with [workspace], else the package).
  initialize (utf-8/utf-16 positions, progress, serverStatus) ->
  initialized; didOpen / didChange (full text, 150 ms debounce) / didSave /
  didClose. Each sent text is kept (last 4 versions): diagnostics for
  version V get their offsets from V's text, then the buffer's change log
  carries them to the current text. Hints (severity 4) dropped; the first
  line of a message is the message (rust-analyzer appends rustc's notes).
- Problems = cargo's + the servers' diagnostics, deduplicated (rust-analyzer
  runs `cargo check` on save too), errors first, by path/line. Problems tab,
  F8 and the editor use it. Its selection is kept by key across rebuilds.
- Bottom bar (right, next to the version): rust-analyzer's progress.
- Scripts: language servers are off in script mode unless `lsp on`;
  `wait_until <ms> <field> <op> <value>`; fields lsp_state, lsp_busy,
  lsp_status; runner header `#! timeout: <s>`. New `lsp_diagnostics` (19).
  19 scripts pass.

### 2026-11-03 (milestone 8 done)
- Release build in `build\release\rad.exe`. 18 scripts, 368 checks pass on
  debug and release; `rad_tests` 37,594,203 checks pass.
- Done-when check: Build/Run/Test (and Check, Stop), the Output tab,
  `cargo check --message-format=json` errors in Problems and in the editor
  (line color, underline, message), clickable locations (Problems rows,
  Output lines, F8). Plumbing for LSP: process with pipes, `json`, the
  diagnostics UI.

### 2026-11-03 (milestone 8, step 5: the user's icons)
- The user's SVG icons (`icons/`: play, stop, build, check, test) are now
  glyphs of rad's icon font: `icons/make_rad_icons.py` (fonttools) writes
  `data/rad_icons.ttf` = raddbg's `data/icons.ttf` (unchanged; raddbg still
  uses it) + the SVGs on free ASCII codepoints (2 % $ ~ t), scaled to the
  same size and center as raddbg's icons; even-odd paths get their holes
  turned around (TrueType fills nonzero). New icon kinds CargoRun/Stop/
  Build/Check/Test, used by the cargo commands (toolbar and Build menu).
  Theme colors apply as for every icon. Screenshot checked; 18 scripts pass.

### 2026-11-03 (milestone 8, step 4: Run in a terminal window, run toolbar)
- User report: a program that reads input (a shell, a database) looped on
  "input error: Incorrect function" under Run. Cause: it ran on a hidden
  console (CREATE_NO_WINDOW) with nothing to type into. User's choice: the
  Visual Studio 2022 way. Run (F5) now builds in the Output tab (errors in
  Problems and the editor, as before), then runs `cargo run -q` in a
  terminal window (the default terminal: Windows Terminal on Windows 11):
  real console (line editors, colors, Ctrl+C), "[process exited with code
  N]" + "Press any key to close this window . . .". Stop kills it.
- New `shell` API `sh_launch_in_terminal` (win32: cmd.exe wrapper on a
  new console, in the run's job object; `done_path` reports the program's
  exit code while the window waits for the key; `hidden` for scripts).
  Linux: stub. macOS (at the end): Terminal.app with the same wrapper as
  a script; exit code / Stop need a small bridge there.
- Top bar center column, port of raddbg's run controls: configuration box
  `[Debug v]` (menu: Debug, Release; `--release` on every cargo command;
  saved in the user file as `cargo_profile`), then Run, Test, Check,
  Build, Stop (Stop disabled while nothing runs). Build menu has the two
  configurations too.
- Scripts: `toolbar` (20), `cargo_run` updated; script command `terminal
  visible|hidden`, target `toolbar <name>`, field `cargo_profile`. Checked
  by hand once with a real window: Windows Terminal tab "stdin_probe
  (debug)" waiting at `db>`; Stop closed it. 18 scripts pass.

### 2026-11-03 (milestone 8, step 3: diagnostics in the editor, scripts)
- Code slice takes per-line background colors, per-line annotations and
  underlined ranges (ports of raddbg's per-line backgrounds and per-line
  extras / exception annotation). An error line looks like raddbg's
  exception line (`bad_pop` + annotation); warnings annotate only, with
  `thread_0` (yellow/orange in every preset; the theme has no warning color).
  Diagnostics anchor in open documents when they arrive and follow edits
  through the change log. Screenshots checked.
- Scripts: `cargo_build` (63), `cargo_run` (23: run exit code, stdout/stderr
  order, failing test, panic location double-click, Clear Output),
  `cargo_stop` (16: no project, Stop kills the job in ~5 frames, Output
  follows only at the end, a command during a run). New: `wait_output`,
  target `output_text`. 17 scripts pass.

### 2026-11-03 (milestone 8, step 2: cargo runner, Output, Problems)
- `rad_cargo.c/.h`: Build (Ctrl+Shift+B), Check (F7), Run (F5), Test
  (Ctrl+Shift+T), Stop (Shift+F5); Run/Stop take raddbg's Run/Kill All keys.
  Saves changed files, runs `cargo <cmd> --message-format=json` in the
  project folder (explorer folder with a Cargo.toml, else the nearest one
  above the editor's file), stdout+stderr on one pipe, in a job object
  (Stop kills cargo, rustc and the program). A runner thread splits lines:
  compiler messages -> rendered text + diagnostic; other cargo JSON dropped;
  the rest printed as is. Paths resolved against the workspace root
  (`cargo locate-project --workspace`).
- Output tab: read-only code view on a TB_Buffer following a content key
  the UI thread fills at most every 100 ms (raddbg's mutable_text copies
  the whole log per append). Follows the end; double-click a location.
- Problems tab: rows like the explorer's; Enter / double-click go there;
  F8 / Shift+F8 walk them. Both tabs open in a panel under the editor (30%)
  on the first command. Bottom bar: raddbg's run-state tags.
- Script `cargo_build` (37 checks): new `wait_cargo`, fields `cargo_*`,
  `diag*`, `output_*`, `line_diag`, targets `output`, `problem`. 15 scripts
  pass (on `build\rad_probe.exe`; `build\rad.exe` was open).

### 2026-11-03 (milestone 8, step 1: json layer)
- New layer `json` (`JSON_`, `src/json/`, base only): parser into an arena
  tree (nil-safe lookups, \uXXXX + surrogate pairs, exact integers, depth
  cap 512, error offset), string escaping, compact writer. `rad_tests`:
  hand-written values/escapes, 18 malformed inputs with their error
  offsets, 400 random documents round-tripped, every prefix and one-byte
  corruption parsed; 37,594,203 checks pass.

### 2026-11-03 (milestone 7: Rust syntax highlighting)
- The visible-range tokenizer, the whole-file pt scan and the edit re-lex
  all had C rules (Rust's `'a` turned the rest of a file into a char
  literal). Rust versions share one pair of helpers. The new resume tests
  found three bugs in the shared edit path; fixed (see milestone 7
  details). Release build: all 14 scripts pass. Design:
  `docs/notes/04-syntax-highlighting.md`.

### 2026-11-03 (handoff)
- Build helpers moved from the session scratchpad into `tests/`
  (`shell_build.bat`, `shell_build_copy.bat`); working rules and a TODO
  list added at the top so a new session can pick up without this one.

### 2026-11-03 (milestone 6 done)
- Release build in `build\release\rad.exe`; all 13 scripts pass on it
  (typing/wheel: UI build ~2 ms, every input in the next frame).

### 2026-11-03 (audit done: loading overlay)
- Ported raddbg's loading overlay. file_watch: an early wake (557 ms
  instead of ~1000) came once in 6 runs from a late reload of the earlier
  write; the script now lets it settle and checks the reload. 13 scripts
  pass (on a test copy: `build\rad.exe` was open).

### 2026-11-03 (audit, panel zoom)
- Ported raddbg's panel zoom; checked by screenshot (zoomed panel fills
  the window, restore icon at the tab bar's right end). 13 scripts pass.

### 2026-11-03 (audit, part 1)
- Error border, tab title qualifiers, selection -> search text; see the
  audit item. 12 scripts pass (find 37, tab_titles 6 checks). Checked by
  screenshot: red border + bottom-bar error, `<src>`/`<tests>` main.rs tabs.

### 2026-11-03 (file watching)
- Folder watches in `base`, watcher thread in rad, explorer refresh on
  change, idle wake-up. Script commands `write_file`, `delete_file`,
  `write_file_later`, `sleep_until_woken`. 11 scripts pass; raddbg and
  rad_tests still build.

### 2026-11-03 (session memory)
- User file load/save + window state commit; runner supports `#! args:`.
  10 scripts, 173 checks pass (on a test copy: `build\rad.exe` was open).

### 2026-11-03 (Go to Line, Find)
- Ported raddbg's query bar, search/goto commands and line editing; see
  milestone 6 details. Before this, no rad UI event carried an action slot,
  so `ui_slot_press(Accept/Cancel)` never fired (Esc did not close context
  menus); Return/Esc now set them. All 8 scripts pass (160 checks).

### 2026-11-03 (one document per file)
- `text_buffer`: change log (last 1,024 changes, one per version) and
  `tb_buffer_map_offset`. Offsets stick to the character after them. New
  `rad_tests` checks: a marker character edits never touch is mapped
  after every random op and must land on itself (36,367,785 checks pass);
  no mapping across a reload or from a future version.
- `rad`: `RAD_Document` table; view states no longer own buffers.
  Script `shared_document` (30 checks) passes, with the others.
- Script gotcha: `wait_idle` waits for the bottom-bar message to fade
  (>4 s), so "close again to discard" checks must not wait in between.

### 2026-11-03 (explorer: special panel)
- User decision: the explorer's tab cannot be closed; no other tabs in its
  panel; its width stays adjustable (no fixed-width sidebar).
- Close Tab (button not built for it, Ctrl+W, middle-click, tab menu) and
  Close Panel do nothing to it. Open File aimed at its panel (focused,
  file drop, explorer Enter) goes to the editor panel; with no editor panel
  left, a new one is made on the right (80%). Open Folder with no explorer
  (only possible from an edited layout) makes a 20% panel on the left
  (`rad_panel_alloc_at_window_edge`). Splitting its panel is allowed: the
  new panel is a normal one.
- Script `explorer_panel` (28 checks) passes; screenshot checked: the
  explorer tab has no close button, the other tabs are unchanged.

### 2026-11-03 (milestone 5: test and measure)
- Script mode + simulated input in `window_manager` + runner, 4 scripts,
  60 checks; all pass on the debug build and on the optimized build
  (`build\release\rad.exe`, the milestone build for the user).
- Bug found by the runner: rad sometimes never exited after a script. The
  script ends during the frame's tick, before that frame's event wait;
  with no frame requested the wait blocked until an OS message arrived,
  which for a background window can be never. Fix: request a frame on
  finish. 12 runs in a row exited in ~2 s (the first run had hung before).
- Measured: every input shows up in the next presented frame (16.6 ms at
  60 Hz); UI build ~2.2 ms release, ~4 ms debug. Table under milestone 5.

### 2026-11-03 (plan revised)
- Agreed with the user: new milestones 5 (test and measure) and 6 (editor
  essentials) before language work; Cargo (8) before LSP (9); removing the
  debugger (10) last on Windows. Debug builds by default, an optimized
  build at the end of every milestone. No Linux build/CI for now.

### 2026-11-03 (scroll fix)
- Bug (user): wheel/scroll bar moved the bar but not the text. Cause: the
  port dropped raddbg's per-frame view animation. Scrolling sets a scroll
  point's `idx` to the target and moves the difference into `off`
  (`ui_scroll_pt_target_idx`); content is drawn at idx+off, so `off` must
  ease to 0 every frame (raddbg_core.c, "animate view states"). Nothing did
  that in rad, so idx+off never changed. Fix: `rad_view_state_animate`
  (code view and explorer scroll points), called in `rad_frame`. Verified
  by a temporary log of the visible line range: 640 posted wheel notches
  -> lines 1921-2015 of a 3,355-line file, offset easing to 0.
- Explorer: "+" in its panel = Open Folder.
- Speed measurements (3,355-line file, 4096x2217 window, wheel scrolling):
  text ready 231 ms after startup; UI build per frame ~4.5 ms debug,
  ~1.6 ms release; frames are vsync-bound (~55-60 fps, 17.7 ms incl. the
  present wait) in both builds. `build rad` defaults to debug (/Od).
- Testing gotchas: `PrintWindow` on a background rad window can block for
  a minute or return blank/stale pixels (first measured "84 s load" was
  this, not rad: rad was idle). Use rad's log (`--logs:<dir>`) for
  measurements. Posted WM_MOUSEWHEEL works without focus (UI hit-tests
  the event position). While the user has `build\rad.exe` open the link
  fails (LNK1104); a test copy can be built with the same cl line to
  another exe name.

### 2026-11-03 (open folder)
- `sh_pick_folder` added to the shell layer (win32 + linux), `OpenFolder`
  command, File menu entry, empty-state button. raddbg and rad_tests still
  build (they share the shell layer).
- Driving the native dialog from a script: find the process's visible
  `#32770` window, press its buttons with `BM_CLICK` (id 1 = Select
  Folder, id 2 = Cancel). The folder edit box was not at the classic
  ComboBoxEx32 id 1148, so the scripted pick used the dialog's start folder.

### 2026-11-03 (explorer)
- Explorer view hooked up: view dispatch, tab title (folder icon + folder
  name), view-state touch + GC of its arenas, default 20/80 layout,
  `last_editor_panel` (where the explorer opens files).
- Build fix: `char_to_lower` does not exist in `base`; it is
  `lower_from_char` (C's implicit declaration let it compile, link failed).
- Scripted run: once, after expand + Enter on `src/rad`, `rad_views.c`
  showed the unsaved-changes dot. The same key sequence twice on scratch
  files stayed clean, and keys only reach the focused panel; the run right
  after could not get the foreground (user active). Likely a user keystroke
  while rad held the foreground; not proven. Nothing was saved.

### 2026-11-03
- Decided: Windows only until the IDE is done; macOS at the very end.
  Milestones renumbered in CLAUDE.md (Mac is now 9 and 10).
- New layer `text_buffer` (TB_): buffer logic moved out of `rad_views.c`.
  Background compaction fixes the "slower until you save" problem (the old
  random-edit test showed it: 135 s for 300-patch sessions). Undo groups
  (typing / backspace / delete merge; line breaks, pauses, cursor moves and
  undo/redo end a group). Dirty = history position != saved position, so
  undoing back to the saved text is clean again. Disk tracking by hash.
- In-app run: one of the user's keystroke bursts ("uit i") landed in rad
  while the test held the foreground. Nothing saved; noted in the testing
  section.

### 2026-11-02
- Built raddbg; read base, ui, draw and the raddbg code view; wrote
  `docs/notes/01-frame.md`, `02-code-view.md`, `03-feasibility-spike.md`.
- Decided: macOS first-class from day one; remove the debugger engine;
  option B (own frontend layer `src/rad/`, no debugger layers linked).
- `config`: replaced its only debugger call (`d_hash_from_string`) with the
  identical `u64_hash_from_seed_str8(5381, ...)`.
- `text`: LF files were reported as "bin"; now "lf" (also fixes raddbg).
- `rad`: window chrome, menus, panels and tabs ported from raddbg; matched
  against raddbg.exe screenshots. Gotcha: windows must be opened after the
  first `wm_get_events`, or Windows draws its own title bar over ours.
- `rad`: read-only code view, then editing + save (commit 675eac1). Gotchas:
  `str8_split` drops trailing empty pieces (a lone "\n" became ""), and
  `make_directory` does not create parent folders.
