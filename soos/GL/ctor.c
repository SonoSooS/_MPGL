#include "core.h"
#include <stdio.h>

DWORD (WINAPI*mGetModuleBaseNameA)
(
    HANDLE  hProcess,
    HMODULE hModule,
    LPSTR   lpBaseName,
    DWORD   nSize
);

/*void __attribute__((noreturn)) WINAPI err()
{
    void* retaddr = __builtin_return_address(0);
    printf("Call nullptr to %016llX\n", retaddr);
    fflush(stdout);
    *((size_t*)0) = 0xC0C0CACE;
    //return;
}*/

void* __attribute__((noinline)) GL_GetProcAddress(const char* name)
{
    size_t ptr = (size_t)wglGetProcAddress(name);
    
    if(!(ptr >> 12) || (ptr >> 12) == ((~0) >> 12))
        ptr = (size_t)GetProcAddress(GetModuleHandleA("opengl32"), name);
    
    if(!ptr)
    {
        printf("NG - %s\n", name);
        return (void*)name;
        //return err;
    }
    
    return (void*)ptr;
    
    /*HANDLE currproc = GetCurrentProcess();
    MEMORY_BASIC_INFORMATION mbi;
    char namebuf[32];
    
    if(mGetModuleBaseNameA && VirtualQuery((void*)ptr, &mbi, sizeof(mbi)))
    {
        HMODULE mod = (HMODULE)mbi.AllocationBase;
        
        if(mGetModuleBaseNameA(currproc, mod, namebuf, sizeof(namebuf)))
        {
            printf("OK - %s (%016X) in %s\n", name, ptr, namebuf);
            return (void*)ptr;
        }
    }
    
    printf("OK - %s (%016X)\n", name, ptr);
    return (void*)ptr;*/
}

extern void(WINAPI*glDummyFunc)(void);
extern void(WINAPI*glDummyFuncEnd)(void);

int __attribute__((optimize("Os"))) GL_LinkFunctions()
{
    int cnt = 0;
    
#define GLINKFUNC(fn) { fn = (void*)GL_GetProcAddress(#fn); }
    
    //ZeroMemory(&glDummyFunc, (&glDummyFuncEnd - &glDummyFunc) * 8);
    
    glDummyFunc = 0;
    
    GLINKFUNC(glCullFace)
    GLINKFUNC(glClear)
    GLINKFUNC(glClearColor)
    GLINKFUNC(glDisable)
    GLINKFUNC(glEnable)
    GLINKFUNC(glFinish)
    GLINKFUNC(glFlush)
    GLINKFUNC(glBlendFunc)
    GLINKFUNC(glDepthFunc)
    GLINKFUNC(glGetString)
    GLINKFUNC(glViewport)
    GLINKFUNC(glDrawArrays)
    GLINKFUNC(glDrawElements)
    GLINKFUNC(glBlendFuncSeparate)
    GLINKFUNC(glBlendColor)
    GLINKFUNC(glBlendEquation)
    GLINKFUNC(glBindBuffer)
    GLINKFUNC(glDeleteBuffers)
    GLINKFUNC(glGenBuffers)
    GLINKFUNC(glBufferData)
    GLINKFUNC(glBufferSubData)
    GLINKFUNC(glBlendEquationSeparate)
    GLINKFUNC(glDrawBuffers)
    GLINKFUNC(glAttachShader)
    GLINKFUNC(glCompileShader)
    GLINKFUNC(glCreateProgram)
    GLINKFUNC(glCreateShader)
    GLINKFUNC(glDeleteProgram)
    GLINKFUNC(glDeleteShader)
    GLINKFUNC(glDetachShader)
    GLINKFUNC(glDisableVertexAttribArray)
    GLINKFUNC(glEnableVertexAttribArray)
    GLINKFUNC(glGetAttribLocation)
    GLINKFUNC(glGetProgramiv)
    GLINKFUNC(glGetProgramInfoLog)
    GLINKFUNC(glGetShaderiv)
    GLINKFUNC(glGetShaderInfoLog)
    GLINKFUNC(glLinkProgram)
    GLINKFUNC(glShaderSource)
    GLINKFUNC(glUseProgram)
    GLINKFUNC(glVertexAttribPointer)
    
    GLINKFUNC(glBindVertexArray)
    GLINKFUNC(glDeleteVertexArrays)
    GLINKFUNC(glGenVertexArrays)
    
    GLINKFUNC(glDebugMessageCallbackARB)
    
    glDummyFuncEnd = 0;
    
#undef GLINKFUNC
    
    if(glGetString)
    {
        printf("OpenGL version: %s\n", glGetString(GL_VERSION));
    }
    else puts("Unknown GL version - no glGetString... oof");
    
    return cnt;
}

void(WINAPI*glDummyFunc)(void);

#undef GLAPI
#define GLAPI
#include "core.h"

void(WINAPI*glDummyFuncEnd)(void);
