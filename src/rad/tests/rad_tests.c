// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

////////////////////////////////
//~ rad: Notes
//
// Console test program for the editor's text buffer, which is the text
// layer's patched representation (base data + TXT_PatchList ->
// TXT_Patched). Each test applies edits and compares the patched memory map
// and line map against a plain string that is edited the same way.
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

#include "base/base_inc.c"
#include "win32/win32_inc.c"
#include "linux/linux_inc.c"
#include "artifact_cache/artifact_cache.c"
#include "content/content.c"
#include "text/text.c"

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

internal void
test_random_edits(char *test_name, U64 seed, U64 edits_count, B32 crlf)
{
  Temp scratch = scratch_begin(0, 0);
  U64 rng = seed;

  //- rad: random base text with short lines
  String8List base_parts = {0};
  for EachIndex(line_idx, 200)
  {
    U64 len = test_rand(&rng)%40;
    U8 *line = push_array(scratch.arena, U8, len);
    for EachIndex(c, len) { line[c] = (U8)('a' + test_rand(&rng)%26); }
    str8_list_push(scratch.arena, &base_parts, str8(line, len));
    str8_list_push(scratch.arena, &base_parts, crlf ? str8_lit("\r\n") : str8_lit("\n"));
  }
  String8 base = str8_list_join(scratch.arena, &base_parts, 0);
  TXT_TextInfo info = test_text_info_from_data(scratch.arena, base);
  TXT_PatchList patches = {0};
  String8 expected = base;
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
  for EachIndex(edit_idx, edits_count)
  {
    //- rad: pick a range on character boundaries (never split "\r\n")
    U64 a = expected.size ? test_rand(&rng)%(expected.size+1) : 0;
    U64 len = (test_rand(&rng)%4 == 0) ? test_rand(&rng)%60 : (test_rand(&rng)%3);
    U64 b = Min(expected.size, a + len);
    if(a > 0 && a < expected.size && expected.str[a] == '\n' && expected.str[a-1] == '\r') { a -= 1; }
    if(b > 0 && b < expected.size && expected.str[b] == '\n' && expected.str[b-1] == '\r') { b += 1; }
    Rng1U64 range = r1u64(a, b);
    String8 replace = crlf ? crlf_snippets[test_rand(&rng)%ArrayCount(crlf_snippets)] : snippets[test_rand(&rng)%ArrayCount(snippets)];
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
//~ rad: Entry Point

internal void
entry_point(CmdLine *cmd_line)
{
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

  //- rad: random edits
  for(U64 seed = 1; seed <= 20; seed += 1)
  {
    test_random_edits("random_lf", seed*0x9E3779B97F4A7C15ull, 300, 0);
    test_random_edits("random_crlf", seed*0xD1B54A32D192ED03ull, 300, 1);
  }

  printf("%I64u checks, %I64u failing test(s)\n", test_checks, test_failures);
  fflush(stdout);
  abort_self(test_failures == 0 ? 0 : 1);
}
