// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

////////////////////////////////
//~ rad: Generated Code

#include "rad/generated/rad.meta.c"

////////////////////////////////
//~ rad: Settings Functions

internal void
rad_setting_store_f64__(RAD_Settings *s, RAD_SettingInfo *info, F64 v)
{
  U8 *ptr = (U8 *)s + info->offset;
  switch(info->kind)
  {
    default:{}break;
    case RAD_SettingKind_Bool:  {*(B32 *)ptr = (v != 0);}break;
    case RAD_SettingKind_F32:   {*(F32 *)ptr = (F32)v;}break;
    case RAD_SettingKind_U64:
    case RAD_SettingKind_Enum:
    case RAD_SettingKind_Theme: {*(U64 *)ptr = (U64)v;}break;
  }
}

internal F64
rad_setting_f64_from_settings(RAD_Settings *s, U64 idx)
{
  RAD_SettingInfo *info = &rad_setting_info_table[idx];
  U8 *ptr = (U8 *)s + info->offset;
  F64 result = 0;
  switch(info->kind)
  {
    default:{}break;
    case RAD_SettingKind_Bool:  {result = (F64)*(B32 *)ptr;}break;
    case RAD_SettingKind_F32:   {result = (F64)*(F32 *)ptr;}break;
    case RAD_SettingKind_U64:
    case RAD_SettingKind_Enum:
    case RAD_SettingKind_Theme: {result = (F64)*(U64 *)ptr;}break;
  }
  return result;
}

internal void
rad_settings_derive__(RAD_Settings *s)
{
  // rad: density -> row and tab heights, in multiples of the font size
  // (Comfortable: raddbg's 3 rows; tabs a little lower than raddbg's 3,
  // closer to Zed's: TODO_2 B2)
  s->row_height = (s->density == 1 ? 2.25f : 3.f);
  s->tab_height = (s->density == 1 ? 2.25f : 2.6f);
}

internal RAD_Settings
rad_default_settings(void)
{
  // NOTE(rad): the defaults of RAD_SettingTable (rad.mdesk)
  RAD_Settings s = {0};
  for EachElement(idx, rad_setting_info_table)
  {
    rad_setting_store_f64__(&s, &rad_setting_info_table[idx], rad_setting_info_table[idx].default_value);
  }
  s.theme_preset = RAD_ThemePreset_Vsa; // rad: the user's theme, first in the table (task G7)
  rad_settings_derive__(&s);
  return s;
}

internal CFG_Node *
rad_setting_cfg_node(String8 name)
{
  // NOTE(rad): user file: `settings: {<name>: <value> ...}`
  CFG_Node *user = cfg_node_child_from_string(cfg_node_root(), str8_lit("user"));
  CFG_Node *settings = cfg_node_child_from_string(user, str8_lit("settings"));
  return cfg_node_child_from_string(settings, name);
}

internal RAD_Settings
rad_settings_from_cfg(void)
{
  // NOTE(rad): defaults, then the user file's values (clamped to their
  // range; enums by index or option name; the theme by preset name)
  RAD_Settings s = rad_default_settings();
  for EachElement(idx, rad_setting_info_table)
  {
    RAD_SettingInfo *info = &rad_setting_info_table[idx];
    CFG_Node *node = rad_setting_cfg_node(info->name);
    String8 value = node->first->string;
    if(value.size == 0)
    {
      continue;
    }
    F64 v = info->default_value;
    B32 good = 1;
    if(info->kind == RAD_SettingKind_String)
    {
      // rad: copied (the user file's node may change during the frame)
      U64 size = Min(value.size, sizeof(rad_state->setting_string_buffers[idx]));
      MemoryCopy(rad_state->setting_string_buffers[idx], value.str, size);
      *(String8 *)((U8 *)&s + info->offset) = str8(rad_state->setting_string_buffers[idx], size);
      continue;
    }
    switch(info->kind)
    {
      default:{good = 0;}break;
      case RAD_SettingKind_Bool:
      {
        v = (str8_match(value, str8_lit("1"), 0) || str8_match(value, str8_lit("true"), StringMatchFlag_CaseInsensitive));
      }break;
      case RAD_SettingKind_U64:
      case RAD_SettingKind_F32:
      {
        v = f64_from_str8(value);
        v = Clamp(info->min, v, info->max);
      }break;
      case RAD_SettingKind_Enum:
      {
        good = 0;
        U8 split = '|';
        Temp scratch = scratch_begin(0, 0);
        String8List options = str8_split(scratch.arena, info->options, &split, 1, 0);
        U64 option_idx = 0;
        for(String8Node *n = options.first; n != 0; n = n->next, option_idx += 1)
        {
          if(str8_match(n->string, value, StringMatchFlag_CaseInsensitive))
          {
            v = (F64)option_idx;
            good = 1;
          }
        }
        if(!good && char_is_digit(value.str[0], 10))
        {
          v = Clamp(info->min, f64_from_str8(value), info->max);
          good = 1;
        }
        scratch_end(scratch);
      }break;
      case RAD_SettingKind_Theme:
      {
        good = 0;
        for EachEnumVal(RAD_ThemePreset, preset)
        {
          if(str8_match(rad_theme_preset_code_string_table[preset], value, StringMatchFlag_CaseInsensitive))
          {
            v = (F64)preset;
            good = 1;
          }
        }
      }break;
    }
    if(good)
    {
      rad_setting_store_f64__(&s, info, v);
    }
  }
  rad_settings_derive__(&s);
  return s;
}

internal String8
rad_setting_value_string(Arena *arena, RAD_Settings *settings, U64 idx)
{
  // NOTE(rad): a setting's value as the user file and the Settings view show it
  RAD_SettingInfo *info = &rad_setting_info_table[idx];
  F64 v = rad_setting_f64_from_settings(settings, idx);
  String8 result = {0};
  switch(info->kind)
  {
    default:{}break;
    case RAD_SettingKind_Bool: {result = (v != 0 ? str8_lit("1") : str8_lit("0"));}break;
    case RAD_SettingKind_U64:  {result = push_str8f(arena, "%I64u", (U64)v);}break;
    case RAD_SettingKind_F32:  {result = push_str8f(arena, "%.2f", v);}break;
    case RAD_SettingKind_Enum:
    {
      U8 split = '|';
      String8List options = str8_split(arena, info->options, &split, 1, 0);
      U64 option_idx = 0;
      for(String8Node *n = options.first; n != 0; n = n->next, option_idx += 1)
      {
        if(option_idx == (U64)v) { result = n->string; }
      }
    }break;
    case RAD_SettingKind_Theme:
    {
      result = rad_theme_preset_code_string_table[Min((U64)v, RAD_ThemePreset_COUNT-1)];
    }break;
    case RAD_SettingKind_String:
    {
      result = push_str8_copy(arena, *(String8 *)((U8 *)settings + info->offset));
    }break;
  }
  return result;
}

internal void
rad_setting_set_f64(String8 name, F64 value)
{
  // NOTE(rad): writes the user file's value (a value equal to the default is
  // written too: the file then says what was chosen); clamped to the range
  for EachElement(idx, rad_setting_info_table)
  {
    RAD_SettingInfo *info = &rad_setting_info_table[idx];
    if(!str8_match(info->name, name, 0))
    {
      continue;
    }
    if(info->kind == RAD_SettingKind_Theme)
    {
      value = Clamp(0, value, RAD_ThemePreset_COUNT-1);
    }
    else
    {
      value = Clamp(info->min, value, info->max);
    }
    Temp scratch = scratch_begin(0, 0);
    RAD_Settings s = rad_state->settings;
    rad_setting_store_f64__(&s, info, value);
    String8 value_string = rad_setting_value_string(scratch.arena, &s, idx);
    CFG_Node *user = cfg_node_child_from_string(cfg_node_root(), str8_lit("user"));
    if(user != &cfg_nil_node)
    {
      CFG_Node *settings = cfg_node_child_from_string(user, str8_lit("settings"));
      if(settings == &cfg_nil_node)
      {
        settings = cfg_node_new(rad_state->cfg, user, str8_lit("settings"));
      }
      CFG_Node *node = cfg_node_child_from_string(settings, name);
      if(node == &cfg_nil_node)
      {
        node = cfg_node_new(rad_state->cfg, settings, name);
      }
      if(node->first == &cfg_nil_node)
      {
        cfg_node_new(rad_state->cfg, node, value_string);
      }
      else
      {
        cfg_node_equip_string(rad_state->cfg, node->first, value_string);
      }
    }
    rad_state->settings = rad_settings_from_cfg();
    rad_request_frame();
    scratch_end(scratch);
    break;
  }
}

internal void
rad_setting_set_string(String8 name, String8 value)
{
  // NOTE(rad): String settings (fonts): written as is; empty = the default
  value = str8_skip_chop_whitespace(value);
  if(value.size == 0)
  {
    rad_setting_revert(name);
  }
  else
  {
    CFG_Node *user = cfg_node_child_from_string(cfg_node_root(), str8_lit("user"));
    if(user != &cfg_nil_node)
    {
      CFG_Node *settings = cfg_node_child_from_string(user, str8_lit("settings"));
      if(settings == &cfg_nil_node)
      {
        settings = cfg_node_new(rad_state->cfg, user, str8_lit("settings"));
      }
      CFG_Node *node = cfg_node_child_from_string(settings, name);
      if(node == &cfg_nil_node)
      {
        node = cfg_node_new(rad_state->cfg, settings, name);
      }
      if(node->first == &cfg_nil_node)
      {
        cfg_node_new(rad_state->cfg, node, value);
      }
      else
      {
        cfg_node_equip_string(rad_state->cfg, node->first, value);
      }
    }
    rad_state->settings = rad_settings_from_cfg();
    rad_request_frame();
  }
}

internal void
rad_setting_revert(String8 name)
{
  // NOTE(rad): back to the default: the user file forgets the value
  CFG_Node *node = rad_setting_cfg_node(name);
  if(node != &cfg_nil_node)
  {
    cfg_node_release(rad_state->cfg, node);
  }
  rad_state->settings = rad_settings_from_cfg();
  rad_request_frame();
}

internal F32
rad_panel_font_size(RAD_Window *ws)
{
  // NOTE(rad): the bottom panel's font size (Output, Problems; TODO_2 B4),
  // scaled like the window's; the editor's zoom does not change it
  F32 result = Clamp(6.f, floor_f32((F32)rad_state->settings.panel_font_size*ws->font_scale), 72.f);
  return result;
}

internal void
rad_hide_mouse_while_typing(void)
{
  // NOTE(rad): not in raddbg (user 2026-11-07: the mouse rested on code
  // while typing, and its docs popped up): typing in the editor hides the
  // pointer, and its hover, until the mouse moves (the event loop)
  if(rad_state->settings.hide_mouse_while_typing)
  {
    if(!rad_state->mouse_hidden_by_typing)
    {
      rad_state->mouse_hidden_by_typing = 1;
      rad_state->mouse_hidden_pos = ui_mouse();
    }
    wm_hide_cursor_until_mouse_moves();
  }
}

internal F32
rad_completion_font_size(RAD_Window *ws)
{
  // NOTE(rad): the completion list's font size (user 2026-11-07), scaled like
  // the window's; 0 in the settings: the window's UI font size (raddbg's)
  F32 result = floor_f32((F32)rad_state->settings.font_size*ws->font_scale);
  if(rad_state->settings.completion_font_size != 0)
  {
    result = floor_f32((F32)rad_state->settings.completion_font_size*ws->font_scale);
  }
  result = Clamp(6.f, result, 72.f);
  return result;
}

internal F32
rad_code_font_size(RAD_Window *ws)
{
  // NOTE(rad): the code font size (editor), scaled like the window's (DPI
  // changes). While a zoom eases: any size in between, marked as scaled for
  // the font cache (drawn from the whole sizes' glyphs, so text grows
  // continuously and nothing new is rasterized; raddbg would floor it)
  F32 setting_pt = (F32)rad_state->settings.code_font_size;
  F32 pt = (rad_state->code_zoom_pt != 0 ? rad_state->code_zoom_pt : setting_pt);
  F32 size = pt*ws->font_scale;
  F32 result = (pt == setting_pt ? floor_f32(size) : size);
  result = Clamp(6.f, result, 72.f);
  fnt_mark_scaled_size(result);
  return result;
}

////////////////////////////////
//~ rad: Main State Accessors

internal Arena *
rad_frame_arena(void)
{
  return rad_state->frame_arenas[rad_state->frame_index%ArrayCount(rad_state->frame_arenas)];
}

internal void
rad_request_frame(void)
{
  rad_state->num_frames_requested = 4;
}

internal void
rad_request_frame_at(U64 us)
{
  // NOTE(rad): a frame at (or soon after) `us`; until then rad may sleep
  if(us <= now_time_us())
  {
    rad_request_frame();
  }
  else if(rad_state->frame_wake_us == 0 || us < rad_state->frame_wake_us)
  {
    rad_state->frame_wake_us = us;
  }
}

internal void
rad_set_error(String8 string)
{
  rad_state->error_string_size = Min(sizeof(rad_state->error_buffer), string.size);
  MemoryCopy(rad_state->error_buffer, string.str, rad_state->error_string_size);
  rad_state->error_t = 1.f;
  rad_request_frame();
}

////////////////////////////////
//~ rad: Commands

internal void
rad_push_cmd(RAD_CmdKind kind, RAD_CmdParams *params)
{
  RAD_CmdNode *n = push_array(rad_state->cmds_arenas[0], RAD_CmdNode, 1);
  n->cmd.kind = kind;
  if(params != 0)
  {
    MemoryCopyStruct(&n->cmd.params, params);
    n->cmd.params.file_path = push_str8_copy(rad_state->cmds_arenas[0], params->file_path);
    n->cmd.params.string = push_str8_copy(rad_state->cmds_arenas[0], params->string);
  }
  if(n->cmd.params.window == 0 && rad_state->current_window != 0)
  {
    n->cmd.params.window = rad_state->current_window->cfg_id;
  }
  SLLQueuePush(rad_state->cmds[0].first, rad_state->cmds[0].last, n);
  rad_state->cmds[0].count += 1;
  rad_request_frame();
}

// rad: the Mac's own keys: they replace the command table's (and are shown
// in menus). Cmd is Ctrl here (the window manager sends Cmd as Ctrl).
#if OS_MAC
read_only global RAD_CmdBinding rad_cmd_mac_binding_table[] =
{
  // rad: Option+Left is "word left" on the Mac (text navigation); back is
  // Cmd+[ as in Xcode and Safari (Zed's Ctrl+- would need the Control key
  // told apart from Cmd)
  {RAD_CmdKind_NavigateBack, WM_Key_LeftBracket, WM_Modifier_Ctrl},
};
#endif

// rad: second keys (not shown in menus)
read_only global RAD_CmdBinding rad_cmd_extra_binding_table[] =
{
  {RAD_CmdKind_IncCodeFontSize, WM_Key_Equal, WM_Modifier_Ctrl|WM_Modifier_Shift},
  {RAD_CmdKind_DecCodeFontSize, WM_Key_Minus, WM_Modifier_Ctrl|WM_Modifier_Shift},
#if OS_MAC
  {RAD_CmdKind_NextTab,         WM_Key_RightBracket, WM_Modifier_Ctrl|WM_Modifier_Shift},
  {RAD_CmdKind_PrevTab,         WM_Key_LeftBracket,  WM_Modifier_Ctrl|WM_Modifier_Shift},
#endif
};

// rad: the explorer's own keys (TODO_2 D2), matched by the explorer while
// its list has focus and never as global commands (Ctrl+C there copies a
// file, in an editor text); menus show them. Ctrl+C / X / V and Delete reach
// the explorer as edit / paste events (rad_text_nav_binding_table), the
// others as key presses.
read_only global RAD_CmdBinding rad_explorer_binding_table[] =
{
  {RAD_CmdKind_ExplorerRename,           WM_Key_F2,        0},
#if OS_MAC
  {RAD_CmdKind_ExplorerDelete,           WM_Key_Backspace, WM_Modifier_Ctrl},
#else
  {RAD_CmdKind_ExplorerDelete,           WM_Key_Delete,    0},
#endif
  {RAD_CmdKind_ExplorerCut,              WM_Key_X,         WM_Modifier_Ctrl},
  {RAD_CmdKind_ExplorerCopy,             WM_Key_C,         WM_Modifier_Ctrl},
  {RAD_CmdKind_ExplorerPaste,            WM_Key_V,         WM_Modifier_Ctrl},
  {RAD_CmdKind_ExplorerCopyPath,         WM_Key_C,         WM_Modifier_Ctrl|WM_Modifier_Alt},
  {RAD_CmdKind_ExplorerCopyRelativePath, WM_Key_C,         WM_Modifier_Ctrl|WM_Modifier_Alt|WM_Modifier_Shift},
  {RAD_CmdKind_ExplorerReveal,           WM_Key_R,         WM_Modifier_Ctrl|WM_Modifier_Alt},
};

internal void
rad_popup_open(String8 title, String8 desc, RAD_CmdKind kind, String8 path)
{
  arena_clear(rad_state->popup_arena);
  rad_state->popup_active   = 1;
  rad_state->popup_title    = push_str8_copy(rad_state->popup_arena, title);
  rad_state->popup_desc     = push_str8_copy(rad_state->popup_arena, desc);
  rad_state->popup_cmd_kind = kind;
  rad_state->popup_cmd_path = push_str8_copy(rad_state->popup_arena, path);
  rad_request_frame();
}

internal CFG_Binding
rad_binding_from_cmd_kind(RAD_CmdKind kind)
{
  CFG_Binding result = rad_cmd_kind_info_table[kind].default_binding;
#if OS_MAC
  for EachElement(idx, rad_cmd_mac_binding_table)
  {
    if(rad_cmd_mac_binding_table[idx].kind == kind)
    {
      result.key = rad_cmd_mac_binding_table[idx].key;
      result.modifiers = rad_cmd_mac_binding_table[idx].modifiers;
      break;
    }
  }
#endif
  return result;
}

internal RAD_CmdKind
rad_cmd_kind_from_key(WM_Key key, WM_Modifiers modifiers)
{
  RAD_CmdKind result = RAD_CmdKind_Null;
  if(key != WM_Key_Null)
  {
    for EachNonZeroEnumVal(RAD_CmdKind, k)
    {
      CFG_Binding binding = rad_binding_from_cmd_kind(k);
      if(binding.key == key && binding.modifiers == modifiers)
      {
        result = k;
        break;
      }
    }
    if(result == RAD_CmdKind_Null) for EachElement(idx, rad_cmd_extra_binding_table)
    {
      if(rad_cmd_extra_binding_table[idx].key == key && rad_cmd_extra_binding_table[idx].modifiers == modifiers)
      {
        result = rad_cmd_extra_binding_table[idx].kind;
        break;
      }
    }
  }
  return result;
}

////////////////////////////////
//~ rad: Fonts

internal FNT_Tag
rad_font_from_slot(RAD_FontSlot slot)
{
  FNT_Tag tag = rad_state->font_slot_table[slot];
  return tag;
}

internal FNT_RasterFlags
rad_raster_flags_from_slot(RAD_FontSlot slot)
{
  FNT_RasterFlags flags = 0;
  if(rad_state->current_window != 0)
  {
    flags = rad_state->current_window->font_slot_raster_flags[slot];
  }
  return flags;
}

////////////////////////////////
//~ rad: Config Helpers

internal CFG_Node *
rad_window_cfg_from_cfg(CFG_Node *cfg)
{
  return cfg_window_from_cfg(cfg);
}

internal CFG_PanelNode *
rad_focused_panel_from_window_cfg(Arena *arena, CFG_Node *window_cfg)
{
  CFG_PanelTree panel_tree = cfg_panel_tree_from_cfg(arena, window_cfg);
  return panel_tree.focused;
}

internal void
rad_focus_panel(CFG_Node *panel)
{
  // NOTE(rad): port of raddbg's RD_CmdKind_FocusPanel - exactly one panel in
  // the window's tree carries a `selected` child.
  Temp scratch = scratch_begin(0, 0);
  CFG_PanelTree panel_tree = cfg_panel_tree_from_cfg(scratch.arena, panel);
  CFG_Node *selection_cfg = &cfg_nil_node;
  for(CFG_PanelNode *p = panel_tree.root;
      p != &cfg_nil_panel_node;
      p = cfg_panel_node_rec__depth_first_pre(panel_tree.root, p).next)
  {
    CFG_Node *p_cfg = p->cfg;
    CFG_Node *p_selection = cfg_node_child_from_string(p_cfg, str8_lit("selected"));
    if(selection_cfg == &cfg_nil_node)
    {
      selection_cfg = p_selection;
    }
    else for(CFG_Node *s = p_selection; s != &cfg_nil_node; s = cfg_node_child_from_string(p_cfg, str8_lit("selected")))
    {
      cfg_node_release(rad_state->cfg, s);
    }
  }
  if(selection_cfg == &cfg_nil_node)
  {
    selection_cfg = cfg_node_alloc(rad_state->cfg);
    cfg_node_equip_string(rad_state->cfg, selection_cfg, str8_lit("selected"));
  }
  if(panel != &cfg_nil_node)
  {
    cfg_node_insert_child(rad_state->cfg, panel, &cfg_nil_node, selection_cfg);
    RAD_Window *ws = rad_window_from_cfg(rad_window_cfg_from_cfg(panel));
    if(ws != 0)
    {
      ws->menu_bar_focused = 0;
    }
    CFG_PanelNode *panel_node = cfg_panel_node_from_tree_cfg(panel_tree.root, panel);
    if(panel_node->first == &cfg_nil_panel_node && !rad_tab_is_tool(panel_node->selected_tab))
    {
      rad_state->last_editor_panel = panel->id;
    }
  }
  scratch_end(scratch);
}

internal void
rad_focus_tab(CFG_Node *tab)
{
  // NOTE(rad): port of raddbg's RD_CmdKind_FocusTab - exactly one tab in each
  // panel carries a `selected` child.
  Temp scratch = scratch_begin(0, 0);
  CFG_Node *panel = tab->parent;
  if(panel != &cfg_nil_node)
  {
    CFG_PanelTree panel_tree = cfg_panel_tree_from_cfg(scratch.arena, panel);
    CFG_PanelNode *panel_node = cfg_panel_node_from_tree_cfg(panel_tree.root, panel);
    CFG_Node *selection_cfg = &cfg_nil_node;
    for(CFG_NodePtrNode *n = panel_node->tabs.first; n != 0; n = n->next)
    {
      CFG_Node *tab_selection_cfg = cfg_node_child_from_string(n->v, str8_lit("selected"));
      if(selection_cfg == &cfg_nil_node)
      {
        selection_cfg = tab_selection_cfg;
        cfg_node_unhook(rad_state->cfg, n->v, selection_cfg);
      }
      else for(CFG_Node *s = tab_selection_cfg; s != &cfg_nil_node; s = cfg_node_child_from_string(n->v, str8_lit("selected")))
      {
        cfg_node_release(rad_state->cfg, s);
      }
    }
    if(selection_cfg == &cfg_nil_node)
    {
      selection_cfg = cfg_node_alloc(rad_state->cfg);
      cfg_node_equip_string(rad_state->cfg, selection_cfg, str8_lit("selected"));
    }
    cfg_node_insert_child(rad_state->cfg, tab, &cfg_nil_node, selection_cfg);
  }
  scratch_end(scratch);
}

internal CFG_Node *
rad_explorer_tab_from_panel(CFG_Node *panel)
{
  // NOTE(rad): the explorer is the only tab of its panel: it cannot be
  // closed, and nothing else is opened into its panel.
  CFG_Node *result = &cfg_nil_node;
  for(CFG_Node *child = panel->first; child != &cfg_nil_node; child = child->next)
  {
    if(str8_match(child->string, str8_lit("explorer"), 0))
    {
      result = child;
      break;
    }
  }
  return result;
}

internal CFG_Node *
rad_panel_alloc_at_window_edge(CFG_Node *window, Side side, F32 pct)
{
  // NOTE(rad): new full-height panel at the window's left (Side_Min) or right
  // (Side_Max) edge, `pct` of the window's width; the other panels keep their
  // layout in the rest.
  Temp scratch = scratch_begin(0, 0);
  CFG_PanelTree panel_tree = cfg_panel_tree_from_cfg(scratch.arena, window);
  CFG_PanelNode *root = panel_tree.root;
  CFG_Node *panel = cfg_node_alloc(rad_state->cfg);
  cfg_node_equip_stringf(rad_state->cfg, panel, "%f", pct);

  // rad: no panels -> new root
  if(root == &cfg_nil_panel_node)
  {
    cfg_node_equip_string(rad_state->cfg, panel, str8_lit("panels"));
    cfg_node_insert_child(rad_state->cfg, window, window->last, panel);
  }

  // rad: root already split left/right -> new first/last child, others shrink
  else if(root->first != &cfg_nil_panel_node && root->split_axis == Axis2_X)
  {
    for(CFG_PanelNode *child = root->first; child != &cfg_nil_panel_node; child = child->next)
    {
      cfg_node_equip_stringf(rad_state->cfg, child->cfg, "%f", child->pct_of_parent*(1.f-pct));
    }
    CFG_Node *prev = (side == Side_Min ? &cfg_nil_node : root->last->cfg);
    cfg_node_insert_child(rad_state->cfg, root->cfg, prev, panel);
  }

  // rad: root is one panel, or split top/bottom -> new left/right root above it
  else
  {
    CFG_Node *old_root = root->cfg;
    CFG_Node *new_root = cfg_node_alloc(rad_state->cfg);
    cfg_node_unhook(rad_state->cfg, window, old_root);
    cfg_node_equip_stringf(rad_state->cfg, old_root, "%f", 1.f-pct);
    cfg_node_equip_string(rad_state->cfg, new_root, str8_lit("panels"));
    cfg_node_insert_child(rad_state->cfg, new_root, new_root->last, side == Side_Min ? panel : old_root);
    cfg_node_insert_child(rad_state->cfg, new_root, new_root->last, side == Side_Min ? old_root : panel);
    cfg_node_insert_child(rad_state->cfg, window, window->last, new_root);
    cfg_node_child_from_string_or_alloc(rad_state->cfg, window, str8_lit("split_x"));
  }
  scratch_end(scratch);
  return panel;
}

internal CFG_Node *
rad_panel_split(CFG_Node *split_panel, Dir2 split_dir, F32 new_pct)
{
  // NOTE(rad): port of the split part of raddbg's RD_CmdKind_NewPanel*/
  // SplitPanel (without tab drag/drop handling): a new empty panel on
  // `split_dir`'s side of `split_panel`. Across the parent's split axis the
  // two share `split_panel`'s space (new: `new_pct`); along it, the new panel
  // gets an equal share (same as raddbg). Does not focus it.
  Temp scratch = scratch_begin(0, 0);
  CFG_Node *window = rad_window_cfg_from_cfg(split_panel);
  CFG_PanelTree panel_tree = cfg_panel_tree_from_cfg(scratch.arena, window);
  Axis2 split_axis = axis2_from_dir2(split_dir);
  Side split_side = side_from_dir2(split_dir);
  CFG_Node *new_panel_cfg = &cfg_nil_node;
  CFG_PanelNode *panel = cfg_panel_node_from_tree_cfg(panel_tree.root, split_panel);
  CFG_PanelNode *parent = panel->parent;

  // rad: splitting on same axis as parent -> insert new sibling on same axis, adjust sizes
  if(parent != &cfg_nil_panel_node && parent->split_axis == split_axis)
  {
    CFG_Node *parent_cfg = parent->cfg;
    CFG_Node *new_cfg = cfg_node_alloc(rad_state->cfg);
    cfg_node_insert_child(rad_state->cfg, parent_cfg, split_side == Side_Max ? split_panel : split_panel->prev, new_cfg);
    if(rad_panel_has_tool_child__(parent))
    {
      // rad: next to a tool window (the explorer's row): the split panel
      // gives half its space; the tool window keeps its size
      cfg_node_equip_stringf(rad_state->cfg, new_cfg, "%f", panel->pct_of_parent*0.5f);
      cfg_node_equip_stringf(rad_state->cfg, split_panel, "%f", panel->pct_of_parent*0.5f);
    }
    else
    {
      cfg_node_equip_stringf(rad_state->cfg, new_cfg, "%f", 1.f/(parent->child_count+1));
      for(CFG_PanelNode *child = parent->first; child != &cfg_nil_panel_node; child = child->next)
      {
        F32 old_pct = child->pct_of_parent;
        F32 new_pct = old_pct * ((F32)(parent->child_count) / (parent->child_count+1));
        cfg_node_equip_stringf(rad_state->cfg, child->cfg, "%f", new_pct);
      }
    }
    new_panel_cfg = new_cfg;
  }

  // rad: splitting on opposite axis as parent - need to create new replacement node, + new sibling
  else
  {
    CFG_Node *split_panel_prev = panel->prev->cfg;
    CFG_Node *new_parent = cfg_node_alloc(rad_state->cfg);
    CFG_Node *new_sibling = cfg_node_alloc(rad_state->cfg);
    if(panel == panel_tree.zoom_root)
    {
      CFG_Node *zoom_panel = cfg_node_child_from_string_or_alloc(rad_state->cfg, split_panel, str8_lit("zoom_root"));
      cfg_node_insert_child(rad_state->cfg, new_parent, &cfg_nil_node, zoom_panel);
    }
    cfg_node_equip_string(rad_state->cfg, new_parent, split_panel->string);
    cfg_node_equip_stringf(rad_state->cfg, split_panel, "%f", 1.f-new_pct);
    cfg_node_equip_stringf(rad_state->cfg, new_sibling, "%f", new_pct);
    if(parent->cfg != &cfg_nil_node)
    {
      cfg_node_unhook(rad_state->cfg, parent->cfg, split_panel);
      cfg_node_insert_child(rad_state->cfg, parent->cfg, split_panel_prev, new_parent);
    }
    else
    {
      cfg_node_equip_string(rad_state->cfg, new_parent, str8_lit("panels"));
      cfg_node_insert_child(rad_state->cfg, window, window->last, new_parent);
      if(split_axis == Axis2_X)
      {
        cfg_node_child_from_string_or_alloc(rad_state->cfg, window, str8_lit("split_x"));
      }
      else
      {
        cfg_node_release(rad_state->cfg, cfg_node_child_from_string(window, str8_lit("split_x")));
      }
    }
    CFG_Node *min = split_panel;
    CFG_Node *max = new_sibling;
    if(split_side == Side_Min)
    {
      Swap(CFG_Node *, min, max);
    }
    cfg_node_insert_child(rad_state->cfg, new_parent, new_parent->last, min);
    cfg_node_insert_child(rad_state->cfg, new_parent, new_parent->last, max);
    new_panel_cfg = new_sibling;
  }
  scratch_end(scratch);
  return new_panel_cfg;
}

internal B32
rad_tab_is_tool(CFG_Node *tab)
{
  // NOTE(rad): tabs that files never open next to: Open File / Go To Location
  // aimed at their panel go to the editor panel instead
  B32 result = (str8_match(tab->string, str8_lit("explorer"), 0) ||
                str8_match(tab->string, str8_lit("output"), 0) ||
                str8_match(tab->string, str8_lit("problems"), 0));
  return result;
}

internal B32
rad_tab_has_code_view(CFG_Node *tab)
{
  // NOTE(rad): tabs showing text in a code view (Go To Line, Find)
  B32 result = (str8_match(tab->string, str8_lit("text"), 0) ||
                str8_match(tab->string, str8_lit("output"), 0));
  return result;
}

////////////////////////////////
//~ rad: Tool Windows

internal RAD_ToolKind
rad_tool_kind_from_panel(CFG_Node *panel)
{
  // NOTE(rad): a tool window is a panel holding tool tabs (the layout keeps
  // them apart from editor tabs)
  RAD_ToolKind result = RAD_ToolKind_Null;
  for(CFG_Node *child = panel->first; child != &cfg_nil_node; child = child->next)
  {
    if(str8_match(child->string, str8_lit("explorer"), 0))
    {
      result = RAD_ToolKind_Explorer;
      break;
    }
    if(str8_match(child->string, str8_lit("output"), 0) || str8_match(child->string, str8_lit("problems"), 0))
    {
      result = RAD_ToolKind_Bottom;
      break;
    }
  }
  return result;
}

internal CFG_Node *
rad_tool_panel_from_window(CFG_Node *window, RAD_ToolKind kind)
{
  Temp scratch = scratch_begin(0, 0);
  CFG_Node *result = &cfg_nil_node;
  CFG_PanelTree tree = cfg_panel_tree_from_cfg(scratch.arena, window);
  for(CFG_PanelNode *p = tree.root; p != &cfg_nil_panel_node; p = cfg_panel_node_rec__depth_first_pre(tree.root, p).next)
  {
    if(p->first == &cfg_nil_panel_node && rad_tool_kind_from_panel(p->cfg) == kind)
    {
      result = p->cfg;
      break;
    }
  }
  scratch_end(scratch);
  return result;
}

internal B32
rad_tool_is_visible(CFG_Node *window, RAD_ToolKind kind)
{
  CFG_Node *panel = rad_tool_panel_from_window(window, kind);
  B32 result = (panel != &cfg_nil_node && f64_from_str8(panel->string) > 0.001);
  return result;
}

internal String8
rad_tool_size_name(RAD_ToolKind kind)
{
  String8 result = (kind == RAD_ToolKind_Explorer ? str8_lit("explorer") : str8_lit("bottom"));
  return result;
}

internal void
rad_tool_set_visible(CFG_Node *window, RAD_ToolKind kind, B32 visible)
{
  // NOTE(rad): hide: the size goes to `tool_sizes` and to the neighbor;
  // show: taken back from the largest sibling (panel rects animate)
  Temp scratch = scratch_begin(0, 0);
  CFG_Node *panel_cfg = rad_tool_panel_from_window(window, kind);
  CFG_PanelTree tree = cfg_panel_tree_from_cfg(scratch.arena, window);
  CFG_PanelNode *panel = cfg_panel_node_from_tree_cfg(tree.root, panel_cfg);
  if(panel != &cfg_nil_panel_node && panel->parent != &cfg_nil_panel_node)
  {
    F32 pct = panel->pct_of_parent;
    CFG_Node *size = cfg_node_child_from_string(cfg_node_child_from_string(window, str8_lit("tool_sizes")), rad_tool_size_name(kind));
    if(!visible && pct > 0.001f)
    {
      CFG_Node *sizes = cfg_node_child_from_string_or_alloc(rad_state->cfg, window, str8_lit("tool_sizes"));
      size = cfg_node_child_from_string_or_alloc(rad_state->cfg, sizes, rad_tool_size_name(kind));
      CFG_PanelNode *neighbor = (panel->next != &cfg_nil_panel_node ? panel->next : panel->prev);
      cfg_node_new_replacef(rad_state->cfg, size, "%f", pct);
      cfg_node_equip_stringf(rad_state->cfg, neighbor->cfg, "%f", neighbor->pct_of_parent + pct);
      cfg_node_equip_stringf(rad_state->cfg, panel_cfg, "%f", 0.f);
      if(tree.focused == panel)
      {
        CFG_Node *editor_panel = rad_editor_panel_from_window(window);
        if(editor_panel != &cfg_nil_node)
        {
          rad_focus_panel(editor_panel);
        }
      }
    }
    else if(visible && pct <= 0.001f)
    {
      F32 restore = (F32)f64_from_str8(size->first->string);
      if(restore < 0.05f || restore > 0.8f)
      {
        restore = (kind == RAD_ToolKind_Explorer ? 0.2f : RAD_BOTTOM_TOOL_DEFAULT_PCT);
      }
      CFG_PanelNode *largest = &cfg_nil_panel_node;
      for(CFG_PanelNode *child = panel->parent->first; child != &cfg_nil_panel_node; child = child->next)
      {
        if(child != panel && (largest == &cfg_nil_panel_node || child->pct_of_parent > largest->pct_of_parent))
        {
          largest = child;
        }
      }
      F32 take = Min(restore, largest->pct_of_parent - 0.05f);
      if(take > 0.f)
      {
        cfg_node_equip_stringf(rad_state->cfg, largest->cfg, "%f", largest->pct_of_parent - take);
        cfg_node_equip_stringf(rad_state->cfg, panel_cfg, "%f", take);
      }
    }
  }
  scratch_end(scratch);
}

internal U64
rad_editor_leaf_count(CFG_Node *window)
{
  Temp scratch = scratch_begin(0, 0);
  U64 result = 0;
  CFG_PanelTree tree = cfg_panel_tree_from_cfg(scratch.arena, window);
  for(CFG_PanelNode *p = tree.root; p != &cfg_nil_panel_node; p = cfg_panel_node_rec__depth_first_pre(tree.root, p).next)
  {
    result += (p->first == &cfg_nil_panel_node && rad_tool_kind_from_panel(p->cfg) == RAD_ToolKind_Null);
  }
  scratch_end(scratch);
  return result;
}

internal B32
rad_panel_has_tool_child__(CFG_PanelNode *parent)
{
  B32 result = 0;
  for(CFG_PanelNode *child = parent->first; child != &cfg_nil_panel_node; child = child->next)
  {
    if(child->first == &cfg_nil_panel_node && rad_tool_kind_from_panel(child->cfg) != RAD_ToolKind_Null)
    {
      result = 1;
      break;
    }
  }
  return result;
}

internal B32
rad_panel_subtree_has_tool__(CFG_PanelNode *root)
{
  B32 result = 0;
  for(CFG_PanelNode *p = root; p != &cfg_nil_panel_node && !result; p = cfg_panel_node_rec__depth_first_pre(root, p).next)
  {
    result = (p->first == &cfg_nil_panel_node && rad_tool_kind_from_panel(p->cfg) != RAD_ToolKind_Null);
  }
  return result;
}

internal void
rad_panel_close(CFG_Node *window, CFG_Node *panel_cfg)
{
  // NOTE(rad): port of raddbg's RD_CmdKind_ClosePanel (the panel and its
  // tabs go; a parent left with one child is replaced by it)
  Temp scratch = scratch_begin(0, 0);
  CFG_PanelTree panel_tree = cfg_panel_tree_from_cfg(scratch.arena, window);
  CFG_PanelNode *panel = cfg_panel_node_from_tree_cfg(panel_tree.root, panel_cfg);
  CFG_PanelNode *parent = panel->parent;
  if(parent != &cfg_nil_panel_node)
  {
    // rad: if we're removing all but the last child of this parent, we should just remove both children.
    if(parent->child_count == 2)
    {
      CFG_PanelNode *discard_child = panel;
      CFG_PanelNode *keep_child = (panel == parent->first ? parent->last : parent->first);
      CFG_PanelNode *grandparent = parent->parent;
      CFG_PanelNode *parent_prev = parent->prev;
      F32 pct_of_parent = parent->pct_of_parent;

      // rad: unhook kept child
      cfg_node_unhook(rad_state->cfg, parent->cfg, keep_child->cfg);

      // rad: unhook this subtree
      if(grandparent != &cfg_nil_panel_node)
      {
        cfg_node_unhook(rad_state->cfg, grandparent->cfg, parent->cfg);
      }

      // rad: release the containing tree
      cfg_node_release(rad_state->cfg, parent->cfg);

      // rad: re-hook our kept child into the overall tree
      if(grandparent == &cfg_nil_panel_node)
      {
        if(keep_child->split_axis == Axis2_X)
        {
          cfg_node_child_from_string_or_alloc(rad_state->cfg, window, str8_lit("split_x"));
        }
        else
        {
          cfg_node_release(rad_state->cfg, cfg_node_child_from_string(window, str8_lit("split_x")));
        }
        cfg_node_equip_string(rad_state->cfg, keep_child->cfg, str8_lit("panels"));
        cfg_node_insert_child(rad_state->cfg, window, window->last, keep_child->cfg);
      }
      else
      {
        cfg_node_insert_child(rad_state->cfg, grandparent->cfg, parent_prev->cfg, keep_child->cfg);
        cfg_node_equip_stringf(rad_state->cfg, keep_child->cfg, "%f", pct_of_parent);
      }

      // rad: keep-child split-axis == grandparent split-axis? bubble keep-child up into grandparent's children
      if(grandparent != &cfg_nil_panel_node && grandparent->split_axis == keep_child->split_axis && keep_child->first != &cfg_nil_panel_node)
      {
        cfg_node_unhook(rad_state->cfg, grandparent->cfg, keep_child->cfg);
        CFG_Node *prev = parent_prev->cfg;
        for(CFG_PanelNode *child = keep_child->first, *next = &cfg_nil_panel_node; child != &cfg_nil_panel_node; child = next)
        {
          next = child->next;
          cfg_node_unhook(rad_state->cfg, keep_child->cfg, child->cfg);
          cfg_node_insert_child(rad_state->cfg, grandparent->cfg, prev, child->cfg);
          prev = child->cfg;
          F32 old_pct = child->pct_of_parent;
          F32 new_pct = old_pct * pct_of_parent;
          cfg_node_equip_stringf(rad_state->cfg, child->cfg, "%f", new_pct);
        }
        cfg_node_release(rad_state->cfg, keep_child->cfg);
      }

      // rad: reset focus, if needed
      if(panel_tree.focused == discard_child)
      {
        CFG_PanelNode *new_focused = cfg_panel_node_from_tree_cfg(panel_tree.root, keep_child->cfg);
        for(CFG_PanelNode *grandchild = new_focused; grandchild != &cfg_nil_panel_node; grandchild = grandchild->first)
        {
          new_focused = grandchild;
        }
        if(keep_child->first == &cfg_nil_panel_node || new_focused->cfg->parent != &cfg_nil_node)
        {
          rad_focus_panel(new_focused->cfg);
        }
      }
    }
    // rad: otherwise we can just remove this child.
    else
    {
      // rad: remove
      CFG_PanelNode *next = &cfg_nil_panel_node;
      F32 removed_size_pct = panel->pct_of_parent;
      B32 has_tool_sibling = rad_panel_has_tool_child__(parent);
      if(next == &cfg_nil_panel_node && rad_tool_kind_from_panel(panel->prev->cfg) == RAD_ToolKind_Null) { next = panel->prev; }
      if(next == &cfg_nil_panel_node && rad_tool_kind_from_panel(panel->next->cfg) == RAD_ToolKind_Null) { next = panel->next; }
      if(next == &cfg_nil_panel_node) { next = panel->prev; }
      if(next == &cfg_nil_panel_node) { next = panel->next; }
      cfg_node_unhook(rad_state->cfg, parent->cfg, panel->cfg);
      cfg_node_release(rad_state->cfg, panel->cfg);

      // rad: resize siblings to this node (rad: next to a tool window,
      // only the neighbor grows; the tool window keeps its size)
      if(has_tool_sibling && next != &cfg_nil_panel_node)
      {
        cfg_node_equip_stringf(rad_state->cfg, next->cfg, "%f", next->pct_of_parent + removed_size_pct);
      }
      else
      {
        CFG_PanelTree new_panel_tree = cfg_panel_tree_from_cfg(scratch.arena, window);
        CFG_PanelNode *new_parent = cfg_panel_node_from_tree_cfg(new_panel_tree.root, parent->cfg);
        for(CFG_PanelNode *child = new_parent->first; child != &cfg_nil_panel_node; child = child->next)
        {
          CFG_Node *cfg = child->cfg;
          F32 old_pct = child->pct_of_parent;
          F32 new_pct = old_pct / (1.f-removed_size_pct);
          cfg_node_equip_stringf(rad_state->cfg, cfg, "%f", new_pct);
        }
      }

      // rad: reset focus, if needed
      if(panel_tree.focused == panel)
      {
        CFG_PanelNode *new_focused = next;
        for(CFG_PanelNode *grandchild = new_focused; grandchild != &cfg_nil_panel_node; grandchild = grandchild->first)
        {
          new_focused = grandchild;
        }
        rad_focus_panel(new_focused->cfg);
      }
    }
  }
  scratch_end(scratch);
}

internal void
rad_window_tool_layout_ensure(CFG_Node *window)
{
  // NOTE(rad): keeps the tool window layout (see RAD_ToolKind): checks it
  // (cheap; every frame), puts the explorer on its side (setting), and
  // rebuilds it from any other layout (older user files, raddbg-style
  // panels): tool tabs come out of their panels, emptied panels close, the
  // rest becomes the editor area.
  Temp scratch = scratch_begin(0, 0);
  if(window == &cfg_nil_node)
  {
    scratch_end(scratch);
    return;
  }
  B32 explorer_right = (rad_state->settings.explorer_side == 1);
  CFG_PanelTree tree = cfg_panel_tree_from_cfg(scratch.arena, window);
  CFG_PanelNode *root = tree.root;

  //- rad: check
  B32 good = 0;
  if(root != &cfg_nil_panel_node && root->child_count == 2 && root->split_axis == Axis2_Y)
  {
    CFG_PanelNode *top = root->first;
    CFG_PanelNode *bottom = root->last;
    B32 bottom_good = (bottom->first == &cfg_nil_panel_node && bottom->tab_side == Side_Max && bottom->tabs.count != 0 &&
                       rad_tool_kind_from_panel(bottom->cfg) == RAD_ToolKind_Bottom);
    for(CFG_NodePtrNode *n = bottom->tabs.first; n != 0; n = n->next)
    {
      bottom_good = bottom_good && (str8_match(n->v->string, str8_lit("output"), 0) || str8_match(n->v->string, str8_lit("problems"), 0));
    }
    if(bottom_good && top->child_count >= 2 && top->split_axis == Axis2_X)
    {
      CFG_PanelNode *ends[] = {top->first, top->last};
      CFG_PanelNode *explorer = &cfg_nil_panel_node;
      for EachElement(idx, ends)
      {
        if(ends[idx]->first == &cfg_nil_panel_node && ends[idx]->tabs.count == 1 &&
           str8_match(ends[idx]->tabs.first->v->string, str8_lit("explorer"), 0))
        {
          explorer = ends[idx];
        }
      }

      // rad: tool tabs anywhere else -> rebuild
      U64 tool_tabs_elsewhere = 0;
      for(CFG_PanelNode *p = root; p != &cfg_nil_panel_node; p = cfg_panel_node_rec__depth_first_pre(root, p).next)
      {
        if(p != explorer && p != bottom)
        {
          for(CFG_NodePtrNode *n = p->tabs.first; n != 0; n = n->next)
          {
            tool_tabs_elsewhere += rad_tab_is_tool(n->v);
          }
        }
      }
      if(explorer != &cfg_nil_panel_node && tool_tabs_elsewhere == 0)
      {
        good = 1;

        // rad: explorer on the wrong side -> move it
        CFG_PanelNode *want = (explorer_right ? top->last : top->first);
        if(explorer != want)
        {
          cfg_node_unhook(rad_state->cfg, top->cfg, explorer->cfg);
          CFG_Node *prev = (explorer_right ? top->cfg->last : &cfg_nil_node);
          if(!explorer_right)
          {
            // rad: before the first child panel (other children are options)
            for(CFG_Node *c = top->cfg->first; c != &cfg_nil_node; c = c->next)
            {
              if(c == top->first->cfg) { break; }
              prev = c;
            }
          }
          cfg_node_insert_child(rad_state->cfg, top->cfg, prev, explorer->cfg);
        }
      }
    }
  }

  //- rad: rebuild
  if(!good)
  {
    //- rad: take the tool tabs out (the first of each kind; others go)
    CFG_Node *explorer_tab = &cfg_nil_node;
    CFG_Node *output_tab = &cfg_nil_node;
    CFG_Node *problems_tab = &cfg_nil_node;
    B32 output_was_visible = 0;
    CFG_NodePtrList tool_tabs = {0};
    for(CFG_PanelNode *p = root; p != &cfg_nil_panel_node; p = cfg_panel_node_rec__depth_first_pre(root, p).next)
    {
      for(CFG_NodePtrNode *n = p->tabs.first; n != 0; n = n->next)
      {
        if(rad_tab_is_tool(n->v))
        {
          cfg_node_ptr_list_push(scratch.arena, &tool_tabs, n->v);
          if(!str8_match(n->v->string, str8_lit("explorer"), 0) && p->pct_of_parent > 0.001f)
          {
            output_was_visible = 1;
          }
        }
      }
    }
    for(CFG_NodePtrNode *n = tool_tabs.first; n != 0; n = n->next)
    {
      CFG_Node *tab = n->v;
      CFG_Node **slot = (str8_match(tab->string, str8_lit("explorer"), 0) ? &explorer_tab :
                         str8_match(tab->string, str8_lit("output"), 0) ? &output_tab : &problems_tab);
      cfg_node_unhook(rad_state->cfg, tab->parent, tab);
      if(*slot == &cfg_nil_node)
      {
        *slot = tab;
      }
      else
      {
        cfg_node_release(rad_state->cfg, tab);
      }
    }

    //- rad: close the panels left empty
    for(U64 iter = 0; iter < 256; iter += 1)
    {
      CFG_PanelTree t = cfg_panel_tree_from_cfg(scratch.arena, window);
      CFG_PanelNode *empty = &cfg_nil_panel_node;
      for(CFG_PanelNode *p = t.root; p != &cfg_nil_panel_node; p = cfg_panel_node_rec__depth_first_pre(t.root, p).next)
      {
        if(p != t.root && p->first == &cfg_nil_panel_node && p->tabs.count == 0)
        {
          empty = p;
          break;
        }
      }
      if(empty == &cfg_nil_panel_node)
      {
        break;
      }
      rad_panel_close(window, empty->cfg);
    }

    //- rad: no panels at all -> one
    CFG_Node *old_root = cfg_node_child_from_string(window, str8_lit("panels"));
    if(old_root == &cfg_nil_node)
    {
      old_root = cfg_node_new(rad_state->cfg, window, str8_lit("panels"));
    }
    CFG_PanelTree old_tree = cfg_panel_tree_from_cfg(scratch.arena, window);
    B32 old_split_x = (cfg_node_child_from_string(window, str8_lit("split_x")) != &cfg_nil_node);
    B32 old_root_is_split = (old_tree.root->first != &cfg_nil_panel_node);
    cfg_node_unhook(rad_state->cfg, window, old_root);
    cfg_node_release(rad_state->cfg, cfg_node_child_from_string(window, str8_lit("split_x")));

    //- rad: new root: top (explorer + editor area), bottom
    CFG_Node *new_root = cfg_node_new(rad_state->cfg, window, str8_lit("panels"));
    CFG_Node *top = cfg_node_newf(rad_state->cfg, new_root, "%f", output_was_visible ? 0.75f : 1.f);
    CFG_Node *bottom = cfg_node_newf(rad_state->cfg, new_root, "%f", output_was_visible ? RAD_BOTTOM_TOOL_DEFAULT_PCT : 0.f);
    CFG_Node *explorer = cfg_node_alloc(rad_state->cfg);
    cfg_node_equip_stringf(rad_state->cfg, explorer, "%f", 0.2f);
    if(!explorer_right)
    {
      cfg_node_insert_child(rad_state->cfg, top, top->last, explorer);
    }
    if(old_split_x && old_root_is_split)
    {
      // rad: left/right panels -> side by side with the explorer
      for(CFG_PanelNode *child = old_tree.root->first; child != &cfg_nil_panel_node; child = child->next)
      {
        cfg_node_unhook(rad_state->cfg, old_root, child->cfg);
        cfg_node_equip_stringf(rad_state->cfg, child->cfg, "%f", child->pct_of_parent*0.8f);
        cfg_node_insert_child(rad_state->cfg, top, top->last, child->cfg);
      }
      cfg_node_release(rad_state->cfg, old_root);
    }
    else
    {
      // rad: one panel, or top/bottom panels -> one editor area node
      cfg_node_equip_stringf(rad_state->cfg, old_root, "%f", 0.8f);
      cfg_node_insert_child(rad_state->cfg, top, top->last, old_root);
    }
    if(explorer_right)
    {
      cfg_node_insert_child(rad_state->cfg, top, top->last, explorer);
    }

    //- rad: the tool tabs into their windows
    if(explorer_tab == &cfg_nil_node)
    {
      explorer_tab = cfg_node_alloc(rad_state->cfg);
      cfg_node_equip_string(rad_state->cfg, explorer_tab, str8_lit("explorer"));
    }
    cfg_node_insert_child(rad_state->cfg, explorer, explorer->last, explorer_tab);
    cfg_node_new(rad_state->cfg, bottom, str8_lit("tabs_on_bottom"));
    if(output_tab == &cfg_nil_node)
    {
      output_tab = cfg_node_alloc(rad_state->cfg);
      cfg_node_equip_string(rad_state->cfg, output_tab, str8_lit("output"));
    }
    if(problems_tab == &cfg_nil_node)
    {
      problems_tab = cfg_node_alloc(rad_state->cfg);
      cfg_node_equip_string(rad_state->cfg, problems_tab, str8_lit("problems"));
    }
    cfg_node_insert_child(rad_state->cfg, bottom, bottom->last, output_tab);
    cfg_node_insert_child(rad_state->cfg, bottom, bottom->last, problems_tab);
    if(cfg_node_child_from_string(output_tab, str8_lit("selected")) == &cfg_nil_node &&
       cfg_node_child_from_string(problems_tab, str8_lit("selected")) == &cfg_nil_node)
    {
      cfg_node_new(rad_state->cfg, output_tab, str8_lit("selected"));
    }
    if(cfg_node_child_from_string(explorer_tab, str8_lit("selected")) == &cfg_nil_node)
    {
      cfg_node_new(rad_state->cfg, explorer_tab, str8_lit("selected"));
    }
    if(!output_was_visible)
    {
      CFG_Node *sizes = cfg_node_child_from_string_or_alloc(rad_state->cfg, window, str8_lit("tool_sizes"));
      CFG_Node *size = cfg_node_child_from_string_or_alloc(rad_state->cfg, sizes, str8_lit("bottom"));
      if(size->first == &cfg_nil_node)
      {
        cfg_node_new_replacef(rad_state->cfg, size, "%f", RAD_BOTTOM_TOOL_DEFAULT_PCT);
      }
    }

    //- rad: a focused panel: keep it; none -> the editor panel
    CFG_PanelTree new_tree = cfg_panel_tree_from_cfg(scratch.arena, window);
    if(new_tree.focused == &cfg_nil_panel_node || new_tree.focused->first != &cfg_nil_panel_node)
    {
      CFG_Node *editor_panel = rad_editor_panel_from_window(window);
      if(editor_panel != &cfg_nil_node)
      {
        rad_focus_panel(editor_panel);
      }
    }
  }
  scratch_end(scratch);
}

////////////////////////////////
//~ rad: Drag/Drop (port of raddbg's rd_drag_*)

internal B32
rad_drag_is_active(void)
{
  return (rad_state->drag_drop_state == RAD_DragDropState_Dragging ||
          rad_state->drag_drop_state == RAD_DragDropState_Dropping);
}

internal void
rad_drag_begin_tab(CFG_Node *panel, CFG_Node *tab)
{
  if(!rad_drag_is_active() && !rad_state->drag_killed)
  {
    rad_state->drag_kind = RAD_DragKind_Tab;
    rad_state->drag_window = rad_window_cfg_from_cfg(panel)->id;
    rad_state->drag_panel = panel->id;
    rad_state->drag_tab = tab->id;
    rad_state->drag_drop_state = RAD_DragDropState_Dragging;
  }
}

internal void
rad_drag_begin_panel(CFG_Node *panel, CFG_Node *selected_tab)
{
  rad_drag_begin_tab(panel, selected_tab);
  if(rad_state->drag_kind == RAD_DragKind_Tab && rad_state->drag_panel == panel->id)
  {
    rad_state->drag_kind = RAD_DragKind_Panel;
  }
}

internal B32
rad_drag_is_dockable(void)
{
  // rad: a drag the panels' drop sites take (tab, tab bar, explorer file)
  B32 result = (rad_drag_is_active() &&
                ((rad_state->drag_kind == RAD_DragKind_Tab && cfg_node_from_id(rad_state->drag_tab) != &cfg_nil_node) ||
                 (rad_state->drag_kind == RAD_DragKind_Panel && cfg_node_from_id(rad_state->drag_panel) != &cfg_nil_node) ||
                 rad_state->drag_kind == RAD_DragKind_FilePath));
  return result;
}

internal void
rad_drag_begin_file(String8 path)
{
  if(!rad_drag_is_active() && !rad_state->drag_killed)
  {
    rad_state->drag_kind = RAD_DragKind_FilePath;
    rad_state->drag_path_size = Min(path.size, sizeof(rad_state->drag_path_buffer));
    MemoryCopy(rad_state->drag_path_buffer, path.str, rad_state->drag_path_size);
    rad_state->drag_drop_state = RAD_DragDropState_Dragging;
  }
}

internal B32
rad_drag_drop(void)
{
  B32 result = 0;
  if(rad_state->drag_drop_state == RAD_DragDropState_Dropping)
  {
    result = 1;
    rad_state->drag_drop_state = RAD_DragDropState_Null;
  }
  return result;
}

internal void
rad_drag_kill(void)
{
  rad_state->drag_drop_state = RAD_DragDropState_Null;
  rad_state->drag_killed = 1;
}

internal void
rad_drop_split(CFG_Node *panel_cfg, Dir2 dir)
{
  // rad: a drop on a split site: the dragged tab, or the dragged file, goes
  // into a new panel on `dir`'s side of `panel_cfg`
  if(rad_state->drag_kind == RAD_DragKind_FilePath)
  {
    rad_cmd(RAD_CmdKind_OpenFile, .dst_panel = panel_cfg->id, .dir2 = dir, .file_path = str8(rad_state->drag_path_buffer, rad_state->drag_path_size));
  }
  else
  {
    rad_cmd(RAD_CmdKind_MoveTabToNewPanel, .dst_panel = panel_cfg->id, .panel = rad_state->drag_panel, .tab = rad_state->drag_tab, .dir2 = dir,
            .all_tabs = (rad_state->drag_kind == RAD_DragKind_Panel));
  }
}

internal Rng2F32
rad_drop_site_rect(Rng2F32 panel_rect, F32 font_size, Dir2 dir)
{
  // NOTE(rad): raddbg's combined split + move-tab drop sites: a compass in
  // the panel's middle (Dir2_Invalid: the center)
  F32 dim_px = ceil_f32(font_size*7.f);
  dim_px = Min(dim_px, Min(dim_2f32(panel_rect).x, dim_2f32(panel_rect).y)/4.f);
  dim_px = Max(dim_px, ceil_f32(font_size*3.f));
  Vec2F32 half = v2f32(dim_px/2, dim_px/2);
  Vec2F32 c = center_2f32(panel_rect);
  Vec2F32 off = {0};
  switch(dir)
  {
    default:{}break;
    case Dir2_Up:   {off = v2f32(0, -dim_px);}break;
    case Dir2_Down: {off = v2f32(0, +dim_px);}break;
    case Dir2_Left: {off = v2f32(-dim_px, 0);}break;
    case Dir2_Right:{off = v2f32(+dim_px, 0);}break;
  }
  c = add_2f32(c, off);
  Rng2F32 result = r2f32(sub_2f32(c, half), add_2f32(c, half));
  return result;
}

////////////////////////////////
//~ rad: Tabs (not in raddbg: pinning, compact widths)

internal B32
rad_tab_is_pinned(CFG_Node *tab)
{
  B32 result = (cfg_node_child_from_string(tab, str8_lit("pinned")) != &cfg_nil_node);
  return result;
}

internal void
rad_tab_move_after_pinned(CFG_Node *tab)
{
  // NOTE(rad): pinned tabs come first (VS2022): the tab goes right after the
  // last pinned tab of its panel (pinned: the end of the pinned ones;
  // unpinned: the first of the others)
  CFG_Node *panel = tab->parent;
  cfg_node_unhook(rad_state->cfg, panel, tab);
  CFG_Node *prev = &cfg_nil_node;
  for(CFG_Node *child = panel->first; child != &cfg_nil_node; child = child->next)
  {
    if(rad_tab_is_pinned(child))
    {
      prev = child;
    }
  }
  cfg_node_insert_child(rad_state->cfg, panel, prev, tab);
}

internal F32
rad_tab_pad_px(void)
{
  return floor_f32(ui_top_font_size()*0.75f);
}

internal F32
rad_tab_button_width_px(void)
{
  return floor_f32(ui_top_font_size()*1.5f);
}

internal F32
rad_tab_end_pad_px(void)
{
  return floor_f32(ui_top_font_size()*0.3f);
}

internal F32
rad_tab_width_target_px(Arena *arena, CFG_Node *tab, B32 is_selected)
{
  // NOTE(rad): compact tabs (TODO_2 B2, Zed's): padding + name + the pin
  // and close buttons' room (kept while they are hidden: hovering a tab
  // does not move the others)
  DR_FStrList fstrs = rad_title_fstrs_from_tab_flags(arena, tab, RAD_TabTitleFlag_TabBar);
  B32 closable = !rad_tab_is_tool(tab);
  F32 result = (dr_dim_from_fstrs(ui_top_tab_size(), &fstrs).x + rad_tab_pad_px() +
                (closable ? (floor_f32(ui_top_font_size()*0.25f) + rad_tab_button_width_px()*2.f + rad_tab_end_pad_px()) : rad_tab_pad_px()));
  result = Min(ui_top_font_size()*20.f, result);
  return result;
}

internal CFG_Node *
rad_tab_from_window_kind(CFG_Node *window, String8 kind)
{
  Temp scratch = scratch_begin(0, 0);
  CFG_Node *result = &cfg_nil_node;
  CFG_PanelTree tree = cfg_panel_tree_from_cfg(scratch.arena, window);
  for(CFG_PanelNode *p = tree.root; p != &cfg_nil_panel_node && result == &cfg_nil_node; p = cfg_panel_node_rec__depth_first_pre(tree.root, p).next)
  {
    for(CFG_NodePtrNode *n = p->tabs.first; n != 0; n = n->next)
    {
      if(str8_match(n->v->string, kind, 0))
      {
        result = n->v;
        break;
      }
    }
  }
  scratch_end(scratch);
  return result;
}

internal CFG_Node *
rad_editor_panel_from_window(CFG_Node *window)
{
  // NOTE(rad): where files open: the last focused panel not showing a tool
  // tab, if it is still a leaf of this window; else the first such leaf
  Temp scratch = scratch_begin(0, 0);
  CFG_Node *result = &cfg_nil_node;
  CFG_Node *first_other = &cfg_nil_node;
  CFG_Node *last_editor = cfg_node_from_id(rad_state->last_editor_panel);
  CFG_PanelTree tree = cfg_panel_tree_from_cfg(scratch.arena, window);
  for(CFG_PanelNode *p = tree.root; p != &cfg_nil_panel_node; p = cfg_panel_node_rec__depth_first_pre(tree.root, p).next)
  {
    if(p->first == &cfg_nil_panel_node && !rad_tab_is_tool(p->selected_tab))
    {
      if(p->cfg == last_editor)
      {
        result = p->cfg;
      }
      if(first_other == &cfg_nil_node)
      {
        first_other = p->cfg;
      }
    }
  }
  if(result == &cfg_nil_node)
  {
    result = first_other;
  }
  scratch_end(scratch);
  return result;
}

internal void
rad_output_tab_reveal(CFG_Node *window)
{
  // NOTE(rad): show the bottom tool window, select its Output tab (the
  // keyboard stays where it is)
  rad_tool_set_visible(window, RAD_ToolKind_Bottom, 1);
  CFG_Node *output_tab = rad_tab_from_window_kind(window, str8_lit("output"));
  if(output_tab == &cfg_nil_node)
  {
    CFG_Node *problems_tab = rad_tab_from_window_kind(window, str8_lit("problems"));
    if(problems_tab != &cfg_nil_node)
    {
      output_tab = cfg_node_alloc(rad_state->cfg);
      cfg_node_equip_string(rad_state->cfg, output_tab, str8_lit("output"));
      cfg_node_insert_child(rad_state->cfg, problems_tab->parent, problems_tab->prev, output_tab);
    }
    else
    {
      CFG_Node *editor_panel = rad_editor_panel_from_window(window);
      if(editor_panel != &cfg_nil_node)
      {
        CFG_Node *bottom_panel = rad_panel_split(editor_panel, Dir2_Down, RAD_BOTTOM_TOOL_DEFAULT_PCT);
        output_tab = cfg_node_new(rad_state->cfg, bottom_panel, str8_lit("output"));
        cfg_node_new(rad_state->cfg, bottom_panel, str8_lit("problems"));
      }
    }
  }
  if(output_tab != &cfg_nil_node)
  {
    rad_focus_tab(output_tab);
  }
}

internal RAD_CmdKind
rad_cmd_kind_from_code_name(String8 code_name)
{
  RAD_CmdKind result = RAD_CmdKind_Null;
  if(code_name.size != 0)
  {
    for EachNonZeroEnumVal(RAD_CmdKind, k)
    {
      if(str8_match(rad_cmd_kind_info_table[k].code_name, code_name, 0))
      {
        result = k;
        break;
      }
    }
  }
  return result;
}

internal String8
rad_tab_query_input(CFG_Node *tab)
{
  // NOTE(rad): port of raddbg's rd_view_query_input
  CFG_Node *query = cfg_node_child_from_string(tab, str8_lit("query"));
  CFG_Node *input = cfg_node_child_from_string(query, str8_lit("input"));
  return input->first->string;
}

internal void
rad_tab_query_open(CFG_Node *tab, RAD_CmdKind kind)
{
  // NOTE(rad): port of raddbg's query setup for a tab-embedded query (the
  // non-floating, non-lister path): same cmd/input cfg, same open/toggle and
  // select-old-input rules
  Temp scratch = scratch_begin(0, 0);
  RAD_CmdKindInfo *cmd_kind_info = &rad_cmd_kind_info_table[kind];
  String8 cmd_name = cmd_kind_info->code_name;
  CFG_Node *query = cfg_node_child_from_string_or_alloc(rad_state->cfg, tab, str8_lit("query"));
  CFG_Node *cmd = cfg_node_child_from_string_or_alloc(rad_state->cfg, query, str8_lit("cmd"));
  CFG_Node *input = cfg_node_child_from_string_or_alloc(rad_state->cfg, query, str8_lit("input"));

  // rad: choose initial input string
  String8 initial_input = {0};
  if(cmd_kind_info->query.flags & RAD_QueryFlag_KeepOldInput)
  {
    initial_input = push_str8_copy(scratch.arena, input->first->string);
  }

  // rad: build query state
  String8 current_query_cmd_name = push_str8_copy(scratch.arena, cmd->first->string);
  cfg_node_new_replace(rad_state->cfg, input, initial_input);
  cfg_node_new_replace(rad_state->cfg, cmd, cmd_name);
  RAD_ViewState *vs = rad_view_state_from_cfg(tab);
  if(!vs->query_is_open && cmd_kind_info->query.flags & RAD_QueryFlag_SelectOldInput)
  {
    vs->query_cursor = initial_input.size;
    vs->query_mark = 0;
  }
  else
  {
    vs->query_cursor = initial_input.size;
    vs->query_mark = vs->query_cursor;
  }
  if(!str8_match(current_query_cmd_name, cmd_name, 0))
  {
    vs->query_is_open = 1;
  }
  else
  {
    vs->query_is_open ^= 1;
  }
  vs->contents_are_focused = 0;
  scratch_end(scratch);
}

internal void
rad_tab_query_complete(CFG_Node *tab)
{
  // NOTE(rad): port of raddbg's RD_CmdKind_CompleteQuery for a tab-embedded
  // query: run the query's command with the input in its slot, then close
  // the query unless the command keeps its input (search)
  CFG_Node *query = cfg_node_child_from_string(tab, str8_lit("query"));
  String8 cmd_name = cfg_node_child_from_string(query, str8_lit("cmd"))->first->string;
  String8 input = rad_tab_query_input(tab);
  RAD_CmdKind kind = rad_cmd_kind_from_code_name(cmd_name);
  RAD_CmdKindInfo *cmd_kind_info = &rad_cmd_kind_info_table[kind];
  if(kind != RAD_CmdKind_Null)
  {
    switch(cmd_kind_info->query.slot)
    {
      default:
      {
        rad_cmd(kind, .panel = tab->parent->id, .tab = tab->id);
      }break;
      case RAD_QuerySlot_LineNum:
      {
        S64 line_num = 0;
        if(try_s64_from_str8_c_rules(str8_skip_chop_whitespace(input), &line_num) && line_num > 0)
        {
          rad_cmd(kind, .panel = tab->parent->id, .tab = tab->id, .line_num = line_num);
        }
      }break;
      case RAD_QuerySlot_String:
      {
        rad_cmd(kind, .panel = tab->parent->id, .tab = tab->id, .string = input);
      }break;
    }
  }
  if(!(cmd_kind_info->query.flags & RAD_QueryFlag_KeepOldInput))
  {
    RAD_ViewState *vs = rad_view_state_from_cfg(tab);
    vs->query_is_open = 0;
    vs->query_string_size = 0;
  }
}

internal String8
rad_file_path_from_tab(CFG_Node *tab)
{
  String8 result = cfg_node_child_from_string(tab, str8_lit("path"))->first->string;
  return result;
}

internal String8List
rad_qualifiers_from_file_path(Arena *arena, String8 file_path)
{
  // NOTE(rad): port of raddbg's disambiguated file name: the folder names,
  // walking up from the file, at which the other open tabs' paths with the
  // same file name drop out ("src" for src/main.rs next to tests/main.rs)
  String8List qualifiers = {0};
  if(rad_state->ambiguous_path_slots_count != 0)
  {
    Temp scratch = scratch_begin(&arena, 1);
    String8 name = str8_skip_last_slash(file_path);
    U64 hash = u64_hash_from_str8(lower_from_str8(scratch.arena, name));
    U64 slot_idx = hash%rad_state->ambiguous_path_slots_count;
    RAD_AmbiguousPathNode *node = 0;
    for(RAD_AmbiguousPathNode *n = rad_state->ambiguous_path_slots[slot_idx]; n != 0; n = n->next)
    {
      if(str8_match(n->name, name, StringMatchFlag_CaseInsensitive))
      {
        node = n;
        break;
      }
    }
    if(node != 0 && node->paths.node_count > 1)
    {
      // rad: get all colliding paths, and their reversed path parts
      String8Array collisions = str8_array_from_list(scratch.arena, &node->paths);
      String8List *collision_parts_reversed = push_array(scratch.arena, String8List, collisions.count);
      for EachIndex(idx, collisions.count)
      {
        String8List parts = str8_split_path(scratch.arena, collisions.v[idx]);
        for(String8Node *n = parts.first; n != 0; n = n->next)
        {
          str8_list_push_front(scratch.arena, &collision_parts_reversed[idx], n->string);
        }
      }

      // rad: the search path's reversed parts
      String8List parts = str8_split_path(scratch.arena, file_path);
      String8List parts_reversed = {0};
      for(String8Node *n = parts.first; n != 0; n = n->next)
      {
        str8_list_push_front(scratch.arena, &parts_reversed, n->string);
      }

      // rad: walk all collisions in lock-step with the search path;
      // disqualify until one path remains; gather qualifiers
      U64 num_collisions_left = collisions.count;
      String8Node **collision_nodes = push_array(scratch.arena, String8Node *, collisions.count);
      for EachIndex(idx, collisions.count)
      {
        collision_nodes[idx] = collision_parts_reversed[idx].first;
      }
      for(String8Node *n = parts_reversed.first; num_collisions_left > 1 && n != 0; n = n->next)
      {
        B32 part_is_qualifier = 0;
        for EachIndex(idx, collisions.count)
        {
          if(collision_nodes[idx] != 0 && !str8_match(collision_nodes[idx]->string, n->string, StringMatchFlag_CaseInsensitive))
          {
            collision_nodes[idx] = 0;
            num_collisions_left -= 1;
            part_is_qualifier = 1;
          }
          else if(collision_nodes[idx] != 0)
          {
            collision_nodes[idx] = collision_nodes[idx]->next;
          }
        }
        if(part_is_qualifier)
        {
          str8_list_push_front(arena, &qualifiers, push_str8_copy(arena, n->string));
        }
      }
    }
    scratch_end(scratch);
  }
  return qualifiers;
}

internal DR_FStrList
rad_title_fstrs_from_tab(Arena *arena, CFG_Node *tab)
{
  return rad_title_fstrs_from_tab_flags(arena, tab, 0);
}

internal DR_FStrList
rad_title_fstrs_from_tab_flags(Arena *arena, CFG_Node *tab, RAD_TabTitleFlags flags)
{
  // NOTE(rad): same layout as raddbg's `rd_title_fstrs_from_cfg` for tabs:
  // [icon, secondary color] "  " [name, primary color]. The tab bar's
  // (TODO_2 B2): files without the icon, without the modified dot (the tab
  // draws it in its close button's place) and the trailing spaces
  DR_FStrList result = {0};
  Vec4F32 rgba = ui_color_from_name(str8_lit("text"));
  Vec4F32 rgba_secondary = rgba;
  UI_TagF("weak")
  {
    rgba_secondary = ui_color_from_name(str8_lit("text"));
  }
  RAD_IconKind icon_kind = RAD_IconKind_Null;
  String8 name = {0};
  String8 file_path = rad_file_path_from_tab(tab);
  if(file_path.size != 0)
  {
    icon_kind = RAD_IconKind_FileOutline;
    name = str8_skip_last_slash(file_path);
  }
  else if(str8_match(tab->string, str8_lit("getting_started"), 0))
  {
    icon_kind = RAD_IconKind_QuestionMark;
    name = str8_lit("Getting Started");
  }
  else if(str8_match(tab->string, str8_lit("explorer"), 0))
  {
    String8 folder = rad_explorer_folder_from_tab(tab);
    icon_kind = RAD_IconKind_FolderClosedFilled;
    name = str8_skip_last_slash(folder);
    if(name.size == 0)
    {
      name = (folder.size != 0 ? folder : str8_lit("Explorer"));
    }
  }
  else if(str8_match(tab->string, str8_lit("output"), 0))
  {
    icon_kind = RAD_IconKind_List;
    name = str8_lit("Output");
  }
  else if(str8_match(tab->string, str8_lit("problems"), 0))
  {
    icon_kind = RAD_IconKind_WarningSmall;
    name = str8_lit("Problems");
  }
  else if(str8_match(tab->string, str8_lit("settings"), 0))
  {
    icon_kind = RAD_IconKind_Gear;
    name = str8_lit("Settings");
  }
  else
  {
    name = tab->string;
  }
  DR_FStrParams params = {rad_font_from_slot(RAD_FontSlot_Main), rad_raster_flags_from_slot(RAD_FontSlot_Main), rgba, ui_top_font_size()};
  if(icon_kind != RAD_IconKind_Null && !(file_path.size != 0 && flags & RAD_TabTitleFlag_TabBar))
  {
    dr_fstrs_push_new(arena, &result, &params, rad_icon_kind_text_table[icon_kind], .font = rad_font_from_slot(RAD_FontSlot_Icons), .raster_flags = rad_raster_flags_from_slot(RAD_FontSlot_Icons), .color = rgba_secondary);
    dr_fstrs_push_new(arena, &result, &params, str8_lit("  "));
  }

  //- rad: same-named files open -> qualifiers ("<src> main.rs"), as raddbg
  if(file_path.size != 0)
  {
    Temp scratch = scratch_begin(&arena, 1);
    String8List qualifiers = rad_qualifiers_from_file_path(scratch.arena, file_path);
    if(qualifiers.node_count != 0) UI_TagF("weak")
    {
      for(String8Node *n = qualifiers.first; n != 0; n = n->next)
      {
        String8 string = push_str8f(arena, "<%S> ", n->string);
        dr_fstrs_push_new(arena, &result, &params, string, .color = ui_color_from_name(str8_lit("text")));
      }
    }
    scratch_end(scratch);
  }

  dr_fstrs_push_new(arena, &result, &params, name);
  if(str8_match(tab->string, str8_lit("problems"), 0) && rad_problems->count != 0)
  {
    dr_fstrs_push_new(arena, &result, &params, push_str8f(arena, " %I64u", rad_problems->count), .color = rgba_secondary);
  }
  if(rad_tab_is_dirty(tab) && !(flags & RAD_TabTitleFlag_TabBar))
  {
    dr_fstrs_push_new(arena, &result, &params, str8_lit(" "));
    dr_fstrs_push_new(arena, &result, &params, rad_icon_kind_text_table[RAD_IconKind_CircleFilled], .font = rad_font_from_slot(RAD_FontSlot_Icons), .raster_flags = rad_raster_flags_from_slot(RAD_FontSlot_Icons), .color = rgba_secondary, .size = ui_top_font_size()*0.6f);
  }
  if(!(flags & RAD_TabTitleFlag_TabBar))
  {
    dr_fstrs_push_new(arena, &result, &params, str8_lit("  "));
  }
  return result;
}

////////////////////////////////
//~ rad: Windows

internal String8
rad_push_window_title(Arena *arena)
{
  String8 result = push_str8f(arena, "%s", BUILD_TITLE " (" BUILD_VERSION_STRING_LITERAL " " BUILD_RELEASE_PHASE_STRING_LITERAL ")");
  return result;
}

internal RAD_Window *
rad_window_open(CFG_Node *window_cfg)
{
  Temp scratch = scratch_begin(0, 0);

  //- rad: allocate
  RAD_Window *ws = rad_state->free_window;
  if(ws != 0)
  {
    SLLStackPop(rad_state->free_window);
  }
  else
  {
    ws = push_array_no_zero(rad_state->arena, RAD_Window, 1);
  }
  MemoryZeroStruct(ws);
  DLLPushBack(rad_state->first_window, rad_state->last_window, ws);
  ws->arena = arena_alloc();
  ws->cfg_id = window_cfg->id;
  ws->ui = ui_state_alloc();
  ws->font_scale = 1.f;

  //- rad: unpack size (default: 4/5 of the primary monitor, same as raddbg)
  Vec2F32 size = {0};
  {
    CFG_Node *size_cfg = cfg_node_child_from_string(window_cfg, str8_lit("size"));
    size.x = (F32)f64_from_str8(size_cfg->first->string);
    size.y = (F32)f64_from_str8(size_cfg->first->next->string);
    if((size.x == 0 || size.y == 0) && rad_script_is_active())
    {
      // rad: scripted tests get the same window on every machine and display
      // (visible-line checks depend on it), small enough for common screens
      // (Windows clamps a window to the screen)
      size = v2f32(1920, 1080);
    }
    if(size.x == 0 || size.y == 0)
    {
      WM_Monitor monitor = wm_primary_monitor();
      Vec2F32 monitor_dim = wm_dim_from_monitor(monitor);
      size = v2f32(monitor_dim.x*4/5, monitor_dim.y*4/5);
    }
    if(size.x == 0 || size.y == 0)
    {
      size = v2f32(1280, 720);
    }
  }

  //- rad: unpack position + monitor (same as raddbg's window open)
  CFG_Node *pos_cfg = cfg_node_child_from_string(window_cfg, str8_lit("pos"));
  B32 has_pos = (pos_cfg != &cfg_nil_node);
  Vec2F32 pos = v2f32((F32)f64_from_str8(pos_cfg->first->string), (F32)f64_from_str8(pos_cfg->first->next->string));
  WM_Monitor preferred_monitor = {0};
  {
    CFG_Node *monitor_cfg = cfg_node_child_from_string(window_cfg, str8_lit("monitor"));
    WM_MonitorArray monitors = wm_push_monitors_array(scratch.arena);
    for EachIndex(idx, monitors.count)
    {
      String8 monitor_name = wm_name_from_monitor(scratch.arena, monitors.v[idx]);
      if(str8_match(monitor_name, monitor_cfg->first->string, StringMatchFlag_CaseInsensitive))
      {
        preferred_monitor = monitors.v[idx];
        break;
      }
    }
  }

  //- rad: create wm window, equip for rendering
  String8 title = rad_push_window_title(scratch.arena);
  WM_WindowFlags window_flags = (!has_pos*WM_WindowFlag_UseDefaultPosition)|WM_WindowFlag_CustomBorder;
  if(rad_script_is_active())
  {
    // rad: scripted tests never take the user's keyboard focus
    window_flags |= WM_WindowFlag_NoActivate;
  }
  ws->os = wm_window_open(r2f32p(pos.x, pos.y, pos.x+size.x, pos.y+size.y), window_flags, title);
  ws->r = r_window_equip(ws->os);
  ws->last_dpi = wm_dpi_from_window(ws->os);
  WM_Monitor zero_monitor = {0};
  if(!wm_monitor_match(zero_monitor, preferred_monitor))
  {
    wm_window_set_monitor(ws->os, preferred_monitor);
  }
  if(!rad_script_is_active() && cfg_node_child_from_string(window_cfg, str8_lit("fullscreen")) != &cfg_nil_node)
  {
    wm_window_set_fullscreen(ws->os, 1);
  }
  if(!rad_script_is_active() && cfg_node_child_from_string(window_cfg, str8_lit("maximized")) != &cfg_nil_node)
  {
    wm_window_set_maximized(ws->os, 1);
  }

  scratch_end(scratch);
  return ws;
}

internal RAD_Window *
rad_window_from_os(WM_Window os)
{
  RAD_Window *result = 0;
  for(RAD_Window *w = rad_state->first_window; w != 0; w = w->next)
  {
    if(wm_window_match(w->os, os))
    {
      result = w;
      break;
    }
  }
  return result;
}

internal RAD_Window *
rad_window_from_cfg(CFG_Node *window_cfg)
{
  RAD_Window *result = 0;
  for(RAD_Window *w = rad_state->first_window; w != 0; w = w->next)
  {
    if(w->cfg_id == window_cfg->id)
    {
      result = w;
      break;
    }
  }
  return result;
}

////////////////////////////////
//~ rad: Views

internal void
rad_view_ui(CFG_Node *tab, Rng2F32 rect)
{
  // NOTE(rad): port of raddbg's rd_view_ui: query bar, then the view
  RAD_ViewState *vs = rad_view_state_from_cfg(tab);

  //////////////////////////////
  //- rad: query extension
  //
  CFG_Node *query_root = cfg_node_child_from_string(tab, str8_lit("query"));
  CFG_Node *input_root = cfg_node_child_from_string(query_root, str8_lit("input"));
  CFG_Node *cmd_root = cfg_node_child_from_string(query_root, str8_lit("cmd"));
  String8 current_input = input_root->first->string;
  B32 search_row_is_open = (vs->query_is_open);
  B32 query_has_focus = (vs->query_is_open && !vs->contents_are_focused);
  F32 search_row_open_t = ui_anim(ui_key_from_stringf(ui_key_zero(), "search_row_open_%p", tab),
                                  (F32)!!search_row_is_open,
                                  .initial = (F32)!!search_row_is_open,
                                  .epsilon = 0.01f,
                                  .rate    = rad_state->menu_animation_rate);
  if(search_row_open_t > 0.001f)
  {
    String8 cmd_name = cmd_root->first->string;
    RAD_CmdKindInfo *cmd_kind_info = &rad_cmd_kind_info_table[rad_cmd_kind_from_code_name(cmd_name)];
    RAD_IconKind icon = cmd_kind_info->icon_kind;

    //- rad: store cfg's string into view's
    vs->query_string_size = Min(sizeof(vs->query_buffer), current_input.size);
    MemoryCopy(vs->query_buffer, current_input.str, vs->query_string_size);

    //- rad: determine dimensions
    F32 search_row_height_target = ui_top_px_height();
    F32 search_row_height = search_row_open_t*search_row_height_target;
    search_row_height = Min(search_row_height, dim_2f32(rect).y);
    rect.y0 += search_row_height;
    rect.y0 = floor_f32(rect.y0);

    //- rad: build container
    UI_Box *search_row = &ui_nil_box;
    UI_PrefHeight(ui_px(search_row_height, 1.f))
    {
      if(!vs->contents_are_focused)
      {
        ui_set_next_border_color(ui_color_from_name(str8_lit("border")));
      }
      search_row = ui_build_box_from_stringf(UI_BoxFlag_DrawBorder|UI_BoxFlag_DrawDropShadow, "###search");
    }

    //- rad: build contents
    UI_Parent(search_row) UI_WidthFill UI_HeightFill UI_Focus(query_has_focus ? UI_FocusKind_On : UI_FocusKind_Off)
      RAD_Font(cmd_kind_info->query.flags & RAD_QueryFlag_CodeInput ? RAD_FontSlot_Code : RAD_FontSlot_Main)
      UI_Flags(UI_BoxFlag_DisableFocusOverlay|UI_BoxFlag_DisableFocusBorder)
    {
      UI_TagF("weak")
      {
        if(cmd_name.size != 0)
        {
          ui_spacer(ui_em(0.5f, 1.f));
          UI_TextAlignment(UI_TextAlign_Center)
            UI_Transparency(1-search_row_open_t)
            UI_PrefWidth(ui_em(3.f, 1.f))
            RAD_Font(RAD_FontSlot_Icons)
            ui_label(rad_icon_kind_text_table[icon == RAD_IconKind_Null ? RAD_IconKind_Find : icon]);
          UI_Transparency(1-search_row_open_t)
            RAD_Font(RAD_FontSlot_Main) UI_PrefWidth(ui_text_dim(1, 1))
            UI_FontSize(ui_top_font_size()*0.85f)
            ui_label(cmd_kind_info->display_name);
        }
        else
        {
          ui_spacer(ui_em(0.5f, 1.f));
          UI_TextAlignment(UI_TextAlign_Center) RAD_Font(RAD_FontSlot_Icons) UI_PrefWidth(ui_em(2.f, 1.f))
            ui_label(rad_icon_kind_text_table[RAD_IconKind_Find]);
        }
      }
      RAD_CellParams params = {0};
      {
        params.flags |= !!(cmd_kind_info->query.flags & RAD_QueryFlag_CodeInput) * RAD_CellFlag_CodeContents;
        params.flags |= RAD_CellFlag_NoBackground;
        params.cursor               = &vs->query_cursor;
        params.mark                 = &vs->query_mark;
        params.edit_buffer          = vs->query_buffer;
        params.edit_string_size_out = &vs->query_string_size;
        params.edit_buffer_size     = sizeof(vs->query_buffer);
        params.pre_edit_value       = current_input;
      }
      UI_Transparency(1-search_row_open_t)
      {
        UI_Signal sig = rad_cellf(&params, "###search");
        if(ui_pressed(sig))
        {
          vs->query_is_open = 1;
          vs->contents_are_focused = 0;
          rad_cmd(RAD_CmdKind_FocusPanel, .panel = tab->parent->id);
        }
      }

      // NOTE(rad): not in raddbg (its code view is read-only): Enter also
      // produces "\n" text; while the query has the keyboard, it must not
      // reach the view contents (an editor)
      if(query_has_focus && ui_is_focus_active())
      {
        for(UI_Event *evt = 0; ui_next_event(&evt);)
        {
          if(evt->kind == UI_EventKind_Text && !(evt->flags & UI_EventFlag_Paste) &&
             str8_find_needle(evt->string, 0, str8_lit("\n"), 0) < evt->string.size)
          {
            ui_eat_event(evt);
          }
        }
      }
    }

    //- rad: commit string to view
    if(input_root == &cfg_nil_node)
    {
      query_root = cfg_node_child_from_string_or_alloc(rad_state->cfg, tab, str8_lit("query"));
      input_root = cfg_node_child_from_string_or_alloc(rad_state->cfg, query_root, str8_lit("input"));
    }
    cfg_node_new_replace(rad_state->cfg, input_root, str8(vs->query_buffer, vs->query_string_size));
  }

  //////////////////////////////
  //- rad: query open, click into the view's contents -> they get the
  // keyboard (raddbg does this for watch views; rad's code view is an editor)
  //
  if(vs->query_is_open)
  {
    for(UI_Event *evt = 0; ui_next_event(&evt);)
    {
      if(evt->kind == UI_EventKind_Press && evt->key == WM_Key_LeftMouseButton && contains_2f32(rect, evt->pos))
      {
        vs->contents_are_focused = 1;
      }
    }
  }

  //////////////////////////////
  //- rad: build main view container
  //
  UI_Box *view_container = &ui_nil_box;
  UI_WidthFill UI_HeightFill
  {
    view_container = ui_build_box_from_key(0, ui_key_zero());
  }

  //////////////////////////////
  //- rad: fill view container
  //
  UI_Parent(view_container)
    UI_PrefHeight(ui_px(floor_f32(ui_top_font_size()*rad_state->settings.row_height), 1.f))
  {
    //////////////////////////////
    //- rad: special-case view: "getting started"
    //
    // NOTE(rad): same layout as the RAD Debugger's "getting_started" view
    // (raddbg_core.c, rd_view_ui), with the target helper replaced by a file one.
    //
    if(str8_match(tab->string, str8_lit("getting_started"), 0))
    {
      ui_set_next_flags(UI_BoxFlag_DefaultFocusNav);
      UI_Focus(UI_FocusKind_On) UI_WidthFill UI_HeightFill UI_NamedColumn(str8_lit("empty_view"))
        UI_Padding(ui_pct(1, 0)) UI_Focus(UI_FocusKind_Null)
      {
        Rng2F32 view_rect = ui_top_parent()->rect;

        //- rad: icon & info
        UI_Padding(ui_em(2.f, 1.f))
        {
          //- rad: icon
          {
            F32 icon_dim = ui_top_font_size()*10.f;
            ui_set_next_flags(UI_BoxFlag_AnimatePosY);
            UI_PrefHeight(ui_px(icon_dim, 1.f))
              UI_Row
              UI_Padding(ui_pct(1, 0))
              UI_PrefWidth(ui_px(icon_dim, 1.f))
            {
              R_Handle texture = rad_state->icon_texture;
              Vec2S32 texture_dim = r_size_from_tex2d(texture);
              ui_image(texture, R_Tex2DSampleKind_Linear, r2f32p(0, 0, texture_dim.x, texture_dim.y), v4f32(1, 1, 1, 1), 0, str8_lit(""));
            }
          }

          //- rad: title
          ui_spacer(ui_em(2.f, 1.f));
          UI_WidthFill UI_PrefHeight(ui_em(2.f, 1.f))
            UI_Row
            UI_Padding(ui_pct(1, 0))
            UI_TextAlignment(UI_TextAlign_Center)
            UI_PrefWidth(ui_text_dim(1, 1))
            UI_FontSize(ui_top_font_size()*2.f)
          {
            ui_label(str8_lit(BUILD_TITLE));
          }

          //- rad: info
          ui_spacer(ui_em(1.f, 1.f));
          UI_WidthFill UI_PrefHeight(ui_em(2.f, 1.f))
            UI_Row
            UI_Padding(ui_pct(1, 0))
            UI_TextAlignment(UI_TextAlign_Center)
            UI_PrefWidth(ui_text_dim(1, 1))
            UI_TagF("weak")
          {
            ui_label(str8_lit(BUILD_VERSION_STRING_LITERAL " " BUILD_RELEASE_PHASE_STRING_LITERAL));
  #if defined(BUILD_GIT_HASH)
            ui_label(str8_lit(BUILD_GIT_HASH));
  #endif
          }
        }

        //- rad: drop-file helper (same box as raddbg's "add target" helper)
        ui_spacer(ui_em(2.f, 1.f));
        UI_PrefHeight(ui_em(30.f, 1.f)) UI_Row UI_Padding(ui_pct(1, 0)) UI_PrefWidth(ui_em(40.f, 1.f))
          UI_TextAlignment(UI_TextAlign_Center)
          UI_CornerRadius(ui_top_font_size()/2.f)
        {
          ui_set_next_child_layout_axis(Axis2_Y);
          UI_Box *box = ui_build_box_from_stringf(UI_BoxFlag_DrawBorder, "###open_file_drop_site");
          UI_Parent(box) UI_Padding(ui_pct(1, 0)) UI_PrefHeight(ui_em(2.5f, 1.f))
          {
            UI_PrefHeight(ui_em(3.75f, 1.f))
              UI_Row
              UI_Padding(ui_pct(1, 0))
              UI_TextAlignment(UI_TextAlign_Center)
              UI_PrefWidth(ui_em(12.f, 1.f))
              UI_CornerRadius(ui_top_font_size()/2.f)
              UI_TagF("pop")
              if(ui_clicked(rad_icon_buttonf(RAD_IconKind_FileOutline, 0, "Open File")))
            {
              rad_cmd(RAD_CmdKind_OpenFile);
            }
            ui_spacer(ui_em(1.f, 1.f));
            UI_TagF("weak")
            {
              ui_label(str8_lit("or drop a file"));
            }
          }
          for(UI_Event *evt = 0; ui_next_event(&evt);)
          {
            if(evt->kind == UI_EventKind_FileDrop &&
               contains_2f32(view_rect, evt->pos) &&
               evt->paths.node_count != 0)
            {
              for(String8Node *n = evt->paths.first; n != 0; n = n->next)
              {
                rad_cmd(RAD_CmdKind_OpenFile, .file_path = n->string);
              }
              ui_eat_event(evt);
            }
          }
        }
        ui_spacer(ui_em(4.f, 1.f));
      }
    }

    //////////////////////////////
    //- rad: text view
    //
    else if(str8_match(tab->string, str8_lit("text"), 0)) UI_FontSize(rad_code_font_size(rad_state->current_window))
    {
      rad_text_view_ui(tab, rect);
    }

    //////////////////////////////
    //- rad: explorer view
    //
    else if(str8_match(tab->string, str8_lit("explorer"), 0))
    {
      rad_explorer_view_ui(tab, rect);
    }

    //////////////////////////////
    //- rad: cargo output, problems
    //
    else if(str8_match(tab->string, str8_lit("output"), 0)) UI_FontSize(rad_panel_font_size(rad_state->current_window))
    {
      rad_output_view_ui(tab, rect);
    }
    else if(str8_match(tab->string, str8_lit("problems"), 0)) UI_FontSize(rad_panel_font_size(rad_state->current_window))
    {
      rad_problems_view_ui(tab, rect);
    }

    //////////////////////////////
    //- rad: settings
    //
    else if(str8_match(tab->string, str8_lit("settings"), 0))
    {
      rad_settings_view_ui(tab, rect);
    }
  }

  //////////////////////////////
  //- rad: catchall completion controls (Esc closes the query; Enter runs
  // its command, unless the view's contents have the keyboard)
  //
  if(vs->query_is_open) UI_Focus(UI_FocusKind_On)
  {
    if(ui_is_focus_active() && ui_slot_press(UI_EventActionSlot_Cancel))
    {
      vs->query_is_open = 0;
      vs->query_string_size = 0;
    }
    if(!vs->contents_are_focused && ui_is_focus_active() && ui_slot_press(UI_EventActionSlot_Accept))
    {
      rad_tab_query_complete(tab);
    }
  }

  vs->last_frame_index_built = rad_state->frame_index;
}

////////////////////////////////
//~ rad: Window Frame

#if COMPILER_MSVC && !BUILD_DEBUG
NO_OPTIMIZE_BEGIN
#endif

internal void
rad_window_frame(RAD_Window *ws)
{
  Temp scratch = scratch_begin(0, 0);
  ProfBeginFunction();

  //////////////////////////////
  //- rad: unpack context
  //
  RAD_Settings *settings  = &rad_state->settings;
  CFG_Node *window        = cfg_node_from_id(ws->cfg_id);
  CFG_PanelTree panel_tree= cfg_panel_tree_from_cfg(scratch.arena, window);
  B32 window_is_focused   = wm_window_is_focused(ws->os);
  if(!window_is_focused)
  {
    ws->menu_bar_key_held = 0;
    ws->menu_bar_focus_press_started = 0;
  }
  ui_select_state(ws->ui);

  //////////////////////////////
  //- rad: compute window's theme
  //
  // NOTE(rad): same as raddbg's "@window_frame_part compute window's theme",
  // minus user/project color overrides (not supported yet). colors are parsed
  // from the same preset metadesk trees, then converted sRGB -> linear.
  //
  {
    MD_Node *theme_tree = rad_state->theme_preset_trees[settings->theme_preset];
    typedef struct ThemePatternNode ThemePatternNode;
    struct ThemePatternNode
    {
      ThemePatternNode *next;
      UI_ThemePattern pattern;
    };
    ThemePatternNode *first_pattern = 0;
    ThemePatternNode *last_pattern = 0;
    U64 pattern_count = 0;
    for(MD_Node *n = theme_tree; !md_node_is_nil(n); n = md_node_rec_depth_first_pre(n, theme_tree).next)
    {
      if(str8_match(n->string, str8_lit("theme_color"), 0))
      {
        MD_Node *tags_child = md_child_from_string(n, str8_lit("tags"), 0);
        MD_Node *value_child = md_child_from_string(n, str8_lit("value"), 0);
        U8 split_char = ' ';
        String8List tags = str8_split(scratch.arena, tags_child->first->string, &split_char, 1, 0);
        U64 color_u64 = 0;
        try_u64_from_str8_c_rules(value_child->first->string, &color_u64);
        Vec4F32 color_linear = linear_from_srgba(rgba_from_u32((U32)color_u64));
        ThemePatternNode *node = push_array(scratch.arena, ThemePatternNode, 1);
        node->pattern.tags = str8_array_from_list(rad_frame_arena(), &tags);
        node->pattern.linear = color_linear;
        SLLQueuePush(first_pattern, last_pattern, node);
        pattern_count += 1;
      }
    }
    ws->theme = push_array(rad_frame_arena(), UI_Theme, 1);
    ws->theme->patterns_count = pattern_count;
    ws->theme->patterns = push_array(rad_frame_arena(), UI_ThemePattern, ws->theme->patterns_count);
    {
      U64 idx = 0;
      for(ThemePatternNode *n = first_pattern; n != 0; n = n->next, idx += 1)
      {
        ws->theme->patterns[idx] = n->pattern;
      }
    }
  }

  //////////////////////////////
  //- rad: compute window's font raster flags
  //
  {
    ws->font_slot_raster_flags[RAD_FontSlot_Icons] = FNT_RasterFlag_Smooth;
    ws->font_slot_raster_flags[RAD_FontSlot_Main] = (settings->smooth_ui_text*FNT_RasterFlag_Smooth)|(settings->hint_ui_text*FNT_RasterFlag_Hinted);
    ws->font_slot_raster_flags[RAD_FontSlot_Code] = (settings->smooth_code_text*FNT_RasterFlag_Smooth)|(settings->hint_code_text*FNT_RasterFlag_Hinted);
    ws->font_slot_raster_flags[RAD_FontSlot_Panel] = ws->font_slot_raster_flags[RAD_FontSlot_Code];
  }

  //////////////////////////////
  //- rad: DPI changes -> xform font size (same as raddbg)
  //
  {
    // NOTE(rad): raddbg scales the window's font size; rad keeps the scale
    // (the sizes are settings: UI and code)
    F32 dpi = wm_dpi_from_window(ws->os);
    if(dpi != ws->last_dpi)
    {
      fnt_reset();
      ws->font_scale = ws->font_scale * (dpi / ws->last_dpi);
      ws->last_dpi = dpi;
    }
  }

  //////////////////////////////
  //- rad: build UI
  //
  ProfScope("build UI")
  {
    ////////////////////////////
    //- rad: set up
    //
    {
      // rad: get top-level font size info
      F32 top_level_font_size = Clamp(6.f, floor_f32((F32)settings->font_size*ws->font_scale), 72.f);

      // rad: build icon info
      UI_IconInfo icon_info = {0};
      {
        icon_info.icon_font = rad_font_from_slot(RAD_FontSlot_Icons);
        icon_info.icon_kind_text_map[UI_IconKind_RightArrow]     = rad_icon_kind_text_table[RAD_IconKind_RightScroll];
        icon_info.icon_kind_text_map[UI_IconKind_DownArrow]      = rad_icon_kind_text_table[RAD_IconKind_DownScroll];
        icon_info.icon_kind_text_map[UI_IconKind_LeftArrow]      = rad_icon_kind_text_table[RAD_IconKind_LeftScroll];
        icon_info.icon_kind_text_map[UI_IconKind_UpArrow]        = rad_icon_kind_text_table[RAD_IconKind_UpScroll];
        icon_info.icon_kind_text_map[UI_IconKind_RightCaret]     = rad_icon_kind_text_table[RAD_IconKind_RightCaret];
        icon_info.icon_kind_text_map[UI_IconKind_DownCaret]      = rad_icon_kind_text_table[RAD_IconKind_DownCaret];
        icon_info.icon_kind_text_map[UI_IconKind_LeftCaret]      = rad_icon_kind_text_table[RAD_IconKind_LeftCaret];
        icon_info.icon_kind_text_map[UI_IconKind_UpCaret]        = rad_icon_kind_text_table[RAD_IconKind_UpCaret];
        icon_info.icon_kind_text_map[UI_IconKind_CheckHollow]    = rad_icon_kind_text_table[RAD_IconKind_CheckHollow];
        icon_info.icon_kind_text_map[UI_IconKind_CheckFilled]    = rad_icon_kind_text_table[RAD_IconKind_CheckFilled];
      }

      // rad: build animation info
      UI_AnimationInfo animation_info = {0};
      {
        animation_info.hot_animation_rate      = rad_state->catchall_animation_rate;
        animation_info.active_animation_rate   = rad_state->catchall_animation_rate;
        animation_info.focus_animation_rate    = 1.f;
        animation_info.tooltip_animation_rate  = rad_state->tooltip_animation_rate;
        animation_info.menu_animation_rate     = rad_state->menu_animation_rate;
        animation_info.scroll_animation_rate   = rad_state->scrolling_animation_rate;
      }

      // rad: begin & push initial stack values
      ui_begin_build(ws->os, &ws->ui_events, &icon_info, ws->theme, &animation_info, rad_state->frame_dt, rad_state->frame_dt);
      ui_push_font(rad_font_from_slot(RAD_FontSlot_Main));
      ui_push_font_size(top_level_font_size);
      ui_push_text_padding(floor_f32(ui_top_font_size()*0.3f));
      ui_push_pref_width(ui_px(floor_f32(ui_top_font_size()*20.f), 1.f));
      ui_push_pref_height(ui_px(floor_f32(ui_top_font_size()*3.f), 1.f));
      ui_push_blur_size(10.f);
      FNT_RasterFlags text_raster_flags = 0;
      if(settings->smooth_ui_text) {text_raster_flags |= FNT_RasterFlag_Smooth;}
      if(settings->hint_ui_text) {text_raster_flags |= FNT_RasterFlag_Hinted;}
      ui_push_text_raster_flags(text_raster_flags);
    }

    ////////////////////////////
    //- rad: calculate code color slot RGBAs
    //
    for EachEnumVal(RAD_CodeColorSlot, s)
    {
      ws->theme_code_colors[s] = ui_color_from_name(rad_code_color_slot_name_table[s]);
    }
    {
      // rad: semantic-token slots a theme has no color for -> a related one
      struct {RAD_CodeColorSlot slot; RAD_CodeColorSlot fallback;} fallbacks[] =
      {
        {RAD_CodeColorSlot_CodeTypeParameter, RAD_CodeColorSlot_CodeType},
        {RAD_CodeColorSlot_CodeLifetime,      RAD_CodeColorSlot_CodeKeyword},
        {RAD_CodeColorSlot_CodeProperty,      RAD_CodeColorSlot_CodeLocal},
        {RAD_CodeColorSlot_CodeUnsafe,        RAD_CodeColorSlot_CodeKeyword},
      };
      for EachElement(idx, fallbacks)
      {
        if(ws->theme_code_colors[fallbacks[idx].slot].w == 0)
        {
          ws->theme_code_colors[fallbacks[idx].slot] = ws->theme_code_colors[fallbacks[idx].fallback];
        }
      }
    }

    ////////////////////////////
    //- rad: calculate top-level rectangles/sizes
    //
    F32 window_edge_px = ui_top_font_size()*0.2f;
    Rng2F32 window_rect = wm_client_rect_from_window(ws->os);
    Vec2F32 window_rect_dim = dim_2f32(window_rect);
    F32 top_bar_dim_px = floor_f32(ui_top_font_size()*3.f);
    Rng2F32 top_bar_rect = r2f32p(window_rect.x0, window_rect.y0, window_rect.x0+window_rect_dim.x, window_rect.y0+top_bar_dim_px);
    // rad: the status bar is lower than raddbg's (the top bar's height), with
    // smaller text, like Zed's (TODO_2 B3)
    F32 bottom_bar_dim_px = floor_f32(ui_top_font_size()*2.f);
    Rng2F32 bottom_bar_rect = r2f32p(window_rect.x0, window_rect_dim.y - bottom_bar_dim_px, window_rect.x0+window_rect_dim.x, window_rect.y0+window_rect_dim.y);
    Rng2F32 content_rect = r2f32p(window_rect.x0, top_bar_rect.y1 + window_edge_px*1.5f, window_rect.x0+window_rect_dim.x, bottom_bar_rect.y0);
    content_rect = pad_2f32(content_rect, -window_edge_px);

    ////////////////////////////
    //- rad: truncated string hover
    //
    if(ui_string_hover_active()) UI_Tooltip
    {
      Temp scratch = scratch_begin(0, 0);
      DR_FStrList fstrs = ui_string_hover_fstrs(scratch.arena);
      UI_Box *box = ui_build_box_from_key(UI_BoxFlag_DrawText, ui_key_zero());
      ui_box_equip_display_fstrs(box, &fstrs);
      scratch_end(scratch);
    }

    ////////////////////////////
    //- rad: popup (port of raddbg's @window_ui_part popup; its PopupAccept /
    // PopupCancel commands are done here)
    //
    {
      if(rad_state->popup_t > 0.005f) UI_TextAlignment(UI_TextAlign_Center) UI_Focus(rad_state->popup_active ? UI_FocusKind_Root : UI_FocusKind_Off)
      {
        UI_Box *bg_box = &ui_nil_box;
        Vec4F32 shadow_color = ui_color_from_name(str8_lit("drop_shadow"));
        shadow_color.w += (1.f - shadow_color.w) * 0.5f;
        UI_Rect(window_rect)
          UI_ChildLayoutAxis(Axis2_X)
          UI_Focus(UI_FocusKind_On)
          UI_Transparency(1-rad_state->popup_t)
          UI_TagF("floating")
          UI_BackgroundColor(shadow_color)
        {
          bg_box = ui_build_box_from_stringf(UI_BoxFlag_FixedSize|
                                             UI_BoxFlag_Floating|
                                             UI_BoxFlag_Clickable|
                                             UI_BoxFlag_Scroll|
                                             UI_BoxFlag_DefaultFocusNav|
                                             UI_BoxFlag_DisableFocusOverlay|
                                             UI_BoxFlag_DisableFocusBorder|
                                             UI_BoxFlag_DrawBackground, "###popup_%p", ws);
        }
        if(rad_state->popup_active) UI_Parent(bg_box) UI_Transparency(1-rad_state->popup_t)
        {
          ui_ctx_menu_close();
          UI_WidthFill UI_PrefHeight(ui_children_sum(1.f)) UI_Column UI_Padding(ui_pct(1, 0)) UI_TagF("floating")
          {
            ui_set_next_blur_size(10*rad_state->popup_t);
            ui_set_next_pref_width(ui_children_sum(1));
            ui_set_next_pref_height(ui_children_sum(1));
            ui_set_next_child_layout_axis(Axis2_Y);
            UI_Box *panel = ui_build_box_from_stringf(UI_BoxFlag_DrawBackground|UI_BoxFlag_DrawBackgroundBlur|UI_BoxFlag_DrawBorder|UI_BoxFlag_DrawDropShadow, "");
            UI_Parent(panel)
            {
              ui_spacer(ui_em(1.5f, 1.f));
              UI_TextRasterFlags(rad_raster_flags_from_slot(RAD_FontSlot_Main)) UI_FontSize(ui_top_font_size()*2.f) UI_PrefHeight(ui_em(3.f, 1.f)) ui_label(rad_state->popup_title);
              UI_PrefHeight(ui_em(3.f, 1.f)) UI_TagF("weak") ui_label(rad_state->popup_desc);
              ui_spacer(ui_em(1.5f, 1.f));
              UI_Row UI_Padding(ui_pct(1.f, 0.f)) UI_PrefWidth(ui_em(16.f, 1.f)) UI_PrefHeight(ui_em(3.5f, 1.f)) UI_CornerRadius(ui_top_font_size()*0.5f)
              {
                UI_TagF("pop")
                  if(ui_clicked(ui_buttonf("OK")) || (ui_key_match(bg_box->default_nav_focus_hot_key, ui_key_zero()) && ui_slot_press(UI_EventActionSlot_Accept)))
                {
                  rad_state->popup_active = 0;
                  rad_cmd(rad_state->popup_cmd_kind, .file_path = rad_state->popup_cmd_path, .string = str8_lit("confirmed"));
                }
                ui_spacer(ui_em(1.f, 1.f));
                if(ui_clicked(ui_buttonf("Cancel")) || ui_slot_press(UI_EventActionSlot_Cancel))
                {
                  rad_state->popup_active = 0;
                }
              }
              ui_spacer(ui_em(3.f, 1.f));
            }
          }
        }
        ui_signal_from_box(bg_box);
      }
    }

    ////////////////////////////
    //- rad: floating views (raddbg: built here, before the bars and panels,
    // so they are on top and take their input first): the completion list
    //
    ProfScope("build floating views")
      UI_Focus(ui_any_ctx_menu_is_open() || ws->menu_bar_focused ? UI_FocusKind_Off : UI_FocusKind_Null)
    {
      rad_quick_open_ui(ws, window, content_rect);
      rad_completion_popup_ui(ws, window);
      rad_signature_popup_ui(ws, window);
    }

    ////////////////////////////
    //- rad: top bar
    //
    ProfScope("build top bar")
    {
      wm_window_clear_custom_border_data(ws->os);
      wm_window_push_custom_edges(ws->os, window_edge_px);
      wm_window_push_custom_title_bar(ws->os, dim_2f32(top_bar_rect).y);
      ui_set_next_flags(UI_BoxFlag_DefaultFocusNav|UI_BoxFlag_DisableFocusOverlay|UI_BoxFlag_DrawDropShadow);
      UI_Focus((ws->menu_bar_focused && window_is_focused && !ui_any_ctx_menu_is_open()) ? UI_FocusKind_On : UI_FocusKind_Null)
        UI_TagF("menu_bar")
        UI_Pane(top_bar_rect, str8_lit("###top_bar"))
        UI_WidthFill UI_Row
        UI_Focus(UI_FocusKind_Null)
      {
        UI_Key menu_bar_group_key = ui_key_from_string(ui_key_zero(), str8_lit("###top_bar_group"));
        MemoryZeroArray(ui_top_parent()->parent->corner_radii);

        //- rad: left column
        {
          ui_set_next_flags(UI_BoxFlag_Clip|UI_BoxFlag_ViewScrollX|UI_BoxFlag_ViewClamp);
          UI_WidthFill UI_NamedRow(str8_lit("###menu_bar"))
          {
            //- rad: icon
            UI_Padding(ui_em(0.5f, 1.f))
            {
              UI_PrefWidth(ui_px(dim_2f32(top_bar_rect).y - ui_top_font_size()*0.8f, 1.f))
                UI_Column
                UI_Padding(ui_em(0.4f, 1.f))
                UI_HeightFill
              {
                R_Handle texture = rad_state->icon_texture;
                Vec2S32 texture_dim = r_size_from_tex2d(texture);
                ui_image(texture, R_Tex2DSampleKind_Linear, r2f32p(0, 0, texture_dim.x, texture_dim.y), v4f32(1, 1, 1, 1), 0, str8_lit(""));
              }
            }

            //- rad: menu items
            if(dim_2f32(top_bar_rect).x > ui_top_font_size()*60)
            {
              ui_set_next_flags(UI_BoxFlag_DrawBackground);
              UI_PrefWidth(ui_children_sum(1)) UI_Row UI_PrefWidth(ui_text_dim(20, 1)) UI_GroupKey(menu_bar_group_key)
              {
                // rad: file menu
                UI_Key file_menu_key = ui_key_from_string(ui_key_zero(), str8_lit("_file_menu_key_"));
                UI_CtxMenu(file_menu_key) UI_TagF("implicit")
                {
                  RAD_CmdKind cmds[] =
                  {
                    RAD_CmdKind_OpenFile,
                    RAD_CmdKind_OpenFolder,
                    RAD_CmdKind_QuickOpen,
                    RAD_CmdKind_Save,
                    RAD_CmdKind_SaveAll,
                    RAD_CmdKind_Null,//-
                    RAD_CmdKind_Settings,
                    RAD_CmdKind_Null,//-
                    RAD_CmdKind_Exit,
                  };
                  U32 codepoints[] =
                  {
                    'o',
                    'f',
                    'g',
                    's',
                    'a',
                    0,//-
                    'e',
                    0,//-
                    'x',
                  };
                  StaticAssert(ArrayCount(codepoints) == ArrayCount(cmds), menu_button_check);
                  UI_PrefWidth(ui_px(rad_cmd_list_menu_width_px(ArrayCount(cmds), cmds), 1.f))
                    rad_cmd_list_menu_buttons(ArrayCount(cmds), cmds, codepoints);
                }

                // rad: edit menu
                UI_Key edit_menu_key = ui_key_from_string(ui_key_zero(), str8_lit("_edit_menu_key_"));
                UI_CtxMenu(edit_menu_key) UI_TagF("implicit")
                {
                  RAD_CmdKind cmds[] =
                  {
                    RAD_CmdKind_Undo,
                    RAD_CmdKind_Redo,
                    RAD_CmdKind_Null,//-
                    RAD_CmdKind_Search,
                    RAD_CmdKind_SearchBackwards,
                    RAD_CmdKind_FindNext,
                    RAD_CmdKind_FindPrev,
                    RAD_CmdKind_GoToLine,
                  };
                  U32 codepoints[] =
                  {
                    'u',
                    'r',
                    0,//-
                    's',
                    'b',
                    'n',
                    'p',
                    'g',
                  };
                  StaticAssert(ArrayCount(codepoints) == ArrayCount(cmds), menu_button_check);
                  UI_PrefWidth(ui_px(rad_cmd_list_menu_width_px(ArrayCount(cmds), cmds), 1.f))
                    rad_cmd_list_menu_buttons(ArrayCount(cmds), cmds, codepoints);
                }

                // rad: window menu
                UI_Key window_menu_key = ui_key_from_string(ui_key_zero(), str8_lit("_window_menu_key_"));
                UI_CtxMenu(window_menu_key) UI_TagF("implicit")
                {
                  RAD_CmdKind cmds[] =
                  {
                    RAD_CmdKind_ToggleFullscreen,
                    RAD_CmdKind_Null,//-
                    RAD_CmdKind_IncWindowFontSize,
                    RAD_CmdKind_DecWindowFontSize,
                  };
                  U32 codepoints[] =
                  {
                    'f',
                    0,//-
                    'i',
                    'd',
                  };
                  StaticAssert(ArrayCount(codepoints) == ArrayCount(cmds), menu_button_check);
                  UI_PrefWidth(ui_px(rad_cmd_list_menu_width_px(ArrayCount(cmds), cmds), 1.f))
                    rad_cmd_list_menu_buttons(ArrayCount(cmds), cmds, codepoints);
                }

                // rad: panel menu
                UI_Key panel_menu_key = ui_key_from_string(ui_key_zero(), str8_lit("_panel_menu_key_"));
                UI_CtxMenu(panel_menu_key) UI_TagF("implicit")
                {
                  RAD_CmdKind cmds[] =
                  {
                    RAD_CmdKind_NewPanelRight,
                    RAD_CmdKind_NewPanelDown,
                    RAD_CmdKind_ClosePanel,
                    RAD_CmdKind_Null,//-
                    RAD_CmdKind_NextPanel,
                    RAD_CmdKind_PrevPanel,
                    RAD_CmdKind_Null,//-
                    RAD_CmdKind_ZoomPanel,
                    RAD_CmdKind_UnzoomPanel,
                    RAD_CmdKind_ToggleZoomPanel,
                  };
                  U32 codepoints[] =
                  {
                    'r',
                    'd',
                    'o',
                    0,//-
                    'n',
                    'p',
                    0,//-
                    'z',
                    'm',
                    't',
                  };
                  StaticAssert(ArrayCount(codepoints) == ArrayCount(cmds), menu_button_check);
                  UI_PrefWidth(ui_px(rad_cmd_list_menu_width_px(ArrayCount(cmds), cmds), 1.f))
                    rad_cmd_list_menu_buttons(ArrayCount(cmds), cmds, codepoints);
                }

                // rad: view menu (not in raddbg; VS2022's View: the tool windows)
                UI_Key view_menu_key = ui_key_from_string(ui_key_zero(), str8_lit("_view_menu_key_"));
                UI_CtxMenu(view_menu_key) UI_TagF("implicit")
                {
                  RAD_CmdKind cmds[] =
                  {
                    RAD_CmdKind_ShowExplorer,
                    RAD_CmdKind_SearchExplorer,
                    RAD_CmdKind_ShowOutput,
                    RAD_CmdKind_ShowProblems,
                    RAD_CmdKind_Null,//-
                    RAD_CmdKind_CloseToolWindow,
                  };
                  U32 codepoints[] =
                  {
                    'e',
                    's',
                    'o',
                    'p',
                    0,//-
                    'c',
                  };
                  StaticAssert(ArrayCount(codepoints) == ArrayCount(cmds), menu_button_check);
                  UI_PrefWidth(ui_px(rad_cmd_list_menu_width_px(ArrayCount(cmds), cmds), 1.f))
                    rad_cmd_list_menu_buttons(ArrayCount(cmds), cmds, codepoints);
                }

                // rad: tab menu
                UI_Key tab_menu_key = ui_key_from_string(ui_key_zero(), str8_lit("_tab_menu_key_"));
                UI_CtxMenu(tab_menu_key) UI_TagF("implicit")
                {
                  RAD_CmdKind cmds[] =
                  {
                    RAD_CmdKind_CloseTab,
                    RAD_CmdKind_TogglePinTab,
                    RAD_CmdKind_Null,//-
                    RAD_CmdKind_NextTab,
                    RAD_CmdKind_PrevTab,
                  };
                  U32 codepoints[] =
                  {
                    'c',
                    'i',
                    0,//-
                    'n',
                    'p',
                  };
                  StaticAssert(ArrayCount(codepoints) == ArrayCount(cmds), menu_button_check);
                  UI_PrefWidth(ui_px(rad_cmd_list_menu_width_px(ArrayCount(cmds), cmds), 1.f))
                    rad_cmd_list_menu_buttons(ArrayCount(cmds), cmds, codepoints);
                }

                // rad: build menu (cargo)
                UI_Key build_menu_key = ui_key_from_string(ui_key_zero(), str8_lit("_build_menu_key_"));
                UI_CtxMenu(build_menu_key) UI_TagF("implicit")
                {
                  RAD_CmdKind cmds[] =
                  {
                    RAD_CmdKind_CargoBuild,
                    RAD_CmdKind_CargoCheck,
                    RAD_CmdKind_CargoRun,
                    RAD_CmdKind_CargoTest,
                    RAD_CmdKind_CargoStop,
                    RAD_CmdKind_Null,//-
                    RAD_CmdKind_CargoDebug,
                    RAD_CmdKind_CargoRelease,
                    RAD_CmdKind_Null,//-
                    RAD_CmdKind_NextError,
                    RAD_CmdKind_PrevError,
                    RAD_CmdKind_Null,//-
                    RAD_CmdKind_ShowOutput,
                    RAD_CmdKind_ShowProblems,
                    RAD_CmdKind_ClearOutput,
                    RAD_CmdKind_Null,//-
                    RAD_CmdKind_RestartLanguageServer,
                  };
                  U32 codepoints[] =
                  {
                    'b',
                    'c',
                    'r',
                    't',
                    's',
                    0,//-
                    'd',
                    'e',
                    0,//-
                    'n',
                    'p',
                    0,//-
                    'o',
                    'm',
                    'l',
                    0,//-
                    'v',
                  };
                  StaticAssert(ArrayCount(codepoints) == ArrayCount(cmds), menu_button_check);
                  UI_PrefWidth(ui_px(rad_cmd_list_menu_width_px(ArrayCount(cmds), cmds), 1.f))
                    rad_cmd_list_menu_buttons(ArrayCount(cmds), cmds, codepoints);
                }

                // rad: help menu
                UI_Key help_menu_key = ui_key_from_string(ui_key_zero(), str8_lit("_help_menu_key_"));
                // (not 50 em as in raddbg: its longest line plus a margin; the
                // user found the menus too wide, 2026-11-07)
                String8 help_credit = str8_lit("Built on the RAD Debugger codebase (MIT license).");
                F32 help_width_px = ceil_f32(Max(fnt_dim_from_tag_size_string(ui_top_font(), ui_top_font_size(), 0, 0, help_credit).x,
                                                 fnt_dim_from_tag_size_string(ui_top_font(), ui_top_font_size(), 0, 0, str8_lit(BUILD_TITLE_STRING_LITERAL)).x) +
                                             ui_top_font_size()*4.f);
                UI_CtxMenu(help_menu_key) UI_PrefWidth(ui_px(help_width_px, 1.f)) UI_TagF("implicit")
                {
                  UI_Row UI_TextAlignment(UI_TextAlign_Center) UI_TagF("weak")
                    ui_label(str8_lit(BUILD_TITLE_STRING_LITERAL));
                  ui_spacer(ui_em(1.f, 1.f));
                  UI_PrefHeight(ui_children_sum(1)) UI_Row UI_Padding(ui_pct(1, 0))
                  {
                    R_Handle texture = rad_state->icon_texture;
                    Vec2S32 texture_dim = r_size_from_tex2d(texture);
                    UI_PrefWidth(ui_px(ui_top_font_size()*10.f, 1.f))
                      UI_PrefHeight(ui_px(ui_top_font_size()*10.f, 1.f))
                      ui_image(texture, R_Tex2DSampleKind_Linear, r2f32p(0, 0, texture_dim.x, texture_dim.y), v4f32(1, 1, 1, 1), 0, str8_lit(""));
                  }
                  ui_spacer(ui_em(1.f, 1.f));
                  UI_TagF("weak")
                    UI_Row UI_TextAlignment(UI_TextAlign_Center)
                    ui_label(help_credit);
                  ui_spacer(ui_em(0.5f, 1.f));
                }

                // rad: buttons
                UI_TextAlignment(UI_TextAlign_Center) UI_HeightFill
                {
                  // rad: set up table
                  struct
                  {
                    String8 name;
                    U32 codepoint;
                    WM_Key key;
                    UI_Key menu_key;
                  }
                  items[] =
                  {
                    {str8_lit("File"),     'f', WM_Key_F, file_menu_key},
                    {str8_lit("Edit"),     'e', WM_Key_E, edit_menu_key},
                    {str8_lit("View"),     'v', WM_Key_V, view_menu_key},
                    {str8_lit("Window"),   'w', WM_Key_W, window_menu_key},
                    {str8_lit("Panel"),    'p', WM_Key_P, panel_menu_key},
                    {str8_lit("Tab"),      'b', WM_Key_B, tab_menu_key},
                    {str8_lit("Build"),    'u', WM_Key_U, build_menu_key},
                    {str8_lit("Help"),     'h', WM_Key_H, help_menu_key},
                  };

                  // rad: determine if one of the menus is already open
                  B32 menu_open = 0;
                  U64 open_menu_idx = 0;
                  for(U64 idx = 0; idx < ArrayCount(items); idx += 1)
                  {
                    if(ui_ctx_menu_is_open(items[idx].menu_key))
                    {
                      menu_open = 1;
                      open_menu_idx = idx;
                      break;
                    }
                  }

                  // rad: navigate between menus
                  U64 open_menu_idx_prime = open_menu_idx;
                  if(menu_open && ws->menu_bar_focused && window_is_focused)
                  {
                    for(UI_Event *evt = 0; ui_next_event(&evt);)
                    {
                      B32 taken = 0;
                      if(evt->delta_2s32.x > 0)
                      {
                        taken = 1;
                        open_menu_idx_prime += 1;
                        open_menu_idx_prime = open_menu_idx_prime%ArrayCount(items);
                      }
                      if(evt->delta_2s32.x < 0)
                      {
                        taken = 1;
                        open_menu_idx_prime = open_menu_idx_prime > 0 ? open_menu_idx_prime-1 : (ArrayCount(items)-1);
                      }
                      if(taken)
                      {
                        ui_eat_event(evt);
                      }
                    }
                  }

                  // rad: make ui
                  UI_TagF("implicit")
                    UI_VisualMarginX(ui_top_font_size()*0.45f)
                    UI_VisualMarginY(ui_top_font_size()*0.5f)
                    UI_CornerRadius(ui_top_font_size()*0.5f)
                    for(U64 idx = 0; idx < ArrayCount(items); idx += 1)
                  {
                    ui_set_next_fastpath_codepoint(items[idx].codepoint);
                    B32 alt_fastpath_key = 0;
                    if(settings->focus_menu_bar_with_alt && ui_key_press(WM_Modifier_Alt, items[idx].key))
                    {
                      alt_fastpath_key = 1;
                    }
                    if((ws->menu_bar_key_held || ws->menu_bar_focused) && !ui_any_ctx_menu_is_open())
                    {
                      ui_set_next_flags(UI_BoxFlag_DrawTextFastpathCodepoint);
                    }
                    UI_TagF(!ui_ctx_menu_is_open(items[idx].menu_key) ? "weak" : "")
                    {
                      UI_Signal sig = rad_menu_bar_button(items[idx].name);
                      wm_window_push_custom_title_bar_client_area(ws->os, sig.box->rect);
                      if(menu_open)
                      {
                        if((ui_hovering(sig) && !ui_ctx_menu_is_open(items[idx].menu_key)) || (open_menu_idx_prime == idx && open_menu_idx_prime != open_menu_idx))
                        {
                          ui_ctx_menu_open(items[idx].menu_key, sig.box->key, v2f32(0, sig.box->rect.y1-sig.box->rect.y0));
                        }
                      }
                      else if(ui_pressed(sig) || alt_fastpath_key)
                      {
                        if(ui_ctx_menu_is_open(items[idx].menu_key))
                        {
                          ui_ctx_menu_close();
                        }
                        else
                        {
                          ui_ctx_menu_open(items[idx].menu_key, sig.box->key, v2f32(0, sig.box->rect.y1-sig.box->rect.y0));
                        }
                      }
                    }
                  }
                }
              }
            }
          }
        }

        //- rad: cargo configuration menu (center column's [Debug v] box)
        MemoryZeroArray(ws->top_bar_cmd_rects);
        UI_Key profile_menu_key = ui_key_from_string(ui_key_zero(), str8_lit("_cargo_profile_menu_key_"));
        UI_CtxMenu(profile_menu_key) UI_PrefWidth(ui_em(18.f, 1.f)) UI_TagF("implicit")
        {
          RAD_CmdKind kinds[] = {RAD_CmdKind_CargoDebug, RAD_CmdKind_CargoRelease};
          String8 names[] = {str8_lit("Debug"), str8_lit("Release")};
          for EachElement(idx, kinds)
          {
            B32 selected = ((idx == 1) == !!rad_cargo_release());
            ui_set_next_child_layout_axis(Axis2_X);
            UI_Box *box = ui_build_box_from_stringf(UI_BoxFlag_DrawBorder|
                                                    UI_BoxFlag_DrawBackground|
                                                    UI_BoxFlag_DrawHotEffects|
                                                    UI_BoxFlag_DrawActiveEffects|
                                                    UI_BoxFlag_Clickable,
                                                    "###cargo_profile_%I64u", (U64)idx);
            UI_Parent(box) UI_HeightFill UI_Padding(ui_em(1.f, 1.f))
            {
              RAD_Font(RAD_FontSlot_Icons)
                UI_PrefWidth(ui_em(2.f, 1.f))
                UI_TextAlignment(UI_TextAlign_Center)
                UI_TagF("weak")
                ui_label(selected ? rad_icon_kind_text_table[RAD_IconKind_Check] : str8_lit(""));
              UI_PrefWidth(ui_text_dim(10, 1.f)) ui_label(names[idx]);
            }
            ws->top_bar_cmd_rects[kinds[idx]] = box->rect;
            if(ui_clicked(ui_signal_from_box(box)))
            {
              rad_cmd(kinds[idx]);
              ui_ctx_menu_close();
            }
          }
        }

        //- rad: center column (port of raddbg's run controls, for cargo:
        // Run, Test, Check, Build, Stop; before them the configuration box,
        // Debug or Release, like Visual Studio's)
        if(dim_2f32(top_bar_rect).x > ui_top_font_size()*60)
          UI_PrefWidth(ui_children_sum(1.f)) UI_Row
          UI_TagF("implicit")
          UI_CornerRadius(ui_top_font_size()*1.f)
          UI_VisualMargin(ui_top_font_size()*0.5f)
        {
          F32 main_font_size = ui_top_font_size();
          B32 cargo_running = rad_cargo_is_running();
          B32 release = rad_cargo_release();

          // rad: configuration box: [Debug v]
          UI_Signal profile_sig = {0};
          {
            ui_set_next_child_layout_axis(Axis2_X);
            ui_set_next_pref_width(ui_children_sum(1.f));
            UI_Box *box = ui_build_box_from_stringf(UI_BoxFlag_DrawBorder|
                                                    UI_BoxFlag_DrawBackground|
                                                    UI_BoxFlag_DrawHotEffects|
                                                    UI_BoxFlag_DrawActiveEffects|
                                                    UI_BoxFlag_Clickable,
                                                    "###cargo_profile_box");
            UI_Parent(box) UI_HeightFill
            {
              ui_spacer(ui_em(0.9f, 1.f));
              UI_PrefWidth(ui_text_dim(1, 1.f)) ui_label(release ? str8_lit("Release") : str8_lit("Debug"));
              ui_spacer(ui_em(0.6f, 1.f));
              RAD_Font(RAD_FontSlot_Icons)
                UI_FontSize(main_font_size*0.75f)
                UI_PrefWidth(ui_text_dim(1, 1.f))
                UI_TextAlignment(UI_TextAlign_Center)
                UI_TagF("weak")
                ui_label(rad_icon_kind_text_table[RAD_IconKind_DownCaret]);
              ui_spacer(ui_em(0.9f, 1.f));
            }
            profile_sig = ui_signal_from_box(box);
          }
          wm_window_push_custom_title_bar_client_area(ws->os, profile_sig.box->rect);
          ws->top_bar_cmd_rects[RAD_CmdKind_Null] = profile_sig.box->rect;
          if(ui_pressed(profile_sig))
          {
            if(ui_ctx_menu_is_open(profile_menu_key))
            {
              ui_ctx_menu_close();
            }
            else
            {
              ui_ctx_menu_open(profile_menu_key, profile_sig.box->key, v2f32(0, profile_sig.box->rect.y1-profile_sig.box->rect.y0));
            }
          }
          if(ui_hovering(profile_sig) && !ui_ctx_menu_is_open(profile_menu_key)) UI_Tooltip
          {
            ui_state->tooltip_anchor_key = profile_sig.box->key;
            ui_label(str8_lit("Configuration (Release builds with --release)"));
          }
          ui_spacer(ui_em(0.5f, 1.f));

          // rad: command buttons
          struct
          {
            RAD_CmdKind kind;
            String8 tag;
            B32 is_enabled;
          }
          buttons[] =
          {
            {RAD_CmdKind_CargoRun,   str8_lit("good"),    1},
            {RAD_CmdKind_CargoTest,  str8_lit("neutral"), 1},
            {RAD_CmdKind_CargoCheck, str8_lit(""),        1},
            {RAD_CmdKind_CargoBuild, str8_lit(""),        1},
            {RAD_CmdKind_CargoStop,  str8_lit("bad"),     cargo_running},
          };
          UI_PrefWidth(ui_px(dim_2f32(top_bar_rect).y, 1))
            RAD_Font(RAD_FontSlot_Icons)
            UI_FontSize(main_font_size*0.85f)
            UI_TextAlignment(UI_TextAlign_Center)
            for EachElement(idx, buttons)
            UI_Flags(buttons[idx].is_enabled ? 0 : UI_BoxFlag_Disabled)
            UI_Tag(buttons[idx].is_enabled ? buttons[idx].tag : str8_lit("weak"))
          {
            RAD_CmdKindInfo *info = &rad_cmd_kind_info_table[buttons[idx].kind];
            UI_Signal sig = ui_buttonf("%S###cargo_btn_%S", rad_icon_kind_text_table[info->icon_kind], info->code_name);
            wm_window_push_custom_title_bar_client_area(ws->os, sig.box->rect);
            ws->top_bar_cmd_rects[buttons[idx].kind] = sig.box->rect;
            ws->top_bar_cmd_icons[buttons[idx].kind] = rad_icon_kind_text_table[info->icon_kind];
            if(ui_hovering(sig)) UI_Tooltip
              RAD_Font(RAD_FontSlot_Main)
              UI_FontSize(main_font_size)
              UI_TextAlignment(UI_TextAlign_Left)
            {
              ui_state->tooltip_anchor_key = sig.box->key;
              ui_set_next_pref_width(ui_children_sum(1));
              UI_Row
              {
                ui_label(info->display_name);
                rad_cmd_binding_labels(buttons[idx].kind, 0);
              }
            }
            if(ui_clicked(sig))
            {
              rad_cmd(buttons[idx].kind);
            }
          }
        }

        //- rad: right column
        UI_WidthFill UI_Row
        {
          ui_spacer(ui_pct(1, 0));

          // rad: min/max/close buttons
          UI_TagF("implicit")
            UI_TagF("weak")
            UI_VisualMargin(ui_top_font_size()*0.5f)
            UI_CornerRadius(ui_top_font_size()*0.9f)
          {
            UI_Signal min_sig = {0};
            UI_Signal max_sig = {0};
            UI_Signal cls_sig = {0};
            Vec2F32 bar_dim = dim_2f32(top_bar_rect);
            F32 button_dim = floor_f32(bar_dim.y);
            UI_PrefWidth(ui_px(button_dim, 1.f))
              UI_FontSize(ui_top_font_size()*0.75f)
            {
              min_sig = rad_icon_buttonf(RAD_IconKind_WindowMinimize,  0, "##minimize");
              max_sig = rad_icon_buttonf(wm_window_is_maximized(ws->os) ? RAD_IconKind_WindowRestore : RAD_IconKind_Window, 0, "##maximize");
            }
            UI_PrefWidth(ui_px(button_dim, 1.f))
              UI_FontSize(ui_top_font_size()*0.85f)
            {
              cls_sig = rad_icon_buttonf(RAD_IconKind_X, 0, "##close");
            }
            if(ui_clicked(min_sig))
            {
              wm_window_set_minimized(ws->os, 1);
            }
            if(ui_clicked(max_sig))
            {
              wm_window_set_maximized(ws->os, !wm_window_is_maximized(ws->os));
            }
            if(ui_clicked(cls_sig))
            {
              rad_cmd(RAD_CmdKind_Exit);
            }
            wm_window_push_custom_title_bar_client_area(ws->os, min_sig.box->rect);
            wm_window_push_custom_title_bar_client_area(ws->os, max_sig.box->rect);
            wm_window_push_custom_title_bar_client_area(ws->os, pad_2f32(cls_sig.box->rect, 2.f));
          }
        }
      }
    }

    ////////////////////////////
    //- rad: bottom bar
    //
    // NOTE(rad): raddbg colors this bar by debugger state: "pop" with
    // "Running..." while the target runs, "good_pop" when it is stopped or
    // finished, "bad_pop" for an exception or a user error. A cargo command
    // takes the target's place: running -> "pop"; it failed (exit code != 0,
    // or cargo could not start) -> "bad_pop" until the next command;
    // otherwise (nothing ran, success, stopped) -> "good_pop".
    //
    ProfScope("build bottom bar") UI_FontSize(floor_f32(ui_top_font_size()*0.85f))
    {
      String8 tag = str8_lit("good_pop");
      B32 show_error = (rad_state->error_t >= 0.01f && rad_state->error_string_size != 0);
      RAD_CargoState *cargo = rad_cargo;
      B32 cargo_running = rad_cargo_is_running();
      B32 cargo_ran = (cargo->run != 0 && !cargo_running);
      B32 cargo_stopped = 0;
      B32 cargo_launch_failed = 0;
      B32 cargo_in_terminal = 0;
      U64 cargo_exit_code = 0;
      MutexScope(cargo->mutex)
      {
        cargo_stopped = cargo->stop_requested;
        cargo_launch_failed = cargo->launch_failed;
        cargo_in_terminal = cargo->in_terminal;
        cargo_exit_code = cargo->exit_code;
      }
      B32 cargo_failed = (cargo_ran && !cargo_stopped && (cargo_launch_failed || cargo_exit_code != 0));
      if(show_error || cargo_failed)
      {
        tag = str8_lit("bad_pop");
      }
      else if(cargo_running)
      {
        tag = str8_lit("pop");
      }

      //- rad: compute fstrs for status explanation
      DR_FStrList status_fstrs = {0};
      if(show_error)
      {
        String8 error_string = str8(rad_state->error_buffer, rad_state->error_string_size);
        DR_FStrParams params = {ui_top_font(), ui_top_text_raster_flags(), ui_color_from_name(str8_lit("text")), ui_top_font_size()};
        dr_fstrs_push_new(scratch.arena, &status_fstrs, &params, rad_icon_kind_text_table[RAD_IconKind_WarningBig],
                          .font = rad_font_from_slot(RAD_FontSlot_Icons),
                          .raster_flags = rad_raster_flags_from_slot(RAD_FontSlot_Icons));
        dr_fstrs_push_new(scratch.arena, &status_fstrs, &params, str8_lit("  "));
        dr_fstrs_push_new(scratch.arena, &status_fstrs, &params, error_string);
      }
      else if(cargo_running || cargo_ran)
      {
        // rad: same layout as raddbg's "Running..." (icon, two spaces, text)
        DR_FStrParams params = {ui_top_font(), ui_top_text_raster_flags(), ui_color_from_name(str8_lit("text")), ui_top_font_size()};
        String8 cmd_name = rad_cargo_cmd_display_name(cargo->run->kind);
        RAD_IconKind icon = RAD_IconKind_Play;
        String8 text = {0};
        String8 counts = {0};
        if(cargo->error_count != 0 || cargo->warning_count != 0)
        {
          counts = push_str8f(scratch.arena, "%I64u error%s, %I64u warning%s",
                              cargo->error_count, cargo->error_count == 1 ? "" : "s",
                              cargo->warning_count, cargo->warning_count == 1 ? "" : "s");
        }
        if(cargo_running && cargo_in_terminal)
        {
          text = push_str8f(scratch.arena, "Running %S in its terminal window...", cargo->run->terminal_title);
        }
        else if(cargo_running && cargo->run->kind == RAD_CargoCmdKind_Run)
        {
          text = str8_lit("Building for cargo run...");
        }
        else if(cargo_running)
        {
          text = push_str8f(scratch.arena, "Running %S...", cmd_name);
        }
        else if(cargo_stopped)
        {
          icon = RAD_IconKind_Stop;
          text = push_str8f(scratch.arena, "%S stopped", cmd_name);
        }
        else if(cargo_launch_failed)
        {
          icon = RAD_IconKind_WarningBig;
          text = str8_lit("Could not start cargo (is it on the PATH?)");
        }
        else if(cargo_exit_code != 0)
        {
          icon = RAD_IconKind_WarningBig;
          text = push_str8f(scratch.arena, "%S failed (exit code %I64u)%s%S", cmd_name, cargo_exit_code, counts.size ? ": " : "", counts);
        }
        else
        {
          icon = RAD_IconKind_Check;
          text = push_str8f(scratch.arena, "%S finished%s%S", cmd_name, counts.size ? ": " : "", counts);
        }
        dr_fstrs_push_new(scratch.arena, &status_fstrs, &params, rad_icon_kind_text_table[icon],
                          .font = rad_font_from_slot(RAD_FontSlot_Icons),
                          .raster_flags = rad_raster_flags_from_slot(RAD_FontSlot_Icons));
        dr_fstrs_push_new(scratch.arena, &status_fstrs, &params, str8_lit("  "));
        dr_fstrs_push_new(scratch.arena, &status_fstrs, &params, text);
      }
      else
      {
        DR_FStrParams params = {ui_top_font(), ui_top_text_raster_flags(), ui_color_from_name(str8_lit("text")), ui_top_font_size()};
        CFG_PanelNode *focused_panel = panel_tree.focused;
        String8 file_path = rad_file_path_from_tab(focused_panel->selected_tab);
        if(file_path.size != 0)
        {
          dr_fstrs_push_new(scratch.arena, &status_fstrs, &params, file_path);
        }
        else
        {
          dr_fstrs_push_new(scratch.arena, &status_fstrs, &params, str8_lit("Ready"));
        }
      }

      //- rad: build bottom bar (its row and contents fill its height: the
      // bar is 2x the UI font, lower than raddbg's, so the default 3x row
      // pushed the text down, out of the window at large UI font sizes)
      UI_Flags(UI_BoxFlag_DrawBackground) UI_CornerRadius(0)
        UI_Tag(tag)
        UI_Pane(bottom_bar_rect, str8_lit("###bottom_bar")) UI_WidthFill UI_HeightFill UI_Row
        UI_Flags(0)
      {
        // rad: build status (keyed: its last layout is kept for scripts)
        UI_PrefWidth(ui_text_dim(10, 1))
        {
          ui_spacer(ui_em(0.5f, 1.f));
          UI_Box *box = ui_build_box_from_string(UI_BoxFlag_DrawText, str8_lit("###status_text"));
          ui_box_equip_display_fstrs(box, &status_fstrs);
          ws->bottom_bar_rect = bottom_bar_rect;
          ws->status_text_rect = box->rect;
        }

        ui_spacer(ui_pct(1, 0));

        // rad: not in raddbg: the language server's work (indexing, cargo
        // check)
        UI_PrefWidth(ui_text_dim(10, 1)) UI_TextAlignment(UI_TextAlign_Center) UI_TagF("weak")
        {
          B32 lsp_busy = 0;
          String8 lsp_status = rad_lsp_status_string(scratch.arena, &lsp_busy);
          if(lsp_status.size != 0)
          {
            if(lsp_busy)
            {
              RAD_Font(RAD_FontSlot_Icons) ui_label(rad_icon_kind_text_table[RAD_IconKind_Refresh]);
            }
            ui_label(lsp_status);
            ui_spacer(ui_em(1.f, 1.f));
          }
        }

        // rad: not in raddbg (Zed's status bar): the focused editor's cursor
        // (line:column) and language. The version is in the Help menu
        {
          CFG_PanelNode *focused_panel = panel_tree.focused;
          CFG_Node *focused_tab = (focused_panel != &cfg_nil_panel_node ? focused_panel->selected_tab : &cfg_nil_node);
          RAD_Document *focused_doc = (str8_match(focused_tab->string, str8_lit("text"), 0) ? rad_document_from_tab(focused_tab) : 0);
          if(focused_doc != 0) UI_PrefWidth(ui_text_dim(10, 1)) UI_TextAlignment(UI_TextAlign_Center)
          {
            RAD_CodeViewState *focused_cv = &rad_view_state_from_cfg(focused_tab)->cv;
            if(focused_cv->layout_cursor_line != 0)
            {
              ui_labelf("%I64d:%I64d", focused_cv->layout_cursor_line, focused_cv->layout_cursor_col);
              ui_spacer(ui_em(0.5f, 1.f));
            }
            ui_label(rad_lang_display_name(focused_doc->lang));
            ui_spacer(ui_em(0.5f, 1.f));
          }
        }
      }
    }

    ////////////////////////////
    //- rad: drag/drop visualization tooltip (raddbg's: the dragged tab's
    // title; rad: without the view preview below it; a file: its name)
    //
    if(rad_drag_is_active())
    {
      CFG_Node *drag_tab = cfg_node_from_id(rad_state->drag_tab);
      DR_FStrList fstrs = {0};
      if((rad_state->drag_kind == RAD_DragKind_Tab || rad_state->drag_kind == RAD_DragKind_Panel) && drag_tab != &cfg_nil_node)
      {
        fstrs = rad_title_fstrs_from_tab(scratch.arena, drag_tab);
      }
      else if(rad_state->drag_kind == RAD_DragKind_FilePath)
      {
        String8 path = str8(rad_state->drag_path_buffer, rad_state->drag_path_size);
        DR_FStrParams params = {ui_top_font(), ui_top_text_raster_flags(), ui_color_from_name(str8_lit("text")), ui_top_font_size()};
        dr_fstrs_push_new(scratch.arena, &fstrs, &params, push_str8_copy(scratch.arena, str8_skip_last_slash(path)));
      }
      if(fstrs.node_count != 0) UI_Tooltip
      {
        UI_Row UI_PrefWidth(ui_text_dim(10, 1))
        {
          UI_Box *name_box = ui_build_box_from_key(UI_BoxFlag_DrawText, ui_key_zero());
          ui_box_equip_display_fstrs(name_box, &fstrs);
        }
      }
    }

    ////////////////////////////
    //- rad: panel non-leaf UI (drag boundaries)
    //
    B32 is_changing_panel_boundaries = 0;
    ProfScope("non-leaf panel UI")
    {
      CFG_PanelNode *panel_tree_root = (panel_tree.zoom_root != &cfg_nil_panel_node) ? panel_tree.zoom_root : panel_tree.root;
      for(CFG_PanelNode *panel = panel_tree_root;
          panel != &cfg_nil_panel_node;
          panel = cfg_panel_node_rec__depth_first_pre(panel_tree_root, panel).next)
      {
        //- rad: continue on leaf panels
        if(panel->first == &cfg_nil_panel_node)
        {
          continue;
        }

        //- rad: grab info
        Axis2 split_axis = panel->split_axis;
        Rng2F32 panel_rect = cfg_target_rect_from_panel_node(content_rect, panel_tree_root, panel);
        if(panel == panel_tree.zoom_root)
        {
          panel_rect = content_rect;
        }

        //- rad: boundary tab-drag/drop sites (port of raddbg's; rad: only
        // next to editor panels, and in a row with a tool window only once
        // it has 2+ editor panels: raddbg shows none for a single panel)
        if(rad_drag_is_dockable())
        {
          //- rad: params
          F32 drop_site_major_dim_px = ceil_f32(ui_top_font_size()*7.f);
          F32 drop_site_minor_dim_px = ceil_f32(ui_top_font_size()*5.f);
          F32 corner_radius = ui_top_font_size()*0.5f;
          F32 padding = ceil_f32(ui_top_font_size()*0.5f);
          B32 panel_is_editor_only = !rad_panel_subtree_has_tool__(panel);
          U64 editor_child_count = 0;
          for(CFG_PanelNode *child = panel->first; child != &cfg_nil_panel_node; child = child->next)
          {
            editor_child_count += (child->pct_of_parent > 0.001f && !rad_panel_subtree_has_tool__(child));
          }

          //- rad: sites across the split axis on the editor area's root
          // (raddbg's special case for its root panel)
          if(panel_is_editor_only && (panel->parent == &cfg_nil_panel_node || rad_panel_subtree_has_tool__(panel->parent))) UI_CornerRadius(corner_radius)
          {
            Vec2F32 panel_rect_center = center_2f32(panel_rect);
            Axis2 axis = axis2_flip(panel->split_axis);
            for EachEnumVal(Side, side)
            {
              UI_Key key = ui_key_from_stringf(ui_key_zero(), "root_extra_split_%i_%p", side, panel->cfg);
              Rng2F32 site_rect = panel_rect;
              site_rect.p0.v[axis2_flip(axis)] = panel_rect_center.v[axis2_flip(axis)] - drop_site_major_dim_px/2;
              site_rect.p1.v[axis2_flip(axis)] = panel_rect_center.v[axis2_flip(axis)] + drop_site_major_dim_px/2;
              site_rect.p0.v[axis] = panel_rect.v[side].v[axis] - drop_site_minor_dim_px/2;
              site_rect.p1.v[axis] = panel_rect.v[side].v[axis] + drop_site_minor_dim_px/2;

              // rad: build
              UI_Box *site_box = &ui_nil_box;
              {
                F32 site_open_t = ui_anim(ui_key_from_stringf(key, "open_t"), 1.f, .rate = rad_state->menu_animation_rate);
                UI_Rect(site_rect) UI_Squish(0.1f-0.1f*site_open_t) UI_Transparency(1-site_open_t)
                {
                  site_box = ui_build_box_from_key(UI_BoxFlag_DropSite|UI_BoxFlag_DrawHotEffects, key);
                  ui_signal_from_box(site_box);
                }
                UI_Box *site_box_viz = &ui_nil_box;
                UI_Parent(site_box) UI_WidthFill UI_HeightFill
                  UI_Padding(ui_px(padding, 1.f))
                  UI_Column
                  UI_Padding(ui_px(padding, 1.f))
                  UI_GroupKey(key)
                {
                  ui_set_next_child_layout_axis(axis2_flip(axis));
                  site_box_viz = ui_build_box_from_key(UI_BoxFlag_DrawBackground|
                                                       UI_BoxFlag_DrawBorder|
                                                       UI_BoxFlag_DrawDropShadow|
                                                       UI_BoxFlag_DrawBackgroundBlur|
                                                       UI_BoxFlag_DrawHotEffects, ui_key_zero());
                }
                UI_Parent(site_box_viz) UI_WidthFill UI_HeightFill UI_Padding(ui_px(padding, 1.f))
                {
                  ui_set_next_child_layout_axis(axis);
                  UI_Box *row_or_column = ui_build_box_from_key(0, ui_key_zero()); UI_Parent(row_or_column) UI_Padding(ui_px(padding, 1.f))
                  {
                    ui_build_box_from_key(UI_BoxFlag_DrawBorder, ui_key_zero());
                    ui_spacer(ui_px(padding, 1.f));
                    ui_build_box_from_key(UI_BoxFlag_DrawBorder, ui_key_zero());
                  }
                }
              }

              // rad: viz
              if(ui_key_match(site_box->key, ui_drop_hot_key()))
              {
                Rng2F32 future_split_rect_target = site_rect;
                future_split_rect_target.p0.v[axis] -= drop_site_major_dim_px;
                future_split_rect_target.p1.v[axis] += drop_site_major_dim_px;
                future_split_rect_target.p0.v[axis2_flip(axis)] = panel_rect.p0.v[axis2_flip(axis)];
                future_split_rect_target.p1.v[axis2_flip(axis)] = panel_rect.p1.v[axis2_flip(axis)];
                future_split_rect_target = pad_2f32(future_split_rect_target, -ui_top_font_size()*2.f);
                Vec2F32 future_split_rect_target_center = center_2f32(future_split_rect_target);
                Rng2F32 future_split_rect =
                {
                  ui_anim(ui_key_from_stringf(ui_key_zero(), "drop_site_v0"), future_split_rect_target.x0, .initial = future_split_rect_target_center.x, .rate = rad_state->menu_animation_rate),
                  ui_anim(ui_key_from_stringf(ui_key_zero(), "drop_site_v1"), future_split_rect_target.y0, .initial = future_split_rect_target_center.y, .rate = rad_state->menu_animation_rate),
                  ui_anim(ui_key_from_stringf(ui_key_zero(), "drop_site_v2"), future_split_rect_target.x1, .initial = future_split_rect_target_center.x, .rate = rad_state->menu_animation_rate),
                  ui_anim(ui_key_from_stringf(ui_key_zero(), "drop_site_v3"), future_split_rect_target.y1, .initial = future_split_rect_target_center.y, .rate = rad_state->menu_animation_rate),
                };
                UI_Rect(future_split_rect) UI_TagF("drop_site") UI_CornerRadius(ui_top_font_size()*2.f)
                {
                  ui_build_box_from_key(UI_BoxFlag_DrawBackground|UI_BoxFlag_DrawBorder, ui_key_zero());
                }
              }

              // rad: drop
              if(ui_key_match(site_box->key, ui_drop_hot_key()) && rad_drag_drop())
              {
                Dir2 dir = (axis == Axis2_Y ? (side == Side_Min ? Dir2_Up : Dir2_Down) :
                            (side == Side_Min ? Dir2_Left : Dir2_Right));
                rad_drop_split(panel->cfg, dir);
              }
            }
          }

          //- rad: iterate all children, build boundary drop sites
          if(editor_child_count >= 2) UI_CornerRadius(corner_radius) for(CFG_PanelNode *child = panel->first;; child = child->next)
          {
            // rad: which side gets the new panel (never a tool window)
            CFG_PanelNode *prev = (child == &cfg_nil_panel_node ? panel->last : child->prev);
            B32 next_ok = (child != &cfg_nil_panel_node && child->pct_of_parent > 0.001f && !rad_panel_subtree_has_tool__(child));
            B32 prev_ok = (prev != &cfg_nil_panel_node && prev->pct_of_parent > 0.001f && !rad_panel_subtree_has_tool__(prev));
            if(next_ok || prev_ok)
            {
              // rad: form rect
              CFG_PanelNode *rect_child = (next_ok ? child : prev);
              Rng2F32 child_rect = cfg_target_rect_from_panel_node_child(panel_rect, panel, rect_child);
              Vec2F32 child_rect_center = center_2f32(child_rect);
              F32 boundary = (next_ok ? child_rect.p0.v[split_axis] : child_rect.p1.v[split_axis]);
              UI_Key key = ui_key_from_stringf(ui_key_zero(), "drop_boundary_%p_%p", panel->cfg, child->cfg);
              Rng2F32 site_rect = r2f32(child_rect_center, child_rect_center);
              site_rect.p0.v[split_axis] = boundary - drop_site_minor_dim_px/2;
              site_rect.p1.v[split_axis] = boundary + drop_site_minor_dim_px/2;
              site_rect.p0.v[axis2_flip(split_axis)] -= drop_site_major_dim_px/2;
              site_rect.p1.v[axis2_flip(split_axis)] += drop_site_major_dim_px/2;

              // rad: build
              UI_Box *site_box = &ui_nil_box;
              {
                F32 site_open_t = ui_anim(ui_key_from_stringf(key, "open_t"), 1.f, .rate = rad_state->menu_animation_rate);
                UI_Rect(site_rect) UI_Squish(0.1f-0.1f*site_open_t) UI_Transparency(1-site_open_t)
                {
                  site_box = ui_build_box_from_key(UI_BoxFlag_DropSite|UI_BoxFlag_DrawHotEffects, key);
                  ui_signal_from_box(site_box);
                }
                UI_Box *site_box_viz = &ui_nil_box;
                UI_Parent(site_box) UI_WidthFill UI_HeightFill
                  UI_Padding(ui_px(padding, 1.f))
                  UI_Column
                  UI_Padding(ui_px(padding, 1.f))
                  UI_GroupKey(key)
                {
                  ui_set_next_child_layout_axis(axis2_flip(split_axis));
                  site_box_viz = ui_build_box_from_key(UI_BoxFlag_DrawBackground|
                                                       UI_BoxFlag_DrawBorder|
                                                       UI_BoxFlag_DrawDropShadow|
                                                       UI_BoxFlag_DrawBackgroundBlur|
                                                       UI_BoxFlag_DrawHotEffects, ui_key_zero());
                }
                UI_Parent(site_box_viz) UI_WidthFill UI_HeightFill UI_Padding(ui_px(padding, 1.f))
                {
                  ui_set_next_child_layout_axis(split_axis);
                  UI_Box *row_or_column = ui_build_box_from_key(0, ui_key_zero()); UI_Parent(row_or_column) UI_Padding(ui_px(padding, 1.f))
                  {
                    ui_build_box_from_key(UI_BoxFlag_DrawBorder, ui_key_zero());
                    ui_spacer(ui_px(padding, 1.f));
                    ui_build_box_from_key(UI_BoxFlag_DrawBorder, ui_key_zero());
                  }
                }
              }

              // rad: viz
              if(ui_key_match(site_box->key, ui_drop_hot_key()))
              {
                Rng2F32 future_split_rect_target = site_rect;
                future_split_rect_target.p0.v[split_axis] -= drop_site_major_dim_px;
                future_split_rect_target.p1.v[split_axis] += drop_site_major_dim_px;
                future_split_rect_target.p0.v[axis2_flip(split_axis)] = child_rect.p0.v[axis2_flip(split_axis)];
                future_split_rect_target.p1.v[axis2_flip(split_axis)] = child_rect.p1.v[axis2_flip(split_axis)];
                future_split_rect_target = pad_2f32(future_split_rect_target, -ui_top_font_size()*2.f);
                Vec2F32 future_split_rect_target_center = center_2f32(future_split_rect_target);
                Rng2F32 future_split_rect =
                {
                  ui_anim(ui_key_from_stringf(ui_key_zero(), "drop_site_v0"), future_split_rect_target.x0, .initial = future_split_rect_target_center.x, .rate = rad_state->menu_animation_rate),
                  ui_anim(ui_key_from_stringf(ui_key_zero(), "drop_site_v1"), future_split_rect_target.y0, .initial = future_split_rect_target_center.y, .rate = rad_state->menu_animation_rate),
                  ui_anim(ui_key_from_stringf(ui_key_zero(), "drop_site_v2"), future_split_rect_target.x1, .initial = future_split_rect_target_center.x, .rate = rad_state->menu_animation_rate),
                  ui_anim(ui_key_from_stringf(ui_key_zero(), "drop_site_v3"), future_split_rect_target.y1, .initial = future_split_rect_target_center.y, .rate = rad_state->menu_animation_rate),
                };
                UI_Rect(future_split_rect) UI_TagF("drop_site") UI_CornerRadius(ui_top_font_size()*2.f)
                {
                  ui_build_box_from_key(UI_BoxFlag_DrawBackground|UI_BoxFlag_DrawBorder, ui_key_zero());
                }
              }

              // rad: drop
              if(ui_key_match(site_box->key, ui_drop_hot_key()) && rad_drag_drop())
              {
                Dir2 dir = (next_ok ? (split_axis == Axis2_X ? Dir2_Left : Dir2_Up) :
                            (split_axis == Axis2_X ? Dir2_Right : Dir2_Down));
                rad_drop_split(rect_child->cfg, dir);
              }
            }

            // rad: exit on opl child
            if(child == &cfg_nil_panel_node)
            {
              break;
            }
          }
        }

        //- rad: do UI for drag boundaries between all children
        for(CFG_PanelNode *child = panel->first;
            child != &cfg_nil_panel_node && child->next != &cfg_nil_panel_node;
            child = child->next)
        {
          CFG_PanelNode *min_child = child;
          CFG_PanelNode *max_child = min_child->next;
          if(min_child->pct_of_parent <= 0.001f || max_child->pct_of_parent <= 0.001f)
          {
            // rad: a hidden tool window has no boundary
            continue;
          }
          Rng2F32 min_child_rect = cfg_target_rect_from_panel_node_child(panel_rect, panel, min_child);
          Rng2F32 max_child_rect = cfg_target_rect_from_panel_node_child(panel_rect, panel, max_child);
          Rng2F32 boundary_rect = {0};
          {
            boundary_rect.p0.v[split_axis] = min_child_rect.p1.v[split_axis] - ui_top_font_size()/3;
            boundary_rect.p1.v[split_axis] = max_child_rect.p0.v[split_axis] + ui_top_font_size()/3;
            boundary_rect.p0.v[axis2_flip(split_axis)] = panel_rect.p0.v[axis2_flip(split_axis)];
            boundary_rect.p1.v[axis2_flip(split_axis)] = panel_rect.p1.v[axis2_flip(split_axis)];
          }

          UI_Rect(boundary_rect)
          {
            ui_set_next_hover_cursor(split_axis == Axis2_X ? WM_Cursor_LeftRight : WM_Cursor_UpDown);
            UI_Box *box = ui_build_box_from_stringf(UI_BoxFlag_Clickable, "###%p_%p", min_child->cfg, max_child->cfg);
            UI_Signal sig = ui_signal_from_box(box);
            if(ui_double_clicked(sig))
            {
              ui_kill_action();
              F32 sum_pct = min_child->pct_of_parent + max_child->pct_of_parent;
              min_child->pct_of_parent = 0.5f * sum_pct;
              max_child->pct_of_parent = 0.5f * sum_pct;
              cfg_node_equip_stringf(rad_state->cfg, min_child->cfg, "%f", min_child->pct_of_parent);
              cfg_node_equip_stringf(rad_state->cfg, max_child->cfg, "%f", max_child->pct_of_parent);
            }
            else if(ui_pressed(sig))
            {
              Vec2F32 v = {min_child->pct_of_parent, max_child->pct_of_parent};
              ui_store_drag_struct(&v);
            }
            else if(ui_dragging(sig))
            {
              Vec2F32 v = *ui_get_drag_struct(Vec2F32);
              Vec2F32 mouse_delta      = ui_drag_delta();
              F32 total_size           = dim_2f32(panel_rect).v[split_axis];
              F32 min_pct__before      = v.v[0];
              F32 min_pixels__before   = min_pct__before * total_size;
              F32 min_pixels__after    = min_pixels__before + mouse_delta.v[split_axis];
              if(min_pixels__after < 50.f)
              {
                min_pixels__after = 50.f;
              }
              F32 min_pct__after       = min_pixels__after / total_size;
              F32 pct_delta            = min_pct__after - min_pct__before;
              F32 max_pct__before      = v.v[1];
              F32 max_pct__after       = max_pct__before - pct_delta;
              F32 max_pixels__after    = max_pct__after * total_size;
              if(max_pixels__after < 50.f)
              {
                max_pixels__after = 50.f;
                max_pct__after = max_pixels__after / total_size;
                pct_delta = -(max_pct__after - max_pct__before);
                min_pct__after = min_pct__before + pct_delta;
              }
              min_child->pct_of_parent = min_pct__after;
              max_child->pct_of_parent = max_pct__after;
              cfg_node_equip_stringf(rad_state->cfg, min_child->cfg, "%f", min_pct__after);
              cfg_node_equip_stringf(rad_state->cfg, max_child->cfg, "%f", max_pct__after);
              is_changing_panel_boundaries = 1;
            }
          }
        }
      }
    }

    ////////////////////////////
    //- rad: animate panels
    //
    {
      B32 window_is_resizing = (ws->last_window_rect.x1 != window_rect.x1 ||
                                ws->last_window_rect.y1 != window_rect.y1);
      Vec2F32 content_rect_dim = dim_2f32(content_rect);
      if(content_rect_dim.x > 0 && content_rect_dim.y > 0)
      {
        for(CFG_PanelNode *panel = panel_tree.root;
            panel != &cfg_nil_panel_node;
            panel = cfg_panel_node_rec__depth_first_pre(panel_tree.root, panel).next)
        {
          B32 is_in_zoom = 0;
          for(CFG_PanelNode *p = panel; p != &cfg_nil_panel_node; p = p->parent)
          {
            if(p == panel_tree.zoom_root)
            {
              is_in_zoom = 1;
              break;
            }
          }
          Rng2F32 target_rect_px = cfg_target_rect_from_panel_node(content_rect, is_in_zoom ? panel_tree.zoom_root : panel_tree.root, panel);
          if(panel == panel_tree.zoom_root)
          {
            target_rect_px = content_rect;
          }
          Rng2F32 target_rect_pct = r2f32p(target_rect_px.x0/content_rect_dim.x,
                                           target_rect_px.y0/content_rect_dim.y,
                                           target_rect_px.x1/content_rect_dim.x,
                                           target_rect_px.y1/content_rect_dim.y);
          B32 reset = (window_is_resizing || ws->window_layout_reset || ws->frames_alive < 5 || is_changing_panel_boundaries);
          ui_anim(ui_key_from_stringf(ui_key_zero(), "panel_%p_x0", panel->cfg), target_rect_pct.x0, .initial = target_rect_pct.x0, .reset = reset, .rate = rad_state->menu_animation_rate);
          ui_anim(ui_key_from_stringf(ui_key_zero(), "panel_%p_y0", panel->cfg), target_rect_pct.y0, .initial = target_rect_pct.y0, .reset = reset, .rate = rad_state->menu_animation_rate);
          ui_anim(ui_key_from_stringf(ui_key_zero(), "panel_%p_x1", panel->cfg), target_rect_pct.x1, .initial = target_rect_pct.x1, .reset = reset, .rate = rad_state->menu_animation_rate);
          ui_anim(ui_key_from_stringf(ui_key_zero(), "panel_%p_y1", panel->cfg), target_rect_pct.y1, .initial = target_rect_pct.y1, .reset = reset, .rate = rad_state->menu_animation_rate);
        }
      }
      ws->window_layout_reset = 0;
    }

    ////////////////////////////
    //- rad: panel leaf UI
    //
    if(content_rect.x1 > content_rect.x0 && content_rect.y1 > content_rect.y0)
    {
      // NOTE(rad): as raddbg: the hoisted panel (zoomed, or last zoomed while
      // it animates back) is built first, in front; the others fade out by
      // panel_zoom_t while a panel is zoomed
      if(ws->panel_hoist_cfg_id != panel_tree.zoom_root->cfg->id && panel_tree.zoom_root->cfg->id != 0)
      {
        ws->panel_hoist_cfg_id = panel_tree.zoom_root->cfg->id;
      }
      CFG_PanelNode *front_panel = cfg_panel_node_from_tree_cfg(panel_tree.root, cfg_node_from_id(ws->panel_hoist_cfg_id));
      CFG_PanelNode *panel_trees[] = {front_panel, panel_tree.root};
      F32 panel_zoom_t = ui_anim(ui_key_from_stringf(ui_key_zero(), "panel_zoom_t"), (F32)(panel_tree.zoom_root != &cfg_nil_panel_node), .initial = (F32)(panel_tree.zoom_root != &cfg_nil_panel_node));
      ws->panel_rect_count = 0;
      ProfScope("leaf panel UI")
        for EachElement(panel_tree_idx, panel_trees)
        for(CFG_PanelNode *panel = panel_trees[panel_tree_idx];
            panel != &cfg_nil_panel_node;
            panel = cfg_panel_node_rec__depth_first_pre(panel_trees[panel_tree_idx], panel).next)
      {
        // rad: the second pass skips the front panel's whole subtree (raddbg
        // skips only the front panel itself, so the children of a zoomed
        // split would be built twice)
        if(panel_trees[panel_tree_idx] != front_panel && front_panel != &cfg_nil_panel_node)
        {
          B32 in_front_panel = 0;
          for(CFG_PanelNode *p = panel; p != &cfg_nil_panel_node; p = p->parent)
          {
            if(p == front_panel)
            {
              in_front_panel = 1;
              break;
            }
          }
          if(in_front_panel)
          {
            continue;
          }
        }
        B32 panel_is_focused = (window_is_focused &&
                                !ws->menu_bar_focused &&
                                !ui_any_ctx_menu_is_open() &&
                                !rad_state->quick_open_active &&
                                panel_tree.focused == panel);
        CFG_Node *selected_tab = panel->selected_tab;
        UI_Focus(panel_is_focused ? UI_FocusKind_Null : UI_FocusKind_Off)
          UI_Transparency(panel_trees[panel_tree_idx] != front_panel ? panel_zoom_t : 0)
        {
          //////////////////////////
          //- rad: calculate UI rectangles
          //
          Vec2F32 content_rect_dim = dim_2f32(content_rect);
          B32 is_in_zoom = 0;
          for(CFG_PanelNode *p = panel; p != &cfg_nil_panel_node; p = p->parent)
          {
            if(p == panel_tree.zoom_root)
            {
              is_in_zoom = 1;
              break;
            }
          }
          Rng2F32 target_rect_px = cfg_target_rect_from_panel_node(content_rect, is_in_zoom ? panel_tree.zoom_root : panel_tree.root, panel);
          if(panel == panel_tree.zoom_root)
          {
            target_rect_px = content_rect;
          }
          Rng2F32 target_rect_pct = r2f32p(target_rect_px.x0 / content_rect_dim.x,
                                           target_rect_px.y0 / content_rect_dim.y,
                                           target_rect_px.x1 / content_rect_dim.x,
                                           target_rect_px.y1 / content_rect_dim.y);
          Rng2F32 panel_rect_pct = r2f32p(ui_anim(ui_key_from_stringf(ui_key_zero(), "panel_%p_x0", panel->cfg), target_rect_pct.x0, .initial = target_rect_pct.x0, .rate = rad_state->menu_animation_rate),
                                          ui_anim(ui_key_from_stringf(ui_key_zero(), "panel_%p_y0", panel->cfg), target_rect_pct.y0, .initial = target_rect_pct.y0, .rate = rad_state->menu_animation_rate),
                                          ui_anim(ui_key_from_stringf(ui_key_zero(), "panel_%p_x1", panel->cfg), target_rect_pct.x1, .initial = target_rect_pct.x1, .rate = rad_state->menu_animation_rate),
                                          ui_anim(ui_key_from_stringf(ui_key_zero(), "panel_%p_y1", panel->cfg), target_rect_pct.y1, .initial = target_rect_pct.y1, .rate = rad_state->menu_animation_rate));
          Rng2F32 panel_rect = r2f32p(panel_rect_pct.x0*content_rect_dim.x,
                                      panel_rect_pct.y0*content_rect_dim.y,
                                      panel_rect_pct.x1*content_rect_dim.x,
                                      panel_rect_pct.y1*content_rect_dim.y);
          panel_rect = pad_2f32(panel_rect, floor_f32(-ui_top_font_size()*0.25f));
          panel_rect = r2f32p(round_f32(panel_rect.x0), round_f32(panel_rect.y0), round_f32(panel_rect.x1), round_f32(panel_rect.y1));
          F32 tab_bar_rheight = floor_f32(ui_top_font_size()*3.5f);
          F32 tab_bar_vheight = floor_f32(ui_top_font_size()*settings->tab_height);
          F32 tab_bar_rv_diff = tab_bar_rheight - tab_bar_vheight;
          Rng2F32 tab_bar_rect = r2f32p(panel_rect.x0, panel_rect.y0, panel_rect.x1, panel_rect.y0 + tab_bar_vheight);
          Rng2F32 content_rect = r2f32p(panel_rect.x0, panel_rect.y0+tab_bar_vheight, panel_rect.x1, panel_rect.y1);
          if(panel->tab_side == Side_Max)
          {
            tab_bar_rect.y0 = panel_rect.y1 - tab_bar_vheight;
            tab_bar_rect.y1 = panel_rect.y1;
            content_rect.y0 = panel_rect.y0;
            content_rect.y1 = panel_rect.y1 - tab_bar_vheight;
          }
          tab_bar_rect = intersect_2f32(tab_bar_rect, panel_rect);
          content_rect = intersect_2f32(content_rect, panel_rect);

          //////////////////////////
          //- rad: tool windows (not in raddbg): a header (title, X) on top;
          // no tab bar for the explorer (one tab); the bottom window's tabs
          // at its bottom (VS2022)
          //
          RAD_ToolKind tool_kind = (panel->first == &cfg_nil_panel_node ? rad_tool_kind_from_panel(panel->cfg) : RAD_ToolKind_Null);
          F32 tool_header_height = 0;
          Rng2F32 tool_header_rect = {0};
          if(tool_kind == RAD_ToolKind_Explorer)
          {
            tool_header_height = tab_bar_vheight;
            tool_header_rect = r2f32p(panel_rect.x0, panel_rect.y0, panel_rect.x1, panel_rect.y0 + tool_header_height);
            tab_bar_rect = r2f32p(0, 0, 0, 0);
            content_rect = r2f32p(panel_rect.x0, panel_rect.y0 + tool_header_height, panel_rect.x1, panel_rect.y1);
          }
          else if(tool_kind == RAD_ToolKind_Bottom)
          {
            tool_header_height = floor_f32(ui_top_font_size()*2.25f);
            tool_header_rect = r2f32p(panel_rect.x0, panel_rect.y0, panel_rect.x1, panel_rect.y0 + tool_header_height);
            content_rect.y0 = Min(content_rect.y1, tool_header_rect.y1);
          }
          tool_header_rect = intersect_2f32(tool_header_rect, panel_rect);
          content_rect = intersect_2f32(content_rect, panel_rect);

          //////////////////////////
          //- rad: not in raddbg: tab rows (setting Tab Overflow: Scroll -> one
          // row, scrolled by the wheel, "..." lists all tabs; Wrap -> more rows)
          //
          U64 tab_rows_count = 1;
          B32 tabs_overflow = 0;
          U64 *tab_rows = 0;
          if(panel->first == &cfg_nil_panel_node && tool_kind != RAD_ToolKind_Explorer && panel->tabs.count != 0)
          {
            B32 wrap = (settings->tab_overflow == 1 && tool_kind == RAD_ToolKind_Null && panel->tab_side == Side_Min);
            F32 spacing_px = floor_f32(ui_top_font_size()*0.4f);
            F32 avail_px = dim_2f32(tab_bar_rect).x - ui_top_font_size()*0.5f - tab_bar_vheight*(tool_kind == RAD_ToolKind_Null ? 2 : 0);
            tab_rows = push_array(scratch.arena, U64, panel->tabs.count);
            F32 x = 0;
            U64 row = 0;
            U64 idx = 0;
            for(CFG_NodePtrNode *n = panel->tabs.first; n != 0; n = n->next, idx += 1)
            {
              F32 w = rad_tab_width_target_px(scratch.arena, n->v, n->v == panel->selected_tab) + spacing_px;
              if(x + w > avail_px && x > 0)
              {
                if(wrap)
                {
                  row += 1;
                  x = 0;
                }
                else
                {
                  tabs_overflow = 1;
                }
              }
              x += w;
              tab_rows[idx] = row;
            }
            tab_rows_count = row+1;
            if(tab_rows_count > 1)
            {
              tab_bar_rect.y1 = Min(panel_rect.y1, tab_bar_rect.y0 + tab_bar_vheight*tab_rows_count);
              content_rect.y0 = Min(content_rect.y1, tab_bar_rect.y1);
            }
          }
          Rng2F32 tab_bar_full_rect = tab_bar_rect;
          if(tabs_overflow)
          {
            tab_bar_rect.x1 -= tab_bar_vheight*(tool_kind == RAD_ToolKind_Null ? 2 : 1);
          }
          if(panel == panel_tree.focused)
          {
            ws->tab_rows_count = tab_rows_count;
            ws->tabs_overflow = tabs_overflow;
          }

          //////////////////////////
          //- rad: for non-leaf panels with focus (subtree selection) -> draw border for all children
          //
          if(panel->first != &cfg_nil_panel_node && panel == panel_tree.focused) UI_Rect(pad_2f32(panel_rect, ui_top_font_size()*0.2f)) UI_Focus(UI_FocusKind_On)
          {
            ui_build_box_from_stringf(UI_BoxFlag_Clickable|UI_BoxFlag_DisableFocusOverlay, "###");
          }

          //////////////////////////
          //- rad: decide to skip this panel (e.g. if it non-leaf, or is too small)
          //
          B32 build_panel = ((panel->first == &cfg_nil_panel_node) && content_rect.x1 > content_rect.x0 && content_rect.y1 > content_rect.y0 && ui_top_transparency() < 0.99f);

          //////////////////////////
          //- rad: record leaf panel rects (scripted input aims drops with them)
          //
          if(build_panel && ws->panel_rect_count < ArrayCount(ws->panel_rects))
          {
            ws->panel_rects_font_size = ui_top_font_size();
            ws->panel_rect_ids[ws->panel_rect_count] = panel->cfg->id;
            ws->panel_rects[ws->panel_rect_count] = panel_rect;
            ws->panel_rect_count += 1;
          }

          //////////////////////////
          //- rad: build combined split+movetab drag/drop sites (port of
          // raddbg's; rad: all four sides, as VS2022's docking guide; not on
          // tool windows)
          //
          UI_Key catchall_drop_site_key = ui_key_from_stringf(ui_key_zero(), "catchall_drop_site_%p", panel->cfg);
          B32 tab_drag_here = (build_panel && tool_kind == RAD_ToolKind_Null && rad_drag_is_dockable());
          if(tab_drag_here && contains_2f32(panel_rect, ui_mouse()) && ui_key_match(ui_drop_hot_key(), ui_key_zero()))
          {
            F32 corner_radius = ui_top_font_size()*0.5f;
            F32 padding = ceil_f32(ui_top_font_size()*0.5f);
            Dir2 dirs[] = {Dir2_Invalid, Dir2_Up, Dir2_Down, Dir2_Left, Dir2_Right};
            UI_Key keys[ArrayCount(dirs)];
            UI_CornerRadius(corner_radius) for EachElement(idx, dirs)
            {
              Dir2 dir = dirs[idx];
              UI_Key key = keys[idx] = ui_key_from_stringf(ui_key_zero(), "drop_split_%i_%p", (int)dir, panel->cfg);
              Rng2F32 rect = rad_drop_site_rect(panel_rect, ui_top_font_size(), dir);
              Axis2 split_axis = axis2_from_dir2(dir);
              Side split_side = side_from_dir2(dir);
              UI_Box *site_box = &ui_nil_box;
              {
                F32 site_open_t = ui_anim(ui_key_from_stringf(key, "open_t"), 1.f, .rate = rad_state->menu_animation_rate);
                UI_Rect(rect) UI_Squish(0.1f-0.1f*site_open_t) UI_Transparency(1-site_open_t)
                {
                  site_box = ui_build_box_from_key(UI_BoxFlag_DropSite|UI_BoxFlag_DrawHotEffects, key);
                  ui_signal_from_box(site_box);
                }
                UI_Box *site_box_viz = &ui_nil_box;
                UI_GroupKey(key)
                  UI_Parent(site_box) UI_WidthFill UI_HeightFill
                  UI_Padding(ui_px(padding, 1.f))
                  UI_Column
                  UI_Padding(ui_px(padding, 1.f))
                {
                  ui_set_next_child_layout_axis(axis2_flip(split_axis));
                  site_box_viz = ui_build_box_from_key(UI_BoxFlag_DrawBackground|
                                                       UI_BoxFlag_DrawBorder|
                                                       UI_BoxFlag_DrawDropShadow|
                                                       UI_BoxFlag_DrawBackgroundBlur|
                                                       UI_BoxFlag_DrawHotEffects, ui_key_zero());
                }
                if(dir != Dir2_Invalid)
                {
                  UI_Parent(site_box_viz) UI_WidthFill UI_HeightFill UI_Padding(ui_px(padding, 1.f))
                  {
                    ui_set_next_child_layout_axis(split_axis);
                    UI_Box *row_or_column = ui_build_box_from_key(0, ui_key_zero());
                    UI_Parent(row_or_column) UI_Padding(ui_px(padding, 1.f)) UI_TagF("drop_site")
                    {
                      if(split_side == Side_Min) { ui_set_next_flags(UI_BoxFlag_DrawBackground); }
                      ui_build_box_from_key(UI_BoxFlag_DrawBorder, ui_key_zero());
                      ui_spacer(ui_px(padding, 1.f));
                      if(split_side == Side_Max) { ui_set_next_flags(UI_BoxFlag_DrawBackground); }
                      ui_build_box_from_key(UI_BoxFlag_DrawBorder, ui_key_zero());
                    }
                  }
                }
                else
                {
                  UI_Parent(site_box_viz) UI_WidthFill UI_HeightFill UI_Padding(ui_px(padding, 1.f))
                  {
                    ui_set_next_child_layout_axis(split_axis);
                    UI_Box *row_or_column = ui_build_box_from_key(0, ui_key_zero());
                    UI_Parent(row_or_column) UI_Padding(ui_px(padding, 1.f)) UI_TagF("drop_site")
                    {
                      ui_build_box_from_key(UI_BoxFlag_DrawBorder|UI_BoxFlag_DrawBackground, ui_key_zero());
                    }
                  }
                }
              }
              if(ui_key_match(site_box->key, ui_drop_hot_key()) && rad_drag_drop())
              {
                if(dir != Dir2_Invalid)
                {
                  rad_drop_split(panel->cfg, dir);
                }
                else if(rad_state->drag_kind == RAD_DragKind_FilePath)
                {
                  // rad: a file from the explorer: open it there
                  rad_cmd(RAD_CmdKind_OpenFile, .panel = panel->cfg->id, .file_path = str8(rad_state->drag_path_buffer, rad_state->drag_path_size));
                }
                else
                {
                  rad_cmd(RAD_CmdKind_MoveTab,
                          .dst_panel = panel->cfg->id,
                          .panel = rad_state->drag_panel,
                          .tab = rad_state->drag_tab,
                          .prev_tab = cfg_node_ptr_list_last(&panel->tabs)->id,
                          .all_tabs = (rad_state->drag_kind == RAD_DragKind_Panel));
                }
              }
            }

            // rad: the future rect of the hot site (raddbg's animated preview)
            for EachElement(idx, dirs)
            {
              if(ui_key_match(ui_drop_hot_key(), keys[idx]))
              {
                Axis2 split_axis = axis2_from_dir2(dirs[idx]);
                Side split_side = side_from_dir2(dirs[idx]);
                Rng2F32 future_split_rect_target = panel_rect;
                if(dirs[idx] != Dir2_Invalid)
                {
                  Vec2F32 panel_center = center_2f32(panel_rect);
                  future_split_rect_target.v[side_flip(split_side)].v[split_axis] = panel_center.v[split_axis];
                }
                future_split_rect_target = pad_2f32(future_split_rect_target, -ui_top_font_size()*2.f);
                Vec2F32 future_split_rect_target_center = center_2f32(future_split_rect_target);
                Rng2F32 future_split_rect =
                {
                  ui_anim(ui_key_from_stringf(ui_key_zero(), "drop_site_v0"), future_split_rect_target.x0, .initial = future_split_rect_target_center.x, .rate = rad_state->menu_animation_rate),
                  ui_anim(ui_key_from_stringf(ui_key_zero(), "drop_site_v1"), future_split_rect_target.y0, .initial = future_split_rect_target_center.y, .rate = rad_state->menu_animation_rate),
                  ui_anim(ui_key_from_stringf(ui_key_zero(), "drop_site_v2"), future_split_rect_target.x1, .initial = future_split_rect_target_center.x, .rate = rad_state->menu_animation_rate),
                  ui_anim(ui_key_from_stringf(ui_key_zero(), "drop_site_v3"), future_split_rect_target.y1, .initial = future_split_rect_target_center.y, .rate = rad_state->menu_animation_rate),
                };
                UI_Rect(future_split_rect) UI_TagF("drop_site") UI_CornerRadius(ui_top_font_size()*2.f)
                {
                  ui_build_box_from_key(UI_BoxFlag_DrawBackground|UI_BoxFlag_DrawBorder, ui_key_zero());
                }
              }
            }
          }

          //////////////////////////
          //- rad: build catch-all panel drop-site (raddbg's: a tab dropped
          // elsewhere in the panel goes into its tab bar, where the mouse is)
          //
          if(tab_drag_here) UI_Rect(panel_rect)
          {
            UI_Box *catchall_drop_site = ui_build_box_from_key(UI_BoxFlag_DropSite, catchall_drop_site_key);
            ui_signal_from_box(catchall_drop_site);
          }

          //////////////////////////
          //- rad: panel not selected? -> darken
          //
          if(build_panel) if(panel != panel_tree.focused)
          {
            UI_Rect(content_rect) UI_TagF("inactive")
              ui_build_box_from_key(UI_BoxFlag_DrawBackground, ui_key_zero());
          }

          //////////////////////////
          //- rad: determine how much this panel is occluding others
          //
          B32 is_occluding = 0;
          if(panel_trees[panel_tree_idx] == front_panel)
          {
            is_occluding = (abs_f32(panel_rect.x0 - target_rect_px.x0) > 2 ||
                            abs_f32(panel_rect.y0 - target_rect_px.y0) > 2);
          }

          //////////////////////////
          //- rad: build panel container box
          //
          UI_Box *panel_box = &ui_nil_box;
          if(build_panel) UI_Rect(content_rect) UI_ChildLayoutAxis(Axis2_Y) UI_CornerRadius(0) UI_Focus(UI_FocusKind_On)
          {
            UI_Key panel_key = ui_key_from_stringf(ui_key_zero(), "panel_box_%p", panel->cfg);
            panel_box = ui_build_box_from_key(UI_BoxFlag_MouseClickable|
                                              UI_BoxFlag_Clip|
                                              UI_BoxFlag_DrawBorder|
                                              UI_BoxFlag_DisableFocusOverlay|
                                              (is_occluding*UI_BoxFlag_DrawBackground)|
                                              ((panel_tree.focused != panel)*UI_BoxFlag_DisableFocusBorder),
                                              panel_key);
          }

          //////////////////////////
          //- rad: loading animation for stable view
          //
          UI_Box *loading_overlay_container = &ui_nil_box;
          if(build_panel) UI_Parent(panel_box) UI_WidthFill UI_HeightFill
          {
            loading_overlay_container = ui_build_box_from_key(UI_BoxFlag_Floating, ui_key_zero());
          }

          //////////////////////////
          //- rad: build selected tab view
          //
          if(build_panel)
            UI_Parent(panel_box)
            UI_Focus(panel_is_focused ? UI_FocusKind_Null : UI_FocusKind_Off)
            UI_WidthFill
          {
            //- rad: build view container
            UI_Box *view_container_box = &ui_nil_box;
            UI_FixedWidth(dim_2f32(content_rect).x)
              UI_FixedHeight(dim_2f32(content_rect).y)
              UI_ChildLayoutAxis(Axis2_Y)
            {
              view_container_box = ui_build_box_from_key(0, ui_key_zero());
            }

            //- rad: build empty view
            UI_Parent(view_container_box) if(selected_tab == &cfg_nil_node && panel->parent != &cfg_nil_panel_node)
            {
              ui_set_next_flags(UI_BoxFlag_DefaultFocusNav);
              UI_Focus(UI_FocusKind_On) UI_WidthFill UI_HeightFill UI_NamedColumn(str8_lit("empty_view")) UI_TagF("weak")
                UI_Padding(ui_pct(1, 0)) UI_Focus(UI_FocusKind_Null)
              {
                UI_PrefHeight(ui_em(3.f, 1.f))
                  UI_Row
                  UI_Padding(ui_pct(1, 0))
                  UI_TextAlignment(UI_TextAlign_Center)
                  UI_PrefWidth(ui_em(15.f, 1.f))
                  UI_CornerRadius(ui_top_font_size()/2.f)
                  UI_TagF("bad_pop")
                {
                  if(ui_clicked(rad_icon_buttonf(RAD_IconKind_X, 0, "Close Panel")))
                  {
                    rad_cmd(RAD_CmdKind_ClosePanel, .panel = panel->cfg->id);
                  }
                }
              }
            }

            //- rad: build tab view
            UI_Parent(view_container_box) if(selected_tab != &cfg_nil_node) ProfScope("build tab view")
            {
              rad_view_ui(selected_tab, content_rect);
            }
          }

          //////////////////////////
          //- rad: loading? -> fill loading overlay container
          //
          if(build_panel && selected_tab != &cfg_nil_node)
          {
            RAD_ViewState *selected_tab_view_state = rad_view_state_from_cfg(selected_tab);
            F32 selected_tab_loading_t = selected_tab_view_state->loading_t;
            if(selected_tab_loading_t > 0.01f) UI_Parent(loading_overlay_container)
            {
              rad_loading_overlay(panel_rect, selected_tab_loading_t, selected_tab_view_state->loading_progress_v, selected_tab_view_state->loading_progress_v_target);
            }
          }

          //////////////////////////
          //- rad: consume panel fallthrough interaction events
          //
          if(build_panel)
          {
            UI_Signal panel_sig = ui_signal_from_box(panel_box);
            if(ui_pressed(panel_sig))
            {
              rad_cmd(RAD_CmdKind_FocusPanel, .panel = panel->cfg->id);
            }
          }

          //////////////////////////
          //- rad: build zoom button (same as raddbg; not on tool windows)
          //
          if(panel_is_focused && tool_kind == RAD_ToolKind_Null)
          {
            UI_TextAlignment(UI_TextAlign_Center)
              UI_TagF(".")
            {
              ui_set_next_child_layout_axis(Axis2_Y);
              UI_Box *container = &ui_nil_box;
              UI_Rect(r2f32p(tab_bar_full_rect.x1 - tab_bar_vheight, tab_bar_full_rect.y0, tab_bar_full_rect.x1, tab_bar_full_rect.y0 + tab_bar_vheight))
              {
                container = ui_build_box_from_stringf(0, "###zoom_panel");
              }
              UI_Parent(container)
              {
                if(panel->tab_side == Side_Max)
                {
                  ui_spacer(ui_px(tab_bar_rv_diff-1.f, 1.f));
                }
                else
                {
                  ui_spacer(ui_px(1.f, 1.f));
                }
                {
                  UI_Box *zoom_box = &ui_nil_box;
                  RAD_Font(RAD_FontSlot_Icons)
                    UI_CornerRadius((tab_bar_vheight - tab_bar_vheight/4.f) / 3.f)
                    UI_VisualMargin(tab_bar_vheight/5.f)
                    UI_TagF("implicit")
                    UI_FontSize(ui_top_font_size()*0.65f)
                  {
                    zoom_box = ui_build_box_from_stringf(UI_BoxFlag_DrawText|
                                                         UI_BoxFlag_DrawBorder|
                                                         UI_BoxFlag_DrawBackground|
                                                         UI_BoxFlag_DrawHotEffects|
                                                         UI_BoxFlag_DrawActiveEffects|
                                                         UI_BoxFlag_Clickable|
                                                         UI_BoxFlag_DisableTextTrunc,
                                                         "%S##zoom_panel_button_%p",
                                                         rad_icon_kind_text_table[panel_tree.zoom_root == panel ? RAD_IconKind_WindowRestore : RAD_IconKind_Window],
                                                         panel->cfg);
                  }
                  UI_Signal sig = ui_signal_from_box(zoom_box);
                  if(ui_pressed(sig))
                  {
                    rad_cmd(RAD_CmdKind_ToggleZoomPanel, .panel = panel->cfg->id);
                  }
                  if(ui_hovering(sig)) UI_Tooltip
                  {
                    // NOTE(rad): raddbg adds the command's binding buttons;
                    // the command has no default binding, so there are none
                    ui_state->tooltip_anchor_key = zoom_box->key;
                    ui_set_next_pref_width(ui_children_sum(1));
                    UI_Row
                    {
                      ui_labelf("Toggle Zoom Panel");
                    }
                  }
                }
              }
            }
          }

          //////////////////////////
          //- rad: build tool window header (title; X hides the window, in the
          // zoom button's style)
          //
          if(build_panel && tool_kind != RAD_ToolKind_Null && tool_header_rect.y1 > tool_header_rect.y0)
          {
            CFG_Node *title_tab = (selected_tab != &cfg_nil_node ? selected_tab : panel->tabs.first ? panel->tabs.first->v : &cfg_nil_node);
            DR_FStrList title_fstrs = rad_title_fstrs_from_tab(scratch.arena, title_tab);
            UI_Box *header_box = &ui_nil_box;
            UI_Rect(tool_header_rect) UI_ChildLayoutAxis(Axis2_X) UI_CornerRadius(0)
            {
              header_box = ui_build_box_from_stringf(UI_BoxFlag_Clickable, "tool_header_%p", panel->cfg);
            }
            UI_Parent(header_box) UI_Focus(UI_FocusKind_Off) UI_PrefHeight(ui_pct(1, 0))
            {
              ui_spacer(ui_em(0.75f, 1.f));
              UI_TagF(panel_tree.focused == panel ? "" : "weak") UI_PrefWidth(ui_text_dim(10, 1))
              {
                UI_Box *name_box = ui_build_box_from_key(UI_BoxFlag_DrawText, ui_key_zero());
                ui_box_equip_display_fstrs(name_box, &title_fstrs);
              }
              ui_spacer(ui_pct(1, 0));
              UI_PrefWidth(ui_px(tool_header_height, 1.f))
                UI_TextAlignment(UI_TextAlign_Center)
                UI_TagF(".")
              {
                UI_Box *close_box = &ui_nil_box;
                RAD_Font(RAD_FontSlot_Icons)
                  UI_CornerRadius((tool_header_height - tool_header_height/4.f) / 3.f)
                  UI_VisualMargin(tool_header_height/5.f)
                  UI_TagF("implicit")
                  UI_FontSize(ui_top_font_size()*0.65f)
                {
                  close_box = ui_build_box_from_stringf(UI_BoxFlag_DrawText|
                                                        UI_BoxFlag_DrawBorder|
                                                        UI_BoxFlag_DrawBackground|
                                                        UI_BoxFlag_DrawHotEffects|
                                                        UI_BoxFlag_DrawActiveEffects|
                                                        UI_BoxFlag_Clickable|
                                                        UI_BoxFlag_DisableTextTrunc,
                                                        "%S##tool_close_%p",
                                                        rad_icon_kind_text_table[RAD_IconKind_X],
                                                        panel->cfg);
                }
                ws->tool_close_rects[tool_kind] = close_box->rect;
                UI_Signal sig = ui_signal_from_box(close_box);
                if(ui_clicked(sig))
                {
                  rad_cmd(RAD_CmdKind_CloseToolWindow, .panel = panel->cfg->id);
                }
                if(ui_hovering(sig)) UI_Tooltip
                {
                  ui_state->tooltip_anchor_key = close_box->key;
                  ui_set_next_pref_width(ui_children_sum(1));
                  UI_Row
                  {
                    ui_label(rad_cmd_kind_info_table[RAD_CmdKind_CloseToolWindow].display_name);
                    rad_cmd_binding_labels(RAD_CmdKind_CloseToolWindow, 0);
                  }
                }
              }
            }
            if(ui_pressed(ui_signal_from_box(header_box)))
            {
              rad_cmd(RAD_CmdKind_FocusPanel, .panel = panel->cfg->id);
            }
          }

          //////////////////////////
          //- rad: compute tab build tasks
          //
          typedef struct TabTask TabTask;
          struct TabTask
          {
            TabTask *next;
            CFG_Node *tab;
            DR_FStrList fstrs;
            F32 tab_width;
            U64 row;
          };
          TabTask *first_tab_task = 0;
          TabTask *last_tab_task = 0;
          U64 tab_task_count = 0;
          UI_TagF("tab")
          {
            B32 reset = (ws->window_layout_reset || ws->frames_alive < 5 || is_changing_panel_boundaries);
            U64 tab_idx = 0;
            for(CFG_NodePtrNode *n = panel->tabs.first; n != 0; n = n->next, tab_idx += 1)
            {
              CFG_Node *tab = n->v;
              if(str8_match(tab->string, str8_lit("text"), 0) ||
                 str8_match(tab->string, str8_lit("explorer"), 0))
              {
                rad_view_state_from_cfg(tab);
              }
              if(str8_match(tab->string, str8_lit("text"), 0))
              {
                // rad: keep the tab's document open (ticked even while hidden)
                rad_document_from_path(rad_file_path_from_tab(tab));
              }
              UI_TagF(tab != panel->selected_tab ? "inactive" : "")
              {
                DR_FStrList tab_title_fstrs = rad_title_fstrs_from_tab_flags(scratch.arena, tab, RAD_TabTitleFlag_TabBar);
                B32 tab_is_selected = (tab == panel->selected_tab);
                F32 tab_width_target = rad_tab_width_target_px(scratch.arena, tab, tab_is_selected);
                F32 tab_width = floor_f32(ui_anim(ui_key_from_stringf(ui_key_zero(), "tab_width_%p", tab), tab_width_target, .initial = reset ? tab_width_target : 0, .rate = rad_state->menu_animation_rate));
                if(build_panel)
                {
                  TabTask *t = push_array(scratch.arena, TabTask, 1);
                  t->tab = tab;
                  t->fstrs = tab_title_fstrs;
                  t->tab_width = tab_width;
                  t->row = (tab_rows != 0 ? tab_rows[tab_idx] : 0);
                  SLLQueuePush(first_tab_task, last_tab_task, t);
                  tab_task_count += 1;
                }
              }
            }
          }

          // rad: one tab bar per row (rows: Tab Overflow = Wrap)
          for(U64 tab_row = 0; tab_row < tab_rows_count; tab_row += 1)
          {
          Rng2F32 tab_row_rect = tab_bar_rect;
          if(tab_rows_count > 1)
          {
            tab_row_rect.y0 = tab_bar_rect.y0 + tab_bar_vheight*tab_row;
            tab_row_rect.y1 = tab_row_rect.y0 + tab_bar_vheight;
          }

          //////////////////////////
          //- rad: build tab bar container
          //
          UI_Box *tab_bar_box = &ui_nil_box;
          if(build_panel && tool_kind != RAD_ToolKind_Explorer) UI_CornerRadius(0) UI_Rect(tab_row_rect)
          {
            tab_bar_box = ui_build_box_from_stringf(UI_BoxFlag_Clip|
                                                    UI_BoxFlag_AllowOverflowY|
                                                    UI_BoxFlag_ViewClampX|
                                                    UI_BoxFlag_ViewScrollX|
                                                    UI_BoxFlag_Clickable,
                                                    "tab_bar_%p_%I64u", panel->cfg, tab_row);
            if(panel->tab_side == Side_Max)
            {
              tab_bar_box->view_off.y = tab_bar_box->view_off_target.y = (tab_bar_rheight - tab_bar_vheight);
            }
            else
            {
              tab_bar_box->view_off.y = tab_bar_box->view_off_target.y = 0;
            }
          }

          //////////////////////////
          //- rad: build tab bar contents
          //
          if(build_panel && tool_kind != RAD_ToolKind_Explorer) UI_Focus(UI_FocusKind_Off) UI_Parent(tab_bar_box) UI_PrefHeight(ui_pct(1, 0)) UI_TagF("tab")
          {
            F32 corner_radius = ui_top_font_size()*0.6f;
            UI_CornerRadius00(panel->tab_side == Side_Min ? corner_radius : 0)
              UI_CornerRadius01(panel->tab_side == Side_Min ? 0 : corner_radius)
              UI_CornerRadius10(panel->tab_side == Side_Min ? corner_radius : 0)
              UI_CornerRadius11(panel->tab_side == Side_Min ? 0 : corner_radius)
              for(TabTask *tab_task = first_tab_task; tab_task != 0; tab_task = tab_task->next)
            {
              CFG_Node *tab = tab_task->tab;
              if(tab_task->row != tab_row)
              {
                continue;
              }

              //- rad: build tab
              DR_FStrList tab_fstrs = tab_task->fstrs;
              F32 tab_width_px = tab_task->tab_width;
              {
                // rad: gather info for this tab
                B32 tab_is_selected = (tab == panel->selected_tab);

                // rad: begin vertical region for this tab
                ui_set_next_child_layout_axis(Axis2_Y);
                ui_set_next_pref_width(ui_px(tab_width_px, 1));
                UI_Box *tab_column_box = ui_build_box_from_stringf(!is_changing_panel_boundaries*UI_BoxFlag_AnimatePosX, "tab_column_%p", tab);

                // rad: choose palette (raddbg's: the dragged tab is hollow)
                B32 omit_name = (rad_drag_is_active() &&
                                 ((rad_state->drag_kind == RAD_DragKind_Tab && rad_state->drag_tab == tab->id) ||
                                  (rad_state->drag_kind == RAD_DragKind_Panel && rad_state->drag_panel == panel->cfg->id)));

                // rad: build tab container box
                UI_Parent(tab_column_box)
                  UI_PrefHeight(ui_px(tab_bar_vheight, 1))
                  UI_TagF(omit_name ? "hollow" : "")
                  UI_TagF(!omit_name && !tab_is_selected ? "inactive" : "")
                {
                  if(panel->tab_side == Side_Max)
                  {
                    ui_spacer(ui_px(tab_bar_rv_diff-1.f, 1.f));
                  }
                  else
                  {
                    ui_spacer(ui_px(1.f, 1.f));
                  }
                  UI_Box *tab_box = ui_build_box_from_stringf(UI_BoxFlag_DrawHotEffects|
                                                              UI_BoxFlag_DrawBackground|
                                                              UI_BoxFlag_DrawBorder|
                                                              (UI_BoxFlag_DrawDropShadow*tab_is_selected)|
                                                              UI_BoxFlag_Clickable,
                                                              "tab_%p", tab);

                  // rad: build tab contents
                  if(!omit_name) UI_Parent(tab_box)
                  {
                    UI_WidthFill UI_Row
                    {
                      ui_spacer(ui_px(rad_tab_pad_px(), 1.f));
                      UI_PrefWidth(ui_text_dim(0, 0)) UI_TextPadding(0)
                      {
                        UI_Box *name_box = ui_build_box_from_key(UI_BoxFlag_DrawText, ui_key_zero());
                        ui_box_equip_display_fstrs(name_box, &tab_fstrs);
                      }
                    }
                    // rad: not in raddbg: pin and close buttons (TODO_2 B2, compact
                    // like Zed's tabs): shown while the tab is hovered; a pinned
                    // tab keeps its pin; a modified file shows a dot in the close
                    // button's place; Tab Close Button = Always keeps every close
                    // button. Their room is kept, and they stay clickable (the
                    // mouse over one hovers the tab, which shows them)
                    RAD_ViewState *tab_vs = rad_view_state_from_cfg(tab);
                    B32 tab_is_pinned = rad_tab_is_pinned(tab);
                    B32 tab_is_hovered = (contains_2f32(tab_box->rect, ui_mouse()) && !rad_drag_is_active());
                    B32 tab_is_dirty = rad_tab_is_dirty(tab);
                    tab_vs->layout_tab_frame = rad_state->frame_index;
                    tab_vs->layout_tab_rect = tab_box->rect;
                    MemoryZeroStruct(&tab_vs->layout_tab_pin_rect);
                    MemoryZeroStruct(&tab_vs->layout_tab_close_rect);
                    F32 button_width_px = rad_tab_button_width_px();
                    F32 button_margin_y = floor_f32((tab_bar_vheight - button_width_px)/2.f);
                    if(!rad_tab_is_tool(tab))
                      UI_PrefWidth(ui_px(button_width_px, 1.f))
                      UI_TextAlignment(UI_TextAlign_Center)
                      RAD_Font(RAD_FontSlot_Icons)
                      UI_FontSize(ui_top_font_size()*0.7f)
                      UI_TagF(".") UI_TagF("tab") UI_TagF(tab_is_pinned ? "" : "weak") UI_TagF("implicit")
                      UI_VisualMarginX(ceil_f32(ui_top_font_size()*0.1f))
                      UI_VisualMarginY(button_margin_y)
                      UI_CornerRadius(ui_top_font_size()*0.35f)
                    {
                      B32 show_pin = (tab_is_pinned || tab_is_hovered);
                      UI_Box *pin_box = ui_build_box_from_stringf(UI_BoxFlag_Clickable|
                                                                  show_pin*(UI_BoxFlag_DrawText|
                                                                            UI_BoxFlag_DrawHotEffects|
                                                                            UI_BoxFlag_DrawActiveEffects),
                                                                  "%S###pin_view_%p", rad_icon_kind_text_table[RAD_IconKind_Pin], tab);
                      tab_vs->layout_tab_pin_rect = pin_box->rect;
                      UI_Signal sig = ui_signal_from_box(pin_box);
                      if(ui_clicked(sig))
                      {
                        rad_cmd(RAD_CmdKind_TogglePinTab, .panel = panel->cfg->id, .tab = tab->id);
                      }
                      if(ui_hovering(sig)) UI_Tooltip RAD_Font(RAD_FontSlot_Main) UI_FontSize(rad_state->settings.font_size)
                      {
                        ui_state->tooltip_anchor_key = pin_box->key;
                        ui_label(tab_is_pinned ? str8_lit("Unpin Tab") : str8_lit("Pin Tab"));
                      }
                    }
                    if(!rad_tab_is_tool(tab))
                    {
                      B32 show_close = (tab_is_hovered || rad_state->settings.tab_close_button == 1);
                      B32 show_dot = (!show_close && tab_is_dirty);
                      UI_PrefWidth(ui_px(button_width_px, 1.f))
                        UI_TextAlignment(UI_TextAlign_Center)
                        RAD_Font(RAD_FontSlot_Icons)
                        UI_FontSize(ui_top_font_size()*(show_dot ? 0.5f : 0.7f))
                        UI_TagF(".") UI_TagF("tab") UI_TagF("weak") UI_TagF("implicit")
                        UI_VisualMarginX(ceil_f32(ui_top_font_size()*0.1f))
                        UI_VisualMarginY(button_margin_y)
                        UI_CornerRadius(ui_top_font_size()*0.35f)
                      {
                        UI_Box *close_box = ui_build_box_from_stringf(UI_BoxFlag_Clickable|
                                                                      (show_close || show_dot)*UI_BoxFlag_DrawText|
                                                                      show_close*(UI_BoxFlag_DrawHotEffects|
                                                                                  UI_BoxFlag_DrawActiveEffects),
                                                                      "%S###close_view_%p",
                                                                      rad_icon_kind_text_table[show_dot ? RAD_IconKind_CircleFilled : RAD_IconKind_X], tab);
                        tab_vs->layout_tab_close_rect = close_box->rect;
                        UI_Signal sig = ui_signal_from_box(close_box);
                        if(ui_clicked(sig) || ui_middle_clicked(sig))
                        {
                          rad_cmd(RAD_CmdKind_CloseTab, .panel = panel->cfg->id, .tab = tab->id);
                        }
                      }
                      ui_spacer(ui_px(rad_tab_end_pad_px(), 1.f));
                    }
                  }

                  // rad: consume events for tab clicking
                  {
                    UI_Signal sig = ui_signal_from_box(tab_box);
                    if(ui_pressed(sig))
                    {
                      rad_cmd(RAD_CmdKind_FocusTab, .panel = panel->cfg->id, .tab = tab->id);
                      rad_cmd(RAD_CmdKind_FocusPanel, .panel = panel->cfg->id);
                    }
                    else if(ui_dragging(sig) && !rad_drag_is_active() && length_2f32(ui_drag_delta()) > 10.f && !rad_tab_is_tool(tab))
                    {
                      // rad: port of raddbg's tab drag (tool tabs stay)
                      rad_drag_begin_tab(panel->cfg, tab);
                    }
                    else if(ui_middle_clicked(sig))
                    {
                      rad_cmd(RAD_CmdKind_CloseTab, .panel = panel->cfg->id, .tab = tab->id);
                    }
                  }
                }

                // rad: space for next tab
                {
                  ui_spacer(ui_px(floor_f32(ui_top_font_size()*0.4f), 1.f));
                }
              }
            }

            // rad: build add-new-tab button (not on tool windows; last row)
            if(tool_kind == RAD_ToolKind_Null && tab_row+1 == tab_rows_count)
              UI_TextAlignment(UI_TextAlign_Center)
              UI_PrefWidth(ui_px(tab_bar_vheight, 1.f))
              UI_PrefHeight(ui_px(tab_bar_vheight, 1.f))
              UI_TagF(".")
            {
              ui_set_next_child_layout_axis(Axis2_Y);
              UI_Box *container = ui_build_box_from_stringf(!is_changing_panel_boundaries*UI_BoxFlag_AnimatePosX, "###add_new_tab");
              UI_Parent(container)
              {
                if(panel->tab_side == Side_Max)
                {
                  ui_spacer(ui_px(tab_bar_rv_diff-1.f, 1.f));
                }
                else
                {
                  ui_spacer(ui_px(1.f, 1.f));
                }
                {
                  UI_Box *add_new_box = &ui_nil_box;
                  RAD_Font(RAD_FontSlot_Icons)
                    UI_CornerRadius((tab_bar_vheight - tab_bar_vheight/4.f) / 3.f)
                    UI_VisualMargin(tab_bar_vheight/5.f)
                    UI_TagF("implicit")
                    UI_TagF("weak")
                  {
                    add_new_box = ui_build_box_from_stringf(UI_BoxFlag_DrawText|
                                                            UI_BoxFlag_DrawBorder|
                                                            UI_BoxFlag_DrawBackground|
                                                            UI_BoxFlag_DrawHotEffects|
                                                            UI_BoxFlag_DrawActiveEffects|
                                                            UI_BoxFlag_Clickable|
                                                            UI_BoxFlag_DisableTextTrunc,
                                                            "%S##add_new_tab_button_%p",
                                                            rad_icon_kind_text_table[RAD_IconKind_Add],
                                                            panel->cfg);
                  }
                  // rad: explorer panels add folders, other panels add files
                  CFG_Node *explorer_tab = &cfg_nil_node;
                  for(CFG_NodePtrNode *n = panel->tabs.first; n != 0; n = n->next)
                  {
                    if(str8_match(n->v->string, str8_lit("explorer"), 0))
                    {
                      explorer_tab = n->v;
                      break;
                    }
                  }
                  RAD_CmdKind add_cmd_kind = (explorer_tab != &cfg_nil_node ? RAD_CmdKind_OpenFolder : RAD_CmdKind_OpenFile);
                  UI_Signal sig = ui_signal_from_box(add_new_box);
                  if(ui_pressed(sig))
                  {
                    rad_cmd(RAD_CmdKind_FocusPanel, .panel = panel->cfg->id);
                    rad_cmd(add_cmd_kind, .panel = panel->cfg->id, .tab = explorer_tab->id);
                  }
                  if(ui_hovering(sig)) UI_Tooltip
                  {
                    ui_state->tooltip_anchor_key = add_new_box->key;
                    ui_set_next_pref_width(ui_children_sum(1));
                    UI_Row
                    {
                      ui_label(rad_cmd_kind_info_table[add_cmd_kind].display_name);
                      rad_cmd_binding_labels(add_cmd_kind, 0);
                    }
                  }
                }
              }
            }

            // rad: interact with tab bar (rad: the wheel scrolls it sideways)
            for(UI_Event *evt = 0; ui_next_event(&evt);)
            {
              if(evt->kind == UI_EventKind_Scroll && evt->modifiers == 0 && evt->delta_2f32.x == 0 && contains_2f32(tab_row_rect, evt->pos))
              {
                evt->delta_2f32 = v2f32(evt->delta_2f32.y, 0);
              }
            }
            UI_Signal tab_bar_sig = ui_signal_from_box(tab_bar_box);

            // rad: not in raddbg (VS2022): dragging a tab bar's free space
            // drags the whole panel (all its tabs)
            if(tool_kind == RAD_ToolKind_Null && ui_dragging(tab_bar_sig) && !rad_drag_is_active() && length_2f32(ui_drag_delta()) > 10.f)
            {
              rad_drag_begin_panel(panel->cfg, panel->selected_tab);
            }
          }
          }

          //////////////////////////
          //- rad: accept tab drops (raddbg's: the catch-all site is hot -> the
          // tab goes after the tab whose end is nearest the mouse)
          //
          if(tab_drag_here && ui_key_match(ui_drop_hot_key(), catchall_drop_site_key) && rad_drag_drop())
          {
            if(rad_state->drag_kind == RAD_DragKind_FilePath)
            {
              String8 path = str8(rad_state->drag_path_buffer, rad_state->drag_path_size);
              rad_cmd(RAD_CmdKind_OpenFile, .panel = panel->cfg->id, .file_path = path);
            }
            else
            {
              CFG_Node *drop_prev = &cfg_nil_node;
              F32 best_distance_px = 1000000.f;
              F32 off = tab_bar_rect.x0; // rad: the first tab is flush with the panel (TODO_2 B1)
              {
                F32 distance = abs_f32(ui_mouse().x - off);
                best_distance_px = distance;
              }
              for(TabTask *task = first_tab_task; task != 0; task = task->next)
              {
                if(task->row != 0) { break; }
                off += task->tab_width + floor_f32(ui_top_font_size()*0.4f);
                F32 distance = abs_f32(ui_mouse().x - off);
                if(distance < best_distance_px)
                {
                  best_distance_px = distance;
                  drop_prev = task->tab;
                }
              }
              rad_cmd(RAD_CmdKind_MoveTab,
                      .dst_panel = panel->cfg->id,
                      .panel = rad_state->drag_panel,
                      .tab = rad_state->drag_tab,
                      .prev_tab = drop_prev->id,
                      .all_tabs = (rad_state->drag_kind == RAD_DragKind_Panel));
            }
          }

          //////////////////////////
          //- rad: not in raddbg: "..." -> a list of all tabs (Tab Overflow =
          // Scroll, when they do not fit); picking one brings it to the front
          // (VS2022)
          //
          if(build_panel && tabs_overflow)
          {
            UI_Key tab_list_menu_key = ui_key_from_stringf(ui_key_zero(), "_tab_list_menu_%p", panel->cfg);
            UI_Box *list_box = &ui_nil_box;
            UI_Box *container = &ui_nil_box;
            UI_Rect(r2f32p(tab_bar_rect.x1, tab_bar_full_rect.y0, tab_bar_rect.x1 + tab_bar_vheight, tab_bar_full_rect.y0 + tab_bar_vheight))
            {
              container = ui_build_box_from_stringf(0, "###tab_list_%p", panel->cfg);
            }
            UI_TextAlignment(UI_TextAlign_Center) UI_TagF(".")
            {
              UI_Parent(container) UI_PrefWidth(ui_pct(1, 0)) UI_PrefHeight(ui_pct(1, 0))
                RAD_Font(RAD_FontSlot_Icons)
                UI_CornerRadius((tab_bar_vheight - tab_bar_vheight/4.f) / 3.f)
                UI_VisualMargin(tab_bar_vheight/5.f)
                UI_TagF("implicit")
                UI_TagF("weak")
              {
                list_box = ui_build_box_from_stringf(UI_BoxFlag_DrawText|
                                                     UI_BoxFlag_DrawBorder|
                                                     UI_BoxFlag_DrawBackground|
                                                     UI_BoxFlag_DrawHotEffects|
                                                     UI_BoxFlag_DrawActiveEffects|
                                                     UI_BoxFlag_Clickable|
                                                     UI_BoxFlag_DisableTextTrunc,
                                                     "%S##tab_list_button_%p", rad_icon_kind_text_table[RAD_IconKind_DownCaret], panel->cfg);
              }
            }
            UI_Signal sig = ui_signal_from_box(list_box);
            if(panel == panel_tree.focused || ws->tab_list_frame+1 < rad_state->frame_index)
            {
              ws->tab_list_frame = rad_state->frame_index;
              ws->tab_list_button_rect = list_box->rect;
            }
            if(ui_pressed(sig))
            {
              ui_ctx_menu_open(tab_list_menu_key, list_box->key, v2f32(0, dim_2f32(list_box->rect).y));
            }
            if(ui_hovering(sig)) UI_Tooltip
            {
              ui_state->tooltip_anchor_key = list_box->key;
              ui_label(str8_lit("All Tabs"));
            }
            UI_CtxMenu(tab_list_menu_key) UI_PrefWidth(ui_em(30.f, 1.f)) UI_TagF("implicit")
            {
              ws->tab_list_menu_frame = rad_state->frame_index;
              ws->tab_list_item_count = 0;
              U64 item_idx = 0;
              for(CFG_NodePtrNode *n = panel->tabs.first; n != 0; n = n->next, item_idx += 1)
              {
                CFG_Node *tab = n->v;
                DR_FStrList fstrs = rad_title_fstrs_from_tab(scratch.arena, tab);
                ui_set_next_child_layout_axis(Axis2_X);
                UI_Box *box = ui_build_box_from_stringf(UI_BoxFlag_DrawBorder|
                                                        UI_BoxFlag_DrawBackground|
                                                        UI_BoxFlag_DrawHotEffects|
                                                        UI_BoxFlag_DrawActiveEffects|
                                                        UI_BoxFlag_Clickable,
                                                        "###tab_list_item_%p", tab);
                UI_Parent(box) UI_HeightFill UI_Padding(ui_em(1.f, 1.f))
                {
                  RAD_Font(RAD_FontSlot_Icons)
                    UI_PrefWidth(ui_em(2.f, 1.f))
                    UI_TextAlignment(UI_TextAlign_Center)
                    UI_TagF("weak")
                    ui_label(tab == panel->selected_tab ? rad_icon_kind_text_table[RAD_IconKind_Check] : str8_lit(""));
                  UI_PrefWidth(ui_text_dim(10, 1.f))
                  {
                    UI_Box *name_box = ui_build_box_from_key(UI_BoxFlag_DrawText, ui_key_zero());
                    ui_box_equip_display_fstrs(name_box, &fstrs);
                  }
                }
                if(item_idx < ArrayCount(ws->tab_list_item_rects))
                {
                  ws->tab_list_item_rects[item_idx] = box->rect;
                  ws->tab_list_item_count = item_idx+1;
                }
                if(ui_clicked(ui_signal_from_box(box)))
                {
                  if(!rad_tab_is_pinned(tab) && !rad_tab_is_tool(tab))
                  {
                    rad_tab_move_after_pinned(tab);
                  }
                  rad_cmd(RAD_CmdKind_FocusTab, .panel = panel->cfg->id, .tab = tab->id);
                  rad_cmd(RAD_CmdKind_FocusPanel, .panel = panel->cfg->id);
                  ui_ctx_menu_close();
                }
              }
            }
          }

          //////////////////////////
          //- rad: accept file drops
          //
          if(build_panel)
          {
            for(UI_Event *evt = 0; ui_next_event(&evt);)
            {
              if(evt->kind == UI_EventKind_FileDrop && contains_2f32(content_rect, evt->pos))
              {
                for(String8Node *n = evt->paths.first; n != 0; n = n->next)
                {
                  rad_cmd(RAD_CmdKind_OpenFile, .file_path = n->string, .panel = panel->cfg->id);
                }
                ui_eat_event(evt);
              }
            }
          }
        }
      }
    }

    ////////////////////////////
    //- rad: Ctrl/Cmd+wheel: the code font size (setting "Mouse Wheel Zoom";
    // raddbg zooms the whole window here). Smooth: the wheel moves a
    // target that the drawn size eases to every frame. The target moves in
    // whole points (a notch, 120 units, = 1 point; a trackpad's or a
    // high-resolution wheel's small steps add up to one), the rest kept
    // until a point is reached either way: a slow or back-and-forth wheel
    // under a point does nothing (user 2026-11-07: with a fractional target,
    // the text followed those small movements, then eased back to the whole
    // size: it "vibrated")
    //
    for(UI_Event *evt = 0; ui_next_event(&evt);)
    {
      if(evt->kind == UI_EventKind_Scroll && evt->modifiers == WM_Modifier_Ctrl)
      {
        ui_eat_event(evt);
        if(settings->mouse_wheel_zoom && evt->delta_2f32.y != 0)
        {
          U64 now_us = now_time_us();
          if(!rad_state->code_zoom_wheeling)
          {
            rad_state->code_zoom_wheeling = 1;
            rad_state->code_zoom_target_pt = (F32)settings->code_font_size;
          }
          if(now_us >= rad_state->code_zoom_wheel_us + 500000)
          {
            // rad: a new wheel motion: what an old one left does not count
            rad_state->code_zoom_wheel_acc = 0;
          }
          F32 min_pt = (F32)rad_setting_info_table[RAD_SettingCode_code_font_size].min;
          F32 max_pt = (F32)rad_setting_info_table[RAD_SettingCode_code_font_size].max;
          F32 acc = rad_state->code_zoom_wheel_acc - evt->delta_2f32.y/120.f;
          F32 steps = (acc >= 0 ? floor_f32(acc) : ceil_f32(acc));
          acc -= steps;
          F32 target_pt = Clamp(min_pt, rad_state->code_zoom_target_pt + steps, max_pt);
          if((target_pt == max_pt && acc > 0) || (target_pt == min_pt && acc < 0))
          {
            acc = 0;
          }
          rad_state->code_zoom_target_pt = target_pt;
          rad_state->code_zoom_wheel_acc = acc;
          rad_state->code_zoom_wheel_us = now_us;
          rad_request_frame();
        }
      }
    }

    ui_end_build();
  }

  //////////////////////////////
  //- rad: animate
  //
  if(ui_animating_from_state(ws->ui))
  {
    rad_request_frame();
  }

  //////////////////////////////
  //- rad: draw UI
  //
  // NOTE(rad): verbatim port of raddbg's "@window_frame_part draw UI" (box
  // tree -> draw calls), minus developer heatmap/debug overlays.
  //
  ws->draw_bucket = dr_bucket_make();
  ws->see_through_count = 0;
  DR_BucketScope(ws->draw_bucket)
    ProfScope("draw UI")
  {
    Temp scratch = scratch_begin(0, 0);
    F32 box_squish_epsilon = 0.001f;
    Rng2F32 window_rect = wm_client_rect_from_window(ws->os);

    //- rad: unpack settings
    F32 rounded_corner_amount = settings->rounded_corner_amount;
    F32 border_softness = 1.f;
    B32 do_background_blur = settings->background_blur;
    B32 force_opaque_floating_backgrounds = settings->opaque_backgrounds;
    B32 do_drop_shadows = settings->drop_shadows;
    Vec4F32 base_background_color = ui_color_from_name(str8_lit("background"));
    Vec4F32 base_border_color = ui_color_from_name(str8_lit("border"));
    Vec4F32 drop_shadow_color = ui_color_from_name(str8_lit("drop_shadow"));

    //- rad: draw background color
    {
      dr_rect(wm_client_rect_from_window(ws->os), base_background_color, 0, 0, 0);
    }

    //- rad: draw window border
    {
      dr_rect(wm_client_rect_from_window(ws->os), base_border_color, 0, 1.f, border_softness*0.5f);
    }

    //- rad: recurse & draw
    for(UI_Box *box = ui_root_from_state(ws->ui); !ui_box_is_nil(box);)
    {
      // rad: get corner radii
      F32 box_corner_radii[Corner_COUNT] =
      {
        box->corner_radii[Corner_00]*rounded_corner_amount,
        box->corner_radii[Corner_01]*rounded_corner_amount,
        box->corner_radii[Corner_10]*rounded_corner_amount,
        box->corner_radii[Corner_11]*rounded_corner_amount,
      };

      // rad: get recursion
      UI_BoxRec rec = ui_box_rec_df_post(box, &ui_nil_box);

      // rad: push transparency
      if(box->transparency != 0)
      {
        dr_push_transparency(box->transparency);
      }

      // rad: push squish
      if(box->squish > box_squish_epsilon)
      {
        Vec2F32 box_dim = dim_2f32(box->rect);
        Vec2F32 anchor_off = {0};
        if(box->flags & UI_BoxFlag_SquishAnchored)
        {
          anchor_off.x = box_dim.x/2.f;
        }
        else
        {
          anchor_off.y = -box_dim.y/8.f;
        }
        Mat3x3F32 box2origin_xform = make_translate_3x3f32(v2f32(-box->rect.x0 - box_dim.x/2 + anchor_off.x, -box->rect.y0 + anchor_off.y));
        Mat3x3F32 scale_xform = make_scale_3x3f32(v2f32(1-box->squish, 1-box->squish));
        Mat3x3F32 origin2box_xform = make_translate_3x3f32(v2f32(box->rect.x0 + box_dim.x/2 - anchor_off.x, box->rect.y0 - anchor_off.y));
        Mat3x3F32 xform = mul_3x3f32(origin2box_xform, mul_3x3f32(scale_xform, box2origin_xform));
        dr_push_xform2d(xform);
        dr_push_tex2d_sample_kind(R_Tex2DSampleKind_Linear);
      }

      // rad: draw drop shadow
      if(do_drop_shadows && box->flags & UI_BoxFlag_DrawDropShadow)
      {
        Rng2F32 drop_shadow_rect = shift_2f32(pad_2f32(box->rect, 8), v2f32(4, 4));
        R_Rect2DInst *inst = dr_rect(drop_shadow_rect, drop_shadow_color, 0.8f, 0, 8.f);
        MemoryCopyArray(inst->corner_radii, box_corner_radii);
      }

      // rad: blur background
      if(do_background_blur && box->flags & UI_BoxFlag_DrawBackgroundBlur)
      {
        R_PassParams_Blur *params = dr_blur(pad_2f32(box->rect, 1.f), box->blur_size*(1-box->transparency), 0);
        MemoryCopyArray(params->corner_radii, box_corner_radii);
      }

      // rad: compute effective active t
      F32 effective_active_t = box->active_t;
      if(!(box->flags & UI_BoxFlag_DrawActiveEffects))
      {
        effective_active_t = 0;
      }
      F32 t = box->hot_t*(1-effective_active_t);

      // rad: compute background color. Not in raddbg: with Background Blur
      // off, the backgrounds made to be blurred are opaque (user 2026-11-07:
      // translucent without the blur, the code showed through the hover docs)
      Vec4F32 box_background_color = box->background_color;
      if(force_opaque_floating_backgrounds && box->flags & UI_BoxFlag_Floating && box->flags & UI_BoxFlag_DrawDropShadow)
      {
        box_background_color.w = 1.f;
      }
      if(!do_background_blur && box->flags & UI_BoxFlag_DrawBackgroundBlur)
      {
        box_background_color.w = 1.f;
      }
      if(box->flags & UI_BoxFlag_DrawBackground && box->flags & UI_BoxFlag_DrawBackgroundBlur &&
         !do_background_blur && box_background_color.w < 0.999f)
      {
        ws->see_through_count += 1;
      }

      // rad: draw background
      if(box->flags & UI_BoxFlag_DrawBackground)
      {
        Rng2F32 box_bg_rect = r2f32p(box->rect.x0 + box->visual_margin.x,
                                     box->rect.y0 + box->visual_margin.y,
                                     box->rect.x1 - box->visual_margin.x,
                                     box->rect.y1 - box->visual_margin.y);

        // rad: hot effect extension (drop shadow)
        if(box->flags & UI_BoxFlag_DrawHotEffects)
        {
          Rng2F32 drop_shadow_rect = shift_2f32(pad_2f32(box_bg_rect, 8), v2f32(4, 4));
          Vec4F32 color = drop_shadow_color;
          color.w *= t*box_background_color.w;
          dr_rect(drop_shadow_rect, color, 0.8f, 0, 8.f);
        }

        // rad: draw background
        R_Rect2DInst *inst = dr_rect(pad_2f32(box_bg_rect, 1.f), box_background_color, 0, 0, border_softness*1.f);
        MemoryCopyArray(inst->corner_radii, box_corner_radii);

        // rad: hot effect extension
        if(box->flags & UI_BoxFlag_DrawHotEffects)
        {
          B32 is_hot = !ui_key_match(box->key, ui_key_zero()) && ui_key_match(box->key, ui_hot_key());
          Vec4F32 hover_color = ui_color_from_tags_key_name(box->tags_key, str8_lit("hover"));

          // rad: brighten
          if(is_hot)
          {
            Vec4F32 color = hover_color;
            color.w *= 0.015f;
            R_Rect2DInst *inst = dr_rect(pad_2f32(box_bg_rect, 1.f), v4f32(0, 0, 0, 0), 0, 0, border_softness*1.f);
            inst->colors[Corner_00] = color;
            inst->colors[Corner_10] = color;
            inst->colors[Corner_01] = color;
            inst->colors[Corner_11] = color;
            MemoryCopyArray(inst->corner_radii, box_corner_radii);
          }
        }

        // rad: active effect extension
        if(box->flags & UI_BoxFlag_DrawActiveEffects)
        {
          Vec4F32 shadow_color = drop_shadow_color;
          shadow_color.w *= 0.5f*box->active_t;
          Vec2F32 shadow_size =
          {
            (box_bg_rect.x1 - box_bg_rect.x0)*0.60f*box->active_t,
            (box_bg_rect.y1 - box_bg_rect.y0)*0.60f*box->active_t,
          };
          shadow_size.x = Clamp(0, shadow_size.x, box->font_size*2.f);
          shadow_size.y = Clamp(0, shadow_size.y, box->font_size*2.f);

          // rad: top -> bottom dark effect
          {
            R_Rect2DInst *inst = dr_rect(r2f32p(box_bg_rect.x0, box_bg_rect.y0, box_bg_rect.x1, box_bg_rect.y0 + shadow_size.y), v4f32(0, 0, 0, 0), 0, 0, 1.f);
            inst->colors[Corner_00] = inst->colors[Corner_10] = shadow_color;
            inst->colors[Corner_01] = inst->colors[Corner_11] = v4f32(0.f, 0.f, 0.f, 0.0f);
            MemoryCopyArray(inst->corner_radii, box_corner_radii);
          }

          // rad: bottom -> top light effect
          {
            R_Rect2DInst *inst = dr_rect(r2f32p(box_bg_rect.x0, box_bg_rect.y1 - shadow_size.y, box_bg_rect.x1, box_bg_rect.y1), v4f32(0, 0, 0, 0), 0, 0, 1.f);
            inst->colors[Corner_00] = inst->colors[Corner_10] = v4f32(0, 0, 0, 0);
            inst->colors[Corner_01] = inst->colors[Corner_11] = v4f32(1.0f, 1.0f, 1.0f, 0.08f*box->active_t);
            MemoryCopyArray(inst->corner_radii, box_corner_radii);
          }

          // rad: left -> right dark effect
          {
            R_Rect2DInst *inst = dr_rect(r2f32p(box_bg_rect.x0, box_bg_rect.y0, box_bg_rect.x0 + shadow_size.x, box_bg_rect.y1), v4f32(0, 0, 0, 0), 0, 0, 1.f);
            inst->colors[Corner_10] = inst->colors[Corner_11] = v4f32(0.f, 0.f, 0.f, 0.f);
            inst->colors[Corner_00] = shadow_color;
            inst->colors[Corner_01] = shadow_color;
            MemoryCopyArray(inst->corner_radii, box_corner_radii);
          }

          // rad: right -> left dark effect
          {
            R_Rect2DInst *inst = dr_rect(r2f32p(box_bg_rect.x1 - shadow_size.x, box_bg_rect.y0, box_bg_rect.x1, box_bg_rect.y1), v4f32(0, 0, 0, 0), 0, 0, 1.f);
            inst->colors[Corner_00] = inst->colors[Corner_01] = v4f32(0.f, 0.f, 0.f, 0.f);
            inst->colors[Corner_10] = shadow_color;
            inst->colors[Corner_11] = shadow_color;
            MemoryCopyArray(inst->corner_radii, box_corner_radii);
          }
        }
      }

      // rad: draw string
      if(box->flags & UI_BoxFlag_DrawText)
      {
        Vec2F32 text_position = ui_box_text_position(box);
        F32 max_x = 100000.f;
        FNT_Run ellipses_run = {0};
        if(!(box->flags & UI_BoxFlag_DisableTextTrunc))
        {
          FNT_Tag ellipses_font = box->font;
          F32 ellipses_size = box->font_size;
          FNT_RasterFlags ellipses_raster_flags = box->text_raster_flags;
          if(box->display_fstrs.last)
          {
            ellipses_font = box->display_fstrs.last->v.params.font;
            ellipses_size = box->display_fstrs.last->v.params.size;
            ellipses_raster_flags = box->display_fstrs.last->v.params.raster_flags;
          }
          max_x = (box->rect.x1-text_position.x);
          ellipses_run = fnt_run_from_string(ellipses_font, ellipses_size, 0, box->tab_size, ellipses_raster_flags, str8_lit("..."));
        }
        if(box->flags & UI_BoxFlag_HasFuzzyMatchRanges) UI_TagF("match")
        {
          Vec4F32 match_color = ui_color_from_tags_key_name(ui_top_tags_key(), str8_lit("background"));
          dr_truncated_fancy_run_fuzzy_matches(text_position, &box->display_fruns, max_x, &box->fuzzy_match_ranges, match_color);
        }
        dr_truncated_fancy_run_list(text_position, &box->display_fruns, max_x, ellipses_run);
      }

      // rad: push clip
      if(box->flags & UI_BoxFlag_Clip)
      {
        Rng2F32 top_clip = dr_top_clip();
        Rng2F32 new_clip = pad_2f32(box->rect, -1);
        if(top_clip.x1 != 0 || top_clip.y1 != 0)
        {
          new_clip = intersect_2f32(new_clip, top_clip);
        }
        dr_push_clip(new_clip);
      }

      // rad: custom draw list
      if(box->flags & UI_BoxFlag_DrawBucket)
      {
        Mat3x3F32 xform = make_translate_3x3f32(box->position_delta);
        DR_XForm2DScope(xform)
        {
          dr_sub_bucket(box->draw_bucket);
        }
      }

      // rad: call custom draw callback
      if(box->custom_draw != 0)
      {
        box->custom_draw(box, box->custom_draw_user_data);
      }

      // rad: pop
      {
        S32 pop_idx = 0;
        for(UI_Box *b = box; !ui_box_is_nil(b) && pop_idx <= rec.pop_count; b = b->parent)
        {
          pop_idx += 1;
          if(b == box && rec.push_count != 0)
          {
            continue;
          }

          // rad: pop clips
          if(b->flags & UI_BoxFlag_Clip)
          {
            dr_pop_clip();
          }

          // rad: get corner radii
          F32 b_corner_radii[Corner_COUNT] =
          {
            b->corner_radii[Corner_00]*rounded_corner_amount,
            b->corner_radii[Corner_01]*rounded_corner_amount,
            b->corner_radii[Corner_10]*rounded_corner_amount,
            b->corner_radii[Corner_11]*rounded_corner_amount,
          };

          // rad: draw border
          if(b->flags & UI_BoxFlag_DrawBorder)
          {
            Vec4F32 border_color = b->border_color;
            Rng2F32 b_border_rect = r2f32p(b->rect.x0 - 1.f + b->visual_margin.x,
                                           b->rect.y0 - 1.f + b->visual_margin.y,
                                           b->rect.x1 + 1.f - b->visual_margin.x,
                                           b->rect.y1 + 1.f - b->visual_margin.y);
            R_Rect2DInst *inst = dr_rect(b_border_rect, border_color, 0, 1.f, border_softness*1.f);
            MemoryCopyArray(inst->corner_radii, b_corner_radii);

            // rad: hover effect
            if(b->flags & UI_BoxFlag_DrawHotEffects)
            {
              Vec4F32 color = ui_color_from_tags_key_name(box->tags_key, str8_lit("hover"));
              if(ui_key_match(b->key, ui_key_zero()) || !ui_key_match(b->key, ui_hot_key()))
              {
                color.w *= b->hot_t;
              }
              color.w *= 0.01f;
              R_Rect2DInst *inst = dr_rect(b_border_rect, color, 0, 1.f, 1.f);
              MemoryCopyArray(inst->corner_radii, b_corner_radii);
            }
          }

          // rad: draw scroll fade
          if(b->flags & (UI_BoxFlag_DrawFadeTop|UI_BoxFlag_DrawFadeBottom|UI_BoxFlag_DrawFadeLeft|UI_BoxFlag_DrawFadeRight))
          {
            Vec2F32 fade_dim = scale_2f32(dim_2f32(b->rect), 0.05f);
            Vec4F32 fade_color = drop_shadow_color;
            if(b->flags & UI_BoxFlag_DrawFadeTop)
            {
              F32 t = ui_anim(ui_key_from_string(b->key, str8_lit("fade_top")), 1.f, .rate = rad_state->catchall_animation_rate);
              Rng2F32 rect = r2f32p(b->rect.x0, b->rect.y0, b->rect.x1, b->rect.y0 + fade_dim.y*t);
              R_Rect2DInst *r = dr_rect(rect, fade_color, 0, 0, 0);
              MemoryCopyArray(r->corner_radii, b_corner_radii);
              r->colors[Corner_01] = r->colors[Corner_11] = v4f32(0, 0, 0, 0);
            }
            if(b->flags & UI_BoxFlag_DrawFadeBottom)
            {
              F32 t = ui_anim(ui_key_from_string(b->key, str8_lit("fade_bottom")), 1.f, .rate = rad_state->catchall_animation_rate);
              Rng2F32 rect = r2f32p(b->rect.x0, b->rect.y1 - fade_dim.y*t, b->rect.x1, b->rect.y1);
              R_Rect2DInst *r = dr_rect(rect, fade_color, 0, 0, 0);
              MemoryCopyArray(r->corner_radii, b_corner_radii);
              r->colors[Corner_00] = r->colors[Corner_10] = v4f32(0, 0, 0, 0);
            }
            if(b->flags & UI_BoxFlag_DrawFadeLeft)
            {
              F32 t = ui_anim(ui_key_from_string(b->key, str8_lit("fade_left")), 1.f, .rate = rad_state->catchall_animation_rate);
              Rng2F32 rect = r2f32p(b->rect.x0, b->rect.y0, b->rect.x1+fade_dim.x*t, b->rect.y1);
              R_Rect2DInst *r = dr_rect(rect, fade_color, 0, 0, 0);
              MemoryCopyArray(r->corner_radii, b_corner_radii);
              r->colors[Corner_11] = r->colors[Corner_10] = v4f32(0, 0, 0, 0);
            }
            if(b->flags & UI_BoxFlag_DrawFadeRight)
            {
              F32 t = ui_anim(ui_key_from_string(b->key, str8_lit("fade_right")), 1.f, .rate = rad_state->catchall_animation_rate);
              Rng2F32 rect = r2f32p(b->rect.x1 - fade_dim.x*t, b->rect.y0, b->rect.x1, b->rect.y1);
              R_Rect2DInst *r = dr_rect(rect, fade_color, 0, 0, 0);
              MemoryCopyArray(r->corner_radii, b_corner_radii);
              r->colors[Corner_00] = r->colors[Corner_01] = v4f32(0, 0, 0, 0);
            }
          }

          // rad: draw sides
          if(b->flags & (UI_BoxFlag_DrawSideTop|UI_BoxFlag_DrawSideBottom|UI_BoxFlag_DrawSideLeft|UI_BoxFlag_DrawSideRight))
          {
            Vec4F32 border_color = b->border_color;
            Rng2F32 r = b->rect;
            F32 half_thickness = 1.f;
            F32 softness = 0.f;
            if(b->flags & UI_BoxFlag_DrawSideTop)
            {
              dr_rect(r2f32p(r.x0, r.y0, r.x1, r.y0+2*half_thickness), border_color, 0, 0, softness);
            }
            if(b->flags & UI_BoxFlag_DrawSideBottom)
            {
              dr_rect(r2f32p(r.x0, r.y1-2*half_thickness, r.x1, r.y1), border_color, 0, 0, softness);
            }
            if(b->flags & UI_BoxFlag_DrawSideLeft)
            {
              dr_rect(r2f32p(r.x0, r.y0, r.x0+2*half_thickness, r.y1), border_color, 0, 0, softness);
            }
            if(b->flags & UI_BoxFlag_DrawSideRight)
            {
              dr_rect(r2f32p(r.x1-2*half_thickness, r.y0, r.x1, r.y1), border_color, 0, 0, softness);
            }
          }

          // rad: draw focus overlay
          if(b->flags & UI_BoxFlag_Clickable && !(b->flags & UI_BoxFlag_DisableFocusOverlay) && b->focus_hot_t > 0.01f)
          {
            String8 extras[] = {str8_lit("focus"), str8_lit("overlay")};
            String8Array extras_array = {extras, ArrayCount(extras)};
            Vec4F32 color = ui_color_from_tags_key_extras(b->tags_key, extras_array);
            color.w *= b->focus_hot_t;
            R_Rect2DInst *inst = dr_rect(b->rect, color, 0, 0, 0.f);
            MemoryCopyArray(inst->corner_radii, b_corner_radii);
          }

          // rad: draw focus border
          if(b->flags & UI_BoxFlag_Clickable && !(b->flags & UI_BoxFlag_DisableFocusBorder) && b->focus_active_t > 0.01f)
          {
            Rng2F32 rect = b->rect;
            if(b->flags & UI_BoxFlag_Floating)
            {
              rect = pad_2f32(rect, 1.f);
              rect = intersect_2f32(window_rect, rect);
            }
            String8 extras[] = {str8_lit("focus"), str8_lit("border")};
            String8Array extras_array = {extras, ArrayCount(extras)};
            Vec4F32 color = ui_color_from_tags_key_extras(b->tags_key, extras_array);
            color.w *= b->focus_active_t;
            R_Rect2DInst *inst = dr_rect(rect, color, 0, 1.f, border_softness*1.f);
            MemoryCopyArray(inst->corner_radii, b_corner_radii);
          }

          // rad: disabled overlay
          if(b->disabled_t >= 0.005f)
          {
            Vec4F32 disabled_overlay_color = v4f32(base_background_color.x, base_background_color.y, base_background_color.z, b->disabled_t*0.3f);
            R_Rect2DInst *inst = dr_rect(b->rect, disabled_overlay_color, 0, 0, 1);
            MemoryCopyArray(inst->corner_radii, b_corner_radii);
          }

          // rad: pop squish
          if(b->squish > box_squish_epsilon)
          {
            dr_pop_xform2d();
            dr_pop_tex2d_sample_kind();
          }

          // rad: pop transparency
          if(b->transparency != 0)
          {
            dr_pop_transparency();
          }
        }
      }

      // rad: next
      box = rec.next;
    }

    //- rad: draw border/overlay color to signify error (same as raddbg)
    if(rad_state->error_t > 0.01f) UI_TagF("bad")
    {
      Vec4F32 color = ui_color_from_name(str8_lit("text"));
      color.w *= rad_state->error_t;
      Rng2F32 rect = wm_client_rect_from_window(ws->os);
      dr_rect(pad_2f32(rect, 24.f), color, 0, 16.f, 12.f);
      dr_rect(rect, v4f32(color.x, color.y, color.z, color.w*0.025f), 0, 0, 0);
    }

    scratch_end(scratch);
  }

  //////////////////////////////
  //- rad: commit window's position/status to the cfg tree (port of raddbg's
  // @window_frame_part, minus the DPI font-size transform), so the session
  // file restores it
  //
  {
    Temp scratch = scratch_begin(0, 0);
    CFG_Node *window = cfg_node_from_id(ws->cfg_id);
    B32 is_fullscreen = wm_window_is_fullscreen(ws->os);
    B32 is_maximized = wm_window_is_maximized(ws->os);
    B32 is_minimized = wm_window_is_minimized(ws->os);
    if(is_fullscreen)
    {
      cfg_node_child_from_string_or_alloc(rad_state->cfg, window, str8_lit("fullscreen"));
    }
    else
    {
      cfg_node_release(rad_state->cfg, cfg_node_child_from_string(window, str8_lit("fullscreen")));
    }
    if(is_maximized)
    {
      cfg_node_child_from_string_or_alloc(rad_state->cfg, window, str8_lit("maximized"));
    }
    else
    {
      cfg_node_release(rad_state->cfg, cfg_node_child_from_string(window, str8_lit("maximized")));
    }
    Rng2F32 window_rect = wm_rect_from_window(ws->os);
    if(!is_fullscreen && !is_maximized && !is_minimized)
    {
      Vec2F32 pairs[2] = {window_rect.p0, dim_2f32(window_rect)};
      String8 names[2] = {str8_lit("pos"), str8_lit("size")};
      for EachIndex(idx, 2)
      {
        CFG_Node *root = cfg_node_child_from_string_or_alloc(rad_state->cfg, window, names[idx]);
        if((S32)pairs[idx].x != (S32)f64_from_str8(root->first->string) ||
           (S32)pairs[idx].y != (S32)f64_from_str8(root->last->string))
        {
          CFG_Node *x = root->first;
          if(x == &cfg_nil_node)
          {
            x = cfg_node_alloc(rad_state->cfg);
            cfg_node_insert_child(rad_state->cfg, root, &cfg_nil_node, x);
          }
          CFG_Node *y = x->next;
          if(y == &cfg_nil_node)
          {
            y = cfg_node_alloc(rad_state->cfg);
            cfg_node_insert_child(rad_state->cfg, root, x, y);
          }
          cfg_node_equip_stringf(rad_state->cfg, x, "%i", (S32)pairs[idx].x);
          cfg_node_equip_stringf(rad_state->cfg, y, "%i", (S32)pairs[idx].y);
        }
      }
    }
    if(!is_minimized)
    {
      String8 monitor_name = wm_name_from_monitor(scratch.arena, wm_monitor_from_window(ws->os));
      CFG_Node *monitor_root = cfg_node_child_from_string_or_alloc(rad_state->cfg, window, str8_lit("monitor"));
      if(!str8_match(monitor_root->first->string, monitor_name, 0))
      {
        cfg_node_new_replace(rad_state->cfg, monitor_root, monitor_name);
      }
    }
    scratch_end(scratch);
  }

  //////////////////////////////
  //- rad: update per-window frame counters/info
  //
  ws->frames_alive += 1;
  ws->last_window_rect = wm_client_rect_from_window(ws->os);

  ProfEnd();
  scratch_end(scratch);
}

#if COMPILER_MSVC && !BUILD_DEBUG
NO_OPTIMIZE_END
#endif

////////////////////////////////
//~ rad: Command Execution

internal void
rad_exec_cmd(RAD_Cmd *cmd)
{
  Temp scratch = scratch_begin(0, 0);
  CFG_Node *window = cfg_node_from_id(cmd->params.window);
  RAD_Window *ws = rad_window_from_cfg(window);
  CFG_PanelTree panel_tree = cfg_panel_tree_from_cfg(scratch.arena, window);
  CFG_Node *panel_cfg = cfg_node_from_id(cmd->params.panel);
  if(panel_cfg == &cfg_nil_node)
  {
    panel_cfg = panel_tree.focused->cfg;
  }
  CFG_Node *tab_cfg = cfg_node_from_id(cmd->params.tab);
  if(tab_cfg == &cfg_nil_node)
  {
    CFG_PanelNode *panel = cfg_panel_node_from_tree_cfg(panel_tree.root, panel_cfg);
    tab_cfg = panel->selected_tab;
  }
  Dir2 split_dir = Dir2_Invalid;
  U64 panel_sib_off = 0;
  U64 panel_child_off = 0;

  //- rad: a command that needs an input, run without one -> open its query
  // bar in the target tab (text views only, for now)
  RAD_CmdKindInfo *cmd_kind_info = &rad_cmd_kind_info_table[cmd->kind];
  B32 query_needed = ((cmd_kind_info->query.flags & RAD_QueryFlag_Required) &&
                      ((cmd_kind_info->query.slot == RAD_QuerySlot_LineNum && cmd->params.line_num == 0) ||
                       (cmd_kind_info->query.slot == RAD_QuerySlot_String && cmd->params.string.size == 0)));
  if(query_needed)
  {
    if(rad_tab_has_code_view(tab_cfg))
    {
      rad_tab_query_open(tab_cfg, cmd->kind);
    }
  }
  else switch(cmd->kind)
  {
    default:{}break;

    //- rad: code navigation (port of raddbg's code view commands)
    case RAD_CmdKind_GoToLine:
    if(rad_tab_has_code_view(tab_cfg))
    {
      rad_view_state_from_cfg(tab_cfg)->cv.goto_line_num = cmd->params.line_num;
    }break;
    case RAD_CmdKind_Search:
    case RAD_CmdKind_SearchBackwards:
    case RAD_CmdKind_FindNext:
    case RAD_CmdKind_FindPrev:
    if(rad_tab_has_code_view(tab_cfg))
    {
      String8 needle = cmd->params.string;
      if(cmd->kind == RAD_CmdKind_FindNext || cmd->kind == RAD_CmdKind_FindPrev)
      {
        needle = rad_tab_query_input(tab_cfg);
      }
      RAD_CodeViewState *cv = &rad_view_state_from_cfg(tab_cfg)->cv;
      cv->find_text_size = Min(sizeof(cv->find_text_buffer), needle.size);
      MemoryCopy(cv->find_text_buffer, needle.str, cv->find_text_size);
      cv->find_text_backward = (cmd->kind == RAD_CmdKind_SearchBackwards || cmd->kind == RAD_CmdKind_FindPrev);
    }break;

    //- rad: files
    case RAD_CmdKind_OpenFile:
    {
      String8 file_path = cmd->params.file_path;
      if(file_path.size == 0)
      {
        file_path = sh_pick_file(scratch.arena, str8_lit("Open File"), str8_zero());
      }

      // rad: dropped on a split site of a panel (F2) -> a new panel there
      CFG_Node *split_dst = cfg_node_from_id(cmd->params.dst_panel);
      if(file_path.size != 0 && split_dst != &cfg_nil_node && rad_tool_kind_from_panel(split_dst) == RAD_ToolKind_Null)
      {
        panel_cfg = rad_panel_split(split_dst, cmd->params.dir2, 0.5f);
      }

      // rad: never into the explorer's panel (focused, dropped on, or passed):
      // into the editor panel instead; none left -> a new one on the right
      CFG_Node *explorer_tab = rad_explorer_tab_from_panel(panel_cfg);
      if(file_path.size != 0 && explorer_tab != &cfg_nil_node)
      {
        panel_cfg = rad_explorer_target_panel_from_tab(explorer_tab);
        if(panel_cfg == explorer_tab->parent)
        {
          panel_cfg = rad_panel_alloc_at_window_edge(rad_window_cfg_from_cfg(explorer_tab), Side_Max, 0.8f);
        }
      }

      // rad: nor into a panel showing Output or Problems: into the editor
      // panel; none -> a new one above this panel
      else if(file_path.size != 0 && rad_tab_is_tool(cfg_panel_node_from_tree_cfg(panel_tree.root, panel_cfg)->selected_tab))
      {
        CFG_Node *editor_panel = rad_editor_panel_from_window(window);
        panel_cfg = (editor_panel != &cfg_nil_node ? editor_panel : rad_panel_split(panel_cfg, Dir2_Up, 0.7f));
      }

      if(file_path.size != 0 && panel_cfg != &cfg_nil_node)
      {
        file_path = path_normalized_from_string(scratch.arena, file_path);

        // rad: already open in this panel? -> focus it
        CFG_Node *existing_tab = &cfg_nil_node;
        CFG_PanelNode *panel = cfg_panel_node_from_tree_cfg(panel_tree.root, panel_cfg);
        for(CFG_NodePtrNode *n = panel->tabs.first; n != 0; n = n->next)
        {
          if(str8_match(rad_file_path_from_tab(n->v), file_path, StringMatchFlag_CaseInsensitive|StringMatchFlag_SlashInsensitive))
          {
            existing_tab = n->v;
            break;
          }
        }

        // rad: not open -> build new tab
        if(existing_tab == &cfg_nil_node)
        {
          existing_tab = cfg_node_new(rad_state->cfg, panel_cfg, str8_lit("text"));
          CFG_Node *path = cfg_node_new(rad_state->cfg, existing_tab, str8_lit("path"));
          cfg_node_new(rad_state->cfg, path, file_path);
        }
        rad_focus_tab(existing_tab);
        rad_focus_panel(panel_cfg);
      }
    }break;
    case RAD_CmdKind_OpenFolder:
    {
      // rad: find the explorer: the passed/selected tab if it is one, else
      // the window's first one
      CFG_Node *explorer_tab = &cfg_nil_node;
      if(str8_match(tab_cfg->string, str8_lit("explorer"), 0))
      {
        explorer_tab = tab_cfg;
      }
      for(CFG_PanelNode *p = panel_tree.root;
          p != &cfg_nil_panel_node && explorer_tab == &cfg_nil_node;
          p = cfg_panel_node_rec__depth_first_pre(panel_tree.root, p).next)
      {
        for(CFG_NodePtrNode *n = p->tabs.first; n != 0; n = n->next)
        {
          if(str8_match(n->v->string, str8_lit("explorer"), 0))
          {
            explorer_tab = n->v;
            break;
          }
        }
      }

      // rad: pick the folder (starting in the current one)
      String8 folder = cmd->params.file_path;
      if(folder.size == 0)
      {
        folder = sh_pick_folder(scratch.arena, str8_lit("Open Folder"), rad_explorer_folder_from_tab(explorer_tab));
      }

      // rad: point the explorer at it (no explorer -> new explorer panel on
      // the left, the explorer as its only tab), focus it
      if(folder.size != 0 && window != &cfg_nil_node)
      {
        folder = path_normalized_from_string(scratch.arena, folder);
        if(explorer_tab == &cfg_nil_node)
        {
          CFG_Node *explorer_panel = rad_panel_alloc_at_window_edge(window, Side_Min, 0.2f);
          explorer_tab = cfg_node_new(rad_state->cfg, explorer_panel, str8_lit("explorer"));
        }
        rad_explorer_set_folder(explorer_tab, folder);
        rad_tool_set_visible(window, RAD_ToolKind_Explorer, 1);
        rad_focus_tab(explorer_tab);
        rad_focus_panel(explorer_tab->parent);
      }
    }break;
    case RAD_CmdKind_Save:
    if(tab_cfg != &cfg_nil_node && str8_match(tab_cfg->string, str8_lit("text"), 0))
    {
      rad_text_view_save(tab_cfg);
    }break;
    case RAD_CmdKind_SaveAll:
    {
      // rad: every open file with unsaved changes, shown or not
      for(RAD_Document *doc = rad_state->first_document; doc != 0; doc = doc->next)
      {
        if(tb_buffer_is_dirty(doc->buffer))
        {
          rad_document_save(doc);
        }
      }
    }break;
    case RAD_CmdKind_Exit:
    {
      // rad: unsaved changes anywhere -> require a second exit within a few seconds
      U64 dirty_count = 0;
      for(RAD_Document *doc = rad_state->first_document; doc != 0; doc = doc->next)
      {
        dirty_count += !!tb_buffer_is_dirty(doc->buffer);
      }
      CFG_ID confirm_id = max_U64;
      if(dirty_count == 0 || (rad_state->discard_confirm_cfg_id == confirm_id && now_time_us() < rad_state->discard_confirm_deadline_us))
      {
        rad_state->quit = 1;
      }
      else
      {
        rad_state->discard_confirm_cfg_id = confirm_id;
        rad_state->discard_confirm_deadline_us = now_time_us() + 4000000;
        rad_set_error(push_str8f(scratch.arena, "%I64u file%s with unsaved changes. Exit again to discard them, or save first (%S).", dirty_count, dirty_count == 1 ? "" : "s",
                                 wm_string_from_modifiers_key(scratch.arena, WM_Modifier_Ctrl, WM_Key_S)));
      }
    }break;

    //- rad: editing
    case RAD_CmdKind_Undo:
    case RAD_CmdKind_Redo:
    if(tab_cfg != &cfg_nil_node && str8_match(tab_cfg->string, str8_lit("text"), 0))
    {
      RAD_ViewState *vs = rad_view_state_from_cfg(tab_cfg);
      RAD_Document *doc = rad_document_from_tab(tab_cfg);
      if(vs->cv.initialized && doc != 0)
      {
        if(cmd->kind == RAD_CmdKind_Undo)
        {
          rad_code_view_undo(&vs->cv, doc->buffer);
        }
        else
        {
          rad_code_view_redo(&vs->cv, doc->buffer);
        }
      }
    }break;

    //- rad: windows
    case RAD_CmdKind_ToggleFullscreen:
    if(ws != 0)
    {
      wm_window_set_fullscreen(ws->os, !wm_window_is_fullscreen(ws->os));
    }break;
    case RAD_CmdKind_IncWindowFontSize:
    {
      fnt_reset();
      rad_setting_set_f64(str8_lit("font_size"), (F64)rad_state->settings.font_size + 1);
    }break;
    case RAD_CmdKind_DecWindowFontSize:
    {
      fnt_reset();
      rad_setting_set_f64(str8_lit("font_size"), (F64)rad_state->settings.font_size - 1);
    }break;

    //- rad: not in raddbg: the code font alone (editor, Output)
    case RAD_CmdKind_IncCodeFontSize:
    {
      rad_state->code_zoom_wheel_acc = 0;
      rad_setting_set_f64(str8_lit("code_font_size"), (F64)rad_state->settings.code_font_size + 1);
    }break;
    case RAD_CmdKind_DecCodeFontSize:
    {
      rad_state->code_zoom_wheel_acc = 0;
      rad_setting_set_f64(str8_lit("code_font_size"), (F64)rad_state->settings.code_font_size - 1);
    }break;
    case RAD_CmdKind_ResetCodeFontSize:
    {
      rad_state->code_zoom_wheel_acc = 0;
      rad_setting_set_f64(str8_lit("code_font_size"), rad_setting_info_table[RAD_SettingCode_code_font_size].default_value);
    }break;

    //- rad: panel focusing
    case RAD_CmdKind_FocusPanel:
    {
      rad_focus_panel(panel_cfg);
    }break;

    //- rad: panel zooming (port of raddbg's)
    case RAD_CmdKind_ToggleZoomPanel:
    case RAD_CmdKind_ZoomPanel:
    case RAD_CmdKind_UnzoomPanel:
    if(ws != 0 && panel_cfg != &cfg_nil_node && rad_tool_kind_from_panel(panel_cfg) == RAD_ToolKind_Null)
    {
      ws->panel_hoist_cfg_id = panel_cfg->id;

      // rad: remove existing zoom
      B32 already_zoomed = 0;
      {
        CFG_PanelNode *zoom_root = panel_tree.zoom_root;
        if(zoom_root->cfg != &cfg_nil_node)
        {
          already_zoomed = 1;
          cfg_node_release(rad_state->cfg, cfg_node_child_from_string(zoom_root->cfg, str8_lit("zoom_root")));
        }
      }

      // rad: not already zoomed? -> zoom
      if(!already_zoomed || cmd->kind == RAD_CmdKind_ZoomPanel)
      {
        cfg_node_child_from_string_or_alloc(rad_state->cfg, panel_cfg, str8_lit("zoom_root"));
      }
    }break;
    case RAD_CmdKind_NextPanel: panel_sib_off = OffsetOf(CFG_PanelNode, next); panel_child_off = OffsetOf(CFG_PanelNode, first); goto cycle;
    case RAD_CmdKind_PrevPanel: panel_sib_off = OffsetOf(CFG_PanelNode, prev); panel_child_off = OffsetOf(CFG_PanelNode, last); goto cycle;
    cycle:;
    {
      // NOTE(rad): port of raddbg's RD_CmdKind_NextPanel/PrevPanel
      CFG_PanelNode *panel_nav_root = (panel_tree.zoom_root != &cfg_nil_panel_node ? panel_tree.zoom_root : panel_tree.root);
      CFG_PanelNode *next_focused = &cfg_nil_panel_node;
      for(CFG_PanelNode *p = panel_tree.focused;
          p != &cfg_nil_panel_node;
          p = cfg_panel_node_rec__depth_first(panel_nav_root, p, panel_sib_off, panel_child_off).next)
      {
        if(p != panel_tree.focused && p->first == &cfg_nil_panel_node && p->pct_of_parent > 0.001f)
        {
          // rad: (hidden tool windows are skipped)
          next_focused = p;
          break;
        }
      }
      if(next_focused == &cfg_nil_panel_node)
      {
        for(CFG_PanelNode *p = panel_nav_root;
            p != &cfg_nil_panel_node;
            p = cfg_panel_node_rec__depth_first(panel_nav_root, p, panel_sib_off, panel_child_off).next)
        {
          if(p != panel_tree.focused && p->first == &cfg_nil_panel_node && p->pct_of_parent > 0.001f)
          {
            next_focused = p;
            break;
          }
        }
      }
      if(next_focused != &cfg_nil_panel_node)
      {
        rad_focus_panel(next_focused->cfg);
      }
    }break;

    //- rad: panel creation
    case RAD_CmdKind_NewPanelRight:{split_dir = Dir2_Right;}goto split;
    case RAD_CmdKind_NewPanelDown: {split_dir = Dir2_Down;}goto split;
    split:;
    if(split_dir != Dir2_Invalid && panel_cfg != &cfg_nil_node && rad_tool_kind_from_panel(panel_cfg) != RAD_ToolKind_Null)
    {
      // rad: tool windows do not split: the editor panel does
      panel_cfg = rad_editor_panel_from_window(window);
    }
    if(split_dir != Dir2_Invalid && panel_cfg != &cfg_nil_node)
    {
      CFG_Node *new_panel_cfg = rad_panel_split(panel_cfg, split_dir, 0.5f);
      if(new_panel_cfg != &cfg_nil_node)
      {
        rad_focus_panel(new_panel_cfg);
      }
    }break;

    //- rad: panel removal
    case RAD_CmdKind_ClosePanel:
    {
      // NOTE(rad): tool windows hide; the last editor panel stays
      RAD_ToolKind tool = rad_tool_kind_from_panel(panel_cfg);
      if(tool != RAD_ToolKind_Null)
      {
        rad_tool_set_visible(window, tool, 0);
      }
      else if(rad_editor_leaf_count(window) > 1)
      {
        rad_panel_close(window, panel_cfg);
      }
    }break;

    //- rad: panel tab controls
    case RAD_CmdKind_FocusTab:
    {
      if(tab_cfg != &cfg_nil_node)
      {
        rad_focus_tab(tab_cfg);
      }
    }break;
    case RAD_CmdKind_NextTab:
    case RAD_CmdKind_PrevTab:
    {
      // NOTE(rad): port of raddbg's RD_CmdKind_NextTab/PrevTab
      CFG_PanelNode *focused = panel_tree.focused;
      B32 forward = (cmd->kind == RAD_CmdKind_NextTab);
      CFG_NodePtrNode *selected_tab_n = 0;
      for(CFG_NodePtrNode *n = focused->tabs.first; n != 0; n = n->next)
      {
        if(n->v == focused->selected_tab)
        {
          selected_tab_n = n;
          break;
        }
      }
      CFG_Node *next_selected_tab = &cfg_nil_node;
      if(selected_tab_n != 0)
      {
        CFG_NodePtrNode *next_n = forward ? selected_tab_n->next : 0;
        if(!forward)
        {
          for(CFG_NodePtrNode *n = focused->tabs.first; n != 0; n = n->next)
          {
            if(n->next == selected_tab_n)
            {
              next_n = n;
              break;
            }
          }
        }
        if(next_n == 0)
        {
          next_n = forward ? focused->tabs.first : focused->tabs.last;
        }
        if(next_n != selected_tab_n)
        {
          next_selected_tab = next_n->v;
        }
      }
      if(next_selected_tab != &cfg_nil_node)
      {
        rad_focus_tab(next_selected_tab);
      }
    }break;
    case RAD_CmdKind_MoveTab:
    case RAD_CmdKind_MoveTabToNewPanel:
    {
      // NOTE(rad): port of raddbg's MoveView / SplitPanel with a view: the tab
      // (all_tabs: every tab of its panel, a dragged tab bar) goes into the
      // target panel (after prev_tab), or a new panel split off it; a source
      // panel left empty closes (VS2022: the empty group goes)
      CFG_Node *src = (cmd->params.all_tabs ? panel_cfg : tab_cfg->parent);
      CFG_Node *dst = cfg_node_from_id(cmd->params.dst_panel);
      CFG_Node *prev = cfg_node_from_id(cmd->params.prev_tab);
      CFG_PanelNode *src_node = cfg_panel_node_from_tree_cfg(panel_tree.root, src);
      if(src_node == &cfg_nil_panel_node || dst == &cfg_nil_node || rad_tool_kind_from_panel(dst) != RAD_ToolKind_Null ||
         (!cmd->params.all_tabs && (tab_cfg == &cfg_nil_node || rad_tab_is_tool(tab_cfg))))
      {
        break;
      }

      //- rad: the tabs to move, in order; the one to select after
      CFG_NodePtrList tabs = {0};
      CFG_Node *select_tab = tab_cfg;
      if(cmd->params.all_tabs)
      {
        select_tab = src_node->selected_tab;
        for(CFG_NodePtrNode *n = src_node->tabs.first; n != 0; n = n->next)
        {
          if(!rad_tab_is_tool(n->v))
          {
            cfg_node_ptr_list_push(scratch.arena, &tabs, n->v);
          }
        }
      }
      else
      {
        cfg_node_ptr_list_push(scratch.arena, &tabs, tab_cfg);
      }
      if(tabs.count == 0 || (cmd->params.all_tabs && dst == src))
      {
        break;
      }
      if(cmd->kind == RAD_CmdKind_MoveTabToNewPanel)
      {
        if(dst == src && src_node->tabs.count <= tabs.count)
        {
          break;
        }
        dst = rad_panel_split(dst, cmd->params.dir2, 0.5f);
        prev = &cfg_nil_node;
      }
      else if(prev == tab_cfg && !cmd->params.all_tabs)
      {
        break;
      }

      //- rad: a new selection in the source panel (moving its selected tab
      // out of it)
      if(!cmd->params.all_tabs && src != dst && src_node->selected_tab == tab_cfg)
      {
        CFG_Node *other = &cfg_nil_node;
        for(CFG_NodePtrNode *n = src_node->tabs.first; n != 0; n = n->next)
        {
          if(n->v != tab_cfg)
          {
            other = n->v;
            if(n->next != 0 && n->next->v == tab_cfg) { break; }
          }
        }
        if(other != &cfg_nil_node)
        {
          rad_focus_tab(other);
        }
      }

      //- rad: move
      for(CFG_NodePtrNode *n = tabs.first; n != 0; n = n->next)
      {
        CFG_Node *tab = n->v;
        cfg_node_unhook(rad_state->cfg, src, tab);
        cfg_node_insert_child(rad_state->cfg, dst, (prev != &cfg_nil_node && prev->parent == dst) ? prev : &cfg_nil_node, tab);
        prev = tab;
      }
      for(CFG_NodePtrNode *n = tabs.first; n != 0; n = n->next)
      {
        // rad: unpinned tabs stay after the pinned ones
        CFG_Node *tab = n->v;
        B32 pinned_after = 0;
        for(CFG_Node *c = tab->next; c != &cfg_nil_node && !rad_tab_is_pinned(tab); c = c->next)
        {
          pinned_after = pinned_after || rad_tab_is_pinned(c);
        }
        if(pinned_after)
        {
          rad_tab_move_after_pinned(tab);
        }
      }
      if(select_tab != &cfg_nil_node)
      {
        rad_focus_tab(select_tab);
      }
      rad_focus_panel(dst);

      //- rad: the source panel left empty -> close it
      CFG_PanelTree new_tree = cfg_panel_tree_from_cfg(scratch.arena, window);
      CFG_PanelNode *new_src = cfg_panel_node_from_tree_cfg(new_tree.root, src);
      if(src != dst && new_src != &cfg_nil_panel_node && new_src->tabs.count == 0 && rad_editor_leaf_count(window) > 1)
      {
        rad_panel_close(window, src);
        rad_focus_panel(dst);
      }
    }break;
    case RAD_CmdKind_TogglePinTab:
    if(tab_cfg != &cfg_nil_node && !rad_tab_is_tool(tab_cfg))
    {
      // rad: not in raddbg (VS2022): pinned tabs come first
      CFG_Node *pinned = cfg_node_child_from_string(tab_cfg, str8_lit("pinned"));
      if(pinned != &cfg_nil_node)
      {
        cfg_node_release(rad_state->cfg, pinned);
      }
      else
      {
        cfg_node_new(rad_state->cfg, tab_cfg, str8_lit("pinned"));
      }
      rad_tab_move_after_pinned(tab_cfg);
    }break;
    case RAD_CmdKind_CloseTab:
    if(rad_tab_is_tool(tab_cfg))
    {
      // rad: tool tabs stay (the tool window's X hides the window)
    }
    else if(tab_cfg != &cfg_nil_node && rad_tab_is_dirty(tab_cfg) &&
            rad_document_tab_count(rad_document_from_tab(tab_cfg)) == 1 &&
            !(rad_state->discard_confirm_cfg_id == tab_cfg->id && now_time_us() < rad_state->discard_confirm_deadline_us))
    {
      // rad: unsaved changes in the file's last tab -> require a second close
      // within a few seconds
      rad_state->discard_confirm_cfg_id = tab_cfg->id;
      rad_state->discard_confirm_deadline_us = now_time_us() + 4000000;
      rad_set_error(push_str8f(scratch.arena, "\"%S\" has unsaved changes. Close it again to discard them, or save first (%S).", str8_skip_last_slash(rad_file_path_from_tab(tab_cfg)),
                               wm_string_from_modifiers_key(scratch.arena, WM_Modifier_Ctrl, WM_Key_S)));
    }
    else if(tab_cfg != &cfg_nil_node)
    {
      // NOTE(rad): port of raddbg's RD_CmdKind_CloseTab
      CFG_PanelNode *panel = cfg_panel_node_from_tree_cfg(panel_tree.root, tab_cfg->parent);
      CFG_Node *next_selected_tab = &cfg_nil_node;
      if(panel->selected_tab == tab_cfg)
      {
        B32 found_selected = 0;
        for(CFG_NodePtrNode *n = panel->tabs.first; n != 0; n = n->next)
        {
          if(n->v == panel->selected_tab)
          {
            found_selected = 1;
          }
          else
          {
            next_selected_tab = n->v;
            if(found_selected)
            {
              break;
            }
          }
        }
      }
      String8 closed_path = (str8_match(tab_cfg->string, str8_lit("text"), 0) ? rad_file_path_from_tab(tab_cfg) : str8_zero());
      if(closed_path.size != 0)
      {
        // rad: remembered for Reopen Closed Tab (TODO_2 E4)
        if(rad_state->closed_tab_count >= 64)
        {
          // rad: keep the newest 32 (the list is rebuilt in a cleared arena)
          RAD_ClosedTab *keep[32] = {0};
          U64 keep_count = 0;
          for(RAD_ClosedTab *t = rad_state->first_closed_tab; t != 0 && keep_count < ArrayCount(keep); t = t->next)
          {
            keep[keep_count] = t;
            keep_count += 1;
          }
          String8 paths[32];
          CFG_ID panels[32];
          for EachIndex(idx, keep_count)
          {
            paths[idx] = push_str8_copy(scratch.arena, keep[idx]->path);
            panels[idx] = keep[idx]->panel_id;
          }
          arena_clear(rad_state->closed_tabs_arena);
          rad_state->first_closed_tab = 0;
          rad_state->closed_tab_count = 0;
          for(U64 idx = keep_count; idx > 0; idx -= 1)
          {
            RAD_ClosedTab *t = push_array(rad_state->closed_tabs_arena, RAD_ClosedTab, 1);
            t->path = push_str8_copy(rad_state->closed_tabs_arena, paths[idx-1]);
            t->panel_id = panels[idx-1];
            SLLStackPush(rad_state->first_closed_tab, t);
            rad_state->closed_tab_count += 1;
          }
        }
        RAD_ClosedTab *t = push_array(rad_state->closed_tabs_arena, RAD_ClosedTab, 1);
        t->path = push_str8_copy(rad_state->closed_tabs_arena, closed_path);
        t->panel_id = tab_cfg->parent->id;
        SLLStackPush(rad_state->first_closed_tab, t);
        rad_state->closed_tab_count += 1;
      }
      cfg_node_release(rad_state->cfg, tab_cfg);
      if(next_selected_tab != &cfg_nil_node)
      {
        rad_focus_tab(next_selected_tab);
      }
    }break;
    case RAD_CmdKind_ToggleLineComment:
    case RAD_CmdKind_MoveLineUp:
    case RAD_CmdKind_MoveLineDown:
    case RAD_CmdKind_DuplicateLineUp:
    case RAD_CmdKind_DuplicateLineDown:
    case RAD_CmdKind_DeleteLine:
    case RAD_CmdKind_InsertLineBelow:
    case RAD_CmdKind_InsertLineAbove:
    if(str8_match(tab_cfg->string, str8_lit("text"), 0))
    {
      // rad: not in raddbg (TODO_2 E1): done by the editor's next build
      rad_view_state_from_cfg(tab_cfg)->cv.pending_line_op = (U32)cmd->kind;
      rad_request_frame();
    }break;
    case RAD_CmdKind_QuickOpen:
    {
      // rad: not in raddbg (TODO_2 E3): toggles
      if(rad_state->quick_open_active)
      {
        rad_state->quick_open_active = 0;
      }
      else
      {
        rad_quick_open_begin(window);
      }
      rad_request_frame();
    }break;
    case RAD_CmdKind_ReopenClosedTab:
    {
      // rad: not in raddbg (TODO_2 E4): the last closed file tab, in its
      // panel if it is still there (files that are gone are skipped)
      for(;rad_state->first_closed_tab != 0;)
      {
        RAD_ClosedTab *t = rad_state->first_closed_tab;
        SLLStackPop(rad_state->first_closed_tab);
        rad_state->closed_tab_count -= 1;
        String8 path = push_str8_copy(scratch.arena, t->path);
        CFG_ID panel_id = t->panel_id;
        if(rad_state->first_closed_tab == 0)
        {
          arena_clear(rad_state->closed_tabs_arena);
        }
        FileProperties props = properties_from_file_path(path);
        if(props.modified != 0 && !(props.flags & FilePropertyFlag_IsFolder))
        {
          rad_cmd(RAD_CmdKind_OpenFile, .file_path = path, .panel = panel_id);
          break;
        }
      }
    }break;

    //- rad: cargo
    case RAD_CmdKind_CargoBuild:
    case RAD_CmdKind_CargoCheck:
    case RAD_CmdKind_CargoRun:
    case RAD_CmdKind_CargoTest:
    if(window != &cfg_nil_node)
    {
      rad_cargo_start(rad_cargo_cmd_kind_from_rad_cmd_kind(cmd->kind), window);
    }break;
    case RAD_CmdKind_CargoStop:
    {
      rad_cargo_stop();
    }break;
    case RAD_CmdKind_CargoDebug:
    case RAD_CmdKind_CargoRelease:
    {
      rad_cargo_set_release(cmd->kind == RAD_CmdKind_CargoRelease);
    }break;
    case RAD_CmdKind_ClearOutput:
    {
      rad_cargo_clear_output();
    }break;
    case RAD_CmdKind_Settings:
    if(window != &cfg_nil_node)
    {
      // rad: one Settings tab per window, opened where files open
      CFG_Node *tab = rad_tab_from_window_kind(window, str8_lit("settings"));
      if(tab == &cfg_nil_node)
      {
        CFG_Node *panel = rad_editor_panel_from_window(window);
        if(panel != &cfg_nil_node)
        {
          tab = cfg_node_new(rad_state->cfg, panel, str8_lit("settings"));
        }
      }
      if(tab != &cfg_nil_node)
      {
        rad_focus_tab(tab);
        rad_focus_panel(tab->parent);
      }
    }break;
    case RAD_CmdKind_ShowOutput:
    case RAD_CmdKind_ShowProblems:
    if(window != &cfg_nil_node)
    {
      // rad: show the bottom tool window, select and focus the asked tab
      rad_tool_set_visible(window, RAD_ToolKind_Bottom, 1);
      String8 kind = (cmd->kind == RAD_CmdKind_ShowOutput ? str8_lit("output") : str8_lit("problems"));
      CFG_Node *tab = rad_tab_from_window_kind(window, kind);
      if(tab != &cfg_nil_node)
      {
        rad_focus_tab(tab);
        rad_focus_panel(tab->parent);
      }
    }break;
    case RAD_CmdKind_ExplorerNewFile:
    case RAD_CmdKind_ExplorerNewFolder:
    case RAD_CmdKind_ExplorerOpen:
    case RAD_CmdKind_ExplorerOpenToSide:
    case RAD_CmdKind_ExplorerCut:
    case RAD_CmdKind_ExplorerCopy:
    case RAD_CmdKind_ExplorerPaste:
    case RAD_CmdKind_ExplorerCopyPath:
    case RAD_CmdKind_ExplorerCopyRelativePath:
    case RAD_CmdKind_ExplorerRename:
    case RAD_CmdKind_ExplorerDelete:
    case RAD_CmdKind_ExplorerReveal:
    if(window != &cfg_nil_node)
    {
      // rad: explorer file operations (TODO_2 D2): done by the explorer
      CFG_Node *tab = rad_tab_from_window_kind(window, str8_lit("explorer"));
      if(tab != &cfg_nil_node)
      {
        rad_explorer_cmd(tab, cmd->kind, &cmd->params);
      }
    }break;
    case RAD_CmdKind_ShowExplorer:
    if(window != &cfg_nil_node)
    {
      rad_tool_set_visible(window, RAD_ToolKind_Explorer, 1);
      CFG_Node *tab = rad_tab_from_window_kind(window, str8_lit("explorer"));
      if(tab != &cfg_nil_node)
      {
        rad_focus_tab(tab);
        rad_focus_panel(tab->parent);
      }
    }break;
    case RAD_CmdKind_SearchExplorer:
    if(window != &cfg_nil_node)
    {
      // rad: VS2022's Search Solution Explorer (Ctrl+;)
      rad_tool_set_visible(window, RAD_ToolKind_Explorer, 1);
      CFG_Node *tab = rad_tab_from_window_kind(window, str8_lit("explorer"));
      if(tab != &cfg_nil_node)
      {
        rad_focus_tab(tab);
        rad_focus_panel(tab->parent);
        rad_view_state_from_cfg(tab)->ex.filter_focused = 1;
      }
    }break;
    case RAD_CmdKind_CloseToolWindow:
    {
      RAD_ToolKind tool = rad_tool_kind_from_panel(panel_cfg);
      if(tool != RAD_ToolKind_Null)
      {
        rad_tool_set_visible(window, tool, 0);
      }
    }break;
    case RAD_CmdKind_NextError:
    case RAD_CmdKind_PrevError:
    if(window != &cfg_nil_node && rad_problems->count != 0)
    {
      U64 count = rad_problems->count;
      U64 current = Min(rad_problems->cursor, count);
      U64 next = 0;
      if(cmd->kind == RAD_CmdKind_NextError)
      {
        next = (current >= count ? 1 : current+1);
      }
      else
      {
        next = (current <= 1 ? count : current-1);
      }
      rad_goto_diagnostic(window, next);
    }break;

    //- rad: locations (problems, output, Next/Previous Error)
    case RAD_CmdKind_GoToLocation:
    if(cmd->params.file_path.size != 0 && window != &cfg_nil_node)
    {
      String8 file_path = path_normalized_from_string(scratch.arena, cmd->params.file_path);

      // rad: a tab showing the file (the selected one of its panel first)
      CFG_Node *tab = &cfg_nil_node;
      CFG_Node *selected_tab = &cfg_nil_node;
      for(CFG_PanelNode *p = panel_tree.root; p != &cfg_nil_panel_node; p = cfg_panel_node_rec__depth_first_pre(panel_tree.root, p).next)
      {
        for(CFG_NodePtrNode *n = p->tabs.first; n != 0; n = n->next)
        {
          if(str8_match(rad_file_path_from_tab(n->v), file_path, StringMatchFlag_CaseInsensitive|StringMatchFlag_SlashInsensitive))
          {
            if(tab == &cfg_nil_node)
            {
              tab = n->v;
            }
            if(selected_tab == &cfg_nil_node && n->v == p->selected_tab)
            {
              selected_tab = n->v;
            }
          }
        }
      }
      if(selected_tab != &cfg_nil_node)
      {
        tab = selected_tab;
      }

      // rad: none -> a new tab in the editor panel
      if(tab == &cfg_nil_node)
      {
        CFG_Node *editor_panel = rad_editor_panel_from_window(window);
        if(editor_panel == &cfg_nil_node)
        {
          editor_panel = rad_panel_alloc_at_window_edge(window, Side_Max, 0.8f);
        }
        tab = cfg_node_new(rad_state->cfg, editor_panel, str8_lit("text"));
        CFG_Node *path = cfg_node_new(rad_state->cfg, tab, str8_lit("path"));
        cfg_node_new(rad_state->cfg, path, file_path);
      }

      // rad: focus it; its code view goes to the line and column once the
      // text is loaded
      rad_focus_tab(tab);
      rad_focus_panel(tab->parent);
      RAD_CodeViewState *cv = &rad_view_state_from_cfg(tab)->cv;
      cv->goto_line_num = Max(1, cmd->params.line_num);
      cv->goto_column_num = cmd->params.column_num;
      cv->goto_column_in_bytes = cmd->params.column_in_bytes;
    }break;

    //- rad: language server: definition (the tab's cursor, or a given
    // offset: Ctrl+click); back to where the last jumps came from
    case RAD_CmdKind_GoToDefinition:
    {
      CFG_Node *tab = cfg_node_from_id(cmd->params.tab);
      if(tab == &cfg_nil_node)
      {
        tab = tab_cfg;
      }
      RAD_Document *doc = rad_document_from_tab(tab);
      if(doc != 0 && window != &cfg_nil_node)
      {
        RAD_CodeViewState *cv = &rad_view_state_from_cfg(tab)->cv;
        U64 off = (cmd->params.has_text_offset ? cmd->params.text_offset : cv->cursor);
        rad_lsp_goto_definition(window, tab, doc, off);
      }
    }break;
    case RAD_CmdKind_NavigateBack:
    if(window != &cfg_nil_node)
    {
      rad_lsp_navigate_back(window);
    }break;
    case RAD_CmdKind_RestartLanguageServer:
    {
      rad_lsp_restart_all();
    }break;

    //- rad: language server: the code actions menu at the cursor (the tab's
    // code view asks / opens it when built)
    case RAD_CmdKind_QuickActions:
    {
      RAD_Document *doc = rad_document_from_tab(tab_cfg);
      if(doc != 0)
      {
        rad_view_state_from_cfg(tab_cfg)->cv.quick_actions = 1;
        rad_request_frame();
      }
    }break;

    //- rad: language server: signature help at the cursor (the tab's code
    // view asks when built)
    case RAD_CmdKind_ParameterInfo:
    {
      RAD_Document *doc = rad_document_from_tab(tab_cfg);
      if(doc != 0)
      {
        rad_view_state_from_cfg(tab_cfg)->cv.parameter_info = 1;
        rad_request_frame();
      }
    }break;

    //- rad: language server: completion list for the word before the cursor
    // (the tab's code view starts it when built)
    case RAD_CmdKind_CompleteWord:
    {
      RAD_Document *doc = rad_document_from_tab(tab_cfg);
      if(doc != 0)
      {
        rad_view_state_from_cfg(tab_cfg)->cv.complete_word = 1;
        rad_request_frame();
      }
    }break;
  }
  scratch_end(scratch);
}

////////////////////////////////
//~ rad: Folder Watches

internal RAD_FolderWatchChange
rad_folder_watch_take_change(String8 folder)
{
  // NOTE(rad): UI thread. a new watch reports a change once (the caller
  // lists the folder anyway); a failed one reports NoWatch (caller polls)
  RAD_FolderWatchChange result = RAD_FolderWatchChange_None;
  MutexScope(rad_state->watch_mutex)
  {
    RAD_FolderWatch *w = 0;
    for(RAD_FolderWatch *n = rad_state->first_watch; n != 0; n = n->next)
    {
      if(str8_match(str8(n->path_buffer, n->path_size), folder, StringMatchFlag_CaseInsensitive|StringMatchFlag_SlashInsensitive))
      {
        w = n;
        break;
      }
    }
    if(w == 0 && folder.size <= sizeof(w->path_buffer))
    {
      w = rad_state->free_watch;
      if(w != 0)
      {
        SLLStackPop(rad_state->free_watch);
      }
      else
      {
        w = push_array_no_zero(rad_state->arena, RAD_FolderWatch, 1);
      }
      MemoryZeroStruct(w);
      MemoryCopy(w->path_buffer, folder.str, folder.size);
      w->path_size = folder.size;
      w->watch = file_watch_alloc(folder, 1);
      w->changed = 1;
      SLLStackPush(rad_state->first_watch, w);
    }
    if(w == 0 || w->watch.u64[0] == 0)
    {
      result = RAD_FolderWatchChange_NoWatch;
    }
    else
    {
      result = (ins_atomic_u32_eval_assign(&w->changed, 0) ? RAD_FolderWatchChange_Changed : RAD_FolderWatchChange_None);
    }
    if(w != 0)
    {
      w->last_frame_index_touched = rad_state->frame_index;
    }
  }
  return result;
}

internal void
rad_folder_watches_gc(void)
{
  MutexScope(rad_state->watch_mutex)
  {
    for(RAD_FolderWatch **link = &rad_state->first_watch; *link != 0;)
    {
      RAD_FolderWatch *w = *link;
      if(w->last_frame_index_touched+2 < rad_state->frame_index)
      {
        *link = w->next;
        file_watch_release(w->watch);
        SLLStackPush(rad_state->free_watch, w);
      }
      else
      {
        link = &w->next;
      }
    }
  }
}

internal void
rad_watch_thread_entry_point(void *p)
{
  // NOTE(rad): every 100 ms: a watched folder changed, or file_stream saw a
  // streamed (open) file change -> flag it and wake the UI thread, which
  // otherwise sleeps until input arrives
  ThreadNameF("rad_watch_thread");
  U64 next_check_us = now_time_us() + 100000;
  for(;;)
  {
    B32 wake = 0;
    MutexScope(rad_state->watch_mutex)
    {
      U64 endt_us = next_check_us;
      if(rad_state->watch_wake_us != 0 && rad_state->watch_wake_us < endt_us)
      {
        endt_us = rad_state->watch_wake_us;
      }
      if(now_time_us() < endt_us)
      {
        cond_var_wait(rad_state->watch_cv, rad_state->watch_mutex, endt_us);
      }
      if(rad_state->watch_wake_us != 0 && now_time_us() >= rad_state->watch_wake_us)
      {
        rad_state->watch_wake_us = 0;
        wake = 1;
      }
    }
    if(wake)
    {
      wm_send_wakeup_event();
    }
    if(now_time_us() < next_check_us)
    {
      continue;
    }
    next_check_us = now_time_us() + 100000;
    B32 changed = 0;
    U64 fs_gen = fs_change_gen();
    if(fs_gen != rad_state->watch_fs_change_gen)
    {
      rad_state->watch_fs_change_gen = fs_gen;
      changed = 1;
    }
    MutexScope(rad_state->watch_mutex)
    {
      for(RAD_FolderWatch *w = rad_state->first_watch; w != 0; w = w->next)
      {
        if(w->watch.u64[0] != 0 && file_watch_check(w->watch))
        {
          ins_atomic_u32_eval_assign(&w->changed, 1);
          changed = 1;
        }
      }
    }
    if(changed)
    {
      ins_atomic_u64_inc_eval(&rad_state->watch_wakeups_sent);
      wm_send_wakeup_event();
    }
  }
}

////////////////////////////////
//~ rad: User Data (Session)

internal String8
rad_user_data_string(Arena *arena)
{
  // NOTE(rad): same layout as raddbg's user file: a header line, then each
  // child of the `user` cfg root
  Temp scratch = scratch_begin(&arena, 1);
  CFG_Node *user = cfg_node_child_from_string(cfg_node_root(), str8_lit("user"));
  String8List strings = {0};
  str8_list_pushf(scratch.arena, &strings, "// raid %s user file\n\n", BUILD_VERSION_STRING_LITERAL);
  for(CFG_Node *child = user->first; child != &cfg_nil_node; child = child->next)
  {
    str8_list_push(scratch.arena, &strings, cfg_string_from_tree(scratch.arena, rad_state->cfg_schema_table, str8_chop_last_slash(rad_state->user_path), child));
  }
  String8 result = str8_list_join(arena, &strings, 0);
  scratch_end(scratch);
  return result;
}

internal B32
rad_user_data_load(String8 path)
{
  // NOTE(rad): port of raddbg's RD_CmdKind_OpenUser, minus legacy formats,
  // projects and key bindings: parse the file into the `user` cfg root.
  // returns 1 if it held a window with panels (else the caller builds the
  // default layout). a file that is not a rad user file is left alone (the
  // path is dropped, so it is never overwritten).
  Temp scratch = scratch_begin(0, 0);
  B32 result = 0;
  String8 data = data_from_file_path(scratch.arena, path);
  // (headers: "// raid <version> user file"; "// rad " before the rename)
  B32 file_is_okay = (data.size == 0 ||
                      str8_match(str8_prefix(data, 8), str8_lit("// raid "), 0) ||
                      str8_match(str8_prefix(data, 7), str8_lit("// rad "), 0));
  if(!file_is_okay)
  {
    log_user_errorf("\"%S\" is not a " BUILD_TITLE " user file; it is left as it is, and the session is not saved.", path);
    rad_state->user_path = str8_zero();
  }
  else if(data.size != 0)
  {
    CFG_Node *user = cfg_node_child_from_string(cfg_node_root(), str8_lit("user"));
    CFG_NodePtrList cfgs = cfg_node_ptr_list_from_string(scratch.arena, rad_state->cfg, rad_state->cfg_schema_table, str8_chop_last_slash(path), data);
    for(CFG_NodePtrNode *n = cfgs.first; n != 0; n = n->next)
    {
      cfg_node_insert_child(rad_state->cfg, user, user->last, n->v);
    }
    CFG_Node *window = cfg_node_child_from_string(user, str8_lit("window"));
    result = (cfg_node_child_from_string(window, str8_lit("panels")) != &cfg_nil_node);
    if(!result)
    {
      cfg_node_release_all_children(rad_state->cfg, user);
    }
    rad_state->user_data_hash = u64_hash_from_str8(rad_user_data_string(scratch.arena));
  }
  scratch_end(scratch);
  return result;
}

internal void
rad_user_data_write(void)
{
  // NOTE(rad): port of raddbg's RD_CmdKind_WriteUserData: write a temp file,
  // move the old file aside, move the temp file in, delete the old file
  if(rad_state->user_path.size != 0)
  {
    Temp scratch = scratch_begin(0, 0);
    String8 dst_path = rad_state->user_path;
    String8 data = rad_user_data_string(scratch.arena);
    B32 dst_exists = (properties_from_file_path(dst_path).created != 0);
    String8 temp_path = push_str8f(scratch.arena, "%S.temp", dst_path);
    String8 overwritten_path = push_str8f(scratch.arena, "%S.old", dst_path);
    B32 temp_write_good = write_data_to_file_path(temp_path, data);
    B32 old_del_good    = (temp_write_good && delete_file_at_path(overwritten_path));
    B32 old_move_good   = (temp_write_good && (!dst_exists || move_file_path(overwritten_path, dst_path)));
    B32 new_move_good   = (old_move_good && move_file_path(dst_path, temp_path));
    (void)old_del_good;
    if(new_move_good && dst_exists)
    {
      delete_file_at_path(overwritten_path);
    }
    else if(!new_move_good && old_move_good && dst_exists)
    {
      move_file_path(dst_path, overwritten_path);
    }
    if(new_move_good)
    {
      rad_state->user_data_hash = u64_hash_from_str8(data);
    }
    scratch_end(scratch);
  }
}

////////////////////////////////
//~ rad: Main Layer Top-Level Calls

#if !defined(STBI_INCLUDE_STB_IMAGE_H)
# define STB_IMAGE_IMPLEMENTATION
# define STBI_ONLY_PNG
# define STBI_ONLY_BMP
# include "third_party/stb/stb_image.h"
#endif

internal void
rad_init(CmdLine *cmdln)
{
  Temp scratch = scratch_begin(0, 0);
  ProfBeginFunction();
  Arena *arena = arena_alloc();
  rad_state = push_array(arena, RAD_State, 1);
  rad_state->arena = arena;
  rad_state->popup_arena = arena_alloc();
  rad_state->closed_tabs_arena = arena_alloc();
  rad_state->quick_open_arena = arena_alloc();
  rad_state->quick_open_match_arena = arena_alloc();
  for(U64 idx = 0; idx < ArrayCount(rad_state->frame_arenas); idx += 1)
  {
    rad_state->frame_arenas[idx] = arena_alloc();
  }
  for(U64 idx = 0; idx < ArrayCount(rad_state->cmds_arenas); idx += 1)
  {
    rad_state->cmds_arenas[idx] = arena_alloc();
  }
  rad_state->log = log_alloc();
  log_select(rad_state->log);
  {
    rad_state->log_path = push_str8f(rad_state->arena, "%S/ui_thread." RAD_LOG_FILE_EXTENSION, g_logs_folder);
    write_data_to_file_path(rad_state->log_path, str8_zero());
  }
  rad_state->num_frames_requested = 2;
  rad_state->settings = rad_default_settings();

  //- rad: set up fonts (same defaults as raddbg with `use_alternative_font_for_ui`)
  {
    rad_state->font_slot_table[RAD_FontSlot_Main]  = fnt_tag_from_static_data_string(&rad_default_main_font_bytes);
    rad_state->font_slot_table[RAD_FontSlot_Code]  = fnt_tag_from_static_data_string(&rad_default_code_font_bytes);
    rad_state->font_slot_table[RAD_FontSlot_Icons] = fnt_tag_from_static_data_string(&rad_icon_font_bytes);
  }

  //- rad: set up theme presets
  {
    for EachEnumVal(RAD_ThemePreset, p)
    {
      rad_state->theme_preset_trees[p] = md_tree_from_string(rad_state->arena, rad_theme_preset_cfg_string_table[p])->first;
    }
  }

  //- rad: set up config tree
  {
    rad_state->cfg = cfg_state_alloc();
    cfg_ctx_select(cfg_state_ctx(rad_state->cfg));
    rad_state->cfg_schema_table = push_array(rad_state->arena, CFG_SchemaTable, 1);
    rad_state->cfg_schema_table->slots_count = 64;
    rad_state->cfg_schema_table->slots = push_array(rad_state->arena, CFG_SchemaNode *, rad_state->cfg_schema_table->slots_count);
    cfg_node_new(rad_state->cfg, cfg_node_root(), str8_lit("user"));
  }

  //- rad: set up view state cache
  {
    rad_state->view_state_slots_count = 256;
    rad_state->view_state_slots = push_array(rad_state->arena, RAD_ViewStateSlot, rad_state->view_state_slots_count);
  }

  //- rad: cargo integration (output, diagnostics, runner thread)
  rad_cargo_init();
  rad_lsp_init();
  rad_lsp->disabled = (cmd_line_string(cmdln, str8_lit("script")).size != 0);

  //- rad: user data file (session; port of raddbg's user file): --user:<path>,
  // else %APPDATA%/raid/default.raid_user (--appdata:<folder> stands for
  // %APPDATA%); scripted tests use none unless given. while the default file
  // does not exist, the one from before the rename (%APPDATA%/rad/
  // default.rad_user) is read instead, and the session is written to the new
  // file at the next save check; the old file is never changed.
  B32 user_loaded = 0;
  {
    String8 user_path = cmd_line_string(cmdln, str8_lit("user"));
    String8 appdata = cmd_line_string(cmdln, str8_lit("appdata"));
    String8 import_path = {0};
    if(user_path.size == 0 && (cmd_line_string(cmdln, str8_lit("script")).size == 0 || appdata.size != 0))
    {
      if(appdata.size == 0)
      {
        appdata = get_process_info()->user_program_config_data_path;
      }
      appdata = path_absolute_dst_from_relative_dst_src(scratch.arena, appdata, get_process_info()->initial_path);
      String8 folder = push_str8f(scratch.arena, "%S/" RAD_DATA_FOLDER_NAME, appdata);
      make_directory(folder);
      user_path = push_str8f(scratch.arena, "%S/default." RAD_USER_FILE_EXTENSION, folder);
      String8 old_path = push_str8f(scratch.arena, "%S/" RAD_OLD_DATA_FOLDER_NAME "/" RAD_OLD_USER_FILE_NAME, appdata);
      if(!file_path_exists(user_path) && file_path_exists(old_path))
      {
        import_path = path_normalized_from_string(scratch.arena, old_path);
      }
    }
    if(user_path.size != 0)
    {
      user_path = path_absolute_dst_from_relative_dst_src(scratch.arena, user_path, get_process_info()->initial_path);
      rad_state->user_path = path_normalized_from_string(rad_state->arena, user_path);
      user_loaded = rad_user_data_load(import_path.size != 0 ? import_path : rad_state->user_path);
      if(import_path.size != 0)
      {
        rad_state->user_data_hash = 0;
      }
    }
  }

  //- rad: restored session -> command line inputs go into it (folder: the
  // explorer's; files: opened like Open File)
  if(user_loaded)
  {
    CFG_Node *window = cfg_node_child_from_string(cfg_node_child_from_string(cfg_node_root(), str8_lit("user")), str8_lit("window"));
    for(String8Node *n = cmdln->inputs.first; n != 0; n = n->next)
    {
      String8 path = path_absolute_dst_from_relative_dst_src(scratch.arena, n->string, get_process_info()->initial_path);
      path = path_normalized_from_string(scratch.arena, path);
      B32 is_folder = !!(properties_from_file_path(path).flags & FilePropertyFlag_IsFolder);
      rad_cmd(is_folder ? RAD_CmdKind_OpenFolder : RAD_CmdKind_OpenFile, .window = window->id, .file_path = path);
    }
  }

  //- rad: build default window layout
  //
  // NOTE(rad): built like raddbg's RD_CmdKind_ResetToDefaultPanels: a root
  // split on x. the left panel (20%) holds the explorer; the right panel
  // (80%, focused) holds one text tab per file argument, or a "getting
  // started" tab when there is none. the explorer's folder is the first
  // folder argument, else the folder of the first file argument.
  //
  if(!user_loaded)
  {
    CFG_Node *user = cfg_node_child_from_string(cfg_node_root(), str8_lit("user"));
    CFG_NodePtrList cfgs = cfg_node_ptr_list_from_string(scratch.arena, rad_state->cfg, rad_state->cfg_schema_table, str8_zero(),
                                                         str8_lit("window: { split_x }"));
    for(CFG_NodePtrNode *n = cfgs.first; n != 0; n = n->next)
    {
      cfg_node_insert_child(rad_state->cfg, user, user->last, n->v);
    }
    CFG_Node *window = cfg_node_child_from_string(user, str8_lit("window"));
    CFG_Node *panels = cfg_node_new(rad_state->cfg, window, str8_lit("panels"));
    CFG_Node *explorer_panel = cfg_node_new(rad_state->cfg, panels, str8_lit("0.20"));
    CFG_Node *editor_panel = cfg_node_new(rad_state->cfg, panels, str8_lit("0.80"));
    cfg_node_new(rad_state->cfg, editor_panel, str8_lit("selected"));

    //- rad: command line inputs which are not flags -> folder for the
    // explorer, files to open
    String8 root_folder = {0};
    String8 first_file_folder = {0};
    B32 opened_file = 0;
    for(String8Node *n = cmdln->inputs.first; n != 0; n = n->next)
    {
      String8 path = path_absolute_dst_from_relative_dst_src(scratch.arena, n->string, get_process_info()->initial_path);
      path = path_normalized_from_string(scratch.arena, path);
      FileProperties props = properties_from_file_path(path);
      if(props.flags & FilePropertyFlag_IsFolder)
      {
        if(root_folder.size == 0)
        {
          root_folder = path;
        }
        continue;
      }
      if(first_file_folder.size == 0)
      {
        first_file_folder = str8_chop_last_slash(path);
      }
      CFG_Node *tab = cfg_node_new(rad_state->cfg, editor_panel, str8_lit("text"));
      CFG_Node *path_cfg = cfg_node_new(rad_state->cfg, tab, str8_lit("path"));
      cfg_node_new(rad_state->cfg, path_cfg, path);
      rad_focus_tab(tab);
      opened_file = 1;
    }
    if(!opened_file)
    {
      CFG_Node *tab = cfg_node_new(rad_state->cfg, editor_panel, str8_lit("getting_started"));
      rad_focus_tab(tab);
    }
    if(root_folder.size == 0)
    {
      root_folder = first_file_folder;
    }

    //- rad: explorer tab
    {
      CFG_Node *tab = cfg_node_new(rad_state->cfg, explorer_panel, str8_lit("explorer"));
      if(root_folder.size != 0)
      {
        CFG_Node *folder_cfg = cfg_node_new(rad_state->cfg, tab, str8_lit("folder"));
        cfg_node_new(rad_state->cfg, folder_cfg, root_folder);
      }
      rad_focus_tab(tab);
    }
    rad_state->last_editor_panel = editor_panel->id;
  }

  //- rad: unpack icon image data (same ICO decode as raddbg's rd_init)
  {
    String8 data = rad_icon_file_bytes;
    U8 *ptr = data.str;
    U8 *opl = ptr+data.size;

    // rad: read header
#pragma pack(push, 1)
    typedef struct ICO_Header ICO_Header;
    struct ICO_Header
    {
      U16 reserved_padding; // must be 0
      U16 image_type; // if 1 -> ICO, if 2 -> CUR
      U16 num_images;
    };
    typedef struct ICO_Entry ICO_Entry;
    struct ICO_Entry
    {
      U8 image_width_px;
      U8 image_height_px;
      U8 num_colors;
      U8 reserved_padding; // should be 0
      union
      {
        U16 ico_color_planes; // in ICO
        U16 cur_hotspot_x_px; // in CUR
      };
      union
      {
        U16 ico_bits_per_pixel; // in ICO
        U16 cur_hotspot_y_px;   // in CUR
      };
      U32 image_data_size;
      U32 image_data_off;
    };
#pragma pack(pop)
    ICO_Header hdr = {0};
    if(ptr+sizeof(hdr) < opl)
    {
      MemoryCopy(&hdr, ptr, sizeof(hdr));
      ptr += sizeof(hdr);
    }

    // rad: read image entries
    U64 entries_count = hdr.num_images;
    ICO_Entry *entries = push_array(scratch.arena, ICO_Entry, hdr.num_images);
    {
      U64 bytes_to_read = sizeof(ICO_Entry)*entries_count;
      bytes_to_read = Min(bytes_to_read, opl-ptr);
      MemoryCopy(entries, ptr, bytes_to_read);
      ptr += bytes_to_read;
    }

    // rad: find largest image
    ICO_Entry *best_entry = 0;
    U64 best_entry_area = 0;
    for(U64 idx = 0; idx < entries_count; idx += 1)
    {
      ICO_Entry *entry = &entries[idx];
      U64 width = entry->image_width_px;
      if(width == 0) { width = 256; }
      U64 height = entry->image_height_px;
      if(height == 0) { height = 256; }
      U64 entry_area = width*height;
      if(entry_area > best_entry_area)
      {
        best_entry = entry;
        best_entry_area = entry_area;
      }
    }

    // rad: deserialize raw image data from best entry's offset
    U8 *image_data = 0;
    Vec2S32 image_dim = {0};
    if(best_entry != 0)
    {
      U8 *file_data_ptr = data.str + best_entry->image_data_off;
      U64 file_data_size = best_entry->image_data_size;
      int width = 0;
      int height = 0;
      int components = 0;
      image_data = stbi_load_from_memory(file_data_ptr, file_data_size, &width, &height, &components, 4);
      image_dim.x = width;
      image_dim.y = height;
    }

    // rad: upload to gpu texture
    rad_state->icon_texture = r_tex2d_alloc(R_ResourceKind_Static, image_dim, R_Tex2DFormat_RGBA8, image_data);

    // rad: release
    stbi_image_free(image_data);
  }

  //- rad: scripted tests (`--script:<file>`, see rad_script.h)
  rad_script_init(cmdln);

  //- rad: folder watches + watcher thread (wakes the UI on disk changes)
  rad_state->watch_mutex = mutex_alloc();
  rad_state->watch_fs_change_gen = fs_change_gen();
  rad_state->watch_cv = cond_var_alloc();
  rad_state->watch_thread = thread_launch(rad_watch_thread_entry_point, 0);

  ProfEnd();
  scratch_end(scratch);
}

////////////////////////////////
//~ rad: Text Navigation Bindings
//
// NOTE(rad): raddbg turns these keys into commands (move_left, copy, ...)
// whose handlers push one UI_Event each (raddbg_core.c, RD_CmdKind_MoveLeft
// and following). This table maps the same default bindings directly to the
// same UI_Event parameters.

typedef struct RAD_TextNavBinding RAD_TextNavBinding;
struct RAD_TextNavBinding
{
  WM_Key key;
  WM_Modifiers modifiers;
  UI_EventKind kind;
  UI_EventFlags flags;
  UI_EventDeltaUnit delta_unit;
  Vec2S32 delta;
};

#define RAD_NavFlags_Horz   (UI_EventFlag_PickSelectSide|UI_EventFlag_ZeroDeltaOnSelect|UI_EventFlag_ExplicitDirectional)
#define RAD_NavFlags_Vert   (UI_EventFlag_ExplicitDirectional|UI_EventFlag_Secondary)
#define RAD_NavFlags_SelH   (UI_EventFlag_KeepMark|UI_EventFlag_ExplicitDirectional)
#define RAD_NavFlags_SelV   (UI_EventFlag_KeepMark|UI_EventFlag_ExplicitDirectional|UI_EventFlag_Secondary)
read_only global RAD_TextNavBinding rad_text_nav_binding_table[] =
{
  // rad: character / line
  {WM_Key_Left,     0,                                  UI_EventKind_Navigate, RAD_NavFlags_Horz,                         UI_EventDeltaUnit_Char,  {-1, +0}},
  {WM_Key_Right,    0,                                  UI_EventKind_Navigate, RAD_NavFlags_Horz,                         UI_EventDeltaUnit_Char,  {+1, +0}},
  {WM_Key_Up,       0,                                  UI_EventKind_Navigate, RAD_NavFlags_Vert,                         UI_EventDeltaUnit_Char,  {+0, -1}},
  {WM_Key_Down,     0,                                  UI_EventKind_Navigate, RAD_NavFlags_Vert,                         UI_EventDeltaUnit_Char,  {+0, +1}},
  {WM_Key_Left,     WM_Modifier_Shift,                  UI_EventKind_Navigate, RAD_NavFlags_SelH,                         UI_EventDeltaUnit_Char,  {-1, +0}},
  {WM_Key_Right,    WM_Modifier_Shift,                  UI_EventKind_Navigate, RAD_NavFlags_SelH,                         UI_EventDeltaUnit_Char,  {+1, +0}},
  {WM_Key_Up,       WM_Modifier_Shift,                  UI_EventKind_Navigate, RAD_NavFlags_SelV,                         UI_EventDeltaUnit_Char,  {+0, -1}},
  {WM_Key_Down,     WM_Modifier_Shift,                  UI_EventKind_Navigate, RAD_NavFlags_SelV,                         UI_EventDeltaUnit_Char,  {+0, +1}},

#if OS_MAC
  // rad: the Mac's keys (macOS text fields, Zed): Option moves by word or
  // paragraph, Cmd (sent as Ctrl) to the line's or the file's ends
  {WM_Key_Left,     WM_Modifier_Alt,                    UI_EventKind_Navigate, UI_EventFlag_ExplicitDirectional,          UI_EventDeltaUnit_Word,  {-1, +0}},
  {WM_Key_Right,    WM_Modifier_Alt,                    UI_EventKind_Navigate, UI_EventFlag_ExplicitDirectional,          UI_EventDeltaUnit_Word,  {+1, +0}},
  {WM_Key_Up,       WM_Modifier_Alt,                    UI_EventKind_Navigate, RAD_NavFlags_Vert,                         UI_EventDeltaUnit_Word,  {+0, -1}},
  {WM_Key_Down,     WM_Modifier_Alt,                    UI_EventKind_Navigate, RAD_NavFlags_Vert,                         UI_EventDeltaUnit_Word,  {+0, +1}},
  {WM_Key_Left,     WM_Modifier_Alt|WM_Modifier_Shift,  UI_EventKind_Navigate, RAD_NavFlags_SelH,                         UI_EventDeltaUnit_Word,  {-1, +0}},
  {WM_Key_Right,    WM_Modifier_Alt|WM_Modifier_Shift,  UI_EventKind_Navigate, RAD_NavFlags_SelH,                         UI_EventDeltaUnit_Word,  {+1, +0}},
  {WM_Key_Up,       WM_Modifier_Alt|WM_Modifier_Shift,  UI_EventKind_Navigate, RAD_NavFlags_SelH,                         UI_EventDeltaUnit_Word,  {+0, -1}},
  {WM_Key_Down,     WM_Modifier_Alt|WM_Modifier_Shift,  UI_EventKind_Navigate, RAD_NavFlags_SelH,                         UI_EventDeltaUnit_Word,  {+0, +1}},
  {WM_Key_Left,     WM_Modifier_Ctrl,                   UI_EventKind_Navigate, 0,                                         UI_EventDeltaUnit_Line,  {-1, +0}},
  {WM_Key_Right,    WM_Modifier_Ctrl,                   UI_EventKind_Navigate, 0,                                         UI_EventDeltaUnit_Line,  {+1, +0}},
  {WM_Key_Left,     WM_Modifier_Ctrl|WM_Modifier_Shift, UI_EventKind_Navigate, UI_EventFlag_KeepMark,                     UI_EventDeltaUnit_Line,  {-1, +0}},
  {WM_Key_Right,    WM_Modifier_Ctrl|WM_Modifier_Shift, UI_EventKind_Navigate, UI_EventFlag_KeepMark,                     UI_EventDeltaUnit_Line,  {+1, +0}},
  {WM_Key_Up,       WM_Modifier_Ctrl,                   UI_EventKind_Navigate, UI_EventFlag_Secondary,                    UI_EventDeltaUnit_Whole, {+0, -1}},
  {WM_Key_Down,     WM_Modifier_Ctrl,                   UI_EventKind_Navigate, UI_EventFlag_Secondary,                    UI_EventDeltaUnit_Whole, {+0, +1}},
  {WM_Key_Up,       WM_Modifier_Ctrl|WM_Modifier_Shift, UI_EventKind_Navigate, UI_EventFlag_KeepMark,                     UI_EventDeltaUnit_Whole, {+0, -1}},
  {WM_Key_Down,     WM_Modifier_Ctrl|WM_Modifier_Shift, UI_EventKind_Navigate, UI_EventFlag_KeepMark,                     UI_EventDeltaUnit_Whole, {+0, +1}},
  {WM_Key_Backspace,WM_Modifier_Alt,                    UI_EventKind_Edit,     UI_EventFlag_Delete|UI_EventFlag_ZeroDeltaOnSelect, UI_EventDeltaUnit_Word, {-1, +0}},
  {WM_Key_Delete,   WM_Modifier_Alt,                    UI_EventKind_Edit,     UI_EventFlag_Delete|UI_EventFlag_ZeroDeltaOnSelect, UI_EventDeltaUnit_Word, {+1, +0}},
  {WM_Key_Backspace,WM_Modifier_Ctrl,                   UI_EventKind_Edit,     UI_EventFlag_Delete|UI_EventFlag_ZeroDeltaOnSelect, UI_EventDeltaUnit_Line, {-1, +0}},
  {WM_Key_Delete,   WM_Modifier_Ctrl,                   UI_EventKind_Edit,     UI_EventFlag_Delete|UI_EventFlag_ZeroDeltaOnSelect, UI_EventDeltaUnit_Line, {+1, +0}},
#else
  // rad: chunk
  {WM_Key_Left,     WM_Modifier_Ctrl,                   UI_EventKind_Navigate, UI_EventFlag_ExplicitDirectional,          UI_EventDeltaUnit_Word,  {-1, +0}},
  {WM_Key_Right,    WM_Modifier_Ctrl,                   UI_EventKind_Navigate, UI_EventFlag_ExplicitDirectional,          UI_EventDeltaUnit_Word,  {+1, +0}},
  {WM_Key_Up,       WM_Modifier_Ctrl,                   UI_EventKind_Navigate, RAD_NavFlags_Vert,                         UI_EventDeltaUnit_Word,  {+0, -1}},
  {WM_Key_Down,     WM_Modifier_Ctrl,                   UI_EventKind_Navigate, RAD_NavFlags_Vert,                         UI_EventDeltaUnit_Word,  {+0, +1}},
  {WM_Key_Left,     WM_Modifier_Ctrl|WM_Modifier_Shift, UI_EventKind_Navigate, RAD_NavFlags_SelH,                         UI_EventDeltaUnit_Word,  {-1, +0}},
  {WM_Key_Right,    WM_Modifier_Ctrl|WM_Modifier_Shift, UI_EventKind_Navigate, RAD_NavFlags_SelH,                         UI_EventDeltaUnit_Word,  {+1, +0}},
  {WM_Key_Up,       WM_Modifier_Ctrl|WM_Modifier_Shift, UI_EventKind_Navigate, RAD_NavFlags_SelH,                         UI_EventDeltaUnit_Word,  {+0, -1}},
  {WM_Key_Down,     WM_Modifier_Ctrl|WM_Modifier_Shift, UI_EventKind_Navigate, RAD_NavFlags_SelH,                         UI_EventDeltaUnit_Word,  {+0, +1}},

#endif

  // rad: page / whole
  {WM_Key_PageUp,   0,                                  UI_EventKind_Navigate, UI_EventFlag_Secondary,                    UI_EventDeltaUnit_Page,  {+0, -1}},
  {WM_Key_PageDown, 0,                                  UI_EventKind_Navigate, UI_EventFlag_Secondary,                    UI_EventDeltaUnit_Page,  {+0, +1}},
  {WM_Key_PageUp,   WM_Modifier_Shift,                  UI_EventKind_Navigate, UI_EventFlag_KeepMark,                     UI_EventDeltaUnit_Page,  {+0, -1}},
  {WM_Key_PageDown, WM_Modifier_Shift,                  UI_EventKind_Navigate, UI_EventFlag_KeepMark,                     UI_EventDeltaUnit_Page,  {+0, +1}},
  {WM_Key_Home,     WM_Modifier_Ctrl,                   UI_EventKind_Navigate, UI_EventFlag_Secondary,                    UI_EventDeltaUnit_Whole, {+0, -1}},
  {WM_Key_End,      WM_Modifier_Ctrl,                   UI_EventKind_Navigate, UI_EventFlag_Secondary,                    UI_EventDeltaUnit_Whole, {+0, +1}},
  {WM_Key_Home,     WM_Modifier_Ctrl|WM_Modifier_Shift, UI_EventKind_Navigate, UI_EventFlag_KeepMark,                     UI_EventDeltaUnit_Whole, {+0, -1}},
  {WM_Key_End,      WM_Modifier_Ctrl|WM_Modifier_Shift, UI_EventKind_Navigate, UI_EventFlag_KeepMark,                     UI_EventDeltaUnit_Whole, {+0, +1}},

  // rad: home / end
  {WM_Key_Home,     0,                                  UI_EventKind_Navigate, 0,                                         UI_EventDeltaUnit_Line,  {-1, +0}},
  {WM_Key_End,      0,                                  UI_EventKind_Navigate, 0,                                         UI_EventDeltaUnit_Line,  {+1, +0}},
  {WM_Key_Home,     WM_Modifier_Shift,                  UI_EventKind_Navigate, UI_EventFlag_KeepMark,                     UI_EventDeltaUnit_Line,  {-1, +0}},
  {WM_Key_End,      WM_Modifier_Shift,                  UI_EventKind_Navigate, UI_EventFlag_KeepMark,                     UI_EventDeltaUnit_Line,  {+1, +0}},

  // rad: copy
  {WM_Key_C,        WM_Modifier_Ctrl,                   UI_EventKind_Edit,     UI_EventFlag_Copy|UI_EventFlag_KeepMark,   UI_EventDeltaUnit_Null,  {+0, +0}},
  {WM_Key_Insert,   WM_Modifier_Ctrl,                   UI_EventKind_Edit,     UI_EventFlag_Copy|UI_EventFlag_KeepMark,   UI_EventDeltaUnit_Null,  {+0, +0}},

  // rad: cut
  {WM_Key_X,        WM_Modifier_Ctrl,                   UI_EventKind_Edit,     UI_EventFlag_Copy|UI_EventFlag_Delete,     UI_EventDeltaUnit_Null,  {+0, +0}},

  // rad: delete / backspace
  {WM_Key_Delete,   0,                                  UI_EventKind_Edit,     UI_EventFlag_Delete|UI_EventFlag_ZeroDeltaOnSelect, UI_EventDeltaUnit_Char, {+1, +0}},
  {WM_Key_Backspace,0,                                  UI_EventKind_Edit,     UI_EventFlag_Delete|UI_EventFlag_ZeroDeltaOnSelect, UI_EventDeltaUnit_Char, {-1, +0}},
#if !OS_MAC
  {WM_Key_Delete,   WM_Modifier_Ctrl,                   UI_EventKind_Edit,     UI_EventFlag_Delete|UI_EventFlag_ZeroDeltaOnSelect, UI_EventDeltaUnit_Word, {+1, +0}},
  {WM_Key_Backspace,WM_Modifier_Ctrl,                   UI_EventKind_Edit,     UI_EventFlag_Delete|UI_EventFlag_ZeroDeltaOnSelect, UI_EventDeltaUnit_Word, {-1, +0}},
#endif
};

internal B32
rad_push_text_nav_ui_events(RAD_Window *ws, Arena *arena, WM_Event *event)
{
  B32 result = 0;

  // rad: select all -> two events (same as raddbg's RD_CmdKind_SelectAll)
  if(event->key == WM_Key_A && event->modifiers == WM_Modifier_Ctrl)
  {
    UI_Event evt1 = zero_struct;
    evt1.kind       = UI_EventKind_Navigate;
    evt1.delta_unit = UI_EventDeltaUnit_Whole;
    evt1.delta_2s32 = v2s32(-1, +0);
    ui_event_list_push(arena, &ws->ui_events, &evt1);
    UI_Event evt2 = zero_struct;
    evt2.kind       = UI_EventKind_Navigate;
    evt2.flags      = UI_EventFlag_KeepMark;
    evt2.delta_unit = UI_EventDeltaUnit_Whole;
    evt2.delta_2s32 = v2s32(+1, +0);
    ui_event_list_push(arena, &ws->ui_events, &evt2);
    result = 1;
  }

  // rad: paste -> text event with the clipboard contents (same as raddbg's RD_CmdKind_Paste)
  if(!result && ((event->key == WM_Key_V && event->modifiers == WM_Modifier_Ctrl) ||
                 (event->key == WM_Key_Insert && event->modifiers == WM_Modifier_Shift)))
  {
    UI_Event evt = zero_struct;
    evt.kind   = UI_EventKind_Text;
    evt.flags  = UI_EventFlag_Paste;
    evt.string = wm_get_clipboard_text(arena);
    if(evt.string.size != 0)
    {
      ui_event_list_push(arena, &ws->ui_events, &evt);
    }
    result = 1;
  }

  // rad: tab -> indentation (4 spaces, the Rust convention); the key stays
  // on the event (a completion list commits on Tab instead)
  if(!result && event->key == WM_Key_Tab && event->modifiers == 0)
  {
    UI_Event evt = zero_struct;
    evt.kind   = UI_EventKind_Text;
    evt.key    = WM_Key_Tab;
    evt.string = str8_lit("    ");
    ui_event_list_push(arena, &ws->ui_events, &evt);
    result = 1;
  }

  // rad: table
  if(!result) for EachElement(idx, rad_text_nav_binding_table)
  {
    RAD_TextNavBinding *b = &rad_text_nav_binding_table[idx];
    if(b->key == event->key && b->modifiers == event->modifiers)
    {
      UI_Event evt = zero_struct;
      evt.kind       = b->kind;
      evt.flags      = b->flags;
      evt.delta_unit = b->delta_unit;
      evt.delta_2s32 = b->delta;
      evt.timestamp_us = event->timestamp_us;
      ui_event_list_push(arena, &ws->ui_events, &evt);
      result = 1;
      break;
    }
  }
  return result;
}

internal void
rad_frame(void)
{
  ProfBeginFunction();
  Temp scratch = scratch_begin(0, 0);
  log_scope_begin();

  //////////////////////////////
  //- rad: do per-frame resets
  //
  arena_clear(rad_frame_arena());

  //////////////////////////////
  //- rad: settings: defaults + the user file (rad.mdesk RAD_SettingTable)
  //
  rad_state->settings = rad_settings_from_cfg();

  //////////////////////////////
  //- rad: get fonts from settings (raddbg's "get fonts from config", with
  // `use_alternative_font_for_ui`: a name or a path; one that does not open
  // -> the built-in font)
  //
  {
    String8 names[] = {rad_state->settings.main_font, rad_state->settings.code_font, rad_state->settings.panel_font};
    RAD_FontSlot slots[] = {RAD_FontSlot_Main, RAD_FontSlot_Code, RAD_FontSlot_Panel};
    String8 *defaults[] = {&rad_default_main_font_bytes, &rad_default_code_font_bytes, 0};
    for EachElement(idx, slots)
    {
      FNT_Tag tag = fnt_tag_zero();
      if(names[idx].size != 0)
      {
        tag = fnt_tag_from_path(names[idx]);
      }
      rad_state->font_slot_is_custom[slots[idx]] = !fnt_tag_match(tag, fnt_tag_zero());
      if(fnt_tag_match(tag, fnt_tag_zero()))
      {
        // rad: the panel font falls back to the code font (set just before)
        tag = (defaults[idx] != 0 ? fnt_tag_from_static_data_string(defaults[idx]) : rad_state->font_slot_table[RAD_FontSlot_Code]);
      }
      rad_state->font_slot_table[slots[idx]] = tag;
    }
  }

  //////////////////////////////
  //- rad: tool window layout of every window (before commands run)
  //
  {
    CFG_NodePtrList windows = cfg_node_top_level_list_from_string(scratch.arena, str8_lit("window"));
    for(CFG_NodePtrNode *n = windows.first; n != 0; n = n->next)
    {
      rad_window_tool_layout_ensure(n->v);
    }
  }

  //////////////////////////////
  //- rad: scripted tests: run the script (queues simulated input for this frame)
  //
  rad_script_tick();

  //////////////////////////////
  //- rad: get events from the OS
  //
  WM_EventList events = wm_get_events(scratch.arena, rad_state->num_frames_requested == 0 && (!rad_script_is_active() || rad_script_lets_rad_sleep()));

  //////////////////////////////
  //- rad: open windows for all window configs which do not have one yet
  //
  // NOTE(rad): this must happen after `wm_get_events` - the win32 window
  // procedure only applies custom-border handling (no native title bar) once
  // the event arena has been set by the first event pump. raddbg opens its
  // windows inside its frame for the same reason.
  //
  {
    CFG_NodePtrList windows = cfg_node_top_level_list_from_string(scratch.arena, str8_lit("window"));
    for(CFG_NodePtrNode *n = windows.first; n != 0; n = n->next)
    {
      if(rad_window_from_cfg(n->v) == 0)
      {
        rad_window_open(n->v);
        rad_request_frame();
      }
    }
  }

  //////////////////////////////
  //- rad: calculate avg length in us of last many frames
  //
  U64 frame_time_history_avg_us = 0;
  {
    U64 num_frames_in_history = Min(ArrayCount(rad_state->frame_time_us_history), rad_state->frame_index);
    U64 frame_time_history_sum_us = 0;
    if(num_frames_in_history > 0)
    {
      for(U64 idx = 0; idx < num_frames_in_history; idx += 1)
      {
        frame_time_history_sum_us += rad_state->frame_time_us_history[idx];
      }
      frame_time_history_avg_us = frame_time_history_sum_us/num_frames_in_history;
    }
  }

  //////////////////////////////
  //- rad: pick target hz (same as raddbg)
  //
  F32 target_hz = wm_get_system_info()->default_refresh_rate;
  if(rad_state->frame_index > 32)
  {
    F32 possible_alternate_hz_targets[] = {target_hz, 60.f, 75.f, 120.f, 144.f, 165.f, 240.f, 360.f};
    F32 best_target_hz = target_hz;
    S64 best_target_hz_frame_time_us_diff = max_S64;
    for(U64 idx = 0; idx < ArrayCount(possible_alternate_hz_targets); idx += 1)
    {
      F32 candidate = possible_alternate_hz_targets[idx];
      if(candidate <= target_hz)
      {
        U64 candidate_frame_time_us = 1000000/(U64)candidate;
        S64 frame_time_us_diff = (S64)frame_time_history_avg_us - (S64)candidate_frame_time_us;
        if(abs_s64(frame_time_us_diff) < best_target_hz_frame_time_us_diff &&
           frame_time_history_avg_us < candidate_frame_time_us + candidate_frame_time_us/4)
        {
          best_target_hz = candidate;
          best_target_hz_frame_time_us_diff = frame_time_us_diff;
        }
      }
    }
    target_hz = best_target_hz;
  }

  //////////////////////////////
  //- rad: target Hz -> delta time
  //
  rad_state->frame_dt = 1.f/target_hz;

  //////////////////////////////
  //- rad: begin measuring actual per-frame work
  //
  U64 begin_time_us = now_time_us();

  //////////////////////////////
  //- rad: consume events
  //
  ProfScope("consume events")
  {
    for(WM_Event *event = events.first, *next = 0; event != 0; event = next)
    {
      next = event->next;
      RAD_Window *ws = rad_window_from_os(event->window);
      B32 take = 0;
      if(ws != 0)
      {
        rad_state->current_window = ws;
      }

      //- rad: the mouse moved: shown again after typing hid it
      if(event->kind == WM_EventKind_MouseMove && rad_state->mouse_hidden_by_typing &&
         (abs_f32(event->pos.x - rad_state->mouse_hidden_pos.x) >= 1.f ||
          abs_f32(event->pos.y - rad_state->mouse_hidden_pos.y) >= 1.f))
      {
        rad_state->mouse_hidden_by_typing = 0;
        rad_request_frame();
      }

      //- rad: try window close
      //- rad: drag/drop drop-kickoff (raddbg's); Esc cancels a drag
      if(event->kind == WM_EventKind_Release && event->key == WM_Key_LeftMouseButton)
      {
        rad_state->drag_killed = 0;
        if(rad_drag_is_active())
        {
          rad_state->drag_drop_state = RAD_DragDropState_Dropping;
        }
      }
      if(!take && rad_drag_is_active() && event->kind == WM_EventKind_Press && event->key == WM_Key_Esc)
      {
        rad_drag_kill();
        take = 1;
      }

      if(!take && event->kind == WM_EventKind_WindowClose && ws != 0)
      {
        take = 1;
        rad_cmd(RAD_CmdKind_Exit);
      }

      //- rad: try menu bar operations (same as raddbg)
      if(ws != 0 && rad_state->settings.focus_menu_bar_with_alt && wm_window_is_focused(ws->os))
      {
        // NOTE(rad): not in raddbg: as in Windows, Alt focuses the menu bar only
        // when released with nothing pressed or scrolled while it was held.
        // raddbg cancels only for bound hotkeys and F keys, so Alt+Up/Down,
        // Alt+Shift+arrows or Alt+click focused the menu bar on release.
        if(ws->menu_bar_focus_press_started &&
           ((event->kind == WM_EventKind_Press && event->key != WM_Key_Alt) || event->kind == WM_EventKind_Scroll))
        {
          ws->menu_bar_focus_press_started = 0;
        }
        if(!take && event->kind == WM_EventKind_Press && event->key == WM_Key_Alt && event->modifiers == 0 && event->is_repeat == 0)
        {
          take = 1;
          rad_request_frame();
          ws->menu_bar_focused_on_press = ws->menu_bar_focused;
          ws->menu_bar_key_held = 1;
          ws->menu_bar_focus_press_started = 1;
        }
        if(!take && event->kind == WM_EventKind_Release && event->key == WM_Key_Alt && event->modifiers == 0 && event->is_repeat == 0)
        {
          take = 1;
          rad_request_frame();
          ws->menu_bar_key_held = 0;
        }
        if(ws->menu_bar_focused && event->kind == WM_EventKind_Press && event->key == WM_Key_Alt && event->modifiers == 0 && event->is_repeat == 0)
        {
          take = 1;
          rad_request_frame();
          ws->menu_bar_focused = 0;
        }
        else if(ws->menu_bar_focus_press_started && !ws->menu_bar_focused && event->kind == WM_EventKind_Release && event->modifiers == 0 && event->key == WM_Key_Alt && event->is_repeat == 0)
        {
          take = 1;
          rad_request_frame();
          ws->menu_bar_focused = !ws->menu_bar_focused_on_press;
          ws->menu_bar_focus_press_started = 0;
        }
        else if(event->kind == WM_EventKind_Press && event->key == WM_Key_Esc && ws->menu_bar_focused)
        {
          take = 1;
          rad_request_frame();
          ws->menu_bar_focused = 0;
        }
      }

      //- rad: try hotkey presses
      if(!take && event->kind == WM_EventKind_Press)
      {
        RAD_CmdKind k = rad_cmd_kind_from_key(event->key, event->modifiers);
        if(k != RAD_CmdKind_Null)
        {
          take = 1;
          rad_cmd(k);
          if(ws != 0 && event->modifiers & WM_Modifier_Alt)
          {
            ws->menu_bar_focus_press_started = 0;
          }
        }
        if(!take && ws != 0 && WM_Key_F1 <= event->key && event->key <= WM_Key_F19)
        {
          ws->menu_bar_focus_press_started = 0;
        }
        rad_request_frame();
      }

      //- rad: try text navigation bindings -> ui navigation events
      if(!take && ws != 0 && event->kind == WM_EventKind_Press)
      {
        take = rad_push_text_nav_ui_events(ws, scratch.arena, event);
      }

      //- rad: Ctrl+Space (Complete Word) also sends a space character: not text
      if(!take && event->kind == WM_EventKind_Text && event->character == ' ' &&
         (event->modifiers & WM_Modifier_Ctrl) && !(event->modifiers & WM_Modifier_Alt))
      {
        take = 1;
      }

      //- rad: do fall-through -> ui event (same mapping as raddbg's RD_CmdKind_WMEvent)
      if(!take && ws != 0)
      {
        take = 1;
        UI_Event ui_event = zero_struct;
        UI_EventKind kind = UI_EventKind_Null;
        {
          switch(event->kind)
          {
            default:{}break;
            case WM_EventKind_Press:     {kind = UI_EventKind_Press;}break;
            case WM_EventKind_Release:   {kind = UI_EventKind_Release;}break;
            case WM_EventKind_MouseMove: {kind = UI_EventKind_MouseMove;}break;
            case WM_EventKind_Text:      {kind = UI_EventKind_Text;}break;
            case WM_EventKind_Scroll:    {kind = UI_EventKind_Scroll;}break;
            case WM_EventKind_FileDrop:  {kind = UI_EventKind_FileDrop;}break;
          }
        }
        ui_event.kind         = kind;
        ui_event.key          = event->key;
        ui_event.modifiers    = event->modifiers;
        if(event->kind == WM_EventKind_Press && event->modifiers == 0)
        {
          // rad: raddbg binds Return/Esc to its `accept`/`cancel` commands,
          // which push press events with these action slots; here the key
          // press itself carries the slot (Return must still reach editors)
          if(event->key == WM_Key_Return) { ui_event.slot = UI_EventActionSlot_Accept; }
          if(event->key == WM_Key_Esc)    { ui_event.slot = UI_EventActionSlot_Cancel; }
        }
        ui_event.string       = event->character ? str8_from_32(scratch.arena, str32(&event->character, 1)) : str8_zero();
        ui_event.paths        = str8_list_copy(scratch.arena, &event->strings);
        ui_event.pos          = event->pos;
        ui_event.delta_2f32   = event->delta;
        ui_event.timestamp_us = event->timestamp_us;
        ui_event_list_push(scratch.arena, &ws->ui_events, &ui_event);
        rad_request_frame();
      }

      //- rad: take
      if(take)
      {
        wm_eat_event(&events, event);
      }
    }
  }

  //////////////////////////////
  //- rad: run commands
  //
  // NOTE(rad): commands pushed during the previous frame's UI build (and by
  // event consumption above) are executed here, before this frame's UI build,
  // so the UI always reflects their effects. commands may push more commands;
  // those run in later iterations of this loop.
  //
  for(U64 cmd_loop_idx = 0; cmd_loop_idx < 4 && rad_state->cmds[0].count != 0; cmd_loop_idx += 1)
  {
    Swap(Arena *, rad_state->cmds_arenas[0], rad_state->cmds_arenas[1]);
    Swap(RAD_CmdList, rad_state->cmds[0], rad_state->cmds[1]);
    arena_clear(rad_state->cmds_arenas[0]);
    MemoryZeroStruct(&rad_state->cmds[0]);
    for(RAD_CmdNode *n = rad_state->cmds[1].first; n != 0; n = n->next)
    {
      rad_exec_cmd(&n->cmd);
    }
  }

  //////////////////////////////
  //- rad: compute animation rates, given settings (same formulas as raddbg)
  //
  {
    RAD_Settings *settings = &rad_state->settings;
    F32 master_animations_f    = (F32)!!settings->animations;
    F32 scrolling_animations_f = (F32)!!settings->scrolling_animations;
    F32 tooltip_animations_f   = (F32)!!settings->tooltip_animations;
    F32 menu_animations_f      = (F32)!!settings->menu_animations;
    rad_state->catchall_animation_rate        = 1 - master_animations_f*pow_f32(2, (-60.f * rad_state->frame_dt));
    rad_state->menu_animation_rate            = 1 - master_animations_f*menu_animations_f*pow_f32(2, (-70.f * rad_state->frame_dt));
    rad_state->menu_animation_rate__slow      = 1 - master_animations_f*menu_animations_f*pow_f32(2, (-50.f * rad_state->frame_dt));
    rad_state->menu_animation_rate__very_slow = 1 - master_animations_f*menu_animations_f*pow_f32(2, (-20.f * rad_state->frame_dt));
    rad_state->rich_hover_animation_rate      = 1 - master_animations_f*menu_animations_f*pow_f32(2, (-50.f * rad_state->frame_dt));
    rad_state->scrolling_animation_rate       = 1 - master_animations_f*scrolling_animations_f*pow_f32(2, (-60.f * rad_state->frame_dt));
    rad_state->zoom_animation_rate            = 1 - master_animations_f*scrolling_animations_f*pow_f32(2, (-30.f * rad_state->frame_dt));
    rad_state->tooltip_animation_rate         = 1 - master_animations_f*tooltip_animations_f*pow_f32(2, (-60.f * rad_state->frame_dt));
  }

  //////////////////////////////
  //- rad: animate view states (scroll offsets ease to their targets)
  //
  rad_view_state_animate();

  //- rad: animate the popup (raddbg's "animate confirmation")
  {
    F32 rate = rad_state->settings.menu_animations ? 1 - pow_f32(2, (-30.f * rad_state->frame_dt)) : 1.f;
    B32 popup_open = rad_state->popup_active;
    rad_state->popup_t += rate * ((F32)!!popup_open-rad_state->popup_t);
    if(abs_f32(rad_state->popup_t - (F32)!!popup_open) > 0.005f)
    {
      rad_request_frame();
    }
  }

  //////////////////////////////
  //- rad: animate the code zoom (the drawn code font size eases to the
  // setting, or to the wheel's target while it turns; 150 ms after the
  // last wheel step the setting takes it: a whole size). The rate is half
  // the scrolling one, so quick wheel notches blend into one motion
  //
  {
    F32 setting_pt = (F32)rad_state->settings.code_font_size;
    if(rad_state->code_zoom_wheeling)
    {
      if(now_time_us() >= rad_state->code_zoom_wheel_us + 150000)
      {
        rad_state->code_zoom_wheeling = 0;
        F32 settled_pt = round_f32(rad_state->code_zoom_target_pt);
        if(settled_pt != setting_pt)
        {
          rad_setting_set_f64(str8_lit("code_font_size"), (F64)settled_pt);
          setting_pt = (F32)rad_state->settings.code_font_size;
        }
      }
      rad_request_frame();
    }
    F32 target_pt = (rad_state->code_zoom_wheeling ? rad_state->code_zoom_target_pt : setting_pt);
    if(rad_state->code_zoom_pt == 0)
    {
      rad_state->code_zoom_pt = target_pt;
    }
    rad_state->code_zoom_pt += (target_pt - rad_state->code_zoom_pt)*rad_state->zoom_animation_rate;
    if(abs_f32(target_pt - rad_state->code_zoom_pt) < 0.01f)
    {
      rad_state->code_zoom_pt = target_pt;
    }
    else
    {
      rad_request_frame();
    }
  }

  //////////////////////////////
  //- rad: tick documents (load/reload from disk, compaction), visible or not
  //
  rad_documents_tick();

  //////////////////////////////
  //- rad: cargo: take the runner's output and diagnostics, start a waiting
  // command, keep the Output tab's text current
  //
  rad_cargo_tick();
  rad_lsp_tick();
  rad_problems_tick();

  //////////////////////////////
  //- rad: compute all ambiguous paths from view titles (port of raddbg's)
  //
  {
    Temp scratch = scratch_begin(0, 0);
    rad_state->ambiguous_path_slots_count = 512;
    rad_state->ambiguous_path_slots = push_array(rad_frame_arena(), RAD_AmbiguousPathNode *, rad_state->ambiguous_path_slots_count);
    for(RAD_Window *ws = rad_state->first_window; ws != 0; ws = ws->next)
    {
      CFG_Node *window = cfg_node_from_id(ws->cfg_id);
      CFG_PanelTree panel_tree = cfg_panel_tree_from_cfg(scratch.arena, window);
      for(CFG_PanelNode *p = panel_tree.root; p != &cfg_nil_panel_node; p = cfg_panel_node_rec__depth_first_pre(panel_tree.root, p).next)
      {
        for(CFG_NodePtrNode *tab_n = p->tabs.first; tab_n != 0; tab_n = tab_n->next)
        {
          String8 file_path = rad_file_path_from_tab(tab_n->v);
          if(file_path.size != 0)
          {
            String8 name = str8_skip_last_slash(file_path);
            U64 hash = u64_hash_from_str8(lower_from_str8(scratch.arena, name));
            U64 slot_idx = hash%rad_state->ambiguous_path_slots_count;
            RAD_AmbiguousPathNode *node = 0;
            for(RAD_AmbiguousPathNode *n = rad_state->ambiguous_path_slots[slot_idx]; n != 0; n = n->next)
            {
              if(str8_match(n->name, name, StringMatchFlag_CaseInsensitive))
              {
                node = n;
                break;
              }
            }
            if(node == 0)
            {
              node = push_array(rad_frame_arena(), RAD_AmbiguousPathNode, 1);
              SLLStackPush(rad_state->ambiguous_path_slots[slot_idx], node);
              node->name = push_str8_copy(rad_frame_arena(), name);
            }
            str8_list_push(rad_frame_arena(), &node->paths, push_str8_copy(rad_frame_arena(), file_path));
          }
        }
      }
    }
    scratch_end(scratch);
  }

  //////////////////////////////
  //- rad: update/render all windows
  //
  U64 build_begin_us = now_time_us();
  {
    dr_begin_frame(rad_font_from_slot(RAD_FontSlot_Icons));
    for(RAD_Window *w = rad_state->first_window; w != 0; w = w->next)
    {
      rad_state->current_window = w;
      rad_window_frame(w);
      MemoryZeroStruct(&w->ui_events);
    }
  }
  U64 build_us = now_time_us() - build_begin_us;

  //////////////////////////////
  //- rad: garbage collect untouched view states and documents
  //
  rad_view_state_gc();
  rad_documents_gc();
  rad_folder_watches_gc();

  //////////////////////////////
  //- rad: save the session (user data) when it changed: checked every 2 s
  // and at exit
  //
  if(rad_state->user_path.size != 0 && (rad_state->quit || now_time_us() >= rad_state->user_data_check_us))
  {
    rad_state->user_data_check_us = now_time_us() + 2000000;
    String8 user_data = rad_user_data_string(scratch.arena);
    if(u64_hash_from_str8(user_data) != rad_state->user_data_hash)
    {
      rad_user_data_write();
    }
  }
  
  //////////////////////////////
  //- rad: clear frame request state
  //
  if(rad_state->num_frames_requested > 0)
  {
    rad_state->num_frames_requested -= 1;
  }

  //////////////////////////////
  //- rad: hand this frame's timed wake-up to the watcher thread
  //
  rad_state->last_frame_wake_us = rad_state->frame_wake_us;
  if(rad_state->frame_wake_us != 0 && rad_state->num_frames_requested == 0)
  {
    MutexScope(rad_state->watch_mutex)
    {
      if(rad_state->watch_wake_us == 0 || rad_state->frame_wake_us < rad_state->watch_wake_us)
      {
        rad_state->watch_wake_us = rad_state->frame_wake_us;
      }
    }
    cond_var_signal(rad_state->watch_cv);
  }
  rad_state->frame_wake_us = 0;

  //////////////////////////////
  //- rad: submit rendering to all windows
  //
  ProfScope("submit rendering to all windows")
  {
    r_begin_frame();
    for(RAD_Window *w = rad_state->first_window; w != 0; w = w->next)
    {
      r_window_begin_frame(w->os, w->r);
      dr_submit_bucket(w->os, w->r, w->draw_bucket);
      r_window_end_frame(w->os, w->r);
    }
    r_end_frame();
  }

  //////////////////////////////
  //- rad: show windows after first frame
  //
  for(RAD_Window *w = rad_state->first_window; w != 0; w = w->next)
  {
    if(w->frames_alive == 1)
    {
      wm_window_first_paint(w->os);
    }
  }

  //////////////////////////////
  //- rad: determine frame time, record into history
  //
  U64 end_time_us = now_time_us();
  U64 frame_time_us = end_time_us-begin_time_us;
  rad_state->frame_time_us_history[rad_state->frame_index%ArrayCount(rad_state->frame_time_us_history)] = frame_time_us;
  rad_script_frame_end(frame_time_us, build_us);

  //////////////////////////////
  //- rad: end drag/drop if needed (raddbg's: a drop nobody took)
  //
  if(rad_state->drag_drop_state == RAD_DragDropState_Dropping)
  {
    rad_state->drag_drop_state = RAD_DragDropState_Null;
  }

  //////////////////////////////
  //- rad: bump frame time counters
  //
  rad_state->frame_index += 1;
  rad_state->time_in_seconds += rad_state->frame_dt;
  rad_state->time_in_us += frame_time_us;

  //////////////////////////////
  //- rad: collect logs
  //
  {
    LogScopeResult log = log_scope_end(scratch.arena);
    append_data_to_file_path(rad_state->log_path, log.strings[LogMsgKind_Info]);
    if(log.strings[LogMsgKind_UserError].size != 0)
    {
      String8List error_log_lines = str8_split(scratch.arena, log.strings[LogMsgKind_UserError], (U8 *)"\n", 1, 0);
      rad_set_error(str8_list_join(scratch.arena, &error_log_lines, &(StringJoin){.sep = str8_lit(" ")}));
    }
  }

  //////////////////////////////
  //- rad: exiting -> stop a running cargo command (and what it started)
  //
  if(rad_state->quit)
  {
    rad_cargo_shutdown();
    rad_lsp_shutdown();
  }

  //////////////////////////////
  //- rad: fade out errors (same rate as raddbg)
  //
  if(rad_state->error_t > 0.01f)
  {
    rad_state->error_t -= rad_state->frame_dt/8.f;
    rad_request_frame();
  }

  scratch_end(scratch);
  ProfEnd();
}
