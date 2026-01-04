#pragma once


struct histogram_data
{
    ULONGLONG delta;
    ULONGLONG count;
};

struct histogram
{
    DWORD index_in;
    DWORD index_out;
    DWORD _unused;
    DWORD count;
    struct histogram_data data[0];
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
