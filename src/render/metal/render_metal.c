// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

// rad: Metal backend for macOS; see render_metal.h. Objective-C without ARC
// (the Mac build compiles the unity build as Objective-C, -fno-objc-arc):
// objects kept across calls are owned (new/retain) and released by hand;
// every hook drains its temporaries in an @autoreleasepool.

////////////////////////////////
//~ rad: Generated Code

#include "render/metal/generated/render_metal.meta.c"

////////////////////////////////
//~ rad: Helpers

internal R_Handle
r_mtl_handle_from_tex2d(R_MTL_Tex2D *t)
{
  R_Handle h = {(U64)t};
  return h;
}

internal R_MTL_Tex2D *
r_mtl_tex2d_from_handle(R_Handle h)
{
  R_MTL_Tex2D *t = (R_MTL_Tex2D *)h.u64[0];
  return t;
}

internal MTLPixelFormat
r_mtl_pixel_format_from_tex2dformat(R_Tex2DFormat fmt)
{
  MTLPixelFormat result = MTLPixelFormatRGBA8Unorm;
  switch(fmt)
  {
    default:{}break;
    case R_Tex2DFormat_R8:    {result = MTLPixelFormatR8Unorm;}break;
    case R_Tex2DFormat_RG8:   {result = MTLPixelFormatRG8Unorm;}break;
    case R_Tex2DFormat_RGBA8: {result = MTLPixelFormatRGBA8Unorm;}break;
    case R_Tex2DFormat_BGRA8: {result = MTLPixelFormatBGRA8Unorm;}break;
    case R_Tex2DFormat_R16:   {result = MTLPixelFormatR16Unorm;}break;
    case R_Tex2DFormat_RGBA16:{result = MTLPixelFormatRGBA16Unorm;}break;
    case R_Tex2DFormat_R32:   {result = MTLPixelFormatR32Float;}break;
    case R_Tex2DFormat_RG32:  {result = MTLPixelFormatRG32Float;}break;
    case R_Tex2DFormat_RGBA32:{result = MTLPixelFormatRGBA32Float;}break;
  }
  return result;
}

internal B32
r_mtl_scissor_from_clip(Rng2F32 clip, Vec2F32 viewport_dim, MTLScissorRect *scissor_out)
{
  // rad: the GL backend's rounding (r_ogl_scissor). Metal's origin is the
  // top-left, like the UI's, so no flip. An empty clip draws nothing (as
  // with D3D11; GL drew unclipped when both sides were empty)
  S32 x0 = Clamp(0, (S32)floor_f32(clip.x0), (S32)viewport_dim.x);
  S32 y0 = Clamp(0, (S32)floor_f32(clip.y0), (S32)viewport_dim.y);
  S32 x1 = Clamp(0, (S32)ceil_f32(clip.x1), (S32)viewport_dim.x);
  S32 y1 = Clamp(0, (S32)ceil_f32(clip.y1), (S32)viewport_dim.y);
  B32 result = (x1 > x0 && y1 > y0);
  if(result)
  {
    scissor_out->x = (NSUInteger)x0;
    scissor_out->y = (NSUInteger)y0;
    scissor_out->width = (NSUInteger)(x1 - x0);
    scissor_out->height = (NSUInteger)(y1 - y0);
  }
  return result;
}

internal void
r_mtl_upload(id<MTLTexture> texture, Rng2S32 subrect, U64 bytes_per_pixel, void *data)
{
  Vec2S32 size = dim_2s32(subrect);
  if(size.x <= 0 || size.y <= 0)
  {
    return;
  }
  U64 bytes_per_row = (U64)size.x*bytes_per_pixel;
  U64 bytes = bytes_per_row*(U64)size.y;

  //- rad: copy into the staging chunk (a fresh one when it is full; command
  // buffers that still read the old one keep it alive). Chunk bytes are
  // never reused, so no blit in flight reads memory written here. No data:
  // zeros (Metal textures start undefined; GL's were zero in practice)
  U64 off = AlignPow2(r_mtl_state->upload_chunk_off, 16);
  if(r_mtl_state->upload_chunk == nil || off + bytes > [r_mtl_state->upload_chunk length])
  {
    [r_mtl_state->upload_chunk release];
    U64 chunk_size = Max(R_MTL_UPLOAD_CHUNK_SIZE, bytes);
    r_mtl_state->upload_chunk = [r_mtl_state->device newBufferWithLength:chunk_size options:MTLResourceStorageModeShared|MTLResourceCPUCacheModeWriteCombined];
    off = 0;
  }
  U8 *dst = (U8 *)[r_mtl_state->upload_chunk contents] + off;
  if(data != 0)
  {
    MemoryCopy(dst, data, bytes);
  }
  else
  {
    MemoryZero(dst, bytes);
  }
  r_mtl_state->upload_chunk_off = off + bytes;
  r_mtl_state->upload_count += 1;

  //- rad: blit from it into the texture
  if(r_mtl_state->upload_cmd == nil)
  {
    r_mtl_state->upload_cmd = [[r_mtl_state->queue commandBuffer] retain];
    r_mtl_state->upload_blit = [[r_mtl_state->upload_cmd blitCommandEncoder] retain];
  }
  [r_mtl_state->upload_blit copyFromBuffer:r_mtl_state->upload_chunk
                              sourceOffset:off
                         sourceBytesPerRow:bytes_per_row
                       sourceBytesPerImage:bytes
                                sourceSize:MTLSizeMake((NSUInteger)size.x, (NSUInteger)size.y, 1)
                                 toTexture:texture
                          destinationSlice:0
                          destinationLevel:0
                         destinationOrigin:MTLOriginMake((NSUInteger)subrect.x0, (NSUInteger)subrect.y0, 0)];
}

internal void
r_mtl_flush_uploads(void)
{
  if(r_mtl_state->upload_cmd != nil)
  {
    [r_mtl_state->upload_blit endEncoding];
    [r_mtl_state->upload_cmd commit];
    [r_mtl_state->upload_blit release];
    [r_mtl_state->upload_cmd release];
    r_mtl_state->upload_blit = nil;
    r_mtl_state->upload_cmd = nil;
  }
}

internal id<MTLRenderCommandEncoder>
r_mtl_window_stage_encoder(R_MTL_Window *w)
{
  if(w->stage_encoder == nil)
  {
    w->stage_encoder = r_mtl_window_encoder(w, w->stage, !w->stage_cleared);
    w->stage_cleared = 1;
  }
  return w->stage_encoder;
}

internal void
r_mtl_window_end_stage_encoder(R_MTL_Window *w)
{
  if(w->stage_encoder != nil)
  {
    [w->stage_encoder endEncoding];
    w->stage_encoder = nil;
  }
}

internal id<MTLRenderCommandEncoder>
r_mtl_window_encoder(R_MTL_Window *w, id<MTLTexture> target, B32 clear)
{
  MTLRenderPassDescriptor *desc = [MTLRenderPassDescriptor renderPassDescriptor];
  desc.colorAttachments[0].texture = target;
  desc.colorAttachments[0].loadAction = clear ? MTLLoadActionClear : MTLLoadActionLoad;
  desc.colorAttachments[0].clearColor = MTLClearColorMake(0, 0, 0, 0);
  desc.colorAttachments[0].storeAction = MTLStoreActionStore;
  id<MTLRenderCommandEncoder> encoder = [w->cmd renderCommandEncoderWithDescriptor:desc];
  return encoder;
}

////////////////////////////////
//~ rad: Backend Hooks

//- rad: top-level layer initialization

r_hook void
r_init(CmdLine *cmdln)
{
  Arena *arena = arena_alloc();
  r_mtl_state = push_array(arena, R_MTL_State, 1);
  r_mtl_state->arena = arena;
  @autoreleasepool
  {
    //- rad: device + queue
    r_mtl_state->device = MTLCreateSystemDefaultDevice();
    if(r_mtl_state->device == nil)
    {
      sh_message(1, str8_lit("Fatal Error"), str8_lit("Couldn't find a Metal device."));
      abort_self(1);
    }
    r_mtl_state->queue = [r_mtl_state->device newCommandQueue];

    //- rad: compile the shaders. Fast math (Metal's default): half the GPU
    // time of precise math at 4096 x 2304, and the same pixels (checked
    // against the GL backend: 3,624 of 9.4M pixels differ, by 1-2 of 255,
    // with precise math as with fast)
    NSError *error = nil;
    MTLCompileOptions *options = [[MTLCompileOptions new] autorelease];
    options.fastMathEnabled = YES;
    NSString *src = [[[NSString alloc] initWithBytes:r_mtl_shader_src.str length:r_mtl_shader_src.size encoding:NSUTF8StringEncoding] autorelease];
    id<MTLLibrary> library = [[r_mtl_state->device newLibraryWithSource:src options:options error:&error] autorelease];
    if(library == nil)
    {
      sh_message(1, str8_lit("Metal Shader Error"), str8_cstring((char *)error.localizedDescription.UTF8String));
      abort_self(1);
    }

    //- rad: pipelines. Stage targets: linear float, blended as GL's
    // glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)
    struct
    {
      id<MTLRenderPipelineState> *out;
      char *vs;
      char *ps;
      MTLPixelFormat format;
      B32 blend;
    }
    pipelines[] =
    {
      {&r_mtl_state->rect_pipeline,    "r_mtl_rect_vs",    "r_mtl_rect_ps",    R_MTL_STAGE_FORMAT, 1},
      {&r_mtl_state->blur_pipeline,    "r_mtl_blur_vs",    "r_mtl_blur_ps",    R_MTL_STAGE_FORMAT, 1},
      {&r_mtl_state->present_pipeline, "r_mtl_present_vs", "r_mtl_present_ps", MTLPixelFormatBGRA8Unorm,  0},
    };
    for EachElement(idx, pipelines)
    {
      MTLRenderPipelineDescriptor *desc = [[MTLRenderPipelineDescriptor new] autorelease];
      desc.vertexFunction   = [[library newFunctionWithName:@(pipelines[idx].vs)] autorelease];
      desc.fragmentFunction = [[library newFunctionWithName:@(pipelines[idx].ps)] autorelease];
      desc.colorAttachments[0].pixelFormat = pipelines[idx].format;
      if(pipelines[idx].blend)
      {
        desc.colorAttachments[0].blendingEnabled             = YES;
        desc.colorAttachments[0].rgbBlendOperation           = MTLBlendOperationAdd;
        desc.colorAttachments[0].alphaBlendOperation         = MTLBlendOperationAdd;
        desc.colorAttachments[0].sourceRGBBlendFactor        = MTLBlendFactorSourceAlpha;
        desc.colorAttachments[0].destinationRGBBlendFactor   = MTLBlendFactorOneMinusSourceAlpha;
        desc.colorAttachments[0].sourceAlphaBlendFactor      = MTLBlendFactorSourceAlpha;
        desc.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
      }
      *pipelines[idx].out = [r_mtl_state->device newRenderPipelineStateWithDescriptor:desc error:&error];
      if(*pipelines[idx].out == nil)
      {
        sh_message(1, str8_lit("Metal Pipeline Error"), str8_cstring((char *)error.localizedDescription.UTF8String));
        abort_self(1);
      }
    }

    //- rad: samplers: repeat, no mips (GL's texture parameters; D3D11's
    // samplers). The blur samples linearly, as D3D11 (raddbg on Windows)
    for EachEnumVal(R_Tex2DSampleKind, k)
    {
      MTLSamplerDescriptor *desc = [[MTLSamplerDescriptor new] autorelease];
      MTLSamplerMinMagFilter filter = (k == R_Tex2DSampleKind_Linear ? MTLSamplerMinMagFilterLinear : MTLSamplerMinMagFilterNearest);
      desc.minFilter = filter;
      desc.magFilter = filter;
      desc.mipFilter = MTLSamplerMipFilterNotMipmapped;
      desc.sAddressMode = MTLSamplerAddressModeRepeat;
      desc.tAddressMode = MTLSamplerAddressModeRepeat;
      r_mtl_state->samplers[k] = [r_mtl_state->device newSamplerStateWithDescriptor:desc];
    }

    //- rad: white texture
    {
      MTLTextureDescriptor *desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:1 height:1 mipmapped:NO];
      desc.usage = MTLTextureUsageShaderRead;
      desc.storageMode = MTLStorageModePrivate;
      r_mtl_state->white_texture = [r_mtl_state->device newTextureWithDescriptor:desc];
      U32 white_pixel = 0xffffffff;
      r_mtl_upload(r_mtl_state->white_texture, r2s32p(0, 0, 1, 1), 4, &white_pixel);
    }
  }
}

//- rad: window setup/teardown

r_hook R_Handle
r_window_equip(WM_Window window)
{
  R_MTL_Window *w = r_mtl_state->free_window;
  if(w != 0)
  {
    SLLStackPop(r_mtl_state->free_window);
  }
  else
  {
    w = push_array(r_mtl_state->arena, R_MTL_Window, 1);
  }
  MemoryZeroStruct(w);
  @autoreleasepool
  {
    //- rad: the window's view hosts a Metal layer. sRGB-tagged like the
    // window (the compositor converts to the display's color space, as it did
    // for the GL surface); at most 2 drawables: one on screen, one drawn
    MAC_WM_Window *mw = mac_wm_window_from_handle(window);
    CAMetalLayer *layer = [[CAMetalLayer alloc] init];
    CGColorSpaceRef srgb = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
    layer.device = r_mtl_state->device;
    layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
    layer.colorspace = srgb;
    layer.framebufferOnly = YES;
    layer.opaque = YES;
    layer.maximumDrawableCount = 2;
    layer.contentsGravity = kCAGravityTopLeft;
    CGColorSpaceRelease(srgb);
    if(mw != 0)
    {
      layer.contentsScale = mw->window.backingScaleFactor;
      mw->view.layer = layer;
      mw->view.wantsLayer = YES;
    }
    w->layer = layer;
    w->frame_arena = arena_alloc();
    w->frame_semaphore = dispatch_semaphore_create(R_MTL_FRAMES_IN_FLIGHT);
  }
  R_Handle result = {(U64)w};
  return result;
}

r_hook void
r_window_unequip(WM_Window window, R_Handle window_equip)
{
  R_MTL_Window *w = (R_MTL_Window *)window_equip.u64[0];
  if(w != 0)
  {
    @autoreleasepool
    {
      //- rad: wait for the window's frames in flight (a semaphore must be
      // back at its initial count before it is released)
      for EachIndex(idx, R_MTL_FRAMES_IN_FLIGHT)
      {
        dispatch_semaphore_wait(w->frame_semaphore, DISPATCH_TIME_FOREVER);
      }
      for EachIndex(idx, R_MTL_FRAMES_IN_FLIGHT)
      {
        dispatch_semaphore_signal(w->frame_semaphore);
      }
      dispatch_release(w->frame_semaphore);
      arena_release(w->frame_arena);
      MAC_WM_Window *mw = mac_wm_window_from_handle(window);
      if(mw != 0 && mw->view.layer == w->layer)
      {
        mw->view.wantsLayer = NO;
        mw->view.layer = nil;
      }
      [w->next_drawable release];
      [w->layer release];
      [w->stage release];
      [w->stage_scratch release];
      for EachElement(idx, w->inst_buffers)
      {
        [w->inst_buffers[idx] release];
      }
    }
    SLLStackPush(r_mtl_state->free_window, w);
  }
}

//- rad: textures

r_hook R_Handle
r_tex2d_alloc(R_ResourceKind kind, Vec2S32 size, R_Tex2DFormat format, void *data)
{
  R_MTL_Tex2D *t = r_mtl_state->free_tex2d;
  if(t != 0)
  {
    SLLStackPop(r_mtl_state->free_tex2d);
  }
  else
  {
    t = push_array(r_mtl_state->arena, R_MTL_Tex2D, 1);
  }
  MemoryZeroStruct(t);
  @autoreleasepool
  {
    MTLTextureDescriptor *desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:r_mtl_pixel_format_from_tex2dformat(format)
                                                                                    width:(NSUInteger)Max(size.x, 1)
                                                                                   height:(NSUInteger)Max(size.y, 1)
                                                                                mipmapped:NO];
    desc.usage = MTLTextureUsageShaderRead;
    desc.storageMode = MTLStorageModePrivate;
    t->texture = [r_mtl_state->device newTextureWithDescriptor:desc];
    t->kind = kind;
    t->format = format;
    t->size = size;
    r_mtl_upload(t->texture, r2s32p(0, 0, size.x, size.y), r_tex2d_format_bytes_per_pixel_table[format], data);
  }
  R_Handle result = r_mtl_handle_from_tex2d(t);
  return result;
}

r_hook void
r_tex2d_release(R_Handle texture)
{
  R_MTL_Tex2D *t = r_mtl_tex2d_from_handle(texture);
  if(t != 0)
  {
    // rad: command buffers that still use it keep it alive
    [t->texture release];
    t->texture = nil;
    SLLStackPush(r_mtl_state->free_tex2d, t);
  }
}

r_hook R_ResourceKind
r_kind_from_tex2d(R_Handle texture)
{
  R_MTL_Tex2D *t = r_mtl_tex2d_from_handle(texture);
  R_ResourceKind result = (t != 0 ? t->kind : R_ResourceKind_Static);
  return result;
}

r_hook Vec2S32
r_size_from_tex2d(R_Handle texture)
{
  R_MTL_Tex2D *t = r_mtl_tex2d_from_handle(texture);
  Vec2S32 result = (t != 0 ? t->size : v2s32(0, 0));
  return result;
}

r_hook R_Tex2DFormat
r_format_from_tex2d(R_Handle texture)
{
  R_MTL_Tex2D *t = r_mtl_tex2d_from_handle(texture);
  R_Tex2DFormat result = (t != 0 ? t->format : R_Tex2DFormat_RGBA8);
  return result;
}

r_hook void
r_fill_tex2d_region(R_Handle texture, Rng2S32 subrect, void *data)
{
  R_MTL_Tex2D *t = r_mtl_tex2d_from_handle(texture);
  if(t != 0)
  {
    @autoreleasepool
    {
      r_mtl_upload(t->texture, subrect, r_tex2d_format_bytes_per_pixel_table[t->format], data);
    }
  }
}

//- rad: buffers (unused: no 3D passes, as in the GL backend)

r_hook R_Handle
r_buffer_alloc(R_ResourceKind kind, U64 size, void *data)
{
  R_Handle result = {0};
  return result;
}

r_hook void
r_buffer_release(R_Handle buffer)
{
}

//- rad: frame markers

r_hook void
r_begin_frame(void)
{
}

r_hook void
r_end_frame(void)
{
  @autoreleasepool
  {
    r_mtl_flush_uploads();
  }
}

r_hook void
r_window_begin_frame(WM_Window os, R_Handle r)
{
  R_MTL_Window *w = (R_MTL_Window *)r.u64[0];
  MAC_WM_Window *mw = mac_wm_window_from_handle(os);
  @autoreleasepool
  {
    //- rad: no passes submitted yet
    arena_clear(w->frame_arena);
    w->first_pass_list = w->last_pass_list = 0;

    //- rad: (re)size the stage targets and the drawables with the window
    // (the new targets hold nothing yet: the next frame is drawn and shown)
    Vec2F32 dim = dim_2f32(wm_client_rect_from_window(os));
    F64 scale = (mw != 0 ? mw->window.backingScaleFactor : 1.0);
    if(dim.x != w->last_client_rect_dim.x || dim.y != w->last_client_rect_dim.y || scale != w->last_scale)
    {
      w->last_client_rect_dim = dim;
      w->last_scale = scale;
      MTLTextureDescriptor *desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:R_MTL_STAGE_FORMAT
                                                                                      width:(NSUInteger)Max(dim.x, 1)
                                                                                     height:(NSUInteger)Max(dim.y, 1)
                                                                                  mipmapped:NO];
      desc.usage = MTLTextureUsageRenderTarget|MTLTextureUsageShaderRead;
      desc.storageMode = MTLStorageModePrivate;
      [w->stage release];
      [w->stage_scratch release];
      w->stage = [r_mtl_state->device newTextureWithDescriptor:desc];
      w->stage_scratch = [r_mtl_state->device newTextureWithDescriptor:desc];
      [w->next_drawable release];
      w->next_drawable = nil;
      w->layer.contentsScale = scale;
      w->layer.drawableSize = CGSizeMake(Max(dim.x, 1), Max(dim.y, 1));
      w->shown_valid = 0;
    }
  }
}

r_hook void
r_window_end_frame(WM_Window os, R_Handle r)
{
  R_MTL_Window *w = (R_MTL_Window *)r.u64[0];
  MAC_WM_Window *mw = mac_wm_window_from_handle(os);
  @autoreleasepool
  {
    //- rad: present only what changes the screen. Not for a window that
    // can't be seen: the compositor drops drawables presented to a covered
    // window (the WM asks for a frame when it is uncovered), and nextDrawable
    // can wait up to a second for one while it is minimized. Not when the
    // frame's draw data (and the textures) equal the frame on screen: rad
    // draws several frames per input, mostly identical, and each presented
    // frame takes a display refresh, so the next input's frame would wait
    // behind them (at 60 Hz, up to two refreshes more while typing)
    B32 visible = (mw != 0 &&
                   (mw->window.occlusionState & NSWindowOcclusionStateVisible) &&
                   w->last_client_rect_dim.x >= 1 && w->last_client_rect_dim.y >= 1);
    U64 hash = u64_hash_from_seed_str8(5381, str8_struct(&w->last_client_rect_dim));
    for(R_MTL_PassListNode *n = w->first_pass_list; n != 0; n = n->next)
    {
      hash = r_mtl_hash_from_passes(hash, n->passes);
    }
    B32 unchanged = (w->shown_valid &&
                     w->shown_hash == hash &&
                     w->shown_upload_count == r_mtl_state->upload_count &&
                     (mw == 0 || !mw->window.inLiveResize));
    if(!visible)
    {
      w->shown_valid = 0;
    }
    
    //- rad: texture uploads go before any later frame in the queue
    r_mtl_flush_uploads();
    
    //- rad: draw and present
    if(visible && !unchanged)
    {
      //- rad: wait until this frame's instance buffer is no longer read by
      // the GPU (the frame R_MTL_FRAMES_IN_FLIGHT ago has completed)
      dispatch_semaphore_wait(w->frame_semaphore, DISPATCH_TIME_FOREVER);
      w->inst_buffer_idx = (w->inst_buffer_idx + 1) % R_MTL_FRAMES_IN_FLIGHT;
      w->inst_buffer_off = 0;
      w->cmd = [[r_mtl_state->queue commandBuffer] retain];
      w->stage_cleared = 0;
      w->scratch_cleared = 0;
      
      //- rad: draw the passes into the stage (cleared even when empty)
      for(R_MTL_PassListNode *n = w->first_pass_list; n != 0; n = n->next)
      {
        r_mtl_window_encode_passes(w, n->passes);
      }
      r_mtl_window_end_stage_encoder(w);
      if(!w->stage_cleared)
      {
        [r_mtl_window_encoder(w, w->stage, 1) endEncoding];
        w->stage_cleared = 1;
      }
      
      //- rad: encode the stage into a drawable: the one taken after the last
      // present (below), else a new one
      id<CAMetalDrawable> drawable = [w->next_drawable autorelease];
      w->next_drawable = nil;
      if(drawable == nil || mw->window.inLiveResize)
      {
        drawable = [w->layer nextDrawable];
      }
      if(drawable != nil)
      {
        MTLRenderPassDescriptor *desc = [MTLRenderPassDescriptor renderPassDescriptor];
        desc.colorAttachments[0].texture = drawable.texture;
        desc.colorAttachments[0].loadAction = MTLLoadActionDontCare;
        desc.colorAttachments[0].storeAction = MTLStoreActionStore;
        id<MTLRenderCommandEncoder> encoder = [w->cmd renderCommandEncoderWithDescriptor:desc];
        [encoder setRenderPipelineState:r_mtl_state->present_pipeline];
        [encoder setFragmentTexture:w->stage atIndex:0];
        [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
        [encoder endEncoding];
        w->shown_valid = 1;
        w->shown_hash = hash;
        w->shown_upload_count = r_mtl_state->upload_count;
      }
      
      //- rad: commit and present; the frame's slot frees when the GPU is
      // done with it. During a live resize, present in Core Animation's
      // transaction, so the new frame lands together with the window's new
      // size (otherwise the content trails the edge being dragged)
      B32 present_with_transaction = (drawable != nil && mw->window.inLiveResize);
      if(w->layer.presentsWithTransaction != present_with_transaction)
      {
        w->layer.presentsWithTransaction = present_with_transaction;
      }
      if(drawable != nil && !present_with_transaction)
      {
        [w->cmd presentDrawable:drawable];
      }
      dispatch_semaphore_t frame_semaphore = w->frame_semaphore;
      [w->cmd addCompletedHandler:^(id<MTLCommandBuffer> cmd) { dispatch_semaphore_signal(frame_semaphore); }];
      [w->cmd commit];
      if(present_with_transaction)
      {
        [w->cmd waitUntilScheduled];
        [drawable present];
      }
      [w->cmd release];
      w->cmd = nil;
      
      //- rad: take the next frame's drawable now, not when the next frame is
      // drawn. With 2 drawables this waits until this frame is on screen, so
      // the next frame reads its input right after a refresh and is shown at
      // the next one. Waiting after building it instead (frames shown back to
      // back, as during the caret's slide after each keystroke) would read
      // the input a refresh earlier: typed text one refresh later on screen
      if(drawable != nil && !present_with_transaction)
      {
        w->next_drawable = [[w->layer nextDrawable] retain];
      }
    }
    else
    {
      r_mtl_state->skipped_frame_count += 1;
    }
  }
}

//- rad: render pass submission

r_hook void
r_window_submit(WM_Window window, R_Handle window_equip, R_PassList *passes)
{
  // rad: drawn at the end of the frame (r_window_end_frame), when it is
  // known whether the frame changes the screen; the pass lists live until then
  R_MTL_Window *w = (R_MTL_Window *)window_equip.u64[0];
  R_MTL_PassListNode *n = push_array(w->frame_arena, R_MTL_PassListNode, 1);
  n->passes = passes;
  SLLQueuePush(w->first_pass_list, w->last_pass_list, n);
}

////////////////////////////////
//~ rad: Pass Encoding

internal U64
r_mtl_hash_from_passes(U64 seed, R_PassList *passes)
{
  // rad: everything a pass draws with: its parameters and instances (keys
  // are zeroed first, so struct padding hashes the same every frame)
  U64 hash = seed;
  for(R_PassNode *pass_n = passes->first; pass_n != 0; pass_n = pass_n->next)
  {
    R_Pass *pass = &pass_n->v;
    hash = u64_hash_from_seed_str8(hash, str8_struct(&pass->kind));
    switch(pass->kind)
    {
      default:{}break;
      case R_PassKind_UI:
      {
        for(R_BatchGroup2DNode *group_n = pass->params_ui->rects.first; group_n != 0; group_n = group_n->next)
        {
          R_BatchGroup2DParams *params = &group_n->params;
          struct {U64 tex; U64 sample_kind; Mat3x3F32 xform; Rng2F32 clip; F32 transparency;} key;
          MemoryZeroStruct(&key);
          key.tex = params->tex.u64[0];
          key.sample_kind = (U64)params->tex_sample_kind;
          key.xform = params->xform;
          key.clip = params->clip;
          key.transparency = params->transparency;
          hash = u64_hash_from_seed_str8(hash, str8_struct(&key));
          for(R_BatchNode *batch_n = group_n->batches.first; batch_n != 0; batch_n = batch_n->next)
          {
            hash = u64_hash_from_seed_str8(hash, str8(batch_n->v.v, batch_n->v.byte_count));
          }
        }
      }break;
      case R_PassKind_Blur:
      {
        R_PassParams_Blur *params = pass->params_blur;
        struct {Rng2F32 rect; Rng2F32 clip; F32 blur_size; F32 corner_radii[Corner_COUNT];} key;
        MemoryZeroStruct(&key);
        key.rect = params->rect;
        key.clip = params->clip;
        key.blur_size = params->blur_size;
        MemoryCopyArray(key.corner_radii, params->corner_radii);
        hash = u64_hash_from_seed_str8(hash, str8_struct(&key));
      }break;
    }
  }
  return hash;
}

internal void
r_mtl_window_encode_passes(R_MTL_Window *w, R_PassList *passes)
{
  Vec2F32 viewport_dim = w->last_client_rect_dim;
  MTLScissorRect full_scissor = {0, 0, (NSUInteger)Max(viewport_dim.x, 1), (NSUInteger)Max(viewport_dim.y, 1)};
  // rad: no autorelease pool here: the stage encoder stays open across
  // calls (r_window_end_frame's pool)
  {
    for(R_PassNode *pass_n = passes->first; pass_n != 0; pass_n = pass_n->next)
    {
      R_Pass *pass = &pass_n->v;
      switch(pass->kind)
      {
        default:{}break;

        ////////////////////////
        //- rad: ui rendering pass
        //
        case R_PassKind_UI:
        {
          //- rad: unpack params
          R_PassParams_UI *params = pass->params_ui;
          R_BatchGroup2DList *rect_batch_groups = &params->rects;

          //- rad: every group's instances packed into this frame's instance
          // buffer (grown when short: a pass already encoded keeps the old one)
          U64 packed_size = 0;
          for(R_BatchGroup2DNode *group_n = rect_batch_groups->first; group_n != 0; group_n = group_n->next)
          {
            packed_size += group_n->batches.byte_count;
          }
          if(packed_size == 0)
          {
            break;
          }
          U64 inst_off = AlignPow2(w->inst_buffer_off, 256);
          id<MTLBuffer> inst_buffer = w->inst_buffers[w->inst_buffer_idx];
          if(inst_buffer == nil || inst_off + packed_size > [inst_buffer length])
          {
            U64 new_size = Max(KB(256), packed_size);
            if(inst_buffer != nil)
            {
              new_size = Max(new_size, 2*[inst_buffer length]);
            }
            [inst_buffer release];
            inst_buffer = [r_mtl_state->device newBufferWithLength:new_size options:MTLResourceStorageModeShared];
            w->inst_buffers[w->inst_buffer_idx] = inst_buffer;
            inst_off = 0;
          }
          {
            U8 *packed = (U8 *)[inst_buffer contents] + inst_off;
            U64 off = 0;
            for(R_BatchGroup2DNode *group_n = rect_batch_groups->first; group_n != 0; group_n = group_n->next)
            {
              for(R_BatchNode *batch_n = group_n->batches.first; batch_n != 0; batch_n = batch_n->next)
              {
                MemoryCopy(packed + off, batch_n->v.v, batch_n->v.byte_count);
                off += batch_n->v.byte_count;
              }
            }
          }
          w->inst_buffer_off = inst_off + packed_size;

          //- rad: draw each batch group (into the stage encoder, which stays
          // open for the next UI pass: the stage is loaded and stored once,
          // not once per pass; 151 MB a pass at 4096 x 2304 in RGBA32F)
          id<MTLRenderCommandEncoder> encoder = r_mtl_window_stage_encoder(w);
          [encoder setRenderPipelineState:r_mtl_state->rect_pipeline];
          U64 group_off = inst_off;
          for(R_BatchGroup2DNode *group_n = rect_batch_groups->first; group_n != 0; group_n = group_n->next)
          {
            R_BatchList *batches = &group_n->batches;
            R_BatchGroup2DParams *group_params = &group_n->params;
            U64 group_inst_off = group_off;
            group_off += batches->byte_count;
            U64 inst_count = (batches->bytes_per_inst != 0 ? batches->byte_count / batches->bytes_per_inst : 0);
            if(inst_count == 0)
            {
              continue;
            }

            //- rad: set up scissor
            MTLScissorRect scissor = full_scissor;
            if(group_params->clip.x0 != 0 ||
               group_params->clip.x1 != 0 ||
               group_params->clip.y0 != 0 ||
               group_params->clip.y1 != 0)
            {
              if(!r_mtl_scissor_from_clip(group_params->clip, viewport_dim, &scissor))
              {
                continue;
              }
            }

            //- rad: unpack texture
            R_Tex2DFormat texture_fmt = R_Tex2DFormat_RGBA8;
            id<MTLTexture> texture = r_mtl_state->white_texture;
            {
              R_MTL_Tex2D *tex = r_mtl_tex2d_from_handle(group_params->tex);
              if(tex != 0 && tex->texture != nil)
              {
                texture = tex->texture;
                texture_fmt = tex->format;
              }
            }
            R_Tex2DSampleKind sample_kind = group_params->tex_sample_kind;
            if(sample_kind >= R_Tex2DSampleKind_COUNT)
            {
              sample_kind = R_Tex2DSampleKind_Nearest;
            }

            //- rad: uniforms (xform: GL's column-major mat3)
            R_MTL_RectUniforms uniforms = {0};
            for EachIndex(col, 3)
            {
              uniforms.xform[col] = v4f32(group_params->xform.v[col][0], group_params->xform.v[col][1], group_params->xform.v[col][2], 0);
            }
            uniforms.texture_sample_channel_map = r_sample_channel_map_from_tex2dformat(texture_fmt);
            uniforms.viewport_size_px = viewport_dim;
            uniforms.opacity = 1.f - group_params->transparency;

            //- rad: draw
            [encoder setVertexBuffer:inst_buffer offset:group_inst_off atIndex:0];
            [encoder setVertexBytes:&uniforms length:sizeof(uniforms) atIndex:1];
            [encoder setFragmentBytes:&uniforms length:sizeof(uniforms) atIndex:1];
            [encoder setVertexTexture:texture atIndex:0];
            [encoder setFragmentTexture:texture atIndex:0];
            [encoder setFragmentSamplerState:r_mtl_state->samplers[sample_kind] atIndex:0];
            [encoder setScissorRect:scissor];
            [encoder drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4 instanceCount:inst_count];
          }
        }break;

        ////////////////////////
        //- rad: blur rendering pass
        //
        case R_PassKind_Blur:
        {
          R_PassParams_Blur *params = pass->params_blur;
          R_MTL_BlurUniforms uniforms = {0};

          //- rjf: form blur sampling kernel
          U64 blur_count = 0;
          {
            //
            // TODO(rjf): adopted from martins' d3d11 impl, should dedup in common render helpers
            //
            Vec4F32 *kernel = uniforms.blur_kernel;
            F32 weights[ArrayCount(uniforms.blur_kernel)*2] = {0};
            F32 blur_size = Min(params->blur_size, ArrayCount(weights));
            blur_count = (U64)round_f32(blur_size);
            F32 stdev = (blur_size-1.f)/2.f;
            F32 one_over_root_2pi_stdev2 = 1/sqrt_f32(2*pi32*stdev*stdev);
            F32 euler32 = 2.718281828459045f;
            weights[0] = 1.f;
            if(stdev > 0.f)
            {
              for(U64 idx = 0; idx < blur_count; idx += 1)
              {
                F32 kernel_x = (F32)idx;
                weights[idx] = one_over_root_2pi_stdev2*pow_f32(euler32, -kernel_x*kernel_x/(2.f*stdev*stdev));
              }
            }
            if(weights[0] > 1.f)
            {
              MemoryZeroArray(weights);
              weights[0] = 1.f;
            }
            else
            {
              // prepare weights & offsets for bilinear lookup
              // blur filter wants to calculate w0*pixel[pos] + w1*pixel[pos+1] + ...
              // with bilinear filter we can do this calulation by doing only w*sample(pos+t) = w*((1-t)*pixel[pos] + t*pixel[pos+1])
              // we can see w0=w*(1-t) and w1=w*t
              // thus w=w0+w1 and t=w1/w
              for(U64 idx = 1; idx < blur_count; idx += 2)
              {
                F32 w0 = weights[idx + 0];
                F32 w1 = weights[idx + 1];
                F32 w = w0 + w1;
                F32 t = w1 / w;

                // each kernel element is float2(weight, offset)
                // weights & offsets are adjusted for bilinear sampling
                // zw elements are not used, a bit of waste but it allows for simpler shader code
                kernel[(idx+1)/2] = v4f32(w, (F32)idx + t, 0, 0);
              }
            }
            kernel[0].x = weights[0];
          }

          //- rad: uniforms
          uniforms.rect = v4f32(params->rect.x0, params->rect.y0, params->rect.x1, params->rect.y1);
          uniforms.corner_radii_px = v4f32(params->corner_radii[0], params->corner_radii[1], params->corner_radii[2], params->corner_radii[3]);
          uniforms.viewport_size = viewport_dim;
          uniforms.blur_count = (U32)blur_count;

          //- rad: set up scissor
          MTLScissorRect scissor = full_scissor;
          if(params->clip.x0 != 0 ||
             params->clip.x1 != 0 ||
             params->clip.y0 != 0 ||
             params->clip.y1 != 0)
          {
            if(!r_mtl_scissor_from_clip(params->clip, viewport_dim, &scissor))
            {
              break;
            }
          }

          //- rad: a blur of width 0 or 1 (a faded-out blur: raddbg scales the
          // size by the box's transparency) copies the stage onto itself:
          // skip it (two passes over the box, every frame, for the editor)
          if(blur_count <= 1)
          {
            break;
          }
          
          //- rad: the stage holds this frame's content (at least cleared)
          // before it is read
          r_mtl_window_end_stage_encoder(w);
          if(!w->stage_cleared)
          {
            [r_mtl_window_encoder(w, w->stage, 1) endEncoding];
            w->stage_cleared = 1;
          }

          //- rjf: pass 1: stage -(horizontal)-> stage_scratch
          //- rjf: pass 2: stage_scratch -(vertical)-> stage
          struct
          {
            id<MTLTexture> src;
            id<MTLTexture> dst;
            Vec2F32 direction;
          }
          blur_passes[] =
          {
            {w->stage, w->stage_scratch, v2f32(1.f/viewport_dim.x, 0)},
            {w->stage_scratch, w->stage, v2f32(0, 1.f/viewport_dim.y)},
          };
          for EachElement(idx, blur_passes)
          {
            B32 clear = 0;
            if(blur_passes[idx].dst == w->stage_scratch)
            {
              clear = !w->scratch_cleared;
              w->scratch_cleared = 1;
            }
            uniforms.direction = blur_passes[idx].direction;
            id<MTLRenderCommandEncoder> encoder = r_mtl_window_encoder(w, blur_passes[idx].dst, clear);
            [encoder setRenderPipelineState:r_mtl_state->blur_pipeline];
            [encoder setVertexBytes:&uniforms length:sizeof(uniforms) atIndex:0];
            [encoder setFragmentBytes:&uniforms length:sizeof(uniforms) atIndex:0];
            [encoder setFragmentTexture:blur_passes[idx].src atIndex:0];
            [encoder setFragmentSamplerState:r_mtl_state->samplers[R_Tex2DSampleKind_Linear] atIndex:0];
            [encoder setScissorRect:scissor];
            [encoder drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4];
            [encoder endEncoding];
          }
        }break;

        ////////////////////////
        //- rad: 3d geometry rendering pass (not drawn, as in the GL backend)
        //
        case R_PassKind_Geo3D:
        {
        }break;
      }
    }
  }
}
