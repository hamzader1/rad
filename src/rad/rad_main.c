// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

////////////////////////////////
//~ rad: Notes
//
// `rad` is the IDE build target. It is a unity build of only the
// non-debugger layers of the RAD Debugger codebase:
//
//   base -> mdesk -> window_manager, shell -> config
//        -> content, file_stream, artifact_cache -> text, mutable_text
//        -> font_provider -> render -> font_cache -> draw -> ui -> rad
//
// No debugger layer (demon, dbg_engine, eval, dbg_info, rdi, pdb, ...) is
// included. The `rad` layer ports the RAD Debugger's window chrome and box
// rendering on top of `ui`/`draw`, so the UI looks identical.

////////////////////////////////
//~ rad: Build Options

#define BUILD_TITLE "rad"
#define OS_FEATURE_GRAPHICAL 1

#define WM_INIT_MANUAL 1
#define FP_INIT_MANUAL 1
#define R_INIT_MANUAL 1
#define FNT_INIT_MANUAL 1

#define ARENA_TABLE_DEBUG BUILD_DEBUG

////////////////////////////////
//~ rad: Includes

//- rad: [h]
#include "base/base_inc.h"
#include "win32/win32_inc.h"
#include "linux/linux_inc.h"
#include "artifact_cache/artifact_cache.h"
#include "mdesk/mdesk.h"
#include "window_manager/window_manager_inc.h"
#include "shell/shell_inc.h"
#include "config/config_inc.h"
#include "content/content.h"
#include "file_stream/file_stream.h"
#include "text/text.h"
#include "text_buffer/text_buffer.h"
#include "mutable_text/mutable_text.h"
#include "font_provider/font_provider_inc.h"
#include "render/render_inc.h"
#include "font_cache/font_cache.h"
#include "draw/draw.h"
#include "ui/ui_inc.h"
#include "rad/rad_inc.h"

//- rad: [c]
#include "base/base_inc.c"
#include "win32/win32_inc.c"
#include "linux/linux_inc.c"
#include "artifact_cache/artifact_cache.c"
#include "mdesk/mdesk.c"
#include "window_manager/window_manager_inc.c"
#include "shell/shell_inc.c"
#include "config/config_inc.c"
#include "content/content.c"
#include "file_stream/file_stream.c"
#include "text/text.c"
#include "text_buffer/text_buffer.c"
#include "mutable_text/mutable_text.c"
#include "font_provider/font_provider_inc.c"
#include "render/render_inc.c"
#include "font_cache/font_cache.c"
#include "draw/draw.c"
#include "ui/ui_inc.c"
#include "rad/rad_inc.c"

////////////////////////////////
//~ rad: Per-Frame Entry Point

internal B32
frame(void)
{
  rad_frame();
  return rad_state->quit;
}

////////////////////////////////
//~ rad: Entry Point

internal void
entry_point(CmdLine *cmd_line)
{
  Temp scratch = scratch_begin(0, 0);

  //- rad: init log
  g_logs_folder = cmd_line_string(cmd_line, str8_lit("logs"));
  if(g_logs_folder.size == 0)
  {
    String8 user_program_logs_data_path = get_process_info()->user_program_logs_data_path;
    make_directory(push_str8f(scratch.arena, "%S/rad", user_program_logs_data_path));
    g_logs_folder = push_str8f(scratch.arena, "%S/rad/logs", user_program_logs_data_path);
  }
  make_directory(g_logs_folder);

  //- rad: manual layer initialization
  {
    wm_init();
    fp_init();
    r_init(cmd_line);
    fnt_init();
    rad_init(cmd_line);
  }

  //- rad: main application loop
  for(B32 quit = 0; !quit;)
  {
    quit = update();
  }

  scratch_end(scratch);
}
