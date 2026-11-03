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
//   text "<string>"             type a string (one text event per character)
//   wheel <notches> <target>    mouse wheel, positive = down, one notch per frame
//   mouse <target>              move the mouse
//   click <target>              left click
//   double_click <target>
//   drag <target> <target>      left press at the first, move, release at the second
//   write_file <path> "<text>"  write a file on disk (as another program would)
//   delete_file <path>
//   write_file_later <ms> <path> "<text>"  the same, from a thread, after <ms>
//   sleep_until_woken           let rad block in its event wait, like an idle
//                               app, until something wakes it (field
//                               last_sleep_ms = how long it slept)
//   wait <ms>
//   wait_frames <count>
//   wait_idle [timeout_ms]      until rad requests no more frames (text loaded,
//                               animations and compaction done); default 10 s
//   expect <field> <op> <value> ops: == != < <= > >= ; numbers compare as numbers
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
//   <x> <y>                     window client pixels
//
// Fields (the editor view is the focused panel's text tab, else the last
// focused editor panel's selected tab; the explorer is the window's first):
//   panel                       focused panel's selected tab kind ("text", "explorer", ...)
//   tab                         its name (file name, folder name, or kind)
//   tab_title                   its title as shown, minus icons ("<src> main.rs")
//   tab_count                   number of tabs in the focused panel
//   panel_count                 number of leaf panels in the window
//   explorer_panel_tabs         number of tabs in the explorer's panel
//   cursor_line cursor_col mark_line mark_col     editor cursor/mark (1-based)
//   visible_first visible_last  editor's visible line range
//   line_count                  editor's line count
//   line <n>                    text of editor line n (without the line end)
//   dirty                       1 if the editor's buffer has unsaved changes
//   file <path>                 contents of a file on disk
//   file_size <path>            its size in bytes
//   explorer_root explorer_rows explorer_selected
//   explorer_row <n>            name of explorer row n (1-based)
//   selection                   the editor's selected text (mark..cursor)
//   query_open query_input      the editor tab's query bar (Find, Go To Line)
//   doc_count                   open documents (one per file, shared by tabs)
//   error                       bottom bar error message ("" when none)
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
  U64 last_sleep_us;

  //- rad: queued input steps (one per frame)
  Arena *input_arena;
  RAD_ScriptInput *first_input;
  RAD_ScriptInput *last_input;
  Vec2F32 mouse;
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
