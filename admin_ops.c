#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/jiffies.h>

#include "gamepad.h"

static unsigned long driver_start_jiffies;
static const char *button_names[] = {
	"DPAD_UP", "DPAD_DOWN", "DPAD_LEFT", "DPAD_RIGHT",
	"START", "SELECT", "LB", "RB", "A", "B", "X", "Y"
};

// Makes dashboard with stats when /proc/gamepad_stats is read
static int gamepad_proc_show(struct seq_file *m, void *v)
{
    struct gamepad_stats stats;
	//Calculate uptime
    unsigned long uptime_sec = (jiffies - driver_start_jiffies) / HZ;
	int i;

	//Get current data from main driver
    gamepad_ioctl(NULL, GAMEPAD_GET_STATS, (unsigned long)&stats);

    seq_printf(m, "=== Xbox Driver Admin Dashboard ===\n");
    seq_printf(m, "Status:            %s\n", stats.is_halted    ? "LOCKED (E-STOP)" : "OPERATIONAL");
    seq_printf(m, "Connection:        %s\n", stats.is_connected ? "OK"              : "DISCONNECTED");
    seq_printf(m, "Driver Uptime:     %lu seconds\n", uptime_sec);
    seq_printf(m, "-----------------------------------\n");

    // Display individual click counts
    seq_printf(m, "BUTTON NAME          | CLICKS\n");
    seq_printf(m, "---------------------|-----------\n");

    for (i = 0; i < MAX_BUTTONS; i++) {
        seq_printf(m, "%-20s | %lu\n", button_names[i], stats.individual_counts[i]);
    }

    seq_printf(m, "-----------------------------------\n");
    seq_printf(m, "Total Unique Clicks: %lu\n", stats.buttons_pressed);
    seq_printf(m, "Raw USB Packets:     %lu\n", stats.packets_received);
    seq_printf(m, "-----------------------------------\n");
    seq_printf(m, "Commands (echo N > /proc/gamepad_stats):\n");
    seq_printf(m, "  1 = Reset stats\n");


    return 0;
}

// handles user input
static ssize_t gamepad_proc_write(struct file *file, const char __user *ubuf, size_t count, loff_t *ppos)
{
    char buf[16];
    size_t len = min(count, sizeof(buf) - 1);

    if (copy_from_user(buf, ubuf, len))
        return -EFAULT;

    buf[len] = '\0';

    if (buf[0] == '1') {
        gamepad_ioctl(NULL, GAMEPAD_RESET, 0);
        pr_info("gamepadAdmin: Stats reset via /proc\n");

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

// Creates the entry in the /proc filesystem
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

// Clean up the module by removing the entry from /proc when the driver is unloaded
void admin_exit(void)
{
    remove_proc_entry("gamepad_stats", NULL);
    pr_info("gamepadAdmin: /proc/gamepad_stats removed\n");
}
EXPORT_SYMBOL(admin_exit);
