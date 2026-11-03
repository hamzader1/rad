// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

////////////////////////////////
//~ rad: Parser Types

typedef struct JSON_Parser JSON_Parser;
struct JSON_Parser
{
  Arena *arena;
  U8 *str;
  U64 size;
  U64 off;
  B32 failed;
  U64 error_off;
};

////////////////////////////////
//~ rad: Parser Helpers

internal void
json_parse_fail__(JSON_Parser *p)
{
  if(!p->failed)
  {
    p->failed = 1;
    p->error_off = p->off;
  }
}

internal void
json_parse_skip_ws__(JSON_Parser *p)
{
  for(;p->off < p->size; p->off += 1)
  {
    U8 c = p->str[p->off];
    if(c != ' ' && c != '\t' && c != '\n' && c != '\r')
    {
      break;
    }
  }
}

internal B32
json_parse_literal__(JSON_Parser *p, String8 literal)
{
  B32 result = (p->off + literal.size <= p->size && MemoryMatch(p->str + p->off, literal.str, literal.size));
  if(result)
  {
    p->off += literal.size;
  }
  else
  {
    json_parse_fail__(p);
  }
  return result;
}

internal U32
json_parse_hex4__(JSON_Parser *p, B32 *ok_out)
{
  // NOTE(rad): p->off is at the first of 4 hex digits
  U32 result = 0;
  B32 ok = (p->off + 4 <= p->size);
  for(U64 idx = 0; ok && idx < 4; idx += 1)
  {
    U8 c = p->str[p->off + idx];
    U32 digit = 0;
    if('0' <= c && c <= '9')      { digit = c - '0'; }
    else if('a' <= c && c <= 'f') { digit = c - 'a' + 10; }
    else if('A' <= c && c <= 'F') { digit = c - 'A' + 10; }
    else                          { ok = 0; }
    result = (result << 4) | digit;
  }
  if(ok)
  {
    p->off += 4;
  }
  *ok_out = ok;
  return result;
}

internal String8
json_parse_string__(JSON_Parser *p)
{
  // NOTE(rad): p->off is at the opening quote. Unescaped text is never
  // longer than its escaped source, so the source length bounds the buffer.
  String8 result = {0};
  U64 start = p->off + 1;
  U64 end = start;
  for(;end < p->size && p->str[end] != '"'; end += 1)
  {
    if(p->str[end] == '\\')
    {
      end += 1;
    }
  }
  if(end >= p->size)
  {
    p->off = p->size;
    json_parse_fail__(p);
    return result;
  }
  U8 *buffer = push_array_no_zero(p->arena, U8, end - start + 1);
  U64 size = 0;
  p->off = start;
  for(;!p->failed && p->off < end;)
  {
    U8 c = p->str[p->off];
    if(c < 0x20)
    {
      json_parse_fail__(p);
    }
    else if(c != '\\')
    {
      buffer[size++] = c;
      p->off += 1;
    }
    else
    {
      p->off += 1;
      U8 esc = (p->off < end ? p->str[p->off] : 0);
      p->off += 1;
      switch(esc)
      {
        case '"':  {buffer[size++] = '"';}break;
        case '\\': {buffer[size++] = '\\';}break;
        case '/':  {buffer[size++] = '/';}break;
        case 'b':  {buffer[size++] = '\b';}break;
        case 'f':  {buffer[size++] = '\f';}break;
        case 'n':  {buffer[size++] = '\n';}break;
        case 'r':  {buffer[size++] = '\r';}break;
        case 't':  {buffer[size++] = '\t';}break;
        case 'u':
        {
          B32 ok = 0;
          U32 codepoint = json_parse_hex4__(p, &ok);
          if(!ok)
          {
            p->off -= 1;
            json_parse_fail__(p);
            break;
          }
          if(0xD800 <= codepoint && codepoint <= 0xDBFF)
          {
            // rad: high surrogate -> needs a following \uDC00..\uDFFF
            U32 low = 0;
            B32 low_ok = 0;
            if(p->off + 2 <= end && p->str[p->off] == '\\' && p->str[p->off+1] == 'u')
            {
              U64 save_off = p->off;
              p->off += 2;
              low = json_parse_hex4__(p, &low_ok);
              low_ok = (low_ok && 0xDC00 <= low && low <= 0xDFFF);
              if(!low_ok)
              {
                p->off = save_off;
              }
            }
            codepoint = (low_ok ? 0x10000 + ((codepoint - 0xD800) << 10) + (low - 0xDC00) : 0xFFFD);
          }
          else if(0xDC00 <= codepoint && codepoint <= 0xDFFF)
          {
            codepoint = 0xFFFD;
          }
          size += utf8_encode(buffer + size, codepoint);
        }break;
        default:
        {
          p->off -= 1;
          json_parse_fail__(p);
        }break;
      }
    }
  }
  if(!p->failed)
  {
    p->off = end + 1;
    result = str8(buffer, size);
  }
  return result;
}

internal void
json_parse_number__(JSON_Parser *p, JSON_Value *value)
{
  // NOTE(rad): -?(0|[1-9][0-9]*)(\.[0-9]+)?([eE][+-]?[0-9]+)?
  U64 start = p->off;
  B32 is_integer = 1;
  if(p->off < p->size && p->str[p->off] == '-')
  {
    p->off += 1;
  }
  U64 int_start = p->off;
  for(;p->off < p->size && char_is_digit(p->str[p->off], 10); p->off += 1);
  U64 int_digits = p->off - int_start;
  B32 ok = (int_digits != 0 && !(int_digits > 1 && p->str[int_start] == '0'));
  if(ok && p->off < p->size && p->str[p->off] == '.')
  {
    is_integer = 0;
    p->off += 1;
    U64 frac_start = p->off;
    for(;p->off < p->size && char_is_digit(p->str[p->off], 10); p->off += 1);
    ok = (p->off != frac_start);
  }
  if(ok && p->off < p->size && (p->str[p->off] == 'e' || p->str[p->off] == 'E'))
  {
    is_integer = 0;
    p->off += 1;
    if(p->off < p->size && (p->str[p->off] == '+' || p->str[p->off] == '-'))
    {
      p->off += 1;
    }
    U64 exp_start = p->off;
    for(;p->off < p->size && char_is_digit(p->str[p->off], 10); p->off += 1);
    ok = (p->off != exp_start);
  }
  if(!ok)
  {
    p->off = start;
    json_parse_fail__(p);
    return;
  }
  value->kind = JSON_ValueKind_Number;
  value->string = push_str8_copy(p->arena, str8(p->str + start, p->off - start));
  if(is_integer)
  {
    // rad: exact for integers (line numbers, ids, offsets)
    F64 magnitude = 0;
    for(U64 idx = int_start; idx < int_start + int_digits; idx += 1)
    {
      magnitude = magnitude*10 + (p->str[idx] - '0');
    }
    value->number = (p->str[start] == '-' ? -magnitude : magnitude);
  }
  else
  {
    // rad: base's f64_from_str8 only knows a lowercase exponent marker
    String8 text = push_str8_copy(p->arena, value->string);
    for EachIndex(idx, text.size)
    {
      if(text.str[idx] == 'E') { text.str[idx] = 'e'; }
    }
    value->number = f64_from_str8(text);
  }
}

internal JSON_Value *
json_parse_value__(JSON_Parser *p, U64 depth)
{
  JSON_Value *value = &json_value_nil;
  json_parse_skip_ws__(p);
  if(p->failed || p->off >= p->size || depth > JSON_MAX_DEPTH)
  {
    json_parse_fail__(p);
    return value;
  }
  value = push_array(p->arena, JSON_Value, 1);
  value->next = value->first = value->last = &json_value_nil;
  U8 c = p->str[p->off];
  switch(c)
  {
    //- rad: object
    case '{':
    {
      value->kind = JSON_ValueKind_Object;
      p->off += 1;
      json_parse_skip_ws__(p);
      if(p->off < p->size && p->str[p->off] == '}')
      {
        p->off += 1;
        break;
      }
      for(;!p->failed;)
      {
        json_parse_skip_ws__(p);
        if(p->off >= p->size || p->str[p->off] != '"')
        {
          json_parse_fail__(p);
          break;
        }
        String8 key = json_parse_string__(p);
        json_parse_skip_ws__(p);
        if(p->failed || p->off >= p->size || p->str[p->off] != ':')
        {
          json_parse_fail__(p);
          break;
        }
        p->off += 1;
        JSON_Value *child = json_parse_value__(p, depth+1);
        if(p->failed)
        {
          break;
        }
        child->key = key;
        if(value->first == &json_value_nil) { value->first = child; } else { value->last->next = child; }
        value->last = child;
        value->child_count += 1;
        json_parse_skip_ws__(p);
        if(p->off < p->size && p->str[p->off] == ',')
        {
          p->off += 1;
        }
        else if(p->off < p->size && p->str[p->off] == '}')
        {
          p->off += 1;
          break;
        }
        else
        {
          json_parse_fail__(p);
        }
      }
    }break;

    //- rad: array
    case '[':
    {
      value->kind = JSON_ValueKind_Array;
      p->off += 1;
      json_parse_skip_ws__(p);
      if(p->off < p->size && p->str[p->off] == ']')
      {
        p->off += 1;
        break;
      }
      for(;!p->failed;)
      {
        JSON_Value *child = json_parse_value__(p, depth+1);
        if(p->failed)
        {
          break;
        }
        if(value->first == &json_value_nil) { value->first = child; } else { value->last->next = child; }
        value->last = child;
        value->child_count += 1;
        json_parse_skip_ws__(p);
        if(p->off < p->size && p->str[p->off] == ',')
        {
          p->off += 1;
        }
        else if(p->off < p->size && p->str[p->off] == ']')
        {
          p->off += 1;
          break;
        }
        else
        {
          json_parse_fail__(p);
        }
      }
    }break;

    //- rad: string
    case '"':
    {
      value->kind = JSON_ValueKind_String;
      value->string = json_parse_string__(p);
    }break;

    //- rad: literals
    case 't':
    {
      value->kind = JSON_ValueKind_Bool;
      value->boolean = json_parse_literal__(p, str8_lit("true"));
    }break;
    case 'f':
    {
      value->kind = JSON_ValueKind_Bool;
      json_parse_literal__(p, str8_lit("false"));
    }break;
    case 'n':
    {
      value->kind = JSON_ValueKind_Null;
      json_parse_literal__(p, str8_lit("null"));
    }break;

    //- rad: number
    default:
    {
      json_parse_number__(p, value);
    }break;
  }
  return value;
}

////////////////////////////////
//~ rad: Parsing

internal JSON_ParseResult
json_parse(Arena *arena, String8 text)
{
  JSON_Parser parser = {arena, text.str, text.size};
  JSON_Value *root = json_parse_value__(&parser, 0);
  json_parse_skip_ws__(&parser);
  if(!parser.failed && parser.off != parser.size)
  {
    json_parse_fail__(&parser);
  }
  JSON_ParseResult result = {0};
  result.ok = !parser.failed;
  result.root = (result.ok ? root : &json_value_nil);
  result.error_off = (result.ok ? 0 : parser.error_off);
  return result;
}

////////////////////////////////
//~ rad: Lookups

internal B32
json_value_is_nil(JSON_Value *value)
{
  return (value == 0 || value == &json_value_nil);
}

internal JSON_Value *
json_value_from_key(JSON_Value *object, String8 key)
{
  JSON_Value *result = &json_value_nil;
  if(!json_value_is_nil(object) && object->kind == JSON_ValueKind_Object)
  {
    for(JSON_Value *child = object->first; child != &json_value_nil; child = child->next)
    {
      if(str8_match(child->key, key, 0))
      {
        result = child;
        break;
      }
    }
  }
  return result;
}

internal JSON_Value *
json_value_from_index(JSON_Value *array, U64 idx)
{
  JSON_Value *result = &json_value_nil;
  if(!json_value_is_nil(array) && array->kind == JSON_ValueKind_Array && idx < array->child_count)
  {
    result = array->first;
    for(U64 i = 0; i < idx; i += 1)
    {
      result = result->next;
    }
  }
  return result;
}

internal String8
json_string_from_value(JSON_Value *value)
{
  String8 result = {0};
  if(!json_value_is_nil(value) && value->kind == JSON_ValueKind_String)
  {
    result = value->string;
  }
  return result;
}

internal F64
json_f64_from_value(JSON_Value *value)
{
  F64 result = 0;
  if(!json_value_is_nil(value) && value->kind == JSON_ValueKind_Number)
  {
    result = value->number;
  }
  return result;
}

internal S64
json_s64_from_value(JSON_Value *value)
{
  S64 result = 0;
  if(!json_value_is_nil(value) && value->kind == JSON_ValueKind_Number)
  {
    F64 n = value->number;
    result = (n >= 9.2233720368547758e18 ? max_S64 : n <= -9.2233720368547758e18 ? min_S64 : (S64)n);
  }
  return result;
}

internal B32
json_b32_from_value(JSON_Value *value)
{
  B32 result = 0;
  if(!json_value_is_nil(value) && value->kind == JSON_ValueKind_Bool)
  {
    result = value->boolean;
  }
  return result;
}

////////////////////////////////
//~ rad: Writing

internal String8
json_escaped_from_str8(Arena *arena, String8 string)
{
  // NOTE(rad): a JSON string literal, quotes included; UTF-8 passes through
  U64 size = 2;
  for EachIndex(idx, string.size)
  {
    U8 c = string.str[idx];
    size += (c == '"' || c == '\\' || c == '\n' || c == '\r' || c == '\t' || c == '\b' || c == '\f') ? 2 : (c < 0x20) ? 6 : 1;
  }
  U8 *buffer = push_array_no_zero(arena, U8, size + 1);
  U64 off = 0;
  buffer[off++] = '"';
  for EachIndex(idx, string.size)
  {
    U8 c = string.str[idx];
    switch(c)
    {
      case '"':  {buffer[off++] = '\\'; buffer[off++] = '"';}break;
      case '\\': {buffer[off++] = '\\'; buffer[off++] = '\\';}break;
      case '\n': {buffer[off++] = '\\'; buffer[off++] = 'n';}break;
      case '\r': {buffer[off++] = '\\'; buffer[off++] = 'r';}break;
      case '\t': {buffer[off++] = '\\'; buffer[off++] = 't';}break;
      case '\b': {buffer[off++] = '\\'; buffer[off++] = 'b';}break;
      case '\f': {buffer[off++] = '\\'; buffer[off++] = 'f';}break;
      default:
      {
        if(c < 0x20)
        {
          local_persist U8 hex[] = "0123456789abcdef";
          buffer[off++] = '\\';
          buffer[off++] = 'u';
          buffer[off++] = '0';
          buffer[off++] = '0';
          buffer[off++] = hex[c >> 4];
          buffer[off++] = hex[c & 0xf];
        }
        else
        {
          buffer[off++] = c;
        }
      }break;
    }
  }
  buffer[off++] = '"';
  buffer[off] = 0;
  return str8(buffer, off);
}

internal void
json_text_from_value__(Arena *arena, String8List *out, JSON_Value *value, B32 with_key)
{
  if(with_key)
  {
    str8_list_push(arena, out, json_escaped_from_str8(arena, value->key));
    str8_list_push(arena, out, str8_lit(":"));
  }
  switch(value->kind)
  {
    case JSON_ValueKind_Null:   {str8_list_push(arena, out, str8_lit("null"));}break;
    case JSON_ValueKind_Bool:   {str8_list_push(arena, out, value->boolean ? str8_lit("true") : str8_lit("false"));}break;
    case JSON_ValueKind_String: {str8_list_push(arena, out, json_escaped_from_str8(arena, value->string));}break;
    case JSON_ValueKind_Number:
    {
      if(value->string.size != 0)
      {
        str8_list_push(arena, out, value->string);
      }
      else
      {
        str8_list_pushf(arena, out, "%.17g", value->number);
      }
    }break;
    case JSON_ValueKind_Array:
    case JSON_ValueKind_Object:
    {
      B32 is_object = (value->kind == JSON_ValueKind_Object);
      str8_list_push(arena, out, is_object ? str8_lit("{") : str8_lit("["));
      for(JSON_Value *child = value->first; child != &json_value_nil; child = child->next)
      {
        if(child != value->first)
        {
          str8_list_push(arena, out, str8_lit(","));
        }
        json_text_from_value__(arena, out, child, is_object);
      }
      str8_list_push(arena, out, is_object ? str8_lit("}") : str8_lit("]"));
    }break;
  }
}

internal String8
json_text_from_value(Arena *arena, JSON_Value *value)
{
  // NOTE(rad): compact text (no whitespace); numbers keep their source text
  String8 result = {0};
  if(!json_value_is_nil(value))
  {
    Temp scratch = scratch_begin(&arena, 1);
    String8List parts = {0};
    json_text_from_value__(scratch.arena, &parts, value, 0);
    result = str8_list_join(arena, &parts, 0);
    scratch_end(scratch);
  }
  return result;
}
