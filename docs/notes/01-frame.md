# How one frame goes from OS events to pixels

All paths are under `src/`. Functions were checked in the code on 2026-11-02.
`raddbg` (the debugger frontend) and `rad` (the IDE frontend) follow the same shape.

```
base_entry_point.c  main_thread_base_entry_point()
  -> layer inits (wm_init, fp_init, r_init, fnt_init, rad_init/rd_init)
  -> entry_point()              rad/rad_main.c  (raddbg/raddbg_main.c)
       loop: update()           base/base_entry_point.c
               fnt_frame()      font_cache: evict/advance glyph cache
               frame()          -> rad_frame() / rd_frame()

rad_frame()                                   rad/rad_core.c
  1. wm_get_events(arena, wait)               window_manager (win32: GetMessage/PeekMessage)
       wait = no frames requested  -> blocks until input (idle = 0% CPU)
  2. open missing windows                     wm_window_open + r_window_equip
       (must be after the first wm_get_events, see "gotchas")
  3. pick target Hz from monitor refresh + frame-time history -> frame_dt
  4. consume events:
       WindowClose -> Exit command
       Alt press/release -> menu bar focus state machine
       key press matching a command binding -> rad_cmd(kind)
       everything else -> UI_Event pushed to window->ui_events
  5. run queued commands (rad_exec_cmd): mutate the config tree
       (panels/tabs/selection), window state, etc.
  6. animation rates from settings: rate = 1 - 2^(-k * dt)
  7. dr_begin_frame(icon font)
     for each window: rad_window_frame(ws)
  8. r_begin_frame; per window: r_window_begin_frame,
     dr_submit_bucket(window's draw bucket), r_window_end_frame; r_end_frame
  9. first frame of a window -> wm_window_first_paint (ShowWindow)

rad_window_frame(ws)                          rad/rad_core.c
  a. theme: preset metadesk tree -> UI_Theme {tags[], linear rgba}
  b. raster flags per font slot, DPI change -> font size *= dpi/last_dpi
  c. ui_begin_build(os window, events, icon info, theme, anim info, dt)
     push top-level stacks: font, font size, text padding, pref w/h, blur
  d. BUILD the box tree (immediate mode, every frame):
       top bar (UI_Pane), menus (UI_CtxMenu), bottom bar,
       panel boundaries, panel boxes, tab bars, tab views
     each ui_build_box_*() looks up/creates a persistent UI_Box by key
     (hash of the string), ui_signal_from_box() reads LAST frame's rects
     + this frame's events -> clicks/hover/drag
  e. ui_end_build(): layout (sizes standalone -> upward -> downward ->
     constraints -> positions, per axis), hot/active animation, focus
  f. DRAW: walk the box tree depth-first, emit draw calls into a DR_Bucket:
     drop shadow, blur, background (+hot/active effects), text runs, clip,
     custom draw, then on pop: border, fades, sides, focus overlay/border,
     disabled overlay
```

## Layers involved (dependency order)

```
base --> window_manager (WM_)  shell (SH_)  config (CFG_)  mdesk (MD_)
     --> font_provider (FP_) --> render (R_) --> font_cache (FNT_)
     --> draw (DR_: batches rects/glyphs into R_ passes)
     --> ui (UI_: box tree, layout, signals, animation, theme lookup)
     --> rad (RAD_: IDE frontend)        raddbg (RD_: debugger frontend)
```

## Key data structures

- `UI_Box` (`ui/ui_core.h`): persistent per key. Per-build: flags, string, sizes,
  colors (resolved from tags via the theme), font. Persistent: hot_t, active_t,
  view_off (scroll), rect from last layout.
- `UI_Theme` / `UI_ThemePattern`: list of (tag set, linear color). A box's color
  is looked up from its tag stack (`UI_TagF("tab")`, `"weak"`, `"pop"`, ...).
- `DR_Bucket`: list of render passes (UI rect instances, blur, 3D) for one window.
- `CFG_Node` tree (`config/`): the window/panel/tab layout. Panels are
  numeric-string nodes (their share of the parent), tabs are identifier nodes,
  `selected` children mark the focused panel and each panel's selected tab.

## Gotchas found while porting

- **Open windows after the first `wm_get_events`.** `w32_wm_wnd_proc` only runs its
  custom-border handling when `w32_wm_event_arena != 0`
  (`win32/window_manager/win32_window_manager.c:337`), and that is set by the first
  `wm_get_events`. A window created earlier gets the native title bar.
- **Settings in raddbg go through the debugger evaluator.**
  `rd_setting_b32_from_name` builds `raw((bool)(%S))` and runs `e_eval_from_string`
  (`raddbg/raddbg_core.c:410`). `rad` uses `RAD_Settings` with the same defaults.
- **Theme colors are sRGB hex in metadesk**, converted with
  `linear_from_srgba(rgba_from_u32(v))`; parsing the hex with
  `try_u64_from_str8_c_rules` gives the same value as raddbg's `e_value_from_stringf`.
