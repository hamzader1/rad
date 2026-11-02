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
//~ rad: Basic Widgets

internal UI_Signal rad_menu_bar_button(String8 string);
internal UI_Signal rad_icon_button(RAD_IconKind kind, FuzzyMatchRangeList *matches, String8 string);
internal UI_Signal rad_icon_buttonf(RAD_IconKind kind, FuzzyMatchRangeList *matches, char *fmt, ...);

////////////////////////////////
//~ rad: Command Widgets

internal void rad_cmd_binding_labels(RAD_CmdKind kind);
internal UI_Signal rad_cmd_spec_button(RAD_CmdKind kind);
internal void rad_cmd_list_menu_buttons(U64 count, RAD_CmdKind *kinds, U32 *fastpath_codepoints);

#endif // RAD_WIDGETS_H
