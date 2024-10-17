#pragma once

struct histogram
{
    ULONGLONG delta;
    ULONGLONG count;
};


extern GLuint shGrfFontShader;
extern GLint attrGrfFontColor;
extern GLint attrGrfFontVertex;
extern GLint attrGrfFontUV;
extern GLint uniGrfFontTime;
extern GLint uniGrfFontNPS;
extern GLint uniGrfFontTexi;
extern GLint uniGrfFontBgColor;

extern GLuint texGrfFont;

void grfDrawFontOverlay(void);
void grfInstallShader(void);
