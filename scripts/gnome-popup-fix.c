#define _WIN32_WINNT 0x0600
#include <windows.h>
#include <stdio.h>
#include <string.h>

static HWND main_window;
static DWORD main_pid;
static HANDLE main_process;
static ULONGLONG scan_due;
static volatile LONG running = 1;
static const char pending_place[] = "WeComGnomePopupFix.PendingPlace";

static BOOL has_class(HWND window, const char *name) {
    char cls[128];
    return GetClassNameA(window, cls, sizeof(cls)) && !strcmp(cls, name);
}

static BOOL same_process(HWND window) {
    DWORD pid = 0;
    GetWindowThreadProcessId(window, &pid);
    return pid && pid == main_pid;
}

static BOOL main_alive(void) {
    return main_process && WaitForSingleObject(main_process, 0) == WAIT_TIMEOUT &&
           same_process(main_window) && has_class(main_window, "WeWorkWindow");
}

static BOOL is_card(HWND window) {
    LONG style = GetWindowLongA(window, GWL_STYLE);
    LONG exstyle = GetWindowLongA(window, GWL_EXSTYLE);
    HWND owner = GetWindow(window, GW_OWNER);
    return window != main_window && same_process(window) && has_class(window, "UserInfoWindow") &&
           IsWindowVisible(window) && (style & WS_POPUP) && !(style & WS_CHILD) &&
           (exstyle & (WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW)) == (WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW) &&
           (!owner || owner == main_window);
}

static BOOL is_card_shadow(HWND window, HWND card) {
    return same_process(window) && has_class(window, "PerryShadowWnd") &&
           GetWindow(window, GW_OWNER) == card && (GetWindowLongA(window, GWL_EXSTYLE) & WS_EX_LAYERED);
}

struct shadow_lookup {
    HWND card;
    BOOL found;
};

static BOOL CALLBACK find_shadow(HWND window, LPARAM value) {
    struct shadow_lookup *lookup = (struct shadow_lookup *)value;
    if (!is_card_shadow(window, lookup->card)) return TRUE;
    lookup->found = TRUE;
    return FALSE;
}

static BOOL CALLBACK fix_shadow(HWND window, LPARAM card_value) {
    HWND card = (HWND)card_value;
    if (!main_alive()) return FALSE;
    if (!is_card(card) || GetWindow(card, GW_OWNER) != main_window || !is_card_shadow(window, card) ||
        !IsWindowVisible(window))
        return TRUE;

    COLORREF color = 0;
    BYTE alpha = 255;
    DWORD flags = 0;
    BOOL had_attributes = GetLayeredWindowAttributes(window, &color, &alpha, &flags);
    if (had_attributes && (flags & LWA_ALPHA) && alpha == 0) return TRUE;
    SetLastError(0);
    BOOL ok = SetLayeredWindowAttributes(window, 0, 0, LWA_ALPHA);
    DWORD error = ok ? 0 : GetLastError();
    BYTE after_alpha = 255;
    DWORD after_flags = 0;
    BOOL verified = GetLayeredWindowAttributes(window, &color, &after_alpha, &after_flags);
    printf("shadow=%p card=%p pid=%lu previous_attributes=%d alpha=%u flags=%lx "
           "set_alpha_zero=%d error=%lu after_attributes=%d alpha=%u flags=%lx\n",
           (void *)window, (void *)card, main_pid, had_attributes, (unsigned)alpha, flags,
           ok, error, verified, (unsigned)after_alpha, after_flags);
    return TRUE;
}

static BOOL CALLBACK fix_card(HWND window, LPARAM unused) {
    (void)unused;
    if (!main_alive()) return FALSE;
    if (!is_card(window)) return TRUE;

    struct shadow_lookup lookup = {window, FALSE};
    EnumWindows(find_shadow, (LPARAM)&lookup);
    if (!lookup.found || !main_alive() || !is_card(window)) return TRUE;

    HWND owner = GetWindow(window, GW_OWNER);
    if (!owner || GetPropA(window, pending_place) == (HANDLE)main_window) {
        HWND foreground = GetForegroundWindow();
        LONG_PTR previous = (LONG_PTR)owner;
        DWORD owner_error = 0;
        if (!owner) {
            if (!SetPropA(window, pending_place, (HANDLE)main_window)) {
                printf("card=%p cannot mark pending placement: %lu\n", (void *)window, GetLastError());
                return TRUE;
            }
            SetLastError(0);
            previous = SetWindowLongPtrA(window, GWLP_HWNDPARENT, (LONG_PTR)main_window);
            owner_error = GetLastError();
        }
        BOOL owned = GetWindow(window, GW_OWNER) == main_window;
        BOOL placed = FALSE;
        DWORD place_error = 0;
        if (owned && main_alive() && is_card(window)) {
            SetLastError(0);
            placed = SetWindowPos(window, HWND_TOP, 0, 0, 0, 0,
                                  SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_FRAMECHANGED);
            if (!placed) place_error = GetLastError();
        }
        if (!owned || placed) RemovePropA(window, pending_place);
        printf("card=%p pid=%lu previous_owner=%p owner=%p owner_error=%lu "
               "placed=%d place_error=%lu foreground_before=%p foreground_after=%p\n",
               (void *)window, main_pid, (void *)previous, (void *)GetWindow(window, GW_OWNER), owner_error,
               placed, place_error, (void *)foreground, (void *)GetForegroundWindow());
        if (!owned || !placed) return TRUE;
    }
    EnumWindows(fix_shadow, (LPARAM)window);
    return TRUE;
}

struct main_candidate {
    HWND window;
    DWORD pid;
    HANDLE process;
    unsigned count;
};

static BOOL CALLBACK find_main(HWND window, LPARAM value) {
    struct main_candidate *candidate = (struct main_candidate *)value;
    if (!has_class(window, "WeWorkWindow") || GetWindow(window, GW_OWNER)) return TRUE;
    DWORD pid = 0;
    GetWindowThreadProcessId(window, &pid);
    HANDLE process = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process) return TRUE;
    char path[32768];
    DWORD length = sizeof(path);
    BOOL valid = QueryFullProcessImageNameA(process, 0, path, &length);
    const char *name = valid ? strrchr(path, '\\') : NULL;
    if (!valid || _stricmp(name ? name + 1 : path, "WXWork.exe") ||
        WaitForSingleObject(process, 0) != WAIT_TIMEOUT) {
        CloseHandle(process);
        return TRUE;
    }
    candidate->count++;
    if (candidate->count == 1) {
        candidate->window = window;
        candidate->pid = pid;
        candidate->process = process;
    } else {
        CloseHandle(process);
    }
    return TRUE;
}

static BOOL bind_main(void) {
    struct main_candidate candidate = {0};
    EnumWindows(find_main, (LPARAM)&candidate);
    if (candidate.count != 1) {
        if (candidate.process) CloseHandle(candidate.process);
        return FALSE;
    }
    main_window = candidate.window;
    main_pid = candidate.pid;
    main_process = candidate.process;
    FILETIME created = {0}, exited, kernel, user;
    GetProcessTimes(main_process, &created, &exited, &kernel, &user);
    printf("bound main=%p pid=%lu created=%08lx:%08lx\n",
           (void *)main_window, main_pid, created.dwHighDateTime, created.dwLowDateTime);
    return TRUE;
}

static void CALLBACK on_event(HWINEVENTHOOK hook, DWORD event, HWND window, LONG object,
                              LONG child, DWORD thread, DWORD time) {
    (void)hook;
    (void)thread;
    (void)time;
    if ((event != EVENT_OBJECT_CREATE && event != EVENT_OBJECT_SHOW) || object != OBJID_WINDOW ||
        child != CHILDID_SELF || !window || !main_process || !same_process(window))
        return;
    if (!has_class(window, "UserInfoWindow") && !has_class(window, "PerryShadowWnd")) return;
    ULONGLONG due = GetTickCount64() + 30;
    if (!scan_due || scan_due > due) scan_due = due;
}

static BOOL WINAPI on_console(DWORD event) {
    if (event != CTRL_C_EVENT && event != CTRL_BREAK_EVENT && event != CTRL_CLOSE_EVENT &&
        event != CTRL_LOGOFF_EVENT && event != CTRL_SHUTDOWN_EVENT)
        return FALSE;
    InterlockedExchange(&running, 0);
    return TRUE;
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    HANDLE mutex = CreateMutexW(NULL, FALSE, L"Local\\WeComGnomePopupFix");
    if (!mutex) return 2;
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        puts("another gnome-popup-fix instance is running in this Wine prefix");
        CloseHandle(mutex);
        return 0;
    }
    SetProcessDPIAware();
    SetConsoleCtrlHandler(on_console, TRUE);
    HWINEVENTHOOK hook = SetWinEventHook(EVENT_OBJECT_CREATE, EVENT_OBJECT_SHOW, NULL, on_event, 0, 0,
                                        WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
    if (!hook) {
        fprintf(stderr, "SetWinEventHook failed: %lu\n", GetLastError());
        CloseHandle(mutex);
        return 2;
    }
    puts("watching UserInfoWindow owner and its direct PerryShadowWnd; no activation or remap");
    ULONGLONG missing_since = GetTickCount64(), next_scan = missing_since;
    while (InterlockedCompareExchange(&running, 1, 1)) {
        ULONGLONG now = GetTickCount64();
        if (main_process && !main_alive()) {
            printf("main identity lost pid=%lu; waiting up to 120 seconds to rebind\n", main_pid);
            CloseHandle(main_process);
            main_process = NULL;
            main_window = NULL;
            main_pid = 0;
            scan_due = 0;
            missing_since = next_scan = now;
        }
        if (now >= next_scan || (scan_due && now >= scan_due)) {
            if (!main_process) bind_main();
            if (main_alive()) EnumWindows(fix_card, 0);
            scan_due = 0;
            next_scan = GetTickCount64() + 1000;
        }
        now = GetTickCount64();
        if (!main_process && now - missing_since >= 120000) {
            puts("no matching main process within 120 seconds; exiting");
            break;
        }
        ULONGLONG due = next_scan;
        if (scan_due && scan_due < due) due = scan_due;
        if (!main_process && missing_since + 120000 < due) due = missing_since + 120000;
        DWORD timeout = due > now ? (DWORD)(due - now) : 0;
        DWORD wait = MsgWaitForMultipleObjects(0, NULL, FALSE, timeout, QS_ALLINPUT);
        if (wait == WAIT_FAILED) {
            fprintf(stderr, "MsgWaitForMultipleObjects failed: %lu\n", GetLastError());
            break;
        }
        MSG message;
        while (PeekMessageW(&message, NULL, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT) InterlockedExchange(&running, 0);
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }
    UnhookWinEvent(hook);
    if (main_process) CloseHandle(main_process);
    CloseHandle(mutex);
    puts("gnome-popup-fix stopped");
    return 0;
}
