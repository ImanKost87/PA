#include <stdio.h>
#include <stdarg.h>
#include "common.h"
#include "ipc_struct.h"

void printLog(FILE *logFile, const char *format, ...) {
    va_list vaList;
    va_start(vaList, format);
    vfprintf(logFile, format, vaList);
    va_end(vaList);
}
