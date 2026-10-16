#include <windows.h>
#include <stdio.h>

static volatile LONG checks;

static void expect(BOOL condition, const char *label)
{
    InterlockedIncrement(&checks);
    if (!condition)
    {
        printf("FAIL %s, error=%lu\n", label, GetLastError());
        fflush(stdout);
        ExitProcess(1);
    }
}

static void check_region(void *address, DWORD state, DWORD protection, void *allocation)
{
    MEMORY_BASIC_INFORMATION information;
    expect(VirtualQuery(address, &information, sizeof(information)) == sizeof(information), "query");
    expect(information.State == state, "state");
    if (state == MEM_COMMIT) expect(information.Protect == protection, "protection");
    if (state != MEM_FREE) expect(information.AllocationBase == allocation, "allocation base");
}

static void check_zero(BYTE *address)
{
    SIZE_T offset;
    for (offset = 0; offset < 4096; ++offset) expect(address[offset] == 0, "zero-filled commit");
}

static void check_boundaries(void)
{
    SIZE_T cycle, entry;
    SIZE_T length = ((SIZE_T)4 << 30) + ((SIZE_T)32 << 20) + 3 * 4096;
    SIZE_T offsets[] = {0, 4096, (16 << 20) - 4096, 16 << 20,
                       ((SIZE_T)4 << 30) - 65536 - 4096, ((SIZE_T)4 << 30) - 65536,
                       ((SIZE_T)4 << 30) + (32 << 20) + 2 * 4096};
    for (cycle = 0; cycle < 8; ++cycle)
    {
        BYTE *base = (BYTE *)(((SIZE_T)1 << 40) + cycle * ((SIZE_T)8 << 30) + 65536);
        BYTE *left = base - 65536;
        BYTE *right = base + ((length + 65535) & ~(SIZE_T)65535);
        DWORD previous;
        expect(VirtualAlloc(left, 65536, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE) == left, "left neighbor");
        expect(VirtualAlloc(right, 65536, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE) == right, "right neighbor");
        left[65535] = 0x71;
        right[0] = 0x37;
        expect(VirtualProtect(right, 65536, PAGE_READONLY, &previous), "protect neighbor");
        expect(VirtualAlloc(base, length, MEM_RESERVE, PAGE_NOACCESS) == base, "reserve across tables");
        check_region(base, MEM_RESERVE, 0, base);
        for (entry = 0; entry < sizeof(offsets) / sizeof(offsets[0]); ++entry)
        {
            BYTE *page = base + offsets[entry];
            expect(VirtualAlloc(page, 4096, MEM_COMMIT, PAGE_READWRITE) == page, "commit");
            check_zero(page);
            page[0] = 0x55;
            page[4095] = 0xaa;
            check_region(page, MEM_COMMIT, PAGE_READWRITE, base);
            expect(VirtualProtect(page, 4096, PAGE_READONLY, &previous), "read-only");
            expect(previous == PAGE_READWRITE, "old protection");
            check_region(page, MEM_COMMIT, PAGE_READONLY, base);
            expect(page[0] == 0x55 && page[4095] == 0xaa, "content retained");
            expect(VirtualProtect(page, 4096, PAGE_NOACCESS, &previous), "no-access");
            check_region(page, MEM_COMMIT, PAGE_NOACCESS, base);
            expect(VirtualProtect(page, 4096, PAGE_READWRITE | PAGE_GUARD, &previous), "guard");
            check_region(page, MEM_COMMIT, PAGE_READWRITE | PAGE_GUARD, base);
            expect(VirtualProtect(page, 4096, PAGE_READWRITE, &previous), "remove guard");
            expect(VirtualFree(page, 4096, MEM_DECOMMIT), "decommit");
            check_region(page, MEM_RESERVE, 0, base);
            expect(VirtualAlloc(page, 4096, MEM_COMMIT, PAGE_READWRITE) == page, "recommit");
            check_zero(page);
        }
        check_region(base + 8192, MEM_RESERVE, 0, base);
        expect(VirtualFree(base, 0, MEM_RELEASE), "release");
        check_region(base, MEM_FREE, 0, NULL);
        check_region(left, MEM_COMMIT, PAGE_READWRITE, left);
        check_region(right, MEM_COMMIT, PAGE_READONLY, right);
        expect(left[65535] == 0x71 && right[0] == 0x37, "neighbor contents");
        expect(VirtualAlloc(base, length, MEM_RESERVE, PAGE_NOACCESS) == base, "reuse address");
        expect(VirtualFree(base, 0, MEM_RELEASE), "release reused address");
        expect(VirtualFree(left, 0, MEM_RELEASE), "release left");
        expect(VirtualFree(right, 0, MEM_RELEASE), "release right");
    }
}

static DWORD WINAPI worker(void *argument)
{
    SIZE_T iteration, identity = (SIZE_T)argument;
    BYTE *base = (BYTE *)(((SIZE_T)32 << 40) + identity * (2 << 20) + 65536);
    for (iteration = 0; iteration < 200; ++iteration)
    {
        expect(VirtualAlloc(base, 1 << 20, MEM_RESERVE, PAGE_NOACCESS) == base, "thread reserve");
        expect(VirtualAlloc(base, 4096, MEM_COMMIT, PAGE_READWRITE) == base, "thread commit");
        expect(base[0] == 0, "thread zero");
        base[0] = (BYTE)(identity + 1);
        check_region(base, MEM_COMMIT, PAGE_READWRITE, base);
        expect(base[0] == identity + 1, "thread contents");
        expect(VirtualFree(base, 0, MEM_RELEASE), "thread release");
    }
    return 0;
}

int main(void)
{
    HANDLE threads[4];
    SIZE_T index;
    DWORD code;
    puts("READY");
    fflush(stdout);
    Sleep(2000);
    check_boundaries();
    for (index = 0; index < 4; ++index)
    {
        threads[index] = CreateThread(NULL, 0, worker, (void *)index, 0, NULL);
        expect(threads[index] != NULL, "create thread");
    }
    for (index = 0; index < 4; ++index)
    {
        expect(WaitForSingleObject(threads[index], 30000) == WAIT_OBJECT_0, "join thread");
        expect(GetExitCodeThread(threads[index], &code) && code == 0, "thread result");
        CloseHandle(threads[index]);
    }
    printf("PASS checks=%ld, boundary_cycles=8, concurrent_cycles=800\n", checks);
    fflush(stdout);
    Sleep(2000);
    return 0;
}
