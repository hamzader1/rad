// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

#ifndef SHELL_H
#define SHELL_H

////////////////////////////////
//~ rjf: @per_os_impl Shell Operations

internal void sh_message(B32 error, String8 title, String8 message);
internal String8 sh_pick_file(Arena *arena, String8 title, String8 initial_path);
internal String8 sh_pick_folder(Arena *arena, String8 title, String8 initial_path);
internal void sh_show_in_file_browser(String8 path);
internal void sh_open_in_browser(String8 url);
internal B32 sh_install_or_uninstall_self(B32 write, B32 install);

////////////////////////////////
//~ rad: Terminal Launching
//
// Runs a command line in a new terminal window, like Visual Studio's
// console for a program it starts: the window shows the command's output,
// takes its input (the command has a real console: line editors, colors and
// Ctrl+C work), and stays open after the command ends, until a key is
// pressed ("Press any key to close this window . . ."). The window belongs
// to the OS's terminal app: on Windows the default terminal (Windows
// Terminal on Windows 11, else the console host); on macOS, Terminal.app.
//
// The returned process stands for the window: it ends when the window's
// command has ended and the key was pressed (or the window was closed), with
// the command's exit code. Closing its process group ends the command, its
// children and the window (Stop). To know when the command itself has ended
// (while its window still waits for the key), give `done_path`: the
// command's exit code is written to that file (decimal, then a line end)
// right when it ends.
//
// `hidden` (scripted tests): no window; the command's input is empty and
// its output (stdout and stderr) goes to `output_file`. Same wrapper, so
// the exit code and the closing message behave as with a window.
//
// NOTE(rad): on Windows the command line goes through cmd.exe: arguments
// and `done_path` must not contain `"`, `%` or `!`.

typedef struct SH_TerminalLaunchParams SH_TerminalLaunchParams;
struct SH_TerminalLaunchParams
{
  String8List cmd_line;
  String8 path;
  String8 title;
  ProcessGroup process_group;
  String8 done_path;
  B32 hidden;
  File output_file;
};

internal Process sh_launch_in_terminal(SH_TerminalLaunchParams *params);

#endif // SHELL_H
