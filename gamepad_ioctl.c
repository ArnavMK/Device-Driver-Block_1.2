#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/string.h>
#include "gamepad.h"

// handles stat requests and hardware resets
long gamepad_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    unsigned long flags;
    struct gamepad_stats temp_stats;
    switch (cmd) {
        case GAMEPAD_GET_STATS:
            spin_lock_irqsave(&myDeviceBuffer.lock, flags);
            memcpy(&temp_stats, &myDeviceStats, sizeof(struct gamepad_stats));
            spin_unlock_irqrestore(&myDeviceBuffer.lock, flags);
            if (file == NULL) {
                memcpy((struct gamepad_stats *)arg, &temp_stats, sizeof(struct gamepad_stats));
            } else {
                if (copy_to_user((struct gamepad_stats __user *)arg,
                                 &temp_stats, sizeof(temp_stats)))
                    return -EFAULT;
            }
            return 0;

        case GAMEPAD_RESET:
            spin_lock_irqsave(&myDeviceBuffer.lock, flags);
            myDeviceStats.buttons_pressed  = 0;
            myDeviceStats.packets_received = 0;
            myDeviceStats.is_halted        = 0;
            myDeviceStats.is_connected     = 1;
            memset(myDeviceStats.individual_counts, 0, sizeof(myDeviceStats.individual_counts));
            spin_unlock_irqrestore(&myDeviceBuffer.lock, flags);
            pr_info("gamepad_ioctl: stats reset\n");
            return 0;


        default:
            return -EINVAL;
    }
}
// make this visible to admin_ops.c file
EXPORT_SYMBOL(gamepad_ioctl);
