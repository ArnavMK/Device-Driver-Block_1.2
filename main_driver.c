#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/usb.h>
#include "gamepadDriver.h"

static int __init gamepadDriver_init(void);
static void __exit gamepadDriver_exit(void);

static const struct usb_device_id controllerArr[] = {
    { USB_DEVICE(0x045e, 0x02ea) },
    { }
};
struct xboxController {
    struct usb_device *usbDev;
    struct input_dev *inputDev;
    unsigned char *buff;
    struct urb *interruptURB;
};

static struct usb_driver controller_driver = {
    .name = "controllerDriver",
    .probe = controller_probe,
    .disconnect = controller_disconnect,
    .id_table = controllerArr,
};

MODULE_DEVICE_TABLE(usb, controllerArr);

struct gamepad_buffer myDeviceBuffer;
int major;

static int __init gamepadDriver_init(void) {
    mutex_init(&myDeviceBuffer.lock);
    myDeviceBuffer.head = 0;
    myDeviceBuffer.tail = 0;
    myDeviceBuffer.count = 0;
    major = register_chrdev(0, DEVICE_NAME, &fops);
    if(major < 0) {
        printk(KERN_ALERT "Failed to register controller\n");
        return major;
    }
    int result = usb_register(&controller_driver);
    if (result) {
        unregister_chrdev(major, DEVICE_NAME);
        printk(KERN_ALERT "Failed to register USB driver\n");
        return result;
    }
    printk(KERN_INFO "Controller loaded with major number %d\n", major);
    return 0;
}

static void __exit gamepadDriver_exit(void) {
    usb_deregister(&controller_driver);
    unregister_chrdev(major, DEVICE_NAME);
    mutex_destroy(&myDeviceBuffer.lock);
    printk(KERN_INFO "Controller unloaded\n");
}

static int controller_probe(struct usb_interface *usbInterface, const struct usb_device_id *id) {
    struct usb_device *usbDev = interface_to_usbdev(usbInterface);
    struct xboxController *controller;
    struct usb_host_interface *interface_desc;
    struct usb_endpoint_descriptor *endpoint;
    int error;
    controller = kzalloc(sizeof(struct xboxController), GFP_KERNEL);
    if (!controller) {
        printk(KERN_ERR "Could not allocate memory for controller\n");
        return -ENOMEM;
    }

    controller->usbDev = usbDev;
    controller->inputDev = input_allocate_device();
    if (!controller->inputDev) {
        printk(KERN_ERR "Could not allocate inputDev device\n");
        kfree(controller);
        return -ENOMEM;
    }

    controller->buff = kzalloc(64, GFP_KERNEL);
    if(!controller->buff) {
        printk(KERN_ERR "Could not allocate buffer for controller\n");
        input_free_device(controller->inputDev);
        kfree(controller);
        return -ENOMEM;
    }

    controller->interruptURB = usb_alloc_urb(0, GFP_KERNEL);
    if(!controller->interruptURB) {
        printk(KERN_ERR "Could not allocate URB for controller\n");
        kfree(controller->buff);
        input_free_device(controller->inputDev);
        kfree(controller);
        return -ENOMEM;
    }

    controller->inputDev->name = "Xbox Controller";
    controller->inputDev->phys = "usb/inputDev0";

    input_set_capability(controller->inputDev, EV_KEY, BTN_A);
    input_set_capability(controller->inputDev, EV_KEY, BTN_B);
    input_set_capability(controller->inputDev, EV_KEY, BTN_X);
    input_set_capability(controller->inputDev, EV_KEY, BTN_Y);
    input_set_capability(controller->inputDev, EV_KEY, BTN_TL);
    input_set_capability(controller->inputDev, EV_KEY, BTN_TR);
    input_set_capability(controller->inputDev, EV_KEY, BTN_SELECT);
    input_set_capability(controller->inputDev, EV_KEY, BTN_START);

    input_set_abs_params(controller->inputDev, ABS_X,  -32768, 32767, 16, 128);
    input_set_abs_params(controller->inputDev, ABS_Y,  -32768, 32767, 16, 128);

    input_set_abs_params(controller->inputDev, ABS_Z,  0, 255, 0, 0);
    input_set_abs_params(controller->inputDev, ABS_RZ, 0, 255, 0, 0);


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
    for (int i = 0; i < interface_desc->desc.bNumEndpoints; i++) {
        endpoint = &interface_desc->endpoint[i].desc;

        if (usb_endpoint_xfer_int(endpoint) && usb_endpoint_dir_in(endpoint)) {
            usb_fill_int_urb(controller->interruptURB, usbDev, usb_rcvintpipe(usbDev, endpoint->bEndpointAddress),
                            controller->buff, 64,
                            controller_irq_callback,
                            controller,
                            endpoint->bInterval);

            break;
        }
    }
    int urb_submit_result = usb_submit_urb(controller->interruptURB, GFP_KERNEL);
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

static void controller_disconnect(struct usb_interface *usbInterface) {
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

static void controller_irq_callback(struct urb *urb) {
    struct xboxController *controller = urb->context;
    unsigned char *buff = controller->buff;
    int status = urb->status;


    if (status) {
        if (status == -ENOENT || status == -ECONNRESET || status == -ESHUTDOWN) {
            return;  // URB was killed, don't resubmit
        }
        printk(KERN_ERR "URB error: %d\n", status);
        goto resubmit;
    }


    input_report_key(controller->inputDev, BTN_A,      buff[2] & 0x01);
    input_report_key(controller->inputDev, BTN_B,      buff[2] & 0x02);
    input_report_key(controller->inputDev, BTN_X,      buff[2] & 0x08);
    input_report_key(controller->inputDev, BTN_Y,      buff[2] & 0x10);
    input_report_key(controller->inputDev, BTN_TL,     buff[2] & 0x40);
    input_report_key(controller->inputDev, BTN_TR,     buff[2] & 0x80);
    input_report_key(controller->inputDev, BTN_SELECT, buff[2] & 0x04);
    input_report_key(controller->inputDev, BTN_START,  buff[2] & 0x20);


    input_report_abs(controller->inputDev, ABS_Z,  buff[4]);
    input_report_abs(controller->inputDev, ABS_RZ, buff[5]);


    input_report_abs(controller->inputDev, ABS_X,
                     (s16)(buff[6] | (buff[7] << 8)));
    input_report_abs(controller->inputDev, ABS_Y,
                     -(s16)(buff[8] | (buff[9] << 8)));  // invert Y axis


    input_report_abs(controller->inputDev, ABS_RX,
                     (s16)(buff[10] | (buff[11] << 8)));
    input_report_abs(controller->inputDev, ABS_RY,
                     -(s16)(buff[12] | (buff[13] << 8)));  // invert Y axis

    // Sync all events to input
    input_sync(controller->inputDev);

resubmit:
    // Resubmit URB to keep receiving reports
    status = usb_submit_urb(urb, GFP_ATOMIC);  // GFP_ATOMIC since we're in interrupt context
    if (status) {
        printk(KERN_ERR "Failed to resubmit URB: %d\n", status);
    }
}

module_init(gamepadDriver_init);
module_exit(gamepadDriver_exit);