// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

////////////////////////////////
//~ rad: Internal Helpers

internal void
tb_buffer_set_base__(TB_Buffer *buffer, U128 hash)
{
  // NOTE(rad): inc before dec, so re-setting the same hash never drops it
  if(!u128_match(hash, u128_zero()))
  {
    c_hash_downstream_inc(hash);
  }
  if(!u128_match(buffer->base_hash, u128_zero()))
  {
    c_hash_downstream_dec(buffer->base_hash);
  }
  buffer->base_hash = hash;
}

internal void
tb_buffer_clear_patches__(TB_Buffer *buffer)
{
  arena_clear(buffer->patch_arenas[0]);
  arena_clear(buffer->patch_arenas[1]);
  buffer->patch_arena_idx = 0;
  MemoryZeroStruct(&buffer->patches);
  buffer->snapshot_hash = u128_zero();
  buffer->snapshot_patch_count = 0;
}

internal void
tb_buffer_clear_history__(TB_Buffer *buffer)
{
  arena_clear(buffer->history_arena);
  buffer->history_first = 0;
  buffer->history_last_applied = 0;
  buffer->history_saved = 0;
  buffer->history_group_open = 0;
}

internal void
tb_buffer_load__(TB_Buffer *buffer, Access *access, U128 hash)
{
  TXT_TextInfo info = txt_text_info_from_hash_lang(access, hash, buffer->lang);
  tb_buffer_set_base__(buffer, hash);
  tb_buffer_clear_patches__(buffer);
  tb_buffer_clear_history__(buffer);
  buffer->crlf = (info.line_end_kind == TXT_LineEndKind_CRLF);
  buffer->disk_hash = hash;
  buffer->changed_on_disk = 0;
  buffer->version += 1;
  TB_Change *change = &buffer->change_log[buffer->version%TB_CHANGE_LOG_SIZE];
  MemoryZeroStruct(change);
  change->version = buffer->version;
  change->reset = 1;
}

internal void
tb_buffer_push_patch__(TB_Buffer *buffer, Rng1U64 range, String8 replace)
{
  txt_patch_list_push_new(buffer->patch_arenas[buffer->patch_arena_idx], &buffer->patches, range, replace);
  buffer->version += 1;
  TB_Change *change = &buffer->change_log[buffer->version%TB_CHANGE_LOG_SIZE];
  MemoryZeroStruct(change);
  change->version = buffer->version;
  change->range = range;
  change->new_size = replace.size;
}

////////////////////////////////
//~ rad: Buffer Lifetime

internal TB_Buffer *
tb_buffer_alloc(TXT_LangKind lang)
{
  Arena *arena = arena_alloc();
  TB_Buffer *buffer = push_array(arena, TB_Buffer, 1);
  buffer->arena = arena;
  buffer->root = c_root_alloc();
  buffer->snapshot_key = c_key_make(buffer->root, c_id_make(0, 1));
  buffer->lang = lang;
  buffer->change_log = push_array(arena, TB_Change, TB_CHANGE_LOG_SIZE);
  buffer->patch_arenas[0] = arena_alloc();
  buffer->patch_arenas[1] = arena_alloc();
  buffer->history_arena = arena_alloc();
  return buffer;
}

internal void
tb_buffer_release(TB_Buffer *buffer)
{
  tb_buffer_set_base__(buffer, u128_zero());
  c_root_release(buffer->root);
  arena_release(buffer->history_arena);
  arena_release(buffer->patch_arenas[1]);
  arena_release(buffer->patch_arenas[0]);
  arena_release(buffer->arena);
}

////////////////////////////////
//~ rad: Source Tracking + Ticking

internal void
tb_buffer_observe_source(TB_Buffer *buffer, Access *access, U128 source_hash)
{
  // NOTE(rad): `source_hash` must already be lexed (lines_count != 0), as
  // returned by txt_text_info_from_key_lang; zero means "nothing yet".
  if(!u128_match(source_hash, u128_zero()))
  {
    if(!tb_buffer_is_loaded(buffer))
    {
      buffer->last_source_hash = source_hash;
      tb_buffer_load__(buffer, access, source_hash);
    }
    else if(!u128_match(source_hash, buffer->last_source_hash))
    {
      buffer->last_source_hash = source_hash;
      buffer->changed_on_disk = !u128_match(source_hash, buffer->disk_hash);
    }
  }

  //- rad: the disk has text we don't have -> load it once that loses nothing
  if(buffer->changed_on_disk && !tb_buffer_is_dirty(buffer))
  {
    tb_buffer_load__(buffer, access, buffer->last_source_hash);
  }
}

internal void
tb_buffer_tick(TB_Buffer *buffer, Access *access, U64 now_us)
{
  Temp scratch = scratch_begin(0, 0);

  //- rad: track when the text last changed
  if(buffer->tick_version != buffer->version)
  {
    buffer->tick_version = buffer->version;
    buffer->last_change_us = now_us;
  }
  if(now_us >= buffer->last_change_us + TB_UNDO_GROUP_PAUSE_US)
  {
    buffer->history_group_open = 0;
  }

  //- rad: compaction landed (snapshot lexed) -> it becomes the new base,
  // and the patches it already contains are dropped
  if(!u128_match(buffer->snapshot_hash, u128_zero()))
  {
    TXT_TextInfo info = txt_text_info_from_hash_lang(access, buffer->snapshot_hash, buffer->lang);
    if(info.lines_count != 0)
    {
      tb_buffer_set_base__(buffer, buffer->snapshot_hash);
      U64 next_arena_idx = (buffer->patch_arena_idx + 1) % ArrayCount(buffer->patch_arenas);
      Arena *next_arena = buffer->patch_arenas[next_arena_idx];
      arena_clear(next_arena);
      TXT_PatchList next_patches = {0};
      U64 patch_idx = 0;
      for EachNode(n, TXT_PatchNode, buffer->patches.first)
      {
        if(patch_idx >= buffer->snapshot_patch_count)
        {
          txt_patch_list_push_new(next_arena, &next_patches, n->v.range, n->v.replace);
        }
        patch_idx += 1;
      }
      arena_clear(buffer->patch_arenas[buffer->patch_arena_idx]);
      buffer->patch_arena_idx = next_arena_idx;
      buffer->patches = next_patches;
      buffer->snapshot_hash = u128_zero();
      buffer->snapshot_patch_count = 0;
    }
  }

  //- rad: edits paused, or too many patches -> start a compaction
  if(u128_match(buffer->snapshot_hash, u128_zero()) && buffer->patches.count != 0 &&
     (buffer->patches.count >= TB_COMPACT_MAX_PATCHES || now_us >= buffer->last_change_us + TB_COMPACT_PAUSE_US))
  {
    TB_Text text = tb_text_from_buffer(scratch.arena, access, buffer);
    if(text.base_info.lines_count != 0)
    {
      Arena *data_arena = arena_alloc();
      String8 data = memory_map_data_from_range(data_arena, &text.patched.memory_map, r1u64(0, text.patched.size));
      buffer->snapshot_hash = c_submit_data(buffer->snapshot_key, &data_arena, data);
      buffer->snapshot_patch_count = buffer->patches.count;
    }
  }

  scratch_end(scratch);
}

////////////////////////////////
//~ rad: Queries

internal B32
tb_buffer_is_loaded(TB_Buffer *buffer)
{
  return !u128_match(buffer->base_hash, u128_zero());
}

internal B32
tb_buffer_is_dirty(TB_Buffer *buffer)
{
  return (buffer->history_last_applied != buffer->history_saved);
}

internal B32
tb_buffer_is_busy(TB_Buffer *buffer)
{
  // NOTE(rad): patches pending -> a compaction is waiting or in flight, so
  // the caller should keep ticking
  return (buffer->patches.count != 0);
}

internal TB_Text
tb_text_from_buffer(Arena *arena, Access *access, TB_Buffer *buffer)
{
  TB_Text text = {0};
  text.base_hash = buffer->base_hash;
  if(tb_buffer_is_loaded(buffer))
  {
    text.base_info = txt_text_info_from_hash_lang(access, buffer->base_hash, buffer->lang);
    text.base_data = c_data_from_hash(access, buffer->base_hash);
  }
  if(text.base_info.lines_count != 0)
  {
    text.patched = txt_patched_from_info_data_patches(arena, &text.base_info, text.base_data, &buffer->patches);
  }
  return text;
}

internal String8
tb_string_from_buffer(Arena *arena, Access *access, TB_Buffer *buffer)
{
  Temp scratch = scratch_begin(&arena, 1);
  TB_Text text = tb_text_from_buffer(scratch.arena, access, buffer);
  String8 result = memory_map_data_from_range(arena, &text.patched.memory_map, r1u64(0, text.patched.size));
  scratch_end(scratch);
  return result;
}

internal B32
tb_buffer_map_offset(TB_Buffer *buffer, U64 version, U64 *off)
{
  // NOTE(rad): returns 0 if the changes since `version` are not all in the
  // log (too old, from the future, or a reload); `*off` is then unchanged
  // and the caller should clamp it to the text.
  B32 result = (version <= buffer->version && buffer->version - version <= TB_CHANGE_LOG_SIZE);
  for(U64 v = version+1; result && v <= buffer->version; v += 1)
  {
    TB_Change *change = &buffer->change_log[v%TB_CHANGE_LOG_SIZE];
    result = (change->version == v && !change->reset);
  }
  if(result)
  {
    U64 o = *off;
    for(U64 v = version+1; v <= buffer->version; v += 1)
    {
      TB_Change *change = &buffer->change_log[v%TB_CHANGE_LOG_SIZE];
      if(o < change->range.min)
      {
        // rad: before the change: stays
      }
      else if(o >= change->range.max)
      {
        // rad: after the change (or at an insertion point): shifts
        o = o - dim_1u64(change->range) + change->new_size;
      }
      else
      {
        // rad: its next character was replaced: end of the replacement
        o = change->range.min + change->new_size;
      }
    }
    *off = o;
  }
  return result;
}

////////////////////////////////
//~ rad: Editing

internal void
tb_buffer_edit(TB_Buffer *buffer, TXT_Patched *current, Rng1U64 range, String8 replace)
{
  Temp scratch = scratch_begin(0, 0);
  range.max = Min(range.max, current->size);
  range.min = Min(range.min, range.max);
  if(range.min != range.max || replace.size != 0)
  {
    String8 old_text = memory_map_data_from_range(scratch.arena, &current->memory_map, range);
    B32 has_newline = (str8_find_needle(replace, 0, str8_lit("\n"), 0) < replace.size ||
                       str8_find_needle(old_text, 0, str8_lit("\n"), 0) < old_text.size);

    //- rad: merge into the open record (typing, backspacing, deleting forward)
    TB_EditRecord *last = buffer->history_last_applied;
    B32 merged = 0;
    if(buffer->history_group_open && last != 0 && last->next == 0 && last != buffer->history_saved && !has_newline &&
       last->old_text.size + last->new_text.size + old_text.size + replace.size <= TB_UNDO_GROUP_MAX_SIZE)
    {
      // rad: typing: insertion right after the text the last record inserted
      if(range.min == range.max && last->new_text.size != 0 && range.min == last->range.min + last->new_text.size)
      {
        last->new_text = push_str8_cat(buffer->history_arena, last->new_text, replace);
        merged = 1;
      }

      // rad: backspace: deletion that ends where the last deletion started
      else if(replace.size == 0 && last->new_text.size == 0 && range.max == last->range.min)
      {
        last->range = r1u64(range.min, last->range.max);
        last->old_text = push_str8_cat(buffer->history_arena, old_text, last->old_text);
        merged = 1;
      }

      // rad: delete: deletion at the same spot as the last deletion
      else if(replace.size == 0 && last->new_text.size == 0 && range.min == last->range.min)
      {
        last->range = r1u64(last->range.min, last->range.max + dim_1u64(range));
        last->old_text = push_str8_cat(buffer->history_arena, last->old_text, old_text);
        merged = 1;
      }
    }

    //- rad: new record (drops any redo tail)
    if(!merged)
    {
      TB_EditRecord *rec = push_array(buffer->history_arena, TB_EditRecord, 1);
      rec->range = range;
      rec->old_text = push_str8_copy(buffer->history_arena, old_text);
      rec->new_text = push_str8_copy(buffer->history_arena, replace);
      rec->prev = last;
      if(last != 0)
      {
        last->next = rec;
      }
      else
      {
        buffer->history_first = rec;
      }
      buffer->history_last_applied = rec;
    }

    //- rad: a line break ends the undo group
    buffer->history_group_open = !has_newline;
    tb_buffer_push_patch__(buffer, range, replace);
  }
  scratch_end(scratch);
}

internal B32
tb_buffer_undo(TB_Buffer *buffer, U64 *cursor_out)
{
  B32 result = 0;
  TB_EditRecord *rec = buffer->history_last_applied;
  if(rec != 0)
  {
    tb_buffer_push_patch__(buffer, r1u64(rec->range.min, rec->range.min + rec->new_text.size), rec->old_text);
    buffer->history_last_applied = rec->prev;
    buffer->history_group_open = 0;
    if(cursor_out != 0)
    {
      *cursor_out = rec->range.min + rec->old_text.size;
    }
    result = 1;
  }
  return result;
}

internal B32
tb_buffer_redo(TB_Buffer *buffer, U64 *cursor_out)
{
  B32 result = 0;
  TB_EditRecord *rec = (buffer->history_last_applied != 0 ? buffer->history_last_applied->next : buffer->history_first);
  if(rec != 0)
  {
    tb_buffer_push_patch__(buffer, rec->range, rec->new_text);
    buffer->history_last_applied = rec;
    buffer->history_group_open = 0;
    if(cursor_out != 0)
    {
      *cursor_out = rec->range.min + rec->new_text.size;
    }
    result = 1;
  }
  return result;
}

internal void
tb_buffer_close_undo_group(TB_Buffer *buffer)
{
  buffer->history_group_open = 0;
}

////////////////////////////////
//~ rad: Saving

internal B32
tb_buffer_save(TB_Buffer *buffer, Access *access, String8 path)
{
  B32 result = 0;
  Temp scratch = scratch_begin(0, 0);
  TB_Text text = tb_text_from_buffer(scratch.arena, access, buffer);
  if(text.base_info.lines_count != 0)
  {
    //- rad: write to a temporary file next to the target, then replace it
    String8 data = memory_map_data_from_range(scratch.arena, &text.patched.memory_map, r1u64(0, text.patched.size));
    String8 tmp_path = push_str8f(scratch.arena, "%S.rad_tmp", path);
    B32 good = write_data_to_file_path(tmp_path, data);
    if(good)
    {
      good = replace_file_path(path, tmp_path);
    }
    if(!good)
    {
      delete_file_at_path(tmp_path);
    }

    //- rad: success -> this history position now matches the disk; the file
    // stream's reload of these bytes will have this hash (no "changed on disk")
    if(good)
    {
      buffer->disk_hash = u128_hash_from_str8(data);
      buffer->history_saved = buffer->history_last_applied;
      buffer->history_group_open = 0;
      buffer->changed_on_disk = 0;
      result = 1;
    }
  }
  scratch_end(scratch);
  return result;
}
