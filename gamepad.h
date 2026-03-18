//
// Created by hende on 09/03/2026.
//

#ifndef GAMEPAD_DRIVER_H
#define GAMEPAD_DRIVER_H

#include <linux/fs.h>
#include <linux/usb.h>
#include <linux/input.h>
#include <linux/ioctl.h>
#include <linux/spinlock.h>
#include <linux/string.h>


//── Device identity
#define DEVICE_NAME "gamepadDriver"
#define BUFFER_SIZE  64  /* URB / circular-buffer capacity */

// ── Xbox One controller USB IDs
#define XBOX_VENDOR_ID 0x045e
#define XBOX_PRODUCT_ID 0x0b12

#define MAX_BUTTONS 12

// ── Circular buffer
struct gamepad_buffer {
    unsigned char data[BUFFER_SIZE];
    int read_pos;
    int write_pos;
    int count;
    spinlock_t lock; 
};

#define GAMEPAD_BTN_DPAD_UP 0x01
#define GAMEPAD_BTN_DPAD_DOWN 0x02
#define GAMEPAD_BTN_DPAD_LEFT 0x04
#define GAMEPAD_BTN_DPAD_RIGHT 0x08
#define GAMEPAD_BTN_START 0x10
#define GAMEPAD_BTN_SELECT 0x20
#define GAMEPAD_BTN_LB 0x40
#define GAMEPAD_BTN_RB 0x80
#define GAMEPAD_BTN_A 0x10
#define GAMEPAD_BTN_B 0x20
#define GAMEPAD_BTN_X 0x40
#define GAMEPAD_BTN_Y 0x80

//── Per-device structure
struct xboxController {
    struct usb_device *usbDev;
    struct input_dev *inputDev;
    unsigned char *buff;
    struct urb *interruptURB;

	unsigned char prev_b4;
	unsigned char prev_b5;
};

struct gamepad_stats {
    unsigned long buttons_pressed;
	unsigned long individual_counts[MAX_BUTTONS];
    unsigned long packets_received;
    int is_connected;
    int is_halted;
};
static const char *button_names[] = {
    "DPAD_UP", "DPAD_DOWN", "DPAD_LEFT", "DPAD_RIGHT",
    "START", "SELECT", "LB", "RB", "A", "B", "X", "Y"
};
extern wait_queue_head_t wq;

// ioctl command definitions
#define GAMEPAD_MAGIC 'G'
#define GAMEPAD_GET_STATS _IOR(GAMEPAD_MAGIC, 1, struct gamepad_stats)
#define GAMEPAD_RESET _IO (GAMEPAD_MAGIC, 2)

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

int controller_probe(struct usb_interface *usbInterface, const struct usb_device_id *id);
void controller_disconnect(struct usb_interface *usbInterface);
void controller_irq_callback(struct urb *urb);

#endif /* GAMEPAD_DRIVER_H */