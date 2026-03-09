#ifndef GAMEPAD_H
#define GAMEPAD_H

#include <linux/fs.h>
#include <linux/wait.h>

#define BUFFER_SIZE 256 // A 256-byte fixed size FIFO

struct cbuffer {
    char data[BUFFER_SIZE];
    int read_pos;
    int write_pos;
    int count; 
};

// Prototypes now use 'cbuffer'
void cbuffer_init(struct cbuffer *buf);
int cbuffer_is_empty(struct cbuffer *buf);
int cbuffer_is_full(struct cbuffer *buf);
void cbuffer_push(struct cbuffer *buf, char new_data);
char cbuffer_pop(struct cbuffer *buf);

#endif
