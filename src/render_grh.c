#include "GL/core.h"

#include "render_grh.h"

void grhPrintShaderInfo(GLuint shader)
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
