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
