#include <unistd.h>
#include <errno.h>
#include <sched.h>
#include "ipc.h"
#include "ipc_struct.h"

int send(void * self, local_id dst, const Message * msg) {
    struct Ipc *ipc = (struct Ipc *) self;

    local_id currentId = ipc->id;
    if (dst != currentId) {
        size_t messageSize;
        size_t messageSize1 = sizeof(Message) + msg->s_header.s_payload_len - MAX_PAYLOAD_LEN;
        size_t messageSize2 = sizeof(MessageHeader) + msg->s_header.s_payload_len;
        if (messageSize1 > messageSize2) {
            messageSize = messageSize1;
        } else {
            messageSize = messageSize2;
        }
        ssize_t sizeWrite;
        while ((sizeWrite = write(ipc->pipes[dst].out, msg, messageSize)) == -1) {
            if (errno != EAGAIN) {
                break;
            }
        }
        if (messageSize != sizeWrite) {
            return -1;
        }
    }

    return 0;
}

int send_multicast(void * self, const Message * msg) {
    struct Ipc *ipc = (struct Ipc *) self;

    local_id processAmount = ipc->n;
    local_id currentId = ipc->id;
    for (local_id i = 0; i < processAmount; i++) {
        if (i != currentId) {
            if (send(ipc, i, msg) == 0) {
                continue;
            } else {
                return -1;
            }
        }
    }

    return 0;
}

int receive(void * self, local_id from, Message * msg) {
    struct Ipc *ipc = (struct Ipc *) self;

    local_id currentId = ipc->id;
    if (from != currentId) {
        size_t headerSize;
        size_t headerSize1 = sizeof(msg->s_header);
        size_t headerSize2 = sizeof(MessageHeader);
        if (headerSize1 > headerSize2) {
            headerSize = headerSize1;
        } else {
            headerSize = headerSize2;
        }
        size_t sizeRead = read(ipc->pipes[from].in, &msg->s_header, headerSize);
        if (headerSize != sizeRead) {
            if (sizeRead == -1) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    return EAGAIN;
                }
            }
            return -1;
        }
    }

    if (from != currentId) {
        size_t otherSize = msg->s_header.s_payload_len;
        size_t sizeRead = read(ipc->pipes[from].in, msg->s_payload, otherSize);
        if (otherSize != sizeRead) {
            return -2;
        }
    }

    return 0;
}

int receive_any(void * self, Message * msg) {
    struct Ipc *ipc = (struct Ipc *) self;

    local_id processAmount = ipc->n;
    while (true) {
        for (local_id i = 0; i < processAmount; i++) {
            local_id currentId = ipc->id;
            if (i != currentId) {
                int status = receive(ipc, i, msg);
                switch (status)
                {
                case EAGAIN:
                    continue;
                    break;
                case 0:
                    return 0;
                default:
                    return status;
                }
            }
        }
    }
}
