/* Isolated two-process Wine IME commit probe. No CRT or daily-prefix access.
 * Run --receiver first, then --sender in the same isolated Wine prefix.
 * Expected wire data is compared as UTF-16 code units, including surrogates.
 */
/* Minimal Windows ABI declarations avoid a dependency on a MinGW CRT SDK. */
#ifdef _WIN64
#define WINAPI __attribute__((ms_abi))
#else
#define WINAPI __attribute__((stdcall))
#endif
#define CALLBACK WINAPI
#define NULL ((void *)0)
#define TRUE 1
#define FALSE 0
typedef unsigned short WCHAR;
typedef unsigned int UINT, DWORD;
typedef int BOOL;
#ifdef _WIN64
typedef unsigned long long SIZE_T, ULONG_PTR, DWORD_PTR, WPARAM;
typedef long long LPARAM, LRESULT;
#else
typedef unsigned int SIZE_T, ULONG_PTR, DWORD_PTR, WPARAM;
typedef int LPARAM, LRESULT;
#endif
typedef void *HANDLE, *HWND, *HMODULE, *HINSTANCE;
typedef LRESULT (CALLBACK *WNDPROC)(HWND, UINT, WPARAM, LPARAM);
typedef struct {
    UINT style; WNDPROC lpfnWndProc; int cbClsExtra, cbWndExtra;
    HINSTANCE hInstance; HANDLE hIcon, hCursor, hbrBackground;
    const WCHAR *lpszMenuName, *lpszClassName;
} WNDCLASSW;
typedef struct {
    HWND hwnd; UINT message; WPARAM wParam; LPARAM lParam;
    DWORD time; struct { int x, y; } pt;
} MSG;
#define IMPORT __declspec(dllimport)
IMPORT HANDLE WINAPI GetStdHandle(DWORD);
IMPORT BOOL WINAPI WriteFile(HANDLE, const void *, DWORD, DWORD *, void *);
IMPORT HMODULE WINAPI GetModuleHandleW(const WCHAR *);
IMPORT HMODULE WINAPI LoadLibraryW(const WCHAR *);
IMPORT void *WINAPI GetProcAddress(HMODULE, const char *);
IMPORT DWORD WINAPI GetCurrentProcessId(void);
IMPORT DWORD WINAPI GetTickCount(void);
IMPORT void WINAPI Sleep(DWORD);
IMPORT WCHAR *WINAPI GetCommandLineW(void);
IMPORT void WINAPI ExitProcess(UINT);
IMPORT unsigned short WINAPI RegisterClassW(const WNDCLASSW *);
IMPORT HWND WINAPI CreateWindowExW(DWORD, const WCHAR *, const WCHAR *, DWORD,
                                   int, int, int, int, HWND, HANDLE, HINSTANCE, void *);
IMPORT LRESULT WINAPI DefWindowProcW(HWND, UINT, WPARAM, LPARAM);
IMPORT void WINAPI PostQuitMessage(int);
IMPORT int WINAPI GetMessageW(MSG *, HWND, UINT, UINT);
IMPORT BOOL WINAPI TranslateMessage(const MSG *);
IMPORT LRESULT WINAPI DispatchMessageW(const MSG *);
IMPORT BOOL WINAPI DestroyWindow(HWND);
IMPORT BOOL WINAPI IsWindow(HWND);
IMPORT HWND WINAPI FindWindowW(const WCHAR *, const WCHAR *);
IMPORT DWORD WINAPI GetWindowThreadProcessId(HWND, DWORD *);
IMPORT LRESULT WINAPI SendMessageTimeoutW(HWND, UINT, WPARAM, LPARAM,
                                        UINT, UINT, DWORD_PTR *);
#define STD_OUTPUT_HANDLE ((DWORD)-11)
#define WM_DESTROY 2
#define WM_CHAR 0x102
#define WM_APP 0x8000
#define WS_OVERLAPPEDWINDOW 0x00cf0000
#define SMTO_BLOCK 1
#define SMTO_ABORTIFHUNG 2

#define QUERY_COUNT    (WM_APP + 1)
#define QUERY_ERRORS   (WM_APP + 2)
#define QUERY_HASH     (WM_APP + 3)
#define RECEIVER_DONE  (WM_APP + 4)
#define IME_DRIVER_CALL 0x305
#define COMMIT_STRING 2
#define QUERY_CAPABILITY 3
#define CAPABILITY_COOKIE 0x57494331

typedef LRESULT (WINAPI *message_call_fn)(HWND, UINT, WPARAM, LPARAM,
                                         void *, DWORD, BOOL);
static const WCHAR class_name[] = L"WineImeCommitIsolatedProbeV1";
static const DWORD lengths[] = {3, 2, 4097, 65536, 128, 129};
static WCHAR payload[65537];
static DWORD received, errors, received_hash = 2166136261u;
static message_call_fn message_call;
static BOOL destroy_receiver, destroy_sender;
/* Stock WoW64 NtUserImeDriverCall thunk requires a four-DWORD params buffer,
 * even when the experimental implementation ignores its contents. */
static DWORD ime_params[4];

/* Suppress CRT helpers even at -O0 or when the headers emit references. */
void *memset(void *p, int c, SIZE_T n)
{
    unsigned char *out = p;
    while (n--) *out++ = (unsigned char)c;
    return p;
}

static void say(const char *s)
{
    DWORD written, n = 0;
    while (s[n]) ++n;
    WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), s, n, &written, NULL);
}

static void number(DWORD n)
{
    char buf[11];
    DWORD at = sizeof(buf), written;
    do { buf[--at] = '0' + n % 10; n /= 10; } while (n);
    WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), buf + at,
              sizeof(buf) - at, &written, NULL);
}

static int fail(const char *what)
{
    say("FAIL: "); say(what); say("\r\n");
    return 1;
}

static WCHAR unit(DWORD which, DWORD index)
{
    static const WCHAR chinese[] = {0x4e2d, 0x6587, 0x6d4b};
    static const WCHAR pair[] = {0xd83d, 0xde00};
    if (which == 0) return chinese[index];
    if (which == 1) return pair[index];
    /* Nonzero BMP units with a case-specific pattern to reveal reordering. */
    return (WCHAR)(0x4e00 + ((index * 37 + which * 101) % 20000));
}

static WCHAR expected(DWORD index)
{
    DWORD which;
    for (which = 0; which < sizeof(lengths) / sizeof(lengths[0]); ++which) {
        if (index < lengths[which]) return unit(which, index);
        index -= lengths[which];
    }
    return 0;
}

static LRESULT CALLBACK receiver_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_CHAR:
        if (wp != expected(received)) ++errors;
        received_hash = (received_hash ^ (DWORD)wp) * 16777619u;
        ++received;
        if (destroy_receiver && received == 1) DestroyWindow(hwnd);
        return 0;
    case QUERY_COUNT: return received;
    case QUERY_ERRORS: return errors;
    case QUERY_HASH: return received_hash;
    case RECEIVER_DONE: PostQuitMessage(0); return 1;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static int receiver(void)
{
    WNDCLASSW cls = {0};
    MSG msg;
    HWND hwnd;
    int status;
    cls.lpfnWndProc = receiver_proc;
    cls.hInstance = GetModuleHandleW(NULL);
    cls.lpszClassName = class_name;
    if (!RegisterClassW(&cls)) return fail("RegisterClassW");
    hwnd = CreateWindowExW(0, class_name, class_name, WS_OVERLAPPEDWINDOW,
                           0, 0, 320, 100, NULL, NULL, cls.hInstance, NULL);
    if (!hwnd) return fail("CreateWindowExW");
    say("READY: receiver pid="); number(GetCurrentProcessId()); say("\r\n");
    while ((status = GetMessageW(&msg, NULL, 0, 0)) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    if (!destroy_receiver) DestroyWindow(hwnd);
    say(destroy_receiver ? "BOUNDARY: receiver units=" : "RECEIVER: units=");
    number(received);
    say(" mismatches="); number(errors);
    say(" hash="); number(received_hash); say("\r\n");
    return status < 0 || errors != 0 || (destroy_receiver && received != 1);
}

static BOOL query(HWND hwnd, UINT msg, DWORD *value)
{
    DWORD_PTR result = 0;
    if (!SendMessageTimeoutW(hwnd, msg, 0, 0, SMTO_ABORTIFHUNG | SMTO_BLOCK,
                             5000, &result)) return FALSE;
    *value = (DWORD)result;
    return TRUE;
}

static BOOL verify(HWND hwnd, DWORD target, DWORD hash)
{
    DWORD start = GetTickCount(), count, mismatches, actual_hash;
    for (;;) {
        if (!query(hwnd, QUERY_COUNT, &count) ||
            !query(hwnd, QUERY_ERRORS, &mismatches)) return FALSE;
        if (mismatches || count > target) return FALSE;
        if (count == target) {
            if (!query(hwnd, QUERY_HASH, &actual_hash)) return FALSE;
            return actual_hash == hash;
        }
        if ((DWORD)(GetTickCount() - start) > 30000) return FALSE;
        Sleep(10);
    }
}

static BOOL commit(HWND hwnd, DWORD count, const WCHAR *text)
{
    return !!message_call(hwnd, COMMIT_STRING, count, (LPARAM)text,
                          ime_params, IME_DRIVER_CALL, FALSE);
}

static BOOL rejected(HWND hwnd, DWORD count, const WCHAR *text)
{
    return !commit(hwnd, count, text);
}

static int sender(void)
{
    HWND hwnd;
    HMODULE module;
    DWORD start = GetTickCount(), pid, which, i, total = 0;
    DWORD hash = 2166136261u, ignored;
    module = LoadLibraryW(L"win32u.dll");
    if (!module) return fail("LoadLibraryW(win32u.dll)");
    message_call = (message_call_fn)GetProcAddress(module, "NtUserMessageCall");
    if (!message_call) return fail("NtUserMessageCall export missing");
    if (message_call(NULL, QUERY_CAPABILITY, 0, 0, ime_params,
                     IME_DRIVER_CALL, FALSE) != CAPABILITY_COOKIE)
        return fail("patched IME capability cookie missing");
    do {
        hwnd = FindWindowW(class_name, NULL);
        if (hwnd) break;
        Sleep(10);
    } while ((DWORD)(GetTickCount() - start) < 10000);
    if (!hwnd) return fail("receiver window missing");
    GetWindowThreadProcessId(hwnd, &pid);
    if (!pid || pid == GetCurrentProcessId()) return fail("receiver must be another process");
    if (!verify(hwnd, 0, hash)) return fail("receiver is not initially empty");
    if (destroy_sender) {
        for (i = 0; i < 3; ++i) payload[i] = unit(0, i);
        if (!commit(hwnd, 3, payload)) return fail("boundary commit rejected");
        say("BOUNDARY: accepted=1 queued_units=3 attempts=1\r\n");
        start = GetTickCount();
        while (IsWindow(hwnd)) {
            if ((DWORD)(GetTickCount() - start) > 10000)
                return fail("boundary receiver window did not disappear");
            Sleep(10);
        }
        say("BOUNDARY: receiver window destroyed; retries=0; expected_received_units=1; acceptance is not an editor transaction\r\n");
        return 0;
    }
    payload[0] = 0x4e2d;
    if (!rejected(hwnd, 0, payload) || !rejected(hwnd, 65537, payload) ||
        !rejected(hwnd, 1, NULL) || !rejected(NULL, 1, payload) ||
        !rejected((HWND)(ULONG_PTR)0xdeadbeef, 1, payload))
        return fail("invalid input was accepted");
    Sleep(100);
    if (!verify(hwnd, 0, hash)) return fail("invalid input delivered units");
    say("PASS: invalid count, null pointer and invalid window\r\n");
    for (which = 0; which < sizeof(lengths) / sizeof(lengths[0]); ++which) {
        for (i = 0; i < lengths[which]; ++i) {
            payload[i] = unit(which, i);
            hash = (hash ^ payload[i]) * 16777619u;
        }
        if (!commit(hwnd, lengths[which], payload)) return fail("valid commit rejected");
        total += lengths[which];
        /* Last two commits are issued consecutively to test queue ordering.
         * The shared sender buffer is overwritten between calls on purpose.
         */
        if (which == 4) continue;
        if (!verify(hwnd, total, hash)) return fail("UTF-16 stream/count/hash mismatch or timeout");
        say("PASS: case="); number(which); say(" cumulative units="); number(total); say("\r\n");
    }
    /* Recheck after a quiet interval so duplicate or late deliveries fail. */
    Sleep(200);
    if (!verify(hwnd, total, hash)) return fail("late or duplicate delivery");
    if (!query(hwnd, RECEIVER_DONE, &ignored) || ignored != 1)
        return fail("receiver completion handshake");
    say("PASS: cross-process Chinese, surrogate pair, 4097/65536 units, ordering and no duplicates\r\n");
    return 0;
}

static BOOL contains(const WCHAR *s, const WCHAR *needle)
{
    const WCHAR *a, *b;
    for (; *s; ++s) {
        for (a = s, b = needle; *b && *a == *b; ++a, ++b) {}
        if (!*b) return TRUE;
    }
    return FALSE;
}

void mainCRTStartup(void)
{
    const WCHAR *cmd = GetCommandLineW();
    int result;
    if (contains(cmd, L"--destroy-receiver")) {
        destroy_receiver = TRUE;
        result = receiver();
    } else if (contains(cmd, L"--destroy-sender")) {
        destroy_sender = TRUE;
        result = sender();
    } else if (contains(cmd, L"--receiver")) result = receiver();
    else if (contains(cmd, L"--sender")) result = sender();
    else result = fail("usage: probe.exe --receiver | --sender | --destroy-receiver | --destroy-sender");
    ExitProcess(result);
}
