// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

#ifndef RAD_LSP_H
#define RAD_LSP_H

////////////////////////////////
//~ rad: Notes
//
// Language servers (rust-analyzer), through the `lsp` layer. One server per
// Cargo workspace that has an open .rs document; it runs until rad quits.
// Everything here runs on the UI thread (rad_lsp_tick, every frame); the
// client's threads only move bytes and wake the UI.
//
//   open .rs document -> workspace root (rad_lsp_root_from_path) -> server
//   server start:  initialize -> (response) -> initialized -> Ready
//   documents:     didOpen (full text, version = the buffer's version)
//                  didChange (full text, at most every RAD_LSP_CHANGE_DELAY_US
//                             while typing; at once before a request)
//                  didSave (rad_lsp_document_saved), didClose (document gone)
//   from server:   publishDiagnostics, $/progress, experimental/serverStatus,
//                  requests (answered: null, or nulls for configuration)
//
// Positions are UTF-8 byte columns when the server agrees (rust-analyzer
// does), else UTF-16 units. Diagnostics name the document version they were
// computed for: the text sent for that version (the last few are kept per
// document) turns their positions into offsets, and the buffer's change log
// carries those to the current text (rad_diagnostic_range_from_document).
//
// Problems (rad_problems): cargo's diagnostics and the servers' in one list
// (duplicates dropped: rust-analyzer runs `cargo check` itself on save),
// errors first, then warnings, then notes; each group by path, line, column.
// The Problems tab, Next/Previous Error and the editor use it. Hints
// (severity 4: rust-analyzer's "related information" copies) are dropped.

////////////////////////////////
//~ rad: Tunables

#define RAD_LSP_CHANGE_DELAY_US     150000
#define RAD_LSP_SNAPSHOT_COUNT      4
#define RAD_LSP_SHUTDOWN_WAIT_US    1000000
#define RAD_LSP_HOVER_DELAY_US      500000
#define RAD_LSP_COMPLETION_MAX_ITEMS 4096
#define RAD_LSP_AUTO_RESTART_MAX    3           // restarts after a crash, per window below
#define RAD_LSP_AUTO_RESTART_WINDOW_US 60000000

////////////////////////////////
//~ rad: Server Types

typedef enum RAD_LspServerState
{
  RAD_LspServerState_Null,
  RAD_LspServerState_Starting,  // initialize sent
  RAD_LspServerState_Ready,     // initialized sent; documents sync
  RAD_LspServerState_Ended,     // exited, failed to start, or broke the protocol
}
RAD_LspServerState;

typedef struct RAD_LspSnapshot RAD_LspSnapshot;
struct RAD_LspSnapshot
{
  Arena *arena;
  B32 valid;
  U64 version;
  String8 text;
  U64 *line_starts;
  U64 line_count;
};

typedef struct RAD_LspDoc RAD_LspDoc;
struct RAD_LspDoc
{
  RAD_LspDoc *next;
  U64 doc_id;
  String8 path;                 // (arena) normalized
  String8 uri;                  // (arena)
  Arena *arena;
  B32 opened;
  B32 seen;                     // still open this frame
  U64 sent_version;
  U64 change_seen_us;           // first frame a not-yet-sent change was seen
  B32 saved_pending;            // send didSave after the next sync
  RAD_LspSnapshot snapshots[RAD_LSP_SNAPSHOT_COUNT];
  U64 snapshot_next;
};

typedef struct RAD_LspFileDiags RAD_LspFileDiags;
struct RAD_LspFileDiags
{
  RAD_LspFileDiags *next;
  Arena *arena;
  String8 path;                 // (arena) normalized
  RAD_Diagnostic **diags;
  U64 count;
};

// NOTE(rad): $/progress entries: fixed slots and buffers (servers send many
// reports; nothing is allocated per report)
#define RAD_LSP_PROGRESS_MAX 16
typedef struct RAD_LspProgress RAD_LspProgress;
struct RAD_LspProgress
{
  B32 active;
  U64 begin_us;
  U8 token[96];
  U64 token_size;
  U8 title[64];
  U64 title_size;
  U8 message[128];
  U64 message_size;
  S64 percentage;               // -1: none
};

typedef struct RAD_LspServer RAD_LspServer;
struct RAD_LspServer
{
  RAD_LspServer *next;
  Arena *arena;
  String8 root;                 // workspace folder, normalized
  String8 root_uri;
  String8 log_path;
  LSP_Client *client;
  RAD_LspServerState state;
  S64 initialize_id;
  B32 utf8_positions;
  B32 quiescent;                // experimental/serverStatus: no work queued
  String8 health_message;       // (health_arena) serverStatus message, if not ok
  Arena *health_arena;
  RAD_LspDoc *first_doc;
  RAD_LspFileDiags *first_file_diags;
  RAD_LspProgress progress[RAD_LSP_PROGRESS_MAX];
  U64 messages_received;
  U64 auto_restart_us[RAD_LSP_AUTO_RESTART_MAX]; // when it was restarted after a crash (0: never)
  U64 restarts;                 // restarts so far (command or crash)
};

typedef struct RAD_LspDocRoot RAD_LspDocRoot;
struct RAD_LspDocRoot
{
  RAD_LspDocRoot *next;
  U64 doc_id;
  String8 root;                 // empty: not in a Cargo project
  B32 external;                 // a library's source (toolchain, cargo registry/git):
                                // goes to an existing server, never starts one
};

// NOTE(rad): hover (Quick Info): the mouse rests on a token of a code view
// for RAD_LSP_HOVER_DELAY_US -> textDocument/hover -> the answer (markdown)
// in a tooltip while the mouse stays on that token
typedef struct RAD_LspHover RAD_LspHover;
struct RAD_LspHover
{
  B32 active;
  U64 frame_seen;               // last frame a view reported the mouse on it
  U64 doc_id;
  U64 doc_version;
  Rng1U64 token_range;
  U64 rest_begin_us;
  B32 requested;
  RAD_LspServer *server;
  S64 request_id;
  B32 answered;
  Arena *arena;
  String8 text;                 // (arena) markdown; empty: nothing to show
};

// NOTE(rad): Go To Definition: one request at a time (a newer one wins)
typedef struct RAD_LspDefinition RAD_LspDefinition;
struct RAD_LspDefinition
{
  B32 pending;
  RAD_LspServer *server;
  S64 request_id;
  CFG_ID window;
  String8 from_path;            // (arena) where the jump starts, for Navigate Backward
  S64 from_line;
  S64 from_column;
  Arena *arena;
};

// NOTE(rad): Navigate Backward: where definition jumps started
#define RAD_LSP_NAV_MAX 64
typedef struct RAD_LspNavEntry RAD_LspNavEntry;
struct RAD_LspNavEntry
{
  U8 path[512];
  U64 path_size;
  S64 line;
  S64 column;
};

// NOTE(rad): completion (Visual Studio's IntelliSense list), one at a time:
//
//   typed: a word's first character, `.`, `::`; or Complete Word (Ctrl+Space)
//     -> textDocument/completion at the cursor (pending edits sent first)
//   answer: items; their edits' ranges -> offsets in the text of the request's
//     version (its snapshot); later carried to the current text by the
//     buffer's change log (tb_buffer_map_offset)
//   list (rows): the items that match the typed word (filter = word start ..
//     cursor): prefix matches first, then substring, then subsequence; each
//     group in the server's order (sortText). A list the server marks
//     incomplete, or one with no matching rows, is asked for again whenever
//     the word changes (one request at a time).
//   popup: raddbg's autocompletion floating view (built before the panels,
//     from the last frame's anchor and rows); its scroll list takes Up/Down/
//     PageUp/PageDown (secondary-keyboard events, as raddbg's). The code view
//     takes Enter/Tab (commit: the item's edit, extended over what was typed
//     since, and its additional edits, e.g. a `use` line; one undo step) and
//     Esc (close). Moving the cursor otherwise, or typing a character that is
//     not part of a word, closes it.
//   imports: items from crates not imported yet (rust-analyzer offers them
//     only to clients that resolve) carry `data`. The selected one is
//     resolved while it is selected (Visual Studio), so a commit usually has
//     its `use` edit already and applies it in the same undo step. Committed
//     before the answer came: the `use` edit is applied when it comes,
//     carried over the commit and any typing since, in the commit's undo step
//     if nothing happened since (else its own; dropped if the commit was
//     undone).
//   documentation: rust-analyzer sends it with the items (rad does not ask
//     to resolve it); the selected item's is shown beside the list.
typedef struct RAD_LspTextEdit RAD_LspTextEdit;
struct RAD_LspTextEdit
{
  Rng1U64 range;                // offsets in the text of the items' version
  String8 text;
};

typedef struct RAD_LspCompletionItem RAD_LspCompletionItem;
struct RAD_LspCompletionItem
{
  String8 label;
  String8 detail;               // shown after the label (a signature, a type)
  String8 filter_text;
  String8 sort_text;
  S64 kind;                     // LSP CompletionItemKind
  B32 has_edit;
  RAD_LspTextEdit edit;         // else: `insert_text` replaces the typed word
  String8 insert_text;
  U64 edit_cursor;              // where the cursor goes in edit.text / insert_text (a
  U64 insert_cursor;            // snippet's first tab stop; else the end)
  RAD_LspTextEdit *more_edits;  // additionalTextEdits
  U64 more_edits_count;
  U64 more_edits_version;       // the text version their ranges are in
  String8 resolve_json;         // the item as sent, if it has edits to resolve (`data`)
  B32 resolved;                 // completionItem/resolve answered (more_edits are its)
  String8 documentation;        // markdown (or plain text)
};

typedef struct RAD_LspResolve RAD_LspResolve;
struct RAD_LspResolve
{
  RAD_LspServer *server;
  S64 request_id;               // 0: none
  U64 doc_id;
  U64 version;                  // the document's version the server had
  U64 items_gen;                // which list's item (RAD_LspCompletion.items_gen)
  U64 item_idx;
  B32 for_commit;               // the item was committed: apply the edits when they come
  U64 commit_step;              // ...in this undo step if still the last one (text_buffer)
};

typedef struct RAD_LspCompletion RAD_LspCompletion;
struct RAD_LspCompletion
{
  B32 active;
  CFG_ID window;
  CFG_ID tab;
  U64 doc_id;
  U64 word_start;               // in the text of `word_version`
  U64 word_version;
  U64 open_gen;                 // changes per session (restarts the open animation)
  U64 items_gen;                // changes per answer (resolve answers find their item by it)

  //- rad: request in flight
  RAD_LspServer *server;
  S64 request_id;               // 0: none
  U64 request_version;
  U8 request_filter[256];       // the typed word when it was sent
  U64 request_filter_size;
  B32 retry;                    // its answer came for text no longer kept: ask again

  //- rad: answer
  Arena *arena;
  B32 answered;
  B32 incomplete;
  U64 items_version;
  RAD_LspCompletionItem *items;
  U64 items_count;
  U8 items_filter[256];         // the typed word the items were computed for
  U64 items_filter_size;

  //- rad: list (rows: indices into items)
  Arena *filter_arena;
  String8 filter;               // (filter_arena)
  U64 *rows;
  U64 rows_count;
  Vec2S64 cursor;               // y: selected row, 1-based (scroll list cursor)
  UI_ScrollPt scroll_pt;

  //- rad: popup (the code view records the anchor; the window builds the
  // popup from it in the next frame)
  U64 anchor_frame;
  Rng2F32 anchor_line_rect;     // window pixels: the word start's line, from the word start
  U64 built_frame;              // last frame the popup was built (with rows)
  U64 doc_built_frame;          // last frame the documentation beside it was built
  B32 commit_requested;         // a row was clicked: the code view commits it
  Rng2F32 layout_rect;          // (scripts) popup rect, first visible row, row height
  S64 layout_first_row;
  Rng2F32 layout_first_row_rect;
};

typedef struct RAD_LspState RAD_LspState;
struct RAD_LspState
{
  Arena *arena;
  RAD_LspServer *first_server;
  RAD_LspServer *last_server;
  RAD_LspDocRoot *first_doc_root;
  B32 not_found;                // rust-analyzer could not be started: not tried again
  B32 disabled;                 // no servers started (scripts, unless `lsp on`)
  U64 diags_gen;
  U64 servers_started;          // launches (restarts too; numbers the log files)
  U64 servers_made;             // servers (one per workspace)
  RAD_LspHover hover;
  RAD_LspDefinition definition;
  RAD_LspNavEntry nav[RAD_LSP_NAV_MAX];
  U64 nav_count;
  RAD_LspCompletion completion;
  RAD_LspResolve resolve;
};

////////////////////////////////
//~ rad: Problems Types (cargo + language servers)

typedef struct RAD_Problems RAD_Problems;
struct RAD_Problems
{
  Arena *arena;
  RAD_Diagnostic **diags;
  U64 count;
  U64 error_count;
  U64 warning_count;
  U64 gen;                      // changes whenever the list is rebuilt
  U64 cursor;                   // 1-based; Next/Previous Error = Problems selection
  Arena *selected_key_arena;
  String8 selected_key;         // the selected one's key (rad_problems_select): the
                                // list's diagnostics can be gone before a rebuild
  U64 cargo_gen_seen;
  U64 lsp_gen_seen;
};

////////////////////////////////
//~ rad: Globals

global RAD_LspState *rad_lsp = 0;
global RAD_Problems *rad_problems = 0;

////////////////////////////////
//~ rad: Position Functions

internal String8 rad_lsp_uri_from_path(Arena *arena, String8 path);
internal String8 rad_lsp_path_from_uri(Arena *arena, String8 uri);
internal U64 rad_lsp_offset_from_position(String8 text, U64 *line_starts, U64 line_count, S64 line, S64 character, B32 utf8);
internal void rad_lsp_position_from_offset(String8 text, U64 off, B32 utf8, S64 *line_out, S64 *character_out);
internal String8 rad_lsp_root_from_path(Arena *arena, String8 path);

////////////////////////////////
//~ rad: Language Server Functions

internal void rad_lsp_init(void);
internal void rad_lsp_tick(void);
internal void rad_lsp_shutdown(void);
internal void rad_lsp_document_saved(RAD_Document *doc);
internal RAD_LspServer *rad_lsp_server_from_path(String8 path);
internal void rad_lsp_server_launch(RAD_LspServer *s);
internal void rad_lsp_server_end(RAD_LspServer *s, B32 clean);
internal void rad_lsp_doc_release(RAD_LspServer *s, RAD_LspDoc *ld);
internal void rad_lsp_server_restart(RAD_LspServer *s);
internal void rad_lsp_restart_all(void);
internal String8 rad_lsp_status_string(Arena *arena, B32 *busy_out);
internal String8 rad_lsp_markdown_from_hover(Arena *arena, JSON_Value *result);
internal String8 rad_lsp_position_params(Arena *arena, RAD_Document *doc, U64 off, RAD_LspServer **server_out);
internal String8 rad_lsp_hover_from_mouse(RAD_Document *doc, Rng1U64 token_range, U64 char_off);
internal void rad_lsp_goto_definition(CFG_Node *window, CFG_Node *tab, RAD_Document *doc, U64 off);
internal void rad_lsp_take_definition(RAD_LspServer *s, LSP_Message *m);
internal void rad_lsp_navigate_back(CFG_Node *window);
internal B32 rad_lsp_path_is_external(String8 path);

////////////////////////////////
//~ rad: Completion Functions

internal B32 rad_lsp_completion_request(CFG_Node *window, CFG_Node *tab, RAD_Document *doc, U64 off, U64 word_start, String8 word, U8 trigger_char, B32 for_incomplete);
internal void rad_lsp_take_completion(RAD_LspServer *s, LSP_Message *m);
internal void rad_lsp_completion_refilter(String8 filter);
internal void rad_lsp_completion_set_filter(String8 filter);
internal void rad_lsp_completion_close(void);
internal B32 rad_lsp_completion_is_shown(CFG_ID tab);
internal void rad_lsp_completion_resolve(RAD_Document *doc, U64 item_idx, B32 for_commit);
internal void rad_lsp_take_resolve(RAD_LspServer *s, LSP_Message *m);
internal void rad_document_apply_edits(RAD_Document *doc, U64 version, RAD_LspTextEdit *edits, U64 edits_count, U64 join_step); // (rad_views.c)

////////////////////////////////
//~ rad: Problems Functions

internal void rad_problems_tick(void);
internal void rad_problems_select(U64 cursor);
internal String8 rad_diagnostic_key(Arena *arena, RAD_Diagnostic *diag);
internal String8 rad_problem_display_path(Arena *arena, RAD_Diagnostic *diag);

#endif // RAD_LSP_H
