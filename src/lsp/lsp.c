// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

////////////////////////////////
//~ rad: Framing

internal void
lsp_frame_parser_init(LSP_FrameParser *p)
{
  MemoryZeroStruct(p);
  for EachElement(idx, p->arenas)
  {
    p->arenas[idx] = arena_alloc(.reserve_size = LSP_MAX_MESSAGE_SIZE + MB(8), .commit_size = KB(64), .flags = ArenaFlag_NoChain);
  }
}

internal void
lsp_frame_parser_release(LSP_FrameParser *p)
{
  for EachElement(idx, p->arenas)
  {
    arena_release(p->arenas[idx]);
  }
  MemoryZeroStruct(p);
}

internal void
lsp_frame_parser_push(LSP_FrameParser *p, String8 bytes)
{
  if(p->failed || bytes.size == 0)
  {
    return;
  }

  //- rad: drop what was taken: the rest (a partial message) moves to the
  // base of the other arena
  if(p->consumed != 0)
  {
    U64 rest_size = p->size - p->consumed;
    Arena *dst = p->arenas[p->arena_idx^1];
    arena_clear(dst);
    U8 *rest = (U8 *)arena_push(dst, rest_size, 1, 0);
    MemoryCopy(rest, p->base + p->consumed, rest_size);
    p->arena_idx ^= 1;
    p->base = rest;
    p->size = rest_size;
    p->consumed = 0;
    if(rest_size == 0)
    {
      p->base = 0;
    }
  }

  //- rad: too much without a complete message -> the stream is broken
  if(p->size + bytes.size > LSP_MAX_MESSAGE_SIZE + LSP_MAX_HEADER_SIZE + MB(4))
  {
    p->failed = 1;
    return;
  }

  //- rad: append (no-chain arena, alignment 1: contiguous)
  Arena *arena = p->arenas[p->arena_idx];
  if(p->base == 0)
  {
    arena_clear(arena);
  }
  U8 *dst = (U8 *)arena_push(arena, bytes.size, 1, 0);
  if(p->base == 0)
  {
    p->base = dst;
  }
  Assert(dst == p->base + p->size);
  MemoryCopy(dst, bytes.str, bytes.size);
  p->size += bytes.size;
}

internal B32
lsp_frame_parser_next(LSP_FrameParser *p, Arena *arena, String8 *body_out)
{
  B32 result = 0;
  String8 data = str8(p->base + p->consumed, p->size - p->consumed);
  U64 header_end = str8_find_needle(data, 0, str8_lit("\r\n\r\n"), 0);
  if(p->failed || data.size == 0)
  {
    // rad: nothing (more) to take
  }
  else if(header_end >= data.size || header_end > LSP_MAX_HEADER_SIZE)
  {
    // rad: header incomplete; too long to ever be one -> broken
    p->failed = (data.size > LSP_MAX_HEADER_SIZE + 3);
  }
  else
  {
    //- rad: headers: `Name: value` lines; Content-Length is required, the
    // others (Content-Type) are ignored
    String8 header = str8_prefix(data, header_end);
    B32 has_length = 0;
    B32 header_ok = 1;
    U64 content_length = 0;
    for(U64 line_start = 0; header_ok && line_start <= header.size;)
    {
      U64 line_end = str8_find_needle(header, line_start, str8_lit("\r\n"), 0);
      String8 line = str8_substr(header, r1u64(line_start, line_end));
      U64 colon = str8_find_needle(line, 0, str8_lit(":"), 0);
      if(colon >= line.size)
      {
        header_ok = 0;
      }
      else
      {
        String8 name = str8_skip_chop_whitespace(str8_prefix(line, colon));
        String8 value = str8_skip_chop_whitespace(str8_skip(line, colon+1));
        if(str8_match(name, str8_lit("Content-Length"), StringMatchFlag_CaseInsensitive))
        {
          U64 v = 0;
          B32 digits_ok = (value.size != 0 && value.size <= 18);
          for(U64 idx = 0; digits_ok && idx < value.size; idx += 1)
          {
            digits_ok = ('0' <= value.str[idx] && value.str[idx] <= '9');
            v = v*10 + (value.str[idx] - '0');
          }
          header_ok = (digits_ok && !has_length);
          has_length = 1;
          content_length = v;
        }
      }
      line_start = line_end + 2;
    }
    if(!header_ok || !has_length || content_length > LSP_MAX_MESSAGE_SIZE)
    {
      p->failed = 1;
    }

    //- rad: body complete -> take it
    else
    {
      U64 body_start = header_end + 4;
      if(data.size - body_start >= content_length)
      {
        *body_out = push_str8_copy(arena, str8(data.str + body_start, content_length));
        p->consumed += body_start + content_length;
        result = 1;
      }
    }
  }
  return result;
}

internal String8
lsp_frame_from_body(Arena *arena, String8 body)
{
  String8 result = push_str8f(arena, "Content-Length: %I64u\r\n\r\n%S", body.size, body);
  return result;
}

////////////////////////////////
//~ rad: Messages

internal LSP_Message
lsp_message_from_body(Arena *arena, String8 body)
{
  LSP_Message m = {0};
  m.params = &json_value_nil;
  m.result = &json_value_nil;
  m.error = &json_value_nil;
  JSON_ParseResult parsed = json_parse(arena, body);
  if(parsed.ok && parsed.root->kind == JSON_ValueKind_Object)
  {
    JSON_Value *method = json_value_from_key(parsed.root, str8_lit("method"));
    JSON_Value *id = json_value_from_key(parsed.root, str8_lit("id"));
    B32 has_id = (id->kind == JSON_ValueKind_Number || id->kind == JSON_ValueKind_String);
    if(method->kind == JSON_ValueKind_String)
    {
      m.kind = (has_id ? LSP_MessageKind_Request : LSP_MessageKind_Notification);
      m.method = method->string;
      m.params = json_value_from_key(parsed.root, str8_lit("params"));
    }
    else if(!json_value_is_nil(id))
    {
      // rad: a response (its id may be null: an answer to a message the
      // server could not read)
      m.kind = LSP_MessageKind_Response;
      m.result = json_value_from_key(parsed.root, str8_lit("result"));
      m.error = json_value_from_key(parsed.root, str8_lit("error"));
      if(!json_value_is_nil(m.error))
      {
        m.result = &json_value_nil;
      }
    }
    if(has_id)
    {
      m.id_text = json_text_from_value(arena, id);
      m.id = (id->kind == JSON_ValueKind_Number ? json_s64_from_value(id) : 0);
    }
  }
  return m;
}

internal String8
lsp_request_body(Arena *arena, S64 id, String8 method, String8 params_json)
{
  Temp scratch = scratch_begin(&arena, 1);
  String8 method_json = json_escaped_from_str8(scratch.arena, method);
  String8 result = (params_json.size != 0 ?
                    push_str8f(arena, "{\"jsonrpc\":\"2.0\",\"id\":%I64d,\"method\":%S,\"params\":%S}", id, method_json, params_json) :
                    push_str8f(arena, "{\"jsonrpc\":\"2.0\",\"id\":%I64d,\"method\":%S}", id, method_json));
  scratch_end(scratch);
  return result;
}

internal String8
lsp_notification_body(Arena *arena, String8 method, String8 params_json)
{
  Temp scratch = scratch_begin(&arena, 1);
  String8 method_json = json_escaped_from_str8(scratch.arena, method);
  String8 result = (params_json.size != 0 ?
                    push_str8f(arena, "{\"jsonrpc\":\"2.0\",\"method\":%S,\"params\":%S}", method_json, params_json) :
                    push_str8f(arena, "{\"jsonrpc\":\"2.0\",\"method\":%S}", method_json));
  scratch_end(scratch);
  return result;
}

internal String8
lsp_response_body(Arena *arena, String8 id_json, String8 result_json)
{
  String8 result = push_str8f(arena, "{\"jsonrpc\":\"2.0\",\"id\":%S,\"result\":%S}",
                              id_json.size != 0 ? id_json : str8_lit("null"),
                              result_json.size != 0 ? result_json : str8_lit("null"));
  return result;
}

////////////////////////////////
//~ rad: Client Threads

internal void
lsp_client_reader_entry_point(void *p)
{
  LSP_Client *c = (LSP_Client *)p;
  ThreadNameF("lsp_reader");
  Arena *arena = arena_alloc();
  U64 buffer_size = KB(64);
  U8 *buffer = push_array_no_zero(arena, U8, buffer_size);
  LSP_FrameParser parser = {0};
  lsp_frame_parser_init(&parser);
  for(B32 done = 0; !done;)
  {
    U64 read_size = file_pipe_read(c->stdout_read, buffer, buffer_size);
    B32 any = 0;
    if(read_size != 0)
    {
      lsp_frame_parser_push(&parser, str8(buffer, read_size));
    }
    MutexScope(c->mutex)
    {
      c->bytes_received += read_size;
      for(String8 body = {0}; lsp_frame_parser_next(&parser, c->inbox_arena, &body);)
      {
        str8_list_push(c->inbox_arena, &c->inbox, body);
        any = 1;
      }
      c->framing_failed = parser.failed;
    }
    done = (read_size == 0 || parser.failed);
    if(any && c->wakeup != 0)
    {
      c->wakeup();
    }
  }
  MutexScope(c->mutex)
  {
    c->server_ended = 1;
  }
  if(c->wakeup != 0)
  {
    c->wakeup();
  }
  lsp_frame_parser_release(&parser);
  arena_release(arena);
}

internal void
lsp_client_writer_entry_point(void *p)
{
  LSP_Client *c = (LSP_Client *)p;
  ThreadNameF("lsp_writer");
  for(B32 stop = 0; !stop;)
  {
    String8List out = {0};
    MutexScope(c->mutex)
    {
      for(;c->outbox.node_count == 0 && !c->writer_stop;)
      {
        cond_var_wait(c->outbox_cv, c->mutex, max_U64);
      }
      stop = (c->writer_stop && c->outbox.node_count == 0);

      // rad: take the outbox; senders go on in the other arena (the one
      // this thread finished writing last time)
      out = c->outbox;
      MemoryZeroStruct(&c->outbox);
      c->outbox_arena_idx ^= 1;
      arena_clear(c->outbox_arenas[c->outbox_arena_idx]);
    }
    for(String8Node *n = out.first; n != 0; n = n->next)
    {
      for(U64 off = 0; off < n->string.size;)
      {
        U64 written = file_pipe_write(c->stdin_write, n->string.str + off, n->string.size - off);
        off = (written == 0 ? n->string.size : off + written);
      }
    }
  }
}

////////////////////////////////
//~ rad: Client Functions

internal LSP_Client *
lsp_client_launch(LSP_ClientParams *params)
{
  Arena *arena = arena_alloc();
  LSP_Client *c = push_array(arena, LSP_Client, 1);
  c->arena = arena;
  c->wakeup = params->wakeup;
  c->next_request_id = 1;
  c->mutex = mutex_alloc();
  c->outbox_cv = cond_var_alloc();
  c->inbox_arena = arena_alloc();
  for EachElement(idx, c->outbox_arenas)
  {
    c->outbox_arenas[idx] = arena_alloc();
  }

  //- rad: pipes: the server reads stdin_pipe and writes stdout_pipe; rad
  // keeps the other ends (not inherited)
  FilePair stdin_pipe = file_pipe_make(1, 0);
  FilePair stdout_pipe = file_pipe_make(0, 1);
  File stderr_file = {0};
  if(params->stderr_path.size != 0)
  {
    stderr_file = file_open(AccessFlag_Write|AccessFlag_ShareRead|AccessFlag_Inherited, params->stderr_path);
  }
  c->group = process_group_make(1);
  {
    ProcessLaunchParams launch = {0};
    launch.cmd_line = params->cmd_line;
    launch.path = params->path;
    launch.inherit_env = 1;
    launch.consoleless = 1;
    launch.process_group = c->group;
    launch.stdin_file = stdin_pipe.read;
    launch.stdout_file = stdout_pipe.write;
    launch.stderr_file = stderr_file;
    c->process = process_launch(&launch);
  }
  file_close(stdin_pipe.read);
  file_close(stdout_pipe.write);
  if(!file_match(stderr_file, file_zero()))
  {
    file_close(stderr_file);
  }
  c->stdin_write = stdin_pipe.write;
  c->stdout_read = stdout_pipe.read;

  //- rad: started -> the threads; else nothing
  if(process_match(c->process, process_zero()))
  {
    file_close(c->stdin_write);
    file_close(c->stdout_read);
    process_group_close(c->group);
    for EachElement(idx, c->outbox_arenas)
    {
      arena_release(c->outbox_arenas[idx]);
    }
    arena_release(c->inbox_arena);
    cond_var_release(c->outbox_cv);
    mutex_release(c->mutex);
    arena_release(arena);
    c = 0;
  }
  else
  {
    c->reader_thread = thread_launch(lsp_client_reader_entry_point, c);
    c->writer_thread = thread_launch(lsp_client_writer_entry_point, c);
  }
  return c;
}

internal void
lsp_client_release(LSP_Client *c, U64 exit_wait_us)
{
  // NOTE(rad): the caller sends `shutdown` + `exit` first if it wants a
  // clean exit. Here: let the writer flush, give the server `exit_wait_us`
  // to end, then end its group (the server and anything it started).
  if(c == 0)
  {
    return;
  }
  U64 endt_us = now_time_us() + exit_wait_us;
  MutexScope(c->mutex)
  {
    c->writer_stop = 1;
  }
  cond_var_broadcast(c->outbox_cv);
  B32 writer_joined = thread_join(c->writer_thread, endt_us);
  B32 process_joined = process_join(c->process, endt_us, 0);
  process_group_close(c->group);
  if(!process_joined)
  {
    process_detach(c->process);
  }
  thread_join(c->reader_thread, max_U64);
  if(!writer_joined)
  {
    thread_join(c->writer_thread, max_U64);
  }
  file_close(c->stdin_write);
  file_close(c->stdout_read);
  for EachElement(idx, c->outbox_arenas)
  {
    arena_release(c->outbox_arenas[idx]);
  }
  arena_release(c->inbox_arena);
  cond_var_release(c->outbox_cv);
  mutex_release(c->mutex);
  arena_release(c->arena);
}

internal B32
lsp_client_is_alive(LSP_Client *c)
{
  B32 result = 0;
  if(c != 0)
  {
    MutexScope(c->mutex)
    {
      result = !c->server_ended;
    }
  }
  return result;
}

internal void
lsp_client_send__(LSP_Client *c, String8 body)
{
  MutexScope(c->mutex)
  {
    Arena *arena = c->outbox_arenas[c->outbox_arena_idx];
    str8_list_push(arena, &c->outbox, lsp_frame_from_body(arena, body));
    c->bytes_sent += body.size;
  }
  cond_var_signal(c->outbox_cv);
}

internal S64
lsp_client_request(LSP_Client *c, String8 method, String8 params_json)
{
  Temp scratch = scratch_begin(0, 0);
  S64 id = c->next_request_id;
  c->next_request_id += 1;
  lsp_client_send__(c, lsp_request_body(scratch.arena, id, method, params_json));
  scratch_end(scratch);
  return id;
}

internal void
lsp_client_notify(LSP_Client *c, String8 method, String8 params_json)
{
  Temp scratch = scratch_begin(0, 0);
  lsp_client_send__(c, lsp_notification_body(scratch.arena, method, params_json));
  scratch_end(scratch);
}

internal void
lsp_client_respond(LSP_Client *c, String8 id_json, String8 result_json)
{
  Temp scratch = scratch_begin(0, 0);
  lsp_client_send__(c, lsp_response_body(scratch.arena, id_json, result_json));
  scratch_end(scratch);
}

internal String8List
lsp_client_take_inbox(LSP_Client *c, Arena *arena)
{
  String8List result = {0};
  MutexScope(c->mutex)
  {
    for(String8Node *n = c->inbox.first; n != 0; n = n->next)
    {
      str8_list_push(arena, &result, push_str8_copy(arena, n->string));
    }
    MemoryZeroStruct(&c->inbox);
    arena_clear(c->inbox_arena);
  }
  return result;
}
