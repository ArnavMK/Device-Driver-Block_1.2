#include "gamepad.h"

void cbuffer_init(struct cbuffer *buf) {
    buf->read_pos = 0;
    buf->write_pos = 0;
    buf->count = 0;
}

int cbuffer_is_empty(struct cbuffer *buf) {
    return (buf->count == 0);
}

int cbuffer_is_full(struct cbuffer *buf) {
    return (buf->count == BUFFER_SIZE);
}

void cbuffer_push(struct cbuffer *buf, char new_data) {
    if (!cbuffer_is_full(buf)) {
        buf->data[buf->write_pos] = new_data;
        buf->write_pos = (buf->write_pos + 1) % BUFFER_SIZE;
        buf->count++;
    }
}

char cbuffer_pop(struct cbuffer *buf) {
    char return_data = 0;
    if (!cbuffer_is_empty(buf)) {
        return_data = buf->data[buf->read_pos];
        buf->read_pos = (buf->read_pos + 1) % BUFFER_SIZE;
        buf->count--;
    }
    return return_data;
}
