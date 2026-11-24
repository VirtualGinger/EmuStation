#pragma once

#ifdef _WIN32

#include <SDL_opengl.h>

// opengl32.dll only exports GL 1.1, so everything newer is fetched from the driver at runtime.
#define EMU_GL_FUNCTIONS(X)                                              \
    X(PFNGLCREATESHADERPROC, glCreateShader)                             \
    X(PFNGLSHADERSOURCEPROC, glShaderSource)                             \
    X(PFNGLCOMPILESHADERPROC, glCompileShader)                           \
    X(PFNGLGETSHADERIVPROC, glGetShaderiv)                               \
    X(PFNGLGETSHADERINFOLOGPROC, glGetShaderInfoLog)                     \
    X(PFNGLDELETESHADERPROC, glDeleteShader)                             \
    X(PFNGLCREATEPROGRAMPROC, glCreateProgram)                           \
    X(PFNGLATTACHSHADERPROC, glAttachShader)                             \
    X(PFNGLLINKPROGRAMPROC, glLinkProgram)                               \
    X(PFNGLGETPROGRAMIVPROC, glGetProgramiv)                             \
    X(PFNGLGETPROGRAMINFOLOGPROC, glGetProgramInfoLog)                   \
    X(PFNGLDELETEPROGRAMPROC, glDeleteProgram)                           \
    X(PFNGLUSEPROGRAMPROC, glUseProgram)                                 \
    X(PFNGLGETUNIFORMBLOCKINDEXPROC, glGetUniformBlockIndex)             \
    X(PFNGLUNIFORMBLOCKBINDINGPROC, glUniformBlockBinding)               \
    X(PFNGLGENBUFFERSPROC, glGenBuffers)                                 \
    X(PFNGLDELETEBUFFERSPROC, glDeleteBuffers)                           \
    X(PFNGLBINDBUFFERPROC, glBindBuffer)                                 \
    X(PFNGLBUFFERDATAPROC, glBufferData)                                 \
    X(PFNGLBUFFERSUBDATAPROC, glBufferSubData)                           \
    X(PFNGLBINDBUFFERBASEPROC, glBindBufferBase)                         \
    X(PFNGLGENVERTEXARRAYSPROC, glGenVertexArrays)                       \
    X(PFNGLDELETEVERTEXARRAYSPROC, glDeleteVertexArrays)                 \
    X(PFNGLBINDVERTEXARRAYPROC, glBindVertexArray)                       \
    X(PFNGLGENFRAMEBUFFERSPROC, glGenFramebuffers)                       \
    X(PFNGLDELETEFRAMEBUFFERSPROC, glDeleteFramebuffers)                 \
    X(PFNGLBINDFRAMEBUFFERPROC, glBindFramebuffer)                       \
    X(PFNGLFRAMEBUFFERTEXTURE2DPROC, glFramebufferTexture2D)             \
    X(PFNGLCHECKFRAMEBUFFERSTATUSPROC, glCheckFramebufferStatus)         \
    X(PFNGLBLITFRAMEBUFFERPROC, glBlitFramebuffer)

#define EMU_DECLARE_GL_FUNCTION(type, name) extern type name;
EMU_GL_FUNCTIONS(EMU_DECLARE_GL_FUNCTION)
#undef EMU_DECLARE_GL_FUNCTION

bool load_gl_functions();

#else

#include <GLES3/gl3.h>

inline bool load_gl_functions() { return true; }

#endif
