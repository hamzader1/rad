// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

////////////////////////////////
//~ rad: Character Widths
//
// NOTE(rad): wcwidth, roughly (Unicode East Asian Wide / Fullwidth, emoji
// presentation; combining marks and zero-width characters take no column:
// they are dropped, there are no grapheme clusters)

internal S32
term_codepoint_width(U32 c)
{
  S32 result = 1;
  if((0x0300 <= c && c <= 0x036F) || (0x0483 <= c && c <= 0x0489) || (0x0591 <= c && c <= 0x05BD) ||
     (0x0610 <= c && c <= 0x061A) || (0x064B <= c && c <= 0x065F) || (0x1AB0 <= c && c <= 0x1AFF) ||
     (0x1DC0 <= c && c <= 0x1DFF) || (0x200B <= c && c <= 0x200F) || (0x2028 <= c && c <= 0x202E) ||
     (0x2060 <= c && c <= 0x2064) || (0x20D0 <= c && c <= 0x20FF) || (0xFE00 <= c && c <= 0xFE0F) ||
     (0xFE20 <= c && c <= 0xFE2F) || c == 0xFEFF || (0xE0100 <= c && c <= 0xE01EF))
  {
    result = 0;
  }
  else if((0x1100 <= c && c <= 0x115F) || c == 0x2329 || c == 0x232A || (0x231A <= c && c <= 0x231B) ||
          (0x23E9 <= c && c <= 0x23EC) || c == 0x23F0 || c == 0x23F3 || (0x25FD <= c && c <= 0x25FE) ||
          (0x2614 <= c && c <= 0x2615) || (0x2648 <= c && c <= 0x2653) || c == 0x267F || c == 0x2693 ||
          c == 0x26A1 || (0x26AA <= c && c <= 0x26AB) || (0x26BD <= c && c <= 0x26BE) || (0x26C4 <= c && c <= 0x26C5) ||
          c == 0x26CE || c == 0x26D4 || c == 0x26EA || (0x26F2 <= c && c <= 0x26F3) || c == 0x26F5 || c == 0x26FA ||
          c == 0x26FD || c == 0x2705 || (0x270A <= c && c <= 0x270B) || c == 0x2728 || c == 0x274C || c == 0x274E ||
          (0x2753 <= c && c <= 0x2755) || c == 0x2757 || (0x2795 <= c && c <= 0x2797) || c == 0x27B0 || c == 0x27BF ||
          (0x2B1B <= c && c <= 0x2B1C) || c == 0x2B50 || c == 0x2B55 || (0x2E80 <= c && c <= 0x303E) ||
          (0x3041 <= c && c <= 0x33FF) || (0x3400 <= c && c <= 0x4DBF) || (0x4E00 <= c && c <= 0x9FFF) ||
          (0xA000 <= c && c <= 0xA4CF) || (0xA960 <= c && c <= 0xA97F) || (0xAC00 <= c && c <= 0xD7A3) ||
          (0xF900 <= c && c <= 0xFAFF) || (0xFE10 <= c && c <= 0xFE19) || (0xFE30 <= c && c <= 0xFE6F) ||
          (0xFF00 <= c && c <= 0xFF60) || (0xFFE0 <= c && c <= 0xFFE6) || (0x16FE0 <= c && c <= 0x18AFF) ||
          (0x1B000 <= c && c <= 0x1B2FF) || c == 0x1F004 || c == 0x1F0CF || c == 0x1F18E ||
          (0x1F191 <= c && c <= 0x1F19A) || (0x1F200 <= c && c <= 0x1F64F) || (0x1F680 <= c && c <= 0x1F6FF) ||
          (0x1F7E0 <= c && c <= 0x1F7EB) || (0x1F90C <= c && c <= 0x1F9FF) || (0x1FA70 <= c && c <= 0x1FAFF) ||
          (0x20000 <= c && c <= 0x2FFFD) || (0x30000 <= c && c <= 0x3FFFD))
  {
    result = 2;
  }
  return result;
}

////////////////////////////////
//~ rad: DEC Special Graphics (ESC ( 0: line drawing for 0x60-0x7E)

read_only global U32 term_line_drawing_table[] =
{
  0x25C6, 0x2592, 0x2409, 0x240C, 0x240D, 0x240A, 0x00B0, 0x00B1,
  0x2424, 0x240B, 0x2518, 0x2510, 0x250C, 0x2514, 0x253C, 0x23BA,
  0x23BB, 0x2500, 0x23BC, 0x23BD, 0x251C, 0x2524, 0x2534, 0x252C,
  0x2502, 0x2264, 0x2265, 0x03C0, 0x2260, 0x00A3, 0x00B7,
};

////////////////////////////////
//~ rad: Lines

internal TERM_Line *
term_lines_alloc(Arena *arena, S32 cols, S32 rows)
{
  TERM_Line *lines = push_array(arena, TERM_Line, rows);
  TERM_Cell *cells = push_array(arena, TERM_Cell, (U64)cols*(U64)rows);
  for(S32 y = 0; y < rows; y += 1)
  {
    lines[y].cells = cells + (U64)y*(U64)cols;
  }
  return lines;
}

internal TERM_Line *
term_lines(TERM_State *t)
{
  TERM_Line *result = (t->alt_active ? t->alt_lines : t->main_lines);
  return result;
}

internal TERM_Cell
term_blank_cell(TERM_State *t)
{
  // NOTE(rad): erasing keeps the current background (xterm's BCE)
  TERM_Cell result = {0};
  result.bg = t->cursor.bg;
  return result;
}

internal void
term_cells_fill(TERM_Cell *cells, S32 count, TERM_Cell cell)
{
  for(S32 x = 0; x < count; x += 1)
  {
    cells[x] = cell;
  }
}

internal void
term_line_split_wide(TERM_State *t, TERM_Line *line, S32 x0, S32 x1)
{
  // NOTE(rad): about to overwrite columns [x0, x1): a two-column character
  // cut in half there becomes blanks
  TERM_Cell blank = term_blank_cell(t);
  if(0 < x0 && x0 < t->cols && (line->cells[x0].flags & TERM_CellFlag_WideTail))
  {
    line->cells[x0-1] = blank;
    line->cells[x0] = blank;
  }
  if(0 < x1 && x1 < t->cols && (line->cells[x1].flags & TERM_CellFlag_WideTail))
  {
    line->cells[x1] = blank;
  }
}

internal void
term_erase(TERM_State *t, S32 y, S32 x0, S32 x1)
{
  x0 = Clamp(0, x0, t->cols);
  x1 = Clamp(0, x1, t->cols);
  if(0 <= y && y < t->rows && x0 < x1)
  {
    TERM_Line *line = &term_lines(t)[y];
    term_line_split_wide(t, line, x0, x1);
    term_cells_fill(line->cells + x0, x1 - x0, term_blank_cell(t));
    if(x1 == t->cols)
    {
      line->wrapped = 0;
    }
  }
}

////////////////////////////////
//~ rad: Scrollback

internal void
term_scrollback_widen(TERM_State *t, S32 cols)
{
  // NOTE(rad): a new ring `cols` wide, the old lines copied oldest first
  Arena *arena = arena_alloc();
  TERM_Cell *cells = push_array_no_zero(arena, TERM_Cell, t->scrollback_cap*(U64)cols);
  B8 *wrapped = push_array(arena, B8, t->scrollback_cap);
  for(U64 idx = 0; idx < t->scrollback_count; idx += 1)
  {
    U64 old_slot = (t->scrollback_first + idx) % t->scrollback_cap;
    S32 n = Min(cols, t->scrollback_cols);
    MemoryCopy(cells + idx*cols, t->scrollback_cells + old_slot*t->scrollback_cols, sizeof(TERM_Cell)*n);
    MemoryZero(cells + idx*cols + n, sizeof(TERM_Cell)*(cols - n));
    wrapped[idx] = t->scrollback_wrapped[old_slot];
  }
  if(t->scrollback_arena != 0)
  {
    arena_release(t->scrollback_arena);
  }
  t->scrollback_arena = arena;
  t->scrollback_cells = cells;
  t->scrollback_wrapped = wrapped;
  t->scrollback_cols = cols;
  t->scrollback_first = 0;
}

internal void
term_scrollback_push(TERM_State *t, TERM_Line *line)
{
  if(t->scrollback_cap != 0)
  {
    if(t->scrollback_cols < t->cols)
    {
      term_scrollback_widen(t, t->cols);
    }
    if(t->scrollback_count == t->scrollback_cap)
    {
      t->scrollback_first = (t->scrollback_first + 1) % t->scrollback_cap;
      t->scrollback_count -= 1;
    }
    U64 slot = (t->scrollback_first + t->scrollback_count) % t->scrollback_cap;
    TERM_Cell *dst = t->scrollback_cells + slot*t->scrollback_cols;
    S32 n = Min(t->cols, t->scrollback_cols);
    MemoryCopy(dst, line->cells, sizeof(TERM_Cell)*n);
    MemoryZero(dst + n, sizeof(TERM_Cell)*(t->scrollback_cols - n));
    t->scrollback_wrapped[slot] = (B8)!!line->wrapped;
    t->scrollback_count += 1;
    t->scrollback_pushed += 1;
  }
}

internal void
term_clear_scrollback(TERM_State *t)
{
  t->scrollback_first = 0;
  t->scrollback_count = 0;
  t->version += 1;
}

////////////////////////////////
//~ rad: Scrolling, Cursor

internal void
term_scroll_up(TERM_State *t, S32 top, S32 bottom, S32 n, B32 to_scrollback)
{
  // NOTE(rad): lines [top, bottom] move up n; n blank lines at the bottom
  TERM_Line *lines = term_lines(t);
  n = (top <= bottom ? Clamp(0, n, bottom - top + 1) : 0);
  if(n != 0)
  {
    Temp scratch = scratch_begin(0, 0);
    if(to_scrollback)
    {
      for(S32 idx = 0; idx < n; idx += 1)
      {
        term_scrollback_push(t, &lines[top + idx]);
      }
    }
    TERM_Line *saved = push_array_no_zero(scratch.arena, TERM_Line, n);
    MemoryCopy(saved, lines + top, sizeof(TERM_Line)*n);
    MemoryCopy(lines + top, lines + top + n, sizeof(TERM_Line)*(bottom - top + 1 - n));
    MemoryCopy(lines + bottom - n + 1, saved, sizeof(TERM_Line)*n);
    TERM_Cell blank = term_blank_cell(t);
    for(S32 y = bottom - n + 1; y <= bottom; y += 1)
    {
      term_cells_fill(lines[y].cells, t->cols, blank);
      lines[y].wrapped = 0;
    }
    scratch_end(scratch);
  }
}

internal void
term_scroll_down(TERM_State *t, S32 top, S32 bottom, S32 n)
{
  // NOTE(rad): lines [top, bottom] move down n; n blank lines at the top
  TERM_Line *lines = term_lines(t);
  n = (top <= bottom ? Clamp(0, n, bottom - top + 1) : 0);
  if(n != 0)
  {
    Temp scratch = scratch_begin(0, 0);
    TERM_Line *saved = push_array_no_zero(scratch.arena, TERM_Line, n);
    MemoryCopy(saved, lines + bottom - n + 1, sizeof(TERM_Line)*n);
    MemoryCopy(lines + top + n, lines + top, sizeof(TERM_Line)*(bottom - top + 1 - n));
    MemoryCopy(lines + top, saved, sizeof(TERM_Line)*n);
    TERM_Cell blank = term_blank_cell(t);
    for(S32 y = top; y < top + n; y += 1)
    {
      term_cells_fill(lines[y].cells, t->cols, blank);
      lines[y].wrapped = 0;
    }
    scratch_end(scratch);
  }
}

internal void
term_index(TERM_State *t)
{
  // NOTE(rad): down a line, scrolling at the bottom margin (into the
  // scrollback when the margin is the screen's top, on the main screen)
  t->cursor.pending_wrap = 0;
  if(t->cursor.y == t->scroll_bottom)
  {
    term_scroll_up(t, t->scroll_top, t->scroll_bottom, 1, (t->scroll_top == 0 && !t->alt_active));
  }
  else if(t->cursor.y < t->rows - 1)
  {
    t->cursor.y += 1;
  }
}

internal void
term_reverse_index(TERM_State *t)
{
  t->cursor.pending_wrap = 0;
  if(t->cursor.y == t->scroll_top)
  {
    term_scroll_down(t, t->scroll_top, t->scroll_bottom, 1);
  }
  else if(t->cursor.y > 0)
  {
    t->cursor.y -= 1;
  }
}

internal void
term_cursor_goto(TERM_State *t, S32 x, S32 y)
{
  // NOTE(rad): rows count from the top margin in origin mode (and stay
  // inside the margins)
  TERM_Cursor *c = &t->cursor;
  if(c->origin_mode)
  {
    c->y = Clamp(t->scroll_top, y + t->scroll_top, t->scroll_bottom);
  }
  else
  {
    c->y = Clamp(0, y, t->rows - 1);
  }
  c->x = Clamp(0, x, t->cols - 1);
  c->pending_wrap = 0;
}

internal void
term_save_cursor(TERM_State *t)
{
  t->saved_cursor[!!t->alt_active] = t->cursor;
}

internal void
term_restore_cursor(TERM_State *t)
{
  t->cursor = t->saved_cursor[!!t->alt_active];
  t->cursor.x = Clamp(0, t->cursor.x, t->cols - 1);
  t->cursor.y = Clamp(0, t->cursor.y, t->rows - 1);
}

internal void
term_clear_screen(TERM_State *t)
{
  for(S32 y = 0; y < t->rows; y += 1)
  {
    term_erase(t, y, 0, t->cols);
  }
}

internal void
term_set_alt(TERM_State *t, B32 on)
{
  t->alt_active = !!on;
  t->scroll_top = 0;
  t->scroll_bottom = t->rows - 1;
}

internal void
term_insert_blanks(TERM_State *t, S32 n)
{
  // NOTE(rad): ICH: the rest of the line moves right (off the end)
  TERM_Cursor *c = &t->cursor;
  TERM_Line *line = &term_lines(t)[c->y];
  n = Clamp(0, n, t->cols - c->x);
  term_line_split_wide(t, line, c->x, c->x);
  MemoryCopy(line->cells + c->x + n, line->cells + c->x, sizeof(TERM_Cell)*(t->cols - c->x - n));
  term_cells_fill(line->cells + c->x, n, term_blank_cell(t));
  if(line->cells[t->cols-1].flags & TERM_CellFlag_Wide)
  {
    line->cells[t->cols-1] = term_blank_cell(t);
  }
  c->pending_wrap = 0;
}

internal void
term_delete_chars(TERM_State *t, S32 n)
{
  // NOTE(rad): DCH: the rest of the line moves left, blanks at the end
  TERM_Cursor *c = &t->cursor;
  TERM_Line *line = &term_lines(t)[c->y];
  n = Clamp(0, n, t->cols - c->x);
  term_line_split_wide(t, line, c->x, c->x + n);
  MemoryCopy(line->cells + c->x, line->cells + c->x + n, sizeof(TERM_Cell)*(t->cols - c->x - n));
  term_cells_fill(line->cells + t->cols - n, n, term_blank_cell(t));
  c->pending_wrap = 0;
}

internal void
term_reset(TERM_State *t)
{
  // NOTE(rad): RIS: everything but the scrollback
  t->alt_active = 1;
  MemoryZeroStruct(&t->cursor);
  term_clear_screen(t);
  t->alt_active = 0;
  term_clear_screen(t);
  MemoryZeroArray(t->saved_cursor);
  t->scroll_top = 0;
  t->scroll_bottom = t->rows - 1;
  t->autowrap = 1;
  t->insert_mode = 0;
  t->app_cursor_keys = 0;
  t->app_keypad = 0;
  t->cursor_visible = 1;
  t->bracketed_paste = 0;
  t->cursor_style = 0;
  t->title_size = 0;
}

////////////////////////////////
//~ rad: Printing

internal void
term_print(TERM_State *t, U32 codepoint)
{
  TERM_Cursor *c = &t->cursor;
  if(c->line_drawing && 0x60 <= codepoint && codepoint <= 0x7E)
  {
    codepoint = term_line_drawing_table[codepoint - 0x60];
  }
  S32 width = term_codepoint_width(codepoint);
  if(width == 0 || (width == 2 && t->cols < 2))
  {
    return;
  }

  //- rad: wrap: the previous character filled the last column, or a two-column one does not fit
  if((c->pending_wrap && t->autowrap) || (width == 2 && c->x == t->cols - 1 && t->autowrap))
  {
    term_lines(t)[c->y].wrapped = 1;
    term_index(t);
    c->x = 0;
  }
  else if(width == 2 && c->x == t->cols - 1)
  {
    c->x = t->cols - 2;
  }
  c->pending_wrap = 0;

  //- rad: write the cell(s)
  if(t->insert_mode)
  {
    term_insert_blanks(t, width);
  }
  TERM_Line *line = &term_lines(t)[c->y];
  term_line_split_wide(t, line, c->x, c->x + width);
  TERM_Cell cell = {codepoint, c->fg, c->bg, c->flags};
  if(width == 2)
  {
    cell.flags |= TERM_CellFlag_Wide;
    TERM_Cell tail = {0, c->fg, c->bg, (TERM_CellFlags)(c->flags | TERM_CellFlag_WideTail)};
    line->cells[c->x+1] = tail;
  }
  line->cells[c->x] = cell;
  t->last_codepoint = codepoint;

  //- rad: advance (at the last column: stay, the next character wraps)
  c->x += width;
  if(c->x >= t->cols)
  {
    c->x = t->cols - 1;
    c->pending_wrap = 1;
  }
}

////////////////////////////////
//~ rad: Control Functions

internal U32
term_param(TERM_State *t, U32 idx, U32 default_value)
{
  // NOTE(rad): a missing or zero parameter is the default
  U32 result = ((idx < t->param_count && t->params[idx] != 0) ? t->params[idx] : default_value);
  return result;
}

internal void
term_reply(TERM_State *t, String8 string)
{
  str8_list_push(t->reply_arena, &t->replies, push_str8_copy(t->reply_arena, string));
}

internal void
term_execute(TERM_State *t, U8 byte)
{
  TERM_Cursor *c = &t->cursor;
  switch(byte)
  {
    default:{}break;
    case 0x07: {t->bell_count += 1;}break;
    case 0x08:
    {
      c->pending_wrap = 0;
      c->x = Max(0, c->x - 1);
    }break;
    case 0x09:
    {
      c->x = Min(t->cols - 1, (c->x/8 + 1)*8);
      c->pending_wrap = 0;
    }break;
    case 0x0A: case 0x0B: case 0x0C: {term_index(t);}break;
    case 0x0D:
    {
      c->x = 0;
      c->pending_wrap = 0;
    }break;
  }
}

internal void
term_esc_dispatch(TERM_State *t, U8 final)
{
  TERM_Cursor *c = &t->cursor;
  if(t->intermediate == 0) switch(final)
  {
    default:{}break;
    case '7': {term_save_cursor(t);}break;
    case '8': {term_restore_cursor(t);}break;
    case 'D': {term_index(t);}break;
    case 'E': {term_index(t); c->x = 0;}break;
    case 'M': {term_reverse_index(t);}break;
    case 'c': {term_reset(t);}break;
    case '=': {t->app_keypad = 1;}break;
    case '>': {t->app_keypad = 0;}break;
    case 'Z': {term_reply(t, str8_lit("\x1b[?1;2c"));}break;
  }
  else if(t->intermediate == '(')
  {
    c->line_drawing = (final == '0');
  }
}

internal void
term_sgr(TERM_State *t)
{
  TERM_Cursor *c = &t->cursor;
  if(t->param_count == 0)
  {
    c->fg = c->bg = 0;
    c->flags = 0;
  }
  for(U32 i = 0; i < t->param_count; i += 1)
  {
    U32 p = t->params[i];
    U32 sub_count = 0;
    for(;i + 1 + sub_count < t->param_count && (t->param_colon_mask & (1u << (i + 1 + sub_count))); sub_count += 1){}
    U32 skip = sub_count;
    switch(p)
    {
      default:
      {
        if(30 <= p && p <= 37)        { c->fg = TERM_ColorKind_Index | (p - 30); }
        else if(40 <= p && p <= 47)   { c->bg = TERM_ColorKind_Index | (p - 40); }
        else if(90 <= p && p <= 97)   { c->fg = TERM_ColorKind_Index | (p - 90 + 8); }
        else if(100 <= p && p <= 107) { c->bg = TERM_ColorKind_Index | (p - 100 + 8); }
      }break;
      case 0:  {c->fg = c->bg = 0; c->flags = 0;}break;
      case 1:  {c->flags |= TERM_CellFlag_Bold;}break;
      case 2:  {c->flags |= TERM_CellFlag_Faint;}break;
      case 3:  {c->flags |= TERM_CellFlag_Italic;}break;
      case 4:
      {
        if(sub_count != 0 && t->params[i+1] == 0) { c->flags &= ~TERM_CellFlag_Underline; }
        else                                      { c->flags |= TERM_CellFlag_Underline; }
      }break;
      case 7:  {c->flags |= TERM_CellFlag_Inverse;}break;
      case 8:  {c->flags |= TERM_CellFlag_Hidden;}break;
      case 9:  {c->flags |= TERM_CellFlag_Strike;}break;
      case 21: {c->flags |= TERM_CellFlag_Underline;}break;
      case 22: {c->flags &= ~(TERM_CellFlag_Bold|TERM_CellFlag_Faint);}break;
      case 23: {c->flags &= ~TERM_CellFlag_Italic;}break;
      case 24: {c->flags &= ~TERM_CellFlag_Underline;}break;
      case 27: {c->flags &= ~TERM_CellFlag_Inverse;}break;
      case 28: {c->flags &= ~TERM_CellFlag_Hidden;}break;
      case 29: {c->flags &= ~TERM_CellFlag_Strike;}break;
      case 39: {c->fg = 0;}break;
      case 49: {c->bg = 0;}break;
      case 38: case 48: case 58:
      {
        // rad: 38;5;n / 38;2;r;g;b, or with ':' (38:5:n, 38:2:[space]:r:g:b)
        TERM_Color color = 0;
        B32 has_color = 0;
        U32 *s = &t->params[i+1];
        U32 s_count = (sub_count != 0 ? sub_count : t->param_count - i - 1);
        if(s_count >= 2 && s[0] == 5)
        {
          color = TERM_ColorKind_Index | (s[1] & 0xff);
          has_color = 1;
          skip = (sub_count != 0 ? sub_count : 2);
        }
        else if(s_count >= 4 && s[0] == 2)
        {
          U32 *rgb = (sub_count >= 5 ? s + 2 : s + 1);
          color = TERM_ColorKind_RGB | (Min(rgb[0], 255) << 16) | (Min(rgb[1], 255) << 8) | Min(rgb[2], 255);
          has_color = 1;
          skip = (sub_count != 0 ? sub_count : 4);
        }
        else
        {
          skip = s_count;
        }
        if(has_color && p == 38) { c->fg = color; }
        if(has_color && p == 48) { c->bg = color; }
      }break;
    }
    i += skip;
  }
}

internal void
term_modes_set(TERM_State *t, B32 on)
{
  TERM_Cursor *c = &t->cursor;
  for(U32 idx = 0; idx < t->param_count; idx += 1)
  {
    U32 mode = t->params[idx];
    if(t->private_marker == 0)
    {
      if(mode == 4) { t->insert_mode = on; }
      continue;
    }
    switch(mode)
    {
      default:{}break;
      case 1:  {t->app_cursor_keys = on;}break;
      case 6:  {c->origin_mode = on; term_cursor_goto(t, 0, 0);}break;
      case 7:  {t->autowrap = on; c->pending_wrap = (on ? c->pending_wrap : 0);}break;
      case 25: {t->cursor_visible = on;}break;
      case 47:
      case 1047:
      {
        if(!on && mode == 1047 && t->alt_active)
        {
          term_clear_screen(t);
        }
        term_set_alt(t, on);
      }break;
      case 1048:
      {
        if(on) { term_save_cursor(t); } else { term_restore_cursor(t); }
      }break;
      case 1049:
      {
        if(on && !t->alt_active)
        {
          term_save_cursor(t);
          term_set_alt(t, 1);
          term_clear_screen(t);
        }
        else if(!on && t->alt_active)
        {
          term_set_alt(t, 0);
          term_restore_cursor(t);
        }
      }break;
      case 2004: {t->bracketed_paste = on;}break;
    }
  }
}

internal void
term_csi_dispatch(TERM_State *t, U8 final)
{
  Temp scratch = scratch_begin(0, 0);
  TERM_Cursor *c = &t->cursor;
  U8 pm = t->private_marker;
  U8 im = t->intermediate;
  if(pm == 0 && im == 0) switch(final)
  {
    default:{}break;
    case '@': {term_insert_blanks(t, (S32)term_param(t, 0, 1));}break;
    case 'A':
    {
      S32 top = (c->y >= t->scroll_top ? t->scroll_top : 0);
      c->y = Max(top, c->y - (S32)term_param(t, 0, 1));
      c->pending_wrap = 0;
    }break;
    case 'B': case 'e':
    {
      S32 bottom = (c->y <= t->scroll_bottom ? t->scroll_bottom : t->rows - 1);
      c->y = Min(bottom, c->y + (S32)term_param(t, 0, 1));
      c->pending_wrap = 0;
    }break;
    case 'C': case 'a': {c->x = Min(t->cols - 1, c->x + (S32)term_param(t, 0, 1)); c->pending_wrap = 0;}break;
    case 'D': {c->x = Max(0, Min(c->x, t->cols - 1) - (S32)term_param(t, 0, 1)); c->pending_wrap = 0;}break;
    case 'E':
    {
      S32 bottom = (c->y <= t->scroll_bottom ? t->scroll_bottom : t->rows - 1);
      c->y = Min(bottom, c->y + (S32)term_param(t, 0, 1));
      c->x = 0;
      c->pending_wrap = 0;
    }break;
    case 'F':
    {
      S32 top = (c->y >= t->scroll_top ? t->scroll_top : 0);
      c->y = Max(top, c->y - (S32)term_param(t, 0, 1));
      c->x = 0;
      c->pending_wrap = 0;
    }break;
    case 'G': case '`': {c->x = Clamp(0, (S32)term_param(t, 0, 1) - 1, t->cols - 1); c->pending_wrap = 0;}break;
    case 'H': case 'f': {term_cursor_goto(t, (S32)term_param(t, 1, 1) - 1, (S32)term_param(t, 0, 1) - 1);}break;
    case 'I':
    {
      for(U32 n = term_param(t, 0, 1); n != 0 && c->x < t->cols - 1; n -= 1)
      {
        c->x = Min(t->cols - 1, (c->x/8 + 1)*8);
      }
      c->pending_wrap = 0;
    }break;
    case 'Z':
    {
      for(U32 n = term_param(t, 0, 1); n != 0 && c->x > 0; n -= 1)
      {
        c->x = ((c->x - 1)/8)*8;
      }
      c->pending_wrap = 0;
    }break;
    case 'J':
    {
      U32 mode = (t->param_count != 0 ? t->params[0] : 0);
      if(mode == 0)
      {
        term_erase(t, c->y, c->x, t->cols);
        for(S32 y = c->y + 1; y < t->rows; y += 1) { term_erase(t, y, 0, t->cols); }
      }
      else if(mode == 1)
      {
        for(S32 y = 0; y < c->y; y += 1) { term_erase(t, y, 0, t->cols); }
        term_erase(t, c->y, 0, c->x + 1);
      }
      else if(mode == 2)
      {
        term_clear_screen(t);
      }
      else if(mode == 3)
      {
        term_clear_scrollback(t);
      }
    }break;
    case 'K':
    {
      U32 mode = (t->param_count != 0 ? t->params[0] : 0);
      if(mode == 0)      { term_erase(t, c->y, c->x, t->cols); }
      else if(mode == 1) { term_erase(t, c->y, 0, c->x + 1); }
      else if(mode == 2) { term_erase(t, c->y, 0, t->cols); }
    }break;
    case 'L':
    if(t->scroll_top <= c->y && c->y <= t->scroll_bottom)
    {
      term_scroll_down(t, c->y, t->scroll_bottom, (S32)term_param(t, 0, 1));
      c->x = 0;
      c->pending_wrap = 0;
    }break;
    case 'M':
    if(t->scroll_top <= c->y && c->y <= t->scroll_bottom)
    {
      term_scroll_up(t, c->y, t->scroll_bottom, (S32)term_param(t, 0, 1), 0);
      c->x = 0;
      c->pending_wrap = 0;
    }break;
    case 'P': {term_delete_chars(t, (S32)term_param(t, 0, 1));}break;
    case 'S': {term_scroll_up(t, t->scroll_top, t->scroll_bottom, (S32)term_param(t, 0, 1), 0);}break;
    case 'T':
    if(t->param_count <= 1)
    {
      term_scroll_down(t, t->scroll_top, t->scroll_bottom, (S32)term_param(t, 0, 1));
    }break;
    case 'X': {term_erase(t, c->y, c->x, c->x + (S32)term_param(t, 0, 1)); c->pending_wrap = 0;}break;
    case 'b':
    if(t->last_codepoint != 0)
    {
      for(U32 n = Min(term_param(t, 0, 1), 65535); n != 0; n -= 1)
      {
        term_print(t, t->last_codepoint);
      }
    }break;
    case 'c':
    if(term_param(t, 0, 0) == 0)
    {
      term_reply(t, str8_lit("\x1b[?1;2c"));
    }break;
    case 'd': {term_cursor_goto(t, c->x, (S32)term_param(t, 0, 1) - 1);}break;
    case 'h': {term_modes_set(t, 1);}break;
    case 'l': {term_modes_set(t, 0);}break;
    case 'm': {term_sgr(t);}break;
    case 'n':
    {
      U32 what = term_param(t, 0, 0);
      if(what == 5)
      {
        term_reply(t, str8_lit("\x1b[0n"));
      }
      else if(what == 6)
      {
        S32 row = c->y + 1 - (c->origin_mode ? t->scroll_top : 0);
        term_reply(t, push_str8f(scratch.arena, "\x1b[%i;%iR", row, c->x + 1));
      }
    }break;
    case 'r':
    {
      S32 top = (S32)term_param(t, 0, 1) - 1;
      S32 bottom = Min((S32)term_param(t, 1, (U32)t->rows), t->rows) - 1;
      if(top < bottom)
      {
        t->scroll_top = top;
        t->scroll_bottom = bottom;
        term_cursor_goto(t, 0, 0);
      }
    }break;
    case 's': {term_save_cursor(t);}break;
    case 'u': {term_restore_cursor(t);}break;
    case 't':
    if(term_param(t, 0, 0) == 18)
    {
      term_reply(t, push_str8f(scratch.arena, "\x1b[8;%i;%it", t->rows, t->cols));
    }break;
  }
  else if(pm == '?' && im == 0) switch(final)
  {
    default:{}break;
    case 'h': {term_modes_set(t, 1);}break;
    case 'l': {term_modes_set(t, 0);}break;
    case 'n':
    if(term_param(t, 0, 0) == 6)
    {
      S32 row = c->y + 1 - (c->origin_mode ? t->scroll_top : 0);
      term_reply(t, push_str8f(scratch.arena, "\x1b[?%i;%iR", row, c->x + 1));
    }break;
  }
  else if(pm == '>' && im == 0 && final == 'c')
  {
    term_reply(t, str8_lit("\x1b[>0;10;1c"));
  }
  else if(pm == 0 && im == ' ' && final == 'q')
  {
    t->cursor_style = term_param(t, 0, 0);
  }
  else if(pm == 0 && im == '!' && final == 'p')
  {
    // rad: DECSTR (soft reset)
    t->insert_mode = 0;
    t->autowrap = 1;
    t->app_cursor_keys = 0;
    t->app_keypad = 0;
    t->cursor_visible = 1;
    t->scroll_top = 0;
    t->scroll_bottom = t->rows - 1;
    c->origin_mode = 0;
    c->fg = c->bg = 0;
    c->flags = 0;
    c->line_drawing = 0;
    MemoryZeroArray(t->saved_cursor);
  }
  else if(im == '$' && final == 'p')
  {
    // rad: DECRQM: 1 set, 2 reset, 0 not recognized
    U32 mode = term_param(t, 0, 0);
    U32 value = 0;
    if(pm == '?')
    {
      switch(mode)
      {
        default:{}break;
        case 1:    {value = t->app_cursor_keys ? 1 : 2;}break;
        case 6:    {value = c->origin_mode ? 1 : 2;}break;
        case 7:    {value = t->autowrap ? 1 : 2;}break;
        case 25:   {value = t->cursor_visible ? 1 : 2;}break;
        case 47: case 1047: case 1049: {value = t->alt_active ? 1 : 2;}break;
        case 2004: {value = t->bracketed_paste ? 1 : 2;}break;
      }
      term_reply(t, push_str8f(scratch.arena, "\x1b[?%u;%u$y", mode, value));
    }
    else
    {
      value = (mode == 4 ? (t->insert_mode ? 1 : 2) : 0);
      term_reply(t, push_str8f(scratch.arena, "\x1b[%u;%u$y", mode, value));
    }
  }
  scratch_end(scratch);
}

internal void
term_osc_dispatch(TERM_State *t)
{
  // NOTE(rad): OSC 0 / 2: the title; the others are dropped
  String8 osc = str8(t->osc_buffer, t->osc_size);
  U64 semicolon = str8_find_needle(osc, 0, str8_lit(";"), 0);
  String8 number = str8_prefix(osc, semicolon);
  if(semicolon < osc.size && (str8_match(number, str8_lit("0"), 0) || str8_match(number, str8_lit("2"), 0)))
  {
    String8 title = str8_skip(osc, semicolon + 1);
    t->title_size = Min(title.size, sizeof(t->title_buffer));
    MemoryCopy(t->title_buffer, title.str, t->title_size);
  }
  t->osc_size = 0;
}

////////////////////////////////
//~ rad: Allocation, Resizing

internal TERM_State *
term_alloc(S32 cols, S32 rows, U64 scrollback_cap)
{
  Arena *arena = arena_alloc();
  TERM_State *t = push_array(arena, TERM_State, 1);
  t->arena = arena;
  t->reply_arena = arena_alloc();
  t->screen_arena = arena_alloc();
  t->cols = Clamp(1, cols, 4096);
  t->rows = Clamp(1, rows, 4096);
  t->main_lines = term_lines_alloc(t->screen_arena, t->cols, t->rows);
  t->alt_lines = term_lines_alloc(t->screen_arena, t->cols, t->rows);
  t->scroll_top = 0;
  t->scroll_bottom = t->rows - 1;
  t->scrollback_cap = scrollback_cap;
  t->autowrap = 1;
  t->cursor_visible = 1;
  return t;
}

internal void
term_release(TERM_State *t)
{
  if(t != 0)
  {
    if(t->scrollback_arena != 0)
    {
      arena_release(t->scrollback_arena);
    }
    arena_release(t->screen_arena);
    arena_release(t->reply_arena);
    arena_release(t->arena);
  }
}

internal void
term_resize(TERM_State *t, S32 cols, S32 rows)
{
  // NOTE(rad): no reflow: lines are cut or padded. A shorter main screen
  // first sends the lines above the cursor's to the scrollback, so the
  // cursor's line stays.
  cols = Clamp(1, cols, 4096);
  rows = Clamp(1, rows, 4096);
  if(cols != t->cols || rows != t->rows)
  {
    Arena *arena = arena_alloc();
    TERM_Line *new_lines[2] = {term_lines_alloc(arena, cols, rows), term_lines_alloc(arena, cols, rows)};
    TERM_Line *old_lines[2] = {t->main_lines, t->alt_lines};
    TERM_Cursor *cursors[2] =
    {
      (t->alt_active ? &t->saved_cursor[0] : &t->cursor),
      (t->alt_active ? &t->cursor : &t->saved_cursor[1]),
    };
    for EachIndex(screen_idx, 2)
    {
      S32 shift = Max(0, cursors[screen_idx]->y - (rows - 1));
      if(screen_idx == 0)
      {
        for(S32 y = 0; y < shift; y += 1)
        {
          term_scrollback_push(t, &old_lines[0][y]);
        }
      }
      for(S32 y = 0; y < rows && y + shift < t->rows; y += 1)
      {
        TERM_Line *src = &old_lines[screen_idx][y + shift];
        TERM_Line *dst = &new_lines[screen_idx][y];
        MemoryCopy(dst->cells, src->cells, sizeof(TERM_Cell)*Min(cols, t->cols));
        dst->wrapped = (cols >= t->cols ? src->wrapped : 1);
        if(dst->cells[cols-1].flags & TERM_CellFlag_Wide)
        {
          MemoryZeroStruct(&dst->cells[cols-1]);
        }
      }
      cursors[screen_idx]->y = Clamp(0, cursors[screen_idx]->y - shift, rows - 1);
      if(cursors[screen_idx]->x >= cols)
      {
        cursors[screen_idx]->x = cols - 1;
        cursors[screen_idx]->pending_wrap = 0;
      }
    }
    for EachIndex(idx, ArrayCount(t->saved_cursor))
    {
      t->saved_cursor[idx].x = Clamp(0, t->saved_cursor[idx].x, cols - 1);
      t->saved_cursor[idx].y = Clamp(0, t->saved_cursor[idx].y, rows - 1);
    }
    arena_release(t->screen_arena);
    t->screen_arena = arena;
    t->main_lines = new_lines[0];
    t->alt_lines = new_lines[1];
    t->cols = cols;
    t->rows = rows;
    t->scroll_top = 0;
    t->scroll_bottom = rows - 1;
    t->version += 1;
  }
}

////////////////////////////////
//~ rad: Parsing

internal void
term_write(TERM_State *t, String8 data)
{
  for(U64 idx = 0; idx < data.size; idx += 1)
  {
    U8 b = data.str[idx];

    //- rad: strings: OSC until BEL or ST (ESC + backslash); DCS / SOS / PM /
    // APC until ST (dropped)
    if(t->parse_state == TERM_ParseState_Osc)
    {
      if(b == 0x07)                  { term_osc_dispatch(t); t->parse_state = TERM_ParseState_Ground; }
      else if(b == 0x1b)             { t->parse_state = TERM_ParseState_OscEscape; }
      else if(b == 0x18 || b == 0x1a) { t->osc_size = 0; t->parse_state = TERM_ParseState_Ground; }
      else if(t->osc_size < sizeof(t->osc_buffer)) { t->osc_buffer[t->osc_size] = b; t->osc_size += 1; }
      continue;
    }
    if(t->parse_state == TERM_ParseState_OscEscape)
    {
      term_osc_dispatch(t);
      t->parse_state = TERM_ParseState_Ground;
      if(b == '\\')
      {
        continue;
      }
      t->parse_state = TERM_ParseState_Escape;
      t->intermediate = 0;
    }
    if(t->parse_state == TERM_ParseState_String)
    {
      if(b == 0x1b)                                 { t->parse_state = TERM_ParseState_StringEscape; }
      else if(b == 0x07 || b == 0x18 || b == 0x1a) { t->parse_state = TERM_ParseState_Ground; }
      continue;
    }
    if(t->parse_state == TERM_ParseState_StringEscape)
    {
      t->parse_state = (b == '\\' ? TERM_ParseState_Ground : TERM_ParseState_String);
      continue;
    }

    //- rad: UTF-8 (in the ground state): a broken sequence prints U+FFFD
    if(t->utf8_remaining != 0)
    {
      if((b & 0xC0) == 0x80)
      {
        t->utf8_codepoint = (t->utf8_codepoint << 6) | (b & 0x3F);
        t->utf8_remaining -= 1;
        if(t->utf8_remaining == 0)
        {
          U32 cp = t->utf8_codepoint;
          term_print(t, (cp < 0x80 || cp > 0x10FFFF || (0xD800 <= cp && cp <= 0xDFFF)) ? 0xFFFD : cp);
        }
        continue;
      }
      t->utf8_remaining = 0;
      term_print(t, 0xFFFD);
    }

    //- rad: C0 controls (in every state but the strings)
    if(b < 0x20 || b == 0x7f)
    {
      if(b == 0x1b)
      {
        t->parse_state = TERM_ParseState_Escape;
        t->intermediate = 0;
      }
      else if(b == 0x18 || b == 0x1a)
      {
        t->parse_state = TERM_ParseState_Ground;
      }
      else if(b != 0x7f)
      {
        term_execute(t, b);
      }
      continue;
    }

    //- rad: states
    switch(t->parse_state)
    {
      default:{}break;
      case TERM_ParseState_Ground:
      {
        if(b < 0x80)                { term_print(t, b); }
        else if((b & 0xE0) == 0xC0) { t->utf8_codepoint = b & 0x1F; t->utf8_remaining = 1; }
        else if((b & 0xF0) == 0xE0) { t->utf8_codepoint = b & 0x0F; t->utf8_remaining = 2; }
        else if((b & 0xF8) == 0xF0) { t->utf8_codepoint = b & 0x07; t->utf8_remaining = 3; }
        else                        { term_print(t, 0xFFFD); }
      }break;
      case TERM_ParseState_Escape:
      {
        if(0x20 <= b && b <= 0x2F)
        {
          t->intermediate = b;
        }
        else if(t->intermediate == 0 && b == '[')
        {
          t->parse_state = TERM_ParseState_Csi;
          t->param_count = 0;
          t->param_colon_mask = 0;
          t->private_marker = 0;
          MemoryZeroArray(t->params);
        }
        else if(t->intermediate == 0 && b == ']')
        {
          t->parse_state = TERM_ParseState_Osc;
          t->osc_size = 0;
        }
        else if(t->intermediate == 0 && (b == 'P' || b == 'X' || b == '^' || b == '_'))
        {
          t->parse_state = TERM_ParseState_String;
        }
        else
        {
          term_esc_dispatch(t, b);
          t->parse_state = TERM_ParseState_Ground;
        }
      }break;
      case TERM_ParseState_Csi:
      {
        if('0' <= b && b <= '9')
        {
          if(t->param_count == 0)
          {
            t->param_count = 1;
          }
          U32 *p = &t->params[t->param_count-1];
          *p = Min(*p*10 + (b - '0'), 65535);
        }
        else if(b == ';' || b == ':')
        {
          if(t->param_count == 0)
          {
            t->param_count = 1;
          }
          if(t->param_count < TERM_PARAM_MAX)
          {
            t->params[t->param_count] = 0;
            if(b == ':')
            {
              t->param_colon_mask |= (1u << t->param_count);
            }
            t->param_count += 1;
          }
        }
        else if(0x3C <= b && b <= 0x3F)
        {
          if(t->param_count == 0 && t->private_marker == 0 && t->intermediate == 0)
          {
            t->private_marker = b;
          }
          else
          {
            t->parse_state = TERM_ParseState_CsiIgnore;
          }
        }
        else if(0x20 <= b && b <= 0x2F)
        {
          t->intermediate = b;
        }
        else if(0x40 <= b && b <= 0x7E)
        {
          term_csi_dispatch(t, b);
          t->parse_state = TERM_ParseState_Ground;
        }
        else
        {
          t->parse_state = TERM_ParseState_CsiIgnore;
        }
      }break;
      case TERM_ParseState_CsiIgnore:
      {
        if(0x40 <= b && b <= 0x7E)
        {
          t->parse_state = TERM_ParseState_Ground;
        }
      }break;
    }
  }
  t->version += 1;
}

internal String8
term_take_replies(Arena *arena, TERM_State *t)
{
  String8 result = str8_list_join(arena, &t->replies, 0);
  MemoryZeroStruct(&t->replies);
  arena_clear(t->reply_arena);
  return result;
}

////////////////////////////////
//~ rad: Lines Out

internal U64
term_line_count(TERM_State *t)
{
  U64 result = (t->alt_active ? 0 : t->scrollback_count) + (U64)t->rows;
  return result;
}

internal U64
term_line_base(TERM_State *t)
{
  U64 result = (t->alt_active ? 0 : t->scrollback_pushed - t->scrollback_count);
  return result;
}

internal TERM_Line
term_line_from_idx(TERM_State *t, U64 line_idx, S32 *cols_out)
{
  TERM_Line result = {0};
  S32 cols = 0;
  U64 scrollback_count = (t->alt_active ? 0 : t->scrollback_count);
  if(line_idx < scrollback_count)
  {
    U64 slot = (t->scrollback_first + line_idx) % t->scrollback_cap;
    result.cells = t->scrollback_cells + slot*t->scrollback_cols;
    result.wrapped = t->scrollback_wrapped[slot];
    cols = t->scrollback_cols;
  }
  else if(line_idx - scrollback_count < (U64)t->rows)
  {
    result = term_lines(t)[line_idx - scrollback_count];
    cols = t->cols;
  }
  if(cols_out != 0)
  {
    *cols_out = cols;
  }
  return result;
}

internal String8
term_string_from_cells(Arena *arena, TERM_Cell *cells, S32 x0, S32 x1, B32 trim)
{
  U8 *buffer = push_array_no_zero(arena, U8, (U64)Max(0, x1 - x0)*4 + 1);
  U64 size = 0;
  for(S32 x = x0; x < x1; x += 1)
  {
    if(!(cells[x].flags & TERM_CellFlag_WideTail))
    {
      size += utf8_encode(buffer + size, cells[x].codepoint != 0 ? cells[x].codepoint : ' ');
    }
  }
  for(;trim && size != 0 && buffer[size-1] == ' '; size -= 1){}
  String8 result = str8(buffer, size);
  return result;
}

internal String8
term_string_from_line(Arena *arena, TERM_State *t, U64 line_idx)
{
  S32 cols = 0;
  TERM_Line line = term_line_from_idx(t, line_idx, &cols);
  String8 result = (line.cells != 0 ? term_string_from_cells(arena, line.cells, 0, cols, 1) : str8_zero());
  return result;
}

internal String8
term_string_from_range(Arena *arena, TERM_State *t, U64 line0, S32 col0, U64 line1, S32 col1)
{
  // NOTE(rad): [line0:col0, line1:col1); wrapped lines join without a newline
  Temp scratch = scratch_begin(&arena, 1);
  String8List parts = {0};
  for(U64 line_idx = line0; line_idx <= line1; line_idx += 1)
  {
    S32 cols = 0;
    TERM_Line line = term_line_from_idx(t, line_idx, &cols);
    if(line.cells == 0)
    {
      break;
    }
    S32 x0 = (line_idx == line0 ? Clamp(0, col0, cols) : 0);
    S32 x1 = (line_idx == line1 ? Clamp(0, col1, cols) : cols);
    B32 joins_next = (line.wrapped && line_idx != line1);
    str8_list_push(scratch.arena, &parts, term_string_from_cells(scratch.arena, line.cells, x0, x1, !joins_next));
    if(line_idx != line1 && !joins_next)
    {
      str8_list_push(scratch.arena, &parts, str8_lit("\n"));
    }
  }
  String8 result = str8_list_join(arena, &parts, 0);
  scratch_end(scratch);
  return result;
}

internal String8
term_title(TERM_State *t)
{
  String8 result = str8(t->title_buffer, t->title_size);
  return result;
}
