# Feasibility spike (macOS) - answers with evidence

Checked in the code on 2026-11-02 on Windows. Nothing here was run on a Mac yet.

## OpenGL version required by the GL render backend

- Shaders are `#version 330 core` (`render/opengl/generated/render_opengl.meta.h:26`).
- The EGL path requests a 3.3 context (`render/opengl/linux/egl/render_opengl_linux_egl.c:119-120`).
- macOS supports up to 4.1 core, so 3.3 core fits. UNVERIFIED on a Mac: extensions used,
  sRGB framebuffer behavior, and whether the blur pass works unchanged.

## x64 intrinsics outside debugger layers

Yes, in `base`:
- `base/base_core.h:276-277` includes `immintrin.h` and `emmintrin.h` for every
  clang/gcc build, with no arch guard. These headers do not exist for ARM64.
- `base/base_core.c:667-669` uses SSE2 (`_mm_loadl_epi64`, `_mm_cmpeq_epi8`,
  `_mm_movemask_epi8`).
- Atomics use `__atomic_*` builtins: portable.
- Build scripts pass `-mcx16` to clang (`build.bat:87`, `build.sh:23`): x86-only flag.

## Linux-only APIs that a `src/mac/base` must replace

`syscall(SYS_futex)`, `memfd_create`, `/proc/self`, `sys/sysinfo.h`,
`pthread_setname_np` signature (in `linux/base`), `epoll` (`linux/socket`), `eventfd`
(`linux/window_manager`). The Linux window manager is X11-only: nothing reusable for Cocoa.

## Event loop and frame pacing

- `wm_get_events(arena, wait)` (win32: `GetMessageW` when waiting, then drains with
  `PeekMessageW`). The app requests frames (`rad_request_frame` sets a 4-frame budget);
  when the budget is 0 the loop blocks in `GetMessageW` until input.
- Frame rate: target Hz from the monitor refresh rate, snapped to a common rate based on
  recent frame times; `frame_dt = 1/target_hz` drives animation rates.
- Presentation (vsync) happens in `r_window_end_frame` (render backend). UNVERIFIED: I did not read the D3D11/GL present code.
- Consequence for Cocoa: the loop is "pump events, build, draw" on the main thread.
  A Cocoa port can keep that shape by pumping `[NSApp nextEventMatchingMask:...]` inside
  `wm_get_events` (blocking when `wait`), which avoids handing the main thread to
  `[NSApp run]`. UNVERIFIED until tried on the Mac (live resize is the known pain point:
  macOS runs a nested loop while resizing).

## Milestone 12 plan (checked in the code on the Mac, 2026-11-05)

- Port surface: no OS switches in `rad`, `ui`, `draw`, `font_cache`, `text`, `config`,
  `content`, `file_stream`, `artifact_cache`, `mutable_text`, `text_buffer`, `json`, `lsp`.
  Only four backends are per-OS:
  - `window_manager` (`wm_*`, ~50 functions, `src/linux/window_manager` is X11-only).
    Script mode's simulated input lives in the shared layer (`wm_sim_*`); the
    backend asks it in `wm_get_events`, `wm_get_modifiers`, `wm_key_is_down`,
    `wm_mouse_from_window`, focus and clipboard queries (Linux: 11 call sites).
  - `shell` (`sh_*`: message box, pick file/folder, show in Finder, open URL,
    launch in terminal).
  - `render`: the GL backend has a 6-function OS seam (`r_ogl_os_load_procedure`,
    `r_ogl_os_init`, `r_ogl_os_window_equip/unequip`, `r_ogl_os_select_window`,
    `r_ogl_os_window_swap`). Catch: the backend declares global function pointers
    named like the GL functions (`glGenBuffers`, 47 of them), and Apple's `gl3.h`
    always declares prototypes with those names (it defines `GL3_PROTOTYPES`
    itself). Fix like the Linux EGL backend does for 3 names: `#define` the
    colliding names away around the Apple include; load with `dlsym`.
  - `font_provider`: the FreeType backend implements all 6 hooks, but opens files
    only. DirectWrite also accepts a font family name (rad's default fonts, task
    H3); the Mac needs CoreText (`CTFontDescriptor` -> `kCTFontURLAttribute`) to
    turn a family name into a file for FreeType.
- Live resize: win32 calls `update()` (one whole frame) from `WM_SIZE`/`WM_PAINT`
  while Windows runs its modal size loop (`win32_window_manager.c:365-370`). Cocoa
  equivalent: call `update()` from the window delegate's resize callback.
- Order: (1) `raid` links on the Mac against the stub window manager and stub
  renderer (proves the frontend compiles); (2) Cocoa window + events + GL context,
  first pixels; (3) FreeType + CoreText names; (4) shell; (5) script runner and
  `screencapture` screenshots; (6) side-by-side with the Windows screenshots.
