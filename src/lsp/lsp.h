// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

#ifndef LSP_H
#define LSP_H

////////////////////////////////
//~ rad: Notes
//
// A client for the Language Server Protocol's transport: JSON-RPC 2.0
// messages over a server process's stdin / stdout, each message framed as
//
//   Content-Length: <n>\r\n
//   \r\n
//   <n bytes of JSON>
//
// Depends on `base` and `json` only. Knows nothing of any particular
// request: callers build the JSON of params and results, and read what the
// server sends.
//
// A client runs the server in a process group (closing it kills the server
// and its children) and two threads:
//
//   reader: stdout -> frame parser -> inbox (raw message bodies), then calls
//           the `wakeup` hook (e.g. to wake the UI thread)
//   writer: outbox -> stdin, so callers never block on a full pipe
//
// The owner (the UI thread) sends with lsp_client_request/notify/respond and
// takes what arrived with lsp_client_take_inbox, parsing each body with
// lsp_message_from_body. Request ids are numbers, counted by the client;
// the server's own requests may use any id, kept as JSON text to answer
// with. The server's stderr (its log) goes to a file, or nowhere.

////////////////////////////////
//~ rad: Tunables

#define LSP_MAX_HEADER_SIZE  KB(8)
#define LSP_MAX_MESSAGE_SIZE MB(256)

////////////////////////////////
//~ rad: Framing

typedef struct LSP_FrameParser LSP_FrameParser;
struct LSP_FrameParser
{
  // NOTE(rad): unconsumed bytes are contiguous at the base of one of two
  // no-chain arenas; compaction copies them to the other one
  Arena *arenas[2];
  U64 arena_idx;
  U8 *base;
  U64 size;
  U64 consumed;
  B32 failed;         // malformed header: nothing after it can be trusted
};

////////////////////////////////
//~ rad: Messages

typedef enum LSP_MessageKind
{
  LSP_MessageKind_Null,         // not a JSON-RPC message
  LSP_MessageKind_Request,      // method + id
  LSP_MessageKind_Notification, // method, no id
  LSP_MessageKind_Response,     // id + result or error
}
LSP_MessageKind;

typedef struct LSP_Message LSP_Message;
struct LSP_Message
{
  LSP_MessageKind kind;
  String8 method;
  S64 id;               // numeric ids (ours are); 0 if the id is not a number
  String8 id_text;      // the id as JSON text (to answer a request with)
  JSON_Value *params;   // requests, notifications (nil if none)
  JSON_Value *result;   // responses (nil when `error` is set)
  JSON_Value *error;    // responses: {code, message} or nil
};

////////////////////////////////
//~ rad: Client

typedef struct LSP_ClientParams LSP_ClientParams;
struct LSP_ClientParams
{
  String8List cmd_line;
  String8 path;                 // working directory
  String8 stderr_path;          // server log file (truncated); empty = dropped
  void (*wakeup)(void);         // called (reader thread) when messages arrive or the server exits
};

typedef struct LSP_Client LSP_Client;
struct LSP_Client
{
  Arena *arena;
  void (*wakeup)(void);
  Process process;
  ProcessGroup group;
  File stdin_write;
  File stdout_read;
  Thread reader_thread;
  Thread writer_thread;
  S64 next_request_id;          // owner thread only

  //- rad: shared, under `mutex`
  Mutex mutex;
  CondVar outbox_cv;
  Arena *inbox_arena;
  String8List inbox;
  Arena *outbox_arenas[2];
  U64 outbox_arena_idx;
  String8List outbox;
  B32 writer_stop;
  B32 server_ended;             // stdout closed: the server exited or broke the framing
  B32 framing_failed;
  U64 bytes_received;
  U64 bytes_sent;
};

////////////////////////////////
//~ rad: Framing Functions

internal void lsp_frame_parser_init(LSP_FrameParser *p);
internal void lsp_frame_parser_release(LSP_FrameParser *p);
internal void lsp_frame_parser_push(LSP_FrameParser *p, String8 bytes);
internal B32 lsp_frame_parser_next(LSP_FrameParser *p, Arena *arena, String8 *body_out);
internal String8 lsp_frame_from_body(Arena *arena, String8 body);

////////////////////////////////
//~ rad: Message Functions

internal LSP_Message lsp_message_from_body(Arena *arena, String8 body);
internal String8 lsp_request_body(Arena *arena, S64 id, String8 method, String8 params_json);
internal String8 lsp_notification_body(Arena *arena, String8 method, String8 params_json);
internal String8 lsp_response_body(Arena *arena, String8 id_json, String8 result_json);

////////////////////////////////
//~ rad: Client Functions

internal LSP_Client *lsp_client_launch(LSP_ClientParams *params);
internal void lsp_client_release(LSP_Client *client, U64 exit_wait_us);
internal B32 lsp_client_is_alive(LSP_Client *client);
internal S64 lsp_client_request(LSP_Client *client, String8 method, String8 params_json);
internal void lsp_client_notify(LSP_Client *client, String8 method, String8 params_json);
internal void lsp_client_respond(LSP_Client *client, String8 id_json, String8 result_json);
internal String8List lsp_client_take_inbox(LSP_Client *client, Arena *arena);

#endif // LSP_H
