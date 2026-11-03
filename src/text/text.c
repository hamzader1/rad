// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

#undef LAYER_COLOR
#define LAYER_COLOR 0xe34cd4ff

////////////////////////////////
//~ rjf: Generated Code

#include "generated/text.meta.c"

////////////////////////////////
//~ rjf: Basic Helpers

internal TXT_LangKind
txt_lang_kind_from_extension(String8 extension)
{
  TXT_LangKind kind = TXT_LangKind_Null;
  for EachElement(idx, txt_ext_lang_kind_table)
  {
    if(str8_match(extension, txt_ext_lang_kind_table[idx].ext, 0))
    {
      kind = txt_ext_lang_kind_table[idx].kind;
      break;
    }
  }
  return kind;
}

internal String8
txt_extension_from_lang_kind(TXT_LangKind kind)
{
  String8 result = txt_lang_kind_ext_table[kind];
  return result;
}

internal TXT_LangKind
txt_lang_kind_from_arch(Arch arch)
{
  TXT_LangKind kind = TXT_LangKind_Null;
  switch(arch)
  {
    default:{}break;
    case Arch_x64:{kind = TXT_LangKind_DisasmX64Intel;}break;
  }
  return kind;
}

internal TXT_LangLexFunctionType *
txt_lex_function_from_lang_kind(TXT_LangKind kind)
{
  TXT_LangLexFunctionType *fn = 0;
  switch(kind)
  {
    default:{}break;
    case TXT_LangKind_C:             {fn = txt_token_array_from_string__c_cpp;}break;
    case TXT_LangKind_CPlusPlus:     {fn = txt_token_array_from_string__c_cpp;}break;
    case TXT_LangKind_Odin:          {fn = txt_token_array_from_string__odin;}break;
    case TXT_LangKind_Jai:           {fn = txt_token_array_from_string__jai;}break;
    case TXT_LangKind_Zig:           {fn = txt_token_array_from_string__zig;}break;
    case TXT_LangKind_Rust:          {fn = txt_token_array_from_string__rust;}break;
    case TXT_LangKind_DisasmX64Intel:{fn = txt_token_array_from_string__disasm_x64_intel;}break;
  }
  return fn;
}

////////////////////////////////
//~ rjf: Token Type Functions

internal void
txt_token_chunk_list_push(Arena *arena, TXT_TokenChunkList *list, U64 cap, TXT_Token *token)
{
  TXT_TokenChunkNode *node = list->last;
  if(node == 0 || node->count >= node->cap)
  {
    node = push_array(arena, TXT_TokenChunkNode, 1);
    SLLQueuePush(list->first, list->last, node);
    node->cap = cap;
    node->v = push_array_no_zero(arena, TXT_Token, node->cap);
    list->chunk_count += 1;
  }
  MemoryCopyStruct(&node->v[node->count], token);
  node->count += 1;
  list->token_count += 1;
}

internal void
txt_token_list_push(Arena *arena, TXT_TokenList *list, TXT_Token *token)
{
  TXT_TokenNode *node = push_array(arena, TXT_TokenNode, 1);
  MemoryCopyStruct(&node->v, token);
  SLLQueuePush(list->first, list->last, node);
  list->count += 1;
}

internal TXT_TokenArray
txt_token_array_from_chunk_list(Arena *arena, TXT_TokenChunkList *list)
{
  TXT_TokenArray array = {0};
  array.count = list->token_count;
  array.v = push_array_no_zero(arena, TXT_Token, array.count);
  U64 idx = 0;
  for(TXT_TokenChunkNode *n = list->first; n != 0; n = n->next)
  {
    MemoryCopy(array.v+idx, n->v, n->count*sizeof(TXT_Token));
    idx += n->count;
  }
  return array;
}

internal TXT_TokenArray
txt_token_array_from_list(Arena *arena, TXT_TokenList *list)
{
  TXT_TokenArray array = {0};
  array.count = list->count;
  array.v = push_array_no_zero(arena, TXT_Token, array.count);
  U64 idx = 0;
  for(TXT_TokenNode *n = list->first; n != 0; n = n->next)
  {
    MemoryCopyStruct(array.v+idx, &n->v);
    idx += 1;
  }
  return array;
}

////////////////////////////////
//~ rjf: Patch Functions

internal void
txt_patch_list_push_new(Arena *arena, TXT_PatchList *list, Rng1U64 range, String8 replace)
{
  TXT_PatchNode *n = push_array(arena, TXT_PatchNode, 1);
  n->v.range = range;
  n->v.replace = str8_copy(arena, replace);
  DLLPushBack(list->first, list->last, n);
  list->count += 1;
}

////////////////////////////////
//~ rjf: Lexing Functions

internal TXT_TokenArray
txt_token_array_from_lang_kind_string(Arena *arena, TXT_LangKind lang_kind, String8 string)
{
  ProfBeginFunction();
  Temp scratch = scratch_begin(&arena, 1);
  U64 chunk_size = Clamp(8, string.size/8, 4096);
  
  //- rjf: gather keywords
  U64 keyword_slots_count = txt_keywords_from_lang_kind_table[lang_kind].count;
  String8Node **keyword_slots = push_array(scratch.arena, String8Node *, keyword_slots_count);
  for EachIndex(idx, txt_keywords_from_lang_kind_table[lang_kind].count)
  {
    String8 keyword = txt_keywords_from_lang_kind_table[lang_kind].v[idx];
    U64 hash = u64_hash_from_str8(keyword);
    U64 slot_idx = hash%keyword_slots_count;
    String8Node *n = push_array(scratch.arena, String8Node, 1);
    SLLStackPush(keyword_slots[slot_idx], n);
    n->string = keyword;
  }
  
  //- rjf: gather multichar symbols
  String8Array multichar_symbols = txt_multichar_symbols_from_lang_kind_table[lang_kind];
  
  //- rjf: gather rules
  U64 tokenizer_rule_slots_count = txt_tokenizer_rules_from_lang_kind_table[lang_kind].count;
  TXT_TokenizerRulePtrNode **tokenizer_rule_opener_slots = push_array(scratch.arena, TXT_TokenizerRulePtrNode *, tokenizer_rule_slots_count);
  TXT_TokenizerRulePtrNode **tokenizer_rule_closer_slots = push_array(scratch.arena, TXT_TokenizerRulePtrNode *, tokenizer_rule_slots_count);
  {
    TXT_TokenizerRuleArray rules = txt_tokenizer_rules_from_lang_kind_table[lang_kind];
    for EachIndex(idx, rules.count)
    {
      TXT_TokenizerRule *r = &rules.v[idx];
      U64 open_hash = u64_hash_from_str8(r->open_string);
      U64 close_hash = u64_hash_from_str8(r->close_string);
      U64 open_slot_idx = open_hash%tokenizer_rule_slots_count;
      U64 close_slot_idx = close_hash%tokenizer_rule_slots_count;
      TXT_TokenizerRulePtrNode *open_n = push_array(scratch.arena, TXT_TokenizerRulePtrNode, 1);
      TXT_TokenizerRulePtrNode *close_n = push_array(scratch.arena, TXT_TokenizerRulePtrNode, 1);
      open_n->v = r;
      close_n->v = r;
      SLLStackPush(tokenizer_rule_opener_slots[open_slot_idx], open_n);
      SLLStackPush(tokenizer_rule_closer_slots[close_slot_idx], close_n);
    }
  }
  
  //- rjf: generate token list
  TXT_TokenChunkList tokens = {0};
  {
    typedef struct TokenTask TokenTask;
    struct TokenTask
    {
      TokenTask *next;
      TXT_TokenizerRule *rule;
      TXT_TokenKind kind;
      U64 start_idx;
    };
    TXT_TokenizerRule *nil_rule = &txt_tokenizer_rules__null[0];
    TokenTask *top_task = 0;
    TokenTask *free_task = 0;
    B32 escaped = 0;
    B32 next_escaped = 0;
    U64 byte_process_start_idx = 0;
    for(U64 idx = 0; idx <= string.size;)
    {
      U8 byte      = (idx+0 < string.size) ? (string.str[idx+0]) : 0;
      U8 next_byte = (idx+1 < string.size) ? (string.str[idx+1]) : 0;
      
      // rjf: unpack next bytes
      U8 next_bytes[] = {byte, next_byte};
      String8 string_1byte = str8(next_bytes, 1);
      String8 string_2byte = str8(next_bytes, 2);
      
      // rjf: update counter
      if((idx-byte_process_start_idx) >= 1000 || idx == string.size)
      {
        add_progress(idx - byte_process_start_idx);
        byte_process_start_idx = idx;
      }
      
      // rjf: escaping
      if(escaped && (byte != '\r' && byte != '\n'))
      {
        next_escaped = 0;
      }
      else if(!escaped && byte == '\\')
      {
        next_escaped = 1;
      }
      
      // rjf: take token starters
      B32 new_token_needed = (top_task == 0);
      TXT_TokenizerRule *new_rule = nil_rule;
      TXT_TokenKind new_token_kind = TXT_TokenKind_Null;
      {
        // rjf: use next bytes to look up a rule from the table
        {
          TXT_TokenizerRule *active_rule = top_task ? top_task->rule : nil_rule;
          U64 hash_1byte = u64_hash_from_str8(string_1byte);
          U64 hash_2byte = u64_hash_from_str8(string_2byte);
          U64 slot_1byte = hash_1byte%tokenizer_rule_slots_count;
          U64 slot_2byte = hash_2byte%tokenizer_rule_slots_count;
          if(new_rule == nil_rule)
          {
            for EachNode(n, TXT_TokenizerRulePtrNode, tokenizer_rule_opener_slots[slot_2byte])
            {
              TXT_TokenizerRule *n_parent_rule = nil_rule;
              if(0 < n->v->parent_num && n->v->parent_num <= txt_tokenizer_rules_from_lang_kind_table[lang_kind].count)
              {
                n_parent_rule = &txt_tokenizer_rules_from_lang_kind_table[lang_kind].v[n->v->parent_num-1];
              }
              if(str8_match(n->v->open_string, string_2byte, 0) &&
                 active_rule == n_parent_rule)
              {
                new_rule = n->v;
                break;
              }
            }
          }
          if(new_rule == nil_rule)
          {
            for EachNode(n, TXT_TokenizerRulePtrNode, tokenizer_rule_opener_slots[slot_1byte])
            {
              TXT_TokenizerRule *n_parent_rule = nil_rule;
              if(0 < n->v->parent_num && n->v->parent_num <= txt_tokenizer_rules_from_lang_kind_table[lang_kind].count)
              {
                n_parent_rule = &txt_tokenizer_rules_from_lang_kind_table[lang_kind].v[n->v->parent_num-1];
              }
              if(str8_match(n->v->open_string, string_1byte, 0) &&
                 active_rule == n_parent_rule)
              {
                new_rule = n->v;
                break;
              }
            }
          }
          if(new_rule != nil_rule)
          {
            new_token_kind = new_rule->token_kind;
          }
        }
        
        // rjf: use next bytes to start a new token based on fallback rules
        if(top_task == 0 && new_rule == nil_rule)
        {
          if(0){}
          else if(char_is_space(byte))             { new_token_kind = TXT_TokenKind_Whitespace; }
          else if(byte == '_' ||
                  byte == '$' ||
                  char_is_alpha(byte))             { new_token_kind = TXT_TokenKind_Identifier; }
          else if(char_is_digit(byte, 10) ||
                  (byte == '.' &&
                   char_is_digit(next_byte, 10)))  { new_token_kind = TXT_TokenKind_Numeric; }
          else if(byte == '~' || byte == '!' ||
                  byte == '%' || byte == '^' ||
                  byte == '&' || byte == '*' ||
                  byte == '(' || byte == ')' ||
                  byte == '-' || byte == '=' ||
                  byte == '+' || byte == '[' ||
                  byte == ']' || byte == '{' ||
                  byte == '}' || byte == ':' ||
                  byte == ';' || byte == ',' ||
                  byte == '.' || byte == '<' ||
                  byte == '>' || byte == '/' ||
                  byte == '?' || byte == '|')      { new_token_kind = TXT_TokenKind_Symbol; }
        }
      }
      
      // rjf: look for ender based on rule's closing symbol
      U64 ender_pad = 0;
      B32 ender_found = 0;
      if(top_task != 0 && idx > top_task->start_idx)
      {
        TXT_TokenKind active_token_kind = top_task->kind;
        TXT_TokenizerRule *active_rule = top_task->rule;
        U64 active_token_start_idx = top_task->start_idx;
        if(idx == string.size)
        {
          ender_pad = 0;
          ender_found = 1;
        }
        else if(active_rule != nil_rule &&
                (str8_match(string_1byte, active_rule->close_string, 0) ||
                 str8_match(string_2byte, active_rule->close_string, 0)))
        {
          ender_found = 1;
          ender_pad = active_rule->close_advance;
        }
        else switch(active_token_kind)
        {
          default:{}break;
          case TXT_TokenKind_Whitespace:
          {
            ender_found = !char_is_space(byte);
          }break;
          case TXT_TokenKind_Identifier:
          {
            ender_found = (!char_is_alpha(byte) && !char_is_digit(byte, 10) && byte != '_' && byte != '$' && byte < 128);
          }break;
          case TXT_TokenKind_Numeric:
          {
            ender_found = (!char_is_alpha(byte) && !char_is_digit(byte, 10) && byte != '_' && byte != '.' && byte != '\'');
          }break;
          case TXT_TokenKind_Symbol:
          {
            ender_found = (byte != '~' && byte != '!' &&
                           byte != '%' && byte != '^' &&
                           byte != '&' && byte != '*' &&
                           byte != '(' && byte != ')' &&
                           byte != '-' && byte != '=' &&
                           byte != '+' && byte != '[' &&
                           byte != ']' && byte != '{' &&
                           byte != '}' && byte != ':' &&
                           byte != ';' && byte != ',' &&
                           byte != '.' && byte != '<' &&
                           byte != '>' && byte != '/' &&
                           byte != '?' && byte != '|');
          }break;
        }
      }
      
      // rjf: if we have a new token to start, but we have an active token, then
      // end the current token (but keep the same stack)
      B32 keep_top_task = 0;
      if(top_task != 0 && new_token_kind != TXT_TokenKind_Null && idx > top_task->start_idx)
      {
        keep_top_task = (!ender_found);
        ender_found = 1;
      }
      
      // rjf: if we have an ender => emit token(s) for current task
      if(ender_found)
      {
        TXT_Token token = {top_task->kind, r1u64(top_task->start_idx, idx+ender_pad)};
        if(!keep_top_task)
        {
          TokenTask *popped = top_task;
          SLLStackPop(top_task);
          SLLStackPush(free_task, popped);
          if(top_task != 0)
          {
            top_task->start_idx = idx+ender_pad;
          }
        }
        else
        {
          top_task->start_idx = idx+ender_pad;
        }
        
        // rjf: trim \r's off of end
        {
          String8 token_string = str8_substr(string, token.range);
          if(token_string.size != 0 && token_string.str[token_string.size-1] == '\r')
          {
            token.range.max -= 1;
            ender_pad += 1;
          }
        }
        
        // rjf: identifier -> keyword in special cases
        if(token.kind == TXT_TokenKind_Identifier)
        {
          String8 token_string = str8_substr(string, token.range);
          U64 hash = u64_hash_from_str8(token_string);
          U64 slot_idx = hash%keyword_slots_count;
          for EachNode(n, String8Node, keyword_slots[slot_idx])
          {
            if(str8_match(token_string, n->string, 0))
            {
              token.kind = TXT_TokenKind_Keyword;
              break;
            }
          }
          txt_token_chunk_list_push(scratch.arena, &tokens, chunk_size, &token);
        }
        
        // rjf: split symbols by maximum-munch-rule
        else if(token.kind == TXT_TokenKind_Symbol)
        {
          String8 token_string = str8_substr(string, token.range);
          for(U64 off = 0, next_off = token_string.size; off < token_string.size; off = next_off)
          {
            B32 found = 0;
            for(U64 idx = 0; idx < multichar_symbols.count; idx += 1)
            {
              if(str8_match(str8_substr(token_string, r1u64(off, off+multichar_symbols.v[idx].size)),
                            multichar_symbols.v[idx],
                            0))
              {
                found = 1;
                next_off = off + Max(1, multichar_symbols.v[idx].size);
                TXT_Token sub_token = {TXT_TokenKind_Symbol, r1u64(token.range.min+off, token.range.min+next_off)};
                txt_token_chunk_list_push(scratch.arena, &tokens, chunk_size, &sub_token);
                break;
              }
            }
            if(!found)
            {
              next_off = off+1;
              TXT_Token sub_token = {TXT_TokenKind_Symbol, r1u64(token.range.min+off, token.range.min+next_off)};
              txt_token_chunk_list_push(scratch.arena, &tokens, chunk_size, &sub_token);
            }
          }
        }
        
        // rjf: all other tokens
        else
        {
          txt_token_chunk_list_push(scratch.arena, &tokens, chunk_size, &token);
        }
      }
      
      // rjf: start new token
      if(new_token_kind != TXT_TokenKind_Null)
      {
        TokenTask *task = free_task;
        if(task != 0)
        {
          SLLStackPop(free_task);
        }
        else
        {
          task = push_array(scratch.arena, TokenTask, 1);
        }
        SLLStackPush(top_task, task);
        top_task->rule = new_rule;
        top_task->kind = new_token_kind;
        top_task->start_idx = idx;
      }
      
      // rjf: invalid token kind -> emit error
      else if(new_token_needed)
      {
        TXT_Token token = {TXT_TokenKind_Error, r1u64(idx, idx+1)};
        txt_token_chunk_list_push(scratch.arena, &tokens, chunk_size, &token);
      }
      
      // rjf: advance by 1 byte if we haven't found an ender
      if(!ender_found)
      {
        idx += 1;
      }
      
      // rjf: advance by ender padding
      idx += ender_pad;
      
      // rjf: advance escaping
      escaped = next_escaped;
    }
  }
  
  //- rjf: token list -> token array
  TXT_TokenArray result = txt_token_array_from_chunk_list(arena, &tokens);
  scratch_end(scratch);
  ProfEnd();
  return result;
}

internal TXT_TokenArray
txt_token_array_from_string__c_cpp(Arena *arena, U64 *bytes_processed_counter, String8 string)
{
  ProfBeginFunction();
  Temp scratch = scratch_begin(&arena, 1);
  U64 chunk_size = Clamp(8, string.size/8, 4096);
  
  //- rjf: generate token list
  TXT_TokenChunkList tokens = {0};
  {
    B32 comment_is_single_line = 0;
    B32 string_is_char = 0;
    TXT_TokenKind active_token_kind = TXT_TokenKind_Null;
    U64 active_token_start_idx = 0;
    B32 escaped = 0;
    B32 next_escaped = 0;
    U64 byte_process_start_idx = 0;
    for(U64 idx = 0; idx <= string.size;)
    {
      U8 byte      = (idx+0 < string.size) ? (string.str[idx+0]) : 0;
      U8 next_byte = (idx+1 < string.size) ? (string.str[idx+1]) : 0;
      
      // rjf: update counter
      if(bytes_processed_counter != 0 && ((idx-byte_process_start_idx) >= 1000 || idx == string.size))
      {
        ins_atomic_u64_add_eval(bytes_processed_counter, (idx-byte_process_start_idx));
        byte_process_start_idx = idx;
      }
      
      // rjf: escaping
      if(escaped && (byte != '\r' && byte != '\n'))
      {
        next_escaped = 0;
      }
      else if(!escaped && byte == '\\')
      {
        next_escaped = 1;
      }
      
      // rjf: take starter, determine active token kind
      if(active_token_kind == TXT_TokenKind_Null)
      {
        // rjf: use next bytes to start a new token
        if(0){}
        else if(char_is_space(byte))             { active_token_kind = TXT_TokenKind_Whitespace; }
        else if(byte == '_' ||
                byte == '$' ||
                char_is_alpha(byte))             { active_token_kind = TXT_TokenKind_Identifier; }
        else if(char_is_digit(byte, 10) ||
                (byte == '.' &&
                 char_is_digit(next_byte, 10)))  { active_token_kind = TXT_TokenKind_Numeric; }
        else if(byte == '"')                     { active_token_kind = TXT_TokenKind_String; string_is_char = 0; }
        else if(byte == '\'')                    { active_token_kind = TXT_TokenKind_String; string_is_char = 1; }
        else if(byte == '/' && next_byte == '/') { active_token_kind = TXT_TokenKind_LineComment; comment_is_single_line = 1; }
        else if(byte == '/' && next_byte == '*') { active_token_kind = TXT_TokenKind_BlockComment; comment_is_single_line = 0; }
        else if(byte == '~' || byte == '!' ||
                byte == '%' || byte == '^' ||
                byte == '&' || byte == '*' ||
                byte == '(' || byte == ')' ||
                byte == '-' || byte == '=' ||
                byte == '+' || byte == '[' ||
                byte == ']' || byte == '{' ||
                byte == '}' || byte == ':' ||
                byte == ';' || byte == ',' ||
                byte == '.' || byte == '<' ||
                byte == '>' || byte == '/' ||
                byte == '?' || byte == '|')      { active_token_kind = TXT_TokenKind_Symbol; }
        else if(byte == '#')                     { active_token_kind = TXT_TokenKind_Meta; }
        
        // rjf: start new token
        if(active_token_kind != TXT_TokenKind_Null)
        {
          active_token_start_idx = idx;
        }
        
        // rjf: invalid token kind -> emit error
        else
        {
          TXT_Token token = {TXT_TokenKind_Error, r1u64(idx, idx+1)};
          txt_token_chunk_list_push(scratch.arena, &tokens, chunk_size, &token);
        }
      }
      
      // rjf: look for ender
      U64 ender_pad = 0;
      B32 ender_found = 0;
      if(active_token_kind != TXT_TokenKind_Null && idx>active_token_start_idx)
      {
        if(idx == string.size)
        {
          ender_pad = 0;
          ender_found = 1;
        }
        else switch(active_token_kind)
        {
          default:break;
          case TXT_TokenKind_Whitespace:
          {
            ender_found = !char_is_space(byte);
          }break;
          case TXT_TokenKind_Identifier:
          {
            ender_found = (!char_is_alpha(byte) && !char_is_digit(byte, 10) && byte != '_' && byte != '$' && byte < 128);
          }break;
          case TXT_TokenKind_Numeric:
          {
            ender_found = (!char_is_alpha(byte) && !char_is_digit(byte, 10) && byte != '_' && byte != '.' && byte != '\'');
          }break;
          case TXT_TokenKind_String:
          {
            ender_found = (!escaped && ((!string_is_char && byte == '"') || (string_is_char && byte == '\'')));
            ender_pad += 1;
          }break;
          case TXT_TokenKind_Symbol:
          {
            ender_found = (byte != '~' && byte != '!' &&
                           byte != '%' && byte != '^' &&
                           byte != '&' && byte != '*' &&
                           byte != '(' && byte != ')' &&
                           byte != '-' && byte != '=' &&
                           byte != '+' && byte != '[' &&
                           byte != ']' && byte != '{' &&
                           byte != '}' && byte != ':' &&
                           byte != ';' && byte != ',' &&
                           byte != '.' && byte != '<' &&
                           byte != '>' && byte != '/' &&
                           byte != '?' && byte != '|');
          }break;
          case TXT_TokenKind_LineComment:
          {
            ender_found = (!escaped && (byte == '\r' || byte == '\n'));
          }break;
          case TXT_TokenKind_BlockComment:
          {
            ender_found = (active_token_start_idx+1 < idx && byte == '*' && next_byte == '/');
            ender_pad += 2;
          }break;
          case TXT_TokenKind_Meta:
          {
            ender_found = (!escaped && (byte == '\r' || byte == '\n'));
          }break;
        }
      }
      
      // rjf: next byte is ender => emit token
      if(ender_found)
      {
        TXT_Token token = {active_token_kind, r1u64(active_token_start_idx, idx+ender_pad)};
        active_token_kind = TXT_TokenKind_Null;
        
        // rjf: identifier -> keyword in special cases
        if(token.kind == TXT_TokenKind_Identifier)
        {
          read_only local_persist String8 cpp_keywords[] =
          {
            str8_lit_comp("alignas"),
            str8_lit_comp("alignof"),
            str8_lit_comp("and"),
            str8_lit_comp("and_eq"),
            str8_lit_comp("asm"),
            str8_lit_comp("atomic_cancel"),
            str8_lit_comp("atomic_commit"),
            str8_lit_comp("atomic_noexcept"),
            str8_lit_comp("auto"),
            str8_lit_comp("bitand"),
            str8_lit_comp("bitor"),
            str8_lit_comp("bool"),
            str8_lit_comp("break"),
            str8_lit_comp("case"),
            str8_lit_comp("catch"),
            str8_lit_comp("char"),
            str8_lit_comp("char8_t"),
            str8_lit_comp("char16_t"),
            str8_lit_comp("char32_t"),
            str8_lit_comp("class"),
            str8_lit_comp("compl"),
            str8_lit_comp("concept"),
            str8_lit_comp("const"),
            str8_lit_comp("consteval"),
            str8_lit_comp("constexpr"),
            str8_lit_comp("constinit"),
            str8_lit_comp("const_cast"),
            str8_lit_comp("continue"),
            str8_lit_comp("co_await"),
            str8_lit_comp("co_return"),
            str8_lit_comp("co_yield"),
            str8_lit_comp("decltype"),
            str8_lit_comp("default"),
            str8_lit_comp("delete"),
            str8_lit_comp("do"),
            str8_lit_comp("double"),
            str8_lit_comp("dynamic_cast"),
            str8_lit_comp("else"),
            str8_lit_comp("enum"),
            str8_lit_comp("explicit"),
            str8_lit_comp("export"),
            str8_lit_comp("extern"),
            str8_lit_comp("false"),
            str8_lit_comp("float"),
            str8_lit_comp("for"),
            str8_lit_comp("friend"),
            str8_lit_comp("goto"),
            str8_lit_comp("if"),
            str8_lit_comp("inline"),
            str8_lit_comp("int"),
            str8_lit_comp("long"),
            str8_lit_comp("mutable"),
            str8_lit_comp("namespace"),
            str8_lit_comp("new"),
            str8_lit_comp("noexcept"),
            str8_lit_comp("not"),
            str8_lit_comp("not_eq"),
            str8_lit_comp("nullptr"),
            str8_lit_comp("operator"),
            str8_lit_comp("or"),
            str8_lit_comp("or_eq"),
            str8_lit_comp("private"),
            str8_lit_comp("protected"),
            str8_lit_comp("public"),
            str8_lit_comp("reflexpr"),
            str8_lit_comp("register"),
            str8_lit_comp("reinterpret_cast"),
            str8_lit_comp("requires"),
            str8_lit_comp("return"),
            str8_lit_comp("short"),
            str8_lit_comp("signed"),
            str8_lit_comp("sizeof"),
            str8_lit_comp("static"),
            str8_lit_comp("static_assert"),
            str8_lit_comp("static_cast"),
            str8_lit_comp("struct"),
            str8_lit_comp("switch"),
            str8_lit_comp("synchronized"),
            str8_lit_comp("template"),
            str8_lit_comp("this"),
            str8_lit_comp("thread_local"),
            str8_lit_comp("throw"),
            str8_lit_comp("true"),
            str8_lit_comp("try"),
            str8_lit_comp("typedef"),
            str8_lit_comp("typeid"),
            str8_lit_comp("typename"),
            str8_lit_comp("union"),
            str8_lit_comp("unsigned"),
            str8_lit_comp("using"),
            str8_lit_comp("virtual"),
            str8_lit_comp("void"),
            str8_lit_comp("volatile"),
            str8_lit_comp("wchar_t"),
            str8_lit_comp("while"),
            str8_lit_comp("xor"),
            str8_lit_comp("xor_eq"),
          };
          String8 token_string = str8_substr(string, r1u64(active_token_start_idx, idx+ender_pad));
          for(U64 keyword_idx = 0; keyword_idx < ArrayCount(cpp_keywords); keyword_idx += 1)
          {
            if(str8_match(cpp_keywords[keyword_idx], token_string, 0))
            {
              token.kind = TXT_TokenKind_Keyword;
              break;
            }
          }
          txt_token_chunk_list_push(scratch.arena, &tokens, chunk_size, &token);
        }
        
        // rjf: split symbols by maximum-munch-rule
        else if(token.kind == TXT_TokenKind_Symbol)
        {
          read_only local_persist String8 c_cpp_multichar_symbol_strings[] =
          {
            str8_lit_comp("<<"),
            str8_lit_comp(">>"),
            str8_lit_comp("<="),
            str8_lit_comp(">="),
            str8_lit_comp("=="),
            str8_lit_comp("!="),
            str8_lit_comp("&&"),
            str8_lit_comp("||"),
            str8_lit_comp("|="),
            str8_lit_comp("&="),
            str8_lit_comp("^="),
            str8_lit_comp("~="),
            str8_lit_comp("+="),
            str8_lit_comp("-="),
            str8_lit_comp("*="),
            str8_lit_comp("/="),
            str8_lit_comp("%="),
            str8_lit_comp("<<="),
            str8_lit_comp(">>="),
            str8_lit_comp("->"),
          };
          String8 token_string = str8_substr(string, r1u64(active_token_start_idx, idx+ender_pad));
          for(U64 off = 0, next_off = token_string.size; off < token_string.size; off = next_off)
          {
            B32 found = 0;
            for(U64 idx = 0; idx < ArrayCount(c_cpp_multichar_symbol_strings); idx += 1)
            {
              if(str8_match(str8_substr(token_string, r1u64(off, off+c_cpp_multichar_symbol_strings[idx].size)),
                            c_cpp_multichar_symbol_strings[idx],
                            0))
              {
                found = 1;
                next_off = off + c_cpp_multichar_symbol_strings[idx].size;
                TXT_Token token = {TXT_TokenKind_Symbol, r1u64(active_token_start_idx+off, active_token_start_idx+next_off)};
                txt_token_chunk_list_push(scratch.arena, &tokens, chunk_size, &token);
                break;
              }
            }
            if(!found)
            {
              next_off = off+1;
              TXT_Token token = {TXT_TokenKind_Symbol, r1u64(active_token_start_idx+off, active_token_start_idx+next_off)};
              txt_token_chunk_list_push(scratch.arena, &tokens, chunk_size, &token);
            }
          }
        }
        
        // rjf: all other tokens
        else
        {
          txt_token_chunk_list_push(scratch.arena, &tokens, chunk_size, &token);
        }
        
        // rjf: increment by ender padding
        idx += ender_pad;
      }
      
      // rjf: advance by 1 byte if we haven't found an ender
      if(!ender_found)
      {
        idx += 1;
      }
      escaped = next_escaped;
    }
  }
  
  //- rjf: token list -> token array
  TXT_TokenArray result = txt_token_array_from_chunk_list(arena, &tokens);
  scratch_end(scratch);
  ProfEnd();
  return result;
}

internal TXT_TokenArray
txt_token_array_from_string__odin(Arena *arena, U64 *bytes_processed_counter, String8 string)
{
  Temp scratch = scratch_begin(&arena, 1);
  
  //- rjf: generate token list
  TXT_TokenChunkList tokens = {0};
  {
    B32 comment_is_single_line = 0;
    B32 string_is_char = 0;
    TXT_TokenKind active_token_kind = TXT_TokenKind_Null;
    U64 active_token_start_idx = 0;
    B32 escaped = 0;
    B32 next_escaped = 0;
    U64 byte_process_start_idx = 0;
    for(U64 idx = 0; idx <= string.size;)
    {
      U8 byte      = (idx+0 < string.size) ? (string.str[idx+0]) : 0;
      U8 next_byte = (idx+1 < string.size) ? (string.str[idx+1]) : 0;
      
      // rjf: update counter
      if(bytes_processed_counter != 0 && ((idx-byte_process_start_idx) >= 1000 || idx == string.size))
      {
        ins_atomic_u64_add_eval(bytes_processed_counter, (idx-byte_process_start_idx));
        byte_process_start_idx = idx;
      }
      
      // rjf: escaping
      if(escaped && (byte != '\r' && byte != '\n'))
      {
        next_escaped = 0;
      }
      else if(!escaped && byte == '\\')
      {
        next_escaped = 1;
      }
      
      // rjf: take starter, determine active token kind
      if(active_token_kind == TXT_TokenKind_Null)
      {
        // rjf: use next bytes to start a new token
        if(0){}
        else if(char_is_space(byte))             { active_token_kind = TXT_TokenKind_Whitespace; }
        else if(byte == '_' ||
                byte == '$' ||
                char_is_alpha(byte))             { active_token_kind = TXT_TokenKind_Identifier; }
        else if(char_is_digit(byte, 10) ||
                (byte == '.' &&
                 char_is_digit(next_byte, 10)))  { active_token_kind = TXT_TokenKind_Numeric; }
        else if(byte == '"')                     { active_token_kind = TXT_TokenKind_String; string_is_char = 0; }
        else if(byte == '\'')                    { active_token_kind = TXT_TokenKind_String; string_is_char = 1; }
        else if(byte == '/' && next_byte == '/') { active_token_kind = TXT_TokenKind_LineComment; comment_is_single_line = 1; }
        else if(byte == '/' && next_byte == '*') { active_token_kind = TXT_TokenKind_BlockComment; comment_is_single_line = 0; }
        else if(byte == '~' || byte == '!' ||
                byte == '%' || byte == '^' ||
                byte == '&' || byte == '*' ||
                byte == '(' || byte == ')' ||
                byte == '-' || byte == '=' ||
                byte == '+' || byte == '[' ||
                byte == ']' || byte == '{' ||
                byte == '}' || byte == ':' ||
                byte == ';' || byte == ',' ||
                byte == '.' || byte == '<' ||
                byte == '>' || byte == '/' ||
                byte == '?' || byte == '|')      { active_token_kind = TXT_TokenKind_Symbol; }
        else if(byte == '#')                     { active_token_kind = TXT_TokenKind_Meta; }
        
        // rjf: start new token
        if(active_token_kind != TXT_TokenKind_Null)
        {
          active_token_start_idx = idx;
        }
        
        // rjf: invalid token kind -> emit error
        else
        {
          TXT_Token token = {TXT_TokenKind_Error, r1u64(idx, idx+1)};
          txt_token_chunk_list_push(scratch.arena, &tokens, 4096, &token);
        }
      }
      
      // rjf: look for ender
      U64 ender_pad = 0;
      B32 ender_found = 0;
      if(active_token_kind != TXT_TokenKind_Null && idx>active_token_start_idx)
      {
        if(idx == string.size)
        {
          ender_pad = 0;
          ender_found = 1;
        }
        else switch(active_token_kind)
        {
          default:break;
          case TXT_TokenKind_Whitespace:
          {
            ender_found = !char_is_space(byte);
          }break;
          case TXT_TokenKind_Identifier:
          {
            ender_found = (!char_is_alpha(byte) && !char_is_digit(byte, 10) && byte != '_' && byte != '$' && byte < 128);
          }break;
          case TXT_TokenKind_Numeric:
          {
            ender_found = (!char_is_alpha(byte) && !char_is_digit(byte, 10) && byte != '_' && byte != '.' && byte != '\'');
          }break;
          case TXT_TokenKind_String:
          {
            ender_found = (!escaped && ((!string_is_char && byte == '"') || (string_is_char && byte == '\'')));
            ender_pad += 1;
          }break;
          case TXT_TokenKind_Symbol:
          {
            ender_found = (byte != '~' && byte != '!' &&
                           byte != '%' && byte != '^' &&
                           byte != '&' && byte != '*' &&
                           byte != '(' && byte != ')' &&
                           byte != '-' && byte != '=' &&
                           byte != '+' && byte != '[' &&
                           byte != ']' && byte != '{' &&
                           byte != '}' && byte != ':' &&
                           byte != ';' && byte != ',' &&
                           byte != '.' && byte != '<' &&
                           byte != '>' && byte != '/' &&
                           byte != '?' && byte != '|');
          }break;
          case TXT_TokenKind_LineComment:
          {
            ender_found = (!escaped && (byte == '\r' || byte == '\n'));
          }break;
          case TXT_TokenKind_BlockComment:
          {
            ender_found = (active_token_start_idx+1 < idx && byte == '*' && next_byte == '/');
            ender_pad += 2;
          }break;
          case TXT_TokenKind_Meta:
          {
            ender_found = (!char_is_alpha(byte) && !char_is_digit(byte, 10) && byte != '_' && byte != '$');
          }break;
        }
      }
      
      // rjf: next byte is ender => emit token
      if(ender_found)
      {
        TXT_Token token = {active_token_kind, r1u64(active_token_start_idx, idx+ender_pad)};
        active_token_kind = TXT_TokenKind_Null;
        
        // rjf: identifier -> keyword in special cases
        if(token.kind == TXT_TokenKind_Identifier)
        {
          read_only local_persist String8 odin_keywords[] =
          {
            str8_lit_comp("align_of"),
            str8_lit_comp("asm"),
            str8_lit_comp("auto_cast"),
            str8_lit_comp("bit_set"),
            str8_lit_comp("break"),
            str8_lit_comp("case"),
            str8_lit_comp("cast"),
            str8_lit_comp("context"),
            str8_lit_comp("continue"),
            str8_lit_comp("defer"),
            str8_lit_comp("distinct"),
            str8_lit_comp("do"),
            str8_lit_comp("dynamic"),
            str8_lit_comp("else"),
            str8_lit_comp("enum"),
            str8_lit_comp("fallthrough"),
            str8_lit_comp("for"),
            str8_lit_comp("foreign"),
            str8_lit_comp("if"),
            str8_lit_comp("in"),
            str8_lit_comp("map"),
            str8_lit_comp("matrix"),
            str8_lit_comp("not_in"),
            str8_lit_comp("or_break"),
            str8_lit_comp("or_continue"),
            str8_lit_comp("or_else"),
            str8_lit_comp("or_return"),
            str8_lit_comp("package"),
            str8_lit_comp("proc"),
            str8_lit_comp("return"),
            str8_lit_comp("size_of"),
            str8_lit_comp("struct"),
            str8_lit_comp("switch"),
            str8_lit_comp("transmute"),
            str8_lit_comp("typeid"),
            str8_lit_comp("union"),
            str8_lit_comp("using"),
            str8_lit_comp("when"),
            str8_lit_comp("where"),
            str8_lit_comp("import"),
          };
          String8 token_string = str8_substr(string, r1u64(active_token_start_idx, idx+ender_pad));
          for(U64 keyword_idx = 0; keyword_idx < ArrayCount(odin_keywords); keyword_idx += 1)
          {
            if(str8_match(odin_keywords[keyword_idx], token_string, 0))
            {
              token.kind = TXT_TokenKind_Keyword;
              break;
            }
          }
          txt_token_chunk_list_push(scratch.arena, &tokens, 4096, &token);
        }
        
        // rjf: split symbols by maximum-munch-rule
        else if(token.kind == TXT_TokenKind_Symbol)
        {
          read_only local_persist String8 odin_multichar_symbol_strings[] =
          {
            str8_lit_comp("<<"),
            str8_lit_comp(">>"),
            str8_lit_comp("<="),
            str8_lit_comp(">="),
            str8_lit_comp("=="),
            str8_lit_comp("!="),
            str8_lit_comp("&&"),
            str8_lit_comp("||"),
            str8_lit_comp("|="),
            str8_lit_comp("&="),
            str8_lit_comp("^="),
            str8_lit_comp("~="),
            str8_lit_comp("+="),
            str8_lit_comp("-="),
            str8_lit_comp("*="),
            str8_lit_comp("/="),
            str8_lit_comp("%="),
            str8_lit_comp("<<="),
            str8_lit_comp(">>="),
            str8_lit_comp("->"),
          };
          String8 token_string = str8_substr(string, r1u64(active_token_start_idx, idx+ender_pad));
          for(U64 off = 0, next_off = token_string.size; off < token_string.size; off = next_off)
          {
            B32 found = 0;
            for(U64 idx = 0; idx < ArrayCount(odin_multichar_symbol_strings); idx += 1)
            {
              if(str8_match(str8_substr(token_string, r1u64(off, off+odin_multichar_symbol_strings[idx].size)),
                            odin_multichar_symbol_strings[idx],
                            0))
              {
                found = 1;
                next_off = off + odin_multichar_symbol_strings[idx].size;
                TXT_Token token = {TXT_TokenKind_Symbol, r1u64(active_token_start_idx+off, active_token_start_idx+next_off)};
                txt_token_chunk_list_push(scratch.arena, &tokens, 4096, &token);
                break;
              }
            }
            if(!found)
            {
              next_off = off+1;
              TXT_Token token = {TXT_TokenKind_Symbol, r1u64(active_token_start_idx+off, active_token_start_idx+next_off)};
              txt_token_chunk_list_push(scratch.arena, &tokens, 4096, &token);
            }
          }
        }
        
        // rjf: all other tokens
        else
        {
          txt_token_chunk_list_push(scratch.arena, &tokens, 4096, &token);
        }
        
        // rjf: increment by ender padding
        idx += ender_pad;
      }
      
      // rjf: advance by 1 byte if we haven't found an ender
      if(!ender_found)
      {
        idx += 1;
      }
      escaped = next_escaped;
    }
  }
  
  //- rjf: token list -> token array
  TXT_TokenArray result = txt_token_array_from_chunk_list(arena, &tokens);
  scratch_end(scratch);
  return result;
}

internal TXT_TokenArray
txt_token_array_from_string__jai(Arena *arena, U64 *bytes_processed_counter, String8 string)
{
  Temp scratch = scratch_begin(&arena, 1);
  
  //- rjf: generate token list
  TXT_TokenChunkList tokens = {0};
  {
    B32 comment_is_single_line = 0;
    B32 string_is_char = 0;
    TXT_TokenKind active_token_kind = TXT_TokenKind_Null;
    U64 active_token_start_idx = 0;
    B32 escaped = 0;
    B32 next_escaped = 0;
    U64 byte_process_start_idx = 0;
    for(U64 idx = 0; idx <= string.size;)
    {
      U8 byte      = (idx+0 < string.size) ? (string.str[idx+0]) : 0;
      U8 next_byte = (idx+1 < string.size) ? (string.str[idx+1]) : 0;
      
      // rjf: update counter
      if(bytes_processed_counter != 0 && ((idx-byte_process_start_idx) >= 1000 || idx == string.size))
      {
        ins_atomic_u64_add_eval(bytes_processed_counter, (idx-byte_process_start_idx));
        byte_process_start_idx = idx;
      }
      
      // rjf: escaping
      if(escaped && (byte != '\r' && byte != '\n'))
      {
        next_escaped = 0;
      }
      else if(!escaped && byte == '\\')
      {
        next_escaped = 1;
      }
      
      // rjf: take starter, determine active token kind
      if(active_token_kind == TXT_TokenKind_Null)
      {
        // rjf: use next bytes to start a new token
        if(0){}
        else if(char_is_space(byte))             { active_token_kind = TXT_TokenKind_Whitespace; }
        else if(byte == '_' ||
                byte == '$' ||
                char_is_alpha(byte))             { active_token_kind = TXT_TokenKind_Identifier; }
        else if(char_is_digit(byte, 10) ||
                (byte == '.' &&
                 char_is_digit(next_byte, 10)))  { active_token_kind = TXT_TokenKind_Numeric; }
        else if(byte == '"')                     { active_token_kind = TXT_TokenKind_String; string_is_char = 0; }
        else if(byte == '\'')                    { active_token_kind = TXT_TokenKind_String; string_is_char = 1; }
        else if(byte == '/' && next_byte == '/') { active_token_kind = TXT_TokenKind_LineComment; comment_is_single_line = 1; }
        else if(byte == '/' && next_byte == '*') { active_token_kind = TXT_TokenKind_BlockComment; comment_is_single_line = 0; }
        else if(byte == '~' || byte == '!' ||
                byte == '%' || byte == '^' ||
                byte == '&' || byte == '*' ||
                byte == '(' || byte == ')' ||
                byte == '-' || byte == '=' ||
                byte == '+' || byte == '[' ||
                byte == ']' || byte == '{' ||
                byte == '}' || byte == ':' ||
                byte == ';' || byte == ',' ||
                byte == '.' || byte == '<' ||
                byte == '>' || byte == '/' ||
                byte == '?' || byte == '|')      { active_token_kind = TXT_TokenKind_Symbol; }
        else if(byte == '#')                     { active_token_kind = TXT_TokenKind_Meta; }
        
        // rjf: start new token
        if(active_token_kind != TXT_TokenKind_Null)
        {
          active_token_start_idx = idx;
        }
        
        // rjf: invalid token kind -> emit error
        else
        {
          TXT_Token token = {TXT_TokenKind_Error, r1u64(idx, idx+1)};
          txt_token_chunk_list_push(scratch.arena, &tokens, 4096, &token);
        }
      }
      
      // rjf: look for ender
      U64 ender_pad = 0;
      B32 ender_found = 0;
      if(active_token_kind != TXT_TokenKind_Null && idx>active_token_start_idx)
      {
        if(idx == string.size)
        {
          ender_pad = 0;
          ender_found = 1;
        }
        else switch(active_token_kind)
        {
          default:break;
          case TXT_TokenKind_Whitespace:
          {
            ender_found = !char_is_space(byte);
          }break;
          case TXT_TokenKind_Identifier:
          {
            ender_found = (!char_is_alpha(byte) && !char_is_digit(byte, 10) && byte != '_' && byte != '$' && byte < 128);
          }break;
          case TXT_TokenKind_Numeric:
          {
            ender_found = (!char_is_alpha(byte) && !char_is_digit(byte, 10) && byte != '_' && byte != '.' && byte != '\'');
          }break;
          case TXT_TokenKind_String:
          {
            ender_found = (!escaped && ((!string_is_char && byte == '"') || (string_is_char && byte == '\'')));
            ender_pad += 1;
          }break;
          case TXT_TokenKind_Symbol:
          {
            ender_found = (byte != '~' && byte != '!' &&
                           byte != '%' && byte != '^' &&
                           byte != '&' && byte != '*' &&
                           byte != '(' && byte != ')' &&
                           byte != '-' && byte != '=' &&
                           byte != '+' && byte != '[' &&
                           byte != ']' && byte != '{' &&
                           byte != '}' && byte != ':' &&
                           byte != ';' && byte != ',' &&
                           byte != '.' && byte != '<' &&
                           byte != '>' && byte != '/' &&
                           byte != '?' && byte != '|');
          }break;
          case TXT_TokenKind_LineComment:
          {
            ender_found = (!escaped && (byte == '\r' || byte == '\n'));
          }break;
          case TXT_TokenKind_BlockComment:
          {
            ender_found = (active_token_start_idx+1 < idx && byte == '*' && next_byte == '/');
            ender_pad += 2;
          }break;
          case TXT_TokenKind_Meta:
          {
            ender_found = (!char_is_alpha(byte) && !char_is_digit(byte, 10) && byte != '_' && byte != '$');
          }break;
        }
      }
      
      // rjf: next byte is ender => emit token
      if(ender_found)
      {
        TXT_Token token = {active_token_kind, r1u64(active_token_start_idx, idx+ender_pad)};
        active_token_kind = TXT_TokenKind_Null;
        
        // rjf: identifier -> keyword in special cases
        if(token.kind == TXT_TokenKind_Identifier)
        {
          read_only local_persist String8 jai_keywords[] =
          {
            str8_lit_comp("bool"),
            str8_lit_comp("true"),
            str8_lit_comp("false"),
            str8_lit_comp("int"),
            str8_lit_comp("s8"),
            str8_lit_comp("u8"),
            str8_lit_comp("s16"),
            str8_lit_comp("u16"),
            str8_lit_comp("s32"),
            str8_lit_comp("u32"),
            str8_lit_comp("s64"),
            str8_lit_comp("u64"),
            str8_lit_comp("s128"),
            str8_lit_comp("u128"),
            str8_lit_comp("float"),
            str8_lit_comp("float32"),
            str8_lit_comp("float64"),
            str8_lit_comp("void"),
            str8_lit_comp("enum"),
            str8_lit_comp("enum_flags"),
            str8_lit_comp("size_of"),
            str8_lit_comp("string"),
            str8_lit_comp("type_of"),
            str8_lit_comp("cast"),
            str8_lit_comp("if"),
            str8_lit_comp("ifs"),
            str8_lit_comp("then"),
            str8_lit_comp("else"),
            str8_lit_comp("case"),
            str8_lit_comp("for"),
            str8_lit_comp("while"),
            str8_lit_comp("break"),
            str8_lit_comp("continue"),
            str8_lit_comp("remove"),
            str8_lit_comp("return"),
            str8_lit_comp("inline"),
            str8_lit_comp("null"),
            str8_lit_comp("defer"),
            str8_lit_comp("xx"),
          };
          String8 token_string = str8_substr(string, r1u64(active_token_start_idx, idx+ender_pad));
          for(U64 keyword_idx = 0; keyword_idx < ArrayCount(jai_keywords); keyword_idx += 1)
          {
            if(str8_match(jai_keywords[keyword_idx], token_string, 0))
            {
              token.kind = TXT_TokenKind_Keyword;
              break;
            }
          }
          txt_token_chunk_list_push(scratch.arena, &tokens, 4096, &token);
        }
        
        // rjf: split symbols by maximum-munch-rule
        else if(token.kind == TXT_TokenKind_Symbol)
        {
          read_only local_persist String8 jai_multichar_symbol_strings[] =
          {
            str8_lit_comp("<<"),
            str8_lit_comp(">>"),
            str8_lit_comp("<="),
            str8_lit_comp(">="),
            str8_lit_comp("=="),
            str8_lit_comp("!="),
            str8_lit_comp("&&"),
            str8_lit_comp("||"),
            str8_lit_comp("|="),
            str8_lit_comp("&="),
            str8_lit_comp("^="),
            str8_lit_comp("~="),
            str8_lit_comp("+="),
            str8_lit_comp("-="),
            str8_lit_comp("*="),
            str8_lit_comp("/="),
            str8_lit_comp("%="),
            str8_lit_comp("<<="),
            str8_lit_comp(">>="),
            str8_lit_comp("->"),
          };
          String8 token_string = str8_substr(string, r1u64(active_token_start_idx, idx+ender_pad));
          for(U64 off = 0, next_off = token_string.size; off < token_string.size; off = next_off)
          {
            B32 found = 0;
            for(U64 idx = 0; idx < ArrayCount(jai_multichar_symbol_strings); idx += 1)
            {
              if(str8_match(str8_substr(token_string, r1u64(off, off+jai_multichar_symbol_strings[idx].size)),
                            jai_multichar_symbol_strings[idx],
                            0))
              {
                found = 1;
                next_off = off + jai_multichar_symbol_strings[idx].size;
                TXT_Token token = {TXT_TokenKind_Symbol, r1u64(active_token_start_idx+off, active_token_start_idx+next_off)};
                txt_token_chunk_list_push(scratch.arena, &tokens, 4096, &token);
                break;
              }
            }
            if(!found)
            {
              next_off = off+1;
              TXT_Token token = {TXT_TokenKind_Symbol, r1u64(active_token_start_idx+off, active_token_start_idx+next_off)};
              txt_token_chunk_list_push(scratch.arena, &tokens, 4096, &token);
            }
          }
        }
        
        // rjf: all other tokens
        else
        {
          txt_token_chunk_list_push(scratch.arena, &tokens, 4096, &token);
        }
        
        // rjf: increment by ender padding
        idx += ender_pad;
      }
      
      // rjf: advance by 1 byte if we haven't found an ender
      if(!ender_found)
      {
        idx += 1;
      }
      escaped = next_escaped;
    }
  }
  
  //- rjf: token list -> token array
  TXT_TokenArray result = txt_token_array_from_chunk_list(arena, &tokens);
  scratch_end(scratch);
  return result;
}

internal TXT_TokenArray
txt_token_array_from_string__zig(Arena *arena, U64 *bytes_processed_counter, String8 string)
{
  Temp scratch = scratch_begin(&arena, 1);
  
  //- rjf: generate token list
  TXT_TokenChunkList tokens = {0};
  {
    B32 string_is_char = 0;
    B32 string_is_line = 0;
    TXT_TokenKind active_token_kind = TXT_TokenKind_Null;
    U64 active_token_start_idx = 0;
    B32 escaped = 0;
    B32 next_escaped = 0;
    U64 byte_process_start_idx = 0;
    for(U64 idx = 0; idx <= string.size;)
    {
      U8 byte        = (idx+0 < string.size) ? (string.str[idx+0]) : 0;
      U8 next_byte   = (idx+1 < string.size) ? (string.str[idx+1]) : 0;
      
      // rjf: update counter
      if(bytes_processed_counter != 0 && ((idx-byte_process_start_idx) >= 1000 || idx == string.size))
      {
        ins_atomic_u64_add_eval(bytes_processed_counter, (idx-byte_process_start_idx));
        byte_process_start_idx = idx;
      }
      
      // rjf: escaping
      if(escaped && (byte != '\r' && byte != '\n'))
      {
        next_escaped = 0;
      }
      else if(!escaped && byte == '\\')
      {
        next_escaped = 1;
      }
      
      // rjf: take starter, determine active token kind
      if(active_token_kind == TXT_TokenKind_Null)
      {
        // rjf: use next bytes to start a new token
        if(0){}
        else if(char_is_space(byte))             { active_token_kind = TXT_TokenKind_Whitespace; }
        else if(byte == '_' ||
                char_is_alpha(byte))             { active_token_kind = TXT_TokenKind_Identifier; }
        else if(char_is_digit(byte, 10) ||
                (byte == '.' &&
                 char_is_digit(next_byte, 10)))  { active_token_kind = TXT_TokenKind_Numeric; }
        else if(byte == '"')                     { active_token_kind = TXT_TokenKind_String; string_is_char = 0; }
        else if(byte == '\'')                    { active_token_kind = TXT_TokenKind_String; string_is_char = 1; }
        else if(byte == '\\' &&
                next_byte == '\\')               { active_token_kind = TXT_TokenKind_String; string_is_line = 1; }
        else if(byte == '/' && next_byte == '/') { active_token_kind = TXT_TokenKind_LineComment; }
        else if(byte == '~' || byte == '!' ||
                byte == '%' || byte == '^' ||
                byte == '&' || byte == '*' ||
                byte == '(' || byte == ')' ||
                byte == '-' || byte == '=' ||
                byte == '+' || byte == '[' ||
                byte == ']' || byte == '{' ||
                byte == '}' || byte == ':' ||
                byte == ';' || byte == ',' ||
                byte == '.' || byte == '<' ||
                byte == '>' || byte == '/' ||
                byte == '?' || byte == '|' ||
                byte == 'c')                     { active_token_kind = TXT_TokenKind_Symbol; }
        
        // rjf: start new token
        if(active_token_kind != TXT_TokenKind_Null)
        {
          active_token_start_idx = idx;
        }
        
        // rjf: invalid token kind -> emit error
        else
        {
          TXT_Token token = {TXT_TokenKind_Error, r1u64(idx, idx+1)};
          txt_token_chunk_list_push(scratch.arena, &tokens, 4096, &token);
        }
      }
      
      // rjf: look for ender
      U64 ender_pad = 0;
      B32 ender_found = 0;
      if(active_token_kind != TXT_TokenKind_Null && idx>active_token_start_idx)
      {
        if(idx == string.size)
        {
          ender_pad = 0;
          ender_found = 1;
        }
        else switch(active_token_kind)
        {
          default:break;
          case TXT_TokenKind_Whitespace:
          {
            ender_found = !char_is_space(byte);
          }break;
          case TXT_TokenKind_Identifier:
          {
            ender_found = (!char_is_alpha(byte) && !char_is_digit(byte, 10) && byte != '_' && byte != '$' && byte < 128);
          }break;
          case TXT_TokenKind_Numeric:
          {
            ender_found = (!char_is_alpha(byte) && !char_is_digit(byte, 10) && byte != '_' && byte != '.' && byte != '\'');
          }break;
          case TXT_TokenKind_String:
          {
            if (string_is_line)
            {
              ender_found = (!escaped && (byte == '\r' || byte == '\n'));
            }
            else
            {
              ender_found = (!escaped && ((!string_is_char && byte == '"') || (string_is_char && byte == '\'')));
              ender_pad += 1;
            }
          }break;
          case TXT_TokenKind_Symbol:
          {
            ender_found = (byte != '~' && byte != '!' &&
                           byte != '%' && byte != '^' &&
                           byte != '&' && byte != '*' &&
                           byte != '(' && byte != ')' &&
                           byte != '-' && byte != '=' &&
                           byte != '+' && byte != '[' &&
                           byte != ']' && byte != '{' &&
                           byte != '}' && byte != ':' &&
                           byte != ';' && byte != ',' &&
                           byte != '.' && byte != '<' &&
                           byte != '>' && byte != '/' &&
                           byte != '?' && byte != '|' &&
                           byte != 'c');
          }break;
          case TXT_TokenKind_LineComment:
          {
            ender_found = (!escaped && (byte == '\r' || byte == '\n'));
          }break;
        }
      }
      
      // rjf: next byte is ender => emit token
      if(ender_found)
      {
        TXT_Token token = {active_token_kind, r1u64(active_token_start_idx, idx+ender_pad)};
        active_token_kind = TXT_TokenKind_Null;
        
        // rjf: identifier -> keyword in special cases
        if(token.kind == TXT_TokenKind_Identifier)
        {
          read_only local_persist String8 zig_keywords[] =
          {
            str8_lit_comp("addrspace"),
            str8_lit_comp("align"),
            str8_lit_comp("allowzero"),
            str8_lit_comp("and"),
            str8_lit_comp("anyframe"),
            str8_lit_comp("anytype"),
            str8_lit_comp("asm"),
            str8_lit_comp("async"),
            str8_lit_comp("await"),
            str8_lit_comp("break"),
            str8_lit_comp("callconv"),
            str8_lit_comp("catch"),
            str8_lit_comp("comptime"),
            str8_lit_comp("const"),
            str8_lit_comp("continue"),
            str8_lit_comp("defer"),
            str8_lit_comp("else"),
            str8_lit_comp("enum"),
            str8_lit_comp("errdefer"),
            str8_lit_comp("error"),
            str8_lit_comp("export"),
            str8_lit_comp("extern"),
            str8_lit_comp("fn"),
            str8_lit_comp("for"),
            str8_lit_comp("if"),
            str8_lit_comp("inline"),
            str8_lit_comp("noalias"),
            str8_lit_comp("nosuspend"),
            str8_lit_comp("noinline"),
            str8_lit_comp("opaque"),
            str8_lit_comp("or"),
            str8_lit_comp("orelse"),
            str8_lit_comp("packed"),
            str8_lit_comp("pub"),
            str8_lit_comp("resume"),
            str8_lit_comp("return"),
            str8_lit_comp("linksection"),
            str8_lit_comp("struct"),
            str8_lit_comp("suspend"),
            str8_lit_comp("switch"),
            str8_lit_comp("test"),
            str8_lit_comp("threadlocal"),
            str8_lit_comp("try"),
            str8_lit_comp("union"),
            str8_lit_comp("unreachable"),
            str8_lit_comp("usingnamespace"),
            str8_lit_comp("var"),
            str8_lit_comp("volatile"),
            str8_lit_comp("while"),
          };
          String8 token_string = str8_substr(string, r1u64(active_token_start_idx, idx+ender_pad));
          for(U64 keyword_idx = 0; keyword_idx < ArrayCount(zig_keywords); keyword_idx += 1)
          {
            if(str8_match(zig_keywords[keyword_idx], token_string, 0))
            {
              token.kind = TXT_TokenKind_Keyword;
              break;
            }
          }
          txt_token_chunk_list_push(scratch.arena, &tokens, 4096, &token);
        }
        
        // rjf: split symbols by maximum-munch-rule
        else if(token.kind == TXT_TokenKind_Symbol)
        {
          read_only local_persist String8 zig_multichar_symbol_strings[] =
          {
            str8_lit_comp("<<"),
            str8_lit_comp(">>"),
            str8_lit_comp("<="),
            str8_lit_comp(">="),
            str8_lit_comp("=="),
            str8_lit_comp("!="),
            str8_lit_comp("&&"),
            str8_lit_comp("||"),
            str8_lit_comp("|="),
            str8_lit_comp("&="),
            str8_lit_comp("^="),
            str8_lit_comp("~="),
            str8_lit_comp("+="),
            str8_lit_comp("-="),
            str8_lit_comp("*="),
            str8_lit_comp("/="),
            str8_lit_comp("%="),
            str8_lit_comp("<<="),
            str8_lit_comp(">>="),
            str8_lit_comp("->"),
          };
          String8 token_string = str8_substr(string, r1u64(active_token_start_idx, idx+ender_pad));
          for(U64 off = 0, next_off = token_string.size; off < token_string.size; off = next_off)
          {
            B32 found = 0;
            for(U64 idx = 0; idx < ArrayCount(zig_multichar_symbol_strings); idx += 1)
            {
              if(str8_match(str8_substr(token_string, r1u64(off, off+zig_multichar_symbol_strings[idx].size)),
                            zig_multichar_symbol_strings[idx],
                            0))
              {
                found = 1;
                next_off = off + zig_multichar_symbol_strings[idx].size;
                TXT_Token token = {TXT_TokenKind_Symbol, r1u64(active_token_start_idx+off, active_token_start_idx+next_off)};
                txt_token_chunk_list_push(scratch.arena, &tokens, 4096, &token);
                break;
              }
            }
            if(!found)
            {
              next_off = off+1;
              TXT_Token token = {TXT_TokenKind_Symbol, r1u64(active_token_start_idx+off, active_token_start_idx+next_off)};
              txt_token_chunk_list_push(scratch.arena, &tokens, 4096, &token);
            }
          }
        }
        
        // rjf: all other tokens
        else
        {
          txt_token_chunk_list_push(scratch.arena, &tokens, 4096, &token);
        }
        
        // rjf: increment by ender padding
        idx += ender_pad;
      }
      
      // rjf: advance by 1 byte if we haven't found an ender
      if(!ender_found)
      {
        idx += 1;
      }
      escaped = next_escaped;
    }
  }
  
  //- rjf: token list -> token array
  TXT_TokenArray result = txt_token_array_from_chunk_list(arena, &tokens);
  scratch_end(scratch);
  return result;
}

internal TXT_TokenArray
txt_token_array_from_string__rust(Arena *arena, U64 *bytes_processed_counter, String8 string)
{
  // NOTE(spey): Rust supports unicode identifiers. They are not handled in any way here,
  // but it might be worth looking into in the future.
#if 0
  Temp scratch = scratch_begin(&arena, 1);
  
  //- rjf: generate token list
  TXT_TokenChunkList tokens = {0};
  {
    S32 multiline_comment_nesting_level = 0;
    S32 raw_string_nesting_level = 0;
    S32 raw_string_ender_nesting_level = 0;
    
    // NOTE(spey): Rust's syntax is designed in such a way that we can't be sure what a token
    // is immediately from the first character, so we have to keep track of some possibilities.
    B32 token_may_be_char = 0;
    B32 token_may_be_lifetime = 0;
    B32 token_may_be_string = 0;
    
    TXT_TokenKind active_token_kind = TXT_TokenKind_Null;
    U64 active_token_start_idx = 0;
    B32 escaped = 0;
    B32 next_escaped = 0;
    U64 byte_process_start_idx = 0;
    for(U64 idx = 0; idx <= string.size;)
    {
      U8 byte      = (idx+0 < string.size) ? (string.str[idx+0]) : 0;
      U8 next_byte = (idx+1 < string.size) ? (string.str[idx+1]) : 0;
      
      // rjf: update counter
      if(bytes_processed_counter != 0 && ((idx-byte_process_start_idx) >= 1000 || idx == string.size))
      {
        ins_atomic_u64_add_eval(bytes_processed_counter, (idx-byte_process_start_idx));
        byte_process_start_idx = idx;
      }
      
      // rjf: escaping
      if(escaped && (byte != '\r' && byte != '\n'))
      {
        next_escaped = 0;
      }
      else if(!escaped && byte == '\\')
      {
        next_escaped = 1;
      }
      
      // rjf: take starter, determine active token kind
      U64 starter_pad = 0;
      
      // spey: special case of starter for nested comments
      if(active_token_kind == TXT_TokenKind_Comment)
      {
        if(byte == '/' && next_byte == '*')      { active_token_kind = TXT_TokenKind_Comment; multiline_comment_nesting_level++; starter_pad = 1; }
      }
      // spey: special case of starter for raw string literals
      else if(active_token_kind == TXT_TokenKind_Identifier && token_may_be_string)
      {
        if(0){}
        else if(byte == 'r' && next_byte == '#') {} // spey: still an identifier that may be a string (this branch triggers for raw byte/C string literals)
        else if(byte == '#' && next_byte == '"') { active_token_kind = TXT_TokenKind_String; token_may_be_string = 0; token_may_be_char = 0; raw_string_nesting_level++; starter_pad = 2; }
        else if(byte == '#' && next_byte == '#') { raw_string_nesting_level++; }
        else                                     { token_may_be_string = 0; token_may_be_char = 0; raw_string_nesting_level = 0; } // spey: confirmed raw identifier
      }
      // spey: regular cases
      else if(active_token_kind == TXT_TokenKind_Null)
      {
        // rjf: use next bytes to start a new token
        if(0){}
        else if(char_is_space(byte))             { active_token_kind = TXT_TokenKind_Whitespace; }
        else if(byte == 'r' && next_byte == '#') { active_token_kind = TXT_TokenKind_Identifier; token_may_be_string = 1; } // spey: either raw identifiers or raw string literals
        else if(char_is_digit(byte, 10) ||
                (byte == '.' &&
                 char_is_digit(next_byte, 10)))  { active_token_kind = TXT_TokenKind_Numeric; }
        else if(byte == '"')                     { active_token_kind = TXT_TokenKind_String; token_may_be_char = 0; }
        else if((byte == 'c' || byte == 'b') &&
                next_byte == '"')               { active_token_kind = TXT_TokenKind_String; token_may_be_char = 0; starter_pad = 1; }
        else if((byte == 'c' || byte == 'b') &&
                next_byte == 'r')               { active_token_kind = TXT_TokenKind_Identifier; token_may_be_string = 1; }
        else if(byte == '_' ||
                char_is_alpha(byte))             { active_token_kind = TXT_TokenKind_Identifier; }
        else if(byte == '/' && next_byte == '/') { active_token_kind = TXT_TokenKind_Comment; starter_pad = 1; }
        else if(byte == '/' && next_byte == '*') { active_token_kind = TXT_TokenKind_Comment; starter_pad = 1; multiline_comment_nesting_level++; }
        else if(byte == '~' || byte == '!' ||
                byte == '%' || byte == '^' ||
                byte == '&' || byte == '*' ||
                byte == '(' || byte == ')' ||
                byte == '-' || byte == '=' ||
                byte == '+' || byte == '[' ||
                byte == ']' || byte == '{' ||
                byte == '}' || byte == ':' ||
                byte == ';' || byte == ',' ||
                byte == '.' || byte == '<' ||
                byte == '>' || byte == '/' ||
                byte == '?' || byte == '|')      { active_token_kind = TXT_TokenKind_Symbol; }
        else if(byte == '\'')                    { active_token_kind = TXT_TokenKind_String; token_may_be_char = 1; token_may_be_lifetime = 1; }
        else if((byte == 'c' || byte == 'b') &&
                next_byte == '\'')              { active_token_kind = TXT_TokenKind_String; token_may_be_char = 1; starter_pad = 1; }
        
        // rjf: start new token
        if(active_token_kind != TXT_TokenKind_Null)
        {
          active_token_start_idx = idx;
        }
        
        // rjf: invalid token kind -> emit error
        else
        {
          TXT_Token token = {TXT_TokenKind_Error, r1u64(idx, idx+1)};
          txt_token_chunk_list_push(scratch.arena, &tokens, 4096, &token);
        }
      }
      
      B32 is_on_starter = idx <= active_token_start_idx || token_may_be_string;
      
      // spey: advance by starter padding byte(s) and reset byte/next_byte values
      idx += starter_pad;
      byte      = (idx+0 < string.size) ? (string.str[idx+0]) : 0;
      next_byte = (idx+1 < string.size) ? (string.str[idx+1]) : 0;
      
      // rjf: look for ender
      U64 ender_pad = 0;
      B32 ender_found = 0;
      if(active_token_kind != TXT_TokenKind_Null && !is_on_starter)
      {
        if(idx == string.size)
        {
          ender_pad = 0;
          ender_found = 1;
        }
        else switch(active_token_kind)
        {
          default:break;
          case TXT_TokenKind_Whitespace:
          {
            ender_found = !char_is_space(byte);
          }break;
          case TXT_TokenKind_Identifier:
          {
            ender_found = (!char_is_alpha(byte) && !char_is_digit(byte, 10) && byte != '_' && byte != '$' && byte != '#' && byte != '!' && byte < 128);
          }break;
          case TXT_TokenKind_Numeric:
          {
            ender_found = (!char_is_alpha(byte) && !char_is_digit(byte, 10) && byte != '_' && byte != '.' && byte != '\'');
          }break;
          case TXT_TokenKind_String:
          {
            if(!escaped)
            {
              if(token_may_be_char)
              {
                if(byte == '\'')
                {
                  // spey: char ending
                  ender_found = 1;
                }
                else if(token_may_be_lifetime && !char_is_alpha(byte) && !char_is_digit(byte, 10) && byte != '_' && byte < 128)
                {
                  // spey: lifetime ending
                  ender_found = 1;
                }
              }
              else
              {
                if(0){}
                
                // spey: regular string
                else if(raw_string_nesting_level == 0)       { ender_found = byte == '"'; }
                
                // spey: raw string
                else if(byte == '"' && next_byte == '#' &&
                        raw_string_ender_nesting_level == 0) { raw_string_ender_nesting_level++; }
                else if(byte == '#' && next_byte != '#' &&
                        raw_string_ender_nesting_level == raw_string_nesting_level &&
                        raw_string_ender_nesting_level >= 0) { ender_found = 1; raw_string_nesting_level = 0; raw_string_ender_nesting_level = 0; }
                else if(byte == '#' && next_byte != '#' &&
                        raw_string_ender_nesting_level >= 0) { raw_string_ender_nesting_level = 0; }
                else if(byte == '#' &&
                        raw_string_ender_nesting_level >= 0) { raw_string_ender_nesting_level++; }
              }
            }
            
            ender_pad += 1;
          }break;
          case TXT_TokenKind_Symbol:
          {
            ender_found = (byte != '~' && byte != '!' &&
                           byte != '%' && byte != '^' &&
                           byte != '&' && byte != '*' &&
                           byte != '(' && byte != ')' &&
                           byte != '-' && byte != '=' &&
                           byte != '+' && byte != '[' &&
                           byte != ']' && byte != '{' &&
                           byte != '}' && byte != ':' &&
                           byte != ';' && byte != ',' &&
                           byte != '.' && byte != '<' &&
                           byte != '>' && byte != '/' &&
                           byte != '?' && byte != '|');
          }break;
          case TXT_TokenKind_Comment:
          {
            if(multiline_comment_nesting_level == 0)
            {
              ender_found = (byte == '\r' || byte == '\n');
            }
            else
            {
              if (byte == '*' && next_byte == '/')
                multiline_comment_nesting_level--;
              
              ender_found = (active_token_start_idx+1 < idx && multiline_comment_nesting_level == 0);
              ender_pad += 2;
            }
          }break;
        }
      }
      
      // rjf: next byte is ender => emit token
      if(ender_found)
      {
        TXT_Token token = {active_token_kind, r1u64(active_token_start_idx, idx+ender_pad)};
        active_token_kind = TXT_TokenKind_Null;
        
        // rjf: identifier -> keyword in special cases
        if(token.kind == TXT_TokenKind_Identifier)
        {
          read_only local_persist String8 rust_keywords[] =
          {
            str8_lit_comp("as"),
            str8_lit_comp("break"),
            str8_lit_comp("const"),
            str8_lit_comp("continue"),
            str8_lit_comp("crate"),
            str8_lit_comp("else"),
            str8_lit_comp("enum"),
            str8_lit_comp("extern"),
            str8_lit_comp("false"),
            str8_lit_comp("fn"),
            str8_lit_comp("for"),
            str8_lit_comp("if"),
            str8_lit_comp("impl"),
            str8_lit_comp("in"),
            str8_lit_comp("let"),
            str8_lit_comp("loop"),
            str8_lit_comp("match"),
            str8_lit_comp("mod"),
            str8_lit_comp("move"),
            str8_lit_comp("mut"),
            str8_lit_comp("pub"),
            str8_lit_comp("ref"),
            str8_lit_comp("return"),
            str8_lit_comp("self"),
            str8_lit_comp("Self"),
            str8_lit_comp("static"),
            str8_lit_comp("struct"),
            str8_lit_comp("super"),
            str8_lit_comp("trait"),
            str8_lit_comp("true"),
            str8_lit_comp("type"),
            str8_lit_comp("unsafe"),
            str8_lit_comp("use"),
            str8_lit_comp("where"),
            str8_lit_comp("while"),
            str8_lit_comp("yield"),
            str8_lit_comp("async"),
            str8_lit_comp("await"),
            str8_lit_comp("dyn"),
            
            // weak keywords
            str8_lit_comp("macro_rules"),
            str8_lit_comp("raw"),
            str8_lit_comp("safe"),
            str8_lit_comp("union"),
          };
          String8 token_string = str8_substr(string, r1u64(active_token_start_idx, idx+ender_pad));
          for(U64 keyword_idx = 0; keyword_idx < ArrayCount(rust_keywords); keyword_idx += 1)
          {
            if(str8_match(rust_keywords[keyword_idx], token_string, 0))
            {
              token.kind = TXT_TokenKind_Keyword;
              break;
            }
          }
          txt_token_chunk_list_push(scratch.arena, &tokens, 4096, &token);
        }
        
        // rjf: split symbols by maximum-munch-rule
        else if(token.kind == TXT_TokenKind_Symbol)
        {
          read_only local_persist String8 rust_multichar_symbol_strings[] =
          {
            str8_lit_comp("<<"),
            str8_lit_comp(">>"),
            str8_lit_comp("<="),
            str8_lit_comp(">="),
            str8_lit_comp("=="),
            str8_lit_comp("!="),
            str8_lit_comp("&&"),
            str8_lit_comp("||"),
            str8_lit_comp("|="),
            str8_lit_comp("&="),
            str8_lit_comp("^="),
            str8_lit_comp("~="),
            str8_lit_comp("+="),
            str8_lit_comp("-="),
            str8_lit_comp("*="),
            str8_lit_comp("/="),
            str8_lit_comp("%="),
            str8_lit_comp("<<="),
            str8_lit_comp(">>="),
            str8_lit_comp("->"),
          };
          String8 token_string = str8_substr(string, r1u64(active_token_start_idx, idx+ender_pad));
          for(U64 off = 0, next_off = token_string.size; off < token_string.size; off = next_off)
          {
            B32 found = 0;
            for(U64 idx = 0; idx < ArrayCount(rust_multichar_symbol_strings); idx += 1)
            {
              if(str8_match(str8_substr(token_string, r1u64(off, off+rust_multichar_symbol_strings[idx].size)),
                            rust_multichar_symbol_strings[idx],
                            0))
              {
                found = 1;
                next_off = off + rust_multichar_symbol_strings[idx].size;
                TXT_Token token = {TXT_TokenKind_Symbol, r1u64(active_token_start_idx+off, active_token_start_idx+next_off)};
                txt_token_chunk_list_push(scratch.arena, &tokens, 4096, &token);
                break;
              }
            }
            if(!found)
            {
              next_off = off+1;
              TXT_Token token = {TXT_TokenKind_Symbol, r1u64(active_token_start_idx+off, active_token_start_idx+next_off)};
              txt_token_chunk_list_push(scratch.arena, &tokens, 4096, &token);
            }
          }
        }
        
        // rjf: all other tokens
        else
        {
          txt_token_chunk_list_push(scratch.arena, &tokens, 4096, &token);
        }
        
        // rjf: increment by starter and ender padding
        idx += ender_pad;
      }
      
      // rjf: advance by 1 byte if we haven't found an ender
      if(!ender_found)
      {
        idx += 1;
      }
      escaped = next_escaped;
    }
  }
  
  //- rjf: token list -> token array
  TXT_TokenArray result = txt_token_array_from_chunk_list(arena, &tokens);
  scratch_end(scratch);
  return result;
#endif
  TXT_TokenArray result = {0};
  return result;
}

internal TXT_TokenArray
txt_token_array_from_string__disasm_x64_intel(Arena *arena, U64 *bytes_processed_counter, String8 string)
{
  Temp scratch = scratch_begin(&arena, 1);
  
  //- rjf: parse tokens
  TXT_TokenChunkList tokens = {0};
  {
    TXT_TokenKind active_token_kind = TXT_TokenKind_Null;
    U64 active_token_start_off = 0;
    U64 off = 0;
    B32 escaped = 0;
    B32 string_is_char = 0;
    S32 brace_nest = 0;
    S32 paren_nest = 0;
    S32 string_tick_nest = 0;
    for(U64 advance = 0; off <= string.size; off += advance)
    {
      U8 byte      = (off+0 < string.size) ? string.str[off+0] : 0;
      U8 next_byte = (off+1 < string.size) ? string.str[off+1] : 0;
      B32 ender_found = 0;
      advance = (active_token_kind != TXT_TokenKind_Null ? 1 : 0);
      if(off == string.size && active_token_kind != TXT_TokenKind_Null)
      {
        ender_found = 1;
        advance = 1;
      }
      switch(active_token_kind)
      {
        default:
        case TXT_TokenKind_Null:
        {
          if(byte == ' ' || byte == '\t' || byte == '\v' || byte == '\f' || byte == '\r' || byte == '\n')
          {
            active_token_start_off = off;
            active_token_kind = TXT_TokenKind_Whitespace;
            advance = 1;
          }
          else if(byte == '>' && brace_nest == 0 && paren_nest == 0)
          {
            active_token_start_off = off;
            active_token_kind = TXT_TokenKind_LineComment;
            advance = 1;
          }
          else if(('a' <= byte && byte <= 'z') || ('A' <= byte && byte <= 'Z') || byte == '_')
          {
            active_token_start_off = off;
            active_token_kind = TXT_TokenKind_Keyword;
            advance = 1;
          }
          else if(byte == '\'')
          {
            active_token_start_off = off;
            active_token_kind = TXT_TokenKind_String;
            advance = 1;
            string_is_char = 1;
          }
          else if(byte == '"')
          {
            active_token_start_off = off;
            active_token_kind = TXT_TokenKind_String;
            advance = 1;
            string_is_char = 0;
          }
          else if(byte == '`')
          {
            active_token_start_off = off;
            active_token_kind = TXT_TokenKind_String;
            advance = 1;
            string_tick_nest += 1;
          }
          else if(('0' <= byte && byte <= '9') || (byte == '.' && '0' <= next_byte && next_byte <= '9'))
          {
            active_token_start_off = off;
            active_token_kind = TXT_TokenKind_Numeric;
            advance = 1;
          }
          else if(byte == '~' || byte == '!' || byte == '%' || byte == '^' ||
                  byte == '&' || byte == '*' || byte == '(' || byte == ')' ||
                  byte == '-' || byte == '=' || byte == '+' || byte == '[' ||
                  byte == ']' || byte == '{' || byte == '}' || byte == ';' ||
                  byte == ':' || byte == '?' || byte == '/' || byte == '<' ||
                  byte == '>' || byte == ',' || byte == '.')
          {
            active_token_start_off = off;
            active_token_kind = TXT_TokenKind_Symbol;
            advance = 1;
            if(byte == '{')
            {
              brace_nest += 1;
            }
            else if(byte == '}')
            {
              brace_nest -= 1;
            }
            if(byte == '(')
            {
              paren_nest += 1;
            }
            else if(byte == ')')
            {
              paren_nest -= 1;
            }
          }
          else
          {
            active_token_start_off = off;
            active_token_kind = TXT_TokenKind_Error;
            advance = 1;
          }
        }break;
        case TXT_TokenKind_Whitespace:
        if(byte != ' ' && byte != '\t' && byte != '\v' && byte != '\f')
        {
          ender_found = 1;
          advance = 0;
        }break;
        case TXT_TokenKind_Keyword:
        if((byte < 'a' || 'z' < byte) && (byte < 'A' || 'Z' < byte) && (byte < '0' || '9' < byte) && byte != '_')
        {
          ender_found = 1;
          advance = 0;
        }break;
        case TXT_TokenKind_String:
        {
          U8 ender_byte = (string_tick_nest > 0 ? '\'' :
                           string_is_char ? '\''
                           : '"');
          if(!escaped && byte == ender_byte)
          {
            if(string_tick_nest > 0)
            {
              string_tick_nest -= 1;
            }
            if(string_tick_nest == 0)
            {
              ender_found = 1;
              advance = 1;
            }
          }
          else if(escaped)
          {
            escaped = 0;
            advance = 1;
          }
          else if(byte == '\\')
          {
            escaped = 1;
            advance = 1;
          }
          else if(string_tick_nest > 0 && byte == '`')
          {
            string_tick_nest += 1;
          }
          else
          {
            U8 byte_class = utf8_class[byte>>3];
            if(byte_class > 1)
            {
              advance = (U64)byte_class;
            }
          }
        }break;
        case TXT_TokenKind_Numeric:
        if((byte < 'a' || 'z' < byte) && (byte < 'A' || 'Z' < byte) && (byte < '0' || '9' < byte) && byte != '.')
        {
          ender_found = 1;
          advance = 0;
        }break;
        case TXT_TokenKind_Symbol:
        if(1)
        {
          // NOTE(rjf): avoiding maximum munch rule for now
          ender_found = 1;
          advance = 0;
        }
        else if(byte != '~' && byte != '!' && byte != '#' && byte != '%' &&
                byte != '^' && byte != '&' && byte != '*' && byte != '(' &&
                byte != ')' && byte != '-' && byte != '=' && byte != '+' &&
                byte != '[' && byte != ']' && byte != '{' && byte != '}' &&
                byte != ';' && byte != ':' && byte != '?' && byte != '/' &&
                byte != '<' && byte != '>' && byte != ',' && byte != '.')
        {
          ender_found = 1;
          advance = 0;
        }break;
        case TXT_TokenKind_Error:
        {
          ender_found = 1;
          advance = 0;
        }break;
        case TXT_TokenKind_LineComment:
        if(byte == '\n')
        {
          ender_found = 1;
          advance = 1;
        }break;
      }
      if(ender_found != 0)
      {
        if(brace_nest != 0 && active_token_kind == TXT_TokenKind_Keyword)
        {
          active_token_kind = TXT_TokenKind_Numeric;
        }
        if(paren_nest != 0 && active_token_kind == TXT_TokenKind_Keyword)
        {
          active_token_kind = TXT_TokenKind_Identifier;
        }
        TXT_Token token = {active_token_kind, r1u64(active_token_start_off, off+advance)};
        txt_token_chunk_list_push(arena, &tokens, 1024, &token);
        active_token_kind = TXT_TokenKind_Null;
        active_token_start_off = token.range.max;
      }
    }
  }
  
  //- rjf: token list -> token array
  TXT_TokenArray result = txt_token_array_from_chunk_list(arena, &tokens);
  scratch_end(scratch);
  return result;
}

////////////////////////////////
//~ rjf: Text Info Extractor Helpers

internal void
txt_line_map_push(Arena *arena, TXT_LineMap *map, Rng1U64 num_range, Rng1U64 *ranges, S64 delta)
{
  TXT_LineMapRangeNode *n = push_array(arena, TXT_LineMapRangeNode, 1);
  n->num_range = num_range;
  n->ranges = ranges;
  n->delta = delta;
  SLLQueuePush(map->first_range, map->last_range, n);
  map->total_line_count += dim_1u64(num_range);
}

internal U64
txt_line_num_from_off(TXT_LineMap *map, U64 off)
{
  U64 result = 0;
  for(TXT_LineMapRangeNode *n = map->first_range; n != 0; n = n->next)
  {
    if(n->num_range.max != n->num_range.min)
    {
      Rng1U64 off_range = r1u64(n->ranges[0].min + n->delta, n->ranges[n->num_range.max-n->num_range.min-1].max + n->delta);
      if(off_range.min <= off && off <= off_range.max)
      {
        U64 min_idx = 0;
        U64 max_idx = dim_1u64(n->num_range)-1;
        for(;min_idx <= max_idx;)
        {
          U64 mid_idx = (max_idx + min_idx) / 2;
          if(n->ranges[mid_idx].max + n->delta < off)
          {
            min_idx = mid_idx + 1;
          }
          else if(off < n->ranges[mid_idx].min + n->delta)
          {
            max_idx = mid_idx - 1;
          }
          else if(n->ranges[mid_idx].min + n->delta <= off && off <= n->ranges[mid_idx].max + n->delta)
          {
            result = n->num_range.min + mid_idx;
            goto break_all;
          }
        }
      }
    }
  }
  break_all:;
  return result;
}

internal Rng1U64
txt_range_from_line_num(TXT_LineMap *map, U64 num)
{
  Rng1U64 result = {0};
  for(TXT_LineMapRangeNode *n = map->first_range; n != 0; n = n->next)
  {
    if(contains_1u64(n->num_range, num))
    {
      result = n->ranges[num - n->num_range.min];
      result.min = (U64)((S64)result.min + n->delta);
      result.max = (U64)((S64)result.max + n->delta);
    }
  }
  return result;
}

internal void
txt_token_pt_map_push(Arena *arena, TXT_TokenPtMap *map, Rng1U64 num_range, TXT_TokenPt *pts, S64 delta)
{
  TXT_TokenPtMapRangeNode *n = push_array(arena, TXT_TokenPtMapRangeNode, 1);
  SLLQueuePush(map->first_range, map->last_range, n);
  n->num_range = num_range;
  n->pts = pts;
  n->delta = delta;
  map->total_pt_count += dim_1u64(num_range);
}

internal U64
txt_token_pt_num_from_off(TXT_TokenPtMap *map, U64 off)
{
  // NOTE(rad): the number of the last pt at or before `off` (0 = none). after
  // edits the map has several ranges (before an edit / re-lexed / after it),
  // not kept in offset order; an `off` between two ranges belongs to the last
  // pt of the earlier one. (this used to find a pt only inside one range and
  // returned 0 = "no token" between ranges, so the text after an edit inside
  // a multi-line comment or string lost its context.) pts at equal offsets:
  // the last one (an ender and the next starter at the same line start).
  U64 result = 0;
  for EachNode(n, TXT_TokenPtMapRangeNode, map->first_range)
  {
    U64 num_pts = dim_1u64(n->num_range);
    if(num_pts != 0 && n->pts[0].off + n->delta <= off)
    {
      U64 lo = 1;
      U64 hi = num_pts;
      for(;lo < hi;)
      {
        U64 mid = (lo + hi) / 2;
        if(n->pts[mid].off + n->delta <= off)
        {
          lo = mid + 1;
        }
        else
        {
          hi = mid;
        }
      }
      result = Max(result, n->num_range.min + lo - 1);
    }
  }
  return result;
}

internal TXT_TokenPt
txt_token_pt_from_num(TXT_TokenPtMap *map, U64 num)
{
  TXT_TokenPt pt = {TXT_TokenKind_Null};
  if(num > 0)
  {
    for EachNode(n, TXT_TokenPtMapRangeNode, map->first_range)
    {
      if(contains_1u64(n->num_range, num))
      {
        pt = n->pts[num - n->num_range.min];
        pt.off += n->delta;
        break;
      }
    }
  }
  return pt;
}

////////////////////////////////
//~ rad: Rust Lexing

internal B32
txt_rust_byte_is_ident(U8 byte)
{
  // NOTE(rad): bytes >= 0x80 are parts of UTF-8 encoded (unicode) identifiers
  return (('a' <= byte && byte <= 'z') ||
          ('A' <= byte && byte <= 'Z') ||
          ('0' <= byte && byte <= '9') ||
          byte == '_' || byte >= 0x80);
}

internal B32
txt_rust_quote_is_char(String8 data, U64 off)
{
  // NOTE(rad): the `'` at `off` starts a char literal ('a', '\n', '\u{1F600}',
  // 'é'), else a lifetime or a loop label ('a, 'static, 'outer:)
  B32 result = 0;
  if(off+1 < data.size)
  {
    U8 c1 = data.str[off+1];
    if(c1 == '\\')
    {
      result = 1;
    }
    else if(c1 != '\'' && c1 != '\n' && c1 != '\r')
    {
      UnicodeDecode decode = utf8_decode(data.str+off+1, data.size-(off+1));
      U64 close_off = off+1+Max(decode.inc, 1);
      result = (close_off < data.size && data.str[close_off] == '\'');
    }
  }
  return result;
}

internal TXT_TokenPt
txt_rust_big_token_from_off(String8 data, U64 off, U64 *starter_size_out)
{
  // NOTE(rad): with no token active, does a big token start at `off`? kind
  // Null if not. aux: String = raw-string hash count + 1 (0 = normal string,
  // with escapes); BlockComment = nesting depth; Meta (attribute) = bracket
  // depth. *starter_size_out = size of the opener ("//", "r##\"", ...).
  TXT_TokenPt result = {TXT_TokenKind_Null, off, 0};
  U64 starter_size = 0;
  U8 b0 = (off+0 < data.size) ? data.str[off+0] : 0;
  U8 b1 = (off+1 < data.size) ? data.str[off+1] : 0;
  U8 b2 = (off+2 < data.size) ? data.str[off+2] : 0;
  switch(b0)
  {
    default:{}break;
    case '/':
    {
      if(b1 == '/')      { result.kind = TXT_TokenKind_LineComment; starter_size = 2; }
      else if(b1 == '*') { result.kind = TXT_TokenKind_BlockComment; result.aux = 1; starter_size = 2; }
    }break;
    case '"':
    {
      result.kind = TXT_TokenKind_String;
      starter_size = 1;
    }break;
    case '\'':
    {
      if(txt_rust_quote_is_char(data, off))
      {
        result.kind = TXT_TokenKind_Char;
        starter_size = 1;
      }
    }break;
    case '#':
    {
      if(b1 == '[')                  { result.kind = TXT_TokenKind_Meta; result.aux = 1; starter_size = 2; }
      else if(b1 == '!' && b2 == '[') { result.kind = TXT_TokenKind_Meta; result.aux = 1; starter_size = 3; }
    }break;

    // rad: prefixed literals: b"..", c"..", b'x', r"..", r#".."#, br#".."#, cr".."
    case 'b':
    case 'c':
    case 'r':
    if(off == 0 || !txt_rust_byte_is_ident(data.str[off-1]))
    {
      U64 r_off = off;
      if(b0 != 'r')
      {
        if(b1 == '"')
        {
          result.kind = TXT_TokenKind_String;
          starter_size = 2;
          break;
        }
        if(b0 == 'b' && b1 == '\'' && txt_rust_quote_is_char(data, off+1))
        {
          result.kind = TXT_TokenKind_Char;
          starter_size = 2;
          break;
        }
        if(b1 != 'r')
        {
          break;
        }
        r_off = off+1;
      }
      U64 hashes = 0;
      for(;r_off+1+hashes < data.size && data.str[r_off+1+hashes] == '#'; hashes += 1);
      if(r_off+1+hashes < data.size && data.str[r_off+1+hashes] == '"')
      {
        result.kind = TXT_TokenKind_String;
        result.aux = hashes+1;
        starter_size = (r_off+1+hashes+1) - off;
      }
    }break;
  }
  if(starter_size_out != 0)
  {
    *starter_size_out = starter_size;
  }
  return result;
}

internal U64
txt_rust_big_token_advance(String8 data, U64 off, TXT_TokenPt *pt, B32 *ended_out)
{
  // NOTE(rad): one step inside the active big token `pt`, at `off` (<
  // data.size): returns the bytes consumed (>= 1), updates pt->aux (nesting),
  // *ended_out = the token ends after those bytes. A token resumed in the
  // middle starts from the aux of its start (nesting 1).
  U64 advance = 1;
  B32 ended = 0;
  U8 byte      = data.str[off];
  U8 next_byte = (off+1 < data.size) ? data.str[off+1] : 0;
  switch(pt->kind)
  {
    default:
    {
      ended = 1;
    }break;
    case TXT_TokenKind_LineComment:
    {
      ended = (byte == '\n');
    }break;
    case TXT_TokenKind_BlockComment:
    {
      if(byte == '/' && next_byte == '*')
      {
        pt->aux += 1;
        advance = 2;
      }
      else if(byte == '*' && next_byte == '/')
      {
        pt->aux = (pt->aux > 1) ? pt->aux-1 : 0;
        ended = (pt->aux == 0);
        advance = 2;
      }
    }break;
    case TXT_TokenKind_String:
    {
      if(pt->aux == 0)
      {
        if(byte == '\\')     { advance = 2; }
        else if(byte == '"') { ended = 1; }
      }
      else if(byte == '"')
      {
        U64 hashes = pt->aux-1;
        B32 closes = (off+1+hashes <= data.size);
        for(U64 idx = 0; closes && idx < hashes; idx += 1)
        {
          closes = (data.str[off+1+idx] == '#');
        }
        if(closes)
        {
          ended = 1;
          advance = 1+hashes;
        }
      }
    }break;
    case TXT_TokenKind_Char:
    {
      if(byte == '\\')                       { advance = 2; }
      else if(byte == '\'' || byte == '\n') { ended = 1; }
    }break;
    case TXT_TokenKind_Meta:
    {
      if(byte == '[')
      {
        pt->aux += 1;
      }
      else if(byte == ']')
      {
        pt->aux = (pt->aux > 1) ? pt->aux-1 : 0;
        ended = (pt->aux == 0);
      }
    }break;
  }
  advance = Min(advance, data.size-off);
  if(ended_out != 0)
  {
    *ended_out = ended;
  }
  return advance;
}

internal TXT_TokenArray
txt_token_array_from_data__rust(Arena *arena, TXT_TokenPt ctx_token_pt, String8 data, U64 base_off)
{
  // NOTE(rad): Rust version of txt_token_array_from_data (same token kinds
  // and conventions). Lifetimes and labels are emitted as keywords; whole
  // attributes as meta; raw identifiers (r#type) never as keywords.
  Temp scratch = scratch_begin(&arena, 1);
  TXT_TokenChunkList tokens = {0};
  String8Array keywords = txt_keywords_from_lang_kind_table[TXT_LangKind_Rust];
  U64 chunk_size = Clamp(8, data.size/8, 4096);
  U64 off = 0;

  //- rad: data starts inside a big token -> finish it first
  if(ctx_token_pt.kind != TXT_TokenKind_Null)
  {
    TXT_TokenPt pt = ctx_token_pt;
    B32 ended = 0;
    for(;off < data.size && !ended;)
    {
      off += txt_rust_big_token_advance(data, off, &pt, &ended);
    }
    TXT_Token token = {pt.kind, r1u64(ctx_token_pt.off, base_off+off)};
    txt_token_chunk_list_push(scratch.arena, &tokens, chunk_size, &token);
  }

  //- rad: all following tokens
  for(;off < data.size;)
  {
    U64 start_off = off;
    U8 byte      = data.str[off];
    U8 next_byte = (off+1 < data.size) ? data.str[off+1] : 0;
    TXT_TokenKind kind = TXT_TokenKind_Null;
    U64 starter_size = 0;
    TXT_TokenPt big = txt_rust_big_token_from_off(data, off, &starter_size);

    // rad: strings, chars, comments, attributes
    if(big.kind != TXT_TokenKind_Null)
    {
      kind = big.kind;
      off += starter_size;
      B32 ended = 0;
      for(;off < data.size && !ended;)
      {
        off += txt_rust_big_token_advance(data, off, &big, &ended);
      }
    }

    // rad: whitespace
    else if(byte == ' ' || byte == '\n' || byte == '\t' || byte == '\r' || byte == '\f' || byte == '\v')
    {
      kind = TXT_TokenKind_Whitespace;
      for(;off < data.size; off += 1)
      {
        U8 b = data.str[off];
        if(b != ' ' && b != '\n' && b != '\t' && b != '\r' && b != '\f' && b != '\v')
        {
          break;
        }
      }
    }

    // rad: lifetimes and labels (char literals were taken above)
    else if(byte == '\'')
    {
      kind = TXT_TokenKind_Keyword;
      for(off += 1; off < data.size && txt_rust_byte_is_ident(data.str[off]); off += 1);
      if(off == start_off+1)
      {
        kind = TXT_TokenKind_Symbol;
      }
    }

    // rad: identifiers, keywords, raw identifiers; `$` starts macro_rules
    // metavariables ($x), as in the C tokenizer
    else if(byte == '_' || byte == '$' || byte >= 0x80 ||
            ('a' <= byte && byte <= 'z') ||
            ('A' <= byte && byte <= 'Z'))
    {
      B32 is_raw = (byte == 'r' && next_byte == '#' && off+2 < data.size &&
                    txt_rust_byte_is_ident(data.str[off+2]) && !('0' <= data.str[off+2] && data.str[off+2] <= '9'));
      kind = TXT_TokenKind_Identifier;
      for(off += (is_raw ? 2 : 1); off < data.size && txt_rust_byte_is_ident(data.str[off]); off += 1);
      if(!is_raw)
      {
        String8 token_string = str8_substr(data, r1u64(start_off, off));
        for EachIndex(idx, keywords.count)
        {
          if(str8_match(keywords.v[idx], token_string, 0))
          {
            kind = TXT_TokenKind_Keyword;
            break;
          }
        }
      }
    }

    // rad: numbers: 1_000, 0xff_u8, 2.5e10f64; a '.' continues only before
    // a digit, once (0..10, 1.max(2), x.0 are not one number)
    else if('0' <= byte && byte <= '9')
    {
      kind = TXT_TokenKind_Numeric;
      B32 seen_dot = 0;
      for(off += 1; off < data.size;)
      {
        U8 b = data.str[off];
        U8 n = (off+1 < data.size) ? data.str[off+1] : 0;
        if(b < 0x80 && txt_rust_byte_is_ident(b))
        {
          off += 1;
        }
        else if(b == '.' && !seen_dot && '0' <= n && n <= '9')
        {
          seen_dot = 1;
          off += 1;
        }
        else
        {
          break;
        }
      }
    }

    // rad: symbols; a run stops where a comment or an attribute starts
    else if(byte == '~' || byte == '!' || byte == '%' || byte == '^' ||
            byte == '&' || byte == '*' || byte == '(' || byte == ')' ||
            byte == '-' || byte == '=' || byte == '+' || byte == '[' ||
            byte == ']' || byte == '{' || byte == '}' || byte == ':' ||
            byte == ';' || byte == ',' || byte == '.' || byte == '<' ||
            byte == '>' || byte == '/' || byte == '?' || byte == '|' ||
            byte == '@' || byte == '#')
    {
      kind = TXT_TokenKind_Symbol;
      for(off += 1; off < data.size; off += 1)
      {
        U8 b = data.str[off];
        if(!(b == '~' || b == '!' || b == '%' || b == '^' ||
             b == '&' || b == '*' || b == '(' || b == ')' ||
             b == '-' || b == '=' || b == '+' || b == '[' ||
             b == ']' || b == '{' || b == '}' || b == ':' ||
             b == ';' || b == ',' || b == '.' || b == '<' ||
             b == '>' || b == '/' || b == '?' || b == '|' ||
             b == '@' || b == '#') ||
           txt_rust_big_token_from_off(data, off, 0).kind != TXT_TokenKind_Null)
        {
          break;
        }
      }
    }

    // rad: anything else
    else
    {
      kind = TXT_TokenKind_Error;
      off += 1;
    }

    TXT_Token token = {kind, r1u64(base_off+start_off, base_off+off)};
    txt_token_chunk_list_push(scratch.arena, &tokens, chunk_size, &token);
  }

  TXT_TokenArray result = txt_token_array_from_chunk_list(arena, &tokens);
  scratch_end(scratch);
  return result;
}

internal TXT_TokenArray
txt_token_array_from_data(Arena *arena, TXT_LangKind lang_kind, TXT_TokenPt ctx_token_pt, String8 data, U64 base_off, U64 limit)
{
  // rad: Rust has its own rules (lifetimes, raw strings, attributes)
  if(lang_kind == TXT_LangKind_Rust)
  {
    return txt_token_array_from_data__rust(arena, ctx_token_pt, data, base_off);
  }
  Temp scratch = scratch_begin(&arena, 1);
  TXT_TokenChunkList tokens = {0};
  if(lang_kind != TXT_LangKind_Null)
  {
    String8Array keywords = txt_keywords_from_lang_kind_table[lang_kind];
    String8Array multichar_symbols = txt_multichar_symbols_from_lang_kind_table[lang_kind];
    U64 chunk_size = Clamp(8, data.size/8, 4096);
    TXT_TokenKind active_token_kind = ctx_token_pt.kind;
    U64 active_token_start_off = ctx_token_pt.off - base_off;
    B32 escaped = 0;
    for(U64 off = 0; off <= data.size;)
    {
      U8 byte      = (off+0 < data.size) ? data.str[off+0] : 0;
      U8 next_byte = (off+1 < data.size) ? data.str[off+1] : 0;
      B32 token_finished = 0;
      U64 advance = 1;
      
      //- rjf: adjust escaping state
      B32 escaped_this_step = 0;
      if(!escaped && active_token_kind != TXT_TokenKind_Null && byte == '\\')
      {
        escaped_this_step = 1;
        escaped = 1;
      }
      
      //- rjf: take starter bytes for new tokens
      if(active_token_kind == TXT_TokenKind_Null)
      {
        if(0){}
        else if(byte == ' ' || byte == '\n' || byte == '\t' ||
                byte == '\r' || byte == '\f' || byte == '\v')
        {
          active_token_kind = TXT_TokenKind_Whitespace;
          advance = 0;
        }
        else if(byte == '_' || byte == '$' ||
                ('a' <= byte && byte <= 'z') ||
                ('A' <= byte && byte <= 'Z'))
        {
          active_token_kind = TXT_TokenKind_Identifier;
          advance = 0;
        }
        else if(('0' <= byte && byte <= '9') ||
                (byte == '.' && ('0' <= next_byte && next_byte <= '9')))
        {
          active_token_kind = TXT_TokenKind_Numeric;
          advance = 0;
        }
        else if(byte == '"')
        {
          active_token_kind = TXT_TokenKind_String;
          advance = 1;
        }
        else if(byte == '\'')
        {
          active_token_kind = TXT_TokenKind_Char;
          advance = 1;
        }
        else if(byte == '/' && next_byte == '/')
        {
          active_token_kind = TXT_TokenKind_LineComment;
          advance = 2;
        }
        else if(byte == '/' && next_byte == '*')
        {
          active_token_kind = TXT_TokenKind_BlockComment;
          advance = 2;
        }
        else if(byte == '#')
        {
          active_token_kind = TXT_TokenKind_Meta;
          advance = 1;
        }
        else if(byte == '~' || byte == '!' ||
                byte == '%' || byte == '^' ||
                byte == '&' || byte == '*' ||
                byte == '(' || byte == ')' ||
                byte == '-' || byte == '=' ||
                byte == '+' || byte == '[' ||
                byte == ']' || byte == '{' ||
                byte == '}' || byte == ':' ||
                byte == ';' || byte == ',' ||
                byte == '.' || byte == '<' ||
                byte == '>' || byte == '/' ||
                byte == '?' || byte == '|')
        {
          active_token_kind = TXT_TokenKind_Symbol;
          advance = 1;
        }
        if(active_token_kind != TXT_TokenKind_Null)
        {
          active_token_start_off = off;
        }
        else
        {
          TXT_Token token = {TXT_TokenKind_Error, r1u64(base_off+off, base_off+off+1)};
          txt_token_chunk_list_push(scratch.arena, &tokens, chunk_size, &token);
          advance = 1;
        }
      }
      
      //- rjf: look for active token enders
      else switch(active_token_kind)
      {
        default:{}break;
        case TXT_TokenKind_Whitespace:
        {
          if(byte != ' ' && byte != '\n' && byte != '\t' &&
             byte != '\r' && byte != '\f' && byte != '\v')
          {
            token_finished = 1;
            advance = 0;
          }
        }break;
        case TXT_TokenKind_Identifier:
        {
          // TODO(rjf): stupid C++ symbol names - `string_literals_like_this'
          if((byte < '0' || '9' < byte) &&
             (byte < 'a' || 'z' < byte) &&
             (byte < 'A' || 'Z' < byte) &&
             byte != '$' &&
             byte != '_')
          {
            token_finished = 1;
            advance = 0;
          }
        }break;
        case TXT_TokenKind_Numeric:
        {
          if((byte < '0' || '9' < byte) &&
             (byte < 'a' || 'z' < byte) &&
             (byte < 'A' || 'Z' < byte) &&
             byte != '.')
          {
            token_finished = 1;
            advance = 0;
          }
        }break;
        case TXT_TokenKind_String:
        {
          if(byte == '"' && !escaped)
          {
            token_finished = 1;
            advance = 1;
          }
        }break;
        case TXT_TokenKind_Char:
        {
          if(byte == '\'' && !escaped)
          {
            token_finished = 1;
            advance = 1;
          }
        }break;
        case TXT_TokenKind_Symbol:
        {
          if(byte != '~' && byte != '!' &&
             byte != '%' && byte != '^' &&
             byte != '&' && byte != '*' &&
             byte != '(' && byte != ')' &&
             byte != '-' && byte != '=' &&
             byte != '+' && byte != '[' &&
             byte != ']' && byte != '{' &&
             byte != '}' && byte != ':' &&
             byte != ';' && byte != ',' &&
             byte != '.' && byte != '<' &&
             byte != '>' && byte != '/' &&
             byte != '?' && byte != '|')
          {
            token_finished = 1;
            advance = 0;
          }
        }break;
        case TXT_TokenKind_LineComment:
        case TXT_TokenKind_Meta:
        {
          if(byte == '\r' && escaped)
          {
            escaped_this_step = 1;
          }
          if(byte == '\n' && !escaped)
          {
            token_finished = 1;
            advance = 1;
          }
        }break;
        case TXT_TokenKind_BlockComment:
        {
          if(byte == '*' && next_byte == '/')
          {
            token_finished = 1;
            advance = 2;
          }
        }break;
      }
      
      //- rjf: finish all tokens if we're at the end of the data
      if(off == data.size)
      {
        token_finished = 1;
        advance = 1;
      }
      
      //- rjf: upgrade identifiers to keywords
      if(token_finished && active_token_kind == TXT_TokenKind_Identifier)
      {
        String8 token_string = str8_substr(data, r1u64(active_token_start_off, off+advance));
        for EachIndex(idx, keywords.count)
        {
          if(str8_match(keywords.v[idx], token_string, 0))
          {
            active_token_kind = TXT_TokenKind_Keyword;
            break;
          }
        }
      }
      
      //- rjf: push completed token
      if(token_finished)
      {
        TXT_Token token = {active_token_kind, r1u64(base_off+active_token_start_off, base_off+off+advance)};
        txt_token_chunk_list_push(scratch.arena, &tokens, chunk_size, &token);
        active_token_kind = TXT_TokenKind_Null;
      }
      
      //- rjf: reset escaped state
      if(!escaped_this_step)
      {
        escaped = 0;
      }
      
      //- rjf: advance
      off += advance;
    }
  }
  TXT_TokenArray result = txt_token_array_from_chunk_list(arena, &tokens);
  scratch_end(scratch);
  return result;
}

internal TXT_Patched
txt_patched_from_info_data_patches(Arena *arena, TXT_TextInfo *info, String8 data, TXT_PatchList *patches)
{
  Temp scratch = scratch_begin(&arena, 1);
  
  //////////////////////////////
  //- rjf: produce default case, where we just have one range which covers the original data
  //
  MemoryMap last_memory_map = {0};
  U64 last_size = data.size;
  TXT_LineMap last_line_map = {0};
  TXT_TokenPtMap last_token_pt_map = {0};
  memory_map_push(scratch.arena, &last_memory_map, r1u64(0, data.size), data.str);
  txt_line_map_push(scratch.arena, &last_line_map, r1u64(1, info->lines_count+1), info->lines_ranges, 0);
  txt_token_pt_map_push(scratch.arena, &last_token_pt_map, r1u64(1, info->big_token_pts_count+1), info->big_token_pts, 0);
  
  //////////////////////////////
  //- rjf: apply the patches in order, each being able to slice/dice the previous memory map
  //
  for EachNode(n, TXT_PatchNode, patches->first)
  {
    MemoryMap next_memory_map = {0};
    TXT_LineMap next_line_map = {0};
    TXT_TokenPtMap next_token_pt_map = {0};
    
    ////////////////////////////
    //- rjf: compute portion of memory before/after this replace-range
    //
    Rng1U64 pre_replace_range = r1u64(0, n->v.range.min);
    Rng1U64 post_replace_range = r1u64(n->v.range.max, last_size);
    
    ////////////////////////////
    //- rjf: map this replace range -> range of replaced newlines
    //
    Rng1U64 replace_line_num_range = {0};
    {
      replace_line_num_range.min = txt_line_num_from_off(&last_line_map, n->v.range.min);
      replace_line_num_range.max = txt_line_num_from_off(&last_line_map, n->v.range.max);
    }
    
    ////////////////////////////
    //- rjf: compute ranges of (unchanged) lines before/after this replace-range
    //
    Rng1U64 pre_replace_line_num_range = r1u64(1, replace_line_num_range.min);
    Rng1U64 post_replace_line_num_range = r1u64(replace_line_num_range.max+1, last_line_map.total_line_count+1);
    
    // NOTE(rad): (the token pts kept around this replace are picked below)

    ////////////////////////////
    //- rjf: compute delta & next size
    //
    S64 size_delta = (S64)n->v.replace.size - (S64)dim_1u64(n->v.range);
    U64 next_size = (U64)((S64)last_size + size_delta);
    
    ////////////////////////////
    //- rjf: compute line count delta, + list of line ranges inside of replace
    //
    S64 line_delta = 0;
    Rng1U64List replace_line_ranges = {0};
    {
      U64 last_line_start_off = 0;
      line_delta -= (S64)dim_1u64(replace_line_num_range);
      for EachIndex(idx, n->v.replace.size)
      {
        if(n->v.replace.str[idx] == '\n')
        {
          line_delta += 1;
          Rng1U64 line_range = r1u64(last_line_start_off, idx);
          if(idx > 0 && n->v.replace.str[idx-1] == '\r')
          {
            line_range.max -= 1;
          }
          rng1u64_list_push(scratch.arena, &replace_line_ranges, line_range);
          last_line_start_off = idx+1;
        }
      }
      rng1u64_list_push(scratch.arena, &replace_line_ranges, r1u64(last_line_start_off, n->v.replace.size));
    }
    
    ////////////////////////////
    //- rjf: push all portions of pre-replace / post-replace ranges in previous memory map
    //
    {
      for EachNode(map_n, MemoryMapRangeNode, last_memory_map.first_range)
      {
        Rng1U64 range_x_pre = intersect_1u64(pre_replace_range, map_n->v.vaddr_range);
        Rng1U64 range_x_post = intersect_1u64(post_replace_range, map_n->v.vaddr_range);
        if(range_x_pre.max > range_x_pre.min)
        {
          memory_map_push(scratch.arena, &next_memory_map, range_x_pre, (U8 *)map_n->v.base + (range_x_pre.min - map_n->v.vaddr_range.min));
        }
        if(range_x_post.max > range_x_post.min)
        {
          Rng1U64 range_x_post_shifted = range_x_post;
          range_x_post_shifted.min = (U64)((S64)range_x_post_shifted.min + size_delta);
          range_x_post_shifted.max = (U64)((S64)range_x_post_shifted.max + size_delta);
          memory_map_push(scratch.arena, &next_memory_map, range_x_post_shifted, (U8 *)map_n->v.base + (range_x_post.min - map_n->v.vaddr_range.min));
        }
      }
    }
    
    ////////////////////////////
    //- rjf: push replaced range
    //
    if(n->v.replace.size != 0)
    {
      memory_map_push(scratch.arena, &next_memory_map, r1u64(n->v.range.min, n->v.range.min + n->v.replace.size), n->v.replace.str);
    }
    
    ////////////////////////////
    //- rjf: push all portions of pre-replace / post-replace ranges in previous line map
    //
    {
      for EachNode(map_n, TXT_LineMapRangeNode, last_line_map.first_range)
      {
        Rng1U64 num_range = map_n->num_range;
        Rng1U64 range_x_pre = intersect_1u64(pre_replace_line_num_range, num_range);
        Rng1U64 range_x_post = intersect_1u64(post_replace_line_num_range, num_range);
        if(range_x_pre.max > range_x_pre.min)
        {
          txt_line_map_push(scratch.arena, &next_line_map, r1u64(range_x_pre.min, range_x_pre.max), map_n->ranges + (range_x_pre.min - num_range.min), map_n->delta);
        }
        if(range_x_post.max > range_x_post.min)
        {
          Rng1U64 range_x_post_shifted = range_x_post;
          range_x_post_shifted.min = (U64)((S64)range_x_post_shifted.min + line_delta);
          range_x_post_shifted.max = (U64)((S64)range_x_post_shifted.max + line_delta);
          txt_line_map_push(scratch.arena, &next_line_map, r1u64(range_x_post_shifted.min, range_x_post_shifted.max), map_n->ranges + (range_x_post.min - num_range.min), map_n->delta + size_delta);
        }
      }
    }
    
    ////////////////////////////
    //- rjf: compute affected line ranges
    //
    U64 affected_line_count = replace_line_ranges.count;
    Rng1U64 *affected_line_ranges = push_array(arena, Rng1U64, affected_line_count);
    {
      Rng1U64Node *replace_line_range_n = replace_line_ranges.first;
      for EachIndex(affected_line_idx, affected_line_count)
      {
        Rng1U64 affected_line_range = {0};
        if(replace_line_range_n != 0)
        {
          Rng1U64 replace_line_range = replace_line_range_n->v;
          affected_line_range = r1u64(replace_line_range.min + n->v.range.min, replace_line_range.max + n->v.range.min);
          replace_line_range_n = replace_line_range_n->next;
        }
        
        // rjf: the first line in the range -> take min from original line map
        if(affected_line_idx == 0)
        {
          Rng1U64 og_line_range = txt_range_from_line_num(&last_line_map, replace_line_num_range.min + affected_line_idx);
          affected_line_range.min = og_line_range.min;
        }
        
        // rjf: the last line in the range -> take remaining suffix from original line map
        if(affected_line_idx == affected_line_count-1 && affected_line_idx >= ClampBot(0, line_delta))
        {
          Rng1U64 og_line_range = txt_range_from_line_num(&last_line_map, replace_line_num_range.max);
          if(og_line_range.max > n->v.range.max)
          {
            affected_line_range.max += og_line_range.max - n->v.range.max;
          }
        }
        
        // rjf: commit
        affected_line_ranges[affected_line_idx] = affected_line_range;
      }
    }
    
    ////////////////////////////
    //- rjf: push affected line ranges
    //
    txt_line_map_push(scratch.arena, &next_line_map, r1u64(replace_line_num_range.min, replace_line_num_range.min + affected_line_count), affected_line_ranges, 0);
    
    ////////////////////////////
    //- rjf: compute token pt delta, + token pt ranges to keep, + new token pts
    //
    S64 token_pt_delta = 0;
    Rng1U64 pre_replace_token_pt_range = {0};
    Rng1U64 post_replace_token_pt_range = {0};
    TXT_TokenPt *new_token_pts = 0;
    U64 new_token_pts_count = 0;
    {
      // NOTE(rad): rewritten. kept before the edit: the pts before the re-lex
      // start; kept after it: the pts at or after the end of the replaced
      // range (shifted by the size delta); the re-lex makes the pts in
      // between, and the numbers stay contiguous by construction. (the old
      // code kept/dropped single pts next to the edit and renumbered only for
      // those, so an edit across several pts gave two ranges the same
      // numbers, and text inserted right before an opener dropped its start
      // pt.) the re-lex starts 3 bytes before the edit: an edit can complete
      // an opener ("/" + "/", "r#" + "\"").
      U64 token_pt_lex_start_off = n->v.range.min - Min(n->v.range.min, 3);
      U64 pre_last_num = (token_pt_lex_start_off > 0) ? txt_token_pt_num_from_off(&last_token_pt_map, token_pt_lex_start_off-1) : 0;
      U64 post_first_num = ((n->v.range.max > 0) ? txt_token_pt_num_from_off(&last_token_pt_map, n->v.range.max-1) : 0) + 1;
      TXT_TokenPt ctx_token_pt = txt_token_pt_from_num(&last_token_pt_map, pre_last_num);

      //- rad: Rust: inside a token at the re-lex start -> re-lex that whole
      // token from its start (if it is not huge): starting inside it can land
      // in the middle of its closer (`"#`) or right after an escape
      if(info->lang == TXT_LangKind_Rust && ctx_token_pt.kind != TXT_TokenKind_Null &&
         token_pt_lex_start_off - ctx_token_pt.off <= KB(64))
      {
        token_pt_lex_start_off = ctx_token_pt.off;
        pre_last_num -= 1;
        ctx_token_pt = txt_token_pt_from_num(&last_token_pt_map, pre_last_num);
      }
      pre_replace_token_pt_range = r1u64(1, pre_last_num+1);
      post_replace_token_pt_range = r1u64(post_first_num, last_token_pt_map.total_pt_count+1);

      //- rjf: re-lex region to find new token pts
      typedef struct TokenPtChunkNode TokenPtChunkNode;
      struct TokenPtChunkNode
      {
        TokenPtChunkNode *next;
        TXT_TokenPt *v;
        U64 count;
        U64 cap;
      };
      TokenPtChunkNode *first_pt_chunk = 0;
      TokenPtChunkNode *last_pt_chunk = 0;
      U64 total_new_pt_count = 0;
      {
        TXT_TokenKind active_token_kind = ctx_token_pt.kind;
        U64 active_token_start_off = 0;
        String8 herestring_marker = {0};
        Rng1U64 relex_range = r1u64(token_pt_lex_start_off, n->v.range.min + n->v.replace.size);
        String8 relex_data = memory_map_data_from_range(scratch.arena, &next_memory_map, relex_range);

        //- rad: Rust: the same big-token helpers as the whole-file scan
        if(info->lang == TXT_LangKind_Rust)
        {
          TXT_TokenPt active_pt = ctx_token_pt;
          String8 lex_data = relex_data;
          for(U64 off = 0; off < lex_data.size;)
          {
            B32 pt_changed = 0;
            TXT_TokenPt pt = {0};
            if(active_pt.kind == TXT_TokenKind_Null)
            {
              U64 starter_size = 0;
              TXT_TokenPt start_pt = txt_rust_big_token_from_off(lex_data, off, &starter_size);
              if(start_pt.kind == TXT_TokenKind_Null)
              {
                off += 1;
              }
              else
              {
                start_pt.off = relex_range.min + off;
                active_pt = start_pt;
                pt = start_pt;
                pt_changed = 1;
                off += starter_size;
              }
            }
            else
            {
              B32 ended = 0;
              off += txt_rust_big_token_advance(lex_data, off, &active_pt, &ended);
              if(ended)
              {
                pt.kind = TXT_TokenKind_Null;
                pt.off = relex_range.min + off;
                MemoryZeroStruct(&active_pt);
                pt_changed = 1;
              }
            }
            if(pt_changed)
            {
              TokenPtChunkNode *chunk = last_pt_chunk;
              if(chunk == 0 || chunk->count >= chunk->cap)
              {
                chunk = push_array(scratch.arena, TokenPtChunkNode, 1);
                SLLQueuePush(first_pt_chunk, last_pt_chunk, chunk);
                chunk->cap = 256;
                chunk->v = push_array(scratch.arena, TXT_TokenPt, chunk->cap);
              }
              chunk->v[chunk->count] = pt;
              chunk->count += 1;
              total_new_pt_count += 1;
            }
          }
        }

        //- rjf: other languages
        else
        {
        B32 escaped = 0;
        for(U64 off = 0; off <= relex_data.size; off += 1)
        {
          U64 extra_advance = 0;
          U8 byte      = (off+0 < relex_data.size) ? relex_data.str[off+0] : 0;
          U8 next_byte = (off+1 < relex_data.size) ? relex_data.str[off+1] : 0;
          
          //- rjf: adjust escaping state
          if(active_token_kind != TXT_TokenKind_Null && byte == '\\')
          {
            escaped = 1;
          }
          
          //- rjf: no active token -> look for starters
          TXT_TokenKind start_active_token_kind = active_token_kind;
          U64 active_token_end_off = 0;
          if(active_token_kind == TXT_TokenKind_Null)
          {
            // rjf: " -> start a string literal
            if(byte == '"')
            {
              active_token_kind = TXT_TokenKind_String;
              herestring_marker.size = 0;
            }
            
            // rjf: ' -> start a char literal
            else if(byte == '\'')
            {
              active_token_kind = TXT_TokenKind_Char;
              herestring_marker.size = 0;
            }
            
            // rjf: R" -> start a C++11+ style herestring
            else if(byte == 'R' && next_byte == '"')
            {
              active_token_kind = TXT_TokenKind_String;
              U64 next_paren_pos = str8_find_needle(str8_prefix(data, off+2+256), off+2, s("("), 0);
              herestring_marker = str8_substr(data, r1u64(off+2, next_paren_pos));
              extra_advance = 1 + herestring_marker.size + 1;
            }
            
            // rjf: // -> start a single-line comment
            else if(byte == '/' && next_byte == '/')
            {
              active_token_kind = TXT_TokenKind_LineComment;
              extra_advance = 1;
            }
            
            // rjf: /* -> start a multi-line comment
            else if(byte == '/' && next_byte == '*')
            {
              active_token_kind = TXT_TokenKind_BlockComment;
              extra_advance = 1;
            }
            
            // rjf: # -> start a meta
            else if(byte == '#')
            {
              active_token_kind = TXT_TokenKind_Meta;
            }
            
            // rjf: got a token kind -> remember its starting offset
            if(active_token_kind != TXT_TokenKind_Null)
            {
              active_token_start_off = off;
            }
          }
          
          //- rjf: look for enders
          else switch(active_token_kind)
          {
            default:{}break;
            case TXT_TokenKind_LineComment:
            case TXT_TokenKind_Meta:
            if(!escaped && byte == '\n')
            {
              active_token_end_off = off;
            }break;
            case TXT_TokenKind_BlockComment:
            if(byte == '*' && next_byte == '/')
            {
              active_token_end_off = off+1;
              extra_advance = 1;
            }break;
            case TXT_TokenKind_String:
            {
              // TODO(rjf): herestrings - we might not have the right herestring marker, given that it may
              // be from an earlier token pt...?
              if(byte == '"' && !escaped)
              {
                active_token_end_off = off+1;
              }
            }break;
            case TXT_TokenKind_Char:
            if(!escaped && byte == '\'')
            {
              active_token_end_off = off+1;
              extra_advance = 1;
            }break;
          }
          
          //- rjf: found ender -> reset state
          if(active_token_end_off > active_token_start_off)
          {
            active_token_kind = TXT_TokenKind_Null;
          }
          
          //- rjf: state changed -> push new pt
          if(active_token_kind != start_active_token_kind)
          {
            TXT_TokenPt pt = {active_token_kind, relex_range.min + off + ((active_token_kind == TXT_TokenKind_Null) ? extra_advance : 0)};
            TokenPtChunkNode *chunk = last_pt_chunk;
            if(chunk == 0 || chunk->count >= chunk->cap)
            {
              chunk = push_array(scratch.arena, TokenPtChunkNode, 1);
              SLLQueuePush(first_pt_chunk, last_pt_chunk, chunk);
              chunk->cap = 256;
              chunk->v = push_array(scratch.arena, TXT_TokenPt, chunk->cap);
            }
            chunk->v[chunk->count] = pt;
            chunk->count += 1;
            total_new_pt_count += 1;
          }
          
          //- rjf: reset escaped state
          escaped = 0;

          //- rjf: do extra advance
          off += extra_advance;
        }
        }
      }
      
      //- rjf: join new token pts
      if(total_new_pt_count != 0)
      {
        new_token_pts_count = total_new_pt_count;
        new_token_pts = push_array(arena, TXT_TokenPt, new_token_pts_count);
        U64 idx = 0;
        for(TokenPtChunkNode *n = first_pt_chunk; n != 0; n = n->next)
        {
          MemoryCopy(new_token_pts + idx, n->v, sizeof(n->v[0]) * n->count);
          idx += n->count;
        }
      }

      //- rad: kept-after pts are numbered right after the new ones
      token_pt_delta = (S64)(pre_replace_token_pt_range.max + new_token_pts_count) - (S64)post_replace_token_pt_range.min;
    }
    
    ////////////////////////////
    //- rjf: push all portions of pre-replace / post-replace ranges in previous token pt map
    //
    {
      for EachNode(n,  TXT_TokenPtMapRangeNode, last_token_pt_map.first_range)
      {
        Rng1U64 num_range = n->num_range;
        Rng1U64 range_x_pre = intersect_1u64(pre_replace_token_pt_range, num_range);
        Rng1U64 range_x_post = intersect_1u64(post_replace_token_pt_range, num_range);
        if(range_x_pre.max > range_x_pre.min)
        {
          txt_token_pt_map_push(scratch.arena, &next_token_pt_map, range_x_pre, n->pts + (range_x_pre.min - num_range.min), n->delta);
        }
        if(range_x_post.max > range_x_post.min)
        {
          Rng1U64 range_x_post_shifted = range_x_post;
          range_x_post_shifted.min = (U64)((S64)range_x_post_shifted.min + token_pt_delta);
          range_x_post_shifted.max = (U64)((S64)range_x_post_shifted.max + token_pt_delta);
          txt_token_pt_map_push(scratch.arena, &next_token_pt_map, range_x_post_shifted, n->pts + (range_x_post.min - num_range.min), n->delta + size_delta);
        }
      }
    }
    
    ////////////////////////////
    //- rjf: push new token pts
    //
    if(new_token_pts_count != 0)
    {
      txt_token_pt_map_push(scratch.arena, &next_token_pt_map, r1u64(pre_replace_token_pt_range.max, pre_replace_token_pt_range.max + new_token_pts_count), new_token_pts, 0);
    }
    
    ////////////////////////////
    //- rjf: advance to the next state
    //
    last_memory_map = next_memory_map;
    last_size = next_size;
    last_line_map = next_line_map;
    last_token_pt_map = next_token_pt_map;
  }
  
  // rjf: fill result
  TXT_Patched result = {0};
  {
    for EachNode(n, MemoryMapRangeNode, last_memory_map.first_range)
    {
      memory_map_push(arena, &result.memory_map, n->v.vaddr_range, n->v.base);
    }
    result.size = last_size;
    for EachNode(n, TXT_LineMapRangeNode, last_line_map.first_range)
    {
      txt_line_map_push(arena, &result.line_map, n->num_range, n->ranges, n->delta);
    }
    for EachNode(n, TXT_TokenPtMapRangeNode, last_token_pt_map.first_range)
    {
      txt_token_pt_map_push(arena, &result.token_pt_map, n->num_range, n->pts, n->delta);
    }
  }
  
  scratch_end(scratch);
  return result;
}

//~ TODO(rjf): old unpatched text viz code:

internal U64
txt_off_from_pt(TXT_TextInfo *info, TXT_PatchList *patches, TxtPt pt)
{
  U64 off = 0;
  {
    if(1 <= pt.line && pt.line <= info->lines_count)
    {
      Rng1U64 line_range = info->lines_ranges[pt.line-1];
      off = line_range.min + (pt.column-1);
    }
  }
  return off;
}

internal TxtPt
txt_pt_from_off__linear_scan(TXT_TextInfo *info, TXT_PatchList *patches, U64 off)
{
  TxtPt pt = {0};
  {
    for(U64 line_idx = 0; line_idx < info->lines_count; line_idx += 1)
    {
      if(contains_1u64(info->lines_ranges[line_idx], off))
      {
        pt.line = (S64)line_idx + 1;
        pt.column = (S64)(off - info->lines_ranges[line_idx].min) + 1;
        break;
      }
    }
  }
  return pt;
}

internal TXT_TokenArray
txt_token_array_from_info_line_num__linear_scan(TXT_TextInfo *info, S64 line_num)
{
  TXT_TokenArray line_tokens = {0};
  if(1 <= line_num && line_num <= info->lines_count)
  {
    Rng1U64 line_range = info->lines_ranges[line_num-1];
    for(U64 token_idx = 0; token_idx < info->tokens.count; token_idx += 1)
    {
      Rng1U64 token_range = info->tokens.v[token_idx].range;
      Rng1U64 token_x_line = intersect_1u64(token_range, line_range);
      if(token_x_line.max > token_x_line.min)
      {
        if(line_tokens.v == 0)
        {
          line_tokens.v = info->tokens.v+token_idx;
        }
        line_tokens.count += 1;
      }
      else if(line_tokens.v != 0)
      {
        break;
      }
    }
  }
  return line_tokens;
}

internal Rng1U64
txt_expr_off_range_from_line_off_range_string_tokens(U64 off, Rng1U64 line_range, String8 line_text, TXT_TokenArray *line_tokens)
{
  Rng1U64 result = {0};
  Temp scratch = scratch_begin(0, 0);
  {
    // rjf: unpack line info
    TXT_Token *line_tokens_first = line_tokens->v;
    TXT_Token *line_tokens_opl = line_tokens->v+line_tokens->count;
    
    // rjf: find token containing `off`
    TXT_Token *pt_token = 0;
    for(TXT_Token *token = line_tokens_first;
        token < line_tokens_opl;
        token += 1)
    {
      if(contains_1u64(token->range, off))
      {
        Rng1U64 token_range_clamped = intersect_1u64(line_range, token->range);
        String8 token_string = str8_substr(line_text, r1u64(token_range_clamped.min - line_range.min, token_range_clamped.max - line_range.min));
        B32 token_ender = 0;
        switch(token->kind)
        {
          default:{}break;
          case TXT_TokenKind_Symbol:
          {
            token_ender = (str8_match(token_string, str8_lit("]"), 0));
          }break;
          case TXT_TokenKind_Identifier:
          case TXT_TokenKind_Numeric:
          case TXT_TokenKind_Keyword:
          case TXT_TokenKind_Meta:
          {
            token_ender = 1;
          }break;
        }
        if(token_ender)
        {
          pt_token = token;
        }
        break;
      }
    }
    
    // rjf: walk forward from pt_token - consume closing braces
    if(pt_token != 0)
    {
      for(TXT_Token *wf_token = pt_token+1;
          wf_token < line_tokens_opl;
          wf_token += 1)
      {
        Rng1U64 wf_token_range_clamped = intersect_1u64(line_range, wf_token->range);
        String8 wf_token_string = str8_substr(line_text, r1u64(wf_token_range_clamped.min - line_range.min, wf_token_range_clamped.max - line_range.min));
        if(wf_token->kind == TXT_TokenKind_Symbol && str8_match(wf_token_string, str8_lit("]"), 0))
        {
          pt_token = wf_token;
        }
        else
        {
          break;
        }
      }
    }
    
    // rjf: found token containing `off`? -> mark that as our initial range
    if(pt_token != 0)
    {
      result = pt_token->range;
    }
    
    // rjf: walk back from pt_token - try to find plausible start of expression
    if(pt_token != 0)
    {
      B32 walkback_done = 0;
      S32 nest = 0;
      for(TXT_Token *wb_token = pt_token;
          wb_token >= line_tokens_first && walkback_done == 0;
          wb_token -= 1)
      {
        Rng1U64 wb_token_range_clamped = intersect_1u64(line_range, wb_token->range);
        String8 wb_token_string = str8_substr(line_text, r1u64(wb_token_range_clamped.min - line_range.min, wb_token_range_clamped.max - line_range.min));
        B32 include_wb_token = 0;
        switch(wb_token->kind)
        {
          default:{}break;
          case TXT_TokenKind_Symbol:
          {
            B32 is_scope_resolution = str8_match(wb_token_string, str8_lit("::"), 0);
            B32 is_dot = str8_match(wb_token_string, str8_lit("."), 0);
            B32 is_arrow = str8_match(wb_token_string, str8_lit("->"), 0);
            B32 is_open_bracket = str8_match(wb_token_string, str8_lit("["), 0);
            B32 is_close_bracket = str8_match(wb_token_string, str8_lit("]"), 0);
            nest -= !!(is_open_bracket);
            nest += !!(is_close_bracket);
            if(is_scope_resolution ||
               is_dot ||
               is_arrow ||
               is_open_bracket||
               is_close_bracket)
            {
              include_wb_token = 1;
            }
          }break;
          case TXT_TokenKind_Identifier:
          case TXT_TokenKind_Numeric:
          {
            include_wb_token = 1;
          }break;
        }
        if(include_wb_token)
        {
          result = union_1u64(result, wb_token->range);
        }
        else if(nest == 0)
        {
          walkback_done = 1;
        }
      }
    }
    
    // rjf: exclude standalone numerics
    if(pt_token != 0 && pt_token->kind == TXT_TokenKind_Numeric && result.min == pt_token->range.min && result.max == pt_token->range.max)
    {
      MemoryZeroStruct(&result);
    }
  }
  scratch_end(scratch);
  return result;
}

internal Rng1U64
txt_expr_off_range_from_info_data_pt(TXT_TextInfo *info, String8 data, TxtPt pt)
{
  Rng1U64 result = {0};
  Temp scratch = scratch_begin(0, 0);
  if(1 <= pt.line && pt.line <= info->lines_count)
  {
    // rjf: unpack line info
    Rng1U64 line_range = info->lines_ranges[pt.line-1];
    String8 line_text = str8_substr(data, line_range);
    TXT_LineTokensSlice line_tokens_slice = txt_line_tokens_slice_from_info_data_line_range(scratch.arena, info, data, r1s64(pt.line, pt.line));
    TXT_TokenArray line_tokens = line_tokens_slice.line_tokens[0];
    TXT_Token *line_tokens_first = line_tokens.v;
    TXT_Token *line_tokens_opl = line_tokens.v+line_tokens.count;
    U64 pt_off = line_range.min + (pt.column-1);
    
    // rjf: grab offset range of expression
    result = txt_expr_off_range_from_line_off_range_string_tokens(pt_off, line_range, line_text, &line_tokens);
  }
  scratch_end(scratch);
  return result;
}

internal String8
txt_string_from_info_data_txt_rng(TXT_TextInfo *info, String8 data, TXT_PatchList *patches, TxtRng rng)
{
  Rng1U64 rng_off = r1u64(txt_off_from_pt(info, patches, rng.min), txt_off_from_pt(info, patches, rng.max));
  String8 result = str8_substr(data, rng_off);
  return result;
}

internal String8
txt_string_from_info_data_line_num(TXT_TextInfo *info, String8 data, S64 line_num)
{
  String8 result = {0};
  if(1 <= line_num && line_num <= info->lines_count)
  {
    result = str8_substr(data, info->lines_ranges[line_num-1]);
  }
  return result;
}

internal TXT_LineTokensSlice
txt_line_tokens_slice_from_info_data_line_range(Arena *arena, TXT_TextInfo *info, String8 data, Rng1S64 line_range)
{
  TXT_LineTokensSlice result = {0};
  Temp scratch = scratch_begin(&arena, 1);
  if(info->lines_count != 0)
  {
    Rng1S64 line_range_clamped = r1s64(Clamp(1, line_range.min, (S64)info->lines_count), Clamp(1, line_range.max, (S64)info->lines_count));
    U64 line_count = (U64)dim_1s64(line_range_clamped)+1;
    
    // rjf: allocate output arrays
    result.line_tokens = push_array(arena, TXT_TokenArray, line_count);
    
    // rjf: binary search to find first token
    TXT_Token *tokens_first = 0;
    ProfScope("binary search to find first token")
    {
      Rng1U64 slice_range = r1u64(info->lines_ranges[line_range_clamped.min-1].min, info->lines_ranges[line_range_clamped.max-1].max);
      U64 min_idx = 0;
      U64 opl_idx = info->tokens.count;
      for(;;)
      {
        U64 mid_idx = (opl_idx+min_idx)/2;
        if(mid_idx >= opl_idx)
        {
          break;
        }
        TXT_Token *mid_token = &info->tokens.v[mid_idx];
        if(mid_token->range.min > slice_range.max)
        {
          opl_idx = mid_idx;
        }
        else if(mid_token->range.max < slice_range.min)
        {
          min_idx = mid_idx;
        }
        else if(tokens_first == 0 || mid_token->range.min < tokens_first->range.min)
        {
          tokens_first = mid_token;
          opl_idx = mid_idx;
        }
        if(mid_idx == min_idx && mid_idx+1 == opl_idx)
        {
          break;
        }
      }
    }
    
    // rjf: grab per-line tokens
    TXT_TokenList *line_tokens_lists = push_array(scratch.arena, TXT_TokenList, line_count);
    if(tokens_first != 0) ProfScope("grab per-line tokens")
    {
      TXT_Token *tokens_opl = info->tokens.v+info->tokens.count;
      U64 line_slice_idx = 0;
      for(TXT_Token *token = tokens_first; token < tokens_opl && line_slice_idx < line_count;)
      {
        if(token->range.min < info->lines_ranges[line_slice_idx+line_range.min-1].max)
        {
          if(token->range.max > info->lines_ranges[line_slice_idx+line_range.min-1].min)
          {
            txt_token_list_push(scratch.arena, &line_tokens_lists[line_slice_idx], token);
          }
          B32 need_token_advance = 0;
          B32 need_line_advance = 0;
          if(token->range.max >= info->lines_ranges[line_slice_idx+line_range.min-1].max)
          {
            need_line_advance = 1;
          }
          if(token->range.max <= info->lines_ranges[line_slice_idx+line_range.min-1].max)
          {
            need_token_advance += 1;
          }
          if(need_line_advance) { line_slice_idx += 1; }
          if(need_token_advance) { token += 1; }
        }
        else
        {
          line_slice_idx += 1;
        }
      }
    }
    
    // rjf: bake per-line tokens to arrays
    for(U64 line_slice_idx = 0; line_slice_idx < line_count; line_slice_idx += 1)
    {
      result.line_tokens[line_slice_idx] = txt_token_array_from_list(arena, &line_tokens_lists[line_slice_idx]);
    }
  }
  scratch_end(scratch);
  return result;
}

internal TXT_ScopeNode *
txt_scope_node_from_info_num(TXT_TextInfo *info, U64 num)
{
  TXT_ScopeNode *result = &txt_scope_node_nil;
  if(1 <= num && num <= info->scope_nodes.count)
  {
    result = &info->scope_nodes.v[num-1];
  }
  return result;
}

internal TXT_ScopeNode *
txt_scope_node_from_info_off(TXT_TextInfo *info, U64 off)
{
  TXT_ScopeNode *result = &txt_scope_node_nil;
  if(info->scope_pts.count != 0)
  {
    U64 first = 0;
    U64 opl = info->scope_pts.count;
    for(;;)
    {
      U64 mid = (first + opl) / 2;
      U64 mid_off = info->tokens.v[info->scope_pts.v[mid].token_idx].range.min;
      if(mid_off < off)
      {
        first = mid;
      }
      else if(off < mid_off)
      {
        opl = mid;
      }
      else
      {
        first = mid;
        break;
      }
      if(opl - first <= 1)
      {
        break;
      }
    }
    if(1 <= info->scope_pts.v[first].scope_num && info->scope_pts.v[first].scope_num <= info->scope_nodes.count)
    {
      TXT_ScopeNode *closest_node = &info->scope_nodes.v[info->scope_pts.v[first].scope_num-1];
      for(TXT_ScopeNode *scope_n = closest_node;
          scope_n != &txt_scope_node_nil;
          scope_n = txt_scope_node_from_info_num(info, scope_n->parent_num))
      {
        if(info->tokens.v[scope_n->token_idx_range.min].range.min <= off && off < info->tokens.v[scope_n->token_idx_range.max].range.max)
        {
          result = scope_n;
          break;
        }
      }
    }
  }
  return result;
}

internal TXT_ScopeNode *
txt_scope_node_from_info_pt(TXT_TextInfo *info, TXT_PatchList *patches, TxtPt pt)
{
  U64 off = txt_off_from_pt(info, patches, pt);
  TXT_ScopeNode *result = txt_scope_node_from_info_off(info, off);
  return result;
}

////////////////////////////////
//~ rjf: Artifact Cache Hooks / Lookups

typedef struct TXT_Artifact TXT_Artifact;
struct TXT_Artifact
{
  Arena *arena;
  U128 data_hash;
  TXT_TextInfo info;
};

typedef struct TXT_ArtifactCreateShared TXT_ArtifactCreateShared;
struct TXT_ArtifactCreateShared
{
  Arena *arena;
  TXT_TextInfo info;
  TXT_Artifact *artifact;
};

internal AC_Artifact
txt_artifact_create(String8 key, B32 *cancel_signal, AC_Status *status_out, U64 *gen_out)
{
  ProfBeginFunction();
  Temp scratch = scratch_begin(0, 0);
  Access *access = access_open();
  
  //- rjf: get shared state
  TXT_ArtifactCreateShared *shared = 0;
  if(lane_idx() == 0)
  {
    shared = push_array(scratch.arena, TXT_ArtifactCreateShared, 1);
  }
  lane_sync_u64(&shared, 0);
  
  //- rjf: unpack key
  U128 hash = {0};
  TXT_LangKind lang = TXT_LangKind_Null;
  str8_deserial_read_struct(key, 0, &hash);
  str8_deserial_read_struct(key, sizeof(hash), &lang);
  String8 data = c_data_from_hash(access, hash);
  TXT_LangLexFunctionType *lex_function = txt_lex_function_from_lang_kind(lang);
  
  //- rjf: data -> text info
  if(!u128_match(hash, u128_zero()))
  {
    if(lane_idx() == 0)
    {
      shared->arena = arena_alloc();
    }
    
    //- rjf: set # of bytes to process
    //                  (line counting)    (line measuring)   (lexing)
    set_progress_target(data.size        + data.size        + data.size*(lang != TXT_LangKind_Null));
    
    //- rjf: count # of lines
    U64 lane_line_count = 0;
    U64 *lane_line_counts = 0;
    if(lane_idx() == 0)
    {
      lane_line_counts = push_array(scratch.arena, U64, lane_count());
    }
    lane_sync_u64(&lane_line_counts, 0);
    {
      Rng1U64 range = lane_range(data.size+1);
      for EachInRange(idx, range)
      {
        if(idx%1000 == 0 && ins_atomic_u32_eval(cancel_signal))
        {
          break;
        }
        if(idx == data.size || data.str[idx] == '\n')
        {
          lane_line_count += 1;
        }
        if(idx && idx%1000 == 0)
        {
          add_progress(1000);
        }
      }
    }
    ins_atomic_u64_add_eval(&shared->info.lines_count, lane_line_count);
    lane_line_counts[lane_idx()] = lane_line_count;
    lane_sync();
    set_progress(Min(data.size, 1024) + data.size);
    
    //- rjf: figure out which starting line idx each lane will take
    U64 *lane_line_base_idxs = 0;
    if(lane_idx() == 0)
    {
      lane_line_base_idxs = push_array(scratch.arena, U64, lane_count());
      U64 idx = 0;
      for EachIndex(l_idx, lane_count())
      {
        lane_line_base_idxs[l_idx] = idx;
        idx += lane_line_counts[l_idx];
      }
    }
    lane_sync_u64(&lane_line_base_idxs, 0);
    
    //- rjf: allocate & store line ranges
    U64 *lane_line_max_size = 0;
    U64 *lane_cr_count = 0;
    if(lane_idx() == 0)
    {
      lane_cr_count = push_array(scratch.arena, U64, lane_count());
      lane_line_max_size = push_array(scratch.arena, U64, lane_count());
      shared->info.lines_ranges = push_array_no_zero(shared->arena, Rng1U64, shared->info.lines_count);
    }
    lane_sync_u64(&lane_line_max_size, 0);
    lane_sync_u64(&lane_cr_count, 0);
    {
      Rng1U64 range = lane_range(data.size+1);
      U64 lane_line_idx = 0;
      U64 line_start_idx = range.min;
      for EachInRange(idx, range)
      {
        if(idx%1000 == 0 && ins_atomic_u32_eval(cancel_signal))
        {
          break;
        }
        if(idx == data.size || data.str[idx] == '\n')
        {
          if(lane_line_idx == 0 && line_start_idx > 0)
          {
            for(U64 idx2 = line_start_idx - 1; idx2 < data.size; idx2 -= 1)
            {
              if(data.str[idx2] == '\n')
              {
                line_start_idx = idx2+1;
                break;
              }
              else if(idx2 == 0)
              {
                line_start_idx = idx2;
                break;
              }
            }
          }
          Rng1U64 line_range = r1u64(line_start_idx, idx);
          if(idx > 0 && data.str[idx-1] == '\r' && line_range.max > line_range.min)
          {
            lane_cr_count[lane_idx()] += 1;
            line_range.max -= 1;
          }
          U64 line_size = dim_1u64(line_range);
          shared->info.lines_ranges[lane_line_base_idxs[lane_idx()] + lane_line_idx] = line_range;
          lane_line_max_size[lane_idx()] = Max(lane_line_max_size[lane_idx()], line_size);
          lane_line_idx += 1;
          line_start_idx = idx+1;
        }
        if(idx && idx%1000 == 0)
        {
          add_progress(1000);
        }
      }
    }
    lane_sync();
    set_progress(Min(data.size, 1024) + data.size + data.size);
    
    //- rjf: find max line size across all lanes
    if(lane_idx() == 0)
    {
      for EachIndex(l_idx, lane_count())
      {
        shared->info.lines_max_size = Max(shared->info.lines_max_size, lane_line_max_size[l_idx]);
      }
    }
    lane_sync();
    
    //- rjf: pick LF/CRLF based on significant ratio of CR characters across all lanes
    {
      TXT_LineEndKind line_end_kind = TXT_LineEndKind_Null;
      if(lane_idx() == 0)
      {
        U64 total_cr_count = 0;
        for EachIndex(l_idx, lane_count())
        {
          total_cr_count += lane_cr_count[l_idx];
        }
        if(total_cr_count > shared->info.lines_count / 4 && total_cr_count > 0)
        {
          line_end_kind = TXT_LineEndKind_CRLF;
        }
        else if(shared->info.lines_count > 1)
        {
          line_end_kind = TXT_LineEndKind_LF;
        }
        shared->info.line_end_kind = line_end_kind;
      }
      lane_sync();
    }
    
    //- rjf: find all token endpoint candidates across all lanes
    //
    // note that these are not necessarily actually token-forming sequences of
    // characters. we're effectively just building an acceleration structure to
    // quickly process the buffer - to skip between sequences of *plausible*
    // token markers. but, for example, if a `//` showed up inside of two `"`s,
    // then there would not be a comment token emitted. we just do this (rather
    // than going through the buffer sequentially) so that we can do this gather
    // step wide, and for the serially-dependent tokenization state machine part,
    // we can do that over much less data.
    //
    typedef struct TokenEndpointCandidateChunkNode TokenEndpointCandidateChunkNode;
    struct TokenEndpointCandidateChunkNode
    {
      TokenEndpointCandidateChunkNode *next;
      U64 *v;
      U64 count;
      U64 cap;
    };
    TokenEndpointCandidateChunkNode **lanes_first_token_endpoint_candidate_chunks = 0;
    TokenEndpointCandidateChunkNode **lanes_last_token_endpoint_candidate_chunks = 0;
    if(lang != TXT_LangKind_Null)
    {
      if(lane_idx() == 0)
      {
        lanes_first_token_endpoint_candidate_chunks = push_array(scratch.arena, TokenEndpointCandidateChunkNode *, lane_count());
        lanes_last_token_endpoint_candidate_chunks = push_array(scratch.arena, TokenEndpointCandidateChunkNode *, lane_count());
      }
      lane_sync_u64(&lanes_first_token_endpoint_candidate_chunks, 0);
      lane_sync_u64(&lanes_last_token_endpoint_candidate_chunks, 0);
      TokenEndpointCandidateChunkNode *first_chunk = 0;
      TokenEndpointCandidateChunkNode *last_chunk = 0;
      Rng1U64 range = lane_range(data.size);
      if(lang == TXT_LangKind_Rust)
      {
        // rad: Rust does not use the candidates (see the Rust scan below)
        range = r1u64(0, 0);
      }
      for EachInRange(off, range)
      {
        U8 byte = data.str[off];
        U8 next_byte = (off+1 < data.size) ? data.str[off+1] : 0;
        B32 off_is_endpoint = 1;
        U64 extra_advance = 0;
        if(byte == '/' && next_byte == '*')
        {
          extra_advance = 1;
        }
        else if(byte == '*' && next_byte == '/')
        {
          extra_advance = 1;
        }
        else if(byte == 'R' && next_byte == '"')
        {
          extra_advance = 1;
        }
        else if(byte == '/' && next_byte == '/')
        {
          extra_advance = 1;
        }
        else if(byte == '#')
        {
          // NOTE(rjf): no-op
        }
        else if(byte == '"' || byte == '\'')
        {
          B32 is_escaped = 0;
          if(off > 0)
          {
            for(U64 lookback_off = off-1; lookback_off > 0; lookback_off -= 1)
            {
              if(data.str[lookback_off] == '\\')
              {
                is_escaped ^= 1;
              }
              else
              {
                break;
              }
            }
          }
          if(is_escaped)
          {
            off_is_endpoint = 0;
          }
        }
        else
        {
          off_is_endpoint = 0;
        }
        if(off_is_endpoint)
        {
          TokenEndpointCandidateChunkNode *chunk = last_chunk;
          if(chunk == 0 || chunk->count >= chunk->cap)
          {
            chunk = push_array(scratch.arena, TokenEndpointCandidateChunkNode, 1);
            SLLQueuePush(first_chunk, last_chunk, chunk);
            chunk->cap = 512;
            chunk->v = push_array(scratch.arena, U64, chunk->cap);
          }
          chunk->v[chunk->count] = off;
          chunk->count += 1;
        }
        off += extra_advance;
      }
      lanes_first_token_endpoint_candidate_chunks[lane_idx()] = first_chunk;
      lanes_last_token_endpoint_candidate_chunks[lane_idx()] = last_chunk;
      lane_sync();
    }
    
    //- rjf: join all token endpoint candidates from all lanes
    TokenEndpointCandidateChunkNode *first_token_endpoint_candidate_chunk = 0;
    TokenEndpointCandidateChunkNode *last_token_endpoint_candidate_chunk = 0;
    if(lang != TXT_LangKind_Null && lane_idx() == 0)
    {
      for EachIndex(l_idx, lane_count())
      {
        if(last_token_endpoint_candidate_chunk == 0)
        {
          first_token_endpoint_candidate_chunk = lanes_first_token_endpoint_candidate_chunks[l_idx];
          last_token_endpoint_candidate_chunk = lanes_last_token_endpoint_candidate_chunks[l_idx];
        }
        else
        {
          last_token_endpoint_candidate_chunk->next = lanes_first_token_endpoint_candidate_chunks[l_idx];
          last_token_endpoint_candidate_chunk = lanes_last_token_endpoint_candidate_chunks[l_idx];
        }
      }
    }
    lane_sync_u64(&first_token_endpoint_candidate_chunk, 0);
    lane_sync_u64(&last_token_endpoint_candidate_chunk, 0);
    
    //- rjf: scan sequence of token endpoint candidates - find big token ranges.
    typedef struct TokenPtChunkNode TokenPtChunkNode;
    struct TokenPtChunkNode
    {
      TokenPtChunkNode *next;
      TXT_TokenPt *v;
      U64 count;
      U64 cap;
    };
    TokenPtChunkNode *first_token_pt_chunk = 0;
    TokenPtChunkNode *last_token_pt_chunk = 0;
    U64 total_token_pt_count = 0;

    //- rad: Rust: one byte scan with the shared big-token helpers (the
    // candidate scan below has C rules: `'` always opens a char, `#` a
    // preprocessor line, block comments do not nest)
    if(lang == TXT_LangKind_Rust && lane_idx() == 0)
    {
      for(U64 off = 0; off < data.size;)
      {
        U64 starter_size = 0;
        TXT_TokenPt start_pt = txt_rust_big_token_from_off(data, off, &starter_size);
        if(start_pt.kind == TXT_TokenKind_Null)
        {
          off += 1;
          continue;
        }
        TXT_TokenPt pt = start_pt;
        U64 end_off = off + starter_size;
        B32 ended = 0;
        for(;end_off < data.size && !ended;)
        {
          end_off += txt_rust_big_token_advance(data, end_off, &pt, &ended);
        }
        TXT_TokenPt pts[] =
        {
          start_pt,
          {TXT_TokenKind_Null, end_off},
        };
        for EachElement(pt_idx, pts)
        {
          TokenPtChunkNode *chunk = last_token_pt_chunk;
          if(chunk == 0 || chunk->count >= chunk->cap)
          {
            chunk = push_array(scratch.arena, TokenPtChunkNode, 1);
            SLLQueuePush(first_token_pt_chunk, last_token_pt_chunk, chunk);
            chunk->cap = 512;
            chunk->v = push_array(scratch.arena, TXT_TokenPt, chunk->cap);
          }
          chunk->v[chunk->count] = pts[pt_idx];
          chunk->count += 1;
          total_token_pt_count += 1;
        }
        off = end_off;
      }
    }
    else if(lang != TXT_LangKind_Null && lane_idx() == 0)
    {
      TXT_TokenKind active_token_kind = TXT_TokenKind_Null;
      U64 active_token_start_off = 0;
      String8 herestring_marker = {0};
      U64 cand_chunk_idx = 0;
      for(TokenEndpointCandidateChunkNode *cand_chunk_n = first_token_endpoint_candidate_chunk; cand_chunk_n != 0;)
      {
        U64 off = cand_chunk_n->v[cand_chunk_idx];
        U8 byte = data.str[off];
        U8 next_byte = (off+1 < data.size) ? data.str[off+1] : 0;
        
        //- rjf: no active token kind -> look for token starter
        TXT_TokenKind start_active_token_kind = active_token_kind;
        if(active_token_kind == TXT_TokenKind_Null)
        {
          // rjf: " -> start a string literal
          if(byte == '"')
          {
            active_token_kind = TXT_TokenKind_String;
            herestring_marker.size = 0;
          }
          
          // rjf: ' -> start a char literal
          else if(byte == '\'')
          {
            active_token_kind = TXT_TokenKind_Char;
            herestring_marker.size = 0;
          }
          
          // rjf: R" -> start a C++11+ style herestring
          else if(byte == 'R' && next_byte == '"')
          {
            active_token_kind = TXT_TokenKind_String;
            U64 next_paren_pos = str8_find_needle(str8_prefix(data, off+2+256), off+2, s("("), 0);
            herestring_marker = str8_substr(data, r1u64(off+2, next_paren_pos));
          }
          
          // rjf: // -> start a single-line comment
          else if(byte == '/' && next_byte == '/')
          {
            active_token_kind = TXT_TokenKind_LineComment;
          }
          
          // rjf: /* -> start a multi-line comment
          else if(byte == '/' && next_byte == '*')
          {
            active_token_kind = TXT_TokenKind_BlockComment;
          }
          
          // rjf: # -> start a meta
          else if(byte == '#')
          {
            active_token_kind = TXT_TokenKind_Meta;
          }
          
          // rjf: got a token kind -> remember its starting offset
          if(active_token_kind != TXT_TokenKind_Null)
          {
            active_token_start_off = off;
          }
        }
        
        //- rjf: end single-line comments & meta by binary searching for the line range -
        // skip all lines that end with an escaped newline
        B32 line_advance = 0;
        U64 active_token_end_off = 0;
        if(start_active_token_kind == TXT_TokenKind_Null && (active_token_kind == TXT_TokenKind_LineComment ||
                                                             active_token_kind == TXT_TokenKind_Meta))
        {
          TXT_LineMap line_map = {0};
          txt_line_map_push(scratch.arena, &line_map, r1u64(1, shared->info.lines_count), shared->info.lines_ranges, 0);
          U64 line_num = txt_line_num_from_off(&line_map, off);
          Rng1U64 line_range = txt_range_from_line_num(&line_map, line_num);
          for(;line_num <= shared->info.lines_count;)
          {
            if(str8_match(s("\\"), str8_substr(data, r1u64(line_range.max-1, line_range.max)), 0))
            {
              line_num += 1;
              line_range = txt_range_from_line_num(&line_map, line_num);
            }
            else
            {
              break;
            }
          }
          active_token_end_off = line_range.max;
          line_advance = 1;
        }
        
        //- rjf: try to end all other cases by looking at subsequent endpoint candidates
        if(start_active_token_kind == active_token_kind && active_token_kind != TXT_TokenKind_Null)
        {
          switch(active_token_kind)
          {
            default:{}break;
            case TXT_TokenKind_String:
            {
              if(byte == 'R' && next_byte == '"')
              {
                active_token_end_off = off+2;
              }
              else if(herestring_marker.size == 0 && byte == '"')
              {
                active_token_end_off = off+1;
              }
              else if(herestring_marker.size != 0 && byte == '"')
              {
                String8 paren_maybe = str8_substr(data, r1u64(off - herestring_marker.size - 1, off - herestring_marker.size));
                String8 herestring_marker_maybe = str8_substr(data, r1u64(off - herestring_marker.size, off));
                if(str8_match(paren_maybe, s(")"), 0) && str8_match(herestring_marker, herestring_marker_maybe, 0))
                {
                  active_token_end_off = off+1;
                }
              }
            }break;
            case TXT_TokenKind_Char:
            {
              if(byte == '\'')
              {
                active_token_end_off = off+1;
              }
            }break;
            case TXT_TokenKind_BlockComment:
            {
              if(byte == '*' && next_byte == '/')
              {
                active_token_end_off = off+2;
              }
            }break;
          }
        }
        
        //- rjf: end all cases with an end-of-buffer
        B32 next_is_end_of_buffer = (cand_chunk_idx >= cand_chunk_n->count && cand_chunk_n->next == 0);
        if(next_is_end_of_buffer)
        {
          active_token_end_off = off+1;
        }
        
        //- rjf: finish the active token if we can
        if(active_token_end_off > active_token_start_off)
        {
          TXT_TokenPt pts[] =
          {
            {active_token_kind, active_token_start_off},
            {TXT_TokenKind_Null, active_token_end_off},
          };
          for EachElement(pt_idx, pts)
          {
            TokenPtChunkNode *chunk = last_token_pt_chunk;
            if(chunk == 0 || chunk->count >= chunk->cap)
            {
              chunk = push_array(scratch.arena, TokenPtChunkNode, 1);
              SLLQueuePush(first_token_pt_chunk, last_token_pt_chunk, chunk);
              chunk->cap = 512;
              chunk->v = push_array(scratch.arena, TXT_TokenPt, chunk->cap);
            }
            chunk->v[chunk->count] = pts[pt_idx];
            chunk->count += 1;
            total_token_pt_count += 1;
            active_token_kind = TXT_TokenKind_Null;
          }
        }
        
        //- rjf: advance across many token candidates until we find the new line
        if(line_advance)
        {
          B32 found = 0;
          U64 scan_cand_chunk_idx = cand_chunk_idx;
          for(TokenEndpointCandidateChunkNode *n = cand_chunk_n; n != 0; n = n->next)
          {
            for(U64 n_idx = scan_cand_chunk_idx; n_idx < n->count; n_idx += 1)
            {
              if(n->v[n_idx] >= active_token_end_off)
              {
                found = 1;
                cand_chunk_n = n;
                cand_chunk_idx = n_idx;
                goto dbl_break_find_candidate_in_next_line;
              }
            }
            scan_cand_chunk_idx = 0;
          }
          dbl_break_find_candidate_in_next_line:;
          if(!found)
          {
            cand_chunk_n = 0;
            cand_chunk_idx = 0;
          }
        }
        
        //- rjf: advance by token candidate
        else
        {
          cand_chunk_idx += 1;
          if(cand_chunk_idx >= cand_chunk_n->count)
          {
            cand_chunk_n = cand_chunk_n->next;
            cand_chunk_idx = 0;
          }
        }
      }
    }
    lane_sync_u64(&first_token_pt_chunk, 0);
    lane_sync_u64(&last_token_pt_chunk, 0);
    lane_sync_u64(&total_token_pt_count, 0);
    
    //- rjf: form final token pt buffer
    if(lane_idx() == 0)
    {
      shared->info.lang = (U32)lang;
      shared->info.big_token_pts_count = total_token_pt_count;
      shared->info.big_token_pts = push_array(shared->arena, TXT_TokenPt, shared->info.big_token_pts_count);
    }
    lane_sync();
    {
      U64 base_idx = 0;
      for EachNode(n, TokenPtChunkNode, first_token_pt_chunk)
      {
        Rng1U64 range = lane_range(n->count);
        MemoryCopy(shared->info.big_token_pts + base_idx + range.min, n->v + range.min, sizeof(n->v[0]) * dim_1u64(range));
        base_idx += n->count;
      }
    }
    lane_sync();
    
    //- rjf: lex function * data -> tokens
#if 1
    if(lane_idx() == 0 && lex_function != 0)
    {
      shared->info.tokens = lex_function(shared->arena, 0, data);
    }
#else
    if(lane_idx() == 0)
    {
      shared->info.tokens = txt_token_array_from_lang_kind_string(shared->arena, lang, data);
    }
#endif
    lane_sync();
    set_progress(Min(data.size, 1024) + data.size + data.size + data.size*(lex_function != 0));
    TXT_TokenArray tokens = shared->info.tokens;
    
    //- rjf: count scope points
    {
      U64 lane_scope_pt_opener_count = 0;
      U64 lane_scope_pt_count = 0;
      Rng1U64 range = lane_range(tokens.count);
      for EachInRange(idx, range)
      {
        if(tokens.v[idx].kind == TXT_TokenKind_Symbol)
        {
          String8 token_string = str8_substr(data, tokens.v[idx].range);
          B32 is_opener = (token_string.str[0] == '{' ||
                           token_string.str[0] == '(' ||
                           token_string.str[0] == '[');
          B32 is_closer = (token_string.str[0] == '}' ||
                           token_string.str[0] == ')' ||
                           token_string.str[0] == ']');
          if(token_string.size == 1 && (is_opener || is_closer))
          {
            lane_scope_pt_count += 1;
            lane_scope_pt_opener_count += !!is_opener;
          }
        }
      }
      ins_atomic_u64_add_eval(&shared->info.scope_pts.count, lane_scope_pt_count);
      ins_atomic_u64_add_eval(&shared->info.scope_nodes.count, lane_scope_pt_opener_count);
    }
    lane_sync();
    
    //- rjf: allocate & fill scope data
    if(lane_idx() == 0)
    {
      shared->info.scope_pts.v = push_array_no_zero(shared->arena, TXT_ScopePt, shared->info.scope_pts.count);
      shared->info.scope_nodes.v = push_array_no_zero(shared->arena, TXT_ScopeNode, shared->info.scope_nodes.count);
      {
        typedef struct ScopeTask ScopeTask;
        struct ScopeTask
        {
          ScopeTask *next;
          U64 scope_idx;
        };
        Temp scratch = scratch_begin(0, 0);
        ScopeTask *top_scope_task = 0;
        ScopeTask *free_scope_task = 0;
        U64 pt_idx = 0;
        U64 scope_idx = 0;
        for EachIndex(token_idx, tokens.count)
        {
          if(token_idx%1000 == 0 && ins_atomic_u32_eval(cancel_signal))
          {
            break;
          }
          if(tokens.v[token_idx].kind == TXT_TokenKind_Symbol)
          {
            String8 token_string = str8_substr(data, tokens.v[token_idx].range);
            B32 is_opener = (token_string.str[0] == '{' ||
                             token_string.str[0] == '(' ||
                             token_string.str[0] == '[');
            B32 is_closer = (token_string.str[0] == '}' ||
                             token_string.str[0] == ')' ||
                             token_string.str[0] == ']');
            
            // rjf: opener symbols -> push scope
            if(is_opener)
            {
              // rjf: insert into scope tree
              TXT_ScopeNode *new_scope = &shared->info.scope_nodes.v[scope_idx];
              new_scope->token_idx_range.min = token_idx;
              if(top_scope_task)
              {
                U64 new_scope_num = scope_idx+1;
                TXT_ScopeNode *parent = &shared->info.scope_nodes.v[top_scope_task->scope_idx];
                if(parent->first_num == 0)
                {
                  parent->first_num = new_scope_num;
                }
                if(parent->last_num != 0)
                {
                  TXT_ScopeNode *prev_scope = &shared->info.scope_nodes.v[parent->last_num-1];
                  prev_scope->next_num = new_scope_num;
                }
                parent->last_num = new_scope_num;
                new_scope->parent_num = top_scope_task->scope_idx+1;
              }
              
              // rjf: push onto scope stack
              ScopeTask *scope_task = free_scope_task;
              if(scope_task)
              {
                SLLStackPop(free_scope_task);
              }
              else
              {
                scope_task = push_array(scratch.arena, ScopeTask, 1);
              }
              scope_task->scope_idx = scope_idx;
              scope_idx += 1;
              SLLStackPush(top_scope_task, scope_task);
            }
            
            // rjf: opener or closer -> fill endpoint
            if(top_scope_task && (is_opener || is_closer))
            {
              shared->info.scope_pts.v[pt_idx].token_idx = token_idx;
              shared->info.scope_pts.v[pt_idx].scope_num = top_scope_task->scope_idx+1;
              pt_idx += 1;
            }
            
            // rjf: closer symbols -> pop
            if(is_closer && top_scope_task != 0)
            {
              ScopeTask *popped = top_scope_task;
              shared->info.scope_nodes.v[popped->scope_idx].token_idx_range.max = token_idx;
              SLLStackPop(top_scope_task);
              SLLStackPush(free_scope_task, popped);
            }
          }
        }
        scratch_end(scratch);
      }
    }
    lane_sync();
  }
  
  //- rjf: cancel -> release
  if(lane_idx() == 0 && ins_atomic_u32_eval(cancel_signal) && shared->arena != 0)
  {
    arena_release(shared->arena);
    shared->arena = 0;
  }
  
  //- rjf: mark dependency on hash
  if(lane_idx() == 0 && shared->arena != 0)
  {
    c_hash_downstream_inc(hash);
  }
  
  //- rjf: package as artifact
  if(lane_idx() == 0 && shared->arena != 0)
  {
    shared->artifact = push_array(shared->arena, TXT_Artifact, 1);
    shared->artifact->arena     = shared->arena;
    shared->artifact->data_hash = hash;
    shared->artifact->info      = shared->info;
  }
  lane_sync();
  AC_Artifact result = {0};
  result.u64[0] = (U64)shared->artifact;
  lane_sync();
  
  access_close(access);
  scratch_end(scratch);
  ProfEnd();
  return result;
}

internal void
txt_artifact_destroy(AC_Artifact artifact)
{
  TXT_Artifact *txt_artifact = (TXT_Artifact *)artifact.u64[0];
  if(txt_artifact == 0) { return; }
  c_hash_downstream_dec(txt_artifact->data_hash);
  arena_release(txt_artifact->arena);
}

internal TXT_TextInfo
txt_text_info_from_hash_lang(Access *access, U128 hash, TXT_LangKind lang)
{
#pragma pack(push, 1)
  struct
  {
    U128 hash;
    TXT_LangKind lang;
  } key = {hash, lang};
#pragma pack(pop)
  String8 key_string = str8_struct(&key);
  B32 stale = 0;
  AC_Artifact artifact = ac_artifact_from_key(access, key_string, txt_artifact_create, txt_artifact_destroy, 0, .flags = AC_Flag_Wide, .stale_out = &stale);
  TXT_Artifact *txt_artifact = (TXT_Artifact *)artifact.u64[0];
  TXT_TextInfo info = {0};
  if(txt_artifact != 0)
  {
    info = txt_artifact->info;
  }
  else if(!stale)
  {
    info = txt_info_nil;
  }
  return info;
}

internal TXT_TextInfo
txt_text_info_from_key_lang(Access *access, C_Key key, TXT_LangKind lang, U128 *hash_out)
{
  TXT_TextInfo result = {0};
  for(U64 rewind_idx = 0; rewind_idx < C_KEY_HASH_HISTORY_COUNT; rewind_idx += 1)
  {
    U128 hash = c_hash_from_key(key, rewind_idx);
    if(!u128_match(hash, u128_zero()))
    {
      result = txt_text_info_from_hash_lang(access, hash, lang);
    }
    if(result.lines_count != 0)
    {
      if(hash_out)
      {
        *hash_out = hash;
      }
      break;
    }
  }
  return result;
}
