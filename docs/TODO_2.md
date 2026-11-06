# TODO 2: back to the IDE (agreed 2026-11-06)

The user's list from 2026-11-06 (after the Metal renderer), plus Claude's
suggestions (marked **[suggestion]**). Same rules as `docs/TASKS.md`:

- Hint images are in `.dev_hints/` (private, never pushed). Names below
  like `0I5T7Lv4` mean `.dev_hints/0I5T7Lv4.jpg`.
- Every task ends with a passing script in `tests/scripts/` (or a
  `rad_tests` check), then commit + push. Tick the box here and add a
  journal entry in `docs/PROGRESS.md` in the same commit.
- "Cause" lines say whether they were verified in the code or are a guess.
- Work happens on the Mac. Windows code that is touched (key tables, shell
  functions) is written for Windows too but cannot be built or run from
  here: marked "Windows: untested" until it is.

Order: keys first (they are used all day), then the look (tabs, status
bar, bottom panel), then diagnostics and quick fixes, then the explorer,
then inlay hints (the biggest change to the code view).

## A. Keys

- [x] **A1. Zoom keys.** Cmd+= / Cmd+- (Ctrl on Windows) zoom the editor
  only; Cmd+Shift+= / Cmd+Shift+- do the same; Cmd+0 resets. No key zooms
  the whole UI any more (Zed has none); the UI font size stays a setting.
  Cause of "Cmd+- opens a new panel below" (verified, `rad.mdesk`):
  Ctrl+- is raddbg's default for Split Panel Down (`NewPanelDown`), and
  the Mac maps Cmd to Ctrl (`mac_window_manager.c`). Code zoom was only
  on Ctrl+Shift+= / -; the whole-UI zoom on Alt+= / -.
  Fix: Ctrl+= / Ctrl+- -> code zoom; Split Panel Down loses its key (menu
  and command list keep it); Alt+= / Alt+- unbound.
  Done 2026-11-06 (scripts `settings`, `text_keys_mac`): second keys and
  the Mac's own keys in `rad_core.c` (`rad_cmd_extra_binding_table`,
  `rad_cmd_mac_binding_table`, `rad_binding_from_cmd_kind`: menus show the
  Mac's key). Also Cmd+Shift+[ / ] = previous / next tab on the Mac.
- [x] **A2. Smooth Cmd+wheel zoom** of the editor only (the setting Mouse
  Wheel Zoom exists; today it steps a whole point per wheel notch). The
  wheel moves a fractional target size; the shown size eases toward it
  every frame (raddbg's animation rate), text drawn at the in-between
  sizes, the line under the mouse kept in place. Must stay at the frame
  rate while zooming (glyphs rasterized per size: measure; quantize the
  in-between sizes if the rasterizing shows in the frame time).
  Done 2026-11-06 (script `zoom_wheel`, new script command `wheel_units`
  for trackpad-sized steps): the drawn size eases at the scrolling
  animation rate, in-between sizes on quarter pixels; 150 ms after the
  last wheel step the setting takes the nearest whole size. Frame time
  while zooming (Mac, M-series): 1.4 ms avg, 2.4 ms max the first time (new
  glyph sizes), 1.0 ms after. Keys (Cmd+= / -) ease the same way. The top
  line stays in place (the line under the mouse moves; later if wanted).
- [x] **A3. macOS text navigation** in the editor and every text field.
  Cause (verified, `rad_core.c` `rad_text_nav_binding_table`): one table
  with Windows meanings; the Mac sends Cmd as Ctrl, so Cmd+Left is "word
  left" and Option+Left is Alt+Left = Go Back.
  On the Mac (Zed / macOS keys):
  - Cmd+Left / Cmd+Right: line start (first non-blank, then column 1) / end
  - Cmd+Up / Cmd+Down: file start / end
  - Option+Left / Option+Right: word left / right
  - Shift + any of these: select
  - Option+Backspace / Option+Delete: delete word left / right
  - Cmd+Backspace / Cmd+Delete: delete to line start / end
  - Home / End (Fn+Left / Fn+Right): line start / end, as now
  Go Back / Go Forward move off Option+Left/Right on the Mac: Ctrl+- /
  Ctrl+Shift+- (Zed, VS Code) need the Mac's Control key told apart from
  Cmd; until then Cmd+[ / Cmd+] (Xcode, Safari). Windows keeps its keys.
  Speed: each key is one event and one frame (no extra work per key).
  Done 2026-11-06 (script `text_keys_mac`, new script command `only_on`):
  Mac rows in `rad_text_nav_binding_table`; Cmd+Backspace inside the
  indentation deletes back to column 1 (never forward: `ui_core.c`).
  Navigate Backward: Cmd+[ on the Mac. Not done: Option+Up/Down move by
  paragraph for now (E1 may make them Move Line Up/Down).

## B. Look

- [x] **B1. Tab bar aligned with the editor** (`0bDuW7FR`: the first tab
  starts right of the panel's left edge). Cause (verified,
  `rad_core.c` "build tab bar contents"): raddbg's `UI_Padding(ui_em(0.5))`
  before the first tab. First tab flush with the panel edge.
  Done 2026-11-06 (checked by screenshot; new script command
  `float_window` keeps a script's window above others, so it is drawn).
- [x] **B2. Compact tabs** (compare with Zed, `KHY9RCOD`): no file icon;
  file name, then pin and close only while the tab is hovered (a pinned
  tab keeps its pin; the selected tab keeps close); less room between
  name and buttons; tab height a little lower. Modified files: the name
  in the theme's modified color (yellow) plus a small dot in the close
  button's place when the tab is not hovered (Zed's dot, VS Code's).
  Done 2026-11-06 (scripts `tabs`, `settings_more`; screenshot): file tabs
  without icon; pin and close in two small slots (1.5 em each, room kept
  so hovering moves nothing), shown while hovered; pinned tabs keep the
  pin; modified: a dot in the close slot (the name keeps its color: yellow
  would read as a warning). Tab bar 2.6x the font (was 3; Compact 2.25).
- [x] **B3. Status bar like Zed's** (`whoAdtpq` -> `a7QhLKp3`): about 2/3
  of the height (today 3x the UI font, the same as the top bar), smaller
  text. Content **[suggestion]**: left: build status; right: language
  server status, Ln:Col of the cursor, the language. The version string
  moves to Help > About.
  Done 2026-11-06 (screenshot): 2x the UI font high, text at 0.85x; right
  side: language server status, `line:column` of the focused editor, its
  language. The version stays in the Help menu (it was there already).
- [x] **B4. Bottom panel (Output, Problems) font settings**: its own font
  name and size ("Panel Font", "Panel Font Size"; empty name = the code
  font, default size one point under the code font), and a smaller default
  height (0.25 -> 0.2 of the window). Note: there is no terminal yet; the
  bottom panel holds Output and Problems (terminal: see suggestions).
  Done 2026-11-06 (script `panel_font`): settings Panel Font (Output's
  text; empty = the code font) and Panel Font Size (Output and Problems,
  default 10); the editor zoom no longer changes the Output. Default
  height 0.2 for new layouts (a saved layout keeps its size).

## C. Diagnostics, quick fixes, inlay hints

- [x] **C1. Inline diagnostics like Zed** (errors `0I5T7Lv4`, warnings
  `JjiH1YEf`): after the line's code, a gap, then the message's first
  line in a rounded box tinted with the severity color (red for errors,
  yellow for warnings), text in that color, no icon. One per line, the
  worst. The wavy underline stays. No full-line red background.
  Today (verified, `rad_views.c` "line color + annotation"): icon + message
  in weak text, all diagnostics of the line chained, error lines tinted.
  Setting "Inline Diagnostics" (on).
  Done 2026-11-06 (script `lsp_diagnostics`, query `inline_diag_count`;
  screenshot): four columns after the code, box tinted 10% with the
  color, the message's first line; colors: the theme's new "warn text"
  (VSA: Zed's #d7ba7d; other presets: their thread_0 yellow) and "bad
  text". Underlines are wavy now. Setting off: raddbg's annotation (icon +
  all messages, right of column 30). The margin icon stays.
- [x] **C2. Quick fixes: setting + 1 s delay.** The screwdriver (code
  actions, G4 of TASKS.md) shows 0.3 s after the cursor rests
  (`RAD_LSP_CODE_ACTION_DELAY_US`). Now: after 1 s, and only with the new
  setting "Show Quick Fixes" on (default on). Ctrl+. still opens the menu
  at once.
  Done 2026-11-06 (script `lsp_code_actions`): not shown 0.6 s after the
  cursor stops, shown after 1 s; setting off: not asked for nor shown,
  Ctrl+. still asks and shows the button with its menu.
- [ ] **C3. Inlay hints** (`PL9eiIVM`: `_k: i32` with `: i32` in weak
  text): LSP `textDocument/inlayHint` for the visible lines, asked after
  each sync like semantic tokens, carried over edits. Drawn inside the
  line as text that is not in the buffer: the cursor, selection,
  mouse-to-offset and underlines skip it. Setting "Inlay Hints" (on).

## D. Explorer

- [x] **D1. Folder icon**: the user's `icons/Folder_16x.svg` for every
  folder, open or closed (two-tone: a 50% fill under a solid outline, both
  #ffdf96: two glyphs drawn on top of each other).
  Done 2026-11-06 (screenshot): `icons/make_rad_icons.py` takes one path
  of an SVG by id: glyphs Folder (outline) and FolderFill; the fill is
  drawn under the row's icon by a custom draw (`RAD_CellParams`
  value_custom_draw). Color: the theme's new "folder text" (VSA only,
  #ffdf96); other themes: the weak text color, as before.
- [ ] **D2. File operations** in a right-click menu (raddbg's context
  menu look) and keys: New File, New Folder (a name field in the tree, as
  Zed and VS Code), Rename (F2, in place), Delete (Cmd+Backspace; to the
  Trash, asks first), Cut / Copy / Paste (Cmd+X / C / V, also between
  folders; Paste of a name that exists gets " copy"), Copy Path, Copy
  Relative Path, Reveal in Finder (Explorer on Windows), Open, Open to the
  Side (new panel right). Open tabs follow renames and moves; a deleted
  open file's tab shows it is gone.
  Needs in `base`/`shell`: move to Trash (`sh_move_to_trash`: NSFileManager
  on the Mac, IFileOperation on Windows), recursive folder copy.
- [ ] **D3. Open in New Window**: rad has one window today. Later, after
  D2 (a second window with its own layout and session).

## E. Suggestions (Claude)

- [ ] **E1. [suggestion] Line editing keys**: toggle line comment (Cmd+/),
  move line up/down (Option+Up/Down), duplicate line (Shift+Option+Down),
  delete line (Cmd+Shift+K), new line below/above (Cmd+Enter /
  Cmd+Shift+Enter), indent/outdent (Cmd+] / Cmd+[ ... conflicts with A3's
  Go Back fallback: decide together).
- [ ] **E2. [suggestion] Auto-closing pairs** ( `()`, `[]`, `{}`, `""` )
  and typing over the closer; matching bracket highlight.
- [ ] **E3. [suggestion] Quick Open (Cmd+P)**: fuzzy file finder over the
  folder (Zed's, VS Code's). Cmd+P is Split Panel Right today (raddbg).
- [ ] **E4. [suggestion] Close tab Cmd+W, reopen closed tab Cmd+Shift+T.**
- [ ] **E5. [suggestion] Integrated terminal** in the bottom panel: a
  shell on a pseudo-terminal (`base` API: pty spawn) and a VT100/xterm
  parser drawing into a grid. Large; after the rest.
- [ ] **E6. [suggestion] Reveal the open file in the explorer** when its
  tab is selected (setting, VS Code's "auto reveal").
