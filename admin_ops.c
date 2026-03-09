#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/jiffies.h>
#include <linux/smp.h>

#include "gamepad_driver.h"

static struct gamepad_stats *stats_ptr = NULL;
static unsigned long driver_start_jiffies;

static int gamepad_proc_show(struct seq_file *m, void *v)
{
    if (!stats_ptr) {
        seq_printf(m, "Error: No stats pointer registered.\n");
        return 0;
    }

    int cpu = raw_smp_processor_id();
    unsigned long uptime_sec = (jiffies - driver_start_jiffies) / HZ;

    seq_printf(m, "=== Xbox Driver Admin Dashboard ===\n");
    seq_printf(m, "Status:            %s\n",
               stats_ptr->is_halted    ? "LOCKED (E-STOP)" : "OPERATIONAL");
    seq_printf(m, "Connection:        %s\n",
               stats_ptr->is_connected ? "OK"              : "DISCONNECTED");
    seq_printf(m, "-----------------------------------\n");
    seq_printf(m, "Active CPU Core:   %d\n",   cpu);
    seq_printf(m, "Driver Uptime:     %lu seconds\n", uptime_sec);
    seq_printf(m, "-----------------------------------\n");
    seq_printf(m, "Total Button Hits: %lu\n",  stats_ptr->buttons_pressed);
    seq_printf(m, "Data Packets:      %lu\n",  stats_ptr->packets_received);
    seq_printf(m, "\n");
    seq_printf(m, "Commands (echo N > /proc/gamepad_stats):\n");
    seq_printf(m, "  0 = Manual disconnect\n");
    seq_printf(m, "  1 = Reset stats + reconnect\n");
    seq_printf(m, "  9 = Emergency stop\n");

    return 0;
}

static ssize_t gamepad_proc_write(struct file *file,
                                  const char __user *ubuf,
                                  size_t count, loff_t *ppos)
{
    char buf[16];
    size_t len = min(count, sizeof(buf) - 1);

    if (!stats_ptr) return -ENODEV;

    if (copy_from_user(buf, ubuf, len))
        return -EFAULT;

    buf[len] = '\0';

    if (buf[0] == '0') {
        stats_ptr->is_connected = 0;
        pr_warn("gamepadAdmin: Manual DISCONNECT via /proc\n");

    } else if (buf[0] == '1') {
        stats_ptr->buttons_pressed  = 0;
        stats_ptr->packets_received = 0;
        stats_ptr->is_connected     = 1;
        stats_ptr->is_halted        = 0;
        pr_info("gamepadAdmin: Stats RESET and RECONNECT via /proc\n");

    } else if (buf[0] == '9') {
        stats_ptr->is_halted = 1;
        pr_crit("gamepadAdmin: EMERGENCY STOP triggered!\n");

    } else {
        pr_warn("gamepadAdmin: Unknown command '%c'\n", buf[0]);
    }

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

int admin_init(struct gamepad_stats *stats)
{
    if (!stats) return -EINVAL;

    stats_ptr            = stats;
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
    stats_ptr = NULL;
    pr_info("gamepadAdmin: /proc/gamepad_stats removed\n");
}
EXPORT_SYMBOL(admin_exit);