#include <stdio.h>
#include <stdarg.h>

extern FILE* gLog;

static void log(const char* fmt, ...)
{
    if (!gLog)
        return;

    va_list args;
    va_start(args, fmt);
    vfprintf(gLog, fmt, args);
    va_end(args);

    fflush(gLog);
}

static void log_buff(const char* name, const void* buff, size_t len)
{
    if (!gLog)
        return;

    fprintf(gLog, "%s: %zu bytes\n", name, len);
    for (size_t i = 0; i < len; i++)
    {
        fprintf(gLog, "%02x ", ((const unsigned char*)buff)[i]);
        if ((i + 1) % 16 == 0)
            fprintf(gLog, "\n");
    }
    fprintf(gLog, "\n");
}
