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
