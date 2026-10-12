typedef unsigned long DWORD;
typedef int BOOL;
typedef unsigned short WORD;
typedef void *HANDLE;
typedef __UINTPTR_TYPE__ ULONG_PTR;
#define API __declspec(dllimport) __stdcall
typedef struct {
    DWORD cbSize, dwFlags;
    const char *lpSource;
    WORD wProcessorArchitecture, wLangId;
    const char *lpAssemblyDirectory, *lpResourceName, *lpApplicationName;
    HANDLE hModule;
} ACTCTXA;
API DWORD GetEnvironmentVariableA(const char *, char *, DWORD);
API HANDLE CreateActCtxA(const ACTCTXA *);
API BOOL ActivateActCtx(HANDLE, ULONG_PTR *);
API HANDLE LoadLibraryA(const char *);
API DWORD GetModuleFileNameA(HANDLE, char *, DWORD);
API DWORD GetLastError(void);
API HANDLE GetStdHandle(DWORD);
API BOOL WriteFile(HANDLE, const void *, DWORD, DWORD *, void *);
API void ExitProcess(DWORD);
static char out[8192], target[2048], context[2048], resource[8], path[2048];
static unsigned pos;
static void text(const char *s) { while (*s && pos < sizeof(out)-1) out[pos++] = *s++; }
static void number(DWORD n) { char b[16]; unsigned k=0; do { b[k++]='0'+n%10; n/=10; } while(n); while(k) out[pos++]=b[--k]; }
static void done(DWORD rc) { DWORD written; WriteFile(GetStdHandle((DWORD)-11),out,pos,&written,0); ExitProcess(rc); }
void mainCRTStartup(void) {
    GetEnvironmentVariableA("WECOM_PROBE_DLL",target,sizeof(target));
    GetEnvironmentVariableA("WECOM_PROBE_ACTCTX",context,sizeof(context));
    GetEnvironmentVariableA("WECOM_PROBE_RESOURCE",resource,sizeof(resource));
    if (!target[0]) { text("Missing target\n"); done(2); }
    if (context[0]) {
        static ACTCTXA act; ULONG_PTR cookie;
        act.cbSize=sizeof(act); act.lpSource=context;
        if (resource[0]=='1') { act.dwFlags=8; act.lpResourceName=(const char *)1; }
        HANDLE h=CreateActCtxA(&act);
        if (h==(HANDLE)(ULONG_PTR)-1) { text("ACTCTX_ERROR=");number(GetLastError());text("\n");done(3); }
        if (!ActivateActCtx(h,&cookie)) { text("ACTIVATE_ERROR=");number(GetLastError());text("\n");done(4); }
        text("ACTCTX_OK\n");
    }
    HANDLE module=LoadLibraryA(target);
    if (!module) { text("LOAD_ERROR=");number(GetLastError());text("\n");done(5); }
    GetModuleFileNameA(module,path,sizeof(path));
    text("LOADED=");text(path);text("\n");done(0);
}
