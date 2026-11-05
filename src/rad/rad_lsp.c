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
  // NOTE(rad): the server whose root holds `path`; else one that has a
  // document with that path open (library files)
  RAD_LspServer *result = 0;
  for(RAD_LspServer *s = rad_lsp->first_server; s != 0 && result == 0; s = s->next)
  {
    for(RAD_LspDoc *ld = s->first_doc; ld != 0; ld = ld->next)
    {
      if(str8_match(ld->path, path, StringMatchFlag_CaseInsensitive|StringMatchFlag_SlashInsensitive))
      {
        result = s;
        break;
      }
    }
  }
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
  rad_lsp->servers_made += 1;
  rad_lsp_server_launch(s);
  scratch_end(scratch);
  return s;
}

internal void
rad_lsp_server_launch(RAD_LspServer *s)
{
  Temp scratch = scratch_begin(0, 0);
  Arena *arena = s->arena;
  String8 root = s->root;
  rad_lsp->servers_started += 1;

  //- rad: log file: <logs>/rust-analyzer_<n>.log (the server's stderr)
  String8 rad_folder = push_str8f(scratch.arena, "%S/" RAD_DATA_FOLDER_NAME, get_process_info()->user_program_logs_data_path);
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
                              "\"clientInfo\":{\"name\":\"" BUILD_TITLE "\"},"
                              "\"rootUri\":%S,"
                              "\"workspaceFolders\":[{\"uri\":%S,\"name\":%S}],"
                              "\"capabilities\":{"
                              "\"general\":{\"positionEncodings\":[\"utf-8\",\"utf-16\"]},"
                              "\"window\":{\"workDoneProgress\":true},"
                              "\"workspace\":{\"workspaceFolders\":true,\"semanticTokens\":{\"refreshSupport\":true}},"
                              "\"textDocument\":{"
                              "\"synchronization\":{\"didSave\":true},"
                              "\"publishDiagnostics\":{\"versionSupport\":true},"
                              "\"hover\":{\"contentFormat\":[\"markdown\",\"plaintext\"]},"
                              "\"definition\":{\"linkSupport\":false},"
                              "\"documentSymbol\":{\"hierarchicalDocumentSymbolSupport\":true},"
                              "\"codeAction\":{\"codeActionLiteralSupport\":{\"codeActionKind\":{\"valueSet\":[\"\",\"quickfix\",\"refactor\",\"refactor.extract\","
                              "\"refactor.inline\",\"refactor.rewrite\",\"source\",\"source.organizeImports\"]}},"
                              "\"resolveSupport\":{\"properties\":[\"edit\"]},\"dataSupport\":true,\"isPreferredSupport\":true},"
                              "\"semanticTokens\":{\"requests\":{\"full\":true},\"formats\":[\"relative\"],"
                              "\"tokenTypes\":[\"namespace\",\"type\",\"class\",\"enum\",\"interface\",\"struct\",\"typeParameter\",\"parameter\",\"variable\","
                              "\"property\",\"enumMember\",\"function\",\"method\",\"macro\",\"keyword\",\"comment\",\"string\",\"number\",\"operator\",\"decorator\"],"
                              "\"tokenModifiers\":[\"declaration\",\"definition\",\"readonly\",\"static\",\"mutable\",\"unsafe\"],"
                              "\"overlappingTokenSupport\":false,\"multilineTokenSupport\":false},"
                              "\"completion\":{\"completionItem\":{\"snippetSupport\":true,"
                              "\"resolveSupport\":{\"properties\":[\"additionalTextEdits\"]}}}"
                              "},"
                              "\"experimental\":{\"serverStatusNotification\":true}"
                              "},"
                              // rad: a function's completion inserts `name($0)` / `name()$0`
                              // (no argument placeholders: rad has no tab stop navigation)
                              "\"initializationOptions\":{\"completion\":{\"callable\":{\"snippets\":\"add_parentheses\"}}}"
                              "}",
                              get_process_info()->pid, uri_json, uri_json, name_json);
    s->initialize_id = lsp_client_request(s->client, str8_lit("initialize"), init);
    s->state = RAD_LspServerState_Starting;
  }
  scratch_end(scratch);
}

internal void
rad_lsp_server_restart(RAD_LspServer *s)
{
  // NOTE(rad): in place (the hover, definition, completion and resolve
  // state point at servers): the old process goes; its documents are opened
  // again on the new one; its diagnostics and progress are dropped; requests
  // in flight to it are forgotten
  rad_lsp_server_end(s, 1);
  for(RAD_LspDoc *ld = s->first_doc, *next = 0; ld != 0; ld = next)
  {
    next = ld->next;
    rad_lsp_doc_release(s, ld);
  }
  s->first_doc = 0;
  for(RAD_LspFileDiags *fd = s->first_file_diags; fd != 0; fd = fd->next)
  {
    arena_clear(fd->arena);
    fd->diags = 0;
    fd->count = 0;
  }
  rad_lsp->diags_gen += 1;
  rad_problems_tick();
  MemoryZeroArray(s->progress);
  s->quiescent = 0;
  s->messages_received = 0;
  s->initialize_id = 0;
  s->utf8_positions = 0;
  arena_clear(s->health_arena);
  s->health_message = str8_zero();
  if(rad_lsp->hover.server == s)
  {
    Arena *hover_arena = rad_lsp->hover.arena;
    arena_clear(hover_arena);
    MemoryZeroStruct(&rad_lsp->hover);
    rad_lsp->hover.arena = hover_arena;
  }
  if(rad_lsp->definition.server == s)
  {
    rad_lsp->definition.pending = 0;
  }
  if(rad_lsp->completion.server == s)
  {
    rad_lsp_completion_close();
  }
  if(rad_lsp->resolve.server == s)
  {
    MemoryZeroStruct(&rad_lsp->resolve);
  }
  if(rad_lsp->signature.server == s)
  {
    rad_lsp_signature_close();
  }
  if(rad_lsp->code_actions.server == s || rad_lsp->code_actions.resolve_server == s)
  {
    rad_lsp->code_actions.request_id = 0;
    rad_lsp->code_actions.resolve_id = 0;
    rad_lsp->code_actions.answered = 0;
    rad_lsp->code_actions.rest_asked = 0;
  }
  s->restarts += 1;
  rad_lsp_server_launch(s);
  rad_request_frame();
}

internal void
rad_lsp_restart_all(void)
{
  // NOTE(rad): Restart Language Server: every server; also tries again to
  // find rust-analyzer if it was missing (the files open start a server)
  rad_lsp->not_found = 0;
  for(RAD_LspServer *s = rad_lsp->first_server; s != 0; s = s->next)
  {
    rad_lsp_server_restart(s);
  }
  rad_request_frame();
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
  if(ld->sem_arena != 0)
  {
    arena_release(ld->sem_arena);
  }
  if(ld->sem_mapped_arena != 0)
  {
    arena_release(ld->sem_mapped_arena);
  }
  if(ld->sym_arena != 0)
  {
    arena_release(ld->sym_arena);
    arena_release(ld->sym_mapped_arena);
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

internal B32
rad_lsp_path_is_external(String8 path)
{
  // NOTE(rad): library sources a definition jump can open: the toolchain's
  // std (rust-src), crates.io and git dependencies
  Temp scratch = scratch_begin(0, 0);
  String8 p = lower_from_str8(scratch.arena, path);
  for EachIndex(idx, p.size)
  {
    if(p.str[idx] == '\\')
    {
      p.str[idx] = '/';
    }
  }
  B32 result = (str8_find_needle(p, 0, str8_lit("/.rustup/toolchains/"), 0) < p.size ||
                str8_find_needle(p, 0, str8_lit("/.cargo/registry/"), 0) < p.size ||
                str8_find_needle(p, 0, str8_lit("/.cargo/git/"), 0) < p.size);
  scratch_end(scratch);
  return result;
}

internal RAD_LspDocRoot *
rad_lsp_doc_root_entry(RAD_Document *doc)
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
    entry->external = rad_lsp_path_is_external(doc->path);
    entry->root = (entry->external ? str8_zero() : rad_lsp_root_from_path(rad_lsp->arena, doc->path));
    SLLStackPush(rad_lsp->first_doc_root, entry);
  }
  return entry;
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
          rad_lsp_take_semantic_legend(s, json_value_from_key(json_value_from_key(m.result, str8_lit("capabilities")), str8_lit("semanticTokensProvider")));
          lsp_client_notify(s->client, str8_lit("initialized"), str8_lit("{}"));
          s->state = RAD_LspServerState_Ready;
        }
      }
      else if(rad_lsp->definition.pending && rad_lsp->definition.server == s && m.id == rad_lsp->definition.request_id)
      {
        rad_lsp_take_definition(s, &m);
      }
      else if(rad_lsp->completion.request_id != 0 && rad_lsp->completion.server == s && m.id == rad_lsp->completion.request_id)
      {
        rad_lsp_take_completion(s, &m);
      }
      else if(rad_lsp->resolve.request_id != 0 && rad_lsp->resolve.server == s && m.id == rad_lsp->resolve.request_id)
      {
        rad_lsp_take_resolve(s, &m);
      }
      else if(rad_lsp->signature.request_id != 0 && rad_lsp->signature.server == s && m.id == rad_lsp->signature.request_id)
      {
        rad_lsp_take_signature(s, &m);
      }
      else if(rad_lsp->code_actions.request_id != 0 && rad_lsp->code_actions.server == s && m.id == rad_lsp->code_actions.request_id)
      {
        rad_lsp_take_code_actions(s, &m);
      }
      else if(rad_lsp->code_actions.resolve_id != 0 && rad_lsp->code_actions.resolve_server == s && m.id == rad_lsp->code_actions.resolve_id)
      {
        rad_lsp_take_code_action_resolve(s, &m);
      }
      else if(rad_lsp_take_semantic_tokens(s, &m))
      {
      }
      else if(rad_lsp_take_document_symbols(s, &m))
      {
      }
      else if(rad_lsp->hover.requested && !rad_lsp->hover.answered &&
              rad_lsp->hover.server == s && m.id == rad_lsp->hover.request_id)
      {
        RAD_LspHover *h = &rad_lsp->hover;
        h->answered = 1;
        h->text = rad_lsp_markdown_from_hover(h->arena, m.result);
      }break;

      //- rad: the server's requests: nothing rad needs to act on; each gets
      // an answer so the server does not wait
      case LSP_MessageKind_Request:
      {
        String8 result = str8_lit("null");
        if(str8_match(m.method, str8_lit("workspace/semanticTokens/refresh"), 0))
        {
          // rad: the server knows more now (indexing done): ask again (the
          // symbols too)
          for(RAD_LspDoc *ld = s->first_doc; ld != 0; ld = ld->next)
          {
            ld->sem_asked = 0;
            ld->sym_asked = 0;
          }
          rad_request_frame();
        }
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
//~ rad: Signature Help (not in raddbg)

internal void
rad_lsp_signature_close(void)
{
  RAD_LspSignature *sh = &rad_lsp->signature;
  Arena *arena = sh->arena;
  U64 open_gen = sh->open_gen;
  if(arena != 0)
  {
    arena_clear(arena);
  }
  MemoryZeroStruct(sh);
  sh->arena = arena;
  sh->open_gen = open_gen;
}

internal B32
rad_lsp_signature_request(CFG_Node *window, CFG_Node *tab, RAD_Document *doc, U64 off, U8 trigger_char)
{
  // NOTE(rad): asks for the signature of the call around `off` (starts the
  // popup, or asks again for the shown one); 0 if no ready server has the
  // document open
  Temp scratch = scratch_begin(0, 0);
  RAD_LspSignature *sh = &rad_lsp->signature;
  RAD_LspServer *s = 0;
  String8 params = rad_lsp_position_params(scratch.arena, doc, off, &s);
  B32 result = (params.size != 0 && params.str[params.size-1] == '}');
  if(result)
  {
    B32 retrigger = (sh->active && sh->tab == tab->id && sh->doc_id == doc->id);
    if(!retrigger)
    {
      rad_lsp_signature_close();
      sh->active = 1;
      sh->window = window->id;
      sh->tab = tab->id;
      sh->doc_id = doc->id;
      sh->open_gen += 1;
    }
    // rad: context: invoked (1), trigger character (2), content change (3)
    String8 context = (trigger_char != 0 ? push_str8f(scratch.arena, "{\"triggerKind\":2,\"triggerCharacter\":\"%c\",\"isRetrigger\":%s}", trigger_char, retrigger ? "true" : "false") :
                       retrigger ? str8_lit("{\"triggerKind\":3,\"isRetrigger\":true}") :
                       str8_lit("{\"triggerKind\":1,\"isRetrigger\":false}"));
    String8 body = push_str8f(scratch.arena, "%S,\"context\":%S}", str8_chop(params, 1), context);
    sh->server = s;
    sh->request_id = lsp_client_request(s->client, str8_lit("textDocument/signatureHelp"), body);
    sh->request_version = doc->buffer->version;
    sh->request_off = off;
  }
  scratch_end(scratch);
  return result;
}

internal void
rad_lsp_take_signature(RAD_LspServer *s, LSP_Message *m)
{
  // NOTE(rad): SignatureHelp {signatures: [{label, documentation,
  // parameters: [{label: string}], activeParameter}], activeSignature,
  // activeParameter}; null or no signatures: not in a call (closes)
  RAD_LspSignature *sh = &rad_lsp->signature;
  sh->request_id = 0;
  JSON_Value *signatures = json_value_from_key(m->result, str8_lit("signatures"));
  if(!sh->active)
  {
  }
  else if(signatures->kind != JSON_ValueKind_Array || signatures->child_count == 0)
  {
    rad_lsp_signature_close();
  }
  else
  {
    if(sh->arena == 0)
    {
      sh->arena = arena_alloc();
    }
    arena_clear(sh->arena);
    S64 active_sig = json_s64_from_value(json_value_from_key(m->result, str8_lit("activeSignature")));
    active_sig = Clamp(0, active_sig, (S64)signatures->child_count-1);
    JSON_Value *sig = signatures->first;
    for(S64 idx = 0; idx < active_sig && sig->next != &json_value_nil; idx += 1)
    {
      sig = sig->next;
    }
    sh->signatures_count = signatures->child_count;
    sh->signature_idx = (U64)active_sig;
    sh->label = push_str8_copy(sh->arena, json_string_from_value(json_value_from_key(sig, str8_lit("label"))));

    //- rad: documentation: a string or MarkupContent
    JSON_Value *doc = json_value_from_key(sig, str8_lit("documentation"));
    sh->documentation = push_str8_copy(sh->arena, str8_skip_chop_whitespace(doc->kind == JSON_ValueKind_String ? doc->string : json_string_from_value(json_value_from_key(doc, str8_lit("value")))));

    //- rad: the active parameter (the signature's, else the answer's); its
    // label is found in the signature label after the `(` (rad does not ask
    // for label offsets)
    JSON_Value *active_param_value = json_value_from_key(sig, str8_lit("activeParameter"));
    if(json_value_is_nil(active_param_value))
    {
      active_param_value = json_value_from_key(m->result, str8_lit("activeParameter"));
    }
    S64 active_param = json_s64_from_value(active_param_value);
    JSON_Value *parameters = json_value_from_key(sig, str8_lit("parameters"));
    sh->param_range = r1u64(0, 0);
    if(parameters->kind == JSON_ValueKind_Array && 0 <= active_param && active_param < (S64)parameters->child_count)
    {
      U64 search_from = str8_find_needle(sh->label, 0, str8_lit("("), 0);
      search_from = (search_from < sh->label.size ? search_from+1 : 0);
      S64 idx = 0;
      for(JSON_Value *p = parameters->first; p != &json_value_nil; p = p->next, idx += 1)
      {
        String8 plabel = json_string_from_value(json_value_from_key(p, str8_lit("label")));
        U64 pos = (plabel.size != 0 ? str8_find_needle(sh->label, search_from, plabel, 0) : sh->label.size);
        if(pos < sh->label.size)
        {
          if(idx == active_param)
          {
            sh->param_range = r1u64(pos, pos + plabel.size);
            break;
          }
          search_from = pos + plabel.size;
        }
      }
    }
    sh->answered = 1;
  }
  rad_request_frame();
}

////////////////////////////////
//~ rad: Code Actions (not in raddbg)

internal B32
rad_lsp_code_actions_request(CFG_Node *tab, RAD_Document *doc, U64 cursor, U64 mark)
{
  // NOTE(rad): the actions for the selection (the cursor if none); the
  // context has no diagnostics: rust-analyzer finds the fixes of its own
  // diagnostics in the range itself
  Temp scratch = scratch_begin(0, 0);
  RAD_LspCodeActions *ca = &rad_lsp->code_actions;
  RAD_LspServer *s = 0;
  U64 min = Min(cursor, mark);
  U64 max = Max(cursor, mark);
  String8 params_min = rad_lsp_position_params(scratch.arena, doc, min, &s);
  B32 result = (params_min.size != 0);
  if(result)
  {
    RAD_LspDoc *ld = rad_lsp_doc_from_id(s, doc->id);
    RAD_LspSnapshot *snap = rad_lsp_snapshot_from_version(ld, doc->buffer->version);
    S64 l0 = 0, c0 = 0, l1 = 0, c1 = 0;
    rad_lsp_position_from_offset(snap->text, min, s->utf8_positions, &l0, &c0);
    rad_lsp_position_from_offset(snap->text, max, s->utf8_positions, &l1, &c1);
    String8 body = push_str8f(scratch.arena,
                              "{\"textDocument\":{\"uri\":%S},"
                              "\"range\":{\"start\":{\"line\":%I64d,\"character\":%I64d},\"end\":{\"line\":%I64d,\"character\":%I64d}},"
                              "\"context\":{\"diagnostics\":[],\"triggerKind\":2}}",
                              json_escaped_from_str8(scratch.arena, ld->uri), l0, c0, l1, c1);
    ca->server = s;
    ca->request_id = lsp_client_request(s->client, str8_lit("textDocument/codeAction"), body);
    ca->request_off = cursor;
    ca->request_mark = mark;
    ca->request_version = doc->buffer->version;
  }
  scratch_end(scratch);
  return result;
}

internal void
rad_lsp_take_code_actions(RAD_LspServer *s, LSP_Message *m)
{
  // NOTE(rad): [CodeAction | Command]; Commands and actions with a command
  // but no edit to resolve are left out (rad runs no client commands)
  RAD_LspCodeActions *ca = &rad_lsp->code_actions;
  ca->request_id = 0;
  if(ca->arena == 0)
  {
    ca->arena = arena_alloc();
  }
  arena_clear(ca->arena);
  ca->count = 0;
  for(JSON_Value *a = m->result->first; m->result->kind == JSON_ValueKind_Array && a != &json_value_nil && ca->count < ArrayCount(ca->actions); a = a->next)
  {
    String8 title = json_string_from_value(json_value_from_key(a, str8_lit("title")));
    JSON_Value *edit = json_value_from_key(a, str8_lit("edit"));
    JSON_Value *data = json_value_from_key(a, str8_lit("data"));
    B32 disabled = !json_value_is_nil(json_value_from_key(a, str8_lit("disabled")));
    if(title.size == 0 || disabled || (json_value_is_nil(edit) && json_value_is_nil(data)))
    {
      continue;
    }
    RAD_LspCodeAction *action = &ca->actions[ca->count];
    action->title = push_str8_copy(ca->arena, title);
    action->kind = push_str8_copy(ca->arena, json_string_from_value(json_value_from_key(a, str8_lit("kind"))));
    action->preferred = json_b32_from_value(json_value_from_key(a, str8_lit("isPreferred")));
    action->json = json_text_from_value(ca->arena, a);
    ca->count += 1;
  }

  //- rad: preferred ones (quick fixes) first, else the server's order
  {
    RAD_LspCodeAction sorted[RAD_LSP_CODE_ACTIONS_MAX];
    U64 n = 0;
    for(U64 pass = 0; pass < 2; pass += 1)
    {
      for EachIndex(idx, ca->count)
      {
        if(!!ca->actions[idx].preferred == (pass == 0))
        {
          sorted[n] = ca->actions[idx];
          n += 1;
        }
      }
    }
    MemoryCopy(ca->actions, sorted, sizeof(sorted[0])*n);
  }
  ca->answered = 1;
  ca->answer_off = ca->request_off;
  ca->answer_mark = ca->request_mark;
  ca->answer_version = ca->request_version;
  rad_request_frame();
}

internal B32
rad_lsp_code_actions_shown(CFG_ID tab, U64 cursor, U64 mark, U64 version)
{
  RAD_LspCodeActions *ca = &rad_lsp->code_actions;
  B32 result = (ca->answered && ca->count != 0 && ca->tab == tab &&
                ca->answer_off == cursor && ca->answer_mark == mark && ca->answer_version == version);
  return result;
}

internal B32
rad_lsp_apply_workspace_edit(RAD_LspServer *s, JSON_Value *edit, U64 *skipped_out)
{
  // NOTE(rad): WorkspaceEdit: documentChanges [TextDocumentEdit {textDocument
  // {uri, version}, edits}] or changes {uri: [TextEdit]}; each open document
  // gets its edits in one undo step, from the text of the version named (or
  // the last one sent); others are counted in `skipped_out`
  Temp scratch = scratch_begin(0, 0);
  B32 applied = 0;
  U64 skipped = 0;
  typedef struct DocEdits DocEdits;
  struct DocEdits
  {
    DocEdits *next;
    String8 uri;
    S64 version;                // -1: not named
    JSON_Value *edits;
  };
  DocEdits *first = 0;
  DocEdits *last = 0;
  JSON_Value *doc_changes = json_value_from_key(edit, str8_lit("documentChanges"));
  for(JSON_Value *dc = doc_changes->first; doc_changes->kind == JSON_ValueKind_Array && dc != &json_value_nil; dc = dc->next)
  {
    JSON_Value *td = json_value_from_key(dc, str8_lit("textDocument"));
    if(td->kind != JSON_ValueKind_Object)
    {
      skipped += 1;   // a file create / rename / delete
      continue;
    }
    DocEdits *de = push_array(scratch.arena, DocEdits, 1);
    de->uri = json_string_from_value(json_value_from_key(td, str8_lit("uri")));
    JSON_Value *version = json_value_from_key(td, str8_lit("version"));
    de->version = (version->kind == JSON_ValueKind_Number ? json_s64_from_value(version) : -1);
    de->edits = json_value_from_key(dc, str8_lit("edits"));
    SLLQueuePush(first, last, de);
  }
  JSON_Value *changes = json_value_from_key(edit, str8_lit("changes"));
  for(JSON_Value *c = changes->first; changes->kind == JSON_ValueKind_Object && c != &json_value_nil; c = c->next)
  {
    DocEdits *de = push_array(scratch.arena, DocEdits, 1);
    de->uri = c->string;
    de->version = -1;
    de->edits = c->first;
    SLLQueuePush(first, last, de);
  }
  for(DocEdits *de = first; de != 0; de = de->next)
  {
    String8 path = rad_lsp_path_from_uri(scratch.arena, de->uri);
    RAD_Document *doc = rad_document_lookup(path);
    RAD_LspDoc *ld = (doc != 0 ? rad_lsp_doc_from_id(s, doc->id) : 0);
    U64 version = (ld != 0 ? (de->version >= 0 ? (U64)de->version : ld->sent_version) : 0);
    RAD_LspSnapshot *snap = (ld != 0 ? rad_lsp_snapshot_from_version(ld, version) : 0);
    if(snap == 0 || de->edits->kind != JSON_ValueKind_Array)
    {
      skipped += 1;
      continue;
    }
    RAD_LspTextEdit *edits = push_array(scratch.arena, RAD_LspTextEdit, de->edits->child_count);
    U64 count = 0;
    for(JSON_Value *e = de->edits->first; e != &json_value_nil; e = e->next)
    {
      edits[count] = rad_lsp_text_edit_from_json(scratch.arena, s, snap, e);
      count += 1;
    }
    rad_document_apply_edits(doc, version, edits, count, 0);
    applied = 1;
  }
  if(skipped_out != 0)
  {
    *skipped_out = skipped;
  }
  scratch_end(scratch);
  return applied;
}

internal void
rad_lsp_code_action_apply(RAD_Document *doc, U64 idx)
{
  // NOTE(rad): the action's edit; none yet -> codeAction/resolve (applied
  // when it comes)
  Temp scratch = scratch_begin(0, 0);
  RAD_LspCodeActions *ca = &rad_lsp->code_actions;
  RAD_LspServer *s = ca->server;
  if(idx < ca->count && s != 0 && s->state == RAD_LspServerState_Ready && s->client != 0)
  {
    RAD_LspCodeAction *action = &ca->actions[idx];
    JSON_Value *a = json_parse(scratch.arena, action->json).root;
    JSON_Value *edit = json_value_from_key(a, str8_lit("edit"));
    if(edit->kind == JSON_ValueKind_Object)
    {
      U64 skipped = 0;
      rad_lsp_apply_workspace_edit(s, edit, &skipped);
      if(skipped != 0)
      {
        rad_set_error(push_str8f(scratch.arena, "%S: %I64u change(s) to files that are not open were not made.", action->title, skipped));
      }
    }
    else
    {
      ca->resolve_server = s;
      ca->resolve_doc_id = doc->id;
      ca->resolve_version = doc->buffer->version;
      ca->resolve_id = lsp_client_request(s->client, str8_lit("codeAction/resolve"), action->json);
    }
    ca->answered = 0;
  }
  scratch_end(scratch);
}

internal void
rad_lsp_take_code_action_resolve(RAD_LspServer *s, LSP_Message *m)
{
  Temp scratch = scratch_begin(0, 0);
  RAD_LspCodeActions *ca = &rad_lsp->code_actions;
  ca->resolve_id = 0;
  JSON_Value *edit = json_value_from_key(m->result, str8_lit("edit"));
  if(edit->kind == JSON_ValueKind_Object)
  {
    U64 skipped = 0;
    rad_lsp_apply_workspace_edit(s, edit, &skipped);
    if(skipped != 0)
    {
      rad_set_error(push_str8f(scratch.arena, "%I64u change(s) to files that are not open were not made.", skipped));
    }
    rad_request_frame();
  }
  else
  {
    rad_set_error(str8_lit("The language server gave no change for this action."));
  }
  scratch_end(scratch);
}

////////////////////////////////
//~ rad: Document Symbols (not in raddbg: the navigation bar)

typedef struct RAD_LspSymbolTask RAD_LspSymbolTask;
struct RAD_LspSymbolTask
{
  RAD_LspSymbolTask *next;
  JSON_Value *v;
  U64 parent;
  U64 depth;
};

internal void
rad_lsp_symbol_tasks_push__(Arena *arena, RAD_LspSymbolTask **stack, JSON_Value *array, U64 parent, U64 depth)
{
  // rad: an array's elements onto the stack, the first on top (document order)
  if(array->kind == JSON_ValueKind_Array && array->child_count != 0)
  {
    JSON_Value **items = push_array(arena, JSON_Value *, array->child_count);
    U64 n = 0;
    for(JSON_Value *v = array->first; v != &json_value_nil && n < array->child_count; v = v->next, n += 1)
    {
      items[n] = v;
    }
    for(U64 idx = n; idx > 0; idx -= 1)
    {
      RAD_LspSymbolTask *t = push_array(arena, RAD_LspSymbolTask, 1);
      t->v = items[idx-1];
      t->parent = parent;
      t->depth = depth;
      SLLStackPush(*stack, t);
    }
  }
}

internal B32
rad_lsp_take_document_symbols(RAD_LspServer *s, LSP_Message *m)
{
  // NOTE(rad): DocumentSymbol[] {name, kind, range, selectionRange,
  // children} (asked for), or SymbolInformation[] {name, kind, location}
  RAD_LspDoc *ld = 0;
  for(RAD_LspDoc *d = s->first_doc; d != 0; d = d->next)
  {
    if(d->sym_request_id != 0 && d->sym_request_id == m->id)
    {
      ld = d;
      break;
    }
  }
  if(ld != 0)
  {
    ld->sym_request_id = 0;
    RAD_LspSnapshot *snap = rad_lsp_snapshot_from_version(ld, ld->sym_request_version);
    if(!json_value_is_nil(m->error) && json_s64_from_value(json_value_from_key(m->error, str8_lit("code"))) == -32801)
    {
      ld->sym_asked = 0;
    }
    else if(snap != 0 && m->result->kind == JSON_ValueKind_Array)
    {
      Temp scratch = scratch_begin(0, 0);
      if(ld->sym_arena == 0)
      {
        ld->sym_arena = arena_alloc();
        ld->sym_mapped_arena = arena_alloc();
      }
      arena_clear(ld->sym_arena);

      //- rad: count, then fill depth-first (an explicit stack)
      typedef RAD_LspSymbolTask Task;
      U64 cap = 0;
      {
        Task *stack = 0;
        rad_lsp_symbol_tasks_push__(scratch.arena, &stack, m->result, 0, 0);
        for(;stack != 0;)
        {
          Task *t = stack;
          SLLStackPop(stack);
          cap += 1;
          rad_lsp_symbol_tasks_push__(scratch.arena, &stack, json_value_from_key(t->v, str8_lit("children")), 0, 0);
        }
      }
      ld->syms = push_array(ld->sym_arena, RAD_LspSymbol, cap+1);
      U64 count = 0;
      {
        Task *stack = 0;
        rad_lsp_symbol_tasks_push__(scratch.arena, &stack, m->result, 0, 0);
        for(;stack != 0 && count < cap;)
        {
          Task *t = stack;
          SLLStackPop(stack);
          JSON_Value *range = json_value_from_key(t->v, str8_lit("range"));
          if(range->kind != JSON_ValueKind_Object)
          {
            range = json_value_from_key(json_value_from_key(t->v, str8_lit("location")), str8_lit("range"));
          }
          JSON_Value *select = json_value_from_key(t->v, str8_lit("selectionRange"));
          if(select->kind != JSON_ValueKind_Object)
          {
            select = range;
          }
          JSON_Value *start = json_value_from_key(range, str8_lit("start"));
          JSON_Value *end = json_value_from_key(range, str8_lit("end"));
          JSON_Value *sel_start = json_value_from_key(select, str8_lit("start"));
          RAD_LspSymbol *sym = &ld->syms[count];
          sym->name = push_str8_copy(ld->sym_arena, json_string_from_value(json_value_from_key(t->v, str8_lit("name"))));
          sym->kind = json_s64_from_value(json_value_from_key(t->v, str8_lit("kind")));
          sym->range.min = rad_lsp_offset_from_position(snap->text, snap->line_starts, snap->line_count,
                                                        json_s64_from_value(json_value_from_key(start, str8_lit("line"))),
                                                        json_s64_from_value(json_value_from_key(start, str8_lit("character"))), s->utf8_positions);
          sym->range.max = rad_lsp_offset_from_position(snap->text, snap->line_starts, snap->line_count,
                                                        json_s64_from_value(json_value_from_key(end, str8_lit("line"))),
                                                        json_s64_from_value(json_value_from_key(end, str8_lit("character"))), s->utf8_positions);
          sym->select_off = rad_lsp_offset_from_position(snap->text, snap->line_starts, snap->line_count,
                                                         json_s64_from_value(json_value_from_key(sel_start, str8_lit("line"))),
                                                         json_s64_from_value(json_value_from_key(sel_start, str8_lit("character"))), s->utf8_positions);
          sym->parent = t->parent;
          sym->depth = t->depth;
          count += 1;
          rad_lsp_symbol_tasks_push__(scratch.arena, &stack, json_value_from_key(t->v, str8_lit("children")), count, t->depth + 1);
        }
      }
      ld->sym_count = count;
      ld->sym_version = ld->sym_request_version;
      ld->sym_valid = 1;
      ld->sym_mapped_valid = 0;
      rad_request_frame();
      scratch_end(scratch);
    }
  }
  return (ld != 0);
}

internal RAD_LspSymbol *
rad_lsp_symbols_from_document(RAD_Document *doc, U64 *count_out)
{
  // NOTE(rad): the document's symbols in its current text, carried once per
  // buffer version (as rad_lsp_token_colors_from_document)
  RAD_LspSymbol *result = 0;
  U64 count = 0;
  RAD_LspServer *s = (doc != 0 ? rad_lsp_server_from_path(doc->path) : 0);
  RAD_LspDoc *ld = (s != 0 ? rad_lsp_doc_from_id(s, doc->id) : 0);
  if(ld != 0 && ld->sym_valid)
  {
    U64 version = doc->buffer->version;
    if(!ld->sym_mapped_valid || ld->sym_mapped_version != version)
    {
      arena_clear(ld->sym_mapped_arena);
      ld->sym_mapped = push_array_no_zero(ld->sym_mapped_arena, RAD_LspSymbol, ld->sym_count+1);
      B32 good = 1;
      for(U64 idx = 0; idx < ld->sym_count && good; idx += 1)
      {
        RAD_LspSymbol sym = ld->syms[idx];
        if(ld->sym_version != version)
        {
          good = (tb_buffer_map_offset(doc->buffer, ld->sym_version, &sym.range.min) &&
                  tb_buffer_map_offset_side(doc->buffer, ld->sym_version, &sym.range.max, Side_Min) &&
                  tb_buffer_map_offset(doc->buffer, ld->sym_version, &sym.select_off));
        }
        ld->sym_mapped[idx] = sym;
      }
      ld->sym_mapped_count = (good ? ld->sym_count : 0);
      ld->sym_mapped_version = version;
      ld->sym_mapped_valid = 1;
    }
    result = ld->sym_mapped;
    count = ld->sym_mapped_count;
  }
  *count_out = count;
  return result;
}

////////////////////////////////
//~ rad: Semantic Tokens (not in raddbg)
//
// NOTE(rad): rust-analyzer's view of each identifier (function, type,
// lifetime, ...) colors it with one of the theme's code color slots; the
// lexer's color stays for the rest (strings, comments, punctuation) and
// until an answer comes. Answers are for a sent version; the buffer's change
// log carries them to the current text, so typing does not flash colors.

internal U8
rad_lsp_semantic_slot_from_type_name(String8 name)
{
  // rad: slot + 1 (0: no color of its own). Zed's mapping of
  // rust-analyzer's types (the user's theme came from Zed): macros as
  // functions, builtin types as types, enum variants as types
  struct {char *name; RAD_CodeColorSlot slot;} map[] =
  {
    {"function", RAD_CodeColorSlot_CodeSymbol},   {"method", RAD_CodeColorSlot_CodeSymbol},
    {"macro", RAD_CodeColorSlot_CodeSymbol},      {"procMacro", RAD_CodeColorSlot_CodeSymbol},
    {"type", RAD_CodeColorSlot_CodeType},         {"class", RAD_CodeColorSlot_CodeType},
    {"enum", RAD_CodeColorSlot_CodeType},         {"interface", RAD_CodeColorSlot_CodeType},
    {"struct", RAD_CodeColorSlot_CodeType},       {"typeAlias", RAD_CodeColorSlot_CodeType},
    {"union", RAD_CodeColorSlot_CodeType},        {"builtinType", RAD_CodeColorSlot_CodeType},
    {"selfTypeKeyword", RAD_CodeColorSlot_CodeType}, {"enumMember", RAD_CodeColorSlot_CodeType},
    {"typeParameter", RAD_CodeColorSlot_CodeTypeParameter},
    {"lifetime", RAD_CodeColorSlot_CodeLifetime},
    {"parameter", RAD_CodeColorSlot_CodeLocal},   {"variable", RAD_CodeColorSlot_CodeLocal},
    {"property", RAD_CodeColorSlot_CodeProperty},
    {"const", RAD_CodeColorSlot_CodeRegister},    {"static", RAD_CodeColorSlot_CodeRegister},
    {"constParameter", RAD_CodeColorSlot_CodeRegister}, {"boolean", RAD_CodeColorSlot_CodeRegister},
    {"attribute", RAD_CodeColorSlot_CodeMeta},    {"builtinAttribute", RAD_CodeColorSlot_CodeMeta},
    {"derive", RAD_CodeColorSlot_CodeMeta},       {"deriveHelper", RAD_CodeColorSlot_CodeMeta},
    {"decorator", RAD_CodeColorSlot_CodeMeta},
    {"keyword", RAD_CodeColorSlot_CodeKeyword},   {"selfKeyword", RAD_CodeColorSlot_CodeKeyword},
  };
  U8 result = 0;
  for EachElement(idx, map)
  {
    if(str8_match(name, str8_cstring(map[idx].name), 0))
    {
      result = (U8)(map[idx].slot + 1);
      break;
    }
  }
  return result;
}

internal void
rad_lsp_take_semantic_legend(RAD_LspServer *s, JSON_Value *provider)
{
  // NOTE(rad): capabilities.semanticTokensProvider: {legend: {tokenTypes,
  // tokenModifiers}, full: true|{delta}}
  MemoryZeroArray(s->sem_type_slots);
  s->sem_unsafe_mask = 0;
  s->sem_supported = 0;
  JSON_Value *legend = json_value_from_key(provider, str8_lit("legend"));
  JSON_Value *types = json_value_from_key(legend, str8_lit("tokenTypes"));
  JSON_Value *mods = json_value_from_key(legend, str8_lit("tokenModifiers"));
  JSON_Value *full = json_value_from_key(provider, str8_lit("full"));
  if(types->kind == JSON_ValueKind_Array && (full->kind == JSON_ValueKind_Object || json_b32_from_value(full)))
  {
    s->sem_supported = 1;
    U64 idx = 0;
    for(JSON_Value *t = types->first; t != &json_value_nil && idx < ArrayCount(s->sem_type_slots); t = t->next, idx += 1)
    {
      s->sem_type_slots[idx] = rad_lsp_semantic_slot_from_type_name(json_string_from_value(t));
    }
    idx = 0;
    for(JSON_Value *m = mods->first; m != &json_value_nil && idx < 64; m = m->next, idx += 1)
    {
      if(str8_match(json_string_from_value(m), str8_lit("unsafe"), 0))
      {
        s->sem_unsafe_mask |= (1ull << idx);
      }
    }
  }
}

internal B32
rad_lsp_take_semantic_tokens(RAD_LspServer *s, LSP_Message *m)
{
  // NOTE(rad): {data: [deltaLine, deltaStart, length, type, modifiers]*},
  // relative to the previous token; positions in the text of the version it
  // was asked for (its snapshot)
  RAD_LspDoc *ld = 0;
  for(RAD_LspDoc *d = s->first_doc; d != 0; d = d->next)
  {
    if(d->sem_request_id != 0 && d->sem_request_id == m->id)
    {
      ld = d;
      break;
    }
  }
  if(ld != 0)
  {
    ld->sem_request_id = 0;
    RAD_LspSnapshot *snap = rad_lsp_snapshot_from_version(ld, ld->sem_request_version);
    JSON_Value *data = json_value_from_key(m->result, str8_lit("data"));
    if(!json_value_is_nil(m->error) && json_s64_from_value(json_value_from_key(m->error, str8_lit("code"))) == -32801)
    {
      // rad: ContentModified: the text changed meanwhile; ask again
      ld->sem_asked = 0;
      ld->sem_retry_us = now_time_us() + 500000;
    }
    else if(snap != 0 && data->kind == JSON_ValueKind_Array)
    {
      if(ld->sem_arena == 0)
      {
        ld->sem_arena = arena_alloc();
        ld->sem_mapped_arena = arena_alloc();
      }
      arena_clear(ld->sem_arena);
      U64 max_count = data->child_count/5;
      ld->sem_tokens = push_array_no_zero(ld->sem_arena, RAD_TokenColor, max_count);
      U64 count = 0;
      S64 line = 0;
      S64 start = 0;
      JSON_Value *v = data->first;
      for(U64 idx = 0; idx < max_count && v != &json_value_nil; idx += 1)
      {
        S64 fields[5] = {0};
        for EachElement(f, fields)
        {
          fields[f] = json_s64_from_value(v);
          v = v->next;
        }
        if(fields[0] != 0)
        {
          start = 0;
        }
        line += fields[0];
        start += fields[1];
        U64 type = (U64)fields[3];
        U64 slot = (type < ArrayCount(s->sem_type_slots) ? s->sem_type_slots[type] : 0);
        if((U64)fields[4] & s->sem_unsafe_mask)
        {
          slot = RAD_CodeColorSlot_CodeUnsafe+1;
        }
        if(slot != 0)
        {
          U64 off_min = rad_lsp_offset_from_position(snap->text, snap->line_starts, snap->line_count, line, start, s->utf8_positions);
          U64 off_max = rad_lsp_offset_from_position(snap->text, snap->line_starts, snap->line_count, line, start + fields[2], s->utf8_positions);
          if(off_min < off_max)
          {
            ld->sem_tokens[count].range = r1u64(off_min, off_max);
            ld->sem_tokens[count].slot = slot-1;
            count += 1;
          }
        }
      }
      ld->sem_count = count;
      ld->sem_version = ld->sem_request_version;
      ld->sem_valid = 1;
      ld->sem_mapped_valid = 0;
      rad_request_frame();
    }
  }
  return (ld != 0);
}

internal RAD_TokenColor *
rad_lsp_token_colors_from_document(RAD_Document *doc, U64 *count_out)
{
  // NOTE(rad): the document's semantic tokens in its current text (sorted);
  // carried once per buffer version. None when the log cannot carry them
  // (a reload), until the next answer.
  RAD_TokenColor *result = 0;
  U64 count = 0;
  RAD_LspServer *s = (doc != 0 ? rad_lsp_server_from_path(doc->path) : 0);
  RAD_LspDoc *ld = (s != 0 ? rad_lsp_doc_from_id(s, doc->id) : 0);
  if(ld != 0 && ld->sem_valid)
  {
    U64 version = doc->buffer->version;
    if(!ld->sem_mapped_valid || ld->sem_mapped_version != version)
    {
      arena_clear(ld->sem_mapped_arena);
      ld->sem_mapped = push_array_no_zero(ld->sem_mapped_arena, RAD_TokenColor, ld->sem_count+1);
      U64 n = 0;
      B32 good = 1;
      for(U64 idx = 0; idx < ld->sem_count && good; idx += 1)
      {
        RAD_TokenColor t = ld->sem_tokens[idx];
        if(ld->sem_version != version)
        {
          good = (tb_buffer_map_offset(doc->buffer, ld->sem_version, &t.range.min) &&
                  tb_buffer_map_offset_side(doc->buffer, ld->sem_version, &t.range.max, Side_Min));
        }
        if(good && t.range.min < t.range.max)
        {
          ld->sem_mapped[n] = t;
          n += 1;
        }
      }
      ld->sem_mapped_count = (good ? n : 0);
      ld->sem_mapped_version = version;
      ld->sem_mapped_valid = 1;
    }
    result = ld->sem_mapped;
    count = ld->sem_mapped_count;
  }
  *count_out = count;
  return result;
}

////////////////////////////////
//~ rad: Requests

internal String8
rad_lsp_markdown_from_hover(Arena *arena, JSON_Value *result)
{
  // NOTE(rad): contents: MarkupContent {kind, value}, or the old forms: a
  // string, {language, value}, or an array of those
  Temp scratch = scratch_begin(&arena, 1);
  JSON_Value *contents = json_value_from_key(result, str8_lit("contents"));
  String8List parts = {0};
  JSON_Value *single[1] = {contents};
  B32 is_array = (contents->kind == JSON_ValueKind_Array);
  for(JSON_Value *c = (is_array ? contents->first : single[0]); c != &json_value_nil; c = (is_array ? c->next : &json_value_nil))
  {
    if(c->kind == JSON_ValueKind_String)
    {
      str8_list_push(scratch.arena, &parts, c->string);
    }
    else if(c->kind == JSON_ValueKind_Object)
    {
      String8 value = json_string_from_value(json_value_from_key(c, str8_lit("value")));
      JSON_Value *language = json_value_from_key(c, str8_lit("language"));
      if(language->kind == JSON_ValueKind_String)
      {
        str8_list_pushf(scratch.arena, &parts, "```%S\n%S\n```", language->string, value);
      }
      else
      {
        str8_list_push(scratch.arena, &parts, value);
      }
    }
  }
  String8 result_text = str8_list_join(arena, &parts, &(StringJoin){.sep = str8_lit("\n\n")});
  result_text = str8_skip_chop_whitespace(result_text);
  scratch_end(scratch);
  return result_text;
}

internal String8
rad_lsp_position_params(Arena *arena, RAD_Document *doc, U64 off, RAD_LspServer **server_out)
{
  // NOTE(rad): {"textDocument":{"uri":..},"position":{"line":..,"character":..}}
  // for an offset in the document's current text, after sending the
  // server any change it has not seen yet. Empty if no ready server has
  // the document open.
  String8 result = {0};
  RAD_LspServer *s = rad_lsp_server_from_path(doc->path);
  RAD_LspDoc *ld = (s != 0 ? rad_lsp_doc_from_id(s, doc->id) : 0);
  if(s != 0 && s->state == RAD_LspServerState_Ready && s->client != 0 && ld != 0 && ld->opened)
  {
    if(ld->sent_version != doc->buffer->version)
    {
      rad_lsp_doc_send(s, ld, doc);
    }
    RAD_LspSnapshot *snap = rad_lsp_snapshot_from_version(ld, doc->buffer->version);
    if(snap != 0)
    {
      S64 line = 0;
      S64 character = 0;
      rad_lsp_position_from_offset(snap->text, off, s->utf8_positions, &line, &character);
      Temp scratch = scratch_begin(&arena, 1);
      result = push_str8f(arena, "{\"textDocument\":{\"uri\":%S},\"position\":{\"line\":%I64d,\"character\":%I64d}}",
                          json_escaped_from_str8(scratch.arena, ld->uri), line, character);
      scratch_end(scratch);
      if(server_out != 0)
      {
        *server_out = s;
      }
    }
  }
  return result;
}

internal String8
rad_lsp_hover_from_mouse(RAD_Document *doc, Rng1U64 token_range, U64 char_off)
{
  // NOTE(rad): called by a code view each frame the mouse is on a token;
  // returns the hover text to show (empty until the answer is there)
  RAD_LspHover *h = &rad_lsp->hover;
  U64 now_us = now_time_us();
  if(!h->active || h->doc_id != doc->id || h->doc_version != doc->buffer->version ||
     h->token_range.min != token_range.min || h->token_range.max != token_range.max)
  {
    Arena *arena = h->arena;
    arena_clear(arena);
    MemoryZeroStruct(h);
    h->arena = arena;
    h->active = 1;
    h->doc_id = doc->id;
    h->doc_version = doc->buffer->version;
    h->token_range = token_range;
    h->rest_begin_us = now_us;
  }
  h->frame_seen = rad_state->frame_index;
  if(!h->requested && now_us >= h->rest_begin_us + RAD_LSP_HOVER_DELAY_US)
  {
    Temp scratch = scratch_begin(0, 0);
    RAD_LspServer *s = 0;
    String8 params = rad_lsp_position_params(scratch.arena, doc, char_off, &s);
    if(params.size != 0)
    {
      h->request_id = lsp_client_request(s->client, str8_lit("textDocument/hover"), params);
      h->server = s;
    }
    h->requested = 1;
    scratch_end(scratch);
  }
  else if(!h->requested)
  {
    rad_request_frame();
  }
  return (h->answered ? h->text : str8_zero());
}

internal void
rad_lsp_goto_definition(CFG_Node *window, CFG_Node *tab, RAD_Document *doc, U64 off)
{
  Temp scratch = scratch_begin(0, 0);
  RAD_LspDefinition *d = &rad_lsp->definition;
  RAD_LspServer *s = 0;
  String8 params = rad_lsp_position_params(scratch.arena, doc, off, &s);
  if(params.size == 0)
  {
    rad_set_error(str8_lit("Go To Definition needs rust-analyzer running for this file (it starts when a .rs file of a Cargo project is open)."));
  }
  else
  {
    arena_clear(d->arena);
    d->pending = 1;
    d->server = s;
    d->window = window->id;
    d->request_id = lsp_client_request(s->client, str8_lit("textDocument/definition"), params);

    // rad: where the jump starts (Navigate Backward)
    d->from_path = push_str8_copy(d->arena, doc->path);
    d->from_line = 1;
    d->from_column = 1;
    Access *access = access_open();
    TB_Text text = tb_text_from_buffer(scratch.arena, access, doc->buffer);
    access_close(access);
    if(text.base_info.lines_count != 0)
    {
      // rad: the jump starts at the asked offset (F12: the cursor; Ctrl+click:
      // the clicked identifier, the cursor does not move there)
      rad_line_column_from_offset(&text.patched, off, &d->from_line, &d->from_column);
    }
  }
  scratch_end(scratch);
}

internal void
rad_lsp_take_definition(RAD_LspServer *s, LSP_Message *m)
{
  // NOTE(rad): Location | Location[] | LocationLink[] | null -> the first one
  Temp scratch = scratch_begin(0, 0);
  RAD_LspDefinition *d = &rad_lsp->definition;
  d->pending = 0;
  JSON_Value *loc = (m->result->kind == JSON_ValueKind_Array ? m->result->first : m->result);
  JSON_Value *uri = json_value_from_key(loc, str8_lit("uri"));
  JSON_Value *range = json_value_from_key(loc, str8_lit("range"));
  if(uri->kind != JSON_ValueKind_String)
  {
    uri = json_value_from_key(loc, str8_lit("targetUri"));
    range = json_value_from_key(loc, str8_lit("targetSelectionRange"));
  }
  String8 path = rad_lsp_path_from_uri(scratch.arena, json_string_from_value(uri));
  CFG_Node *window = cfg_node_from_id(d->window);
  if(path.size == 0)
  {
    rad_set_error(str8_lit("No definition found."));
  }
  else if(window != &cfg_nil_node)
  {
    JSON_Value *start = json_value_from_key(range, str8_lit("start"));
    S64 line = json_s64_from_value(json_value_from_key(start, str8_lit("line"))) + 1;
    S64 column = json_s64_from_value(json_value_from_key(start, str8_lit("character"))) + 1;

    // rad: remember where it came from
    if(rad_lsp->nav_count == RAD_LSP_NAV_MAX)
    {
      MemoryCopy(&rad_lsp->nav[0], &rad_lsp->nav[1], sizeof(rad_lsp->nav[0])*(RAD_LSP_NAV_MAX-1));
      rad_lsp->nav_count -= 1;
    }
    RAD_LspNavEntry *e = &rad_lsp->nav[rad_lsp->nav_count];
    rad_lsp->nav_count += 1;
    e->path_size = Min(d->from_path.size, sizeof(e->path));
    MemoryCopy(e->path, d->from_path.str, e->path_size);
    e->line = d->from_line;
    e->column = d->from_column;

    rad_cmd(RAD_CmdKind_GoToLocation, .window = window->id, .file_path = path,
            .line_num = line, .column_num = column, .column_in_bytes = s->utf8_positions);
  }
  scratch_end(scratch);
}

internal void
rad_lsp_navigate_back(CFG_Node *window)
{
  if(rad_lsp->nav_count != 0)
  {
    rad_lsp->nav_count -= 1;
    RAD_LspNavEntry *e = &rad_lsp->nav[rad_lsp->nav_count];
    rad_cmd(RAD_CmdKind_GoToLocation, .window = window->id, .file_path = str8(e->path, e->path_size),
            .line_num = e->line, .column_num = e->column);
  }
}

////////////////////////////////
//~ rad: Completion

internal B32
rad_lsp_completion_request(CFG_Node *window, CFG_Node *tab, RAD_Document *doc, U64 off, U64 word_start, String8 word, U8 trigger_char, B32 for_incomplete)
{
  // NOTE(rad): starts a completion session for the tab (or, for_incomplete,
  // asks again within the current one); `word`: what is typed between the
  // word start and `off`. 0 if no ready server has the document open
  Temp scratch = scratch_begin(0, 0);
  RAD_LspCompletion *c = &rad_lsp->completion;
  RAD_LspServer *s = 0;
  String8 params = rad_lsp_position_params(scratch.arena, doc, off, &s);
  B32 result = (params.size != 0 && params.str[params.size-1] == '}');
  if(result)
  {
    //- rad: new session
    B32 continues = (for_incomplete && c->active && c->tab == tab->id && c->doc_id == doc->id);
    if(!continues)
    {
      rad_lsp_completion_close();
      c->active       = 1;
      c->window       = window->id;
      c->tab          = tab->id;
      c->doc_id       = doc->id;
      c->word_start   = word_start;
      c->word_version = doc->buffer->version;
      c->open_gen    += 1;
      c->cursor       = v2s64(0, 1);
      c->filter       = push_str8_copy(c->filter_arena, word);
    }

    //- rad: context: invoked (1), typed trigger character (2), an incomplete
    // list asked again (3)
    String8 context = (for_incomplete ? str8_lit("{\"triggerKind\":3}") :
                       trigger_char != 0 ? push_str8f(scratch.arena, "{\"triggerKind\":2,\"triggerCharacter\":\"%c\"}", trigger_char) :
                       str8_lit("{\"triggerKind\":1}"));
    String8 body = push_str8f(scratch.arena, "%S,\"context\":%S}", str8_chop(params, 1), context);
    c->server = s;
    c->retry = 0;
    c->request_id = lsp_client_request(s->client, str8_lit("textDocument/completion"), body);
    c->request_version = doc->buffer->version;
    c->request_filter_size = Min(word.size, sizeof(c->request_filter));
    MemoryCopy(c->request_filter, word.str, c->request_filter_size);
  }
  scratch_end(scratch);
  return result;
}

internal RAD_LspTextEdit
rad_lsp_text_edit_from_json(Arena *arena, RAD_LspServer *s, RAD_LspSnapshot *snap, JSON_Value *edit)
{
  // NOTE(rad): TextEdit {range, newText}; InsertReplaceEdit {insert, replace,
  // newText} (not asked for; its replace range if a server sends one)
  RAD_LspTextEdit result = {0};
  JSON_Value *range = json_value_from_key(edit, str8_lit("range"));
  if(range->kind != JSON_ValueKind_Object)
  {
    range = json_value_from_key(edit, str8_lit("replace"));
  }
  JSON_Value *start = json_value_from_key(range, str8_lit("start"));
  JSON_Value *end = json_value_from_key(range, str8_lit("end"));
  result.range.min = rad_lsp_offset_from_position(snap->text, snap->line_starts, snap->line_count,
                                                  json_s64_from_value(json_value_from_key(start, str8_lit("line"))),
                                                  json_s64_from_value(json_value_from_key(start, str8_lit("character"))),
                                                  s->utf8_positions);
  result.range.max = rad_lsp_offset_from_position(snap->text, snap->line_starts, snap->line_count,
                                                  json_s64_from_value(json_value_from_key(end, str8_lit("line"))),
                                                  json_s64_from_value(json_value_from_key(end, str8_lit("character"))),
                                                  s->utf8_positions);
  if(result.range.max < result.range.min)
  {
    Swap(U64, result.range.min, result.range.max);
  }
  result.text = push_str8_copy(arena, json_string_from_value(json_value_from_key(edit, str8_lit("newText"))));
  return result;
}

internal String8
rad_lsp_text_from_snippet(Arena *arena, String8 snippet, U64 *cursor_out)
{
  // NOTE(rad): LSP snippet syntax -> plain text. Tab stops ($1, ${1}) are
  // dropped, placeholders (${1:text}, nested) keep their text, a choice
  // (${1|a,b|}) its first option, a variable ($NAME, ${NAME:default}) its
  // default; \$ \} \\ are escapes. The cursor goes to the lowest tab stop
  // above 0, else to $0, else to the end. The text is never longer.
  U8 *out = push_array_no_zero(arena, U8, snippet.size+1);
  U64 out_size = 0;
  U64 best_stop = max_U64;
  U64 best_pos = 0;
  B32 has_zero = 0;
  U64 zero_pos = 0;
  U64 depth = 0;
  for(U64 i = 0; i < snippet.size;)
  {
    U8 c = snippet.str[i];
    if(c == '\\' && i+1 < snippet.size && (snippet.str[i+1] == '$' || snippet.str[i+1] == '}' || snippet.str[i+1] == '\\' ||
                                            snippet.str[i+1] == ',' || snippet.str[i+1] == '|'))
    {
      out[out_size++] = snippet.str[i+1];
      i += 2;
    }
    else if(c == '$' && i+1 < snippet.size && (char_is_digit(snippet.str[i+1], 10) || snippet.str[i+1] == '{' ||
                                              char_is_alpha(snippet.str[i+1]) || snippet.str[i+1] == '_'))
    {
      B32 braced = (snippet.str[i+1] == '{');
      U64 j = i + 1 + braced;
      U64 num_start = j;
      while(j < snippet.size && char_is_digit(snippet.str[j], 10)) { j += 1; }
      B32 is_stop = (j > num_start);
      U64 stop = (is_stop ? u64_from_str8(str8_substr(snippet, r1u64(num_start, j)), 10) : 0);
      if(!is_stop)
      {
        while(j < snippet.size && (char_is_alpha(snippet.str[j]) || char_is_digit(snippet.str[j], 10) || snippet.str[j] == '_')) { j += 1; }
      }
      if(is_stop)
      {
        if(stop == 0 && !has_zero) { has_zero = 1; zero_pos = out_size; }
        if(stop != 0 && stop < best_stop) { best_stop = stop; best_pos = out_size; }
      }
      if(!braced)
      {
        i = j;
      }
      else if(j < snippet.size && snippet.str[j] == ':')
      {
        depth += 1;
        i = j + 1;
      }
      else if(j < snippet.size && snippet.str[j] == '|')
      {
        U64 k = j + 1;
        while(k < snippet.size && snippet.str[k] != ',' && snippet.str[k] != '|') { out[out_size++] = snippet.str[k]; k += 1; }
        while(k < snippet.size && !(snippet.str[k] == '|' && k+1 < snippet.size && snippet.str[k+1] == '}')) { k += 1; }
        i = Min(k + 2, snippet.size);
      }
      else
      {
        while(j < snippet.size && snippet.str[j] != '}') { j += 1; }
        i = Min(j + 1, snippet.size);
      }
    }
    else if(c == '}' && depth > 0)
    {
      depth -= 1;
      i += 1;
    }
    else
    {
      out[out_size++] = c;
      i += 1;
    }
  }
  *cursor_out = (best_stop != max_U64 ? best_pos : has_zero ? zero_pos : out_size);
  String8 result = str8(out, out_size);
  return result;
}

internal void
rad_lsp_take_completion(RAD_LspServer *s, LSP_Message *m)
{
  // NOTE(rad): CompletionList {isIncomplete, items} | CompletionItem[] | null
  RAD_LspCompletion *c = &rad_lsp->completion;
  c->request_id = 0;
  RAD_LspDoc *ld = rad_lsp_doc_from_id(s, c->doc_id);
  RAD_LspSnapshot *snap = (ld != 0 ? rad_lsp_snapshot_from_version(ld, c->request_version) : 0);
  c->retry = (snap == 0);
  if(snap != 0)
  {
    //- rad: unpack the list
    JSON_Value *items = m->result;
    B32 incomplete = 0;
    if(m->result->kind == JSON_ValueKind_Object)
    {
      items = json_value_from_key(m->result, str8_lit("items"));
      incomplete = json_b32_from_value(json_value_from_key(m->result, str8_lit("isIncomplete")));
    }
    U64 count = (items->kind == JSON_ValueKind_Array ? Min(items->child_count, RAD_LSP_COMPLETION_MAX_ITEMS) : 0);

    //- rad: items
    arena_clear(c->arena);
    c->items_gen    += 1;
    c->answered      = 1;
    c->incomplete    = incomplete;
    c->items_version = c->request_version;
    c->items_count   = count;
    c->items         = push_array(c->arena, RAD_LspCompletionItem, count);
    c->items_filter_size = c->request_filter_size;
    MemoryCopy(c->items_filter, c->request_filter, c->request_filter_size);
    U64 idx = 0;
    for(JSON_Value *it = items->first; it != &json_value_nil && idx < count; it = it->next, idx += 1)
    {
      RAD_LspCompletionItem *item = &c->items[idx];
      String8 label       = json_string_from_value(json_value_from_key(it, str8_lit("label")));
      String8 detail      = json_string_from_value(json_value_from_key(it, str8_lit("detail")));
      String8 filter_text = json_string_from_value(json_value_from_key(it, str8_lit("filterText")));
      String8 sort_text   = json_string_from_value(json_value_from_key(it, str8_lit("sortText")));
      String8 insert_text = json_string_from_value(json_value_from_key(it, str8_lit("insertText")));
      detail = str8_prefix(detail, str8_find_needle(detail, 0, str8_lit("\n"), 0));
      item->label       = push_str8_copy(c->arena, label);
      item->detail      = push_str8_copy(c->arena, str8_skip_chop_whitespace(detail));
      item->filter_text = (filter_text.size != 0 ? push_str8_copy(c->arena, filter_text) : item->label);
      item->sort_text   = (sort_text.size != 0 ? push_str8_copy(c->arena, sort_text) : item->label);
      item->insert_text = (insert_text.size != 0 ? push_str8_copy(c->arena, insert_text) : item->label);
      item->kind        = json_s64_from_value(json_value_from_key(it, str8_lit("kind")));
      JSON_Value *edit = json_value_from_key(it, str8_lit("textEdit"));
      if(edit->kind == JSON_ValueKind_Object)
      {
        item->has_edit = 1;
        item->edit = rad_lsp_text_edit_from_json(c->arena, s, snap, edit);
      }
      item->edit_cursor = item->edit.text.size;
      item->insert_cursor = item->insert_text.size;
      item->more_edits_version = c->request_version;
      {
        // rad: documentation: a string or MarkupContent {kind, value}
        JSON_Value *doc_value = json_value_from_key(it, str8_lit("documentation"));
        String8 documentation = (doc_value->kind == JSON_ValueKind_String ? json_string_from_value(doc_value) :
                                 json_string_from_value(json_value_from_key(doc_value, str8_lit("value"))));
        item->documentation = push_str8_copy(c->arena, str8_skip_chop_whitespace(documentation));
      }
      if(json_s64_from_value(json_value_from_key(it, str8_lit("insertTextFormat"))) == 2)
      {
        // rad: a snippet: plain text, the cursor at its first tab stop
        item->insert_text = rad_lsp_text_from_snippet(c->arena, item->insert_text, &item->insert_cursor);
        item->edit.text = rad_lsp_text_from_snippet(c->arena, item->edit.text, &item->edit_cursor);
      }
      if(!json_value_is_nil(json_value_from_key(it, str8_lit("data"))))
      {
        item->resolve_json = json_text_from_value(c->arena, it);
      }
      JSON_Value *more = json_value_from_key(it, str8_lit("additionalTextEdits"));
      if(more->kind == JSON_ValueKind_Array && more->child_count != 0)
      {
        item->more_edits = push_array(c->arena, RAD_LspTextEdit, more->child_count);
        for(JSON_Value *e = more->first; e != &json_value_nil; e = e->next)
        {
          item->more_edits[item->more_edits_count] = rad_lsp_text_edit_from_json(c->arena, s, snap, e);
          item->more_edits_count += 1;
        }
      }
    }

    //- rad: rows for the word typed so far
    Temp scratch = scratch_begin(0, 0);
    String8 filter = push_str8_copy(scratch.arena, c->filter);
    rad_lsp_completion_refilter(filter);
    scratch_end(scratch);
  }
  rad_request_frame();
}

internal U32
rad_lsp_completion_rank(String8 filter, String8 text)
{
  // NOTE(rad): 0: starts with the filter (same case), 1: (any case), 2:
  // contains it, 3: contains its characters in order; max_U32: no match
  U32 result = max_U32;
  if(filter.size == 0 || str8_match(str8_prefix(text, filter.size), filter, 0))
  {
    result = 0;
  }
  else if(str8_match(str8_prefix(text, filter.size), filter, StringMatchFlag_CaseInsensitive))
  {
    result = 1;
  }
  else if(str8_find_needle(text, 0, filter, StringMatchFlag_CaseInsensitive) < text.size)
  {
    result = 2;
  }
  else
  {
    U64 filter_idx = 0;
    for(U64 idx = 0; idx < text.size && filter_idx < filter.size; idx += 1)
    {
      if(lower_from_char(text.str[idx]) == lower_from_char(filter.str[filter_idx]))
      {
        filter_idx += 1;
      }
    }
    if(filter_idx == filter.size)
    {
      result = 3;
    }
  }
  return result;
}

typedef struct RAD_LspCompletionRow RAD_LspCompletionRow;
struct RAD_LspCompletionRow
{
  U32 rank;
  U64 item_idx;
  RAD_LspCompletionItem *item;
};

internal int
rad_lsp_completion_row_compare(RAD_LspCompletionRow *a, RAD_LspCompletionRow *b)
{
  int result = 0;
  if(a->rank != b->rank)
  {
    result = (a->rank < b->rank ? -1 : 1);
  }
  else
  {
    String8 as = a->item->sort_text;
    String8 bs = b->item->sort_text;
    int c = MemoryCompare(as.str, bs.str, Min(as.size, bs.size));
    result = (c != 0 ? c :
              as.size != bs.size ? (as.size < bs.size ? -1 : 1) :
              a->item_idx != b->item_idx ? (a->item_idx < b->item_idx ? -1 : 1) : 0);
  }
  return result;
}

internal void
rad_lsp_completion_refilter(String8 filter)
{
  // NOTE(rad): rows = the matching items, best first; selection = the first
  Temp scratch = scratch_begin(0, 0);
  RAD_LspCompletion *c = &rad_lsp->completion;
  filter = push_str8_copy(scratch.arena, filter);
  arena_clear(c->filter_arena);
  c->filter = push_str8_copy(c->filter_arena, filter);
  RAD_LspCompletionRow *rows = push_array(scratch.arena, RAD_LspCompletionRow, c->items_count);
  U64 rows_count = 0;
  for EachIndex(idx, c->items_count)
  {
    U32 rank = rad_lsp_completion_rank(filter, c->items[idx].filter_text);
    if(rank != max_U32)
    {
      rows[rows_count].rank = rank;
      rows[rows_count].item_idx = idx;
      rows[rows_count].item = &c->items[idx];
      rows_count += 1;
    }
  }
  quick_sort(rows, rows_count, sizeof(rows[0]), rad_lsp_completion_row_compare);
  c->rows = push_array(c->filter_arena, U64, rows_count);
  for EachIndex(idx, rows_count)
  {
    c->rows[idx] = rows[idx].item_idx;
  }
  c->rows_count = rows_count;
  c->cursor = v2s64(0, 1);
  MemoryZeroStruct(&c->scroll_pt);
  scratch_end(scratch);
}

internal void
rad_lsp_completion_set_filter(String8 filter)
{
  RAD_LspCompletion *c = &rad_lsp->completion;
  if(!str8_match(filter, c->filter, 0))
  {
    rad_lsp_completion_refilter(filter);
    rad_request_frame();
  }
}

internal void
rad_lsp_completion_close(void)
{
  RAD_LspCompletion *c = &rad_lsp->completion;
  if(c->active)
  {
    rad_request_frame();
  }
  Arena *arena = c->arena;
  Arena *filter_arena = c->filter_arena;
  U64 open_gen = c->open_gen;
  U64 items_gen = c->items_gen;
  arena_clear(arena);
  arena_clear(filter_arena);
  MemoryZeroStruct(c);
  c->arena = arena;
  c->filter_arena = filter_arena;
  c->open_gen = open_gen;
  c->items_gen = items_gen;
}

internal void
rad_lsp_completion_resolve(RAD_Document *doc, U64 item_idx, B32 for_commit)
{
  // NOTE(rad): while selected, or before the commit's edit: the server gets
  // the text as it is now, and its answer's ranges are in it
  // (rad_lsp_take_resolve). One request at a time: a newer one replaces it.
  RAD_LspCompletion *c = &rad_lsp->completion;
  RAD_LspServer *s = c->server;
  RAD_LspDoc *ld = (s != 0 ? rad_lsp_doc_from_id(s, doc->id) : 0);
  RAD_LspCompletionItem *item = (item_idx < c->items_count ? &c->items[item_idx] : 0);
  if(s != 0 && s->state == RAD_LspServerState_Ready && s->client != 0 && ld != 0 && ld->opened && item != 0 && item->resolve_json.size != 0)
  {
    if(ld->sent_version != doc->buffer->version)
    {
      rad_lsp_doc_send(s, ld, doc);
    }
    RAD_LspResolve *r = &rad_lsp->resolve;
    r->server = s;
    r->doc_id = doc->id;
    r->version = doc->buffer->version;
    r->items_gen = c->items_gen;
    r->item_idx = item_idx;
    r->for_commit = for_commit;
    r->commit_step = 0;
    r->request_id = lsp_client_request(s->client, str8_lit("completionItem/resolve"), item->resolve_json);
  }
}

internal void
rad_lsp_take_resolve(RAD_LspServer *s, LSP_Message *m)
{
  Temp scratch = scratch_begin(0, 0);
  RAD_LspResolve *r = &rad_lsp->resolve;
  r->request_id = 0;
  RAD_LspDoc *ld = rad_lsp_doc_from_id(s, r->doc_id);
  RAD_LspSnapshot *snap = (ld != 0 ? rad_lsp_snapshot_from_version(ld, r->version) : 0);
  RAD_Document *doc = 0;
  for(RAD_Document *d = rad_state->first_document; d != 0; d = d->next)
  {
    if(d->id == r->doc_id)
    {
      doc = d;
      break;
    }
  }
  JSON_Value *more = json_value_from_key(m->result, str8_lit("additionalTextEdits"));
  RAD_LspTextEdit *edits = 0;
  U64 edits_count = 0;
  if(snap != 0 && more->kind == JSON_ValueKind_Array && more->child_count != 0)
  {
    edits = push_array(scratch.arena, RAD_LspTextEdit, more->child_count);
    for(JSON_Value *e = more->first; e != &json_value_nil; e = e->next)
    {
      edits[edits_count] = rad_lsp_text_edit_from_json(scratch.arena, s, snap, e);
      edits_count += 1;
    }
  }

  //- rad: asked while selected, list still showing those items: the item
  // keeps the edits (its commit applies them in the same undo step)
  RAD_LspCompletion *c = &rad_lsp->completion;
  if(!r->for_commit && snap != 0 && c->active && c->items_gen == r->items_gen && r->item_idx < c->items_count)
  {
    RAD_LspCompletionItem *item = &c->items[r->item_idx];
    item->resolved = 1;
    item->more_edits = push_array(c->arena, RAD_LspTextEdit, edits_count);
    item->more_edits_count = edits_count;
    item->more_edits_version = r->version;
    for EachIndex(idx, edits_count)
    {
      item->more_edits[idx].range = edits[idx].range;
      item->more_edits[idx].text = push_str8_copy(c->arena, edits[idx].text);
    }
  }

  //- rad: the item was committed before the answer came: now, into the
  // commit's undo step if nothing happened since
  else if(r->for_commit && doc != 0 && edits_count != 0)
  {
    rad_document_apply_edits(doc, r->version, edits, edits_count, r->commit_step);
  }
  r->for_commit = 0;
  r->commit_step = 0;
  scratch_end(scratch);
}

internal B32
rad_lsp_completion_is_shown(CFG_ID tab)
{
  // NOTE(rad): the popup was built this frame (before the panels), with rows
  RAD_LspCompletion *c = &rad_lsp->completion;
  return (c->active && c->tab == tab && c->built_frame == rad_state->frame_index && c->rows_count != 0);
}

////////////////////////////////
//~ rad: Language Server Functions

internal void
rad_lsp_init(void)
{
  Arena *arena = arena_alloc();
  rad_lsp = push_array(arena, RAD_LspState, 1);
  rad_lsp->arena = arena;
  rad_lsp->hover.arena = arena_alloc();
  rad_lsp->definition.arena = arena_alloc();
  rad_lsp->completion.arena = arena_alloc();
  rad_lsp->completion.filter_arena = arena_alloc();
  Arena *problems_arena = arena_alloc();
  rad_problems = push_array(problems_arena, RAD_Problems, 1);
  rad_problems->arena = arena_alloc();
  rad_problems->selected_key_arena = arena_alloc();
}

internal void
rad_lsp_tick(void)
{
  U64 now_us = now_time_us();

  //- rad: hover: no view had the mouse on its token last frame -> over
  if(rad_lsp->hover.active && rad_lsp->hover.frame_seen+1 < rad_state->frame_index)
  {
    Arena *hover_arena = rad_lsp->hover.arena;
    arena_clear(hover_arena);
    MemoryZeroStruct(&rad_lsp->hover);
    rad_lsp->hover.arena = hover_arena;
  }

  //- rad: messages; servers that went away
  for(RAD_LspServer *s = rad_lsp->first_server; s != 0; s = s->next)
  {
    if(s->client != 0)
    {
      rad_lsp_server_take_messages(s);
      if(!lsp_client_is_alive(s->client) && s->state != RAD_LspServerState_Ended)
      {
        // rad: a crash: restarted, at most RAD_LSP_AUTO_RESTART_MAX times per
        // RAD_LSP_AUTO_RESTART_WINDOW_US (then Restart Language Server)
        String8 log_path = push_str8_copy(rad_frame_arena(), s->log_path);
        rad_lsp_server_end(s, 0);
        U64 oldest_idx = 0;
        U64 recent = 0;
        for EachElement(idx, s->auto_restart_us)
        {
          if(s->auto_restart_us[idx] != 0 && now_us < s->auto_restart_us[idx] + RAD_LSP_AUTO_RESTART_WINDOW_US)
          {
            recent += 1;
          }
          if(s->auto_restart_us[idx] < s->auto_restart_us[oldest_idx])
          {
            oldest_idx = idx;
          }
        }
        if(recent < RAD_LSP_AUTO_RESTART_MAX && !rad_lsp->disabled)
        {
          s->auto_restart_us[oldest_idx] = now_us;
          rad_set_error(push_str8f(rad_frame_arena(), "rust-analyzer stopped; restarting it (log: %S).", log_path));
          rad_lsp_server_restart(s);
        }
        else
        {
          rad_set_error(push_str8f(rad_frame_arena(), "rust-analyzer stopped again; not restarted (Build > Restart Language Server). Log: %S", log_path));
        }
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
    RAD_LspDocRoot *root_entry = rad_lsp_doc_root_entry(doc);
    String8 root = root_entry->root;
    RAD_LspServer *s = 0;
    if(root_entry->external)
    {
      // rad: a library file: the server that jumped there (or the first)
      s = (rad_lsp->definition.server != 0 ? rad_lsp->definition.server : rad_lsp->first_server);
    }
    else if(root.size != 0)
    {
      s = rad_lsp_server_from_root(root);
      if(s == 0 && !rad_lsp->not_found && !rad_lsp->disabled)
      {
        s = rad_lsp_server_start(root);
      }
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
      if(!changed && s->sem_supported && ld->opened && ld->sem_request_id == 0 && !ld->sem_asked && now_us < ld->sem_retry_us)
      {
        rad_request_frame();
      }
      else if(!changed && s->sem_supported && ld->opened && ld->sem_request_id == 0 &&
              (!ld->sem_asked || ld->sem_request_version != ld->sent_version))
      {
        // rad: semantic tokens for the text the server has
        Temp scratch = scratch_begin(0, 0);
        ld->sem_request_id = lsp_client_request(s->client, str8_lit("textDocument/semanticTokens/full"),
                                                push_str8f(scratch.arena, "{\"textDocument\":{\"uri\":%S}}", json_escaped_from_str8(scratch.arena, ld->uri)));
        ld->sem_request_version = ld->sent_version;
        ld->sem_asked = 1;
        scratch_end(scratch);
      }
      if(!changed && ld->opened && ld->sym_request_id == 0 &&
         (!ld->sym_asked || ld->sym_request_version != ld->sent_version))
      {
        // rad: document symbols for the text the server has
        Temp scratch = scratch_begin(0, 0);
        ld->sym_request_id = lsp_client_request(s->client, str8_lit("textDocument/documentSymbol"),
                                                push_str8f(scratch.arena, "{\"textDocument\":{\"uri\":%S}}", json_escaped_from_str8(scratch.arena, ld->uri)));
        ld->sym_request_version = ld->sent_version;
        ld->sym_asked = 1;
        scratch_end(scratch);
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
