#include <stdio.h>

#include "GL/core.h"

#include "render_grh.h"

void grhPrintShaderInfoLog(GLuint shader)
{
    GLint loglen = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &loglen);
    if(loglen > 0)
    {
        char ilog[256];
        GLsizei outloglen = 0;
        glGetShaderInfoLog(shader, 256, &outloglen, ilog);
        printf("%*.*s\n", outloglen, outloglen, ilog);
    }
    else
    {
        //puts("Unknown generic shader error");
    }
}

void grhPrintProgramInfoLog(GLuint program)
{
    GLint loglen = 0;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &loglen);
    if(loglen > 0)
    {
        char ilog[256];
        GLsizei outloglen = 0;
        glGetShaderInfoLog(program, 256, &outloglen, ilog);
        printf("%*.*s\n", outloglen, outloglen, ilog);
    }
    else
    {
        //puts("Unknown generic shader program error");
    }
}
