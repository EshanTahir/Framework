#include "gl.h"
#include "SDL.h"
#include "Core/log.h"

#include <cstring>

#define LOAD(name) name = reinterpret_cast<decltype(name)>(SDL_GL_GetProcAddress(#name)); if (!name) { FW_CORE_ERROR("Missing GL fn {0}", #name); ok = false; }

void (*glClear)(GLbitfield) = nullptr;
void (*glClearColor)(GLfloat, GLfloat, GLfloat, GLfloat) = nullptr;
void (*glViewport)(GLint, GLint, GLsizei, GLsizei) = nullptr;
void (*glEnable)(GLenum) = nullptr;
void (*glDisable)(GLenum) = nullptr;
void (*glBlendFunc)(GLenum, GLenum) = nullptr;
void (*glGenBuffers)(GLsizei, GLuint*) = nullptr;
void (*glBindBuffer)(GLenum, GLuint) = nullptr;
void (*glBufferData)(GLenum, GLsizeiptr, const void*, GLenum) = nullptr;
void (*glBufferSubData)(GLenum, GLintptr, GLsizeiptr, const void*) = nullptr;
void (*glDeleteBuffers)(GLsizei, const GLuint*) = nullptr;
void (*glGenVertexArrays)(GLsizei, GLuint*) = nullptr;
void (*glBindVertexArray)(GLuint) = nullptr;
void (*glDeleteVertexArrays)(GLsizei, const GLuint*) = nullptr;
void (*glEnableVertexAttribArray)(GLuint) = nullptr;
void (*glVertexAttribPointer)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*) = nullptr;
void (*glDrawArrays)(GLenum, GLint, GLsizei) = nullptr;
void (*glDrawElements)(GLenum, GLsizei, GLenum, const void*) = nullptr;
GLuint (*glCreateShader)(GLenum) = nullptr;
void (*glShaderSource)(GLuint, GLsizei, const GLchar* const*, const GLint*) = nullptr;
void (*glCompileShader)(GLuint) = nullptr;
void (*glGetShaderiv)(GLuint, GLenum, GLint*) = nullptr;
void (*glGetShaderInfoLog)(GLuint, GLsizei, GLsizei*, GLchar*) = nullptr;
void (*glDeleteShader)(GLuint) = nullptr;
GLuint (*glCreateProgram)() = nullptr;
void (*glAttachShader)(GLuint, GLuint) = nullptr;
void (*glLinkProgram)(GLuint) = nullptr;
void (*glGetProgramiv)(GLuint, GLenum, GLint*) = nullptr;
void (*glGetProgramInfoLog)(GLuint, GLsizei, GLsizei*, GLchar*) = nullptr;
void (*glUseProgram)(GLuint) = nullptr;
void (*glDeleteProgram)(GLuint) = nullptr;
GLint (*glGetUniformLocation)(GLuint, const GLchar*) = nullptr;
void (*glUniform1i)(GLint, GLint) = nullptr;
void (*glUniform1f)(GLint, GLfloat) = nullptr;
void (*glUniform2f)(GLint, GLfloat, GLfloat) = nullptr;
void (*glUniform4f)(GLint, GLfloat, GLfloat, GLfloat, GLfloat) = nullptr;
void (*glUniformMatrix4fv)(GLint, GLsizei, GLboolean, const GLfloat*) = nullptr;
void (*glGenTextures)(GLsizei, GLuint*) = nullptr;
void (*glBindTexture)(GLenum, GLuint) = nullptr;
void (*glTexParameteri)(GLenum, GLenum, GLint) = nullptr;
void (*glTexImage2D)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*) = nullptr;
void (*glDeleteTextures)(GLsizei, const GLuint*) = nullptr;
void (*glActiveTexture)(GLenum) = nullptr;
void (*glPixelStorei)(GLenum, GLint) = nullptr;
void (*glGenFramebuffers)(GLsizei, GLuint*) = nullptr;
void (*glBindFramebuffer)(GLenum, GLuint) = nullptr;
void (*glDeleteFramebuffers)(GLsizei, const GLuint*) = nullptr;
void (*glFramebufferTexture2D)(GLenum, GLenum, GLenum, GLuint, GLint) = nullptr;
GLenum (*glCheckFramebufferStatus)(GLenum) = nullptr;
void (*glScissor)(GLint, GLint, GLsizei, GLsizei) = nullptr;
void (*glUniform3f)(GLint, GLfloat, GLfloat, GLfloat) = nullptr;

bool FW_LoadGL()
{
    bool ok = true;
    LOAD(glClear);
    LOAD(glClearColor);
    LOAD(glViewport);
    LOAD(glEnable);
    LOAD(glDisable);
    LOAD(glBlendFunc);
    LOAD(glGenBuffers);
    LOAD(glBindBuffer);
    LOAD(glBufferData);
    LOAD(glBufferSubData);
    LOAD(glDeleteBuffers);
    LOAD(glGenVertexArrays);
    LOAD(glBindVertexArray);
    LOAD(glDeleteVertexArrays);
    LOAD(glEnableVertexAttribArray);
    LOAD(glVertexAttribPointer);
    LOAD(glDrawArrays);
    LOAD(glDrawElements);
    LOAD(glCreateShader);
    LOAD(glShaderSource);
    LOAD(glCompileShader);
    LOAD(glGetShaderiv);
    LOAD(glGetShaderInfoLog);
    LOAD(glDeleteShader);
    LOAD(glCreateProgram);
    LOAD(glAttachShader);
    LOAD(glLinkProgram);
    LOAD(glGetProgramiv);
    LOAD(glGetProgramInfoLog);
    LOAD(glUseProgram);
    LOAD(glDeleteProgram);
    LOAD(glGetUniformLocation);
    LOAD(glUniform1i);
    LOAD(glUniform1f);
    LOAD(glUniform2f);
    LOAD(glUniform4f);
    LOAD(glUniformMatrix4fv);
    LOAD(glGenTextures);
    LOAD(glBindTexture);
    LOAD(glTexParameteri);
    LOAD(glTexImage2D);
    LOAD(glDeleteTextures);
    LOAD(glActiveTexture);
    LOAD(glPixelStorei);
    LOAD(glGenFramebuffers);
    LOAD(glBindFramebuffer);
    LOAD(glDeleteFramebuffers);
    LOAD(glFramebufferTexture2D);
    LOAD(glCheckFramebufferStatus);
    LOAD(glScissor);
    LOAD(glUniform3f);
    return ok;
}
