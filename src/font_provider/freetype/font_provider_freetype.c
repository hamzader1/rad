// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

////////////////////////////////
//~ rjf: Helpers

internal FP_FT_Font
fp_ft_font_from_handle(FP_Handle handle)
{
  FP_FT_Font result = {(FT_Face)handle.u64[0]};
  return result;
}

internal FP_Handle
fp_ft_handle_from_font(FP_FT_Font font)
{
  FP_Handle result = {(U64)font.face};
  return result;
}

////////////////////////////////
//~ rjf: Backend Implementations

fp_hook void
fp_init(void)
{
  Arena *arena = arena_alloc();
  fp_ft_state = push_array(arena, FP_FT_State, 1);
  fp_ft_state->arena = arena;
  FT_Init_FreeType(&fp_ft_state->library);
}

#if OS_MAC
internal String8
fp_ft_mac_path_from_family_name(Arena *arena, String8 name, String8 *postscript_name_out)
{
  // rad: like DirectWrite's FindFamilyName: a family name (case does not
  // matter) -> the file of its regular face, via CoreText
  String8 result = {0};
  @autoreleasepool
  {
    NSString *wanted = [[[NSString alloc] initWithBytes:name.str length:name.size encoding:NSUTF8StringEncoding] autorelease];
    NSString *family = nil;
    NSArray *families = [(NSArray *)CTFontManagerCopyAvailableFontFamilyNames() autorelease];
    for(NSString *f in families)
    {
      if([f caseInsensitiveCompare:wanted] == NSOrderedSame) { family = f; break; }
    }
    if(family != nil)
    {
      NSDictionary *attributes = @{(NSString *)kCTFontFamilyNameAttribute: family};
      CTFontDescriptorRef descriptor = CTFontDescriptorCreateWithAttributes((CFDictionaryRef)attributes);
      CTFontRef font = CTFontCreateWithFontDescriptor(descriptor, 12.0, 0);
      NSURL *url = [(NSURL *)CTFontCopyAttribute(font, kCTFontURLAttribute) autorelease];
      NSString *postscript_name = [(NSString *)CTFontCopyPostScriptName(font) autorelease];
      if(url != nil && url.path != nil)
      {
        result = str8_copy(arena, str8_cstring((char *)url.path.UTF8String));
        *postscript_name_out = str8_copy(arena, str8_cstring((char *)postscript_name.UTF8String));
      }
      CFRelease(font);
      CFRelease(descriptor);
    }
  }
  return result;
}
#endif

internal FT_Face
fp_ft_face_from_path(String8 path, String8 postscript_name)
{
  // rad: one face of a file; in a collection (.ttc) the one with the given
  // PostScript name, else the first
  Temp scratch = scratch_begin(0, 0);
  char *path_cstr = (char *)push_str8_copy(scratch.arena, path).str;
  FT_Face face = 0;
  if(FT_New_Face(fp_ft_state->library, path_cstr, 0, &face) == 0 && face->num_faces > 1 && postscript_name.size != 0)
  {
    FT_Long face_count = face->num_faces;
    for(FT_Long idx = 0; idx < face_count; idx += 1)
    {
      FT_Face candidate = 0;
      if(FT_New_Face(fp_ft_state->library, path_cstr, idx, &candidate) != 0) { continue; }
      const char *ps = FT_Get_Postscript_Name(candidate);
      if(ps != 0 && str8_match(str8_cstring((char *)ps), postscript_name, 0))
      {
        FT_Done_Face(face);
        face = candidate;
        break;
      }
      FT_Done_Face(candidate);
    }
  }
  scratch_end(scratch);
  return face;
}

fp_hook FP_Handle
fp_font_open(String8 path)
{
  Temp scratch = scratch_begin(0, 0);
  FP_FT_Font font = {0};

  //- rad: the path as given
  if(path.size != 0 && properties_from_file_path(path).created != 0)
  {
    font.face = fp_ft_face_from_path(path, str8_zero());
  }

#if OS_MAC
  //- rad: a file name in the font folders (like DirectWrite's user/system
  // font folders), then a family name
  if(font.face == 0 && path.size != 0)
  {
    String8 home = str8_cstring(getenv("HOME"));
    String8 folders[] =
    {
      push_str8f(scratch.arena, "%S/Library/Fonts", home),
      str8_lit("/Library/Fonts"),
      str8_lit("/System/Library/Fonts"),
      str8_lit("/System/Library/Fonts/Supplemental"),
    };
    for EachElement(idx, folders)
    {
      String8 candidate = push_str8f(scratch.arena, "%S/%S", folders[idx], path);
      if(properties_from_file_path(candidate).created != 0)
      {
        font.face = fp_ft_face_from_path(candidate, str8_zero());
        if(font.face != 0) { break; }
      }
    }
  }
  if(font.face == 0 && path.size != 0)
  {
    String8 postscript_name = {0};
    String8 family_path = fp_ft_mac_path_from_family_name(scratch.arena, path, &postscript_name);
    if(family_path.size != 0)
    {
      font.face = fp_ft_face_from_path(family_path, postscript_name);
    }
  }
#endif

  FP_Handle handle = fp_ft_handle_from_font(font);
  scratch_end(scratch);
  return handle;
}

fp_hook FP_Handle
fp_font_open_from_static_data_string(String8 *data_ptr)
{
  FP_FT_Font font = {0};
  FT_New_Memory_Face(fp_ft_state->library, data_ptr->str, (FT_Long)data_ptr->size, 0, &font.face);
  FP_Handle handle = fp_ft_handle_from_font(font);
  return handle;
}

fp_hook void
fp_font_close(FP_Handle handle)
{
  FP_FT_Font font = fp_ft_font_from_handle(handle);
  if(font.face != 0)
  {
    FT_Done_Face(font.face);
  }
}

fp_hook FP_Metrics
fp_metrics_from_font(FP_Handle handle)
{
  FP_FT_Font font = fp_ft_font_from_handle(handle);
  FP_Metrics result = {0};
  if(font.face != 0)
  {
    result.design_units_per_em = (F32)(font.face->units_per_EM);
    result.ascent              = (F32)font.face->ascender;
    result.descent             = -(F32)font.face->descender;
    result.line_gap            = (F32)(font.face->height - font.face->ascender + font.face->descender);
    result.capital_height      = (F32)(font.face->ascender);
  }
  return result;
}

fp_hook FP_RasterResult
fp_raster(Arena *arena, FP_Handle handle, F32 size, FP_RasterFlags flags, String8 string)
{
  ProfBeginFunction();
  FP_FT_Font font = fp_ft_font_from_handle(handle);
  FP_RasterResult result = {0};
  if(font.face != 0)
  {
    Temp scratch = scratch_begin(&arena, 1);
    
    //- rjf: unpack font
    FT_Face face = font.face;
    FT_Set_Pixel_Sizes(face, 0, (FT_UInt)((96.f/72.f) * size));
    S64 ascent  = face->size->metrics.ascender >> 6;
    S64 descent = abs_s64(face->size->metrics.descender >> 6);
    S64 height  = face->size->metrics.height >> 6;
    
    //- rjf: unpack string
    String32 string32 = str32_from_8(scratch.arena, string);
    
    //- rjf: measure
    S32 total_width = 0;
    for EachIndex(idx, string32.size)
    {
      FT_Load_Char(face, string32.str[idx], FT_LOAD_RENDER);
      total_width += (face->glyph->advance.x >> 6);
    }
    
    //- rjf: allocate & fill atlas w/ rasterization
    Vec2S16 dim = {(S16)total_width+1, height+1};
    U64 atlas_size = dim.x * dim.y * 4;
    U8 *atlas = push_array(arena, U8, atlas_size);
    S32 baseline = ascent;
    S32 atlas_write_x = 0;
    for EachIndex(idx, string32.size)
    {
      FT_Load_Char(face, string32.str[idx], FT_LOAD_RENDER);
      FT_Bitmap *bmp = &face->glyph->bitmap;
      S32 top = face->glyph->bitmap_top;
      S32 left = face->glyph->bitmap_left;
      for(S32 row = 0; row < (S32)bmp->rows; row += 1)
      {
        S32 y = baseline - top + row;
        for(S32 col = 0; col < (S32)bmp->width; col += 1)
        {
          S32 x = atlas_write_x + left + col;
          U64 off = (y*dim.x + x)*4;
          if(off+4 <= atlas_size)
          {
            atlas[off+0] = 255;
            atlas[off+1] = 255;
            atlas[off+2] = 255;
            atlas[off+3] = bmp->buffer[row*bmp->pitch + col];
          }
        }
      }
      atlas_write_x += (face->glyph->advance.x >> 6);
    }
    
    //- rjf: fill result
    result.atlas_dim = dim;
    result.advance   = (F32)total_width;
    result.atlas     = atlas;
    scratch_end(scratch);
  }
  ProfEnd();
  return result;
}
