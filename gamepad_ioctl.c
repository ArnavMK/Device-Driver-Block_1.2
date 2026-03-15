#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/fs.h>

#include "gamepad.h"

long gamepad_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    switch (cmd) {
        case GAMEPAD_GET_STATS:
    printk(KERN_INFO "ioctl GET_STATS: pkts=%lu btns=%lu\n",
           myDeviceStats.packets_received,
           myDeviceStats.buttons_pressed);
    if (file == NULL) {
        memcpy((struct gamepad_stats *)arg, &myDeviceStats,
               sizeof(struct gamepad_stats));
    } else {
        if (copy_to_user((struct gamepad_stats __user *)arg,
                         &myDeviceStats, sizeof(myDeviceStats)))
            return -EFAULT;
    }
    return 0;

        case GAMEPAD_RESET:
            myDeviceStats.buttons_pressed  = 0;
            myDeviceStats.packets_received = 0;
            myDeviceStats.is_halted        = 0;
            myDeviceStats.is_connected     = 1;
            pr_info("gamepad_ioctl: stats reset\n");
            return 0;

        case GAMEPAD_ESTOP:
            myDeviceStats.is_halted = 1;
            pr_crit("gamepad_ioctl: EMERGENCY STOP\n");
            return 0;

        default:
            return -EINVAL;
    }
}
EXPORT_SYMBOL(gamepad_ioctl);