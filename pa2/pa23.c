#include <stdbool.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <string.h>
#include "ipc_struct.h"
#include "banking.h"

void transfer(void * parent_data, local_id src, local_id dst,
              balance_t amount) {
    TransferOrder order;
    order.s_src = src;
    order.s_dst = dst;
    order.s_amount = amount;

    Message messageOrder;
    messageOrder.s_header.s_magic = MESSAGE_MAGIC;
    messageOrder.s_header.s_type = TRANSFER;
    messageOrder.s_header.s_payload_len = sizeof(TransferOrder);

    send(parent_data, src, &messageOrder);

    bool expr = true;
    while (expr) {
        receive(parent_data, dst, &messageOrder);
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            continue;
        } else {
            expr = false;
        }
    }
}

void makeHistory(local_id n, struct Ipc *ipc) {
    AllHistory allHistory;
    allHistory.s_history_len = 0;

    local_id i = 1;
    while (i < n) {
        Message message;

        while (true) {
            receive(ipc, i, &message);
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            } else {
                break;
            }
        }

        allHistory.s_history_len += 1;
        memcpy(&allHistory.s_history[i - 1], message.s_payload, sizeof(BalanceHistory));

        i += 1;
    }

    print_history(&allHistory);
}

int main(int argc, char * argv[]) {
    local_id n;
    balance_t *balances;

    if (argc < 3) {
        exit(EXIT_FAILURE);
    } else {
        n = (local_id) (atoi(argv[2]) + 1);
        balances = malloc(n * sizeof(balance_t));
        balances[0] = 0;
        for (local_id i = 1; i < n; i++) {
            balances[i] = (balance_t)(atoi(argv[2 + i]));
        }
    }

    struct Ipc *ipcs = runMainProcess(n, balances);

    Message receiveAnyNBMessage1;
    receiveAnyNonBlocking(ipcs, &receiveAnyNBMessage1);

    bank_robbery(ipcs, n - 1);

    Message messageMulticast;
    messageMulticast.s_header.s_magic = MESSAGE_MAGIC;
    messageMulticast.s_header.s_type = STOP;
    send_multicast(ipcs, &messageMulticast);

    makeHistory(n, ipcs);

    Message receiveAnyNBMessage2;
    receiveAnyNonBlocking(ipcs, &receiveAnyNBMessage2);

    local_id i = 0;
    while (i < n) {
        wait(&ipcs[i].pid);
        i++;
    }

    closePipes(ipcs);

    return 0;
}
