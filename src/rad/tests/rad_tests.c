// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

////////////////////////////////
//~ rad: Notes
//
// Console test program for the editor's text buffer:
// - the text layer's patched representation (base data + TXT_PatchList ->
//   TXT_Patched);
// - the text_buffer layer (TB_Buffer) on top of it: undo/redo, undo groups,
//   compaction, save, reload / "changed on disk", empty and large texts;
// - the json layer: parsing, escapes, errors, random round trips.
// Each test applies edits and compares the patched memory map and line map
// against a plain string that is edited the same way.
//
// Build: `build rad_tests`, run: `build\rad_tests.exe`. Exit code 0 = pass.

////////////////////////////////
//~ rad: Build Options

#define BUILD_TITLE "rad_tests"
#define BUILD_CONSOLE_INTERFACE 1

////////////////////////////////
//~ rad: Includes

#include "base/base_inc.h"
#include "win32/win32_inc.h"
#include "linux/linux_inc.h"
#include "artifact_cache/artifact_cache.h"
#include "content/content.h"
#include "text/text.h"
#include "text_buffer/text_buffer.h"
#include "json/json.h"
#include "lsp/lsp.h"

#include "base/base_inc.c"
#include "win32/win32_inc.c"
#include "linux/linux_inc.c"
#include "artifact_cache/artifact_cache.c"
#include "content/content.c"
#include "text/text.c"
#include "text_buffer/text_buffer.c"
#include "json/json.c"
#include "lsp/lsp.c"

////////////////////////////////
//~ rad: Helpers

global U64 test_failures = 0;
global U64 test_checks = 0;

internal TXT_TextInfo
test_text_info_from_data(Arena *arena, String8 data)
{
  // NOTE(rad): same line convention as the text layer: a line's range
  // excludes its '\n' and a '\r' right before it; there is always >= 1 line.
  TXT_TextInfo info = {0};
  U64 line_count = 1;
  for EachIndex(idx, data.size) { line_count += (data.str[idx] == '\n'); }
  info.lines_count = line_count;
  info.lines_ranges = push_array(arena, Rng1U64, line_count);
  U64 line_idx = 0;
  U64 line_start = 0;
  for(U64 idx = 0; idx <= data.size; idx += 1)
  {
    if(idx == data.size || data.str[idx] == '\n')
    {
      U64 line_end = idx;
      if(line_end > line_start && data.str[line_end-1] == '\r') { line_end -= 1; }
      info.lines_ranges[line_idx] = r1u64(line_start, line_end);
      info.lines_max_size = Max(info.lines_max_size, line_end - line_start);
      line_idx += 1;
      line_start = idx+1;
    }
  }
  return info;
}

internal String8
test_string_replace(Arena *arena, String8 s, Rng1U64 range, String8 replace)
{
  String8List parts = {0};
  str8_list_push(arena, &parts, str8_prefix(s, range.min));
  str8_list_push(arena, &parts, replace);
  str8_list_push(arena, &parts, str8_skip(s, range.max));
  return str8_list_join(arena, &parts, 0);
}

internal B32
test_check_patched(char *test_name, U64 step, TXT_Patched *patched, String8 expected)
{
  Temp scratch = scratch_begin(0, 0);
  B32 good = 1;

  //- rad: bytes
  String8 actual = memory_map_data_from_range(scratch.arena, &patched->memory_map, r1u64(0, patched->size));
  test_checks += 1;
  if(patched->size != expected.size || !str8_match(actual, expected, 0))
  {
    good = 0;
    printf("FAIL %s step %I64u: bytes differ (size %I64u, expected %I64u)\n", test_name, step, patched->size, expected.size);
  }

  //- rad: lines
  TXT_TextInfo expected_info = test_text_info_from_data(scratch.arena, expected);
  test_checks += 1;
  if(patched->line_map.total_line_count != expected_info.lines_count)
  {
    good = 0;
    printf("FAIL %s step %I64u: line count %I64u, expected %I64u\n", test_name, step, patched->line_map.total_line_count, expected_info.lines_count);
  }
  for(U64 line_num = 1; good && line_num <= expected_info.lines_count; line_num += 1)
  {
    Rng1U64 actual_range = txt_range_from_line_num(&patched->line_map, line_num);
    Rng1U64 expected_range = expected_info.lines_ranges[line_num-1];
    test_checks += 1;
    if(actual_range.min != expected_range.min || actual_range.max != expected_range.max)
    {
      good = 0;
      printf("FAIL %s step %I64u: line %I64u range [%I64u, %I64u), expected [%I64u, %I64u)\n", test_name, step, line_num, actual_range.min, actual_range.max, expected_range.min, expected_range.max);
    }
  }

  //- rad: offset -> line number
  for(U64 line_num = 1; good && line_num <= expected_info.lines_count; line_num += 1)
  {
    Rng1U64 expected_range = expected_info.lines_ranges[line_num-1];
    for(U64 off = expected_range.min; good && off <= expected_range.max; off += 1)
    {
      U64 actual_line_num = txt_line_num_from_off(&patched->line_map, off);
      test_checks += 1;
      if(actual_line_num != line_num)
      {
        good = 0;
        printf("FAIL %s step %I64u: offset %I64u -> line %I64u, expected %I64u\n", test_name, step, off, actual_line_num, line_num);
      }
    }
  }

  if(!good)
  {
    test_failures += 1;
  }
  scratch_end(scratch);
  return good;
}

typedef struct TestEdit TestEdit;
struct TestEdit
{
  U64 min;
  U64 max;
  char *replace;
};

internal void
test_edits(char *test_name, char *base_cstr, TestEdit *edits, U64 edits_count)
{
  Temp scratch = scratch_begin(0, 0);
  String8 base = str8_cstring(base_cstr);
  TXT_TextInfo info = test_text_info_from_data(scratch.arena, base);
  TXT_PatchList patches = {0};
  String8 expected = base;
  for EachIndex(idx, edits_count)
  {
    Rng1U64 range = r1u64(edits[idx].min, edits[idx].max);
    String8 replace = str8_cstring(edits[idx].replace);
    txt_patch_list_push_new(scratch.arena, &patches, range, replace);
    expected = test_string_replace(scratch.arena, expected, range, replace);
    TXT_Patched patched = txt_patched_from_info_data_patches(scratch.arena, &info, base, &patches);
    if(!test_check_patched(test_name, idx, &patched, expected))
    {
      break;
    }
  }
  scratch_end(scratch);
}

////////////////////////////////
//~ rad: Real Text Info Test
//
// Same checks, but the base TXT_TextInfo comes from the real (async) text
// layer, with its real line ranges and token points.

internal void
test_edits_real_info(char *test_name, char *base_cstr, TXT_LangKind lang, TestEdit *edits, U64 edits_count)
{
  Temp scratch = scratch_begin(0, 0);
  String8 base = str8_cstring(base_cstr);
  C_Key key = c_key_make(c_root_alloc(), c_id_make(0, 1));
  Arena *data_arena = arena_alloc();
  String8 data_copy = push_str8_copy(data_arena, base);
  c_submit_data(key, &data_arena, data_copy);
  TXT_TextInfo info = {0};
  U128 hash = {0};
  String8 data = {0};
  Access *access = access_open();
  for(U64 start_us = now_time_us(); now_time_us() - start_us < 10000000;)
  {
    info = txt_text_info_from_key_lang(access, key, lang, &hash);
    if(info.lines_count != 0) { break; }
    access_close(access);
    sleep_ms(1);
    access = access_open();
  }
  data = c_data_from_hash(access, hash);
  if(info.lines_count == 0)
  {
    printf("FAIL %s: text info never became ready\n", test_name);
    test_failures += 1;
  }
  else
  {
    //- rad: first, the unpatched info must match our line convention
    TXT_PatchList patches = {0};
    TXT_Patched patched0 = txt_patched_from_info_data_patches(scratch.arena, &info, data, &patches);
    B32 good = test_check_patched(test_name, max_U64, &patched0, base);
    String8 expected = base;
    for(U64 idx = 0; good && idx < edits_count; idx += 1)
    {
      Rng1U64 range = r1u64(edits[idx].min, edits[idx].max);
      String8 replace = str8_cstring(edits[idx].replace);
      txt_patch_list_push_new(scratch.arena, &patches, range, replace);
      expected = test_string_replace(scratch.arena, expected, range, replace);
      TXT_Patched patched = txt_patched_from_info_data_patches(scratch.arena, &info, data, &patches);
      good = test_check_patched(test_name, idx, &patched, expected);
    }
  }
  access_close(access);
  scratch_end(scratch);
}

////////////////////////////////
//~ rad: Random Edit Test

internal U64
test_rand(U64 *state)
{
  // xorshift64*
  U64 x = *state;
  x ^= x >> 12; x ^= x << 25; x ^= x >> 27;
  *state = x;
  return x * 2685821657736338717ull;
}

internal String8
test_random_text(Arena *arena, U64 *rng, U64 lines_count, B32 crlf)
{
  // rad: random text with short lines of lowercase letters
  String8List parts = {0};
  for EachIndex(line_idx, lines_count)
  {
    U64 len = test_rand(rng)%40;
    U8 *line = push_array(arena, U8, len);
    for EachIndex(c, len) { line[c] = (U8)('a' + test_rand(rng)%26); }
    str8_list_push(arena, &parts, str8(line, len));
    str8_list_push(arena, &parts, crlf ? str8_lit("\r\n") : str8_lit("\n"));
  }
  return str8_list_join(arena, &parts, 0);
}

internal Rng1U64
test_random_range(U64 *rng, String8 text)
{
  // rad: a range on character boundaries (never splits "\r\n")
  U64 a = text.size ? test_rand(rng)%(text.size+1) : 0;
  U64 len = (test_rand(rng)%4 == 0) ? test_rand(rng)%60 : (test_rand(rng)%3);
  U64 b = Min(text.size, a + len);
  if(a > 0 && a < text.size && text.str[a] == '\n' && text.str[a-1] == '\r') { a -= 1; }
  if(b > 0 && b < text.size && text.str[b] == '\n' && text.str[b-1] == '\r') { b += 1; }
  return r1u64(a, b);
}

internal String8
test_random_snippet(U64 *rng, B32 crlf)
{
  String8 snippets[] =
  {
    str8_lit("x"), str8_lit("hello"), str8_lit("\n"), str8_lit("\n\n"), str8_lit("a\nb"),
    str8_lit("\nfoo\nbar\n"), str8_lit(""), str8_lit("    "), str8_lit("fn main() {\n}\n"),
  };
  String8 crlf_snippets[] =
  {
    str8_lit("x"), str8_lit("hello"), str8_lit("\r\n"), str8_lit("\r\n\r\n"), str8_lit("a\r\nb"),
    str8_lit("\r\nfoo\r\nbar\r\n"), str8_lit(""), str8_lit("    "), str8_lit("fn main() {\r\n}\r\n"),
  };
  return crlf ? crlf_snippets[test_rand(rng)%ArrayCount(crlf_snippets)] : snippets[test_rand(rng)%ArrayCount(snippets)];
}

internal void
test_random_edits(char *test_name, U64 seed, U64 edits_count, B32 crlf)
{
  Temp scratch = scratch_begin(0, 0);
  U64 rng = seed;
  String8 base = test_random_text(scratch.arena, &rng, 200, crlf);
  TXT_TextInfo info = test_text_info_from_data(scratch.arena, base);
  TXT_PatchList patches = {0};
  String8 expected = base;
  for EachIndex(edit_idx, edits_count)
  {
    Rng1U64 range = test_random_range(&rng, expected);
    String8 replace = test_random_snippet(&rng, crlf);
    txt_patch_list_push_new(scratch.arena, &patches, range, replace);
    expected = test_string_replace(scratch.arena, expected, range, replace);
    TXT_Patched patched = txt_patched_from_info_data_patches(scratch.arena, &info, base, &patches);
    if(!test_check_patched(test_name, edit_idx, &patched, expected))
    {
      printf("  last edit: [%I64u, %I64u) <- \"%.*s\"\n", range.min, range.max, str8_varg(escaped_from_raw_str8(scratch.arena, replace)));
      break;
    }
  }
  scratch_end(scratch);
}

////////////////////////////////
//~ rad: Text Buffer Test Harness
//
// Drives TB_Buffer like the editor does: a fake "file stream" key provides
// the source text, edits go through tb_buffer_edit, compaction runs through
// tb_buffer_tick with a fake clock (the real async lexer still runs).

typedef struct TestBuf TestBuf;
struct TestBuf
{
  TB_Buffer *buffer;
  C_Key source_key;
  TXT_LangKind lang;
  U64 now_us;
};

internal void
test_expect(char *test_name, char *what, B32 cond)
{
  test_checks += 1;
  if(!cond)
  {
    printf("FAIL %s: %s\n", test_name, what);
    test_failures += 1;
  }
}

internal U128
test_buf_submit_source(TestBuf *t, String8 text)
{
  // rad: like the file stream: submit the bytes, then wait until they are lexed
  Arena *data_arena = arena_alloc();
  String8 copy = push_str8_copy(data_arena, text);
  U128 hash = c_submit_data(t->source_key, &data_arena, copy);
  B32 ready = 0;
  for(U64 start_us = now_time_us(); !ready && now_time_us() - start_us < 10000000;)
  {
    Access *access = access_open();
    ready = (txt_text_info_from_hash_lang(access, hash, t->lang).lines_count != 0);
    access_close(access);
    if(!ready) { sleep_ms(1); }
  }
  test_expect("source", "source text got lexed", ready);
  return hash;
}

internal void
test_buf_observe(TestBuf *t, U128 hash)
{
  Access *access = access_open();
  tb_buffer_observe_source(t->buffer, access, hash);
  access_close(access);
}

internal TestBuf
test_buf_open(String8 text, TXT_LangKind lang)
{
  TestBuf t = {0};
  t.lang = lang;
  t.source_key = c_key_make(c_root_alloc(), c_id_make(0, 1));
  t.buffer = tb_buffer_alloc(lang);
  U128 hash = test_buf_submit_source(&t, text);
  test_buf_observe(&t, hash);
  return t;
}

internal void
test_buf_close(TestBuf *t)
{
  tb_buffer_release(t->buffer);
  c_root_release(t->source_key.root);
}

internal void
test_buf_tick(TestBuf *t, U64 advance_us)
{
  t->now_us += advance_us;
  Access *access = access_open();
  tb_buffer_tick(t->buffer, access, t->now_us);
  access_close(access);
}

internal void
test_buf_settle(char *test_name, TestBuf *t)
{
  // rad: tick until every patch is compacted into the base
  for(U64 start_us = now_time_us(); tb_buffer_is_busy(t->buffer) && now_time_us() - start_us < 10000000;)
  {
    test_buf_tick(t, TB_COMPACT_PAUSE_US);
    if(tb_buffer_is_busy(t->buffer)) { sleep_ms(1); }
  }
  test_expect(test_name, "compaction finished", !tb_buffer_is_busy(t->buffer));
}

internal B32
test_buf_check(char *test_name, U64 step, TestBuf *t, String8 expected)
{
  Temp scratch = scratch_begin(0, 0);
  Access *access = access_open();
  TB_Text text = tb_text_from_buffer(scratch.arena, access, t->buffer);
  B32 good = test_check_patched(test_name, step, &text.patched, expected);
  access_close(access);
  scratch_end(scratch);
  return good;
}

internal void
test_buf_edit(TestBuf *t, U64 min, U64 max, String8 replace)
{
  Temp scratch = scratch_begin(0, 0);
  Access *access = access_open();
  TB_Text text = tb_text_from_buffer(scratch.arena, access, t->buffer);
  tb_buffer_edit(t->buffer, &text.patched, r1u64(min, max), replace);
  access_close(access);
  scratch_end(scratch);
}

internal B32
test_buf_save(TestBuf *t, String8 path)
{
  Access *access = access_open();
  B32 good = tb_buffer_save(t->buffer, access, path);
  access_close(access);
  return good;
}

////////////////////////////////
//~ rad: Text Buffer Tests

internal void
test_buffer_random(char *test_name, U64 seed, U64 ops_count, B32 crlf)
{
  // rad: random edits / undos / redos / ticks / saves against a model: the
  // list of texts at each history position (each edit its own undo record)
  // (also: a marker '@' that edits never touch; its offset is mapped through
  // the change log after every op, like another view's cursor, and must
  // always land on the '@')
  Temp scratch = scratch_begin(0, 0);
  U64 rng = seed;
  String8 base = test_random_text(scratch.arena, &rng, 100, crlf);
  {
    U64 at = test_rand(&rng)%(base.size+1);
    if(at > 0 && at < base.size && base.str[at] == '\n' && base.str[at-1] == '\r') { at -= 1; }
    base = push_str8f(scratch.arena, "%S@%S", str8_prefix(base, at), str8_skip(base, at));
  }
  TestBuf t = test_buf_open(base, TXT_LangKind_Rust);
  U64 marker_off = str8_find_needle(base, 0, str8_lit("@"), 0);
  U64 marker_version = t.buffer->version;
  String8 save_path = push_str8f(scratch.arena, "%S/rad_tests_save.tmp", get_process_info()->binary_path);
  String8 *states = push_array(scratch.arena, String8, ops_count+1);
  U64 states_count = 1;
  U64 pos = 0;
  S64 saved_pos = 0;
  states[0] = base;
  U64 last_version = t.buffer->version;
  for EachIndex(op_idx, ops_count)
  {
    U64 r = test_rand(&rng)%100;
    B32 changed = 0;
    if(r < 60)
    {
      Rng1U64 range = test_random_range(&rng, states[pos]);
      String8 replace = test_random_snippet(&rng, crlf);
      U64 at = str8_find_needle(states[pos], 0, str8_lit("@"), 0);
      if(range.min <= at && at < range.max) { range.max = range.min; }
      if(range.min == range.max && replace.size == 0) { replace = str8_lit("x"); }
      tb_buffer_close_undo_group(t.buffer);
      test_buf_edit(&t, range.min, range.max, replace);
      if(saved_pos > (S64)pos) { saved_pos = -1; }
      states[pos+1] = test_string_replace(scratch.arena, states[pos], range, replace);
      pos += 1;
      states_count = pos+1;
      changed = 1;
    }
    else if(r < 75)
    {
      B32 did = tb_buffer_undo(t.buffer, 0);
      test_expect(test_name, "undo available exactly when there is history", did == (pos > 0));
      if(did) { pos -= 1; changed = 1; }
    }
    else if(r < 87)
    {
      B32 did = tb_buffer_redo(t.buffer, 0);
      test_expect(test_name, "redo available exactly when there is a redo tail", did == (pos+1 < states_count));
      if(did) { pos += 1; changed = 1; }
    }
    else if(r < 95)
    {
      test_buf_tick(&t, test_rand(&rng)%(2*TB_COMPACT_PAUSE_US));
    }
    else if(r < 98)
    {
      test_buf_settle(test_name, &t);
    }
    else
    {
      test_expect(test_name, "save succeeded", test_buf_save(&t, save_path));
      test_expect(test_name, "saved bytes match", str8_match(data_from_file_path(scratch.arena, save_path), states[pos], 0));
      saved_pos = (S64)pos;
    }
    test_expect(test_name, "version moves on every text change, and only then", changed ? (t.buffer->version > last_version) : (t.buffer->version == last_version));
    test_expect(test_name, "dirty iff history position != saved position", tb_buffer_is_dirty(t.buffer) == ((S64)pos != saved_pos));
    test_expect(test_name, "offsets map through the change log", tb_buffer_map_offset(t.buffer, marker_version, &marker_off));
    test_expect(test_name, "a mapped offset stays on its character", marker_off == str8_find_needle(states[pos], 0, str8_lit("@"), 0));
    marker_version = t.buffer->version;
    last_version = t.buffer->version;
    if(!test_buf_check(test_name, op_idx, &t, states[pos]))
    {
      break;
    }
  }
  test_buf_settle(test_name, &t);
  test_buf_check(test_name, max_U64, &t, states[pos]);
  delete_file_at_path(save_path);
  test_buf_close(&t);
  scratch_end(scratch);
}

internal void
test_buffer_undo_groups(void)
{
  char *name = "undo_groups";
  String8 base = str8_lit("fn x() {\n}\n");
  TestBuf t = test_buf_open(base, TXT_LangKind_Rust);

  //- rad: typing merges into one record
  test_buf_edit(&t, 8, 8, str8_lit("a"));
  test_buf_edit(&t, 9, 9, str8_lit("b"));
  test_buf_edit(&t, 10, 10, str8_lit("c"));
  test_buf_check(name, 0, &t, str8_lit("fn x() {abc\n}\n"));
  tb_buffer_undo(t.buffer, 0);
  test_buf_check(name, 1, &t, base);
  tb_buffer_redo(t.buffer, 0);
  test_buf_check(name, 2, &t, str8_lit("fn x() {abc\n}\n"));

  //- rad: redo closed the group -> next typing is a new record
  test_buf_edit(&t, 11, 11, str8_lit("d"));
  tb_buffer_undo(t.buffer, 0);
  test_buf_check(name, 3, &t, str8_lit("fn x() {abc\n}\n"));
  tb_buffer_undo(t.buffer, 0);
  test_buf_check(name, 4, &t, base);

  //- rad: a line break is its own record and ends the group
  test_buf_edit(&t, 8, 8, str8_lit("a"));
  test_buf_edit(&t, 9, 9, str8_lit("\n"));
  test_buf_edit(&t, 10, 10, str8_lit("b"));
  test_buf_check(name, 5, &t, str8_lit("fn x() {a\nb\n}\n"));
  tb_buffer_undo(t.buffer, 0);
  test_buf_check(name, 6, &t, str8_lit("fn x() {a\n\n}\n"));
  tb_buffer_undo(t.buffer, 0);
  test_buf_check(name, 7, &t, str8_lit("fn x() {a\n}\n"));
  tb_buffer_undo(t.buffer, 0);
  test_buf_check(name, 8, &t, base);

  //- rad: backspacing merges
  test_buf_edit(&t, 8, 8, str8_lit("abcd"));
  tb_buffer_close_undo_group(t.buffer);
  test_buf_edit(&t, 11, 12, str8_lit(""));
  test_buf_edit(&t, 10, 11, str8_lit(""));
  test_buf_edit(&t, 9, 10, str8_lit(""));
  test_buf_check(name, 9, &t, str8_lit("fn x() {a\n}\n"));
  tb_buffer_undo(t.buffer, 0);
  test_buf_check(name, 10, &t, str8_lit("fn x() {abcd\n}\n"));

  //- rad: forward deleting merges
  test_buf_edit(&t, 8, 9, str8_lit(""));
  test_buf_edit(&t, 8, 9, str8_lit(""));
  test_buf_edit(&t, 8, 9, str8_lit(""));
  test_buf_check(name, 11, &t, str8_lit("fn x() {d\n}\n"));
  tb_buffer_undo(t.buffer, 0);
  test_buf_check(name, 12, &t, str8_lit("fn x() {abcd\n}\n"));
  tb_buffer_undo(t.buffer, 0);
  test_buf_check(name, 13, &t, base);

  //- rad: typing over a selection merges with the typing after it
  test_buf_edit(&t, 3, 4, str8_lit("y"));
  test_buf_edit(&t, 4, 4, str8_lit("z"));
  test_buf_check(name, 14, &t, str8_lit("fn yz() {\n}\n"));
  tb_buffer_undo(t.buffer, 0);
  test_buf_check(name, 15, &t, base);

  //- rad: a pause ends the group
  test_buf_edit(&t, 8, 8, str8_lit("a"));
  test_buf_tick(&t, 0);
  test_buf_tick(&t, TB_UNDO_GROUP_PAUSE_US);
  test_buf_edit(&t, 9, 9, str8_lit("b"));
  tb_buffer_undo(t.buffer, 0);
  test_buf_check(name, 16, &t, str8_lit("fn x() {a\n}\n"));

  //- rad: groups survive compaction
  tb_buffer_close_undo_group(t.buffer);
  test_buf_edit(&t, 9, 9, str8_lit("b"));
  test_buf_edit(&t, 10, 10, str8_lit("c"));
  test_buf_settle(name, &t);
  test_buf_check(name, 17, &t, str8_lit("fn x() {abc\n}\n"));
  tb_buffer_undo(t.buffer, 0);
  test_buf_check(name, 18, &t, str8_lit("fn x() {a\n}\n"));
  tb_buffer_undo(t.buffer, 0);
  test_buf_check(name, 19, &t, base);
  test_expect(name, "clean after undoing everything", !tb_buffer_is_dirty(t.buffer));
  test_buf_close(&t);
}

internal void
test_buffer_disk(void)
{
  char *name = "disk";
  Temp scratch = scratch_begin(0, 0);
  String8 path = push_str8f(scratch.arena, "%S/rad_tests_disk.tmp", get_process_info()->binary_path);
  String8 a = str8_lit("one\ntwo\n");
  TestBuf t = test_buf_open(a, TXT_LangKind_Null);
  test_expect(name, "loads clean", tb_buffer_is_loaded(t.buffer) && !tb_buffer_is_dirty(t.buffer));
  test_buf_check(name, 0, &t, a);

  //- rad: external change while clean -> reload
  String8 b = str8_lit("one\ntwo\nthree\n");
  U64 version_before = t.buffer->version;
  test_buf_observe(&t, test_buf_submit_source(&t, b));
  test_expect(name, "clean buffer reloads", !t.buffer->changed_on_disk && t.buffer->version > version_before);
  test_buf_check(name, 1, &t, b);
  {
    U64 off = 4;
    test_expect(name, "offsets do not map across a reload", !tb_buffer_map_offset(t.buffer, version_before, &off) && off == 4);
    test_expect(name, "offsets map from the current version", tb_buffer_map_offset(t.buffer, t.buffer->version, &off) && off == 4);
    test_expect(name, "offsets do not map from the future", !tb_buffer_map_offset(t.buffer, t.buffer->version+1, &off));
  }

  //- rad: external change while dirty -> keep the edits, flag it
  test_buf_edit(&t, 0, 0, str8_lit("zero\n"));
  U128 c_hash = test_buf_submit_source(&t, str8_lit("changed\n"));
  test_buf_observe(&t, c_hash);
  test_expect(name, "dirty buffer flags the change", t.buffer->changed_on_disk && tb_buffer_is_dirty(t.buffer));
  test_buf_check(name, 2, &t, str8_lit("zero\none\ntwo\nthree\n"));

  //- rad: undo back to clean -> the waiting disk change loads
  tb_buffer_undo(t.buffer, 0);
  test_buf_observe(&t, c_hash);
  test_expect(name, "undo to clean loads the disk change", !t.buffer->changed_on_disk && !tb_buffer_is_dirty(t.buffer));
  test_buf_check(name, 3, &t, str8_lit("changed\n"));
  test_expect(name, "reload clears history", !tb_buffer_undo(t.buffer, 0));

  //- rad: save; the file stream then reports our own bytes -> not a change
  test_buf_edit(&t, 0, 0, str8_lit("saved "));
  test_expect(name, "save succeeded", test_buf_save(&t, path));
  test_expect(name, "clean after save", !tb_buffer_is_dirty(t.buffer));
  String8 on_disk = data_from_file_path(scratch.arena, path);
  test_expect(name, "saved bytes match", str8_match(on_disk, str8_lit("saved changed\n"), 0));
  test_buf_observe(&t, test_buf_submit_source(&t, on_disk));
  test_expect(name, "own save is not a change on disk", !t.buffer->changed_on_disk && !tb_buffer_is_dirty(t.buffer));
  test_buf_check(name, 4, &t, on_disk);

  //- rad: undo past the save -> dirty; redo back to it -> clean
  tb_buffer_undo(t.buffer, 0);
  test_expect(name, "dirty after undoing the saved edit", tb_buffer_is_dirty(t.buffer));
  tb_buffer_redo(t.buffer, 0);
  test_expect(name, "clean after redoing back to the save", !tb_buffer_is_dirty(t.buffer));

  //- rad: undo past the save, then a new edit -> the saved state is gone
  tb_buffer_undo(t.buffer, 0);
  test_buf_edit(&t, 0, 0, str8_lit("other "));
  tb_buffer_undo(t.buffer, 0);
  test_expect(name, "dirty forever once the saved state is dropped", tb_buffer_is_dirty(t.buffer));
  test_buf_check(name, 5, &t, str8_lit("changed\n"));

  delete_file_at_path(path);
  test_buf_close(&t);
  scratch_end(scratch);
}

internal void
test_buffer_empty(void)
{
  char *name = "empty";
  TestBuf t = test_buf_open(str8_lit(""), TXT_LangKind_Null);
  test_expect(name, "empty text loads", tb_buffer_is_loaded(t.buffer));
  test_buf_check(name, 0, &t, str8_lit(""));
  test_buf_edit(&t, 0, 0, str8_lit("x"));
  test_buf_settle(name, &t);
  test_buf_check(name, 1, &t, str8_lit("x"));
  test_buf_edit(&t, 0, 1, str8_lit(""));
  test_buf_settle(name, &t);
  test_buf_check(name, 2, &t, str8_lit(""));
  tb_buffer_undo(t.buffer, 0);
  test_buf_settle(name, &t);
  test_buf_check(name, 3, &t, str8_lit("x"));
  test_buf_close(&t);
}

internal void
test_buffer_large(void)
{
  // rad: ~2 MB text, many edits with compactions in between; the model is a
  // flat byte array edited in place
  char *name = "large";
  Temp scratch = scratch_begin(0, 0);
  U64 rng = 0x5DEECE66Dull;
  String8 base = test_random_text(scratch.arena, &rng, 100000, 0);
  U64 cap = base.size + 1000000;
  U8 *model = push_array_no_zero(scratch.arena, U8, cap);
  U64 model_size = base.size;
  MemoryCopy(model, base.str, base.size);
  TestBuf t = test_buf_open(base, TXT_LangKind_Null);
  for EachIndex(edit_idx, 2000)
  {
    Rng1U64 range = test_random_range(&rng, str8(model, model_size));
    String8 replace = test_random_snippet(&rng, 0);
    test_buf_edit(&t, range.min, range.max, replace);
    MemoryCopy(model + range.min + replace.size, model + range.max, model_size - range.max);
    MemoryCopy(model + range.min, replace.str, replace.size);
    model_size = model_size - dim_1u64(range) + replace.size;
    test_buf_tick(&t, test_rand(&rng)%(2*TB_COMPACT_PAUSE_US));
    if(edit_idx%500 == 499)
    {
      test_buf_settle(name, &t);
      if(!test_buf_check(name, edit_idx, &t, str8(model, model_size)))
      {
        break;
      }
    }
  }
  test_buf_close(&t);
  scratch_end(scratch);
}

////////////////////////////////
//~ rad: Rust Lexing Tests
//
// 1. Token kinds: the visible-range tokenizer on a source with every tricky
//    construct (lifetimes vs chars, raw strings, nested comments, multi-line
//    attributes, numbers).
// 2. Resume: the code view tokenizes from the first visible line, starting
//    from the token pt before it (strings/comments/attributes spanning
//    lines). Tokenizing any line that way must give the same kinds as
//    tokenizing the whole text, with the real whole-file token pts and with
//    the pts re-lexed after edits.

global char *test_rust_source =
"#![allow(dead_code)]\n"
"//! crate doc\n"
"use std::fmt;\n"
"\n"
"/// doc comment with 'quote and \"quote\n"
"#[derive(Debug, Clone)]\n"
"struct Holder<'a, T: 'static> {\n"
"    name: &'a str,\n"
"    raw: &'static str,\n"
"    value: T,\n"
"}\n"
"\n"
"#[cfg(all(\n"
"    feature = \"x\",\n"
"    not(test)\n"
"))]\n"
"fn multi() {}\n"
"\n"
"fn main() {\n"
"    let c = 'x';\n"
"    let e = '\\n';\n"
"    let q = '\\'';\n"
"    let u = '\\u{1F600}';\n"
"    let b = b'a';\n"
"    let s = \"tab\\t \\\"quoted\\\" \\\\\";\n"
"    let r = r\"C:\\path\\\";\n"
"    let r2 = r#\"has \"quotes\" inside\"#;\n"
"    let r3 = br##\"a \"# b\"##;\n"
"    let ml = \"first line\n"
"second line\";\n"
"    let raw_ml = r#\"line one \"\n"
"line two \"#;\n"
"    /* outer /* inner */ still outer */\n"
"    /* block\n"
"       comment */\n"
"    let n = 1_000_000u64 + 0xff_u8 as u64 + 2.5e10 as u64;\n"
"    for i in 0..10 { let _ = i; }\n"
"    'outer: loop { break 'outer; }\n"
"    let r#type = 1;\n"
"    println!(\"{}\", r#type);\n"
"}\n";

global char *test_token_kind_names[] =
{
  "Null", "Error", "Whitespace", "Keyword", "Identifier", "Numeric",
  "String", "Char", "Symbol", "LineComment", "BlockComment", "Meta",
};

typedef struct TestTokenExpect TestTokenExpect;
struct TestTokenExpect
{
  char *needle; // "<prefix>|<target>": target is the token, right after prefix
  TXT_TokenKind kind;
};

internal void
test_rust_tokens(void)
{
  Temp scratch = scratch_begin(0, 0);
  String8 src = str8_cstring(test_rust_source);
  TXT_TokenPt null_pt = {0};
  TXT_TokenArray tokens = txt_token_array_from_data(scratch.arena, TXT_LangKind_Rust, null_pt, src, 0, max_U64);

  //- rad: tokens tile the source; no error tokens
  {
    U64 next_off = 0;
    B32 tiles = 1;
    U64 error_count = 0;
    for EachIndex(idx, tokens.count)
    {
      tiles = (tiles && tokens.v[idx].range.min == next_off);
      next_off = tokens.v[idx].range.max;
      error_count += (tokens.v[idx].kind == TXT_TokenKind_Error);
    }
    test_checks += 1;
    if(!tiles || next_off != src.size || error_count != 0)
    {
      printf("FAIL rust_tokens: tiles %i, end %I64u of %I64u, %I64u error tokens\n", tiles, next_off, src.size, error_count);
      test_failures += 1;
    }
  }

  //- rad: expected tokens
  TestTokenExpect expects[] =
  {
    {"|#![allow(dead_code)]",                       TXT_TokenKind_Meta},
    {"|//! crate doc\n",                            TXT_TokenKind_LineComment},
    {"|/// doc comment with 'quote and \"quote\n",  TXT_TokenKind_LineComment},
    {"|#[derive(Debug, Clone)]",                    TXT_TokenKind_Meta},
    {"|struct",                                     TXT_TokenKind_Keyword},
    {"struct |Holder",                              TXT_TokenKind_Identifier},
    {"Holder<|'a",                                  TXT_TokenKind_Keyword},
    {"T: |'static",                                 TXT_TokenKind_Keyword},
    {"&'a |str",                                    TXT_TokenKind_Keyword},
    {"|#[cfg(all(\n    feature = \"x\",\n    not(test)\n))]", TXT_TokenKind_Meta},
    {"\n|fn",                                       TXT_TokenKind_Keyword},
    {"c = |'x'",                                    TXT_TokenKind_Char},
    {"e = |'\\n'",                                  TXT_TokenKind_Char},
    {"q = |'\\''",                                  TXT_TokenKind_Char},
    {"u = |'\\u{1F600}'",                           TXT_TokenKind_Char},
    {"b = |b'a'",                                   TXT_TokenKind_Char},
    {"s = |\"tab\\t \\\"quoted\\\" \\\\\"",         TXT_TokenKind_String},
    {"r = |r\"C:\\path\\\"",                        TXT_TokenKind_String},
    {"r2 = |r#\"has \"quotes\" inside\"#",          TXT_TokenKind_String},
    {"r3 = |br##\"a \"# b\"##",                     TXT_TokenKind_String},
    {"ml = |\"first line\nsecond line\"",           TXT_TokenKind_String},
    {"raw_ml = |r#\"line one \"\nline two \"#",     TXT_TokenKind_String},
    {"|/* outer /* inner */ still outer */",        TXT_TokenKind_BlockComment},
    {"|/* block\n       comment */",                TXT_TokenKind_BlockComment},
    {"|1_000_000u64",                               TXT_TokenKind_Numeric},
    {"|0xff_u8",                                    TXT_TokenKind_Numeric},
    {"|2.5e10",                                     TXT_TokenKind_Numeric},
    {"as |u64",                                     TXT_TokenKind_Keyword},
    {"in |0",                                       TXT_TokenKind_Numeric},
    {"0..|10",                                      TXT_TokenKind_Numeric},
    {"|'outer",                                     TXT_TokenKind_Keyword},
    {"break |'outer",                               TXT_TokenKind_Keyword},
    {"let |r#type",                                 TXT_TokenKind_Identifier},
    {"|println",                                    TXT_TokenKind_Identifier},
    {"(|\"{}\"",                                    TXT_TokenKind_String},
  };
  for EachElement(idx, expects)
  {
    String8 needle = str8_cstring(expects[idx].needle);
    U64 bar = str8_find_needle(needle, 0, str8_lit("|"), 0);
    String8 prefix = str8_prefix(needle, bar);
    String8 target = str8_skip(needle, bar+1);
    String8 joined = push_str8f(scratch.arena, "%S%S", prefix, target);
    U64 pos = str8_find_needle(src, 0, joined, 0);
    Rng1U64 want = r1u64(pos + prefix.size, pos + prefix.size + target.size);
    TXT_Token *got = 0;
    for EachIndex(t_idx, tokens.count)
    {
      if(tokens.v[t_idx].range.min <= want.min && want.min < tokens.v[t_idx].range.max)
      {
        got = &tokens.v[t_idx];
        break;
      }
    }
    test_checks += 1;
    if(pos >= src.size || got == 0 || got->kind != expects[idx].kind ||
       got->range.min != want.min || got->range.max != want.max)
    {
      String8 got_string = (got != 0) ? str8_substr(src, got->range) : str8_lit("(none)");
      printf("FAIL rust_tokens: `%.*s` expected %s [%I64u, %I64u), got %s [%I64u, %I64u) `%.*s`\n",
             str8_varg(target), test_token_kind_names[expects[idx].kind], want.min, want.max,
             got ? test_token_kind_names[got->kind] : "-", got ? got->range.min : 0, got ? got->range.max : 0,
             str8_varg(got_string));
      test_failures += 1;
    }
  }
  scratch_end(scratch);
}

internal B32
test_rust_check_resume(char *test_name, U64 step, TXT_Patched *patched, String8 text)
{
  Temp scratch = scratch_begin(0, 0);
  B32 good = 1;

  //- rad: kinds from tokenizing the whole text
  TXT_TokenKind *kinds = push_array(scratch.arena, TXT_TokenKind, text.size+1);
  TXT_TokenPt null_pt = {0};
  TXT_TokenArray all = txt_token_array_from_data(scratch.arena, TXT_LangKind_Rust, null_pt, text, 0, max_U64);
  for EachIndex(idx, all.count)
  {
    for(U64 off = all.v[idx].range.min; off < all.v[idx].range.max && off < text.size; off += 1)
    {
      kinds[off] = all.v[idx].kind;
    }
  }

  //- rad: each line, tokenized from its token pt context (as the code view)
  for(U64 line_num = 1; good && line_num <= patched->line_map.total_line_count; line_num += 1)
  {
    Rng1U64 line = txt_range_from_line_num(&patched->line_map, line_num);
    U64 ctx_num = txt_token_pt_num_from_off(&patched->token_pt_map, line.min);
    TXT_TokenPt ctx = txt_token_pt_from_num(&patched->token_pt_map, ctx_num);
    TXT_TokenArray part = txt_token_array_from_data(scratch.arena, TXT_LangKind_Rust, ctx, str8_substr(text, line), line.min, max_U64);
    test_checks += 1;
    for(U64 idx = 0; good && idx < part.count; idx += 1)
    {
      for(U64 off = Max(part.v[idx].range.min, line.min); off < Min(part.v[idx].range.max, line.max); off += 1)
      {
        if(kinds[off] != part.v[idx].kind)
        {
          String8 line_string = str8_substr(text, line);
          printf("FAIL %s step %I64u: line %I64u col %I64u is %s from the line's context (pt %s at %I64u), %s in the whole text: `%.*s`\n",
                 test_name, step, line_num, off-line.min+1, test_token_kind_names[part.v[idx].kind],
                 test_token_kind_names[ctx.kind], ctx.off, test_token_kind_names[kinds[off]], str8_varg(line_string));
          for(U64 num = 1; num <= patched->token_pt_map.total_pt_count; num += 1)
          {
            TXT_TokenPt dbg = txt_token_pt_from_num(&patched->token_pt_map, num);
            if(dbg.off + 80 > line.min && dbg.off < line.max + 40)
            {
              printf("  DEBUG pt %I64u: %s at %I64u aux %I64u\n", num, test_token_kind_names[dbg.kind], dbg.off, dbg.aux);
            }
          }
          for EachNode(rn, TXT_TokenPtMapRangeNode, patched->token_pt_map.first_range)
          {
            printf("  DEBUG range [%I64u, %I64u) delta %I64d\n", rn->num_range.min, rn->num_range.max, rn->delta);
          }
          good = 0;
          break;
        }
      }
    }
  }
  if(!good)
  {
    test_failures += 1;
  }
  scratch_end(scratch);
  return good;
}

typedef struct TestAnchorEdit TestAnchorEdit;
struct TestAnchorEdit
{
  char *anchor;     // edit at the first occurrence of this text
  U64 delete_size;  // bytes deleted there
  char *insert;     // then inserted there
};

internal U64 test_rand(U64 *state);

internal TestAnchorEdit *
test_rust_random_edits(Arena *arena, U64 seed, U64 count)
{
  // NOTE(rad): complete snippets (every token in them closed), each inserted
  // at the start of a random line of the test source whose first line
  // (anchor) is outside any multi-line token. the snippets end in a newline.
  char *snippets[] =
  {
    "let s = \"a\nb\";\n",
    "/* x\n y */\n",
    "let r = r#\"q \" \"#;\n",
    "fn f<'a>(x: &'a u8) -> char { 'z' }\n",
    "// note 'x \"y\n",
    "#[cfg(any(\n    a,\n    b))]\n",
    "let c = '\\'';\n",
    "let n = 0x_ff_u8 + 1_0.5e3 as u8;\n",
    "/* a /* b */ c */\n",
    "let b = br##\"#\"\"##;\n",
    "'l: loop { break 'l; }\n",
  };
  char *anchors[] =
  {
    "use std", "#[derive", "struct Holder", "    value", "fn multi",
    "fn main", "    let c", "    let s =", "    let r2", "    let n =",
    "    for i", "    'outer", "    println", "}\n", "    raw:",
  };
  // NOTE(rad): `count` inserts, then the same snippets deleted again, last
  // first (as undo would); 2*count edits
  U64 state = seed;
  TestAnchorEdit *edits = push_array(arena, TestAnchorEdit, count*2);
  for EachIndex(idx, count)
  {
    edits[idx].anchor = anchors[test_rand(&state) % ArrayCount(anchors)];
    edits[idx].delete_size = 0;
    edits[idx].insert = snippets[test_rand(&state) % ArrayCount(snippets)];
    edits[count*2-1-idx].anchor = edits[idx].insert;
    edits[count*2-1-idx].delete_size = str8_cstring(edits[idx].insert).size;
    edits[count*2-1-idx].insert = "";
  }
  return edits;
}

internal void
test_rust_resume(char *test_name, TestAnchorEdit *edits, U64 edits_count)
{
  Temp scratch = scratch_begin(0, 0);
  String8 base = str8_cstring(test_rust_source);
  C_Key key = c_key_make(c_root_alloc(), c_id_make(0, 1));
  Arena *data_arena = arena_alloc();
  String8 data_copy = push_str8_copy(data_arena, base);
  c_submit_data(key, &data_arena, data_copy);
  TXT_TextInfo info = {0};
  U128 hash = {0};
  Access *access = access_open();
  for(U64 start_us = now_time_us(); now_time_us() - start_us < 10000000;)
  {
    info = txt_text_info_from_key_lang(access, key, TXT_LangKind_Rust, &hash);
    if(info.lines_count != 0) { break; }
    access_close(access);
    sleep_ms(1);
    access = access_open();
  }
  String8 data = c_data_from_hash(access, hash);
  test_checks += 1;
  if(info.lines_count == 0)
  {
    printf("FAIL %s: text info never became ready\n", test_name);
    test_failures += 1;
  }
  else
  {
    TXT_PatchList patches = {0};
    TXT_Patched patched0 = txt_patched_from_info_data_patches(scratch.arena, &info, data, &patches);
    B32 good = test_rust_check_resume(test_name, max_U64, &patched0, base);
    String8 expected = base;
    for(U64 idx = 0; good && idx < edits_count; idx += 1)
    {
      U64 at = str8_find_needle(expected, 0, str8_cstring(edits[idx].anchor), 0);
      Rng1U64 range = r1u64(at, at + edits[idx].delete_size);
      String8 replace = str8_cstring(edits[idx].insert);
      txt_patch_list_push_new(scratch.arena, &patches, range, replace);
      expected = test_string_replace(scratch.arena, expected, range, replace);
      TXT_Patched patched = txt_patched_from_info_data_patches(scratch.arena, &info, data, &patches);
      String8 text = memory_map_data_from_range(scratch.arena, &patched.memory_map, r1u64(0, patched.size));
      good = test_check_patched(test_name, idx, &patched, expected);
      good = good && test_rust_check_resume(test_name, idx, &patched, text);
    }
  }
  access_close(access);
  scratch_end(scratch);
}

////////////////////////////////
//~ rad: JSON Tests

internal void
test_json_expect_text(char *test_name, char *input, char *expected_text)
{
  // NOTE(rad): parse, then compare the compact re-serialization
  Temp scratch = scratch_begin(0, 0);
  JSON_ParseResult parsed = json_parse(scratch.arena, str8_cstring(input));
  String8 text = json_text_from_value(scratch.arena, parsed.root);
  test_checks += 1;
  if(!parsed.ok || !str8_match(text, str8_cstring(expected_text), 0))
  {
    printf("FAIL %s: ok %i, got `%.*s`, expected `%s`\n", test_name, parsed.ok, str8_varg(text), expected_text);
    test_failures += 1;
  }
  scratch_end(scratch);
}

internal void
test_json_expect_error(char *test_name, String8 input, U64 expected_error_off)
{
  Temp scratch = scratch_begin(0, 0);
  JSON_ParseResult parsed = json_parse(scratch.arena, input);
  test_checks += 1;
  if(parsed.ok || !json_value_is_nil(parsed.root) || parsed.error_off != expected_error_off)
  {
    printf("FAIL %s: `%.*s` ok %i, error at %I64u, expected an error at %I64u\n", test_name, str8_varg(input), parsed.ok, parsed.error_off, expected_error_off);
    test_failures += 1;
  }
  scratch_end(scratch);
}

internal String8
test_json_random_text(Arena *arena, U64 *rng, U64 depth)
{
  // NOTE(rad): a random JSON value in compact form (the parser's writer
  // output for it must be the same text)
  String8 result = {0};
  U64 kind = test_rand(rng) % (depth < 4 ? 7 : 5);
  switch(kind)
  {
    case 0: {result = str8_lit("null");}break;
    case 1: {result = (test_rand(rng) & 1) ? str8_lit("true") : str8_lit("false");}break;
    case 2:
    {
      char *numbers[] = {"0", "-0", "7", "-12", "3.25", "1e9", "-2.5E-3", "123456789012", "0.5e+2"};
      result = str8_cstring(numbers[test_rand(rng) % ArrayCount(numbers)]);
    }break;
    case 3:
    case 4:
    {
      // rad: random code points, written the way the writer escapes them
      U8 raw[64];
      U64 raw_size = 0;
      U64 count = test_rand(rng) % 8;
      for EachIndex(idx, count)
      {
        U32 choices[] = {'a', 'Z', ' ', '"', '\\', '\n', '\t', 0x01, 0x1f, 0xe9, 0x4e2d, 0x1f600, '/'};
        raw_size += utf8_encode(raw + raw_size, choices[test_rand(rng) % ArrayCount(choices)]);
      }
      result = json_escaped_from_str8(arena, str8(raw, raw_size));
    }break;
    case 5:
    case 6:
    {
      B32 is_object = (kind == 6);
      String8List parts = {0};
      str8_list_push(arena, &parts, is_object ? str8_lit("{") : str8_lit("["));
      U64 count = test_rand(rng) % 5;
      for EachIndex(idx, count)
      {
        if(idx != 0) { str8_list_push(arena, &parts, str8_lit(",")); }
        if(is_object) { str8_list_pushf(arena, &parts, "\"k%I64u\":", idx); }
        str8_list_push(arena, &parts, test_json_random_text(arena, rng, depth+1));
      }
      str8_list_push(arena, &parts, is_object ? str8_lit("}") : str8_lit("]"));
      result = str8_list_join(arena, &parts, 0);
    }break;
  }
  return result;
}

internal void
test_json(void)
{
  //- rad: values, nesting, re-serialization
  test_json_expect_text("json_basic", " { \"a\" : [1, -2.5, true, false, null, \"x\"], \"b\": {} , \"c\":[ ] } ",
                        "{\"a\":[1,-2.5,true,false,null,\"x\"],\"b\":{},\"c\":[]}");
  test_json_expect_text("json_scalar_number", "\t42\r\n", "42");
  test_json_expect_text("json_scalar_string", "\"hi\"", "\"hi\"");
  test_json_expect_text("json_numbers", "[0,-0,10,1.5e3,2E-2,-0.25]", "[0,-0,10,1.5e3,2E-2,-0.25]");

  //- rad: escapes, unicode
  {
    Temp scratch = scratch_begin(0, 0);
    JSON_ParseResult parsed = json_parse(scratch.arena, str8_lit("[\"a\\\"b\\\\c\\/d\\n\\t\\r\\b\\f\", \"\\u00e9\\u4E2D\", \"\\ud83d\\ude00\", \"\\ud800x\", \"\\udc00\"]"));
    test_expect("json_escapes", "parses", parsed.ok);
    test_expect("json_escapes", "simple escapes", str8_match(json_string_from_value(json_value_from_index(parsed.root, 0)), str8_lit("a\"b\\c/d\n\t\r\b\f"), 0));
    test_expect("json_escapes", "\\u -> utf-8", str8_match(json_string_from_value(json_value_from_index(parsed.root, 1)), str8_lit("\xc3\xa9\xe4\xb8\xad"), 0));
    test_expect("json_escapes", "surrogate pair", str8_match(json_string_from_value(json_value_from_index(parsed.root, 2)), str8_lit("\xf0\x9f\x98\x80"), 0));
    test_expect("json_escapes", "lone high surrogate", str8_match(json_string_from_value(json_value_from_index(parsed.root, 3)), str8_lit("\xef\xbf\xbdx"), 0));
    test_expect("json_escapes", "lone low surrogate", str8_match(json_string_from_value(json_value_from_index(parsed.root, 4)), str8_lit("\xef\xbf\xbd"), 0));
    String8 escaped = json_escaped_from_str8(scratch.arena, str8_lit("q\"b\\s\n\x01\x7f\xc3\xa9"));
    test_expect("json_escapes", "writer escapes", str8_match(escaped, str8_lit("\"q\\\"b\\\\s\\n\\u0001\x7f\xc3\xa9\""), 0));
    scratch_end(scratch);
  }

  //- rad: lookups (a cargo compiler message, shortened); nil chains
  {
    Temp scratch = scratch_begin(0, 0);
    String8 text = str8_lit("{\"reason\":\"compiler-message\",\"message\":{\"rendered\":\"error[E0425]: cannot find value `x`\\n --> src\\\\main.rs:2:20\\n\",\"level\":\"error\",\"code\":{\"code\":\"E0425\"},\"spans\":[{\"file_name\":\"src\\\\main.rs\",\"line_start\":2,\"column_start\":20,\"is_primary\":true,\"expansion\":null}]}}");
    JSON_ParseResult parsed = json_parse(scratch.arena, text);
    JSON_Value *message = json_value_from_key(parsed.root, str8_lit("message"));
    JSON_Value *span = json_value_from_index(json_value_from_key(message, str8_lit("spans")), 0);
    test_expect("json_lookup", "parses", parsed.ok);
    test_expect("json_lookup", "reason", str8_match(json_string_from_value(json_value_from_key(parsed.root, str8_lit("reason"))), str8_lit("compiler-message"), 0));
    test_expect("json_lookup", "nested code", str8_match(json_string_from_value(json_value_from_key(json_value_from_key(message, str8_lit("code")), str8_lit("code"))), str8_lit("E0425"), 0));
    test_expect("json_lookup", "file name", str8_match(json_string_from_value(json_value_from_key(span, str8_lit("file_name"))), str8_lit("src\\main.rs"), 0));
    test_expect("json_lookup", "line", json_s64_from_value(json_value_from_key(span, str8_lit("line_start"))) == 2);
    test_expect("json_lookup", "column", json_s64_from_value(json_value_from_key(span, str8_lit("column_start"))) == 20);
    test_expect("json_lookup", "bool", json_b32_from_value(json_value_from_key(span, str8_lit("is_primary"))) == 1);
    test_expect("json_lookup", "null is not nil", !json_value_is_nil(json_value_from_key(span, str8_lit("expansion"))) && json_value_from_key(span, str8_lit("expansion"))->kind == JSON_ValueKind_Null);
    test_expect("json_lookup", "missing key -> nil", json_value_is_nil(json_value_from_key(span, str8_lit("nope"))));
    test_expect("json_lookup", "nil chain", json_value_is_nil(json_value_from_index(json_value_from_key(json_value_from_key(parsed.root, str8_lit("x")), str8_lit("y")), 3)));
    test_expect("json_lookup", "index past end -> nil", json_value_is_nil(json_value_from_index(json_value_from_key(message, str8_lit("spans")), 1)));
    test_expect("json_lookup", "wrong kind -> zero", json_s64_from_value(json_value_from_key(parsed.root, str8_lit("reason"))) == 0 && json_string_from_value(json_value_from_key(span, str8_lit("line_start"))).size == 0);
    scratch_end(scratch);
  }

  //- rad: malformed input -> ok = 0, nil root, error offset
  test_json_expect_error("json_error_empty", str8_lit(""), 0);
  test_json_expect_error("json_error_ws_only", str8_lit("  "), 2);
  test_json_expect_error("json_error_open_object", str8_lit("{"), 1);
  test_json_expect_error("json_error_trailing_comma", str8_lit("[1,]"), 3);
  test_json_expect_error("json_error_missing_colon", str8_lit("{\"a\" 1}"), 5);
  test_json_expect_error("json_error_unquoted_key", str8_lit("{a:1}"), 1);
  test_json_expect_error("json_error_leading_zero", str8_lit("01"), 0);
  test_json_expect_error("json_error_bare_dot", str8_lit("1."), 0);
  test_json_expect_error("json_error_bare_minus", str8_lit("-"), 0);
  test_json_expect_error("json_error_bad_exponent", str8_lit("1e"), 0);
  test_json_expect_error("json_error_open_string", str8_lit("\"abc"), 4);
  test_json_expect_error("json_error_bad_escape", str8_lit("\"a\\x\""), 3);
  test_json_expect_error("json_error_short_unicode", str8_lit("\"\\u12\""), 2);
  test_json_expect_error("json_error_control_char", str8_lit("\"a\nb\""), 2);
  test_json_expect_error("json_error_bad_literal", str8_lit("tru"), 0);
  test_json_expect_error("json_error_two_values", str8_lit("[1] 2"), 4);
  test_json_expect_error("json_error_missing_value", str8_lit("{\"a\":}"), 5);
  test_json_expect_error("json_error_backslash_at_end", str8_lit("\"\\"), 2);
  {
    Temp scratch = scratch_begin(0, 0);
    U64 depth = JSON_MAX_DEPTH+2;
    U8 *deep = push_array(scratch.arena, U8, depth*2);
    for EachIndex(idx, depth) { deep[idx] = '['; deep[depth*2-1-idx] = ']'; }
    JSON_ParseResult parsed = json_parse(scratch.arena, str8(deep, depth*2));
    test_expect("json_error_too_deep", "rejected", !parsed.ok && json_value_is_nil(parsed.root));
    String8 ok_deep = str8(deep + 3, depth*2 - 6);
    test_expect("json_max_depth", "accepted", json_parse(scratch.arena, ok_deep).ok);
    scratch_end(scratch);
  }

  //- rad: random documents: writer(parser(text)) == text; every prefix and
  // every one-byte corruption parses without reading past the input
  for(U64 seed = 1; seed <= 400; seed += 1)
  {
    Temp scratch = scratch_begin(0, 0);
    U64 rng = seed*0x9E3779B97F4A7C15ull;
    String8 text = push_str8_copy(scratch.arena, test_json_random_text(scratch.arena, &rng, 0));
    JSON_ParseResult parsed = json_parse(scratch.arena, text);
    String8 rewritten = json_text_from_value(scratch.arena, parsed.root);
    test_checks += 1;
    if(!parsed.ok || !str8_match(rewritten, text, 0))
    {
      printf("FAIL json_random seed %I64u: ok %i\n  in:  %.*s\n  out: %.*s\n", seed, parsed.ok, str8_varg(text), str8_varg(rewritten));
      test_failures += 1;
    }
    for(U64 prefix_size = 0; prefix_size < text.size; prefix_size += 1)
    {
      // NOTE(rad): a heap copy of exactly the prefix, so a read past the
      // end would hit unrelated memory (and the page heap, if enabled)
      U8 *copy = push_array_no_zero(scratch.arena, U8, prefix_size);
      MemoryCopy(copy, text.str, prefix_size);
      JSON_ParseResult p = json_parse(scratch.arena, str8(copy, prefix_size));
      if(p.ok)
      {
        // rad: only numbers have valid proper prefixes ("12" of "123")
        test_checks += 1;
        if(p.root->kind != JSON_ValueKind_Number)
        {
          printf("FAIL json_prefix seed %I64u: prefix %I64u of `%.*s` parsed\n", seed, prefix_size, str8_varg(text));
          test_failures += 1;
        }
      }
    }
    for(U64 idx = 0; idx < text.size; idx += 1)
    {
      U8 saved = text.str[idx];
      text.str[idx] = (U8)test_rand(&rng);
      JSON_ParseResult p = json_parse(scratch.arena, text);
      (void)p;
      text.str[idx] = saved;
    }
    scratch_end(scratch);
  }
}

////////////////////////////////
//~ rad: LSP Framing + Messages

internal void
test_lsp_expect_bodies(char *test_name, String8 stream, U64 chunk_size, String8 *expected, U64 expected_count, B32 expect_failed)
{
  // NOTE(rad): feeds `stream` in chunks of `chunk_size` bytes (0 = all at
  // once), taking messages after every chunk
  Temp scratch = scratch_begin(0, 0);
  LSP_FrameParser p = {0};
  lsp_frame_parser_init(&p);
  U64 got_count = 0;
  B32 good = 1;
  U64 step = (chunk_size == 0 ? Max(stream.size, 1) : chunk_size);
  for(U64 off = 0; off < stream.size; off += step)
  {
    lsp_frame_parser_push(&p, str8_substr(stream, r1u64(off, Min(off+step, stream.size))));
    for(String8 body = {0}; lsp_frame_parser_next(&p, scratch.arena, &body);)
    {
      if(got_count >= expected_count || !str8_match(body, expected[got_count], 0))
      {
        good = 0;
      }
      got_count += 1;
    }
  }
  test_checks += 1;
  if(!good || got_count != expected_count || !!p.failed != !!expect_failed)
  {
    printf("FAIL %s (chunks of %I64u): %I64u messages (expected %I64u), failed %i (expected %i), bodies %s\n",
           test_name, chunk_size, got_count, expected_count, p.failed, expect_failed, good ? "ok" : "differ");
    test_failures += 1;
  }
  lsp_frame_parser_release(&p);
  scratch_end(scratch);
}

internal void
test_lsp(void)
{
  Temp scratch = scratch_begin(0, 0);

  //- rad: framing: one message, then three in one stream, fed whole and
  // in every chunk size from 1 byte up
  {
    String8 bodies[] =
    {
      str8_lit("{\"jsonrpc\":\"2.0\",\"id\":1,\"result\":null}"),
      str8_lit("{\"x\":\"a\\r\\n\\r\\nb\"}"),
      str8_lit(""),
      str8_lit("{\"jsonrpc\":\"2.0\",\"method\":\"$/progress\",\"params\":{\"value\":{\"kind\":\"end\"}}}"),
    };
    String8List stream_parts = {0};
    str8_list_pushf(scratch.arena, &stream_parts, "Content-Length: %I64u\r\n\r\n%S", bodies[0].size, bodies[0]);
    str8_list_pushf(scratch.arena, &stream_parts, "content-length:%I64u\r\nContent-Type: application/vscode-jsonrpc; charset=utf-8\r\n\r\n%S", bodies[1].size, bodies[1]);
    str8_list_pushf(scratch.arena, &stream_parts, "Content-Length: 0\r\n\r\n");
    str8_list_pushf(scratch.arena, &stream_parts, "Content-Length:  %I64u \r\n\r\n%S", bodies[3].size, bodies[3]);
    String8 stream = str8_list_join(scratch.arena, &stream_parts, 0);
    test_lsp_expect_bodies("lsp_one", lsp_frame_from_body(scratch.arena, bodies[0]), 0, bodies, 1, 0);
    for(U64 chunk_size = 0; chunk_size <= stream.size; chunk_size += 1)
    {
      test_lsp_expect_bodies("lsp_stream", stream, chunk_size, bodies, ArrayCount(bodies), 0);
    }
    test_lsp_expect_bodies("lsp_partial", str8_prefix(stream, stream.size-1), 7, bodies, ArrayCount(bodies)-1, 0);
  }

  //- rad: framing: a large body, in reads of 64 KB (as the reader thread)
  {
    U64 size = MB(3) + 17;
    U8 *big = push_array(scratch.arena, U8, size);
    for EachIndex(idx, size) { big[idx] = 'a' + (idx % 26); }
    String8 bodies[] = {str8(big, size), str8_lit("{}")};
    String8List stream_parts = {0};
    str8_list_push(scratch.arena, &stream_parts, lsp_frame_from_body(scratch.arena, bodies[0]));
    str8_list_push(scratch.arena, &stream_parts, lsp_frame_from_body(scratch.arena, bodies[1]));
    test_lsp_expect_bodies("lsp_big", str8_list_join(scratch.arena, &stream_parts, 0), KB(64), bodies, 2, 0);
  }

  //- rad: framing: broken headers -> failed, nothing after them
  {
    String8 ok_body = str8_lit("{}");
    struct {char *name; char *stream; U64 expected_count;} cases[] =
    {
      {"lsp_no_length",      "Content-Type: x\r\n\r\n{}", 0},
      {"lsp_bad_length",     "Content-Length: 1x\r\n\r\n{}", 0},
      {"lsp_negative",       "Content-Length: -2\r\n\r\n{}", 0},
      {"lsp_no_colon",       "Content-Length 2\r\n\r\n{}", 0},
      {"lsp_empty_header",   "\r\n\r\n{}", 0},
      {"lsp_two_lengths",    "Content-Length: 2\r\nContent-Length: 2\r\n\r\n{}", 0},
      {"lsp_too_big",        "Content-Length: 999999999999\r\n\r\n{}", 0},
      {"lsp_after_good_one", "Content-Length: 2\r\n\r\n{}Content-Length: \r\n\r\n", 1},
    };
    for EachElement(idx, cases)
    {
      test_lsp_expect_bodies(cases[idx].name, str8_cstring(cases[idx].stream), 0, &ok_body, cases[idx].expected_count, 1);
      test_lsp_expect_bodies(cases[idx].name, str8_cstring(cases[idx].stream), 1, &ok_body, cases[idx].expected_count, 1);
    }

    // rad: a header that never ends
    U64 size = LSP_MAX_HEADER_SIZE + 64;
    U8 *junk = push_array(scratch.arena, U8, size);
    MemorySet(junk, 'x', size);
    test_lsp_expect_bodies("lsp_endless_header", str8(junk, size), 100, &ok_body, 0, 1);
  }

  //- rad: messages
  {
    LSP_Message m = lsp_message_from_body(scratch.arena, str8_lit("{\"jsonrpc\":\"2.0\",\"id\":7,\"result\":{\"a\":1}}"));
    test_expect("lsp_response", "kind", m.kind == LSP_MessageKind_Response);
    test_expect("lsp_response", "id", m.id == 7 && str8_match(m.id_text, str8_lit("7"), 0));
    test_expect("lsp_response", "result", json_s64_from_value(json_value_from_key(m.result, str8_lit("a"))) == 1);
    test_expect("lsp_response", "no error", json_value_is_nil(m.error));

    m = lsp_message_from_body(scratch.arena, str8_lit("{\"jsonrpc\":\"2.0\",\"id\":8,\"result\":null}"));
    test_expect("lsp_null_result", "kind", m.kind == LSP_MessageKind_Response && m.id == 8);
    test_expect("lsp_null_result", "result is null, not missing", !json_value_is_nil(m.result) && m.result->kind == JSON_ValueKind_Null);

    m = lsp_message_from_body(scratch.arena, str8_lit("{\"jsonrpc\":\"2.0\",\"id\":9,\"error\":{\"code\":-32601,\"message\":\"no\"}}"));
    test_expect("lsp_error", "kind", m.kind == LSP_MessageKind_Response && m.id == 9);
    test_expect("lsp_error", "error", json_s64_from_value(json_value_from_key(m.error, str8_lit("code"))) == -32601);
    test_expect("lsp_error", "no result", json_value_is_nil(m.result));

    m = lsp_message_from_body(scratch.arena, str8_lit("{\"jsonrpc\":\"2.0\",\"id\":\"ab\\\"c\",\"method\":\"window/workDoneProgress/create\",\"params\":{\"token\":\"t\"}}"));
    test_expect("lsp_server_request", "kind", m.kind == LSP_MessageKind_Request);
    test_expect("lsp_server_request", "method", str8_match(m.method, str8_lit("window/workDoneProgress/create"), 0));
    test_expect("lsp_server_request", "string id kept as JSON", str8_match(m.id_text, str8_lit("\"ab\\\"c\""), 0) && m.id == 0);
    test_expect("lsp_server_request", "params", str8_match(json_string_from_value(json_value_from_key(m.params, str8_lit("token"))), str8_lit("t"), 0));

    m = lsp_message_from_body(scratch.arena, str8_lit("{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/publishDiagnostics\",\"params\":{}}"));
    test_expect("lsp_notification", "kind", m.kind == LSP_MessageKind_Notification && m.id_text.size == 0);

    m = lsp_message_from_body(scratch.arena, str8_lit("[1,2]"));
    test_expect("lsp_not_a_message", "array", m.kind == LSP_MessageKind_Null);
    m = lsp_message_from_body(scratch.arena, str8_lit("{\"jsonrpc\":"));
    test_expect("lsp_not_a_message", "broken JSON", m.kind == LSP_MessageKind_Null);

    //- rad: what we send parses back to what was meant
    String8 body = lsp_request_body(scratch.arena, 12, str8_lit("text\"Document/hover"), str8_lit("{\"p\":[1]}"));
    m = lsp_message_from_body(scratch.arena, body);
    test_expect("lsp_request_body", "round trip", m.kind == LSP_MessageKind_Request && m.id == 12 &&
                str8_match(m.method, str8_lit("text\"Document/hover"), 0) &&
                json_s64_from_value(json_value_from_index(json_value_from_key(m.params, str8_lit("p")), 0)) == 1);
    body = lsp_request_body(scratch.arena, 13, str8_lit("shutdown"), str8_zero());
    test_expect("lsp_request_body", "no params", str8_match(body, str8_lit("{\"jsonrpc\":\"2.0\",\"id\":13,\"method\":\"shutdown\"}"), 0));
    body = lsp_notification_body(scratch.arena, str8_lit("exit"), str8_zero());
    test_expect("lsp_notification_body", "no params", str8_match(body, str8_lit("{\"jsonrpc\":\"2.0\",\"method\":\"exit\"}"), 0));
    body = lsp_response_body(scratch.arena, str8_lit("\"x\""), str8_zero());
    test_expect("lsp_response_body", "null result", str8_match(body, str8_lit("{\"jsonrpc\":\"2.0\",\"id\":\"x\",\"result\":null}"), 0));
  }

  scratch_end(scratch);
}

////////////////////////////////
//~ rad: Entry Point

internal void
test_section_end(char *section_name, U64 *start_us)
{
  U64 end_us = now_time_us();
  printf("  %-44s %8I64u ms\n", section_name, (end_us - *start_us)/1000);
  fflush(stdout);
  *start_us = end_us;
}

internal void
entry_point(CmdLine *cmd_line)
{
  U64 section_start_us = now_time_us();

  //- rad: json parsing and writing
  test_json();
  test_section_end("json: values, escapes, errors, random", &section_start_us);

  //- rad: LSP framing and messages
  test_lsp();
  test_section_end("lsp: framing, messages", &section_start_us);

  //- rad: hand-written cases
  {
    TestEdit edits[] =
    {
      {1, 1, "\n\n"},   // two newlines at end of line 1
      {3, 3, "xyz"},    // type on the new empty line
      {5, 6, ""},       // delete one char
      {2, 3, ""},       // join two lines (delete a newline)
    };
    test_edits("newlines_then_typing", "a\nb\nc\n", edits, ArrayCount(edits));
  }
  {
    TestEdit edits[] =
    {
      {79, 79, "\n\n"},
      {81, 81, "l"}, {82, 82, "e"}, {83, 83, "t"},
    };
    test_edits("sample_rs_case", "// A small Rust file for checking the code view.\nuse std::collections::HashMap;\n\n#[derive(Debug, Clone)]\nstruct Point {\n", edits, ArrayCount(edits));
  }
  {
    TestEdit edits[] =
    {
      {0, 0, "first\n"},
      {0, 6, ""},
      {6, 11, "X\nY\nZ"},
    };
    test_edits("insert_delete_at_start", "line one\nline two\nline three", edits, ArrayCount(edits));
  }
  test_section_end("patched text: hand-written", &section_start_us);

  //- rad: real text-layer info (same cases, lexed as Rust and as plain text)
  {
    TestEdit edits[] =
    {
      {79, 79, "\n\n"},
      {81, 81, "l"}, {82, 82, "e"}, {83, 83, "t"},
    };
    char *text = "// A small Rust file for checking the code view.\nuse std::collections::HashMap;\n\n#[derive(Debug, Clone)]\nstruct Point {\n    x: f64,\n}\n";
    test_edits_real_info("real_info_rust", text, TXT_LangKind_Rust, edits, ArrayCount(edits));
    test_edits_real_info("real_info_null", text, TXT_LangKind_Null, edits, ArrayCount(edits));
  }
  test_section_end("patched text: real lexer info", &section_start_us);

  //- rad: Rust lexing: token kinds; resuming at any line, before and after edits
  test_rust_tokens();
  test_rust_resume("rust_resume", 0, 0);
  {
    TestAnchorEdit edits[] =
    {
      {"fn main",       0, "fn g<'b>(v: &'b str) -> char { 'z' }\n"},
      {"    let c",     0, "    let x = r#\"new \"raw\"#;\n"},
      {"line one",      0, "more "},
      {"    /* outer", 39, ""},
      {"#[cfg",         0, "/* added\n   comment */\n"},
      {"    'outer",    0, "    let s2 = \"two\nlines\";\n"},
    };
    test_rust_resume("rust_resume_edits", edits, ArrayCount(edits));
  }
  for(U64 seed = 1; seed <= 20; seed += 1)
  {
    Temp scratch = scratch_begin(0, 0);
    TestAnchorEdit *edits = test_rust_random_edits(scratch.arena, seed*0x9E3779B97F4A7C15ull, 25);
    test_rust_resume("rust_resume_random", edits, 50);
    scratch_end(scratch);
  }
  test_section_end("rust lexing: kinds, resume, edits", &section_start_us);

  //- rad: random edits
  // NOTE(rad): cost grows ~cubically with the patch count (every step
  // re-applies every patch, and lookups walk the patched maps), so keep this
  // short; long edit sessions are covered by the buffer tests (compaction).
  for(U64 seed = 1; seed <= 10; seed += 1)
  {
    test_random_edits("random_lf", seed*0x9E3779B97F4A7C15ull, 150, 0);
    test_random_edits("random_crlf", seed*0xD1B54A32D192ED03ull, 150, 1);
  }
  test_section_end("patched text: random edits", &section_start_us);

  //- rad: text buffer (undo/redo, groups, compaction, save, disk changes)
  test_buffer_undo_groups();
  test_buffer_disk();
  test_buffer_empty();
  test_section_end("buffer: undo groups, disk, empty", &section_start_us);
  for(U64 seed = 1; seed <= 10; seed += 1)
  {
    test_buffer_random("buffer_random_lf", seed*0x9E3779B97F4A7C15ull, 400, 0);
    test_buffer_random("buffer_random_crlf", seed*0xD1B54A32D192ED03ull, 400, 1);
  }
  test_section_end("buffer: random edits/undo/redo/save", &section_start_us);
  test_buffer_large();
  test_section_end("buffer: large text (~2 MB)", &section_start_us);

  printf("%I64u checks, %I64u failing test(s)\n", test_checks, test_failures);
  fflush(stdout);
  abort_self(test_failures == 0 ? 0 : 1);
}
