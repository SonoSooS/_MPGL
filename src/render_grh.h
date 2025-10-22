#pragma once

#define BIND_IF(cmd, variable, ...) if(SH_VALID(variable)) { cmd(variable, __VA_ARGS__); }

void grhPrintShaderInfoLog(GLuint shader);
void grhPrintProgramInfoLog(GLuint program);
