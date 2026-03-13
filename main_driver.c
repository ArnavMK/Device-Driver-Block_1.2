#include "gamepad.h"

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/spinlock.h>
#include <linux/wait.h>
#include <linux/usb.h>
#include <linux/input.h>
#include <linux/slab.h>

MODULE_LICENSE("GPL"); 
MODULE_AUTHOR("Mark,Damien,Arnav,Jerry");
MODULE_DESCRIPTION("Controller device driver USB");
MODULE_VERSION("1.0");

static int __init gamepadDriver_init(void);
static void __exit gamepadDriver_exit(void);

static const struct usb_device_id controllerArr[] = {
    { USB_DEVICE(XBOX_VENDOR_ID, XBOX_PRODUCT_ID) },
    { USB_DEVICE_INTERFACE_NUMBER(XBOX_VENDOR_ID, XBOX_PRODUCT_ID, 0) },
    { USB_VENDOR_AND_INTERFACE_INFO(XBOX_VENDOR_ID, 0xff, 0x47, 0xd0) },
    { }
};

static struct usb_driver controller_driver = {
    .name       = "controllerDriver",
    .probe      = controller_probe,
    .disconnect = controller_disconnect,
    .id_table   = controllerArr,
};

MODULE_DEVICE_TABLE(usb, controllerArr);

struct gamepad_buffer myDeviceBuffer;
struct gamepad_stats  myDeviceStats;
int major;

static int __init gamepadDriver_init(void)
{
    int result;

    myDeviceBuffer.read_pos  = 0;
    myDeviceBuffer.write_pos = 0;
    myDeviceBuffer.count     = 0;
    spin_lock_init(&myDeviceBuffer.lock);

    major = register_chrdev(0, DEVICE_NAME, &fops);
    if (major < 0) {
        printk(KERN_ALERT "Failed to register controller\n");
        return major;
    }

    result = usb_register(&controller_driver);
    if (result) {
        unregister_chrdev(major, DEVICE_NAME);
        printk(KERN_ALERT "Failed to register USB driver\n");
        return result;
    }

    printk(KERN_INFO "Controller loaded with major number %d\n", major);
    return 0;
}

static void __exit gamepadDriver_exit(void)
{
    usb_deregister(&controller_driver);
    unregister_chrdev(major, DEVICE_NAME);
    printk(KERN_INFO "Controller unloaded\n");
}

int controller_probe(struct usb_interface *usbInterface, const struct usb_device_id *id)
{
    struct usb_device *usbDev = interface_to_usbdev(usbInterface);
    struct xboxController *controller;
    struct usb_host_interface *interface_desc;
    struct usb_endpoint_descriptor *endpoint;
    int error;
    int i;
    int found_endpoint = 0;
    int urb_submit_result;
    printk(KERN_INFO "Controller probe called for device %04x:%04x\n",
           usbDev->descriptor.idVendor, usbDev->descriptor.idProduct);
    if (usbInterface->cur_altsetting->desc.bInterfaceNumber != 0) {
        return -ENODEV;
    }
    controller = kzalloc(sizeof(struct xboxController), GFP_KERNEL);
    if (!controller) {
        printk(KERN_ERR "Could not allocate memory for controller\n");
        return -ENOMEM;
    }

    controller->usbDev   = usbDev;
    controller->inputDev = input_allocate_device();
    if (!controller->inputDev) {
        printk(KERN_ERR "Could not allocate inputDev device\n");
        kfree(controller);
        return -ENOMEM;
    }

    controller->buff = kzalloc(64, GFP_KERNEL);
    if (!controller->buff) {
        printk(KERN_ERR "Could not allocate buffer for controller\n");
        input_free_device(controller->inputDev);
        kfree(controller);
        return -ENOMEM;
    }

    controller->interruptURB = usb_alloc_urb(0, GFP_KERNEL);
    if (!controller->interruptURB) {
        printk(KERN_ERR "Could not allocate URB for controller\n");
        kfree(controller->buff);
        input_free_device(controller->inputDev);
        kfree(controller);
        return -ENOMEM;
    }

   
    controller->inputDev->name       = "Xbox Controller";
    controller->inputDev->phys       = "usb/inputDev0";
    controller->inputDev->id.bustype = BUS_USB;
    controller->inputDev->id.vendor  = XBOX_VENDOR_ID;
    controller->inputDev->id.product = XBOX_PRODUCT_ID;
    controller->inputDev->id.version = 0x0100;

    /* Buttons */
    input_set_capability(controller->inputDev, EV_KEY, BTN_A);
    input_set_capability(controller->inputDev, EV_KEY, BTN_B);
    input_set_capability(controller->inputDev, EV_KEY, BTN_X);
    input_set_capability(controller->inputDev, EV_KEY, BTN_Y);
    input_set_capability(controller->inputDev, EV_KEY, BTN_TL);
    input_set_capability(controller->inputDev, EV_KEY, BTN_TR);
    input_set_capability(controller->inputDev, EV_KEY, BTN_SELECT);
    input_set_capability(controller->inputDev, EV_KEY, BTN_START);

    /* D-pad */
    input_set_capability(controller->inputDev, EV_KEY, BTN_DPAD_UP);
    input_set_capability(controller->inputDev, EV_KEY, BTN_DPAD_DOWN);
    input_set_capability(controller->inputDev, EV_KEY, BTN_DPAD_LEFT);
    input_set_capability(controller->inputDev, EV_KEY, BTN_DPAD_RIGHT);

    /* Left stick */
    input_set_abs_params(controller->inputDev, ABS_X,  -32768, 32767, 16, 128);
    input_set_abs_params(controller->inputDev, ABS_Y,  -32768, 32767, 16, 128);

    /* Triggers */
    input_set_abs_params(controller->inputDev, ABS_Z,  0, 255, 0, 0);
    input_set_abs_params(controller->inputDev, ABS_RZ, 0, 255, 0, 0);

    /* Right stick */
    input_set_abs_params(controller->inputDev, ABS_RX, -32768, 32767, 16, 128);
    input_set_abs_params(controller->inputDev, ABS_RY, -32768, 32767, 16, 128);

    usb_set_intfdata(usbInterface, controller);

    error = input_register_device(controller->inputDev);
    if (error) {
        printk(KERN_ERR "Could not register inputDev device\n");
        usb_free_urb(controller->interruptURB);
        input_free_device(controller->inputDev);
        kfree(controller->buff);
        kfree(controller);
        return error;
    }

    interface_desc = usbInterface->cur_altsetting;
    for (i = 0; i < interface_desc->desc.bNumEndpoints; i++) {
        endpoint = &interface_desc->endpoint[i].desc;
        if (usb_endpoint_xfer_int(endpoint) && usb_endpoint_dir_in(endpoint)) {
            usb_fill_int_urb(
                controller->interruptURB,
                usbDev,
                usb_rcvintpipe(usbDev, endpoint->bEndpointAddress),
                controller->buff,
                64,
                controller_irq_callback,
                controller,
                endpoint->bInterval
            );
            found_endpoint = 1;
            break;
        }
    }

   
    if (!found_endpoint) {
        printk(KERN_ERR "No interrupt IN endpoint found for controller\n");
        input_unregister_device(controller->inputDev);
        kfree(controller->buff);
        usb_free_urb(controller->interruptURB);
        kfree(controller);
        return -ENODEV;
    }

    urb_submit_result = usb_submit_urb(controller->interruptURB, GFP_KERNEL);
    if (urb_submit_result) {
        printk(KERN_ERR "Could not submit URB for controller\n");
        input_unregister_device(controller->inputDev);
        kfree(controller->buff);
        usb_free_urb(controller->interruptURB);
        kfree(controller);
        return urb_submit_result;
    }

    printk(KERN_INFO "Controller Connected.\n");
    return 0;
}

void controller_disconnect(struct usb_interface *usbInterface)
{
    struct xboxController *controller = usb_get_intfdata(usbInterface);

    usb_set_intfdata(usbInterface, NULL);

    if (controller) {
        usb_kill_urb(controller->interruptURB);
        usb_free_urb(controller->interruptURB);
        kfree(controller->buff);
        input_unregister_device(controller->inputDev);
        kfree(controller);
    }

    printk(KERN_INFO "Controller Disconnected.\n");
}

void controller_irq_callback(struct urb *urb)
{
    struct xboxController *controller = urb->context;
    unsigned char         *buff       = controller->buff;
    int                    status     = urb->status;

    if (status) {
        /* URB was intentionally killed — do not resubmit */
        if (status == -ENOENT || status == -ECONNRESET || status == -ESHUTDOWN)
            return;
        printk(KERN_ERR "URB error: %d\n", status);
        goto resubmit;
    }

    /* Buttons (byte 2) */
    input_report_key(controller->inputDev, BTN_A,      buff[2] & 0x01);
    input_report_key(controller->inputDev, BTN_B,      buff[2] & 0x02);
    input_report_key(controller->inputDev, BTN_X,      buff[2] & 0x08);
    input_report_key(controller->inputDev, BTN_Y,      buff[2] & 0x10);
    input_report_key(controller->inputDev, BTN_TL,     buff[2] & 0x40);
    input_report_key(controller->inputDev, BTN_TR,     buff[2] & 0x80);
    input_report_key(controller->inputDev, BTN_SELECT, buff[2] & 0x04);
    input_report_key(controller->inputDev, BTN_START,  buff[2] & 0x20);

    /* D-pad (byte 1) */
    input_report_key(controller->inputDev, BTN_DPAD_UP,    buff[1] & 0x01);
    input_report_key(controller->inputDev, BTN_DPAD_DOWN,  buff[1] & 0x02);
    input_report_key(controller->inputDev, BTN_DPAD_LEFT,  buff[1] & 0x04);
    input_report_key(controller->inputDev, BTN_DPAD_RIGHT, buff[1] & 0x08);

    /* Triggers (bytes 4–5) */
    input_report_abs(controller->inputDev, ABS_Z,  buff[4]);
    input_report_abs(controller->inputDev, ABS_RZ, buff[5]);

    /* Left stick (bytes 6–9) */
    input_report_abs(controller->inputDev, ABS_X,
                     (s16)(buff[6] | (buff[7] << 8)));
    input_report_abs(controller->inputDev, ABS_Y,
                     -(s16)(buff[8] | (buff[9] << 8)));   /* invert Y */

    /* Right stick (bytes 10–13) */
    input_report_abs(controller->inputDev, ABS_RX,
                     (s16)(buff[10] | (buff[11] << 8)));
    input_report_abs(controller->inputDev, ABS_RY,
                     -(s16)(buff[12] | (buff[13] << 8))); /* invert Y */

    input_sync(controller->inputDev);

resubmit:
    status = usb_submit_urb(urb, GFP_ATOMIC);
    if (status)
        printk(KERN_ERR "Failed to resubmit URB: %d\n", status);
}

module_init(gamepadDriver_init);
module_exit(gamepadDriver_exit);