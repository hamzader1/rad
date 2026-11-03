// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

////////////////////////////////
//~ rad: View State Functions

internal RAD_ViewState *
rad_view_state_from_cfg(CFG_Node *cfg)
{
  RAD_ViewState *vs = 0;
  if(cfg != &cfg_nil_node)
  {
    U64 hash = u64_hash_from_seed_str8(5381, str8_struct(&cfg->id));
    U64 slot_idx = hash%rad_state->view_state_slots_count;
    RAD_ViewStateSlot *slot = &rad_state->view_state_slots[slot_idx];
    for(RAD_ViewState *v = slot->first; v != 0; v = v->hash_next)
    {
      if(v->cfg_id == cfg->id)
      {
        vs = v;
        break;
      }
    }
    if(vs == 0)
    {
      vs = rad_state->free_view_state;
      if(vs != 0)
      {
        SLLStackPop_N(rad_state->free_view_state, hash_next);
      }
      else
      {
        vs = push_array_no_zero(rad_state->arena, RAD_ViewState, 1);
      }
      MemoryZeroStruct(vs);
      vs->cfg_id = cfg->id;
      vs->arena = arena_alloc();
      SLLStackPush_N(slot->first, vs, hash_next);
    }
    vs->last_frame_index_touched = rad_state->frame_index;
  }
  return vs;
}

internal void
rad_view_state_gc(void)
{
  for EachIndex(slot_idx, rad_state->view_state_slots_count)
  {
    RAD_ViewStateSlot *slot = &rad_state->view_state_slots[slot_idx];
    RAD_ViewState *prev = 0;
    for(RAD_ViewState *vs = slot->first, *next = 0; vs != 0; vs = next)
    {
      next = vs->hash_next;
      CFG_Node *cfg = cfg_node_from_id(vs->cfg_id);
      if(cfg == &cfg_nil_node || vs->last_frame_index_touched+2 < rad_state->frame_index)
      {
        if(prev == 0) { slot->first = next; } else { prev->hash_next = next; }
        if(vs->cv.buffer != 0)
        {
          tb_buffer_release(vs->cv.buffer);
        }
        if(vs->ex.initialized)
        {
          arena_release(vs->ex.expansions_arena);
          arena_release(vs->ex.rows_arena);
        }
        arena_release(vs->arena);
        SLLStackPush_N(rad_state->free_view_state, vs, hash_next);
      }
      else
      {
        prev = vs;
      }
    }
  }
}

////////////////////////////////
//~ rad: Code Views

internal void
rad_code_view_init(RAD_CodeViewState *cv, TXT_LangKind lang_kind)
{
  if(cv->initialized == 0)
  {
    cv->initialized = 1;
    cv->preferred_column = 1;
    cv->buffer = tb_buffer_alloc(lang_kind);
    cv->initial_center = 1;
    cv->center_cursor = 1;
  }
}

internal B32
rad_code_view_undo(RAD_CodeViewState *cv)
{
  U64 cursor = cv->cursor;
  B32 result = tb_buffer_undo(cv->buffer, &cursor);
  if(result)
  {
    cv->cursor = cv->mark = cursor;
    cv->contain_cursor = 1;
  }
  return result;
}

internal B32
rad_code_view_redo(RAD_CodeViewState *cv)
{
  U64 cursor = cv->cursor;
  B32 result = tb_buffer_redo(cv->buffer, &cursor);
  if(result)
  {
    cv->cursor = cv->mark = cursor;
    cv->contain_cursor = 1;
  }
  return result;
}

internal void
rad_code_view_build(Arena *arena, RAD_CodeViewState *cv, Rng2F32 rect, TXT_LangKind lang_kind, TXT_TextInfo *text_info, String8 text_data)
{
  // NOTE(rad): port of raddbg's `rd_code_view_build` (raddbg/raddbg_views.c),
  // minus breakpoints, thread IPs, watch pins, debug-info lines, disassembly,
  // and the view query/search commands.
  ProfBeginFunction();
  Temp scratch = scratch_begin(&arena, 1);
  RAD_Settings *settings = &rad_state->settings;

  //////////////////////////////
  //- rad: unpack state
  //
  UI_ScrollPt2 scroll_pos = cv->scroll_pos;

  //////////////////////////////
  //- rad: extract invariants
  //
  F32 main_font_size = ui_bottom_font_size();
  FNT_Tag code_font = rad_font_from_slot(RAD_FontSlot_Code);
  F32 code_font_size = ui_top_font_size();
  F32 code_tab_size = fnt_column_size_from_tag_size(code_font, code_font_size)*settings->tab_width;
  FNT_Metrics code_font_metrics = fnt_metrics_from_tag_size(code_font, code_font_size);
  F32 code_line_height = ceil_f32(fnt_line_height_from_metrics(&code_font_metrics) * 1.5f);
  F32 big_glyph_advance = fnt_dim_from_tag_size_string(code_font, code_font_size, 0, 0, str8_lit("H")).x;
  Vec2F32 panel_box_dim = dim_2f32(rect);
  F32 scroll_bar_dim = floor_f32(main_font_size*1.5f);
  Vec2F32 code_area_dim = v2f32(panel_box_dim.x - scroll_bar_dim, panel_box_dim.y - scroll_bar_dim);
  S64 num_possible_visible_lines = (S64)(code_area_dim.y/code_line_height)+1;
  B32 do_line_numbers = settings->show_line_numbers;
  B32 text_is_ready = (text_info->lines_count != 0);

  //////////////////////////////
  //- rad: do keyboard interaction, compute patched text state
  //
  TXT_Patched text_patched = txt_patched_from_info_data_patches(scratch.arena, text_info, text_data, &cv->buffer->patches);
  B32 snap[Axis2_COUNT] = {0};
  UI_Focus(UI_FocusKind_On) if(ui_is_focus_active() && text_is_ready)
  {
    U64 line_count_per_page = ClampBot(num_possible_visible_lines, 10) - 10;
    U64 *cursor = &cv->cursor;
    U64 *mark = &cv->mark;
    S64 *preferred_column = &cv->preferred_column;
    for(UI_Event *evt = 0; ui_next_event(&evt);)
    {
      if(evt->kind != UI_EventKind_Navigate && evt->kind != UI_EventKind_Edit && evt->kind != UI_EventKind_Text)
      {
        continue;
      }
      U64 start_cursor = *cursor;
      U64 start_mark = *mark;
      Vec2S32 delta = evt->delta_2s32;
      U64 line_count = text_patched.line_map.total_line_count;
      U64 line_num = txt_line_num_from_off(&text_patched.line_map, *cursor);
      Rng1U64 line_range = txt_range_from_line_num(&text_patched.line_map, line_num);
      String8 line = {0};
      line.size = dim_1u64(line_range);
      line.str = push_array(scratch.arena, U8, line.size);
      memory_map_read(&text_patched.memory_map, line_range, line.str);

      //- rad: interpret event as single-line text op
      UI_TxtOp single_line_op = ui_single_line_txt_op_from_event(scratch.arena, evt, line, line_range, *cursor, *mark);

      //- rad: read-only views skip replace-ranges (same as raddbg)
      if(!cv->editable && (single_line_op.replace.size != 0 || evt->flags & UI_EventFlag_Delete))
      {
        continue;
      }

      //- rad: newline insertion -> auto-indent with the current line's leading
      // whitespace; CRLF files get CRLF line ends
      if(cv->editable && evt->string.size != 0)
      {
        String8 insert = evt->string;
        if(str8_match(insert, str8_lit("\n"), 0))
        {
          U64 indent_size = 0;
          for(;indent_size < line.size && (line.str[indent_size] == ' ' || line.str[indent_size] == '\t'); indent_size += 1);
          indent_size = Min(indent_size, (*cursor > line_range.min ? *cursor - line_range.min : 0));
          insert = push_str8f(scratch.arena, "\n%S", str8_prefix(line, indent_size));
        }
        if(str8_find_needle(insert, 0, str8_lit("\n"), 0) < insert.size)
        {
          // rad: normalize line ends to the file's convention ("\r\n" or "\n");
          // NOTE: not str8_split - it drops trailing empty pieces, which would
          // turn a lone "\n" into an empty insertion.
          U8 *normalized = push_array(scratch.arena, U8, insert.size*2);
          U64 normalized_size = 0;
          for EachIndex(idx, insert.size)
          {
            U8 c = insert.str[idx];
            if(c == '\r' && idx+1 < insert.size && insert.str[idx+1] == '\n')
            {
              continue;
            }
            if(c == '\n' && cv->buffer->crlf)
            {
              normalized[normalized_size++] = '\r';
            }
            normalized[normalized_size++] = c;
          }
          insert = str8(normalized, normalized_size);
        }
        if(insert.str != evt->string.str)
        {
          single_line_op.replace = insert;
        }
      }

      //- rad: apply single-line navigations
      *cursor = single_line_op.cursor;
      *mark = single_line_op.mark;

      //- rad: determine if we need to navigate
      B32 need_nav = (start_cursor == start_mark || !(evt->flags & UI_EventFlag_ZeroDeltaOnSelect));

      //- rad: wrap lines right
      if(need_nav && evt->delta_unit != UI_EventDeltaUnit_Whole && evt->delta_unit != UI_EventDeltaUnit_Line && delta.x > 0 && start_cursor == line_range.max && line_num+1 <= line_count)
      {
        Rng1U64 next_line_range = txt_range_from_line_num(&text_patched.line_map, line_num+1);
        *cursor = next_line_range.min;
        *preferred_column = 1;
      }

      //- rad: wrap lines left
      if(need_nav && evt->delta_unit != UI_EventDeltaUnit_Whole && evt->delta_unit != UI_EventDeltaUnit_Line && delta.x < 0 && start_cursor == line_range.min && line_num-1 >= 1)
      {
        Rng1U64 prev_line_range = txt_range_from_line_num(&text_patched.line_map, line_num-1);
        *cursor = prev_line_range.max;
        *preferred_column = (S64)dim_1u64(prev_line_range)+1;
      }

      //- rad: movement down (plain)
      if(need_nav && evt->delta_unit == UI_EventDeltaUnit_Char && delta.y > 0 && line_num+1 <= line_count)
      {
        Rng1U64 next_line_range = txt_range_from_line_num(&text_patched.line_map, line_num+1);
        *cursor = next_line_range.min + *preferred_column;
        *cursor = clamp_1u64(next_line_range, *cursor);
      }

      //- rad: movement up (plain)
      if(need_nav && evt->delta_unit == UI_EventDeltaUnit_Char && delta.y < 0 && line_num > 1)
      {
        Rng1U64 prev_line_range = txt_range_from_line_num(&text_patched.line_map, line_num-1);
        *cursor = prev_line_range.min + *preferred_column;
        *cursor = clamp_1u64(prev_line_range, *cursor);
      }

      //- rad: movement down (chunk)
      if(need_nav && evt->delta_unit == UI_EventDeltaUnit_Word && delta.y > 0 && line_num+1 <= line_count)
      {
        B32 done = 0;
        for(U64 scan_line_num = line_num+1; !done && scan_line_num <= line_count; scan_line_num += 1)
        {
          Temp scratch = scratch_begin(&arena, 1);
          Rng1U64 line_range = txt_range_from_line_num(&text_patched.line_map, scan_line_num);
          String8 line = memory_map_data_from_range(scratch.arena, &text_patched.memory_map, line_range);
          String8 line_without_whitespace = str8_skip_chop_whitespace(line);
          if(line_without_whitespace.size == 0)
          {
            *cursor = line_range.min + (U64)(line_without_whitespace.str - line.str);
            done = 1;
          }
          else if(scan_line_num == line_count)
          {
            *cursor = text_patched.size;
          }
          scratch_end(scratch);
        }
      }

      //- rad: movement up (chunk)
      if(need_nav && evt->delta_unit == UI_EventDeltaUnit_Word && delta.y < 0 && line_num > 1)
      {
        B32 done = 0;
        for(U64 scan_line_num = line_num-1; !done && scan_line_num > 0; scan_line_num -= 1)
        {
          Temp scratch = scratch_begin(&arena, 1);
          Rng1U64 line_range = txt_range_from_line_num(&text_patched.line_map, scan_line_num);
          String8 line = memory_map_data_from_range(scratch.arena, &text_patched.memory_map, line_range);
          String8 line_without_whitespace = str8_skip_chop_whitespace(line);
          if(line_without_whitespace.size == 0)
          {
            *cursor = line_range.min + (U64)(line_without_whitespace.str - line.str);
            done = 1;
          }
          else if(scan_line_num == 1)
          {
            *cursor = 0;
          }
          scratch_end(scratch);
        }
      }

      //- rad: movement down (page)
      if(need_nav && evt->delta_unit == UI_EventDeltaUnit_Page && delta.y > 0)
      {
        U64 advance = line_count_per_page;
        U64 next_line = line_num + advance;
        U64 next_line_clamped = Clamp(1, next_line, text_patched.line_map.total_line_count);
        Rng1U64 next_line_range = txt_range_from_line_num(&text_patched.line_map, next_line_clamped);
        *cursor = next_line_range.min;
      }

      //- rad: movement up (page)
      if(need_nav && evt->delta_unit == UI_EventDeltaUnit_Page && delta.y < 0)
      {
        S64 advance = -(S64)line_count_per_page;
        if(line_num < line_count_per_page)
        {
          advance = -(S64)(line_num - 1);
        }
        U64 next_line = (U64)((S64)line_num + advance);
        U64 next_line_clamped = Clamp(1, next_line, text_patched.line_map.total_line_count);
        Rng1U64 next_line_range = txt_range_from_line_num(&text_patched.line_map, next_line_clamped);
        *cursor = next_line_range.min;
      }

      //- rad: movement to endpoint (+)
      if(need_nav && evt->delta_unit == UI_EventDeltaUnit_Whole && (delta.y > 0 || delta.x > 0))
      {
        *cursor = text_patched.size;
      }

      //- rad: movement to endpoint (-)
      if(need_nav && evt->delta_unit == UI_EventDeltaUnit_Whole && (delta.y < 0 || delta.x < 0))
      {
        *cursor = 0;
      }

      //- rad: get replaced-range; adjust based on multi-line logic (same as raddbg)
      Rng1U64 replaced_range = single_line_op.range;
      if(*cursor != single_line_op.cursor && (evt->flags & UI_EventFlag_Delete || evt->string.size != 0))
      {
        replaced_range = r1u64(*mark, *cursor);
      }

      //- rad: in some cases, we want to pick a selection side based on the delta
      if(*cursor != *mark && evt->flags & UI_EventFlag_PickSelectSide)
      {
        if(delta.x < 0 || delta.y < 0)
        {
          *cursor = *mark = Min(*cursor, *mark);
        }
        else if(delta.x > 0 || delta.y > 0)
        {
          *cursor = *mark = Max(*cursor, *mark);
        }
      }

      //- rad: do copy if needed
      if(evt->flags & UI_EventFlag_Copy)
      {
        String8 text = memory_map_data_from_range(scratch.arena, &text_patched.memory_map, r1u64(*cursor, *mark));
        wm_set_clipboard_text(text);
      }

      //- rad: stick mark to cursor, when we don't want to keep it in the same spot
      if(!(evt->flags & UI_EventFlag_KeepMark))
      {
        *mark = *cursor;
      }

      //- rad: push patch if we have one (same as raddbg, plus undo history);
      // moving the cursor without editing ends the current undo group
      if(cv->editable && (replaced_range.max != replaced_range.min || single_line_op.replace.size != 0))
      {
        tb_buffer_edit(cv->buffer, &text_patched, replaced_range, single_line_op.replace);
        text_patched = txt_patched_from_info_data_patches(scratch.arena, text_info, text_data, &cv->buffer->patches);
        *cursor = *mark = replaced_range.min + single_line_op.replace.size;
        U64 line_num = txt_line_num_from_off(&text_patched.line_map, *cursor);
        Rng1U64 line_range = txt_range_from_line_num(&text_patched.line_map, line_num);
        *preferred_column = (*cursor - line_range.min);
        snap[Axis2_Y] = 1;
      }
      else if(*cursor != start_cursor)
      {
        tb_buffer_close_undo_group(cv->buffer);
      }

      //- rad: consume
      ui_eat_event(evt);

      //- rad: changed cursor -> snap in X
      if(*cursor != start_cursor)
      {
        snap[Axis2_X] = 1;
      }

      //- rad: changed cursor line -> snap in Y
      if(*cursor < line_range.min || line_range.max < *cursor)
      {
        snap[Axis2_Y] = 1;
      }
    }
  }

  //////////////////////////////
  //- rad: determine visible line range / count
  //
  Rng1S64 visible_line_num_range = r1s64(scroll_pos.y.idx + (S64)(scroll_pos.y.off) + 1 - !!(scroll_pos.y.off < 0),
                                         scroll_pos.y.idx + (S64)(scroll_pos.y.off) + 1 + num_possible_visible_lines);
  Rng1S64 target_visible_line_num_range = r1s64(scroll_pos.y.idx + 1,
                                                scroll_pos.y.idx + 1 + num_possible_visible_lines);
  U64 visible_line_count = 0;
  {
    visible_line_num_range.min = Clamp(1, visible_line_num_range.min, (S64)text_patched.line_map.total_line_count);
    visible_line_num_range.max = Clamp(1, visible_line_num_range.max, (S64)text_patched.line_map.total_line_count);
    visible_line_num_range.min = Max(1, visible_line_num_range.min);
    visible_line_num_range.max = Max(1, visible_line_num_range.max);
    target_visible_line_num_range.min = Clamp(1, target_visible_line_num_range.min, (S64)text_patched.line_map.total_line_count);
    target_visible_line_num_range.max = Clamp(1, target_visible_line_num_range.max, (S64)text_patched.line_map.total_line_count);
    target_visible_line_num_range.min = Max(1, target_visible_line_num_range.min);
    target_visible_line_num_range.max = Max(1, target_visible_line_num_range.max);
    visible_line_count = (U64)dim_1s64(visible_line_num_range)+1;
  }

  //////////////////////////////
  //- rad: calculate scroll bounds
  //
  S64 line_size_x = 0;
  Rng1S64 scroll_idx_rng[Axis2_COUNT] = {0};
  {
    line_size_x = (text_info->lines_max_size*big_glyph_advance*3)/2;
    line_size_x = ClampBot(line_size_x, (S64)big_glyph_advance*120);
    line_size_x = ClampBot(line_size_x, (S64)code_area_dim.x);
    scroll_idx_rng[Axis2_X] = r1s64(0, line_size_x-(S64)code_area_dim.x);
    scroll_idx_rng[Axis2_Y] = r1s64(0, (S64)text_patched.line_map.total_line_count-1);
  }

  //////////////////////////////
  //- rad: calculate line-range-dependent info
  //
  F32 line_num_width_px = 0;
  if(do_line_numbers)
  {
    line_num_width_px = floor_f32(big_glyph_advance * (log10(visible_line_num_range.max) + 3));
  }
  Rng1U64 visible_byte_range = r1u64(txt_range_from_line_num(&text_patched.line_map, visible_line_num_range.min).min,
                                     txt_range_from_line_num(&text_patched.line_map, visible_line_num_range.max).max);
  String8 visible_data = memory_map_data_from_range(scratch.arena, &text_patched.memory_map, visible_byte_range);

  //////////////////////////////
  //- rad: find tokens for visible byte range
  //
  U64 ctx_token_pt_num = txt_token_pt_num_from_off(&text_patched.token_pt_map, visible_byte_range.min);
  TXT_TokenPt ctx_token_pt = txt_token_pt_from_num(&text_patched.token_pt_map, ctx_token_pt_num);
  TXT_TokenArray tokens = txt_token_array_from_data(scratch.arena, lang_kind, ctx_token_pt, visible_data, visible_byte_range.min, max_U64);

  //////////////////////////////
  //- rad: prepare code slice info bundle, for the viewable region of text
  //
  RAD_CodeSliceParams code_slice_params = {0};
  {
    // rad: fill basics
    code_slice_params.flags = RAD_CodeSliceFlag_Clickable;
    if(do_line_numbers)
    {
      code_slice_params.flags |= RAD_CodeSliceFlag_LineNums;
    }
    code_slice_params.line_num_range            = visible_line_num_range;
    code_slice_params.line_text                 = push_array(scratch.arena, String8, visible_line_count);
    code_slice_params.line_ranges               = push_array(scratch.arena, Rng1U64, visible_line_count);
    code_slice_params.line_tokens               = push_array(scratch.arena, TXT_TokenArray, visible_line_count);
    code_slice_params.text_info                 = text_info;
    code_slice_params.patches                   = &cv->buffer->patches;
    code_slice_params.text_data                 = text_data;
    code_slice_params.font                      = code_font;
    code_slice_params.font_size                 = code_font_size;
    code_slice_params.tab_size                  = code_tab_size;
    code_slice_params.line_height_px            = code_line_height;
    code_slice_params.line_num_width_px         = line_num_width_px;
    code_slice_params.line_text_max_width_px    = (F32)line_size_x;
    code_slice_params.margin_float_off_px       = scroll_pos.x.idx + floor_f32(scroll_pos.x.off);

    // rad: fill line text / ranges
    {
      S64 line_num = visible_line_num_range.min;
      U64 line_idx = visible_line_num_range.min-1;
      for(U64 visible_line_idx = 0;
          visible_line_idx < visible_line_count && line_idx < text_patched.line_map.total_line_count;
          visible_line_idx += 1, line_idx += 1, line_num += 1)
      {
        Rng1U64 line_range = txt_range_from_line_num(&text_patched.line_map, line_num);
        String8 line_text = {0};
        line_text.size = dim_1u64(line_range);
        line_text.str = push_array(scratch.arena, U8, line_text.size);
        memory_map_read(&text_patched.memory_map, line_range, line_text.str);
        code_slice_params.line_text[visible_line_idx]   = line_text;
        code_slice_params.line_ranges[visible_line_idx] = line_range;
      }
    }

    // rad: bucket tokens by line
    {
      U64 token_idx = 0;
      for EachIndex(line_idx, visible_line_count)
      {
        Temp scratch2 = scratch_begin(&scratch.arena, 1);
        Rng1U64 line_range = code_slice_params.line_ranges[line_idx];
        TXT_TokenList line_tokens = {0};
        for(;token_idx < tokens.count;)
        {
          Rng1U64 token_range = tokens.v[token_idx].range;
          if(dim_1u64(intersect_1u64(token_range, line_range)) != 0)
          {
            txt_token_list_push(scratch2.arena, &line_tokens, &tokens.v[token_idx]);
          }
          if(token_range.max <= line_range.max || token_range.max <= line_range.min)
          {
            token_idx += 1;
          }
          else if(line_range.max <= token_range.max)
          {
            break;
          }
        }
        code_slice_params.line_tokens[line_idx] = txt_token_array_from_list(scratch.arena, &line_tokens);
        scratch_end(scratch2);
      }
    }
  }

  //////////////////////////////
  //- rad: build container
  //
  UI_Box *container_box = &ui_nil_box;
  {
    ui_set_next_pref_width(ui_px(code_area_dim.x, 1));
    ui_set_next_pref_height(ui_px(code_area_dim.y, 1));
    ui_set_next_child_layout_axis(Axis2_Y);
    container_box = ui_build_box_from_stringf(UI_BoxFlag_Clip|
                                              UI_BoxFlag_Scroll|
                                              UI_BoxFlag_AllowOverflowX|
                                              UI_BoxFlag_AllowOverflowY,
                                              "###code_area");
  }

  //////////////////////////////
  //- rad: do goto line
  //
  if(cv->goto_line_num != 0 && text_is_ready)
  {
    S64 line_num = cv->goto_line_num;
    cv->goto_line_num = 0;
    line_num = Clamp(1, line_num, (S64)text_patched.line_map.total_line_count);
    Rng1U64 range = txt_range_from_line_num(&text_patched.line_map, line_num);
    cv->cursor = cv->mark = range.min;
    cv->center_cursor = !cv->force_contain_only && (!cv->contain_cursor || (line_num < target_visible_line_num_range.min+4 || target_visible_line_num_range.max-4 < line_num));
  }

  //////////////////////////////
  //- rad: build container contents
  //
  UI_Parent(container_box)
  {
    //- rad: build fractional space
    container_box->view_off.x = container_box->view_off_target.x = scroll_pos.x.idx + scroll_pos.x.off;
    container_box->view_off.y = container_box->view_off_target.y = code_line_height*mod_f32(scroll_pos.y.off, 1.f) + code_line_height*(scroll_pos.y.off < 0) - code_line_height*(scroll_pos.y.off == -1.f && scroll_pos.y.idx == 1);

    //- rad: build code slice
    RAD_CodeSliceSignal sig = {0};
    UI_Focus(UI_FocusKind_On)
    {
      sig = rad_code_slicef(&code_slice_params, &cv->cursor, &cv->mark, &cv->preferred_column, "code_slice");
    }

    //- rad: dragging & outside region? -> contain cursor
    if(ui_dragging(sig.base) && sig.base.event_flags == 0)
    {
      if(!contains_2f32(sig.base.box->rect, ui_mouse()))
      {
        cv->contain_cursor = 1;
      }
      else
      {
        snap[Axis2_X] = 1;
      }
    }
  }

  //////////////////////////////
  //- rad: apply post-build view snapping rules
  //
  if(text_is_ready)
  {
    U64 cursor = cv->cursor;
    U64 cursor_line_num = txt_line_num_from_off(&text_patched.line_map, cursor);
    B32 cursor_in_range = (1 <= cursor_line_num && cursor_line_num <= text_patched.line_map.total_line_count);

    // rad: contain => snap
    if(cv->contain_cursor)
    {
      cv->contain_cursor = 0;
      snap[Axis2_X] = 1;
      snap[Axis2_Y] = 1;
    }

    // rad: center cursor
    if(cv->center_cursor)
    {
      cv->center_cursor = 0;
      if(cursor_in_range)
      {
        Rng1U64 cursor_line_range = txt_range_from_line_num(&text_patched.line_map, cursor_line_num);
        String8 cursor_line = memory_map_data_from_range(scratch.arena, &text_patched.memory_map, cursor_line_range);
        F32 cursor_advance = fnt_dim_from_tag_size_string(code_font, code_font_size, 0, code_tab_size, str8_prefix(cursor_line, cursor-cursor_line_range.min)).x;

        // rad: scroll x
        {
          S64 new_idx = (S64)(cursor_advance - code_area_dim.x/2);
          new_idx = Clamp(scroll_idx_rng[Axis2_X].min, new_idx, scroll_idx_rng[Axis2_X].max);
          ui_scroll_pt_target_idx(&scroll_pos.x, new_idx);
          snap[Axis2_X] = 0;
        }

        // rad: scroll y
        {
          S64 new_idx = ((S64)cursor_line_num-1) - num_possible_visible_lines/2 + 2;
          new_idx = Clamp(scroll_idx_rng[Axis2_Y].min, new_idx, scroll_idx_rng[Axis2_Y].max);
          ui_scroll_pt_target_idx(&scroll_pos.y, new_idx);
          snap[Axis2_Y] = 0;
        }

        // rad: just snap if this is the initial center
        if(cv->initial_center)
        {
          scroll_pos.x.off = 0;
          scroll_pos.y.off = 0;
          cv->initial_center = 0;
        }
      }
    }

    // rad: snap in X
    if(snap[Axis2_X] && cursor_in_range)
    {
      Rng1U64 cursor_line_range = txt_range_from_line_num(&text_patched.line_map, cursor_line_num);
      String8 cursor_line = memory_map_data_from_range(scratch.arena, &text_patched.memory_map, cursor_line_range);
      S64 cursor_off = (S64)(fnt_dim_from_tag_size_string(code_font, code_font_size, 0, code_tab_size, str8_prefix(cursor_line, cursor-cursor_line_range.min)).x + line_num_width_px);
      Rng1S64 visible_pixel_range =
      {
        scroll_pos.x.idx,
        scroll_pos.x.idx + (S64)code_area_dim.x,
      };
      Rng1S64 cursor_pixel_range =
      {
        cursor_off - (S64)(big_glyph_advance*4) - (S64)(line_num_width_px),
        cursor_off + (S64)(big_glyph_advance*4),
      };
      S64 min_delta = Min(0, cursor_pixel_range.min - visible_pixel_range.min);
      S64 max_delta = Max(0, cursor_pixel_range.max - visible_pixel_range.max);
      S64 new_idx = scroll_pos.x.idx+min_delta+max_delta;
      new_idx = Clamp(scroll_idx_rng[Axis2_X].min, new_idx, scroll_idx_rng[Axis2_X].max);
      ui_scroll_pt_target_idx(&scroll_pos.x, new_idx);
    }

    // rad: snap in Y
    if(snap[Axis2_Y])
    {
      Rng1S64 cursor_visibility_range = r1s64((S64)cursor_line_num-4, (S64)cursor_line_num+4);
      cursor_visibility_range.min = ClampBot(0, cursor_visibility_range.min);
      cursor_visibility_range.max = ClampBot(0, cursor_visibility_range.max);
      S64 min_delta = Min(0, cursor_visibility_range.min-(target_visible_line_num_range.min));
      S64 max_delta = Max(0, cursor_visibility_range.max-(target_visible_line_num_range.min+num_possible_visible_lines));
      S64 new_idx = scroll_pos.y.idx+min_delta+max_delta;
      new_idx = Clamp(0, new_idx, (S64)text_patched.line_map.total_line_count-1);
      ui_scroll_pt_target_idx(&scroll_pos.y, new_idx);
    }
  }

  //////////////////////////////
  //- rad: build horizontal scroll bar
  //
  {
    ui_set_next_fixed_x(0);
    ui_set_next_fixed_y(code_area_dim.y);
    ui_set_next_fixed_width(panel_box_dim.x - scroll_bar_dim);
    ui_set_next_fixed_height(scroll_bar_dim);
    {
      scroll_pos.x = ui_scroll_bar(Axis2_X,
                                   ui_px(scroll_bar_dim, 1.f),
                                   scroll_pos.x,
                                   scroll_idx_rng[Axis2_X],
                                   (S64)code_area_dim.x);
    }
  }

  //////////////////////////////
  //- rad: build vertical scroll bar
  //
  {
    ui_set_next_fixed_x(code_area_dim.x);
    ui_set_next_fixed_y(0);
    ui_set_next_fixed_width(scroll_bar_dim);
    ui_set_next_fixed_height(panel_box_dim.y - scroll_bar_dim);
    {
      scroll_pos.y = ui_scroll_bar(Axis2_Y,
                                   ui_px(scroll_bar_dim, 1.f),
                                   scroll_pos.y,
                                   scroll_idx_rng[Axis2_Y],
                                   num_possible_visible_lines);
    }
  }

  //////////////////////////////
  //- rad: top-level container interaction (scrolling)
  //
  if(text_is_ready)
  {
    UI_Signal sig = ui_signal_from_box(container_box);
    if(sig.scroll.x != 0)
    {
      S64 new_idx = scroll_pos.x.idx+sig.scroll.x*big_glyph_advance;
      new_idx = clamp_1s64(scroll_idx_rng[Axis2_X], new_idx);
      ui_scroll_pt_target_idx(&scroll_pos.x, new_idx);
    }
    if(sig.scroll.y != 0)
    {
      S64 new_idx = scroll_pos.y.idx + sig.scroll.y;
      new_idx = clamp_1s64(scroll_idx_rng[Axis2_Y], new_idx);
      ui_scroll_pt_target_idx(&scroll_pos.y, new_idx);
    }
    ui_scroll_pt_clamp_idx(&scroll_pos.x, scroll_idx_rng[Axis2_X]);
    ui_scroll_pt_clamp_idx(&scroll_pos.y, scroll_idx_rng[Axis2_Y]);
  }

  //////////////////////////////
  //- rad: store state
  //
  cv->scroll_pos = scroll_pos;
  cv->force_contain_only = 0;

  scratch_end(scratch);
  ProfEnd();
}

////////////////////////////////
//~ rad: Text View

internal void
rad_text_view_ui(CFG_Node *tab, Rng2F32 rect)
{
  // NOTE(rad): port of raddbg's text view (raddbg/raddbg_views.c,
  // RD_VIEW_UI_FUNCTION_DEF(text)), minus debug-info checksum/override logic.
  RAD_ViewState *vs = rad_view_state_from_cfg(tab);
  RAD_CodeViewState *cv = &vs->cv;
  Temp scratch = scratch_begin(0, 0);
  Access *access = access_open();

  //////////////////////////////
  //- rad: set up invariants
  //
  F32 main_font_size = ui_bottom_font_size();
  F32 bottom_bar_height = main_font_size*2.f;
  Rng2F32 code_area_rect = r2f32p(rect.x0, rect.y0, rect.x1, rect.y1 - bottom_bar_height);
  Rng2F32 bottom_bar_rect = r2f32p(rect.x0, rect.y1 - bottom_bar_height, rect.x1, rect.y1);

  //////////////////////////////
  //- rad: unpack parameterization info
  //
  String8 file_path = rad_file_path_from_tab(tab);
  FileProperties props = properties_from_file_path(file_path);
  B32 file_is_missing = (file_path.size != 0 && props.modified == 0);
  C_Key text_key = fs_key_from_path_range(file_path, r1u64(0, props.size), 0);
  TXT_LangKind lang_kind = txt_lang_kind_from_extension(str8_skip_last_dot(file_path));
  rad_code_view_init(cv, lang_kind);
  cv->editable = 1;
  TB_Buffer *buffer = cv->buffer;

  //////////////////////////////
  //- rad: feed the file stream's current content to the buffer (load, reload,
  // "changed on disk"), let it compact its patches, then read its text
  //
  U128 source_hash = {0};
  txt_text_info_from_key_lang(access, text_key, lang_kind, &source_hash);
  tb_buffer_observe_source(buffer, access, source_hash);
  tb_buffer_tick(buffer, access, now_time_us());
  TB_Text text = tb_text_from_buffer(scratch.arena, access, buffer);
  TXT_TextInfo info = text.base_info;
  String8 data = text.base_data;
  B32 key_has_data = (info.lines_count != 0);
  if(tb_buffer_is_busy(buffer))
  {
    rad_request_frame();
  }

  //////////////////////////////
  //- rad: build missing file interface
  //
  if(file_is_missing)
  {
    UI_WidthFill UI_HeightFill UI_Column UI_Padding(ui_pct(1, 0))
    {
      UI_PrefWidth(ui_children_sum(1)) UI_PrefHeight(ui_em(3, 1))
        UI_Row UI_Padding(ui_pct(1, 0))
        UI_PrefWidth(ui_text_dim(1, 1))
        UI_TagF("weak")
      {
        RAD_Font(RAD_FontSlot_Icons) ui_label(rad_icon_kind_text_table[RAD_IconKind_WarningBig]);
        ui_labelf("Could not find \"%S\".", file_path);
      }
    }
  }

  //////////////////////////////
  //- rad: text not ready yet -> keep requesting frames until the async lex lands
  //
  if(!file_is_missing && !key_has_data)
  {
    rad_request_frame();
  }

  //////////////////////////////
  //- rad: build code contents
  //
  if(!file_is_missing)
  {
    rad_code_view_build(scratch.arena, cv, code_area_rect, lang_kind, &info, data);
  }

  //////////////////////////////
  //- rad: produce patched text info, unpack cursor info in patched text
  //
  TXT_Patched patched = txt_patched_from_info_data_patches(scratch.arena, &info, data, &buffer->patches);
  U64 cursor_line_num = txt_line_num_from_off(&patched.line_map, cv->cursor);
  Rng1U64 cursor_line_range = txt_range_from_line_num(&patched.line_map, cursor_line_num);

  //////////////////////////////
  //- rad: build bottom bar
  //
  if(!file_is_missing && key_has_data) UI_FontSize(main_font_size) UI_TagF(".")
  {
    ui_set_next_rect(shift_2f32(bottom_bar_rect, scale_2f32(rect.p0, -1.f)));
    ui_set_next_flags(UI_BoxFlag_DrawBackground);
    UI_Row
      UI_TextAlignment(UI_TextAlign_Center)
      UI_PrefWidth(ui_text_dim(10, 1))
      UI_TagF("weak")
    {
      RAD_Font(RAD_FontSlot_Code)
      {
        ui_label(file_path);
        ui_spacer(ui_em(1.5f, 1));
        ui_labelf("Line: %I64d, Column: %I64d, Offset: 0x%I64x", cursor_line_num, 1 + cv->cursor - cursor_line_range.min, cv->cursor);
        ui_spacer(ui_pct(1, 0));
        if(buffer->changed_on_disk)
        {
          ui_labelf("(changed on disk)");
        }
        if(tb_buffer_is_dirty(buffer))
        {
          ui_labelf("(modified)");
        }
        ui_labelf("%s",
                  buffer->crlf                               ? "crlf" :
                  info.line_end_kind == TXT_LineEndKind_Null ? "bin" :
                  "lf");
      }
    }
  }

  access_close(access);
  scratch_end(scratch);
}

internal B32
rad_text_view_save(CFG_Node *tab)
{
  B32 result = 0;
  RAD_ViewState *vs = rad_view_state_from_cfg(tab);
  RAD_CodeViewState *cv = &vs->cv;
  String8 file_path = rad_file_path_from_tab(tab);
  if(cv->initialized && file_path.size != 0 && tb_buffer_is_loaded(cv->buffer))
  {
    Access *access = access_open();
    result = tb_buffer_save(cv->buffer, access, file_path);
    access_close(access);
    if(!result)
    {
      log_user_errorf("Could not save \"%S\".", file_path);
    }
  }
  return result;
}

internal B32
rad_tab_is_dirty(CFG_Node *tab)
{
  B32 result = 0;
  if(str8_match(tab->string, str8_lit("text"), 0))
  {
    RAD_ViewState *vs = rad_view_state_from_cfg(tab);
    result = (vs != 0 && vs->cv.initialized && tb_buffer_is_dirty(vs->cv.buffer));
  }
  return result;
}

////////////////////////////////
//~ rad: Explorer

#define RAD_EXPLORER_MAX_ROWS 100000
#define RAD_EXPLORER_REFRESH_US 1000000

typedef struct RAD_ExplorerRowNode RAD_ExplorerRowNode;
struct RAD_ExplorerRowNode
{
  RAD_ExplorerRowNode *next;
  RAD_ExplorerRow v;
};

typedef struct RAD_ExplorerRowList RAD_ExplorerRowList;
struct RAD_ExplorerRowList
{
  RAD_ExplorerRowNode *first;
  RAD_ExplorerRowNode *last;
  U64 count;
};

internal String8
rad_explorer_folder_from_tab(CFG_Node *tab)
{
  String8 result = cfg_node_child_from_string(tab, str8_lit("folder"))->first->string;
  return result;
}

internal void
rad_explorer_set_folder(CFG_Node *tab, String8 folder)
{
  //- rad: write the tab's `folder` child
  CFG_Node *folder_cfg = cfg_node_child_from_string_or_alloc(rad_state->cfg, tab, str8_lit("folder"));
  if(folder_cfg->first != &cfg_nil_node)
  {
    cfg_node_equip_string(rad_state->cfg, folder_cfg->first, folder);
  }
  else
  {
    cfg_node_new(rad_state->cfg, folder_cfg, folder);
  }

  //- rad: new root -> forget expansions, selection, scroll
  RAD_ViewState *vs = rad_view_state_from_cfg(tab);
  RAD_ExplorerState *ex = &vs->ex;
  if(ex->initialized)
  {
    arena_clear(ex->expansions_arena);
    ex->first_expansion = ex->last_expansion = ex->free_expansion = 0;
    MemoryZeroStruct(&ex->scroll_pos);
    ex->cursor = ex->mark = v2s64(0, 0);
    ex->selected_path_size = 0;
    ex->rows_dirty = 1;
  }
}

internal B32
rad_explorer_is_expanded(RAD_ExplorerState *ex, String8 path)
{
  B32 result = 0;
  for(RAD_ExplorerExpansion *e = ex->first_expansion; e != 0; e = e->next)
  {
    if(str8_match(e->path, path, 0))
    {
      result = 1;
      break;
    }
  }
  return result;
}

internal void
rad_explorer_set_expanded(RAD_ExplorerState *ex, String8 path, B32 expanded)
{
  RAD_ExplorerExpansion *existing = 0;
  for(RAD_ExplorerExpansion *e = ex->first_expansion; e != 0; e = e->next)
  {
    if(str8_match(e->path, path, 0))
    {
      existing = e;
      break;
    }
  }
  if(expanded && existing == 0)
  {
    RAD_ExplorerExpansion *e = ex->free_expansion;
    if(e != 0)
    {
      SLLStackPop(ex->free_expansion);
    }
    else
    {
      e = push_array_no_zero(ex->expansions_arena, RAD_ExplorerExpansion, 1);
    }
    MemoryZeroStruct(e);
    e->path = push_str8_copy(ex->expansions_arena, path);
    DLLPushBack(ex->first_expansion, ex->last_expansion, e);
  }
  else if(!expanded && existing != 0)
  {
    DLLRemove(ex->first_expansion, ex->last_expansion, existing);
    SLLStackPush(ex->free_expansion, existing);
  }
  ex->rows_dirty = 1;
}

internal int
rad_explorer_file_info_compare(FileInfo *a, FileInfo *b)
{
  // NOTE(rad): folders first, then by name (case-insensitive, then exact)
  int result = 0;
  B32 a_is_folder = !!(a->props.flags & FilePropertyFlag_IsFolder);
  B32 b_is_folder = !!(b->props.flags & FilePropertyFlag_IsFolder);
  if(a_is_folder != b_is_folder)
  {
    result = (a_is_folder ? -1 : +1);
  }
  for(U64 idx = 0; result == 0 && idx < a->name.size && idx < b->name.size; idx += 1)
  {
    U8 ca = lower_from_char(a->name.str[idx]);
    U8 cb = lower_from_char(b->name.str[idx]);
    result = (ca < cb ? -1 : ca > cb ? +1 : 0);
  }
  if(result == 0)
  {
    result = (a->name.size < b->name.size ? -1 : a->name.size > b->name.size ? +1 : 0);
  }
  if(result == 0)
  {
    result = (str8_is_before(a->name, b->name) ? -1 : str8_is_before(b->name, a->name) ? +1 : 0);
  }
  return result;
}

internal void
rad_explorer_rows_push_folder__(Arena *arena, RAD_ExplorerState *ex, RAD_ExplorerRowList *rows, String8 folder, S32 depth)
{
  Temp scratch = scratch_begin(&arena, 1);

  //- rad: list the folder
  typedef struct InfoNode InfoNode;
  struct InfoNode
  {
    InfoNode *next;
    FileInfo v;
  };
  InfoNode *first_info = 0;
  InfoNode *last_info = 0;
  U64 infos_count = 0;
  FileIter *it = file_iter_begin(scratch.arena, folder, 0);
  for(FileInfo info = {0}; file_iter_next(scratch.arena, it, &info);)
  {
    if(str8_match(info.name, str8_lit(".git"), 0))
    {
      continue;
    }
    InfoNode *n = push_array(scratch.arena, InfoNode, 1);
    n->v = info;
    SLLQueuePush(first_info, last_info, n);
    infos_count += 1;
  }
  file_iter_end(it);

  //- rad: sort: folders first, then by name
  FileInfo *infos = push_array(scratch.arena, FileInfo, infos_count);
  {
    U64 idx = 0;
    for(InfoNode *n = first_info; n != 0; n = n->next, idx += 1)
    {
      infos[idx] = n->v;
    }
  }
  quick_sort(infos, infos_count, sizeof(infos[0]), rad_explorer_file_info_compare);

  //- rad: push rows; recurse into expanded folders
  B32 folder_has_trailing_slash = (folder.size != 0 && (folder.str[folder.size-1] == '/' || folder.str[folder.size-1] == '\\'));
  for EachIndex(idx, infos_count)
  {
    if(rows->count >= RAD_EXPLORER_MAX_ROWS)
    {
      break;
    }
    FileInfo *info = &infos[idx];
    RAD_ExplorerRowNode *n = push_array(arena, RAD_ExplorerRowNode, 1);
    n->v.path = push_str8f(arena, "%S%s%S", folder, folder_has_trailing_slash ? "" : "/", info->name);
    n->v.name = str8_skip_last_slash(n->v.path);
    n->v.depth = depth;
    n->v.is_folder = !!(info->props.flags & FilePropertyFlag_IsFolder);
    n->v.is_expanded = (n->v.is_folder && rad_explorer_is_expanded(ex, n->v.path));
    SLLQueuePush(rows->first, rows->last, n);
    rows->count += 1;
    if(n->v.is_expanded)
    {
      rad_explorer_rows_push_folder__(arena, ex, rows, n->v.path, depth+1);
    }
  }

  scratch_end(scratch);
}

internal void
rad_explorer_rows_build(RAD_ExplorerState *ex, String8 root)
{
  arena_clear(ex->rows_arena);
  ex->rows_root = push_str8_copy(ex->rows_arena, root);
  RAD_ExplorerRowList rows = {0};
  rad_explorer_rows_push_folder__(ex->rows_arena, ex, &rows, ex->rows_root, 0);
  ex->rows = push_array(ex->rows_arena, RAD_ExplorerRow, rows.count);
  ex->rows_count = rows.count;
  {
    U64 idx = 0;
    for(RAD_ExplorerRowNode *n = rows.first; n != 0; n = n->next, idx += 1)
    {
      ex->rows[idx] = n->v;
    }
  }
  ex->rows_build_us = now_time_us();
  ex->rows_dirty = 0;

  //- rad: keep the selection on the same path
  String8 selected_path = str8(ex->selected_path_buffer, ex->selected_path_size);
  if(selected_path.size != 0)
  {
    for EachIndex(idx, ex->rows_count)
    {
      if(str8_match(ex->rows[idx].path, selected_path, 0))
      {
        ex->cursor = ex->mark = v2s64(0, (S64)idx+1);
        break;
      }
    }
  }
  ex->cursor.y = Clamp(0, ex->cursor.y, (S64)ex->rows_count);
  ex->mark.y = Clamp(0, ex->mark.y, (S64)ex->rows_count);
}

internal CFG_Node *
rad_explorer_target_panel_from_tab(CFG_Node *tab)
{
  // NOTE(rad): files open in the last focused panel that does not show an
  // explorer; otherwise in the first other leaf panel; otherwise here.
  Temp scratch = scratch_begin(0, 0);
  CFG_Node *explorer_panel = tab->parent;
  CFG_Node *result = cfg_node_from_id(rad_state->last_editor_panel);
  CFG_PanelTree panel_tree = cfg_panel_tree_from_cfg(scratch.arena, explorer_panel);
  B32 result_is_in_tree = 0;
  CFG_Node *first_other_leaf = &cfg_nil_node;
  for(CFG_PanelNode *p = panel_tree.root;
      p != &cfg_nil_panel_node;
      p = cfg_panel_node_rec__depth_first_pre(panel_tree.root, p).next)
  {
    if(p->first == &cfg_nil_panel_node && p->cfg != explorer_panel)
    {
      if(first_other_leaf == &cfg_nil_node)
      {
        first_other_leaf = p->cfg;
      }
      if(p->cfg == result)
      {
        result_is_in_tree = 1;
      }
    }
  }
  if(!result_is_in_tree)
  {
    result = first_other_leaf;
  }
  if(result == &cfg_nil_node)
  {
    result = explorer_panel;
  }
  scratch_end(scratch);
  return result;
}

internal void
rad_explorer_view_ui(CFG_Node *tab, Rng2F32 rect)
{
  Temp scratch = scratch_begin(0, 0);
  RAD_ViewState *vs = rad_view_state_from_cfg(tab);
  RAD_ExplorerState *ex = &vs->ex;
  if(!ex->initialized)
  {
    ex->initialized = 1;
    ex->expansions_arena = arena_alloc();
    ex->rows_arena = arena_alloc();
    ex->rows_dirty = 1;
  }
  String8 root = rad_explorer_folder_from_tab(tab);

  //////////////////////////////
  //- rad: no folder -> empty state (same layout as the text view's missing
  // file, plus the getting-started view's "Open File" button style)
  //
  if(root.size == 0)
  {
    ui_set_next_flags(UI_BoxFlag_DefaultFocusNav);
    UI_Focus(UI_FocusKind_On) UI_WidthFill UI_HeightFill UI_NamedColumn(str8_lit("empty_explorer"))
      UI_Padding(ui_pct(1, 0)) UI_Focus(UI_FocusKind_Null)
    {
      UI_PrefWidth(ui_children_sum(1)) UI_PrefHeight(ui_em(3, 1))
        UI_Row UI_Padding(ui_pct(1, 0))
        UI_PrefWidth(ui_text_dim(1, 1))
        UI_TagF("weak")
      {
        ui_labelf("No folder open.");
      }
      ui_spacer(ui_em(1.f, 1.f));
      UI_PrefHeight(ui_em(3.75f, 1.f))
        UI_Row
        UI_Padding(ui_pct(1, 0))
        UI_TextAlignment(UI_TextAlign_Center)
        UI_PrefWidth(ui_em(12.f, 1.f))
        UI_CornerRadius(ui_top_font_size()/2.f)
        UI_TagF("pop")
        if(ui_clicked(rad_icon_buttonf(RAD_IconKind_FolderOpenOutline, 0, "Open Folder")))
      {
        rad_cmd(RAD_CmdKind_OpenFolder, .tab = tab->id);
      }
    }
  }

  //////////////////////////////
  //- rad: rebuild rows when stale
  //
  if(root.size != 0 && (ex->rows_dirty || !str8_match(ex->rows_root, root, 0) || now_time_us() >= ex->rows_build_us + RAD_EXPLORER_REFRESH_US))
  {
    rad_explorer_rows_build(ex, root);
  }

  //////////////////////////////
  //- rad: unpack
  //
  F32 row_height_px = ui_top_px_height();
  Vec2F32 rect_dim = dim_2f32(rect);
  F32 contents_width_px = (rect_dim.x - floor_f32(ui_bottom_font_size()*1.5f));
  S64 rows_count = (root.size != 0 ? (S64)ex->rows_count : 0);
  String8 toggle_path = {0};
  String8 open_path = {0};

  //////////////////////////////
  //- rad: keyboard: Left/Right collapse/expand (or move to parent/child),
  // Enter opens a file or toggles a folder
  //
  UI_Focus(UI_FocusKind_On) if(ui_is_focus_active() && rows_count != 0)
  {
    for(UI_Event *evt = 0; ui_next_event(&evt);)
    {
      S64 row_idx = ex->cursor.y - 1;
      if(row_idx < 0 || row_idx >= rows_count)
      {
        continue;
      }
      RAD_ExplorerRow *row = &ex->rows[row_idx];
      if(evt->kind == UI_EventKind_Navigate && evt->delta_unit == UI_EventDeltaUnit_Char &&
         evt->delta_2s32.y == 0 && evt->delta_2s32.x != 0)
      {
        if(evt->delta_2s32.x > 0)
        {
          if(row->is_folder && !row->is_expanded)
          {
            toggle_path = push_str8_copy(scratch.arena, row->path);
          }
          else if(row->is_folder && row_idx+1 < rows_count && ex->rows[row_idx+1].depth > row->depth)
          {
            ex->cursor = ex->mark = v2s64(0, row_idx+2);
          }
        }
        else
        {
          if(row->is_folder && row->is_expanded)
          {
            toggle_path = push_str8_copy(scratch.arena, row->path);
          }
          else for(S64 idx = row_idx-1; idx >= 0; idx -= 1)
          {
            if(ex->rows[idx].depth < row->depth)
            {
              ex->cursor = ex->mark = v2s64(0, idx+1);
              break;
            }
          }
        }
        ui_eat_event(evt);
      }
      else if(evt->kind == UI_EventKind_Press && evt->key == WM_Key_Return && evt->modifiers == 0)
      {
        if(row->is_folder)
        {
          toggle_path = push_str8_copy(scratch.arena, row->path);
        }
        else
        {
          open_path = push_str8_copy(scratch.arena, row->path);
        }
        ui_eat_event(evt);
      }
    }
  }

  //////////////////////////////
  //- rad: build table (port of raddbg's watch view table: scroll list,
  // row boxes, one cell per row)
  //
  if(root.size != 0)
  {
    UI_ScrollListParams scroll_list_params = {0};
    {
      scroll_list_params.flags         = UI_ScrollListFlag_All;
      scroll_list_params.row_height_px = row_height_px;
      scroll_list_params.dim_px        = rect_dim;
      scroll_list_params.cursor_range  = r2s64(v2s64(0, 0), v2s64(0, rows_count));
      scroll_list_params.item_range    = r1s64(0, rows_count);
      scroll_list_params.cursor_min_is_empty_selection[Axis2_Y] = 1;
    }
    Rng1S64 visible_row_rng = {0};
    UI_ScrollListSignal scroll_list_sig = {0};
    UI_Focus(UI_FocusKind_On)
      UI_ScrollList(&scroll_list_params, &ex->scroll_pos.y, &ex->cursor, &ex->mark, &visible_row_rng, &scroll_list_sig)
      UI_Focus(UI_FocusKind_Null)
    {
      ui_set_next_pref_height(ui_children_sum(1));
      ui_set_next_child_layout_axis(Axis2_Y);
      UI_Box *table = ui_build_box_from_string(0, str8_lit("table"));
      UI_Parent(table)
        for(S64 row_idx = visible_row_rng.min; row_idx <= visible_row_rng.max && row_idx < rows_count; row_idx += 1)
      {
        RAD_ExplorerRow *row = &ex->rows[row_idx];
        U64 row_hash = u64_hash_from_str8(row->path);
        B32 row_selected = (ex->cursor.y == row_idx+1);

        //- rad: row title (same layout as raddbg's rd_title_fstrs_from_file_path)
        DR_FStrList fstrs = {0};
        {
          RAD_IconKind icon_kind = (!row->is_folder ? RAD_IconKind_FileOutline :
                                    row->is_expanded ? RAD_IconKind_FolderOpenFilled :
                                    RAD_IconKind_FolderClosedFilled);
          DR_FStrParams params = {rad_font_from_slot(RAD_FontSlot_Main), rad_raster_flags_from_slot(RAD_FontSlot_Main), ui_color_from_name(str8_lit("text")), ui_top_font_size()};
          UI_TagF("weak")
          {
            dr_fstrs_push_new(scratch.arena, &fstrs, &params,
                              rad_icon_kind_text_table[icon_kind],
                              .font = rad_font_from_slot(RAD_FontSlot_Icons),
                              .raster_flags = rad_raster_flags_from_slot(RAD_FontSlot_Icons),
                              .color = ui_color_from_name(str8_lit("text")));
          }
          dr_fstrs_push_new(scratch.arena, &fstrs, &params, str8_lit("  "));
          dr_fstrs_push_new(scratch.arena, &fstrs, &params, row->name);
        }

        //- rad: build row box
        UI_BoxFlags row_flags = UI_BoxFlag_DisableFocusOverlay;
        if(row_idx & 1)
        {
          ui_set_next_tag(str8_lit("alt"));
          row_flags |= UI_BoxFlag_DrawBackground;
        }
        ui_set_next_pref_width(ui_px(contents_width_px, 1.f));
        ui_set_next_pref_height(ui_px(row_height_px, 1.f));
        ui_set_next_focus_hot(row_selected ? UI_FocusKind_On : UI_FocusKind_Off);
        UI_Box *row_box = ui_build_box_from_stringf(row_flags|((row_idx+1 == rows_count)*UI_BoxFlag_DrawSideBottom)|UI_BoxFlag_Clickable, "row_%I64x", row_hash);

        //- rad: build cell
        UI_Signal sig = {0};
        B32 next_expanded = row->is_expanded;
        UI_Parent(row_box)
        {
          UI_Box *cell_box = &ui_nil_box;
          UI_PrefWidth(ui_px(contents_width_px, 1.f))
          {
            ui_set_next_fixed_height(floor_f32(row_height_px));
            cell_box = ui_build_box_from_stringf(UI_BoxFlag_DrawSideLeft, "cell_%I64x", row_hash);
          }
          RAD_CellParams cell_params = {0};
          cell_params.flags        = (RAD_CellFlag_KeyboardClickable|RAD_CellFlag_NoBackground|
                                      (row->is_folder ? RAD_CellFlag_Expander : RAD_CellFlag_ExpanderSpace));
          cell_params.depth        = row->depth;
          cell_params.value_fstrs  = fstrs;
          cell_params.expanded_out = &next_expanded;
          UI_Parent(cell_box)
            UI_FocusHot(row_selected ? UI_FocusKind_On : UI_FocusKind_Off)
            UI_FocusActive(UI_FocusKind_Off)
            RAD_Font(RAD_FontSlot_Code)
            UI_TagF("weak")
          {
            sig = rad_cellf(&cell_params, "###cell_%I64x", row_hash);
          }
        }
        UI_Signal row_sig = ui_signal_from_box(row_box);

        //- rad: interactions: press selects; double-click opens a file or
        // toggles a folder; the expander toggles
        if(ui_pressed(sig) || ui_pressed(row_sig))
        {
          ex->cursor = ex->mark = v2s64(0, row_idx+1);
          rad_cmd(RAD_CmdKind_FocusPanel, .panel = tab->parent->id);
        }
        if(ui_double_clicked(sig) || ui_double_clicked(row_sig))
        {
          if(row->is_folder)
          {
            toggle_path = push_str8_copy(scratch.arena, row->path);
          }
          else
          {
            open_path = push_str8_copy(scratch.arena, row->path);
          }
        }
        if(next_expanded != row->is_expanded)
        {
          toggle_path = push_str8_copy(scratch.arena, row->path);
        }
      }
    }
  }

  //////////////////////////////
  //- rad: remember the selected path (selection survives row rebuilds)
  //
  if(1 <= ex->cursor.y && ex->cursor.y <= rows_count)
  {
    String8 path = ex->rows[ex->cursor.y-1].path;
    ex->selected_path_size = Min(path.size, sizeof(ex->selected_path_buffer));
    MemoryCopy(ex->selected_path_buffer, path.str, ex->selected_path_size);
  }

  //////////////////////////////
  //- rad: apply deferred actions (rows rebuild next frame)
  //
  if(toggle_path.size != 0)
  {
    rad_explorer_set_expanded(ex, toggle_path, !rad_explorer_is_expanded(ex, toggle_path));
    rad_request_frame();
  }
  if(open_path.size != 0)
  {
    rad_cmd(RAD_CmdKind_OpenFile, .panel = rad_explorer_target_panel_from_tab(tab)->id, .file_path = open_path);
  }

  scratch_end(scratch);
}
