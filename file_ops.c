#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h> 
#include <linux/wait.h>    
#include <linux/mutex.h>   
#include "gamepad.h"

// 1. Global Variables & Synchronization Tools

static int     dev_open(struct inode *inodep, struct file *filep);
static int     dev_release(struct inode *inodep, struct file *filep);
static ssize_t dev_read(struct file *filep, char *buffer, size_t len, loff_t *offset);
static ssize_t dev_write(struct file *filep, const char *buffer, size_t len, loff_t *offset);

struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = dev_open,
    .release = dev_release,
    .read = dev_read,
    .write = dev_write,
    .unlocked_ioctl = gamepad_ioctl,
};

extern wait_queue_head_t wq;   // The waiting room for sleeping threads

// 2. Open & Release Functions


int dev_open(struct inode *inodep, struct file *filep) {
    printk(KERN_INFO "Gamepad: Device opened\n");
    // Initialize the buffer the first time the device is opened
    gamepad_buffer_init(&myDeviceBuffer); 
    return 0; // 0 means success
}

int dev_release(struct inode *inodep, struct file *filep) {
    printk(KERN_INFO "Gamepad: Device closed\n");
    return 0;
}

// 3. The READ Function (Game asks for input)
ssize_t dev_read(struct file *filep, char *buffer, size_t len, loff_t *offset) {
    char data_to_send;

    // STEP 1: Block (sleep) if the buffer is empty.
    // The process will wait here until dev_write() wakes it up.
    if (wait_event_interruptible(wq, !gamepad_buffer_is_empty(&myDeviceBuffer))) {
        return -ERESTARTSYS; // Safely handle if the user hits Ctrl+C while sleeping
    }

    // STEP 2: Grab the lock! We are about to touch the shared buffer.
    if (spin_lock_interruptible(&myDeviceBuffer.lock)) {
        return -ERESTARTSYS; 
    }

    // Double check it's still not empty (another thread might have grabbed the data first!)
    if (gamepad_buffer_is_empty(&myDeviceBuffer)) {
        spin_unlock(&myDeviceBuffer.lock);
        return 0; 
    }

    // STEP 3: Pop the data using your helper
    data_to_send = gamepad_buffer_pop(&myDeviceBuffer);

    // STEP 4: We are done with the buffer, unlock it for others.
    spin_unlock(&myDeviceBuffer.lock);

    // STEP 5: Send the data safely to the User Space App
    if (copy_to_user(buffer, &data_to_send, 1) != 0) {
        return -EFAULT; // Failed to send data
    }
    
    // STEP 6: Wake up any writers that were stuck waiting for space
    wake_up_interruptible(&wq);

    return 1; // We successfully read 1 byte
}

// 4. The WRITE Function (Hardware sends input)
ssize_t dev_write(struct file *filep, const char *buffer, size_t len, loff_t *offset) {
    char data_received;

    // STEP 1: Get the data safely from User Space first
    if (copy_from_user(&data_received, buffer, 1) != 0) {
        return -EFAULT; 
    }

    // STEP 2: Block (sleep) if the buffer is completely full.
    if (wait_event_interruptible(wq, !gamepad_buffer_is_full(&myDeviceBuffer))) {
        return -ERESTARTSYS;
    }

    // STEP 3: Grab the lock! We are about to modify the buffer.
    if (spin_lock_bh(&myDeviceBuffer.lock)) {
        return -ERESTARTSYS;
    }

    // STEP 4: Push the data using your helper
    // We only push if space exists (protecting against race conditions)
    if (!gamepad_buffer_is_full(&myDeviceBuffer)) {
        gamepad_buffer_push(&myDeviceBuffer, data_received);
    }

    // STEP 5: Unlock the buffer
    spin_unlock_bh(&myDeviceBuffer.lock);

    // STEP 6: Wake up any readers (games) waiting for new button presses
    wake_up_interruptible(&wq);

    return 1; // We successfully wrote 1 byte
}
