#pragma once

#define BIND_IF(cmd, variable, ...) if(variable >= 0) { cmd(variable, __VA_ARGS__); }

void grhPrintShaderInfoLog(GLuint shader);
void grhPrintProgramInfoLog(GLuint program);
