// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

#ifndef RAD_SCRIPT_H
#define RAD_SCRIPT_H

////////////////////////////////
//~ rad: Script Mode
//
// `rad --script:<file> [--script_out:<file>] [--logs:<dir>]` runs a test
// script, then exits. Input goes through the window manager's simulated
// input (window_manager.h, "Simulated Input"), so it behaves like real OS
// input but needs no window focus: the window opens without activating,
// real keyboard/mouse input is ignored, and the clipboard is private. Results
// are appended to the out file (default: `<script>.results.txt`) as they
// happen, one line each:
//
//   PASS <line>: <command>
//   FAIL <line>: <command> (got <value>)
//   ERROR <line>: <message>
//   PRINT <line>: <field> = <value>
//   MEASURE <name>: <stats>
//   DONE pass <n> fail <n> error <n>
//
// Script language: one command per line; `#` starts a comment line; paths
// are relative to the script's folder; strings are in double quotes with
// escapes \n \t \" \\. Input commands run one input step per frame, then
// let 2 frames pass before the next command.
//
//   open <path>                 open a file (Open File command)
//   open_folder <path>          point the explorer at a folder (Open Folder)
//   cmd <code_name>             run a command by its rad.mdesk code name
//   key <chord> [count]         press+release, e.g. `key ctrl+s`, `key page_down 3`
//                               (key names: window_manager.mdesk cfg names)
//   key_down <key>              press and hold a key (e.g. `key_down alt`; chords
//   key_up <key>                in between name the held modifier: `key alt+up`)
//   text "<string>"             type a string (one text event per character)
//   wheel <notches> <target>    mouse wheel, positive = down, one notch per frame
//   wheel_units <units> <count> <target>   `count` scroll steps of `units` each
//                               (a notch is 120; a trackpad sends small steps)
//   mouse <target>              move the mouse
//   click <target>              left click
//   double_click <target>
//   ctrl_click <target>         left click with Ctrl held
//   right_click <target>        right click
//   drag <target> <target>      left press at the first, move, release at the second
//   mouse_down <target>         left press there (no release; a held drag)
//   mouse_up <target>           left release there
//   write_file <path> "<text>"  write a file on disk (as another program would)
//   delete_file <path>
//   write_file_later <ms> <path> "<text>"  the same, from a thread, after <ms>
//   sleep_until_woken           let rad block in its event wait, like an idle
//                               app, until the disk watcher wakes it (other OS
//                               messages that end the wait let it sleep again;
//                               field last_sleep_ms = how long it slept)
//   wait <ms>
//   wait_frames <count>
//   wait_idle [timeout_ms]      until rad requests no more frames (text loaded,
//                               animations and compaction done); default 10 s
//   wait_cargo [timeout_ms]     until no cargo command runs or waits to; 120 s
//   wait_output "<text>" [timeout_ms]  until the cargo output contains it; 120 s
//   wait_until <timeout_ms> <field> <op> <value>  until the check passes (one
//                               PASS), or FAIL at the timeout; e.g. for a
//                               language server: wait_until 90000 diag_count == 2
//   lsp_kill                    kill the editor file's language server process (a crash)
//   set_setting <name> <number> set a setting (rad.mdesk RAD_SettingTable; enums: index)
//   only_on <mac|windows|linux> the rest of the script runs on that OS only
//   float_window [0|1]          keep the window above other apps' windows
//                               (screenshots: covered windows are not drawn)
//   lsp on|off                  language servers (rust-analyzer): off by default in
//                               scripts (slow to start; a script that does not test
//                               them stays deterministic)
//   terminal visible|hidden     where Run puts the program: by default a hidden
//                               terminal whose output goes to the Output tab;
//                               `visible` opens a real window (manual checks)
//   expect <field> <op> <value> ops: == != < <= > >= ; numbers compare as numbers
//                               (decimals too: "11.50" == 11.5)
//   print <field>
//   measure_begin <name>
//   measure_end <name>          three stats over the frames in between:
//                               frame: input wait end -> frame presented (includes
//                               waiting for vsync, so ~16.7 ms at 60 Hz is the floor);
//                               ui build: CPU time building + laying out the UI;
//                               input latency: input injected -> its frame presented
//                               (app side only; the OS and compositor add more)
//   quit
//
// Targets (positions come from the last frame's layout):
//   code <line> <col>           a visible text position in the editor view
//   code_center                 middle of the editor view
//   explorer <name>             middle of the visible explorer row named <name>
//   explorer_expander <name>    that row's expander arrow
//   explorer_center             middle of the explorer view
//   output <line> <col>         a visible text position in the Output tab
//   output_text "<text>"        the first Output line containing it, at its start
//   output_center               middle of the Output view
//   problem <n>                 middle of visible Problems row n (1-based)
//   tool_close <explorer|bottom> a tool window's X button
//   explorer_filter             the explorer's search filter row
//   fold <line>                 the editor's fold chevron of a visible line
//   nav_box <item|member>       a navigation bar box
//   nav_list_item <n>           entry n (1-based) of its open list
//   code_actions_button         the code actions button (screwdriver)
//   code_action_item <n>        item n (1-based) of its menu
//   panel_site <n> <site>       editor panel n (tree order, tool windows skipped):
//                               a docking compass site (center up down left
//                               right), an edge's middle (edge_left edge_right
//                               edge_up edge_down: boundary drop sites) or its
//                               tab bar's free end (tabs)
//   tab_box <name>              a tab (any panel), by its name
//   tab_pin <name>              its pin button (shown while the tab is hovered, or pinned)
//   tab_close <name>            its close button (shown while the tab is hovered)
//   tab_list                    the "..." all-tabs button (tabs overflow)
//   tab_list_item <n>           item n (1-based) of its menu
//   setting_row <name>          middle of a visible Settings row
//   setting_control <name>      its toggle switch or slider
//   setting_revert <name>       its revert button (only on changed settings)
//   settings_filter             the Settings filter row
//   setting_option <n>          option n (1-based) of the open options menu
//   completion <n>              middle of visible completion list row n (1-based)
//   completion_list             middle of the completion list
//   code_below <col>            under the editor's last line (2 lines down), at a
//                               column as if on that line
//   tab_code "<name>"           middle of the code view of the shown tab with that
//                               name, in any panel (focused or not)
//   toolbar <name>              a top bar button: cargo_run cargo_test cargo_check
//                               cargo_build cargo_stop, configuration (the
//                               Debug/Release box), or its open menu's rows
//                               cargo_debug cargo_release
//   <x> <y>                     window client pixels
//
// Fields (the editor view is the focused panel's text tab, else the last
// focused editor panel's selected tab; the explorer is the window's first):
//   panel                       focused panel's selected tab kind ("text", "explorer", ...)
//   tab                         its name (file name, folder name, or kind)
//   tab_title                   its title as shown, minus icons ("<src> main.rs")
//   tab_count                   number of tabs in the focused panel
//   panel_count                 number of leaf panels in the window
//   zoomed                      1 if a panel is zoomed (Toggle Panel Zoom)
//   menu_bar_focused            1 if the menu bar has the keyboard (Alt tapped)
//   tab_pinned                  1 if the focused panel's selected tab is pinned
//   tab_index                   its position in the panel (1-based)
//   tab_rows                    tab bar rows of the focused panel
//   tabs_overflow               1 if its tabs do not fit in one row (Scroll)
//   explorer_visible            1 if the explorer tool window is shown
//   explorer_filter             the explorer search filter's text
//   explorer_filter_focused     1 if the explorer filter has the keyboard
//   explorer_row_height         explorer row height in pixels
//   explorer_on_right           1 if the explorer is docked on the right
//   bottom_visible              1 if the bottom tool window (Output, Problems) is shown
//   settings_rows               rows the Settings tab shows (the filter's matches)
//   settings_selected           the selected setting's name ("" = none)
//   settings_filter             the Settings filter's text
//   settings_filter_focused     1 if the filter has the keyboard
//   settings_editing            1 while a text setting (a font) is edited in place
//   font_custom <main|code>     1 if the UI Font / Code Font setting opened a font
//   setting <name>              a setting's value as stored (enums: the option's name)
//   editor_font_size            the editor view's font size (code font setting)
//   editor_font_size_drawn      the size it was drawn at last frame, 2 decimals
//                               (in between while a zoom eases)
//   editor_line_height          its line height last frame (pixels, 2 decimals)
//   output_font_size            the Output view's font size (Panel Font Size)
//   status_text_fits            1 if the status bar's text box lies inside the bar
//   menu_width_em               the open menu's width in UI font sizes (0: none)
//   menu_rows_fit               1 if every row of the open menu holds its contents
//   mouse_hidden                1 while typing hid the mouse pointer (setting
//                               hide_mouse_while_typing; until the mouse moves)
//   toolbar_icon.<code_name>    the icon font glyph that top bar button drew ("s":
//                               raddbg's Stop)
//   inline_diag_count           editor lines showing an inline diagnostic (visible ones)
//   inlay_hint_count            inlay hints shown on the editor's visible lines
//   key_label <command>         the key its tooltips show ("Cmd + S" on the Mac,
//                               "Ctrl + S" elsewhere), "" if none
//   menu_key_label <command>    the key its menus show (Apple's symbols on the
//                               Mac, e.g. U+2318 S; key_label's words elsewhere)
//   lsp_hint_state              the editor's inlay hint requests: "supported asked
//                               in_flight valid count"
//   panel_tabs <n>              editor panel n's tab names, space-separated
//   drag_active                 1 while a tab or file is being dragged
//   nav_item nav_member         the navigation bar's item / member names ("" = none)
//   nav_list_count nav_list_rows nav_list_first   its open list: entries, rows it
//                               shows at most, first entry shown (1-based); 0 if closed
//   symbol_count                the editor document's symbols (documentSymbol)
//   code_actions_count          code actions in the last answer
//   code_action <n>             the title of action n (1-based)
//   code_actions_shown          1 if the code actions button is shown
//   signature_open              1 if signature help (Parameter Info) is shown
//   signature_label             its signature; signature_param: the active parameter
//   semantic_count              the editor document's semantic tokens (rust-analyzer)
//   semantic_slot <line> <col>  code color slot of the one starting there ("" if none)
//   fold_count                  folded ranges in the editor view
//   line_hidden <line>          1 if the line is folded away
//   ui_font_size                the window's font size (UI font setting)
//   explorer_panel_tabs         number of tabs in the explorer's panel
//   cursor_line cursor_col mark_line mark_col     editor cursor/mark (1-based)
//   visible_first visible_last  editor's visible line range
//   line_count                  editor's line count
//   line <n>                    text of editor line n (without the line end)
//   token <n> <col>             kind of the token there, as the code view lexes it:
//                               keyword, identifier, string, char, numeric, symbol,
//                               line_comment, block_comment, meta, whitespace, error
//   dirty                       1 if the editor's buffer has unsaved changes
//   loading                     editor tab's loading overlay strength, 0..1
//   file <path>                 contents of a file on disk
//   path_exists <path>          1 if a file or folder is there
//   popup_active                1 while the confirmation popup is open
//   explorer_editing            none, new_file, new_folder or rename (a name typed in the tree)
//   explorer_menu_open          1 while the explorer's context menu is open
//   file_size <path>            its size in bytes
//   explorer_root explorer_rows explorer_selected
//   explorer_row <n>            name of explorer row n (1-based)
//   explorer_selection          the selected rows' names, ", " between (TODO_2 F2)
//   selection                   the editor's selected text (mark..cursor)
//   query_open query_input      the editor tab's query bar (Find, Go To Line)
//   doc_count                   open documents (one per file, shared by tabs)
//   error                       bottom bar error message ("" when none)
//   cargo_status                none, running, stopped, launch_failed, failed
//                               (exit code != 0), finished
//   cargo_cmd cargo_exit_code   last cargo command ("cargo build") and its exit code
//   cargo_profile               "debug" or "release" (the configuration box)
//   lsp_state                   the editor file's language server: none, starting,
//                               ready, ended (none also when rust-analyzer is missing)
//   lsp_busy                    1 while it reports progress or is not quiescent
//   lsp_status                  its bottom bar text ("" when idle)
//   lsp_servers                 language servers started so far (one per workspace)
//   lsp_restarts                their restarts (command or after a crash)
//   cursor_line_text            the editor's line at the cursor
//   ctrl_link                   the identifier highlighted under the mouse while
//                               Ctrl is held (Go To Definition link), "" if none
//   hover_text                  the hover tooltip's text (markdown; "" when none)
//   hover_shown                 1 if the hover tooltip was built last frame
//   hover_covers_token          1 if it overlaps the hovered token's line
//   hover_has "<text>"          1 if it contains the text
//   completion_open             1 if the completion list is shown
//   completion_count            its rows; completion_row <n>: row n's label
//   completion_selected         the selected row's label; completion_cursor: its number
//   completion_has "<label>"    1 if a row has that label
//   completion_filter           the typed word it is filtered by
//   completion_doc              the selected item's documentation ("" if none);
//                               completion_doc_shown: 1 if shown beside the list;
//                               completion_kind: its LSP CompletionItemKind;
//                               completion_resolved: 1 if its resolve came back
//   completion_top completion_bottom   first/last row fully visible in it;
//                               completion_below: rows under the last visible one;
//                               completion_selected_visible: 1 if the selection is
//   completion_width_em completion_height_em   the list's size in font heights
//   completion_font_size        the list's font size last frame (0: not shown)
//                               (settings completion_width/_height; 0: not shown)
//   diag_count diag_errors diag_warnings   problems: the last cargo command's
//                               diagnostics and the language servers', merged
//   diag <n>                    "<kind> <path>:<line>:<col> <message>" (path
//                               relative to the workspace root)
//   diag_selected               Next/Previous Error position (= Problems selection)
//   line_diag <n>               kind of the diagnostic now on editor line n, or ""
//   output_lines                lines written to the cargo output so far
//   output_line <n>             one of them
//   output_has "<text>"         1 if the output contains the text
//   output_line_count output_cursor_line output_cursor_at_end   the Output tab's
//                               text and cursor (it shows the output <= 100 ms later)
//   output_top_line output_bottom_line   the first and last line the Output
//                               shows whole (the bottom one may be past the text)
//   output_line_numbers         1 if the Output shows line numbers (it does not)
//   output_at_bottom            1 if its last line with text is on its bottom row
//                               (or all of it shows from line 1)
//   elapsed_ms                  time since the script started
//   last_sleep_ms               duration of the last sleep_until_woken

////////////////////////////////
//~ rad: Script Types

#define RAD_SCRIPT_SETTLE_FRAMES 2
#define RAD_SCRIPT_MAX_SAMPLES 16384
#define RAD_SCRIPT_DEFAULT_TIMEOUT_US 10000000
#define RAD_SCRIPT_TOTAL_TIMEOUT_US 300000000

typedef struct RAD_ScriptLine RAD_ScriptLine;
struct RAD_ScriptLine
{
  U64 line_num;
  String8 text;
  String8 *args;
  U64 args_count;
};

typedef enum RAD_ScriptInputKind
{
  RAD_ScriptInputKind_Null,
  RAD_ScriptInputKind_Key,
  RAD_ScriptInputKind_KeyDown,
  RAD_ScriptInputKind_KeyUp,
  RAD_ScriptInputKind_Text,
  RAD_ScriptInputKind_Wheel,
  RAD_ScriptInputKind_MouseMove,
  RAD_ScriptInputKind_MousePress,
  RAD_ScriptInputKind_MouseRelease,
}
RAD_ScriptInputKind;

typedef struct RAD_ScriptInput RAD_ScriptInput;
struct RAD_ScriptInput
{
  RAD_ScriptInput *next;
  RAD_ScriptInputKind kind;
  WM_Modifiers modifiers;
  WM_Key key;
  U32 character;
  Vec2F32 pos;
  F32 wheel_delta;
};

typedef struct RAD_ScriptDelayedWrite RAD_ScriptDelayedWrite;
struct RAD_ScriptDelayedWrite
{
  U64 delay_ms;
  String8 path;
  String8 data;
};

typedef enum RAD_ScriptWaitKind
{
  RAD_ScriptWaitKind_Null,
  RAD_ScriptWaitKind_Time,
  RAD_ScriptWaitKind_Frames,
  RAD_ScriptWaitKind_Idle,
  RAD_ScriptWaitKind_Woken,
  RAD_ScriptWaitKind_Cargo,
  RAD_ScriptWaitKind_Output,
  RAD_ScriptWaitKind_Until,
}
RAD_ScriptWaitKind;

typedef struct RAD_ScriptState RAD_ScriptState;
struct RAD_ScriptState
{
  Arena *arena;
  String8 path;
  String8 folder;
  String8 out_path;
  U64 start_us;

  //- rad: program
  RAD_ScriptLine *lines;
  U64 lines_count;
  U64 next_line_idx;
  B32 done;

  //- rad: results
  U64 pass_count;
  U64 fail_count;
  U64 error_count;

  //- rad: waiting
  RAD_ScriptWaitKind wait_kind;
  U64 wait_line_num;
  U64 wait_until_us;
  U64 wait_frames_left;
  U64 sleep_begin_us;
  U64 sleep_begin_wakeups;      // rad_state->watch_wakeups_sent when the sleep began
  U64 delayed_writes_pending;   // write_file_later threads not done yet (atomic)
  U64 last_sleep_us;
  U8 wait_text_buffer[256];
  U64 wait_text_size;
  B32 terminal_visible;
  RAD_ScriptLine *wait_line;

  //- rad: queued input steps (one per frame)
  Arena *input_arena;
  RAD_ScriptInput *first_input;
  RAD_ScriptInput *last_input;
  Vec2F32 mouse;
  WM_Modifiers held_modifiers; // key_down of Ctrl/Shift/Alt: held for the next inputs until key_up
  U64 input_injected_us;

  //- rad: measurement
  B32 measuring;
  String8 measure_name;
  U64 measure_work_us[RAD_SCRIPT_MAX_SAMPLES];
  U64 measure_work_count;
  U64 measure_build_us[RAD_SCRIPT_MAX_SAMPLES];
  U64 measure_build_count;
  U64 measure_latency_us[RAD_SCRIPT_MAX_SAMPLES];
  U64 measure_latency_count;
};

////////////////////////////////
//~ rad: Globals

global RAD_ScriptState *rad_script = 0;

////////////////////////////////
//~ rad: Script Functions

internal void rad_script_init(CmdLine *cmdln);
internal B32 rad_script_is_active(void);
internal B32 rad_script_lets_rad_sleep(void);
internal void rad_script_delayed_write_entry_point(void *p);
internal void rad_script_tick(void);
internal void rad_script_frame_end(U64 frame_work_us, U64 ui_build_us);

#endif // RAD_SCRIPT_H
