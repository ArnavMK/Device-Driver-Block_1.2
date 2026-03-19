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

extern struct file_operations fops;

static const struct usb_device_id controllerArr[] = {
    { USB_DEVICE(XBOX_VENDOR_ID, XBOX_PRODUCT_ID) },
    {} 
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


//Load Module
static int __init gamepadDriver_init(void)
{
    int result;

	memset(&myDeviceStats, 0, sizeof(struct gamepad_stats));
    myDeviceBuffer.read_pos  = 0;
    myDeviceBuffer.write_pos = 0;
    myDeviceBuffer.count     = 0;
    spin_lock_init(&myDeviceBuffer.lock);

    major = register_chrdev(0, DEVICE_NAME, &fops);
    if (major < 0) {
        printk(KERN_ALERT "Failed to register controller\n");
        return major;
    }

    admin_init();

    result = usb_register(&controller_driver);
    if (result) {
        unregister_chrdev(major, DEVICE_NAME);
        printk(KERN_ALERT "Failed to register USB driver\n");
        return result;
    }


    printk(KERN_INFO "Controller loaded with major number %d\n", major);
    return 0;
}
//Unload Module
static void __exit gamepadDriver_exit(void)
{
    admin_exit(); // Clean up the admin module (proc interface)
    usb_deregister(&controller_driver);
    unregister_chrdev(major, DEVICE_NAME);
    printk(KERN_INFO "Controller unloaded\n");
}
//Controller Plugged In
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
    u8 *gip_buf;                          
    static const u8 gip_pkt[] = { 0x05, 0x20, 0x00, 0x01, 0x00 }; 
    
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

    usb_set_intfdata(usbInterface, controller);

    //Buttons
    input_set_capability(controller->inputDev, EV_KEY, BTN_A);
    input_set_capability(controller->inputDev, EV_KEY, BTN_B);
    input_set_capability(controller->inputDev, EV_KEY, BTN_X);
    input_set_capability(controller->inputDev, EV_KEY, BTN_Y);
    input_set_capability(controller->inputDev, EV_KEY, BTN_TL);
    input_set_capability(controller->inputDev, EV_KEY, BTN_TR);
    input_set_capability(controller->inputDev, EV_KEY, BTN_SELECT);
    input_set_capability(controller->inputDev, EV_KEY, BTN_START);

    // D-Pad
    input_set_capability(controller->inputDev, EV_KEY, BTN_DPAD_UP);
    input_set_capability(controller->inputDev, EV_KEY, BTN_DPAD_DOWN);
    input_set_capability(controller->inputDev, EV_KEY, BTN_DPAD_LEFT);
    input_set_capability(controller->inputDev, EV_KEY, BTN_DPAD_RIGHT);

    // Left Joystick
    input_set_abs_params(controller->inputDev, ABS_X,  -32768, 32767, 16, 128);
    input_set_abs_params(controller->inputDev, ABS_Y,  -32768, 32767, 16, 128);

    // Right Joystick
    input_set_abs_params(controller->inputDev, ABS_RX, -32768, 32767, 16, 128);
    input_set_abs_params(controller->inputDev, ABS_RY, -32768, 32767, 16, 128);

    //Register input device
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
    //find place were to receive interrupt data
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
    //Urb start to receive data from controller
    urb_submit_result = usb_submit_urb(controller->interruptURB, GFP_KERNEL);
    if (urb_submit_result) {
        printk(KERN_ERR "Could not submit URB for controller\n");
        input_unregister_device(controller->inputDev);
        kfree(controller->buff);
        usb_free_urb(controller->interruptURB);
        kfree(controller);
        return urb_submit_result;
    }

    //Send GIP packet and wake controller
    gip_buf = kmemdup(gip_pkt, sizeof(gip_pkt), GFP_KERNEL);
    if (gip_buf) {
        usb_interrupt_msg(usbDev,
            usb_sndintpipe(usbDev, 0x02),
            gip_buf, sizeof(gip_pkt),
            NULL, 1000);
        kfree(gip_buf);
    }
    spin_lock(&myDeviceBuffer.lock);
    myDeviceStats.is_connected = 1;
    spin_unlock(&myDeviceBuffer.lock);
    printk(KERN_INFO "Controller Connected.\n");
    return 0;
}

//Controller unplugged
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
    spin_lock(&myDeviceBuffer.lock);
    myDeviceStats.is_connected = 0;
    spin_unlock(&myDeviceBuffer.lock);
    printk(KERN_INFO "Controller Disconnected.\n");
}

void controller_irq_callback(struct urb *urb)
{
    struct xboxController *controller = urb->context;
    unsigned char *buff = controller->buff;
    int status = urb->status;
    unsigned char clicks_b4;
    unsigned char clicks_b5;
    unsigned char btn_id = 0;

    if (status) {
        if (status == -ENOENT || status == -ECONNRESET || status == -ESHUTDOWN)
            return;
        printk(KERN_ERR "gamepadDriver: URB error: %d\n", status);
        goto resubmit;
    }

    if (buff[0] != 0x20)
        goto resubmit;

    clicks_b4 = buff[4] & ~controller->prev_b4;
    clicks_b5 = buff[5] & ~controller->prev_b5;

    //Buttons
    spin_lock(&myDeviceBuffer.lock);
    if (clicks_b4 & 0x04) myDeviceStats.individual_counts[4]++;  // Start
    if (clicks_b4 & 0x08) myDeviceStats.individual_counts[5]++;  // Select
    if (clicks_b4 & 0x10) myDeviceStats.individual_counts[8]++;  // A
    if (clicks_b4 & 0x20) myDeviceStats.individual_counts[9]++;  // B
    if (clicks_b4 & 0x40) myDeviceStats.individual_counts[10]++; // X
    if (clicks_b4 & 0x80) myDeviceStats.individual_counts[11]++; // Y

    // D-Pad, Lb, Rb
    if (clicks_b5 & 0x01) myDeviceStats.individual_counts[0]++;  // DPAD_UP
    if (clicks_b5 & 0x02) myDeviceStats.individual_counts[1]++;  // DPAD_DOWN
    if (clicks_b5 & 0x04) myDeviceStats.individual_counts[2]++;  // DPAD_LEFT
    if (clicks_b5 & 0x08) myDeviceStats.individual_counts[3]++;  // DPAD_RIGHT
    if (clicks_b5 & 0x10) myDeviceStats.individual_counts[6]++;  // LB
    if (clicks_b5 & 0x20) myDeviceStats.individual_counts[7]++;  // RB

    //Increase button press count and push to buffer
    if (clicks_b4 || clicks_b5) {
        myDeviceStats.buttons_pressed++;

        if (clicks_b4 & 0x10) btn_id = GAMEPAD_BTN_A;
        else if (clicks_b4 & 0x20) btn_id = GAMEPAD_BTN_B;
        else if (clicks_b4 & 0x40) btn_id = GAMEPAD_BTN_X;
        else if (clicks_b4 & 0x80) btn_id = GAMEPAD_BTN_Y;
        else if (clicks_b5 & 0x10) btn_id = GAMEPAD_BTN_LB;
        else if (clicks_b5 & 0x20) btn_id = GAMEPAD_BTN_RB;
        else if (clicks_b4 & 0x04) btn_id = GAMEPAD_BTN_START;
        else if (clicks_b4 & 0x08) btn_id = GAMEPAD_BTN_SELECT;
        else if (clicks_b5 & 0x01) btn_id = GAMEPAD_BTN_DPAD_UP;
        else if (clicks_b5 & 0x02) btn_id = GAMEPAD_BTN_DPAD_DOWN;
        else if (clicks_b5 & 0x04) btn_id = GAMEPAD_BTN_DPAD_LEFT;
        else if (clicks_b5 & 0x08) btn_id = GAMEPAD_BTN_DPAD_RIGHT;

        if (btn_id) {
            if (!gamepad_buffer_is_full(&myDeviceBuffer)){
                gamepad_buffer_push(&myDeviceBuffer, btn_id);
            }
        }
    }
    spin_unlock(&myDeviceBuffer.lock);
    if(btn_id) {
        wake_up_interruptible(&wq);
    }
    // Buttons 
    input_report_key(controller->inputDev, BTN_A, buff[4] & 0x10);
    input_report_key(controller->inputDev, BTN_B, buff[4] & 0x20);
    input_report_key(controller->inputDev, BTN_X, buff[4] & 0x40);
    input_report_key(controller->inputDev, BTN_Y, buff[4] & 0x80);
    input_report_key(controller->inputDev, BTN_START, buff[4] & 0x04);
    input_report_key(controller->inputDev, BTN_SELECT, buff[4] & 0x08);

    // Buttons and D-Pad 
    input_report_key(controller->inputDev, BTN_TL, buff[5] & 0x10);
    input_report_key(controller->inputDev, BTN_TR, buff[5] & 0x20);
    input_report_key(controller->inputDev, BTN_THUMBL, buff[5] & 0x40);
    input_report_key(controller->inputDev, BTN_THUMBR, buff[5] & 0x80);
    input_report_key(controller->inputDev, BTN_DPAD_UP, buff[5] & 0x01);
    input_report_key(controller->inputDev, BTN_DPAD_DOWN, buff[5] & 0x02);
    input_report_key(controller->inputDev, BTN_DPAD_LEFT, buff[5] & 0x04);
    input_report_key(controller->inputDev, BTN_DPAD_RIGHT, buff[5] & 0x08);


    // Joysticks
    input_report_abs(controller->inputDev, ABS_X,(__s16)(buff[10] | (buff[11] << 8)));
    input_report_abs(controller->inputDev, ABS_Y, -(__s16)(buff[12] | (buff[13] << 8)));
    input_report_abs(controller->inputDev, ABS_RX, (__s16)(buff[14] | (buff[15] << 8)));
    input_report_abs(controller->inputDev, ABS_RY, -(__s16)(buff[16] | (buff[17] << 8)));

    input_sync(controller->inputDev);
    spin_lock(&myDeviceBuffer.lock);
    myDeviceStats.packets_received++;
    myDeviceStats.is_connected = 1;
    spin_unlock(&myDeviceBuffer.lock);

    controller->prev_b4 = buff[4];
    controller->prev_b5 = buff[5];
//Resubmit so that keep getting data
resubmit:
    status = usb_submit_urb(urb, GFP_ATOMIC);
    if (status)
        printk(KERN_ERR "gamepadDriver: Failed to resubmit URB: %d\n", status);
}

module_init(gamepadDriver_init);
module_exit(gamepadDriver_exit);