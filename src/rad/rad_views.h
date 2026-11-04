// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

#ifndef RAD_VIEWS_H
#define RAD_VIEWS_H

////////////////////////////////
//~ rad: Document Types
//
// One document per open file, shared by every tab that shows the file. The
// text (TB_Buffer: base + patches, undo history, version, saving, disk
// tracking) belongs to the document; cursor, selection and scroll belong to
// each tab (RAD_CodeViewState). Documents are ticked every frame, visible or
// not, and released two frames after no tab touched them.

typedef struct RAD_Document RAD_Document;
struct RAD_Document
{
  RAD_Document *next;
  RAD_Document *prev;
  Arena *arena;
  U64 id;       // unique per document (a reopened file gets a new one)
  String8 path;
  TXT_LangKind lang;
  TB_Buffer *buffer;
  B32 is_missing;
  U64 last_frame_index_touched;
};

////////////////////////////////
//~ rad: Code View Types
//
// Port of raddbg's RD_CodeViewState (raddbg/raddbg_views.h), minus
// debugger/disassembly state. The text is the tab's document's TB_Buffer;
// `buffer_version` is the buffer version `cursor` and `mark` refer to (when
// another tab changed the text, they are mapped through the change log).

typedef struct RAD_CodeViewState RAD_CodeViewState;
struct RAD_CodeViewState
{
  // rad: stable state
  B32 initialized;
  S64 preferred_column;
  U64 cursor;
  U64 mark;
  UI_ScrollPt2 scroll_pos;

  // rad: text
  U64 buffer_version;
  B32 editable;

  // rad: per-frame command info
  S64 goto_line_num;
  S64 goto_column_num;
  B32 goto_column_in_bytes;
  U8 find_text_buffer[1024];
  U64 find_text_size;
  B32 find_text_backward;
  B32 initial_center;
  B32 center_cursor;
  B32 contain_cursor;
  B32 force_contain_only;

  // rad: read-only views that grow (Output): the text size last frame, so a
  // cursor at the end can follow new text; the last double-click, if any
  U64 last_text_size;
  B32 double_clicked;
  U64 double_clicked_off;

  // rad: Complete Word (Ctrl+Space) asked for; the view starts it when built
  B32 complete_word;

  // rad: a completion left the cursor before the closing bracket it inserted:
  // typing that bracket there steps over it (offsets in the text of `typeover_version`; off once
  // the cursor leaves [typeover_min, typeover_off])
  B32 typeover_armed;
  U8 typeover_char;
  U64 typeover_min;
  U64 typeover_off;
  U64 typeover_version;

  // rad: layout of the last build, in window pixels (scripted input aims
  // with it; see rad_script.h)
  Rng2F32 layout_code_rect;
  Rng2F32 layout_text_rect;
  F32 layout_text_x_off;
  F32 layout_line_height;
  Rng1S64 layout_visible_lines;
  FNT_Tag layout_font;
  F32 layout_font_size;
  F32 layout_tab_size;
  B32 layout_link_hovered;   // rad: Ctrl held over an identifier (Go To Definition link)
  Rng1U64 layout_link_range;
};

////////////////////////////////
//~ rad: Explorer Types
//
// The explorer view: a folder tree built like raddbg's watch view (scroll
// list of rows; each row one cell with depth indentation and an expander;
// alternating "alt" row backgrounds; selection = focus-hot row). The root
// folder is the tab's `folder` child. Rows are rebuilt from the file system
// when an expansion changes, and once a second to pick up disk changes.

typedef struct RAD_ExplorerExpansion RAD_ExplorerExpansion;
struct RAD_ExplorerExpansion
{
  RAD_ExplorerExpansion *next;
  RAD_ExplorerExpansion *prev;
  String8 path;
};

typedef struct RAD_ExplorerRow RAD_ExplorerRow;
struct RAD_ExplorerRow
{
  String8 path;
  String8 name;
  S32 depth;
  B32 is_folder;
  B32 is_expanded;
};

typedef struct RAD_ExplorerState RAD_ExplorerState;
struct RAD_ExplorerState
{
  B32 initialized;

  // rad: expanded folders (full paths)
  Arena *expansions_arena;
  RAD_ExplorerExpansion *first_expansion;
  RAD_ExplorerExpansion *last_expansion;
  RAD_ExplorerExpansion *free_expansion;

  // rad: rows (flattened visible tree)
  Arena *rows_arena;
  String8 rows_root;
  RAD_ExplorerRow *rows;
  U64 rows_count;
  U64 rows_build_us;
  B32 rows_dirty;

  // rad: selection + scroll (cursor.y = 1-based row, 0 = none)
  UI_ScrollPt2 scroll_pos;
  Vec2S64 cursor;
  Vec2S64 mark;
  U8 selected_path_buffer[4096];
  U64 selected_path_size;

  // rad: search filter (VS2022's Search Solution Explorer): non-empty ->
  // the rows are the matches in the whole tree, with their parent folders
  B32 filter_focused;
  U8 filter_buffer[256];
  U64 filter_size;
  U64 filter_cursor;
  U64 filter_mark;
  U8 rows_filter_buffer[256];
  U64 rows_filter_size;
  Rng2F32 layout_filter_rect;

  // rad: layout of the last build, in window pixels (scripted input aims
  // with it; see rad_script.h). `layout_first_row_rect` is the rect of row
  // `layout_first_row` (0-based); rows are `layout_row_height` apart.
  Rng2F32 layout_rect;
  S64 layout_first_row;
  Rng2F32 layout_first_row_rect;
  F32 layout_row_height;
  F32 layout_font_size;
};

////////////////////////////////
//~ rad: Problems Types
//
// The Problems view: the last cargo run's diagnostics, one row each, built
// like the explorer (raddbg's watch-table rows). Enter or a double-click
// goes to the location; Next/Previous Error (F8 / Shift+F8) move the
// selection too.

typedef struct RAD_ProblemsState RAD_ProblemsState;
struct RAD_ProblemsState
{
  // rad: selection + scroll (cursor.y = 1-based row, 0 = none)
  UI_ScrollPt2 scroll_pos;
  Vec2S64 cursor;
  Vec2S64 mark;
  U64 diags_gen_seen;

  // rad: layout of the last build (scripted input aims with it)
  Rng2F32 layout_rect;
  S64 layout_first_row;
  Rng2F32 layout_first_row_rect;
  F32 layout_row_height;
};

////////////////////////////////
//~ rad: Settings View Types

typedef struct RAD_SettingsState RAD_SettingsState;
struct RAD_SettingsState
{
  // rad: selection + scroll (cursor.y = 1-based row, 0 = none)
  UI_ScrollPt2 scroll_pos;
  Vec2S64 cursor;
  Vec2S64 mark;

  // rad: filter row (a line edit; has the keyboard while `filter_focused`)
  B32 filter_focused;
  U8 filter_buffer[256];
  U64 filter_size;
  U64 filter_cursor;
  U64 filter_mark;

  // rad: the setting whose options menu is open (enums, the theme)
  U64 menu_setting;

  // rad: layout of the last build (scripted input aims with it); per
  // setting, zero if not visible
  U64 layout_frame;
  Rng2F32 layout_filter_rect;
  U64 layout_rows_count;
  U64 layout_row_settings[RAD_SettingCode_COUNT];
  Rng2F32 layout_row_rects[RAD_SettingCode_COUNT];
  Rng2F32 layout_control_rects[RAD_SettingCode_COUNT];
  Rng2F32 layout_revert_rects[RAD_SettingCode_COUNT];
  Rng2F32 layout_menu_rects[64];
  U64 layout_menu_count;
  U64 layout_menu_frame;
};

////////////////////////////////
//~ rad: View State Cache Types

typedef struct RAD_ViewState RAD_ViewState;
struct RAD_ViewState
{
  RAD_ViewState *hash_next;
  CFG_ID cfg_id;
  U64 last_frame_index_touched;
  Arena *arena;
  RAD_CodeViewState cv;
  RAD_ExplorerState ex;
  RAD_ProblemsState pr;
  RAD_SettingsState ss;

  // rad: the tab's own boxes, last layout (scripted input aims with them)
  U64 layout_tab_frame;
  Rng2F32 layout_tab_rect;
  Rng2F32 layout_tab_pin_rect;
  Rng2F32 layout_tab_close_rect;

  // rad: query bar (port of raddbg's RD_ViewState query state; the command
  // and committed input live in the tab's `query` cfg child)
  B32 query_is_open;
  B32 contents_are_focused;
  U64 query_cursor;
  U64 query_mark;
  U8 query_buffer[1024];
  U64 query_string_size;

  // rad: loading overlay (port of raddbg's RD_ViewState loading info)
  U64 last_frame_index_built;
  F32 loading_t;
  F32 loading_t_target;
  U64 loading_progress_v;
  U64 loading_progress_v_target;
};

typedef struct RAD_ViewStateSlot RAD_ViewStateSlot;
struct RAD_ViewStateSlot
{
  RAD_ViewState *first;
};

////////////////////////////////
//~ rad: View State Functions

internal RAD_ViewState *rad_view_state_from_cfg(CFG_Node *cfg);
internal void rad_store_view_loading_info(CFG_Node *tab, B32 is_loading, U64 progress_u64, U64 progress_u64_target);
internal void rad_view_state_animate(void);
internal void rad_view_state_gc(void);

////////////////////////////////
//~ rad: Documents

internal RAD_Document *rad_document_lookup(String8 path);
internal RAD_Document *rad_document_from_path(String8 path);
internal RAD_Document *rad_document_from_tab(CFG_Node *tab);
internal U64 rad_document_tab_count(RAD_Document *doc);
internal B32 rad_document_save(RAD_Document *doc);
internal void rad_documents_tick(void);
internal void rad_documents_gc(void);

////////////////////////////////
//~ rad: Code Views

internal void rad_code_view_init(RAD_CodeViewState *cv);
internal B32 rad_code_view_undo(RAD_CodeViewState *cv, TB_Buffer *buffer);
internal B32 rad_code_view_redo(RAD_CodeViewState *cv, TB_Buffer *buffer);
internal void rad_code_view_build(Arena *arena, CFG_Node *tab, RAD_CodeViewState *cv, TB_Buffer *buffer, Rng2F32 rect, TXT_LangKind lang_kind, TXT_TextInfo *text_info, String8 text_data);

////////////////////////////////
//~ rad: Completion (the language server's list; see rad_lsp.h)

internal B32 rad_char_is_word(U8 c);
internal void rad_code_view_completion_commit(Arena *arena, RAD_Document *doc, TB_Buffer *buffer, TXT_TextInfo *text_info, String8 text_data, TXT_Patched *text_patched, U64 *cursor, RAD_CodeViewState *cv);
internal void rad_code_view_completion_update(CFG_Node *tab, RAD_CodeViewState *cv, TB_Buffer *buffer, TXT_Patched *text, B32 focused, B32 typed, U8 typed_char, B32 moved);
internal void rad_completion_popup_ui(RAD_Window *ws, CFG_Node *window);

////////////////////////////////
//~ rad: Text View

internal void rad_text_view_ui(CFG_Node *tab, Rng2F32 rect);
internal B32 rad_text_view_save(CFG_Node *tab);
internal B32 rad_tab_is_dirty(CFG_Node *tab);

////////////////////////////////
//~ rad: Explorer

internal String8 rad_explorer_folder_from_tab(CFG_Node *tab);
internal void rad_explorer_set_folder(CFG_Node *tab, String8 folder);
internal B32 rad_explorer_is_expanded(RAD_ExplorerState *ex, String8 path);
internal void rad_explorer_set_expanded(RAD_ExplorerState *ex, String8 path, B32 expanded);
internal void rad_explorer_rows_build(RAD_ExplorerState *ex, String8 root);
internal CFG_Node *rad_explorer_target_panel_from_tab(CFG_Node *tab);
internal void rad_explorer_view_ui(CFG_Node *tab, Rng2F32 rect);

////////////////////////////////
//~ rad: Output + Problems

internal void rad_output_view_ui(CFG_Node *tab, Rng2F32 rect);
internal void rad_problems_view_ui(CFG_Node *tab, Rng2F32 rect);
internal void rad_goto_diagnostic(CFG_Node *window, U64 diag_num);

////////////////////////////////
//~ rad: Settings

internal void rad_settings_view_ui(CFG_Node *tab, Rng2F32 rect);

#endif // RAD_VIEWS_H
