#pragma once

#include <stddef.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdarg.h>
#include "banking.h"
#include "pa2345.h"
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
    balance_t balance;
    struct Pipe pipes[MAX_PROCESS_ID + 1];
};

struct Ipc *runMainProcess(local_id n, balance_t *balance);
bool runChildProcess(FILE *logFile, struct Ipc ipc);
int receiveAnyNonBlocking(void * self, Message * msg);
void closePipes(struct Ipc *ipc);
void createMessage(Message *message, MessageType type, const char *format, ...);

void printLog(FILE *logFile, const char *format, ...);

void transfer(void * parent_data, local_id src, local_id dst,
              balance_t amount);
