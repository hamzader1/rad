// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

#ifndef RAD_VIEWS_H
#define RAD_VIEWS_H

////////////////////////////////
//~ rad: Code View Types
//
// Port of raddbg's RD_CodeViewState (raddbg/raddbg_views.h), minus
// debugger/disassembly state. The text itself (base + patches, undo history,
// versions, saving, disk tracking) lives in a TB_Buffer (text_buffer layer).

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
  TB_Buffer *buffer;
  B32 editable;

  // rad: per-frame command info
  S64 goto_line_num;
  B32 initial_center;
  B32 center_cursor;
  B32 contain_cursor;
  B32 force_contain_only;
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
};

typedef struct RAD_ViewStateSlot RAD_ViewStateSlot;
struct RAD_ViewStateSlot
{
  RAD_ViewState *first;
};

////////////////////////////////
//~ rad: View State Functions

internal RAD_ViewState *rad_view_state_from_cfg(CFG_Node *cfg);
internal void rad_view_state_gc(void);

////////////////////////////////
//~ rad: Code Views

internal void rad_code_view_init(RAD_CodeViewState *cv, TXT_LangKind lang_kind);
internal B32 rad_code_view_undo(RAD_CodeViewState *cv);
internal B32 rad_code_view_redo(RAD_CodeViewState *cv);
internal void rad_code_view_build(Arena *arena, RAD_CodeViewState *cv, Rng2F32 rect, TXT_LangKind lang_kind, TXT_TextInfo *text_info, String8 text_data);

////////////////////////////////
//~ rad: Text View

internal void rad_text_view_ui(CFG_Node *tab, Rng2F32 rect);
internal B32 rad_text_view_save(CFG_Node *tab);
internal B32 rad_tab_is_dirty(CFG_Node *tab);

#endif // RAD_VIEWS_H
