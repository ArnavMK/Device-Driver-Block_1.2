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
    struct usb_device *udev;           
    struct input_dev *input;           
    unsigned char *buff;     
    struct urb *irq_urb;               
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

static int controller_probe(struct usb_interface *intf, const struct usb_device_id *id) {
    struct usb_device *udev = interface_to_usbdev(intf);
    struct xboxController *controller;
    int error;
    controller = kzalloc(sizeof(struct xboxController), GFP_KERNEL);
    if (!controller) {
        printk(KERN_ERR "Could not allocate memory for controller\n");
        return -ENOMEM;
    }
    
    controller->udev = udev;
    controller->input = input_allocate_device();
    if (!controller->input) {
        printk(KERN_ERR "Could not allocate input device\n");
        kfree(controller);
        return -ENOMEM;
    }
    controller->input->name = "Xbox Controller";
    controller->input->phys = "usb/input0";

    input_set_capability(controller->input, EV_KEY, BTN_A);
    input_set_capability(controller->input, EV_KEY, BTN_B);
    input_set_capability(controller->input, EV_KEY, BTN_X);
    input_set_capability(controller->input, EV_KEY, BTN_Y);
    input_set_capability(controller->input, EV_KEY, BTN_TL);
    input_set_capability(controller->input, EV_KEY, BTN_TR);
    input_set_capability(controller->input, EV_KEY, BTN_SELECT);
    input_set_capability(controller->input, EV_KEY, BTN_START);
    
    input_set_abs_params(controller->input, ABS_X,  -32768, 32767, 16, 128);
    input_set_abs_params(controller->input, ABS_Y,  -32768, 32767, 16, 128);

    input_set_abs_params(controller->input, ABS_Z,  0, 255, 0, 0);
    input_set_abs_params(controller->input, ABS_RZ, 0, 255, 0, 0);


    input_set_abs_params(controller->input, ABS_RX, -32768, 32767, 16, 128);
    input_set_abs_params(controller->input, ABS_RY, -32768, 32767, 16, 128);

    usb_set_intfdata(intf, controller); 
    error = input_register_device(controller->input);
    if (error) {
        printk(KERN_ERR "Could not register input device\n");
        input_free_device(controller->input);
        kfree(controller);
        return error;
    }

    iface_desc = intf->cur_altsetting;
    for (i = 0; i < iface_desc->desc.bNumEndpoints; i++) {
    endpoint = &iface_desc->endpoint[i].desc;

    if (usb_endpoint_xfer_int(endpoint) && usb_endpoint_dir_in(endpoint)) {
        usb_fill_int_urb(controller->irq_urb, udev, usb_rcvintpipe(udev, endpoint->bEndpointAddress), 
                         controller->buff, 64,
                         controller_irq_callback, 
                         controller, 
                         endpoint->bInterval);
        
        break; 
    }
}
    printk(KERN_INFO "Controller Connected.\n");
    return 0;
}

static void controller_disconnect(struct usb_interface *intf) {
    struct xboxController *controller = usb_get_intfdata(intf);
    input_unregister_device(controller->input);
    usb_set_intfdata(intf, NULL);
    kfree(controller);
    printk(KERN_INFO "Controller Disconnected.\n");
}

module_init(gamepadDriver_init);
module_exit(gamepadDriver_exit);