#pragma once

#include <stddef.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdarg.h>
#include "pa1.h"
#include "ipc.h"

struct Pipe {
    int in;
    int out;
};

struct Ipc {
    local_id n;
    local_id id;
    pid_t pid;
    pid_t parentPid;
    struct Pipe pipes[MAX_PROCESS_ID + 1];
};

struct Ipc *runMainProcess(local_id n);
bool runChildProcess(FILE *logFile, struct Ipc ipc);
void closePipes(struct Ipc *ipc);
void closeProcessPipes(struct Ipc *ipc);
void createMessage(Message *message, MessageType type, const char *format, ...);

void printLog(FILE *logFile, const char *format, ...);
