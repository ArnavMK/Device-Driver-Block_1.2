//
// Created by hende on 09/03/2026.
//

#ifndef GAMEPAD_DRIVER_H
#define GAMEPAD_DRIVER_H

#include <linux/fs.h>
#include <linux/mutex.h>
#include <linux/usb.h>
#include <linux/input.h>
#include <linux/ioctl.h>
#include <linux/spinlock.h>

//── Device identity
#define DEVICE_NAME "gamepadDriver"
#define BUFFER_SIZE  64  /* URB / circular-buffer capacity */

// ── Xbox One controller USB IDs
#define XBOX_VENDOR_ID 0x045e
#define XBOX_PRODUCT_ID 0x02ea

// ── Circular buffer
struct gamepad_buffer {
    unsigned char data[BUFFER_SIZE];
    int read_pos;
    int write_pos;
    int count;
    spinlock_t lock;
};

//── Per-device structure
struct xboxController {
    struct usb_device *usbDev;
    struct input_dev *inputDev;
    unsigned char *buff;
    struct urb *interruptURB;
};

struct gamepad_stats {
    unsigned long buttons_pressed;
    unsigned long packets_received;
    int is_connected;
    int is_halted;
};
DECLARE_WAIT_QUEUE_HEAD(wq);  

// ioctl command definitions
#define GAMEPAD_MAGIC      'G'
#define GAMEPAD_GET_STATS  _IOR(GAMEPAD_MAGIC, 1, struct gamepad_stats)
#define GAMEPAD_RESET      _IO (GAMEPAD_MAGIC, 2)
#define GAMEPAD_ESTOP      _IO (GAMEPAD_MAGIC, 3)

//Buffer Commands
void gamepad_buffer_init(struct gamepad_buffer *buf);
int gamepad_buffer_is_empty(struct gamepad_buffer *buf);
int gamepad_buffer_is_full(struct gamepad_buffer *buf);
void gamepad_buffer_push(struct gamepad_buffer *buf, char new_data);
char gamepad_buffer_pop(struct gamepad_buffer *buf);

// ── Globals (defined in gamepadDriver.c)
extern struct gamepad_buffer myDeviceBuffer;
extern struct gamepad_stats  myDeviceStats;
extern int major;
extern struct file_operations fops;


// ioctl handler
long gamepad_ioctl(struct file *file, unsigned int cmd, unsigned long arg);

// Admin proc interface
int  admin_init(void);
void admin_exit(void);

#endif /* GAMEPAD_DRIVER_H */