#ifdef _WIN32

#include "gl_functions.h"

#include <SDL.h>

#include <cstdio>

#define EMU_DEFINE_GL_FUNCTION(type, name) type name = nullptr;
EMU_GL_FUNCTIONS(EMU_DEFINE_GL_FUNCTION)
#undef EMU_DEFINE_GL_FUNCTION

bool load_gl_functions() {
    bool isComplete = true;

#define EMU_LOAD_GL_FUNCTION(type, name)                                   \
    name = reinterpret_cast<type>(SDL_GL_GetProcAddress(#name));           \
    if (!name) {                                                           \
        std::fprintf(stderr, "Missing OpenGL function: %s\n", #name);      \
        isComplete = false;                                                \
    }
    EMU_GL_FUNCTIONS(EMU_LOAD_GL_FUNCTION)
#undef EMU_LOAD_GL_FUNCTION

    return isComplete;
}

#endif
