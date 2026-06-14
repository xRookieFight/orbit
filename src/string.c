#include "string.h"

void* memset(void* dest, int value, size_t count)
{
    void* ret = dest;
    size_t words = count >> 3;
    uint64_t pat = (uint8_t)value;
    pat |= pat << 8;
    pat |= pat << 16;
    pat |= pat << 32;
    __asm__ volatile("rep stosq"
                     : "+D"(dest), "+c"(words)
                     : "a"(pat)
                     : "memory");
    size_t rem = count & 7;
    __asm__ volatile("rep stosb"
                     : "+D"(dest), "+c"(rem)
                     : "a"((uint8_t)value)
                     : "memory");
    return ret;
}

void* memcpy(void* dest, const void* src, size_t count)
{
    void* ret = dest;
    size_t words = count >> 3;
    size_t rem = count & 7;
    __asm__ volatile("rep movsq"
                     : "+D"(dest), "+S"(src), "+c"(words)
                     :
                     : "memory");
    __asm__ volatile("rep movsb"
                     : "+D"(dest), "+S"(src), "+c"(rem)
                     :
                     : "memory");
    return ret;
}

void* memmove(void* dest, const void* src, size_t count)
{
    uint8_t* d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;
    if (d < s) {
        while (count--)
            *d++ = *s++;
    } else if (d > s) {
        d += count;
        s += count;
        while (count--)
            *--d = *--s;
    }
    return dest;
}

int memcmp(const void* a, const void* b, size_t count)
{
    const uint8_t* x = (const uint8_t*)a;
    const uint8_t* y = (const uint8_t*)b;
    while (count--) {
        if (*x != *y)
            return (int)*x - (int)*y;
        x++;
        y++;
    }
    return 0;
}

size_t strlen(const char* s)
{
    size_t n = 0;
    while (s[n])
        n++;
    return n;
}

int strcmp(const char* a, const char* b)
{
    while (*a && (*a == *b)) {
        a++;
        b++;
    }
    return (int)(uint8_t)*a - (int)(uint8_t)*b;
}

int strncmp(const char* a, const char* b, size_t n)
{
    while (n && *a && (*a == *b)) {
        a++;
        b++;
        n--;
    }
    if (n == 0)
        return 0;
    return (int)(uint8_t)*a - (int)(uint8_t)*b;
}

char* strcpy(char* dest, const char* src)
{
    char* p = dest;
    while ((*p++ = *src++))
        ;
    return dest;
}

char* strncpy(char* dest, const char* src, size_t n)
{
    size_t i = 0;
    for (; i < n && src[i]; i++)
        dest[i] = src[i];
    for (; i < n; i++)
        dest[i] = '\0';
    return dest;
}

char* strcat(char* dest, const char* src)
{
    char* p = dest + strlen(dest);
    while ((*p++ = *src++))
        ;
    return dest;
}

char* strchr(const char* s, int c)
{
    while (*s) {
        if (*s == (char)c)
            return (char*)s;
        s++;
    }
    if ((char)c == '\0')
        return (char*)s;
    return NULL;
}

char* strrchr(const char* s, int c)
{
    const char* last = NULL;
    while (*s) {
        if (*s == (char)c)
            last = s;
        s++;
    }
    return (char*)last;
}

void strlcpy(char* dest, const char* src, size_t size)
{
    if (size == 0)
        return;
    size_t i = 0;
    for (; i + 1 < size && src[i]; i++)
        dest[i] = src[i];
    dest[i] = '\0';
}

int atoi(const char* s)
{
    int sign = 1;
    int value = 0;
    while (*s == ' ' || *s == '\t')
        s++;
    if (*s == '-') {
        sign = -1;
        s++;
    } else if (*s == '+') {
        s++;
    }
    while (*s >= '0' && *s <= '9') {
        value = value * 10 + (*s - '0');
        s++;
    }
    return value * sign;
}

void utoa(unsigned int value, char* buffer, int base)
{
    static const char digits[] = "0123456789abcdef";
    char tmp[33];
    int i = 0;
    if (value == 0) {
        buffer[0] = '0';
        buffer[1] = '\0';
        return;
    }
    while (value && i < 32) {
        tmp[i++] = digits[value % (unsigned)base];
        value /= (unsigned)base;
    }
    int j = 0;
    while (i > 0)
        buffer[j++] = tmp[--i];
    buffer[j] = '\0';
}

void itoa(int value, char* buffer, int base)
{
    if (base == 10 && value < 0) {
        buffer[0] = '-';
        utoa((unsigned int)(-value), buffer + 1, base);
        return;
    }
    utoa((unsigned int)value, buffer, base);
}
