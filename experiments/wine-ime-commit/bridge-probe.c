/* Native mock of the experimental bridge. No Wine, X server or UI needed. */
#define _GNU_SOURCE
#include <assert.h>
#include <dlfcn.h>
#include <locale.h>

#ifndef IME_BRIDGE_SOURCE
#error "Define IME_BRIDGE_SOURCE to the temporary patched ime-bridge.c"
#endif
static void *probe_dlsym(void *, const char *);
#define dlsym probe_dlsym
#include IME_BRIDGE_SOURCE
#undef dlsym

static const char chinese[] = "\xe4\xb8\xad\xe6\x96\x87";
static int attempts, accepted_units, accept_commit, focus_changes;
static int gui_calls, wrong_class, same_process;
static uintptr_t client_window = 42;

static int lookup(XIC ic, XKeyPressedEvent *event, char *buffer, int size,
                  KeySym *keysym, Status *status)
{
    (void)ic; (void)event;
    assert(size >= 6);
    memcpy(buffer, chinese, 6);
    *keysym = 0x1234;
    *status = XLookupChars;
    return 6;
}

static void *probe_dlsym(void *lib, const char *name)
{
    (void)lib;
    return !strcmp(name, "XmbLookupString") ? (void *)lookup : NULL;
}

static int get_gui(uint32_t id, GuiInfo *info)
{
    assert(id == 0);
    assert(info->size == sizeof(*info));
    info->focus = (Hwnd)(uintptr_t)((focus_changes && gui_calls) ? 9 : 1);
    ++gui_calls;
    return 1;
}

static Hwnd get_ancestor(Hwnd hwnd, uint32_t flag)
{
    assert(hwnd == (Hwnd)1);
    assert(flag == 1 || flag == 2);
    return flag == 2 ? (Hwnd)3 : (Hwnd)2;
}

static Hwnd get_pid(Hwnd hwnd, int flag)
{
    assert(flag == 0);
    return same_process ? (Hwnd)1 : hwnd;
}

static int get_class(Hwnd hwnd, int flag, UnicodeString *str)
{
    assert(flag == 0);
    const char *name = wrong_class ? "OtherWindow" :
        (hwnd == (Hwnd)1 ? "Chrome_WidgetWin_1" : "CefBrowserWindow");
    size_t length = strlen(name);
    assert(length * sizeof(uint16_t) <= str->capacity);
    for (size_t i = 0; i < length; ++i) str->buffer[i] = (uint16_t)name[i];
    return (int)length;
}

static Hwnd prop(Hwnd hwnd, const uint16_t *key)
{
    static const char expected_key[] = "__wine_x11_whole_window";
    assert(hwnd == (Hwnd)3);
    for (size_t i = 0; i < sizeof(expected_key); ++i)
        assert(key[i] == (unsigned char)expected_key[i]);
    return (Hwnd)client_window;
}

/* Unix ABI: exactly seven parameters, with the complete UTF-16 payload. */
static intptr_t call(Hwnd hwnd, uint32_t msg, uintptr_t count, intptr_t text,
                     void *params, uint32_t type, int ansi)
{
    const uint16_t *units = (const uint16_t *)text;
    assert(hwnd == (Hwnd)1);
    assert(msg == 2 && count == 2);
    assert(params == NULL && type == 0x305 && ansi == 0);
    assert(units && units[0] == 0x4e2d && units[1] == 0x6587);
    ++attempts;
    if (!accept_commit) return 0;
    accepted_units += (int)count;
    return 1;
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    assert(setlocale(LC_ALL, "C.UTF-8"));
    is_input_host = 1;
    gui_info = get_gui; ancestor = get_ancestor; query_window = get_pid;
    class_name = get_class; get_prop = prop; message_call = call;
    accept_commit = 1;
    int expected_attempts = 0, expected_accept = 0;
    if (!strcmp(argv[1], "accepted")) expected_attempts = expected_accept = 1;
    else if (!strcmp(argv[1], "rejected")) {
        accept_commit = 0; expected_attempts = 1;
    } else if (!strcmp(argv[1], "client-mismatch")) client_window = 43;
    else if (!strcmp(argv[1], "focus-changed")) focus_changes = 1;
    else if (!strcmp(argv[1], "other-process")) is_input_host = 0;
    else if (!strcmp(argv[1], "class-mismatch")) wrong_class = 1;
    else if (!strcmp(argv[1], "same-process")) same_process = 1;
    else if (strcmp(argv[1], "ordinary-key")) assert(!"unknown case");

    XKeyPressedEvent event = {.window = 42, .keycode = 0};
    if (!strcmp(argv[1], "ordinary-key")) event.keycode = 38;
    char buffer[16];
    memset(buffer, 0x5a, sizeof(buffer));
    KeySym keysym = NoSymbol;
    Status status = XLookupNone;
    int result = XmbLookupString(NULL, &event, buffer, sizeof(buffer), &keysym, &status);
    assert(attempts == expected_attempts);
    assert(accepted_units == (expected_accept ? 2 : 0));
    /* Forwarding never mutates the underlying lookup bytes. */
    assert(!memcmp(buffer, chinese, 6));
    for (size_t i = 6; i < sizeof(buffer); ++i) assert(buffer[i] == 0x5a);
    assert(result == (expected_accept ? 0 : 6));
    assert(status == (expected_accept ? XLookupNone : XLookupChars));
    assert(keysym == (expected_accept ? NoSymbol : 0x1234));
    if (focus_changes) assert(gui_calls == 2);
    printf("PASS: %s attempts=%d accepted_units=%d return_bytes=%d status=%d\n",
           argv[1], attempts, accepted_units, result, status);
    return 0;
}
