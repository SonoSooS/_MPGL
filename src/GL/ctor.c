#include <stdio.h>

#include "core.h"
#undef GLDEFINE

DWORD (WINAPI*mGetModuleBaseNameA)
(
    HANDLE  hProcess,
    HMODULE hModule,
    LPSTR   lpBaseName,
    DWORD   nSize
);

/*void __attribute__((noreturn)) WINAPI err() // not (void) !
{
    void* retaddr = __builtin_return_address(0);
    printf("Call nullptr to %016llX\n", retaddr);
    fflush(stdout);
    __builtin_trap();
}*/

void* __attribute__((noinline)) GL_GetProcAddress(const char* name)
{
    size_t ptr = (size_t)wglGetProcAddress(name);
    
    // why can't some drivers decide what to return as invalid value???
    if(!(ptr >> 12) || (ptr >> 12) == ((~0) >> 12))
    {
        // try fish out the missing function from the stock DLL (already a lost cause though)
        ptr = (size_t)GetProcAddress(GetModuleHandleA("opengl32"), name);
    }
    
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

extern void(WINAPI*glAAADummyFunc)(void);
extern void(WINAPI*glZZZDummyFuncEnd)(void);

int __attribute__((optimize("Os"))) GL_LinkFunctions()
{
    int cnt = 0;
    
    
    glAAADummyFunc = 0;
//#define GLDEFINE(proc, fn) { fn = (proc)GL_GetProcAddress(#fn); }
//#include "core.h"
    glZZZDummyFuncEnd = 0;
#undef GLDEFINE
    
    void** ptr = (void*)(((void**)&glAAADummyFunc) + 1);
    void** ptr_end = (void*)(((void**)&glZZZDummyFuncEnd) - 0);
    
    while(ptr < ptr_end)
    {
        const char* name = *ptr;
        if(name)
            *ptr = (void*)GL_GetProcAddress(name);
        
        ptr++;
    }
    
    if(glGetString)
    {
        printf("OpenGL version: %s\n", glGetString(GL_VERSION));
    }
    else puts("Unknown GL version - no glGetString... oof");
    
    return cnt;
}


void(WINAPI*glAAADummyFunc)(void) = 0;

#define GLDEFINE(proc, name) proc name = (const void*)(const char* const) #name ;
#include "core.h"
#undef GLDEFINE

void(WINAPI*glZZZDummyFuncEnd)(void) = 0;
