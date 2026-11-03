// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

#ifndef JSON_H
#define JSON_H

////////////////////////////////
//~ rad: Notes
//
// JSON (RFC 8259) parsing into a tree, and writing helpers. Depends only on
// `base`. Used for cargo's `--message-format=json` output and (later) the
// language server protocol.
//
// Parsing builds the whole tree in one arena. Objects and arrays are values
// with children; an object's children carry their member names in `key`.
// Strings and keys are unescaped to UTF-8 (\uXXXX, surrogate pairs; a lone
// surrogate becomes U+FFFD). Numbers keep their source text in `string` and
// the parsed value in `number`. Lookups that find nothing return the nil
// value (`json_value_is_nil`), which is safe to look into further, so
// `json_value_from_key(json_value_from_key(root, "a"), "b")` never needs a
// check in between.
//
// Malformed input (or nesting deeper than JSON_MAX_DEPTH) gives `ok = 0`,
// the nil root, and the byte offset of the error. The parser never reads
// past the input.

////////////////////////////////
//~ rad: Tunables

#define JSON_MAX_DEPTH 512

////////////////////////////////
//~ rad: Value Types

typedef enum JSON_ValueKind
{
  JSON_ValueKind_Null,
  JSON_ValueKind_Bool,
  JSON_ValueKind_Number,
  JSON_ValueKind_String,
  JSON_ValueKind_Array,
  JSON_ValueKind_Object,
}
JSON_ValueKind;

typedef struct JSON_Value JSON_Value;
struct JSON_Value
{
  JSON_Value *next;
  JSON_Value *first;
  JSON_Value *last;
  U64 child_count;
  JSON_ValueKind kind;
  String8 key;    // object members: member name
  String8 string; // strings: the unescaped text; numbers: the source text
  F64 number;
  B32 boolean;
};

typedef struct JSON_ParseResult JSON_ParseResult;
struct JSON_ParseResult
{
  JSON_Value *root;
  B32 ok;
  U64 error_off;
};

////////////////////////////////
//~ rad: Globals

global read_only JSON_Value json_value_nil = {&json_value_nil, &json_value_nil, &json_value_nil};

////////////////////////////////
//~ rad: Parsing

internal JSON_ParseResult json_parse(Arena *arena, String8 text);

////////////////////////////////
//~ rad: Lookups

internal B32 json_value_is_nil(JSON_Value *value);
internal JSON_Value *json_value_from_key(JSON_Value *object, String8 key);
internal JSON_Value *json_value_from_index(JSON_Value *array, U64 idx);
internal String8 json_string_from_value(JSON_Value *value);
internal F64 json_f64_from_value(JSON_Value *value);
internal S64 json_s64_from_value(JSON_Value *value);
internal B32 json_b32_from_value(JSON_Value *value);

////////////////////////////////
//~ rad: Writing

internal String8 json_escaped_from_str8(Arena *arena, String8 string);
internal String8 json_text_from_value(Arena *arena, JSON_Value *value);

#endif // JSON_H
