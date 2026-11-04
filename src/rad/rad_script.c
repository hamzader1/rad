// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

////////////////////////////////
//~ rad: Script Results

internal void
rad_script_resultf(char *fmt, ...)
{
  Temp scratch = scratch_begin(0, 0);
  va_list args;
  va_start(args, fmt);
  String8 string = push_str8fv(scratch.arena, fmt, args);
  va_end(args);
  append_data_to_file_path(rad_script->out_path, push_str8f(scratch.arena, "%S\n", string));
  scratch_end(scratch);
}

internal void
rad_script_error(RAD_ScriptLine *line, String8 message)
{
  rad_script->error_count += 1;
  rad_script_resultf("ERROR %I64u: %S", line ? line->line_num : 0, message);
}

internal void
rad_script_finish(void)
{
  if(!rad_script->done)
  {
    if(rad_script->measuring)
    {
      rad_script_error(0, push_str8f(rad_frame_arena(), "measure \"%S\" was never ended", rad_script->measure_name));
    }
    rad_script_resultf("DONE pass %I64u fail %I64u error %I64u", rad_script->pass_count, rad_script->fail_count, rad_script->error_count);
    rad_script->done = 1;
    rad_state->quit = 1;

    // NOTE(rad): the script runs before this frame's event wait; without a
    // frame request, that wait would block until some OS message arrives
    // (forever, for a window in the background), and the frame that returns
    // `quit` would never end.
    rad_request_frame();
  }
}

////////////////////////////////
//~ rad: Script Parsing

internal RAD_ScriptLine
rad_script_line_from_text(Arena *arena, U64 line_num, String8 text)
{
  // NOTE(rad): whitespace-separated args; "double quoted" args may contain
  // spaces and the escapes \n \t \" \\.
  String8List args = {0};
  for(U64 idx = 0; idx < text.size;)
  {
    if(char_is_space(text.str[idx]))
    {
      idx += 1;
    }
    else if(text.str[idx] == '"')
    {
      U8 *buffer = push_array(arena, U8, text.size);
      U64 size = 0;
      for(idx += 1; idx < text.size && text.str[idx] != '"'; idx += 1)
      {
        U8 c = text.str[idx];
        if(c == '\\' && idx+1 < text.size)
        {
          idx += 1;
          c = text.str[idx];
          c = (c == 'n' ? '\n' : c == 't' ? '\t' : c);
        }
        buffer[size] = c;
        size += 1;
      }
      idx += 1;
      str8_list_push(arena, &args, str8(buffer, size));
    }
    else
    {
      U64 start = idx;
      for(;idx < text.size && !char_is_space(text.str[idx]); idx += 1);
      str8_list_push(arena, &args, str8_substr(text, r1u64(start, idx)));
    }
  }
  RAD_ScriptLine line = {0};
  line.line_num = line_num;
  line.text = text;
  line.args_count = args.node_count;
  line.args = push_array(arena, String8, line.args_count);
  {
    U64 idx = 0;
    for(String8Node *n = args.first; n != 0; n = n->next, idx += 1)
    {
      line.args[idx] = n->string;
    }
  }
  return line;
}

internal B32
rad_script_chord_from_string(String8 string, WM_Modifiers *modifiers_out, WM_Key *key_out)
{
  U8 split_char = '+';
  Temp scratch = scratch_begin(0, 0);
  String8List parts = str8_split(scratch.arena, string, &split_char, 1, 0);
  WM_Modifiers modifiers = 0;
  WM_Key key = WM_Key_Null;
  for(String8Node *n = parts.first; n != 0; n = n->next)
  {
    B32 is_last = (n->next == 0);
    if(!is_last && str8_match(n->string, str8_lit("ctrl"), StringMatchFlag_CaseInsensitive))       { modifiers |= WM_Modifier_Ctrl; }
    else if(!is_last && str8_match(n->string, str8_lit("shift"), StringMatchFlag_CaseInsensitive)) { modifiers |= WM_Modifier_Shift; }
    else if(!is_last && str8_match(n->string, str8_lit("alt"), StringMatchFlag_CaseInsensitive))   { modifiers |= WM_Modifier_Alt; }
    else for EachEnumVal(WM_Key, k)
    {
      if(k != WM_Key_Null && str8_match(n->string, wm_key_cfg_name_table[k], StringMatchFlag_CaseInsensitive))
      {
        key = k;
        break;
      }
    }
  }
  scratch_end(scratch);
  *modifiers_out = modifiers;
  *key_out = key;
  return (key != WM_Key_Null);
}

internal String8
rad_script_path(Arena *arena, String8 path)
{
  String8 result = path_absolute_dst_from_relative_dst_src(arena, path, rad_script->folder);
  result = path_normalized_from_string(arena, result);
  return result;
}

////////////////////////////////
//~ rad: Script State Queries

internal CFG_PanelTree
rad_script_panel_tree(Arena *arena)
{
  CFG_PanelTree tree = {&cfg_nil_panel_node, &cfg_nil_panel_node, &cfg_nil_panel_node};
  if(rad_state->first_window != 0)
  {
    tree = cfg_panel_tree_from_cfg(arena, cfg_node_from_id(rad_state->first_window->cfg_id));
  }
  return tree;
}

internal CFG_Node *
rad_script_editor_tab(void)
{
  // NOTE(rad): the focused panel's selected tab if it is a text tab, else the
  // last focused editor panel's selected tab
  Temp scratch = scratch_begin(0, 0);
  CFG_PanelTree tree = rad_script_panel_tree(scratch.arena);
  CFG_Node *tab = tree.focused->selected_tab;
  if(!str8_match(tab->string, str8_lit("text"), 0))
  {
    CFG_PanelNode *panel = cfg_panel_node_from_tree_cfg(tree.root, cfg_node_from_id(rad_state->last_editor_panel));
    tab = panel->selected_tab;
  }
  if(!str8_match(tab->string, str8_lit("text"), 0))
  {
    tab = &cfg_nil_node;
  }
  scratch_end(scratch);
  return tab;
}

internal CFG_Node *
rad_script_explorer_tab(void)
{
  Temp scratch = scratch_begin(0, 0);
  CFG_PanelTree tree = rad_script_panel_tree(scratch.arena);
  CFG_Node *result = &cfg_nil_node;
  for(CFG_PanelNode *p = tree.root; p != &cfg_nil_panel_node && result == &cfg_nil_node; p = cfg_panel_node_rec__depth_first_pre(tree.root, p).next)
  {
    for(CFG_NodePtrNode *n = p->tabs.first; n != 0; n = n->next)
    {
      if(str8_match(n->v->string, str8_lit("explorer"), 0))
      {
        result = n->v;
        break;
      }
    }
  }
  scratch_end(scratch);
  return result;
}

internal String8
rad_script_tab_name(CFG_Node *tab)
{
  String8 result = tab->string;
  String8 file_path = rad_file_path_from_tab(tab);
  if(file_path.size != 0)
  {
    result = str8_skip_last_slash(file_path);
  }
  else if(str8_match(tab->string, str8_lit("explorer"), 0))
  {
    String8 folder = rad_explorer_folder_from_tab(tab);
    result = (folder.size != 0 ? str8_skip_last_slash(folder) : str8_lit("Explorer"));
  }
  return result;
}

internal B32
rad_script_editor_text(Arena *arena, RAD_CodeViewState **cv_out, RAD_Document **doc_out, TB_Text *text_out, String8 *error_out)
{
  B32 result = 0;
  CFG_Node *tab = rad_script_editor_tab();
  RAD_Document *doc = rad_document_from_tab(tab);
  if(tab == &cfg_nil_node)
  {
    *error_out = str8_lit("no editor view");
  }
  else
  {
    RAD_CodeViewState *cv = &rad_view_state_from_cfg(tab)->cv;
    if(!cv->initialized || doc == 0 || !tb_buffer_is_loaded(doc->buffer))
    {
      *error_out = str8_lit("editor text not loaded");
    }
    else
    {
      Access *access = access_open();
      *text_out = tb_text_from_buffer(arena, access, doc->buffer);
      access_close(access);
      *cv_out = cv;
      *doc_out = doc;
      result = (text_out->base_info.lines_count != 0);
      if(!result)
      {
        *error_out = str8_lit("editor text not loaded");
      }
    }
  }
  return result;
}

internal String8
rad_script_line_text(Arena *arena, TB_Text *text, S64 line_num)
{
  Rng1U64 range = txt_range_from_line_num(&text->patched.line_map, (U64)line_num);
  String8 result = memory_map_data_from_range(arena, &text->patched.memory_map, range);
  return result;
}

internal CFG_Node *
rad_script_window_tab(String8 kind)
{
  CFG_Node *result = &cfg_nil_node;
  if(rad_state->first_window != 0)
  {
    result = rad_tab_from_window_kind(cfg_node_from_id(rad_state->first_window->cfg_id), kind);
  }
  return result;
}

internal String8
rad_script_diag_string(Arena *arena, RAD_Diagnostic *diag)
{
  // NOTE(rad): "<kind> <path relative to the workspace root>:<line>:<col> <message>"
  Temp scratch = scratch_begin(&arena, 1);
  String8 path = rad_problem_display_path(scratch.arena, diag);
  String8 result = push_str8f(arena, "%S %S:%I64d:%I64d %S", rad_diagnostic_kind_name(diag->kind), path, diag->line, diag->column, diag->message);
  scratch_end(scratch);
  return result;
}

internal B32
rad_script_compare(String8 actual, String8 op, String8 expected, B32 *pass_out)
{
  // NOTE(rad): returns 0 for an unknown operator (or an ordering of non-numbers)
  S64 a = 0;
  S64 e = 0;
  B32 numeric = (try_s64_from_str8_c_rules(actual, &a) && try_s64_from_str8_c_rules(expected, &e));
  B32 op_valid = 1;
  B32 pass = 0;
  if(str8_match(op, str8_lit("=="), 0))      { pass = numeric ? (a == e) : str8_match(actual, expected, 0); }
  else if(str8_match(op, str8_lit("!="), 0)) { pass = numeric ? (a != e) : !str8_match(actual, expected, 0); }
  else if(numeric && str8_match(op, str8_lit("<"), 0))  { pass = (a <  e); }
  else if(numeric && str8_match(op, str8_lit("<="), 0)) { pass = (a <= e); }
  else if(numeric && str8_match(op, str8_lit(">"), 0))  { pass = (a >  e); }
  else if(numeric && str8_match(op, str8_lit(">="), 0)) { pass = (a >= e); }
  else { op_valid = 0; }
  *pass_out = pass;
  return op_valid;
}

internal B32
rad_script_field(Arena *arena, String8 *args, U64 args_count, String8 *value_out, String8 *error_out)
{
  B32 result = 1;
  String8 name = (args_count > 0 ? args[0] : str8_zero());
  String8 value = {0};
  Temp scratch = scratch_begin(&arena, 1);
  CFG_PanelTree tree = rad_script_panel_tree(scratch.arena);

  //- rad: focused panel
  if(str8_match(name, str8_lit("panel"), 0))
  {
    value = push_str8_copy(arena, tree.focused->selected_tab->string);
  }
  else if(str8_match(name, str8_lit("tab"), 0))
  {
    value = push_str8_copy(arena, rad_script_tab_name(tree.focused->selected_tab));
  }
  else if(str8_match(name, str8_lit("tab_title"), 0))
  {
    // rad: title text as the tab shows it, without icons ("<src> main.rs")
    CFG_Node *tab = tree.focused->selected_tab;
    String8 file_path = rad_file_path_from_tab(tab);
    String8List parts = {0};
    String8List qualifiers = rad_qualifiers_from_file_path(arena, file_path);
    for(String8Node *n = qualifiers.first; n != 0; n = n->next)
    {
      str8_list_pushf(arena, &parts, "<%S> ", n->string);
    }
    str8_list_push(arena, &parts, rad_script_tab_name(tab));
    value = str8_list_join(arena, &parts, 0);
  }
  else if(str8_match(name, str8_lit("loading"), 0))
  {
    // rad: the editor tab's loading overlay strength (0 = not shown)
    CFG_Node *tab = rad_script_editor_tab();
    value = push_str8f(arena, "%.2f", (tab == &cfg_nil_node) ? 0.f : rad_view_state_from_cfg(tab)->loading_t);
  }
  else if(str8_match(name, str8_lit("menu_bar_focused"), 0))
  {
    // rad: 1 if the menu bar has the keyboard (Alt tapped)
    value = push_str8f(arena, "%i", (int)(rad_state->first_window != 0 && rad_state->first_window->menu_bar_focused));
  }
  else if(str8_match(name, str8_lit("zoomed"), 0))
  {
    // rad: 1 if a panel is zoomed (fills the window)
    value = push_str8f(arena, "%i", (int)(tree.zoom_root != &cfg_nil_panel_node));
  }
  else if(str8_match(name, str8_lit("tab_count"), 0))
  {
    value = push_str8f(arena, "%I64u", tree.focused->tabs.count);
  }
  else if(str8_match(name, str8_lit("panel_count"), 0) || str8_match(name, str8_lit("explorer_panel_tabs"), 0))
  {
    U64 leaf_count = 0;
    U64 explorer_panel_tabs = 0;
    for(CFG_PanelNode *p = tree.root; p != &cfg_nil_panel_node; p = cfg_panel_node_rec__depth_first_pre(tree.root, p).next)
    {
      leaf_count += (p->first == &cfg_nil_panel_node);
      if(rad_explorer_tab_from_panel(p->cfg) != &cfg_nil_node)
      {
        explorer_panel_tabs = p->tabs.count;
      }
    }
    value = push_str8f(arena, "%I64u", str8_match(name, str8_lit("panel_count"), 0) ? leaf_count : explorer_panel_tabs);
  }

  //- rad: editor
  else if(str8_match(name, str8_lit("cursor_line"), 0) || str8_match(name, str8_lit("cursor_col"), 0) ||
          str8_match(name, str8_lit("mark_line"), 0) || str8_match(name, str8_lit("mark_col"), 0) ||
          str8_match(name, str8_lit("line_count"), 0) || str8_match(name, str8_lit("line"), 0) ||
          str8_match(name, str8_lit("token"), 0) ||
          str8_match(name, str8_lit("visible_first"), 0) || str8_match(name, str8_lit("visible_last"), 0) ||
          str8_match(name, str8_lit("dirty"), 0) || str8_match(name, str8_lit("cursor_line_text"), 0))
  {
    RAD_CodeViewState *cv = 0;
    RAD_Document *doc = 0;
    TB_Text text = {0};
    result = rad_script_editor_text(scratch.arena, &cv, &doc, &text, error_out);
    if(result)
    {
      // rad: the view maps its cursor when it is next built; map a copy now
      U64 off = (str8_match(name, str8_lit("mark_line"), 0) || str8_match(name, str8_lit("mark_col"), 0)) ? cv->mark : cv->cursor;
      if(!tb_buffer_map_offset(doc->buffer, cv->buffer_version, &off))
      {
        off = Min(off, text.patched.size);
      }
      U64 line_num = txt_line_num_from_off(&text.patched.line_map, off);
      Rng1U64 line_range = txt_range_from_line_num(&text.patched.line_map, line_num);
      if(str8_match(name, str8_lit("cursor_line"), 0) || str8_match(name, str8_lit("mark_line"), 0))
      {
        value = push_str8f(arena, "%I64u", line_num);
      }
      else if(str8_match(name, str8_lit("cursor_line_text"), 0))
      {
        value = memory_map_data_from_range(arena, &text.patched.memory_map, line_range);
      }
      else if(str8_match(name, str8_lit("cursor_col"), 0) || str8_match(name, str8_lit("mark_col"), 0))
      {
        value = push_str8f(arena, "%I64u", off - line_range.min + 1);
      }
      else if(str8_match(name, str8_lit("line_count"), 0))
      {
        value = push_str8f(arena, "%I64u", text.patched.line_map.total_line_count);
      }
      else if(str8_match(name, str8_lit("visible_first"), 0))
      {
        value = push_str8f(arena, "%I64d", cv->layout_visible_lines.min);
      }
      else if(str8_match(name, str8_lit("visible_last"), 0))
      {
        value = push_str8f(arena, "%I64d", cv->layout_visible_lines.max);
      }
      else if(str8_match(name, str8_lit("dirty"), 0))
      {
        value = push_str8f(arena, "%i", !!tb_buffer_is_dirty(doc->buffer));
      }
      else if(str8_match(name, str8_lit("token"), 0))
      {
        // rad: kind of the token at <line> <col>, lexed as the code view does
        // it: from the line's token pt context (strings/comments spanning
        // lines), with the visible-range tokenizer
        S64 n = 0;
        S64 col = 0;
        if(args_count < 3 || !try_s64_from_str8_c_rules(args[1], &n) || !try_s64_from_str8_c_rules(args[2], &col) ||
           n < 1 || (U64)n > text.patched.line_map.total_line_count || col < 1)
        {
          *error_out = push_str8f(arena, "token: needs <line> <col>, line in 1..%I64u", text.patched.line_map.total_line_count);
          result = 0;
        }
        else
        {
          Rng1U64 range = txt_range_from_line_num(&text.patched.line_map, (U64)n);
          U64 ctx_num = txt_token_pt_num_from_off(&text.patched.token_pt_map, range.min);
          TXT_TokenPt ctx = txt_token_pt_from_num(&text.patched.token_pt_map, ctx_num);
          String8 line_data = memory_map_data_from_range(scratch.arena, &text.patched.memory_map, range);
          TXT_TokenArray tokens = txt_token_array_from_data(scratch.arena, doc->lang, ctx, line_data, range.min, max_U64);
          U64 off = range.min + (U64)col - 1;
          String8 kind_names[] =
          {
            str8_lit_comp("null"), str8_lit_comp("error"), str8_lit_comp("whitespace"), str8_lit_comp("keyword"),
            str8_lit_comp("identifier"), str8_lit_comp("numeric"), str8_lit_comp("string"), str8_lit_comp("char"),
            str8_lit_comp("symbol"), str8_lit_comp("line_comment"), str8_lit_comp("block_comment"), str8_lit_comp("meta"),
          };
          value = str8_lit("none");
          for EachIndex(idx, tokens.count)
          {
            if(tokens.v[idx].range.min <= off && off < tokens.v[idx].range.max && tokens.v[idx].kind < ArrayCount(kind_names))
            {
              value = kind_names[tokens.v[idx].kind];
              break;
            }
          }
        }
      }
      else
      {
        S64 n = 0;
        if(args_count < 2 || !try_s64_from_str8_c_rules(args[1], &n) || n < 1 || (U64)n > text.patched.line_map.total_line_count)
        {
          *error_out = push_str8f(arena, "line: needs a line number in 1..%I64u", text.patched.line_map.total_line_count);
          result = 0;
        }
        else
        {
          value = rad_script_line_text(arena, &text, n);
        }
      }
    }
  }

  //- rad: files on disk
  else if(str8_match(name, str8_lit("file"), 0) || str8_match(name, str8_lit("file_size"), 0))
  {
    if(args_count < 2)
    {
      *error_out = push_str8f(arena, "%S: needs a path", name);
      result = 0;
    }
    else if(str8_match(name, str8_lit("file"), 0))
    {
      value = data_from_file_path(arena, rad_script_path(scratch.arena, args[1]));
    }
    else
    {
      value = push_str8f(arena, "%I64u", properties_from_file_path(rad_script_path(scratch.arena, args[1])).size);
    }
  }

  //- rad: explorer
  else if(str8_match(name, str8_lit("explorer_root"), 0) || str8_match(name, str8_lit("explorer_rows"), 0) ||
          str8_match(name, str8_lit("explorer_selected"), 0) || str8_match(name, str8_lit("explorer_row"), 0))
  {
    CFG_Node *tab = rad_script_explorer_tab();
    RAD_ExplorerState *ex = (tab != &cfg_nil_node ? &rad_view_state_from_cfg(tab)->ex : 0);
    if(ex == 0)
    {
      *error_out = str8_lit("no explorer");
      result = 0;
    }
    else if(str8_match(name, str8_lit("explorer_root"), 0))
    {
      value = push_str8_copy(arena, str8_skip_last_slash(rad_explorer_folder_from_tab(tab)));
    }
    else if(str8_match(name, str8_lit("explorer_rows"), 0))
    {
      value = push_str8f(arena, "%I64u", ex->rows_count);
    }
    else if(str8_match(name, str8_lit("explorer_selected"), 0))
    {
      if(1 <= ex->cursor.y && (U64)ex->cursor.y <= ex->rows_count)
      {
        value = push_str8_copy(arena, ex->rows[ex->cursor.y-1].name);
      }
    }
    else
    {
      S64 n = 0;
      if(args_count < 2 || !try_s64_from_str8_c_rules(args[1], &n) || n < 1 || (U64)n > ex->rows_count)
      {
        *error_out = push_str8f(arena, "explorer_row: needs a row number in 1..%I64u", ex->rows_count);
        result = 0;
      }
      else
      {
        value = push_str8_copy(arena, ex->rows[n-1].name);
      }
    }
  }

  //- rad: the editor tab's query bar
  else if(str8_match(name, str8_lit("query_open"), 0) || str8_match(name, str8_lit("query_input"), 0))
  {
    CFG_Node *tab = rad_script_editor_tab();
    if(tab == &cfg_nil_node)
    {
      *error_out = str8_lit("no editor view");
      result = 0;
    }
    else if(str8_match(name, str8_lit("query_open"), 0))
    {
      value = push_str8f(arena, "%i", !!rad_view_state_from_cfg(tab)->query_is_open);
    }
    else
    {
      value = push_str8_copy(arena, rad_tab_query_input(tab));
    }
  }

  //- rad: the editor's selected text (between mark and cursor)
  else if(str8_match(name, str8_lit("selection"), 0))
  {
    RAD_CodeViewState *cv = 0;
    RAD_Document *doc = 0;
    TB_Text text = {0};
    result = rad_script_editor_text(scratch.arena, &cv, &doc, &text, error_out);
    if(result)
    {
      U64 cursor = cv->cursor;
      U64 mark = cv->mark;
      if(!tb_buffer_map_offset(doc->buffer, cv->buffer_version, &cursor) ||
         !tb_buffer_map_offset(doc->buffer, cv->buffer_version, &mark))
      {
        cursor = Min(cursor, text.patched.size);
        mark = Min(mark, text.patched.size);
      }
      value = memory_map_data_from_range(arena, &text.patched.memory_map, r1u64(Min(cursor, mark), Max(cursor, mark)));
    }
  }

  //- rad: open documents (one per file, shared by its tabs)
  else if(str8_match(name, str8_lit("doc_count"), 0))
  {
    U64 count = 0;
    for(RAD_Document *doc = rad_state->first_document; doc != 0; doc = doc->next)
    {
      count += 1;
    }
    value = push_str8f(arena, "%I64u", count);
  }

  else if(str8_match(name, str8_lit("last_sleep_ms"), 0))
  {
    value = push_str8f(arena, "%I64u", rad_script->last_sleep_us/1000);
  }

  //- rad: time since the script started
  else if(str8_match(name, str8_lit("elapsed_ms"), 0))
  {
    value = push_str8f(arena, "%I64u", (now_time_us() - rad_script->start_us)/1000);
  }

  //- rad: bottom bar error
  else if(str8_match(name, str8_lit("error"), 0))
  {
    if(rad_state->error_t >= 0.01f && rad_state->error_string_size != 0)
    {
      value = push_str8_copy(arena, str8(rad_state->error_buffer, rad_state->error_string_size));
    }
  }

  //- rad: hover tooltip
  else if(str8_match(name, str8_lit("lsp_servers"), 0))
  {
    value = push_str8f(arena, "%I64u", rad_lsp->servers_started);
  }
  else if(str8_match(name, str8_lit("hover_text"), 0))
  {
    value = (rad_lsp->hover.answered ? rad_lsp->hover.text : str8_zero());
  }

  //- rad: the completion list (shown: built in the last frame)
  else if(str8_match(name, str8_lit("completion_open"), 0) || str8_match(name, str8_lit("completion_count"), 0) ||
          str8_match(name, str8_lit("completion_selected"), 0) || str8_match(name, str8_lit("completion_row"), 0) ||
          str8_match(name, str8_lit("completion_has"), 0) || str8_match(name, str8_lit("completion_cursor"), 0) ||
          str8_match(name, str8_lit("completion_filter"), 0) || str8_match(name, str8_lit("completion_top"), 0) ||
          str8_match(name, str8_lit("completion_bottom"), 0) || str8_match(name, str8_lit("completion_below"), 0) ||
          str8_match(name, str8_lit("completion_selected_visible"), 0))
  {
    RAD_LspCompletion *c = &rad_lsp->completion;
    B32 shown = (c->active && c->rows_count != 0 && c->built_frame+1 >= rad_state->frame_index);
    U64 rows_count = (c->active ? c->rows_count : 0);
    if(str8_match(name, str8_lit("completion_open"), 0))
    {
      value = (shown ? str8_lit("1") : str8_lit("0"));
    }
    else if(str8_match(name, str8_lit("completion_top"), 0) || str8_match(name, str8_lit("completion_bottom"), 0) ||
            str8_match(name, str8_lit("completion_below"), 0) || str8_match(name, str8_lit("completion_selected_visible"), 0))
    {
      // rad: first/last row fully inside the list, from the last frame's layout
      S64 top = 0, bottom = 0;
      F32 row_height = dim_2f32(c->layout_first_row_rect).y;
      if(shown && row_height > 0)
      {
        for(S64 n = c->layout_first_row+1; n <= (S64)rows_count; n += 1)
        {
          F32 row_y0 = c->layout_first_row_rect.y0 + (F32)((n-1) - c->layout_first_row)*row_height;
          if(row_y0 + row_height > c->layout_rect.y1 + 0.5f) { break; }
          if(row_y0 >= c->layout_rect.y0 - 0.5f)
          {
            if(top == 0) { top = n; }
            bottom = n;
          }
        }
      }
      S64 v = (str8_match(name, str8_lit("completion_top"), 0) ? top :
               str8_match(name, str8_lit("completion_bottom"), 0) ? bottom :
               str8_match(name, str8_lit("completion_below"), 0) ? (bottom != 0 ? (S64)rows_count - bottom : 0) :
               (top != 0 && top <= c->cursor.y && c->cursor.y <= bottom));
      value = push_str8f(arena, "%I64d", v);
    }
    else if(str8_match(name, str8_lit("completion_count"), 0))
    {
      value = push_str8f(arena, "%I64u", rows_count);
    }
    else if(str8_match(name, str8_lit("completion_cursor"), 0))
    {
      value = push_str8f(arena, "%I64d", (rows_count != 0 ? c->cursor.y : 0));
    }
    else if(str8_match(name, str8_lit("completion_filter"), 0))
    {
      value = (c->active ? c->filter : str8_zero());
    }
    else if(str8_match(name, str8_lit("completion_selected"), 0))
    {
      if(1 <= c->cursor.y && (U64)c->cursor.y <= rows_count)
      {
        value = c->items[c->rows[c->cursor.y-1]].label;
      }
    }
    else if(str8_match(name, str8_lit("completion_row"), 0))
    {
      S64 n = 0;
      if(args_count < 2 || !try_s64_from_str8_c_rules(args[1], &n) || n < 1)
      {
        *error_out = str8_lit("completion_row: needs a row number");
        result = 0;
      }
      else if((U64)n <= rows_count)
      {
        value = c->items[c->rows[n-1]].label;
      }
    }
    else
    {
      value = str8_lit("0");
      for EachIndex(idx, rows_count)
      {
        if(args_count >= 2 && str8_match(c->items[c->rows[idx]].label, args[1], 0))
        {
          value = str8_lit("1");
          break;
        }
      }
    }
  }
  else if(str8_match(name, str8_lit("hover_has"), 0))
  {
    String8 hover = (rad_lsp->hover.answered ? rad_lsp->hover.text : str8_zero());
    value = ((args_count >= 2 && str8_find_needle(hover, 0, args[1], 0) < hover.size) ? str8_lit("1") : str8_lit("0"));
  }

  //- rad: the editor file's language server
  else if(str8_match(name, str8_lit("lsp_state"), 0) || str8_match(name, str8_lit("lsp_busy"), 0) ||
          str8_match(name, str8_lit("lsp_status"), 0))
  {
    CFG_Node *tab = rad_script_editor_tab();
    RAD_Document *doc = rad_document_from_tab(tab);
    RAD_LspServer *s = (doc != 0 ? rad_lsp_server_from_path(doc->path) : 0);
    B32 busy = 0;
    String8 status = rad_lsp_status_string(arena, &busy);
    if(str8_match(name, str8_lit("lsp_state"), 0))
    {
      value = (s == 0 ? str8_lit("none") :
               s->state == RAD_LspServerState_Starting ? str8_lit("starting") :
               s->state == RAD_LspServerState_Ready ? str8_lit("ready") :
               str8_lit("ended"));
    }
    else if(str8_match(name, str8_lit("lsp_busy"), 0))
    {
      value = (busy ? str8_lit("1") : str8_lit("0"));
    }
    else
    {
      value = status;
    }
  }

  //- rad: cargo configuration
  else if(str8_match(name, str8_lit("cargo_profile"), 0))
  {
    value = (rad_cargo_release() ? str8_lit("release") : str8_lit("debug"));
  }

  //- rad: cargo command state
  else if(str8_match(name, str8_lit("cargo_status"), 0) || str8_match(name, str8_lit("cargo_exit_code"), 0) ||
          str8_match(name, str8_lit("cargo_cmd"), 0))
  {
    RAD_CargoState *c = rad_cargo;
    B32 stopped = 0;
    B32 launch_failed = 0;
    U64 exit_code = 0;
    MutexScope(c->mutex)
    {
      stopped = c->stop_requested;
      launch_failed = c->launch_failed;
      exit_code = c->exit_code;
    }
    if(str8_match(name, str8_lit("cargo_exit_code"), 0))
    {
      value = push_str8f(arena, "%I64u", exit_code);
    }
    else if(str8_match(name, str8_lit("cargo_cmd"), 0))
    {
      value = (c->run != 0 ? rad_cargo_cmd_display_name(c->run->kind) : str8_zero());
    }
    else
    {
      value = (c->run == 0             ? str8_lit("none") :
               rad_cargo_is_running()  ? str8_lit("running") :
               stopped                 ? str8_lit("stopped") :
               launch_failed           ? str8_lit("launch_failed") :
               exit_code != 0          ? str8_lit("failed") :
               str8_lit("finished"));
    }
  }

  //- rad: diagnostics of the last cargo command
  else if(str8_match(name, str8_lit("diag_count"), 0))
  {
    value = push_str8f(arena, "%I64u", rad_problems->count);
  }
  else if(str8_match(name, str8_lit("diag_errors"), 0))
  {
    value = push_str8f(arena, "%I64u", rad_problems->error_count);
  }
  else if(str8_match(name, str8_lit("diag_warnings"), 0))
  {
    value = push_str8f(arena, "%I64u", rad_problems->warning_count);
  }
  else if(str8_match(name, str8_lit("diag_selected"), 0))
  {
    value = push_str8f(arena, "%I64u", rad_problems->cursor);
  }
  else if(str8_match(name, str8_lit("diag"), 0))
  {
    S64 n = 0;
    if(args_count < 2 || !try_s64_from_str8_c_rules(args[1], &n) || n < 1 || (U64)n > rad_problems->count)
    {
      *error_out = push_str8f(arena, "diag: needs a number in 1..%I64u", rad_problems->count);
      result = 0;
    }
    else
    {
      value = rad_script_diag_string(arena, rad_problems->diags[n-1]);
    }
  }

  //- rad: the cargo output text (as written so far; the Output tab shows it
  // up to 100 ms later)
  else if(str8_match(name, str8_lit("output_lines"), 0) || str8_match(name, str8_lit("output_line"), 0) ||
          str8_match(name, str8_lit("output_has"), 0))
  {
    String8 output = {0};
    MutexScope(rad_cargo->mutex)
    {
      output = push_str8_copy(scratch.arena, str8(rad_cargo->output_base, rad_cargo->output_size));
    }
    U8 split_char = '\n';
    String8List lines = str8_split(scratch.arena, output, &split_char, 1, StringSplitFlag_KeepEmpties);
    if(str8_match(name, str8_lit("output_has"), 0))
    {
      if(args_count < 2)
      {
        *error_out = str8_lit("output_has: needs a string");
        result = 0;
      }
      else
      {
        value = push_str8f(arena, "%i", str8_find_needle(output, 0, args[1], 0) < output.size);
      }
    }
    else if(str8_match(name, str8_lit("output_lines"), 0))
    {
      value = push_str8f(arena, "%I64u", lines.node_count);
    }
    else
    {
      S64 n = 0;
      if(args_count < 2 || !try_s64_from_str8_c_rules(args[1], &n) || n < 1 || (U64)n > lines.node_count)
      {
        *error_out = push_str8f(arena, "output_line: needs a line number in 1..%I64u", lines.node_count);
        result = 0;
      }
      else
      {
        String8Node *node = lines.first;
        for(S64 idx = 1; idx < n; idx += 1) { node = node->next; }
        String8 line = node->string;
        if(line.size != 0 && line.str[line.size-1] == '\r') { line.size -= 1; }
        value = push_str8_copy(arena, line);
      }
    }
  }

  //- rad: the Output tab's cursor line (follows the end while it is there)
  else if(str8_match(name, str8_lit("output_cursor_line"), 0) || str8_match(name, str8_lit("output_line_count"), 0) ||
          str8_match(name, str8_lit("output_cursor_at_end"), 0))
  {
    CFG_Node *tab = rad_script_window_tab(str8_lit("output"));
    if(tab == &cfg_nil_node)
    {
      *error_out = str8_lit("no output tab");
      result = 0;
    }
    else
    {
      Access *access = access_open();
      TB_Text text = tb_text_from_buffer(scratch.arena, access, rad_cargo->output_buffer);
      access_close(access);
      RAD_CodeViewState *cv = &rad_view_state_from_cfg(tab)->cv;
      U64 count = (text.base_info.lines_count != 0 ? text.patched.line_map.total_line_count : 0);
      U64 line = (count != 0 ? txt_line_num_from_off(&text.patched.line_map, Min(cv->cursor, text.patched.size)) : 0);
      value = push_str8f(arena, "%I64u", (str8_match(name, str8_lit("output_line_count"), 0) ? count :
                                          str8_match(name, str8_lit("output_cursor_at_end"), 0) ? (U64)(line == count) :
                                          line));
    }
  }

  //- rad: the diagnostic shown on an editor line ("error", "warning", ...,
  // or "" for none), where it is now (it follows edits)
  else if(str8_match(name, str8_lit("line_diag"), 0))
  {
    RAD_CodeViewState *cv = 0;
    RAD_Document *doc = 0;
    TB_Text text = {0};
    S64 n = 0;
    result = rad_script_editor_text(scratch.arena, &cv, &doc, &text, error_out);
    if(result && (args_count < 2 || !try_s64_from_str8_c_rules(args[1], &n)))
    {
      *error_out = str8_lit("line_diag: needs a line number");
      result = 0;
    }
    for(U64 idx = 0; result && idx < rad_problems->count; idx += 1)
    {
      RAD_Diagnostic *diag = rad_problems->diags[idx];
      if(str8_match(diag->path, doc->path, StringMatchFlag_CaseInsensitive|StringMatchFlag_SlashInsensitive))
      {
        Rng1U64 range = rad_diagnostic_range_from_document(diag, doc, &text.patched);
        if((S64)txt_line_num_from_off(&text.patched.line_map, range.min) == n)
        {
          value = rad_diagnostic_kind_name(diag->kind);
          break;
        }
      }
    }
  }

  else
  {
    *error_out = push_str8f(arena, "unknown field \"%S\"", name);
    result = 0;
  }

  scratch_end(scratch);
  *value_out = value;
  return result;
}

////////////////////////////////
//~ rad: Script Targets

internal B32
rad_script_target(Arena *arena, String8 *args, U64 args_count, U64 *args_used_out, Vec2F32 *pos_out, String8 *error_out)
{
  B32 result = 0;
  Vec2F32 pos = {0};
  U64 args_used = 0;
  String8 kind = (args_count > 0 ? args[0] : str8_zero());
  Temp scratch = scratch_begin(&arena, 1);

  //- rad: editor targets
  if(str8_match(kind, str8_lit("code"), 0) || str8_match(kind, str8_lit("code_center"), 0))
  {
    RAD_CodeViewState *cv = 0;
    RAD_Document *doc = 0;
    TB_Text text = {0};
    if(rad_script_editor_text(scratch.arena, &cv, &doc, &text, error_out))
    {
      if(str8_match(kind, str8_lit("code_center"), 0))
      {
        pos = center_2f32(cv->layout_code_rect);
        args_used = 1;
        result = 1;
      }
      else
      {
        S64 line_num = 0;
        S64 col = 0;
        if(args_count < 3 || !try_s64_from_str8_c_rules(args[1], &line_num) || !try_s64_from_str8_c_rules(args[2], &col) || col < 1)
        {
          *error_out = str8_lit("code: needs <line> <col>");
        }
        else if(line_num < cv->layout_visible_lines.min || cv->layout_visible_lines.max < line_num)
        {
          *error_out = push_str8f(arena, "code: line %I64d is not visible (visible: %I64d-%I64d)", line_num, cv->layout_visible_lines.min, cv->layout_visible_lines.max);
        }
        else
        {
          String8 line_text = rad_script_line_text(scratch.arena, &text, line_num);
          String8 prefix = str8_prefix(line_text, (U64)col-1);
          F32 advance = fnt_dim_from_tag_size_string(cv->layout_font, cv->layout_font_size, 0, cv->layout_tab_size, prefix).x;
          pos.x = cv->layout_text_rect.x0 + cv->layout_text_x_off + advance + 1.f;
          pos.y = cv->layout_text_rect.y0 + ((F32)(line_num - cv->layout_visible_lines.min) + 0.5f)*cv->layout_line_height;
          args_used = 3;
          result = 1;
        }
      }
    }
  }

  //- rad: the code view of a shown tab in any panel, by its name
  else if(str8_match(kind, str8_lit("tab_code"), 0))
  {
    if(args_count < 2)
    {
      *error_out = str8_lit("tab_code: needs a tab name");
    }
    else
    {
      CFG_PanelTree tree = rad_script_panel_tree(scratch.arena);
      for(CFG_PanelNode *p = tree.root; p != &cfg_nil_panel_node && !result; p = cfg_panel_node_rec__depth_first_pre(tree.root, p).next)
      {
        CFG_Node *tab = p->selected_tab;
        if(tab != &cfg_nil_node && str8_match(rad_script_tab_name(tab), args[1], 0))
        {
          RAD_CodeViewState *cv = &rad_view_state_from_cfg(tab)->cv;
          if(cv->initialized && cv->layout_code_rect.x1 > cv->layout_code_rect.x0)
          {
            pos = center_2f32(cv->layout_code_rect);
            args_used = 2;
            result = 1;
          }
        }
      }
      if(!result)
      {
        *error_out = push_str8f(arena, "tab_code: no shown code view on a tab named \"%S\"", args[1]);
      }
    }
  }

  //- rad: explorer targets
  else if(str8_match(kind, str8_lit("explorer"), 0) || str8_match(kind, str8_lit("explorer_expander"), 0) || str8_match(kind, str8_lit("explorer_center"), 0))
  {
    CFG_Node *tab = rad_script_explorer_tab();
    RAD_ExplorerState *ex = (tab != &cfg_nil_node ? &rad_view_state_from_cfg(tab)->ex : 0);
    if(ex == 0)
    {
      *error_out = str8_lit("no explorer");
    }
    else if(str8_match(kind, str8_lit("explorer_center"), 0))
    {
      pos = center_2f32(ex->layout_rect);
      args_used = 1;
      result = 1;
    }
    else if(args_count < 2)
    {
      *error_out = push_str8f(arena, "%S: needs a row name", kind);
    }
    else
    {
      S64 row_idx = -1;
      for EachIndex(idx, ex->rows_count)
      {
        if(str8_match(ex->rows[idx].name, args[1], 0))
        {
          row_idx = (S64)idx;
          break;
        }
      }
      F32 row_y0 = ex->layout_first_row_rect.y0 + (F32)(row_idx - ex->layout_first_row)*ex->layout_row_height;
      if(row_idx < 0)
      {
        *error_out = push_str8f(arena, "%S: no row named \"%S\"", kind, args[1]);
      }
      else if(row_y0 < ex->layout_rect.y0 || ex->layout_rect.y1 < row_y0 + ex->layout_row_height)
      {
        *error_out = push_str8f(arena, "%S: row \"%S\" is not visible", kind, args[1]);
      }
      else
      {
        // NOTE(rad): rad_cell layout: depth * 1em indent, then a 2em expander
        F32 expander_size_px = floor_f32(ex->layout_font_size*2.f);
        pos.x = (str8_match(kind, str8_lit("explorer_expander"), 0)
                 ? ex->layout_first_row_rect.x0 + ex->rows[row_idx].depth*ex->layout_font_size + expander_size_px/2
                 : center_2f32(ex->layout_first_row_rect).x);
        pos.y = row_y0 + ex->layout_row_height/2;
        args_used = 2;
        result = 1;
      }
    }
  }

  //- rad: Output tab: a visible text position
  else if(str8_match(kind, str8_lit("output"), 0))
  {
    CFG_Node *tab = rad_script_window_tab(str8_lit("output"));
    S64 line_num = 0;
    S64 col = 0;
    if(tab == &cfg_nil_node)
    {
      *error_out = str8_lit("output: no output tab");
    }
    else if(args_count < 3 || !try_s64_from_str8_c_rules(args[1], &line_num) || !try_s64_from_str8_c_rules(args[2], &col) || col < 1)
    {
      *error_out = str8_lit("output: needs <line> <col>");
    }
    else
    {
      RAD_CodeViewState *cv = &rad_view_state_from_cfg(tab)->cv;
      Access *access = access_open();
      TB_Text text = tb_text_from_buffer(scratch.arena, access, rad_cargo->output_buffer);
      access_close(access);
      if(text.base_info.lines_count == 0 || line_num < cv->layout_visible_lines.min || cv->layout_visible_lines.max < line_num)
      {
        *error_out = push_str8f(arena, "output: line %I64d is not visible (visible: %I64d-%I64d)", line_num, cv->layout_visible_lines.min, cv->layout_visible_lines.max);
      }
      else
      {
        String8 line_text = rad_script_line_text(scratch.arena, &text, line_num);
        String8 prefix = str8_prefix(line_text, (U64)col-1);
        F32 advance = fnt_dim_from_tag_size_string(cv->layout_font, cv->layout_font_size, 0, cv->layout_tab_size, prefix).x;
        pos.x = cv->layout_text_rect.x0 + cv->layout_text_x_off + advance + 1.f;
        pos.y = cv->layout_text_rect.y0 + ((F32)(line_num - cv->layout_visible_lines.min) + 0.5f)*cv->layout_line_height;
        args_used = 3;
        result = 1;
      }
    }
  }

  //- rad: Output tab: the first line containing a text, at the text's start
  else if(str8_match(kind, str8_lit("output_text"), 0))
  {
    CFG_Node *tab = rad_script_window_tab(str8_lit("output"));
    if(tab == &cfg_nil_node || args_count < 2 || args[1].size == 0)
    {
      *error_out = str8_lit("output_text: needs an output tab and \"<text>\"");
    }
    else
    {
      RAD_CodeViewState *cv = &rad_view_state_from_cfg(tab)->cv;
      Access *access = access_open();
      TB_Text text = tb_text_from_buffer(scratch.arena, access, rad_cargo->output_buffer);
      access_close(access);
      U64 line_count = (text.base_info.lines_count != 0 ? text.patched.line_map.total_line_count : 0);
      S64 found_line = 0;
      U64 found_col = 0;
      for(U64 line_num = 1; line_num <= line_count && found_line == 0; line_num += 1)
      {
        String8 line_text = rad_script_line_text(scratch.arena, &text, (S64)line_num);
        U64 pos = str8_find_needle(line_text, 0, args[1], 0);
        if(pos < line_text.size)
        {
          found_line = (S64)line_num;
          found_col = pos;
        }
      }
      if(found_line == 0)
      {
        *error_out = push_str8f(arena, "output_text: no line contains \"%S\"", args[1]);
      }
      else if(found_line < cv->layout_visible_lines.min || cv->layout_visible_lines.max < found_line)
      {
        *error_out = push_str8f(arena, "output_text: line %I64d is not visible (visible: %I64d-%I64d)", found_line, cv->layout_visible_lines.min, cv->layout_visible_lines.max);
      }
      else
      {
        String8 line_text = rad_script_line_text(scratch.arena, &text, found_line);
        F32 advance = fnt_dim_from_tag_size_string(cv->layout_font, cv->layout_font_size, 0, cv->layout_tab_size, str8_prefix(line_text, found_col)).x;
        pos.x = cv->layout_text_rect.x0 + cv->layout_text_x_off + advance + 1.f;
        pos.y = cv->layout_text_rect.y0 + ((F32)(found_line - cv->layout_visible_lines.min) + 0.5f)*cv->layout_line_height;
        args_used = 2;
        result = 1;
      }
    }
  }

  //- rad: top bar: a command button (by code name), or `configuration`
  // (the Debug/Release box); its menu's rows are cargo_debug, cargo_release
  else if(str8_match(kind, str8_lit("toolbar"), 0))
  {
    RAD_CmdKind cmd_kind = RAD_CmdKind_COUNT;
    if(args_count >= 2 && str8_match(args[1], str8_lit("configuration"), 0))
    {
      cmd_kind = RAD_CmdKind_Null;
    }
    else if(args_count >= 2)
    {
      cmd_kind = rad_cmd_kind_from_code_name(args[1]);
      cmd_kind = (cmd_kind == RAD_CmdKind_Null ? RAD_CmdKind_COUNT : cmd_kind);
    }
    Rng2F32 rect = {0};
    if(cmd_kind != RAD_CmdKind_COUNT && rad_state->first_window != 0)
    {
      rect = rad_state->first_window->top_bar_cmd_rects[cmd_kind];
    }
    if(rect.x1 <= rect.x0)
    {
      *error_out = str8_lit("toolbar: needs a button shown in the top bar (cargo_run ... cargo_stop, configuration, or a configuration menu row)");
    }
    else
    {
      pos = center_2f32(rect);
      args_used = 2;
      result = 1;
    }
  }

  //- rad: Problems tab: a visible row (1-based)
  else if(str8_match(kind, str8_lit("problem"), 0))
  {
    CFG_Node *tab = rad_script_window_tab(str8_lit("problems"));
    S64 n = 0;
    if(tab == &cfg_nil_node)
    {
      *error_out = str8_lit("problem: no problems tab");
    }
    else if(args_count < 2 || !try_s64_from_str8_c_rules(args[1], &n) || n < 1 || (U64)n > rad_problems->count)
    {
      *error_out = push_str8f(arena, "problem: needs a row number in 1..%I64u", rad_problems->count);
    }
    else
    {
      RAD_ProblemsState *pr = &rad_view_state_from_cfg(tab)->pr;
      F32 row_y0 = pr->layout_first_row_rect.y0 + (F32)((n-1) - pr->layout_first_row)*pr->layout_row_height;
      if(row_y0 < pr->layout_rect.y0 || pr->layout_rect.y1 < row_y0 + pr->layout_row_height)
      {
        *error_out = push_str8f(arena, "problem: row %I64d is not visible", n);
      }
      else
      {
        pos.x = center_2f32(pr->layout_first_row_rect).x;
        pos.y = row_y0 + pr->layout_row_height/2;
        args_used = 2;
        result = 1;
      }
    }
  }

  //- rad: the completion list as a whole (wheel target)
  else if(str8_match(kind, str8_lit("completion_list"), 0))
  {
    RAD_LspCompletion *c = &rad_lsp->completion;
    if(!c->active || c->rows_count == 0 || c->built_frame+1 < rad_state->frame_index)
    {
      *error_out = str8_lit("completion_list: no list shown");
    }
    else
    {
      pos = center_2f32(c->layout_rect);
      args_used = 1;
      result = 1;
    }
  }

  //- rad: the completion list's rows
  else if(str8_match(kind, str8_lit("completion"), 0))
  {
    RAD_LspCompletion *c = &rad_lsp->completion;
    S64 n = 0;
    if(!c->active || c->rows_count == 0 || c->built_frame+1 < rad_state->frame_index)
    {
      *error_out = str8_lit("completion: no list shown");
    }
    else if(args_count < 2 || !try_s64_from_str8_c_rules(args[1], &n) || n < 1 || (U64)n > c->rows_count)
    {
      *error_out = push_str8f(arena, "completion: needs a row number in 1..%I64u", c->rows_count);
    }
    else
    {
      F32 row_height = dim_2f32(c->layout_first_row_rect).y;
      F32 row_y0 = c->layout_first_row_rect.y0 + (F32)((n-1) - c->layout_first_row)*row_height;
      if(row_y0 < c->layout_rect.y0 || c->layout_rect.y1 < row_y0 + row_height)
      {
        *error_out = push_str8f(arena, "completion: row %I64d is not visible", n);
      }
      else
      {
        pos.x = center_2f32(c->layout_first_row_rect).x;
        pos.y = row_y0 + row_height/2;
        args_used = 2;
        result = 1;
      }
    }
  }

  //- rad: window pixels
  else
  {
    S64 x = 0;
    S64 y = 0;
    if(args_count >= 2 && try_s64_from_str8_c_rules(args[0], &x) && try_s64_from_str8_c_rules(args[1], &y))
    {
      pos = v2f32((F32)x, (F32)y);
      args_used = 2;
      result = 1;
    }
    else
    {
      *error_out = push_str8f(arena, "unknown target \"%S\"", kind);
    }
  }

  scratch_end(scratch);
  *pos_out = pos;
  *args_used_out = args_used;
  return result;
}

////////////////////////////////
//~ rad: Script Input

internal RAD_ScriptInput *
rad_script_push_input(RAD_ScriptInputKind kind, Vec2F32 pos)
{
  RAD_ScriptInput *input = push_array(rad_script->input_arena, RAD_ScriptInput, 1);
  input->kind = kind;
  input->pos = pos;
  SLLQueuePush(rad_script->first_input, rad_script->last_input, input);
  return input;
}

internal void
rad_script_inject(RAD_ScriptInput *input)
{
  WM_Window window = rad_state->first_window->os;
  wm_sim_set_modifiers(input->modifiers);
  switch(input->kind)
  {
    default:{}break;

    //- rad: key: press, the text Windows would send for it (WM_CHAR), release
    case RAD_ScriptInputKind_Key:
    {
      WM_Event *press = wm_sim_push_event(window, WM_EventKind_Press);
      press->key = input->key;
      press->modifiers = input->modifiers;
      if(!(input->modifiers & (WM_Modifier_Ctrl|WM_Modifier_Alt)))
      {
        U32 character = wm_codepoint_from_modifiers_and_key(input->modifiers, input->key);
        if(character >= 32 || character == '\n')
        {
          WM_Event *text = wm_sim_push_event(window, WM_EventKind_Text);
          text->character = character;
        }
      }
      WM_Event *release = wm_sim_push_event(window, WM_EventKind_Release);
      release->key = input->key;
      release->modifiers = input->modifiers;
    }break;

    //- rad: key held down / let go (a modifier key's own event carries no
    // modifier, as win32 sends it; while held, the modifier is down)
    case RAD_ScriptInputKind_KeyDown:
    case RAD_ScriptInputKind_KeyUp:
    {
      B32 is_down = (input->kind == RAD_ScriptInputKind_KeyDown);
      WM_Modifiers key_modifier = (input->key == WM_Key_Alt   ? WM_Modifier_Alt :
                                   input->key == WM_Key_Ctrl  ? WM_Modifier_Ctrl :
                                   input->key == WM_Key_Shift ? WM_Modifier_Shift : 0);
      WM_Event *evt = wm_sim_push_event(window, is_down ? WM_EventKind_Press : WM_EventKind_Release);
      evt->key = input->key;
      evt->modifiers = (input->modifiers & ~key_modifier);
      wm_sim_set_key_is_down(input->key, is_down);
      wm_sim_set_modifiers(is_down ? (input->modifiers | key_modifier) : (input->modifiers & ~key_modifier));
    }break;

    //- rad: typed character
    case RAD_ScriptInputKind_Text:
    {
      WM_Event *text = wm_sim_push_event(window, WM_EventKind_Text);
      text->character = input->character;
    }break;

    //- rad: mouse
    case RAD_ScriptInputKind_Wheel:
    case RAD_ScriptInputKind_MouseMove:
    case RAD_ScriptInputKind_MousePress:
    case RAD_ScriptInputKind_MouseRelease:
    {
      rad_script->mouse = input->pos;
      wm_sim_set_mouse(input->pos);
      WM_EventKind kind = (input->kind == RAD_ScriptInputKind_Wheel     ? WM_EventKind_Scroll :
                           input->kind == RAD_ScriptInputKind_MouseMove ? WM_EventKind_MouseMove :
                           input->kind == RAD_ScriptInputKind_MousePress ? WM_EventKind_Press :
                           WM_EventKind_Release);
      WM_Event *evt = wm_sim_push_event(window, kind);
      evt->pos = input->pos;
      evt->modifiers = input->modifiers;
      if(input->kind == RAD_ScriptInputKind_Wheel)
      {
        // NOTE(rad): same sign as win32 (WM_MOUSEWHEEL): +y scrolls down
        evt->delta = v2f32(0, input->wheel_delta);
      }
      if(input->kind == RAD_ScriptInputKind_MousePress || input->kind == RAD_ScriptInputKind_MouseRelease)
      {
        evt->key = WM_Key_LeftMouseButton;
        wm_sim_set_key_is_down(WM_Key_LeftMouseButton, input->kind == RAD_ScriptInputKind_MousePress);
      }
    }break;
  }
}

////////////////////////////////
//~ rad: Script Commands

internal void
rad_script_wait_frames(U64 count)
{
  rad_script->wait_kind = RAD_ScriptWaitKind_Frames;
  rad_script->wait_frames_left = count;
}

internal String8
rad_script_measure_stats(Arena *arena, U64 *samples, U64 count)
{
  String8 result = str8_lit("no samples");
  if(count != 0)
  {
    Temp scratch = scratch_begin(&arena, 1);
    U64 *sorted = push_array(scratch.arena, U64, count);
    MemoryCopy(sorted, samples, sizeof(U64)*count);
    for(U64 i = 1; i < count; i += 1)
    {
      U64 v = sorted[i];
      U64 j = i;
      for(;j > 0 && sorted[j-1] > v; j -= 1)
      {
        sorted[j] = sorted[j-1];
      }
      sorted[j] = v;
    }
    U64 sum = 0;
    for EachIndex(idx, count)
    {
      sum += sorted[idx];
    }
    result = push_str8f(arena, "avg %.2f ms, p50 %.2f ms, p95 %.2f ms, max %.2f ms (n %I64u)",
                        (F64)sum/count/1000.0,
                        sorted[count/2]/1000.0,
                        sorted[Min(count-1, (count*95)/100)]/1000.0,
                        sorted[count-1]/1000.0,
                        count);
    scratch_end(scratch);
  }
  return result;
}

internal void
rad_script_run_line(RAD_ScriptLine *line)
{
  Temp scratch = scratch_begin(0, 0);
  String8 cmd = line->args[0];
  String8 *args = line->args+1;
  U64 args_count = line->args_count-1;
  String8 error = {0};

  //- rad: rad commands
  if(str8_match(cmd, str8_lit("open"), 0) || str8_match(cmd, str8_lit("open_folder"), 0))
  {
    if(args_count < 1)
    {
      error = push_str8f(scratch.arena, "%S: needs a path", cmd);
    }
    else
    {
      RAD_CmdKind kind = (str8_match(cmd, str8_lit("open"), 0) ? RAD_CmdKind_OpenFile : RAD_CmdKind_OpenFolder);
      rad_cmd(kind, .file_path = rad_script_path(scratch.arena, args[0]));
      rad_script_wait_frames(RAD_SCRIPT_SETTLE_FRAMES);
    }
  }
  else if(str8_match(cmd, str8_lit("cmd"), 0))
  {
    RAD_CmdKind kind = RAD_CmdKind_Null;
    for EachNonZeroEnumVal(RAD_CmdKind, k)
    {
      if(args_count >= 1 && str8_match(rad_cmd_kind_info_table[k].code_name, args[0], 0))
      {
        kind = k;
        break;
      }
    }
    if(kind == RAD_CmdKind_Null)
    {
      error = push_str8f(scratch.arena, "cmd: unknown command \"%S\"", args_count >= 1 ? args[0] : str8_zero());
    }
    else
    {
      rad_cmd(kind);
      rad_script_wait_frames(RAD_SCRIPT_SETTLE_FRAMES);
    }
  }

  //- rad: keyboard input
  else if(str8_match(cmd, str8_lit("key"), 0))
  {
    WM_Modifiers modifiers = 0;
    WM_Key key = WM_Key_Null;
    U64 count = 1;
    if(args_count < 1 || !rad_script_chord_from_string(args[0], &modifiers, &key) ||
       (args_count >= 2 && !try_u64_from_str8_c_rules(args[1], &count)))
    {
      error = push_str8f(scratch.arena, "key: bad chord or count in \"%S\"", line->text);
    }
    else for EachIndex(idx, count)
    {
      RAD_ScriptInput *input = rad_script_push_input(RAD_ScriptInputKind_Key, rad_script->mouse);
      input->modifiers = modifiers;
      input->key = key;
    }
  }
  else if(str8_match(cmd, str8_lit("key_down"), 0) || str8_match(cmd, str8_lit("key_up"), 0))
  {
    WM_Modifiers modifiers = 0;
    WM_Key key = WM_Key_Null;
    if(args_count < 1 || !rad_script_chord_from_string(args[0], &modifiers, &key))
    {
      error = push_str8f(scratch.arena, "%S: bad key in \"%S\"", cmd, line->text);
    }
    else
    {
      RAD_ScriptInputKind kind = (str8_match(cmd, str8_lit("key_down"), 0) ? RAD_ScriptInputKind_KeyDown : RAD_ScriptInputKind_KeyUp);
      RAD_ScriptInput *input = rad_script_push_input(kind, rad_script->mouse);
      input->modifiers = modifiers;
      input->key = key;
    }
  }
  else if(str8_match(cmd, str8_lit("text"), 0))
  {
    if(args_count < 1)
    {
      error = str8_lit("text: needs a string");
    }
    else
    {
      String32 text32 = str32_from_8(scratch.arena, args[0]);
      for EachIndex(idx, text32.size)
      {
        RAD_ScriptInput *input = rad_script_push_input(RAD_ScriptInputKind_Text, rad_script->mouse);
        input->character = text32.str[idx];
      }
    }
  }

  //- rad: mouse input
  else if(str8_match(cmd, str8_lit("wheel"), 0))
  {
    S64 notches = 0;
    U64 used = 0;
    Vec2F32 pos = {0};
    if(args_count < 2 || !try_s64_from_str8_c_rules(args[0], &notches))
    {
      error = str8_lit("wheel: needs <notches> <target>");
    }
    else if(rad_script_target(scratch.arena, args+1, args_count-1, &used, &pos, &error))
    {
      rad_script_push_input(RAD_ScriptInputKind_MouseMove, pos);
      for EachIndex(idx, (U64)abs_s64(notches))
      {
        RAD_ScriptInput *input = rad_script_push_input(RAD_ScriptInputKind_Wheel, pos);
        input->wheel_delta = (notches > 0 ? +120.f : -120.f);
      }
    }
  }
  else if(str8_match(cmd, str8_lit("mouse"), 0) || str8_match(cmd, str8_lit("click"), 0) || str8_match(cmd, str8_lit("double_click"), 0) ||
          str8_match(cmd, str8_lit("ctrl_click"), 0))
  {
    U64 used = 0;
    Vec2F32 pos = {0};
    if(rad_script_target(scratch.arena, args, args_count, &used, &pos, &error))
    {
      rad_script_push_input(RAD_ScriptInputKind_MouseMove, pos);
      U64 click_count = (str8_match(cmd, str8_lit("mouse"), 0) ? 0 : str8_match(cmd, str8_lit("double_click"), 0) ? 2 : 1);
      WM_Modifiers modifiers = (str8_match(cmd, str8_lit("ctrl_click"), 0) ? WM_Modifier_Ctrl : 0);
      for EachIndex(idx, click_count)
      {
        rad_script_push_input(RAD_ScriptInputKind_MousePress, pos)->modifiers = modifiers;
        rad_script_push_input(RAD_ScriptInputKind_MouseRelease, pos)->modifiers = modifiers;
      }
    }
  }
  else if(str8_match(cmd, str8_lit("drag"), 0))
  {
    U64 used0 = 0;
    U64 used1 = 0;
    Vec2F32 pos0 = {0};
    Vec2F32 pos1 = {0};
    if(rad_script_target(scratch.arena, args, args_count, &used0, &pos0, &error) &&
       rad_script_target(scratch.arena, args+used0, args_count-used0, &used1, &pos1, &error))
    {
      rad_script_push_input(RAD_ScriptInputKind_MouseMove, pos0);
      rad_script_push_input(RAD_ScriptInputKind_MousePress, pos0);
      rad_script_push_input(RAD_ScriptInputKind_MouseMove, mix_2f32(pos0, pos1, 0.5f));
      rad_script_push_input(RAD_ScriptInputKind_MouseMove, pos1);
      rad_script_push_input(RAD_ScriptInputKind_MouseRelease, pos1);
    }
  }

  //- rad: changing files on disk (as another program would)
  else if(str8_match(cmd, str8_lit("write_file"), 0))
  {
    if(args_count < 2 || !write_data_to_file_path(rad_script_path(scratch.arena, args[0]), args[1]))
    {
      error = str8_lit("write_file: needs <path> \"<content>\" (or the write failed)");
    }
  }
  else if(str8_match(cmd, str8_lit("delete_file"), 0))
  {
    if(args_count < 1 || !delete_file_at_path(rad_script_path(scratch.arena, args[0])))
    {
      error = str8_lit("delete_file: needs <path> (or the delete failed)");
    }
  }

  else if(str8_match(cmd, str8_lit("write_file_later"), 0))
  {
    U64 ms = 0;
    if(args_count < 3 || !try_u64_from_str8_c_rules(args[0], &ms))
    {
      error = str8_lit("write_file_later: needs <ms> <path> \"<content>\"");
    }
    else
    {
      RAD_ScriptDelayedWrite *w = push_array(rad_script->arena, RAD_ScriptDelayedWrite, 1);
      w->delay_ms = ms;
      w->path = rad_script_path(rad_script->arena, args[1]);
      w->data = push_str8_copy(rad_script->arena, args[2]);
      thread_launch(rad_script_delayed_write_entry_point, w);
    }
  }
  else if(str8_match(cmd, str8_lit("sleep_until_woken"), 0))
  {
    rad_script->wait_kind = RAD_ScriptWaitKind_Woken;
    rad_script->sleep_begin_us = now_time_us();
  }

  //- rad: waiting
  else if(str8_match(cmd, str8_lit("wait"), 0))
  {
    U64 ms = 0;
    if(args_count < 1 || !try_u64_from_str8_c_rules(args[0], &ms))
    {
      error = str8_lit("wait: needs milliseconds");
    }
    else
    {
      rad_script->wait_kind = RAD_ScriptWaitKind_Time;
      rad_script->wait_until_us = now_time_us() + ms*1000;
    }
  }
  else if(str8_match(cmd, str8_lit("wait_frames"), 0))
  {
    U64 count = 0;
    if(args_count < 1 || !try_u64_from_str8_c_rules(args[0], &count))
    {
      error = str8_lit("wait_frames: needs a count");
    }
    else
    {
      rad_script_wait_frames(count);
    }
  }
  else if(str8_match(cmd, str8_lit("wait_cargo"), 0))
  {
    U64 ms = 120000;
    if(args_count >= 1 && !try_u64_from_str8_c_rules(args[0], &ms))
    {
      error = str8_lit("wait_cargo: bad timeout");
    }
    else
    {
      rad_script->wait_kind = RAD_ScriptWaitKind_Cargo;
      rad_script->wait_line_num = line->line_num;
      rad_script->wait_until_us = now_time_us() + ms*1000;
    }
  }
  else if(str8_match(cmd, str8_lit("wait_until"), 0))
  {
    U64 ms = 0;
    if(args_count < 4 || !try_u64_from_str8_c_rules(args[0], &ms))
    {
      error = str8_lit("wait_until: needs <timeout_ms> <field> <op> <value>");
    }
    else
    {
      rad_script->wait_kind = RAD_ScriptWaitKind_Until;
      rad_script->wait_line_num = line->line_num;
      rad_script->wait_until_us = now_time_us() + ms*1000;
      rad_script->wait_line = line;
    }
  }
  else if(str8_match(cmd, str8_lit("lsp"), 0))
  {
    if(args_count == 1 && (str8_match(args[0], str8_lit("on"), 0) || str8_match(args[0], str8_lit("off"), 0)))
    {
      rad_lsp->disabled = str8_match(args[0], str8_lit("off"), 0);
    }
    else
    {
      error = str8_lit("lsp: needs on or off");
    }
  }
  else if(str8_match(cmd, str8_lit("terminal"), 0))
  {
    if(args_count == 1 && (str8_match(args[0], str8_lit("visible"), 0) || str8_match(args[0], str8_lit("hidden"), 0)))
    {
      rad_script->terminal_visible = str8_match(args[0], str8_lit("visible"), 0);
    }
    else
    {
      error = str8_lit("terminal: needs visible or hidden");
    }
  }
  else if(str8_match(cmd, str8_lit("wait_output"), 0))
  {
    U64 ms = 120000;
    if(args_count < 1 || args[0].size == 0 || args[0].size > sizeof(rad_script->wait_text_buffer) ||
       (args_count >= 2 && !try_u64_from_str8_c_rules(args[1], &ms)))
    {
      error = str8_lit("wait_output: needs \"<text>\" [timeout_ms]");
    }
    else
    {
      rad_script->wait_kind = RAD_ScriptWaitKind_Output;
      rad_script->wait_line_num = line->line_num;
      rad_script->wait_until_us = now_time_us() + ms*1000;
      rad_script->wait_text_size = args[0].size;
      MemoryCopy(rad_script->wait_text_buffer, args[0].str, args[0].size);
    }
  }
  else if(str8_match(cmd, str8_lit("wait_idle"), 0))
  {
    U64 ms = RAD_SCRIPT_DEFAULT_TIMEOUT_US/1000;
    if(args_count >= 1 && !try_u64_from_str8_c_rules(args[0], &ms))
    {
      error = str8_lit("wait_idle: bad timeout");
    }
    else
    {
      rad_script->wait_kind = RAD_ScriptWaitKind_Idle;
      rad_script->wait_line_num = line->line_num;
      rad_script->wait_until_us = now_time_us() + ms*1000;
    }
  }

  //- rad: checks
  else if(str8_match(cmd, str8_lit("expect"), 0) || str8_match(cmd, str8_lit("print"), 0))
  {
    B32 is_expect = str8_match(cmd, str8_lit("expect"), 0);
    U64 field_args_count = (is_expect ? (args_count >= 3 ? args_count-2 : 0) : args_count);
    String8 actual = {0};
    if(field_args_count == 0)
    {
      error = (is_expect ? str8_lit("expect: needs <field> <op> <value>") : str8_lit("print: needs a field"));
    }
    else if(rad_script_field(scratch.arena, args, field_args_count, &actual, &error))
    {
      String8 actual_escaped = escaped_from_raw_str8(scratch.arena, actual);
      if(!is_expect)
      {
        rad_script_resultf("PRINT %I64u: %S = \"%S\"", line->line_num, str8_skip_chop_whitespace(str8_skip(line->text, 5)), actual_escaped);
      }
      else
      {
        String8 op = args[args_count-2];
        String8 expected = args[args_count-1];
        B32 pass = 0;
        B32 op_valid = rad_script_compare(actual, op, expected, &pass);
        if(!op_valid)
        {
          error = push_str8f(scratch.arena, "expect: bad operator \"%S\" (or not a number)", op);
        }
        else if(pass)
        {
          rad_script->pass_count += 1;
          rad_script_resultf("PASS %I64u: %S", line->line_num, line->text);
        }
        else
        {
          rad_script->fail_count += 1;
          rad_script_resultf("FAIL %I64u: %S (got \"%S\")", line->line_num, line->text, actual_escaped);
        }
      }
    }
  }

  //- rad: measurement
  else if(str8_match(cmd, str8_lit("measure_begin"), 0))
  {
    if(rad_script->measuring)
    {
      error = str8_lit("measure_begin: a measure is already running");
    }
    else
    {
      rad_script->measuring = 1;
      rad_script->measure_name = push_str8_copy(rad_script->arena, args_count >= 1 ? args[0] : str8_lit("unnamed"));
      rad_script->measure_work_count = 0;
      rad_script->measure_build_count = 0;
      rad_script->measure_latency_count = 0;
    }
  }
  else if(str8_match(cmd, str8_lit("measure_end"), 0))
  {
    if(!rad_script->measuring)
    {
      error = str8_lit("measure_end: no measure is running");
    }
    else
    {
      rad_script->measuring = 0;
      rad_script_resultf("MEASURE %S: frame %S", rad_script->measure_name, rad_script_measure_stats(scratch.arena, rad_script->measure_work_us, rad_script->measure_work_count));
      rad_script_resultf("MEASURE %S: ui build %S", rad_script->measure_name, rad_script_measure_stats(scratch.arena, rad_script->measure_build_us, rad_script->measure_build_count));
      rad_script_resultf("MEASURE %S: input latency %S", rad_script->measure_name, rad_script_measure_stats(scratch.arena, rad_script->measure_latency_us, rad_script->measure_latency_count));
    }
  }

  //- rad: end
  else if(str8_match(cmd, str8_lit("quit"), 0))
  {
    rad_script_finish();
  }

  else
  {
    error = push_str8f(scratch.arena, "unknown command \"%S\"", cmd);
  }

  if(error.size != 0)
  {
    rad_script_error(line, error);
  }
  scratch_end(scratch);
}

////////////////////////////////
//~ rad: Script Top-Level Calls

internal void
rad_script_init(CmdLine *cmdln)
{
  String8 path = cmd_line_string(cmdln, str8_lit("script"));
  if(path.size != 0)
  {
    Arena *arena = arena_alloc();
    rad_script = push_array(arena, RAD_ScriptState, 1);
    rad_script->arena = arena;
    rad_script->input_arena = arena_alloc();
    rad_script->start_us = now_time_us();
    rad_script->path = path_normalized_from_string(arena, path_absolute_dst_from_relative_dst_src(arena, path, get_process_info()->initial_path));
    rad_script->folder = str8_chop_last_slash(rad_script->path);
    rad_script->out_path = cmd_line_string(cmdln, str8_lit("script_out"));
    if(rad_script->out_path.size == 0)
    {
      rad_script->out_path = push_str8f(arena, "%S.results.txt", rad_script->path);
    }
    rad_script->out_path = path_absolute_dst_from_relative_dst_src(arena, rad_script->out_path, get_process_info()->initial_path);
    write_data_to_file_path(rad_script->out_path, push_str8f(arena, "# rad script results: %S\n", rad_script->path));

    //- rad: parse lines (keep line numbers: empty lines and comments count)
    String8 data = data_from_file_path(arena, rad_script->path);
    U8 split_char = '\n';
    String8List text_lines = str8_split(arena, data, &split_char, 1, StringSplitFlag_KeepEmpties);
    rad_script->lines = push_array(arena, RAD_ScriptLine, text_lines.node_count);
    U64 line_num = 0;
    for(String8Node *n = text_lines.first; n != 0; n = n->next)
    {
      line_num += 1;
      String8 text = str8_skip_chop_whitespace(n->string);
      if(text.size != 0 && text.str[0] != '#')
      {
        rad_script->lines[rad_script->lines_count] = rad_script_line_from_text(arena, line_num, text);
        rad_script->lines_count += 1;
      }
    }
    if(data.size == 0)
    {
      rad_script_error(0, push_str8f(arena, "could not read script \"%S\"", rad_script->path));
      rad_script_finish();
    }

    //- rad: input comes from the script from now on
    wm_sim_set_enabled(1);
  }
}

internal void
rad_script_delayed_write_entry_point(void *p)
{
  RAD_ScriptDelayedWrite *w = (RAD_ScriptDelayedWrite *)p;
  sleep_ms((U32)w->delay_ms);
  write_data_to_file_path(w->path, w->data);
}

internal B32
rad_script_lets_rad_sleep(void)
{
  return (rad_script != 0 && !rad_script->done && rad_script->wait_kind == RAD_ScriptWaitKind_Woken);
}

internal B32
rad_script_is_active(void)
{
  return (rad_script != 0 && !rad_script->done);
}

internal void
rad_script_tick(void)
{
  if(!rad_script_is_active() || rad_state->first_window == 0 || rad_state->frame_index < 4)
  {
    return;
  }
  U64 now_us = now_time_us();
  rad_script->input_injected_us = 0;
  wm_sim_set_modifiers(0);

  //- rad: whole-script timeout
  if(now_us > rad_script->start_us + RAD_SCRIPT_TOTAL_TIMEOUT_US)
  {
    rad_script_error(0, str8_lit("script timed out"));
    rad_script_finish();
    return;
  }

  //- rad: queued input: one step per frame, then let the UI settle
  if(rad_script->first_input != 0)
  {
    RAD_ScriptInput *input = rad_script->first_input;
    SLLQueuePop(rad_script->first_input, rad_script->last_input);
    rad_script_inject(input);
    rad_script->input_injected_us = now_us;
    if(rad_script->first_input == 0)
    {
      arena_clear(rad_script->input_arena);
      rad_script_wait_frames(RAD_SCRIPT_SETTLE_FRAMES);
    }
    return;
  }

  //- rad: waits
  switch(rad_script->wait_kind)
  {
    default:{}break;
    case RAD_ScriptWaitKind_Until:
    {
      Temp scratch = scratch_begin(0, 0);
      RAD_ScriptLine *wl = rad_script->wait_line;
      String8 *field_args = wl->args + 2;
      U64 field_args_count = wl->args_count - 4;
      String8 actual = {0};
      String8 field_error = {0};
      B32 pass = 0;
      B32 op_valid = 1;
      if(rad_script_field(scratch.arena, field_args, field_args_count, &actual, &field_error))
      {
        op_valid = rad_script_compare(actual, wl->args[wl->args_count-2], wl->args[wl->args_count-1], &pass);
      }
      B32 keep_waiting = (!pass && op_valid && now_us < rad_script->wait_until_us);
      if(!keep_waiting)
      {
        if(pass)
        {
          rad_script->pass_count += 1;
          rad_script_resultf("PASS %I64u: %S", wl->line_num, wl->text);
        }
        else if(!op_valid)
        {
          rad_script->error_count += 1;
          rad_script_resultf("ERROR %I64u: wait_until: bad operator (or not a number)", wl->line_num);
        }
        else
        {
          rad_script->fail_count += 1;
          rad_script_resultf("FAIL %I64u: %S (timed out; got \"%S\"%s%S)", wl->line_num, wl->text,
                             escaped_from_raw_str8(scratch.arena, actual), field_error.size ? ", " : "", field_error);
        }
      }
      scratch_end(scratch);
      if(keep_waiting)
      {
        return;
      }
    }break;
    case RAD_ScriptWaitKind_Time:
    if(now_us < rad_script->wait_until_us)
    {
      return;
    }break;
    case RAD_ScriptWaitKind_Frames:
    if(rad_script->wait_frames_left != 0)
    {
      rad_script->wait_frames_left -= 1;
      return;
    }break;
    case RAD_ScriptWaitKind_Woken:
    {
      // rad: any frame after the sleep began means rad was woken
      rad_script->last_sleep_us = now_us - rad_script->sleep_begin_us;
    }break;
    case RAD_ScriptWaitKind_Cargo:
    if(rad_cargo_is_running() || rad_cargo->pending_kind != RAD_CargoCmdKind_Null)
    {
      if(now_us < rad_script->wait_until_us)
      {
        return;
      }
      rad_script->error_count += 1;
      rad_script_resultf("ERROR %I64u: wait_cargo timed out", rad_script->wait_line_num);
    }break;
    case RAD_ScriptWaitKind_Output:
    {
      B32 found = 0;
      String8 needle = str8(rad_script->wait_text_buffer, rad_script->wait_text_size);
      MutexScope(rad_cargo->mutex)
      {
        found = (str8_find_needle(str8(rad_cargo->output_base, rad_cargo->output_size), 0, needle, 0) < rad_cargo->output_size);
      }
      if(!found)
      {
        if(now_us < rad_script->wait_until_us)
        {
          return;
        }
        rad_script->error_count += 1;
        rad_script_resultf("ERROR %I64u: wait_output timed out", rad_script->wait_line_num);
      }
    }break;
    case RAD_ScriptWaitKind_Idle:
    if(rad_state->num_frames_requested != 0)
    {
      if(now_us < rad_script->wait_until_us)
      {
        return;
      }
      rad_script->error_count += 1;
      rad_script_resultf("ERROR %I64u: wait_idle timed out", rad_script->wait_line_num);
    }break;
  }
  rad_script->wait_kind = RAD_ScriptWaitKind_Null;

  //- rad: run lines until one waits or queues input
  for(;rad_script->next_line_idx < rad_script->lines_count &&
      !rad_script->done &&
      rad_script->wait_kind == RAD_ScriptWaitKind_Null &&
      rad_script->first_input == 0;)
  {
    RAD_ScriptLine *line = &rad_script->lines[rad_script->next_line_idx];
    rad_script->next_line_idx += 1;
    rad_script_run_line(line);
  }
  if(rad_script->next_line_idx >= rad_script->lines_count &&
     rad_script->wait_kind == RAD_ScriptWaitKind_Null &&
     rad_script->first_input == 0)
  {
    rad_script_finish();
  }
}

internal void
rad_script_frame_end(U64 frame_work_us, U64 ui_build_us)
{
  if(rad_script != 0 && rad_script->measuring)
  {
    if(rad_script->measure_work_count < RAD_SCRIPT_MAX_SAMPLES)
    {
      rad_script->measure_work_us[rad_script->measure_work_count] = frame_work_us;
      rad_script->measure_work_count += 1;
    }
    if(rad_script->measure_build_count < RAD_SCRIPT_MAX_SAMPLES)
    {
      rad_script->measure_build_us[rad_script->measure_build_count] = ui_build_us;
      rad_script->measure_build_count += 1;
    }
    if(rad_script->input_injected_us != 0 && rad_script->measure_latency_count < RAD_SCRIPT_MAX_SAMPLES)
    {
      rad_script->measure_latency_us[rad_script->measure_latency_count] = now_time_us() - rad_script->input_injected_us;
      rad_script->measure_latency_count += 1;
    }
  }
}
