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
- [x] **C3. Inlay hints** (`PL9eiIVM`: `_k: i32` with `: i32` in weak
  text): LSP `textDocument/inlayHint` for the visible lines, asked after
  each sync like semantic tokens, carried over edits. Drawn inside the
  line as text that is not in the buffer: the cursor, selection,
  mouse-to-offset and underlines skip it. Setting "Inlay Hints" (on).
  Done 2026-11-06 (script `lsp_diagnostics`, query `inlay_hint_count`;
  screenshot): asked for the whole text (the range must end at the text's
  end: one line past it is an error for rust-analyzer), weak text color,
  no background. Every x position in the code slice goes through
  `rad_code_slice_x_from_col` / `rad_code_slice_col_from_x`; popups and
  scrolling add `rad_inlay_hints_width_before`. A hint where text is
  typed hides until the next answer. Type and parameter hints both show
  (rust-analyzer's defaults); no per-kind setting yet.

## D. Explorer

- [x] **D1. Folder icon**: the user's `icons/Folder_16x.svg` for every
  folder, open or closed (two-tone: a 50% fill under a solid outline, both
  #ffdf96: two glyphs drawn on top of each other).
  Done 2026-11-06 (screenshot): `icons/make_rad_icons.py` takes one path
  of an SVG by id: glyphs Folder (outline) and FolderFill; the fill is
  drawn under the row's icon by a custom draw (`RAD_CellParams`
  value_custom_draw). Color: the theme's new "folder text" (VSA only,
  #ffdf96); other themes: the weak text color, as before.
- [x] **D2. File operations** in a right-click menu (raddbg's context
  menu look) and keys: New File, New Folder (a name field in the tree, as
  Zed and VS Code), Rename (F2, in place), Delete (Cmd+Backspace; to the
  Trash, asks first), Cut / Copy / Paste (Cmd+X / C / V, also between
  folders; Paste of a name that exists gets " copy"), Copy Path, Copy
  Relative Path, Reveal in Finder (Explorer on Windows), Open, Open to the
  Side (new panel right). Open tabs follow renames and moves; a deleted
  open file's tab shows it is gone.
  Needs in `base`/`shell`: move to Trash (`sh_move_to_trash`: NSFileManager
  on the Mac, IFileOperation on Windows), recursive folder copy.
  Done 2026-11-06 (script `explorer_ops`; screenshots of the menu, the
  name field and the popup): commands `explorer_*` in RAD_CmdTable (no
  global keys: `rad_explorer_binding_table` holds the explorer's own,
  which menus show); right-click menus for a file, a folder, and the empty
  area (the root); New File / New Folder type the name in a row first in
  the folder ("a/b.rs" makes `a`); Rename selects the name without its
  extension; Enter / Esc / a click on another row; Delete asks with a port
  of raddbg's confirmation popup (`rad_popup_open`), then
  `sh_move_to_trash` (Mac: NSFileManager; Windows: SHFileOperationW with
  FOF_ALLOWUNDO, untested; Linux: `gio trash`, untested); Paste of a
  folder into itself goes next to it. Renames and moves carry open
  documents (text, undo; a new id, so the language server reopens them),
  tabs, open folders and the explorer clipboard (`rad_paths_moved`).
  Not done: several rows selected at once; drag and drop inside the tree;
  key labels say "Ctrl" / "Alt" on the Mac in every menu (Cmd / Option).
- [ ] **D3. Open in New Window** (later, user 2026-11-06: "do not make
  new window for now"): rad has one window today.

## E. Suggestions (Claude)

- [ ] **E1. [suggestion] Line editing keys**: toggle line comment (Cmd+/),
  move line up/down (Option+Up/Down), duplicate line (Shift+Option+Down),
  delete line (Cmd+Shift+K), new line below/above (Cmd+Enter /
  Cmd+Shift+Enter), indent/outdent (Cmd+] / Cmd+[ ... conflicts with A3's
  Go Back fallback: decide together).
- [ ] **E2. [suggestion] Auto-closing pairs** ( `()`, `[]`, `{}`, `""` )
  and typing over the closer; matching bracket highlight. User: yes, with
  an on/off setting.
- [ ] **E3. [suggestion] Quick Open (Cmd+P)**: fuzzy file finder over the
  folder (Zed's, VS Code's). Cmd+P is Split Panel Right today (raddbg).
- [x] **E4. [suggestion] Close tab Cmd+W, reopen closed tab Cmd+Shift+T.**
  Done 2026-11-06 (script `reopen_tab`): Cmd+W was already Close Tab;
  Reopen Closed Tab (Cmd/Ctrl+Shift+T) reopens closed file tabs newest
  first, in their panel if it is still there (up to 64 remembered, files
  that are gone skipped). Cargo Test moves from Ctrl+Shift+T to F6 (next
  to Run F5 and Check F7).
- [ ] **E5. [suggestion] Integrated terminal** in the bottom panel: a
  shell on a pseudo-terminal (`base` API: pty spawn) and a VT100/xterm
  parser drawing into a grid. Large; after the rest.
- [ ] **E6. [suggestion] Reveal the open file in the explorer** when its
  tab is selected (setting, VS Code's "auto reveal").

## F. Second list (user, 2026-11-06)

User: "first Solve the issue of Mouse wheel ... with my mouse it does not
... second make explorer select several items at once, and cmd/options
instead of ctrl/alt also ... add your suggestions from E1 to E6 ... E2
... on/off from settings, and please settings is growing, so group by
business, like UI business, editor.. etc.. and compact mode is not great
yet since it has like when we have two lines they get crossed."
Order: F1 first, then the rest; E1-E6 above are approved (E2 with a
setting).

- [x] **F1. Mouse wheel with an external mouse (Mac).** The user's mouse
  (Cooler Master MM710, LinearMouse installed) does not scroll; the
  trackpad does. Not reproducible with synthetic wheel events (line
  units, fractional lines: they scroll). Found: precise (pixel) wheel
  events outside a trackpad gesture (no phase: high-resolution wheels,
  mouse utilities) were accumulated like trackpad points, so a notch of a
  few points did nothing. Now each such event moves at least one step;
  line events whose fractional fields are 0 fall back to the whole-line
  fields; pinch (and LinearMouse's Cmd+wheel "pinch zoom") zooms the
  code. `RAID_SCROLL_LOG=1` prints each wheel event's fields.
  Done 2026-11-06 (checked with synthetic events posted at the window:
  3 phase-less 3-point events scroll 3 lines, were 0; line events as
  before). Not script-testable (scripts inject below the AppKit
  handler). Unverified with the user's mouse: if it still fails, run
  `RAID_SCROLL_LOG=1 build/raid` and scroll once.
- [x] **F2. Explorer: several items selected at once** (Cmd+click toggles,
  Shift+click a range, Shift+Up/Down); Cut / Copy / Paste / Delete act
  on all of them.
  Done 2026-11-06 (script `explorer_ops`, query `explorer_selection`;
  screenshot): kept as paths (survives row rebuilds); Cut / Copy / Delete
  / Copy Path take every selected row (a row inside a selected folder is
  left out); Delete asks once ("these 3 items"); pasted items end up
  selected; a right click inside the selection keeps it. Rename, New and
  Open act on the cursor's row. Also: the explorer's search hint shows
  the real key (it said "Ctrl+;" on the Mac). Not done: dragging several
  rows.
- [x] **F3. Mac key labels**: menus and the command palette show Cmd /
  Option (and Zed-like symbols?) instead of Ctrl / Alt on the Mac.
  Done 2026-11-06 (script `text_keys_mac`, query `key_label`): words, not
  symbols (the UI font has no ⌘ glyph): "Cmd + Option + C"; also the
  "save first" messages and the quick-fix tooltip.
- [x] **F4. Settings grouped**: rows under group headers (Appearance /
  UI, Editor, Explorer, Language Server, Build, ...), in a fixed order.
  Done 2026-11-06 (script `settings_view`): Appearance, Fonts, Editor,
  Language Server, Explorer, Tabs, Animations, Window; a header row (name
  over a line) per group with matches; Up/Down skip headers.
- [x] **F5. Compact density: two-line rows crossed.** Settings rows
  (name + description) take raddbg's cell layout, sized for 5-font-size
  rows; in Compact (4) the two lines were squeezed into each other and
  the controls were off center.
  Done 2026-11-06 (screenshots at the user's sizes, UI font 25): in
  Compact each line gets one text line's height, centered; Comfortable
  is unchanged (raddbg's).

