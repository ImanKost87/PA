#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include "pa1.h"
#include "ipc_struct.h"
#include "common.h"

void closePipes(struct Ipc *ipc) {
    local_id processAmount = ipc[0].n;
    for (local_id i = 0; i < processAmount; i++) {
        local_id currentId = ipc[i].id;
        for (local_id j = 0; j < processAmount; j++) {
            if (i != currentId) {
                if (ipc[i].pipes[j].in > 0) {
                    close(ipc[i].pipes[j].in);
                }
                if (ipc[i].pipes[j].out > 0) {
                    close(ipc[i].pipes[j].out);
                }
            }
        }
    }
}

void closeProcessPipes(struct Ipc *ipc) {
    local_id processAmount = ipc->n;
    for (local_id i = 0; i < processAmount; i++) {
        if (i != ipc->id) {
            if (ipc->pipes[i].in > 0) {
                close(ipc->pipes[i].in);
            }
            if (ipc->pipes[i].out > 0) {
                close(ipc->pipes[i].out);
            }
        }
    }
}

struct Ipc *runMainProcess(local_id n) {
    struct Ipc *ipcs = malloc(n * sizeof(struct Ipc));

    FILE *pipesLogFile = fopen(pipes_log, "a");
    FILE *eventLogFile = fopen(events_log, "a");

    pid_t parentPid = getpid();

    for (local_id i = 0; i < n; i++) {
        ipcs[i].n = n;
        ipcs[i].id = i;
        ipcs[i].parentPid = parentPid;
        for (int j = 0; j < n; j++) {
            if (i != j) {
                int descriptors[2];
                if (pipe(descriptors) == -1) {
                    perror("Problem with creating pipe");
                    exit(EXIT_FAILURE);
                } else {
                    ipcs[i].pipes[j].in = descriptors[0];
                    ipcs[j].pipes[i].out = descriptors[1];
                }
            }
        }


    }

    for (local_id i = 0; i < n; i++) {
        if (i != PARENT_ID) {
            ipcs[i].pid = fork();
            if (ipcs[i].pid == -1) {
                perror("Problem with fork");
                exit(EXIT_FAILURE);
            } else if (ipcs[i].pid == 0) {
                ipcs[i].pid = getpid();
                // Close pipes for once opened pipe
                for (local_id k = 0; k < n; k++) {
                    if (i != k) {
                        for (local_id l = 0; l < n; l++) {
                            if (l != k) {
                                close(ipcs[k].pipes[l].in);
                                close(ipcs[k].pipes[l].out);
                            }
                        }
                    }
                }
                //
                exit(!runChildProcess(eventLogFile, ipcs[i])); // <- Already close all pipes in the end
            }
        } else {
            ipcs[PARENT_ID].pid = parentPid;
        }
//        for (local_id j = 0; j < n; j++) {
//            if (i != j) {
//                printLog(pipesLogFile,
//                         "From %d to %d: In - %d / Out - %d\n",
//                         (int) i, (int) j, ipcs[i].pipes[j].in, ipcs[i].pipes[j].out); //Have duplicated output
//            }
//        }
    }


    fclose(pipesLogFile);
    fclose(eventLogFile);

    return ipcs;
}

bool runChildProcess(FILE *logFile, struct Ipc ipc) {
    printLog(logFile, log_started_fmt, ipc.id, ipc.pid, ipc.parentPid);

    Message multicastMessageStarted;
    createMessage(&multicastMessageStarted,
                  STARTED,
                  log_started_fmt,
                  ipc.id, ipc.pid, ipc.parentPid); // Add ipc ifno
    send_multicast(&ipc, &multicastMessageStarted); //STARTED

    local_id i = 1;
    local_id processAmount = ipc.n;
    while (i < processAmount) {
        Message receiveMessage;
        if (i != ipc.id) {
            receive(&ipc, i, &receiveMessage);
        }
        i++;
    }

    printLog(logFile, log_received_all_started_fmt, ipc.id);
    printLog(logFile, log_done_fmt, ipc.id);

    Message multicastMessageDone;
    createMessage(&multicastMessageDone,
                  DONE,
                  log_done_fmt,
                  ipc.id); // Add ipc ifno
    send_multicast(&ipc, &multicastMessageDone); //DONE

    i = 1;
    while (i < processAmount) {
        Message receiveMessage;
        if (i != ipc.id) {
            receive(&ipc, i, &receiveMessage);
        }
        i++;
    }

    closeProcessPipes(&ipc);
    printLog(logFile, log_received_all_done_fmt, ipc.id);

    return 0;
}

void createMessage(Message *message, MessageType type, const char *format, ...) {
    va_list ap;
    va_start(ap, format);

    message->s_header.s_magic = MESSAGE_MAGIC;
    message->s_header.s_type = type;
    message->s_header.s_payload_len = vsnprintf(message->s_payload, MAX_PAYLOAD_LEN, format, ap);


    va_end(ap);
}
