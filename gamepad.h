//
// Created by hende on 09/03/2026.
//

#ifndef GAMEPAD_DRIVER_H
#define GAMEPAD_DRIVER_H

#include <linux/fs.h>
#include <linux/mutex.h>
#include <linux/usb.h>
#include <linux/input.h>

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

//Buffer Commands
void gamepad_buffer_init(struct gamepad_buffer *buf);
int gamepad_buffer_is_empty(struct gamepad_buffer *buf);
int gamepad_buffer_is_full(struct gamepad_buffer *buf);
void gamepad_buffer_push(struct gamepad_buffer *buf, char new_data);
char gamepad_buffer_pop(struct gamepad_buffer *buf);

// ── Globals (defined in gamepadDriver.c)
extern struct gamepad_buffer myDeviceBuffer;
extern int major;
extern const struct file_operations fops;

// ── USB probe / disconnect
static int  controller_probe(struct usb_interface *usbInterface, const struct usb_device_id *id);
static void controller_disconnect(struct usb_interface *usbInterface);

// ── URB interrupt callback
static void controller_irq_callback(struct urb *urb);

#endif /* GAMEPAD_DRIVER_H */