// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

////////////////////////////////
//~ rad: Notes
//
// `rad` is the IDE build target. It is built from the same layers as the
// RAD Debugger (`raddbg`), so the UI is the debugger's UI, unmodified. This
// file is the IDE's own unity-build entry point; it starts the same layers as
// `raddbg/raddbg_main.c`, but only the normal graphical execution mode - no
// IPC listener, no JIT attach, no command-line binary utility mode.

////////////////////////////////
//~ rad: Build Options

#define BUILD_TITLE "rad"
#define OS_FEATURE_GRAPHICAL 1

#define DMN_INIT_MANUAL 1
#define D_INIT_MANUAL 1
#define WM_INIT_MANUAL 1
#define FP_INIT_MANUAL 1
#define R_INIT_MANUAL 1
#define FNT_INIT_MANUAL 1
#define RD_INIT_MANUAL 1

#define ARENA_TABLE_DEBUG BUILD_DEBUG

////////////////////////////////
//~ rad: Includes

//- rjf: [h]
#include "base/base_inc.h"
#include "x64/x64_inc.h"
#include "win32/win32_inc.h"
#include "linux/linux_inc.h"
#include "linker/base_ext/base_bit_array.h"
#include "artifact_cache/artifact_cache.h"
#include "rdi/rdi_local.h"
#include "rdi_make/rdi_make_local.h"
#include "minidump/minidump.h"
#include "minidump/minidump_parse.h"
#include "mdesk/mdesk.h"
#include "window_manager/window_manager_inc.h"
#include "shell/shell_inc.h"
#include "socket/socket_inc.h"
#include "http/http_inc.h"
#include "symbol_server/symbol_server_inc.h"
#include "config/config_inc.h"
#include "content/content.h"
#include "file_stream/file_stream.h"
#include "text/text.h"
#include "mutable_text/mutable_text.h"
#include "coff/coff.h"
#include "coff/coff_parse.h"
#include "pe/pe.h"
#include "elf/elf.h"
#include "gnu/gnu.h"
#include "gnu/gnu_parse.h"
#include "elf/elf_parse.h"
#include "elf/elf_dump.h"
#include "codeview/codeview.h"
#include "codeview/codeview_parse.h"
#include "msf/msf.h"
#include "msf/msf_parse.h"
#include "pdb/pdb.h"
#include "pdb/pdb_parse.h"
#include "pdb/pdb_stringize.h"
#include "dwarf/dwarf_inc.h"
#include "eh_frame/eh_frame.h"
#include "eh_frame/eh_frame_dump.h"
#include "unwind/unwind_inc.h"
#include "rdi_from_coff/rdi_from_coff.h"
#include "rdi_from_elf/rdi_from_elf.h"
#include "rdi_from_codeview/rdi_from_codeview.h"
#include "rdi_from_pdb/rdi_from_pdb.h"
#include "rdi_from_dwarf/rdi_from_dwarf.h"
#include "radbin/radbin.h"
#include "arch/arch_inc.h"
#include "dbg_info/dbg_info.h"
#include "disasm/disasm_inc.h"
#include "stap/stap_parse.h"
#include "demon/demon_inc.h"
#include "dbg_server/dbg_server.h"
#include "eval/eval_inc.h"
#include "eval2/eval2.h"
#include "dbg_engine/dbg_engine_inc.h"
#include "eval_visualization/eval_visualization_inc.h"
#include "font_provider/font_provider_inc.h"
#include "render/render_inc.h"
#include "font_cache/font_cache.h"
#include "draw/draw.h"
#include "ui/ui_inc.h"
#include "raddbg/raddbg_inc.h"

//- rjf: [c]
#include "base/base_inc.c"
#include "x64/x64.c"
#include "win32/win32_inc.c"
#include "linux/linux_inc.c"
#include "linker/base_ext/base_bit_array.c"
#include "artifact_cache/artifact_cache.c"
#include "rdi/rdi_local.c"
#include "rdi_make/rdi_make_local.c"
#include "minidump/minidump.c"
#include "minidump/minidump_parse.c"
#include "mdesk/mdesk.c"
#include "window_manager/window_manager_inc.c"
#include "shell/shell_inc.c"
#include "socket/socket_inc.c"
#include "http/http_inc.c"
#include "symbol_server/symbol_server_inc.c"
#include "config/config_inc.c"
#include "content/content.c"
#include "file_stream/file_stream.c"
#include "text/text.c"
#include "mutable_text/mutable_text.c"
#include "coff/coff.c"
#include "coff/coff_parse.c"
#include "pe/pe.c"
#include "elf/elf.c"
#include "gnu/gnu.c"
#include "gnu/gnu_parse.c"
#include "elf/elf_parse.c"
#include "elf/elf_dump.c"
#include "codeview/codeview.c"
#include "codeview/codeview_parse.c"
#include "msf/msf.c"
#include "msf/msf_parse.c"
#include "pdb/pdb.c"
#include "pdb/pdb_parse.c"
#include "pdb/pdb_stringize.c"
#include "dwarf/dwarf_inc.c"
#include "eh_frame/eh_frame.c"
#include "eh_frame/eh_frame_dump.c"
#include "unwind/unwind_inc.c"
#include "rdi_from_coff/rdi_from_coff.c"
#include "rdi_from_elf/rdi_from_elf.c"
#include "rdi_from_codeview/rdi_from_codeview.c"
#include "rdi_from_pdb/rdi_from_pdb.c"
#include "rdi_from_dwarf/rdi_from_dwarf.c"
#include "radbin/radbin.c"
#include "arch/arch_inc.c"
#include "dbg_info/dbg_info.c"
#include "disasm/disasm_inc.c"
#include "stap/stap_parse.c"
#include "demon/demon_inc.c"
#include "dbg_server/dbg_server.c"
#include "eval/eval_inc.c"
#include "eval2/eval2.c"
#include "dbg_engine/dbg_engine_inc.c"
#include "eval_visualization/eval_visualization_inc.c"
#include "font_provider/font_provider_inc.c"
#include "render/render_inc.c"
#include "font_cache/font_cache.c"
#include "draw/draw.c"
#include "ui/ui_inc.c"
#include "raddbg/raddbg_inc.c"

////////////////////////////////
//~ rad: Ctrl -> Main Thread Wakeup Hook

internal D_WAKEUP_FUNCTION_DEF(rad_wakeup_hook_ctrl)
{
  wm_send_wakeup_event();
}

////////////////////////////////
//~ rad: Per-Frame Entry Point

internal B32
frame(void)
{
  rd_frame();
  return rd_state->quit;
}

////////////////////////////////
//~ rad: Entry Point

internal void
entry_point(CmdLine *cmd_line)
{
  Temp scratch = scratch_begin(0, 0);
  
  //- rad: init log
  g_logs_folder = cmd_line_string(cmd_line, s("logs"));
  if(g_logs_folder.size == 0)
  {
    String8 user_program_logs_data_path = get_process_info()->user_program_logs_data_path;
    g_logs_folder = push_str8f(scratch.arena, "%S/rad/logs", user_program_logs_data_path);
  }
  make_directory(g_logs_folder);
  
  //- rad: windows -> turn off output handles, as the debugger layers need to
  // control those for target processes (same as raddbg)
#if OS_WINDOWS
  {
    HANDLE output_handles[3] =
    {
      GetStdHandle(STD_INPUT_HANDLE),
      GetStdHandle(STD_OUTPUT_HANDLE),
      GetStdHandle(STD_ERROR_HANDLE),
    };
    for(U64 idx = 0; idx < ArrayCount(output_handles); idx += 1)
    {
      B32 duplicate = 0;
      for(U64 idx2 = 0; idx2 < idx; idx2 += 1)
      {
        if(output_handles[idx2] == output_handles[idx])
        {
          duplicate = 1;
          break;
        }
      }
      if(duplicate)
      {
        output_handles[idx] = 0;
      }
    }
    for(U64 idx = 0; idx < ArrayCount(output_handles); idx += 1)
    {
      if(output_handles[idx] != 0)
      {
        CloseHandle(output_handles[idx]);
      }
    }
    SetStdHandle(STD_INPUT_HANDLE, 0);
    SetStdHandle(STD_OUTPUT_HANDLE, 0);
    SetStdHandle(STD_ERROR_HANDLE, 0);
  }
#endif
  
  //- rad: manual layer initialization
  {
    dmn_init();
    d_init();
    wm_init();
    fp_init();
    r_init(cmd_line);
    fnt_init();
    rd_init(cmd_line);
    d_set_wakeup_hook(rad_wakeup_hook_ctrl);
  }
  
  //- rad: main application loop
  for(B32 quit = 0; !quit;)
  {
    quit = update();
  }
  
  scratch_end(scratch);
}
