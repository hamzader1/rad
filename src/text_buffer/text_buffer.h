// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

#ifndef TEXT_BUFFER_H
#define TEXT_BUFFER_H

////////////////////////////////
//~ rad: Notes
//
// An editable text buffer on top of the immutable `content` and `text`
// caches. Depends only on `base`, `content` and `text`.
//
//   base blob (content hash `base_hash`, pinned, already lexed by `text`)
//     + patches (ordered edit log, each relative to the text after the last)
//     = current text
//
// Edits push patches. `tb_buffer_tick` compacts: once edits pause (or the
// patch list gets long), it submits the current text as a new content blob
// under the buffer's own key. When the text layer has lexed that blob, it
// becomes the new base and the patches it contains are dropped. Compaction
// does not change the text, so it does not change `version`.
//
// `version` increments on every change of the text: edit, undo, redo,
// reload from disk.
//
// Change log: the last TB_CHANGE_LOG_SIZE changes, one per version, so an
// offset taken at an older version (another view's cursor, a language
// server's answer) can be mapped to the current text with
// `tb_buffer_map_offset`. Offsets stick to the character after them: an
// insertion at an offset pushes it right; an offset whose next character is
// replaced moves to the end of the replacement. A reload (or a version older
// than the log) cannot be mapped.
//
// Undo history: one record per edit (consecutive typing / deleting merges
// into one record until a pause), in the coordinates of the text right
// before that edit, so it stays valid across compactions. Dirty means the
// history position differs from the one at the last save or load.
//
// Disk tracking: the caller passes the hash of the file's current content
// to `tb_buffer_observe_source` every frame. The buffer reacts only when
// that hash changes:
//   - the hash of the bytes it wrote itself -> nothing to do;
//   - anything else, buffer clean -> reload (new base, edits and history
//     cleared);
//   - anything else, buffer dirty -> keep the edits, set `changed_on_disk`
//     (reloads later if the buffer becomes clean again, e.g. by undo).

////////////////////////////////
//~ rad: Tunables

#define TB_COMPACT_PAUSE_US      100000
#define TB_COMPACT_MAX_PATCHES   32
#define TB_UNDO_GROUP_PAUSE_US   1000000
#define TB_UNDO_GROUP_MAX_SIZE   256
#define TB_CHANGE_LOG_SIZE       1024

////////////////////////////////
//~ rad: Change Log Types

typedef struct TB_Change TB_Change;
struct TB_Change
{
  U64 version;    // the version this change produced
  Rng1U64 range;  // replaced range, in the text of version-1
  U64 new_size;   // size of the text that replaced it
  B32 reset;      // whole text replaced (load / reload): not mappable
};

////////////////////////////////
//~ rad: Edit History Types

typedef struct TB_EditRecord TB_EditRecord;
struct TB_EditRecord
{
  TB_EditRecord *next;
  TB_EditRecord *prev;
  Rng1U64 range;
  String8 old_text;
  String8 new_text;
};

////////////////////////////////
//~ rad: Buffer Types

typedef struct TB_Buffer TB_Buffer;
struct TB_Buffer
{
  Arena *arena;
  C_Root root;
  C_Key snapshot_key;
  TXT_LangKind lang;
  B32 crlf;

  // rad: base + patches
  U128 base_hash;
  Arena *patch_arenas[2];
  U64 patch_arena_idx;
  TXT_PatchList patches;

  // rad: compaction in flight (zero hash = none)
  U128 snapshot_hash;
  U64 snapshot_patch_count;

  // rad: versions + timing (timing only advances in tb_buffer_tick)
  U64 version;
  U64 tick_version;
  U64 last_change_us;
  TB_Change *change_log; // [TB_CHANGE_LOG_SIZE], slot = version % size

  // rad: undo history
  Arena *history_arena;
  TB_EditRecord *history_first;
  TB_EditRecord *history_last_applied;
  TB_EditRecord *history_saved;
  B32 history_group_open;

  // rad: disk tracking
  U128 disk_hash;
  U128 last_source_hash;
  B32 changed_on_disk;
};

typedef struct TB_Text TB_Text;
struct TB_Text
{
  U128 base_hash;
  TXT_TextInfo base_info;
  String8 base_data;
  TXT_Patched patched;
};

////////////////////////////////
//~ rad: Buffer Lifetime

internal TB_Buffer *tb_buffer_alloc(TXT_LangKind lang);
internal void tb_buffer_release(TB_Buffer *buffer);

////////////////////////////////
//~ rad: Source Tracking + Ticking

internal void tb_buffer_observe_source(TB_Buffer *buffer, Access *access, U128 source_hash);
internal void tb_buffer_tick(TB_Buffer *buffer, Access *access, U64 now_us);

////////////////////////////////
//~ rad: Queries

internal B32 tb_buffer_is_loaded(TB_Buffer *buffer);
internal B32 tb_buffer_is_dirty(TB_Buffer *buffer);
internal B32 tb_buffer_is_busy(TB_Buffer *buffer);
internal TB_Text tb_text_from_buffer(Arena *arena, Access *access, TB_Buffer *buffer);
internal String8 tb_string_from_buffer(Arena *arena, Access *access, TB_Buffer *buffer);
internal B32 tb_buffer_map_offset(TB_Buffer *buffer, U64 version, U64 *off);

////////////////////////////////
//~ rad: Editing

internal void tb_buffer_edit(TB_Buffer *buffer, TXT_Patched *current, Rng1U64 range, String8 replace);
internal B32 tb_buffer_undo(TB_Buffer *buffer, U64 *cursor_out);
internal B32 tb_buffer_redo(TB_Buffer *buffer, U64 *cursor_out);
internal void tb_buffer_close_undo_group(TB_Buffer *buffer);

////////////////////////////////
//~ rad: Saving

internal B32 tb_buffer_save(TB_Buffer *buffer, Access *access, String8 path);

#endif // TEXT_BUFFER_H
