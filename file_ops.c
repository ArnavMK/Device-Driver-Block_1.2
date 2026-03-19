#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h> 
#include <linux/wait.h>     
#include "gamepad.h"

// global variables & synchronization tools
int dev_open(struct inode *inodep, struct file *filep);
int dev_release(struct inode *inodep, struct file *filep);
ssize_t dev_read(struct file *filep, char *buffer, size_t len, loff_t *offset);
ssize_t dev_write(struct file *filep, const char *buffer, size_t len, loff_t *offset);

struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = dev_open,
    .release = dev_release,
    .read = dev_read,
    .write = dev_write,
    .unlocked_ioctl = gamepad_ioctl,
};

DECLARE_WAIT_QUEUE_HEAD(wq);

// open & release functions
int dev_open(struct inode *inodep, struct file *filep) {
    printk(KERN_INFO "Gamepad: Device opened\n");

    gamepad_buffer_init(&myDeviceBuffer); 
    return 0;
}

int dev_release(struct inode *inodep, struct file *filep) {
    printk(KERN_INFO "Gamepad: Device closed\n");
    return 0;
}

// read function
ssize_t dev_read(struct file *filep, char *buffer, size_t len, loff_t *offset) {
    char data_to_send;

    if (wait_event_interruptible(wq, !gamepad_buffer_is_empty(&myDeviceBuffer))) {
        return -ERESTARTSYS;
    }

    spin_lock_bh(&myDeviceBuffer.lock);

    if (gamepad_buffer_is_empty(&myDeviceBuffer)) {
        spin_unlock_bh(&myDeviceBuffer.lock);
        return 0; 
    }

    data_to_send = gamepad_buffer_pop(&myDeviceBuffer);

    spin_unlock_bh(&myDeviceBuffer.lock);

    if (copy_to_user(buffer, &data_to_send, 1) != 0) {
        return -EFAULT;
    }
    
    wake_up_interruptible(&wq);

    return 1;
}

// write function
ssize_t dev_write(struct file *filep, const char *buffer, size_t len, loff_t *offset) {
    char data_received;

    if (copy_from_user(&data_received, buffer, 1) != 0) {
        return -EFAULT; 
    }

    if (wait_event_interruptible(wq, !gamepad_buffer_is_full(&myDeviceBuffer))) {
        return -ERESTARTSYS;
    }

    spin_lock_bh(&myDeviceBuffer.lock);

    if (!gamepad_buffer_is_full(&myDeviceBuffer)) {
        gamepad_buffer_push(&myDeviceBuffer, data_received);
    }

    spin_unlock_bh(&myDeviceBuffer.lock);

    wake_up_interruptible(&wq);

    return 1;
}
