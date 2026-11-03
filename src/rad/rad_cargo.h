// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

#ifndef RAD_CARGO_H
#define RAD_CARGO_H

////////////////////////////////
//~ rad: Notes
//
// Cargo integration. Build / Check / Run / Test run
// `cargo <subcommand> --message-format=json` in the project folder (see
// rad_cargo_folder_from_window), after saving every changed file. stdout and
// stderr share one pipe, so the Output tab shows both in the order a
// terminal would. A runner thread reads the pipe line by line:
//
//   {"reason":"compiler-message",...} -> its "rendered" text goes to the
//                                        output; the message becomes a
//                                        diagnostic (file, line, column)
//   other {"reason":...} lines         -> dropped (artifacts, build
//                                        scripts, build-finished)
//   anything else                      -> output as is (cargo's status
//                                        lines, the program's own output)
//
// Diagnostic paths are relative to the workspace root, which the runner
// asks cargo for first (`cargo locate-project --workspace`).
//
// The command runs in a process group (a job object on Windows): Stop
// closes the group, which kills cargo, rustc and the program together.
//
// Threads: the runner thread appends to the output and to the diagnostics
// inbox under `mutex`, then wakes the UI. The UI thread (rad_cargo_tick,
// every frame) submits the output to `output_key` (at most every 100 ms
// while it grows, at once when the run ends; the Output tab's buffer
// follows that key), moves inbox diagnostics into `diags`, and joins the
// thread when the run is over. raddbg's output log (mutable_text) copies
// the whole log on every append; cargo output can be large and chatty, so
// it is not used here.

////////////////////////////////
//~ rad: Tunables

#define RAD_CARGO_OUTPUT_MAX_SIZE   MB(32)
#define RAD_CARGO_OUTPUT_SUBMIT_US  100000
#define RAD_CARGO_MAX_DIAGNOSTICS   100000

////////////////////////////////
//~ rad: Diagnostic Types

typedef enum RAD_DiagnosticKind
{
  RAD_DiagnosticKind_Error,
  RAD_DiagnosticKind_Warning,
  RAD_DiagnosticKind_Note,
  RAD_DiagnosticKind_Help,
  RAD_DiagnosticKind_COUNT
}
RAD_DiagnosticKind;

typedef struct RAD_Diagnostic RAD_Diagnostic;
struct RAD_Diagnostic
{
  RAD_Diagnostic *next;
  RAD_DiagnosticKind kind;
  String8 path;      // absolute, normalized
  S64 line;          // 1-based; start of the primary span
  S64 column;        // 1-based, in characters (rustc's convention)
  S64 end_line;
  S64 end_column;
  String8 code;      // "E0425", "unused_variables", or empty
  String8 message;   // "cannot find value `x` in this scope"
  String8 label;     // the primary span's label, or empty
  String8 rendered;  // the full text, as cargo prints it in a terminal

  // rad: where it sits in the file's open document (UI thread only; see
  // rad_diagnostic_range_from_document)
  U64 doc_id;
  U64 doc_version;
  Rng1U64 doc_range;
};

////////////////////////////////
//~ rad: Cargo Types

typedef enum RAD_CargoCmdKind
{
  RAD_CargoCmdKind_Null,
  RAD_CargoCmdKind_Build,
  RAD_CargoCmdKind_Check,
  RAD_CargoCmdKind_Run,
  RAD_CargoCmdKind_Test,
  RAD_CargoCmdKind_COUNT
}
RAD_CargoCmdKind;

typedef struct RAD_CargoRun RAD_CargoRun;
struct RAD_CargoRun
{
  // rad: set by the UI thread before the runner starts, then read-only
  Arena *arena;
  RAD_CargoCmdKind kind;
  String8 folder;
  String8List cmd_line;
  ProcessGroup group;
  Thread thread;
  U64 start_us;

  // rad: runner thread only
  U64 *seen_hashes;
  U64 seen_count;
};

typedef struct RAD_CargoState RAD_CargoState;
struct RAD_CargoState
{
  Arena *arena;
  Mutex mutex;

  //- rad: current or last run. `run` is owned by the UI thread; the fields
  // below it are written under `mutex`.
  RAD_CargoRun *run;
  RAD_CargoCmdKind pending_kind;  // starts once the current run is over
  CFG_ID pending_window;
  String8 workspace_root;         // (run arena) diagnostic paths' base
  B32 launched;
  B32 launch_failed;
  B32 stop_requested;
  B32 group_closed;
  B32 done;
  U64 exit_code;
  U64 end_us;

  //- rad: output text: one contiguous block, appended by the runner
  Arena *output_arena;
  U8 *output_base;
  U64 output_size;
  U64 output_gen;
  B32 output_truncated;

  //- rad: output submission (UI thread): the Output tab shows `output_buffer`,
  // which follows the data under `output_key`
  U64 output_submitted_gen;
  U64 output_submit_us;
  C_Root output_root;
  C_Key output_key;
  TB_Buffer *output_buffer;

  //- rad: diagnostics. the runner pushes to the inbox under `mutex`; the UI
  // thread moves them to `diags` (they live in the run's arena)
  RAD_Diagnostic *inbox_first;
  RAD_Diagnostic *inbox_last;
  Arena *diags_arena;
  RAD_Diagnostic **diags;
  U64 diags_count;
  U64 diags_cap;
  U64 error_count;
  U64 warning_count;
  U64 diags_gen;
  U64 diag_cursor;                // 1-based; Next/Previous Error position
};

////////////////////////////////
//~ rad: Globals

global RAD_CargoState *rad_cargo = 0;

////////////////////////////////
//~ rad: Diagnostic Functions

internal String8 rad_diagnostic_kind_name(RAD_DiagnosticKind kind);
internal RAD_DiagnosticKind rad_diagnostic_kind_from_level(String8 level);
internal RAD_Diagnostic *rad_diagnostic_from_cargo_message(Arena *arena, JSON_Value *message, String8 workspace_root);
internal U64 rad_offset_from_line_column(TXT_Patched *patched, S64 line, S64 column);
internal void rad_line_column_from_offset(TXT_Patched *patched, U64 off, S64 *line_out, S64 *column_out);
internal Rng1U64 rad_diagnostic_range_from_document(RAD_Diagnostic *diag, struct RAD_Document *doc, TXT_Patched *patched);
internal B32 rad_location_from_text_line(Arena *arena, String8 text, String8 root, String8 *path_out, S64 *line_out, S64 *column_out);

////////////////////////////////
//~ rad: Cargo Functions

internal void rad_cargo_init(void);
internal String8 rad_cargo_cmd_display_name(RAD_CargoCmdKind kind);
internal RAD_CargoCmdKind rad_cargo_cmd_kind_from_rad_cmd_kind(RAD_CmdKind kind);
internal String8 rad_cargo_folder_from_window(Arena *arena, CFG_Node *window);
internal String8 rad_cargo_workspace_root(Arena *arena);
internal B32 rad_cargo_is_running(void);
internal void rad_cargo_start(RAD_CargoCmdKind kind, CFG_Node *window);
internal void rad_cargo_stop(void);
internal void rad_cargo_clear_output(void);
internal void rad_cargo_output_append(String8 text);
internal void rad_cargo_tick(void);
internal void rad_cargo_shutdown(void);
internal void rad_cargo_thread_entry_point(void *p);

#endif // RAD_CARGO_H
