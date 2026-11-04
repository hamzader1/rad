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

internal void
rad_loading_overlay(Rng2F32 rect, F32 loading_t, U64 progress_v, U64 progress_v_target)
{
  // NOTE(rad): port of raddbg's rd_loading_overlay
  if(loading_t >= 0.001f) UI_Focus(UI_FocusKind_Off)
  {
    // rad: set up dimensions
    F32 width = ui_top_font_size() * 10;
    F32 height = ui_top_font_size() * 1.f;
    F32 min_thickness = ui_top_font_size()/2;
    F32 trail = ui_top_font_size() * 4;
    F32 t = pow_f32(sin_f32((F32)rad_state->time_in_seconds / 1.8f), 2.f);
    F64 v = 1.f - abs_f32(0.5f - t);

    // rad: build indicator
    UI_CornerRadius(height/3.f) UI_Transparency(1-loading_t)
    {
      // rad: rects
      Rng2F32 indicator_region_rect =
        r2f32p((rect.x0 + rect.x1)/2 - width/2  - rect.x0,
               (rect.y0 + rect.y1)/2 - height/2 - rect.y0,
               (rect.x0 + rect.x1)/2 + width/2  - rect.x0,
               (rect.y0 + rect.y1)/2 + height/2 - rect.y0);
      Rng2F32 indicator_rect =
        r2f32p(indicator_region_rect.x0 + width*t - min_thickness/2 - trail*v,
               indicator_region_rect.y0,
               indicator_region_rect.x0 + width*t + min_thickness/2 + trail*v,
               indicator_region_rect.y1);
      indicator_rect.x0 = Clamp(indicator_region_rect.x0, indicator_rect.x0, indicator_region_rect.x1);
      indicator_rect.x1 = Clamp(indicator_region_rect.x0, indicator_rect.x1, indicator_region_rect.x1);
      indicator_rect = pad_2f32(indicator_rect, -1.f);

      // rad: the view has loading *progress* info? -> draw extra progress layer
      if(progress_v != progress_v_target) UI_TagF("drop_site")
      {
        F64 pct_done_f64 = ((F64)progress_v/(F64)progress_v_target);
        F32 pct_done = (F32)pct_done_f64;
        Rng2F32 pct_rect = r2f32p(indicator_region_rect.x0,
                                  indicator_region_rect.y0,
                                  indicator_region_rect.x0 + (indicator_region_rect.x1 - indicator_region_rect.x0)*pct_done,
                                  indicator_region_rect.y1);
        UI_Rect(pct_rect)
          ui_build_box_from_key(UI_BoxFlag_DrawBackground|UI_BoxFlag_Floating, ui_key_zero());
      }

      // rad: fill
      UI_TagF("pop") UI_Rect(indicator_rect)
        ui_build_box_from_key(UI_BoxFlag_DrawBackground|UI_BoxFlag_Floating, ui_key_zero());

      // rad: animated bar
      UI_Rect(indicator_region_rect)
      {
        UI_Box *box = ui_build_box_from_stringf(UI_BoxFlag_DrawBackground|UI_BoxFlag_DrawBorder|UI_BoxFlag_Floating|UI_BoxFlag_Clickable, "bg_system_status");
        ui_signal_from_box(box);
      }
    }

    // rad: build background
    UI_WidthFill UI_HeightFill UI_Transparency(1-loading_t) UI_BlurSize(10.f*loading_t)
    {
      ui_set_next_blur_size(10.f*loading_t);
      ui_build_box_from_key(UI_BoxFlag_DrawBackground|UI_BoxFlag_DrawBackgroundBlur|UI_BoxFlag_Floating, ui_key_zero());
    }
  }
}

internal UI_Signal
rad_cell(RAD_CellParams *params, String8 string)
{
  // NOTE(rad): port of raddbg's rd_cell (raddbg/raddbg_widgets.c): same
  // boxes, flags, sizes and tags. Only cells given an `edit_buffer` edit
  // (raddbg's cells always can); everything edit-related is gated on that.
  ProfBeginFunction();
  Temp scratch = scratch_begin(0, 0);
  B32 do_cursor_trail = rad_state->settings.animations && rad_state->settings.cursor_trail;

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
  B32 is_editable     = (params->edit_buffer != 0);
  B32 is_focus_hot    = ui_is_focus_hot();
  B32 is_focus_active = is_editable && ui_is_focus_active();
  B32 is_focus_active_disabled = is_editable && (!ui_is_focus_active() && ui_top_focus_active() == UI_FocusKind_On);
  B32 is_editing      = (is_focus_active || is_focus_active_disabled);

  //////////////////////////////
  //- rad: determine which sub-cell components we'll build
  //
  B32 build_toggle_switch = !!(params->flags & RAD_CellFlag_ToggleSwitch) && !is_focus_active;
  B32 build_slider        = !!(params->flags & RAD_CellFlag_Slider) && !is_focus_active;
  B32 build_lhs_name_desc = (params->meta_fstrs.node_count != 0 || params->description.size != 0);
  B32 build_line_edit     = (params->pre_edit_value.size != 0 || params->value_fstrs.node_count != 0);
  DR_FStrList lhs_name_fstrs = params->meta_fstrs;

  //////////////////////////////
  //- rad: build top-level box
  //
  if(is_editing)
  {
    ui_set_next_hover_cursor(WM_Cursor_IBar);
  }
  UI_Box *box = ui_build_box_from_key(UI_BoxFlag_MouseClickable|
                                      (!!build_lhs_name_desc*UI_BoxFlag_DisableFocusBorder)|
                                      (!!(params->flags & RAD_CellFlag_KeyboardClickable)*UI_BoxFlag_KeyboardClickable)|
                                      UI_BoxFlag_ClickToFocus|
                                      (!!(params->flags & RAD_CellFlag_Button)*UI_BoxFlag_DrawHotEffects)|
                                      (!!(params->flags & RAD_CellFlag_SingleClickActivate)*UI_BoxFlag_DrawActiveEffects)|
                                      (!(params->flags & RAD_CellFlag_NoBackground)*UI_BoxFlag_DrawBackground)|
                                      (!!(params->flags & RAD_CellFlag_Border)*UI_BoxFlag_DrawBorder)|
                                      ((is_auto_focus_hot || is_auto_focus_active)*UI_BoxFlag_KeyboardClickable)|
                                      is_editing*(UI_BoxFlag_Clip),
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
  //- rad: build left-hand-side container box
  //
  UI_Box *lhs_box = &ui_nil_box;
  if(build_lhs_name_desc)
  {
    UI_Parent(box) UI_WidthFill UI_ChildLayoutAxis(Axis2_Y)
    {
      if(ui_top_text_alignment() == UI_TextAlign_Left && (params->flags & (RAD_CellFlag_Expander|RAD_CellFlag_ExpanderSpace|RAD_CellFlag_ExpanderPlaceholder)) == 0)
      {
        ui_spacer(ui_em(1.f, 1.f));
      }
      lhs_box = ui_build_box_from_stringf(0, "lhs_box");
    }
  }

  //////////////////////////////
  //- rad: build left-hand-side name/desc box
  //
  if(build_lhs_name_desc) UI_Parent(lhs_box) UI_Padding(ui_em(3.f, 0.f)) UI_WidthFill UI_HeightFill
  {
    FuzzyMatchRangeList fuzzy_matches = {0};
    if(params->search_needle.size != 0)
    {
      fuzzy_matches = dr_fuzzy_match_find_from_fstrs(scratch.arena, &lhs_name_fstrs, params->search_needle);
    }
    UI_Row
    {
      UI_Box *name_box = ui_build_box_from_key(UI_BoxFlag_DrawText, ui_key_zero());
      ui_box_equip_display_fstrs(name_box, &lhs_name_fstrs);
      ui_box_equip_fuzzy_match_ranges(name_box, &fuzzy_matches);
    }
    if(params->description.size != 0) RAD_Font(RAD_FontSlot_Main) UI_FontSize(ui_top_font_size()*0.85f)
    {
      UI_Row
      {
        UI_Box *desc_box = ui_label(params->description).box;
        FuzzyMatchRangeList desc_fuzzy_matches = fuzzy_match_find(scratch.arena, params->search_needle, params->description);
        ui_box_equip_fuzzy_match_ranges(desc_box, &desc_fuzzy_matches);
      }
    }
  }

  //////////////////////////////
  //- rad: build line edit container box
  //
  // NOTE(rad): raddbg builds it only with a value (or while editing); rad's
  // cells without a name/description column always build it (as before)
  //
  UI_Box *edit_box = &ui_nil_box;
  F32 editable_edit_box_dim = dim_2f32(box->rect).x;
  UI_Size edit_box_size = ui_pct(1, 0);
  if(build_lhs_name_desc)
  {
    if(is_editing)
    {
      editable_edit_box_dim = floor_f32(dim_2f32(box->rect).x*0.5f);
      edit_box_size = ui_px(editable_edit_box_dim, 1.f);
    }
    else
    {
      edit_box_size = ui_children_sum(1);
    }
  }
  if(!build_lhs_name_desc || is_editing || build_line_edit)
    UI_Parent(box) UI_PrefWidth(edit_box_size)
  {
    if(ui_top_px_height() > ui_top_font_size()*3.f)
    {
      ui_set_next_pref_width(ui_children_sum(1));
      UI_Column UI_Padding(ui_em(1, 0)) UI_Focus(UI_FocusKind_On)
      {
        UI_PrefHeight(ui_em(3.f, 1.f)) UI_CornerRadius(ui_top_font_size()*0.5f)
          edit_box = ui_build_box_from_stringf((!!is_editing*UI_BoxFlag_DrawBorder)|
                                               UI_BoxFlag_Clickable|
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
    if(params->line_edit_key_out)
    {
      params->line_edit_key_out[0] = edit_box->key;
    }
  }

  //////////////////////////////
  //- rad: build scrollable container box
  //
  UI_Box *scrollable_box = &ui_nil_box;
  if(edit_box != &ui_nil_box)
  {
    UI_Parent(edit_box) UI_PrefWidth(ui_children_sum(0))
    {
      scrollable_box = ui_build_box_from_stringf(is_focus_active*(UI_BoxFlag_AllowOverflowX|UI_BoxFlag_Clip), "scroll_box_%p", params->edit_buffer);
    }
  }

  //////////////////////////////
  //- rad: build revert-button
  //
  if(params->flags & RAD_CellFlag_RevertButton && !is_editing)
  {
    // NOTE(rad): raddbg parents it to the line edit box; a cell with no
    // value (a toggle switch) has none in rad -> the cell box
    UI_Parent(edit_box != &ui_nil_box ? edit_box : box)
      UI_PrefWidth(ui_em(2.f, 1.f))
    {
      UI_TagF(".")
        UI_TagF("weak")
        UI_TagF("implicit")
        UI_Column
        UI_Padding(ui_pct(1, 0))
        UI_PrefHeight(ui_em(2.f, 1.f))
        UI_CornerRadius(ui_top_font_size()*0.5f)
        RAD_Font(RAD_FontSlot_Icons)
        UI_TextAlignment(UI_TextAlign_Center)
      {
        UI_Box *revert_box = ui_build_box_from_stringf(UI_BoxFlag_DrawText|
                                                       UI_BoxFlag_DrawHotEffects|
                                                       UI_BoxFlag_DrawBorder|
                                                       UI_BoxFlag_DrawBackground|
                                                       UI_BoxFlag_DisableFocusOverlay|
                                                       UI_BoxFlag_DisableFocusBorder|
                                                       UI_BoxFlag_Clickable,
                                                       "%S##revert", rad_icon_kind_text_table[RAD_IconKind_Undo]);
        UI_Signal sig = ui_signal_from_box(revert_box);
        if(params->revert_rect_out) { params->revert_rect_out[0] = revert_box->rect; }
        if(ui_hovering(sig)) UI_Tooltip RAD_Font(RAD_FontSlot_Main)
        {
          ui_state->tooltip_anchor_key = revert_box->key;
          ui_label(str8_lit("Revert To Default"));
        }
        if(ui_pressed(sig) && params->revert_out)
        {
          params->revert_out[0] = 1;
        }
      }
      // TODO(rjf): @hack
      if(build_toggle_switch || build_slider)
      {
        ui_spacer(ui_em(1.f, 1.f));
      }
    }
  }

  //////////////////////////////
  //- rad: build toggle-switch
  //
  if(build_toggle_switch) UI_Parent(box)
  {
    B32 is_toggled = !!params->toggled_out[0];
    F32 toggle_t = ui_anim(ui_key_from_stringf(key, "toggled"), (F32)is_toggled, .initial = (F32)is_toggled, .rate = rad_state->menu_animation_rate);
    F32 height_px = ceil_f32(ui_top_font_size() * 1.75f);
    F32 padding_px = ceil_f32((ui_top_px_height() - height_px) / 2.f);
    UI_PrefWidth(ui_children_sum(1.f))
      UI_HeightFill
      UI_Column UI_Padding(ui_px(padding_px, 1.f))
      UI_Row
    {
      if(ui_top_text_alignment() == UI_TextAlign_Center)
      {
        ui_spacer(ui_em(1.f, 0.f));
      }
      UI_PrefWidth(ui_em(3.5f, 1.f))
        UI_PrefHeight(ui_px(height_px, 1.f))
        UI_CornerRadius(floor_f32(height_px/2.f - 1.f))
        UI_TagF(is_toggled ? "good_pop" : "")
        UI_GroupKey(ui_key_from_stringf(ui_key_zero(), "toggle_switch_group_key"))
      {
        UI_Box *switch_box = ui_build_box_from_stringf(UI_BoxFlag_DrawHotEffects|UI_BoxFlag_DrawBorder|UI_BoxFlag_DrawBackground|UI_BoxFlag_Clickable, "toggle_switch");
        UI_Parent(switch_box)
        {
          RAD_Font(RAD_FontSlot_Icons) UI_PrefWidth(ui_pct(toggle_t, 0)) UI_Transparency(1.f - toggle_t)
          {
            ui_build_box_from_stringf(UI_BoxFlag_DisableTextTrunc | (toggle_t > 0.001f ? UI_BoxFlag_DrawText : 0),
                                      "%S", rad_icon_kind_text_table[RAD_IconKind_Check]);
          }
          UI_BackgroundColor(ui_color_from_name(str8_lit("text")))
            UI_PrefWidth(ui_px(height_px, 1.f))
          {
            F32 extratoggler_padding_px = floor_f32(ui_top_font_size()*0.35f);
            F32 toggler_size_px = ceil_f32(height_px - extratoggler_padding_px*2.f) - 1.f;
            UI_Column UI_Padding(ui_px(extratoggler_padding_px, 1.f))
              UI_Row UI_Padding(ui_px(extratoggler_padding_px, 1.f))
              UI_PrefWidth(ui_px(toggler_size_px, 1.f))
              UI_PrefHeight(ui_px(toggler_size_px, 1.f))
              UI_CornerRadius(floor_f32(toggler_size_px/2.f - 1.f))
            {
              ui_build_box_from_key(UI_BoxFlag_DrawBackground|UI_BoxFlag_DrawDropShadow, ui_key_zero());
            }
          }
          ui_spacer(ui_pct(1.f-toggle_t, 0));
        }
        UI_Signal switch_sig = ui_signal_from_box(switch_box);
        if(params->control_rect_out) { params->control_rect_out[0] = switch_box->rect; }

        // rjf: press -> toggle, & gather this key
        if(ui_pressed(switch_sig))
        {
          if(ui_dragging(switch_sig))
          {
            ui_store_drag_struct(&switch_box->key);
          }
          params->toggled_out[0] ^= 1;
        }

        // rjf: dragging -> check if key is in batch of touched keys. if so, do nothing, otherwise, toggle.
        // always store this new key if not in batch
        if(ui_dragging(switch_sig))
        {
          String8 all_keys_data = ui_get_drag_data(sizeof(UI_Key));
          UI_Key *keys = (UI_Key *)all_keys_data.str;
          U64 keys_count = all_keys_data.size / sizeof(UI_Key);
          B32 key_is_touched = 0;
          for EachIndex(idx, keys_count)
          {
            if(ui_key_match(keys[idx], switch_box->key))
            {
              key_is_touched = 1;
              break;
            }
          }
          if(!key_is_touched)
          {
            params->toggled_out[0] ^= 1;
            UI_Key *new_keys = push_array(scratch.arena, UI_Key, keys_count+1);
            MemoryCopy(new_keys, keys, sizeof(UI_Key)*keys_count);
            new_keys[keys_count] = switch_box->key;
            ui_store_drag_data(str8((U8 *)new_keys, sizeof(UI_Key) * (keys_count+1)));
          }
        }
      }
      if(ui_top_text_alignment() == UI_TextAlign_Center)
      {
        ui_spacer(ui_em(1.f, 0.f));
      }
    }
    if(ui_top_text_alignment() == UI_TextAlign_Left)
    {
      ui_spacer(ui_em(1.f, 1.f));
    }
  }

  //////////////////////////////
  //- rad: build slider
  //
  if(build_slider) UI_Parent(box)
  {
    F32 height_px = ceil_f32(ui_top_font_size() * 1.75f);
    F32 padding_px = ceil_f32((ui_top_px_height() - height_px) / 2.f);
    UI_PrefWidth(ui_children_sum(1.f))
      UI_HeightFill
      UI_Column UI_Padding(ui_px(padding_px, 1.f))
      UI_Row
      UI_PrefWidth(ui_pct(0.5f - 0.2f*(!!build_lhs_name_desc), 0.f))
      UI_PrefHeight(ui_px(height_px, 1.f))
      UI_CornerRadius(floor_f32(height_px/2.f - 1.f))
    {
      F32 extratoggler_padding_px = floor_f32(ui_top_font_size()*0.35f);
      F32 toggler_size_px = ceil_f32(height_px - extratoggler_padding_px*2.f) - 1.f;
      ui_set_next_hover_cursor(WM_Cursor_LeftRight);
      UI_Box *slider_box = ui_build_box_from_stringf(UI_BoxFlag_DrawHotEffects|UI_BoxFlag_DrawBorder|UI_BoxFlag_DrawBackground|UI_BoxFlag_Clickable, "slider");
      if(params->control_rect_out) { params->control_rect_out[0] = slider_box->rect; }
      UI_Parent(slider_box) UI_TagF("pop")
      {
        UI_Signal sig = ui_signal_from_box(slider_box);
        if(ui_dragging(sig))
        {
          if(ui_pressed(sig))
          {
            ui_store_drag_struct(params->slider_value_out);
          }
          F32 draggable_region_size_px = dim_2f32(slider_box->rect).x - (extratoggler_padding_px*2 + toggler_size_px);
          F32 initial_pct = *ui_get_drag_struct(F32);
          F32 current_pct = initial_pct + (ui_drag_delta().x / draggable_region_size_px);
          params->slider_value_out[0] = current_pct;
        }

        UI_Box *fill_box = &ui_nil_box;
        UI_PrefWidth(ui_children_sum(0))
          UI_MinWidth(toggler_size_px + extratoggler_padding_px*2)
          fill_box = ui_build_box_from_key(UI_BoxFlag_DrawBackground|UI_BoxFlag_DrawBorder, ui_key_zero());
        UI_Parent(fill_box)
        {
          ui_spacer(ui_pct(Clamp(0, params->slider_value_out[0], 1), 0.f));
          UI_BackgroundColor(ui_color_from_name(str8_lit("text")))
            UI_PrefWidth(ui_px(height_px, 1.f))
          {
            UI_Column UI_Padding(ui_px(extratoggler_padding_px, 1.f))
              UI_Row UI_Padding(ui_px(extratoggler_padding_px, 1.f))
              UI_PrefWidth(ui_px(toggler_size_px, 1.f))
              UI_PrefHeight(ui_px(toggler_size_px, 1.f))
              UI_CornerRadius(floor_f32(toggler_size_px/2.f - 1.f))
            {
              ui_build_box_from_key(UI_BoxFlag_DrawBackground|UI_BoxFlag_DrawDropShadow, ui_key_zero());
            }
          }
        }
        ui_spacer(ui_pct(1-Clamp(0, params->slider_value_out[0], 1), 0.f));
      }
    }
    ui_spacer(ui_em(1.f, 1.f));
  }

  //////////////////////////////
  //- rad: do non-textual edits (delete, copy)
  //
  B32 commit = 0;
  if(is_editable && !is_focus_active && is_focus_hot)
  {
    for(UI_Event *evt = 0; ui_next_event(&evt);)
    {
      if(evt->flags & UI_EventFlag_Copy)
      {
        wm_set_clipboard_text(params->pre_edit_value);
      }
      if(evt->flags & UI_EventFlag_Delete)
      {
        commit = 1;
        params->edit_string_size_out[0] = 0;
      }
    }
  }

  //////////////////////////////
  //- rad: get signal
  //
  UI_Signal sig = ui_signal_from_box(box);
  if(commit)
  {
    sig.f |= UI_SignalFlag_Commit;
  }

  //////////////////////////////
  //- rad: do start/end editing interaction
  //
  B32 focus_started = 0;
  if(is_editable && !is_focus_active)
  {
    B32 start_editing_via_sig = (ui_double_clicked(sig) || sig.f&UI_SignalFlag_KeyboardPressed);
    B32 start_editing_via_typing = 0;
    if(is_focus_hot)
    {
      for(UI_Event *evt = 0; ui_next_event(&evt);)
      {
        if(evt->string.size != 0 || evt->flags & UI_EventFlag_Paste)
        {
          start_editing_via_typing = 1;
          break;
        }
      }
    }
    if(is_focus_hot && ui_slot_press(UI_EventActionSlot_Edit))
    {
      start_editing_via_typing = 1;
    }
    if(start_editing_via_sig || start_editing_via_typing)
    {
      String8 edit_string = params->pre_edit_value;
      edit_string.size = Min(params->edit_buffer_size, params->pre_edit_value.size);
      MemoryCopy(params->edit_buffer, edit_string.str, edit_string.size);
      params->edit_string_size_out[0] = edit_string.size;
      ui_set_auto_focus_active_key(key);
      if(!(params->flags & RAD_CellFlag_Button))
      {
        ui_kill_action();
      }
      params->cursor[0] = edit_string.size;
      params->mark[0] = 0;
      focus_started = 1;
    }
  }
  else if(is_focus_active && sig.f&UI_SignalFlag_KeyboardPressed)
  {
    ui_set_auto_focus_active_key(ui_key_zero());
    sig.f |= UI_SignalFlag_Commit;
  }

  //////////////////////////////
  //- rad: take navigation actions for editing
  //
  if(is_editable && !(params->flags & RAD_CellFlag_DisableEdit) && (is_focus_active || focus_started))
  {
    Temp scratch = scratch_begin(0, 0);
    for(UI_Event *evt = 0; ui_next_event(&evt);)
    {
      String8 edit_string = str8(params->edit_buffer, params->edit_string_size_out[0]);

      // rad: do not consume anything that doesn't fit a single-line's
      // operations (rad: incl. the "\n" text that Enter also produces)
      if((evt->kind != UI_EventKind_Edit &&
          evt->kind != UI_EventKind_Navigate &&
          evt->kind != UI_EventKind_Text) ||
         evt->delta_2s32.y != 0 ||
         (evt->kind == UI_EventKind_Text && !(evt->flags & UI_EventFlag_Paste) &&
          str8_find_needle(evt->string, 0, str8_lit("\n"), 0) < evt->string.size))
      {
        continue;
      }

      // rad: map this action to an op
      UI_TxtOp op = ui_single_line_txt_op_from_event(scratch.arena, evt, edit_string, r1u64(0, edit_string.size), params->cursor[0], params->mark[0]);

      // rad: perform replace range
      if(op.range.min != op.range.max || op.replace.size != 0)
      {
        String8 new_string = ui_push_string_replace_range(scratch.arena, edit_string, op.range, op.replace);
        new_string.size = Min(params->edit_buffer_size, new_string.size);
        MemoryCopy(params->edit_buffer, new_string.str, new_string.size);
        params->edit_string_size_out[0] = new_string.size;
      }

      // rad: perform copy
      if(evt->flags & UI_EventFlag_Copy)
      {
        wm_set_clipboard_text(op.copy);
      }

      // rad: commit op's changed cursor & mark to caller-provided state
      params->cursor[0] = op.cursor;
      params->mark[0] = op.mark;

      // rad: consume event
      ui_eat_event(evt);
    }
    scratch_end(scratch);
  }

  //////////////////////////////
  //- rad: compute fancy strings
  //
  DR_FStrList fstrs = {0};
  {
    //- rad: (not editing)
    if(!is_editing && params->value_fstrs.total_size != 0)
    {
      fstrs = params->value_fstrs;
    }
    else if(!is_editing && params->flags & RAD_CellFlag_CodeContents && params->pre_edit_value.size != 0)
    {
      fstrs = rad_fstrs_from_code_string(scratch.arena, 1, ui_color_from_name(str8_lit("text")), params->pre_edit_value);
    }
    else if(!is_editing)
    {
      String8 display_string = params->pre_edit_value;
      if(params->pre_edit_value.size == 0)
      {
        display_string = ui_display_part_from_key_string(string);
      }
      UI_TagF("weak")
      {
        DR_FStrParams fstr_params = {ui_top_font(), ui_top_text_raster_flags(), ui_color_from_name(str8_lit("text")), ui_top_font_size()};
        dr_fstrs_push_new(scratch.arena, &fstrs, &fstr_params, display_string);
      }
    }

    //- rad: (editing)
    else
    {
      String8 edit_string = str8(params->edit_buffer, params->edit_string_size_out[0]);
      if(params->flags & RAD_CellFlag_CodeContents)
      {
        fstrs = rad_fstrs_from_code_string(scratch.arena, 1.f, ui_color_from_name(str8_lit("text")), edit_string);
      }
      else
      {
        DR_FStrParams fstr_params = {ui_top_font(), ui_top_text_raster_flags(), ui_color_from_name(str8_lit("text")), ui_top_font_size()};
        dr_fstrs_push_new(scratch.arena, &fstrs, &fstr_params, edit_string);
      }
    }
  }

  //////////////////////////////
  //- rad: build scrolled contents
  //
  U64 mouse_off = 0;
  F32 cursor_off = 0;
  if(scrollable_box != &ui_nil_box) UI_Parent(scrollable_box)
  {
    FuzzyMatchRangeList fuzzy_matches = {0};
    if(params->search_needle.size != 0)
    {
      fuzzy_matches = dr_fuzzy_match_find_from_fstrs(scratch.arena, &fstrs, params->search_needle);
    }
    if(ui_top_text_alignment() == UI_TextAlign_Left && (params->flags & (RAD_CellFlag_Expander|RAD_CellFlag_ExpanderSpace|RAD_CellFlag_ExpanderPlaceholder)) == 0)
    {
      ui_spacer(ui_em(0.5f, 1.f));
    }
    if(is_focus_active)
    {
      ui_set_next_flags(UI_BoxFlag_DisableTextTrunc);
    }
    ui_set_next_pref_width(ui_text_dim(ui_top_font_size()*0.5f, 0));
    UI_Box *text_box = ui_build_box_from_stringf(UI_BoxFlag_DrawText, "###text_box");
    ui_box_equip_display_fstrs(text_box, &fstrs);
    ui_box_equip_fuzzy_match_ranges(text_box, &fuzzy_matches);
    if(is_editing)
    {
      String8 edit_string = str8(params->edit_buffer, params->edit_string_size_out[0]);
      UI_LineEditDrawData *draw_data = push_array(ui_build_arena(), UI_LineEditDrawData, 1);
      draw_data->edited_string = push_str8_copy(ui_build_arena(), edit_string);
      draw_data->cursor = params->cursor[0];
      draw_data->mark = params->mark[0];
      draw_data->trail = do_cursor_trail && !ui_dragging(sig);
      ui_box_equip_custom_draw(text_box, ui_line_edit_draw, draw_data);
      Vec2F32 text2mouse = sub_2f32(ui_mouse(), ui_box_text_position(text_box));
      FNT_Tag font = ui_top_font();
      F32 font_size = ui_top_font_size();
      if(params->flags & RAD_CellFlag_CodeContents)
      {
        font = rad_font_from_slot(RAD_FontSlot_Code);
      }
      mouse_off = fnt_char_pos_from_tag_size_string_p(font, font_size, 0, ui_top_tab_size(), edit_string, text2mouse.x);
      cursor_off = fnt_dim_from_tag_size_string(ui_top_font(), ui_top_font_size(), 0, ui_top_tab_size(), str8_prefix(edit_string, params->cursor[0])).x;
    }
  }

  //////////////////////////////
  //- rad: click+drag
  //
  if(is_focus_active && ui_dragging(sig))
  {
    if(ui_pressed(sig))
    {
      params->mark[0] = mouse_off;
    }
    params->cursor[0] = mouse_off;
  }
  if(!is_focus_active && is_focus_active_disabled && ui_pressed(sig))
  {
    params->cursor[0] = params->mark[0] = mouse_off;
  }

  //////////////////////////////
  //- rad: focus cursor
  //
  if(scrollable_box != &ui_nil_box)
  {
    F32 visible_dim_px = editable_edit_box_dim - expander_size_px - ui_top_font_size()*params->depth;
    if(visible_dim_px > 0)
    {
      Rng1F32 cursor_range_px  = r1f32(cursor_off-ui_top_font_size()*2.f, cursor_off+ui_top_font_size()*1.f);
      Rng1F32 visible_range_px = r1f32(scrollable_box->view_off_target.x, scrollable_box->view_off_target.x + visible_dim_px);
      cursor_range_px.min = ClampBot(0, cursor_range_px.min);
      cursor_range_px.max = ClampBot(0, cursor_range_px.max);
      F32 min_delta = cursor_range_px.min-visible_range_px.min;
      F32 max_delta = cursor_range_px.max-visible_range_px.max;
      min_delta = Min(min_delta, 0);
      max_delta = Max(max_delta, 0);
      scrollable_box->view_off_target.x += min_delta;
      scrollable_box->view_off_target.x += max_delta;
    }
    if(!is_editing)
    {
      scrollable_box->view_off_target.x = scrollable_box->view_off.x = 0;
    }
  }

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

internal DR_FStrList
rad_fstrs_from_code_string(Arena *arena, F32 alpha, Vec4F32 base_color, String8 string)
{
  return rad_fstrs_from_code_string_lang(arena, alpha, base_color, TXT_LangKind_C, string);
}

internal DR_FStrList
rad_fstrs_from_code_string_lang(Arena *arena, F32 alpha, Vec4F32 base_color, TXT_LangKind lang, String8 string)
{
  // NOTE(rad): port of raddbg's rd_fstrs_from_code_string, minus the
  // debug-info identifier lookup (identifiers keep their token color, as in
  // raddbg with no debug info loaded) and the indirection size change (off
  // in raddbg too); the language is a parameter (raddbg: always C)
  ProfBeginFunction();
  Temp scratch = scratch_begin(&arena, 1);
  DR_FStrList fstrs = {0};
  // NOTE(rad): Rust: the editor's lexer (txt_token_array_from_string__rust
  // is compiled out), from the top-level state
  TXT_TokenArray tokens = (lang == TXT_LangKind_Rust ? txt_token_array_from_data__rust(scratch.arena, (TXT_TokenPt){0}, string, 0) :
                           txt_token_array_from_string__c_cpp(scratch.arena, 0, string));
  TXT_Token *tokens_opl = tokens.v+tokens.count;
  for(TXT_Token *token = tokens.v; token < tokens_opl; token += 1)
  {
    RAD_CodeColorSlot token_color_slot = rad_code_color_slot_from_txt_token_kind(token->kind);
    Vec4F32 token_color_rgba = rad_rgba_from_code_color_slot(token_color_slot);
    String8 token_string = str8_substr(string, token->range);
    switch(token->kind)
    {
      default:
      {
        token_color_rgba.w *= alpha;
        DR_FStr fstr = {token_string, {ui_top_font(), ui_top_text_raster_flags(), token_color_rgba, ui_top_font_size()}};
        dr_fstrs_push(arena, &fstrs, &fstr);
      }break;
      case TXT_TokenKind_Numeric:
      {
        token_color_rgba.w *= alpha;
        Vec4F32 token_color_rgba_alt = rad_rgba_from_code_color_slot(RAD_CodeColorSlot_CodeNumericAltDigitGroup);
        token_color_rgba_alt.w *= alpha;
        F32 font_size = ui_top_font_size();

        // rad: unpack string
        U32 base = 10;
        U64 prefix_skip = 0;
        U64 digit_group_size = 3;
        if(str8_match(str8_prefix(token_string, 2), str8_lit("0x"), StringMatchFlag_CaseInsensitive))
        {
          base = 16;
          prefix_skip = 2;
          digit_group_size = 4;
        }
        else if(str8_match(str8_prefix(token_string, 2), str8_lit("0b"), StringMatchFlag_CaseInsensitive))
        {
          base = 2;
          prefix_skip = 2;
          digit_group_size = 8;
        }
        else if(str8_match(str8_prefix(token_string, 2), str8_lit("0o"), StringMatchFlag_CaseInsensitive))
        {
          base = 8;
          prefix_skip = 2;
          digit_group_size = 2;
        }

        // rad: grab string parts
        U64 dot_pos = str8_find_needle(token_string, 0, str8_lit("."), 0);
        String8 prefix = str8_prefix(token_string, prefix_skip);
        String8 whole = str8_substr(token_string, r1u64(prefix_skip, dot_pos));
        String8 decimal = str8_skip(token_string, dot_pos);

        // rad: determine # of digits
        U64 num_digits = 0;
        for(U64 idx = 0; idx < whole.size; idx += 1)
        {
          num_digits += char_is_digit(whole.str[idx], base);
        }

        // rad: push prefix
        {
          DR_FStr fstr = {prefix, {ui_top_font(), ui_top_text_raster_flags(), token_color_rgba, font_size}};
          dr_fstrs_push(arena, &fstrs, &fstr);
        }

        // rad: push digit groups
        {
          B32 odd = 0;
          U64 start_idx = 0;
          U64 num_digits_passed = digit_group_size - num_digits%digit_group_size;
          for(U64 idx = 0; idx <= whole.size; idx += 1)
          {
            U8 byte = idx < whole.size ? whole.str[idx] : 0;
            if(num_digits_passed >= digit_group_size || idx == whole.size)
            {
              num_digits_passed = 0;
              if(start_idx < idx)
              {
                DR_FStr fstr = {str8_substr(whole, r1u64(start_idx, idx)), {ui_top_font(), ui_top_text_raster_flags(), odd ? token_color_rgba_alt : token_color_rgba, font_size}};
                dr_fstrs_push(arena, &fstrs, &fstr);
                start_idx = idx;
                odd ^= 1;
              }
            }
            if(char_is_digit(byte, base))
            {
              num_digits_passed += 1;
            }
          }
        }

        // rad: push decimal
        {
          DR_FStr fstr = {decimal, {ui_top_font(), ui_top_text_raster_flags(), token_color_rgba, font_size}};
          dr_fstrs_push(arena, &fstrs, &fstr);
        }
      }break;
    }
  }
  scratch_end(scratch);
  ProfEnd();
  return fstrs;
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

  //- rad: not in raddbg: the slice can be taller than its lines (the view
  // fills its panel; a click below the last line goes to that line)
  F32 slice_height_px = Max(params->line_height_px*(dim_1s64(params->line_num_range)+1), params->min_height_px);

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
    ui_set_next_pref_height(ui_px(slice_height_px, 1.f));
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
      ui_set_next_pref_height(ui_px(slice_height_px, 1.f));
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
    ui_set_next_pref_height(ui_px(slice_height_px, 1.f));
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
    // NOTE(rad): line_num_range is inclusive (dim+1 lines). raddbg clamps to
    // dim-1 here, so the mouse never reached the last visible line: with the
    // whole file on screen its last line could not be clicked or dragged from.
    mouse_y_line_idx_signed = Clamp(0, mouse_y_line_idx_signed, dim_1s64(params->line_num_range));
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
    result.text_rect = text_container_box->rect;
    result.text_x_off = params->line_num_width_px + line_num_padding_px;
  }

  //////////////////////////////
  //- rad: mouse point -> mouse token range, mouse line range
  //
  Rng1U64 mouse_token_rng = r1u64(mouse_off, mouse_off);
  Rng1U64 mouse_line_rng = r1u64(mouse_off, mouse_off);
  if(mouse_y_line_idx <= (U64)dim_1s64(params->line_num_range))
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
  //- rad: not in raddbg: the character under the mouse (not the nearest
  // boundary) and its token, if not whitespace (language server hover)
  //
  TXT_TokenKind mouse_token_kind = TXT_TokenKind_Null;
  {
    Vec2F32 mouse = ui_mouse();
    S64 line_count = dim_1s64(params->line_num_range)+1;
    S64 raw_line_idx = (S64)floor_f32((mouse.y - text_container_box->rect.y0) / params->line_height_px);
    if(contains_2f32(text_container_box->rect, mouse) && 0 <= raw_line_idx && raw_line_idx < line_count)
    {
      String8 line_string = params->line_text[raw_line_idx];
      F32 x = mouse.x - text_container_box->rect.x0 - params->line_num_width_px - line_num_padding_px;
      U64 boundary = fnt_char_pos_from_tag_size_string_p(params->font, params->font_size, 0, params->tab_size, line_string, x);
      F32 boundary_x = fnt_dim_from_tag_size_string(params->font, params->font_size, 0, params->tab_size, str8_prefix(line_string, boundary)).x;
      U64 char_idx = ((boundary_x > x && boundary > 0) ? boundary-1 : boundary);
      if(x >= 0 && char_idx < line_string.size)
      {
        Rng1U64 line_range = params->line_ranges[raw_line_idx];
        U64 char_off = line_range.min + char_idx;
        TXT_TokenArray *line_tokens = &params->line_tokens[raw_line_idx];
        for EachIndex(token_idx, line_tokens->count)
        {
          TXT_Token *token = &line_tokens->v[token_idx];
          if(token->range.min <= char_off && char_off < token->range.max && token->kind != TXT_TokenKind_Whitespace)
          {
            result.mouse_over_text = 1;
            result.mouse_char_off = char_off;
            result.mouse_token_range = intersect_1u64(token->range, line_range);
            mouse_token_kind = token->kind;
            break;
          }
        }
      }
    }
  }

  //////////////////////////////
  //- rad: interact with text box
  //
  B32 search_query_invalidated = 0;
  UI_Signal text_container_sig = ui_signal_from_box(text_container_box);

  //- rad: not in raddbg: Ctrl held over an identifier or keyword -> a link
  // to its definition (Visual Studio's Ctrl+click): highlighted, hand
  // cursor; a press there does not move the cursor or start a selection
  B32 link_drag = ui_key_match(rad_state->code_slice_link_press_key, text_container_box->key);
  B32 link_hovered = ((params->flags & RAD_CodeSliceFlag_CtrlLinks) && result.mouse_over_text &&
                      (mouse_token_kind == TXT_TokenKind_Identifier || mouse_token_kind == TXT_TokenKind_Keyword) &&
                      (text_container_sig.event_flags & (WM_Modifier_Ctrl|WM_Modifier_Shift|WM_Modifier_Alt)) == WM_Modifier_Ctrl &&
                      ui_hovering(text_container_sig) && (!ui_dragging(text_container_sig) || ui_pressed(text_container_sig) || link_drag));
  if(ui_pressed(text_container_sig))
  {
    link_drag = link_hovered;
    rad_state->code_slice_link_press_key = (link_hovered ? text_container_box->key : ui_key_zero());
    if(link_hovered)
    {
      result.link_pressed = 1;
      result.link_off = result.mouse_char_off;
    }
  }
  if(link_drag && !ui_dragging(text_container_sig))
  {
    rad_state->code_slice_link_press_key = ui_key_zero();
  }
  if(link_hovered)
  {
    text_container_box->hover_cursor = WM_Cursor_HandPoint;
    result.link_hovered = 1;
    result.link_range = result.mouse_token_range;
  }
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
    if(ui_dragging(text_container_sig) && !link_drag)
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
  //- rad: determine starting offset for each line, at which we can begin
  // placing extra info to the right (same as raddbg, minus its margins)
  //
  F32 *line_extras_off = push_array(scratch.arena, F32, dim_1s64(params->line_num_range)+1);
  if(params->line_annotations != 0)
  {
    U64 line_idx = 0;
    for(S64 line_num = params->line_num_range.min;
        line_num <= params->line_num_range.max;
        line_num += 1, line_idx += 1)
    {
      // NOTE(rad): raddbg's offset also counts its priority/catchall margins,
      // which rad does not have; the line's text padding takes their place
      F32 line_text_dim = fnt_dim_from_tag_size_string(params->font, params->font_size, 0, params->tab_size, params->line_text[line_idx]).x + params->line_num_width_px + line_num_padding_px;
      line_extras_off[line_idx] = Max(line_text_dim, params->font_size*30);
    }
  }

  //////////////////////////////
  //- rad: build per-line annotations (port of raddbg's per-line extra
  // annotation containers + exception annotations; built before the line
  // text, so they draw over a colored line)
  //
  if(params->line_annotations != 0)
    UI_PrefWidth(ui_children_sum(1)) UI_PrefHeight(ui_px(params->line_height_px, 1.f)) UI_Parent(text_container_box) UI_Focus(UI_FocusKind_Off)
  {
    U64 line_idx = 0;
    for(S64 line_num = params->line_num_range.min;
        line_num < params->line_num_range.max;
        line_num += 1, line_idx += 1)
    {
      if(params->line_annotations[line_idx].node_count == 0)
      {
        continue;
      }
      ui_set_next_fixed_x(line_extras_off[line_idx]);
      ui_set_next_fixed_y(line_idx*params->line_height_px);
      UI_Box *line_extras_box = ui_build_box_from_stringf(0, "###extras_%I64x", line_idx);
      UI_Parent(line_extras_box) UI_PrefWidth(ui_text_dim(10, 1)) UI_TextAlignment(UI_TextAlign_Center) UI_PrefHeight(ui_px(params->line_height_px, 1.f))
      {
        ui_spacer(ui_em(1.5f, 1.f));
        UI_Box *box = ui_build_box_from_stringf(UI_BoxFlag_DrawText, "###annotation");
        ui_box_equip_display_fstrs(box, &params->line_annotations[line_idx]);
      }
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
    ui_set_next_pref_height(ui_px(slice_height_px, 1.f));
    ui_build_box_from_key(0, ui_key_zero());
  }

  //////////////////////////////
  //- rad: build line text
  //
  UI_Parent(text_container_box) ProfScope("build line text") UI_Focus(UI_FocusKind_Off)
  {
    ui_set_next_pref_height(ui_px(slice_height_px, 1.f));
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
        Vec4F32 line_bg_color = (params->line_bg_colors != 0 ? params->line_bg_colors[line_idx] : v4f32(0, 0, 0, 0));
        if(line_bg_color.w != 0)
        {
          ui_set_next_flags(UI_BoxFlag_DrawBackground);
          ui_set_next_background_color(line_bg_color);
        }
        ui_set_next_tab_size(params->tab_size);
        UI_Box *line_box = ui_build_box_from_key(UI_BoxFlag_DisableTextTrunc|UI_BoxFlag_DrawText|UI_BoxFlag_DisableIDString, line_key);
        DR_Bucket *line_bucket = dr_bucket_make();
        dr_push_bucket(line_bucket);
        ui_box_equip_display_fstrs(line_box, &line_fstrs);

        // rad: not in raddbg: the Ctrl+hover link (Go To Definition target)
        if(result.link_hovered && line_range.min <= result.link_range.min && result.link_range.max <= line_range.max)
        {
          F32 x0 = fnt_dim_from_tag_size_string(line_box->font, line_box->font_size, 0, params->tab_size, str8_prefix(line_string, result.link_range.min - line_range.min)).x;
          F32 x1 = fnt_dim_from_tag_size_string(line_box->font, line_box->font_size, 0, params->tab_size, str8_prefix(line_string, result.link_range.max - line_range.min)).x;
          Rng2F32 link_rect =
          {
            line_box->rect.x0+line_num_padding_px+x0-2.f,
            floor_f32(line_box->rect.y0) + 1.f,
            line_box->rect.x0+line_num_padding_px+x1+2.f,
            ceil_f32(line_box->rect.y1) - 1.f,
          };
          dr_rect(link_rect, ui_color_from_name(str8_lit("selection")), params->font_size*0.2f, 0, 1.f);
          dr_rect(link_rect, highlight_color, params->font_size*0.2f, 1.f, 1.f);
        }

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
          Rng1U64 next_line_range = (line_idx+1 <= (U64)dim_1s64(params->line_num_range)) ? params->line_ranges[line_idx+1] : r1u64(0, 0);
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

        // rad: not in raddbg: underlined ranges (diagnostic spans). an empty
        // range, or one that only covers the line end, underlines one column.
        for(RAD_CodeSliceUnderline *u = params->first_underline; u != 0; u = u->next)
        {
          Rng1U64 range_in_line = intersect_1u64(u->range, line_range);
          B32 starts_here = (line_range.min <= u->range.min && u->range.min <= line_range.max);
          B32 overlaps = (range_in_line.min < range_in_line.max);
          if(starts_here || overlaps)
          {
            U64 min_col = (Max(u->range.min, line_range.min) - line_range.min);
            U64 max_col = (Min(u->range.max, line_range.max) - line_range.min);
            F32 x0 = fnt_dim_from_tag_size_string(line_box->font, line_box->font_size, 0, params->tab_size, str8_prefix(line_string, min_col)).x;
            F32 x1 = fnt_dim_from_tag_size_string(line_box->font, line_box->font_size, 0, params->tab_size, str8_prefix(line_string, max_col)).x;
            if(x1 <= x0)
            {
              x1 = x0 + fnt_dim_from_tag_size_string(line_box->font, line_box->font_size, 0, params->tab_size, str8_lit("H")).x;
            }
            F32 thickness = ClampBot(1.f, floor_f32(line_box->font_size/8.f));
            F32 y = floor_f32(center_2f32(line_box->rect).y + line_box->font_size*0.7f);
            Rng2F32 underline_rect = r2f32p(ui_box_text_position(line_box).x + x0, y,
                                            ui_box_text_position(line_box).x + x1, y + thickness);
            Vec4F32 color = u->color;
            if(!is_focused)
            {
              color.w *= 0.75f;
            }
            dr_rect(underline_rect, color, 0, 0, 0.f);
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

////////////////////////////////
//~ rad: Markdown (not in raddbg: language server hover text)

internal String8
rad_markdown_inline_plain(Arena *arena, String8 line)
{
  // NOTE(rad): inline markup dropped: [text](url) and [text] -> text,
  // `code` -> code, **bold** -> bold, \x -> x
  U8 *out = push_array_no_zero(arena, U8, line.size + 1);
  U64 size = 0;
  for(U64 idx = 0; idx < line.size; idx += 1)
  {
    U8 c = line.str[idx];
    U8 next = (idx+1 < line.size ? line.str[idx+1] : 0);
    B32 next_is_punctuation = (0x21 <= next && next <= 0x7e && !char_is_alpha(next) && !char_is_digit(next, 10));
    if(c == '\\' && next_is_punctuation)
    {
      out[size++] = next;
      idx += 1;
    }
    else if(c == '*' && next == '*')
    {
      idx += 1;
    }
    else if(c == ']' && next == '(')
    {
      U64 close = str8_find_needle(line, idx+2, str8_lit(")"), 0);
      idx = (close < line.size ? close : line.size);
    }
    else if(c == '`' || c == '[' || c == ']')
    {
      // rad: dropped
    }
    else
    {
      out[size++] = c;
    }
  }
  out[size] = 0;
  return str8(out, size);
}

internal U64
rad_markdown_wrapped_labels(String8 text, F32 max_width_px, U64 max_lines)
{
  // NOTE(rad): labels for `text` broken at spaces to fit max_width_px (a
  // word wider than that gets a line of its own); returns the line count
  FNT_Tag font = ui_top_font();
  F32 font_size = ui_top_font_size();
  U64 lines = 0;
  for(U64 off = 0; off < text.size && lines < max_lines;)
  {
    U64 end = off;
    for(U64 next = off; next <= text.size;)
    {
      U64 space = str8_find_needle(text, next, str8_lit(" "), 0);
      F32 width = fnt_dim_from_tag_size_string(font, font_size, 0, 0, str8_substr(text, r1u64(off, space))).x;
      if(width > max_width_px && end > off)
      {
        break;
      }
      end = space;
      next = space + 1;
    }
    UI_PrefWidth(ui_text_dim(1, 1.f)) ui_label(str8_substr(text, r1u64(off, end)));
    lines += 1;
    off = end + 1;
  }
  return lines;
}

internal void
rad_markdown_ui(String8 markdown, F32 max_width_px, U64 max_lines)
{
  // NOTE(rad): builds a column of labels (e.g. in a tooltip): fenced code
  // blocks in the code font, colored by the editor's lexer (Rust, or C for
  // c/cpp blocks); paragraphs (consecutive text lines) joined and wrapped at
  // max_width_px; `---` a divider; headings and list items start a new
  // paragraph; reference-link definitions dropped. At most max_lines lines.
  Temp scratch = scratch_begin(0, 0);
  B32 in_code = 0;
  TXT_LangKind code_lang = TXT_LangKind_Rust;
  U64 lines_shown = 0;
  B32 gap_pending = 0;
  B32 any_shown = 0;
  String8List paragraph = {0};
  Vec4F32 text_color = ui_color_from_name(str8_lit("text"));
  for(U64 line_start = 0; line_start <= markdown.size && lines_shown < max_lines;)
  {
    U64 line_end = str8_find_needle(markdown, line_start, str8_lit("\n"), 0);
    B32 at_end = (line_end >= markdown.size);
    String8 line = str8_substr(markdown, r1u64(line_start, line_end));
    line_start = line_end + 1;
    if(line.size != 0 && line.str[line.size-1] == '\r')
    {
      line.size -= 1;
    }
    String8 trimmed = str8_skip_chop_whitespace(line);
    B32 is_fence = str8_match(str8_prefix(trimmed, 3), str8_lit("```"), 0);
    B32 is_rule = (!in_code && (str8_match(trimmed, str8_lit("---"), 0) || str8_match(trimmed, str8_lit("***"), 0)));
    B32 is_blank = (!in_code && trimmed.size == 0);
    B32 is_heading = (!in_code && trimmed.size != 0 && trimmed.str[0] == '#');
    B32 is_item = (!in_code && (str8_match(str8_prefix(trimmed, 2), str8_lit("- "), 0) || str8_match(str8_prefix(trimmed, 2), str8_lit("* "), 0)));
    B32 is_ref_def = (!in_code && trimmed.size != 0 && trimmed.str[0] == '[' && str8_find_needle(trimmed, 0, str8_lit("]: "), 0) < trimmed.size);
    B32 is_text = (!in_code && !is_fence && !is_rule && !is_blank && !is_ref_def);

    //- rad: a paragraph ends at anything but a plain text line
    if(paragraph.node_count != 0 && (!is_text || is_heading || is_item))
    {
      if(gap_pending)
      {
        ui_spacer(ui_em(0.5f, 1.f));
        gap_pending = 0;
      }
      String8 text = str8_list_join(scratch.arena, &paragraph, &(StringJoin){.sep = str8_lit(" ")});
      lines_shown += rad_markdown_wrapped_labels(rad_markdown_inline_plain(scratch.arena, text), max_width_px, max_lines - lines_shown);
      any_shown = 1;
      MemoryZeroStruct(&paragraph);
    }

    //- rad: fences
    if(is_fence)
    {
      in_code = !in_code;
      if(in_code)
      {
        String8 lang = str8_skip(trimmed, 3);
        code_lang = (str8_match(lang, str8_lit("c"), 0) || str8_match(lang, str8_lit("cpp"), 0) ? TXT_LangKind_C : TXT_LangKind_Rust);
      }
    }

    //- rad: gaps (collapsed; none before the first line)
    else if(is_blank || is_ref_def)
    {
      gap_pending = (any_shown || paragraph.node_count != 0);
    }

    //- rad: code line
    else if(in_code && lines_shown < max_lines)
    {
      if(gap_pending)
      {
        ui_spacer(ui_em(0.5f, 1.f));
        gap_pending = 0;
      }
      RAD_Font(RAD_FontSlot_Code) UI_PrefWidth(ui_text_dim(1, 1.f))
      {
        // NOTE(rad): the box keeps the fstrs until the frame is drawn: they
        // go in the UI build arena
        DR_FStrList fstrs = rad_fstrs_from_code_string_lang(ui_build_arena(), 1.f, text_color, code_lang, line);
        UI_Box *box = ui_build_box_from_key(UI_BoxFlag_DrawText|UI_BoxFlag_DisableTextTrunc, ui_key_zero());
        ui_box_equip_display_fstrs(box, &fstrs);
      }
      lines_shown += 1;
      any_shown = 1;
    }

    //- rad: rule
    else if(is_rule)
    {
      UI_TagF("floating") ui_divider(ui_em(1.f, 1.f));
      gap_pending = 0;
    }

    //- rad: text: into the paragraph (headings without their #s)
    else if(is_text)
    {
      if(is_heading)
      {
        U64 space = str8_find_needle(trimmed, 0, str8_lit(" "), 0);
        trimmed = str8_skip_chop_whitespace(str8_skip(trimmed, space));
        gap_pending = any_shown;
      }
      str8_list_push(scratch.arena, &paragraph, trimmed);
    }
    if(at_end)
    {
      break;
    }
  }

  //- rad: the last paragraph
  if(paragraph.node_count != 0 && lines_shown < max_lines)
  {
    if(gap_pending)
    {
      ui_spacer(ui_em(0.5f, 1.f));
    }
    String8 text = str8_list_join(scratch.arena, &paragraph, &(StringJoin){.sep = str8_lit(" ")});
    lines_shown += rad_markdown_wrapped_labels(rad_markdown_inline_plain(scratch.arena, text), max_width_px, max_lines - lines_shown);
  }
  if(lines_shown >= max_lines)
  {
    UI_TagF("weak") UI_PrefWidth(ui_text_dim(1, 1.f)) ui_label(str8_lit("..."));
  }
  scratch_end(scratch);
}
