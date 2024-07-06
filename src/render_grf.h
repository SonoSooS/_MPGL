#pragma once

void grfFontSetBg(DWORD color);
void grfDrawFontString(int32_t x, int32_t y, int32_t scale, DWORD color, const char* text);
void grfDrawFontOverlay(void);
void grfCompileFontShader(void);
