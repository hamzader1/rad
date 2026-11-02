// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

#ifndef RAD_CORE_H
#define RAD_CORE_H

////////////////////////////////
//~ rad: Notes
//
// The `rad` layer is the IDE frontend. It is built directly on the same
// lower layers as the RAD Debugger frontend (`raddbg`) - base, ui, draw,
// render, font_cache, window_manager, config - but it does not depend on any
// debugger layer. Window chrome (top bar, bottom bar, panels, tabs) and box
// rendering are ported from `raddbg/raddbg_core.c` with the same `ui_` calls,
// sizes, tags, and theme values, so the IDE looks identical to the debugger.
//
// Where the debugger reads settings through its config tree + expression
// evaluator, this layer uses `RAD_Settings`, filled with the exact defaults
// from the debugger's user/window schemas (`raddbg/raddbg.mdesk`).

////////////////////////////////
//~ rad: Font Slots

typedef enum RAD_FontSlot
{
  RAD_FontSlot_Main,
  RAD_FontSlot_Code,
  RAD_FontSlot_Icons,
  RAD_FontSlot_COUNT
}
RAD_FontSlot;

////////////////////////////////
//~ rad: Generated Code

#include "rad/generated/rad.meta.h"

////////////////////////////////
//~ rad: Settings

typedef struct RAD_Settings RAD_Settings;
struct RAD_Settings
{
  //- rad: user settings (defaults: raddbg.mdesk @schema user)
  B32 animations;
  B32 scrolling_animations;
  B32 tooltip_animations;
  B32 menu_animations;
  B32 opaque_backgrounds;
  B32 background_blur;
  B32 drop_shadows;
  F32 rounded_corner_amount;
  U64 tab_width;
  B32 focus_menu_bar_with_alt;
  RAD_ThemePreset theme_preset;

  //- rad: window settings (defaults: raddbg.mdesk @schema window)
  B32 smooth_ui_text;
  B32 hint_ui_text;
  B32 smooth_code_text;
  B32 hint_code_text;
  F32 font_size;
  F32 row_height;
  F32 tab_height;
};

////////////////////////////////
//~ rad: Commands

typedef struct RAD_CmdParams RAD_CmdParams;
struct RAD_CmdParams
{
  CFG_ID window;
  CFG_ID panel;
  CFG_ID tab;
  String8 file_path;
};

typedef struct RAD_Cmd RAD_Cmd;
struct RAD_Cmd
{
  RAD_CmdKind kind;
  RAD_CmdParams params;
};

typedef struct RAD_CmdNode RAD_CmdNode;
struct RAD_CmdNode
{
  RAD_CmdNode *next;
  RAD_Cmd cmd;
};

typedef struct RAD_CmdList RAD_CmdList;
struct RAD_CmdList
{
  RAD_CmdNode *first;
  RAD_CmdNode *last;
  U64 count;
};

////////////////////////////////
//~ rad: Window State

typedef struct RAD_Window RAD_Window;
struct RAD_Window
{
  RAD_Window *next;
  RAD_Window *prev;

  //- rad: allocation & identity
  Arena *arena;
  CFG_ID cfg_id;

  //- rad: os/render/ui handles
  WM_Window os;
  R_Handle r;
  UI_State *ui;
  UI_EventList ui_events;
  DR_Bucket *draw_bucket;

  //- rad: per-frame artifacts
  UI_Theme *theme;
  FNT_RasterFlags font_slot_raster_flags[RAD_FontSlot_COUNT];
  Vec4F32 theme_code_colors[RAD_CodeColorSlot_COUNT];

  //- rad: persistent state
  F32 font_size;
  F32 last_dpi;
  U64 frames_alive;
  Rng2F32 last_window_rect;
  B32 window_layout_reset;

  //- rad: menu bar state
  B32 menu_bar_focused;
  B32 menu_bar_focused_on_press;
  B32 menu_bar_key_held;
  B32 menu_bar_focus_press_started;
};

////////////////////////////////
//~ rad: Main State

typedef struct RAD_State RAD_State;
struct RAD_State
{
  //- rad: allocation
  Arena *arena;
  Arena *frame_arenas[2];

  //- rad: logging
  Log *log;
  String8 log_path;

  //- rad: frame state
  B32 quit;
  U64 frame_index;
  F32 frame_dt;
  F64 time_in_seconds;
  U64 time_in_us;
  U64 frame_time_us_history[64];
  U64 num_frames_requested;

  //- rad: animation rates (computed each frame from settings, same formulas as raddbg)
  F32 catchall_animation_rate;
  F32 menu_animation_rate;
  F32 menu_animation_rate__slow;
  F32 menu_animation_rate__very_slow;
  F32 rich_hover_animation_rate;
  F32 scrolling_animation_rate;
  F32 tooltip_animation_rate;

  //- rad: commands (slot 0: being pushed, slot 1: being executed)
  Arena *cmds_arenas[2];
  RAD_CmdList cmds[2];

  //- rad: settings
  RAD_Settings settings;

  //- rad: config tree (window/panel/tab layout)
  CFG_State *cfg;
  CFG_SchemaTable *cfg_schema_table;

  //- rad: theme presets
  MD_Node *theme_preset_trees[RAD_ThemePreset_COUNT];

  //- rad: fonts & icons
  FNT_Tag font_slot_table[RAD_FontSlot_COUNT];
  R_Handle icon_texture;

  //- rad: windows
  RAD_Window *first_window;
  RAD_Window *last_window;
  RAD_Window *free_window;
  RAD_Window *current_window;
};

////////////////////////////////
//~ rad: Globals

global RAD_State *rad_state = 0;

////////////////////////////////
//~ rad: Settings Functions

internal RAD_Settings rad_default_settings(void);

////////////////////////////////
//~ rad: Main State Accessors

internal Arena *rad_frame_arena(void);
internal void rad_request_frame(void);

////////////////////////////////
//~ rad: Commands

internal void rad_push_cmd(RAD_CmdKind kind, RAD_CmdParams *params);
#define rad_cmd(kind, ...) rad_push_cmd((kind), &(RAD_CmdParams){.window = 0, __VA_ARGS__})
internal void rad_exec_cmd(RAD_Cmd *cmd);

////////////////////////////////
//~ rad: Fonts

internal FNT_Tag rad_font_from_slot(RAD_FontSlot slot);
internal FNT_RasterFlags rad_raster_flags_from_slot(RAD_FontSlot slot);
#define RAD_Font(slot) UI_Font(rad_font_from_slot(slot)) UI_TextRasterFlags(rad_raster_flags_from_slot((slot)))

////////////////////////////////
//~ rad: Config Helpers

internal CFG_Node *rad_window_cfg_from_cfg(CFG_Node *cfg);
internal void rad_focus_panel(CFG_Node *panel);
internal void rad_focus_tab(CFG_Node *tab);
internal String8 rad_file_path_from_tab(CFG_Node *tab);
internal DR_FStrList rad_title_fstrs_from_tab(Arena *arena, CFG_Node *tab);

////////////////////////////////
//~ rad: Windows

internal String8 rad_push_window_title(Arena *arena);
internal RAD_Window *rad_window_open(CFG_Node *window_cfg);
internal RAD_Window *rad_window_from_os(WM_Window os);
internal RAD_Window *rad_window_from_cfg(CFG_Node *window_cfg);
internal void rad_view_ui(CFG_Node *tab, Rng2F32 rect);
internal void rad_window_frame(RAD_Window *ws);

////////////////////////////////
//~ rad: Main Layer Top-Level Calls

internal void rad_init(CmdLine *cmdln);
internal void rad_frame(void);

#endif // RAD_CORE_H
