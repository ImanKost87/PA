#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include "pa2345.h"
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

struct Ipc *runMainProcess(local_id n, balance_t *balances) {
    struct Ipc *ipcs = malloc(n * sizeof(struct Ipc));

    FILE *pipesLogFile = fopen(pipes_log, "a");
    FILE *eventLogFile = fopen(events_log, "a");

    pid_t parentPid = getpid();

    for (local_id i = 0; i < n; i++) {
        ipcs[i].n = n;
        ipcs[i].id = i;
        ipcs[i].balance = balances[i];
        ipcs[i].parentPid = parentPid;
        for (int j = 0; j < n; j++) {
            if (i != j) {
                int descriptors[2];
                if (pipe(descriptors) == -1) {
                    perror("Problem with pipe");
                    exit(EXIT_FAILURE);
                } else {
                    //Check later
                    int flag1 = fcntl(descriptors[0], F_GETFL);
                    if (flag1 == -1) {
                        perror("Problem with nonblock");
                        exit(EXIT_FAILURE);
                    }
                    if (fcntl(descriptors[0], F_SETFL, flag1 | O_NONBLOCK) == -1) {
                        perror("Problem with nonblock");
                        exit(EXIT_FAILURE);
                    }
                    int flag2 = fcntl(descriptors[1], F_GETFL);
                    if (flag2 == -1) {
                        perror("Problem with nonblock");
                        exit(EXIT_FAILURE);
                    }
                    if (fcntl(descriptors[1], F_SETFL, flag2 | O_NONBLOCK) == -1) {
                        perror("Problem with nonblock");
                        exit(EXIT_FAILURE);
                    }
                    //
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
                exit(!runChildProcess(eventLogFile,ipcs[i]));
            }
        } else {
            ipcs[PARENT_ID].pid = parentPid;
        }
    }


    fclose(pipesLogFile);
    fclose(eventLogFile);

    return ipcs;
} // +--

bool runChildProcess(FILE *logFile, struct Ipc ipc) {

    printLog(logFile,
             log_started_fmt,
             get_physical_time(), ipc.id, ipc.pid, ipc.parentPid, ipc.balance);

    Message multicastMessageStarted;
    createMessage(&multicastMessageStarted,
                  STARTED,
                  log_started_fmt,
                  get_physical_time(), ipc.id, ipc.pid, ipc.parentPid, ipc.balance);                  
    send_multicast(&ipc, &multicastMessageStarted);
    
    local_id i = 1;
    local_id proccessAmount = ipc.n;
    while (i < proccessAmount) {
        Message msg;
        if (i != ipc.id) {
            receiveBlocking(&ipc, i, &msg);
        }
        i++;
    }

    printLog(logFile,
             log_received_all_started_fmt,
             get_physical_time(), ipc.id);

    BalanceHistory history;
    history.s_id = ipc.id;
    history.s_history_len = 0;

    size_t count = 1;
    bool exp = true;
    while (exp) {
        Message historyMessage;
        receive_any(&ipc, &historyMessage);

        const timestamp_t endTime = get_physical_time();
        timestamp_t time = history.s_history_len;
        while(time < endTime) {
            history.s_history[time].s_time = time;
            history.s_history[time].s_balance = ipc.balance;
            history.s_history[time].s_balance_pending_in = 0;
            time += 1;
        }
        history.s_history_len = endTime;

        switch (historyMessage.s_header.s_type) {
            case STOP: {
                count += 1;
                printLog(logFile,
                         log_done_fmt,
                         get_physical_time(), ipc.id, ipc.balance);

                Message multicastMessageDone;
                createMessage(&multicastMessageDone,
                              DONE,
                              log_done_fmt,
                              get_physical_time(), ipc.id, ipc.balance);                  
                send_multicast(&ipc, &multicastMessageDone);
                break;
            }
            case TRANSFER: {
                TransferOrder *order = (TransferOrder *) historyMessage.s_payload;
                if (ipc.id == order->s_dst) {
                    printLog(logFile,
                             log_transfer_in_fmt,
                             endTime, ipc.id, order->s_amount, order->s_src);

                    Message ackMessage;
                    ackMessage.s_header.s_magic = MESSAGE_MAGIC;
                    ackMessage.s_header.s_type = ACK;
                    ackMessage.s_header.s_payload_len = 0;

                    ipc.balance += order->s_amount;

                    send(&ipc, PARENT_ID, &ackMessage);
                } else if (ipc.id == order->s_src) {
                    ipc.balance -= order->s_amount;

                    send(&ipc, order->s_dst, &historyMessage);

                    printLog(logFile,
                             log_transfer_out_fmt,
                             endTime, ipc.id, order->s_amount, order->s_dst);
                }
                break;
            }
            case DONE: {
                count += 1;
                if (count == proccessAmount) {
                    exp = false;
                }
                break;
            }
        }
    }

    uint8_t len = history.s_history_len;
    history.s_history[len].s_time = len;
    history.s_history[len].s_balance = ipc.balance;
    history.s_history_len = len + 1;

    Message balanceMessage;
    balanceMessage.s_header.s_magic = MESSAGE_MAGIC;
    balanceMessage.s_header.s_type = BALANCE_HISTORY;
    balanceMessage.s_header.s_payload_len = sizeof(BalanceHistory); 
    memcpy(balanceMessage.s_payload, &history, balanceMessage.s_header.s_payload_len);
    send(&ipc, PARENT_ID, &balanceMessage);

    printLog(logFile,
             log_received_all_done_fmt, 
             get_physical_time(), ipc.id);

    closeProcessPipes(&ipc);

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
