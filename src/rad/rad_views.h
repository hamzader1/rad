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
  B32 initial_center;
  B32 center_cursor;
  B32 contain_cursor;
  B32 force_contain_only;

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
};

typedef struct RAD_ViewStateSlot RAD_ViewStateSlot;
struct RAD_ViewStateSlot
{
  RAD_ViewState *first;
};

////////////////////////////////
//~ rad: View State Functions

internal RAD_ViewState *rad_view_state_from_cfg(CFG_Node *cfg);
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
internal void rad_code_view_build(Arena *arena, RAD_CodeViewState *cv, TB_Buffer *buffer, Rng2F32 rect, TXT_LangKind lang_kind, TXT_TextInfo *text_info, String8 text_data);

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

#endif // RAD_VIEWS_H
