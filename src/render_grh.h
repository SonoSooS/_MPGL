#pragma once

#define BIND_IF(variable, cmd, ...) if(variable >= 0) { cmd(variable, __VA_ARGS__); }

void grhPrintShaderInfoLog(GLuint shader);
void grhPrintProgramInfoLog(GLuint program);
