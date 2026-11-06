// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

#ifndef RENDER_METAL_H
#define RENDER_METAL_H

// rad: Metal backend (macOS). Same frame structure as the OpenGL backend, so
// the pixels match: the UI is drawn in linear color into a float "stage"
// texture per window (blur passes read it back through a scratch texture),
// then one pass encodes it to sRGB into the window's CAMetalLayer drawable.
//
//   r_window_begin_frame: (re)size stage + scratch textures
//   r_window_submit:      records the pass list
//   r_window_end_frame:   texture uploads committed first; then, if the
//                         window can be seen and the frame's draw data
//                         differs from the frame on screen:
//                         UI passes -> one render encoder on the stage
//                         (consecutive ones share it: one load + store)
//                         blur pass -> stage -> scratch -> stage (skipped
//                         when its width is 0 or 1: a copy onto itself)
//                         stage -> drawable; present
//
// Latency: identical frames are not presented (each presented frame takes a
// display refresh, and the next input's frame would queue behind it); at
// most 2 drawables exist, and the next frame's drawable is taken right after
// a present: that waits for the refresh that shows the frame, so the next
// frame reads its input then and is shown at the following refresh.

#import <Metal/Metal.h>
#import <QuartzCore/QuartzCore.h>

////////////////////////////////
//~ rad: Generated Code

#include "render/metal/generated/render_metal.meta.h"

////////////////////////////////
//~ rad: Constants

// rad: frames whose instance buffers can be in use by the GPU at once
#define R_MTL_FRAMES_IN_FLIGHT 3

// rad: the stage's format: raddbg's on D3D11 (DXGI_FORMAT_R16G16B16A16_FLOAT;
// the GL backend uses RGBA32F, twice the memory traffic for no visible gain)
#define R_MTL_STAGE_FORMAT MTLPixelFormatRGBA16Float

// rad: texture upload staging buffers are allocated in chunks of this size
#define R_MTL_UPLOAD_CHUNK_SIZE MB(4)

////////////////////////////////
//~ rad: Shader Uniforms (match the structs in render_metal.mdesk)

typedef struct R_MTL_RectUniforms R_MTL_RectUniforms;
struct R_MTL_RectUniforms
{
  Vec4F32 xform[3];
  Mat4x4F32 texture_sample_channel_map;
  Vec2F32 viewport_size_px;
  F32 opacity;
  F32 _pad;
};

typedef struct R_MTL_BlurUniforms R_MTL_BlurUniforms;
struct R_MTL_BlurUniforms
{
  Vec4F32 rect;
  Vec4F32 corner_radii_px;
  Vec2F32 direction;
  Vec2F32 viewport_size;
  U32 blur_count;
  U32 _pad[3];
  Vec4F32 blur_kernel[32];
};

////////////////////////////////
//~ rad: Resource Types

typedef struct R_MTL_Tex2D R_MTL_Tex2D;
struct R_MTL_Tex2D
{
  R_MTL_Tex2D *next;
  id<MTLTexture> texture;
  R_ResourceKind kind;
  R_Tex2DFormat format;
  Vec2S32 size;
};

typedef struct R_MTL_PassListNode R_MTL_PassListNode;
struct R_MTL_PassListNode
{
  R_MTL_PassListNode *next;
  R_PassList *passes;
};

typedef struct R_MTL_Window R_MTL_Window;
struct R_MTL_Window
{
  R_MTL_Window *next;
  CAMetalLayer *layer;
  id<CAMetalDrawable> next_drawable;

  // rad: linear UI targets, at the window's size in pixels
  id<MTLTexture> stage;
  id<MTLTexture> stage_scratch;
  Vec2F32 last_client_rect_dim;
  F64 last_scale;

  // rad: per frame: the submitted passes, drawn at the end of the frame
  Arena *frame_arena;
  R_MTL_PassListNode *first_pass_list;
  R_MTL_PassListNode *last_pass_list;
  id<MTLCommandBuffer> cmd;
  id<MTLRenderCommandEncoder> stage_encoder; // open across UI passes
  B32 stage_cleared;
  B32 scratch_cleared;
  
  // rad: what the screen shows: the last presented frame's draw data hash
  // and the texture uploads done by then
  B32 shown_valid;
  U64 shown_hash;
  U64 shown_upload_count;

  // rad: instance data, one buffer per frame in flight
  dispatch_semaphore_t frame_semaphore;
  id<MTLBuffer> inst_buffers[R_MTL_FRAMES_IN_FLIGHT];
  U64 inst_buffer_idx;
  U64 inst_buffer_off;
};

////////////////////////////////
//~ rad: State

typedef struct R_MTL_State R_MTL_State;
struct R_MTL_State
{
  Arena *arena;
  id<MTLDevice> device;
  id<MTLCommandQueue> queue;
  id<MTLRenderPipelineState> rect_pipeline;
  id<MTLRenderPipelineState> blur_pipeline;
  id<MTLRenderPipelineState> present_pipeline;
  id<MTLSamplerState> samplers[R_Tex2DSampleKind_COUNT];
  id<MTLTexture> white_texture;
  R_MTL_Tex2D *free_tex2d;
  R_MTL_Window *free_window;

  // rad: texture uploads, blits in a command buffer committed before the
  // next frame's, so the GPU never reads a texture while it is written
  id<MTLCommandBuffer> upload_cmd;
  id<MTLBlitCommandEncoder> upload_blit;
  id<MTLBuffer> upload_chunk;
  U64 upload_chunk_off;
  U64 upload_count;
  
  // rad: frames not drawn (unchanged, or the window can't be seen)
  U64 skipped_frame_count;
};

global R_MTL_State *r_mtl_state = 0;

////////////////////////////////
//~ rad: Helpers

internal R_Handle r_mtl_handle_from_tex2d(R_MTL_Tex2D *t);
internal R_MTL_Tex2D *r_mtl_tex2d_from_handle(R_Handle h);
internal MTLPixelFormat r_mtl_pixel_format_from_tex2dformat(R_Tex2DFormat fmt);
internal B32 r_mtl_scissor_from_clip(Rng2F32 clip, Vec2F32 viewport_dim, MTLScissorRect *scissor_out);
internal void r_mtl_upload(id<MTLTexture> texture, Rng2S32 subrect, U64 bytes_per_pixel, void *data);
internal void r_mtl_flush_uploads(void);
internal id<MTLRenderCommandEncoder> r_mtl_window_encoder(R_MTL_Window *w, id<MTLTexture> target, B32 clear);
internal id<MTLRenderCommandEncoder> r_mtl_window_stage_encoder(R_MTL_Window *w);
internal void r_mtl_window_end_stage_encoder(R_MTL_Window *w);
internal U64 r_mtl_hash_from_passes(U64 seed, R_PassList *passes);
internal void r_mtl_window_encode_passes(R_MTL_Window *w, R_PassList *passes);

#endif // RENDER_METAL_H
