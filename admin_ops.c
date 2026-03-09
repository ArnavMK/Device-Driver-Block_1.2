#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/jiffies.h>
#include <linux/smp.h>

#include "gamepad_driver.h"

static unsigned long driver_start_jiffies;

static int gamepad_proc_show(struct seq_file *m, void *v)
{
    struct file *dev_file;
    struct gamepad_stats stats;

    dev_file = filp_open("/dev/gamepad", O_RDONLY, 0);
    if (IS_ERR(dev_file)) {
        seq_printf(m, "Error: Could not open /dev/gamepad (is the driver loaded?)\n");
        return 0;
    }

    if (!dev_file->f_op || !dev_file->f_op->unlocked_ioctl) {
        seq_printf(m, "Error: /dev/gamepad does not support ioctl\n");
        filp_close(dev_file, NULL);
        return 0;
    }

    dev_file->f_op->unlocked_ioctl(dev_file, GAMEPAD_GET_STATS,
                                   (unsigned long)&stats);
    filp_close(dev_file, NULL);

    unsigned long uptime_sec = (jiffies - driver_start_jiffies) / HZ;

    seq_printf(m, "=== Xbox Driver Admin Dashboard ===\n");
    seq_printf(m, "Status:            %s\n", stats.is_halted    ? "LOCKED (E-STOP)" : "OPERATIONAL");
    seq_printf(m, "Connection:        %s\n", stats.is_connected ? "OK"              : "DISCONNECTED");
    seq_printf(m, "-----------------------------------\n");
    seq_printf(m, "Driver Uptime:     %lu seconds\n", uptime_sec);
    seq_printf(m, "Total Button Hits: %lu\n", stats.buttons_pressed);
    seq_printf(m, "Data Packets:      %lu\n", stats.packets_received);
    seq_printf(m, "-----------------------------------\n");
    seq_printf(m, "Commands (echo N > /proc/gamepad_stats):\n");
    seq_printf(m, "  1 = Reset stats\n");
    seq_printf(m, "  9 = Emergency stop\n");

    return 0;
}

static ssize_t gamepad_proc_write(struct file *file,
                                  const char __user *ubuf,
                                  size_t count, loff_t *ppos)
{
    char buf[16];
    size_t len = min(count, sizeof(buf) - 1);
    struct file *dev_file;

    if (copy_from_user(buf, ubuf, len))
        return -EFAULT;

    buf[len] = '\0';

    dev_file = filp_open("/dev/gamepad", O_RDWR, 0);
    if (IS_ERR(dev_file)) {
        pr_err("gamepadAdmin: Could not open /dev/gamepad\n");
        return -ENODEV;
    }

    if (buf[0] == '1') {
        dev_file->f_op->unlocked_ioctl(dev_file, GAMEPAD_RESET, 0);
        pr_info("gamepadAdmin: Stats RESET via /proc\n");

    } else if (buf[0] == '9') {
        dev_file->f_op->unlocked_ioctl(dev_file, GAMEPAD_ESTOP, 0);
        pr_crit("gamepadAdmin: EMERGENCY STOP via /proc\n");

    } else {
        pr_warn("gamepadAdmin: Unknown command '%c'\n", buf[0]);
    }

    filp_close(dev_file, NULL);
    return count;
}

static int gamepad_proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, gamepad_proc_show, NULL);
}

static const struct proc_ops admin_proc_ops = {
    .proc_open    = gamepad_proc_open,
    .proc_read    = seq_read,
    .proc_write   = gamepad_proc_write,
    .proc_lseek   = seq_lseek,
    .proc_release = single_release,
};

int admin_init(void)
{
    driver_start_jiffies = jiffies;

    if (!proc_create("gamepad_stats", 0666, NULL, &admin_proc_ops)) {
        pr_err("gamepadAdmin: Failed to create /proc/gamepad_stats\n");
        return -ENOMEM;
    }

    pr_info("gamepadAdmin: /proc/gamepad_stats created\n");
    return 0;
}
EXPORT_SYMBOL(admin_init);

void admin_exit(void)
{
    remove_proc_entry("gamepad_stats", NULL);
    pr_info("gamepadAdmin: /proc/gamepad_stats removed\n");
}
EXPORT_SYMBOL(admin_exit);