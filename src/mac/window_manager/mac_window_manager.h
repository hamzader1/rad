// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

#ifndef MAC_WINDOW_MANAGER_H
#define MAC_WINDOW_MANAGER_H

////////////////////////////////
//~ rad: Includes

#include "mac/mac_cocoa.h"

////////////////////////////////
//~ rad: Notes
//
// Cocoa backend of the window manager layer. The app keeps its
// "pump events, build UI, draw" loop on the main thread: wm_get_events pumps
// NSApp's queue itself (nextEventMatchingMask), so NSApp never runs its own
// loop. Input events are translated here, before AppKit dispatch; key events
// are never handed to AppKit (no beeps, and key-ups arrive while Cmd is held).
//
// Units: the app works in pixels with a DPI (96 = 1x), like on Windows.
// Cocoa works in points (bottom-left origin), so everything is converted with
// the window's backing scale factor (Retina: DPI 192).
//
// Keys: Cmd and Control both act as Ctrl (Cmd+S, Cmd+C, Cmd+click as Mac users
// expect; rad's bindings are written with Ctrl); Option is Alt.
//
// Custom border (rad's own title bar, like raddbg on Windows): a titled window
// with a transparent, full-size title bar and hidden traffic lights. Clicks in
// the app-reported title bar (outside its client areas) drag the window;
// clicks on the edges go to AppKit's native resize.

////////////////////////////////
//~ rad: Window Types

typedef struct MAC_WM_WindowClientArea MAC_WM_WindowClientArea;
struct MAC_WM_WindowClientArea
{
  MAC_WM_WindowClientArea *next;
  Rng2F32 rect;
};

typedef struct MAC_WM_Window MAC_WM_Window;
struct MAC_WM_Window
{
  MAC_WM_Window *next;
  MAC_WM_Window *prev;
  NSWindow *window;
  NSView *view;
  id delegate;
  B32 custom_border;
  B32 no_activate;
  F32 custom_title_bar_thickness;
  F32 custom_edge_thickness;
  MAC_WM_WindowClientArea *first_client_area;
  MAC_WM_WindowClientArea *last_client_area;
  Vec2F32 scroll_accum;
};

////////////////////////////////
//~ rad: State

typedef struct MAC_WM_State MAC_WM_State;
struct MAC_WM_State
{
  Arena *arena;
  MAC_WM_Window *first_window;
  MAC_WM_Window *last_window;
  MAC_WM_Window *free_window;
  MAC_WM_WindowClientArea *free_client_area;
  WM_SystemInfo gfx_info;
  WM_Cursor last_set_cursor;
  WM_Modifiers modifier_state;
  U64 key_down_state[(WM_Key_COUNT+63)/64];
  U64 get_events_depth;
  B32 in_live_resize;
};

////////////////////////////////
//~ rad: Globals

global MAC_WM_State *mac_wm_state = 0;
global Arena *mac_wm_event_arena = 0;
global WM_EventList mac_wm_event_list = {0};

////////////////////////////////
//~ rad: Helpers

internal MAC_WM_Window *mac_wm_window_from_handle(WM_Window handle);
internal MAC_WM_Window *mac_wm_window_from_nswindow(NSWindow *window);
internal F32 mac_wm_scale_from_window(MAC_WM_Window *w);
internal Vec2F32 mac_wm_pixel_pos_from_event(MAC_WM_Window *w, NSEvent *event);
internal WM_Event *mac_wm_push_event(WM_EventKind kind, MAC_WM_Window *w);
internal WM_Key mac_wm_key_from_event(NSEvent *event, B32 *out_right_sided);
internal WM_Modifiers mac_wm_modifiers_from_flags(NSEventModifierFlags flags);
internal B32 mac_wm_handle_event(NSEvent *event);

#endif // MAC_WINDOW_MANAGER_H
