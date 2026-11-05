// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

////////////////////////////////
//~ rad: Diagnostic Functions

internal String8
rad_diagnostic_kind_name(RAD_DiagnosticKind kind)
{
  String8 result = str8_lit("error");
  switch(kind)
  {
    default:{}break;
    case RAD_DiagnosticKind_Warning: {result = str8_lit("warning");}break;
    case RAD_DiagnosticKind_Note:    {result = str8_lit("note");}break;
    case RAD_DiagnosticKind_Help:    {result = str8_lit("help");}break;
  }
  return result;
}

internal RAD_DiagnosticKind
rad_diagnostic_kind_from_level(String8 level)
{
  // NOTE(rad): rustc levels: "error", "error: internal compiler error",
  // "warning", "note", "help", "failure-note"
  RAD_DiagnosticKind result = RAD_DiagnosticKind_Error;
  if(str8_match(level, str8_lit("warning"), 0))
  {
    result = RAD_DiagnosticKind_Warning;
  }
  else if(str8_match(level, str8_lit("note"), 0) || str8_match(level, str8_lit("failure-note"), 0))
  {
    result = RAD_DiagnosticKind_Note;
  }
  else if(str8_match(level, str8_lit("help"), 0))
  {
    result = RAD_DiagnosticKind_Help;
  }
  return result;
}

internal RAD_Diagnostic *
rad_diagnostic_from_cargo_message(Arena *arena, JSON_Value *message, String8 workspace_root)
{
  // NOTE(rad): `message` is the "message" object of a compiler-message (the
  // rustc JSON diagnostic). Messages without a primary span ("aborting due
  // to 2 previous errors", "N warnings emitted") give no diagnostic.
  RAD_Diagnostic *diag = 0;
  Temp scratch = scratch_begin(&arena, 1);

  //- rad: primary span
  JSON_Value *span = &json_value_nil;
  JSON_Value *spans = json_value_from_key(message, str8_lit("spans"));
  for(JSON_Value *s = spans->first; s != &json_value_nil; s = s->next)
  {
    if(json_b32_from_value(json_value_from_key(s, str8_lit("is_primary"))))
    {
      span = s;
      break;
    }
  }

  //- rad: span inside a macro defined elsewhere ("<std macros>", ...) -> its
  // call site
  String8 file_name = json_string_from_value(json_value_from_key(span, str8_lit("file_name")));
  for(U64 depth = 0; depth < 64 && !json_value_is_nil(span) && (file_name.size == 0 || file_name.str[0] == '<'); depth += 1)
  {
    span = json_value_from_key(json_value_from_key(span, str8_lit("expansion")), str8_lit("span"));
    file_name = json_string_from_value(json_value_from_key(span, str8_lit("file_name")));
  }

  //- rad: fill
  if(!json_value_is_nil(span) && file_name.size != 0 && file_name.str[0] != '<')
  {
    String8 path = path_absolute_dst_from_relative_dst_src(scratch.arena, file_name, workspace_root);
    diag = push_array(arena, RAD_Diagnostic, 1);
    diag->kind       = rad_diagnostic_kind_from_level(json_string_from_value(json_value_from_key(message, str8_lit("level"))));
    diag->path       = path_normalized_from_string(arena, path);
    diag->line       = json_s64_from_value(json_value_from_key(span, str8_lit("line_start")));
    diag->column     = json_s64_from_value(json_value_from_key(span, str8_lit("column_start")));
    diag->end_line   = json_s64_from_value(json_value_from_key(span, str8_lit("line_end")));
    diag->end_column = json_s64_from_value(json_value_from_key(span, str8_lit("column_end")));
    diag->code       = push_str8_copy(arena, json_string_from_value(json_value_from_key(json_value_from_key(message, str8_lit("code")), str8_lit("code"))));
    diag->message    = push_str8_copy(arena, json_string_from_value(json_value_from_key(message, str8_lit("message"))));
    diag->label      = push_str8_copy(arena, json_string_from_value(json_value_from_key(span, str8_lit("label"))));
    diag->rendered   = push_str8_copy(arena, json_string_from_value(json_value_from_key(message, str8_lit("rendered"))));
    diag->line       = Max(diag->line, 1);
    diag->column     = Max(diag->column, 1);
    if(diag->end_line < diag->line || (diag->end_line == diag->line && diag->end_column < diag->column))
    {
      diag->end_line = diag->line;
      diag->end_column = diag->column;
    }
  }

  scratch_end(scratch);
  return diag;
}

internal U64
rad_offset_from_line_column(TXT_Patched *patched, S64 line, S64 column, B32 column_in_bytes)
{
  // NOTE(rad): 1-based line, 1-based column in characters (UTF-8 decoded),
  // or in bytes; clamped to the line
  Temp scratch = scratch_begin(0, 0);
  S64 line_count = (S64)patched->line_map.total_line_count;
  line = Clamp(1, line, Max(line_count, 1));
  Rng1U64 range = txt_range_from_line_num(&patched->line_map, (U64)line);
  String8 text = memory_map_data_from_range(scratch.arena, &patched->memory_map, range);
  U64 off = 0;
  if(column_in_bytes)
  {
    off = (U64)Max(column - 1, 0);
  }
  for(S64 c = 1; !column_in_bytes && c < column && off < text.size; c += 1)
  {
    UnicodeDecode decode = utf8_decode(text.str + off, text.size - off);
    off += Max(decode.inc, 1);
  }
  scratch_end(scratch);
  return range.min + Min(off, text.size);
}

internal Rng1U64
rad_diagnostic_range_from_document(RAD_Diagnostic *diag, RAD_Document *doc, TXT_Patched *patched)
{
  // NOTE(rad): the first time a diagnostic meets its document, its line and
  // column become offsets in the document's text; after that the offsets
  // follow edits through the buffer's change log. A reload or a new document
  // (the file was closed and opened again) starts over from line/column.
  TB_Buffer *buffer = doc->buffer;
  B32 mapped = 0;
  if(diag->doc_id == doc->id)
  {
    U64 min = diag->doc_range.min;
    U64 max = diag->doc_range.max;
    if(tb_buffer_map_offset(buffer, diag->doc_version, &min) &&
       tb_buffer_map_offset(buffer, diag->doc_version, &max))
    {
      diag->doc_range = r1u64(Min(min, patched->size), Min(Max(min, max), patched->size));
      diag->doc_version = buffer->version;
      mapped = 1;
    }
  }
  if(!mapped)
  {
    U64 min = rad_offset_from_line_column(patched, diag->line, diag->column, diag->column_in_bytes);
    U64 max = rad_offset_from_line_column(patched, diag->end_line, diag->end_column, diag->column_in_bytes);
    diag->doc_id = doc->id;
    diag->doc_version = buffer->version;
    diag->doc_range = r1u64(min, Max(min, max));
  }
  return diag->doc_range;
}

internal B32
rad_location_from_text_line(Arena *arena, String8 text, String8 root, String8 *path_out, S64 *line_out, S64 *column_out)
{
  // NOTE(rad): the first `<path>:<line>[:<column>]` in a line of output whose
  // path is an existing file, relative paths taken from `root`:
  //   " --> src\main.rs:2:20", "panicked at src/main.rs:5:9:", "C:/x/y.rs:3"
  B32 result = 0;
  Temp scratch = scratch_begin(&arena, 1);
  for(U64 colon = 1; !result && colon + 1 < text.size; colon += 1)
  {
    if(text.str[colon] != ':' || !char_is_digit(text.str[colon+1], 10))
    {
      continue;
    }

    //- rad: line, column
    U64 idx = colon+1;
    S64 line = 0;
    for(;idx < text.size && char_is_digit(text.str[idx], 10) && line < 100000000; idx += 1)
    {
      line = line*10 + (text.str[idx] - '0');
    }
    S64 column = 1;
    if(idx + 1 < text.size && text.str[idx] == ':' && char_is_digit(text.str[idx+1], 10))
    {
      column = 0;
      for(idx += 1; idx < text.size && char_is_digit(text.str[idx], 10) && column < 100000000; idx += 1)
      {
        column = column*10 + (text.str[idx] - '0');
      }
    }

    //- rad: path: back to whitespace, a quote, or a bracket
    U64 start = colon;
    for(;start > 0; start -= 1)
    {
      U8 c = text.str[start-1];
      if(c == ' ' || c == '\t' || c == '"' || c == '\'' || c == '`' || c == '(' || c == '[' || c == '<' || c == '=' || c == ',')
      {
        break;
      }
    }
    String8 path = str8_substr(text, r1u64(start, colon));
    if(path.size == 0 || line < 1)
    {
      continue;
    }
    String8 full_path = path_normalized_from_string(scratch.arena, path_absolute_dst_from_relative_dst_src(scratch.arena, path, root));
    FileProperties props = properties_from_file_path(full_path);
    if(props.modified != 0 && !(props.flags & FilePropertyFlag_IsFolder))
    {
      result = 1;
      *path_out = push_str8_copy(arena, full_path);
      *line_out = line;
      *column_out = Max(column, 1);
    }
  }
  scratch_end(scratch);
  return result;
}

////////////////////////////////
//~ rad: Cargo Helpers

internal String8
rad_cargo_cmd_display_name(RAD_CargoCmdKind kind)
{
  String8 result = {0};
  switch(kind)
  {
    default:{}break;
    case RAD_CargoCmdKind_Build: {result = str8_lit("cargo build");}break;
    case RAD_CargoCmdKind_Check: {result = str8_lit("cargo check");}break;
    case RAD_CargoCmdKind_Run:   {result = str8_lit("cargo run");}break;
    case RAD_CargoCmdKind_Test:  {result = str8_lit("cargo test");}break;
  }
  return result;
}

internal RAD_CargoCmdKind
rad_cargo_cmd_kind_from_rad_cmd_kind(RAD_CmdKind kind)
{
  RAD_CargoCmdKind result = RAD_CargoCmdKind_Null;
  switch(kind)
  {
    default:{}break;
    case RAD_CmdKind_CargoBuild: {result = RAD_CargoCmdKind_Build;}break;
    case RAD_CmdKind_CargoCheck: {result = RAD_CargoCmdKind_Check;}break;
    case RAD_CmdKind_CargoRun:   {result = RAD_CargoCmdKind_Run;}break;
    case RAD_CmdKind_CargoTest:  {result = RAD_CargoCmdKind_Test;}break;
  }
  return result;
}

internal B32
rad_cargo_folder_has_manifest__(String8 folder)
{
  Temp scratch = scratch_begin(0, 0);
  FileProperties props = properties_from_file_path(push_str8f(scratch.arena, "%S/Cargo.toml", folder));
  B32 result = (props.modified != 0 && !(props.flags & FilePropertyFlag_IsFolder));
  scratch_end(scratch);
  return result;
}

internal String8
rad_cargo_manifest_folder_above__(String8 folder)
{
  // NOTE(rad): `folder` or the nearest folder above it with a Cargo.toml
  String8 result = {0};
  for(String8 dir = folder; dir.size != 0;)
  {
    if(rad_cargo_folder_has_manifest__(dir))
    {
      result = dir;
      break;
    }
    String8 parent = str8_chop_last_slash(dir);
    if(parent.size >= dir.size)
    {
      break;
    }
    dir = parent;
  }
  return result;
}

internal String8
rad_cargo_folder_from_window(Arena *arena, CFG_Node *window)
{
  // NOTE(rad): where cargo runs: the explorer's folder if it has a
  // Cargo.toml; else the nearest folder with one above the editor's file;
  // else the nearest one above the explorer's folder; else none
  Temp scratch = scratch_begin(&arena, 1);
  String8 result = {0};
  String8 explorer_folder = {0};
  String8 file_folder = {0};
  CFG_PanelTree tree = cfg_panel_tree_from_cfg(scratch.arena, window);
  for(CFG_PanelNode *p = tree.root; p != &cfg_nil_panel_node; p = cfg_panel_node_rec__depth_first_pre(tree.root, p).next)
  {
    for(CFG_NodePtrNode *n = p->tabs.first; n != 0; n = n->next)
    {
      if(explorer_folder.size == 0 && str8_match(n->v->string, str8_lit("explorer"), 0))
      {
        explorer_folder = rad_explorer_folder_from_tab(n->v);
      }
    }
  }
  {
    CFG_PanelNode *editor_panel = cfg_panel_node_from_tree_cfg(tree.root, cfg_node_from_id(rad_state->last_editor_panel));
    String8 file_path = rad_file_path_from_tab(tree.focused->selected_tab);
    if(file_path.size == 0)
    {
      file_path = rad_file_path_from_tab(editor_panel->selected_tab);
    }
    if(file_path.size != 0)
    {
      file_folder = str8_chop_last_slash(file_path);
    }
  }
  if(explorer_folder.size != 0 && rad_cargo_folder_has_manifest__(explorer_folder))
  {
    result = explorer_folder;
  }
  if(result.size == 0 && file_folder.size != 0)
  {
    result = rad_cargo_manifest_folder_above__(file_folder);
  }
  if(result.size == 0 && explorer_folder.size != 0)
  {
    result = rad_cargo_manifest_folder_above__(explorer_folder);
  }
  result = push_str8_copy(arena, result);
  scratch_end(scratch);
  return result;
}

////////////////////////////////
//~ rad: Cargo Output

internal void
rad_cargo_output_append__locked(String8 text)
{
  RAD_CargoState *c = rad_cargo;
  if(text.size != 0 && !c->output_truncated)
  {
    if(c->output_size + text.size > RAD_CARGO_OUTPUT_MAX_SIZE)
    {
      String8 note = str8_lit("\n[output truncated: more than 32 MB]\n");
      text = note;
      c->output_truncated = 1;
    }
    U8 *dst = (U8 *)arena_push(c->output_arena, text.size, 1, 0);
    if(c->output_base == 0)
    {
      c->output_base = dst;
    }
    MemoryCopy(dst, text.str, text.size);
    c->output_size += text.size;
    c->output_gen += 1;
  }
}

internal void
rad_cargo_output_append(String8 text)
{
  MutexScope(rad_cargo->mutex)
  {
    rad_cargo_output_append__locked(text);
  }
}

internal void
rad_cargo_clear_output(void)
{
  RAD_CargoState *c = rad_cargo;
  MutexScope(c->mutex)
  {
    arena_clear(c->output_arena);
    c->output_base = 0;
    c->output_size = 0;
    c->output_truncated = 0;
    c->output_gen += 1;
  }
  rad_request_frame();
}

////////////////////////////////
//~ rad: Cargo Runner Thread

internal String8
rad_cargo_capture_stdout__(Arena *arena, String8List *cmd_line, String8 folder)
{
  // NOTE(rad): runs a short command to completion; returns its stdout
  String8List parts = {0};
  FilePair pipe = file_pipe_make(0, 1);
  ProcessLaunchParams params = {0};
  params.cmd_line = *cmd_line;
  params.path = folder;
  params.inherit_env = 1;
  params.consoleless = 1;
  params.stdout_file = pipe.write;
  Process process = process_launch(&params);
  file_close(pipe.write);
  if(!process_match(process, process_zero()))
  {
    U8 buffer[4096];
    for(U64 size = 0; (size = file_pipe_read(pipe.read, buffer, sizeof(buffer))) != 0;)
    {
      str8_list_push(arena, &parts, push_str8_copy(arena, str8(buffer, size)));
    }
    process_join(process, max_U64, 0);
  }
  file_close(pipe.read);
  return str8_list_join(arena, &parts, 0);
}

internal void
rad_cargo_process_line__(Arena *arena, RAD_CargoRun *run, String8 workspace_root, String8 line, B32 parse_messages, String8List *out, RAD_Diagnostic **first_diag, RAD_Diagnostic **last_diag)
{
  // NOTE(rad): output pieces go into `arena` (they outlive `line`);
  // diagnostics into the run's arena. `parse_messages` = 0: the program's
  // own output (no cargo messages in it)
  if(line.size != 0 && line.str[line.size-1] == '\r')
  {
    line.size -= 1;
  }
  B32 handled = 0;
  if(parse_messages && line.size != 0 && line.str[0] == '{')
  {
    Temp scratch = scratch_begin(&arena, 1);
    JSON_ParseResult parsed = json_parse(scratch.arena, line);
    String8 reason = json_string_from_value(json_value_from_key(parsed.root, str8_lit("reason")));
    if(parsed.ok && reason.size != 0)
    {
      handled = 1;
      if(str8_match(reason, str8_lit("compiler-message"), 0))
      {
        JSON_Value *message = json_value_from_key(parsed.root, str8_lit("message"));
        String8 rendered = json_string_from_value(json_value_from_key(message, str8_lit("rendered")));
        str8_list_push(arena, out, push_str8_copy(arena, rendered));
        RAD_Diagnostic *diag = rad_diagnostic_from_cargo_message(run->arena, message, workspace_root);

        //- rad: drop repeats (`cargo test` reports a library's warnings for
        // both the library and its test build)
        if(diag != 0)
        {
          String8 key = push_str8f(scratch.arena, "%i|%S|%I64d|%I64d|%S", diag->kind, diag->path, diag->line, diag->column, diag->message);
          U64 hash = u64_hash_from_str8(key);
          B32 seen = (run->seen_count >= RAD_CARGO_MAX_DIAGNOSTICS);
          for(U64 idx = 0; !seen && idx < run->seen_count; idx += 1)
          {
            seen = (run->seen_hashes[idx] == hash);
          }
          if(!seen)
          {
            if(run->seen_hashes == 0)
            {
              run->seen_hashes = push_array(run->arena, U64, RAD_CARGO_MAX_DIAGNOSTICS);
            }
            run->seen_hashes[run->seen_count] = hash;
            run->seen_count += 1;
            SLLQueuePush(*first_diag, *last_diag, diag);
          }
        }
      }
    }
    scratch_end(scratch);
  }
  if(!handled)
  {
    str8_list_push(arena, out, push_str8f(arena, "%S\n", line));
  }
}

internal void
rad_cargo_publish__(String8List *out, RAD_Diagnostic *first_diag, RAD_Diagnostic *last_diag)
{
  RAD_CargoState *c = rad_cargo;
  MutexScope(c->mutex)
  {
    for(String8Node *n = out->first; n != 0; n = n->next)
    {
      rad_cargo_output_append__locked(n->string);
    }
    if(first_diag != 0)
    {
      if(c->inbox_last == 0) { c->inbox_first = first_diag; } else { c->inbox_last->next = first_diag; }
      c->inbox_last = last_diag;
    }
  }
  wm_send_wakeup_event();
}

internal void
rad_cargo_read_output__(RAD_CargoRun *run, String8 workspace_root, File pipe_read, B32 parse_messages)
{
  // NOTE(rad): reads until every writer of the pipe has exited (or was
  // killed); publishes what each read completes
  Temp scratch = scratch_begin(0, 0);
  U64 read_buffer_size = KB(64);
  U8 *read_buffer = push_array_no_zero(scratch.arena, U8, read_buffer_size);
  Arena *line_arena = arena_alloc();
  String8List pending = {0};
  for(;;)
  {
    U64 read_size = file_pipe_read(pipe_read, read_buffer, read_buffer_size);
    B32 at_end = (read_size == 0);
    Temp temp = temp_begin(scratch.arena);
    String8List out = {0};
    RAD_Diagnostic *first_diag = 0;
    RAD_Diagnostic *last_diag = 0;
    U64 line_start = 0;
    for(U64 idx = 0; idx < read_size; idx += 1)
    {
      if(read_buffer[idx] == '\n')
      {
        String8 line = str8(read_buffer + line_start, idx - line_start);
        if(pending.node_count != 0)
        {
          str8_list_push(line_arena, &pending, line);
          line = str8_list_join(line_arena, &pending, 0);
        }
        rad_cargo_process_line__(temp.arena, run, workspace_root, line, parse_messages, &out, &first_diag, &last_diag);
        MemoryZeroStruct(&pending);
        arena_clear(line_arena);
        line_start = idx+1;
      }
    }
    if(line_start < read_size)
    {
      str8_list_push(line_arena, &pending, push_str8_copy(line_arena, str8(read_buffer + line_start, read_size - line_start)));
    }
    if(at_end && pending.node_count != 0)
    {
      String8 line = str8_list_join(line_arena, &pending, 0);
      rad_cargo_process_line__(temp.arena, run, workspace_root, line, parse_messages, &out, &first_diag, &last_diag);
      MemoryZeroStruct(&pending);
      arena_clear(line_arena);
    }
    if(out.node_count != 0 || first_diag != 0)
    {
      rad_cargo_publish__(&out, first_diag, last_diag);
    }
    temp_end(temp);
    if(at_end)
    {
      break;
    }
  }
  arena_release(line_arena);
  scratch_end(scratch);
}

internal U64
rad_cargo_run_in_terminal__(RAD_CargoRun *run, String8 workspace_root, B32 *window_left_open_out, B32 *window_closed_early_out)
{
  // NOTE(rad): runs `run->terminal_cmd_line` in a terminal window (or, in
  // script mode, a hidden one whose output goes to the Output tab); returns
  // the program's exit code once the program has ended. The window then
  // waits for a key: it is left open (*window_left_open_out), in the run's
  // process group. *window_closed_early_out: the window went away before
  // the program ended (closed, or Stop).
  RAD_CargoState *c = rad_cargo;
  Temp scratch = scratch_begin(0, 0);
  U64 exit_code = 0;

  //- rad: while the window starts, Stop only flags it (the group must not
  // close under the launch)
  B32 stop_before_launch = 0;
  MutexScope(c->mutex)
  {
    stop_before_launch = c->stop_requested;
    if(!stop_before_launch)
    {
      c->launched = 0;
    }
  }
  if(!stop_before_launch)
  {
    //- rad: the file the window writes the program's exit code to
    String8 done_folder = push_str8f(scratch.arena, "%S/" RAD_DATA_FOLDER_NAME, get_process_info()->user_program_cache_data_path);
    make_directory(done_folder);
    String8 done_path = push_str8f(scratch.arena, "%S/run_%u_%I64u.exit", done_folder, get_process_info()->pid, run->start_us);
    delete_file_at_path(done_path);

    //- rad: launch
    String8 cmd_text = str8_list_join(scratch.arena, &run->terminal_cmd_line, &(StringJoin){.sep = str8_lit(" ")});
    rad_cargo_output_append(push_str8f(scratch.arena, "> %S    (%s)\n", cmd_text, run->terminal_hidden ? "hidden terminal: script mode" : "in a terminal window"));
    FilePair pipe = {0};
    if(run->terminal_hidden)
    {
      pipe = file_pipe_make(0, 1);
    }
    SH_TerminalLaunchParams params = {0};
    params.cmd_line = run->terminal_cmd_line;
    params.path = run->folder;
    params.title = run->terminal_title;
    params.process_group = run->group;
    params.done_path = done_path;
    params.hidden = run->terminal_hidden;
    params.output_file = pipe.write;
    Process window = sh_launch_in_terminal(&params);
    if(run->terminal_hidden)
    {
      file_close(pipe.write);
    }
    B32 window_launched = !process_match(window, process_zero());
    MutexScope(c->mutex)
    {
      c->launched = 1;
      c->in_terminal = window_launched;
      if(c->stop_requested && !c->group_closed)
      {
        process_group_close(run->group);
        c->group_closed = 1;
      }
    }
    wm_send_wakeup_event();

    //- rad: wait for the program to end (exit code in the done file), or
    // for the window to go away
    if(!window_launched)
    {
      rad_cargo_output_append(str8_lit("error: could not open a terminal window for the program\n"));
      exit_code = 1;
    }
    else
    {
      if(run->terminal_hidden)
      {
        rad_cargo_read_output__(run, workspace_root, pipe.read, 0);
      }
      B32 window_ended = 0;
      B32 program_ended = 0;
      for(;!window_ended && !program_ended;)
      {
        U64 window_code = 0;
        window_ended = process_join(window, now_time_us() + 50000, &window_code);
        Temp temp = temp_begin(scratch.arena);
        String8 done_data = data_from_file_path(temp.arena, done_path);
        S64 program_code = 0;
        if(str8_find_needle(done_data, 0, str8_lit("\n"), 0) < done_data.size &&
           try_s64_from_str8_c_rules(str8_skip_chop_whitespace(done_data), &program_code))
        {
          program_ended = 1;
          exit_code = (U64)(U32)(S32)program_code;
        }
        else if(window_ended)
        {
          exit_code = window_code;
        }
        temp_end(temp);
      }
      if(!window_ended)
      {
        process_detach(window);
        *window_left_open_out = 1;
      }
      if(!program_ended)
      {
        *window_closed_early_out = 1;
      }
    }
    if(run->terminal_hidden)
    {
      file_close(pipe.read);
    }
    delete_file_at_path(done_path);
  }

  scratch_end(scratch);
  return exit_code;
}

internal void
rad_cargo_thread_entry_point(void *p)
{
  RAD_CargoRun *run = (RAD_CargoRun *)p;
  RAD_CargoState *c = rad_cargo;
  ThreadNameF("rad_cargo_thread");
  Temp scratch = scratch_begin(0, 0);

  //////////////////////////////
  //- rad: workspace root (rustc reports workspace files relative to it)
  //
  String8 workspace_root = run->folder;
  {
    String8List cmd_line = {0};
    str8_list_push(scratch.arena, &cmd_line, str8_lit("cargo"));
    str8_list_push(scratch.arena, &cmd_line, str8_lit("locate-project"));
    str8_list_push(scratch.arena, &cmd_line, str8_lit("--workspace"));
    str8_list_push(scratch.arena, &cmd_line, str8_lit("--message-format"));
    str8_list_push(scratch.arena, &cmd_line, str8_lit("plain"));
    String8 manifest = str8_skip_chop_whitespace(rad_cargo_capture_stdout__(scratch.arena, &cmd_line, run->folder));
    if(str8_match(str8_skip_last_slash(manifest), str8_lit("Cargo.toml"), StringMatchFlag_CaseInsensitive))
    {
      workspace_root = path_normalized_from_string(run->arena, str8_chop_last_slash(manifest));
    }
  }
  MutexScope(c->mutex)
  {
    c->workspace_root = workspace_root;
  }

  //////////////////////////////
  //- rad: launch: stdout + stderr on one pipe, in the run's process group
  //
  FilePair pipe = file_pipe_make(0, 1);
  Process process = {0};
  B32 stop_before_launch = 0;
  MutexScope(c->mutex)
  {
    stop_before_launch = c->stop_requested;
  }
  if(!stop_before_launch)
  {
    ProcessLaunchParams params = {0};
    params.cmd_line = run->cmd_line;
    params.path = run->folder;
    params.inherit_env = 1;
    params.consoleless = 1;
    params.process_group = run->group;
    params.stdout_file = pipe.write;
    params.stderr_file = pipe.write;
    process = process_launch(&params);
  }
  file_close(pipe.write);
  B32 launched = !process_match(process, process_zero());
  MutexScope(c->mutex)
  {
    c->launched = launched;
    c->launch_failed = (!launched && !stop_before_launch);
    if(c->stop_requested && !c->group_closed)
    {
      process_group_close(run->group);
      c->group_closed = 1;
    }
  }
  if(!launched && !stop_before_launch)
  {
    rad_cargo_output_append(str8_lit("error: could not start `cargo` (is Rust installed, and cargo on the PATH?)\n"));
  }

  //////////////////////////////
  //- rad: read lines until every writer has exited (or was killed)
  //
  if(launched)
  {
    rad_cargo_read_output__(run, workspace_root, pipe.read, 1);
  }
  file_close(pipe.read);

  //////////////////////////////
  //- rad: exit code
  //
  U64 exit_code = 0;
  if(launched)
  {
    process_join(process, max_U64, &exit_code);
  }

  //////////////////////////////
  //- rad: Run: the build went through -> the program, in a terminal window
  //
  B32 window_left_open = 0;
  B32 window_closed_early = 0;
  if(launched && exit_code == 0 && run->terminal_cmd_line.node_count != 0)
  {
    exit_code = rad_cargo_run_in_terminal__(run, workspace_root, &window_left_open, &window_closed_early);
  }

  //////////////////////////////
  //- rad: close the group (kills anything the command left behind), but
  // not under a terminal window that still shows the program's output
  //
  B32 stopped = 0;
  MutexScope(c->mutex)
  {
    stopped = c->stop_requested;
    if(!c->group_closed && window_left_open)
    {
      if(c->window_groups_count == ArrayCount(c->window_groups))
      {
        process_group_close(c->window_groups[0]);
        MemoryCopy(&c->window_groups[0], &c->window_groups[1], sizeof(c->window_groups[0])*(ArrayCount(c->window_groups)-1));
        c->window_groups_count -= 1;
      }
      c->window_groups[c->window_groups_count] = run->group;
      c->window_groups_count += 1;
      c->group_closed = 1;
    }
    if(!c->group_closed)
    {
      process_group_close(run->group);
      c->group_closed = 1;
    }
  }
  U64 end_us = now_time_us();
  F64 seconds = (F64)(end_us - run->start_us)/1000000.0;
  if(stopped)
  {
    rad_cargo_output_append(push_str8f(scratch.arena, "[stopped after %.2f s]\n", seconds));
  }
  else if(window_closed_early)
  {
    rad_cargo_output_append(push_str8f(scratch.arena, "[terminal window closed after %.2f s]\n", seconds));
  }
  else if(launched)
  {
    rad_cargo_output_append(push_str8f(scratch.arena, "[exit code %I64u, %.2f s]\n", exit_code, seconds));
  }
  MutexScope(c->mutex)
  {
    c->exit_code = exit_code;
    c->end_us = end_us;
    c->done = 1;
  }
  wm_send_wakeup_event();
  scratch_end(scratch);
}

////////////////////////////////
//~ rad: Cargo Commands

internal void
rad_cargo_init(void)
{
  Arena *arena = arena_alloc();
  rad_cargo = push_array(arena, RAD_CargoState, 1);
  rad_cargo->arena = arena;
  rad_cargo->mutex = mutex_alloc();
  rad_cargo->output_arena = arena_alloc(.reserve_size = RAD_CARGO_OUTPUT_MAX_SIZE + MB(2), .commit_size = KB(64), .flags = ArenaFlag_NoChain);
  rad_cargo->output_root = c_root_alloc();
  rad_cargo->output_key = c_key_make(rad_cargo->output_root, c_id_make(0, 1));
  rad_cargo->output_buffer = tb_buffer_alloc(TXT_LangKind_Null);
  rad_cargo->diags_arena = arena_alloc();
}

internal String8
rad_cargo_workspace_root(Arena *arena)
{
  // NOTE(rad): the runner sets it once it asked cargo; until then (and with
  // no run yet) it is the run's folder, or empty
  String8 result = {0};
  MutexScope(rad_cargo->mutex)
  {
    result = push_str8_copy(arena, rad_cargo->workspace_root);
  }
  return result;
}

internal void
rad_line_column_from_offset(TXT_Patched *patched, U64 off, S64 *line_out, S64 *column_out)
{
  // NOTE(rad): inverse of rad_offset_from_line_column (column in characters)
  Temp scratch = scratch_begin(0, 0);
  U64 line = txt_line_num_from_off(&patched->line_map, off);
  Rng1U64 range = txt_range_from_line_num(&patched->line_map, line);
  String8 text = memory_map_data_from_range(scratch.arena, &patched->memory_map, r1u64(range.min, Clamp(range.min, off, range.max)));
  S64 column = 1;
  for(U64 idx = 0; idx < text.size; column += 1)
  {
    UnicodeDecode decode = utf8_decode(text.str + idx, text.size - idx);
    idx += Max(decode.inc, 1);
  }
  *line_out = (S64)line;
  *column_out = column;
  scratch_end(scratch);
}

internal B32
rad_cargo_release(void)
{
  // NOTE(rad): the Debug/Release choice lives in the user file
  // (`cargo_profile: release`), so it is part of the session
  CFG_Node *user = cfg_node_child_from_string(cfg_node_root(), str8_lit("user"));
  CFG_Node *profile = cfg_node_child_from_string(user, str8_lit("cargo_profile"));
  return str8_match(profile->first->string, str8_lit("release"), 0);
}

internal void
rad_cargo_set_release(B32 release)
{
  CFG_Node *user = cfg_node_child_from_string(cfg_node_root(), str8_lit("user"));
  if(user != &cfg_nil_node)
  {
    CFG_Node *profile = cfg_node_child_from_string(user, str8_lit("cargo_profile"));
    if(profile == &cfg_nil_node)
    {
      profile = cfg_node_new(rad_state->cfg, user, str8_lit("cargo_profile"));
    }
    String8 value = (release ? str8_lit("release") : str8_lit("debug"));
    if(profile->first == &cfg_nil_node)
    {
      cfg_node_new(rad_state->cfg, profile, value);
    }
    else
    {
      cfg_node_equip_string(rad_state->cfg, profile->first, value);
    }
  }
}

internal B32
rad_cargo_is_running(void)
{
  // NOTE(rad): until the UI thread joined the runner (rad_cargo_tick)
  RAD_CargoRun *run = rad_cargo->run;
  return (run != 0 && run->thread.u64[0] != 0);
}

internal void
rad_cargo_stop__(void)
{
  RAD_CargoState *c = rad_cargo;
  if(rad_cargo_is_running())
  {
    MutexScope(c->mutex)
    {
      c->stop_requested = 1;
      if(c->launched && !c->group_closed)
      {
        process_group_close(c->run->group);
        c->group_closed = 1;
      }
    }
  }
}

internal void
rad_cargo_stop(void)
{
  rad_cargo->pending_kind = RAD_CargoCmdKind_Null;
  rad_cargo_stop__();
}

internal void
rad_cargo_start(RAD_CargoCmdKind kind, CFG_Node *window)
{
  RAD_CargoState *c = rad_cargo;
  Temp scratch = scratch_begin(0, 0);

  //- rad: a run is going -> stop it; this one starts when it is over
  if(rad_cargo_is_running())
  {
    c->pending_kind = kind;
    c->pending_window = window->id;
    rad_cargo_stop__();
  }

  //- rad: no project -> say so
  String8 folder = {0};
  if(c->pending_kind == RAD_CargoCmdKind_Null)
  {
    folder = rad_cargo_folder_from_window(scratch.arena, window);
    if(folder.size == 0)
    {
      rad_set_error(str8_lit("No Cargo project found: open a folder with a Cargo.toml (File > Open Folder)."));
    }
  }

  //- rad: start
  if(c->pending_kind == RAD_CargoCmdKind_Null && folder.size != 0)
  {
    // rad: cargo reads the files from disk: save every changed file first
    for(RAD_Document *doc = rad_state->first_document; doc != 0; doc = doc->next)
    {
      if(tb_buffer_is_dirty(doc->buffer))
      {
        rad_document_save(doc);
      }
    }

    // rad: the previous run's diagnostics and output go
    if(c->run != 0)
    {
      arena_release(c->run->arena);
      c->run = 0;
    }
    arena_clear(c->diags_arena);
    c->diags = 0;
    c->diags_count = 0;
    c->diags_cap = 0;
    c->error_count = 0;
    c->warning_count = 0;
    c->diags_gen += 1;
    rad_problems_select(0);
    rad_problems_tick();
    rad_cargo_clear_output();

    // rad: the run
    Arena *arena = arena_alloc();
    RAD_CargoRun *run = push_array(arena, RAD_CargoRun, 1);
    run->arena = arena;
    run->kind = kind;
    run->folder = push_str8_copy(arena, folder);
    run->start_us = now_time_us();
    // rad: Run = a build here (its errors in Problems), then, if it went
    // through, `cargo run -q` in a terminal window (like Visual Studio: the
    // build in the Output window, the program in a console window)
    B32 release = rad_cargo_release();
    str8_list_push(arena, &run->cmd_line, str8_lit("cargo"));
    switch(kind)
    {
      default:{}break;
      case RAD_CargoCmdKind_Build: {str8_list_push(arena, &run->cmd_line, str8_lit("build"));}break;
      case RAD_CargoCmdKind_Check: {str8_list_push(arena, &run->cmd_line, str8_lit("check"));}break;
      case RAD_CargoCmdKind_Run:   {str8_list_push(arena, &run->cmd_line, str8_lit("build"));}break;
      case RAD_CargoCmdKind_Test:  {str8_list_push(arena, &run->cmd_line, str8_lit("test"));}break;
    }
    if(release)
    {
      str8_list_push(arena, &run->cmd_line, str8_lit("--release"));
    }
    str8_list_push(arena, &run->cmd_line, str8_lit("--message-format=json"));
    if(kind == RAD_CargoCmdKind_Run)
    {
      str8_list_push(arena, &run->terminal_cmd_line, str8_lit("cargo"));
      str8_list_push(arena, &run->terminal_cmd_line, str8_lit("run"));
      str8_list_push(arena, &run->terminal_cmd_line, str8_lit("-q"));
      if(release)
      {
        str8_list_push(arena, &run->terminal_cmd_line, str8_lit("--release"));
      }
      run->terminal_title = push_str8f(arena, "%S (%s)", str8_skip_last_slash(run->folder), release ? "release" : "debug");
      run->terminal_hidden = (rad_script_is_active() && !rad_script->terminal_visible);
    }
    run->group = process_group_make(1);
    MutexScope(c->mutex)
    {
      c->workspace_root = run->folder;
      c->launched = 0;
      c->launch_failed = 0;
      c->in_terminal = 0;
      c->stop_requested = 0;
      c->group_closed = 0;
      c->done = 0;
      c->exit_code = 0;
      c->end_us = 0;
      c->inbox_first = c->inbox_last = 0;
    }
    c->run = run;
    String8 cmd_text = str8_list_join(scratch.arena, &run->cmd_line, &(StringJoin){.sep = str8_lit(" ")});
    rad_cargo_output_append(push_str8f(scratch.arena, "> %S    (%S)\n", cmd_text, folder));
    rad_output_tab_reveal(window);
    run->thread = thread_launch(rad_cargo_thread_entry_point, run);
  }

  rad_request_frame();
  scratch_end(scratch);
}

internal void
rad_cargo_tick(void)
{
  RAD_CargoState *c = rad_cargo;
  U64 now_us = now_time_us();

  //- rad: take what the runner published
  RAD_Diagnostic *first_new = 0;
  B32 done = 0;
  U64 output_gen = 0;
  MutexScope(c->mutex)
  {
    first_new = c->inbox_first;
    c->inbox_first = c->inbox_last = 0;
    done = c->done;
    output_gen = c->output_gen;
  }
  U64 first_new_idx = c->diags_count;
  for(RAD_Diagnostic *diag = first_new, *next = 0; diag != 0; diag = next)
  {
    next = diag->next;
    diag->next = 0;
    if(c->diags_count == c->diags_cap)
    {
      U64 new_cap = Max(64, c->diags_cap*2);
      RAD_Diagnostic **new_diags = push_array(c->diags_arena, RAD_Diagnostic *, new_cap);
      if(c->diags_count != 0)
      {
        MemoryCopy(new_diags, c->diags, sizeof(new_diags[0])*c->diags_count);
      }
      c->diags = new_diags;
      c->diags_cap = new_cap;
    }
    c->diags[c->diags_count] = diag;
    c->diags_count += 1;
    c->error_count += (diag->kind == RAD_DiagnosticKind_Error);
    c->warning_count += (diag->kind == RAD_DiagnosticKind_Warning);
  }
  if(first_new != 0)
  {
    c->diags_gen += 1;
    rad_request_frame();
  }

  //- rad: new diagnostics in open files -> anchor them in the text now, so
  // edits from here on move them (views anchor the rest when they show them)
  if(first_new != 0)
  {
    Temp scratch = scratch_begin(0, 0);
    Access *access = access_open();
    for(U64 idx = first_new_idx; idx < c->diags_count; idx += 1)
    {
      RAD_Diagnostic *diag = c->diags[idx];
      RAD_Document *doc = rad_document_lookup(diag->path);
      if(doc != 0 && tb_buffer_is_loaded(doc->buffer))
      {
        Temp temp = temp_begin(scratch.arena);
        TB_Text text = tb_text_from_buffer(temp.arena, access, doc->buffer);
        if(text.base_info.lines_count != 0)
        {
          rad_diagnostic_range_from_document(diag, doc, &text.patched);
        }
        temp_end(temp);
      }
    }
    access_close(access);
    scratch_end(scratch);
  }

  //- rad: the run is over -> join the runner
  if(rad_cargo_is_running() && done)
  {
    thread_join(c->run->thread, max_U64);
    MemoryZeroStruct(&c->run->thread);
    rad_request_frame();
  }

  //- rad: a command waited for the previous run -> start it
  if(c->pending_kind != RAD_CargoCmdKind_Null && !rad_cargo_is_running())
  {
    RAD_CargoCmdKind kind = c->pending_kind;
    CFG_Node *window = cfg_node_from_id(c->pending_window);
    c->pending_kind = RAD_CargoCmdKind_Null;
    if(window != &cfg_nil_node)
    {
      rad_cargo_start(kind, window);
    }
  }

  //- rad: submit the output text (at most every 100 ms while it grows)
  if(output_gen != c->output_submitted_gen)
  {
    if(done || !rad_cargo_is_running() || now_us >= c->output_submit_us + RAD_CARGO_OUTPUT_SUBMIT_US)
    {
      Arena *data_arena = arena_alloc();
      String8 data = {0};
      MutexScope(c->mutex)
      {
        data = push_str8_copy(data_arena, str8(c->output_base, c->output_size));
        c->output_submitted_gen = c->output_gen;
      }
      c_submit_data(c->output_key, &data_arena, data);
      c->output_submit_us = now_us;
    }
    rad_request_frame();
  }

  //- rad: the Output tab's buffer follows the submitted text (it is never
  // edited, so every new text loads)
  Access *access = access_open();
  U128 hash = {0};
  txt_text_info_from_key_lang(access, c->output_key, TXT_LangKind_Null, &hash);
  tb_buffer_observe_source(c->output_buffer, access, hash);
  if(!u128_match(hash, c_hash_from_key(c->output_key, 0)))
  {
    rad_request_frame();
  }
  access_close(access);
}

internal void
rad_cargo_shutdown(void)
{
  // NOTE(rad): exit -> kill what is running (the job object would also do it
  // when rad's handle to it closes)
  rad_cargo_stop();
}
