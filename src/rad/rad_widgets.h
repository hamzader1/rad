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
//~ rad: Code Slice Types
//
// Port of raddbg's RD_CodeSliceParams/RD_CodeSliceSignal, minus breakpoint,
// thread, watch pin, and debug-info line data.

typedef U32 RAD_CodeSliceFlags;
enum
{
  RAD_CodeSliceFlag_Clickable = (1<<0),
  RAD_CodeSliceFlag_LineNums  = (1<<1),
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

  // rad: visual parameters
  FNT_Tag font;
  F32 font_size;
  F32 tab_size;
  String8 search_query;
  F32 line_height_px;
  F32 line_num_width_px;
  F32 line_text_max_width_px;
  F32 margin_float_off_px;
};

typedef struct RAD_CodeSliceSignal RAD_CodeSliceSignal;
struct RAD_CodeSliceSignal
{
  UI_Signal base;
  U64 mouse_off;
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
//~ rad: Code Slice

internal RAD_CodeColorSlot rad_code_color_slot_from_txt_token_kind(TXT_TokenKind kind);
internal Vec4F32 rad_rgba_from_code_color_slot(RAD_CodeColorSlot slot);
internal RAD_CodeSliceSignal rad_code_slice(RAD_CodeSliceParams *params, U64 *cursor, U64 *mark, S64 *preferred_column, String8 string);
internal RAD_CodeSliceSignal rad_code_slicef(RAD_CodeSliceParams *params, U64 *cursor, U64 *mark, S64 *preferred_column, char *fmt, ...);

#endif // RAD_WIDGETS_H
