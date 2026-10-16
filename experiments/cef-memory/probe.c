typedef unsigned long DWORD;
typedef int BOOL;
typedef void *HANDLE;
#define API __declspec(dllimport) __stdcall
API DWORD GetEnvironmentVariableA(const char *, char *, DWORD);
API BOOL SetDllDirectoryA(const char *);
API HANDLE LoadLibraryExA(const char *, HANDLE, DWORD);
API void *GetProcAddress(HANDLE, const char *);
API DWORD GetLastError(void);
API DWORD GetCurrentProcessId(void);
API HANDLE GetStdHandle(DWORD);
API BOOL WriteFile(HANDLE, const void *, DWORD, DWORD *, void *);
API void Sleep(DWORD);
API void ExitProcess(DWORD);
static char output[2048], target[1024], directory[1024], mode[16];
static unsigned position;
static volatile unsigned touched;
static void text(const char *value) {
    while (*value && position < sizeof(output) - 1) output[position++] = *value++;
}
static void number(DWORD value) {
    char digits[16];
    unsigned count = 0;
    do { digits[count++] = '0' + value % 10; value /= 10; } while (value);
    while (count) output[position++] = digits[--count];
}
static void flush(void) {
    DWORD written;
    WriteFile(GetStdHandle((DWORD)-11), output, position, &written, 0);
    position = 0;
}
void mainCRTStartup(void) {
    GetEnvironmentVariableA("WECOM_MEMORY_DLL", target, sizeof(target));
    GetEnvironmentVariableA("WECOM_MEMORY_DIRECTORY", directory, sizeof(directory));
    GetEnvironmentVariableA("WECOM_MEMORY_MODE", mode, sizeof(mode));
    if (!target[0]) ExitProcess(2);
    SetDllDirectoryA(directory);
    HANDLE module = LoadLibraryExA(target, 0, mode[0] == 'm' ? 1 : 0);
    text("PID="); number(GetCurrentProcessId());
    if (!module) {
        text(" LOAD_ERROR="); number(GetLastError()); text("\n"); flush(); ExitProcess(3);
    }
    text(" LOAD_OK");
    if (mode[0] != 'm') {
        int (__cdecl *version)(int) = (void *)GetProcAddress(module, "cef_version_info");
        if (!version) { text(" EXPORT_MISSING\n"); flush(); ExitProcess(4); }
        text(" CEF_MAJOR="); number(version(0));
        text(" CHROME_MAJOR="); number(version(4));
    }
    if (mode[0] == 't') {
        unsigned char *image = module;
        DWORD pe_offset = *(DWORD *)(image + 60);
        unsigned short section_count = *(unsigned short *)(image + pe_offset + 6);
        unsigned short optional_size = *(unsigned short *)(image + pe_offset + 20);
        unsigned char *sections = image + pe_offset + 24 + optional_size;
        for (unsigned index = 0; index < section_count; index++) {
            unsigned char *section = sections + index * 40;
            DWORD virtual_size = *(DWORD *)(section + 8);
            DWORD address = *(DWORD *)(section + 12);
            DWORD flags = *(DWORD *)(section + 36);
            if (!(flags & 0x40000000) || (flags & 0x02000000)) continue;
            for (DWORD offset = 0; offset < virtual_size; offset += 4096) {
                touched += *(volatile unsigned char *)(image + address + offset);
            }
        }
        text(" TOUCHED="); number(touched);
    }
    text(" READY\n"); flush();
    Sleep(25000);
    ExitProcess(0);
}
