/*
 * GLAD: OpenGL 3.3 Core Profile Loader Implementation
 */

#include <glad/glad.h>
#include <stdio.h>

/* Forward declare glfwGetProcAddress to avoid including GLFW headers which pull in <GL/gl.h> */
void* glfwGetProcAddress(const char* namez);

/* Function pointers */
PFNGLGENBUFFERSPROC glad_glGenBuffers = NULL;
PFNGLDELETEBUFFERSPROC glad_glDeleteBuffers = NULL;
PFNGLBINDBUFFERPROC glad_glBindBuffer = NULL;
PFNGLBUFFERDATAPROC glad_glBufferData = NULL;
PFNGLBUFFERSUBDATAPROC glad_glBufferSubData = NULL;

PFNGLGENVERTEXARRAYSPROC glad_glGenVertexArrays = NULL;
PFNGLDELETEVERTEXARRAYSPROC glad_glDeleteVertexArrays = NULL;
PFNGLBINDVERTEXARRAYPROC glad_glBindVertexArray = NULL;

PFNGLENABLEVERTEXATTRIBARRAYPROC glad_glEnableVertexAttribArray = NULL;
PFNGLDISABLEVERTEXATTRIBARRAYPROC glad_glDisableVertexAttribArray = NULL;
PFNGLVERTEXATTRIBPOINTERPROC glad_glVertexAttribPointer = NULL;

PFNGLCREATESHADERPROC glad_glCreateShader = NULL;
PFNGLDELETESHADERPROC glad_glDeleteShader = NULL;
PFNGLSHADERSOURCEPROC glad_glShaderSource = NULL;
PFNGLCOMPILESHADERPROC glad_glCompileShader = NULL;
PFNGLGETSHADERIVPROC glad_glGetShaderiv = NULL;
PFNGLGETSHADERINFOLOGPROC glad_glGetShaderInfoLog = NULL;

PFNGLCREATEPROGRAMPROC glad_glCreateProgram = NULL;
PFNGLDELETEPROGRAMPROC glad_glDeleteProgram = NULL;
PFNGLATTACHSHADERPROC glad_glAttachShader = NULL;
PFNGLDETACHSHADERPROC glad_glDetachShader = NULL;
PFNGLLINKPROGRAMPROC glad_glLinkProgram = NULL;
PFNGLUSEPROGRAMPROC glad_glUseProgram = NULL;
PFNGLGETPROGRAMIVPROC glad_glGetProgramiv = NULL;
PFNGLGETPROGRAMINFOLOGPROC glad_glGetProgramInfoLog = NULL;

PFNGLGETUNIFORMLOCATIONPROC glad_glGetUniformLocation = NULL;
PFNGLUNIFORM1FPROC glad_glUniform1f = NULL;
PFNGLUNIFORM3FPROC glad_glUniform3f = NULL;
PFNGLUNIFORMMATRIX4FVPROC glad_glUniformMatrix4fv = NULL;

/* Function pointer types */

static void* get_proc(const char* namez) {
    void* result = glfwGetProcAddress(namez);
    if (!result) {
        printf("GLAD: Failed to load %s\n", namez);
    }
    return result;
}

int gladLoadGL(void) {
    printf("GLAD: Loading OpenGL extensions...\n");
    
    /* Load VBO functions */
    glad_glGenBuffers = (PFNGLGENBUFFERSPROC)get_proc("glGenBuffers");
    glad_glDeleteBuffers = (PFNGLDELETEBUFFERSPROC)get_proc("glDeleteBuffers");
    glad_glBindBuffer = (PFNGLBINDBUFFERPROC)get_proc("glBindBuffer");
    glad_glBufferData = (PFNGLBUFFERDATAPROC)get_proc("glBufferData");
    glad_glBufferSubData = (PFNGLBUFFERSUBDATAPROC)get_proc("glBufferSubData");
    
    /* Load VAO functions */
    glad_glGenVertexArrays = (PFNGLGENVERTEXARRAYSPROC)get_proc("glGenVertexArrays");
    glad_glDeleteVertexArrays = (PFNGLDELETEVERTEXARRAYSPROC)get_proc("glDeleteVertexArrays");
    glad_glBindVertexArray = (PFNGLBINDVERTEXARRAYPROC)get_proc("glBindVertexArray");
    
    /* Load vertex attribute functions */
    glad_glEnableVertexAttribArray = (PFNGLENABLEVERTEXATTRIBARRAYPROC)get_proc("glEnableVertexAttribArray");
    glad_glDisableVertexAttribArray = (PFNGLDISABLEVERTEXATTRIBARRAYPROC)get_proc("glDisableVertexAttribArray");
    glad_glVertexAttribPointer = (PFNGLVERTEXATTRIBPOINTERPROC)get_proc("glVertexAttribPointer");
    
    /* Load shader functions */
    glad_glCreateShader = (PFNGLCREATESHADERPROC)get_proc("glCreateShader");
    glad_glDeleteShader = (PFNGLDELETESHADERPROC)get_proc("glDeleteShader");
    glad_glShaderSource = (PFNGLSHADERSOURCEPROC)get_proc("glShaderSource");
    glad_glCompileShader = (PFNGLCOMPILESHADERPROC)get_proc("glCompileShader");
    glad_glGetShaderiv = (PFNGLGETSHADERIVPROC)get_proc("glGetShaderiv");
    glad_glGetShaderInfoLog = (PFNGLGETSHADERINFOLOGPROC)get_proc("glGetShaderInfoLog");
    
    /* Load program functions */
    glad_glCreateProgram = (PFNGLCREATEPROGRAMPROC)get_proc("glCreateProgram");
    glad_glDeleteProgram = (PFNGLDELETEPROGRAMPROC)get_proc("glDeleteProgram");
    glad_glAttachShader = (PFNGLATTACHSHADERPROC)get_proc("glAttachShader");
    glad_glDetachShader = (PFNGLDETACHSHADERPROC)get_proc("glDetachShader");
    glad_glLinkProgram = (PFNGLLINKPROGRAMPROC)get_proc("glLinkProgram");
    glad_glUseProgram = (PFNGLUSEPROGRAMPROC)get_proc("glUseProgram");
    glad_glGetProgramiv = (PFNGLGETPROGRAMIVPROC)get_proc("glGetProgramiv");
    glad_glGetProgramInfoLog = (PFNGLGETPROGRAMINFOLOGPROC)get_proc("glGetProgramInfoLog");
    
    /* Load uniform functions */
    glad_glGetUniformLocation = (PFNGLGETUNIFORMLOCATIONPROC)get_proc("glGetUniformLocation");
    glad_glUniform1f = (PFNGLUNIFORM1FPROC)get_proc("glUniform1f");
    glad_glUniform3f = (PFNGLUNIFORM3FPROC)get_proc("glUniform3f");
    glad_glUniformMatrix4fv = (PFNGLUNIFORMMATRIX4FVPROC)get_proc("glUniformMatrix4fv");
    
    /* Check if essential functions loaded */
    if (!glad_glGenBuffers || !glad_glBindBuffer || !glad_glBufferData) {
        printf("GLAD: Failed to load essential VBO functions\n");
        return 0;
    }
    
    printf("GLAD: OpenGL extensions loaded successfully\n");
    return 1;
}

/* Wrapper functions that call through function pointers */
void glGenBuffers(GLsizei n, GLuint *buffers) {
    glad_glGenBuffers(n, buffers);
}

void glDeleteBuffers(GLsizei n, const GLuint *buffers) {
    glad_glDeleteBuffers(n, buffers);
}

void glBindBuffer(GLenum target, GLuint buffer) {
    glad_glBindBuffer(target, buffer);
}

void glBufferData(GLenum target, GLsizeiptr size, const void *data, GLenum usage) {
    glad_glBufferData(target, size, data, usage);
}

void glBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const void *data) {
    glad_glBufferSubData(target, offset, size, data);
}

void glGenVertexArrays(GLsizei n, GLuint *arrays) {
    glad_glGenVertexArrays(n, arrays);
}

void glDeleteVertexArrays(GLsizei n, const GLuint *arrays) {
    glad_glDeleteVertexArrays(n, arrays);
}

void glBindVertexArray(GLuint array) {
    glad_glBindVertexArray(array);
}

void glEnableVertexAttribArray(GLuint index) {
    glad_glEnableVertexAttribArray(index);
}

void glDisableVertexAttribArray(GLuint index) {
    glad_glDisableVertexAttribArray(index);
}

void glVertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void *pointer) {
    glad_glVertexAttribPointer(index, size, type, normalized, stride, pointer);
}

GLuint glCreateShader(GLenum type) {
    return glad_glCreateShader(type);
}

void glDeleteShader(GLuint shader) {
    glad_glDeleteShader(shader);
}

void glShaderSource(GLuint shader, GLsizei count, const GLchar *const*string, const GLint *length) {
    glad_glShaderSource(shader, count, string, length);
}

void glCompileShader(GLuint shader) {
    glad_glCompileShader(shader);
}

void glGetShaderiv(GLuint shader, GLenum pname, GLint *params) {
    glad_glGetShaderiv(shader, pname, params);
}

void glGetShaderInfoLog(GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *infoLog) {
    glad_glGetShaderInfoLog(shader, bufSize, length, infoLog);
}

GLuint glCreateProgram(void) {
    return glad_glCreateProgram();
}

void glDeleteProgram(GLuint program) {
    glad_glDeleteProgram(program);
}

void glAttachShader(GLuint program, GLuint shader) {
    glad_glAttachShader(program, shader);
}

void glDetachShader(GLuint program, GLuint shader) {
    glad_glDetachShader(program, shader);
}

void glLinkProgram(GLuint program) {
    glad_glLinkProgram(program);
}

void glUseProgram(GLuint program) {
    glad_glUseProgram(program);
}

void glGetProgramiv(GLuint program, GLenum pname, GLint *params) {
    glad_glGetProgramiv(program, pname, params);
}

void glGetProgramInfoLog(GLuint program, GLsizei bufSize, GLsizei *length, GLchar *infoLog) {
    glad_glGetProgramInfoLog(program, bufSize, length, infoLog);
}

GLint glGetUniformLocation(GLuint program, const GLchar *name) {
    return glad_glGetUniformLocation(program, name);
}

void glUniform1f(GLint location, GLfloat v0) {
    glad_glUniform1f(location, v0);
}

void glUniform3f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2) {
    glad_glUniform3f(location, v0, v1, v2);
}

void glUniformMatrix4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) {
    glad_glUniformMatrix4fv(location, count, transpose, value);
}
