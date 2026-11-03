# Syntax highlighting (how the code view colors text)

Written 2026-11-03 (milestone 7, Rust).

## Pipeline

```
file load (async, text layer)              every frame (code view)
  whole-file scan -> "big token" pts         pt before the first visible line
  (strings, chars, comments, attributes       |  (txt_token_pt_num_from_off)
   = tokens that may span lines)              v
  pt = {kind, off, aux}: a token starts    txt_token_array_from_data(ctx pt,
  (kind) or ends (Null) at off               visible bytes) -> tokens -> colors
                                              (rad_widgets.c: kind -> code color slot)
edits (TB_ patches, per frame)
  txt_patched_from_info_data_patches:
  keep pts before the edit, re-lex the
  edited region, keep pts after it shifted
```

The whole-file token list (`lex_function`, `txt_token_array_from_string__*`)
is disabled (`#if 0`) in this tree; only the pts and the visible-range
tokenizer matter.

## Rust (`src/text/text.c`, "rad: Rust Lexing")

C rules do not fit Rust: `'` also starts lifetimes, `#` starts attributes and
raw strings, block comments nest. One pair of helpers decides big tokens and
is used by all three paths (whole-file scan, re-lex after edits, visible
tokenizer), so they agree:

- `txt_rust_big_token_from_off`: does a big token start here? `"..."`,
  `b"..."`, `c"..."`, raw `r#"..."#` / `br##"..."##` (aux = hashes + 1),
  `'x'` / `b'x'` (only when a closing quote follows one char or an escape;
  otherwise it is a lifetime), `//...` to end of line, `/* */` (aux =
  nesting), `#[...]` / `#![...]` (aux = bracket depth).
- `txt_rust_big_token_advance`: one step inside one (escapes, raw closers,
  nesting).
- `txt_token_array_from_data__rust`: the visible tokenizer. Lifetimes and
  labels are emitted as keywords (VS Code's default theme colors them like
  keywords), whole attributes as meta, `r#type` as an identifier, numbers
  keep `_` and suffixes, primitive types are in the keyword table.

## Edits (shared with C)

`txt_patched_from_info_data_patches` picks the pts to keep around each edit:
before the re-lex start (3 bytes before the edit), and at or after the end of
the replaced range; the re-lex fills in between; numbers are contiguous by
construction. Rust re-lexes a token it starts inside from that token's start
(up to 64 KB back). `txt_token_pt_num_from_off` returns the last pt at or
before an offset across all map ranges (the ranges are not in offset order).

Exact after any edit that inserts or deletes whole tokens (rad_tests: 20
seeds x 50 random edits). Approximate while a token is left open (typing `"`
opens a string; the pts after the edit still describe the old text) until the
buffer compacts and the file is re-scanned.

## Tests

- `rad_tests`: token kinds for every construct; "resume" check: tokenizing
  each line from its pt context must give the same kinds as tokenizing the
  whole text, before and after hand-written and random edits.
- `tests/scripts/rust_syntax.radtest` with `tests/fixtures/syntax.rs`: field
  `token <line> <col>` gives the kind the code view uses there.
