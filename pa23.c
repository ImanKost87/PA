#include <stdbool.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <string.h>
#include "ipc_struct.h"
#include "banking.h"

int receiveBlocking(void * self, local_id id, Message * msg) {
    struct Ipc *ipc = (struct Ipc *) self;
    local_id currentId = id;
    bool state = true;
    while (true) {
        const int res = receive(ipc, currentId, msg);
        if (res != 0) {
            if (res == EAGAIN) {
                continue;
            } else {
                state = false;
            }
        }
        return res;
    }
}

void receiveAnyBlocking(void * self, local_id n) {
    local_id i = 1;
    while (i < n) {
        Message msg;
        receiveBlocking(self, i, &msg);
        i++;
    }
}

void transfer(void * parent_data, local_id src, local_id dst,
              balance_t amount) {
    struct Ipc * ipc = (struct Ipc *) parent_data;

    TransferOrder order;
    order.s_src = src;
    order.s_dst = dst;
    order.s_amount = amount;

    Message transferMessage;
    transferMessage.s_header.s_magic = MESSAGE_MAGIC;
    transferMessage.s_header.s_type = TRANSFER;
    transferMessage.s_header.s_payload_len = sizeof(TransferOrder);

    memcpy(&transferMessage.s_payload, &order, sizeof(TransferOrder));
    send(ipc, src, &transferMessage);

    receiveBlocking(ipc, dst, &transferMessage);
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
    struct Ipc *ipcParent = (&ipcs)[0]; //BAD VERY BAD

    receiveAnyBlocking(ipcParent, n);

    bank_robbery(ipcs, n - 1);

    Message stopMessage;
    stopMessage.s_header.s_magic = MESSAGE_MAGIC;
    stopMessage.s_header.s_type = STOP;
    send_multicast(ipcParent, &stopMessage);

    receiveAnyBlocking(ipcParent, n);

    AllHistory allHistory;
    allHistory.s_history_len = n - 1;

    local_id i = 1;
    while (i < n) {
        Message msg;
        receiveBlocking(ipcParent, i, &msg);

        const BalanceHistory * const history = (BalanceHistory *) msg.s_payload;

        allHistory.s_history[i - 1].s_id = history->s_id;
        allHistory.s_history[i - 1].s_history_len = history->s_history_len;

        size_t j = 0;
        while (j < history->s_history_len) {
            allHistory.s_history[i - 1].s_history[j] = history->s_history[j];
            j++;
        }
        i++;
    }

    print_history(&allHistory);

    i = 0;
    while (i < n) {
        wait(&ipcs[i].pid);
        i++;
    }

    closePipes(ipcs);
    free(balances);
    free(ipcs);

    return 0;
}
