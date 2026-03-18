#include "gamepad.h"

void gamepad_buffer_init(struct gamepad_buffer *buf) {
    buf->read_pos = 0;
    buf->write_pos = 0;
    buf->count = 0;
}

int gamepad_buffer_is_empty(struct gamepad_buffer *buf) {
    return (buf->count == 0);
}

int gamepad_buffer_is_full(struct gamepad_buffer *buf) {
    return (buf->count == BUFFER_SIZE);
}

void gamepad_buffer_push(struct gamepad_buffer *buf, char new_data) {
    if (!gamepad_buffer_is_full(buf)) {
        buf->data[buf->write_pos] = new_data;
        buf->write_pos = (buf->write_pos + 1) % BUFFER_SIZE;
        buf->count++;
    }
}

char gamepad_buffer_pop(struct gamepad_buffer *buf) {
    char return_data = 0;
    if (!gamepad_buffer_is_empty(buf)) {
        return_data = buf->data[buf->read_pos];
        buf->read_pos = (buf->read_pos + 1) % BUFFER_SIZE;
        buf->count--;
    }
    return return_data;
}
