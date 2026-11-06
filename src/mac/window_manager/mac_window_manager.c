// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

////////////////////////////////
//~ rad: Cocoa Classes

@interface MAC_WM_View : NSView
@end

@implementation MAC_WM_View
- (BOOL)acceptsFirstResponder { return YES; }
- (BOOL)acceptsFirstMouse:(NSEvent *)event { return YES; }
- (BOOL)isOpaque { return YES; }
@end

@interface MAC_WM_WindowDelegate : NSObject<NSWindowDelegate>
@end

@implementation MAC_WM_WindowDelegate
- (BOOL)windowShouldClose:(NSWindow *)sender
{
  MAC_WM_Window *w = mac_wm_window_from_nswindow(sender);
  if(w != 0)
  {
    mac_wm_push_event(WM_EventKind_WindowClose, w);
  }
  return NO;
}
- (void)windowDidResignKey:(NSNotification *)notification
{
  MAC_WM_Window *w = mac_wm_window_from_nswindow(notification.object);
  if(w != 0)
  {
    mac_wm_push_event(WM_EventKind_WindowLoseFocus, w);
  }
  MemoryZeroArray(mac_wm_state->key_down_state);
  mac_wm_state->modifier_state = 0;
}
- (void)windowDidChangeOcclusionState:(NSNotification *)notification
{
  // rad: frames drawn while the window was covered never reached the screen
  // (the Metal renderer does not present them), so it shows an old one:
  // draw a new frame now that it can be seen
  NSWindow *window = notification.object;
  if(window.occlusionState & NSWindowOcclusionStateVisible)
  {
    wm_send_wakeup_event();
  }
}
- (void)windowDidResize:(NSNotification *)notification
{
  // rad: AppKit runs its own loop while the user resizes; draw a whole frame
  // from here (like win32's WM_SIZE -> update()), without pumping events.
  NSWindow *window = notification.object;
  if(window.inLiveResize && mac_wm_state->in_live_resize == 0)
  {
    mac_wm_state->in_live_resize = 1;
    update();
    mac_wm_state->in_live_resize = 0;
  }
}
@end

////////////////////////////////
//~ rad: Helpers

internal MAC_WM_Window *
mac_wm_window_from_handle(WM_Window handle)
{
  return (MAC_WM_Window *)handle.u64[0];
}

internal MAC_WM_Window *
mac_wm_window_from_nswindow(NSWindow *window)
{
  MAC_WM_Window *result = 0;
  for(MAC_WM_Window *w = mac_wm_state->first_window; w != 0; w = w->next)
  {
    if(w->window == window)
    {
      result = w;
      break;
    }
  }
  return result;
}

internal F32
mac_wm_scale_from_window(MAC_WM_Window *w)
{
  F32 result = 1.f;
  if(w != 0 && w->window != nil)
  {
    result = (F32)w->window.backingScaleFactor;
  }
  return result;
}

internal Vec2F32
mac_wm_pixel_pos_from_point(MAC_WM_Window *w, NSPoint window_point)
{
  NSPoint p = [w->view convertPoint:window_point fromView:nil];
  F32 scale = mac_wm_scale_from_window(w);
  Vec2F32 result = v2f32((F32)p.x*scale, (F32)(w->view.bounds.size.height - p.y)*scale);
  return result;
}

internal Vec2F32
mac_wm_pixel_pos_from_event(MAC_WM_Window *w, NSEvent *event)
{
  return mac_wm_pixel_pos_from_point(w, event.locationInWindow);
}

internal WM_Event *
mac_wm_push_event(WM_EventKind kind, MAC_WM_Window *w)
{
  // rad: outside wm_get_events (e.g. a delegate callback during live resize)
  // there is no list to push to; such events land in a throwaway slot
  local_persist WM_Event throwaway_event = {0};
  MemoryZeroStruct(&throwaway_event);
  WM_Event *event = &throwaway_event;
  if(mac_wm_event_arena != 0)
  {
    event = wm_event_list_push_new(mac_wm_event_arena, &mac_wm_event_list, kind);
    event->timestamp_us = now_time_us();
    event->window.u64[0] = (U64)w;
    event->modifiers = mac_wm_state->modifier_state;
  }
  return event;
}

internal WM_Modifiers
mac_wm_modifiers_from_flags(NSEventModifierFlags flags)
{
  WM_Modifiers result = 0;
  if(flags & (NSEventModifierFlagCommand|NSEventModifierFlagControl)) { result |= WM_Modifier_Ctrl; }
  if(flags & NSEventModifierFlagShift)                                { result |= WM_Modifier_Shift; }
  if(flags & NSEventModifierFlagOption)                               { result |= WM_Modifier_Alt; }
  return result;
}

internal WM_Key
mac_wm_key_from_keycode(U16 keycode, B32 *out_right_sided)
{
  // rad: macOS virtual key codes (Carbon's kVK_*; positional, US layout)
  WM_Key key = WM_Key_Null;
  B32 right_sided = 0;
  switch(keycode)
  {
    default:{}break;
    case 0x00:{key = WM_Key_A;}break;
    case 0x01:{key = WM_Key_S;}break;
    case 0x02:{key = WM_Key_D;}break;
    case 0x03:{key = WM_Key_F;}break;
    case 0x04:{key = WM_Key_H;}break;
    case 0x05:{key = WM_Key_G;}break;
    case 0x06:{key = WM_Key_Z;}break;
    case 0x07:{key = WM_Key_X;}break;
    case 0x08:{key = WM_Key_C;}break;
    case 0x09:{key = WM_Key_V;}break;
    case 0x0B:{key = WM_Key_B;}break;
    case 0x0C:{key = WM_Key_Q;}break;
    case 0x0D:{key = WM_Key_W;}break;
    case 0x0E:{key = WM_Key_E;}break;
    case 0x0F:{key = WM_Key_R;}break;
    case 0x10:{key = WM_Key_Y;}break;
    case 0x11:{key = WM_Key_T;}break;
    case 0x12:{key = WM_Key_1;}break;
    case 0x13:{key = WM_Key_2;}break;
    case 0x14:{key = WM_Key_3;}break;
    case 0x15:{key = WM_Key_4;}break;
    case 0x16:{key = WM_Key_6;}break;
    case 0x17:{key = WM_Key_5;}break;
    case 0x18:{key = WM_Key_Equal;}break;
    case 0x19:{key = WM_Key_9;}break;
    case 0x1A:{key = WM_Key_7;}break;
    case 0x1B:{key = WM_Key_Minus;}break;
    case 0x1C:{key = WM_Key_8;}break;
    case 0x1D:{key = WM_Key_0;}break;
    case 0x1E:{key = WM_Key_RightBracket;}break;
    case 0x1F:{key = WM_Key_O;}break;
    case 0x20:{key = WM_Key_U;}break;
    case 0x21:{key = WM_Key_LeftBracket;}break;
    case 0x22:{key = WM_Key_I;}break;
    case 0x23:{key = WM_Key_P;}break;
    case 0x24:{key = WM_Key_Return;}break;
    case 0x25:{key = WM_Key_L;}break;
    case 0x26:{key = WM_Key_J;}break;
    case 0x27:{key = WM_Key_Quote;}break;
    case 0x28:{key = WM_Key_K;}break;
    case 0x29:{key = WM_Key_Semicolon;}break;
    case 0x2A:{key = WM_Key_BackSlash;}break;
    case 0x2B:{key = WM_Key_Comma;}break;
    case 0x2C:{key = WM_Key_Slash;}break;
    case 0x2D:{key = WM_Key_N;}break;
    case 0x2E:{key = WM_Key_M;}break;
    case 0x2F:{key = WM_Key_Period;}break;
    case 0x30:{key = WM_Key_Tab;}break;
    case 0x31:{key = WM_Key_Space;}break;
    case 0x32:{key = WM_Key_Tick;}break;
    case 0x33:{key = WM_Key_Backspace;}break;
    case 0x35:{key = WM_Key_Esc;}break;
    case 0x36:{key = WM_Key_Ctrl; right_sided = 1;}break; // right command
    case 0x37:{key = WM_Key_Ctrl;}break;                  // command
    case 0x38:{key = WM_Key_Shift;}break;
    case 0x39:{key = WM_Key_CapsLock;}break;
    case 0x3A:{key = WM_Key_Alt;}break;
    case 0x3B:{key = WM_Key_Ctrl;}break;
    case 0x3C:{key = WM_Key_Shift; right_sided = 1;}break;
    case 0x3D:{key = WM_Key_Alt; right_sided = 1;}break;
    case 0x3E:{key = WM_Key_Ctrl; right_sided = 1;}break;
    case 0x40:{key = WM_Key_F17;}break;
    case 0x41:{key = WM_Key_NumPeriod;}break;
    case 0x43:{key = WM_Key_NumStar;}break;
    case 0x45:{key = WM_Key_NumPlus;}break;
    case 0x47:{key = WM_Key_NumLock;}break;
    case 0x4B:{key = WM_Key_NumSlash;}break;
    case 0x4C:{key = WM_Key_Return;}break;
    case 0x4E:{key = WM_Key_NumMinus;}break;
    case 0x4F:{key = WM_Key_F18;}break;
    case 0x50:{key = WM_Key_F19;}break;
    case 0x52:{key = WM_Key_Num0;}break;
    case 0x53:{key = WM_Key_Num1;}break;
    case 0x54:{key = WM_Key_Num2;}break;
    case 0x55:{key = WM_Key_Num3;}break;
    case 0x56:{key = WM_Key_Num4;}break;
    case 0x57:{key = WM_Key_Num5;}break;
    case 0x58:{key = WM_Key_Num6;}break;
    case 0x59:{key = WM_Key_Num7;}break;
    case 0x5A:{key = WM_Key_F20;}break;
    case 0x5B:{key = WM_Key_Num8;}break;
    case 0x5C:{key = WM_Key_Num9;}break;
    case 0x60:{key = WM_Key_F5;}break;
    case 0x61:{key = WM_Key_F6;}break;
    case 0x62:{key = WM_Key_F7;}break;
    case 0x63:{key = WM_Key_F3;}break;
    case 0x64:{key = WM_Key_F8;}break;
    case 0x65:{key = WM_Key_F9;}break;
    case 0x67:{key = WM_Key_F11;}break;
    case 0x69:{key = WM_Key_F13;}break;
    case 0x6A:{key = WM_Key_F16;}break;
    case 0x6B:{key = WM_Key_F14;}break;
    case 0x6D:{key = WM_Key_F10;}break;
    case 0x6E:{key = WM_Key_Menu;}break;
    case 0x6F:{key = WM_Key_F12;}break;
    case 0x71:{key = WM_Key_F15;}break;
    case 0x72:{key = WM_Key_Insert;}break;
    case 0x73:{key = WM_Key_Home;}break;
    case 0x74:{key = WM_Key_PageUp;}break;
    case 0x75:{key = WM_Key_Delete;}break;
    case 0x76:{key = WM_Key_F4;}break;
    case 0x77:{key = WM_Key_End;}break;
    case 0x78:{key = WM_Key_F2;}break;
    case 0x79:{key = WM_Key_PageDown;}break;
    case 0x7A:{key = WM_Key_F1;}break;
    case 0x7B:{key = WM_Key_Left;}break;
    case 0x7C:{key = WM_Key_Right;}break;
    case 0x7D:{key = WM_Key_Down;}break;
    case 0x7E:{key = WM_Key_Up;}break;
  }
  if(out_right_sided != 0) { *out_right_sided = right_sided; }
  return key;
}

internal WM_Key
mac_wm_key_from_event(NSEvent *event, B32 *out_right_sided)
{
  WM_Key key = mac_wm_key_from_keycode(event.keyCode, out_right_sided);

  // rad: letters and digits follow the keyboard layout (like Windows' virtual
  // keys), so Ctrl+Z is the key labeled Z on AZERTY/QWERTZ layouts too
  if(event.type == NSEventTypeKeyDown || event.type == NSEventTypeKeyUp)
  {
    NSString *chars = event.charactersIgnoringModifiers;
    if(chars.length == 1)
    {
      unichar c = [chars characterAtIndex:0];
      if('A' <= c && c <= 'Z') { c += 'a'-'A'; }
      if('a' <= c && c <= 'z')
      {
        local_persist WM_Key letter_keys[] =
        {
          WM_Key_A, WM_Key_B, WM_Key_C, WM_Key_D, WM_Key_E, WM_Key_F, WM_Key_G, WM_Key_H, WM_Key_I,
          WM_Key_J, WM_Key_K, WM_Key_L, WM_Key_M, WM_Key_N, WM_Key_O, WM_Key_P, WM_Key_Q, WM_Key_R,
          WM_Key_S, WM_Key_T, WM_Key_U, WM_Key_V, WM_Key_W, WM_Key_X, WM_Key_Y, WM_Key_Z,
        };
        key = letter_keys[c-'a'];
      }
    }
  }
  return key;
}

internal void
mac_wm_set_key_down(WM_Key key, B32 down)
{
  if(down) { mac_wm_state->key_down_state[key/64] |=  (1ull<<(key%64)); }
  else     { mac_wm_state->key_down_state[key/64] &= ~(1ull<<(key%64)); }
}

internal B32
mac_wm_point_is_in_title_bar(MAC_WM_Window *w, Vec2F32 pos)
{
  B32 result = 0;
  if(w->custom_border && pos.y < w->custom_title_bar_thickness)
  {
    result = 1;
    for(MAC_WM_WindowClientArea *a = w->first_client_area; a != 0; a = a->next)
    {
      if(contains_2f32(a->rect, pos))
      {
        result = 0;
        break;
      }
    }
  }
  return result;
}

internal B32
mac_wm_point_is_on_edge(MAC_WM_Window *w, Vec2F32 pos)
{
  B32 result = 0;
  if(w->custom_border && !(w->window.styleMask & NSWindowStyleMaskFullScreen))
  {
    Rng2F32 rect = wm_client_rect_from_window((WM_Window){(U64)w});
    F32 t = w->custom_edge_thickness;
    result = (pos.x < rect.x0+t || rect.x1-t <= pos.x || pos.y < rect.y0+t || rect.y1-t <= pos.y);
  }
  return result;
}

internal void
mac_wm_push_text(MAC_WM_Window *w, NSEvent *event)
{
  // rad: like win32's WM_CHAR: no text for Ctrl (Cmd/Control) combinations,
  // control characters dropped, Return -> '\n'; AppKit's function-key
  // characters (U+F700..U+F8FF) dropped. Option (AltGr-like) keeps its text.
  NSEventModifierFlags flags = event.modifierFlags;
  if(flags & (NSEventModifierFlagCommand|NSEventModifierFlagControl)) { return; }
  NSString *chars = event.characters;
  NSUInteger count = chars.length;
  for(NSUInteger idx = 0; idx < count; idx += 1)
  {
    U32 c = [chars characterAtIndex:idx];
    if(0xD800 <= c && c <= 0xDBFF && idx+1 < count)
    {
      U32 lo = [chars characterAtIndex:idx+1];
      if(0xDC00 <= lo && lo <= 0xDFFF)
      {
        c = 0x10000 + ((c - 0xD800) << 10) + (lo - 0xDC00);
        idx += 1;
      }
    }
    if(c == '\r' || c == 0x03) { c = '\n'; }
    B32 is_text = ((c >= 32 && c != 127) || c == '\n') && !(0xF700 <= c && c <= 0xF8FF);
    if(is_text)
    {
      WM_Event *e = mac_wm_push_event(WM_EventKind_Text, w);
      e->character = c;
    }
  }
}

internal B32
mac_wm_handle_event(NSEvent *event)
{
  // rad: returns whether AppKit should still see the event
  B32 forward = 1;
  NSEventType type = event.type;
  MAC_WM_Window *w = mac_wm_window_from_nswindow(event.window);
  if(type != NSEventTypeApplicationDefined)
  {
    mac_wm_state->modifier_state = mac_wm_modifiers_from_flags(event.modifierFlags);
  }
  switch(type)
  {
    default:{}break;

    //- rad: wakeups (wm_send_wakeup_event)
    case NSEventTypeApplicationDefined:
    {
      forward = 0;
    }break;

    //- rad: keys
    case NSEventTypeKeyDown:
    case NSEventTypeKeyUp:
    {
      forward = 0;
      if(w == 0) { break; }
      B32 down = (type == NSEventTypeKeyDown);
      B32 right_sided = 0;
      WM_Key key = mac_wm_key_from_event(event, &right_sided);

      // rad: Cmd+Q quits, as on every Mac app (closes the window)
      if(down && key == WM_Key_Q && (event.modifierFlags & NSEventModifierFlagCommand))
      {
        mac_wm_push_event(WM_EventKind_WindowClose, w);
        break;
      }

      if(key != WM_Key_Null)
      {
        WM_Event *e = mac_wm_push_event(down ? WM_EventKind_Press : WM_EventKind_Release, w);
        e->key = key;
        e->is_repeat = down && event.isARepeat;
        e->repeat_count = 1;
        e->right_sided = right_sided;
        mac_wm_set_key_down(key, down);
      }
      if(down)
      {
        mac_wm_push_text(w, event);
      }
    }break;

    //- rad: modifier keys
    case NSEventTypeFlagsChanged:
    {
      forward = 0;
      if(w == 0) { break; }
      B32 right_sided = 0;
      WM_Key key = mac_wm_key_from_keycode(event.keyCode, &right_sided);
      NSEventModifierFlags flag = 0;
      switch(event.keyCode)
      {
        default:{}break;
        case 0x36: case 0x37: {flag = NSEventModifierFlagCommand;}break;
        case 0x3B: case 0x3E: {flag = NSEventModifierFlagControl;}break;
        case 0x38: case 0x3C: {flag = NSEventModifierFlagShift;}break;
        case 0x3A: case 0x3D: {flag = NSEventModifierFlagOption;}break;
      }
      if(flag != 0 && key != WM_Key_Null)
      {
        B32 down = !!(event.modifierFlags & flag);
        WM_Event *e = mac_wm_push_event(down ? WM_EventKind_Press : WM_EventKind_Release, w);
        e->key = key;
        e->right_sided = right_sided;
        if(key == WM_Key_Alt)   { e->modifiers &= ~WM_Modifier_Alt; }
        if(key == WM_Key_Ctrl)  { e->modifiers &= ~WM_Modifier_Ctrl; }
        if(key == WM_Key_Shift) { e->modifiers &= ~WM_Modifier_Shift; }
        mac_wm_set_key_down(key, down);
      }
    }break;

    //- rad: mouse buttons
    case NSEventTypeLeftMouseDown:
    case NSEventTypeRightMouseDown:
    case NSEventTypeOtherMouseDown:
    {
      if(w == 0) { break; }
      Vec2F32 pos = mac_wm_pixel_pos_from_event(w, event);
      Rng2F32 client_rect = wm_client_rect_from_window((WM_Window){(U64)w});
      B32 outside = !contains_2f32(client_rect, pos);
      if(type == NSEventTypeLeftMouseDown && (outside || mac_wm_point_is_on_edge(w, pos)))
      {
        // rad: native resize from the edges
        break;
      }
      forward = !w->custom_border;
      if(w->custom_border && !w->window.isKeyWindow)
      {
        [w->window makeKeyAndOrderFront:nil];
        [NSApp activateIgnoringOtherApps:YES];
      }
      if(type == NSEventTypeLeftMouseDown && mac_wm_point_is_in_title_bar(w, pos))
      {
        forward = 0;
        if(event.clickCount == 2)
        {
          [w->window zoom:nil];
        }
        else
        {
          [w->window performWindowDragWithEvent:event];
        }
        break;
      }
      WM_Event *e = mac_wm_push_event(WM_EventKind_Press, w);
      e->key = (type == NSEventTypeLeftMouseDown  ? WM_Key_LeftMouseButton :
                type == NSEventTypeRightMouseDown ? WM_Key_RightMouseButton :
                WM_Key_MiddleMouseButton);
      e->pos = pos;
      mac_wm_set_key_down(e->key, 1);
    }break;
    case NSEventTypeLeftMouseUp:
    case NSEventTypeRightMouseUp:
    case NSEventTypeOtherMouseUp:
    {
      if(w == 0) { break; }
      forward = !w->custom_border;
      WM_Key key = (type == NSEventTypeLeftMouseUp  ? WM_Key_LeftMouseButton :
                    type == NSEventTypeRightMouseUp ? WM_Key_RightMouseButton :
                    WM_Key_MiddleMouseButton);
      WM_Event *e = mac_wm_push_event(WM_EventKind_Release, w);
      e->key = key;
      e->pos = mac_wm_pixel_pos_from_event(w, event);
      mac_wm_set_key_down(key, 0);
    }break;

    //- rad: mouse motion
    case NSEventTypeMouseMoved:
    case NSEventTypeLeftMouseDragged:
    case NSEventTypeRightMouseDragged:
    case NSEventTypeOtherMouseDragged:
    {
      if(w == 0) { break; }
      WM_Event *e = mac_wm_push_event(WM_EventKind_MouseMove, w);
      e->pos = mac_wm_pixel_pos_from_event(w, event);
      if(!wm_sim_is_enabled() && contains_2f32(wm_client_rect_from_window((WM_Window){(U64)w}), e->pos))
      {
        wm_set_cursor(mac_wm_state->last_set_cursor);
      }
    }break;

    //- rad: scrolling. Win32 units: 120 per wheel notch, and the UI scrolls
    // one step per 40 (at least one per event). Trackpads send many small
    // deltas, so they are accumulated and sent in whole steps.
    case NSEventTypeScrollWheel:
    {
      forward = 0;
      if(w == 0) { break; }
      Vec2F32 delta = {0};
      if(event.hasPreciseScrollingDeltas)
      {
        if(event.phase == NSEventPhaseBegan) { w->scroll_accum = v2f32(0, 0); }
        F32 units_per_point = 2.5f;
        w->scroll_accum.x += (F32)event.scrollingDeltaX*units_per_point;
        w->scroll_accum.y -= (F32)event.scrollingDeltaY*units_per_point;
        for EachIndex(axis, 2)
        {
          F32 steps = (F32)(S32)(w->scroll_accum.v[axis]/40.f);
          delta.v[axis] = steps*40.f;
          w->scroll_accum.v[axis] -= steps*40.f;
        }
      }
      else
      {
        delta = v2f32((F32)event.scrollingDeltaX*120.f, -(F32)event.scrollingDeltaY*120.f);
      }
      if(delta.x != 0 || delta.y != 0)
      {
        WM_Event *e = mac_wm_push_event(WM_EventKind_Scroll, w);
        e->pos = mac_wm_pixel_pos_from_event(w, event);
        e->delta = delta;
      }
    }break;
  }
  return forward;
}

////////////////////////////////
//~ rad: @per_os_impl Basic Initialization (Implemented Per-OS)

internal void
wm_init(void)
{
  Arena *arena = arena_alloc();
  mac_wm_state = push_array(arena, MAC_WM_State, 1);
  mac_wm_state->arena = arena;

  //- rad: application
  [NSApplication sharedApplication];
  [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
  [NSApp finishLaunching];

  //- rad: system info
  mac_wm_state->gfx_info.double_click_time = (F32)[NSEvent doubleClickInterval];
  mac_wm_state->gfx_info.caret_blink_time = 0.5f;
  mac_wm_state->gfx_info.default_refresh_rate = 60.f;
  if(@available(macOS 12.0, *))
  {
    NSInteger fps = NSScreen.mainScreen.maximumFramesPerSecond;
    if(fps > 0) { mac_wm_state->gfx_info.default_refresh_rate = (F32)fps; }
  }
}

////////////////////////////////
//~ rad: @per_os_impl Graphics System Info (Implemented Per-OS)

internal WM_SystemInfo *
wm_get_system_info(void)
{
  return &mac_wm_state->gfx_info;
}

////////////////////////////////
//~ rad: @per_os_impl Clipboards (Implemented Per-OS)

internal void
wm_set_clipboard_text(String8 string)
{
  if(wm_sim_is_enabled()) { wm_sim_set_clipboard_text(string); return; }
  @autoreleasepool
  {
    NSString *ns = [[[NSString alloc] initWithBytes:string.str length:string.size encoding:NSUTF8StringEncoding] autorelease];
    NSPasteboard *pb = [NSPasteboard generalPasteboard];
    [pb clearContents];
    if(ns != nil) { [pb setString:ns forType:NSPasteboardTypeString]; }
  }
}

internal String8
wm_get_clipboard_text(Arena *arena)
{
  if(wm_sim_is_enabled()) { return wm_sim_get_clipboard_text(arena); }
  String8 result = {0};
  @autoreleasepool
  {
    NSString *ns = [[NSPasteboard generalPasteboard] stringForType:NSPasteboardTypeString];
    if(ns != nil)
    {
      result = str8_copy(arena, str8_cstring((char *)ns.UTF8String));
    }
  }
  return result;
}

////////////////////////////////
//~ rad: @per_os_impl Windows (Implemented Per-OS)

internal NSRect
mac_wm_screen_rect_from_pixel_rect(Rng2F32 rect)
{
  // rad: pixels, top-left origin of the primary screen -> points, bottom-left
  NSScreen *primary = NSScreen.screens.firstObject;
  F32 scale = primary ? (F32)primary.backingScaleFactor : 1.f;
  F32 primary_h = primary ? (F32)primary.frame.size.height : 0;
  NSRect result = NSMakeRect(rect.x0/scale, primary_h - rect.y1/scale, (rect.x1-rect.x0)/scale, (rect.y1-rect.y0)/scale);
  return result;
}

internal Rng2F32
mac_wm_pixel_rect_from_screen_rect(NSRect rect)
{
  NSScreen *primary = NSScreen.screens.firstObject;
  F32 scale = primary ? (F32)primary.backingScaleFactor : 1.f;
  F32 primary_h = primary ? (F32)primary.frame.size.height : 0;
  F32 top = primary_h - (F32)(rect.origin.y + rect.size.height);
  Rng2F32 result = r2f32p((F32)rect.origin.x*scale, top*scale, (F32)(rect.origin.x + rect.size.width)*scale, (top + (F32)rect.size.height)*scale);
  return result;
}

internal WM_Window
wm_window_open(Rng2F32 rect, WM_WindowFlags flags, String8 title)
{
  //- rad: allocate
  MAC_WM_Window *w = mac_wm_state->free_window;
  if(w != 0) { SLLStackPop(mac_wm_state->free_window); MemoryZeroStruct(w); }
  else       { w = push_array(mac_wm_state->arena, MAC_WM_Window, 1); }
  DLLPushBack(mac_wm_state->first_window, mac_wm_state->last_window, w);
  w->custom_border = !!(flags & WM_WindowFlag_CustomBorder);
  w->no_activate = !!(flags & WM_WindowFlag_NoActivate);

  @autoreleasepool
  {
    //- rad: make window
    NSWindowStyleMask style = (NSWindowStyleMaskTitled|NSWindowStyleMaskClosable|NSWindowStyleMaskMiniaturizable|NSWindowStyleMaskResizable);
    if(w->custom_border) { style |= NSWindowStyleMaskFullSizeContentView; }
    NSRect content_rect = mac_wm_screen_rect_from_pixel_rect(rect);
    w->window = [[NSWindow alloc] initWithContentRect:content_rect styleMask:style backing:NSBackingStoreBuffered defer:NO];
    w->window.releasedWhenClosed = NO;
    w->window.acceptsMouseMovedEvents = YES;
    w->window.collectionBehavior = NSWindowCollectionBehaviorFullScreenPrimary;
    w->window.tabbingMode = NSWindowTabbingModeDisallowed;
    w->window.colorSpace = [NSColorSpace sRGBColorSpace]; // rad: our pixels are sRGB (as on Windows)
    w->window.title = [[[NSString alloc] initWithBytes:title.str length:title.size encoding:NSUTF8StringEncoding] autorelease];
    if(w->custom_border)
    {
      w->window.titlebarAppearsTransparent = YES;
      w->window.titleVisibility = NSWindowTitleHidden;
      [w->window standardWindowButton:NSWindowCloseButton].hidden = YES;
      [w->window standardWindowButton:NSWindowMiniaturizeButton].hidden = YES;
      [w->window standardWindowButton:NSWindowZoomButton].hidden = YES;
    }
    if(flags & WM_WindowFlag_UseDefaultPosition)
    {
      [w->window center];
    }

    //- rad: content view
    w->view = [[MAC_WM_View alloc] initWithFrame:NSMakeRect(0, 0, content_rect.size.width, content_rect.size.height)];
    w->view.wantsBestResolutionOpenGLSurface = YES;
    w->window.contentView = w->view;
    [w->window makeFirstResponder:w->view];

    //- rad: delegate
    w->delegate = [[MAC_WM_WindowDelegate alloc] init];
    w->window.delegate = w->delegate;
  }

  WM_Window handle = {(U64)w};
  return handle;
}

internal void
wm_window_close(WM_Window handle)
{
  MAC_WM_Window *w = mac_wm_window_from_handle(handle);
  if(w == 0) { return; }
  @autoreleasepool
  {
    w->window.delegate = nil;
    [w->window orderOut:nil];
    [w->window close];
    [w->delegate release];
    [w->view release];
    [w->window release];
  }
  for(MAC_WM_WindowClientArea *a = w->first_client_area, *next = 0; a != 0; a = next)
  {
    next = a->next;
    SLLStackPush(mac_wm_state->free_client_area, a);
  }
  DLLRemove(mac_wm_state->first_window, mac_wm_state->last_window, w);
  SLLStackPush(mac_wm_state->free_window, w);
}

internal void
wm_window_set_title(WM_Window handle, String8 title)
{
  MAC_WM_Window *w = mac_wm_window_from_handle(handle);
  if(w == 0) { return; }
  @autoreleasepool
  {
    w->window.title = [[[NSString alloc] initWithBytes:title.str length:title.size encoding:NSUTF8StringEncoding] autorelease];
  }
}

internal void
wm_window_first_paint(WM_Window handle)
{
  MAC_WM_Window *w = mac_wm_window_from_handle(handle);
  if(w == 0) { return; }
  if(w->no_activate)
  {
    // rad: script runs: show the window without taking focus from the user
    [w->window orderFront:nil];
  }
  else
  {
    [w->window makeKeyAndOrderFront:nil];
    [NSApp activateIgnoringOtherApps:YES];
  }
}

internal void
wm_window_focus(WM_Window handle)
{
  MAC_WM_Window *w = mac_wm_window_from_handle(handle);
  if(w == 0 || w->no_activate) { return; }
  [w->window makeKeyAndOrderFront:nil];
  [NSApp activateIgnoringOtherApps:YES];
}

internal B32
wm_window_is_focused(WM_Window handle)
{
  MAC_WM_Window *w = mac_wm_window_from_handle(handle);
  if(w == 0) { return 0; }
  if(wm_sim_is_enabled()) { return 1; }
  return !!w->window.isKeyWindow;
}

internal B32
wm_window_is_fullscreen(WM_Window handle)
{
  MAC_WM_Window *w = mac_wm_window_from_handle(handle);
  if(w == 0) { return 0; }
  return !!(w->window.styleMask & NSWindowStyleMaskFullScreen);
}

internal void
wm_window_set_fullscreen(WM_Window handle, B32 fullscreen)
{
  MAC_WM_Window *w = mac_wm_window_from_handle(handle);
  if(w == 0) { return; }
  if(!!fullscreen != wm_window_is_fullscreen(handle))
  {
    [w->window toggleFullScreen:nil];
  }
}

internal B32
wm_window_is_maximized(WM_Window handle)
{
  MAC_WM_Window *w = mac_wm_window_from_handle(handle);
  if(w == 0) { return 0; }
  return !!w->window.isZoomed;
}

internal void
wm_window_set_maximized(WM_Window handle, B32 maximized)
{
  MAC_WM_Window *w = mac_wm_window_from_handle(handle);
  if(w == 0) { return; }
  if(!!maximized != !!w->window.isZoomed)
  {
    [w->window zoom:nil];
  }
}

internal B32
wm_window_is_minimized(WM_Window handle)
{
  MAC_WM_Window *w = mac_wm_window_from_handle(handle);
  if(w == 0) { return 0; }
  return !!w->window.isMiniaturized;
}

internal void
wm_window_set_minimized(WM_Window handle, B32 minimized)
{
  MAC_WM_Window *w = mac_wm_window_from_handle(handle);
  if(w == 0) { return; }
  if(minimized) { [w->window miniaturize:nil]; }
  else          { [w->window deminiaturize:nil]; }
}

internal void
wm_window_bring_to_front(WM_Window handle)
{
  MAC_WM_Window *w = mac_wm_window_from_handle(handle);
  if(w == 0) { return; }
  [w->window orderFront:nil];
}

internal NSScreen *
mac_wm_screen_from_monitor(WM_Monitor monitor)
{
  NSScreen *result = nil;
  for(NSScreen *screen in NSScreen.screens)
  {
    NSNumber *number = screen.deviceDescription[@"NSScreenNumber"];
    if(number != nil && (U64)number.unsignedIntValue == monitor.u64[0])
    {
      result = screen;
      break;
    }
  }
  return result;
}

internal void
wm_window_set_monitor(WM_Window handle, WM_Monitor monitor)
{
  MAC_WM_Window *w = mac_wm_window_from_handle(handle);
  NSScreen *screen = mac_wm_screen_from_monitor(monitor);
  if(w == 0 || screen == nil || w->window.screen == screen) { return; }
  NSRect visible = screen.visibleFrame;
  NSRect frame = w->window.frame;
  frame.size.width  = Min(frame.size.width, visible.size.width);
  frame.size.height = Min(frame.size.height, visible.size.height);
  frame.origin.x = visible.origin.x + (visible.size.width - frame.size.width)/2;
  frame.origin.y = visible.origin.y + (visible.size.height - frame.size.height)/2;
  [w->window setFrame:frame display:NO];
}

internal void
wm_window_clear_custom_border_data(WM_Window handle)
{
  MAC_WM_Window *w = mac_wm_window_from_handle(handle);
  if(w == 0) { return; }
  for(MAC_WM_WindowClientArea *a = w->first_client_area, *next = 0; a != 0; a = next)
  {
    next = a->next;
    SLLStackPush(mac_wm_state->free_client_area, a);
  }
  w->first_client_area = w->last_client_area = 0;
  w->custom_title_bar_thickness = 0;
  w->custom_edge_thickness = 0;
}

internal void
wm_window_push_custom_title_bar(WM_Window handle, F32 thickness)
{
  MAC_WM_Window *w = mac_wm_window_from_handle(handle);
  if(w == 0) { return; }
  w->custom_title_bar_thickness = thickness;
}

internal void
wm_window_push_custom_edges(WM_Window handle, F32 thickness)
{
  MAC_WM_Window *w = mac_wm_window_from_handle(handle);
  if(w == 0) { return; }
  w->custom_edge_thickness = thickness;
}

internal void
wm_window_push_custom_title_bar_client_area(WM_Window handle, Rng2F32 rect)
{
  MAC_WM_Window *w = mac_wm_window_from_handle(handle);
  if(w == 0) { return; }
  MAC_WM_WindowClientArea *a = mac_wm_state->free_client_area;
  if(a != 0) { SLLStackPop(mac_wm_state->free_client_area); MemoryZeroStruct(a); }
  else       { a = push_array(mac_wm_state->arena, MAC_WM_WindowClientArea, 1); }
  a->rect = rect;
  SLLQueuePush(w->first_client_area, w->last_client_area, a);
}

internal Rng2F32
wm_rect_from_window(WM_Window handle)
{
  MAC_WM_Window *w = mac_wm_window_from_handle(handle);
  if(w == 0) { return r2f32p(0, 0, 0, 0); }
  return mac_wm_pixel_rect_from_screen_rect(w->window.frame);
}

internal Rng2F32
wm_client_rect_from_window(WM_Window handle)
{
  MAC_WM_Window *w = mac_wm_window_from_handle(handle);
  if(w == 0) { return r2f32p(0, 0, 0, 0); }
  NSRect backing = [w->view convertRectToBacking:w->view.bounds];
  return r2f32p(0, 0, (F32)backing.size.width, (F32)backing.size.height);
}

internal F32
wm_dpi_from_window(WM_Window handle)
{
  MAC_WM_Window *w = mac_wm_window_from_handle(handle);
  return 96.f*mac_wm_scale_from_window(w);
}

////////////////////////////////
//~ rad: @per_os_impl External Windows (Implemented Per-OS)

internal WM_ExtWindow
wm_focused_external_window(void)
{
  WM_ExtWindow result = {0};
  return result;
}

internal void
wm_focus_external_window(WM_ExtWindow ext_window)
{
}

////////////////////////////////
//~ rad: @per_os_impl Monitors (Implemented Per-OS)

internal WM_MonitorArray
wm_push_monitors_array(Arena *arena)
{
  WM_MonitorArray result = {0};
  @autoreleasepool
  {
    NSArray<NSScreen *> *screens = NSScreen.screens;
    result.count = screens.count;
    result.v = push_array(arena, WM_Monitor, result.count);
    for EachIndex(idx, result.count)
    {
      NSNumber *number = screens[idx].deviceDescription[@"NSScreenNumber"];
      result.v[idx].u64[0] = number.unsignedIntValue;
    }
  }
  return result;
}

internal WM_Monitor
wm_primary_monitor(void)
{
  WM_Monitor result = {CGMainDisplayID()};
  return result;
}

internal WM_Monitor
wm_monitor_from_window(WM_Window handle)
{
  WM_Monitor result = wm_primary_monitor();
  MAC_WM_Window *w = mac_wm_window_from_handle(handle);
  if(w != 0 && w->window.screen != nil)
  {
    NSNumber *number = w->window.screen.deviceDescription[@"NSScreenNumber"];
    result.u64[0] = number.unsignedIntValue;
  }
  return result;
}

internal String8
wm_name_from_monitor(Arena *arena, WM_Monitor monitor)
{
  String8 result = {0};
  @autoreleasepool
  {
    NSScreen *screen = mac_wm_screen_from_monitor(monitor);
    if(screen != nil)
    {
      result = str8_copy(arena, str8_cstring((char *)screen.localizedName.UTF8String));
    }
  }
  return result;
}

internal Vec2F32
wm_dim_from_monitor(WM_Monitor monitor)
{
  Vec2F32 result = {0};
  NSScreen *screen = mac_wm_screen_from_monitor(monitor);
  if(screen != nil)
  {
    F32 scale = (F32)screen.backingScaleFactor;
    result = v2f32((F32)screen.frame.size.width*scale, (F32)screen.frame.size.height*scale);
  }
  return result;
}

internal F32
wm_dpi_from_monitor(WM_Monitor monitor)
{
  NSScreen *screen = mac_wm_screen_from_monitor(monitor);
  return 96.f*(screen != nil ? (F32)screen.backingScaleFactor : 1.f);
}

////////////////////////////////
//~ rad: @per_os_impl Events (Implemented Per-OS)

internal void
wm_send_wakeup_event(void)
{
  // rad: any thread; posting an event is thread-safe
  @autoreleasepool
  {
    NSEvent *event = [NSEvent otherEventWithType:NSEventTypeApplicationDefined location:NSMakePoint(0, 0) modifierFlags:0 timestamp:0 windowNumber:0 context:nil subtype:0 data1:0 data2:0];
    [NSApp postEvent:event atStart:NO];
  }
}

internal WM_EventList
wm_get_events(Arena *arena, B32 wait)
{
  WM_EventList result = {0};

  //- rad: called from a frame drawn during live resize -> don't pump (AppKit's
  // resize loop owns the queue); just deliver simulated input
  if(mac_wm_state->get_events_depth > 0 || mac_wm_state->in_live_resize)
  {
    wm_sim_take_events(arena, &result);
    return result;
  }

  mac_wm_state->get_events_depth += 1;
  mac_wm_event_arena = arena;
  MemoryZeroStruct(&mac_wm_event_list);
  if(wm_sim_events_pending())
  {
    wait = 0;
  }
  @autoreleasepool
  {
    for(B32 first = 1;; first = 0)
    {
      NSDate *until = (first && wait) ? [NSDate distantFuture] : [NSDate distantPast];
      NSEvent *event = [NSApp nextEventMatchingMask:NSEventMaskAny untilDate:until inMode:NSDefaultRunLoopMode dequeue:YES];
      if(event == nil)
      {
        break;
      }
      if(mac_wm_handle_event(event))
      {
        [NSApp sendEvent:event];
      }
    }
  }
  result = mac_wm_event_list;
  mac_wm_event_arena = 0;
  MemoryZeroStruct(&mac_wm_event_list);
  mac_wm_state->get_events_depth -= 1;
  wm_sim_take_events(arena, &result);

  //- rad: the refresh rate is the one of the screen showing the key window
  // (else the first window), which can change when a window moves to
  // another screen (a 60 Hz monitor next to a 120 Hz laptop panel). rad
  // derives its animation step from it; its frame-time guess alone is wrong
  // when frames do not wait for the display (the Metal renderer skips
  // unchanged ones)
  {
    NSWindow *window = NSApp.keyWindow;
    if(window == nil && mac_wm_state->first_window != 0)
    {
      window = mac_wm_state->first_window->window;
    }
    if(@available(macOS 12.0, *))
    {
      NSInteger fps = window.screen.maximumFramesPerSecond;
      if(fps > 0)
      {
        mac_wm_state->gfx_info.default_refresh_rate = (F32)fps;
      }
    }
  }
  return result;
}

internal WM_Modifiers
wm_get_modifiers(void)
{
  if(wm_sim_is_enabled()) { return wm_sim_modifiers(); }
  return mac_wm_modifiers_from_flags(NSEvent.modifierFlags);
}

internal B32
wm_key_is_down(WM_Key key)
{
  if(wm_sim_is_enabled()) { return wm_sim_key_is_down(key); }
  return !!(mac_wm_state->key_down_state[key/64] & (1ull<<(key%64)));
}

internal Vec2F32
wm_mouse_from_window(WM_Window handle)
{
  MAC_WM_Window *w = mac_wm_window_from_handle(handle);
  if(w == 0) { return v2f32(0, 0); }
  if(wm_sim_is_enabled()) { return wm_sim_mouse(); }
  return mac_wm_pixel_pos_from_point(w, w->window.mouseLocationOutsideOfEventStream);
}

////////////////////////////////
//~ rad: @per_os_impl Cursors (Implemented Per-OS)

internal void
wm_set_cursor(WM_Cursor cursor)
{
  mac_wm_state->last_set_cursor = cursor;
  if(wm_sim_is_enabled()) { return; }
  NSCursor *ns_cursor = NSCursor.arrowCursor;
  switch(cursor)
  {
    default:{}break;
    case WM_Cursor_IBar:      {ns_cursor = NSCursor.IBeamCursor;}break;
    case WM_Cursor_LeftRight: {ns_cursor = NSCursor.resizeLeftRightCursor;}break;
    case WM_Cursor_UpDown:    {ns_cursor = NSCursor.resizeUpDownCursor;}break;
    case WM_Cursor_HandPoint: {ns_cursor = NSCursor.pointingHandCursor;}break;
    case WM_Cursor_Disabled:  {ns_cursor = NSCursor.operationNotAllowedCursor;}break;
    case WM_Cursor_DownRight:
    case WM_Cursor_UpRight:
    case WM_Cursor_UpDownLeftRight:
    {
      if(@available(macOS 15.0, *))
      {
        NSCursorFrameResizePosition position = (cursor == WM_Cursor_DownRight ? NSCursorFrameResizePositionBottomRight :
                                                cursor == WM_Cursor_UpRight   ? NSCursorFrameResizePositionTopRight :
                                                NSCursorFrameResizePositionBottomRight);
        ns_cursor = [NSCursor frameResizeCursorFromPosition:position inDirections:NSCursorFrameResizeDirectionsAll];
      }
      else
      {
        ns_cursor = NSCursor.crosshairCursor;
      }
    }break;
  }
  [ns_cursor set];
}
