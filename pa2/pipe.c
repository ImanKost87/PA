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
                    if (fcntl(descriptors[1], F_SETFL, flag1 | O_NONBLOCK) == -1) {
                        perror("Problem with nonblock");
                        exit(EXIT_FAILURE);
                    }
                    //
                    ipcs[i].pipes[j].in = descriptors[0];
                    ipcs[j].pipes[i].out = descriptors[1];

                }
            }
        }

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
                  get_physical_time(), ipc.id, ipc.pid, ipc.parentPid, ipc.balance); // Add ipc info
    send_multicast(&ipc, &multicastMessageStarted); //STARTED

    Message receiveNBMessage;
    receiveAnyNonBlocking(&ipc, &receiveNBMessage);

    printLog(logFile,
             log_received_all_started_fmt,
             get_physical_time(), ipc.id);

    BalanceHistory history;
    history.s_id = ipc.id;
    history.s_history_len = 0;

    local_id currentId = ipc.id;
    balance_t currentBalance = ipc.balance;
    bool expr = true;
    while (expr) {
        Message historyMessage;

        if (receive_any(&ipc, &historyMessage) == EAGAIN) {
            continue;
        }

        const timestamp_t endTime = get_physical_time();
        timestamp_t i = history.s_history_len;
        while(i < endTime) {
            history.s_history[i].s_time = i;
            history.s_history[i].s_balance = currentBalance;
            i += 1;
        }

        history.s_history_len = endTime;

        int16_t type = historyMessage.s_header.s_type;
        switch (type) {
            case TRANSFER: {
                TransferOrder order;
                memcpy(&order, historyMessage.s_payload, sizeof(order));
                if (currentId == order.s_dst) {
                    printLog(logFile,
                             log_transfer_in_fmt,
                             endTime, currentId, order.s_amount, order.s_src);

                    Message ackMessage;
                    ackMessage.s_header.s_magic = MESSAGE_MAGIC;
                    ackMessage.s_header.s_type = ACK;
                    ackMessage.s_header.s_payload_len = 0;

                    ipc.balance += order.s_amount;

                    send(&ipc, PARENT_ID, &ackMessage);
                } else {
                    printLog(logFile,
                             log_transfer_out_fmt,
                             endTime, currentId, order.s_amount, order.s_dst);

                    ipc.balance -= order.s_amount;

                    send(&ipc, order.s_dst, &historyMessage);
                }
                break;
            }
            case STOP: {
                expr = !expr;
                break;
            }
            case DONE: {
                expr = true;
                break;
            }
        }
    }


    int last = history.s_history_len;
    history.s_history[last].s_time = history.s_history_len;
    history.s_history[last].s_balance = currentBalance;
    history.s_history[last].s_balance_pending_in = 0;
    history.s_history_len += 1;

    Message balanceMessage;
    balanceMessage.s_header.s_magic = MESSAGE_MAGIC;
    balanceMessage.s_header.s_type = BALANCE_HISTORY;
    balanceMessage.s_header.s_local_time = get_physical_time();
    memcpy(balanceMessage.s_payload, &history, sizeof(history));
    balanceMessage.s_header.s_payload_len = sizeof(history);

    send(&ipc, PARENT_ID, &balanceMessage);
    printLog(logFile,
             log_received_all_done_fmt,
             balanceMessage.s_header.s_local_time, currentId);

    Message messageDone;
    createMessage(&messageDone,
                  DONE,
                  log_done_fmt,
                  get_physical_time(), currentId, currentBalance);
    send_multicast(&ipc, &messageDone); //DONE

    Message receiveAnyNBMessage;
    receiveAnyNonBlocking(&ipc, &receiveAnyNBMessage);

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

int receiveAnyNonBlocking(void * self, Message * msg) {
    struct Ipc *ipc = (struct Ipc *) self;

    local_id processAmount = ipc->n;
    local_id currentId = ipc->id;
    for (local_id i = 1; i < processAmount; i++) {
        if (i != currentId) {
            int r;
            while (true) {
                r = receive(ipc, i, msg);
                if (errno != EAGAIN && errno != EWOULDBLOCK) {
                    break;
                }
            }
            if (r) {
                return -1;
            }
        }
    }

    return 0;
}
