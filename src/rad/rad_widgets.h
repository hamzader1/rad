// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

#ifndef RAD_WIDGETS_H
#define RAD_WIDGETS_H

////////////////////////////////
//~ rad: Notes
//
// Ports of the RAD Debugger's frontend widgets (`raddbg/raddbg_widgets.c`),
// with the same box flags, sizes, and tags, minus the debugger-specific parts
// (config-driven rebinding, eval-driven names).

////////////////////////////////
//~ rad: Cell Types
//
// Port of raddbg's RD_CellFlags/RD_CellParams: depth indentation, expander,
// value text, and single-line editing (cells with an `edit_buffer`). No
// toggle switches, sliders, bindings, notes, locks, autocompletion, or
// name/description column.

typedef U32 RAD_CellFlags;
enum
{
  //- rad: expander
  RAD_CellFlag_Expander            = (1<<0),
  RAD_CellFlag_ExpanderSpace       = (1<<1),
  RAD_CellFlag_ExpanderPlaceholder = (1<<2),

  //- rad: behavior
  RAD_CellFlag_KeyboardClickable   = (1<<3),
  RAD_CellFlag_SingleClickActivate = (1<<4),
  RAD_CellFlag_DisableEdit         = (1<<8),

  //- rad: contents
  RAD_CellFlag_CodeContents        = (1<<9),

  //- rad: appearance
  RAD_CellFlag_Border              = (1<<5),
  RAD_CellFlag_NoBackground        = (1<<6),
  RAD_CellFlag_Button              = (1<<7),

  //- rad: extra widgets (raddbg's toggle switch, slider, revert button)
  RAD_CellFlag_ToggleSwitch        = (1<<10),
  RAD_CellFlag_Slider              = (1<<11),
  RAD_CellFlag_RevertButton        = (1<<12),
};

typedef struct RAD_CellParams RAD_CellParams;
struct RAD_CellParams
{
  //- rad: catchall parameters
  RAD_CellFlags flags;
  S32 depth;
  String8 pre_edit_value;
  String8 search_needle;
  DR_FStrList value_fstrs;

  //- rad: left-hand-side name + description (raddbg's meta_fstrs, description)
  DR_FStrList meta_fstrs;
  String8 description;

  //- rad: expander r/w info
  B32 *expanded_out;

  //- rad: toggle-switch, slider, revert r/w info
  B32 *toggled_out;
  F32 *slider_value_out;
  B32 *revert_out;

  //- rad: not in raddbg: where the toggle switch / slider and the revert
  // button are (last layout; scripted input aims with them)
  Rng2F32 *control_rect_out;
  Rng2F32 *revert_rect_out;

  //- rad: text editing r/w info
  U64 *cursor;
  U64 *mark;
  U8 *edit_buffer;
  U64 edit_buffer_size;
  U64 *edit_string_size_out;
  UI_Key *line_edit_key_out;
};

////////////////////////////////
//~ rad: Code Slice Types
//
// Port of raddbg's RD_CodeSliceParams/RD_CodeSliceSignal, minus breakpoint,
// thread, watch pin, and debug-info line data. Where raddbg colors a line and
// annotates it from debugger state (a thread stopped by an exception), rad
// takes the colors and annotations as parameters (cargo diagnostics).

typedef U32 RAD_CodeSliceFlags;
enum
{
  RAD_CodeSliceFlag_Clickable = (1<<0),
  RAD_CodeSliceFlag_LineNums  = (1<<1),
  RAD_CodeSliceFlag_CtrlLinks = (1<<2), // rad: Ctrl+hover/click on an identifier (Go To Definition)
  RAD_CodeSliceFlag_PriorityMargin = (1<<3), // rad: raddbg's margins (empty: breakpoints later)
  RAD_CodeSliceFlag_CatchallMargin = (1<<4), // rad: ...the line's diagnostic icon
};

typedef struct RAD_CodeSliceUnderline RAD_CodeSliceUnderline;
struct RAD_CodeSliceUnderline
{
  RAD_CodeSliceUnderline *next;
  Rng1U64 range;
  Vec4F32 color;
};

// NOTE(rad): a color for a range of the text (semantic tokens), by slot
typedef struct RAD_TokenColor RAD_TokenColor;
struct RAD_TokenColor
{
  Rng1U64 range;
  U64 slot;                     // RAD_CodeColorSlot
};

typedef struct RAD_CodeSliceParams RAD_CodeSliceParams;
struct RAD_CodeSliceParams
{
  // rad: content
  RAD_CodeSliceFlags flags;
  Rng1S64 line_num_range;
  String8 *line_text;
  Rng1U64 *line_ranges;
  TXT_TokenArray *line_tokens;
  TXT_TextInfo *text_info;
  TXT_PatchList *patches;
  String8 text_data;

  // rad: per visible line (optional): a background color (alpha 0 = none)
  // and text drawn after the line - ports of raddbg's per-line background
  // colors and per-line extras (its exception annotation); text ranges to
  // underline (diagnostic spans)
  Vec4F32 *line_bg_colors;
  DR_FStrList *line_annotations;
  RAD_CodeSliceUnderline *first_underline;
  DR_FStrList *line_margin_fstrs; // rad: drawn in the catchall margin
  RAD_TokenColor *token_colors; // rad: sorted by range.min; a lexer token starting at one takes its color
  U64 token_colors_count;
  S64 *line_nums;       // rad: per visible row, its line number (folds hide lines); 0 = rows are lines
  U8 *line_fold_kinds;  // rad: per visible row: 0 none, 1 a foldable range starts here, 2 folded here

  // rad: visual parameters
  FNT_Tag font;
  F32 font_size;
  F32 tab_size;
  String8 search_query;
  F32 line_height_px;
  F32 priority_margin_width_px;
  F32 catchall_margin_width_px;
  F32 fold_width_px;    // rad: the fold column, the right part of line_num_width_px
  F32 line_num_width_px;
  F32 line_text_max_width_px;
  F32 margin_float_off_px;
  F32 min_height_px; // rad: not in raddbg: line numbers and the clickable text area reach at least this far down (fill the view)
};

typedef struct RAD_CodeSliceSignal RAD_CodeSliceSignal;
struct RAD_CodeSliceSignal
{
  UI_Signal base;
  U64 mouse_off;
  Rng2F32 text_rect;  // rad: text container (line numbers + text), window pixels
  F32 text_x_off;     // rad: x of column 1, relative to text_rect.x0
  B32 mouse_over_text;      // rad: over a character that is not whitespace
  U64 mouse_char_off;       // rad: that character
  Rng1U64 mouse_token_range; // rad: its token
  B32 link_hovered;         // rad: Ctrl held over an identifier/keyword (RAD_CodeSliceFlag_CtrlLinks)
  B32 link_pressed;         // rad: ...and pressed there (the cursor did not move)
  U64 link_off;             // rad: the character under the mouse then
  Rng1U64 link_range;       // rad: its token
  U64 fold_toggle_idx;      // rad: visible row whose fold chevron / "..." was clicked (max_U64: none)
};

////////////////////////////////
//~ rad: Basic Widgets

internal UI_Signal rad_menu_bar_button(String8 string);
internal UI_Signal rad_icon_button(RAD_IconKind kind, FuzzyMatchRangeList *matches, String8 string);
internal UI_Signal rad_icon_buttonf(RAD_IconKind kind, FuzzyMatchRangeList *matches, char *fmt, ...);

////////////////////////////////
//~ rad: Command Widgets

internal void rad_cmd_binding_labels(RAD_CmdKind kind);
internal UI_Signal rad_cmd_spec_button(RAD_CmdKind kind);
internal void rad_cmd_list_menu_buttons(U64 count, RAD_CmdKind *kinds, U32 *fastpath_codepoints);

////////////////////////////////
//~ rad: Cells

internal void rad_loading_overlay(Rng2F32 rect, F32 loading_t, U64 progress_v, U64 progress_v_target);
internal UI_Signal rad_cell(RAD_CellParams *params, String8 string);
internal UI_Signal rad_cellf(RAD_CellParams *params, char *fmt, ...);

////////////////////////////////
//~ rad: Code Slice

internal RAD_CodeColorSlot rad_code_color_slot_from_txt_token_kind(TXT_TokenKind kind);
internal Vec4F32 rad_rgba_from_code_color_slot(RAD_CodeColorSlot slot);
internal DR_FStrList rad_fstrs_from_code_string(Arena *arena, F32 alpha, Vec4F32 base_color, String8 string);
internal DR_FStrList rad_fstrs_from_code_string_lang(Arena *arena, F32 alpha, Vec4F32 base_color, TXT_LangKind lang, String8 string);
internal String8 rad_markdown_inline_plain(Arena *arena, String8 line);
internal void rad_markdown_ui(String8 markdown, F32 max_width_px, U64 max_lines);
internal RAD_CodeSliceSignal rad_code_slice(RAD_CodeSliceParams *params, U64 *cursor, U64 *mark, S64 *preferred_column, String8 string);
internal RAD_CodeSliceSignal rad_code_slicef(RAD_CodeSliceParams *params, U64 *cursor, U64 *mark, S64 *preferred_column, char *fmt, ...);

#endif // RAD_WIDGETS_H
