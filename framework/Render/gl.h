#pragma once

// Minimal OpenGL 3.3 core loader via SDL_GL_GetProcAddress
#include <cstddef>
#include <cstdint>

using GLenum = unsigned int;
using GLbitfield = unsigned int;
using GLuint = unsigned int;
using GLint = int;
using GLsizei = int;
using GLfloat = float;
using GLboolean = unsigned char;
using GLchar = char;
using GLsizeiptr = ptrdiff_t;
using GLintptr = ptrdiff_t;

constexpr GLenum GL_FALSE = 0;
constexpr GLenum GL_TRUE = 1;
constexpr GLenum GL_COLOR_BUFFER_BIT = 0x00004000;
constexpr GLenum GL_DEPTH_BUFFER_BIT = 0x00000100;
constexpr GLenum GL_TRIANGLES = 0x0004;
constexpr GLenum GL_LINES = 0x0001;
constexpr GLenum GL_FLOAT = 0x1406;
constexpr GLenum GL_UNSIGNED_INT = 0x1405;
constexpr GLenum GL_UNSIGNED_BYTE = 0x1401;
constexpr GLenum GL_ARRAY_BUFFER = 0x8892;
constexpr GLenum GL_ELEMENT_ARRAY_BUFFER = 0x8893;
constexpr GLenum GL_STATIC_DRAW = 0x88E4;
constexpr GLenum GL_DYNAMIC_DRAW = 0x88E8;
constexpr GLenum GL_TEXTURE_2D = 0x0DE1;
constexpr GLenum GL_TEXTURE0 = 0x84C0;
constexpr GLenum GL_RGBA = 0x1908;
constexpr GLenum GL_RGB = 0x1907;
constexpr GLenum GL_BLEND = 0x0BE2;
constexpr GLenum GL_ONE = 1;
constexpr GLenum GL_SRC_ALPHA = 0x0302;
constexpr GLenum GL_ONE_MINUS_SRC_ALPHA = 0x0303;
constexpr GLenum GL_NEAREST = 0x2600;
constexpr GLenum GL_LINEAR = 0x2601;
constexpr GLenum GL_TEXTURE_MIN_FILTER = 0x2801;
constexpr GLenum GL_TEXTURE_MAG_FILTER = 0x2800;
constexpr GLenum GL_TEXTURE_WRAP_S = 0x2802;
constexpr GLenum GL_TEXTURE_WRAP_T = 0x2803;
constexpr GLenum GL_UNPACK_ALIGNMENT = 0x0CF5;
constexpr GLenum GL_CLAMP_TO_EDGE = 0x812F;
constexpr GLenum GL_REPEAT = 0x2901;
constexpr GLenum GL_FRAGMENT_SHADER = 0x8B30;
constexpr GLenum GL_VERTEX_SHADER = 0x8B31;
constexpr GLenum GL_COMPILE_STATUS = 0x8B81;
constexpr GLenum GL_LINK_STATUS = 0x8B82;
constexpr GLenum GL_FRAMEBUFFER = 0x8D40;
constexpr GLenum GL_COLOR_ATTACHMENT0 = 0x8CE0;
constexpr GLenum GL_FRAMEBUFFER_COMPLETE = 0x8CD5;
constexpr GLenum GL_SCISSOR_TEST = 0x0C11;
constexpr GLenum GL_DST_COLOR = 0x0306;
constexpr GLenum GL_ZERO = 0;
constexpr GLenum GL_ONE_MINUS_DST_COLOR = 0x0307;
constexpr GLenum GL_TEXTURE1 = 0x84C1;
constexpr GLenum GL_RGBA8 = 0x8058;
constexpr GLenum GL_TRIANGLE_STRIP = 0x0005;

bool FW_LoadGL();

extern void (*glClear)(GLbitfield);
extern void (*glClearColor)(GLfloat, GLfloat, GLfloat, GLfloat);
extern void (*glViewport)(GLint, GLint, GLsizei, GLsizei);
extern void (*glEnable)(GLenum);
extern void (*glDisable)(GLenum);
extern void (*glBlendFunc)(GLenum, GLenum);
extern void (*glGenBuffers)(GLsizei, GLuint*);
extern void (*glBindBuffer)(GLenum, GLuint);
extern void (*glBufferData)(GLenum, GLsizeiptr, const void*, GLenum);
extern void (*glBufferSubData)(GLenum, GLintptr, GLsizeiptr, const void*);
extern void (*glDeleteBuffers)(GLsizei, const GLuint*);
extern void (*glGenVertexArrays)(GLsizei, GLuint*);
extern void (*glBindVertexArray)(GLuint);
extern void (*glDeleteVertexArrays)(GLsizei, const GLuint*);
extern void (*glEnableVertexAttribArray)(GLuint);
extern void (*glVertexAttribPointer)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*);
extern void (*glDrawArrays)(GLenum, GLint, GLsizei);
extern void (*glDrawElements)(GLenum, GLsizei, GLenum, const void*);
extern GLuint (*glCreateShader)(GLenum);
extern void (*glShaderSource)(GLuint, GLsizei, const GLchar* const*, const GLint*);
extern void (*glCompileShader)(GLuint);
extern void (*glGetShaderiv)(GLuint, GLenum, GLint*);
extern void (*glGetShaderInfoLog)(GLuint, GLsizei, GLsizei*, GLchar*);
extern void (*glDeleteShader)(GLuint);
extern GLuint (*glCreateProgram)();
extern void (*glAttachShader)(GLuint, GLuint);
extern void (*glLinkProgram)(GLuint);
extern void (*glGetProgramiv)(GLuint, GLenum, GLint*);
extern void (*glGetProgramInfoLog)(GLuint, GLsizei, GLsizei*, GLchar*);
extern void (*glUseProgram)(GLuint);
extern void (*glDeleteProgram)(GLuint);
extern GLint (*glGetUniformLocation)(GLuint, const GLchar*);
extern void (*glUniform1i)(GLint, GLint);
extern void (*glUniform1f)(GLint, GLfloat);
extern void (*glUniform2f)(GLint, GLfloat, GLfloat);
extern void (*glUniform4f)(GLint, GLfloat, GLfloat, GLfloat, GLfloat);
extern void (*glUniformMatrix4fv)(GLint, GLsizei, GLboolean, const GLfloat*);
extern void (*glGenTextures)(GLsizei, GLuint*);
extern void (*glBindTexture)(GLenum, GLuint);
extern void (*glTexParameteri)(GLenum, GLenum, GLint);
extern void (*glTexImage2D)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*);
extern void (*glDeleteTextures)(GLsizei, const GLuint*);
extern void (*glActiveTexture)(GLenum);
extern void (*glPixelStorei)(GLenum, GLint);
extern void (*glGenFramebuffers)(GLsizei, GLuint*);
extern void (*glBindFramebuffer)(GLenum, GLuint);
extern void (*glDeleteFramebuffers)(GLsizei, const GLuint*);
extern void (*glFramebufferTexture2D)(GLenum, GLenum, GLenum, GLuint, GLint);
extern GLenum (*glCheckFramebufferStatus)(GLenum);
extern void (*glScissor)(GLint, GLint, GLsizei, GLsizei);
extern void (*glUniform3f)(GLint, GLfloat, GLfloat, GLfloat);

// Compatibility aliases used by AssetCache
#define glad_glGenTextures glGenTextures
#define FW_USE_OPENGL 1
