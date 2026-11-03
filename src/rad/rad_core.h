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
//~ rad: Command Query Types
//
// Port of raddbg's RD_Query: a command that needs an input (a line number,
// a string) and is run without one opens the query bar of the tab it targets;
// Enter runs it with the typed input (see rad.mdesk, RAD_CmdTable q_* columns).

typedef enum RAD_QuerySlot
{
  RAD_QuerySlot_Null,
  RAD_QuerySlot_LineNum,
  RAD_QuerySlot_String,
}
RAD_QuerySlot;

typedef U32 RAD_QueryFlags;
enum
{
  RAD_QueryFlag_KeepOldInput   = (1<<0),
  RAD_QueryFlag_SelectOldInput = (1<<1),
  RAD_QueryFlag_CodeInput      = (1<<2),
  RAD_QueryFlag_Required       = (1<<3),
};

typedef struct RAD_Query RAD_Query;
struct RAD_Query
{
  RAD_QuerySlot slot;
  RAD_QueryFlags flags;
};

////////////////////////////////
//~ rad: Folder Watch Types
//
// One OS folder watch (base: file_watch_*) per watched folder, checked by a
// background thread every 100 ms; the UI thread takes the change flag.

typedef enum RAD_FolderWatchChange
{
  RAD_FolderWatchChange_None,
  RAD_FolderWatchChange_Changed,
  RAD_FolderWatchChange_NoWatch,
}
RAD_FolderWatchChange;

typedef struct RAD_FolderWatch RAD_FolderWatch;
struct RAD_FolderWatch
{
  RAD_FolderWatch *next;
  U8 path_buffer[1024];
  U64 path_size;
  FileWatch watch;
  U32 changed;
  U64 last_frame_index_touched;
};

////////////////////////////////
//~ rad: Ambiguous Path Types
//
// Port of raddbg's RD_AmbiguousPathNode: per frame, every tab's file path,
// grouped by file name, so tab titles of same-named files get qualifiers
// ("<src> main.rs", "<tests> main.rs").

typedef struct RAD_AmbiguousPathNode RAD_AmbiguousPathNode;
struct RAD_AmbiguousPathNode
{
  RAD_AmbiguousPathNode *next;
  String8 name;
  String8List paths;
};

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
  B32 cursor_scope_lines;
  B32 cursor_scope_end_annotations;
  B32 cursor_trail;
  RAD_ThemePreset theme_preset;

  //- rad: text view settings (defaults: raddbg.mdesk @schema text)
  B32 show_line_numbers;

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
  S64 line_num;
  String8 string;
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

  //- rad: user-facing error / warning (shown in the bottom bar, same as raddbg)
  U8 error_buffer[512];
  U64 error_string_size;
  F32 error_t;

  //- rad: "unsaved changes" confirmation (second close/exit within a short
  // time discards the changes)
  CFG_ID discard_confirm_cfg_id;
  U64 discard_confirm_deadline_us;

  //- rad: last focused panel that is not showing an explorer (where the
  // explorer opens files)
  CFG_ID last_editor_panel;

  //- rad: tab file paths by file name (this frame; for tab titles)
  U64 ambiguous_path_slots_count;
  RAD_AmbiguousPathNode **ambiguous_path_slots;

  //- rad: per-tab view states
  U64 view_state_slots_count;
  struct RAD_ViewStateSlot *view_state_slots;
  struct RAD_ViewState *free_view_state;

  // rad: folder watches + the watcher thread, which wakes the UI when a
  // watched folder or a streamed file (file_stream change gen) changes
  Mutex watch_mutex;
  struct RAD_FolderWatch *first_watch;
  struct RAD_FolderWatch *free_watch;
  U64 watch_fs_change_gen;
  Thread watch_thread;
  U64 docs_fs_change_gen;
  U64 docs_refresh_until_us;

  // rad: user data file (session: windows, panels, tabs, explorer folder;
  // port of raddbg's user file). empty path = no file (scripted tests)
  String8 user_path;
  U64 user_data_hash;
  U64 user_data_check_us;

  // rad: documents (one text buffer per open file, shared by its tabs)
  struct RAD_Document *first_document;
  struct RAD_Document *last_document;
  struct RAD_Document *free_document;

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
internal void rad_set_error(String8 string);

////////////////////////////////
//~ rad: Commands

internal void rad_push_cmd(RAD_CmdKind kind, RAD_CmdParams *params);
#define rad_cmd(kind, ...) rad_push_cmd((kind), &(RAD_CmdParams){.window = 0, __VA_ARGS__})
internal void rad_exec_cmd(RAD_Cmd *cmd);

////////////////////////////////
//~ rad: Folder Watch Functions

internal RAD_FolderWatchChange rad_folder_watch_take_change(String8 folder);
internal void rad_folder_watches_gc(void);
internal void rad_watch_thread_entry_point(void *p);

////////////////////////////////
//~ rad: User Data (Session) Functions

internal String8 rad_user_data_string(Arena *arena);
internal B32 rad_user_data_load(String8 path);
internal void rad_user_data_write(void);

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
internal CFG_Node *rad_explorer_tab_from_panel(CFG_Node *panel);
internal CFG_Node *rad_panel_alloc_at_window_edge(CFG_Node *window, Side side, F32 pct);
internal RAD_CmdKind rad_cmd_kind_from_code_name(String8 code_name);
internal String8 rad_tab_query_input(CFG_Node *tab);
internal void rad_tab_query_open(CFG_Node *tab, RAD_CmdKind kind);
internal void rad_tab_query_complete(CFG_Node *tab);
internal String8 rad_file_path_from_tab(CFG_Node *tab);
internal String8List rad_qualifiers_from_file_path(Arena *arena, String8 file_path);
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
