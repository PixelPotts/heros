#include "string.h"

void *memcpy(void *dst, const void *src, size_t n)
{
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;

    /* Word-aligned fast copy */
    if (((uintptr_t)d & 3) == 0 && ((uintptr_t)s & 3) == 0) {
        while (n >= 4) {
            *(uint32_t *)d = *(const uint32_t *)s;
            d += 4; s += 4; n -= 4;
        }
    }
    while (n--) *d++ = *s++;
    return dst;
}

void *memset(void *dst, int c, size_t n)
{
    uint8_t *d = (uint8_t *)dst;
    uint8_t val = (uint8_t)c;

    /* Word-aligned fast fill */
    if (((uintptr_t)d & 3) == 0 && n >= 4) {
        uint32_t w = val | (val << 8) | (val << 16) | (val << 24);
        while (n >= 4) {
            *(uint32_t *)d = w;
            d += 4; n -= 4;
        }
    }
    while (n--) *d++ = val;
    return dst;
}

void *memmove(void *dst, const void *src, size_t n)
{
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;

    if (d < s || d >= s + n) {
        return memcpy(dst, src, n);
    }
    /* Overlapping, copy backwards */
    d += n;
    s += n;
    while (n--) *--d = *--s;
    return dst;
}

int memcmp(const void *a, const void *b, size_t n)
{
    const uint8_t *pa = (const uint8_t *)a;
    const uint8_t *pb = (const uint8_t *)b;
    for (size_t i = 0; i < n; i++) {
        if (pa[i] != pb[i])
            return (int)pa[i] - (int)pb[i];
    }
    return 0;
}

size_t strlen(const char *s)
{
    size_t len = 0;
    while (s[len]) len++;
    return len;
}

int strcmp(const char *a, const char *b)
{
    while (*a && *a == *b) { a++; b++; }
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

int strncmp(const char *a, const char *b, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        if (a[i] != b[i] || a[i] == '\0')
            return (int)(unsigned char)a[i] - (int)(unsigned char)b[i];
    }
    return 0;
}

char *strcpy(char *dst, const char *src)
{
    char *d = dst;
    while ((*d++ = *src++));
    return dst;
}

char *strncpy(char *dst, const char *src, size_t n)
{
    size_t i;
    for (i = 0; i < n && src[i]; i++)
        dst[i] = src[i];
    for (; i < n; i++)
        dst[i] = '\0';
    return dst;
}

char *strcat(char *dst, const char *src)
{
    char *d = dst + strlen(dst);
    while ((*d++ = *src++));
    return dst;
}

char *strchr(const char *s, int c)
{
    while (*s) {
        if (*s == (char)c) return (char *)s;
        s++;
    }
    return (c == '\0') ? (char *)s : (void *)0;
}

char *strrchr(const char *s, int c)
{
    const char *last = (void *)0;
    while (*s) {
        if (*s == (char)c) last = s;
        s++;
    }
    if (c == '\0') return (char *)s;
    return (char *)last;
}

char *strstr(const char *haystack, const char *needle)
{
    if (!*needle) return (char *)haystack;
    size_t nlen = strlen(needle);
    while (*haystack) {
        if (strncmp(haystack, needle, nlen) == 0)
            return (char *)haystack;
        haystack++;
    }
    return (void *)0;
}

int atoi(const char *s)
{
    int neg = 0, val = 0;
    while (*s == ' ' || *s == '\t') s++;
    if (*s == '-') { neg = 1; s++; }
    else if (*s == '+') s++;
    while (*s >= '0' && *s <= '9')
        val = val * 10 + (*s++ - '0');
    return neg ? -val : val;
}

void itoa(int val, char *buf, int base)
{
    char tmp[34];
    int i = 0, neg = 0;
    unsigned uval;

    if (val < 0 && base == 10) {
        neg = 1;
        uval = (unsigned)(-(val + 1)) + 1;
    } else {
        uval = (unsigned)val;
    }

    if (uval == 0) tmp[i++] = '0';
    while (uval > 0) {
        unsigned d = uval % (unsigned)base;
        tmp[i++] = (d < 10) ? ('0' + d) : ('a' + d - 10);
        uval /= (unsigned)base;
    }
    if (neg) tmp[i++] = '-';

    int j = 0;
    while (--i >= 0) buf[j++] = tmp[i];
    buf[j] = '\0';
}

void utoa(unsigned val, char *buf, int base)
{
    char tmp[34];
    int i = 0;

    if (val == 0) tmp[i++] = '0';
    while (val > 0) {
        unsigned d = val % (unsigned)base;
        tmp[i++] = (d < 10) ? ('0' + d) : ('a' + d - 10);
        val /= (unsigned)base;
    }

    int j = 0;
    while (--i >= 0) buf[j++] = tmp[i];
    buf[j] = '\0';
}

/* ── GCC soft-division symbols (needed for rv32i without M extension) ── */
unsigned __udivsi3(unsigned num, unsigned den)
{
    if (den == 0) return 0;
    unsigned q = 0;
    for (int i = 31; i >= 0; i--) {
        if ((num >> i) >= den) {
            q |= (1U << i);
            num -= (den << i);
        }
    }
    return q;
}

unsigned __umodsi3(unsigned num, unsigned den)
{
    return num - __udivsi3(num, den) * den;
}

int __divsi3(int num, int den)
{
    int neg = 0;
    if (num < 0) { neg = !neg; num = -num; }
    if (den < 0) { neg = !neg; den = -den; }
    int q = (int)__udivsi3((unsigned)num, (unsigned)den);
    return neg ? -q : q;
}

int __modsi3(int num, int den)
{
    return num - __divsi3(num, den) * den;
}

/* ── 64-bit division (GCC emits for uint64_t / uint64_t) ─────── */
typedef unsigned long long uint64;

uint64 __udivdi3(uint64 num, uint64 den)
{
    if (den == 0) return 0;
    if (num < den) return 0;
    if (den == 1) return num;

    /* Handle 32-bit denominator (common case) */
    if ((den >> 32) == 0) {
        uint32_t d = (uint32_t)den;
        uint32_t q_hi = 0, q_lo = 0;
        uint32_t n_hi = (uint32_t)(num >> 32);
        uint32_t n_lo = (uint32_t)num;

        if (n_hi >= d) {
            q_hi = n_hi / d;
            n_hi = n_hi % d;
        }
        /* Combine remainder with low word */
        uint64 combined = ((uint64)n_hi << 32) | n_lo;
        /* Simple long division */
        q_lo = 0;
        for (int i = 31; i >= 0; i--) {
            uint64 shifted = (uint64)d << i;
            if (combined >= shifted) {
                q_lo |= (1U << i);
                combined -= shifted;
            }
        }
        return ((uint64)q_hi << 32) | q_lo;
    }

    /* Full 64-bit division via binary long division */
    uint64 q = 0;
    uint64 r = 0;
    for (int i = 63; i >= 0; i--) {
        r = (r << 1) | ((num >> i) & 1);
        if (r >= den) {
            r -= den;
            q |= ((uint64)1 << i);
        }
    }
    return q;
}

uint64 __umoddi3(uint64 num, uint64 den)
{
    return num - __udivdi3(num, den) * den;
}

long long __divdi3(long long num, long long den)
{
    int neg = 0;
    if (num < 0) { neg = !neg; num = -num; }
    if (den < 0) { neg = !neg; den = -den; }
    long long q = (long long)__udivdi3((uint64)num, (uint64)den);
    return neg ? -q : q;
}

long long __moddi3(long long num, long long den)
{
    return num - __divdi3(num, den) * den;
}
