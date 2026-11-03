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
| 4. File explorer                     | Done (keyboard, mouse, Open Folder checked). "+" in its panel = Open Folder. |
| 5 to 11                              | Not started. |

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

## Next steps

1. User check: wheel/scroll-bar scrolling in a long file and a long
   explorer list (fixed 2026-11-03, verified by log, not by eye); the
   explorer "+" button; use a release build (`build rad release`).
2. Explorer as a "special panel" beyond "+": user to decide (tab not
   closable? fixed-width sidebar? other tabs kept out of it?).
3. Editor speed: user to say what feels slow (typing delay, scrolling,
   opening files, startup). Measured so far: see the 2026-11-03 (scroll fix)
   journal entry.
4. Milestone 5, Rust syntax highlighting: a Rust lexer in `text`, in the
   style of the existing C lexer (`txt_lang_kind_from_extension` -> `.rs`).
5. For milestone 9 (Mac, at the end), already known: `base_core.h:276-277`
   and `base_core.c:667` need ARM64 guards; drop `-mcx16` on ARM64.

## Known limits

- Scope lines and end-of-scope annotations are off while patches exist,
  i.e. while typing; they come back ~100 ms after typing stops (compaction).
- Compaction re-lexes the whole file. Fine for normal files; on very large
  files typed into very fast, patches pile up while a lex is in flight.
- One buffer per tab, not per file. Fine while a file can only be open in
  one tab; LSP will want one buffer per file.
- Buffers only tick while their tab is visible (background tabs don't
  notice disk changes until shown).
- Undo history grows without limit while a file is open.
- Open File uses the native dialog (`sh_pick_file`), not raddbg's lister.
  Open Folder likewise (`sh_pick_folder`).
- Closing the explorer panel and then using Open Folder puts a new explorer
  tab in the focused panel; it does not rebuild the 20/80 layout.
- The explorer re-lists expanded folders from disk once a second, on the
  UI thread (no file watching); very large expanded trees are capped at
  100,000 rows. Fine for inkdb (whole `target/` = 2,171 entries); huge
  expanded trees would stutter once a second. Fix later: file watching.
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
  Taking the foreground interrupts the user, and their own keystrokes then
  land in rad (happened once). Prefer `rad_tests`; keep app runs short.

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
  `rd_code_view_build`) with editing; the text view feeds the file stream's
  content to the tab's `TB_Buffer` and ticks it each frame. The explorer
  view (tab `explorer`, child `folder`) at the end of the file.
- `rad_widgets.h/.c` also hold `rad_cell` (port of raddbg's `rd_cell`,
  read-only), the row widget of the explorer.
- `tests/rad_tests.c`: console tests on base/content/text/text_buffer.

`src/text_buffer/` (`TB_`): the editable buffer, see the notes at the top
of `text_buffer.h`.

## Journal

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
