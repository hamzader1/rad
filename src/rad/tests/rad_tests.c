// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

////////////////////////////////
//~ rad: Notes
//
// Console test program for the editor's text buffer:
// - the text layer's patched representation (base data + TXT_PatchList ->
//   TXT_Patched);
// - the text_buffer layer (TB_Buffer) on top of it: undo/redo, undo groups,
//   compaction, save, reload / "changed on disk", empty and large texts.
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

#include "base/base_inc.c"
#include "win32/win32_inc.c"
#include "linux/linux_inc.c"
#include "artifact_cache/artifact_cache.c"
#include "content/content.c"
#include "text/text.c"
#include "text_buffer/text_buffer.c"

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
