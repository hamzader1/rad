// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

#ifndef RENDER_OPENGL_MAC_H
#define RENDER_OPENGL_MAC_H

// rad: Apple's gl3.h always declares prototypes for every GL function, but the
// GL backend declares global function pointers with the same names
// (R_OGL_ProcedureXList). Rename the colliding prototypes away while the
// header is included (the Linux EGL backend does this for 3 names). Keep this
// list equal to R_OGL_ProcedureXList.
#define GL_SILENCE_DEPRECATION
#define glGenBuffers glGenBuffers__apple
#define glBindBuffer glBindBuffer__apple
#define glDeleteBuffers glDeleteBuffers__apple
#define glGenVertexArrays glGenVertexArrays__apple
#define glBindVertexArray glBindVertexArray__apple
#define glGenFramebuffers glGenFramebuffers__apple
#define glDeleteFramebuffers glDeleteFramebuffers__apple
#define glCreateProgram glCreateProgram__apple
#define glCreateShader glCreateShader__apple
#define glShaderSource glShaderSource__apple
#define glCompileShader glCompileShader__apple
#define glGetShaderiv glGetShaderiv__apple
#define glGetShaderInfoLog glGetShaderInfoLog__apple
#define glGetProgramiv glGetProgramiv__apple
#define glGetProgramInfoLog glGetProgramInfoLog__apple
#define glAttachShader glAttachShader__apple
#define glLinkProgram glLinkProgram__apple
#define glValidateProgram glValidateProgram__apple
#define glDeleteShader glDeleteShader__apple
#define glUseProgram glUseProgram__apple
#define glGetUniformLocation glGetUniformLocation__apple
#define glGetAttribLocation glGetAttribLocation__apple
#define glEnableVertexAttribArray glEnableVertexAttribArray__apple
#define glVertexAttribPointer glVertexAttribPointer__apple
#define glBufferData glBufferData__apple
#define glBufferSubData glBufferSubData__apple
#define glBlendFuncSeparate glBlendFuncSeparate__apple
#define glUniform1f glUniform1f__apple
#define glUniform2f glUniform2f__apple
#define glUniform3f glUniform3f__apple
#define glUniform4f glUniform4f__apple
#define glUniform4fv glUniform4fv__apple
#define glUniformMatrix3fv glUniformMatrix3fv__apple
#define glUniformMatrix4fv glUniformMatrix4fv__apple
#define glUniform1i glUniform1i__apple
#define glTexImage3D glTexImage3D__apple
#define glTexSubImage3D glTexSubImage3D__apple
#define glGenerateMipmap glGenerateMipmap__apple
#define glBindAttribLocation glBindAttribLocation__apple
#define glBindFragDataLocation glBindFragDataLocation__apple
#define glBindFramebuffer glBindFramebuffer__apple
#define glActiveTexture glActiveTexture__apple
#define glVertexAttribDivisor glVertexAttribDivisor__apple
#define glDrawArraysInstanced glDrawArraysInstanced__apple
#define glDebugMessageCallback glDebugMessageCallback__apple
#define glFramebufferTexture2D glFramebufferTexture2D__apple
#define glBlitFramebuffer glBlitFramebuffer__apple
#include <OpenGL/gl3.h>
#undef glGenBuffers
#undef glBindBuffer
#undef glDeleteBuffers
#undef glGenVertexArrays
#undef glBindVertexArray
#undef glGenFramebuffers
#undef glDeleteFramebuffers
#undef glCreateProgram
#undef glCreateShader
#undef glShaderSource
#undef glCompileShader
#undef glGetShaderiv
#undef glGetShaderInfoLog
#undef glGetProgramiv
#undef glGetProgramInfoLog
#undef glAttachShader
#undef glLinkProgram
#undef glValidateProgram
#undef glDeleteShader
#undef glUseProgram
#undef glGetUniformLocation
#undef glGetAttribLocation
#undef glEnableVertexAttribArray
#undef glVertexAttribPointer
#undef glBufferData
#undef glBufferSubData
#undef glBlendFuncSeparate
#undef glUniform1f
#undef glUniform2f
#undef glUniform3f
#undef glUniform4f
#undef glUniform4fv
#undef glUniformMatrix3fv
#undef glUniformMatrix4fv
#undef glUniform1i
#undef glTexImage3D
#undef glTexSubImage3D
#undef glGenerateMipmap
#undef glBindAttribLocation
#undef glBindFragDataLocation
#undef glBindFramebuffer
#undef glActiveTexture
#undef glVertexAttribDivisor
#undef glDrawArraysInstanced
#undef glDebugMessageCallback
#undef glFramebufferTexture2D
#undef glBlitFramebuffer

typedef struct R_OGL_MAC_State R_OGL_MAC_State;
struct R_OGL_MAC_State
{
  Arena *arena;
  NSOpenGLContext *context;
  NSView *current_view;
  Vec2F32 current_view_dim;
  GLuint present_program;
  GLuint present_vao;
};

global R_OGL_MAC_State *r_ogl_mac_state = 0;

#endif // RENDER_OPENGL_MAC_H
