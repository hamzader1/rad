# Progress log

Read this first when starting a session. Update it in the same commit as the
work: change "Status" and "Next steps", and add a dated journal entry (newest
on top). CLAUDE.md holds the goals and decisions; this file holds where we are.

## Status (2026-11-03)

Windows only for now; macOS is milestones 9-10, at the very end (decided 2026-11-03).

| Milestone (CLAUDE.md section 6)      | State |
|--------------------------------------|-------|
| 1. Build and read                    | Done. `build rad` works; notes in `docs/notes/`; option B chosen. |
| 2. Editor view without a debugger    | Done. |
| 3. Editable text core                | Mostly done, see below. |
| 4 to 11                              | Not started. |

Milestone 3 details:
- Checked in the running app: typing, Enter (keeps indent), Backspace,
  Ctrl+S save (file on disk updated, no temp file left, dirty dot cleared).
- Checked by `rad_tests` (46,316,762 checks, 0 failures): patched text, line
  map and offset-to-line after random edits, LF and CRLF.
- NOT checked yet: Ctrl+Z / Ctrl+Y in the app, Delete, Ctrl+X / Ctrl+V,
  "close/exit again to discard" guards, panel split/close, large files.
  `rad_tests` does not cover undo/redo or the save rebase.

## Next steps

1. Move the edit buffer (patch list, undo history, save rebase) out of
   `rad_views.c` into UI-independent code, and test undo/redo/rebase in
   `rad_tests`.
2. Check the untested items above in the app.
3. Milestone 4: file explorer panel.
4. For milestone 9 (Mac, at the end), already known: `base_core.h:276-277`
   and `base_core.c:667` need ARM64 guards; drop `-mcx16` on ARM64.

## Known limits

- Every edit appends to a patch list that is re-applied each frame until the
  next save, so editing slows down the longer you go without saving. Needs
  compaction or a real buffer (rope / piece table).
- Scope lines and end-of-scope annotations are off while a file has unsaved
  edits (the lexer's token data is for the saved text).
- Open File uses the native dialog (`sh_pick_file`), not raddbg's lister.
- Settings are hardcoded raddbg defaults (`rad_default_settings`). Window
  position and panel layout are not saved between runs.
- Tab inserts 4 spaces. No smart indent after `{`.

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
- Building from Claude Code's shell: it sets `NoDefaultCurrentDirectoryInExePath=1`,
  which breaks build.bat's bare `metagen.exe` call. Unset it, put
  `C:\Program Files (x86)\Microsoft Visual Studio\Installer` on PATH, call
  `vcvars64.bat`, then `build.bat`.
- Driving the UI from a script: screenshot with `PrintWindow(hwnd, hdc, 2)`.
  Plain keys work through `PostMessage`; Ctrl/Shift do not (the win32 layer
  reads them with `GetKeyState`), so chords need `keybd_event` while rad is
  the foreground window. rad ignores keys when its window is not focused.

## Code map (`src/rad/`)

- `rad_main.c`: entry point and unity build (which layers get linked).
- `rad.mdesk`: icon, theme and code-color tables copied from `raddbg.mdesk`
  (RD_ -> RAD_), plus the command table (names, icons, default keys).
- `rad_core.h/.c`: state, settings, commands (`rad_exec_cmd`), fonts, the
  window frame (port of `rd_window_frame`: top bar, menus, bottom bar,
  panels, tabs), the box renderer (verbatim), the per-frame loop.
- `rad_widgets.h/.c`: menu bar button, icon buttons, command buttons, the
  code slice (port of `rd_code_slice`).
- `rad_views.h/.c`: view states, the code view (port of
  `rd_code_view_build`) with editing, undo/redo, save, rebase after save.
- `tests/rad_tests.c`: console tests on base/content/text.

## Journal

### 2026-11-03
- Decided: Windows only until the IDE is done; macOS at the very end.
  Milestones renumbered in CLAUDE.md (Mac is now 9 and 10).

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
