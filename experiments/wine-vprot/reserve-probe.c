typedef unsigned long DWORD;
typedef unsigned long long SIZE_T;
typedef void *HANDLE;
#define API __declspec(dllimport) __stdcall
API void *VirtualAlloc(void *, SIZE_T, DWORD, DWORD);
API int VirtualFree(void *, SIZE_T, DWORD);
API HANDLE GetStdHandle(DWORD);
API int WriteFile(HANDLE, const void *, DWORD, DWORD *, void *);
API void Sleep(DWORD);
API void ExitProcess(DWORD);

static void report(const char *message, DWORD length) {
    DWORD written;
    WriteFile(GetStdHandle((DWORD)-11), message, length, &written, 0);
}

void mainCRTStartup(void) {
    report("BASELINE\n", 9);
    Sleep(8000);
    void *reserved = VirtualAlloc(0, (SIZE_T)1 << 40, 0x2000, 0x01);
    if (!reserved) {
        report("RESERVE_FAILED\n", 15);
        ExitProcess(1);
    }
    report("RESERVED_1_TIB\n", 15);
    Sleep(8000);
    if (!VirtualFree(reserved, 0, 0x8000)) ExitProcess(2);
    report("FREED\n", 6);
    Sleep(8000);
    ExitProcess(0);
}
