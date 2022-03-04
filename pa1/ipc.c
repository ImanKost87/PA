#include <unistd.h>
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
        ssize_t sizeWrite = write(ipc->pipes[dst].out, msg, messageSize);
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
        ssize_t sizeRead = read(ipc->pipes[from].in, &msg->s_header, headerSize);
        if (headerSize != sizeRead) {
            return -1;
        }
    }

    if (from != currentId) {
        size_t otherSize = msg->s_header.s_payload_len;
        ssize_t sizeRead = read(ipc->pipes[from].in, msg->s_payload, otherSize);
        if (otherSize != sizeRead) {
            return -2;
        }
    }

    return 0;
}

int receive_any(void * self, Message * msg) {
    struct Ipc *ipc = (struct Ipc *) self;

    local_id processAmount = ipc->n;
    for (local_id i = 0; i < processAmount; i++) {
        local_id currentId = ipc->id;
        if (i != currentId) {
            if (receive(ipc, i, msg) == 0) {
                return 0;
            } else {
                return -1;
            }
        }
    }

    return 0;
}
