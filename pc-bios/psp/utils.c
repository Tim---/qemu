#include <stdint.h>
#include <stddef.h>

void __assert_func(const char *filename, int line, const char *assert_func, const char *expr)
{
    while(1);
}

void *memcpy(void *dst, void *src, size_t n)
{
    uint8_t *d = dst;
    uint8_t *s = src;
    while(n) {
        *d++ = *s++;
        n--;
    }
}
