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
  Stage paths explicitly (metagen leaves ~20 unchanged generated files
  marked modified). But after an `.mdesk` change, stage the generated files
  that really changed: `git diff --ignore-cr-at-eol --stat -- '*generated*'`
  lists them (until 2026-11-05 `rad.meta.*` in git lagged rad.mdesk by
  tasks G2-G7; builds rerun metagen, so only the repo copy was stale).
- Text with backslashes (Windows paths, C escapes) must not go through a
  Bash heredoc: `\r` and `\n` turn into real control characters. Write such
  edit scripts with the file-writing tool and run `python -I <file>`.

## Status (2026-11-06)

Windows only for now; macOS is milestones 11-12, at the very end (decided 2026-11-03).
Plan revised 2026-11-03 (see CLAUDE.md section 6). Builds: unoptimized day to day; an
optimized build in `build\release\raid.exe` at the end of every milestone. The product
is called Raid since milestone 10 (`raid.exe`); the code layer keeps the name `rad`.
Older entries below say `rad.exe`.

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
| 9. LSP client                        | Done. `lsp` layer; one rust-analyzer per Cargo workspace: diagnostics (Problems + editor), hover, Go To Definition (F12, Ctrl+click, Alt+Left back), completion list (as you type, `.`, `::`, Ctrl+Space; imports) (see journal). 22 scripts, 453 checks, pass (debug and release, `build\release\rad.exe` 2026-11-04). |
| 10. Remove debugger, rebrand         | Done. Debugger, debug-info layers, tools and their third-party code out of the repo and the build (421 files; tag `raddbg-reference` keeps them). Renamed to Raid: `raid.exe`, title/About, `%APPDATA%\raid\` (the old session file is imported once), README. Logo unchanged (the user's, later). 42 scripts, 833 checks, pass (debug and release, `build\release\raid.exe` 2026-11-05). |
| 11. Mac foundation                  | Done (on the Mac, not from Windows). `src/mac/base` (from `src/linux/base`), ARM64 in `base`, `./build.sh rad_tests` on macOS: 37,610,802 checks, 0 failures (debug and release, ARM64, macOS 15.7). CI: `.github/workflows/macos.yml`. |
| 12. Mac UI                           | Done (2026-11-06; the user tested it on the Mac). Cocoa window manager, Metal renderer (2026-11-06; the first one, OpenGL 4.1, stays as a fallback: `./build.sh raid opengl`), FreeType + CoreText font names, Mac shell incl. Terminal.app runs. `tests/run_scripts.sh`: 45 scripts, all pass on the release build. Left for later (TODO): IME/dead keys, file drop, .app bundle, a side-by-side screenshot check against Windows. |
| 13. Debugging                        | Not started. |

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

0. **Now: `docs/TODO_2.md`** (the user's list of 2026-11-06 plus
   suggestions). `docs/TASKS.md` (A-H) is done. Hint images are in
   `.dev_hints/` (private, git-ignored, never push it).
1. Speed, continued (see the 2026-11-06 journal entries; app side:
   `tests/perf/run_perf.sh`; key to screen: `tests/perf/typelat.m`). The
   Mac renderer is Metal now; typing latency on a 60 Hz monitor is at
   Zed's level (~35 ms, most of it the monitor and the compositor).
   Candidates: the 120 Hz panel not measured with the final build (only
   the external monitor was connected); fuse the present into the UI pass
   with framebuffer fetch (memoryless stage, ~1.5 ms less GPU at
   4096 x 2304); a fast-typing measurement (typelat's keys are 180-320 ms
   apart; the caret slide after each key lasts 12 frames, 200 ms at 60 Hz);
   fold ranges from a worker or incrementally, the compaction copy/hash off
   the UI thread.
   - Logo: the user will supply one later (`data/logo.ico` for the exe,
     window and About; `data/logo.png` for Linux).
   - LICENSE holds Epic's notice only; the user may add their own line.
2. Mac leftovers (TODO below): IME/dead keys, file drop, .app bundle.
3. Not yet compared side by side with raddbg.exe: the query bar, the zoom
   button, the loading overlay (line-by-line ports; needs raddbg focused;
   raddbg.exe now builds from the `raddbg-reference` tag, see "Build, run,
   test").

## TODO (later)

- Mac: IME / dead keys, file drop onto the window, an .app bundle, a
  side-by-side screenshot check against Windows (milestone 12 leftovers).

- Tab drag/drop between panels (port raddbg's: drop sites on tab bars and
  panel edges, the drag visualization, `MoveTab`). Asked for by the user
  2026-11-03, after milestone 6.

## Known limits

- Scope lines and end-of-scope annotations are off while patches exist,
  i.e. while typing; they come back ~0.5 s after typing stops (compaction).
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
  threads, about once a second), not by OS notifications. Folder watches
  (explorer) are OS notifications on Windows and the Mac (FSEvents); Linux
  still polls once a second.
- Settings: `RAD_SettingTable` in rad.mdesk, stored in the user file under
  `settings:`, File > Settings tab. A number can't be typed in yet (slider,
  Left/Right).
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
- `file_watch.radtest` failed once (2026-11-04) in a full run: rad was
  woken after 471 ms instead of the 1 s write (>= 800 checked); passed 3
  of 3 right after. Timing under load; not investigated.
- LSP: documents are synced with their full text on every change (no
  incremental sync); fine for normal files.
- LSP: one rust-analyzer per Cargo workspace (the nearest Cargo.toml with
  `[workspace]`, else the package), started on the first open .rs file and
  kept until rad quits. Build > Restart Language Server restarts every
  server (in place); a crashed one is restarted, at most 3 times a minute.
  A server that breaks the protocol stays ended until the command. Library sources (paths with
  `/.rustup/toolchains/`, `/.cargo/registry/`, `/.cargo/git/`) join an
  existing server by that path test, never start one.
- LSP hover: markdown is reduced to code blocks (code font, lexed) and
  wrapped paragraphs; links, emphasis, tables are dropped as plain text.
- LSP completion: snippets become plain text (the cursor at the first tab
  stop; no Tab between arguments; rust-analyzer is asked for
  `add_parentheses`, so no argument placeholders). Labels show rust-
  analyzer's `()`/`(…)`/`<…>`; import items show rust-analyzer's
  "(use path)" in the label. An import's `use` line is resolved while the
  item is selected; one committed before the answer gets it later (the same
  undo step if nothing happened since, else a second one). A word start opens the
  list only when it is the only character typed in its frame. Filtering:
  prefix, then substring, then subsequence (raddbg's highlight marks
  substring matches only).

## Build, run, test (Windows)

- Open "x64 Native Tools Command Prompt for VS 2022" (VS 2022 Build Tools,
  MSVC 14.44, Windows SDK 10.0.26100), `cd` to the repo, then:
  - `build raid` -> `build\raid.exe` (source: `src\rad\rad_main.c`)
  - `build rad_tests` -> `build\rad_tests.exe` (prints the check count)
  - `build` with no target builds `raid`. Only `raid` and `rad_tests` are
    left (milestone 10).
- raddbg.exe, the reference for side-by-side UI screenshots, builds from
  the `raddbg-reference` tag in a second worktree:
  `git worktree add ..\rad_reference raddbg-reference`, then in that folder
  `tests\shell_build.bat raddbg` -> `..\rad_reference\build\raddbg.exe`.
  (A copy built before the removal may still sit in `build\raddbg.exe`;
  builds here no longer write or delete it.)
- Run: `build\raid.exe [folder or file ...]`. With no file it shows the
  start page.
- Data: `%APPDATA%\raid\` (`--appdata:<folder>` stands for `%APPDATA%`):
  session and settings `default.raid_user` (imported once from
  `%APPDATA%\rad\default.rad_user` while it does not exist), log
  `logs\ui_thread.raid_log` (written by `log_infof`; `--logs:<dir>`
  elsewhere), rust-analyzer's stderr `logs\rust-analyzer_<n>.log`.
- Every build reruns metagen, which writes `generated/*.meta.*` with CRLF.
  `git status` then shows ~20 modified files with no real change. Stage
  paths explicitly (see the rule at the top for real changes).
- Building from Claude Code's shell (no developer prompt):
  `cmd /c tests\shell_build.bat raid [release]` (any build.bat target). It
  unsets `NoDefaultCurrentDirectoryInExePath` (which breaks build.bat's bare
  `metagen.exe` call), loads `vcvars64.bat`, then calls `build.bat`.
- UI tests: `powershell -ExecutionPolicy Bypass -File tests\run_scripts.ps1
  [-Exe build\raid.exe] [-Filter scroll]`. They take no focus, so they can
  run while you work. A new check = a line in a `tests\scripts\*.radtest`
  (language at the top of `src/rad/rad_script.h`).
- If `build\raid.exe` is open, linking it fails (LNK1104): build a copy with
  `cmd /c tests\shell_build_copy.bat [debug|release] [name]` (default
  `build\rad_probe.exe`) and pass it to the runner with `-Exe`.
- Screenshots: `tests\screenshot.ps1 -Script <file> -Out <png>` runs a
  script (ending in `wait <ms>` + `quit`) and captures the window with
  `PrintWindow` after `-AfterMs`, without focus. Script windows are
  1920x1080 px (fixed, any display). Works because rad renders
  every frame while a script runs. Use it for side-by-side UI checks.
- Old way, avoid: driving the real window with `PostMessage`/`keybd_event`
  needs the foreground, interrupts the user, and their keystrokes then land
  in rad (happened once). `PrintWindow` on an idle background rad window
  (not scripted) can block for a minute or return blank pixels.

## Build, run, test (macOS)

- Needs Xcode (or its command-line tools), clang, and for `raid` later
  FreeType (`brew install freetype pkg-config`).
- `bash build.sh rad_tests [release]` -> `build/rad_tests` (prints the
  check count; exit code 0 = pass). `bash build.sh raid [release]` ->
  `build/raid`.
- `tests/run_scripts.sh [-e build/raid] [-f filter]`: every `.radtest`.
- `tests/perf/run_perf.sh [-e exe] [-s "3000 10000"] [-l "off on"]`: typing
  benchmarks on generated Rust crates (MEASURE lines, not pass/fail).
- Profiling: `sample <pid> 6 -file out.txt` while a script types.
- Renderer: Metal (`src/render/metal`). `bash build.sh raid opengl` builds
  the OpenGL one instead (the Mac's first backend; with it,
  `RAID_SWAP_INTERVAL=0 build/raid` turns off the refresh wait).
- Key-to-screen latency of any app (Raid, Zed, ...): build `tests/perf/typelat.m`
  (build line at its top) -> `build/typelat`; the terminal needs Accessibility
  and Screen Recording. Unattended, on a scratch session (keys go to that
  process only): `build/raid --user:/tmp/lat.rad_user some.txt &` then
  `build/typelat -pid $! -d 1 -n 40`. The user file keeps the monitor
  (`monitor:`), so reusing a copy pins the window to one screen.
- CI: `.github/workflows/macos.yml` builds and runs `rad_tests` (debug and
  release) on every push.

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
- `src/lsp/` (`LSP_`, base + json): language server client: Content-Length
  framing, JSON-RPC messages, a process with pipes and a reader and a
  writer thread (`LSP_Client`).
- `rad_lsp.h/.c`: rust-analyzer per workspace (notes at the top of the
  header): document sync, diagnostics, progress, hover, Go To Definition and
  Navigate Backward, completion (request, items, filtering, resolve), and
  the merged Problems list (`rad_problems`). The completion popup, commit
  and triggers are in `rad_views.c` (`rad_completion_popup_ui`,
  `rad_code_view_completion_*`); the popup is built in `rad_window_frame`
  before the top bar (raddbg's floating views).
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

### 2026-11-06 (TODO_2: keys, zoom, tabs, status bar, panel font, explorer, inlay hints)
- `docs/TODO_2.md`: the user's new list (keys, look, diagnostics like Zed,
  inlay hints, explorer file operations) plus suggestions.
- Cmd+- split the panel: Ctrl+- was raddbg's Split Panel Down and the Mac
  sends Cmd as Ctrl. Now Cmd+= / Cmd+- (and with Shift) zoom the code;
  no key zooms the whole UI. Commands can have second keys and Mac-only
  keys (`rad_cmd_extra_binding_table`, `rad_cmd_mac_binding_table`).
- Mac text keys: Cmd+arrows to line/file ends, Option+arrows by word,
  Option/Cmd+Backspace/Delete; Navigate Backward is Cmd+[ on the Mac
  (Option+Left was Alt+Left = back).
- Cmd+wheel zoom is smooth: a fractional target eased every frame,
  quarter-pixel sizes, settles on a whole size 150 ms after the wheel
  stops; 1.0-1.4 ms per frame while zooming.
- Tabs: flush with the panel, no file icon, pin/close only on hover (room
  kept), modified dot in the close slot, a little lower. Status bar 2x the
  font (was 3x) with line:column and the language. Panel Font / Panel Font
  Size for Output and Problems.
- Inline diagnostics like Zed's (setting Inline Diagnostics): the line's
  worst error/warning in a tinted box after the code; wavy underlines; no
  red line background. Themes get a "warn text" color.
- Explorer folders: the user's two-tone folder icon (`icons/Folder_16x.svg`),
  the same open or closed.
- Explorer file operations: right-click menus (raddbg's look), New File /
  New Folder / Rename typed in the tree, Delete to the Trash after raddbg's
  confirmation popup (ported: `rad_popup_open`), Cut / Copy / Paste, Copy
  Path, Copy Relative Path, Reveal, Open, Open to the Side; the explorer's
  own keys (F2, Cmd+C/X/V, Cmd+Backspace, Cmd+Option+C/R). Open tabs and
  documents follow renames and moves. New: `sh_move_to_trash`, script
  `right_click`, queries `path_exists`, `popup_active`,
  `explorer_editing`, `explorer_menu_open`.
- Fixed: the explorer's keys were global (Cmd+C in the editor ran
  Explorer Copy): `rad_binding_from_cmd_kind` fell back to their table;
  now only the menu labels do.
- Inlay hints (setting Inlay Hints): rust-analyzer's `: i32` and
  parameter names, in the weak text color, inside the line but not in
  the text; cursor, clicks, selection, underlines and popups skip them.
  Carried over edits like the symbols. Query `inlay_hint_count`,
  `lsp_hint_state`.
- Quick fixes (the screwdriver): 1 s after the cursor stops (was 0.3 s),
  setting Show Quick Fixes (Ctrl+. works either way).
- Script language: `only_on`, `float_window` (a covered window is not
  drawn by Metal: screenshots of script runs need it), `wheel_units`,
  query `output_font_size`. Scripts no longer press Alt+Left for back
  (`cmd navigate_back`: the key differs per OS).

### 2026-11-06 (Metal renderer for the Mac)
- Why: on a 60 Hz monitor typing felt slow next to Zed. Measured key to
  screen with the new `tests/perf/typelat.m` (posts keys, watches the
  window with ScreenCaptureKit): GL Raid 41.6 ms avg vs Zed 34.7 (the
  user's run); Raid's samples clustered at ~41 and ~48 ms, its best equal
  to Zed's.
- `src/render/metal` (`R_BACKEND_METAL`, the Mac default; `build.sh raid
  opengl` for GL): the same frame structure as the GL backend, shaders
  ported line for line (`render_metal.mdesk`), one CAMetalLayer per window
  (sRGB-tagged like the window). Pixels: 3,624 of 9.4M differ from GL, by
  1-2 of 255 (stage RGBA16F, D3D11's format, instead of GL's RGBA32F).
  The blur samples linearly, as D3D11 (GL sampled nearest).
- What made it fast, measured (GPU time per frame at 4096 x 2304):
  precise math 9.9 ms -> fast math 4.1 ms, same pixels; consecutive UI
  passes share one encoder (one load/store of the stage, not one per
  pass); blurs of width 0-1 are skipped (raddbg fades a blur by scaling
  its size: the editor got two full-area copy passes every frame).
- Frames identical to the one on screen are not drawn or presented (hash
  of the passes' parameters and instances, plus texture uploads). rad
  draws several frames per input and each presented frame takes a refresh;
  per keystroke 12 frames change (raddbg's caret slide), the rest are
  skipped. Covered or minimized windows are not drawn (the compositor
  drops those drawables; nextDrawable can stall a second); the WM wakes a
  frame when a window is uncovered (`windowDidChangeOcclusionState`).
- The next frame's drawable is taken right after a present (2 drawables):
  the wait for the refresh comes before the next frame reads its input.
  Live resize presents in Core Animation's transaction.
- WM: the refresh rate is the key window's screen's (was the main screen's
  at startup).
- Mac folder watches: FSEvents (was a stub: the explorer saw a new file
  only at its once-a-second rescan). Found because `file_watch` failed on
  Metal (explorer_rows 4, expected 5); why it passed on GL was not checked.
- Result, 60 Hz monitor, 40 keys each, same session: GL 36.5 ms avg (40.4
  median), Metal 34.6 (33.2 median, p95 41.6) = Zed's level. Before the GPU
  fixes Metal was slower than GL (42-48 ms; 224 ms without the skip).
- `tests/perf/typelat.m -pid <pid>`: keys to that process only (unattended
  runs). 44 scripts (871 checks) pass on Metal; rad_tests 37,610,902
  checks, 0 failing; the OpenGL build still compiles.

### 2026-11-06 (milestone 12 done; typing speed; completion list size)
- Milestone 12 closed: the user tested Raid on the Mac; the release build
  (`./build.sh raid release`) passes every script (the Launch Services stall
  of 2026-11-05 did not come back). IME, file drop, .app bundle and the
  screenshot comparison moved to TODO.
- Speed. New benchmark: `tests/perf/run_perf.sh` (generated Rust crates,
  human-paced typing and a burst, LSP on/off). Profiled with `sample`.
  Found and fixed, in order of measured cost:
  1. Semantic tokens were carried over every change since the server's
     answer, for every token, on every keystroke (O(tokens x changes); ~87%
     of frame time at 10k lines with rust-analyzer). Now the last carried
     copy is moved over the new changes only, in place:
     `tb_buffer_map_sorted_ranges` (binary search to the first touched
     token, a tight shift loop for insertions). Document symbols (navbar)
     the same way (they nest: a plain per-change loop).
  2. GL submit on macOS: one `glBufferSubData` per batch into a buffer the
     last draw still used made Apple's GL-on-Metal flush the context into a
     new Metal command buffer each time (and wait for a free one). Now a UI
     pass packs all instances and uploads them with one `glBufferData`
     (orphaning); uniform locations looked up once per pass. Pixels
     unchanged (screenshot diff: only an animating hover differs).
  3. Fold ranges: a whole-file copy + scan on every keystroke (~3 ms/MB).
     Now the last ranges are carried by their line-start offsets while
     patches exist (no line conversion when one edit kept the line count),
     and rescanned once the text settles (compaction), without a copy
     (`memory_map_data_from_range_no_copy`).
  4. Compaction pause 100 ms -> 500 ms (it copied + hashed the file after
     most keystrokes at a normal typing pace), and it no longer keeps frames
     coming while it waits: new `rad_request_frame_at` (the watcher thread
     wakes the UI at the time). The 150 ms didChange wait uses it too.
  5. LSP: incremental `didChange` (the span between the common prefix and
     suffix of the last sent text and this one, never splitting a UTF-8
     character) when the server's textDocumentSync change kind is 2;
     positions from the snapshot's line starts (binary search; was a scan
     from offset 0 per request); the text materialized straight into the
     snapshot; responses parsed on the client's reader thread
     (`lsp_client_take_messages`, one arena per message).
  Script-mode input latency (app side, no vsync), avg / p95, before -> after:
  3k lines LSP on burst 5.3/8.3 -> 1.6/2.9 ms; 10k LSP on human 6.2/7.4 ->
  1.7/2.7, burst 13.1/18.3 -> 1.5/2.9; 50k (1.3 MB) LSP off human 7.3/8.0 ->
  1.7/2.7. Every case is now ~1.5-1.7 ms whatever the file size; the main
  thread mostly waits in `flushBuffer`. Tried and reverted: 2 frames per
  input instead of 4 (tool_windows: the close button stopped working).
- Bug found by the new sync test: arrows, Backspace and Delete stepped by
  bytes (a raddbg TODO in `ui_single_line_txt_op_from_event`), so
  Backspace after `é` left a broken byte. They step by UTF-8 characters now.
- Completion list size: settings "Completion List Width" and "Completion
  List Height" (Editor group, in text heights, default 30 x 30 = raddbg's).
- Tests: new `lsp_sync` (incremental sync with multi-byte edits),
  `lsp_completion_size`; `lsp_semantic` now waits for `u32`'s color (its
  first answer could come before rust-analyzer resolved builtin types: it
  failed 4/4 on the old binary on this machine); `settings_view` counts 33
  rows; `rad_tests`: range carrying against per-offset mapping (random
  edits) and the no-copy read. 45 scripts pass (release), rad_tests
  37,610,902 checks, 0 failing.

### 2026-11-05 (performance follow-up)
- Corrected the first report's claim that no Mac timing results existed:
  completed `.perf_tmp` script outputs were present but had not been included.
  The paired 3k-line LSP-off/LSP-on runs and 5k/10k LSP-on runs are recorded
  in `PERFORMANCE_ISSUES.md`; compiler flags were not captured, so they are
  not labeled release measurements.
- Deeper source tracing found that semantic tokens and document symbols are
  mapped across the buffer change log after each edit. Each offset performs
  validation and remapping passes through intervening edits; token/symbol
  counts multiply this work. LSP response JSON is parsed on the UI thread,
  and the 150 ms change delay requests more frames while it waits.
- Confirmed the Mac backend uses swap interval 1 and samples the main
  screen's maximum refresh rate at startup. A 120 Hz display can present at
  120 Hz when frames meet the ~8.3 ms budget, but moving displays may leave
  the startup rate stale.
- Rebuilt the optimized Mac executable successfully. Its script launch
  stalled with Launch Services/XPC errors and produced no release timings;
  the original `build/raid` was restored. A verified release comparison and
  per-stage profile remain open work.

### 2026-11-05 (performance investigation)
- Traced typing through the patch-based text buffer, editable code view,
  document tick, compaction, frame loop, Mac OpenGL present, and rust-analyzer
  synchronization. Findings and source references: `PERFORMANCE_ISSUES.md`.
- Confirmed a per-version full-file fold scan, full-document LSP serialization
  on the UI thread at 150 ms intervals, and whole-document compaction/hash on
  the UI thread after 100 ms or 32 patches. The old 3,000-line 60 Hz release
  baseline measured 2.4 ms average UI build and 16.6 ms input-to-present, but
  it is not a current Mac benchmark and script mode leaves LSP disabled by
  default.
- Built a separate optimized ARM64 executable. The first Mac perf-script
  attempt did not reach script initialization; a later follow-up found
  completed local script output, but the flags were not recorded. See the
  follow-up entry for the release launch retry and its limits.

### 2026-11-05 (milestone 12, step 2: Raid runs on the Mac)
- `src/mac/window_manager` (Cocoa): wm_get_events pumps NSApp's queue itself
  (nextEventMatchingMask; NSApp never runs its loop); input translated
  before AppKit dispatch, key events never handed to AppKit (no beeps,
  key-ups with Cmd held). Pixels + DPI like Windows (96 x backing scale).
  Cmd and Control = Ctrl, Option = Alt; Cmd+Q closes the window. Custom
  border: transparent full-size title bar, traffic lights hidden, rad's
  own buttons; title-bar drag via performWindowDragWithEvent, edges resize
  natively. Live resize draws whole frames from windowDidResize (no
  nested pump). Trackpad scroll accumulated into 40-unit steps (2.5 units
  per point; tune by feel). Script mode (wm_sim_*) wired like Linux.
- `src/render/opengl/mac`: one NSOpenGLContext (4.1 core) shared by all
  windows. Apple's gl3.h prototypes collide with the backend's function
  pointers: renamed away around the include (list = R_OGL_ProcedureXList).
  First run looked too dark (user): macOS's window framebuffer ignores
  GL_FRAMEBUFFER_SRGB, so the linear staging showed raw. Now a present pass
  encodes sRGB (render_opengl.c, `#if OS_MAC`), and windows are tagged
  sRGB (no wide-gamut shift). Checked by readback: the frame's colors are
  the theme's hex values (#202020, #252525, #d4d4d4). Debug aid:
  `RAID_SCREENSHOT=<png>` writes the 60th frame (no Screen Recording
  permission needed). glDebugMessageCallback is absent on macOS (guarded).
- Fonts: FreeType opens a path, else a file name in the Mac font folders,
  else a family name via CoreText (case-insensitive; .ttc faces picked by
  PostScript name).
- Shell: `sh_launch_in_terminal` opens a Terminal.app window running a
  script (title, cd, command, exit code -> done file, "[process exited with
  code N]", any key). Raid holds a watcher process; a guardian in the
  script kills it when the watcher dies (Stop). Hidden (script mode): /bin/sh
  with output to the pipe. Visible mode NOT yet tried by hand.
- Base (Mac and Linux): a ProcessGroup held one pid, so Cargo's run (build
  and program in one group) could not launch the program. Now up to 16
  members, each its own process group, all killed on close. Linux: same
  edit, not compiled.
- Tests: `tests/run_scripts.sh` (bash port of run_scripts.ps1). Scripts made
  portable: `#! args` paths use `/`; cargo_run checks `src` and `main.rs:23:9`
  separately; `#! mac: "a" => "b"` header lines swap Windows font names for
  Mac ones (settings_more). 42 scripts, 834 checks, all pass.

### 2026-11-05 (milestone 12, step 1: raid links on the Mac)
- Port surface checked: only `window_manager`, `shell`, the GL backend's
  `r_ogl_os_*` seam and `font_provider` are per-OS; everything above has no
  OS switches (details and order: `docs/notes/03-feasibility-spike.md`).
- On macOS `raid` compiles as Objective-C (`-x objective-c`, no ARC; C is
  valid Objective-C, so the unity build stays one translation unit).
  `src/mac/mac_cocoa.h` imports Cocoa once, with our `internal`/`global`/
  `local_persist` macros and CarbonCore's `FileInfo` kept out of its way.
- `src/mac/shell`: NSAlert, NSOpenPanel (file/folder), Finder reveal,
  `openURL`. Not done: install/uninstall (needs an .app bundle) and
  `sh_launch_in_terminal` (returns no process, like Linux).
- Font provider: FreeType on macOS. Family names (H3) need CoreText; not yet.
- Built with `-DWM_STUB=1 -DR_BACKEND=0` for now (build.sh TODO): 0 warnings,
  starts and runs (no window).

### 2026-11-05 (milestone 11: Mac foundation; milestone 11 done)
- Done on the Mac itself (we are on one now), not "compile-only from
  Windows" as the plan said. Machine: Apple Silicon (arm64), macOS 15.7.3,
  Homebrew clang 21.1.8, Xcode, FreeType from Homebrew.
- `base` on ARM64: `ARCH_64BIT` is now set for ARM64 (it was x64 only, so
  pointer atomics and `IntFromPtr` hit their `#error`s); the "x64 only"
  `#error` allows ARM64; `immintrin.h`/`emmintrin.h` only on x86/x64
  (`base_core.c`'s SSE2 code already had a portable `#else`);
  `PathStyle_SystemAbsolute` and the raddbg-markup stubs cover `OS_MAC`.
- `src/mac/base/mac_base.{h,c}` (`MAC_`): a copy of the Linux backend with
  the Linux-only parts replaced: `arc4random_buf` (getrandom),
  `shm_open`+`shm_unlink` (memfd_create), `pthread_threadid_np` (gettid),
  `pthread_setname_np` (prctl), dispatch semaphores for unnamed semaphores
  and polled `sem_trywait` for named ones (macOS has no unnamed `sem_init`
  and no `sem_timedwait`), a mutex+condvar barrier (no
  `pthread_barrier_t`), relative condvar waits
  (`pthread_cond_timedwait_relative_np`; no CLOCK_MONOTONIC condvars),
  `fcopyfile` (macOS `sendfile` only sends to sockets), `F_PREALLOCATE`
  (fallocate), `host_statistics64` / `hw.memsize` / `_NSGetExecutablePath`
  / `sysctl(KERN_PROC)` (sysconf pages, /proc). `<mach/mach.h>` has struct
  members named `internal`, so `mac_base.h` undefines our `internal` macro
  around it. Data folders: `~/Library/Application Support/raid/` (settings
  and logs, like `%APPDATA%`), cache in `~/Library/Caches`.
- Not ported (same as Linux on ARM64, no users today): memory
  placeholders and the demand-paging fault handler (`return 0`).
  File watching polls, like Linux (`file_watch_check` always says changed).
- `build.sh`: macOS flags (no `-msse2`, `-mcx16`, `-D_GNU_SOURCE`, `-lrt`,
  GNU linker groups); two `[[ -v ]]` tests replaced (macOS ships bash 3.2).
  `rad_tests.c` printed with MSVC's `%I64u`; now `%llu` (works on both).
- `rad_tests`: 37,610,802 checks, 0 failures, debug and release, ~10 s.
  NOT rebuilt on Windows after the shared `base` edits (no Windows machine
  here); they are preprocessor-only for MSVC/x64.

### 2026-11-05 (milestone 10, step 2: Raid; milestone 10 done)
- The user named the product Raid; the logo stays until theirs comes.
- Product-facing: `BUILD_TITLE "Raid"` (window title, start page, About,
  status bar, rust-analyzer's `clientInfo`); build target `raid` ->
  `raid.exe` (build.bat, build.sh, run_scripts.ps1 and screenshot.ps1
  defaults); `%APPDATA%\raid\` with `default.raid_user` and
  `logs\ui_thread.raid_log` (names in rad_core.h "Product Names On Disk");
  user-file header `// raid <version> user file` (`// rad ` still read);
  crash dump `Raid_crash_dump.dmp`; issues link -> github.com/hamzader1/rad;
  X11 class `raid`/`Raid` and the `~/.local/bin/raid` link (linux, not
  compiled). The layer keeps its code name (`src/rad/`, `RAD_`, `rad_tests`,
  `.radtest`): renaming identifiers would show nowhere.
- Session import: while `<appdata>/raid/default.raid_user` does not exist,
  the user file is read from `<appdata>/rad/default.rad_user` and the hash
  reset, so the first save check writes the new file; the old one is never
  written. New `--appdata:<folder>` (stands for %APPDATA%; in script mode
  it opts into the default user file) makes that testable: scripts
  `session_import_1/2` (fixture `tests/fixtures/appdata/rad/`). Found
  while writing it: tab paths in a user file are taken as written (only
  schema `path` fields are resolved against the file's folder, and rad's
  tabs have no schema), so the fixture holds no file paths.
- `git describe` matched the new `raddbg-reference` tag ("raddbg-
  reference-1-g<hash>" in the start page and status bar): build.bat and
  build.sh now describe with `--match "v[0-9]*"` (version tags only).
- README.md rewritten for Raid (credit and MIT notice kept); LICENSE
  unchanged (Epic's notice).
- Checked: screenshot (start page "Raid", status bar
  "Raid (0.9.30 ALPHA) ... [<hash>]"); rad_tests 37,610,802 checks, 0 failing;
  all 42 UI scripts pass (833 checks) on debug and on release
  (`build\release\raid.exe`, the milestone build).

### 2026-11-05 (milestone 10, step 1: the debugger out of the repo)
- The rule (user): remove only what is unused now AND not needed later.
- Measured: the include closure of `rad_main.c`, `rad_tests.c` and
  `metagen_main.c` (every `#include "..."` on both sides of every `#if`, so
  Linux files count as used): 194 files. base; win32 + linux (base, shell,
  window_manager; win32/x64); artifact_cache, mdesk, window_manager, shell,
  config, content, file_stream, text, text_buffer, mutable_text, json, lsp,
  font_provider (dwrite, freetype), render (d3d11, opengl, stub), font_cache,
  draw, ui; `lib_raddbg_markup` (via `base_markup.h`); `x64/x64_exceptions.h`
  (linux base); third_party martins_hash, martins_memfun, radsort, stb,
  xxHash. Every other folder was included only from debugger or tool entry
  points (raddbg, torture, radlink, radbin, raddump, scratch, mule).
- Checked against later milestones: Mac (11-12) needs base, linux/base as
  the template, window_manager, shell, render/opengl, font_provider/freetype:
  all kept. Debugging (13, lldb-dap): DAP is JSON over stdio with LSP's
  framing (json + lsp); lldb does symbols, unwinding, expressions and
  disassembly, so no engine or debug-info layer is needed; raddbg's views
  are read from the tag when ported. C/C++ later: clangd over LSP and the
  `text` layer's C lexer. http/socket: no planned use (and no Mac backend);
  restore from the tag if that changes.
- Removed (421 files): `raddbg`, the engine and debug-info layers, the tools,
  `zydis`/`blake3`/`sinfl`/`rad_lzb_simple`/`martins_bitscan` (list in
  CLAUDE.md section 3); all of `x64/` but `x64_exceptions.h`; the empty
  `metagen_base`/`metagen_os` stubs; `natvis/raddbg.natstepfilter`;
  `obsolete/`; root `run_tests.bat/.sh` (radlink/radbin torture tests),
  `launch.vs.json`, `project.4coder`, `project.raddbg`, `CHANGELOG.md`
  (raddbg's); `data/` fonts nothing embeds (Inconsolata, Roboto,
  liberation-mono).
- Kept on purpose: `natvis/base.natvis` (linked into rad.pdb),
  `base/tests/base_tests.c` (a base test program, useful for the macOS
  runner in milestone 11), `data/icons.ttf` (input of make_rad_icons.py),
  `data/logo.png` (linux shell), `lib_raddbg_markup`. Kept layers are not
  trimmed inside (unused ui widgets etc. may serve later ports).
- build.bat / build.sh: targets `rad` and `rad_tests` only, default `rad`;
  dropped the blake3 assembly, Oodle, PGO (its training run was radlink's),
  com_shim. Kept: msvc/clang, debug/release, asan, ubsan, telemetry, spall,
  opengl, dwarf, git hash, natvis, icon, metagen. `.gitattributes`: lines
  for removed files dropped.
- Tag `raddbg-reference` (pushed) = the last commit with all of it.
- Checked: metagen rebuilt from scratch and run (12 mdesk files, 31
  tables); kept layers' generated output unchanged apart from CRLF, except
  `rad.meta.*`, stale in git since G2-G7 (now committed); `rad_tests`
  37,610,802 checks, 0 failing; all 40 UI scripts pass (822 checks, debug; first written as 548, a
  count cut short by `tail`).

### 2026-11-04 (tasks H1-H3: feedback on G)
- H1: completion rows push the label as one plain fstr (`text` color).
  Icon `SymbolInterface` (U+E001, `symbol-interface.svg`) for completion
  kind 8 and symbol kind 11; `data/rad_icons.ttf` has 97 glyphs.
- H2: the hover tooltip anchors through `ui_state->tooltip_anchor_key` to
  a floating 1px box at the token (root parent, window pixels); side and
  line cap from `RAD_LspHover.natural_height` (measured on uncut frames,
  so the choice is stable). Ctrl held -> no hover. Script fields
  `hover_shown`, `hover_covers_token`.
- H3: `fp_font_open` (dwrite) falls back to
  `IDWriteFactory_GetSystemFontCollection` + `FindFamilyName` +
  `GetFirstMatchingFont(400, 5, 0)`; the face's first file becomes the
  handle's file (vtable wrappers added in `font_provider_dwrite.h`).
- All 40 scripts pass.

### 2026-11-04 (task G5: navigation bar)
- rad_lsp.c "Document Symbols": `textDocument/documentSymbol` per sent
  version (like semantic tokens); the hierarchy flattened in document
  order (`RAD_LspSymbol`: parent index + 1, depth; explicit stack, the JSON
  values being singly linked); `rad_lsp_symbols_from_document` carries
  ranges and name offsets to the current version.
- rad_views.c `rad_navigation_bar_ui` (built after the code view, over
  `nav_bar_rect`); the text view now places the code view in a box at
  `code_area_rect` (the code view lays out from its parent's origin).
- Setting `navigation_bar`; script fields `nav_item`, `nav_member`,
  `symbol_count`; targets `nav_box <item|member>`, `nav_list_item <n>`.
- Section G done. All 39 scripts pass.

### 2026-11-04 (task G4: code actions)
- rad_lsp.c "Code Actions": `RAD_LspCodeActions` (rest: cursor, mark,
  version, time; one request at a time; the answer is shown only for the
  state it was asked for); `rad_lsp_apply_workspace_edit` (documentChanges
  or changes; snapshots of the version named); resolve when an action has
  no edit yet (rust-analyzer always resolves).
- Code slice `action_row` -> the button (`RAD_IconKind_Screwdriver`, U+E000:
  the first icon past ASCII, UTF-8 in `RAD_IconTable`); the code view
  opens the menu (`_code_actions_menu_%p`).
- Command QuickActions (Ctrl+.); script fields `code_actions_count`,
  `code_action <n>`, `code_actions_shown` (laid out); targets
  `code_actions_button`, `code_action_item <n>`.
- Found by `lsp_semantic`: the button over column 1 of an unindented line
  took the click meant for the text; it now goes left of the text there.

### 2026-11-04 (task G3: signature help)
- rad_lsp.c "Signature Help": `RAD_LspSignature` (one at a time; the last
  asked version + cursor); `rad_lsp_take_signature` finds the active
  parameter's label in the signature label after its `(` (no label
  offsets asked for).
- rad_views.c: `rad_code_view_completion_update` starts / re-asks / closes
  it; the code view records its anchor (the cursor's line); Esc closes it
  when no completion list takes Esc; `rad_signature_popup_ui` (built with
  the completion popup, before the panels).
- Command ParameterInfo (Ctrl+Shift+Space); script fields
  `signature_open`, `signature_label`, `signature_param`.

### 2026-11-04 (task G6: semantic tokens)
- rad_lsp.c "Semantic Tokens": legend from the initialize answer
  (`sem_type_slots`: type index -> slot + 1; `sem_unsafe_mask`); a request
  per sent version (`sem_asked`, `sem_request_version`; ContentModified ->
  again after 0.5 s; `workspace/semanticTokens/refresh` -> again); the
  answer -> offsets in the snapshot of that version (`sem_tokens`);
  `rad_lsp_token_colors_from_document` carries them to the buffer's
  version once per version (start: default side, end: Side_Min).
- Code slice: `token_colors` (sorted); a lexer token that starts at one
  takes its slot (not comments or strings).
- `RAD_CodeColorTable`: CodeTypeParameter, CodeLifetime, CodeProperty,
  CodeUnsafe; vsa has them, other presets fall back (rad_window_frame).
- `lsp_ctrl_link`: its "cursor still on line 3 after 2 frames" check was
  a race (the definition can come within 2 frames now); it checks that
  the cursor did not go to the click instead.

### 2026-11-04 (task G7: the user's theme)
- `RAD_ThemePresetTable`: `Vsa` first (default index 0; settings store the
  preset by name, so older user files keep theirs). Generated from the
  Default (Dark) block with vsa's values substituted, tag by tag.
- The lexer alone cannot tell functions and types from other identifiers:
  they get vsa's orange/gold with semantic tokens (G6).

### 2026-11-04 (task G2: completion icons)
- `icons/make_rad_icons.py` packs the 9 `symbol-*.svg` on `, / : ; = _ |
  \ `` (the last free ASCII codepoints of raddbg's icons.ttf; more
  icons need private-use codepoints). Run it with
  `uv run --no-project --with fonttools python icons/make_rad_icons.py`.
- `RAD_IconTable`: `Symbol*` kinds; `rad_icon_from_lsp_completion_kind`
  maps LSP kinds to them (same code colors as before).

### 2026-11-04 (task G1: gutter)
- The user's second round is TASKS.md section G (before milestone 10).
- Line numbers right-aligned in `line_num_width_px - fold_width_px`;
  `do_margins = 0` (E1's margins off; the slice keeps their code); the
  fold chevron tooltip waits 1 s (`rad_state->fold_hover_key` /
  `fold_hover_begin_us`, a frame requested while waiting).

### 2026-11-04 (task B2, the rest: fonts, scroll past end, close button)
- Setting kind `String` (c type String8, pointing into
  `rad_state->setting_string_buffers`, so a value survives the user
  file's node changing); `rad_setting_set_string` (empty = revert).
- rad_frame: "get fonts from settings" after reading the settings (port
  of raddbg's, with `use_alternative_font_for_ui`); `font_slot_is_custom`
  for scripts.
- Settings view: String rows edit in place (raddbg's watch-cell editing:
  `text_editing`, the row's `UI_FocusActive` on, the scroll list's Nav off
  while editing); "Built-in" weak when empty.
- Scroll Past End off: the Y range ends at total - visible + 2; a view
  past it is moved to the max first (`ui_scroll_pt_clamp_idx` would send
  it to the top).
- Script fields `settings_editing`, `font_custom <main|code>`; script
  `settings_more` (18). All 35 scripts pass.

### 2026-11-04 (tasks F1, F2: drag and drop)
- rad_core.c "Drag/Drop": port of raddbg's `rd_drag_*` (Null / Dragging /
  Dropping; a left release sets Dropping, a drop site that is drop-hot
  takes it with `rad_drag_drop`, the end of the frame clears an untaken
  one). Kinds: Tab, Panel (a tab bar's free space: all its tabs),
  FilePath (an explorer row). Esc kills a drag; `drag_killed` blocks a
  restart until the button is up (the tab is still held).
- Leaf panels: the docking compass (`rad_drop_site_rect`, raddbg's
  combined sites with all four sides) and the catch-all site; non-leaf
  panels: raddbg's boundary sites and its root extra split, limited to
  editor panels (`rad_panel_subtree_has_tool__`).
- Commands `MoveTab` / `MoveTabToNewPanel` (params `dst_panel`,
  `prev_tab`, `dir2`, `all_tabs`); OpenFile takes `dst_panel` + `dir2`
  (a file dropped on a side). `rad_drop_split` picks one.
- Script: `mouse_down` / `mouse_up`; `drag` holds one more frame at the
  end (sites built there need a rect); target `panel_site <n> <site>`;
  fields `panel_tabs <n>`, `drag_active`; script `drag_tabs` (28).
- `shared_document` flaked once under load (wait_idle after the 4 s
  message fade, 10 s timeout); passed on every rerun.

### 2026-11-04 (task E2: code folding)
- rad_views.c "Code Folding": `rad_fold_ranges_from_text` (byte scanner:
  brace pairs with inner lines -> [open, close-1], comment runs, use runs;
  sorted), cached per buffer version in the code view's `fold_arena`.
  Folds: `fold_offs` (first-line offsets, mapped through edits with
  `tb_buffer_map_offset`); `rad_code_view_fold_map` = folded ranges,
  outermost only; rows <-> lines: `rad_fold_row_from_line` /
  `rad_fold_line_from_row` (walk the sorted folds).
- `rad_code_view_build` scrolls in rows (scroll range, visible range,
  cursor centering and containment, diagnostics, completion anchor); the
  slice gets `line_nums`, `line_fold_kinds`, `fold_width_px` (a fold column
  in the line numbers' column: chevrons, tooltip) and draws a "..." box
  after a folded line; `fold_toggle_idx` comes back on clicks.
- The cursor in a fold: vertical moves skip it, anything else unfolds it.
- Script: fields `fold_count`, `line_hidden <line>`; target `fold <line>`;
  `code` targets use rows; script `folding` (26).

### 2026-11-04 (task E1: glyph margins)
- `rad_code_slice`: raddbg's priority and catchall margins
  (`RAD_CodeSliceFlag_PriorityMargin/CatchallMargin`, widths 3.5 glyph
  advances, as raddbg's RD_CodeViewBuildFlag_Margins) before the line
  numbers; the line numbers and their background move right by them; the
  cursor's scroll-into-view counts them. Text tabs only (not Output).
- Catchall margin content: `line_margin_fstrs`, filled by the code view
  with the icon of each line's worst diagnostic (error > warning > info).

### 2026-11-04 (tasks D1-D3: compact tabs, overflow, pinning)
- Tab widths from `rad_tab_width_target_px`; the X only on the selected
  and hovered tab (room kept); pin button on pinned and selected tabs;
  Pin Tab command (`pinned` child on the tab; pinned tabs first).
- Setting `tab_overflow`: Scroll (wheel over the tab bar -> sideways; a
  down-caret "All Tabs" button with a menu; picking a tab moves it to the
  front) or Wrap (rows computed before the panel's rects; one tab bar box
  per row; zoom button on the first row, "+" on the last).
- Script: fields `tab_pinned`, `tab_index`, `tab_rows`, `tabs_overflow`;
  targets `tab_box`, `tab_pin`, `tab_close`, `tab_list`, `tab_list_item`;
  script `tabs` (20). Tested with build\rad_probe.exe (rad.exe was open).

### 2026-11-04 (task C2: explorer search filter, compact rows)
- Explorer filter (`RAD_ExplorerState.filter_*`): a filter row above the
  list (column layout, as Settings). Non-empty -> rows from
  `rad_explorer_rows_push_filtered__`: the whole tree (expanded or not),
  matches + parent folders (a folder row is rolled back if nothing under it
  matches), .git and target skipped, 200000 entries at most; match
  highlights through the cell's `search_needle`.
- Keys: typing in the list or Ctrl+; (Search Explorer, View menu) -> the
  filter; Down/Enter -> the first match; Esc clears, Esc again leaves;
  Enter / double-click on a folder while filtering clears the filter and
  reveals it. Settings' filter: Down is a Navigate event; fixed there too.
- Setting `explorer_compact_rows` (2 font sizes).
- Script: fields `explorer_filter`, `explorer_filter_focused`,
  `explorer_row_height`; target `explorer_filter`; script
  `explorer_filter` (31).

### 2026-11-04 (tasks C1, C3, C2 layout: tool windows)
- Tool windows (`RAD_ToolKind`, rad_core.c "Tool Windows"): leaf panels
  holding tool tabs (explorer; output + problems). Every frame, before
  commands, `rad_window_tool_layout_ensure` checks the layout
  `panels (y): [top (x): explorer | editor...] [bottom: tabs_on_bottom]`,
  moves the explorer to its side (setting `explorer_side`), or rebuilds it
  from any other layout (old user files: tool tabs out, emptied panels
  closed, the rest becomes the editor area).
- Hidden = panel size 0, size kept under `window: tool_sizes`; no drag
  boundary next to it; Next/Prev Panel skip it. Header (title + X in the
  zoom button's style); explorer: no tab bar; bottom: tabs at its bottom.
  No "+", no zoom, tool tabs have no close button.
- Commands: Show Explorer (Ctrl+Alt+L), Show Output (Ctrl+Alt+O), Show
  Problems, Close Tool Window (Shift+Esc); View menu. Close Panel on a tool
  window hides it; splits on one split the editor panel; a split next to
  the explorer takes space only from the split panel; closing a panel next
  to it gives the space to the neighbor; the last editor panel stays.
- `ClosePanel`'s body is now `rad_panel_close` (also used by the rebuild).
- Script: `panel_count` counts editor panels only; fields
  `explorer_visible`, `bottom_visible`, `explorer_on_right`; target
  `tool_close`; scripts `tool_windows` (44), `layout_migrate` (8; fixture
  `old_layout.rad_user`); `explorer_panel` replaced.

### 2026-11-04 (tasks B1 step 2: Settings tab)
- `rad_settings_view_ui` (rad_views.c): raddbg's User Settings lister as a
  tab (`settings` tab kind, File > Settings, one per window, opens where
  files open). Filter row in the query bar's look; rows of `rad_cell`s 5
  font sizes tall (raddbg's `do_big_rows`; Compact: 4).
- `rad_cell` gained raddbg's name/description column, toggle switch,
  slider and revert button (ported from rd_cell), plus rect outputs for
  scripts. Fix: the "focus cursor" step wrote into `ui_nil_box` (read-only)
  for cells without a line edit; it now skips them.
- Code zoom keys moved to Ctrl+Shift+= / Ctrl+Shift+- / Ctrl+0: step 1's
  Ctrl+Shift+Comma had taken raddbg's Previous Panel binding.
- Script: fields `settings_rows`, `settings_selected`, `settings_filter`,
  `settings_filter_focused`; targets `setting_row`, `setting_control`,
  `setting_revert`, `settings_filter`, `setting_option`; script
  `settings_view` (33).

### 2026-11-04 (tasks B1 step 1, B3: settings table, code font zoom)
- `RAD_SettingTable` (rad.mdesk) generates `RAD_Settings`,
  `RAD_SettingCode`, `rad_setting_info_table` (kind, offset, default,
  range, group, name, description, enum options). The hand-written struct
  and defaults are gone. Values: the user file's `settings:` node, read
  every frame (`rad_settings_from_cfg`), written by `rad_setting_set_f64`
  (clamped; enums by index, themes by preset).
- New: `density` (Comfortable/Compact -> row/tab heights 3/3 or 2.25/2.5),
  `code_font_size`, `mouse_wheel_zoom`. Fonts: a window keeps a DPI scale;
  UI size = font_size x scale, code size (text and Output views) =
  code_font_size x scale. Alt+= / Alt+- change font_size; Zoom In/Out
  (Code) and Ctrl+wheel (if Mouse Wheel Zoom) change code_font_size;
  Reset Zoom (Code). (Keys: see the B1 step 2 entry.)
- Script: field `setting <name>`, `editor_font_size`, `ui_font_size`;
  command `set_setting`; script `settings` (21).

### 2026-11-04 (tasks A10-A11: restart rust-analyzer, file_watch flake)
- A10: `rad_lsp_server_restart` (in place: the old process goes, its
  documents are opened again, diagnostics/progress dropped, requests to it
  forgotten); command Restart Language Server (Build menu, also retries a
  missing rust-analyzer); a process exit restarts it up to 3 times per
  minute, then the bottom bar says to use the command. `servers_started`
  now counts launches (log names); field `lsp_servers` counts servers,
  `lsp_restarts` restarts. Script command `lsp_kill`; script `lsp_restart`
  (20).
- A11: `sleep_until_woken` counted any wake of the event wait, and Windows
  wakes it for other messages too. Now: the watcher thread counts its
  wakeups (`watch_wakeups_sent`); the sleep ends on one of those, once no
  `write_file_later` is pending. `file_watch` 20 of 20 next to a parallel
  full run (27 of 27; runners need separate TEMP folders, the runner wipes
  `%TEMP%\rad_script_tests`).

### 2026-11-04 (tasks A8-A9: completion docs and icons, one-step import undo)
- `text_buffer`: undo steps. Records made between `tb_buffer_begin_step`
  and `tb_buffer_end_step` share a step; undo/redo take a whole step (redo's
  cursor follows the later records). `tb_buffer_join_step` reopens the last
  step (nothing done since); `tb_buffer_step_is_applied`. rad_tests
  `undo_steps` (37,610,802 checks pass).
- Completion commit and `rad_document_apply_edits` use steps. The selected
  item's completionItem/resolve is sent while it is selected; its answer
  stays on the item (`more_edits_version` = the text it is in), so the
  commit applies the `use` line in the same step. A commit before the answer
  marks the request `for_commit`: the answer joins the commit's step, is
  its own step if the user edited since, or is dropped if the commit was
  undone.
- Documentation (rust-analyzer sends it inline) in a box beside the list:
  tooltip-like padding, `rad_markdown_ui`, 36 em wide, right of the list or
  left if it does not fit. Kind icons from raddbg's font in the kind's code
  color (`rad_icon_from_lsp_completion_kind`). Screenshot checked.
- Fields completion_doc/doc_shown/kind/resolved. `lsp_completion` (59):
  one Ctrl+Z for HashMap, a HashSet import resolved before Enter arrives
  with the commit; `lsp_completion_scroll` (24): checked_add's docs. 26
  scripts pass.

### 2026-11-04 (tasks A7: functions complete with their parentheses)
- initialize: `snippetSupport: true` and `initializationOptions.completion.
  callable.snippets = "add_parentheses"` (rad answers
  workspace/configuration with null, so rust-analyzer reads these).
- `rad_lsp_text_from_snippet`: snippet -> text (tab stops dropped,
  placeholders/choices/variables keep their text, escapes); the cursor goes
  to the lowest tab stop > 0, else $0, else the end.
- Commit puts the cursor there; if it is right before a closing bracket the
  item inserted (`)`, `>`, `]`, `}`), typing that bracket steps over it
  (until the cursor leaves the brackets).
- With snippets, rust-analyzer also sends postfix items (`.if`, `.match`,
  ...): u32's list is ~280 rows. `lsp_completion` updated (labels `total()`,
  `new()`, `HashMap<…>`), new checks for `scale(…)` and the step-over; the
  fixture got `fn scale` after `main`. 26 scripts pass.

### 2026-11-04 (tasks A4-A6: Ctrl+click links, last line, editor fills panel)
- A4: with Ctrl held, the identifier/keyword under the mouse gets a box
  (selection color + focus border, as in the user's image) and the hand
  cursor (`RAD_CodeSliceFlag_CtrlLinks`, editable views). A Ctrl+press on
  it does not move the cursor or select; it sends Go To Definition for that
  identifier, and the jump's origin (Navigate Backward) is the identifier.
- A5: the code slice clamped the mouse's line index one line short (also
  in raddbg, `raddbg_widgets.c` "mouse y => index"), so the file's last
  line could not be clicked or dragged from while the end was on screen.
  The same off-by-one in the hover lookup and the selection corners.
- A6: the slice is at least as tall as the view (`min_height_px`): line
  numbers and the clickable text area reach the panel bottom; a click or
  drag below the last line lands on the last line.
- Script mode: keys held with `key_down` stay held for later inputs and
  frames (the tick reset modifiers to 0). Field `ctrl_link`, target
  `code_below <col>`. Scripts `lsp_ctrl_link` (16), `editor_mouse` (16).
  26 scripts pass. Screenshot checked.

### 2026-11-04 (tasks A3: panel focus, Alt; fixed script window size)
- Clicking into a panel's code now focuses that panel (raddbg's code view
  does `FocusPanel` on press; the port had dropped it).
- Alt focuses the menu bar only when nothing is pressed or scrolled while
  it is held (as in Windows; raddbg cancels only for bound keys and F keys).
- Script mode: window 1920x1080 px when the user file gives no size (was
  4/5 of the primary monitor). The user's display changed (5120 -> 3025 px
  wide) and the cargo scripts' Output lines fell out of view. cargo_build
  and cargo_run now bring an Output line into view with Find before a
  double-click; scroll's PageDown check is size-independent.
- New script commands `key_down`/`key_up`, field `menu_bar_focused`, target
  `tab_code "<name>"`; script `panel_focus` (14). 24 scripts pass.

### 2026-11-04 (tasks A2: completion list scrolling)
- Bug (user): the completion list stopped scrolling after a few rows with
  the wheel or Up/Down (dragging the bar worked). Cause: its scroll point
  (`rad_lsp->completion.scroll_pt`) was not eased by
  `rad_view_state_animate` (raddbg's lister is a view state; ours is not).
- Script `lsp_completion_scroll` (20; fails 5 checks without the fix):
  fields completion_top/bottom/below/selected_visible, target
  `completion_list`. 23 scripts pass.
- User decisions: tool windows are not draggable; the explorer is on the
  left, a setting moves it right; the bottom window's tabs are at its
  bottom (VS2022).

### 2026-11-04 (task list before milestone 10; build icon)
- New `docs/TASKS.md`: the user's list (explorer and output as tool
  windows, View menu, settings, code-only zoom, compact tabs with pins and
  overflow, glyph/fold margins, Ctrl+hover definition, focus and cursor
  bugs, completion fixes, drag and drop) merged with suggestions, in order.
- `.dev_hints/` git-ignored (the user's private hint images).
- A1 done: the user's new `icons/build.svg` in `data/rad_icons.ttf`
  (`uv run --no-project --with fonttools python icons/make_rad_icons.py`;
  fontTools is not installed globally). Screenshot checked.

### 2026-11-04 (milestone 9 done)
- Release build in `build\release\rad.exe`; 22 scripts (453 checks) and
  rad_tests (37,610,518 checks) pass on debug and release.

### 2026-11-04 (milestone 9, step 5: completion list)
- As you type (Visual Studio's IntelliSense): a word's first character,
  `.`, `::`, or Complete Word (Ctrl+Space, new command) sends
  textDocument/completion; the list is filtered by the typed word (prefix,
  substring, subsequence; then the server's order) and asked for again when
  the server marks it incomplete or nothing matches.
- Popup: port of raddbg's autocompletion floating view (container flags,
  30 em, at most 30 em of rows, open animation, built before the panels so
  it is on top; its scroll list takes Up/Down/PageUp/PageDown through
  raddbg's secondary-keyboard permission). Rows like the Problems rows,
  label in code colors, detail weak, the typed word highlighted
  (`RAD_CellParams.search_needle`, ported from `rd_cell`).
- Enter/Tab/click commit (the item's edit, extended over what was typed
  since; additional edits; one undo step); Esc closes; moving the cursor
  or typing a non-word character closes it. Enter's newline and the space
  Windows sends with Ctrl+Space are not typed.
- Imports: rust-analyzer offers items from crates not imported yet only to
  clients that resolve them: rad advertises `resolveSupport`
  (additionalTextEdits) and sends completionItem/resolve on commit; the
  `use` line is applied when it comes.
- `text_buffer`: `tb_buffer_map_offset_side` (Side_Min: an insertion at an
  offset leaves it in place), for the start of the typed word and of edit
  ranges; rad_tests check it in the random-edit test.
- New fixture `lsp_complete`, script `lsp_completion` (45); fields
  completion_open/count/selected/row/has/cursor/filter, target
  `completion <n>`.

### 2026-11-04 (milestone 9, step 4: go to definition)
- Go To Definition (F12, Ctrl+click, as in VS2022) sends
  textDocument/definition (pending edits first); the answer (Location,
  Location[] or LocationLink[]: the first one) opens the file through
  GoToLocation, at the UTF-8 column (`column_in_bytes`). Navigate Backward
  (Alt+Left) returns to where the jump started (64-entry stack). "No
  definition found." in the bottom bar otherwise.
- Library sources (`.rustup/toolchains`, `.cargo/registry`, `.cargo/git`)
  never start a server: they attach to the server that answered (or the
  first one), so std files get hover and further jumps.
- Script `lsp_definition` (14); fields lsp_servers, cursor_line_text;
  command ctrl_click. 21 scripts pass (`file_watch` failed once on its
  sleep timing under load, then passed 3 of 3: flaky, not this change).

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
