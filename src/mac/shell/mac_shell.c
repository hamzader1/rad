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

internal Process
sh_launch_in_terminal(SH_TerminalLaunchParams *params)
{
  // TODO(rad): Terminal.app running win32's wrapper (exit code + done file,
  // Stop support). Not done yet; Cargo's "Run in terminal" is unavailable.
  Process result = {0};
  return result;
}
