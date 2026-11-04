# Tasks before milestone 10 (agreed 2026-11-04)

The user's list from 2026-11-04, plus Claude's suggestions (marked
**[suggestion]**). This is the whole focus until it is done. Milestone 10
(remove the debugger, rebrand) waits.

Rules for this list:
- Hint images are in `.dev_hints/` (private, git-ignored, never pushed). Names
  below like `oxg7eMtQ` mean `.dev_hints/oxg7eMtQ.jpg`.
- Look: raddbg's theme, fonts, icons, tab/row/button widgets and sizes.
  Arrangement and behavior beyond raddbg: Visual Studio 2022 (memory
  "IDE model is VS2022").
- Every task ends with a passing script in `tests/scripts/` (or a
  `rad_tests` check), then commit + push. Update the checkbox here and the
  journal in `docs/PROGRESS.md` in the same commit.
- "Cause" lines say whether they were verified in the code or are a guess.

Order: bugs in what exists first (A), then settings (B, other tasks need
them), then layout (C), tabs (D), the gutter (E), drag and drop (F).

## A. Bugs and fixes in existing features

- [x] **A1. Build button icon.** The user replaced `icons/build.svg`.
  Regenerate `data/rad_icons.ttf` (`python icons/make_rad_icons.py`),
  rebuild, check by screenshot.
- [x] **A2. Completion list does not scroll** with the wheel or arrow keys
  (moves for a few items, then stops; dragging the scroll bar works).
  Cause (verified in code, same bug as the early scroll fix): the popup's
  scroll point (`c->scroll_pt`, `rad_completion_popup_ui`) is not in the
  list `rad_view_state_animate` eases every frame (only `cv`, `ex`, `pr`).
  Done when: a script scrolls a 100-item list by wheel and by Down to the
  last item and the visible rows follow.
- [x] **A3. Panel focus.** Clicking into the code of another panel must
  focus that panel (today only a click on empty panel space does).
  Cause (verified): the panel takes focus only from fallthrough presses
  (`rad_core.c` "consume panel fallthrough interaction events"); the code
  view's own box takes the press first. Fix: focus on any press inside the
  panel's rect before its content handles it.
  Also: focus sometimes jumps to the menu bar while typing. Suspect (not
  verified): a lone Alt press/release focuses the menu bar
  (`focus_menu_bar_with_alt`, raddbg + Windows behavior); Alt+Left (Navigate
  Backward) or AltGr may end as a "lone Alt". Reproduce with a script first.
  Done when: scripts click between two editor panels and check the focused
  panel; Alt+Left and AltGr typing leave the menu bar unfocused.
  Done 2026-11-04: the code view focuses its panel on press (raddbg's
  step, dropped in the port); Alt focuses the menu bar only if nothing was
  pressed or scrolled while it was held (raddbg cancelled only for bound
  keys, so Alt+Up/Down, Alt+Shift+arrows, Alt+click focused it). AltGr not
  tested (scripts cannot send it); if focus still jumps, tell what was
  pressed just before.
- [x] **A4. Ctrl+click and Ctrl+hover go to definition** (`tIHhIDuW`).
  Holding Ctrl highlights the identifier under the mouse (VS2022: link
  color + underline, hand cursor); Ctrl+click goes to the definition of
  *that* identifier, not of the token at the cursor, and must not move the
  cursor somewhere else first.
  Done when: a script Ctrl+clicks a type while the cursor sits elsewhere
  and lands on its definition; field `ctrl_link` shows the hint.
  Done 2026-11-04 (script `lsp_ctrl_link`). The request already used the
  mouse position; what was missing was the hint and that the press moved
  the cursor first. Navigate Backward returns to the clicked identifier.
  Not reproduced: a Ctrl+click that went to the wrong place. If it still
  happens, note the file and line.
- [x] **A5. Cursor precision / last line.** Dragging a selection from the
  last line up selects from the line before it (13 lines: 13 -> 1 selects
  12 -> 1). It must select all lines; deleting them leaves one empty line 1.
  Cause (found, also in raddbg): the code slice clamped the mouse's line
  index to dim-1 of an inclusive line range, so the last visible line was
  unreachable whenever the file's end was on screen. Fixed; script
  `editor_mouse`.
- [x] **A6. The editor must fill its panel** (`tdW501KQ`). Below the last
  line there is an empty area that is not part of the editor. The text
  area and the line-number gutter go to the bottom of the panel; a click or
  drag below the last line goes to the end of the last line.
  **[suggestion]** Setting "Scroll past end" (default on, as VS2022 lets
  the last line scroll up to the top).
- [x] **A7. Completion inserts functions without `()`.** Advertise
  `snippetSupport`, turn the snippet into text (`$0`, `${1:x}`, `$1`), put
  the cursor at the first tab stop (inside the parentheses).
  **[suggestion]** Tab stop navigation (Tab to the next argument) as a later
  step; first just the cursor inside `()`.
  Done 2026-11-04: rust-analyzer gets `completion.callable.snippets =
  add_parentheses` (no argument placeholders); `name()` puts the cursor
  after, `name(…)` inside; generic types in type position get `<>`. Typing
  the closing bracket right before the inserted one steps over it.
- [x] **A8. Completion documentation and icons.** Documentation of the
  selected item beside the list (completionItem/resolve, same markdown
  reduction as hover). A kind icon per row, from raddbg's icon font
  (function, field, type, module, keyword, ...); no new icon style.
  Done 2026-11-04: rust-analyzer sends the documentation with the items
  (no resolve needed); a box beside the list (right, else left) shows the
  selected item's, like the hover tooltip. Icons: raddbg's Cube (functions),
  Grid (types), Dot (fields/variables/keywords), List (constants, enum
  members), Module, Pencil (snippets), each in that token kind's code color.
- [x] **A9. Undo an auto-import in one Ctrl+Z.** Resolve the selected item
  while it is selected (VS2022 does), so the `use` line is usually there at
  commit and goes into the same undo group; if it arrives later, add it to
  the commit's undo group (new `text_buffer` call, checked by `rad_tests`).
  Done 2026-11-04: `text_buffer` undo steps (begin/end/join, rad_tests
  `undo_steps`); the selected import item is resolved while selected; a
  late answer joins the commit's step if nothing happened since, else is
  its own step, and is dropped if the commit was undone.
- [x] **A10. Restart rust-analyzer.** Command "Restart Language Server"
  (menu + command palette); automatic restart when the server exits, at most
  3 times in a minute, with the reason in the bottom bar.
  **[suggestion]** Click the bottom-bar rust-analyzer status for a small
  menu: Restart, Show Log.
  Done 2026-11-04 (script `lsp_restart`): Build > Restart Language Server;
  a crash restarts it (3 per minute). The bottom-bar menu is not done.
- [x] **A11. Flaky `file_watch` script.** Failed once under load (woken
  after 471 ms instead of >= 800). Find why rad woke early (likely a late
  change notification from the earlier write) and make the check measure
  the right thing; 20 runs in a row under a parallel full run must pass.
  Done 2026-11-04: `sleep_until_woken` ended on any OS message (the mouse
  over the window, other test windows); it now ends only on a watcher
  wakeup after any pending `write_file_later`. 20 of 20 passed next to a
  full parallel run (27 of 27).

## B. Settings

- [x] **B1. Settings storage and Settings view.** No hardcoded values: every
  setting gets a row in a settings table in `rad.mdesk` (name, type,
  default, range, display name, description), ported from raddbg's
  `RD_SchemaTable` (`@schema user`, `@schema window`), kept in the user file
  (`%APPDATA%/rad/default.rad_user`), applied live. A Settings tab (Tools >
  Settings, Ctrl+Comma is taken by the explorer, so: **[suggestion]**
  Ctrl+Shift+Comma) that looks like raddbg's settings table (`rad_cell`
  rows, the same check boxes, sliders, text fields) with a filter box.
  Replaces `rad_default_settings`.
  Done 2026-11-04 (scripts `settings`, `settings_view`). File > Settings
  (no key: Ctrl+Shift+Comma is raddbg's Previous Panel). Rows are raddbg's
  big lister rows (5 font sizes; Compact: 4): name + description, toggle
  switch, value + slider, revert button (changed settings only); enums and
  the theme open a menu of their options. Keyboard: Up/Down, Space/Enter,
  Left/Right; typing filters. Not yet: typing a number into a value.
- [x] **B2. The settings themselves** (first set). 2026-11-04: in the table
  and applied: all animations, opaque backgrounds, blur, drop shadows,
  rounded corners, density (row and tab heights), UI and code font sizes,
  smooth/hint text, Mouse Wheel Zoom, line numbers, tab width, scope
  lines/annotations, theme, Alt focuses the menu bar, explorer side and
  compact rows. Left: tab overflow and close button (D), scroll past end,
  font faces.
  Done 2026-11-04 (script `settings_more`): Tab Close Button (On Hover /
  Always), Scroll Past End (on = raddbg's), UI Font / Code Font (raddbg's
  `main_font` / `code_font`, a new String kind edited in place: Enter or
  double-click starts, Enter commits, Esc cancels). The font provider
  takes a file name looked up in the user and system font folders
  (`consola.ttf`) or a path, not a family name (raddbg's behavior); one
  that does not open falls back to the built-in font.
  - Animations (all), Scrolling Animations ("smooth movement"), Tooltip
    Animations, Menu Animations, Cursor Trail (raddbg's).
  - Transparency: Opaque Backgrounds, Background Blur (raddbg's); Drop
    Shadows, Rounded Corner Amount.
  - Density: **Compact / Comfortable** (row height, tab height, paddings;
    compact = smaller rows, tabs, headers everywhere).
  - Fonts: UI font size (window), **Code font size** (editor only), fonts,
    smooth/hint text (raddbg's).
  - **Mouse Wheel Zoom**: "Whether to zoom the editor font size with the
    mouse wheel while holding the primary modifier key" (Ctrl on Windows,
    Cmd on macOS). Default on.
  - Tabs: overflow mode (D2), close button (always / on hover).
  - Explorer: compact rows (C2).
  - Editor: show line numbers, tab width, scroll past end, cursor scope
    lines/annotations.
  - Theme preset (raddbg's presets).
- [x] **B3. Zoom the code font alone.** Today Ctrl+wheel zooms the whole
  layout (`rad_core.c`, Ctrl+Scroll -> `IncWindowFontSize`). New:
  Ctrl+wheel over an editor = code font size (when Mouse Wheel Zoom is on);
  Ctrl+Shift+Period / Ctrl+Shift+Comma (VS2022 Zoom In/Out) and a reset.
  Alt+= / Alt+- (raddbg's keys) stay the whole-window font size.
  Done when: a script zooms the code font and checks the UI font did not
  change, and the size is saved in the user file.
  Done 2026-11-04 (script `settings`). Keys: Ctrl+Shift+= / Ctrl+Shift+-,
  Ctrl+0 resets (Ctrl+Shift+Comma stays raddbg's Previous Panel).

## C. Layout: tool windows

Today the explorer and Output/Problems are tabs in normal panels. They
become VS2022-style tool windows: fixed dock areas outside the editor panel
tree, each with a header (title, X), no "+", no zoom button.

```
+------------------------------------------------------------+
| top bar: menus (File Edit View ...), run toolbar           |
+--------------+---------------------------------------------+
| Explorer   X | [main.rs][mod.rs]          editor panels:   |
| [search...]  |  splits, tabs, drag and drop (F)            |
| tree rows    |                                             |
+--------------+---------------------------------------------+
| Output                                               v  X  |  <- full width,
|  ...                                                        |     fixed at the
| [Output] [Problems]                                         |     bottom
+------------------------------------------------------------+
| bottom bar (status)                                        |
+------------------------------------------------------------+
```

- [x] **C1. View menu.** New top-level menu "View": Explorer, Output,
  Problems (shows the tool window again after X). **[suggestion]** VS2022
  keys: Ctrl+Alt+L (Solution Explorer), Ctrl+Alt+O (Output), Ctrl+\, E
  (Error List) if chords are cheap; otherwise only menu items.
  Done 2026-11-04 (script `tool_windows`): View > Explorer (Ctrl+Alt+L),
  Output (Ctrl+Alt+O), Problems (no key: no chords yet), Close Tool Window
  (Shift+Esc, VS2022's). The Tab menu's Alt key was V by mistake; now B.
- [x] **C2. Explorer as a tool window** (left, width adjustable). Header:
  folder name, X to close. No "+" next to the folder name; no Toggle Zoom.
  Open Folder stays in File and in the empty state. A search/filter box
  with a search icon: filters files and folders by name, keeps the parents
  of matches, Esc clears (**[suggestion]** Ctrl+; focuses it, as VS2022).
  Compact rows from the setting (B2). Old user files with the explorer as a
  panel tab load into the new layout.
  2026-11-04, layout part done (scripts `tool_windows`, `layout_migrate`):
  tool windows are leaf panels holding tool tabs, kept by
  `rad_window_tool_layout_ensure` (every frame, before commands) in
  `panels (y): [top (x): explorer | editor panels...] [bottom]`; Explorer
  Side setting (Left/Right); hidden = size 0 (size kept in `tool_sizes`);
  header with title + X (zoom button style), no tab bar, no "+", no zoom;
  Close Panel / X / Shift+Esc hide; splits go to the editor panel and take
  space only from the split panel; the last editor panel cannot close.
  Filter + compact rows done 2026-11-04 (script `explorer_filter`): a
  filter row on top (the query bar's look); typing in the explorer or
  Ctrl+; (Search Explorer) gives it the keyboard; matches by name in the
  whole tree (fuzzy, case-insensitive) with their parent folders, skipping
  .git and target; Down/Enter go to the first match; Enter on a folder
  clears the filter and reveals the folder; Esc clears, Esc again leaves.
  Setting Compact Explorer Rows (2 font sizes).
- [x] **C3. Bottom tool window** (`QJK3Syr5`): docked below everything, full
  window width, height adjustable, compact header (title, collapse, X),
  tabs Output / Problems at the bottom as VS2022 shows them, rows and text
  smaller than the editor's (code font size of its own, from the density
  setting). X hides it; View brings it back; a cargo command shows it.
  **[suggestion]** Later: a Terminal tab here.
  Done 2026-11-04 (scripts `tool_windows`, `cargo_build`): full width under
  the explorer and editors, hidden until a cargo command or View shows it,
  header (2.25 font sizes: title + X), Output / Problems tabs at the bottom
  (raddbg's `tabs_on_bottom`). Not done: a font size of its own (it uses
  the code font size), a collapse button.

## D. Tabs

- [x] **D1. Compact tabs** (`TN8f33D5`): width = icon + title + buttons
  (not raddbg's fixed width), close X on the active and hovered tab,
  dirty mark kept.
  Done 2026-11-04 (script `tabs`): `rad_tab_width_target_px` = title + 1em
  + room for X (+ the pin's on pinned and selected tabs); the selected
  tab no longer grows when its panel is focused; X drawn on the selected
  and hovered tab only (room kept). raddbg's tab shape and colors kept.
- [x] **D2. Overflow setting** when the tabs do not fit: (1) wrap into more
  rows, or (2) one row scrolled with the mouse wheel. **[suggestion]** A
  "..." button at the right end of the tab bar listing all tabs (VS2022).
  Done 2026-11-04 (script `tabs`): setting Tab Overflow. Scroll: the wheel
  over the tab bar scrolls it sideways; a down-caret button (left of the
  zoom button) lists all tabs; picking one brings it to the front (after
  the pinned ones, as VS2022). Wrap: one tab bar per row, "+" on the last.
  Not done: keeping the selected tab in view after Next Tab in Scroll mode.
- [x] **D3. Pin tabs.** Pin icon on the tab; pinned tabs move to the front
  and stay there; unpin with the same icon; saved in the session.
  Done 2026-11-04 (script `tabs`): Pin Tab command (Tab menu, pin button
  on pinned and selected tabs); a `pinned` child of the tab cfg (saved
  with the session); `rad_tab_move_after_pinned` keeps the order.

## E. Editor gutter

- [x] **E1. Glyph margin** between the panel edge and the line numbers
  (`oxg7eMtQ`), for breakpoints later. raddbg's code slice has this
  ("priority" and "catchall" margins, cut from the port); bring them back
  with raddbg's sizes. **[suggestion]** Until breakpoints exist, show the
  diagnostic icon of a line there, and VS2022's change bar (unsaved /
  saved changes) at its right edge.
  Done 2026-11-04 (checked on a screenshot; the 32 scripts still pass):
  raddbg's priority and catchall margins are back in text tabs (3.5 glyph
  advances each, floating when scrolled sideways, as raddbg); the priority
  margin is empty (breakpoints later); the catchall margin shows the icon
  of the line's worst diagnostic. Not done: the change bar.
- [x] **E2. Code folding.** A fold margin between line numbers and text: a
  down arrow appears on hover at the first line of a block, tooltip "Click
  to collapse the range"; collapsed blocks show one line with a "..."
  marker, the arrow points right, click expands. Ranges from rust-analyzer
  (`textDocument/foldingRange`: blocks, comment runs, `use` groups), brace
  scopes from the lexer as fallback. Needs the code view to map visible
  rows to buffer lines (cursor moves, scrolling, find, go to line, edits
  inside a fold all expand or skip it). Biggest task of this list.
  Plan (2026-11-04): (1) ranges: brace pairs over 1+ lines and runs of
  comment lines, from the lexer's tokens (rust-analyzer's foldingRange
  later); (2) per code view a fold list (start offsets, mapped through
  edits by the buffer's version mapping); a fold hides lines start+1..end;
  (3) the code view scrolls in rows: row <-> line through the sorted folds
  (no per-line tables); the code slice gets per-row line numbers
  (`line_nums`), everything else stays per visible row; (4) a fold column
  right of the line numbers: chevron on hover at a range start (tooltip
  "Click to collapse the range"), right chevron + "..." after a folded
  line; (5) the cursor landing in a fold (moves, Find, Go To Line, edits)
  expands it.
  Done 2026-11-04 (script `folding`, fixture `fold.rs`) as planned, with
  the byte scanner instead of the lexer for ranges (strings, raw strings,
  char literals and comments skipped). Up/Down skip a fold; folding a range
  with the cursor inside moves it to the fold's first line (VS2022); the
  "..." box and the right chevron unfold. Not done: rust-analyzer's
  foldingRange, a Toggle Fold key (VS2022's Ctrl+M, Ctrl+M is a chord),
  folds saved in the session.

## F. Drag and drop

- [x] **F1. Drag tabs and panels** (`vPebvQb0`). Drag a tab, or a whole
  panel by its tab bar, over a panel: a docking guide in the middle (center
  = add as tab; left/right/up/down = split) plus edge targets, and a preview
  of the future rect. raddbg already has drop sites and the animated
  preview (`raddbg_core.c` "panel non-leaf UI (drag boundaries, drag/drop
  sites)") and `MoveTab`; port them, add the center compass in the same
  style. Tool windows (C) are not drop targets for editor tabs.
  Done 2026-11-04 (script `drag_tabs`): raddbg's drag state machine,
  combined split + move-tab sites (rad: all four sides plus the center,
  the docking compass), the animated future rect, the catch-all site (a
  drop on a tab bar goes after the nearest tab end), boundary sites
  between editor panels and on the editor area's edges, the hollow
  dragged tab and the title tooltip. Dragging a tab bar's free space
  moves the whole panel (all its tabs). A panel left empty closes; Esc
  cancels a drag. Tool windows are neither dragged nor drop targets. Not
  done: raddbg's live view preview inside the drag tooltip (rad shows the
  title only); dragging between windows (one window for now).
- [x] **F2. Drag files**: from the explorer into a panel or a tab bar
  (opens there), and from Windows Explorer onto a specific panel.
  Done 2026-11-04: an explorer file row dragged uses the same drop sites
  (center or tab bar: opens in that panel; a side: in a new panel there).
  Windows Explorer drops onto a panel already opened there (OS FileDrop
  event, checked per panel rect). Folders are not draggable.

## Ideas for later (not in this round)

- **[suggestion]** Signature help popup when typing `(` (follows A7).
- **[suggestion]** Code actions / quick fixes (the screwdriver in
  `tIHhIDuW`): LSP `textDocument/codeAction` in the glyph margin.
- **[suggestion]** Navigation bar under the tabs (`TN8f33D5`: type and
  member dropdowns) from `textDocument/documentSymbol`.
- **[suggestion]** Semantic token colors from rust-analyzer (functions,
  macros, types colored like VS2022).

## Decisions (user, 2026-11-04)

1. Tool windows are not draggable. The explorer is fixed on the left; a
   setting moves it to the right (Explorer Side: Left / Right).
2. The bottom tool window has its tabs at the bottom, as VS2022
   (`QJK3Syr5`).
