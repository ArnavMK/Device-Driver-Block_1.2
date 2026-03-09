//
// Created by hende on 09/03/2026.
//

#ifndef GAMEPAD_DRIVER_H
#define GAMEPAD_DRIVER_H

#include <linux/fs.h>
#include <linux/mutex.h>
#include <linux/usb.h>
#include <linux/input.h>

/* ── Device identity ─────────────────────────────────────────── */
#define DEVICE_NAME     "gamepadDriver"
#define BUFFER_SIZE     64          /* URB / circular-buffer capacity */

/* ── Xbox One controller USB IDs ─────────────────────────────── */
#define XBOX_VENDOR_ID   0x045e
#define XBOX_PRODUCT_ID  0x02ea

/* ── Circular buffer ─────────────────────────────────────────── */
struct gamepad_buffer {
    unsigned char   data[BUFFER_SIZE];
    int             head;
    int             tail;
    int             count;
    struct mutex    lock;
};

/* ── Per-device structure ────────────────────────────────────── */
struct xboxController {
    struct usb_device   *usbDev;
    struct input_dev    *inputDev;
    unsigned char       *buff;
    struct urb          *interruptURB;
};

/* ── Globals (defined in gamepadDriver.c) ────────────────────── */
extern struct gamepad_buffer    myDeviceBuffer;
extern int                      major;
extern const struct file_operations fops;

/* ── USB probe / disconnect ──────────────────────────────────── */
static int  controller_probe(struct usb_interface *usbInterface,
                             const struct usb_device_id *id);
static void controller_disconnect(struct usb_interface *usbInterface);

/* ── URB interrupt callback ──────────────────────────────────── */
static void controller_irq_callback(struct urb *urb);

#endif /* GAMEPAD_DRIVER_H */