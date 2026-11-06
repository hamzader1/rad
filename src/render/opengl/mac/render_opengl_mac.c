// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

// rad: macOS OS portion of the OpenGL backend. One NSOpenGLContext (4.1 core;
// the shaders are 3.3 core) is shared by all windows: selecting a window
// points the context at that window's view. GL objects are created once
// (buffers, textures, programs, VAOs) and stay valid across windows.

internal VoidProc *
r_ogl_os_load_procedure(char *name)
{
  // rad: OpenGL.framework exports every GL entry point directly
  VoidProc *result = (VoidProc *)dlsym(RTLD_DEFAULT, name);
  return result;
}

internal void
r_ogl_os_init(CmdLine *cmdln)
{
  Arena *arena = arena_alloc();
  r_ogl_mac_state = push_array(arena, R_OGL_MAC_State, 1);
  r_ogl_mac_state->arena = arena;
  @autoreleasepool
  {
    NSOpenGLPixelFormatAttribute attributes[] =
    {
      NSOpenGLPFAOpenGLProfile, NSOpenGLProfileVersion4_1Core,
      NSOpenGLPFAColorSize, 24,
      NSOpenGLPFAAlphaSize, 8,
      NSOpenGLPFADoubleBuffer,
      NSOpenGLPFAAccelerated,
      0,
    };
    NSOpenGLPixelFormat *format = [[[NSOpenGLPixelFormat alloc] initWithAttributes:attributes] autorelease];
    if(format == nil)
    {
      sh_message(1, str8_lit("Fatal Error"), str8_lit("Couldn't find an OpenGL 4.1 core pixel format."));
      abort_self(1);
    }
    r_ogl_mac_state->context = [[NSOpenGLContext alloc] initWithFormat:format shareContext:nil];
    if(r_ogl_mac_state->context == nil)
    {
      sh_message(1, str8_lit("Fatal Error"), str8_lit("Couldn't create an OpenGL context."));
      abort_self(1);
    }
    // rad: RAID_SWAP_INTERVAL=0: flushBuffer does not wait for the display's
    // refresh (the compositor still shows whole frames; rad only draws when
    // asked). For latency comparisons (tests/perf/typelat.m).
    GLint swap_interval = 1;
    char *swap_interval_env = getenv("RAID_SWAP_INTERVAL");
    if(swap_interval_env != 0 && swap_interval_env[0] == '0')
    {
      swap_interval = 0;
    }
    [r_ogl_mac_state->context setValues:&swap_interval forParameter:NSOpenGLContextParameterSwapInterval];
    [r_ogl_mac_state->context makeCurrentContext];
  }
}

internal R_Handle
r_ogl_os_window_equip(WM_Window window)
{
  // rad: nothing per window; the WM window's view is the drawable
  R_Handle result = {window.u64[0]};
  return result;
}

internal void
r_ogl_os_window_unequip(WM_Window os, R_Handle r)
{
  MAC_WM_Window *w = mac_wm_window_from_handle(os);
  if(w != 0 && r_ogl_mac_state->current_view == w->view)
  {
    [r_ogl_mac_state->context clearDrawable];
    r_ogl_mac_state->current_view = nil;
  }
}

internal void
r_ogl_os_select_window(WM_Window os, R_Handle r)
{
  MAC_WM_Window *w = mac_wm_window_from_handle(os);
  if(w == 0) { return; }
  NSOpenGLContext *context = r_ogl_mac_state->context;
  Vec2F32 dim = dim_2f32(wm_client_rect_from_window(os));
  if(r_ogl_mac_state->current_view != w->view)
  {
    [context setView:w->view];
    r_ogl_mac_state->current_view = w->view;
    r_ogl_mac_state->current_view_dim = dim;
  }
  else if(dim.x != r_ogl_mac_state->current_view_dim.x || dim.y != r_ogl_mac_state->current_view_dim.y)
  {
    [context update];
    r_ogl_mac_state->current_view_dim = dim;
  }
  [context makeCurrentContext];
}

internal GLuint
r_ogl_mac_compile_shader(GLenum kind, char *source)
{
  GLuint shader = glCreateShader(kind);
  glShaderSource(shader, 1, &source, 0);
  glCompileShader(shader);
  GLint ok = 0;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
  if(!ok)
  {
    char log[1024] = {0};
    glGetShaderInfoLog(shader, sizeof(log), 0, log);
    sh_message(1, str8_lit("OpenGL Error"), str8_cstring(log));
  }
  return shader;
}

internal void
r_ogl_mac_present(GLuint stage_texture, Vec2F32 dim)
{
  //- rad: lazily build the present pass: one full-window triangle that reads
  // the linear stage texture texel-for-texel and writes sRGB-encoded values
  if(r_ogl_mac_state->present_program == 0)
  {
    char *vs_src =
    "#version 330 core\n"
    "void main()\n"
    "{\n"
    "  vec2 p = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);\n"
    "  gl_Position = vec4(p*2.0 - 1.0, 0.0, 1.0);\n"
    "}\n";
    char *fs_src =
    "#version 330 core\n"
    "uniform sampler2D stage;\n"
    "out vec4 final_color;\n"
    "void main()\n"
    "{\n"
    "  vec3 c = clamp(texelFetch(stage, ivec2(gl_FragCoord.xy), 0).rgb, 0.0, 1.0);\n"
    "  vec3 lo = c*12.92;\n"
    "  vec3 hi = 1.055*pow(c, vec3(1.0/2.4)) - 0.055;\n"
    "  final_color = vec4(mix(lo, hi, step(vec3(0.0031308), c)), 1.0);\n"
    "}\n";
    GLuint vs = r_ogl_mac_compile_shader(GL_VERTEX_SHADER, vs_src);
    GLuint fs = r_ogl_mac_compile_shader(GL_FRAGMENT_SHADER, fs_src);
    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glBindFragDataLocation(program, 0, "final_color");
    glLinkProgram(program);
    glDeleteShader(vs);
    glDeleteShader(fs);
    r_ogl_mac_state->present_program = program;
    glGenVertexArrays(1, &r_ogl_mac_state->present_vao);
  }
  
  //- rad: draw into the window's framebuffer
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glDisable(GL_FRAMEBUFFER_SRGB);
  glDisable(GL_BLEND);
  glDisable(GL_SCISSOR_TEST);
  glViewport(0, 0, (S32)dim.x, (S32)dim.y);
  glUseProgram(r_ogl_mac_state->present_program);
  glBindVertexArray(r_ogl_mac_state->present_vao);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, stage_texture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); // no mips: keep it complete
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glUniform1i(glGetUniformLocation(r_ogl_mac_state->present_program, "stage"), 0);
  glDrawArrays(GL_TRIANGLES, 0, 3);
  glBindVertexArray(0);
  glUseProgram(0);
  glEnable(GL_FRAMEBUFFER_SRGB);

  //- rad: debug aid: RAID_SCREENSHOT=<path.png> writes the 60th presented
  // frame (read back from the GPU, so no Screen Recording permission needed)
  local_persist U64 present_count = 0;
  present_count += 1;
  char *screenshot_path = getenv("RAID_SCREENSHOT");
  if(screenshot_path != 0 && present_count == 60)
  {
    Temp scratch = scratch_begin(0, 0);
    S32 w = (S32)dim.x, h = (S32)dim.y;
    U8 *pixels = push_array(scratch.arena, U8, (U64)w*h*4);
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    @autoreleasepool
    {
      NSBitmapImageRep *rep = [[[NSBitmapImageRep alloc] initWithBitmapDataPlanes:0 pixelsWide:w pixelsHigh:h bitsPerSample:8 samplesPerPixel:4 hasAlpha:YES isPlanar:NO colorSpaceName:NSDeviceRGBColorSpace bytesPerRow:w*4 bitsPerPixel:32] autorelease];
      for(S32 y = 0; y < h; y += 1)
      {
        MemoryCopy(rep.bitmapData + (U64)y*w*4, pixels + (U64)(h-1-y)*w*4, (U64)w*4);
      }
      NSData *png = [rep representationUsingType:NSBitmapImageFileTypePNG properties:@{}];
      [png writeToFile:[NSString stringWithUTF8String:screenshot_path] atomically:YES];
    }
    scratch_end(scratch);
  }
}

internal void
r_ogl_os_window_swap(WM_Window os, R_Handle r)
{
  [r_ogl_mac_state->context flushBuffer];
}
