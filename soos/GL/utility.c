#include <windows.h>
#include <string.h>
#include <stdio.h>

#include "utility.h"

static const char* (WINAPI*glGetExtString)(void) = 0;
static const char* (WINAPI*glGetArbString)(HDC dc) = 0;
static HDC (WINAPI*glGetCurrentDC)(void) = 0;
static int (*glSwapControl)(int) = 0;

int uglSupportsExt(const char* ext)
{
    if(!glGetExtString && !glGetArbString)
    {
        glGetExtString = GL_GetProcAddress("wglGetExtensionsStringEXT");
        glGetArbString = GL_GetProcAddress("wglGetExtensionsStringARB");
        glGetCurrentDC = GL_GetProcAddress("wglGetCurrentDC");
    }
    
    const char* extstring = 0;
    
    if(glGetArbString && glGetCurrentDC)
        extstring = glGetArbString(glGetCurrentDC());
    
    if(!extstring && glGetExtString)
        extstring = glGetExtString();
    
    if(!extstring)
    {
        puts("No extstrng");
        return 0;
    }
    
    int len = strlen(ext);
    
    for(;;)
    {
        while(*extstring == ' ')
            extstring++;
        
        if(!*extstring)
            break;
        
        const char* end = extstring;
        
        while(*end && *end != ' ') end++;
        
        DWORD elen = end - extstring;
        
        //printf("EXT: %*.*s\n", elen, elen, extstring);
        
        if(elen != len)
        {
            extstring = end;
            continue;
        }
        
        if(!strncmp(ext, extstring, len))
            return 1;
            
        extstring = end;
    }
    
    return 0;
}

int uglSwapControl(int swap)
{
    if(!glSwapControl)
    {
        if(!uglSupportsExt("WGL_EXT_swap_control"))
            return 0;
        
        glSwapControl = GL_GetProcAddress("wglSwapIntervalEXT");
        if(!glSwapControl)
            return 0;
    }
    
    return glSwapControl(swap);
}

void uglSwapBuffers(HDC dc)
{
    wglSwapLayerBuffers(dc, WGL_SWAP_MAIN_PLANE);
}
