// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

////////////////////////////////
//~ rad: Helpers

internal NSString *
mac_sh_nsstring_from_str8(String8 string)
{
  NSString *result = [[[NSString alloc] initWithBytes:string.str length:string.size encoding:NSUTF8StringEncoding] autorelease];
  return result ? result : @"";
}

internal String8
mac_sh_pick(Arena *arena, String8 title, String8 initial_path, B32 folders)
{
  String8 result = {0};
  @autoreleasepool
  {
    NSOpenPanel *panel = [NSOpenPanel openPanel];
    panel.canChooseFiles          = !folders;
    panel.canChooseDirectories    = !!folders;
    panel.allowsMultipleSelection = NO;
    panel.message                 = mac_sh_nsstring_from_str8(title);
    if(initial_path.size != 0)
    {
      panel.directoryURL = [NSURL fileURLWithPath:mac_sh_nsstring_from_str8(initial_path) isDirectory:YES];
    }
    if([panel runModal] == NSModalResponseOK)
    {
      const char *path = panel.URLs.firstObject.path.UTF8String;
      if(path != 0)
      {
        result = str8_copy(arena, str8_cstring((char *)path));
      }
    }
  }
  return result;
}

////////////////////////////////
//~ rad: @per_os_impl Shell Operations

internal void
sh_message(B32 error, String8 title, String8 message)
{
  @autoreleasepool
  {
    NSAlert *alert = [[[NSAlert alloc] init] autorelease];
    alert.alertStyle      = error ? NSAlertStyleCritical : NSAlertStyleInformational;
    alert.messageText     = mac_sh_nsstring_from_str8(title);
    alert.informativeText = mac_sh_nsstring_from_str8(message);
    [alert runModal];
  }
}

internal String8
sh_pick_file(Arena *arena, String8 title, String8 initial_path)
{
  return mac_sh_pick(arena, title, initial_path, 0);
}

internal String8
sh_pick_folder(Arena *arena, String8 title, String8 initial_path)
{
  return mac_sh_pick(arena, title, initial_path, 1);
}

internal void
sh_show_in_file_browser(String8 path)
{
  @autoreleasepool
  {
    NSURL *url = [NSURL fileURLWithPath:mac_sh_nsstring_from_str8(path)];
    [[NSWorkspace sharedWorkspace] activateFileViewerSelectingURLs:@[url]];
  }
}

internal void
sh_open_in_browser(String8 url)
{
  @autoreleasepool
  {
    NSURL *ns_url = [NSURL URLWithString:mac_sh_nsstring_from_str8(url)];
    if(ns_url == nil || ![[NSWorkspace sharedWorkspace] openURL:ns_url])
    {
      Temp scratch = scratch_begin(0, 0);
      sh_message(1, str8_lit("Error"), str8f(scratch.arena, "Could not open %S in a browser.", url));
      scratch_end(scratch);
    }
  }
}

internal B32
sh_install_or_uninstall_self(B32 write, B32 install)
{
  // TODO(rad): macOS apps install as an .app bundle (milestone 12, later);
  // nothing to register here yet.
  return 0;
}

////////////////////////////////
//~ rad: @per_os_impl Terminal Launching

internal String8
mac_sh_quoted(Arena *arena, String8 string)
{
  // rad: one POSIX shell word: '...' with ' written as '\''
  String8List parts = {0};
  str8_list_push(arena, &parts, str8_lit("'"));
  U64 start = 0;
  for(U64 idx = 0; idx <= string.size; idx += 1)
  {
    if(idx == string.size || string.str[idx] == '\'')
    {
      str8_list_push(arena, &parts, str8_substr(string, r1u64(start, idx)));
      if(idx < string.size) { str8_list_push(arena, &parts, str8_lit("'\\''")); }
      start = idx+1;
    }
  }
  str8_list_push(arena, &parts, str8_lit("'"));
  return str8_list_join(arena, &parts, 0);
}

internal Process
sh_launch_in_terminal(SH_TerminalLaunchParams *params)
{
  // NOTE(rad): macOS version of win32's console wrapper. Files next to the
  // done file (<base>.exit): <base>.command (the script Terminal.app runs),
  // <base>.pid (the script's pid), <base>.watcher (the watcher's pid).
  //
  //   rad -> watcher (/bin/sh, our child, in params->process_group)
  //            opens <base>.command in a new Terminal.app window, then lives
  //            until the script ends: rad sees "window ended" when it exits.
  //   Terminal.app -> /bin/sh <base>.command
  //            title, cd, the command, exit code -> done file, "[process
  //            exited with code N]", waits for a key. A guardian subshell
  //            kills the script's process group when the watcher dies, so
  //            Stop (process_group_close kills the watcher's group) also
  //            ends a program running in Terminal.app.
  //
  // Hidden (script mode): /bin/sh runs the command directly with output to
  // params->output_file, and writes the done file.
  Process result = {0};
  Temp scratch = scratch_begin(0, 0);

  //- rad: command line as shell words
  String8List words = {0};
  for(String8Node *n = params->cmd_line.first; n != 0; n = n->next)
  {
    str8_list_push(scratch.arena, &words, mac_sh_quoted(scratch.arena, n->string));
  }
  String8 cmd = str8_list_join(scratch.arena, &words, &(StringJoin){.sep = str8_lit(" ")});

  //- rad: file names
  String8 done_path = params->done_path;
  if(done_path.size == 0)
  {
    done_path = push_str8f(scratch.arena, "%S/raid_run_%u_%I64u.exit", get_process_info()->user_program_cache_data_path, get_process_info()->pid, now_time_us());
  }
  String8 base = str8_chop_last_dot(done_path);
  String8 script_path  = push_str8f(scratch.arena, "%S.command", base);
  String8 pid_path     = push_str8f(scratch.arena, "%S.pid", base);
  String8 watcher_path = push_str8f(scratch.arena, "%S.watcher", base);
  String8 dir = params->path.size != 0 ? params->path : get_current_path(scratch.arena);

  //- rad: title, minus control characters
  String8 title = push_str8_copy(scratch.arena, params->title.size != 0 ? params->title : str8_lit(BUILD_TITLE));
  for EachIndex(idx, title.size) { if(title.str[idx] < 32) { title.str[idx] = '_'; } }

  if(params->hidden)
  {
    //- rad: hidden: run directly, output to the given file
    String8 script = push_str8f(scratch.arena,
                                "cd %S || exit 1\n"
                                "%S\n"
                                "code=$?\n"
                                "echo $code > %S\n"
                                "echo\n"
                                "echo \"[process exited with code $code]\"\n"
                                "printf 'Press any key to close this window . . .'\n"
                                "exit $code\n",
                                mac_sh_quoted(scratch.arena, dir), cmd, mac_sh_quoted(scratch.arena, done_path));
    File null_file = file_open(AccessFlag_Read, str8_lit("/dev/null"));
    ProcessLaunchParams launch = {0};
    str8_list_push(scratch.arena, &launch.cmd_line, str8_lit("/bin/sh"));
    str8_list_push(scratch.arena, &launch.cmd_line, str8_lit("-c"));
    str8_list_push(scratch.arena, &launch.cmd_line, script);
    launch.path = dir;
    launch.inherit_env = 1;
    launch.process_group = params->process_group;
    launch.stdout_file = params->output_file;
    launch.stderr_file = params->output_file;
    launch.stdin_file = null_file;
    result = process_launch(&launch);
    file_close(null_file);
  }
  else
  {
    //- rad: the script Terminal.app runs
    String8 script = push_str8f(scratch.arena,
                                "printf '\\033]0;%%s\\007' %S\n"
                                "echo $$ > %S\n"
                                "trap ':' INT\n"
                                "W=$(cat %S 2>/dev/null)\n"
                                "( trap ':' INT; while kill -0 \"$W\" 2>/dev/null; do sleep 0.2; done; kill -KILL -$$ ) 2>/dev/null &\n"
                                "G=$!\n"
                                "cd %S || exit 1\n"
                                "%S\n"
                                "code=$?\n"
                                "kill $G 2>/dev/null\n"
                                "echo $code > %S\n"
                                "rm -f %S %S\n"
                                "echo\n"
                                "echo \"[process exited with code $code]\"\n"
                                "printf 'Press any key to close this window . . .'\n"
                                "stty -icanon -echo 2>/dev/null; dd bs=1 count=1 >/dev/null 2>&1; stty icanon echo 2>/dev/null\n"
                                "echo\n"
                                "exit $code\n",
                                mac_sh_quoted(scratch.arena, title),
                                mac_sh_quoted(scratch.arena, pid_path),
                                mac_sh_quoted(scratch.arena, watcher_path),
                                mac_sh_quoted(scratch.arena, dir),
                                cmd,
                                mac_sh_quoted(scratch.arena, done_path),
                                mac_sh_quoted(scratch.arena, script_path),
                                mac_sh_quoted(scratch.arena, watcher_path));
    delete_file_at_path(pid_path);
    write_data_to_file_path(script_path, script);

    //- rad: the AppleScript that opens it (an AppleScript string: escape \ and ")
    String8 run_line = push_str8f(scratch.arena, "exec /bin/sh %S", mac_sh_quoted(scratch.arena, script_path));
    String8List escaped = {0};
    for EachIndex(idx, run_line.size)
    {
      U8 c = run_line.str[idx];
      if(c == '\\' || c == '"') { str8_list_push(scratch.arena, &escaped, str8_lit("\\")); }
      str8_list_push(scratch.arena, &escaped, str8_substr(run_line, r1u64(idx, idx+1)));
    }
    String8 apple_script = push_str8f(scratch.arena,
                                      "tell application \"Terminal\"\n"
                                      "  activate\n"
                                      "  do script \"%S\"\n"
                                      "end tell\n",
                                      str8_list_join(scratch.arena, &escaped, 0));

    //- rad: the watcher (our child, in the group)
    String8 watcher = push_str8f(scratch.arena,
                                 "echo $$ > %S\n"
                                 "/usr/bin/osascript -e %S >/dev/null || exit 1\n"
                                 "i=0; while [ ! -s %S ] && [ $i -lt 300 ]; do sleep 0.1; i=$((i+1)); done\n"
                                 "p=$(cat %S 2>/dev/null); [ -n \"$p\" ] || exit 1\n"
                                 "while kill -0 \"$p\" 2>/dev/null; do sleep 0.2; done\n"
                                 "rm -f %S\n",
                                 mac_sh_quoted(scratch.arena, watcher_path),
                                 mac_sh_quoted(scratch.arena, apple_script),
                                 mac_sh_quoted(scratch.arena, pid_path),
                                 mac_sh_quoted(scratch.arena, pid_path),
                                 mac_sh_quoted(scratch.arena, pid_path));
    File null_file = file_open(AccessFlag_Read, str8_lit("/dev/null"));
    ProcessLaunchParams launch = {0};
    str8_list_push(scratch.arena, &launch.cmd_line, str8_lit("/bin/sh"));
    str8_list_push(scratch.arena, &launch.cmd_line, str8_lit("-c"));
    str8_list_push(scratch.arena, &launch.cmd_line, watcher);
    launch.path = dir;
    launch.inherit_env = 1;
    launch.process_group = params->process_group;
    launch.stdin_file = null_file;
    result = process_launch(&launch);
    file_close(null_file);
  }

  scratch_end(scratch);
  return result;
}
