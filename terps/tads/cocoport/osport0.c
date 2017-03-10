#include <string.h>
#include <ctype.h>

int
memicmp( const char* s1, const char* s2, size_t len )
{
    int i;
    for (i = 0; i < len; i++)
    {
        if (tolower(s1[i]) != tolower(s2[i]))
            return (int)tolower(s1[i]) - (int)tolower(s2[i]);
    }
    return 0;
}

size_t
wcslen( const wchar_t* s )
{
    size_t len = 0;
    while (*s++)
        len++;
    return len;
}

wchar_t*
wcscpy( wchar_t* dst, const wchar_t* src )
{
    wchar_t *start = dst;
    while ((*dst++ = *src++) != 0)
        ;
    return start;
}

