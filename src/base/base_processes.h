// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

#ifndef BASE_PROCESSES_H
#define BASE_PROCESSES_H

typedef struct ProcessInfo ProcessInfo;
struct ProcessInfo
{
  U32 pid;
  B32 large_pages_allowed;
  String8 binary_file_path;
  String8 binary_path;
  String8 initial_path;
  String8 user_program_config_data_path;
  String8 user_program_cache_data_path;
  String8 user_program_logs_data_path;
  String8List module_load_paths;
  String8List environment;
};

typedef struct Process Process;
struct Process
{
  U64 u64[1];
};

typedef struct ProcessGroup ProcessGroup;
struct ProcessGroup
{
  U64 u64[1];
};

// rad: not in raddbg: a process on a pseudo-terminal (the integrated
// terminal): u64[0..1] on POSIX: master fd, pid; on Windows: the pseudo
// console, its input write end, its output read end, the process
typedef struct Pty Pty;
struct Pty
{
  U64 u64[4];
};

typedef struct PtyLaunchParams PtyLaunchParams;
struct PtyLaunchParams
{
  String8List cmd_line;
  String8 path;
  String8List env; // "NAME=value", over the inherited environment
  U32 cols;
  U32 rows;
};

typedef struct ProcessNode ProcessNode;
struct ProcessNode
{
  ProcessNode *next;
  Process v;
};

typedef struct ProcessList ProcessList;
struct ProcessList
{
  ProcessNode *first;
  ProcessNode *last;
  U64 count;
};

typedef struct ProcessLaunchParams ProcessLaunchParams;
struct ProcessLaunchParams
{
  String8List cmd_line;
  String8 path;
  String8List env;
  B32 inherit_env;
  B32 debug_subprocesses;
  B32 consoleless;
  B32 new_console;
  ProcessGroup process_group;
  File stdout_file;
  File stderr_file;
  File stdin_file;
};

////////////////////////////////
//~ rjf: Handle Type Functions

internal Process process_zero(void);
internal B32 process_match(Process a, Process b);
internal void process_list_push(Arena *arena, ProcessList *list, Process p);

////////////////////////////////
//~ rjf: Process Launcher Helpers

internal Process launch_cmd_line(String8 string);
internal Process launch_cmd_linef(char *fmt, ...);

////////////////////////////////
//~ rjf: @per_os_impl Aborting

internal void abort_self(U64 exit_code);

////////////////////////////////
//~ rjf: @per_os_impl Process Info

internal ProcessInfo *get_process_info(void);
internal String8 get_current_path(Arena *arena);
internal U32 get_process_start_time_unix(void);

////////////////////////////////
//~ rjf: @per_os_impl Child Processes

internal Process process_launch(ProcessLaunchParams *params);
internal U64 pid_from_process(Process process);
internal B32 process_poll(Process process, U64 *exit_code_out);
internal B32 process_is_active(Process process);
internal B32 process_join(Process process, U64 endt_us, U64 *exit_code_out);
internal void process_detach(Process process);
internal B32 process_kill(Process process);
internal B32 process_send_ctrl_c(Process process);

////////////////////////////////
//~ rad: @per_os_impl Pseudo-Terminals (not in raddbg)
//
// pty_read waits until there is output, the terminal's end (the process and
// every process sharing the terminal exited: *is_end_out), or endt_us;
// pty_write never blocks (it returns what was taken); pty_close ends the
// process (hang-up, then kill) and frees the terminal: no pty_read may be
// running on another thread then.

internal Pty pty_launch(PtyLaunchParams *params);
internal B32 pty_is_zero(Pty pty);
internal U64 pty_read(Pty pty, void *out_data, U64 size, U64 endt_us, B32 *is_end_out);
internal U64 pty_write(Pty pty, void *data, U64 size);
internal void pty_resize(Pty pty, U32 cols, U32 rows);
internal void pty_close(Pty pty);

internal ProcessGroup process_group_make(B32 kill_on_close);
internal B32 process_group_add(ProcessGroup group, Process process);
internal void process_group_close(ProcessGroup group);

#endif // BASE_PROCESSES_H
