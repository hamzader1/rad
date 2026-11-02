// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

#ifndef RAD_VIEWS_H
#define RAD_VIEWS_H

////////////////////////////////
//~ rad: Edit History Types
//
// One record per applied edit, in the coordinate space of the text right
// before that edit. Undo applies the inverse as a new patch, redo re-applies
// the record; so history stays valid when patches are rebased after a save.

typedef struct RAD_EditRecord RAD_EditRecord;
struct RAD_EditRecord
{
  RAD_EditRecord *next;
  RAD_EditRecord *prev;
  Rng1U64 range;
  String8 old_text;
  String8 new_text;
};

////////////////////////////////
//~ rad: Code View Types
//
// Port of raddbg's RD_CodeViewState (raddbg/raddbg_views.h), minus
// debugger/disassembly state. The buffer is the immutable file content plus
// `patches` (see docs/notes/02-code-view.md):
//
//   base blob (content hash `base_hash`, pinned while patches exist)
//     + patches (ordered edit log, each relative to the text after the last)
//     = current text
//
// `version` counts applied patches (edits, undos, redos); `saved_version` is
// the version written by the last save, so dirty == (version != saved_version).

typedef struct RAD_CodeViewState RAD_CodeViewState;
struct RAD_CodeViewState
{
  // rad: stable state
  B32 initialized;
  S64 preferred_column;
  U64 cursor;
  U64 mark;
  UI_ScrollPt2 scroll_pos;

  // rad: patch state
  Arena *patch_arena;
  TXT_PatchList patches;
  U128 base_hash;
  U128 current_hash;

  // rad: editing state
  B32 editable;
  B32 crlf;
  U64 version;
  U64 saved_version;
  Arena *history_arena;
  RAD_EditRecord *history_first;
  RAD_EditRecord *history_last_applied;

  // rad: save state (waiting for the file stream to reload the saved file)
  B32 save_pending;
  U64 save_patch_count;
  Arena *save_arena;
  String8 save_text;

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

internal void rad_code_view_init(RAD_CodeViewState *cv, Arena *arena);
internal B32 rad_code_view_is_dirty(RAD_CodeViewState *cv);
internal void rad_code_view_push_patch(RAD_CodeViewState *cv, Rng1U64 range, String8 replace);
internal void rad_code_view_edit(RAD_CodeViewState *cv, TXT_Patched *patched, Rng1U64 range, String8 replace);
internal B32 rad_code_view_undo(RAD_CodeViewState *cv);
internal B32 rad_code_view_redo(RAD_CodeViewState *cv);
internal void rad_code_view_build(Arena *arena, RAD_CodeViewState *cv, Rng2F32 rect, TXT_LangKind lang_kind, TXT_TextInfo *text_info, String8 text_data);

////////////////////////////////
//~ rad: Text View

internal void rad_text_view_ui(CFG_Node *tab, Rng2F32 rect);
internal B32 rad_text_view_save(CFG_Node *tab);
internal B32 rad_tab_is_dirty(CFG_Node *tab);

#endif // RAD_VIEWS_H
