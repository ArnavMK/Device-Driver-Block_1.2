#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h> // For copy_to_user / copy_from_user
#include <linux/wait.h>    // For wait_event_interruptible
#include <linux/mutex.h>   // For Mutex locks
#include "circular_buffer.h"
#include "gamepad.h"

// ---------------------------------------------------------
// 1. Global Variables & Synchronization Tools
// ---------------------------------------------------------

static struct cbuffer my_cbuffer; 
DECLARE_WAIT_QUEUE_HEAD(wq);       // The waiting room for sleeping threads
DEFINE_MUTEX(cbuffer_lock);        // The lock to protect our buffer

// ---------------------------------------------------------
// 2. Open & Release Functions
// ---------------------------------------------------------

ssize_t dev_open(struct inode *inodep, struct file *filep) {
    printk(KERN_INFO "Gamepad: Device opened\n");
    // Initialize the buffer the first time the device is opened
    cbuffer_init(&my_cbuffer); 
    return 0; // 0 means success
}

int dev_release(struct inode *inodep, struct file *filep) {
    printk(KERN_INFO "Gamepad: Device closed\n");
    return 0;
}

// ---------------------------------------------------------
// 3. The READ Function (Game asks for input)
// ---------------------------------------------------------

ssize_t dev_read(struct file *filep, char *buffer, size_t len, loff_t *offset) {
    char data_to_send;

    // STEP 1: Block (sleep) if the buffer is empty.
    // The process will wait here until dev_write() wakes it up.
    if (wait_event_interruptible(wq, !cbuffer_is_empty(&my_cbuffer))) {
        return -ERESTARTSYS; // Safely handle if the user hits Ctrl+C while sleeping
    }

    // STEP 2: Grab the lock! We are about to touch the shared buffer.
    if (mutex_lock_interruptible(&cbuffer_lock)) {
        return -ERESTARTSYS; 
    }

    // Double check it's still not empty (another thread might have grabbed the data first!)
    if (cbuffer_is_empty(&my_cbuffer)) {
        mutex_unlock(&cbuffer_lock);
        return 0; 
    }

    // STEP 3: Pop the data using your helper
    data_to_send = cbuffer_pop(&my_cbuffer);

    // STEP 4: We are done with the buffer, unlock it for others.
    mutex_unlock(&cbuffer_lock);

    // STEP 5: Send the data safely to the User Space App
    if (copy_to_user(buffer, &data_to_send, 1) != 0) {
        return -EFAULT; // Failed to send data
    }
    
    // STEP 6: Wake up any writers that were stuck waiting for space
    wake_up_interruptible(&wq);

    return 1; // We successfully read 1 byte
}

// ---------------------------------------------------------
// 4. The WRITE Function (Hardware sends input)
// ---------------------------------------------------------

ssize_t dev_write(struct file *filep, const char *buffer, size_t len, loff_t *offset) {
    char data_received;

    // STEP 1: Get the data safely from User Space first
    if (copy_from_user(&data_received, buffer, 1) != 0) {
        return -EFAULT; 
    }

    // STEP 2: Block (sleep) if the buffer is completely full.
    if (wait_event_interruptible(wq, !cbuffer_is_full(&my_cbuffer))) {
        return -ERESTARTSYS;
    }

    // STEP 3: Grab the lock! We are about to modify the buffer.
    if (mutex_lock_interruptible(&cbuffer_lock)) {
        return -ERESTARTSYS;
    }

    // STEP 4: Push the data using your helper
    // We only push if space exists (protecting against race conditions)
    if (!cbuffer_is_full(&my_cbuffer)) {
        cbuffer_push(&my_cbuffer, data_received);
    }

    // STEP 5: Unlock the buffer
    mutex_unlock(&cbuffer_lock);

    // STEP 6: Wake up any readers (games) waiting for new button presses
    wake_up_interruptible(&wq);

    return 1; // We successfully wrote 1 byte
}
