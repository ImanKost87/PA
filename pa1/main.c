#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include "ipc_struct.h"
#include "ipc.h"


int main(int argc, char *argv[]) {
    local_id n;

    if (argc != 3) {
        exit(EXIT_FAILURE);
    } else {
        n = (local_id) (atoi(argv[2]) + 1);
    }

    struct Ipc *ipcs = runMainProcess(n);

    //Close for PARENT
    for (local_id k = 0; k < n; k++) {
        if (PARENT_ID != k) {
            for (local_id l = 0; l < n; l++) {
                if (l != k) {
                    close(ipcs[k].pipes[l].in);
                    close(ipcs[k].pipes[l].out);
                }
            }
        }
    }

    Message receiveMessage;
    for (local_id i = 1; i < n; i++) {
        if (ipcs[i].id != i) {
            receive(ipcs, i, &receiveMessage);
        }
    }

    for (local_id i = 1; i < n; i++) {
        if (ipcs[i].id != i) {
            receive(ipcs, i, &receiveMessage);
        }
    }

    local_id i = 0;
    while (i < n) {
        wait(&ipcs[i].pid);
        i++;
    }

    closePipes(ipcs);

    return 0;
}
