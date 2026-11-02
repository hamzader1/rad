# How the read-only code view works (raddbg)

Checked in the code on 2026-11-02. Files: `raddbg/raddbg_views.c` (text view +
`rd_code_view_build`), `raddbg/raddbg_widgets.c` (`rd_code_slice`), `text/text.h`.

## Data path: file -> pixels

```
tab cfg "text: expression: <file path>"
  -> rd_regs()->text_key = key for (file space, byte range)      (file_stream / content)
  -> txt_text_info_from_key_lang(access, key, lang, &hash)        text/text.c
       content cache gives an immutable blob by 128-bit hash;
       the text layer lexes it asynchronously (artifact_cache)
       -> TXT_TextInfo: line ranges, line count, max line size,
          token "points" (lexer state checkpoints), scope tree
  -> data = c_data_from_hash(access, hash)
  -> rd_code_view_build(...)                                       raddbg_views.c:25
       TXT_Patched = txt_patched_from_info_data_patches(info, data, &cv->patches)
         memory_map + line_map + token_pt_map with patches applied
       keyboard: ui_single_line_txt_op_from_event + multi-line rules
         -> cursor/mark move, copy; (text replacement path exists but is
            disabled: "TODO(rjf): skip replace-ranges for now", line 161)
       visible line range from scroll position
       tokens for the visible bytes: txt_token_array_from_data(lang, ctx point, ...)
       fill RD_CodeSliceParams (per visible line: text, range, tokens,
         + debugger extras: breakpoints, thread IPs, watch pins, line info)
       container box (clip, scroll) -> rd_code_slice(params, &cursor, &mark)
       snapping (contain/center cursor), scroll bars (ui_scroll_bar), wheel
  -> text view bottom bar: path, "Line: N, Column: M", "(read only)", lf/crlf
```

## Where "read-only" comes from

1. The text comes from the **content cache**, which is keyed by hash and immutable.
   A file change produces a new hash (hot reload), it never mutates the old blob.
2. `rd_code_view_build` drops any text op that would replace text (line 161-165), so
   typed characters and deletes never become patches.
3. The view bottom bar prints the literal label `(read only)`.

## What already exists for editing

`TXT_PatchList` (range + replacement string) and `txt_patched_from_info_data_patches`
build a patched view over the immutable base: a memory map for bytes, a line map, and a
token-point map, so lexing can restart close to an edit. That is a piece-table-like
buffer: **base blob (immutable) + ordered patch list = current text**. Plan for milestone 4:

- current buffer = base + patches; **version = number of applied patches**;
- undo/redo = move a cursor over the patch list; save = materialize, write, rebase
  (new base hash, empty patch list);
- an LSP `didChange` is exactly one patch (range + text) -> incremental sync is natural.

## Assumptions an editable buffer breaks

- Line count / max line size come from the async `TXT_TextInfo` of the **base** blob;
  after edits they must come from `TXT_Patched` (already the case in most of
  `rd_code_view_build`).
- Tokens are computed from the base's token points; after a patch, tokens near and after
  the edit are relexed from the nearest earlier token point (`token_pt_map`).
- Hot reload from disk replaces the base hash; with unsaved patches, the patches would
  apply to different text. Need a "file changed on disk" policy before enabling saves.
- Cursor/mark are byte offsets into the patched text; anything storing offsets
  (diagnostics from rust-analyzer, search results) must be mapped across versions.
