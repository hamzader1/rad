// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

////////////////////////////////
//~ rad: Basic Widgets

internal UI_Signal
rad_menu_bar_button(String8 string)
{
  UI_Box *box = ui_build_box_from_string(UI_BoxFlag_DrawText|UI_BoxFlag_DrawBorder|UI_BoxFlag_DrawBackground|UI_BoxFlag_Clickable|UI_BoxFlag_DrawHotEffects, string);
  UI_Signal sig = ui_signal_from_box(box);
  return sig;
}

internal UI_Signal
rad_icon_button(RAD_IconKind kind, FuzzyMatchRangeList *matches, String8 string)
{
  String8 display_string = ui_display_part_from_key_string(string);
  ui_set_next_child_layout_axis(Axis2_X);
  UI_Box *box = ui_build_box_from_string(UI_BoxFlag_Clickable|
                                         UI_BoxFlag_DrawBorder|
                                         UI_BoxFlag_DrawBackground|
                                         UI_BoxFlag_DrawHotEffects|
                                         UI_BoxFlag_DrawActiveEffects,
                                         string);
  UI_Parent(box)
  {
    if(display_string.size == 0)
    {
      ui_spacer(ui_pct(1, 0));
    }
    else
    {
      ui_spacer(ui_em(1.f, 1.f));
    }
    UI_TextAlignment(UI_TextAlign_Center)
      RAD_Font(RAD_FontSlot_Icons)
      UI_PrefWidth(ui_em(2.f, 1.f))
      UI_PrefHeight(ui_pct(1, 0))
      UI_FlagsAdd(UI_BoxFlag_DisableTextTrunc)
      UI_TagF("weak")
      ui_label(rad_icon_kind_text_table[kind]);
    if(display_string.size != 0)
    {
      UI_PrefWidth(ui_pct(1.f, 0.f))
      {
        UI_Box *box = ui_label(display_string).box;
        if(matches != 0)
        {
          ui_box_equip_fuzzy_match_ranges(box, matches);
        }
      }
    }
    if(display_string.size == 0)
    {
      ui_spacer(ui_pct(1, 0));
    }
    else
    {
      ui_spacer(ui_em(1.f, 1.f));
    }
  }
  UI_Signal result = ui_signal_from_box(box);
  return result;
}

internal UI_Signal
rad_icon_buttonf(RAD_IconKind kind, FuzzyMatchRangeList *matches, char *fmt, ...)
{
  Temp scratch = scratch_begin(0, 0);
  va_list args;
  va_start(args, fmt);
  String8 string = push_str8fv(scratch.arena, fmt, args);
  va_end(args);
  UI_Signal sig = rad_icon_button(kind, matches, string);
  scratch_end(scratch);
  return sig;
}

////////////////////////////////
//~ rad: Command Widgets

internal void
rad_cmd_binding_labels(RAD_CmdKind kind)
{
  // NOTE(rad): same box as raddbg's `rd_cmd_binding_buttons` with
  // `RD_CmdBindingButtonFlag_NoEdit` - rebinding is not supported yet.
  Temp scratch = scratch_begin(0, 0);
  CFG_Binding binding = rad_cmd_kind_info_table[kind].default_binding;
  if(binding.key != WM_Key_Null) UI_CornerRadius(ui_top_font_size()*0.5f)
  {
    ui_spacer(ui_em(1.f, 1.f));
    String8 keybinding_str = wm_string_from_modifiers_key(scratch.arena, binding.modifiers, binding.key);
    ui_set_next_text_alignment(UI_TextAlign_Center);
    ui_set_next_group_key(ui_key_zero());
    ui_set_next_pref_width(ui_text_dim(ui_top_font_size()*1.f, 1));
    ui_build_box_from_stringf(UI_BoxFlag_DrawText|
                              UI_BoxFlag_DrawActiveEffects|
                              UI_BoxFlag_DrawHotEffects|
                              UI_BoxFlag_DrawBorder|
                              UI_BoxFlag_DrawBackground,
                              "%S###bind_btn_%S_%x_%x", keybinding_str, rad_cmd_kind_info_table[kind].code_name, binding.key, binding.modifiers);
  }
  scratch_end(scratch);
}

internal UI_Signal
rad_cmd_spec_button(RAD_CmdKind kind)
{
  RAD_CmdKindInfo *info = &rad_cmd_kind_info_table[kind];
  ui_set_next_child_layout_axis(Axis2_X);
  UI_Box *box = ui_build_box_from_stringf(UI_BoxFlag_DrawBorder|
                                          UI_BoxFlag_DrawBackground|
                                          UI_BoxFlag_DrawHotEffects|
                                          UI_BoxFlag_DrawActiveEffects|
                                          UI_BoxFlag_Clickable,
                                          "###cmd_%p", info);
  UI_Parent(box) UI_HeightFill UI_Padding(ui_em(1.f, 1.f))
  {
    if(info->icon_kind != RAD_IconKind_Null)
    {
      RAD_Font(RAD_FontSlot_Icons)
        UI_PrefWidth(ui_em(2.f, 1.f))
        UI_TextAlignment(UI_TextAlign_Center)
        UI_TagF("weak")
      {
        ui_label(rad_icon_kind_text_table[info->icon_kind]);
      }
    }
    UI_PrefWidth(ui_text_dim(10, 1.f))
    {
      UI_Flags(UI_BoxFlag_DrawTextFastpathCodepoint)
        UI_FastpathCodepoint(box->fastpath_codepoint)
        ui_label(info->display_name);
      ui_spacer(ui_pct(1, 0));
      ui_set_next_flags(UI_BoxFlag_Clickable);
      ui_set_next_group_key(ui_key_zero());
      UI_PrefWidth(ui_children_sum(1))
        UI_FontSize(ui_top_font_size()*0.95f) UI_HeightFill
        UI_NamedRow(str8_lit("###bindings"))
        UI_TagF("weak")
        UI_FastpathCodepoint(0)
      {
        rad_cmd_binding_labels(kind);
      }
    }
  }
  UI_Signal sig = ui_signal_from_box(box);
  return sig;
}

internal void
rad_cmd_list_menu_buttons(U64 count, RAD_CmdKind *kinds, U32 *fastpath_codepoints)
{
  for EachIndex(idx, count)
  {
    if(kinds[idx] == RAD_CmdKind_Null)
    {
      UI_TagF("floating") ui_divider(ui_em(1.f, 1.f));
    }
    else
    {
      ui_set_next_fastpath_codepoint(fastpath_codepoints[idx]);
      UI_Signal sig = rad_cmd_spec_button(kinds[idx]);
      if(ui_clicked(sig))
      {
        rad_cmd(kinds[idx]);
        ui_ctx_menu_close();
        if(rad_state->current_window != 0)
        {
          rad_state->current_window->menu_bar_focused = 0;
        }
      }
    }
  }
}

////////////////////////////////
//~ rad: Cells

internal UI_Signal
rad_cell(RAD_CellParams *params, String8 string)
{
  // NOTE(rad): port of raddbg's rd_cell (raddbg/raddbg_widgets.c), the
  // non-editing path: same boxes, flags, sizes and tags.
  ProfBeginFunction();
  Temp scratch = scratch_begin(0, 0);

  //////////////////////////////
  //- rad: unpack visual metrics
  //
  F32 expander_size_px = floor_f32(ui_top_font_size()*2.f);

  //////////////////////////////
  //- rad: make key
  //
  UI_Key key = ui_key_from_string(ui_active_seed_key(), string);

  //////////////////////////////
  //- rad: calculate & push focus
  //
  B32 is_auto_focus_hot = ui_is_key_auto_focus_hot(key);
  B32 is_auto_focus_active = ui_is_key_auto_focus_active(key);
  if(is_auto_focus_hot) { ui_push_focus_hot(UI_FocusKind_On); }
  if(is_auto_focus_active) { ui_push_focus_active(UI_FocusKind_On); }

  //////////////////////////////
  //- rad: build top-level box
  //
  UI_Box *box = ui_build_box_from_key(UI_BoxFlag_MouseClickable|
                                      (!!(params->flags & RAD_CellFlag_KeyboardClickable)*UI_BoxFlag_KeyboardClickable)|
                                      UI_BoxFlag_ClickToFocus|
                                      (!!(params->flags & RAD_CellFlag_Button)*UI_BoxFlag_DrawHotEffects)|
                                      (!!(params->flags & RAD_CellFlag_SingleClickActivate)*UI_BoxFlag_DrawActiveEffects)|
                                      (!(params->flags & RAD_CellFlag_NoBackground)*UI_BoxFlag_DrawBackground)|
                                      (!!(params->flags & RAD_CellFlag_Border)*UI_BoxFlag_DrawBorder)|
                                      ((is_auto_focus_hot || is_auto_focus_active)*UI_BoxFlag_KeyboardClickable),
                                      key);

  //////////////////////////////
  //- rad: build indent
  //
  UI_Parent(box) for(S32 idx = 0; idx < params->depth; idx += 1)
  {
    ui_set_next_flags(UI_BoxFlag_DrawSideLeft);
    ui_spacer(ui_em(1.f, 1.f));
  }

  //////////////////////////////
  //- rad: build expander (or placeholder, or space)
  //
  {
    //- rad: build expander
    if(params->flags & RAD_CellFlag_Expander) UI_PrefWidth(ui_px(expander_size_px, 1.f)) UI_Parent(box)
      UI_Flags(UI_BoxFlag_DrawSideLeft)
      UI_Focus(UI_FocusKind_Off)
    {
      UI_Signal expander_sig = ui_expanderf(params->expanded_out[0], "expander");
      if(ui_pressed(expander_sig))
      {
        params->expanded_out[0] ^= 1;
      }
    }

    //- rad: build expander placeholder
    else if(params->flags & RAD_CellFlag_ExpanderPlaceholder) UI_Parent(box) UI_PrefWidth(ui_px(expander_size_px, 1.f)) UI_Focus(UI_FocusKind_Off)
    {
      UI_TagF("weak")
        UI_Flags(UI_BoxFlag_DrawSideLeft)
        RAD_Font(RAD_FontSlot_Icons)
        UI_TextAlignment(UI_TextAlign_Center)
        ui_label(rad_icon_kind_text_table[RAD_IconKind_Dot]);
    }

    //- rad: build expander space
    else if(params->flags & RAD_CellFlag_ExpanderSpace) UI_Parent(box) UI_Focus(UI_FocusKind_Off)
    {
      UI_Flags(UI_BoxFlag_DrawSideLeft) ui_spacer(ui_px(expander_size_px, 1.f));
    }
  }

  //////////////////////////////
  //- rad: build line edit container box
  //
  UI_Box *edit_box = &ui_nil_box;
  UI_Parent(box) UI_PrefWidth(ui_pct(1, 0))
  {
    if(ui_top_px_height() > ui_top_font_size()*3.f)
    {
      ui_set_next_pref_width(ui_children_sum(1));
      UI_Column UI_Padding(ui_em(1, 0)) UI_Focus(UI_FocusKind_On)
      {
        UI_PrefHeight(ui_em(3.f, 1.f)) UI_CornerRadius(ui_top_font_size()*0.5f)
          edit_box = ui_build_box_from_stringf(UI_BoxFlag_Clickable|
                                               UI_BoxFlag_DisableFocusOverlay,
                                               "edit_box");
      }
      if(ui_top_text_alignment() == UI_TextAlign_Left)
      {
        ui_spacer(ui_em(1.f, 1.f));
      }
    }
    else
    {
      edit_box = ui_build_box_from_stringf(0, "edit_box");
    }
  }

  //////////////////////////////
  //- rad: build scrollable container box
  //
  UI_Box *scrollable_box = &ui_nil_box;
  UI_Parent(edit_box) UI_PrefWidth(ui_children_sum(0))
  {
    scrollable_box = ui_build_box_from_stringf(0, "scroll_box");
  }

  //////////////////////////////
  //- rad: get signal
  //
  UI_Signal sig = ui_signal_from_box(box);

  //////////////////////////////
  //- rad: compute fancy strings
  //
  DR_FStrList fstrs = params->value_fstrs;
  if(fstrs.total_size == 0)
  {
    String8 display_string = ui_display_part_from_key_string(string);
    UI_TagF("weak")
    {
      DR_FStrParams fstr_params = {ui_top_font(), ui_top_text_raster_flags(), ui_color_from_name(str8_lit("text")), ui_top_font_size()};
      dr_fstrs_push_new(scratch.arena, &fstrs, &fstr_params, display_string);
    }
  }

  //////////////////////////////
  //- rad: build scrolled contents
  //
  UI_Parent(scrollable_box)
  {
    if(ui_top_text_alignment() == UI_TextAlign_Left && (params->flags & (RAD_CellFlag_Expander|RAD_CellFlag_ExpanderSpace|RAD_CellFlag_ExpanderPlaceholder)) == 0)
    {
      ui_spacer(ui_em(0.5f, 1.f));
    }
    ui_set_next_pref_width(ui_text_dim(ui_top_font_size()*0.5f, 0));
    UI_Box *text_box = ui_build_box_from_stringf(UI_BoxFlag_DrawText, "###text_box");
    ui_box_equip_display_fstrs(text_box, &fstrs);
  }
  scrollable_box->view_off_target.x = scrollable_box->view_off.x = 0;

  //////////////////////////////
  //- rad: pop focus
  //
  if(is_auto_focus_hot) { ui_pop_focus_hot(); }
  if(is_auto_focus_active) { ui_pop_focus_active(); }

  ProfEnd();
  scratch_end(scratch);
  return sig;
}

internal UI_Signal
rad_cellf(RAD_CellParams *params, char *fmt, ...)
{
  Temp scratch = scratch_begin(0, 0);
  va_list args;
  va_start(args, fmt);
  String8 string = push_str8fv(scratch.arena, fmt, args);
  va_end(args);
  UI_Signal sig = rad_cell(params, string);
  scratch_end(scratch);
  return sig;
}

////////////////////////////////
//~ rad: Code Slice
//
// NOTE(rad): port of raddbg's `rd_code_slice` (raddbg/raddbg_widgets.c). Kept
// verbatim: top-level container, line numbers + their floating background,
// text container + mouse mapping + click/drag selection, cursor scope lines
// and scope-end annotations, per-token fancy strings, search highlights,
// selection rects, cursor + cursor trail. Removed: breakpoint/thread/watch pin
// margins, debug-info line coloring, drag/drop of debugger entities, hover
// evaluation, right-click debugger command menu.

internal RAD_CodeColorSlot
rad_code_color_slot_from_txt_token_kind(TXT_TokenKind kind)
{
  RAD_CodeColorSlot color = RAD_CodeColorSlot_CodeDefault;
  switch(kind)
  {
    default:break;
    case TXT_TokenKind_Keyword:     {color = RAD_CodeColorSlot_CodeKeyword;}break;
    case TXT_TokenKind_Numeric:     {color = RAD_CodeColorSlot_CodeNumeric;}break;
    case TXT_TokenKind_String:      {color = RAD_CodeColorSlot_CodeString;}break;
    case TXT_TokenKind_Char:        {color = RAD_CodeColorSlot_CodeString;}break;
    case TXT_TokenKind_Meta:        {color = RAD_CodeColorSlot_CodeMeta;}break;
    case TXT_TokenKind_LineComment: {color = RAD_CodeColorSlot_CodeComment;}break;
    case TXT_TokenKind_BlockComment:{color = RAD_CodeColorSlot_CodeComment;}break;
    case TXT_TokenKind_Symbol:      {color = RAD_CodeColorSlot_CodeDelimiterOperator;}break;
  }
  return color;
}

internal Vec4F32
rad_rgba_from_code_color_slot(RAD_CodeColorSlot slot)
{
  Vec4F32 result = {0};
  if(rad_state->current_window != 0)
  {
    result = rad_state->current_window->theme_code_colors[slot];
  }
  return result;
}

internal RAD_CodeSliceSignal
rad_code_slice(RAD_CodeSliceParams *params, U64 *cursor, U64 *mark, S64 *preferred_column, String8 string)
{
  RAD_CodeSliceSignal result = {0};
  ProfBeginFunction();
  Temp scratch = scratch_begin(0, 0);
  B32 is_focused = ui_is_focus_active();
  F32 line_num_padding_px = ui_top_font_size()*1.f;
  RAD_Settings *settings = &rad_state->settings;
  B32 do_scope_lines = settings->cursor_scope_lines;
  B32 do_scope_end_annotations = settings->cursor_scope_end_annotations;
  B32 do_cursor_trail = settings->animations && settings->cursor_trail;
  Vec4F32 pop_color = {0};
  UI_TagF("pop")
  {
    pop_color = ui_color_from_name(str8_lit("background"));
  }
  Vec4F32 highlight_color = {0};
  UI_TagF("focus")
  {
    highlight_color = ui_color_from_name(str8_lit("border"));
  }

  //////////////////////////////
  //- rad: build top-level container
  //
  UI_Box *top_container_box = &ui_nil_box;
  {
    ui_set_next_child_layout_axis(Axis2_X);
    ui_set_next_pref_width(ui_px(params->line_text_max_width_px, 1));
    ui_set_next_pref_height(ui_children_sum(1));
    top_container_box = ui_build_box_from_string(UI_BoxFlag_DisableFocusEffects|UI_BoxFlag_DrawBorder, string);
  }

  //////////////////////////////
  //- rad: build line numbers
  //
  if(params->flags & RAD_CodeSliceFlag_LineNums) UI_Parent(top_container_box) ProfScope("build line numbers") UI_Focus(UI_FocusKind_Off)
    UI_TagF("floating")
  {
    Rng1U64 select_rng = r1u64(*cursor, *mark);
    ui_set_next_fixed_x(floor_f32(params->margin_float_off_px));
    ui_set_next_pref_width(ui_px(params->line_num_width_px, 1.f));
    ui_set_next_pref_height(ui_px(params->line_height_px*(dim_1s64(params->line_num_range)+1), 1.f));
    ui_set_next_flags(UI_BoxFlag_DrawSideRight);
    UI_Column
      UI_PrefHeight(ui_px(params->line_height_px, 1.f))
      RAD_Font(RAD_FontSlot_Code)
      UI_FontSize(params->font_size)
      UI_CornerRadius(0)
    {
      U64 line_idx = 0;
      for(S64 line_num = params->line_num_range.min;
          line_num <= params->line_num_range.max;
          line_num += 1, line_idx += 1)
      {
        Rng1U64 line_range = params->line_ranges[line_idx];
        B32 line_is_selected = (dim_1u64(intersect_1u64(select_rng, line_range)) != 0) || (line_range.min <= *cursor && *cursor <= line_range.max);
        UI_TagF(line_is_selected ? "" : "weak")
          ui_build_box_from_stringf(UI_BoxFlag_DrawText, "%I64u##line_num", line_num);
      }
    }
  }

  //////////////////////////////
  //- rad: build background for line numbers & margins
  //
  {
    UI_Parent(top_container_box) UI_TagF("floating")
    {
      ui_set_next_pref_width(ui_px(params->line_num_width_px, 1));
      ui_set_next_pref_height(ui_px(params->line_height_px*(dim_1s64(params->line_num_range)+1), 1.f));
      ui_set_next_fixed_x(floor_f32(params->margin_float_off_px));
      ui_build_box_from_key(UI_BoxFlag_DrawBackgroundBlur|UI_BoxFlag_DrawBackground|UI_BoxFlag_DrawDropShadow, ui_key_zero());
    }
  }

  //////////////////////////////
  //- rad: build main text container box, for mouse interaction on both lines & line numbers
  //
  UI_Box *text_container_box = &ui_nil_box;
  UI_Parent(top_container_box) UI_Focus(UI_FocusKind_Off)
  {
    ui_set_next_hover_cursor(WM_Cursor_IBar);
    ui_set_next_pref_height(ui_px(params->line_height_px*(dim_1s64(params->line_num_range)+1), 1.f));
    text_container_box = ui_build_box_from_string(UI_BoxFlag_Clickable*!!(params->flags & RAD_CodeSliceFlag_Clickable), str8_lit("text_container"));
  }

  //////////////////////////////
  //- rad: mouse -> text coordinates
  //
  U64 mouse_off = 0;
  U64 mouse_y_line_idx = 0;
  ProfScope("mouse -> text coordinates")
  {
    Vec2F32 mouse = ui_mouse();

    // rad: mouse y => index
    S64 mouse_y_line_idx_signed = ((mouse.y - text_container_box->rect.y0) / params->line_height_px);
    mouse_y_line_idx_signed = Clamp(0, mouse_y_line_idx_signed, dim_1s64(params->line_num_range)-1);
    mouse_y_line_idx = (U64)mouse_y_line_idx_signed;

    // rad: index => line num
    S64 line_num = (params->line_num_range.min + mouse_y_line_idx);
    Rng1U64 line_range = (params->line_num_range.min <= line_num && line_num <= params->line_num_range.max) ? (params->line_ranges[mouse_y_line_idx]) : r1u64(0, 0);
    String8 line_string = (params->line_num_range.min <= line_num && line_num <= params->line_num_range.max) ? (params->line_text[mouse_y_line_idx]) : str8_zero();

    // rad: mouse x * string => line offset
    U64 mouse_line_off = fnt_char_pos_from_tag_size_string_p(params->font, params->font_size, 0, params->tab_size, line_string, mouse.x-text_container_box->rect.x0-params->line_num_width_px-line_num_padding_px);

    // rad: compute
    mouse_off = line_range.min + mouse_line_off;

    // rad: clamp
    {
      Rng1U64 legal_range = r1u64(params->line_ranges[0].min, params->line_ranges[dim_1s64(params->line_num_range)].max);
      mouse_off = clamp_1u64(legal_range, mouse_off);
    }

    result.mouse_off = mouse_off;
  }

  //////////////////////////////
  //- rad: mouse point -> mouse token range, mouse line range
  //
  Rng1U64 mouse_token_rng = r1u64(mouse_off, mouse_off);
  Rng1U64 mouse_line_rng = r1u64(mouse_off, mouse_off);
  if(mouse_y_line_idx < (U64)dim_1s64(params->line_num_range))
  {
    TXT_TokenArray *line_tokens = &params->line_tokens[mouse_y_line_idx];
    Rng1U64 line_range = params->line_ranges[mouse_y_line_idx];
    for(U64 line_token_idx = 0; line_token_idx < line_tokens->count; line_token_idx += 1)
    {
      TXT_Token *line_token = &line_tokens->v[line_token_idx];
      if(contains_1u64(line_token->range, mouse_off))
      {
        Rng1U64 line_token_range_clamped = intersect_1u64(line_token->range, line_range);
        mouse_token_rng = r1u64(line_token_range_clamped.min, line_token_range_clamped.max);
        break;
      }
    }
    mouse_line_rng = line_range;
  }

  //////////////////////////////
  //- rad: interact with text box
  //
  B32 search_query_invalidated = 0;
  UI_Signal text_container_sig = ui_signal_from_box(text_container_box);
  {
    //- rad: determine mouse drag range
    Rng1U64 mouse_drag_rng = r1u64(mouse_off, mouse_off);
    if(text_container_sig.f & UI_SignalFlag_LeftTripleDragging)
    {
      mouse_drag_rng = mouse_line_rng;
    }
    else if(text_container_sig.f & UI_SignalFlag_LeftDoubleDragging)
    {
      mouse_drag_rng = mouse_token_rng;
    }

    //- rad: clicking/dragging over the text container
    if(ui_dragging(text_container_sig))
    {
      if(ui_pressed(text_container_sig))
      {
        *cursor = mouse_drag_rng.max;
        *mark = mouse_drag_rng.min;
      }
      if(mouse_off < *mark)
      {
        *cursor = mouse_drag_rng.min;
      }
      else
      {
        *cursor = mouse_drag_rng.max;
      }
      *preferred_column = *cursor - mouse_line_rng.min;
    }

    //- rad: dragging will invalidate the search string, so we don't want to draw it while dragging/releasing
    if(ui_dragging(text_container_sig) || ui_released(text_container_sig))
    {
      search_query_invalidated = 1;
    }

    //- rad: commit text container signal to main output
    result.base = text_container_sig;
  }

  //////////////////////////////
  //- rad: cursor -> scope info
  //
  // NOTE(rad): scope info is computed from the unpatched base text, so it is
  // only used while the buffer has no edits (until scopes are recomputed for
  // patched text).
  //
  TXT_ScopeNode *cursor_scope_node = &txt_scope_node_nil;
  if(params->text_info != 0 && params->patches->first == 0)
  {
    cursor_scope_node = txt_scope_node_from_info_off(params->text_info, *cursor);
  }

  //////////////////////////////
  //- rad: equip cursor scope rendering info
  //
  if(do_scope_lines && cursor_scope_node != &txt_scope_node_nil)
  {
    F32 scope_line_thickness = params->font_size*0.1f;
    scope_line_thickness = Max(scope_line_thickness, 1.f);
    DR_Bucket *bucket = dr_bucket_make();
    DR_BucketScope(bucket)
    {
      Vec2F32 text_base_pos = v2f32(text_container_box->rect.x0 + params->line_num_width_px + line_num_padding_px,
                                    text_container_box->rect.y0);
      F32 ancestor_chain_depth = 0;
      for(TXT_ScopeNode *scope_n = cursor_scope_node;
          scope_n != &txt_scope_node_nil;
          scope_n = txt_scope_node_from_info_num(params->text_info, scope_n->parent_num), ancestor_chain_depth += 1)
      {
        Vec4F32 scope_line_color = highlight_color;
        F32 scope_line_color_target = highlight_color.w;
        scope_line_color_target *= 1 - ancestor_chain_depth / 6.f;
        scope_line_color_target = Max(0.2f, scope_line_color_target);
        F32 scope_line_color_t = ui_anim(ui_key_from_stringf(text_container_box->key, "###scope_depth_%I64x_%I64x", scope_n->token_idx_range.min, scope_n->token_idx_range.max), scope_line_color_target, .rate = rad_state->menu_animation_rate__slow);
        scope_line_color.w = scope_line_color_t*0.5f;
        Rng1U64 token_idx_range = scope_n->token_idx_range;
        Rng1U64 off_range = r1u64(params->text_info->tokens.v[token_idx_range.min].range.min, params->text_info->tokens.v[token_idx_range.max].range.min);
        TxtRng txt_range = txt_rng(txt_pt_from_off__linear_scan(params->text_info, params->patches, off_range.min), txt_pt_from_off__linear_scan(params->text_info, params->patches, off_range.max));

        //- rad: single-line scopes (underline)
        if(txt_range.min.line == txt_range.max.line && contains_1s64(params->line_num_range, txt_range.min.line))
        {
          S64 line_num = txt_range.min.line;
          U64 line_idx = (U64)(line_num - params->line_num_range.min);
          String8 line_string = params->line_text[line_idx];
          Rng1U64 line_off_range = r1u64(off_range.min - params->line_ranges[line_idx].min, off_range.max+1 - params->line_ranges[line_idx].min);
          Rng1F32 x_px_range = r1f32(fnt_dim_from_tag_size_string(params->font, params->font_size, 0, params->tab_size, str8_prefix(line_string, line_off_range.min)).x,
                                     fnt_dim_from_tag_size_string(params->font, params->font_size, 0, params->tab_size, str8_prefix(line_string, line_off_range.max)).x);
          F32 line_y = line_idx*params->line_height_px;
          Rng2F32 underline_rect = r2f32p(text_base_pos.x + x_px_range.min,
                                          text_base_pos.y + line_y + params->line_height_px*0.5f,
                                          text_base_pos.x + x_px_range.max+1,
                                          text_base_pos.y + line_y + params->line_height_px + params->font_size*0.1f);
          F32 midpoint = center_1f32(r1f32(underline_rect.x0, underline_rect.x1));
          F32 t = ui_anim(ui_key_from_stringf(text_container_box->key, "###scope_%I64x_%I64x", scope_n->token_idx_range.min, scope_n->token_idx_range.max), 1.f, .rate = rad_state->catchall_animation_rate);
          Rng2F32 underline_clip = {0};
          underline_clip.x0 = mix_1f32(midpoint, underline_rect.x0 - params->font_size, t);
          underline_clip.x1 = mix_1f32(midpoint, underline_rect.x1 + params->font_size, t);
          underline_clip.y0 = underline_rect.y0 + (underline_rect.y1 - underline_rect.y0) * 0.65f;
          underline_clip.y1 = 10000;
          DR_ClipScope(underline_clip)
          {
            dr_rect(underline_rect, scope_line_color, params->font_size*0.1f, scope_line_thickness, 1.f);
          }
        }

        //- rad: cross-line scopes
        if(txt_range.min.line != txt_range.max.line && params->line_num_range.max > txt_range.min.line && params->line_num_range.min < txt_range.max.line)
        {
          String8 opener_line = txt_string_from_info_data_line_num(params->text_info, params->text_data, txt_range.min.line);
          String8 closer_line = txt_string_from_info_data_line_num(params->text_info, params->text_data, txt_range.max.line);
          String8 opener_line_pre_opener = str8_prefix(opener_line, txt_range.min.column-1);
          String8 closer_line_pre_closer = str8_prefix(closer_line, txt_range.max.column-1);
          F32 opener_line_pre_opener_px = fnt_dim_from_tag_size_string(params->font, params->font_size, 0, params->tab_size, opener_line_pre_opener).x;
          F32 closer_line_pre_closer_px = fnt_dim_from_tag_size_string(params->font, params->font_size, 0, params->tab_size, closer_line_pre_closer).x;
          F32 indent_depth_px = Min(opener_line_pre_opener_px, closer_line_pre_closer_px);
          Rng1F32 scope_range_y_px = r1f32(0, dim_2f32(text_container_box->rect).y);
          if(contains_1s64(params->line_num_range, txt_range.min.line))
          {
            scope_range_y_px.min = (txt_range.min.line - params->line_num_range.min) * params->line_height_px;
          }
          if(contains_1s64(params->line_num_range, txt_range.max.line))
          {
            scope_range_y_px.max = ((txt_range.max.line - params->line_num_range.min) + 1) * params->line_height_px;
          }
          F32 midpoint = center_1f32(scope_range_y_px);
          F32 t = ui_anim(ui_key_from_stringf(text_container_box->key, "###scope_%I64x_%I64x", scope_n->token_idx_range.min, scope_n->token_idx_range.max), 1.f, .rate = rad_state->catchall_animation_rate);
          Rng2F32 scope_rect = r2f32p(text_base_pos.x + indent_depth_px - params->font_size*0.2f,
                                      text_base_pos.y + scope_range_y_px.min,
                                      text_base_pos.x + indent_depth_px - params->font_size*0.2f + params->font_size*1.f,
                                      text_base_pos.y + scope_range_y_px.max);
          Rng2F32 scope_clip_rect = {0};
          {
            scope_clip_rect.x0 = scope_rect.x0 - params->font_size*10.f;
            scope_clip_rect.x1 = scope_rect.x0 + (scope_rect.x1 - scope_rect.x0)*0.4f;
            scope_clip_rect.y0 = mix_1f32(midpoint, scope_rect.y0 - params->font_size*0.1f, t);
            scope_clip_rect.y1 = mix_1f32(midpoint, scope_rect.y1 + params->font_size*0.1f, t);
          }
          DR_ClipScope(scope_clip_rect)
          {
            dr_rect(scope_rect, scope_line_color, params->font_size*0.1f, scope_line_thickness, 1.f);
          }
        }

        //- rad: scope ending annotations
        if(do_scope_end_annotations && txt_range.min.line != txt_range.max.line && contains_1s64(params->line_num_range, txt_range.max.line)) UI_TagF("weak")
        {
          String8 opener_line = str8_skip_chop_whitespace(txt_string_from_info_data_line_num(params->text_info, params->text_data, txt_range.min.line));
          String8 scope_title_string = opener_line;
          if(str8_match(opener_line, str8_lit("{"), 0) ||
             str8_match(opener_line, str8_lit("["), 0) ||
             str8_match(opener_line, str8_lit("("), 0))
          {
            scope_title_string = str8_skip_chop_whitespace(txt_string_from_info_data_line_num(params->text_info, params->text_data, txt_range.min.line-1));
          }
          if(!str8_match(scope_title_string, str8_lit("{"), 0) &&
             !str8_match(scope_title_string, str8_lit("["), 0) &&
             !str8_match(scope_title_string, str8_lit("("), 0))
          {
            F32 t = ui_anim(ui_key_from_stringf(text_container_box->key, "###scope_end_annotation_%I64x_%I64x", scope_n->token_idx_range.min, scope_n->token_idx_range.max), 1.f, .rate = rad_state->catchall_animation_rate);
            String8 closer_line = txt_string_from_info_data_line_num(params->text_info, params->text_data, txt_range.max.line);
            F32 closer_line_px = fnt_dim_from_tag_size_string(params->font, params->font_size, 0, params->tab_size, closer_line).x;
            Vec4F32 color = ui_color_from_name(str8_lit("text"));
            color.w *= 0.5f*t;
            dr_text(params->font, params->font_size * 0.85f, 0, 0, ui_top_text_raster_flags(),
                    v2f32(text_base_pos.x + closer_line_px + ui_top_font_size()*0.5f*t,
                          text_base_pos.y + (txt_range.max.line - params->line_num_range.min) * params->line_height_px + params->line_height_px*0.7f),
                    color, scope_title_string);
          }
        }
      }
    }
    ui_box_equip_draw_bucket(text_container_box, bucket);
  }

  //////////////////////////////
  //- rad: produce fancy strings for each line
  //
  DR_FStrList *lines_fstrs = push_array(scratch.arena, DR_FStrList, dim_1s64(params->line_num_range)+1);
  {
    DR_FStrParams fstr_params =
    {
      params->font,
      rad_raster_flags_from_slot(RAD_FontSlot_Code),
      rad_rgba_from_code_color_slot(RAD_CodeColorSlot_CodeDefault),
      params->font_size,
    };
    U64 line_idx = 0;
    for(S64 line_num = params->line_num_range.min;
        line_num <= params->line_num_range.max;
        line_num += 1, line_idx += 1)
    {
      String8 line_string = params->line_text[line_idx];
      Rng1U64 line_range = params->line_ranges[line_idx];
      TXT_TokenArray *line_tokens = &params->line_tokens[line_idx];
      DR_FStrList fstrs = {0};
      if(line_tokens->count == 0)
      {
        dr_fstrs_push_new(scratch.arena, &fstrs, &fstr_params, line_string);
      }
      else
      {
        TXT_Token *line_tokens_first = line_tokens->v;
        TXT_Token *line_tokens_opl = line_tokens->v + line_tokens->count;
        for(TXT_Token *token = line_tokens_first; token < line_tokens_opl; token += 1)
        {
          // rad: token -> token string
          String8 token_string = {0};
          {
            Rng1U64 token_range = r1u64(0, line_string.size);
            if(token->range.min > line_range.min)
            {
              token_range.min += token->range.min-line_range.min;
            }
            if(token->range.max < line_range.max)
            {
              token_range.max = token->range.max-line_range.min;
            }
            token_string = str8_substr(line_string, token_range);
          }

          // rad: token -> token color
          RAD_CodeColorSlot token_color_slot = rad_code_color_slot_from_txt_token_kind(token->kind);
          Vec4F32 token_color = rad_rgba_from_code_color_slot(token_color_slot);

          // rad: scope endpoints enclosing cursor -> highlight
          for(TXT_ScopeNode *scope_n = cursor_scope_node;
              scope_n != &txt_scope_node_nil;
              scope_n = txt_scope_node_from_info_num(params->text_info, scope_n->parent_num))
          {
            if(params->text_info->tokens.v[scope_n->token_idx_range.min].range.min == token->range.min ||
               params->text_info->tokens.v[scope_n->token_idx_range.max].range.min == token->range.min)
            {
              token_color = highlight_color;
              break;
            }
          }

          // rad: push fancy string
          dr_fstrs_push_new(scratch.arena, &fstrs, &fstr_params, token_string, .color = token_color);
        }
      }
      lines_fstrs[line_idx] = fstrs;
    }
  }

  //////////////////////////////
  //- rad: (cursor*mark) -> list(text_range*color)
  //
  typedef struct TxtRngColorPairNode TxtRngColorPairNode;
  struct TxtRngColorPairNode
  {
    TxtRngColorPairNode *next;
    Rng1U64 range;
    Vec4F32 color;
  };
  TxtRngColorPairNode *first_txt_rng_color_pair = 0;
  TxtRngColorPairNode *last_txt_rng_color_pair = 0;
  {
    TxtRngColorPairNode *n = push_array(scratch.arena, TxtRngColorPairNode, 1);
    n->range = r1u64(*cursor, *mark);
    n->color = ui_color_from_name(str8_lit("selection"));
    SLLQueuePush(first_txt_rng_color_pair, last_txt_rng_color_pair, n);
  }

  //////////////////////////////
  //- rad: build line numbers region (line number interaction should be basically identical to lines)
  //
  if(params->flags & RAD_CodeSliceFlag_LineNums) UI_Parent(text_container_box) ProfScope("build line number interaction box") UI_Focus(UI_FocusKind_Off)
  {
    ui_set_next_pref_width(ui_px(params->line_num_width_px, 1.f));
    ui_set_next_pref_height(ui_px(params->line_height_px*(dim_1s64(params->line_num_range)+1), 1.f));
    ui_build_box_from_key(0, ui_key_zero());
  }

  //////////////////////////////
  //- rad: build line text
  //
  UI_Parent(text_container_box) ProfScope("build line text") UI_Focus(UI_FocusKind_Off)
  {
    ui_set_next_pref_height(ui_px(params->line_height_px*(dim_1s64(params->line_num_range)+1), 1.f));
    UI_WidthFill
      UI_Column
      UI_PrefHeight(ui_px(params->line_height_px, 1.f))
      RAD_Font(RAD_FontSlot_Code)
      UI_FontSize(params->font_size)
      UI_CornerRadius(0)
    {
      U64 line_idx = 0;
      for(S64 line_num = params->line_num_range.min;
          line_num <= params->line_num_range.max; line_num += 1, line_idx += 1)
      {
        String8 line_string = params->line_text[line_idx];
        Rng1U64 line_range = params->line_ranges[line_idx];
        DR_FStrList line_fstrs = lines_fstrs[line_idx];
        ui_set_next_text_padding(line_num_padding_px);
        UI_Key line_key = ui_key_from_stringf(top_container_box->key, "ln_%I64x", line_num);
        ui_set_next_tab_size(params->tab_size);
        UI_Box *line_box = ui_build_box_from_key(UI_BoxFlag_DisableTextTrunc|UI_BoxFlag_DrawText|UI_BoxFlag_DisableIDString, line_key);
        DR_Bucket *line_bucket = dr_bucket_make();
        dr_push_bucket(line_bucket);
        ui_box_equip_display_fstrs(line_box, &line_fstrs);

        // rad: extra rendering for strings that are currently being searched for
        if(!search_query_invalidated && params->search_query.size != 0)
        {
          for(U64 needle_pos = 0; needle_pos < line_string.size;)
          {
            needle_pos = str8_find_needle(line_string, needle_pos, params->search_query, StringMatchFlag_CaseInsensitive);
            if(needle_pos < line_string.size)
            {
              Rng1U64 match_range = r1u64(needle_pos, needle_pos+params->search_query.size);
              Rng1F32 match_column_pixel_off_range =
              {
                fnt_dim_from_tag_size_string(line_box->font, line_box->font_size, 0, params->tab_size, str8_prefix(line_string, match_range.min)).x,
                fnt_dim_from_tag_size_string(line_box->font, line_box->font_size, 0, params->tab_size, str8_prefix(line_string, match_range.max)).x,
              };
              Rng2F32 match_rect =
              {
                line_box->rect.x0+line_num_padding_px+match_column_pixel_off_range.min,
                line_box->rect.y0,
                line_box->rect.x0+line_num_padding_px+match_column_pixel_off_range.max+2.f,
                line_box->rect.y1,
              };
              Vec4F32 color = pop_color;
              if(!is_focused)
              {
                color.w *= 0.5f;
              }
              color.w *= 0.2f;
              dr_rect(match_rect, color, 4.f, 0, 1.f);
              needle_pos += 1;
            }
          }
        }

        // rad: extra rendering for list(text_range*color)
        {
          Rng1U64 prev_line_range = (line_idx > 0) ? params->line_ranges[line_idx-1] : r1u64(0, 0);
          Rng1U64 next_line_range = (line_idx+1 < dim_1s64(params->line_num_range)) ? params->line_ranges[line_idx+1] : r1u64(0, 0);
          for(TxtRngColorPairNode *n = first_txt_rng_color_pair; n != 0; n = n->next)
          {
            Rng1U64 select_range = n->range;
            Rng1U64 select_range_in_line = intersect_1u64(select_range, line_range);
            if(select_range_in_line.min < select_range_in_line.max)
            {
              Rng1U64 select_range_in_prev_line = intersect_1u64(prev_line_range, select_range);
              Rng1U64 select_range_in_next_line = intersect_1u64(next_line_range, select_range);
              B32 prev_line_good = (select_range_in_prev_line.min < select_range_in_prev_line.max);
              B32 next_line_good = (select_range_in_next_line.min < select_range_in_next_line.max);
              Rng1F32 select_column_pixel_off_range =
              {
                fnt_dim_from_tag_size_string(line_box->font, line_box->font_size, 0, params->tab_size, str8_prefix(line_string, select_range_in_line.min - line_range.min)).x,
                fnt_dim_from_tag_size_string(line_box->font, line_box->font_size, 0, params->tab_size, str8_prefix(line_string, select_range_in_line.max - line_range.min)).x,
              };
              Rng2F32 select_rect =
              {
                line_box->rect.x0+line_num_padding_px+select_column_pixel_off_range.min-2.f,
                floor_f32(line_box->rect.y0) - 1.f,
                line_box->rect.x0+line_num_padding_px+select_column_pixel_off_range.max+2.f,
                ceil_f32(line_box->rect.y1) + 1.f,
              };
              Vec4F32 color = n->color;
              if(!is_focused)
              {
                color.w *= 0.5f;
              }
              F32 rounded_radius = params->font_size*0.4f;
              R_Rect2DInst *inst = dr_rect(select_rect, color, rounded_radius, 0, 1);
              Rng1U64 prev_line_selection_off_range = r1u64(select_range_in_prev_line.min - prev_line_range.min, select_range_in_prev_line.max - prev_line_range.min);
              Rng1U64 next_line_selection_off_range = r1u64(select_range_in_next_line.min - next_line_range.min, select_range_in_next_line.max - next_line_range.min);
              Rng1U64 crnt_line_selection_off_range = r1u64(select_range_in_line.min - line_range.min, select_range_in_line.max - line_range.min);
              inst->corner_radii[Corner_00] = !prev_line_good || prev_line_selection_off_range.min > crnt_line_selection_off_range.min ? rounded_radius : 0.f;
              inst->corner_radii[Corner_10] = (!prev_line_good || crnt_line_selection_off_range.max > prev_line_selection_off_range.max || crnt_line_selection_off_range.max < prev_line_selection_off_range.min) ? rounded_radius : 0.f;
              inst->corner_radii[Corner_01] = (!next_line_good || next_line_selection_off_range.min > crnt_line_selection_off_range.min || next_line_selection_off_range.max < crnt_line_selection_off_range.min) ? rounded_radius : 0.f;
              inst->corner_radii[Corner_11] = !next_line_good || crnt_line_selection_off_range.max > next_line_selection_off_range.max ? rounded_radius : 0.f;
            }
          }
        }

        // rad: extra rendering for cursor position
        if(line_range.min <= *cursor && *cursor <= line_range.max)
        {
          Vec2F32 advance = fnt_dim_from_tag_size_string(line_box->font, line_box->font_size, 0, params->tab_size, str8_prefix(line_string, *cursor - line_range.min));
          F32 cursor_y = text_container_box->rect.y0 + line_idx*params->line_height_px - params->font_size*0.125f;
          F32 cursor_y__animated = ui_anim(ui_key_from_stringf(text_container_box->key, "cursor_y_px"), cursor_y, .initial = cursor_y);
          F32 cursor_off_pixels = advance.x;
          F32 cursor_off_pixels__animated = ui_anim(ui_key_from_stringf(text_container_box->key, "cursor_off_px"), cursor_off_pixels, .initial = cursor_off_pixels);
          F32 cursor_thickness = ClampBot(1.f, floor_f32(line_box->font_size/10.f));
          Rng2F32 cursor_rect =
          {
            ui_box_text_position(line_box).x+cursor_off_pixels,
            line_box->rect.y0-params->font_size*0.125f,
            ui_box_text_position(line_box).x+cursor_off_pixels+cursor_thickness,
            line_box->rect.y1+params->font_size*0.125f,
          };
          Rng1F32 trail_off_span = r1f32(cursor_off_pixels__animated, cursor_off_pixels);
          Rng2F32 trail_rect =
          {
            ui_box_text_position(line_box).x+trail_off_span.min,
            line_box->rect.y0-params->font_size*0.125f,
            ui_box_text_position(line_box).x+trail_off_span.max,
            line_box->rect.y1+params->font_size*0.125f,
          };
          Vec4F32 cursor_color = ui_color_from_name(str8_lit("cursor"));
          Vec4F32 trail_color = cursor_color;
          if(!is_focused)
          {
            cursor_color.w *= 0.5f;
          }
          trail_color.w *= 0.25f;
          dr_rect(cursor_rect, cursor_color, 1.f, 0, 0.f);
          if(do_cursor_trail && !ui_key_match(ui_active_key(UI_MouseButtonKind_Left), text_container_box->key))
          {
            R_Rect2DInst *trail_inst = dr_rect(trail_rect, trail_color, ui_top_font_size()*0.2f, 0, 1.f);
            trail_inst->shear = cursor_y - cursor_y__animated;
            if(cursor_off_pixels > cursor_off_pixels__animated)
            {
              trail_inst->dst = shift_2f32(trail_inst->dst, v2f32(0, -trail_inst->shear));
              trail_inst->colors[Corner_00].w *= 0.1f;
              trail_inst->colors[Corner_01].w *= 0.1f;
            }
            else
            {
              trail_inst->shear *= -1;
              trail_inst->colors[Corner_10].w *= 0.1f;
              trail_inst->colors[Corner_11].w *= 0.1f;
            }
          }
        }

        // rad: equip bucket
        if(line_bucket->passes.count != 0)
        {
          ui_box_equip_draw_bucket(line_box, line_bucket);
        }

        dr_pop_bucket();
      }
    }
  }

  scratch_end(scratch);
  ProfEnd();
  return result;
}

internal RAD_CodeSliceSignal
rad_code_slicef(RAD_CodeSliceParams *params, U64 *cursor, U64 *mark, S64 *preferred_column, char *fmt, ...)
{
  Temp scratch = scratch_begin(0, 0);
  va_list args;
  va_start(args, fmt);
  String8 string = push_str8fv(scratch.arena, fmt, args);
  RAD_CodeSliceSignal sig = rad_code_slice(params, cursor, mark, preferred_column, string);
  va_end(args);
  scratch_end(scratch);
  return sig;
}
