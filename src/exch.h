static void writecon(const char* str)
{
    fputs(str, stdout);
}

static void wriptr(size_t ptr)
{
#ifdef _M_IX86
    printf("%08X", ptr);
#else
    printf("%016llX", ptr);
#endif
}

static void hex32(DWORD wrd)
{
    printf("%08X", wrd);
}

static void wriptra(size_t ptr)
{
    HANDLE currproc = GetCurrentProcess();
    HMODULE mod;
    char namebuf[32];
    MEMORY_BASIC_INFORMATION mbi;
    
    wriptr(ptr);
    
    if(!VirtualQuery((LPCVOID)ptr, &mbi, sizeof(mbi)))
        return;
    
    mod = (HMODULE)mbi.AllocationBase;
    
    namebuf[0] = '\0';
    if(!mGetModuleBaseNameA || !mGetModuleBaseNameA(currproc, mod, namebuf, sizeof(namebuf)))
    {
        namebuf[0] = '?';
        namebuf[1] = '?';
        namebuf[2] = '?';
        namebuf[3] = '\0';
    }
    
    namebuf[31] = 0;
    writecon(" (");
    writecon(namebuf);
    writecon("+");
    hex32((DWORD)((BYTE*)ptr - (BYTE*)mod));
    writecon(")");
}

static LONG WINAPI crashhandler(LPEXCEPTION_POINTERS exc)
{
    //stdconout = CreateFileA("_mmcrash.log", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    
#ifdef _M_IX86
    BYTE* stacktrace[16];
    
    DWORD ret = 0; //RtlCaptureStackBackTrace(0, (sizeof(stacktrace) / sizeof(size_t)), (void**)stacktrace, 0);
#else
    BYTE* stacktrace[62];
    DWORD ret = RtlCaptureStackBackTrace(7, (sizeof(stacktrace) / sizeof(size_t)) - 7, (void**)stacktrace, 0);
#endif
    
    writecon("\n  ========[Stacktrace]========\n ");
    
    if(ret)
    {
        while(ret--)
        {
            writecon("\n  - ");
            wriptra((size_t)stacktrace[ret]);
        }
        writecon("\n ");
    }
    else
    {
        writecon("  * No stack trace available\n");
    }
    
#ifdef _M_IX86
    writecon("\n Stack values:");
    {
        writecon("\n    "); wriptra(*(((size_t*)exc->ContextRecord->Esp) -9));
        writecon("\n    "); wriptra(*(((size_t*)exc->ContextRecord->Esp) -8));
        writecon("\n    "); wriptra(*(((size_t*)exc->ContextRecord->Esp) -7));
        writecon("\n    "); wriptra(*(((size_t*)exc->ContextRecord->Esp) -6));
        writecon("\n    "); wriptra(*(((size_t*)exc->ContextRecord->Esp) -5));
        writecon("\n    "); wriptra(*(((size_t*)exc->ContextRecord->Esp) -4));
        writecon("\n    "); wriptra(*(((size_t*)exc->ContextRecord->Esp) -3));
        writecon("\n    "); wriptra(*(((size_t*)exc->ContextRecord->Esp) -2));
        writecon("\n    "); wriptra(*(((size_t*)exc->ContextRecord->Esp) -1));
        writecon("\n    "); wriptra(*(((size_t*)exc->ContextRecord->Esp)   )); writecon(" <--");
        writecon("\n    "); wriptra(*(((size_t*)exc->ContextRecord->Esp) +1));
        writecon("\n    "); wriptra(*(((size_t*)exc->ContextRecord->Esp) +2));
        writecon("\n    "); wriptra(*(((size_t*)exc->ContextRecord->Esp) +3));
        writecon("\n    "); wriptra(*(((size_t*)exc->ContextRecord->Esp) +4));
        writecon("\n    "); wriptra(*(((size_t*)exc->ContextRecord->Esp) +5));
        writecon("\n    "); wriptra(*(((size_t*)exc->ContextRecord->Esp) +6));
    }
#endif
    
    writecon("\n\n  ========[Exception]========\n ");

#define arre(wat) case wat: writecon(#wat); break
    
    const struct _EXCEPTION_RECORD* currecord = exc->ExceptionRecord;
    for(;;)
    {
        writecon("\n  Code: ");
        switch(exc->ExceptionRecord->ExceptionCode)
        {
            arre(EXCEPTION_ACCESS_VIOLATION);
            arre(EXCEPTION_ARRAY_BOUNDS_EXCEEDED);
            arre(EXCEPTION_BREAKPOINT);
            arre(EXCEPTION_DATATYPE_MISALIGNMENT);
            arre(EXCEPTION_FLT_DENORMAL_OPERAND);
            arre(EXCEPTION_FLT_DIVIDE_BY_ZERO);
            arre(EXCEPTION_FLT_INEXACT_RESULT);
            arre(EXCEPTION_FLT_INVALID_OPERATION);
            arre(EXCEPTION_FLT_OVERFLOW);
            arre(EXCEPTION_FLT_STACK_CHECK);
            arre(EXCEPTION_FLT_UNDERFLOW);
            arre(EXCEPTION_ILLEGAL_INSTRUCTION);
            arre(EXCEPTION_IN_PAGE_ERROR);
            arre(EXCEPTION_INT_DIVIDE_BY_ZERO);
            arre(EXCEPTION_INT_OVERFLOW);
            arre(EXCEPTION_INVALID_DISPOSITION);
            arre(EXCEPTION_NONCONTINUABLE_EXCEPTION);
            arre(EXCEPTION_PRIV_INSTRUCTION);
            arre(EXCEPTION_SINGLE_STEP);
            arre(EXCEPTION_STACK_OVERFLOW);
            default: writecon("unknown "); hex32(exc->ExceptionRecord->ExceptionCode); break;
        }
        writecon("\n  Address: "); wriptr((size_t)exc->ExceptionRecord->ExceptionAddress);
        
        if(exc->ExceptionRecord->NumberParameters)
        {
            writecon("\n Param[0]: "); wriptr(exc->ExceptionRecord->ExceptionInformation[0]);
            writecon("\n Param[1]: "); wriptr(exc->ExceptionRecord->ExceptionInformation[1]);
            writecon("\n Param[2]: "); wriptr(exc->ExceptionRecord->ExceptionInformation[2]);
        }
        writecon("\n ");
        
        if(currecord->ExceptionRecord && currecord->ExceptionRecord != currecord)
        {
            currecord = currecord->ExceptionRecord;
            continue;
        }
        else
            break;
    }
#undef arre
    
    writecon("\n  ========[Regdump]========\n ");
    
#ifdef _M_AMD64
    writecon("\n PC : "); wriptra(exc->ContextRecord->Rip);
    
    writecon("\n RAX: "); wriptr(exc->ContextRecord->Rax);
    writecon("   RCX: "); wriptr(exc->ContextRecord->Rcx);
    writecon("   RDX: "); wriptr(exc->ContextRecord->Rdx);
    writecon("   RBX: "); wriptr(exc->ContextRecord->Rbx);
    writecon("\n RSP: "); wriptr(exc->ContextRecord->Rsp);
    writecon("   RBP: "); wriptr(exc->ContextRecord->Rbp);
    writecon("   RSI: "); wriptr(exc->ContextRecord->Rsi);
    writecon("   RDI: "); wriptr(exc->ContextRecord->Rdi);
    writecon("\n DR0: "); wriptr(exc->ContextRecord->Dr0);
    writecon("   DR1: "); wriptr(exc->ContextRecord->Dr1);
    writecon("   DR2: "); wriptr(exc->ContextRecord->Dr2);
    writecon("   DR3: "); wriptr(exc->ContextRecord->Dr3);
    writecon("\n DR6: "); wriptr(exc->ContextRecord->Dr6);
    writecon("   DR7: "); wriptr(exc->ContextRecord->Dr7);
    writecon("   R8 : "); wriptr(exc->ContextRecord->R8);
    writecon("   R9 : "); wriptr(exc->ContextRecord->R9);
    writecon("\n R10: "); wriptr(exc->ContextRecord->R10);
    writecon("   R11: "); wriptr(exc->ContextRecord->R11);
    writecon("   R12: "); wriptr(exc->ContextRecord->R12);
    writecon("   R13: "); wriptr(exc->ContextRecord->R13);
    writecon("\n R14: "); wriptr(exc->ContextRecord->R14);
    writecon("   R15: "); wriptr(exc->ContextRecord->R15);
    writecon("\n LBT: "); wriptr(exc->ContextRecord->LastBranchToRip);
    writecon("   LBF: "); wriptr(exc->ContextRecord->LastBranchFromRip);
    writecon("   LET: "); wriptr(exc->ContextRecord->LastExceptionToRip);
    writecon("   LEF: "); wriptr(exc->ContextRecord->LastExceptionFromRip);
#else
#ifdef _M_ARM64
    writecon("\n PC : "); wriptra(exc->ContextRecord->Pc);
    
    writecon("   LR : "); wriptr(exc->ContextRecord->Lr);
    writecon("   SP : "); wriptr(exc->ContextRecord->Sp);
    writecon("\n FP : "); wriptr(exc->ContextRecord->Fp);
    writecon("   CPS: "); wriptr(exc->ContextRecord->Cpsr);
    writecon("   FPS: "); wriptr(exc->ContextRecord->Fpsr);
    writecon("   FPC: "); wriptr(exc->ContextRecord->Fpcr);
    writecon("\n X0 : "); wriptr(exc->ContextRecord->X[0]);
    writecon("   X1 : "); wriptr(exc->ContextRecord->X[1]);
    writecon("   X2 : "); wriptr(exc->ContextRecord->X[2]);
    writecon("   X3 : "); wriptr(exc->ContextRecord->X[3]);
    writecon("\n X4 : "); wriptr(exc->ContextRecord->X[4]);
    writecon("   X5 : "); wriptr(exc->ContextRecord->X[5]);
    writecon("   X6 : "); wriptr(exc->ContextRecord->X[6]);
    writecon("   X7 : "); wriptr(exc->ContextRecord->X[7]);
    writecon("\n X8 : "); wriptr(exc->ContextRecord->X[8]);
    writecon("   X9 : "); wriptr(exc->ContextRecord->X[9]);
    writecon("   X10: "); wriptr(exc->ContextRecord->X[10]);
    writecon("   X11: "); wriptr(exc->ContextRecord->X[11]);
    writecon("\n X12: "); wriptr(exc->ContextRecord->X[12]);
    writecon("   X13: "); wriptr(exc->ContextRecord->X[13]);
    writecon("   X14: "); wriptr(exc->ContextRecord->X[14]);
    writecon("   X15: "); wriptr(exc->ContextRecord->X[15]);
    writecon("\n X16: "); wriptr(exc->ContextRecord->X[16]);
    writecon("   X17: "); wriptr(exc->ContextRecord->X[17]);
    writecon("   X18: "); wriptr(exc->ContextRecord->X[18]);
    writecon("   X19: "); wriptr(exc->ContextRecord->X[19]);
    writecon("\n X20: "); wriptr(exc->ContextRecord->X[20]);
    writecon("   X21: "); wriptr(exc->ContextRecord->X[21]);
    writecon("   X22: "); wriptr(exc->ContextRecord->X[22]);
    writecon("   X23: "); wriptr(exc->ContextRecord->X[23]);
    writecon("\n X24: "); wriptr(exc->ContextRecord->X[24]);
    writecon("   X25: "); wriptr(exc->ContextRecord->X[25]);
    writecon("   X26: "); wriptr(exc->ContextRecord->X[26]);
    writecon("   X27: "); wriptr(exc->ContextRecord->X[27]);
    writecon("\n X28: "); wriptr(exc->ContextRecord->X[28]);
#else
#ifdef _M_IX86
    writecon("\n PC : "); wriptra(exc->ContextRecord->Eip);
    
    writecon("\n EDI: "); wriptr(exc->ContextRecord->Edi);
    writecon("   ESI: "); wriptr(exc->ContextRecord->Esi);
    writecon("\n EBX: "); wriptr(exc->ContextRecord->Ebx);
    writecon("   EDX: "); wriptr(exc->ContextRecord->Edx);
    writecon("   ECX: "); wriptr(exc->ContextRecord->Ecx);
    writecon("   EAX: "); wriptr(exc->ContextRecord->Eax);
    writecon("\n EBP: "); wriptr(exc->ContextRecord->Ebp);
    writecon("   ESP: "); wriptr(exc->ContextRecord->Esp);
    writecon("   EIP: "); wriptr(exc->ContextRecord->Eip);
#else
    writecon("  * Regdumps are not supported on this platform");
#endif
#endif
#endif
    //CloseHandle(stdconout);
    //stdconout = GetStdHandle(-11);
    
    /*if(exc->ExceptionRecord->ExceptionCode == EXCEPTION_PRIV_INSTRUCTION)
    {
        writecon("\n  Data: ");
        writecon(exc->ExceptionRecord->ExceptionAddress);
        writecon("\n");
    }*/
    
    writecon("\n");
    
    fflush(stdout);
    fflush(stderr);
    
    HANDLE evt = CreateEventA(0, 0, 0, 0);
    WaitForSingleObject(evt, INFINITE);
    CloseHandle(evt);
    
    return EXCEPTION_EXECUTE_HANDLER;
}
