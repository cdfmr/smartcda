#pragma once

#include <stdio.h>
#include <time.h>
#include <string>
#include "config.h"

#pragma warning(disable: 4996)

inline char * now()
{
    static char timebuf[32];
    time_t t;
    time(&t);
    struct tm *tminfo = localtime(&t);
    strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", tminfo);
    return timebuf;
}

#define dbgstr(format, ...)               \
{                                         \
    char buffer[1024];                    \
    sprintf(buffer, format, __VA_ARGS__); \
    OutputDebugString(buffer);            \
}

extern FILE *flog;
#define filelog(format, ...)                \
{                                           \
    if (flog) {                             \
        fprintf(flog, "%s - ", now());      \
        fprintf(flog, format, __VA_ARGS__); \
        fprintf(flog, "\n");                \
        fflush(flog);                       \
    }                                       \
}

#define debug(format, ...)            \
{                                     \
    if (config.debug) {               \
        dbgstr (format, __VA_ARGS__); \
        filelog(format, __VA_ARGS__); \
    }                                 \
}

void open_log(const std::string &filename);
void close_log();
