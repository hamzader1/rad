// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

#ifndef TERMINAL_H
#define TERMINAL_H

////////////////////////////////
//~ rad: Terminal Emulator (not in raddbg; TODO_2 E5, the integrated terminal)
//
// A VT100 / xterm screen. The output of a program on a pseudo-terminal goes
// in (term_write); a grid of cells comes out: the screen (main or alternate)
// under the main screen's scrollback. Replies the terminal owes the program
// (cursor position, device attributes, size) are collected for the caller to
// write back (term_take_replies). Depends on base only.
//
//   line index space (term_line_count, term_line_from_idx):
//   [0, scrollback_count)            scrollback, oldest first (main screen)
//   [scrollback_count, +rows)        the screen
//
// Not done: mouse reporting, tab stops other than every 8 columns, reflow
// of wrapped lines on resize, double-width / double-height lines, the
// kitty keyboard protocol, images.

////////////////////////////////
//~ rad: Cells

// 0: the default color; TERM_ColorKind_Index | 0..255; TERM_ColorKind_RGB | 0xRRGGBB
typedef U32 TERM_Color;
#define TERM_ColorKind_Mask  0xff000000u
#define TERM_ColorKind_Index 0x01000000u
#define TERM_ColorKind_RGB   0x02000000u

typedef U16 TERM_CellFlags;
enum
{
  TERM_CellFlag_Bold      = (1<<0),
  TERM_CellFlag_Faint     = (1<<1),
  TERM_CellFlag_Italic    = (1<<2),
  TERM_CellFlag_Underline = (1<<3),
  TERM_CellFlag_Inverse   = (1<<4),
  TERM_CellFlag_Hidden    = (1<<5),
  TERM_CellFlag_Strike    = (1<<6),
  TERM_CellFlag_Wide      = (1<<7), // a two-column character's first column
  TERM_CellFlag_WideTail  = (1<<8), // its second column (no codepoint)
};

typedef struct TERM_Cell TERM_Cell;
struct TERM_Cell
{
  U32 codepoint; // 0: blank
  TERM_Color fg;
  TERM_Color bg;
  TERM_CellFlags flags;
};

typedef struct TERM_Line TERM_Line;
struct TERM_Line
{
  TERM_Cell *cells;
  B32 wrapped; // continues on the next line (autowrap)
};

////////////////////////////////
//~ rad: State

typedef enum TERM_ParseState
{
  TERM_ParseState_Ground,
  TERM_ParseState_Escape,
  TERM_ParseState_Csi,
  TERM_ParseState_CsiIgnore,
  TERM_ParseState_Osc,
  TERM_ParseState_OscEscape,
  TERM_ParseState_String,       // DCS / SOS / PM / APC: dropped until ST
  TERM_ParseState_StringEscape,
}
TERM_ParseState;

typedef struct TERM_Cursor TERM_Cursor;
struct TERM_Cursor
{
  S32 x;
  S32 y;
  TERM_Color fg;
  TERM_Color bg;
  TERM_CellFlags flags;
  B32 pending_wrap;   // at the last column after a character: the next one wraps
  B32 origin_mode;    // DECOM: rows count from the top margin
  B32 line_drawing;   // G0 is DEC Special Graphics
};

#define TERM_PARAM_MAX 32
#define TERM_OSC_MAX 4096

typedef struct TERM_State TERM_State;
struct TERM_State
{
  Arena *arena;
  Arena *screen_arena;     // both screens (new one on resize)
  Arena *scrollback_arena; // the scrollback ring (new one when it widens)
  Arena *reply_arena;
  S32 cols;
  S32 rows;

  // rad: screens: rows lines each
  TERM_Line *main_lines;
  TERM_Line *alt_lines;
  B32 alt_active;
  TERM_Cursor cursor;
  TERM_Cursor saved_cursor[2]; // DECSC / DECRC, per screen (main, alternate)
  S32 scroll_top;              // the scrolling margins (DECSTBM), inclusive
  S32 scroll_bottom;

  // rad: scrollback (main screen only): a ring of scrollback_cap lines of
  // scrollback_cols cells
  TERM_Cell *scrollback_cells;
  B8 *scrollback_wrapped;
  S32 scrollback_cols;
  U64 scrollback_cap;
  U64 scrollback_first;
  U64 scrollback_count;
  U64 scrollback_pushed; // lines ever pushed: absolute line ids (term_line_base)

  // rad: modes
  B32 autowrap;        // DECAWM
  B32 insert_mode;     // IRM
  B32 app_cursor_keys; // DECCKM
  B32 app_keypad;      // DECKPAM
  B32 cursor_visible;  // DECTCEM
  B32 bracketed_paste; // 2004
  U32 cursor_style;    // DECSCUSR (0, 1: blinking block, 2: block, 3/4: underline, 5/6: bar)

  // rad: parser
  TERM_ParseState parse_state;
  U32 utf8_codepoint;
  U32 utf8_remaining;
  U32 params[TERM_PARAM_MAX];
  U32 param_count;
  U32 param_colon_mask; // bit i: params[i] followed a ':' (a sub-parameter)
  B32 param_started;
  U8 private_marker;    // '?' '>' '=' '<' or 0
  U8 intermediate;      // the last intermediate byte (0x20-0x2F) or 0
  U8 osc_buffer[TERM_OSC_MAX];
  U64 osc_size;
  U32 last_codepoint;   // for REP

  // rad: outputs
  U8 title_buffer[256];
  U64 title_size;
  String8List replies;
  U64 bell_count;
  U64 version; // changes with the screen
};

////////////////////////////////
//~ rad: Functions

internal S32 term_codepoint_width(U32 codepoint);

internal TERM_State *term_alloc(S32 cols, S32 rows, U64 scrollback_cap);
internal void term_release(TERM_State *t);
internal void term_resize(TERM_State *t, S32 cols, S32 rows);
internal void term_write(TERM_State *t, String8 data);
internal String8 term_take_replies(Arena *arena, TERM_State *t);
internal void term_clear_scrollback(TERM_State *t);

internal U64 term_line_count(TERM_State *t);
internal U64 term_line_base(TERM_State *t); // the absolute id of line index 0
internal TERM_Line term_line_from_idx(TERM_State *t, U64 line_idx, S32 *cols_out);
internal String8 term_string_from_line(Arena *arena, TERM_State *t, U64 line_idx);
internal String8 term_string_from_range(Arena *arena, TERM_State *t, U64 line0, S32 col0, U64 line1, S32 col1);
internal String8 term_title(TERM_State *t);

#endif // TERMINAL_H
