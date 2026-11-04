// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

////////////////////////////////
//~ rad: View State Functions

internal RAD_ViewState *
rad_view_state_from_cfg(CFG_Node *cfg)
{
  RAD_ViewState *vs = 0;
  if(cfg != &cfg_nil_node)
  {
    U64 hash = u64_hash_from_seed_str8(5381, str8_struct(&cfg->id));
    U64 slot_idx = hash%rad_state->view_state_slots_count;
    RAD_ViewStateSlot *slot = &rad_state->view_state_slots[slot_idx];
    for(RAD_ViewState *v = slot->first; v != 0; v = v->hash_next)
    {
      if(v->cfg_id == cfg->id)
      {
        vs = v;
        break;
      }
    }
    if(vs == 0)
    {
      vs = rad_state->free_view_state;
      if(vs != 0)
      {
        SLLStackPop_N(rad_state->free_view_state, hash_next);
      }
      else
      {
        vs = push_array_no_zero(rad_state->arena, RAD_ViewState, 1);
      }
      MemoryZeroStruct(vs);
      vs->cfg_id = cfg->id;
      vs->arena = arena_alloc();
      SLLStackPush_N(slot->first, vs, hash_next);
    }
    vs->last_frame_index_touched = rad_state->frame_index;
  }
  return vs;
}

internal void
rad_store_view_loading_info(CFG_Node *tab, B32 is_loading, U64 progress_u64, U64 progress_u64_target)
{
  // NOTE(rad): port of raddbg's rd_store_view_loading_info
  RAD_ViewState *view_state = rad_view_state_from_cfg(tab);
  B32 loading_state_is_new = (is_loading && view_state->loading_t_target != (F32)!!is_loading);
  view_state->loading_t_target = (F32)!!is_loading;
  view_state->loading_progress_v = progress_u64;
  view_state->loading_progress_v_target = progress_u64_target;
  if(loading_state_is_new || view_state->last_frame_index_built+1 < rad_state->frame_index)
  {
    view_state->loading_t = view_state->loading_t_target;
  }
}

internal void
rad_view_state_animate(void)
{
  // NOTE(rad): port of raddbg's per-frame view state animation (rd_frame,
  // "animate view states"). scrolling sets a scroll point's `idx` to the
  // target at once and moves the difference into `off` (see
  // ui_scroll_pt_target_idx), so the content is drawn at idx+off and only
  // slides to the target while `off` eases to 0 here. without this, views
  // never move when scrolled with the wheel or the scroll bar.
  B32 any_window_is_focused = 0;
  for(RAD_Window *ws = rad_state->first_window; ws != 0; ws = ws->next)
  {
    if(wm_window_is_focused(ws->os))
    {
      any_window_is_focused = 1;
      break;
    }
  }
  F32 slow_rate = 1 - pow_f32(2, (-10.f * rad_state->frame_dt));
  for EachIndex(slot_idx, rad_state->view_state_slots_count)
  {
    for(RAD_ViewState *vs = rad_state->view_state_slots[slot_idx].first; vs != 0; vs = vs->hash_next)
    {
      //- rad: loading overlay fades in/out; a selected tab's view sets the
      // target again each frame it is still loading (same as raddbg)
      F32 loading_t_diff = (vs->loading_t_target - vs->loading_t);
      vs->loading_t += loading_t_diff * slow_rate;
      if(any_window_is_focused && abs_f32(loading_t_diff) > 0.01f)
      {
        rad_request_frame();
      }
      CFG_Node *vcfg = cfg_node_from_id(vs->cfg_id);
      if(cfg_node_child_from_string(vcfg, str8_lit("selected")) != &cfg_nil_node)
      {
        if(vs->loading_t_target > 0.5f && any_window_is_focused)
        {
          rad_request_frame();
        }
        vs->loading_t_target = 0;
      }

      UI_ScrollPt2 *scroll_pts[] = {&vs->cv.scroll_pos, &vs->ex.scroll_pos, &vs->pr.scroll_pos, &vs->ss.scroll_pos};
      for EachElement(idx, scroll_pts)
      {
        UI_ScrollPt2 *scroll_pos = scroll_pts[idx];
        F32 scroll_x_diff = (-scroll_pos->x.off);
        F32 scroll_y_diff = (-scroll_pos->y.off);
        scroll_pos->x.off += scroll_x_diff*rad_state->scrolling_animation_rate;
        scroll_pos->y.off += scroll_y_diff*rad_state->scrolling_animation_rate;
        if(abs_f32(scroll_x_diff) > 0.01f || abs_f32(scroll_y_diff) > 0.01f)
        {
          rad_request_frame();
        }
        if(abs_f32(scroll_x_diff) <= 0.01f)
        {
          scroll_pos->x.off = 0;
        }
        if(abs_f32(scroll_y_diff) <= 0.01f)
        {
          scroll_pos->y.off = 0;
        }
      }
    }
  }

  //- rad: the completion list's scroll point. not a view state here (in
  // raddbg the autocompletion lister is a view, so it eases with the loop
  // above); without this, wheel and Up/Down stop moving its rows.
  {
    UI_ScrollPt *scroll_pt = &rad_lsp->completion.scroll_pt;
    F32 scroll_diff = (-scroll_pt->off);
    scroll_pt->off += scroll_diff*rad_state->scrolling_animation_rate;
    if(abs_f32(scroll_diff) > 0.01f)
    {
      rad_request_frame();
    }
    else
    {
      scroll_pt->off = 0;
    }
  }
}

internal void
rad_view_state_gc(void)
{
  for EachIndex(slot_idx, rad_state->view_state_slots_count)
  {
    RAD_ViewStateSlot *slot = &rad_state->view_state_slots[slot_idx];
    RAD_ViewState *prev = 0;
    for(RAD_ViewState *vs = slot->first, *next = 0; vs != 0; vs = next)
    {
      next = vs->hash_next;
      CFG_Node *cfg = cfg_node_from_id(vs->cfg_id);
      if(cfg == &cfg_nil_node || vs->last_frame_index_touched+2 < rad_state->frame_index)
      {
        if(prev == 0) { slot->first = next; } else { prev->hash_next = next; }
        if(vs->ex.initialized)
        {
          arena_release(vs->ex.expansions_arena);
          arena_release(vs->ex.rows_arena);
        }
        arena_release(vs->arena);
        SLLStackPush_N(rad_state->free_view_state, vs, hash_next);
      }
      else
      {
        prev = vs;
      }
    }
  }
}

////////////////////////////////
//~ rad: Documents

internal RAD_Document *
rad_document_lookup(String8 path)
{
  RAD_Document *result = 0;
  if(path.size != 0)
  {
    for(RAD_Document *doc = rad_state->first_document; doc != 0; doc = doc->next)
    {
      if(str8_match(doc->path, path, StringMatchFlag_CaseInsensitive|StringMatchFlag_SlashInsensitive))
      {
        result = doc;
        break;
      }
    }
  }
  return result;
}

internal RAD_Document *
rad_document_from_path(String8 path)
{
  RAD_Document *doc = rad_document_lookup(path);
  if(doc == 0 && path.size != 0)
  {
    doc = rad_state->free_document;
    if(doc != 0)
    {
      SLLStackPop(rad_state->free_document);
    }
    else
    {
      doc = push_array_no_zero(rad_state->arena, RAD_Document, 1);
    }
    MemoryZeroStruct(doc);
    doc->arena = arena_alloc();
    rad_state->document_id_gen += 1;
    doc->id = rad_state->document_id_gen;
    doc->path = push_str8_copy(doc->arena, path);
    doc->lang = txt_lang_kind_from_extension(str8_skip_last_dot(path));
    doc->buffer = tb_buffer_alloc(doc->lang);
    DLLPushBack(rad_state->first_document, rad_state->last_document, doc);
  }
  if(doc != 0)
  {
    doc->last_frame_index_touched = rad_state->frame_index;
  }
  return doc;
}

internal RAD_Document *
rad_document_from_tab(CFG_Node *tab)
{
  RAD_Document *doc = 0;
  if(str8_match(tab->string, str8_lit("text"), 0))
  {
    doc = rad_document_lookup(rad_file_path_from_tab(tab));
  }
  return doc;
}

internal U64
rad_document_tab_count(RAD_Document *doc)
{
  // NOTE(rad): text tabs showing this document, in all windows
  Temp scratch = scratch_begin(0, 0);
  U64 count = 0;
  CFG_NodePtrList windows = cfg_node_top_level_list_from_string(scratch.arena, str8_lit("window"));
  for(CFG_NodePtrNode *wn = windows.first; wn != 0; wn = wn->next)
  {
    CFG_PanelTree tree = cfg_panel_tree_from_cfg(scratch.arena, wn->v);
    for(CFG_PanelNode *p = tree.root; p != &cfg_nil_panel_node; p = cfg_panel_node_rec__depth_first_pre(tree.root, p).next)
    {
      for(CFG_NodePtrNode *tn = p->tabs.first; tn != 0; tn = tn->next)
      {
        count += (rad_document_from_tab(tn->v) == doc);
      }
    }
  }
  scratch_end(scratch);
  return count;
}

internal B32
rad_document_save(RAD_Document *doc)
{
  B32 result = 0;
  if(doc != 0 && tb_buffer_is_loaded(doc->buffer))
  {
    Access *access = access_open();
    result = tb_buffer_save(doc->buffer, access, doc->path);
    access_close(access);
    if(result)
    {
      rad_lsp_document_saved(doc);
    }
    if(!result)
    {
      log_user_errorf("Could not save \"%S\".", doc->path);
    }
  }
  return result;
}

internal void
rad_documents_tick(void)
{
  // NOTE(rad): feed each file stream's current content to its buffer (load,
  // reload, "changed on disk"), let the buffer compact its patches. after
  // file_stream saw a file change (the watcher thread woke us), keep
  // drawing for a second so the new content gets streamed, lexed and shown.
  U64 fs_gen = fs_change_gen();
  if(fs_gen != rad_state->docs_fs_change_gen)
  {
    rad_state->docs_fs_change_gen = fs_gen;
    rad_state->docs_refresh_until_us = now_time_us() + 1000000;
  }
  if(now_time_us() < rad_state->docs_refresh_until_us)
  {
    rad_request_frame();
  }
  Access *access = access_open();
  for(RAD_Document *doc = rad_state->first_document; doc != 0; doc = doc->next)
  {
    FileProperties props = properties_from_file_path(doc->path);
    doc->is_missing = (props.modified == 0);
    C_Key text_key = fs_key_from_path_range(doc->path, r1u64(0, props.size), 0);
    U128 source_hash = {0};
    txt_text_info_from_key_lang(access, text_key, doc->lang, &source_hash);
    tb_buffer_observe_source(doc->buffer, access, source_hash);
    tb_buffer_tick(doc->buffer, access, now_time_us());
    if(tb_buffer_is_busy(doc->buffer) || (!doc->is_missing && !tb_buffer_is_loaded(doc->buffer)))
    {
      rad_request_frame();
    }
  }
  access_close(access);
}

internal void
rad_documents_gc(void)
{
  for(RAD_Document *doc = rad_state->first_document, *next = 0; doc != 0; doc = next)
  {
    next = doc->next;
    if(doc->last_frame_index_touched+2 < rad_state->frame_index)
    {
      DLLRemove(rad_state->first_document, rad_state->last_document, doc);
      tb_buffer_release(doc->buffer);
      arena_release(doc->arena);
      SLLStackPush(rad_state->free_document, doc);
    }
  }
}

////////////////////////////////
//~ rad: Code Views

internal void
rad_code_view_init(RAD_CodeViewState *cv)
{
  if(cv->initialized == 0)
  {
    cv->initialized = 1;
    cv->preferred_column = 1;
    cv->initial_center = 1;
    cv->center_cursor = 1;
  }
}

internal B32
rad_code_view_undo(RAD_CodeViewState *cv, TB_Buffer *buffer)
{
  U64 cursor = cv->cursor;
  B32 result = tb_buffer_undo(buffer, &cursor);
  if(result)
  {
    cv->cursor = cv->mark = cursor;
    cv->buffer_version = buffer->version;
    cv->contain_cursor = 1;
  }
  return result;
}

internal B32
rad_code_view_redo(RAD_CodeViewState *cv, TB_Buffer *buffer)
{
  U64 cursor = cv->cursor;
  B32 result = tb_buffer_redo(buffer, &cursor);
  if(result)
  {
    cv->cursor = cv->mark = cursor;
    cv->buffer_version = buffer->version;
    cv->contain_cursor = 1;
  }
  return result;
}

////////////////////////////////
//~ rad: Completion

internal B32
rad_char_is_word(U8 c)
{
  return (char_is_alpha(c) || char_is_digit(c, 10) || c == '_' || c >= 0x80);
}

internal void
rad_document_apply_edits(RAD_Document *doc, U64 version, RAD_LspTextEdit *edits, U64 edits_count, U64 join_step)
{
  // NOTE(rad): edits in the text of an older version (a language server's
  // answer): carried to the current text (an edit's start stays before what
  // was inserted there), then applied from the end backwards, one undo step:
  // `join_step` if it is still the last one (a completion's late `use` line);
  // nothing if that step was undone. The views map their cursors when next
  // built.
  Temp scratch = scratch_begin(0, 0);
  TB_Buffer *buffer = doc->buffer;
  Access *access = access_open();
  TB_Text text = tb_text_from_buffer(scratch.arena, access, buffer);
  if(text.base_info.lines_count != 0 && (join_step == 0 || tb_buffer_step_is_applied(buffer, join_step)))
  {
    RAD_LspTextEdit *mapped = push_array(scratch.arena, RAD_LspTextEdit, edits_count);
    U64 mapped_count = 0;
    for EachIndex(idx, edits_count)
    {
      RAD_LspTextEdit e = edits[idx];
      if(tb_buffer_map_offset_side(buffer, version, &e.range.min, Side_Min) &&
         tb_buffer_map_offset(buffer, version, &e.range.max) &&
         e.range.min <= e.range.max && e.range.max <= text.patched.size)
      {
        mapped[mapped_count] = e;
        mapped_count += 1;
      }
    }
    for(U64 i = 1; i < mapped_count; i += 1)
    {
      for(U64 j = i; j > 0 && mapped[j-1].range.min < mapped[j].range.min; j -= 1)
      {
        Swap(RAD_LspTextEdit, mapped[j-1], mapped[j]);
      }
    }
    TXT_Patched patched = text.patched;
    if(!tb_buffer_join_step(buffer, join_step))
    {
      tb_buffer_begin_step(buffer);
    }
    for EachIndex(idx, mapped_count)
    {
      tb_buffer_edit(buffer, &patched, mapped[idx].range, mapped[idx].text);
      patched = txt_patched_from_info_data_patches(scratch.arena, &text.base_info, text.base_data, &buffer->patches);
    }
    tb_buffer_end_step(buffer);
    rad_request_frame();
  }
  access_close(access);
  scratch_end(scratch);
}

internal void
rad_code_view_completion_commit(Arena *arena, RAD_Document *doc, TB_Buffer *buffer, TXT_TextInfo *text_info, String8 text_data, TXT_Patched *text_patched, U64 *cursor, RAD_CodeViewState *cv)
{
  // NOTE(rad): the selected item: its edit (from where the word started,
  // over what was typed since the request) and its additional edits (a `use`
  // line), applied from the end of the text backwards so that earlier
  // offsets stay valid, as one undo step; the cursor goes after the inserted
  // text. `text_patched` is rebuilt in `arena`.
  RAD_LspCompletion *c = &rad_lsp->completion;
  if(1 <= c->cursor.y && c->cursor.y <= (S64)c->rows_count)
  {
    Temp scratch = scratch_begin(&arena, 1);
    U64 item_idx = c->rows[c->cursor.y-1];
    RAD_LspCompletionItem *item = &c->items[item_idx];

    //- rad: edits the server computes on request (an import), not here yet:
    // the answer on its way (asked while selected) or asked for now; applied
    // when they come, into this commit's undo step
    RAD_LspResolve *r = &rad_lsp->resolve;
    B32 resolve_pending = 0;
    if(doc != 0 && item->resolve_json.size != 0 && !item->resolved && item->more_edits_count == 0)
    {
      if(r->request_id != 0 && r->items_gen == c->items_gen && r->item_idx == item_idx)
      {
        r->for_commit = 1;
      }
      else
      {
        rad_lsp_completion_resolve(doc, item_idx, 1);
      }
      resolve_pending = (r->request_id != 0 && r->for_commit);
    }

    //- rad: the main edit, in the current text
    RAD_LspTextEdit main_edit = {r1u64(Min(c->word_start, *cursor), *cursor), item->insert_text};
    U64 main_cursor = item->insert_cursor;
    if(item->has_edit)
    {
      U64 min = item->edit.range.min;
      U64 max = item->edit.range.max;
      if(tb_buffer_map_offset_side(buffer, c->items_version, &min, Side_Min) &&
         tb_buffer_map_offset(buffer, c->items_version, &max) &&
         min <= *cursor)
      {
        main_edit.range = r1u64(min, Max(max, *cursor));
        main_edit.text = item->edit.text;
        main_cursor = item->edit_cursor;
      }
    }
    main_cursor = Min(main_cursor, main_edit.text.size);

    //- rad: the additional edits that still map (and do not overlap it)
    U64 edits_count = 0;
    RAD_LspTextEdit *edits = push_array(scratch.arena, RAD_LspTextEdit, item->more_edits_count+1);
    for EachIndex(idx, item->more_edits_count)
    {
      RAD_LspTextEdit e = item->more_edits[idx];
      if(tb_buffer_map_offset_side(buffer, item->more_edits_version, &e.range.min, Side_Min) &&
         tb_buffer_map_offset(buffer, item->more_edits_version, &e.range.max) &&
         (e.range.max <= main_edit.range.min || main_edit.range.max <= e.range.min))
      {
        edits[edits_count] = e;
        edits_count += 1;
      }
    }
    U64 main_idx = edits_count;
    edits[edits_count] = main_edit;
    edits_count += 1;

    //- rad: order: last in the text first
    U64 *order = push_array(scratch.arena, U64, edits_count);
    for EachIndex(idx, edits_count)
    {
      order[idx] = idx;
    }
    for(U64 i = 1; i < edits_count; i += 1)
    {
      for(U64 j = i; j > 0 && edits[order[j-1]].range.min < edits[order[j]].range.min; j -= 1)
      {
        Swap(U64, order[j-1], order[j]);
      }
    }

    //- rad: apply (one undo step)
    S64 shift = 0;
    U64 step = tb_buffer_begin_step(buffer);
    for EachIndex(idx, edits_count)
    {
      RAD_LspTextEdit *e = &edits[order[idx]];
      tb_buffer_edit(buffer, text_patched, e->range, e->text);
      *text_patched = txt_patched_from_info_data_patches(arena, text_info, text_data, &buffer->patches);
      if(order[idx] != main_idx && e->range.max <= main_edit.range.min)
      {
        shift += (S64)e->text.size - (S64)dim_1u64(e->range);
      }
    }
    tb_buffer_end_step(buffer);
    if(resolve_pending)
    {
      r->commit_step = step;
    }
    U64 main_start = (U64)((S64)main_edit.range.min + shift);
    *cursor = main_start + main_cursor;

    //- rad: the cursor is right before a closing bracket the item inserted (a
    // function's `)`, a generic type's `>`): typing it will step over it
    cv->typeover_armed = 0;
    if(main_cursor < main_edit.text.size &&
       (main_edit.text.str[main_cursor] == ')' || main_edit.text.str[main_cursor] == '>' ||
        main_edit.text.str[main_cursor] == ']' || main_edit.text.str[main_cursor] == '}'))
    {
      cv->typeover_armed = 1;
      cv->typeover_char = main_edit.text.str[main_cursor];
      cv->typeover_min = main_start;
      cv->typeover_off = *cursor;
      cv->typeover_version = buffer->version;
    }
    scratch_end(scratch);
  }
  rad_lsp_completion_close();
}

internal void
rad_code_view_completion_update(CFG_Node *tab, RAD_CodeViewState *cv, TB_Buffer *buffer, TXT_Patched *text, B32 focused, B32 typed, U8 typed_char, B32 moved)
{
  // NOTE(rad): after the view's input: start, follow or close the list
  // (rad_lsp.h, "completion")
  Temp scratch = scratch_begin(0, 0);
  RAD_LspCompletion *c = &rad_lsp->completion;
  RAD_Document *doc = rad_document_from_tab(tab);
  CFG_Node *window = cfg_window_from_cfg(tab);
  B32 complete_word = cv->complete_word;
  cv->complete_word = 0;
  B32 here = (c->active && c->tab == tab->id);

  //- rad: close: no document, the view lost the keyboard, the cursor moved
  // (not by typing), a selection
  if(here && (doc == 0 || !cv->editable || doc->id != c->doc_id || !focused || moved || cv->cursor != cv->mark))
  {
    rad_lsp_completion_close();
    here = 0;
  }

  //- rad: the word start, in the current text (typing at it stays after it)
  if(here && c->word_version != buffer->version)
  {
    U64 off = c->word_start;
    if(tb_buffer_map_offset_side(buffer, c->word_version, &off, Side_Min))
    {
      c->word_start = off;
      c->word_version = buffer->version;
    }
    else
    {
      rad_lsp_completion_close();
      here = 0;
    }
  }

  //- rad: start: the first character of a word (not a number), `.`, `::`;
  // Complete Word: the word before the cursor
  U64 cursor = cv->cursor;
  if(doc != 0 && cv->editable && focused && cursor == cv->mark && (typed || complete_word))
  {
    String8 before = memory_map_data_from_range(scratch.arena, &text->memory_map, r1u64(cursor > 128 ? cursor-128 : 0, cursor));
    U64 word_size = 0;
    for(;word_size < before.size && rad_char_is_word(before.str[before.size-1-word_size]); word_size += 1);
    U64 word_start = cursor - word_size;
    String8 word = str8_postfix(before, word_size);
    U8 char_before_typed = (before.size >= 2 ? before.str[before.size-2] : 0);
    if(complete_word)
    {
      if(!rad_lsp_completion_request(window, tab, doc, cursor, word_start, word, 0, 0))
      {
        rad_set_error(str8_lit("Complete Word needs rust-analyzer running for this file (it starts when a .rs file of a Cargo project is open)."));
      }
    }
    else if(rad_char_is_word(typed_char))
    {
      if(!here && word_size == 1 && !char_is_digit(typed_char, 10))
      {
        rad_lsp_completion_request(window, tab, doc, cursor, word_start, word, 0, 0);
      }
    }
    else
    {
      if(here)
      {
        rad_lsp_completion_close();
      }
      if(typed_char == '.' || (typed_char == ':' && char_before_typed == ':'))
      {
        rad_lsp_completion_request(window, tab, doc, cursor, cursor, str8_zero(), typed_char, 0);
      }
    }
    here = (c->active && c->tab == tab->id);
  }

  //- rad: the typed word -> the list's filter; asked for again (one request
  // at a time) when the word changed and the list is marked incomplete or
  // has nothing that matches (servers add items as the word grows: imports)
  if(here)
  {
    String8 word = {0};
    B32 is_word = (c->word_start <= cursor && cursor - c->word_start <= 128);
    if(is_word)
    {
      word = memory_map_data_from_range(scratch.arena, &text->memory_map, r1u64(c->word_start, cursor));
      for EachIndex(idx, word.size)
      {
        is_word = (is_word && rad_char_is_word(word.str[idx]));
      }
    }
    if(!is_word)
    {
      rad_lsp_completion_close();
    }
    else
    {
      rad_lsp_completion_set_filter(word);
      String8 items_filter = str8(c->items_filter, c->items_filter_size);
      if(c->request_id == 0 &&
         (c->retry || (c->answered && (c->incomplete || c->rows_count == 0) && !str8_match(word, items_filter, 0))))
      {
        rad_lsp_completion_request(window, tab, doc, cursor, c->word_start, word, 0, 1);
      }
    }
  }

  //- rad: the selected item's import edit is asked for while it is selected
  // (one request at a time), so its commit has the `use` line at once
  if(c->active && c->tab == tab->id && doc != 0 && rad_lsp->resolve.request_id == 0 &&
     1 <= c->cursor.y && c->cursor.y <= (S64)c->rows_count)
  {
    U64 item_idx = c->rows[c->cursor.y-1];
    RAD_LspCompletionItem *item = &c->items[item_idx];
    if(item->resolve_json.size != 0 && !item->resolved && item->more_edits_count == 0)
    {
      rad_lsp_completion_resolve(doc, item_idx, 0);
    }
  }
  scratch_end(scratch);
}

internal void
rad_icon_from_lsp_completion_kind(S64 kind, RAD_IconKind *icon_out, RAD_CodeColorSlot *color_out)
{
  // NOTE(rad): not in raddbg (its lister rows have no kind icons): LSP
  // CompletionItemKind -> one of raddbg's icons, in the code color of that
  // kind of token (Visual Studio's shapes where raddbg has one: a cube for
  // functions, a grid for types)
  RAD_IconKind icon = RAD_IconKind_Dot;
  RAD_CodeColorSlot color = RAD_CodeColorSlot_CodeDefault;
  switch(kind)
  {
    default:{}break;
    case 2: case 3: case 4:                   {icon = RAD_IconKind_Cube;   color = RAD_CodeColorSlot_CodeSymbol;}break;  // method, function, constructor
    case 5: case 6: case 10:                  {icon = RAD_IconKind_Dot;    color = RAD_CodeColorSlot_CodeLocal;}break;   // field, variable, property
    case 7: case 8: case 13: case 22: case 25:{icon = RAD_IconKind_Grid;   color = RAD_CodeColorSlot_CodeType;}break;    // class, interface, enum, struct, type parameter
    case 9:                                   {icon = RAD_IconKind_Module; color = RAD_CodeColorSlot_CodeDefault;}break; // module
    case 11: case 12: case 20: case 21:       {icon = RAD_IconKind_List;   color = RAD_CodeColorSlot_CodeNumeric;}break; // unit, value, enum member, constant
    case 14: case 24:                         {icon = RAD_IconKind_Dot;    color = RAD_CodeColorSlot_CodeKeyword;}break; // keyword, operator
    case 15:                                  {icon = RAD_IconKind_Pencil; color = RAD_CodeColorSlot_CodeMeta;}break;    // snippet
    case 17:                                  {icon = RAD_IconKind_FileOutline;         color = RAD_CodeColorSlot_CodeDefault;}break;
    case 19:                                  {icon = RAD_IconKind_FolderClosedOutline; color = RAD_CodeColorSlot_CodeDefault;}break;
  }
  *icon_out = icon;
  *color_out = color;
}

internal void
rad_completion_popup_ui(RAD_Window *ws, CFG_Node *window)
{
  // NOTE(rad): port of raddbg's autocompletion floating view (raddbg_core.c,
  // "@window_ui_part build all floating views"): same container, size,
  // placement, animation, focus and permissions; its rows are built like the
  // Problems rows (raddbg's watch rows: one code-colored cell, the typed
  // word highlighted). Built before the panels, from the last frame's anchor.
  RAD_LspCompletion *c = &rad_lsp->completion;
  if(c->active && c->window == window->id && c->anchor_frame+1 < rad_state->frame_index)
  {
    // rad: its view was not built last frame (another tab, a hidden panel)
    rad_lsp_completion_close();
  }
  if(c->active && c->window == window->id && c->rows_count != 0)
    RAD_Font(RAD_FontSlot_Code)
    UI_TagF("floating")
  {
    Temp scratch = scratch_begin(0, 0);

    //- rad: size and place (raddbg: 30 em wide, at most 30 em of rows, below
    // the anchor; not in raddbg: kept inside the window, above the line when
    // it does not fit below)
    F32 row_height_px = ui_top_px_height();
    U64 max_row_count = (U64)floor_f32(ui_top_font_size()*30.f / row_height_px);
    U64 needed_row_count = Min(max_row_count, c->rows_count);
    F32 width_px = floor_f32(30.f*ui_top_font_size());
    F32 height_px = needed_row_count*row_height_px;
    Rng2F32 window_rect = wm_client_rect_from_window(ws->os);
    Rng2F32 rect = r2f32p(c->anchor_line_rect.x0, c->anchor_line_rect.y1,
                          c->anchor_line_rect.x0 + width_px, c->anchor_line_rect.y1 + height_px);
    if(rect.y1 > window_rect.y1 && c->anchor_line_rect.y0 - height_px >= window_rect.y0)
    {
      rect.y0 = c->anchor_line_rect.y0 - height_px;
      rect.y1 = c->anchor_line_rect.y0;
    }
    if(rect.x1 > window_rect.x1)
    {
      F32 shift = Min(rect.x1 - window_rect.x1, rect.x0 - window_rect.x0);
      rect.x0 -= shift;
      rect.x1 -= shift;
    }
    F32 open_t = ui_anim(ui_key_from_stringf(ui_key_zero(), "completion_open_%I64x", c->open_gen), 1.f,
                         .rate = rad_state->menu_animation_rate,
                         .initial = 0.f);

    //- rad: build
    UI_Focus(UI_FocusKind_On)
      UI_PermissionFlags(UI_PermissionFlag_KeyboardSecondary|UI_PermissionFlag_Clicks|UI_PermissionFlag_ScrollX|UI_PermissionFlag_ScrollY)
    {
      //- rad: top-level container box (raddbg's)
      UI_Box *container = &ui_nil_box;
      UI_Rect(rect) UI_ChildLayoutAxis(Axis2_Y)
        UI_Squish(0.1f-0.1f*open_t)
        UI_Transparency(1.f-open_t)
        UI_CornerRadius(ui_top_font_size()*0.15f)
      {
        container = ui_build_box_from_stringf(UI_BoxFlag_Clickable|
                                              UI_BoxFlag_DrawBorder|
                                              UI_BoxFlag_DrawBackground|
                                              UI_BoxFlag_DrawBackgroundBlur|
                                              UI_BoxFlag_RoundChildrenByParent|
                                              UI_BoxFlag_DisableFocusOverlay|
                                              UI_BoxFlag_DrawDropShadow|
                                              UI_BoxFlag_SquishAnchored,
                                              "floating_view_container_completion");
      }

      //- rad: contents: the rows (raddbg: the watch view's scroll list)
      UI_Parent(container) UI_Focus(UI_FocusKind_Null)
      {
        ui_set_next_pref_width(ui_pct(1, 0));
        ui_set_next_pref_height(ui_px(height_px, 1));
        ui_set_next_child_layout_axis(Axis2_Y);
        UI_Box *view_contents_container = ui_build_box_from_stringf(UI_BoxFlag_Clip, "###view_contents_container");
        UI_Parent(view_contents_container) UI_WidthFill
        {
          S64 rows_count = (S64)c->rows_count;
          F32 contents_width_px = (width_px - floor_f32(ui_bottom_font_size()*1.5f));
          UI_ScrollListParams scroll_list_params = {0};
          {
            scroll_list_params.flags         = UI_ScrollListFlag_All;
            scroll_list_params.row_height_px = row_height_px;
            scroll_list_params.dim_px        = v2f32(width_px, height_px);
            scroll_list_params.cursor_range  = r2s64(v2s64(0, 1), v2s64(0, rows_count));
            scroll_list_params.item_range    = r1s64(0, rows_count);
          }
          Vec2S64 mark = c->cursor;
          Rng1S64 visible_row_rng = {0};
          UI_ScrollListSignal scroll_list_sig = {0};
          UI_Focus(UI_FocusKind_On)
            UI_ScrollList(&scroll_list_params, &c->scroll_pt, &c->cursor, &mark, &visible_row_rng, &scroll_list_sig)
            UI_Focus(UI_FocusKind_Null)
          {
            ui_set_next_pref_height(ui_children_sum(1));
            ui_set_next_child_layout_axis(Axis2_Y);
            UI_Box *table = ui_build_box_from_string(0, str8_lit("table"));
            UI_Parent(table)
              for(S64 row_idx = visible_row_rng.min; row_idx <= visible_row_rng.max && row_idx < rows_count; row_idx += 1)
            {
              RAD_LspCompletionItem *item = &c->items[c->rows[row_idx]];
              B32 row_selected = (c->cursor.y == row_idx+1);

              //- rad: row text: kind icon (in the code color of that kind)  label
              // (code colors)  detail (weak)
              DR_FStrList fstrs = {0};
              {
                RAD_IconKind icon_kind = RAD_IconKind_Dot;
                RAD_CodeColorSlot icon_color_slot = RAD_CodeColorSlot_CodeDefault;
                rad_icon_from_lsp_completion_kind(item->kind, &icon_kind, &icon_color_slot);
                DR_FStrParams icon_params = {rad_font_from_slot(RAD_FontSlot_Icons), rad_raster_flags_from_slot(RAD_FontSlot_Icons), rad_rgba_from_code_color_slot(icon_color_slot), ui_top_font_size()};
                dr_fstrs_push_new(scratch.arena, &fstrs, &icon_params, rad_icon_kind_text_table[icon_kind]);
                DR_FStrParams space_params = {rad_font_from_slot(RAD_FontSlot_Code), rad_raster_flags_from_slot(RAD_FontSlot_Code), ui_color_from_name(str8_lit("text")), ui_top_font_size()};
                dr_fstrs_push_new(scratch.arena, &fstrs, &space_params, str8_lit("  "));
              }
              DR_FStrList label_fstrs = rad_fstrs_from_code_string_lang(scratch.arena, 1.f, ui_color_from_name(str8_lit("text")), TXT_LangKind_Rust, item->label);
              dr_fstrs_concat_in_place(&fstrs, &label_fstrs);
              if(item->detail.size != 0)
              {
                Vec4F32 weak_color = {0};
                UI_TagF("weak") weak_color = ui_color_from_name(str8_lit("text"));
                DR_FStrParams params = {rad_font_from_slot(RAD_FontSlot_Code), rad_raster_flags_from_slot(RAD_FontSlot_Code), weak_color, ui_top_font_size()};
                dr_fstrs_push_new(scratch.arena, &fstrs, &params, push_str8f(scratch.arena, "  %S", item->detail));
              }

              //- rad: build row box
              UI_BoxFlags row_flags = UI_BoxFlag_DisableFocusOverlay;
              if(row_idx & 1)
              {
                ui_set_next_tag(str8_lit("alt"));
                row_flags |= UI_BoxFlag_DrawBackground;
              }
              ui_set_next_pref_width(ui_px(contents_width_px, 1.f));
              ui_set_next_pref_height(ui_px(row_height_px, 1.f));
              ui_set_next_focus_hot(row_selected ? UI_FocusKind_On : UI_FocusKind_Off);
              UI_Box *row_box = ui_build_box_from_stringf(row_flags|UI_BoxFlag_Clickable, "row_%I64x", row_idx);
              if(row_idx == visible_row_rng.min)
              {
                c->layout_first_row      = row_idx;
                c->layout_first_row_rect = row_box->rect;
              }

              //- rad: build cell
              UI_Signal sig = {0};
              UI_Parent(row_box)
              {
                UI_Box *cell_box = &ui_nil_box;
                UI_PrefWidth(ui_px(contents_width_px, 1.f))
                {
                  ui_set_next_fixed_height(floor_f32(row_height_px));
                  cell_box = ui_build_box_from_stringf(UI_BoxFlag_DrawSideLeft, "cell_%I64x", row_idx);
                }
                RAD_CellParams cell_params = {0};
                cell_params.flags         = (RAD_CellFlag_KeyboardClickable|RAD_CellFlag_NoBackground|RAD_CellFlag_CodeContents);
                cell_params.value_fstrs   = fstrs;
                cell_params.search_needle = c->filter;
                UI_Parent(cell_box)
                  UI_FocusHot(row_selected ? UI_FocusKind_On : UI_FocusKind_Off)
                  UI_FocusActive(UI_FocusKind_Off)
                  UI_TagF("weak")
                {
                  sig = rad_cellf(&cell_params, "###cell_%I64x", row_idx);
                }
              }
              UI_Signal row_sig = ui_signal_from_box(row_box);

              //- rad: a click uses it (raddbg: a click on a lister/autocomplete row accepts it)
              if(ui_pressed(sig) || ui_pressed(row_sig))
              {
                c->cursor = v2s64(0, row_idx+1);
                c->commit_requested = 1;
                rad_request_frame();
              }
            }
          }
        }
      }

      //- rad: interact with container (takes the clicks between rows)
      ui_signal_from_box(container);

      //- rad: not in raddbg: the selected item's documentation, beside the
      // list (right of it, else left), laid out like a tooltip
      RAD_LspCompletionItem *selected = ((1 <= c->cursor.y && c->cursor.y <= (S64)c->rows_count) ? &c->items[c->rows[c->cursor.y-1]] : 0);
      if(selected != 0 && selected->documentation.size != 0)
        RAD_Font(RAD_FontSlot_Main)
      {
        F32 gap_px = floor_f32(ui_top_font_size()*0.5f);
        F32 doc_width_px = floor_f32(ui_top_font_size()*36.f);
        F32 x = rect.x1 + gap_px;
        if(x + doc_width_px > window_rect.x1 && rect.x0 - gap_px - doc_width_px >= window_rect.x0)
        {
          x = rect.x0 - gap_px - doc_width_px;
        }
        ui_set_next_fixed_x(x);
        ui_set_next_fixed_y(rect.y0);
        ui_set_next_pref_width(ui_children_sum(1));
        ui_set_next_pref_height(ui_children_sum(1));
        ui_set_next_child_layout_axis(Axis2_Y);
        UI_Box *doc_box = &ui_nil_box;
        UI_Squish(0.1f-0.1f*open_t)
          UI_Transparency(1.f-open_t)
          UI_CornerRadius(ui_top_font_size()*0.15f)
        {
          doc_box = ui_build_box_from_stringf(UI_BoxFlag_Clickable|
                                              UI_BoxFlag_DrawBorder|
                                              UI_BoxFlag_DrawBackground|
                                              UI_BoxFlag_DrawBackgroundBlur|
                                              UI_BoxFlag_DisableFocusOverlay|
                                              UI_BoxFlag_DrawDropShadow|
                                              UI_BoxFlag_SquishAnchored,
                                              "floating_view_container_completion_doc");
        }
        UI_Parent(doc_box) UI_Focus(UI_FocusKind_Null)
        {
          UI_PrefWidth(ui_px(0, 1)) ui_spacer(ui_em(0.75f, 1.f));
          ui_set_next_pref_width(ui_children_sum(1));
          ui_set_next_pref_height(ui_children_sum(1));
          UI_Row
          {
            UI_PrefHeight(ui_px(0, 1)) ui_spacer(ui_em(1.f, 1.f));
            ui_set_next_pref_width(ui_children_sum(1));
            ui_set_next_pref_height(ui_children_sum(1));
            UI_Column UI_PrefHeight(ui_px(floor_f32(ui_top_font_size()*1.75f), 1.f))
            {
              rad_markdown_ui(selected->documentation, doc_width_px - 2*ui_top_font_size(), 18);
            }
            UI_PrefHeight(ui_px(0, 1)) ui_spacer(ui_em(1.f, 1.f));
          }
          UI_PrefWidth(ui_px(0, 1)) ui_spacer(ui_em(0.75f, 1.f));
        }
        ui_signal_from_box(doc_box);
        c->doc_built_frame = rad_state->frame_index;
      }
    }
    c->built_frame = rad_state->frame_index;
    c->layout_rect = rect;
    scratch_end(scratch);
  }
}

internal void
rad_code_view_build(Arena *arena, CFG_Node *tab, RAD_CodeViewState *cv, TB_Buffer *buffer, Rng2F32 rect, TXT_LangKind lang_kind, TXT_TextInfo *text_info, String8 text_data)
{
  // NOTE(rad): port of raddbg's `rd_code_view_build` (raddbg/raddbg_views.c),
  // minus breakpoints, thread IPs, watch pins, debug-info lines, disassembly,
  // and the view query/search commands.
  ProfBeginFunction();
  Temp scratch = scratch_begin(&arena, 1);
  RAD_Settings *settings = &rad_state->settings;

  //////////////////////////////
  //- rad: unpack state
  //
  UI_ScrollPt2 scroll_pos = cv->scroll_pos;

  //////////////////////////////
  //- rad: extract invariants
  //
  F32 main_font_size = ui_bottom_font_size();
  FNT_Tag code_font = rad_font_from_slot(RAD_FontSlot_Code);
  F32 code_font_size = ui_top_font_size();
  F32 code_tab_size = fnt_column_size_from_tag_size(code_font, code_font_size)*settings->tab_width;
  FNT_Metrics code_font_metrics = fnt_metrics_from_tag_size(code_font, code_font_size);
  F32 code_line_height = ceil_f32(fnt_line_height_from_metrics(&code_font_metrics) * 1.5f);
  F32 big_glyph_advance = fnt_dim_from_tag_size_string(code_font, code_font_size, 0, 0, str8_lit("H")).x;
  Vec2F32 panel_box_dim = dim_2f32(rect);
  F32 scroll_bar_dim = floor_f32(main_font_size*1.5f);
  Vec2F32 code_area_dim = v2f32(panel_box_dim.x - scroll_bar_dim, panel_box_dim.y - scroll_bar_dim);
  S64 num_possible_visible_lines = (S64)(code_area_dim.y/code_line_height)+1;
  B32 do_line_numbers = settings->show_line_numbers;
  B32 text_is_ready = (text_info->lines_count != 0);

  //////////////////////////////
  //- rad: do keyboard interaction, compute patched text state
  //
  TXT_Patched text_patched = txt_patched_from_info_data_patches(scratch.arena, text_info, text_data, &buffer->patches);
  B32 snap[Axis2_COUNT] = {0};

  //- rad: not in raddbg: the language server's completion list (rad_lsp.h):
  // a clicked row is committed; this frame's input is noted for it
  RAD_LspCompletion *cmp = &rad_lsp->completion;
  B32 cmp_shown = rad_lsp_completion_is_shown(tab->id);
  B32 cmp_typed = 0;
  U8 cmp_typed_char = 0;
  B32 cmp_moved = 0;
  B32 cmp_eat_newline = 0;
  B32 view_focused = 0;
  UI_Focus(UI_FocusKind_On) view_focused = ui_is_focus_active();
  if(cmp->active && cmp->tab == tab->id && cmp->commit_requested && cv->editable && text_is_ready)
  {
    rad_code_view_completion_commit(scratch.arena, rad_document_from_tab(tab), buffer, text_info, text_data, &text_patched, &cv->cursor, cv);
    cv->mark = cv->cursor;
    cmp_shown = 0;
    snap[Axis2_X] = snap[Axis2_Y] = 1;
  }

  UI_Focus(UI_FocusKind_On) if(ui_is_focus_active() && text_is_ready)
  {
    U64 line_count_per_page = ClampBot(num_possible_visible_lines, 10) - 10;
    U64 *cursor = &cv->cursor;
    U64 *mark = &cv->mark;
    S64 *preferred_column = &cv->preferred_column;
    for(UI_Event *evt = 0; ui_next_event(&evt);)
    {
      //- rad: not in raddbg: a shown completion list takes Enter and Tab
      // (commit) and Esc (close); Enter's "\n" text event goes with it
      if(cmp_shown && cv->editable)
      {
        B32 is_commit = ((evt->kind == UI_EventKind_Press && evt->slot == UI_EventActionSlot_Accept) ||
                         (evt->kind == UI_EventKind_Text && evt->key == WM_Key_Tab));
        B32 is_cancel = (evt->kind == UI_EventKind_Press && evt->slot == UI_EventActionSlot_Cancel);
        if(is_commit)
        {
          rad_code_view_completion_commit(scratch.arena, rad_document_from_tab(tab), buffer, text_info, text_data, &text_patched, cursor, cv);
          *mark = *cursor;
          U64 line_num = txt_line_num_from_off(&text_patched.line_map, *cursor);
          Rng1U64 line_range = txt_range_from_line_num(&text_patched.line_map, line_num);
          *preferred_column = (S64)(*cursor - line_range.min);
          cmp_shown = 0;
          cmp_eat_newline = (evt->kind == UI_EventKind_Press);
          snap[Axis2_X] = snap[Axis2_Y] = 1;
          ui_eat_event(evt);
          continue;
        }
        if(is_cancel)
        {
          rad_lsp_completion_close();
          cmp_shown = 0;
          ui_eat_event(evt);
          continue;
        }
      }
      if(cmp_eat_newline && evt->kind == UI_EventKind_Text && str8_match(evt->string, str8_lit("\n"), 0))
      {
        cmp_eat_newline = 0;
        ui_eat_event(evt);
        continue;
      }

      if(evt->kind != UI_EventKind_Navigate && evt->kind != UI_EventKind_Edit && evt->kind != UI_EventKind_Text)
      {
        continue;
      }
      U64 start_cursor = *cursor;
      U64 start_mark = *mark;
      Vec2S32 delta = evt->delta_2s32;
      U64 line_count = text_patched.line_map.total_line_count;
      U64 line_num = txt_line_num_from_off(&text_patched.line_map, *cursor);
      Rng1U64 line_range = txt_range_from_line_num(&text_patched.line_map, line_num);
      String8 line = {0};
      line.size = dim_1u64(line_range);
      line.str = push_array(scratch.arena, U8, line.size);
      memory_map_read(&text_patched.memory_map, line_range, line.str);

      //- rad: interpret event as single-line text op
      UI_TxtOp single_line_op = ui_single_line_txt_op_from_event(scratch.arena, evt, line, line_range, *cursor, *mark);

      //- rad: read-only views skip replace-ranges (same as raddbg)
      if(!cv->editable && (single_line_op.replace.size != 0 || evt->flags & UI_EventFlag_Delete))
      {
        continue;
      }

      //- rad: newline insertion -> auto-indent with the current line's leading
      // whitespace; CRLF files get CRLF line ends
      if(cv->editable && evt->string.size != 0)
      {
        String8 insert = evt->string;
        if(str8_match(insert, str8_lit("\n"), 0))
        {
          U64 indent_size = 0;
          for(;indent_size < line.size && (line.str[indent_size] == ' ' || line.str[indent_size] == '\t'); indent_size += 1);
          indent_size = Min(indent_size, (*cursor > line_range.min ? *cursor - line_range.min : 0));
          insert = push_str8f(scratch.arena, "\n%S", str8_prefix(line, indent_size));
        }
        if(str8_find_needle(insert, 0, str8_lit("\n"), 0) < insert.size)
        {
          // rad: normalize line ends to the file's convention ("\r\n" or "\n");
          // NOTE: not str8_split - it drops trailing empty pieces, which would
          // turn a lone "\n" into an empty insertion.
          U8 *normalized = push_array(scratch.arena, U8, insert.size*2);
          U64 normalized_size = 0;
          for EachIndex(idx, insert.size)
          {
            U8 c = insert.str[idx];
            if(c == '\r' && idx+1 < insert.size && insert.str[idx+1] == '\n')
            {
              continue;
            }
            if(c == '\n' && buffer->crlf)
            {
              normalized[normalized_size++] = '\r';
            }
            normalized[normalized_size++] = c;
          }
          insert = str8(normalized, normalized_size);
        }
        if(insert.str != evt->string.str)
        {
          single_line_op.replace = insert;
        }
      }

      //- rad: apply single-line navigations
      *cursor = single_line_op.cursor;
      *mark = single_line_op.mark;

      //- rad: determine if we need to navigate
      B32 need_nav = (start_cursor == start_mark || !(evt->flags & UI_EventFlag_ZeroDeltaOnSelect));

      //- rad: wrap lines right
      if(need_nav && evt->delta_unit != UI_EventDeltaUnit_Whole && evt->delta_unit != UI_EventDeltaUnit_Line && delta.x > 0 && start_cursor == line_range.max && line_num+1 <= line_count)
      {
        Rng1U64 next_line_range = txt_range_from_line_num(&text_patched.line_map, line_num+1);
        *cursor = next_line_range.min;
        *preferred_column = 1;
      }

      //- rad: wrap lines left
      if(need_nav && evt->delta_unit != UI_EventDeltaUnit_Whole && evt->delta_unit != UI_EventDeltaUnit_Line && delta.x < 0 && start_cursor == line_range.min && line_num-1 >= 1)
      {
        Rng1U64 prev_line_range = txt_range_from_line_num(&text_patched.line_map, line_num-1);
        *cursor = prev_line_range.max;
        *preferred_column = (S64)dim_1u64(prev_line_range)+1;
      }

      //- rad: movement down (plain)
      if(need_nav && evt->delta_unit == UI_EventDeltaUnit_Char && delta.y > 0 && line_num+1 <= line_count)
      {
        Rng1U64 next_line_range = txt_range_from_line_num(&text_patched.line_map, line_num+1);
        *cursor = next_line_range.min + *preferred_column;
        *cursor = clamp_1u64(next_line_range, *cursor);
      }

      //- rad: movement up (plain)
      if(need_nav && evt->delta_unit == UI_EventDeltaUnit_Char && delta.y < 0 && line_num > 1)
      {
        Rng1U64 prev_line_range = txt_range_from_line_num(&text_patched.line_map, line_num-1);
        *cursor = prev_line_range.min + *preferred_column;
        *cursor = clamp_1u64(prev_line_range, *cursor);
      }

      //- rad: movement down (chunk)
      if(need_nav && evt->delta_unit == UI_EventDeltaUnit_Word && delta.y > 0 && line_num+1 <= line_count)
      {
        B32 done = 0;
        for(U64 scan_line_num = line_num+1; !done && scan_line_num <= line_count; scan_line_num += 1)
        {
          Temp scratch = scratch_begin(&arena, 1);
          Rng1U64 line_range = txt_range_from_line_num(&text_patched.line_map, scan_line_num);
          String8 line = memory_map_data_from_range(scratch.arena, &text_patched.memory_map, line_range);
          String8 line_without_whitespace = str8_skip_chop_whitespace(line);
          if(line_without_whitespace.size == 0)
          {
            *cursor = line_range.min + (U64)(line_without_whitespace.str - line.str);
            done = 1;
          }
          else if(scan_line_num == line_count)
          {
            *cursor = text_patched.size;
          }
          scratch_end(scratch);
        }
      }

      //- rad: movement up (chunk)
      if(need_nav && evt->delta_unit == UI_EventDeltaUnit_Word && delta.y < 0 && line_num > 1)
      {
        B32 done = 0;
        for(U64 scan_line_num = line_num-1; !done && scan_line_num > 0; scan_line_num -= 1)
        {
          Temp scratch = scratch_begin(&arena, 1);
          Rng1U64 line_range = txt_range_from_line_num(&text_patched.line_map, scan_line_num);
          String8 line = memory_map_data_from_range(scratch.arena, &text_patched.memory_map, line_range);
          String8 line_without_whitespace = str8_skip_chop_whitespace(line);
          if(line_without_whitespace.size == 0)
          {
            *cursor = line_range.min + (U64)(line_without_whitespace.str - line.str);
            done = 1;
          }
          else if(scan_line_num == 1)
          {
            *cursor = 0;
          }
          scratch_end(scratch);
        }
      }

      //- rad: movement down (page)
      if(need_nav && evt->delta_unit == UI_EventDeltaUnit_Page && delta.y > 0)
      {
        U64 advance = line_count_per_page;
        U64 next_line = line_num + advance;
        U64 next_line_clamped = Clamp(1, next_line, text_patched.line_map.total_line_count);
        Rng1U64 next_line_range = txt_range_from_line_num(&text_patched.line_map, next_line_clamped);
        *cursor = next_line_range.min;
      }

      //- rad: movement up (page)
      if(need_nav && evt->delta_unit == UI_EventDeltaUnit_Page && delta.y < 0)
      {
        S64 advance = -(S64)line_count_per_page;
        if(line_num < line_count_per_page)
        {
          advance = -(S64)(line_num - 1);
        }
        U64 next_line = (U64)((S64)line_num + advance);
        U64 next_line_clamped = Clamp(1, next_line, text_patched.line_map.total_line_count);
        Rng1U64 next_line_range = txt_range_from_line_num(&text_patched.line_map, next_line_clamped);
        *cursor = next_line_range.min;
      }

      //- rad: movement to endpoint (+)
      if(need_nav && evt->delta_unit == UI_EventDeltaUnit_Whole && (delta.y > 0 || delta.x > 0))
      {
        *cursor = text_patched.size;
      }

      //- rad: movement to endpoint (-)
      if(need_nav && evt->delta_unit == UI_EventDeltaUnit_Whole && (delta.y < 0 || delta.x < 0))
      {
        *cursor = 0;
      }

      //- rad: get replaced-range; adjust based on multi-line logic (same as raddbg)
      Rng1U64 replaced_range = single_line_op.range;
      if(*cursor != single_line_op.cursor && (evt->flags & UI_EventFlag_Delete || evt->string.size != 0))
      {
        replaced_range = r1u64(*mark, *cursor);
      }

      //- rad: in some cases, we want to pick a selection side based on the delta
      if(*cursor != *mark && evt->flags & UI_EventFlag_PickSelectSide)
      {
        if(delta.x < 0 || delta.y < 0)
        {
          *cursor = *mark = Min(*cursor, *mark);
        }
        else if(delta.x > 0 || delta.y > 0)
        {
          *cursor = *mark = Max(*cursor, *mark);
        }
      }

      //- rad: do copy if needed
      if(evt->flags & UI_EventFlag_Copy)
      {
        String8 text = memory_map_data_from_range(scratch.arena, &text_patched.memory_map, r1u64(*cursor, *mark));
        wm_set_clipboard_text(text);
      }

      //- rad: stick mark to cursor, when we don't want to keep it in the same spot
      if(!(evt->flags & UI_EventFlag_KeepMark))
      {
        *mark = *cursor;
      }

      //- rad: not in raddbg: typing the closing bracket a completion inserted,
      // right before it, steps over it (Visual Studio)
      String8 typeover_string = str8(&cv->typeover_char, 1);
      if(cv->editable && cv->typeover_armed && evt->kind == UI_EventKind_Text && !(evt->flags & UI_EventFlag_Paste) &&
         str8_match(evt->string, typeover_string, 0) && str8_match(single_line_op.replace, typeover_string, 0) &&
         replaced_range.min == replaced_range.max && start_cursor == start_mark)
      {
        U64 off = cv->typeover_off;
        if(tb_buffer_map_offset(buffer, cv->typeover_version, &off) && off == replaced_range.min && off < text_patched.size &&
           str8_match(memory_map_data_from_range(scratch.arena, &text_patched.memory_map, r1u64(off, off+1)), typeover_string, 0))
        {
          replaced_range = r1u64(off, off);
          single_line_op.replace = str8_zero();
          *cursor = *mark = off+1;
        }
        cv->typeover_armed = 0;
      }

      //- rad: push patch if we have one (same as raddbg, plus undo history);
      // moving the cursor without editing ends the current undo group
      if(cv->editable && (replaced_range.max != replaced_range.min || single_line_op.replace.size != 0))
      {
        // rad: not in raddbg: one typed character (completion triggers), a
        // deletion (the list follows), anything else (it closes)
        if(evt->kind == UI_EventKind_Text && !(evt->flags & UI_EventFlag_Paste) && evt->key != WM_Key_Tab &&
           evt->string.size == 1 && single_line_op.replace.size == 1 && replaced_range.min == replaced_range.max)
        {
          cmp_typed = 1;
          cmp_typed_char = evt->string.str[0];
        }
        else if(!(evt->flags & UI_EventFlag_Delete) || single_line_op.replace.size != 0 || start_cursor != start_mark)
        {
          cmp_moved = 1;
        }
        tb_buffer_edit(buffer, &text_patched, replaced_range, single_line_op.replace);
        text_patched = txt_patched_from_info_data_patches(scratch.arena, text_info, text_data, &buffer->patches);
        *cursor = *mark = replaced_range.min + single_line_op.replace.size;
        U64 line_num = txt_line_num_from_off(&text_patched.line_map, *cursor);
        Rng1U64 line_range = txt_range_from_line_num(&text_patched.line_map, line_num);
        *preferred_column = (*cursor - line_range.min);
        snap[Axis2_Y] = 1;
      }
      else if(*cursor != start_cursor)
      {
        tb_buffer_close_undo_group(buffer);
        cmp_moved = 1;
      }

      //- rad: consume
      ui_eat_event(evt);

      //- rad: changed cursor -> snap in X
      if(*cursor != start_cursor)
      {
        snap[Axis2_X] = 1;
      }

      //- rad: changed cursor line -> snap in Y
      if(*cursor < line_range.min || line_range.max < *cursor)
      {
        snap[Axis2_Y] = 1;
      }
    }
  }

  //////////////////////////////
  //- rad: not in raddbg: the `)` step-over ends once the cursor leaves the
  // parentheses (or the text is no longer the one it was armed in)
  //
  if(cv->typeover_armed)
  {
    U64 min = cv->typeover_min;
    U64 max = cv->typeover_off;
    if(!tb_buffer_map_offset_side(buffer, cv->typeover_version, &min, Side_Min) ||
       !tb_buffer_map_offset(buffer, cv->typeover_version, &max) ||
       cv->cursor < min || max < cv->cursor || cv->cursor != cv->mark)
    {
      cv->typeover_armed = 0;
    }
  }

  //////////////////////////////
  //- rad: not in raddbg: completion list: start, follow or close it
  //
  if(text_is_ready)
  {
    rad_code_view_completion_update(tab, cv, buffer, &text_patched, view_focused, cmp_typed, cmp_typed_char, cmp_moved);
  }

  //////////////////////////////
  //- rad: determine visible line range / count
  //
  Rng1S64 visible_line_num_range = r1s64(scroll_pos.y.idx + (S64)(scroll_pos.y.off) + 1 - !!(scroll_pos.y.off < 0),
                                         scroll_pos.y.idx + (S64)(scroll_pos.y.off) + 1 + num_possible_visible_lines);
  Rng1S64 target_visible_line_num_range = r1s64(scroll_pos.y.idx + 1,
                                                scroll_pos.y.idx + 1 + num_possible_visible_lines);
  U64 visible_line_count = 0;
  {
    visible_line_num_range.min = Clamp(1, visible_line_num_range.min, (S64)text_patched.line_map.total_line_count);
    visible_line_num_range.max = Clamp(1, visible_line_num_range.max, (S64)text_patched.line_map.total_line_count);
    visible_line_num_range.min = Max(1, visible_line_num_range.min);
    visible_line_num_range.max = Max(1, visible_line_num_range.max);
    target_visible_line_num_range.min = Clamp(1, target_visible_line_num_range.min, (S64)text_patched.line_map.total_line_count);
    target_visible_line_num_range.max = Clamp(1, target_visible_line_num_range.max, (S64)text_patched.line_map.total_line_count);
    target_visible_line_num_range.min = Max(1, target_visible_line_num_range.min);
    target_visible_line_num_range.max = Max(1, target_visible_line_num_range.max);
    visible_line_count = (U64)dim_1s64(visible_line_num_range)+1;
  }

  //////////////////////////////
  //- rad: calculate scroll bounds
  //
  S64 line_size_x = 0;
  Rng1S64 scroll_idx_rng[Axis2_COUNT] = {0};
  {
    line_size_x = (text_info->lines_max_size*big_glyph_advance*3)/2;
    line_size_x = ClampBot(line_size_x, (S64)big_glyph_advance*120);
    line_size_x = ClampBot(line_size_x, (S64)code_area_dim.x);
    scroll_idx_rng[Axis2_X] = r1s64(0, line_size_x-(S64)code_area_dim.x);
    scroll_idx_rng[Axis2_Y] = r1s64(0, (S64)text_patched.line_map.total_line_count-1);
  }

  //////////////////////////////
  //- rad: calculate line-range-dependent info
  //
  F32 line_num_width_px = 0;
  if(do_line_numbers)
  {
    line_num_width_px = floor_f32(big_glyph_advance * (log10(visible_line_num_range.max) + 3));
  }
  Rng1U64 visible_byte_range = r1u64(txt_range_from_line_num(&text_patched.line_map, visible_line_num_range.min).min,
                                     txt_range_from_line_num(&text_patched.line_map, visible_line_num_range.max).max);
  String8 visible_data = memory_map_data_from_range(scratch.arena, &text_patched.memory_map, visible_byte_range);

  //////////////////////////////
  //- rad: find tokens for visible byte range
  //
  U64 ctx_token_pt_num = txt_token_pt_num_from_off(&text_patched.token_pt_map, visible_byte_range.min);
  TXT_TokenPt ctx_token_pt = txt_token_pt_from_num(&text_patched.token_pt_map, ctx_token_pt_num);
  TXT_TokenArray tokens = txt_token_array_from_data(scratch.arena, lang_kind, ctx_token_pt, visible_data, visible_byte_range.min, max_U64);

  //////////////////////////////
  //- rad: selection on single line, no query? -> set search text (same as
  // raddbg: Ctrl+F / F3 then search for the selected text). only written
  // when it differs (raddbg replaces it every frame).
  //
  if(tab != &cfg_nil_node)
  {
    U64 cursor_line_num = txt_line_num_from_off(&text_patched.line_map, cv->cursor);
    Rng1U64 cursor_line_range = txt_range_from_line_num(&text_patched.line_map, cursor_line_num);
    if(cursor_line_range.min <= cv->mark && cv->mark <= cursor_line_range.max)
    {
      RAD_ViewState *vs = rad_view_state_from_cfg(tab);
      if(!vs->query_is_open)
      {
        CFG_Node *query = cfg_node_child_from_string_or_alloc(rad_state->cfg, tab, str8_lit("query"));
        CFG_Node *input = cfg_node_child_from_string_or_alloc(rad_state->cfg, query, str8_lit("input"));
        String8 text = memory_map_data_from_range(scratch.arena, &text_patched.memory_map, r1u64(cv->cursor, cv->mark));
        if(text.size >= 256)
        {
          text = str8_zero();
        }
        if(!str8_match(input->first->string, text, 0))
        {
          cfg_node_new_replace(rad_state->cfg, input, text);
        }
      }
    }
  }

  //////////////////////////////
  //- rad: get active search query
  //
  String8 search_query = cfg_node_child_from_string(cfg_node_child_from_string(tab, str8_lit("query")), str8_lit("input"))->first->string;

  //////////////////////////////
  //- rad: prepare code slice info bundle, for the viewable region of text
  //
  RAD_CodeSliceParams code_slice_params = {0};
  {
    // rad: fill basics
    code_slice_params.flags = RAD_CodeSliceFlag_Clickable;
    if(do_line_numbers)
    {
      code_slice_params.flags |= RAD_CodeSliceFlag_LineNums;
    }
    code_slice_params.line_num_range            = visible_line_num_range;
    code_slice_params.line_text                 = push_array(scratch.arena, String8, visible_line_count);
    code_slice_params.line_ranges               = push_array(scratch.arena, Rng1U64, visible_line_count);
    code_slice_params.line_tokens               = push_array(scratch.arena, TXT_TokenArray, visible_line_count);
    code_slice_params.text_info                 = text_info;
    code_slice_params.patches                   = &buffer->patches;
    code_slice_params.text_data                 = text_data;
    code_slice_params.font                      = code_font;
    code_slice_params.font_size                 = code_font_size;
    code_slice_params.tab_size                  = code_tab_size;
    code_slice_params.line_height_px            = code_line_height;
    code_slice_params.search_query              = search_query;
    code_slice_params.line_num_width_px         = line_num_width_px;
    code_slice_params.line_text_max_width_px    = (F32)line_size_x;
    code_slice_params.margin_float_off_px       = scroll_pos.x.idx + floor_f32(scroll_pos.x.off);
    code_slice_params.min_height_px             = code_area_dim.y + 2*code_line_height;
    if(cv->editable)
    {
      code_slice_params.flags |= RAD_CodeSliceFlag_CtrlLinks;
    }

    // rad: fill line text / ranges
    {
      S64 line_num = visible_line_num_range.min;
      U64 line_idx = visible_line_num_range.min-1;
      for(U64 visible_line_idx = 0;
          visible_line_idx < visible_line_count && line_idx < text_patched.line_map.total_line_count;
          visible_line_idx += 1, line_idx += 1, line_num += 1)
      {
        Rng1U64 line_range = txt_range_from_line_num(&text_patched.line_map, line_num);
        String8 line_text = {0};
        line_text.size = dim_1u64(line_range);
        line_text.str = push_array(scratch.arena, U8, line_text.size);
        memory_map_read(&text_patched.memory_map, line_range, line_text.str);
        code_slice_params.line_text[visible_line_idx]   = line_text;
        code_slice_params.line_ranges[visible_line_idx] = line_range;
      }
    }

    // rad: bucket tokens by line
    {
      U64 token_idx = 0;
      for EachIndex(line_idx, visible_line_count)
      {
        Temp scratch2 = scratch_begin(&scratch.arena, 1);
        Rng1U64 line_range = code_slice_params.line_ranges[line_idx];
        TXT_TokenList line_tokens = {0};
        for(;token_idx < tokens.count;)
        {
          Rng1U64 token_range = tokens.v[token_idx].range;
          if(dim_1u64(intersect_1u64(token_range, line_range)) != 0)
          {
            txt_token_list_push(scratch2.arena, &line_tokens, &tokens.v[token_idx]);
          }
          if(token_range.max <= line_range.max || token_range.max <= line_range.min)
          {
            token_idx += 1;
          }
          else if(line_range.max <= token_range.max)
          {
            break;
          }
        }
        code_slice_params.line_tokens[line_idx] = txt_token_array_from_list(scratch.arena, &line_tokens);
        scratch_end(scratch2);
      }
    }

    // rad: not in raddbg: the last cargo command's diagnostics in this file,
    // where they are now (they follow edits). an error colors its line and
    // annotates it like raddbg's exception line ("bad_pop"); a warning
    // annotates only. both underline their span.
    // NOTE(rad): the theme has no warning color; `thread_0` is the yellow /
    // orange of every preset (rustc prints warnings in yellow).
    RAD_Document *doc = rad_document_from_tab(tab);
    if(doc != 0 && rad_problems->count != 0 && text_is_ready)
    {
      Vec4F32 error_line_color = {0};
      Vec4F32 error_text_color = {0};
      Vec4F32 error_mark_color = {0};
      Vec4F32 weak_text_color = {0};
      Vec4F32 warning_mark_color = ui_color_from_name(str8_lit("thread_0"));
      UI_TagF("bad_pop")
      {
        error_line_color = ui_color_from_name(str8_lit("background"));
        error_text_color = ui_color_from_name(str8_lit("text"));
      }
      UI_TagF("bad")
      {
        error_mark_color = ui_color_from_name(str8_lit("text"));
      }
      UI_TagF("weak")
      {
        weak_text_color = ui_color_from_name(str8_lit("text"));
      }
      code_slice_params.line_bg_colors   = push_array(scratch.arena, Vec4F32, visible_line_count);
      code_slice_params.line_annotations = push_array(scratch.arena, DR_FStrList, visible_line_count);
      RAD_CodeSliceUnderline *last_underline = 0;
      DR_FStrParams fstr_params = {rad_font_from_slot(RAD_FontSlot_Main), rad_raster_flags_from_slot(RAD_FontSlot_Main), weak_text_color, code_font_size};
      for EachIndex(diag_idx, rad_problems->count)
      {
        RAD_Diagnostic *diag = rad_problems->diags[diag_idx];
        if(!str8_match(diag->path, doc->path, StringMatchFlag_CaseInsensitive|StringMatchFlag_SlashInsensitive))
        {
          continue;
        }
        Rng1U64 range = rad_diagnostic_range_from_document(diag, doc, &text_patched);
        B32 is_error = (diag->kind == RAD_DiagnosticKind_Error);
        B32 is_warning = (diag->kind == RAD_DiagnosticKind_Warning);

        // rad: span underline
        if((is_error || is_warning) && range.min <= visible_byte_range.max && visible_byte_range.min <= range.max)
        {
          RAD_CodeSliceUnderline *underline = push_array(scratch.arena, RAD_CodeSliceUnderline, 1);
          underline->range = range;
          underline->color = (is_error ? error_mark_color : warning_mark_color);
          if(is_error && txt_line_num_from_off(&text_patched.line_map, range.min) == txt_line_num_from_off(&text_patched.line_map, range.max))
          {
            // rad: on its error-colored line, a red mark would vanish
            underline->color = error_text_color;
          }
          SLLQueuePush(code_slice_params.first_underline, last_underline, underline);
        }

        // rad: line color + annotation: [icon] message
        S64 line_num = (S64)txt_line_num_from_off(&text_patched.line_map, range.min);
        if(visible_line_num_range.min <= line_num && line_num <= visible_line_num_range.max)
        {
          U64 line_idx = (U64)(line_num - visible_line_num_range.min);
          DR_FStrList *fstrs = &code_slice_params.line_annotations[line_idx];
          RAD_IconKind icon_kind = (is_error ? RAD_IconKind_WarningBig : is_warning ? RAD_IconKind_WarningSmall : RAD_IconKind_Info);
          Vec4F32 icon_color = (is_error ? error_text_color : is_warning ? warning_mark_color : weak_text_color);
          Vec4F32 text_color = (is_error ? error_text_color : weak_text_color);
          if(is_error)
          {
            code_slice_params.line_bg_colors[line_idx] = error_line_color;
          }
          if(fstrs->node_count != 0)
          {
            dr_fstrs_push_new(scratch.arena, fstrs, &fstr_params, str8_lit("    "));
          }
          dr_fstrs_push_new(scratch.arena, fstrs, &fstr_params, rad_icon_kind_text_table[icon_kind],
                            .font = rad_font_from_slot(RAD_FontSlot_Icons),
                            .raster_flags = rad_raster_flags_from_slot(RAD_FontSlot_Icons),
                            .color = icon_color);
          dr_fstrs_push_new(scratch.arena, fstrs, &fstr_params, str8_lit("  "));
          dr_fstrs_push_new(scratch.arena, fstrs, &fstr_params, diag->message, .color = text_color);
        }
      }
    }
  }

  //////////////////////////////
  //- rad: build container
  //
  UI_Box *container_box = &ui_nil_box;
  {
    ui_set_next_pref_width(ui_px(code_area_dim.x, 1));
    ui_set_next_pref_height(ui_px(code_area_dim.y, 1));
    ui_set_next_child_layout_axis(Axis2_Y);
    container_box = ui_build_box_from_stringf(UI_BoxFlag_Clip|
                                              UI_BoxFlag_Scroll|
                                              UI_BoxFlag_AllowOverflowX|
                                              UI_BoxFlag_AllowOverflowY,
                                              "###code_area");
  }

  //////////////////////////////
  //- rad: do searching operations (port of raddbg's code view search; the
  // match is selected, mark at its start, cursor at its end)
  //
  if(cv->find_text_size != 0 && text_is_ready)
  {
    U64 search_chunk_size = KB(4);
    String8 needle = str8(cv->find_text_buffer, cv->find_text_size);

    //- rad: find text (forward)
    if(!cv->find_text_backward)
    {
      Rng1U64 ranges[] =
      {
        r1u64(cv->cursor+1, text_patched.size),
        r1u64(0, cv->cursor+1),
      };
      B32 found = 0;
      for EachElement(range_idx, ranges)
      {
        for(U64 off = ranges[range_idx].min; off < ranges[range_idx].max; off += search_chunk_size)
        {
          Temp temp = temp_begin(scratch.arena);
          String8 data = memory_map_data_from_range(temp.arena, &text_patched.memory_map, r1u64(off, off+search_chunk_size));
          U64 hint_needle_pos = str8_find_needle(data, 0, needle, StringMatchFlag_CaseInsensitive|StringMatchFlag_RightSideSloppy);
          if(hint_needle_pos < data.size)
          {
            String8 candidate = memory_map_data_from_range(temp.arena, &text_patched.memory_map, r1u64(off+hint_needle_pos, off+hint_needle_pos+needle.size));
            if(str8_match(candidate, needle, StringMatchFlag_CaseInsensitive))
            {
              found = 1;
              cv->mark = off+hint_needle_pos;
              cv->cursor = cv->mark + needle.size;
            }
          }
          temp_end(temp);
          if(found) { goto done_fwd; }
        }
      }
      done_fwd:;
      if(!found)
      {
        log_user_errorf("Could not find `%S`", needle);
      }
      cv->center_cursor = found;
    }

    //- rad: find text (backward)
    else if(cv->cursor > 0)
    {
      Rng1U64 ranges[] =
      {
        r1u64(0, Min(cv->cursor, cv->mark)),
        r1u64(cv->cursor+1, text_patched.size),
      };
      B32 found = 0;
      for EachElement(range_idx, ranges)
      {
        // NOTE(rad): not in raddbg: an empty range ending at 0 would wrap
        // start_off around to ~2^64 and walk down ~2^52 empty chunks
        if(ranges[range_idx].max == 0)
        {
          continue;
        }
        U64 start_off = ranges[range_idx].max + (search_chunk_size-1);
        start_off -= start_off%search_chunk_size;
        start_off -= search_chunk_size;
        for(U64 off = start_off;; off -= search_chunk_size)
        {
          Temp temp = temp_begin(scratch.arena);
          String8 data = memory_map_data_from_range(temp.arena, &text_patched.memory_map, r1u64(off, Min(off+search_chunk_size, ranges[range_idx].max)));
          U64 hint_needle_pos = str8_find_needle(data, 0, needle, StringMatchFlag_CaseInsensitive|StringMatchFlag_RightSideSloppy);
          for(;hint_needle_pos < data.size;)
          {
            U64 next_hint_needle_pos = str8_find_needle(data, hint_needle_pos+1, needle, StringMatchFlag_CaseInsensitive|StringMatchFlag_RightSideSloppy);
            if(next_hint_needle_pos < data.size)
            {
              hint_needle_pos = next_hint_needle_pos;
            }
            else
            {
              break;
            }
          }
          if(hint_needle_pos < data.size)
          {
            String8 candidate = memory_map_data_from_range(temp.arena, &text_patched.memory_map, r1u64(off+hint_needle_pos, off+hint_needle_pos+needle.size));
            if(str8_match(candidate, needle, StringMatchFlag_CaseInsensitive))
            {
              found = 1;
              cv->mark = off+hint_needle_pos;
              cv->cursor = cv->mark + needle.size;
            }
          }
          temp_end(temp);
          if(found) { goto done_bwd; }
          if(off == 0) { break; }
        }
      }
      done_bwd:;
      if(!found)
      {
        log_user_errorf("Could not find `%S`", needle);
      }
      cv->center_cursor = found;
    }
    cv->find_text_size = 0;
  }

  //////////////////////////////
  //- rad: do goto line
  //
  if(cv->goto_line_num != 0 && text_is_ready)
  {
    S64 line_num = cv->goto_line_num;
    S64 column_num = cv->goto_column_num;
    B32 column_in_bytes = cv->goto_column_in_bytes;
    cv->goto_line_num = 0;
    cv->goto_column_num = 0;
    cv->goto_column_in_bytes = 0;
    line_num = Clamp(1, line_num, (S64)text_patched.line_map.total_line_count);
    Rng1U64 range = txt_range_from_line_num(&text_patched.line_map, line_num);
    cv->cursor = cv->mark = range.min;

    // rad: not in raddbg: a column too (Go To Location: 1-based, characters)
    if(column_num > 1)
    {
      cv->cursor = cv->mark = rad_offset_from_line_column(&text_patched, line_num, column_num, column_in_bytes);
      cv->preferred_column = (S64)(cv->cursor - range.min);
    }
    cv->center_cursor = !cv->force_contain_only && (!cv->contain_cursor || (line_num < target_visible_line_num_range.min+4 || target_visible_line_num_range.max-4 < line_num));
  }

  //////////////////////////////
  //- rad: build container contents
  //
  UI_Parent(container_box)
  {
    //- rad: build fractional space
    container_box->view_off.x = container_box->view_off_target.x = scroll_pos.x.idx + scroll_pos.x.off;
    container_box->view_off.y = container_box->view_off_target.y = code_line_height*mod_f32(scroll_pos.y.off, 1.f) + code_line_height*(scroll_pos.y.off < 0) - code_line_height*(scroll_pos.y.off == -1.f && scroll_pos.y.idx == 1);

    //- rad: build code slice
    RAD_CodeSliceSignal sig = {0};
    U64 cursor_before_slice = cv->cursor;
    U64 mark_before_slice = cv->mark;
    UI_Focus(UI_FocusKind_On)
    {
      sig = rad_code_slicef(&code_slice_params, &cv->cursor, &cv->mark, &cv->preferred_column, "code_slice");
    }

    //- rad: press code slice? -> focus panel (same as raddbg)
    if(ui_pressed(sig.base))
    {
      rad_cmd(RAD_CmdKind_FocusPanel, .panel = tab->parent->id);
    }

    //- rad: not in raddbg: the mouse moved the cursor -> the completion list
    // closes; else it is placed under its word's start (next frame)
    if(cmp->active && cmp->tab == tab->id)
    {
      if(cv->cursor != cursor_before_slice || cv->mark != mark_before_slice)
      {
        rad_lsp_completion_close();
      }
      else if(text_is_ready)
      {
        S64 line_num = (S64)txt_line_num_from_off(&text_patched.line_map, cmp->word_start);
        if(visible_line_num_range.min <= line_num && line_num <= visible_line_num_range.max)
        {
          Rng1U64 line_range = txt_range_from_line_num(&text_patched.line_map, (U64)line_num);
          String8 prefix = memory_map_data_from_range(scratch.arena, &text_patched.memory_map, r1u64(line_range.min, Max(line_range.min, cmp->word_start)));
          F32 advance = fnt_dim_from_tag_size_string(code_font, code_font_size, 0, code_tab_size, prefix).x;
          F32 x = sig.text_rect.x0 + sig.text_x_off + advance;
          F32 y = sig.text_rect.y0 + (F32)(line_num - visible_line_num_range.min)*code_line_height;
          cmp->anchor_line_rect = r2f32p(x, y, x + code_line_height, y + code_line_height);
          cmp->anchor_frame = rad_state->frame_index;
        }
      }
    }

    //- rad: not in raddbg: hover (Quick Info): the mouse rests on a token
    // -> the file's language server -> its answer in a tooltip
    {
      RAD_Document *hover_doc = rad_document_from_tab(tab);
      B32 mouse_on_token = (hover_doc != 0 && text_is_ready && sig.mouse_over_text &&
                            ui_hovering(sig.base) && !ui_dragging(sig.base) && !ui_any_ctx_menu_is_open());
      String8 hover_text = (mouse_on_token ? rad_lsp_hover_from_mouse(hover_doc, sig.mouse_token_range, sig.mouse_char_off) : str8_zero());
      if(hover_text.size != 0) UI_Tooltip
      {
        rad_markdown_ui(hover_text, ui_top_font_size()*45.f, 24);
      }
    }

    //- rad: not in raddbg: Ctrl+click on an identifier -> Go To Definition
    // of that identifier (the cursor stays where it was until the jump)
    if(sig.link_pressed)
    {
      rad_cmd(RAD_CmdKind_GoToDefinition, .tab = tab->id, .has_text_offset = 1, .text_offset = sig.link_off);
    }
    cv->layout_link_hovered = sig.link_hovered;
    cv->layout_link_range   = sig.link_range;

    //- rad: double-click -> remember where (the Output view goes to the
    // location written there)
    if(ui_double_clicked(sig.base))
    {
      cv->double_clicked = 1;
      cv->double_clicked_off = sig.mouse_off;
    }

    //- rad: record layout (for scripted input)
    cv->layout_code_rect     = container_box->rect;
    cv->layout_text_rect     = sig.text_rect;
    cv->layout_text_x_off    = sig.text_x_off;
    cv->layout_line_height   = code_line_height;
    cv->layout_visible_lines = visible_line_num_range;
    cv->layout_font          = code_font;
    cv->layout_font_size     = code_font_size;
    cv->layout_tab_size      = code_tab_size;

    //- rad: dragging & outside region? -> contain cursor
    if(ui_dragging(sig.base) && sig.base.event_flags == 0)
    {
      if(!contains_2f32(sig.base.box->rect, ui_mouse()))
      {
        cv->contain_cursor = 1;
      }
      else
      {
        snap[Axis2_X] = 1;
      }
    }
  }

  //////////////////////////////
  //- rad: apply post-build view snapping rules
  //
  if(text_is_ready)
  {
    U64 cursor = cv->cursor;
    U64 cursor_line_num = txt_line_num_from_off(&text_patched.line_map, cursor);
    B32 cursor_in_range = (1 <= cursor_line_num && cursor_line_num <= text_patched.line_map.total_line_count);

    // rad: contain => snap
    if(cv->contain_cursor)
    {
      cv->contain_cursor = 0;
      snap[Axis2_X] = 1;
      snap[Axis2_Y] = 1;
    }

    // rad: center cursor
    if(cv->center_cursor)
    {
      cv->center_cursor = 0;
      if(cursor_in_range)
      {
        Rng1U64 cursor_line_range = txt_range_from_line_num(&text_patched.line_map, cursor_line_num);
        String8 cursor_line = memory_map_data_from_range(scratch.arena, &text_patched.memory_map, cursor_line_range);
        F32 cursor_advance = fnt_dim_from_tag_size_string(code_font, code_font_size, 0, code_tab_size, str8_prefix(cursor_line, cursor-cursor_line_range.min)).x;

        // rad: scroll x
        {
          S64 new_idx = (S64)(cursor_advance - code_area_dim.x/2);
          new_idx = Clamp(scroll_idx_rng[Axis2_X].min, new_idx, scroll_idx_rng[Axis2_X].max);
          ui_scroll_pt_target_idx(&scroll_pos.x, new_idx);
          snap[Axis2_X] = 0;
        }

        // rad: scroll y
        {
          S64 new_idx = ((S64)cursor_line_num-1) - num_possible_visible_lines/2 + 2;
          new_idx = Clamp(scroll_idx_rng[Axis2_Y].min, new_idx, scroll_idx_rng[Axis2_Y].max);
          ui_scroll_pt_target_idx(&scroll_pos.y, new_idx);
          snap[Axis2_Y] = 0;
        }

        // rad: just snap if this is the initial center
        if(cv->initial_center)
        {
          scroll_pos.x.off = 0;
          scroll_pos.y.off = 0;
          cv->initial_center = 0;
        }
      }
    }

    // rad: snap in X
    if(snap[Axis2_X] && cursor_in_range)
    {
      Rng1U64 cursor_line_range = txt_range_from_line_num(&text_patched.line_map, cursor_line_num);
      String8 cursor_line = memory_map_data_from_range(scratch.arena, &text_patched.memory_map, cursor_line_range);
      S64 cursor_off = (S64)(fnt_dim_from_tag_size_string(code_font, code_font_size, 0, code_tab_size, str8_prefix(cursor_line, cursor-cursor_line_range.min)).x + line_num_width_px);
      Rng1S64 visible_pixel_range =
      {
        scroll_pos.x.idx,
        scroll_pos.x.idx + (S64)code_area_dim.x,
      };
      Rng1S64 cursor_pixel_range =
      {
        cursor_off - (S64)(big_glyph_advance*4) - (S64)(line_num_width_px),
        cursor_off + (S64)(big_glyph_advance*4),
      };
      S64 min_delta = Min(0, cursor_pixel_range.min - visible_pixel_range.min);
      S64 max_delta = Max(0, cursor_pixel_range.max - visible_pixel_range.max);
      S64 new_idx = scroll_pos.x.idx+min_delta+max_delta;
      new_idx = Clamp(scroll_idx_rng[Axis2_X].min, new_idx, scroll_idx_rng[Axis2_X].max);
      ui_scroll_pt_target_idx(&scroll_pos.x, new_idx);
    }

    // rad: snap in Y
    if(snap[Axis2_Y])
    {
      Rng1S64 cursor_visibility_range = r1s64((S64)cursor_line_num-4, (S64)cursor_line_num+4);
      cursor_visibility_range.min = ClampBot(0, cursor_visibility_range.min);
      cursor_visibility_range.max = ClampBot(0, cursor_visibility_range.max);
      S64 min_delta = Min(0, cursor_visibility_range.min-(target_visible_line_num_range.min));
      S64 max_delta = Max(0, cursor_visibility_range.max-(target_visible_line_num_range.min+num_possible_visible_lines));
      S64 new_idx = scroll_pos.y.idx+min_delta+max_delta;
      new_idx = Clamp(0, new_idx, (S64)text_patched.line_map.total_line_count-1);
      ui_scroll_pt_target_idx(&scroll_pos.y, new_idx);
    }
  }

  //////////////////////////////
  //- rad: build horizontal scroll bar
  //
  {
    ui_set_next_fixed_x(0);
    ui_set_next_fixed_y(code_area_dim.y);
    ui_set_next_fixed_width(panel_box_dim.x - scroll_bar_dim);
    ui_set_next_fixed_height(scroll_bar_dim);
    {
      scroll_pos.x = ui_scroll_bar(Axis2_X,
                                   ui_px(scroll_bar_dim, 1.f),
                                   scroll_pos.x,
                                   scroll_idx_rng[Axis2_X],
                                   (S64)code_area_dim.x);
    }
  }

  //////////////////////////////
  //- rad: build vertical scroll bar
  //
  {
    ui_set_next_fixed_x(code_area_dim.x);
    ui_set_next_fixed_y(0);
    ui_set_next_fixed_width(scroll_bar_dim);
    ui_set_next_fixed_height(panel_box_dim.y - scroll_bar_dim);
    {
      scroll_pos.y = ui_scroll_bar(Axis2_Y,
                                   ui_px(scroll_bar_dim, 1.f),
                                   scroll_pos.y,
                                   scroll_idx_rng[Axis2_Y],
                                   num_possible_visible_lines);
    }
  }

  //////////////////////////////
  //- rad: top-level container interaction (scrolling)
  //
  if(text_is_ready)
  {
    UI_Signal sig = ui_signal_from_box(container_box);
    if(sig.scroll.x != 0)
    {
      S64 new_idx = scroll_pos.x.idx+sig.scroll.x*big_glyph_advance;
      new_idx = clamp_1s64(scroll_idx_rng[Axis2_X], new_idx);
      ui_scroll_pt_target_idx(&scroll_pos.x, new_idx);
    }
    if(sig.scroll.y != 0)
    {
      S64 new_idx = scroll_pos.y.idx + sig.scroll.y;
      new_idx = clamp_1s64(scroll_idx_rng[Axis2_Y], new_idx);
      ui_scroll_pt_target_idx(&scroll_pos.y, new_idx);
    }
    ui_scroll_pt_clamp_idx(&scroll_pos.x, scroll_idx_rng[Axis2_X]);
    ui_scroll_pt_clamp_idx(&scroll_pos.y, scroll_idx_rng[Axis2_Y]);
  }

  //////////////////////////////
  //- rad: store state
  //
  cv->scroll_pos = scroll_pos;
  cv->force_contain_only = 0;

  scratch_end(scratch);
  ProfEnd();
}

////////////////////////////////
//~ rad: Text View

internal void
rad_text_view_ui(CFG_Node *tab, Rng2F32 rect)
{
  // NOTE(rad): port of raddbg's text view (raddbg/raddbg_views.c,
  // RD_VIEW_UI_FUNCTION_DEF(text)), minus debug-info checksum/override logic.
  RAD_ViewState *vs = rad_view_state_from_cfg(tab);
  RAD_CodeViewState *cv = &vs->cv;
  Temp scratch = scratch_begin(0, 0);
  Access *access = access_open();

  //////////////////////////////
  //- rad: set up invariants
  //
  F32 main_font_size = ui_bottom_font_size();
  F32 bottom_bar_height = main_font_size*2.f;
  Rng2F32 code_area_rect = r2f32p(rect.x0, rect.y0, rect.x1, rect.y1 - bottom_bar_height);
  Rng2F32 bottom_bar_rect = r2f32p(rect.x0, rect.y1 - bottom_bar_height, rect.x1, rect.y1);

  //////////////////////////////
  //- rad: unpack parameterization info
  //
  String8 file_path = rad_file_path_from_tab(tab);
  RAD_Document *doc = rad_document_from_path(file_path);
  if(doc == 0)
  {
    access_close(access);
    scratch_end(scratch);
    return;
  }
  B32 file_is_missing = doc->is_missing;
  TXT_LangKind lang_kind = doc->lang;
  rad_code_view_init(cv);
  cv->editable = 1;
  TB_Buffer *buffer = doc->buffer;

  //////////////////////////////
  //- rad: read the document's text (loading, reloading and compaction
  // happen in rad_documents_tick)
  //
  TB_Text text = tb_text_from_buffer(scratch.arena, access, buffer);
  TXT_TextInfo info = text.base_info;
  String8 data = text.base_data;
  B32 key_has_data = (info.lines_count != 0);

  //////////////////////////////
  //- rad: the text changed since this view last saw it (another tab edited
  // it, undo, a reload) -> carry cursor and mark over to the current text
  //
  if(cv->buffer_version != buffer->version)
  {
    if(!tb_buffer_map_offset(buffer, cv->buffer_version, &cv->cursor) ||
       !tb_buffer_map_offset(buffer, cv->buffer_version, &cv->mark))
    {
      cv->cursor = Min(cv->cursor, text.patched.size);
      cv->mark = Min(cv->mark, text.patched.size);
    }
    cv->buffer_version = buffer->version;
  }

  //////////////////////////////
  //- rad: build missing file interface
  //
  if(file_is_missing)
  {
    UI_WidthFill UI_HeightFill UI_Column UI_Padding(ui_pct(1, 0))
    {
      UI_PrefWidth(ui_children_sum(1)) UI_PrefHeight(ui_em(3, 1))
        UI_Row UI_Padding(ui_pct(1, 0))
        UI_PrefWidth(ui_text_dim(1, 1))
        UI_TagF("weak")
      {
        RAD_Font(RAD_FontSlot_Icons) ui_label(rad_icon_kind_text_table[RAD_IconKind_WarningBig]);
        ui_labelf("Could not find \"%S\".", file_path);
      }
    }
  }

  //////////////////////////////
  //- rad: text not ready yet -> keep requesting frames until the async lex lands
  //
  if(!file_is_missing && !key_has_data)
  {
    rad_store_view_loading_info(tab, 1, info.bytes_processed, info.bytes_to_process);
    rad_request_frame();
  }

  //////////////////////////////
  //- rad: build code contents
  //
  if(!file_is_missing)
  {
    rad_code_view_build(scratch.arena, tab, cv, buffer, code_area_rect, lang_kind, &info, data);
    cv->buffer_version = buffer->version;
  }

  //////////////////////////////
  //- rad: produce patched text info, unpack cursor info in patched text
  //
  TXT_Patched patched = txt_patched_from_info_data_patches(scratch.arena, &info, data, &buffer->patches);
  U64 cursor_line_num = txt_line_num_from_off(&patched.line_map, cv->cursor);
  Rng1U64 cursor_line_range = txt_range_from_line_num(&patched.line_map, cursor_line_num);

  //////////////////////////////
  //- rad: build bottom bar
  //
  if(!file_is_missing && key_has_data) UI_FontSize(main_font_size) UI_TagF(".")
  {
    ui_set_next_rect(shift_2f32(bottom_bar_rect, scale_2f32(rect.p0, -1.f)));
    ui_set_next_flags(UI_BoxFlag_DrawBackground);
    UI_Row
      UI_TextAlignment(UI_TextAlign_Center)
      UI_PrefWidth(ui_text_dim(10, 1))
      UI_TagF("weak")
    {
      RAD_Font(RAD_FontSlot_Code)
      {
        ui_label(file_path);
        ui_spacer(ui_em(1.5f, 1));
        ui_labelf("Line: %I64d, Column: %I64d, Offset: 0x%I64x", cursor_line_num, 1 + cv->cursor - cursor_line_range.min, cv->cursor);
        ui_spacer(ui_pct(1, 0));
        if(buffer->changed_on_disk)
        {
          ui_labelf("(changed on disk)");
        }
        if(tb_buffer_is_dirty(buffer))
        {
          ui_labelf("(modified)");
        }
        ui_labelf("%s",
                  buffer->crlf                               ? "crlf" :
                  info.line_end_kind == TXT_LineEndKind_Null ? "bin" :
                  "lf");
      }
    }
  }

  access_close(access);
  scratch_end(scratch);
}

internal B32
rad_text_view_save(CFG_Node *tab)
{
  return rad_document_save(rad_document_from_tab(tab));
}

internal B32
rad_tab_is_dirty(CFG_Node *tab)
{
  RAD_Document *doc = rad_document_from_tab(tab);
  B32 result = (doc != 0 && tb_buffer_is_dirty(doc->buffer));
  return result;
}

////////////////////////////////
//~ rad: Explorer

#define RAD_EXPLORER_MAX_ROWS 100000
#define RAD_EXPLORER_REFRESH_US 1000000

typedef struct RAD_ExplorerRowNode RAD_ExplorerRowNode;
struct RAD_ExplorerRowNode
{
  RAD_ExplorerRowNode *next;
  RAD_ExplorerRow v;
};

typedef struct RAD_ExplorerRowList RAD_ExplorerRowList;
struct RAD_ExplorerRowList
{
  RAD_ExplorerRowNode *first;
  RAD_ExplorerRowNode *last;
  U64 count;
};

internal String8
rad_explorer_folder_from_tab(CFG_Node *tab)
{
  String8 result = cfg_node_child_from_string(tab, str8_lit("folder"))->first->string;
  return result;
}

internal void
rad_explorer_set_folder(CFG_Node *tab, String8 folder)
{
  //- rad: write the tab's `folder` child
  CFG_Node *folder_cfg = cfg_node_child_from_string_or_alloc(rad_state->cfg, tab, str8_lit("folder"));
  if(folder_cfg->first != &cfg_nil_node)
  {
    cfg_node_equip_string(rad_state->cfg, folder_cfg->first, folder);
  }
  else
  {
    cfg_node_new(rad_state->cfg, folder_cfg, folder);
  }

  //- rad: new root -> forget expansions, selection, scroll
  RAD_ViewState *vs = rad_view_state_from_cfg(tab);
  RAD_ExplorerState *ex = &vs->ex;
  if(ex->initialized)
  {
    arena_clear(ex->expansions_arena);
    ex->first_expansion = ex->last_expansion = ex->free_expansion = 0;
    MemoryZeroStruct(&ex->scroll_pos);
    ex->cursor = ex->mark = v2s64(0, 0);
    ex->selected_path_size = 0;
    ex->rows_dirty = 1;
  }
}

internal B32
rad_explorer_is_expanded(RAD_ExplorerState *ex, String8 path)
{
  B32 result = 0;
  for(RAD_ExplorerExpansion *e = ex->first_expansion; e != 0; e = e->next)
  {
    if(str8_match(e->path, path, 0))
    {
      result = 1;
      break;
    }
  }
  return result;
}

internal void
rad_explorer_set_expanded(RAD_ExplorerState *ex, String8 path, B32 expanded)
{
  RAD_ExplorerExpansion *existing = 0;
  for(RAD_ExplorerExpansion *e = ex->first_expansion; e != 0; e = e->next)
  {
    if(str8_match(e->path, path, 0))
    {
      existing = e;
      break;
    }
  }
  if(expanded && existing == 0)
  {
    RAD_ExplorerExpansion *e = ex->free_expansion;
    if(e != 0)
    {
      SLLStackPop(ex->free_expansion);
    }
    else
    {
      e = push_array_no_zero(ex->expansions_arena, RAD_ExplorerExpansion, 1);
    }
    MemoryZeroStruct(e);
    e->path = push_str8_copy(ex->expansions_arena, path);
    DLLPushBack(ex->first_expansion, ex->last_expansion, e);
  }
  else if(!expanded && existing != 0)
  {
    DLLRemove(ex->first_expansion, ex->last_expansion, existing);
    SLLStackPush(ex->free_expansion, existing);
  }
  ex->rows_dirty = 1;
}

internal int
rad_explorer_file_info_compare(FileInfo *a, FileInfo *b)
{
  // NOTE(rad): folders first, then by name (case-insensitive, then exact)
  int result = 0;
  B32 a_is_folder = !!(a->props.flags & FilePropertyFlag_IsFolder);
  B32 b_is_folder = !!(b->props.flags & FilePropertyFlag_IsFolder);
  if(a_is_folder != b_is_folder)
  {
    result = (a_is_folder ? -1 : +1);
  }
  for(U64 idx = 0; result == 0 && idx < a->name.size && idx < b->name.size; idx += 1)
  {
    U8 ca = lower_from_char(a->name.str[idx]);
    U8 cb = lower_from_char(b->name.str[idx]);
    result = (ca < cb ? -1 : ca > cb ? +1 : 0);
  }
  if(result == 0)
  {
    result = (a->name.size < b->name.size ? -1 : a->name.size > b->name.size ? +1 : 0);
  }
  if(result == 0)
  {
    result = (str8_is_before(a->name, b->name) ? -1 : str8_is_before(b->name, a->name) ? +1 : 0);
  }
  return result;
}

internal void
rad_explorer_rows_push_folder__(Arena *arena, RAD_ExplorerState *ex, RAD_ExplorerRowList *rows, String8 folder, S32 depth)
{
  Temp scratch = scratch_begin(&arena, 1);

  //- rad: list the folder
  typedef struct InfoNode InfoNode;
  struct InfoNode
  {
    InfoNode *next;
    FileInfo v;
  };
  InfoNode *first_info = 0;
  InfoNode *last_info = 0;
  U64 infos_count = 0;
  FileIter *it = file_iter_begin(scratch.arena, folder, 0);
  for(FileInfo info = {0}; file_iter_next(scratch.arena, it, &info);)
  {
    if(str8_match(info.name, str8_lit(".git"), 0))
    {
      continue;
    }
    InfoNode *n = push_array(scratch.arena, InfoNode, 1);
    n->v = info;
    SLLQueuePush(first_info, last_info, n);
    infos_count += 1;
  }
  file_iter_end(it);

  //- rad: sort: folders first, then by name
  FileInfo *infos = push_array(scratch.arena, FileInfo, infos_count);
  {
    U64 idx = 0;
    for(InfoNode *n = first_info; n != 0; n = n->next, idx += 1)
    {
      infos[idx] = n->v;
    }
  }
  quick_sort(infos, infos_count, sizeof(infos[0]), rad_explorer_file_info_compare);

  //- rad: push rows; recurse into expanded folders
  B32 folder_has_trailing_slash = (folder.size != 0 && (folder.str[folder.size-1] == '/' || folder.str[folder.size-1] == '\\'));
  for EachIndex(idx, infos_count)
  {
    if(rows->count >= RAD_EXPLORER_MAX_ROWS)
    {
      break;
    }
    FileInfo *info = &infos[idx];
    RAD_ExplorerRowNode *n = push_array(arena, RAD_ExplorerRowNode, 1);
    n->v.path = push_str8f(arena, "%S%s%S", folder, folder_has_trailing_slash ? "" : "/", info->name);
    n->v.name = str8_skip_last_slash(n->v.path);
    n->v.depth = depth;
    n->v.is_folder = !!(info->props.flags & FilePropertyFlag_IsFolder);
    n->v.is_expanded = (n->v.is_folder && rad_explorer_is_expanded(ex, n->v.path));
    SLLQueuePush(rows->first, rows->last, n);
    rows->count += 1;
    if(n->v.is_expanded)
    {
      rad_explorer_rows_push_folder__(arena, ex, rows, n->v.path, depth+1);
    }
  }

  scratch_end(scratch);
}

internal void
rad_explorer_rows_build(RAD_ExplorerState *ex, String8 root)
{
  arena_clear(ex->rows_arena);
  ex->rows_root = push_str8_copy(ex->rows_arena, root);
  RAD_ExplorerRowList rows = {0};
  rad_explorer_rows_push_folder__(ex->rows_arena, ex, &rows, ex->rows_root, 0);
  ex->rows = push_array(ex->rows_arena, RAD_ExplorerRow, rows.count);
  ex->rows_count = rows.count;
  {
    U64 idx = 0;
    for(RAD_ExplorerRowNode *n = rows.first; n != 0; n = n->next, idx += 1)
    {
      ex->rows[idx] = n->v;
    }
  }
  ex->rows_build_us = now_time_us();
  ex->rows_dirty = 0;

  //- rad: keep the selection on the same path
  String8 selected_path = str8(ex->selected_path_buffer, ex->selected_path_size);
  if(selected_path.size != 0)
  {
    for EachIndex(idx, ex->rows_count)
    {
      if(str8_match(ex->rows[idx].path, selected_path, 0))
      {
        ex->cursor = ex->mark = v2s64(0, (S64)idx+1);
        break;
      }
    }
  }
  ex->cursor.y = Clamp(0, ex->cursor.y, (S64)ex->rows_count);
  ex->mark.y = Clamp(0, ex->mark.y, (S64)ex->rows_count);
}

internal CFG_Node *
rad_explorer_target_panel_from_tab(CFG_Node *tab)
{
  // NOTE(rad): files open in the last focused panel that does not show an
  // explorer; otherwise in the first other leaf panel; otherwise here.
  Temp scratch = scratch_begin(0, 0);
  CFG_Node *explorer_panel = tab->parent;
  CFG_Node *result = cfg_node_from_id(rad_state->last_editor_panel);
  CFG_PanelTree panel_tree = cfg_panel_tree_from_cfg(scratch.arena, explorer_panel);
  B32 result_is_in_tree = 0;
  CFG_Node *first_other_leaf = &cfg_nil_node;
  for(CFG_PanelNode *p = panel_tree.root;
      p != &cfg_nil_panel_node;
      p = cfg_panel_node_rec__depth_first_pre(panel_tree.root, p).next)
  {
    if(p->first == &cfg_nil_panel_node && p->cfg != explorer_panel)
    {
      if(first_other_leaf == &cfg_nil_node)
      {
        first_other_leaf = p->cfg;
      }
      if(p->cfg == result)
      {
        result_is_in_tree = 1;
      }
    }
  }
  if(!result_is_in_tree)
  {
    result = first_other_leaf;
  }
  if(result == &cfg_nil_node)
  {
    result = explorer_panel;
  }
  scratch_end(scratch);
  return result;
}

internal void
rad_explorer_view_ui(CFG_Node *tab, Rng2F32 rect)
{
  Temp scratch = scratch_begin(0, 0);
  RAD_ViewState *vs = rad_view_state_from_cfg(tab);
  RAD_ExplorerState *ex = &vs->ex;
  if(!ex->initialized)
  {
    ex->initialized = 1;
    ex->expansions_arena = arena_alloc();
    ex->rows_arena = arena_alloc();
    ex->rows_dirty = 1;
  }
  String8 root = rad_explorer_folder_from_tab(tab);

  //////////////////////////////
  //- rad: no folder -> empty state (same layout as the text view's missing
  // file, plus the getting-started view's "Open File" button style)
  //
  if(root.size == 0)
  {
    ui_set_next_flags(UI_BoxFlag_DefaultFocusNav);
    UI_Focus(UI_FocusKind_On) UI_WidthFill UI_HeightFill UI_NamedColumn(str8_lit("empty_explorer"))
      UI_Padding(ui_pct(1, 0)) UI_Focus(UI_FocusKind_Null)
    {
      UI_PrefWidth(ui_children_sum(1)) UI_PrefHeight(ui_em(3, 1))
        UI_Row UI_Padding(ui_pct(1, 0))
        UI_PrefWidth(ui_text_dim(1, 1))
        UI_TagF("weak")
      {
        ui_labelf("No folder open.");
      }
      ui_spacer(ui_em(1.f, 1.f));
      UI_PrefHeight(ui_em(3.75f, 1.f))
        UI_Row
        UI_Padding(ui_pct(1, 0))
        UI_TextAlignment(UI_TextAlign_Center)
        UI_PrefWidth(ui_em(12.f, 1.f))
        UI_CornerRadius(ui_top_font_size()/2.f)
        UI_TagF("pop")
        if(ui_clicked(rad_icon_buttonf(RAD_IconKind_FolderOpenOutline, 0, "Open Folder")))
      {
        rad_cmd(RAD_CmdKind_OpenFolder, .tab = tab->id);
      }
    }
  }

  //////////////////////////////
  //- rad: rebuild rows when stale: expansions changed, a new root, or the
  // root's folder watch saw a change (no watch -> once a second)
  //
  if(root.size != 0)
  {
    RAD_FolderWatchChange change = rad_folder_watch_take_change(root);
    if(ex->rows_dirty || !str8_match(ex->rows_root, root, 0) ||
       change == RAD_FolderWatchChange_Changed ||
       (change == RAD_FolderWatchChange_NoWatch && now_time_us() >= ex->rows_build_us + RAD_EXPLORER_REFRESH_US))
    {
      rad_explorer_rows_build(ex, root);
    }
  }

  //////////////////////////////
  //- rad: unpack
  //
  F32 row_height_px = ui_top_px_height();
  Vec2F32 rect_dim = dim_2f32(rect);
  F32 contents_width_px = (rect_dim.x - floor_f32(ui_bottom_font_size()*1.5f));
  S64 rows_count = (root.size != 0 ? (S64)ex->rows_count : 0);
  String8 toggle_path = {0};
  String8 open_path = {0};
  ex->layout_rect = rect;
  ex->layout_row_height = row_height_px;
  ex->layout_font_size = ui_top_font_size();

  //////////////////////////////
  //- rad: keyboard: Left/Right collapse/expand (or move to parent/child),
  // Enter opens a file or toggles a folder
  //
  UI_Focus(UI_FocusKind_On) if(ui_is_focus_active() && rows_count != 0)
  {
    for(UI_Event *evt = 0; ui_next_event(&evt);)
    {
      S64 row_idx = ex->cursor.y - 1;
      if(row_idx < 0 || row_idx >= rows_count)
      {
        continue;
      }
      RAD_ExplorerRow *row = &ex->rows[row_idx];
      if(evt->kind == UI_EventKind_Navigate && evt->delta_unit == UI_EventDeltaUnit_Char &&
         evt->delta_2s32.y == 0 && evt->delta_2s32.x != 0)
      {
        if(evt->delta_2s32.x > 0)
        {
          if(row->is_folder && !row->is_expanded)
          {
            toggle_path = push_str8_copy(scratch.arena, row->path);
          }
          else if(row->is_folder && row_idx+1 < rows_count && ex->rows[row_idx+1].depth > row->depth)
          {
            ex->cursor = ex->mark = v2s64(0, row_idx+2);
          }
        }
        else
        {
          if(row->is_folder && row->is_expanded)
          {
            toggle_path = push_str8_copy(scratch.arena, row->path);
          }
          else for(S64 idx = row_idx-1; idx >= 0; idx -= 1)
          {
            if(ex->rows[idx].depth < row->depth)
            {
              ex->cursor = ex->mark = v2s64(0, idx+1);
              break;
            }
          }
        }
        ui_eat_event(evt);
      }
      else if(evt->kind == UI_EventKind_Press && evt->key == WM_Key_Return && evt->modifiers == 0)
      {
        if(row->is_folder)
        {
          toggle_path = push_str8_copy(scratch.arena, row->path);
        }
        else
        {
          open_path = push_str8_copy(scratch.arena, row->path);
        }
        ui_eat_event(evt);
      }
    }
  }

  //////////////////////////////
  //- rad: build table (port of raddbg's watch view table: scroll list,
  // row boxes, one cell per row)
  //
  if(root.size != 0)
  {
    UI_ScrollListParams scroll_list_params = {0};
    {
      scroll_list_params.flags         = UI_ScrollListFlag_All;
      scroll_list_params.row_height_px = row_height_px;
      scroll_list_params.dim_px        = rect_dim;
      scroll_list_params.cursor_range  = r2s64(v2s64(0, 0), v2s64(0, rows_count));
      scroll_list_params.item_range    = r1s64(0, rows_count);
      scroll_list_params.cursor_min_is_empty_selection[Axis2_Y] = 1;
    }
    Rng1S64 visible_row_rng = {0};
    UI_ScrollListSignal scroll_list_sig = {0};
    UI_Focus(UI_FocusKind_On)
      UI_ScrollList(&scroll_list_params, &ex->scroll_pos.y, &ex->cursor, &ex->mark, &visible_row_rng, &scroll_list_sig)
      UI_Focus(UI_FocusKind_Null)
    {
      ui_set_next_pref_height(ui_children_sum(1));
      ui_set_next_child_layout_axis(Axis2_Y);
      UI_Box *table = ui_build_box_from_string(0, str8_lit("table"));
      UI_Parent(table)
        for(S64 row_idx = visible_row_rng.min; row_idx <= visible_row_rng.max && row_idx < rows_count; row_idx += 1)
      {
        RAD_ExplorerRow *row = &ex->rows[row_idx];
        U64 row_hash = u64_hash_from_str8(row->path);
        B32 row_selected = (ex->cursor.y == row_idx+1);

        //- rad: row title (same layout as raddbg's rd_title_fstrs_from_file_path)
        DR_FStrList fstrs = {0};
        {
          RAD_IconKind icon_kind = (!row->is_folder ? RAD_IconKind_FileOutline :
                                    row->is_expanded ? RAD_IconKind_FolderOpenFilled :
                                    RAD_IconKind_FolderClosedFilled);
          DR_FStrParams params = {rad_font_from_slot(RAD_FontSlot_Main), rad_raster_flags_from_slot(RAD_FontSlot_Main), ui_color_from_name(str8_lit("text")), ui_top_font_size()};
          UI_TagF("weak")
          {
            dr_fstrs_push_new(scratch.arena, &fstrs, &params,
                              rad_icon_kind_text_table[icon_kind],
                              .font = rad_font_from_slot(RAD_FontSlot_Icons),
                              .raster_flags = rad_raster_flags_from_slot(RAD_FontSlot_Icons),
                              .color = ui_color_from_name(str8_lit("text")));
          }
          dr_fstrs_push_new(scratch.arena, &fstrs, &params, str8_lit("  "));
          dr_fstrs_push_new(scratch.arena, &fstrs, &params, row->name);
        }

        //- rad: build row box
        UI_BoxFlags row_flags = UI_BoxFlag_DisableFocusOverlay;
        if(row_idx & 1)
        {
          ui_set_next_tag(str8_lit("alt"));
          row_flags |= UI_BoxFlag_DrawBackground;
        }
        ui_set_next_pref_width(ui_px(contents_width_px, 1.f));
        ui_set_next_pref_height(ui_px(row_height_px, 1.f));
        ui_set_next_focus_hot(row_selected ? UI_FocusKind_On : UI_FocusKind_Off);
        UI_Box *row_box = ui_build_box_from_stringf(row_flags|((row_idx+1 == rows_count)*UI_BoxFlag_DrawSideBottom)|UI_BoxFlag_Clickable, "row_%I64x", row_hash);

        //- rad: record layout (for scripted input)
        if(row_idx == visible_row_rng.min)
        {
          ex->layout_first_row      = row_idx;
          ex->layout_first_row_rect = row_box->rect;
        }

        //- rad: build cell
        UI_Signal sig = {0};
        B32 next_expanded = row->is_expanded;
        UI_Parent(row_box)
        {
          UI_Box *cell_box = &ui_nil_box;
          UI_PrefWidth(ui_px(contents_width_px, 1.f))
          {
            ui_set_next_fixed_height(floor_f32(row_height_px));
            cell_box = ui_build_box_from_stringf(UI_BoxFlag_DrawSideLeft, "cell_%I64x", row_hash);
          }
          RAD_CellParams cell_params = {0};
          cell_params.flags        = (RAD_CellFlag_KeyboardClickable|RAD_CellFlag_NoBackground|
                                      (row->is_folder ? RAD_CellFlag_Expander : RAD_CellFlag_ExpanderSpace));
          cell_params.depth        = row->depth;
          cell_params.value_fstrs  = fstrs;
          cell_params.expanded_out = &next_expanded;
          UI_Parent(cell_box)
            UI_FocusHot(row_selected ? UI_FocusKind_On : UI_FocusKind_Off)
            UI_FocusActive(UI_FocusKind_Off)
            RAD_Font(RAD_FontSlot_Code)
            UI_TagF("weak")
          {
            sig = rad_cellf(&cell_params, "###cell_%I64x", row_hash);
          }
        }
        UI_Signal row_sig = ui_signal_from_box(row_box);

        //- rad: interactions: press selects; double-click opens a file or
        // toggles a folder; the expander toggles
        if(ui_pressed(sig) || ui_pressed(row_sig))
        {
          ex->cursor = ex->mark = v2s64(0, row_idx+1);
          rad_cmd(RAD_CmdKind_FocusPanel, .panel = tab->parent->id);
        }
        if(ui_double_clicked(sig) || ui_double_clicked(row_sig))
        {
          if(row->is_folder)
          {
            toggle_path = push_str8_copy(scratch.arena, row->path);
          }
          else
          {
            open_path = push_str8_copy(scratch.arena, row->path);
          }
        }
        if(next_expanded != row->is_expanded)
        {
          toggle_path = push_str8_copy(scratch.arena, row->path);
        }
      }
    }
  }

  //////////////////////////////
  //- rad: remember the selected path (selection survives row rebuilds)
  //
  if(1 <= ex->cursor.y && ex->cursor.y <= rows_count)
  {
    String8 path = ex->rows[ex->cursor.y-1].path;
    ex->selected_path_size = Min(path.size, sizeof(ex->selected_path_buffer));
    MemoryCopy(ex->selected_path_buffer, path.str, ex->selected_path_size);
  }

  //////////////////////////////
  //- rad: apply deferred actions (rows rebuild next frame)
  //
  if(toggle_path.size != 0)
  {
    rad_explorer_set_expanded(ex, toggle_path, !rad_explorer_is_expanded(ex, toggle_path));
    rad_request_frame();
  }
  if(open_path.size != 0)
  {
    rad_cmd(RAD_CmdKind_OpenFile, .panel = rad_explorer_target_panel_from_tab(tab)->id, .file_path = open_path);
  }

  scratch_end(scratch);
}

////////////////////////////////
//~ rad: Output + Problems

internal void
rad_output_view_ui(CFG_Node *tab, Rng2F32 rect)
{
  // NOTE(rad): the cargo output in a read-only code view (raddbg's Output tab
  // is a text view on its output log, too). A cursor at the end of the text
  // follows new output, like a terminal; clicking elsewhere stops that,
  // Ctrl+End follows again. A double-click on a line with a
  // `<path>:<line>:<column>`, or on the first line of a compiler message,
  // goes there.
  RAD_ViewState *vs = rad_view_state_from_cfg(tab);
  RAD_CodeViewState *cv = &vs->cv;
  Temp scratch = scratch_begin(0, 0);
  Access *access = access_open();
  TB_Buffer *buffer = rad_cargo->output_buffer;
  rad_code_view_init(cv);
  cv->editable = 0;

  //////////////////////////////
  //- rad: text
  //
  TB_Text text = tb_text_from_buffer(scratch.arena, access, buffer);
  TXT_TextInfo info = text.base_info;
  String8 data = text.base_data;
  B32 has_text = (info.lines_count != 0 && text.patched.size != 0);

  //////////////////////////////
  //- rad: new text -> a cursor at the end of the old text moves to the new end
  //
  if(cv->buffer_version != buffer->version)
  {
    if(cv->cursor == cv->mark && cv->cursor >= cv->last_text_size)
    {
      cv->cursor = cv->mark = text.patched.size;
      cv->contain_cursor = 1;
    }
    cv->cursor = Min(cv->cursor, text.patched.size);
    cv->mark = Min(cv->mark, text.patched.size);
    cv->buffer_version = buffer->version;
  }

  //////////////////////////////
  //- rad: build (empty: same layout as the explorer's "No folder open.")
  //
  if(!has_text)
  {
    UI_WidthFill UI_HeightFill UI_Column UI_Padding(ui_pct(1, 0))
    {
      UI_PrefWidth(ui_children_sum(1)) UI_PrefHeight(ui_em(3, 1))
        UI_Row UI_Padding(ui_pct(1, 0))
        UI_PrefWidth(ui_text_dim(1, 1))
        UI_TagF("weak")
      {
        ui_labelf("No output yet.");
      }
    }
  }
  else
  {
    rad_code_view_build(scratch.arena, tab, cv, buffer, rect, TXT_LangKind_Null, &info, data);
    cv->buffer_version = buffer->version;
  }
  cv->last_text_size = text.patched.size;

  //////////////////////////////
  //- rad: double-click -> the location on that line (from a message's first
  // line: on the lines below it, up to a blank line)
  //
  if(cv->double_clicked)
  {
    cv->double_clicked = 0;
    if(has_text)
    {
      String8 root = rad_cargo_workspace_root(scratch.arena);
      U64 line_count = text.patched.line_map.total_line_count;
      U64 line_num = txt_line_num_from_off(&text.patched.line_map, cv->double_clicked_off);
      for(U64 scan = line_num; 1 <= scan && scan <= line_count && scan < line_num+4; scan += 1)
      {
        Rng1U64 range = txt_range_from_line_num(&text.patched.line_map, scan);
        String8 line = memory_map_data_from_range(scratch.arena, &text.patched.memory_map, range);
        String8 path = {0};
        S64 loc_line = 0;
        S64 loc_column = 0;
        if(rad_location_from_text_line(scratch.arena, line, root, &path, &loc_line, &loc_column))
        {
          rad_cmd(RAD_CmdKind_GoToLocation, .file_path = path, .line_num = loc_line, .column_num = loc_column);
          break;
        }
        if(str8_skip_chop_whitespace(line).size == 0)
        {
          break;
        }
      }
    }
  }

  access_close(access);
  scratch_end(scratch);
}

internal void
rad_goto_diagnostic(CFG_Node *window, U64 diag_num)
{
  // NOTE(rad): selects the diagnostic (Problems view, Next/Previous Error) and
  // goes to it. In an open document it is where it moved with the edits made
  // since the build; otherwise at the reported line and column.
  RAD_Problems *problems = rad_problems;
  if(1 <= diag_num && diag_num <= problems->count)
  {
    Temp scratch = scratch_begin(0, 0);
    RAD_Diagnostic *diag = problems->diags[diag_num-1];
    S64 line = diag->line;
    S64 column = diag->column;
    RAD_Document *doc = rad_document_lookup(diag->path);
    if(doc != 0 && tb_buffer_is_loaded(doc->buffer))
    {
      Access *access = access_open();
      TB_Text text = tb_text_from_buffer(scratch.arena, access, doc->buffer);
      if(text.base_info.lines_count != 0)
      {
        Rng1U64 range = rad_diagnostic_range_from_document(diag, doc, &text.patched);
        rad_line_column_from_offset(&text.patched, range.min, &line, &column);
      }
      access_close(access);
    }
    rad_problems_select(diag_num);
    rad_cmd(RAD_CmdKind_GoToLocation, .window = window->id, .file_path = diag->path, .line_num = line, .column_num = column);
    scratch_end(scratch);
  }
}

internal void
rad_problems_view_ui(CFG_Node *tab, Rng2F32 rect)
{
  // NOTE(rad): rows built like the explorer's (raddbg's watch-table rows);
  // the selection is the shared Next/Previous Error position
  Temp scratch = scratch_begin(0, 0);
  RAD_ViewState *vs = rad_view_state_from_cfg(tab);
  RAD_ProblemsState *pr = &vs->pr;
  RAD_Problems *problems = rad_problems;
  S64 rows_count = (S64)problems->count;
  U64 goto_num = 0;

  //////////////////////////////
  //- rad: unpack
  //
  F32 row_height_px = ui_top_px_height();
  Vec2F32 rect_dim = dim_2f32(rect);
  F32 contents_width_px = (rect_dim.x - floor_f32(ui_bottom_font_size()*1.5f));
  pr->layout_rect = rect;
  pr->layout_row_height = row_height_px;

  //////////////////////////////
  //- rad: selection = the shared position; moved from outside (F8, a new
  // run) -> scroll it into view
  //
  {
    // NOTE(rad): the list is rebuilt whenever a server publishes (while
    // typing): only a change of the selected row counts
    S64 shared = (S64)Min(problems->cursor, (U64)rows_count);
    B32 moved_outside = (pr->cursor.y != shared);
    pr->diags_gen_seen = problems->gen;
    pr->cursor = pr->mark = v2s64(0, shared);
    if(moved_outside && shared != 0)
    {
      S64 visible_rows = Max(1, (S64)(rect_dim.y / row_height_px));
      S64 row_idx = shared-1;
      if(row_idx < pr->scroll_pos.y.idx || row_idx >= pr->scroll_pos.y.idx + visible_rows - 1)
      {
        S64 new_idx = Clamp(0, row_idx - visible_rows/2, Max(0, rows_count-1));
        ui_scroll_pt_target_idx(&pr->scroll_pos.y, new_idx);
      }
    }
  }

  //////////////////////////////
  //- rad: empty -> say so
  //
  if(rows_count == 0)
  {
    UI_WidthFill UI_HeightFill UI_Column UI_Padding(ui_pct(1, 0))
    {
      UI_PrefWidth(ui_children_sum(1)) UI_PrefHeight(ui_em(3, 1))
        UI_Row UI_Padding(ui_pct(1, 0))
        UI_PrefWidth(ui_text_dim(1, 1))
        UI_TagF("weak")
      {
        ui_labelf("%s", rad_cargo_is_running() ? "No problems so far." : "No problems.");
      }
    }
  }

  //////////////////////////////
  //- rad: keyboard: Enter goes to the selected problem
  //
  UI_Focus(UI_FocusKind_On) if(ui_is_focus_active() && rows_count != 0)
  {
    for(UI_Event *evt = 0; ui_next_event(&evt);)
    {
      if(evt->kind == UI_EventKind_Press && evt->key == WM_Key_Return && evt->modifiers == 0 && pr->cursor.y >= 1)
      {
        goto_num = (U64)pr->cursor.y;
        ui_eat_event(evt);
      }
    }
  }

  //////////////////////////////
  //- rad: table (same scroll list, rows and cells as the explorer)
  //
  if(rows_count != 0)
  {
    UI_ScrollListParams scroll_list_params = {0};
    {
      scroll_list_params.flags         = UI_ScrollListFlag_All;
      scroll_list_params.row_height_px = row_height_px;
      scroll_list_params.dim_px        = rect_dim;
      scroll_list_params.cursor_range  = r2s64(v2s64(0, 0), v2s64(0, rows_count));
      scroll_list_params.item_range    = r1s64(0, rows_count);
      scroll_list_params.cursor_min_is_empty_selection[Axis2_Y] = 1;
    }
    Rng1S64 visible_row_rng = {0};
    UI_ScrollListSignal scroll_list_sig = {0};
    UI_Focus(UI_FocusKind_On)
      UI_ScrollList(&scroll_list_params, &pr->scroll_pos.y, &pr->cursor, &pr->mark, &visible_row_rng, &scroll_list_sig)
      UI_Focus(UI_FocusKind_Null)
    {
      ui_set_next_pref_height(ui_children_sum(1));
      ui_set_next_child_layout_axis(Axis2_Y);
      UI_Box *table = ui_build_box_from_string(0, str8_lit("table"));
      UI_Parent(table)
        for(S64 row_idx = visible_row_rng.min; row_idx <= visible_row_rng.max && row_idx < rows_count; row_idx += 1)
      {
        RAD_Diagnostic *diag = problems->diags[row_idx];
        B32 row_selected = (pr->cursor.y == row_idx+1);

        //- rad: row text: [icon]  message  code  path:line:column
        DR_FStrList fstrs = {0};
        {
          RAD_IconKind icon_kind = (diag->kind == RAD_DiagnosticKind_Error   ? RAD_IconKind_WarningBig :
                                    diag->kind == RAD_DiagnosticKind_Warning ? RAD_IconKind_WarningSmall :
                                    RAD_IconKind_Info);
          Vec4F32 text_color = ui_color_from_name(str8_lit("text"));
          Vec4F32 weak_color = text_color;
          Vec4F32 icon_color = text_color;
          UI_TagF("weak")
          {
            weak_color = ui_color_from_name(str8_lit("text"));
          }
          if(diag->kind == RAD_DiagnosticKind_Error)
          {
            UI_TagF("bad") icon_color = ui_color_from_name(str8_lit("text"));
          }
          else if(diag->kind != RAD_DiagnosticKind_Warning)
          {
            icon_color = weak_color;
          }
          String8 path = rad_problem_display_path(scratch.arena, diag);
          DR_FStrParams params = {rad_font_from_slot(RAD_FontSlot_Main), rad_raster_flags_from_slot(RAD_FontSlot_Main), text_color, ui_top_font_size()};
          dr_fstrs_push_new(scratch.arena, &fstrs, &params, rad_icon_kind_text_table[icon_kind],
                            .font = rad_font_from_slot(RAD_FontSlot_Icons),
                            .raster_flags = rad_raster_flags_from_slot(RAD_FontSlot_Icons),
                            .color = icon_color);
          dr_fstrs_push_new(scratch.arena, &fstrs, &params, str8_lit("  "));
          dr_fstrs_push_new(scratch.arena, &fstrs, &params, diag->message);
          if(diag->code.size != 0)
          {
            dr_fstrs_push_new(scratch.arena, &fstrs, &params, push_str8f(scratch.arena, "  %S", diag->code), .color = weak_color);
          }
          dr_fstrs_push_new(scratch.arena, &fstrs, &params, push_str8f(scratch.arena, "  %S:%I64d:%I64d", path, diag->line, diag->column), .color = weak_color);
        }

        //- rad: build row box
        UI_BoxFlags row_flags = UI_BoxFlag_DisableFocusOverlay;
        if(row_idx & 1)
        {
          ui_set_next_tag(str8_lit("alt"));
          row_flags |= UI_BoxFlag_DrawBackground;
        }
        ui_set_next_pref_width(ui_px(contents_width_px, 1.f));
        ui_set_next_pref_height(ui_px(row_height_px, 1.f));
        ui_set_next_focus_hot(row_selected ? UI_FocusKind_On : UI_FocusKind_Off);
        UI_Box *row_box = ui_build_box_from_stringf(row_flags|((row_idx+1 == rows_count)*UI_BoxFlag_DrawSideBottom)|UI_BoxFlag_Clickable, "row_%I64x", row_idx);

        //- rad: record layout (for scripted input)
        if(row_idx == visible_row_rng.min)
        {
          pr->layout_first_row      = row_idx;
          pr->layout_first_row_rect = row_box->rect;
        }

        //- rad: build cell
        UI_Signal sig = {0};
        UI_Parent(row_box)
        {
          UI_Box *cell_box = &ui_nil_box;
          UI_PrefWidth(ui_px(contents_width_px, 1.f))
          {
            ui_set_next_fixed_height(floor_f32(row_height_px));
            cell_box = ui_build_box_from_stringf(UI_BoxFlag_DrawSideLeft, "cell_%I64x", row_idx);
          }
          RAD_CellParams cell_params = {0};
          cell_params.flags       = (RAD_CellFlag_KeyboardClickable|RAD_CellFlag_NoBackground);
          cell_params.value_fstrs = fstrs;
          UI_Parent(cell_box)
            UI_FocusHot(row_selected ? UI_FocusKind_On : UI_FocusKind_Off)
            UI_FocusActive(UI_FocusKind_Off)
            RAD_Font(RAD_FontSlot_Code)
            UI_TagF("weak")
          {
            sig = rad_cellf(&cell_params, "###cell_%I64x", row_idx);
          }
        }
        UI_Signal row_sig = ui_signal_from_box(row_box);

        //- rad: interactions: press selects; double-click goes there
        if(ui_pressed(sig) || ui_pressed(row_sig))
        {
          pr->cursor = pr->mark = v2s64(0, row_idx+1);
          rad_cmd(RAD_CmdKind_FocusPanel, .panel = tab->parent->id);
        }
        if(ui_double_clicked(sig) || ui_double_clicked(row_sig))
        {
          goto_num = (U64)row_idx+1;
        }
      }
    }
  }

  //////////////////////////////
  //- rad: selection -> the shared position; deferred go-to
  //
  if(problems->cursor != (U64)Clamp(0, pr->cursor.y, rows_count))
  {
    rad_problems_select((U64)Clamp(0, pr->cursor.y, rows_count));
  }
  if(goto_num != 0)
  {
    rad_goto_diagnostic(cfg_window_from_cfg(tab), goto_num);
    pr->cursor = pr->mark = v2s64(0, (S64)goto_num);
  }

  scratch_end(scratch);
}

////////////////////////////////
//~ rad: Settings View

internal void
rad_settings_view_ui(CFG_Node *tab, Rng2F32 rect)
{
  // NOTE(rad): raddbg's User Settings lister (a query view with big rows:
  // `do_big_rows`, 5 font sizes): name + description on the left; toggle
  // switch, value + slider, revert button on the right (rd_cell). rad shows
  // it as a tab, with a filter row on top in the query bar's look. Enums
  // and the theme open a menu of their options (raddbg: an expandable row).
  Temp scratch = scratch_begin(0, 0);
  RAD_ViewState *vs = rad_view_state_from_cfg(tab);
  RAD_SettingsState *ss = &vs->ss;
  RAD_Settings *settings = &rad_state->settings;
  UI_Key menu_key = ui_key_from_stringf(ui_key_zero(), "_settings_options_menu_%p", tab);

  //////////////////////////////
  //- rad: unpack
  //
  F32 filter_row_height_px = ui_top_px_height();
  F32 row_height_px = floor_f32(ui_top_font_size()*(settings->density == 1 ? 4.f : 5.f));
  Vec2F32 rect_dim = dim_2f32(rect);
  F32 list_height_px = Max(0, rect_dim.y - filter_row_height_px);
  F32 contents_width_px = (rect_dim.x - floor_f32(ui_bottom_font_size()*1.5f));
  ss->layout_frame = rad_state->frame_index;
  MemoryZeroArray(ss->layout_row_rects);
  MemoryZeroArray(ss->layout_control_rects);
  MemoryZeroArray(ss->layout_revert_rects);

  //////////////////////////////
  //- rad: rows: the settings whose name, description or group match the filter
  //
  String8 needle = str8(ss->filter_buffer, ss->filter_size);
  S64 rows_count = 0;
  for EachElement(idx, rad_setting_info_table)
  {
    RAD_SettingInfo *info = &rad_setting_info_table[idx];
    B32 matches = (needle.size == 0);
    String8 haystacks[] = {info->display_name, info->description, info->group};
    for EachElement(h_idx, haystacks)
    {
      FuzzyMatchRangeList m = fuzzy_match_find(scratch.arena, needle, haystacks[h_idx]);
      if(!matches && m.count != 0 && m.count == m.needle_part_count)
      {
        matches = 1;
      }
    }
    if(matches)
    {
      ss->layout_row_settings[rows_count] = idx;
      rows_count += 1;
    }
  }
  ss->layout_rows_count = (U64)rows_count;
  ss->cursor.y = ss->mark.y = Clamp(0, ss->cursor.y, rows_count);
  U64 selected_setting = (ss->cursor.y >= 1 ? ss->layout_row_settings[ss->cursor.y-1] : max_U64);

  //////////////////////////////
  //- rad: a setting's value -> slider position, and back
  //
  #define rad_setting_pct_from_value(info, v) ((F32)(((v) - (info)->min) / Max(1e-9, (info)->max - (info)->min)))

  //////////////////////////////
  //- rad: keyboard (list has it): Up/Down select (scroll list), Enter/Space
  // toggle or open the options, Left/Right step a value or an option; typing
  // goes to the filter. Filter has it: Down/Enter go to the list, Esc clears.
  //
  B32 open_menu_for_selected = 0;
  UI_Focus(UI_FocusKind_On) if(ui_is_focus_active())
  {
    for(UI_Event *evt = 0; ui_next_event(&evt);)
    {
      if(ss->filter_focused)
      {
        if(evt->kind == UI_EventKind_Press && evt->modifiers == 0 && (evt->key == WM_Key_Down || evt->key == WM_Key_Return))
        {
          ss->filter_focused = 0;
          if(ss->cursor.y == 0 && rows_count != 0)
          {
            ss->cursor = ss->mark = v2s64(0, 1);
          }
          ui_eat_event(evt);
        }
        else if(evt->kind == UI_EventKind_Press && evt->modifiers == 0 && evt->key == WM_Key_Esc)
        {
          if(ss->filter_size != 0)
          {
            ss->filter_size = ss->filter_cursor = ss->filter_mark = 0;
          }
          else
          {
            ss->filter_focused = 0;
          }
          ui_eat_event(evt);
        }
        continue;
      }
      if(evt->kind == UI_EventKind_Text && evt->string.size != 0 && !str8_match(evt->string, str8_lit(" "), 0) &&
         str8_find_needle(evt->string, 0, str8_lit("\n"), 0) == evt->string.size)
      {
        // rad: typing -> the filter (the filter cell takes this event below)
        ss->filter_focused = 1;
        continue;
      }
      if(selected_setting == max_U64 || evt->modifiers != 0)
      {
        continue;
      }
      RAD_SettingInfo *info = &rad_setting_info_table[selected_setting];
      F64 v = rad_setting_f64_from_settings(settings, selected_setting);
      B32 activate = ((evt->kind == UI_EventKind_Press && evt->key == WM_Key_Return) ||
                      (evt->kind == UI_EventKind_Text && str8_match(evt->string, str8_lit(" "), 0)));
      S32 step = 0;
      if(evt->kind == UI_EventKind_Navigate && !(evt->flags & UI_EventFlag_KeepMark) && evt->delta_unit == UI_EventDeltaUnit_Char && evt->delta_2s32.y == 0)
      {
        step = (evt->delta_2s32.x < 0 ? -1 : evt->delta_2s32.x > 0 ? +1 : 0);
      }
      if(activate)
      {
        if(info->kind == RAD_SettingKind_Bool)
        {
          rad_setting_set_f64(info->name, v != 0 ? 0 : 1);
        }
        else if(info->kind == RAD_SettingKind_Enum || info->kind == RAD_SettingKind_Theme)
        {
          open_menu_for_selected = 1;
        }
        ui_eat_event(evt);
      }
      else if(evt->kind == UI_EventKind_Text && str8_match(evt->string, str8_lit("\n"), 0))
      {
        ui_eat_event(evt);
      }
      else if(step != 0)
      {
        F64 step_size = (info->kind == RAD_SettingKind_F32 ? 0.05 : 1.0);
        F64 max = (info->kind == RAD_SettingKind_Theme ? (F64)(RAD_ThemePreset_COUNT-1) : info->max);
        if(info->kind == RAD_SettingKind_Bool)
        {
          rad_setting_set_f64(info->name, step > 0 ? 1 : 0);
        }
        else
        {
          rad_setting_set_f64(info->name, Clamp(info->min, v + step*step_size, max));
        }
        ui_eat_event(evt);
      }
    }
  }

  //////////////////////////////
  //- rad: column: filter row, then the list
  //
  UI_WidthFill UI_HeightFill UI_Column
  {
    //- rad: filter row (the query bar's look: rad_view_ui)
    UI_Box *filter_row = &ui_nil_box;
    UI_PrefHeight(ui_px(filter_row_height_px, 1.f))
    {
      if(ss->filter_focused)
      {
        ui_set_next_border_color(ui_color_from_name(str8_lit("border")));
      }
      filter_row = ui_build_box_from_stringf(UI_BoxFlag_DrawBorder|UI_BoxFlag_DrawDropShadow, "###settings_filter_row");
    }
    ss->layout_filter_rect = filter_row->rect;
    UI_Parent(filter_row) UI_WidthFill UI_HeightFill UI_Focus(ss->filter_focused ? UI_FocusKind_On : UI_FocusKind_Off)
      RAD_Font(RAD_FontSlot_Main)
      UI_Flags(UI_BoxFlag_DisableFocusOverlay|UI_BoxFlag_DisableFocusBorder)
    {
      UI_TagF("weak")
      {
        ui_spacer(ui_em(0.5f, 1.f));
        UI_TextAlignment(UI_TextAlign_Center) RAD_Font(RAD_FontSlot_Icons) UI_PrefWidth(ui_em(2.f, 1.f))
          ui_label(rad_icon_kind_text_table[RAD_IconKind_Find]);
      }
      RAD_CellParams params = {0};
      {
        params.flags |= RAD_CellFlag_NoBackground;
        params.cursor               = &ss->filter_cursor;
        params.mark                 = &ss->filter_mark;
        params.edit_buffer          = ss->filter_buffer;
        params.edit_string_size_out = &ss->filter_size;
        params.edit_buffer_size     = sizeof(ss->filter_buffer);
        params.pre_edit_value       = needle;
      }
      UI_FocusHot(ss->filter_focused ? UI_FocusKind_On : UI_FocusKind_Off)
        UI_FocusActive(ss->filter_focused ? UI_FocusKind_On : UI_FocusKind_Off)
      {
        UI_Signal sig = rad_cellf(&params, "Search Settings###settings_filter");
        if(ui_pressed(sig))
        {
          ss->filter_focused = 1;
          rad_cmd(RAD_CmdKind_FocusPanel, .panel = tab->parent->id);
        }
      }
    }

    //- rad: nothing matches -> say so
    if(rows_count == 0)
    {
      UI_WidthFill UI_HeightFill UI_Column UI_Padding(ui_pct(1, 0))
      {
        UI_PrefWidth(ui_children_sum(1)) UI_PrefHeight(ui_em(3, 1))
          UI_Row UI_Padding(ui_pct(1, 0))
          UI_PrefWidth(ui_text_dim(1, 1))
          UI_TagF("weak")
        {
          ui_labelf("No settings match \"%S\".", needle);
        }
      }
    }

    //- rad: the list (same scroll list and rows as the Problems view)
    if(rows_count != 0)
    {
      UI_ScrollListParams scroll_list_params = {0};
      {
        scroll_list_params.flags         = UI_ScrollListFlag_All;
        scroll_list_params.row_height_px = row_height_px;
        scroll_list_params.dim_px        = v2f32(rect_dim.x, list_height_px);
        scroll_list_params.cursor_range  = r2s64(v2s64(0, 0), v2s64(0, rows_count));
        scroll_list_params.item_range    = r1s64(0, rows_count);
        scroll_list_params.cursor_min_is_empty_selection[Axis2_Y] = 1;
      }
      Rng1S64 visible_row_rng = {0};
      UI_ScrollListSignal scroll_list_sig = {0};
      UI_Focus(ss->filter_focused ? UI_FocusKind_Off : UI_FocusKind_On)
        UI_ScrollList(&scroll_list_params, &ss->scroll_pos.y, &ss->cursor, &ss->mark, &visible_row_rng, &scroll_list_sig)
        UI_Focus(UI_FocusKind_Null)
        UI_PrefHeight(ui_px(row_height_px, 1.f))
      {
        ui_set_next_pref_height(ui_children_sum(1));
        ui_set_next_child_layout_axis(Axis2_Y);
        UI_Box *table = ui_build_box_from_string(0, str8_lit("table"));
        UI_Parent(table)
          for(S64 row_idx = visible_row_rng.min; row_idx <= visible_row_rng.max && row_idx < rows_count; row_idx += 1)
        {
          U64 setting_idx = ss->layout_row_settings[row_idx];
          RAD_SettingInfo *info = &rad_setting_info_table[setting_idx];
          B32 row_selected = (ss->cursor.y == row_idx+1);
          F64 value = rad_setting_f64_from_settings(settings, setting_idx);
          String8 value_string = rad_setting_value_string(scratch.arena, settings, setting_idx);
          CFG_Node *value_cfg = rad_setting_cfg_node(info->name);
          B32 is_set = (value_cfg != &cfg_nil_node && value != info->default_value);

          //- rad: name, value
          DR_FStrList meta_fstrs = {0};
          DR_FStrList value_fstrs = {0};
          {
            DR_FStrParams params = {rad_font_from_slot(RAD_FontSlot_Main), rad_raster_flags_from_slot(RAD_FontSlot_Main), ui_color_from_name(str8_lit("text")), ui_top_font_size()};
            dr_fstrs_push_new(scratch.arena, &meta_fstrs, &params, info->display_name);
            if(info->kind == RAD_SettingKind_U64 || info->kind == RAD_SettingKind_F32)
            {
              dr_fstrs_push_new(scratch.arena, &value_fstrs, &params, value_string,
                                .font = rad_font_from_slot(RAD_FontSlot_Code),
                                .raster_flags = rad_raster_flags_from_slot(RAD_FontSlot_Code),
                                .color = rad_rgba_from_code_color_slot(RAD_CodeColorSlot_CodeNumeric));
            }
            else if(info->kind == RAD_SettingKind_Enum || info->kind == RAD_SettingKind_Theme)
            {
              String8 display = value_string;
              if(info->kind == RAD_SettingKind_Theme)
              {
                display = rad_theme_preset_display_string_table[Min((U64)value, RAD_ThemePreset_COUNT-1)];
              }
              dr_fstrs_push_new(scratch.arena, &value_fstrs, &params, display);
              dr_fstrs_push_new(scratch.arena, &value_fstrs, &params, str8_lit("  "));
              dr_fstrs_push_new(scratch.arena, &value_fstrs, &params, rad_icon_kind_text_table[RAD_IconKind_DownCaret],
                                .font = rad_font_from_slot(RAD_FontSlot_Icons),
                                .raster_flags = rad_raster_flags_from_slot(RAD_FontSlot_Icons),
                                .size = ui_top_font_size()*0.8f);
            }
          }

          //- rad: build row box
          UI_BoxFlags row_flags = UI_BoxFlag_DisableFocusOverlay;
          if(row_idx & 1)
          {
            ui_set_next_tag(str8_lit("alt"));
            row_flags |= UI_BoxFlag_DrawBackground;
          }
          ui_set_next_pref_width(ui_px(contents_width_px, 1.f));
          ui_set_next_focus_hot(row_selected ? UI_FocusKind_On : UI_FocusKind_Off);
          UI_Box *row_box = ui_build_box_from_stringf(row_flags|((row_idx+1 == rows_count)*UI_BoxFlag_DrawSideBottom)|UI_BoxFlag_Clickable, "setting_row_%S", info->name);
          ss->layout_row_rects[setting_idx] = row_box->rect;

          //- rad: build cell
          B32 toggled = (value != 0);
          F32 slider_value = rad_setting_pct_from_value(info, value);
          B32 revert = 0;
          UI_Signal sig = {0};
          UI_Parent(row_box)
          {
            RAD_CellParams cell_params = {0};
            cell_params.flags            = (RAD_CellFlag_KeyboardClickable|RAD_CellFlag_NoBackground);
            cell_params.meta_fstrs       = meta_fstrs;
            cell_params.description      = info->description;
            cell_params.value_fstrs      = value_fstrs;
            cell_params.search_needle    = needle;
            cell_params.toggled_out      = &toggled;
            cell_params.slider_value_out = &slider_value;
            cell_params.revert_out       = &revert;
            cell_params.control_rect_out = &ss->layout_control_rects[setting_idx];
            cell_params.revert_rect_out  = &ss->layout_revert_rects[setting_idx];
            if(info->kind == RAD_SettingKind_Bool)
            {
              cell_params.flags |= RAD_CellFlag_ToggleSwitch;
            }
            if(info->kind == RAD_SettingKind_U64 || info->kind == RAD_SettingKind_F32)
            {
              cell_params.flags |= RAD_CellFlag_Slider;
            }
            if(is_set)
            {
              cell_params.flags |= RAD_CellFlag_RevertButton;
            }
            UI_WidthFill
              UI_FocusHot(row_selected ? UI_FocusKind_On : UI_FocusKind_Off)
              UI_FocusActive(UI_FocusKind_Off)
            {
              sig = rad_cellf(&cell_params, "###setting_%S", info->name);
            }
          }
          UI_Signal row_sig = ui_signal_from_box(row_box);

          //- rad: interactions
          if(ui_pressed(sig) || ui_pressed(row_sig))
          {
            ss->cursor = ss->mark = v2s64(0, row_idx+1);
            ss->filter_focused = 0;
            rad_cmd(RAD_CmdKind_FocusPanel, .panel = tab->parent->id);
          }
          if((ui_clicked(sig) || ui_clicked(row_sig)) &&
             (info->kind == RAD_SettingKind_Enum || info->kind == RAD_SettingKind_Theme))
          {
            ss->menu_setting = setting_idx;
            ui_ctx_menu_open(menu_key, row_box->key, v2f32(dim_2f32(row_box->rect).x - ui_top_font_size()*20.f, dim_2f32(row_box->rect).y));
          }
          if(ui_double_clicked(sig) && info->kind == RAD_SettingKind_Bool)
          {
            toggled = !toggled;
          }
          if(open_menu_for_selected && row_selected)
          {
            ss->menu_setting = setting_idx;
            ui_ctx_menu_open(menu_key, row_box->key, v2f32(dim_2f32(row_box->rect).x - ui_top_font_size()*20.f, dim_2f32(row_box->rect).y));
          }

          //- rad: changes -> the user file
          if(revert)
          {
            rad_setting_revert(info->name);
          }
          else if(info->kind == RAD_SettingKind_Bool && toggled != (value != 0))
          {
            rad_setting_set_f64(info->name, toggled ? 1 : 0);
          }
          else if(info->kind == RAD_SettingKind_U64 || info->kind == RAD_SettingKind_F32)
          {
            if(slider_value != rad_setting_pct_from_value(info, value))
            {
              F64 new_value = info->min + Clamp(0, slider_value, 1)*(info->max - info->min);
              if(info->kind == RAD_SettingKind_U64)
              {
                new_value = round_f64(new_value);
              }
              else
              {
                new_value = round_f64(new_value*100.0)/100.0;
              }
              if(new_value != value)
              {
                rad_setting_set_f64(info->name, new_value);
              }
            }
          }
        }
      }
    }
  }

  //////////////////////////////
  //- rad: options menu (enums, the theme): the cargo profile menu's rows
  //
  UI_CtxMenu(menu_key) UI_PrefWidth(ui_em(20.f, 1.f)) UI_TagF("implicit")
  {
    U64 setting_idx = Min(ss->menu_setting, RAD_SettingCode_COUNT-1);
    RAD_SettingInfo *info = &rad_setting_info_table[setting_idx];
    U64 current = (U64)rad_setting_f64_from_settings(settings, setting_idx);
    String8List options = {0};
    if(info->kind == RAD_SettingKind_Theme)
    {
      for EachEnumVal(RAD_ThemePreset, preset)
      {
        str8_list_push(scratch.arena, &options, rad_theme_preset_display_string_table[preset]);
      }
    }
    else
    {
      U8 split = '|';
      options = str8_split(scratch.arena, info->options, &split, 1, 0);
    }
    ss->layout_menu_frame = rad_state->frame_index;
    ss->layout_menu_count = 0;
    U64 option_idx = 0;
    for(String8Node *n = options.first; n != 0; n = n->next, option_idx += 1)
    {
      B32 selected = (option_idx == current);
      ui_set_next_child_layout_axis(Axis2_X);
      UI_Box *box = ui_build_box_from_stringf(UI_BoxFlag_DrawBorder|
                                              UI_BoxFlag_DrawBackground|
                                              UI_BoxFlag_DrawHotEffects|
                                              UI_BoxFlag_DrawActiveEffects|
                                              UI_BoxFlag_Clickable,
                                              "###setting_option_%I64u", option_idx);
      UI_Parent(box) UI_HeightFill UI_Padding(ui_em(1.f, 1.f))
      {
        RAD_Font(RAD_FontSlot_Icons)
          UI_PrefWidth(ui_em(2.f, 1.f))
          UI_TextAlignment(UI_TextAlign_Center)
          UI_TagF("weak")
          ui_label(selected ? rad_icon_kind_text_table[RAD_IconKind_Check] : str8_lit(""));
        UI_PrefWidth(ui_text_dim(10, 1.f)) ui_label(n->string);
      }
      if(option_idx < ArrayCount(ss->layout_menu_rects))
      {
        ss->layout_menu_rects[option_idx] = box->rect;
        ss->layout_menu_count = option_idx+1;
      }
      if(ui_clicked(ui_signal_from_box(box)))
      {
        rad_setting_set_f64(info->name, (F64)option_idx);
        ui_ctx_menu_close();
      }
    }
  }
  #undef rad_setting_pct_from_value

  scratch_end(scratch);
}
