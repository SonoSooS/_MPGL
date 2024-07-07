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


void grfFontSetBg(DWORD color);
void grfDrawFontString(int32_t x, int32_t y, int32_t scale, DWORD color, const char* text);
void grfDrawFontOverlay(void);
void grfInstallShader(void);
