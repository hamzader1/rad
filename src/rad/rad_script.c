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
rad_script_editor_text(Arena *arena, RAD_CodeViewState **cv_out, TB_Text *text_out, String8 *error_out)
{
  B32 result = 0;
  CFG_Node *tab = rad_script_editor_tab();
  if(tab == &cfg_nil_node)
  {
    *error_out = str8_lit("no editor view");
  }
  else
  {
    RAD_CodeViewState *cv = &rad_view_state_from_cfg(tab)->cv;
    if(!cv->initialized || !tb_buffer_is_loaded(cv->buffer))
    {
      *error_out = str8_lit("editor text not loaded");
    }
    else
    {
      Access *access = access_open();
      *text_out = tb_text_from_buffer(arena, access, cv->buffer);
      access_close(access);
      *cv_out = cv;
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
  else if(str8_match(name, str8_lit("tab_count"), 0))
  {
    value = push_str8f(arena, "%I64u", tree.focused->tabs.count);
  }

  //- rad: editor
  else if(str8_match(name, str8_lit("cursor_line"), 0) || str8_match(name, str8_lit("cursor_col"), 0) ||
          str8_match(name, str8_lit("mark_line"), 0) || str8_match(name, str8_lit("mark_col"), 0) ||
          str8_match(name, str8_lit("line_count"), 0) || str8_match(name, str8_lit("line"), 0) ||
          str8_match(name, str8_lit("visible_first"), 0) || str8_match(name, str8_lit("visible_last"), 0) ||
          str8_match(name, str8_lit("dirty"), 0))
  {
    RAD_CodeViewState *cv = 0;
    TB_Text text = {0};
    result = rad_script_editor_text(scratch.arena, &cv, &text, error_out);
    if(result)
    {
      U64 off = (str8_match(name, str8_lit("mark_line"), 0) || str8_match(name, str8_lit("mark_col"), 0)) ? cv->mark : cv->cursor;
      U64 line_num = txt_line_num_from_off(&text.patched.line_map, off);
      Rng1U64 line_range = txt_range_from_line_num(&text.patched.line_map, line_num);
      if(str8_match(name, str8_lit("cursor_line"), 0) || str8_match(name, str8_lit("mark_line"), 0))
      {
        value = push_str8f(arena, "%I64u", line_num);
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
        value = push_str8f(arena, "%i", !!tb_buffer_is_dirty(cv->buffer));
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
  else if(str8_match(name, str8_lit("file"), 0))
  {
    if(args_count < 2)
    {
      *error_out = str8_lit("file: needs a path");
      result = 0;
    }
    else
    {
      value = data_from_file_path(arena, rad_script_path(scratch.arena, args[1]));
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
    TB_Text text = {0};
    if(rad_script_editor_text(scratch.arena, &cv, &text, error_out))
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
  else if(str8_match(cmd, str8_lit("mouse"), 0) || str8_match(cmd, str8_lit("click"), 0) || str8_match(cmd, str8_lit("double_click"), 0))
  {
    U64 used = 0;
    Vec2F32 pos = {0};
    if(rad_script_target(scratch.arena, args, args_count, &used, &pos, &error))
    {
      rad_script_push_input(RAD_ScriptInputKind_MouseMove, pos);
      U64 click_count = (str8_match(cmd, str8_lit("click"), 0) ? 1 : str8_match(cmd, str8_lit("double_click"), 0) ? 2 : 0);
      for EachIndex(idx, click_count)
      {
        rad_script_push_input(RAD_ScriptInputKind_MousePress, pos);
        rad_script_push_input(RAD_ScriptInputKind_MouseRelease, pos);
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
