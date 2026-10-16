#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

typedef unsigned char BYTE;
typedef uintptr_t UINT_PTR;
static const UINT_PTR host_page_mask = 4095;
static int force_failure;
static size_t discarded, fallback_calls;

static int checked_madvise(void *base, size_t size, int advice)
{
    assert(!((uintptr_t)base & host_page_mask));
    assert(!(size & host_page_mask));
    assert(size && advice == MADV_DONTNEED);
    if (force_failure)
    {
        ++fallback_calls;
        errno = EINVAL;
        return -1;
    }
    ++discarded;
    return madvise(base, size, advice);
}

#define madvise checked_madvise
#include "vprot-range.inc"
#undef madvise

int main(void)
{
    const size_t capacity = 32768;
    const size_t lengths[] = {0, 1, 4095, 4096, 4097, 8191, 8192, 8193, 16384};
    const BYTE values[] = {0, 0x7f, 0xff};
    size_t offset, length_index, value_index, cases = 0;
    BYTE *mapping, *base, *expected;
    assert(sysconf(_SC_PAGESIZE) == 4096);
    mapping = mmap(NULL, capacity + 8192, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    assert(mapping != MAP_FAILED);
    base = mapping + 4096;
    assert(!mprotect(base, capacity, PROT_READ | PROT_WRITE));
    expected = malloc(capacity);
    assert(expected);
    for (force_failure = 0; force_failure < 2; ++force_failure)
        for (offset = 0; offset < 4096; ++offset)
            for (length_index = 0; length_index < sizeof(lengths) / sizeof(lengths[0]); ++length_index)
                for (value_index = 0; value_index < sizeof(values) / sizeof(values[0]); ++value_index)
                {
                    size_t length = lengths[length_index];
                    BYTE value = values[value_index];
                    memset(base, 0xa5, capacity);
                    memset(expected, 0xa5, capacity);
                    memset(expected + offset, value, length);
                    set_page_vprot_range(base + offset, length, value);
                    assert(!memcmp(base, expected, capacity));
                    ++cases;
                }
    assert(discarded && fallback_calls);
    printf("PASS cases=%zu madvise_success=%zu injected_failures=%zu\n", cases, discarded, fallback_calls);
    free(expected);
    assert(!munmap(mapping, capacity + 8192));
    return 0;
}
