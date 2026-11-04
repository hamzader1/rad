// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

////////////////////////////////
//~ rad: Positions

internal String8
rad_lsp_uri_from_path(Arena *arena, String8 path)
{
  // NOTE(rad): file:///C:/a%20b/c.rs: forward slashes, a slash before a
  // drive letter, bytes other than unreserved ones percent-encoded
  U8 *buffer = push_array_no_zero(arena, U8, 8 + path.size*3 + 1);
  U64 size = 0;
  MemoryCopy(buffer, "file://", 7);
  size += 7;
  if(path.size >= 2 && path.str[1] == ':')
  {
    buffer[size] = '/';
    size += 1;
  }
  for EachIndex(idx, path.size)
  {
    U8 c = path.str[idx];
    if(c == '\\')
    {
      c = '/';
    }
    B32 plain = (('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || ('0' <= c && c <= '9') ||
                 c == '-' || c == '.' || c == '_' || c == '~' || c == '/' || c == ':');
    if(plain)
    {
      buffer[size] = c;
      size += 1;
    }
    else
    {
      static const char hex[] = "0123456789ABCDEF";
      buffer[size+0] = '%';
      buffer[size+1] = hex[c >> 4];
      buffer[size+2] = hex[c & 15];
      size += 3;
    }
  }
  buffer[size] = 0;
  return str8(buffer, size);
}

internal String8
rad_lsp_path_from_uri(Arena *arena, String8 uri)
{
  // NOTE(rad): file:///c:/x%20y -> C:/x y; anything else -> empty
  String8 result = {0};
  if(str8_match(str8_prefix(uri, 7), str8_lit("file://"), StringMatchFlag_CaseInsensitive))
  {
    String8 rest = str8_skip(uri, 7);
    U8 *buffer = push_array_no_zero(arena, U8, rest.size + 1);
    U64 size = 0;
    for(U64 idx = 0; idx < rest.size; idx += 1)
    {
      U8 c = rest.str[idx];
      if(c == '%' && idx+2 < rest.size)
      {
        U8 hi = rest.str[idx+1];
        U8 lo = rest.str[idx+2];
        S32 hv = (('0' <= hi && hi <= '9') ? hi-'0' : ('a' <= (hi|32) && (hi|32) <= 'f') ? (hi|32)-'a'+10 : -1);
        S32 lv = (('0' <= lo && lo <= '9') ? lo-'0' : ('a' <= (lo|32) && (lo|32) <= 'f') ? (lo|32)-'a'+10 : -1);
        if(hv >= 0 && lv >= 0)
        {
          c = (U8)(hv*16 + lv);
          idx += 2;
        }
      }
      buffer[size] = c;
      size += 1;
    }
    buffer[size] = 0;
    result = str8(buffer, size);
    if(result.size >= 3 && result.str[0] == '/' && result.str[2] == ':')
    {
      result = str8_skip(result, 1);
      result.str[0] = upper_from_char(result.str[0]);
    }
  }
  return result;
}

internal U64
rad_lsp_offset_from_position(String8 text, U64 *line_starts, U64 line_count, S64 line, S64 character, B32 utf8)
{
  // NOTE(rad): 0-based line; character in UTF-8 bytes or UTF-16 units of
  // that line; clamped to the line (its end excludes "\n" / "\r\n")
  U64 result = 0;
  if(line < 0)
  {
    result = 0;
  }
  else if((U64)line >= line_count)
  {
    result = text.size;
  }
  else
  {
    U64 start = line_starts[line];
    U64 end = ((U64)line+1 < line_count ? line_starts[line+1] : text.size);
    if(end > start && text.str[end-1] == '\n') { end -= 1; }
    if(end > start && text.str[end-1] == '\r') { end -= 1; }
    character = Max(character, 0);
    if(utf8)
    {
      result = start + Min((U64)character, end - start);
    }
    else
    {
      U64 off = start;
      for(S64 units = 0; off < end && units < character;)
      {
        UnicodeDecode decode = utf8_decode(text.str + off, end - off);
        units += (decode.codepoint >= 0x10000 ? 2 : 1);
        off += Max(decode.inc, 1);
      }
      result = Min(off, end);
    }
  }
  return result;
}

internal void
rad_lsp_position_from_offset(String8 text, U64 off, B32 utf8, S64 *line_out, S64 *character_out)
{
  off = Min(off, text.size);
  S64 line = 0;
  U64 line_start = 0;
  for(U64 idx = 0; idx < off; idx += 1)
  {
    if(text.str[idx] == '\n')
    {
      line += 1;
      line_start = idx+1;
    }
  }
  S64 character = 0;
  if(utf8)
  {
    character = (S64)(off - line_start);
  }
  else
  {
    for(U64 idx = line_start; idx < off;)
    {
      UnicodeDecode decode = utf8_decode(text.str + idx, off - idx);
      character += (decode.codepoint >= 0x10000 ? 2 : 1);
      idx += Max(decode.inc, 1);
    }
  }
  *line_out = line;
  *character_out = character;
}

internal String8
rad_lsp_root_from_path(Arena *arena, String8 path)
{
  // NOTE(rad): the package = the nearest folder with a Cargo.toml; the
  // workspace = the nearest folder at or above it whose Cargo.toml has a
  // [workspace] table (else the package is its own workspace)
  Temp scratch = scratch_begin(&arena, 1);
  String8 package = {0};
  String8 workspace = {0};
  for(String8 folder = str8_chop_last_slash(path); folder.size != 0 && workspace.size == 0;)
  {
    String8 manifest = push_str8f(scratch.arena, "%S/Cargo.toml", folder);
    FileProperties props = properties_from_file_path(manifest);
    if(props.modified != 0 && !(props.flags & FilePropertyFlag_IsFolder))
    {
      if(package.size == 0)
      {
        package = folder;
      }
      String8 data = data_from_file_path(scratch.arena, manifest);
      if(str8_find_needle(data, 0, str8_lit("[workspace]"), 0) < data.size)
      {
        workspace = folder;
      }
    }
    String8 parent = str8_chop_last_slash(folder);
    folder = (parent.size < folder.size ? parent : str8_zero());
  }
  String8 result = push_str8_copy(arena, workspace.size != 0 ? workspace : package);
  scratch_end(scratch);
  return result;
}

////////////////////////////////
//~ rad: Snapshots (the texts sent to a server, by version)

internal RAD_LspSnapshot *
rad_lsp_snapshot_push(RAD_LspDoc *ld, U64 version, String8 text)
{
  RAD_LspSnapshot *snap = &ld->snapshots[ld->snapshot_next % RAD_LSP_SNAPSHOT_COUNT];
  ld->snapshot_next += 1;
  if(snap->arena == 0)
  {
    snap->arena = arena_alloc();
  }
  arena_clear(snap->arena);
  snap->valid = 1;
  snap->version = version;
  snap->text = push_str8_copy(snap->arena, text);
  U64 line_count = 1;
  for EachIndex(idx, text.size) { line_count += (text.str[idx] == '\n'); }
  snap->line_starts = push_array_no_zero(snap->arena, U64, line_count);
  snap->line_count = line_count;
  snap->line_starts[0] = 0;
  U64 line_idx = 1;
  for EachIndex(idx, text.size)
  {
    if(text.str[idx] == '\n')
    {
      snap->line_starts[line_idx] = idx+1;
      line_idx += 1;
    }
  }
  return snap;
}

internal RAD_LspSnapshot *
rad_lsp_snapshot_from_version(RAD_LspDoc *ld, U64 version)
{
  RAD_LspSnapshot *result = 0;
  for EachElement(idx, ld->snapshots)
  {
    if(ld->snapshots[idx].valid && ld->snapshots[idx].version == version)
    {
      result = &ld->snapshots[idx];
      break;
    }
  }
  return result;
}

////////////////////////////////
//~ rad: Servers

internal RAD_LspServer *
rad_lsp_server_from_root(String8 root)
{
  RAD_LspServer *result = 0;
  for(RAD_LspServer *s = rad_lsp->first_server; s != 0; s = s->next)
  {
    if(str8_match(s->root, root, StringMatchFlag_CaseInsensitive|StringMatchFlag_SlashInsensitive))
    {
      result = s;
      break;
    }
  }
  return result;
}

internal RAD_LspServer *
rad_lsp_server_from_path(String8 path)
{
  // NOTE(rad): the server whose root holds `path`
  RAD_LspServer *result = 0;
  for(RAD_LspServer *s = rad_lsp->first_server; s != 0; s = s->next)
  {
    if(path.size > s->root.size &&
       str8_match(str8_prefix(path, s->root.size), s->root, StringMatchFlag_CaseInsensitive|StringMatchFlag_SlashInsensitive) &&
       char_is_slash(path.str[s->root.size]) &&
       (result == 0 || s->root.size > result->root.size))
    {
      result = s;
    }
  }
  return result;
}

internal RAD_LspDoc *
rad_lsp_doc_from_id(RAD_LspServer *s, U64 doc_id)
{
  RAD_LspDoc *result = 0;
  for(RAD_LspDoc *ld = s->first_doc; ld != 0; ld = ld->next)
  {
    if(ld->doc_id == doc_id)
    {
      result = ld;
      break;
    }
  }
  return result;
}

internal RAD_LspServer *
rad_lsp_server_start(String8 root)
{
  Temp scratch = scratch_begin(0, 0);
  Arena *arena = arena_alloc();
  RAD_LspServer *s = push_array(arena, RAD_LspServer, 1);
  s->arena = arena;
  s->root = push_str8_copy(arena, root);
  s->root_uri = rad_lsp_uri_from_path(arena, root);
  s->health_arena = arena_alloc();
  s->quiescent = 0;
  SLLQueuePush(rad_lsp->first_server, rad_lsp->last_server, s);
  rad_lsp->servers_started += 1;

  //- rad: log file: <logs>/rust-analyzer_<n>.log (the server's stderr)
  String8 rad_folder = push_str8f(scratch.arena, "%S/rad", get_process_info()->user_program_logs_data_path);
  String8 logs_folder = push_str8f(scratch.arena, "%S/logs", rad_folder);
  make_directory(rad_folder);
  make_directory(logs_folder);
  s->log_path = push_str8f(arena, "%S/rust-analyzer_%I64u.log", logs_folder, rad_lsp->servers_started);

  //- rad: launch
  LSP_ClientParams params = {0};
  str8_list_push(scratch.arena, &params.cmd_line, str8_lit("rust-analyzer"));
  params.path = root;
  params.stderr_path = s->log_path;
  params.wakeup = wm_send_wakeup_event;
  s->client = lsp_client_launch(&params);
  if(s->client == 0)
  {
    s->state = RAD_LspServerState_Ended;
    rad_lsp->not_found = 1;
    rad_set_error(str8_lit("rust-analyzer could not be started: install it with `rustup component add rust-analyzer`."));
  }

  //- rad: initialize
  else
  {
    String8 uri_json = json_escaped_from_str8(scratch.arena, s->root_uri);
    String8 name_json = json_escaped_from_str8(scratch.arena, str8_skip_last_slash(root));
    String8 init = push_str8f(scratch.arena,
                              "{\"processId\":%u,"
                              "\"clientInfo\":{\"name\":\"rad\"},"
                              "\"rootUri\":%S,"
                              "\"workspaceFolders\":[{\"uri\":%S,\"name\":%S}],"
                              "\"capabilities\":{"
                              "\"general\":{\"positionEncodings\":[\"utf-8\",\"utf-16\"]},"
                              "\"window\":{\"workDoneProgress\":true},"
                              "\"workspace\":{\"workspaceFolders\":true},"
                              "\"textDocument\":{"
                              "\"synchronization\":{\"didSave\":true},"
                              "\"publishDiagnostics\":{\"versionSupport\":true},"
                              "\"hover\":{\"contentFormat\":[\"markdown\",\"plaintext\"]},"
                              "\"definition\":{\"linkSupport\":false},"
                              "\"completion\":{\"completionItem\":{\"snippetSupport\":false}}"
                              "},"
                              "\"experimental\":{\"serverStatusNotification\":true}"
                              "}}",
                              get_process_info()->pid, uri_json, uri_json, name_json);
    s->initialize_id = lsp_client_request(s->client, str8_lit("initialize"), init);
    s->state = RAD_LspServerState_Starting;
  }
  scratch_end(scratch);
  return s;
}

internal void
rad_lsp_server_end(RAD_LspServer *s, B32 clean)
{
  // NOTE(rad): clean: shutdown + exit, a moment to go; then the process
  // group goes (the server and the `cargo check` it may be running)
  if(s->client != 0)
  {
    if(clean && s->state == RAD_LspServerState_Ready && lsp_client_is_alive(s->client))
    {
      lsp_client_request(s->client, str8_lit("shutdown"), str8_zero());
      lsp_client_notify(s->client, str8_lit("exit"), str8_zero());
    }
    lsp_client_release(s->client, clean ? RAD_LSP_SHUTDOWN_WAIT_US : 0);
    s->client = 0;
  }
  s->state = RAD_LspServerState_Ended;
}

////////////////////////////////
//~ rad: Document Sync

internal void
rad_lsp_doc_send(RAD_LspServer *s, RAD_LspDoc *ld, RAD_Document *doc)
{
  // NOTE(rad): didOpen the first time, then didChange with the full text
  Temp scratch = scratch_begin(0, 0);
  Access *access = access_open();
  String8 text = tb_string_from_buffer(scratch.arena, access, doc->buffer);
  access_close(access);
  U64 version = doc->buffer->version;
  String8 uri_json = json_escaped_from_str8(scratch.arena, ld->uri);
  String8 text_json = json_escaped_from_str8(scratch.arena, text);
  if(!ld->opened)
  {
    lsp_client_notify(s->client, str8_lit("textDocument/didOpen"),
                      push_str8f(scratch.arena, "{\"textDocument\":{\"uri\":%S,\"languageId\":\"rust\",\"version\":%I64u,\"text\":%S}}", uri_json, version, text_json));
    ld->opened = 1;
  }
  else
  {
    lsp_client_notify(s->client, str8_lit("textDocument/didChange"),
                      push_str8f(scratch.arena, "{\"textDocument\":{\"uri\":%S,\"version\":%I64u},\"contentChanges\":[{\"text\":%S}]}", uri_json, version, text_json));
  }
  rad_lsp_snapshot_push(ld, version, text);
  ld->sent_version = version;
  ld->change_seen_us = 0;
  scratch_end(scratch);
}

internal void
rad_lsp_doc_release(RAD_LspServer *s, RAD_LspDoc *ld)
{
  if(ld->opened && s->state == RAD_LspServerState_Ready && s->client != 0)
  {
    Temp scratch = scratch_begin(0, 0);
    lsp_client_notify(s->client, str8_lit("textDocument/didClose"),
                      push_str8f(scratch.arena, "{\"textDocument\":{\"uri\":%S}}", json_escaped_from_str8(scratch.arena, ld->uri)));
    scratch_end(scratch);
  }
  for EachElement(idx, ld->snapshots)
  {
    if(ld->snapshots[idx].arena != 0)
    {
      arena_release(ld->snapshots[idx].arena);
    }
  }
  arena_release(ld->arena);
}

internal void
rad_lsp_document_saved(RAD_Document *doc)
{
  for(RAD_LspServer *s = rad_lsp->first_server; s != 0; s = s->next)
  {
    RAD_LspDoc *ld = rad_lsp_doc_from_id(s, doc->id);
    if(ld != 0)
    {
      ld->saved_pending = 1;
    }
  }
}

internal String8
rad_lsp_doc_root(RAD_Document *doc)
{
  // NOTE(rad): cached per document (one look at the disk per open file)
  RAD_LspDocRoot *entry = 0;
  for(RAD_LspDocRoot *e = rad_lsp->first_doc_root; e != 0; e = e->next)
  {
    if(e->doc_id == doc->id)
    {
      entry = e;
      break;
    }
  }
  if(entry == 0)
  {
    entry = push_array(rad_lsp->arena, RAD_LspDocRoot, 1);
    entry->doc_id = doc->id;
    entry->root = rad_lsp_root_from_path(rad_lsp->arena, doc->path);
    SLLStackPush(rad_lsp->first_doc_root, entry);
  }
  return entry->root;
}

////////////////////////////////
//~ rad: Messages From Servers

internal RAD_Diagnostic *
rad_lsp_diagnostic_from_json(Arena *arena, RAD_LspServer *s, String8 path, JSON_Value *d)
{
  // NOTE(rad): 0 for hints (severity 4)
  RAD_Diagnostic *diag = 0;
  JSON_Value *severity_value = json_value_from_key(d, str8_lit("severity"));
  S64 severity = (severity_value->kind == JSON_ValueKind_Number ? json_s64_from_value(severity_value) : 1);
  if(severity != 4)
  {
    JSON_Value *range = json_value_from_key(d, str8_lit("range"));
    JSON_Value *start = json_value_from_key(range, str8_lit("start"));
    JSON_Value *end = json_value_from_key(range, str8_lit("end"));
    JSON_Value *code = json_value_from_key(d, str8_lit("code"));
    diag = push_array(arena, RAD_Diagnostic, 1);
    diag->kind = (severity == 1 ? RAD_DiagnosticKind_Error : severity == 2 ? RAD_DiagnosticKind_Warning : RAD_DiagnosticKind_Note);
    diag->path = push_str8_copy(arena, path);
    diag->line = json_s64_from_value(json_value_from_key(start, str8_lit("line"))) + 1;
    diag->column = json_s64_from_value(json_value_from_key(start, str8_lit("character"))) + 1;
    diag->end_line = json_s64_from_value(json_value_from_key(end, str8_lit("line"))) + 1;
    diag->end_column = json_s64_from_value(json_value_from_key(end, str8_lit("character"))) + 1;
    diag->column_in_bytes = s->utf8_positions;
    diag->code = push_str8_copy(arena, code->kind == JSON_ValueKind_String ? code->string : code->kind == JSON_ValueKind_Number ? code->string : str8_zero());
    // rad: rust-analyzer appends rustc's notes to the message, one per line:
    // the message is the first line (as cargo's), the rest is in `rendered`
    String8 full_message = json_string_from_value(json_value_from_key(d, str8_lit("message")));
    U64 first_line_end = str8_find_needle(full_message, 0, str8_lit("\n"), 0);
    diag->message = push_str8_copy(arena, str8_prefix(full_message, first_line_end));
    String8 rendered = json_string_from_value(json_value_from_key(json_value_from_key(d, str8_lit("data")), str8_lit("rendered")));
    diag->rendered = (rendered.size != 0 ? push_str8_copy(arena, rendered) :
                      push_str8f(arena, "%S%s%S%s: %S\n", rad_diagnostic_kind_name(diag->kind),
                                 diag->code.size ? "[" : "", diag->code, diag->code.size ? "]" : "", full_message));
  }
  return diag;
}

internal void
rad_lsp_take_diagnostics(RAD_LspServer *s, JSON_Value *params)
{
  Temp scratch = scratch_begin(0, 0);
  String8 path = rad_lsp_path_from_uri(scratch.arena, json_string_from_value(json_value_from_key(params, str8_lit("uri"))));
  if(path.size != 0)
  {
    path = path_normalized_from_string(scratch.arena, path);

    //- rad: the file's list (replaced on every publish)
    RAD_LspFileDiags *fd = 0;
    for(RAD_LspFileDiags *f = s->first_file_diags; f != 0; f = f->next)
    {
      if(str8_match(f->path, path, StringMatchFlag_CaseInsensitive|StringMatchFlag_SlashInsensitive))
      {
        fd = f;
        break;
      }
    }
    if(fd == 0)
    {
      fd = push_array(s->arena, RAD_LspFileDiags, 1);
      fd->arena = arena_alloc();
      SLLStackPush(s->first_file_diags, fd);
    }
    arena_clear(fd->arena);
    fd->path = push_str8_copy(fd->arena, path);
    JSON_Value *list = json_value_from_key(params, str8_lit("diagnostics"));
    fd->diags = push_array(fd->arena, RAD_Diagnostic *, list->child_count);
    fd->count = 0;

    //- rad: the version they are for -> the text sent for it -> offsets
    JSON_Value *version_value = json_value_from_key(params, str8_lit("version"));
    RAD_Document *doc = rad_document_lookup(path);
    RAD_LspDoc *ld = (doc != 0 ? rad_lsp_doc_from_id(s, doc->id) : 0);
    RAD_LspSnapshot *snap = 0;
    if(ld != 0 && version_value->kind == JSON_ValueKind_Number)
    {
      snap = rad_lsp_snapshot_from_version(ld, (U64)json_s64_from_value(version_value));
    }
    for(JSON_Value *d = list->first; d != &json_value_nil; d = d->next)
    {
      RAD_Diagnostic *diag = rad_lsp_diagnostic_from_json(fd->arena, s, path, d);
      if(diag != 0)
      {
        if(snap != 0)
        {
          JSON_Value *range = json_value_from_key(d, str8_lit("range"));
          JSON_Value *start = json_value_from_key(range, str8_lit("start"));
          JSON_Value *end = json_value_from_key(range, str8_lit("end"));
          U64 min = rad_lsp_offset_from_position(snap->text, snap->line_starts, snap->line_count,
                                                 json_s64_from_value(json_value_from_key(start, str8_lit("line"))),
                                                 json_s64_from_value(json_value_from_key(start, str8_lit("character"))), s->utf8_positions);
          U64 max = rad_lsp_offset_from_position(snap->text, snap->line_starts, snap->line_count,
                                                 json_s64_from_value(json_value_from_key(end, str8_lit("line"))),
                                                 json_s64_from_value(json_value_from_key(end, str8_lit("character"))), s->utf8_positions);
          diag->doc_id = doc->id;
          diag->doc_version = snap->version;
          diag->doc_range = r1u64(min, Max(min, max));
        }
        fd->diags[fd->count] = diag;
        fd->count += 1;
      }
    }
    rad_lsp->diags_gen += 1;
    rad_request_frame();
  }
  scratch_end(scratch);
}

internal void
rad_lsp_take_progress(RAD_LspServer *s, JSON_Value *params)
{
  Temp scratch = scratch_begin(0, 0);
  JSON_Value *token_value = json_value_from_key(params, str8_lit("token"));
  String8 token = (token_value->kind == JSON_ValueKind_String ? token_value->string : json_text_from_value(scratch.arena, token_value));
  token = str8_prefix(token, sizeof(s->progress[0].token));
  JSON_Value *value = json_value_from_key(params, str8_lit("value"));
  String8 kind = json_string_from_value(json_value_from_key(value, str8_lit("kind")));
  JSON_Value *title = json_value_from_key(value, str8_lit("title"));
  JSON_Value *message = json_value_from_key(value, str8_lit("message"));
  JSON_Value *percentage = json_value_from_key(value, str8_lit("percentage"));

  //- rad: the slot for this token (a free one for `begin`)
  RAD_LspProgress *slot = 0;
  for EachElement(idx, s->progress)
  {
    RAD_LspProgress *p = &s->progress[idx];
    if(p->active && str8_match(str8(p->token, p->token_size), token, 0))
    {
      slot = p;
      break;
    }
  }
  if(slot == 0 && str8_match(kind, str8_lit("begin"), 0))
  {
    for EachElement(idx, s->progress)
    {
      if(!s->progress[idx].active)
      {
        slot = &s->progress[idx];
        MemoryZeroStruct(slot);
        slot->active = 1;
        slot->begin_us = now_time_us();
        slot->percentage = -1;
        slot->token_size = token.size;
        MemoryCopy(slot->token, token.str, token.size);
        break;
      }
    }
  }
  if(slot != 0)
  {
    if(str8_match(kind, str8_lit("end"), 0))
    {
      slot->active = 0;
    }
    else
    {
      if(title->kind == JSON_ValueKind_String)
      {
        slot->title_size = Min(title->string.size, sizeof(slot->title));
        MemoryCopy(slot->title, title->string.str, slot->title_size);
      }
      if(message->kind == JSON_ValueKind_String)
      {
        slot->message_size = Min(message->string.size, sizeof(slot->message));
        MemoryCopy(slot->message, message->string.str, slot->message_size);
      }
      if(percentage->kind == JSON_ValueKind_Number)
      {
        slot->percentage = json_s64_from_value(percentage);
      }
    }
  }
  scratch_end(scratch);
}

internal void
rad_lsp_server_take_messages(RAD_LspServer *s)
{
  Temp scratch = scratch_begin(0, 0);
  String8List bodies = lsp_client_take_inbox(s->client, scratch.arena);
  for(String8Node *n = bodies.first; n != 0; n = n->next)
  {
    Temp temp = temp_begin(scratch.arena);
    LSP_Message m = lsp_message_from_body(temp.arena, n->string);
    s->messages_received += 1;
    switch(m.kind)
    {
      default:{}break;

      //- rad: responses
      case LSP_MessageKind_Response:
      if(m.id == s->initialize_id && s->state == RAD_LspServerState_Starting)
      {
        if(!json_value_is_nil(m.error))
        {
          rad_set_error(push_str8f(temp.arena, "rust-analyzer did not start: %S", json_string_from_value(json_value_from_key(m.error, str8_lit("message")))));
          rad_lsp_server_end(s, 0);
        }
        else
        {
          String8 encoding = json_string_from_value(json_value_from_key(json_value_from_key(m.result, str8_lit("capabilities")), str8_lit("positionEncoding")));
          s->utf8_positions = str8_match(encoding, str8_lit("utf-8"), 0);
          lsp_client_notify(s->client, str8_lit("initialized"), str8_lit("{}"));
          s->state = RAD_LspServerState_Ready;
        }
      }break;

      //- rad: the server's requests: nothing rad needs to act on; each gets
      // an answer so the server does not wait
      case LSP_MessageKind_Request:
      {
        String8 result = str8_lit("null");
        if(str8_match(m.method, str8_lit("workspace/configuration"), 0))
        {
          U64 count = json_value_from_key(m.params, str8_lit("items"))->child_count;
          String8List nulls = {0};
          for EachIndex(idx, count)
          {
            str8_list_push(temp.arena, &nulls, str8_lit("null"));
          }
          result = push_str8f(temp.arena, "[%S]", str8_list_join(temp.arena, &nulls, &(StringJoin){.sep = str8_lit(",")}));
        }
        lsp_client_respond(s->client, m.id_text, result);
      }break;

      //- rad: notifications
      case LSP_MessageKind_Notification:
      {
        if(str8_match(m.method, str8_lit("textDocument/publishDiagnostics"), 0))
        {
          rad_lsp_take_diagnostics(s, m.params);
        }
        else if(str8_match(m.method, str8_lit("$/progress"), 0))
        {
          rad_lsp_take_progress(s, m.params);
        }
        else if(str8_match(m.method, str8_lit("experimental/serverStatus"), 0))
        {
          s->quiescent = json_b32_from_value(json_value_from_key(m.params, str8_lit("quiescent")));
          String8 health = json_string_from_value(json_value_from_key(m.params, str8_lit("health")));
          String8 message = json_string_from_value(json_value_from_key(m.params, str8_lit("message")));
          arena_clear(s->health_arena);
          s->health_message = (str8_match(health, str8_lit("ok"), 0) ? str8_zero() : push_str8_copy(s->health_arena, message));
        }
        else if(str8_match(m.method, str8_lit("window/showMessage"), 0) &&
                json_s64_from_value(json_value_from_key(m.params, str8_lit("type"))) == 1)
        {
          rad_set_error(push_str8f(temp.arena, "rust-analyzer: %S", json_string_from_value(json_value_from_key(m.params, str8_lit("message")))));
        }
      }break;
    }
    temp_end(temp);
  }
  scratch_end(scratch);
}

////////////////////////////////
//~ rad: Language Server Functions

internal void
rad_lsp_init(void)
{
  Arena *arena = arena_alloc();
  rad_lsp = push_array(arena, RAD_LspState, 1);
  rad_lsp->arena = arena;
  Arena *problems_arena = arena_alloc();
  rad_problems = push_array(problems_arena, RAD_Problems, 1);
  rad_problems->arena = arena_alloc();
  rad_problems->selected_key_arena = arena_alloc();
}

internal void
rad_lsp_tick(void)
{
  U64 now_us = now_time_us();

  //- rad: messages; servers that went away
  for(RAD_LspServer *s = rad_lsp->first_server; s != 0; s = s->next)
  {
    if(s->client != 0)
    {
      rad_lsp_server_take_messages(s);
      if(!lsp_client_is_alive(s->client) && s->state != RAD_LspServerState_Ended)
      {
        rad_set_error(push_str8f(rad_frame_arena(), "rust-analyzer stopped (log: %S).", s->log_path));
        rad_lsp_server_end(s, 0);
      }
    }
    for(RAD_LspDoc *ld = s->first_doc; ld != 0; ld = ld->next)
    {
      ld->seen = 0;
    }
  }

  //- rad: open .rs documents -> their workspace's server (started when
  // needed) -> open / change / save
  for(RAD_Document *doc = rad_state->first_document; doc != 0; doc = doc->next)
  {
    if(!str8_match(str8_skip_last_dot(doc->path), str8_lit("rs"), StringMatchFlag_CaseInsensitive) ||
       !tb_buffer_is_loaded(doc->buffer))
    {
      continue;
    }
    String8 root = rad_lsp_doc_root(doc);
    if(root.size == 0)
    {
      continue;
    }
    RAD_LspServer *s = rad_lsp_server_from_root(root);
    if(s == 0 && !rad_lsp->not_found && !rad_lsp->disabled)
    {
      s = rad_lsp_server_start(root);
    }
    if(s == 0 || s->state == RAD_LspServerState_Ended)
    {
      continue;
    }
    RAD_LspDoc *ld = rad_lsp_doc_from_id(s, doc->id);
    if(ld == 0)
    {
      Arena *arena = arena_alloc();
      ld = push_array(arena, RAD_LspDoc, 1);
      ld->arena = arena;
      ld->doc_id = doc->id;
      ld->path = push_str8_copy(arena, doc->path);
      ld->uri = rad_lsp_uri_from_path(arena, doc->path);
      SLLStackPush(s->first_doc, ld);
    }
    ld->seen = 1;
    if(s->state == RAD_LspServerState_Ready)
    {
      B32 changed = (!ld->opened || ld->sent_version != doc->buffer->version);
      if(changed && ld->opened && ld->change_seen_us == 0)
      {
        ld->change_seen_us = now_us;
      }
      if(changed && (!ld->opened || now_us >= ld->change_seen_us + RAD_LSP_CHANGE_DELAY_US))
      {
        rad_lsp_doc_send(s, ld, doc);
      }
      else if(changed)
      {
        rad_request_frame();
      }
      if(!changed && ld->saved_pending)
      {
        Temp scratch = scratch_begin(0, 0);
        lsp_client_notify(s->client, str8_lit("textDocument/didSave"),
                          push_str8f(scratch.arena, "{\"textDocument\":{\"uri\":%S}}", json_escaped_from_str8(scratch.arena, ld->uri)));
        ld->saved_pending = 0;
        scratch_end(scratch);
      }
    }
  }

  //- rad: published diagnostics replaced old ones: rebuild the list now
  rad_problems_tick();

  //- rad: documents that went away -> didClose
  for(RAD_LspServer *s = rad_lsp->first_server; s != 0; s = s->next)
  {
    for(RAD_LspDoc **ptr = &s->first_doc; *ptr != 0;)
    {
      RAD_LspDoc *ld = *ptr;
      if(!ld->seen)
      {
        *ptr = ld->next;
        rad_lsp_doc_release(s, ld);
      }
      else
      {
        ptr = &ld->next;
      }
    }
  }
}

internal void
rad_lsp_shutdown(void)
{
  for(RAD_LspServer *s = rad_lsp->first_server; s != 0; s = s->next)
  {
    rad_lsp_server_end(s, 1);
  }
}

internal String8
rad_lsp_status_string(Arena *arena, B32 *busy_out)
{
  // NOTE(rad): for the bottom bar: what the servers are doing, or empty
  String8 result = {0};
  B32 busy = 0;
  for(RAD_LspServer *s = rad_lsp->first_server; s != 0 && result.size == 0; s = s->next)
  {
    if(s->state == RAD_LspServerState_Starting)
    {
      result = str8_lit("rust-analyzer: starting");
      busy = 1;
    }
    else if(s->state == RAD_LspServerState_Ready)
    {
      RAD_LspProgress *p = 0;
      for EachElement(idx, s->progress)
      {
        if(s->progress[idx].active && (p == 0 || s->progress[idx].begin_us < p->begin_us))
        {
          p = &s->progress[idx];
        }
      }
      if(p != 0)
      {
        String8 title = str8(p->title, p->title_size);
        String8 message = str8(p->message, p->message_size);
        result = push_str8f(arena, "rust-analyzer: %S%s%S", title, message.size ? " " : "", message);
        if(p->percentage >= 0)
        {
          result = push_str8f(arena, "%S (%I64d%%)", result, p->percentage);
        }
        busy = 1;
      }
      else if(!s->quiescent)
      {
        result = str8_lit("rust-analyzer: working");
        busy = 1;
      }
      else if(s->health_message.size != 0)
      {
        result = push_str8f(arena, "rust-analyzer: %S", s->health_message);
      }
    }
  }
  if(busy_out != 0)
  {
    *busy_out = busy;
  }
  return result;
}

////////////////////////////////
//~ rad: Problems (cargo + language servers)

internal String8
rad_diagnostic_key(Arena *arena, RAD_Diagnostic *diag)
{
  Temp scratch = scratch_begin(&arena, 1);
  String8 result = push_str8f(arena, "%i|%S|%I64d|%I64d|%S", diag->kind, lower_from_str8(scratch.arena, diag->path), diag->line, diag->column, diag->message);
  scratch_end(scratch);
  return result;
}

internal int
rad_problem_compare(RAD_Diagnostic **a_ptr, RAD_Diagnostic **b_ptr)
{
  RAD_Diagnostic *a = *a_ptr;
  RAD_Diagnostic *b = *b_ptr;
  int result = 0;
  if(a->kind != b->kind)
  {
    result = (a->kind < b->kind ? -1 : 1);
  }
  else
  {
    Temp scratch = scratch_begin(0, 0);
    String8 ap = lower_from_str8(scratch.arena, a->path);
    String8 bp = lower_from_str8(scratch.arena, b->path);
    int c = strncmp((char *)ap.str, (char *)bp.str, Min(ap.size, bp.size));
    result = (c != 0 ? c :
              ap.size != bp.size ? (ap.size < bp.size ? -1 : 1) :
              a->line != b->line ? (a->line < b->line ? -1 : 1) :
              a->column != b->column ? (a->column < b->column ? -1 : 1) : 0);
    scratch_end(scratch);
  }
  return result;
}

internal void
rad_problems_tick(void)
{
  RAD_Problems *p = rad_problems;
  if(p->cargo_gen_seen == rad_cargo->diags_gen && p->lsp_gen_seen == rad_lsp->diags_gen)
  {
    return;
  }
  Temp scratch = scratch_begin(0, 0);

  //- rad: the selection, by key (the list's diagnostics are new objects;
  // the old ones may be gone already)
  String8 selected_key = (p->cursor != 0 ? push_str8_copy(scratch.arena, p->selected_key) : str8_zero());

  //- rad: gather: the servers' first (they follow edits by version), then
  // cargo's that are not already there
  U64 cap = rad_cargo->diags_count;
  for(RAD_LspServer *s = rad_lsp->first_server; s != 0; s = s->next)
  {
    for(RAD_LspFileDiags *fd = s->first_file_diags; fd != 0; fd = fd->next)
    {
      cap += fd->count;
    }
  }
  arena_clear(p->arena);
  p->diags = push_array(p->arena, RAD_Diagnostic *, cap);
  p->count = 0;
  U64 table_size = cap*2 + 1;
  U64 *table = push_array(scratch.arena, U64, table_size);
  for(U64 pass = 0; pass < 2; pass += 1)
  {
    for(RAD_LspServer *s = (pass == 0 ? rad_lsp->first_server : 0); s != 0; s = s->next)
    {
      for(RAD_LspFileDiags *fd = s->first_file_diags; fd != 0; fd = fd->next)
      {
        for EachIndex(idx, fd->count)
        {
          p->diags[p->count] = fd->diags[idx];
          p->count += 1;
        }
      }
    }
    if(pass == 1)
    {
      for EachIndex(idx, rad_cargo->diags_count)
      {
        p->diags[p->count] = rad_cargo->diags[idx];
        p->count += 1;
      }
    }
  }

  //- rad: drop duplicates (first one stays)
  U64 kept = 0;
  for EachIndex(idx, p->count)
  {
    Temp temp = temp_begin(scratch.arena);
    U64 hash = u64_hash_from_str8(rad_diagnostic_key(temp.arena, p->diags[idx]));
    hash = (hash == 0 ? 1 : hash);
    temp_end(temp);
    B32 seen = 0;
    U64 slot = hash % table_size;
    for(;table[slot] != 0; slot = (slot+1) % table_size)
    {
      if(table[slot] == hash)
      {
        seen = 1;
        break;
      }
    }
    if(!seen)
    {
      table[slot] = hash;
      p->diags[kept] = p->diags[idx];
      kept += 1;
    }
  }
  p->count = kept;

  //- rad: order: errors, warnings, notes; each by path, line, column
  quick_sort(p->diags, p->count, sizeof(p->diags[0]), rad_problem_compare);
  p->error_count = 0;
  p->warning_count = 0;
  for EachIndex(idx, p->count)
  {
    p->error_count += (p->diags[idx]->kind == RAD_DiagnosticKind_Error);
    p->warning_count += (p->diags[idx]->kind == RAD_DiagnosticKind_Warning);
  }

  //- rad: keep the selection on the same diagnostic if it is still there
  U64 cursor = Min(p->cursor, p->count);
  if(selected_key.size != 0)
  {
    for EachIndex(idx, p->count)
    {
      Temp temp = temp_begin(scratch.arena);
      B32 same = str8_match(rad_diagnostic_key(temp.arena, p->diags[idx]), selected_key, 0);
      temp_end(temp);
      if(same)
      {
        cursor = idx+1;
        break;
      }
    }
  }
  rad_problems_select(cursor);
  p->gen += 1;
  p->cargo_gen_seen = rad_cargo->diags_gen;
  p->lsp_gen_seen = rad_lsp->diags_gen;
  rad_request_frame();
  scratch_end(scratch);
}

internal void
rad_problems_select(U64 cursor)
{
  RAD_Problems *p = rad_problems;
  p->cursor = Min(cursor, p->count);
  arena_clear(p->selected_key_arena);
  p->selected_key = (p->cursor != 0 ? rad_diagnostic_key(p->selected_key_arena, p->diags[p->cursor-1]) : str8_zero());
}

internal String8
rad_problem_display_path(Arena *arena, RAD_Diagnostic *diag)
{
  // NOTE(rad): relative to the last cargo command's workspace, or to the
  // root of the language server that reported it; else the full path
  Temp scratch = scratch_begin(&arena, 1);
  String8 roots[2] = {rad_cargo_workspace_root(scratch.arena), str8_zero()};
  RAD_LspServer *s = rad_lsp_server_from_path(diag->path);
  if(s != 0)
  {
    roots[1] = s->root;
  }
  String8 result = diag->path;
  for EachElement(idx, roots)
  {
    String8 root = roots[idx];
    if(root.size != 0 && diag->path.size > root.size+1 &&
       str8_match(str8_prefix(diag->path, root.size), root, StringMatchFlag_CaseInsensitive|StringMatchFlag_SlashInsensitive) &&
       char_is_slash(diag->path.str[root.size]))
    {
      result = str8_skip(diag->path, root.size+1);
      break;
    }
  }
  result = push_str8_copy(arena, result);
  scratch_end(scratch);
  return result;
}
