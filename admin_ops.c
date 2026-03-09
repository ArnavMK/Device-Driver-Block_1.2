#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/jiffies.h>
#include <linux/smp.h>

static struct gamepad_stats *stats_ptr = NULL;
unsigned long driver_start_jiffies;


static int gamepad_proc_show(struct seq_file *m, void *v) {
    if (!stats_ptr) {
        seq_printf(m, "Error: No stats pointer registered.\n");
        return 0;
    }

    int cpu = raw_smp_processor_id();
    unsigned long uptime_seconds = (jiffies - driver_start_jiffies) / HZ;

    seq_printf(m, "=== Xbox Driver Admin Dashboard ===\n");
    seq_printf(m, "Status:            %s\n", stats_ptr->is_halted ? "LOCKED (E-STOP)" : "OPERATIONAL");
    seq_printf(m, "Connection:        %s\n", stats_ptr->is_connected ? "OK" : "DISCONNECTED");
    seq_printf(m, "-----------------------------------\n");
    seq_printf(m, "Active CPU Core:   %d\n", cpu);
    seq_printf(m, "Driver Uptime:     %lu seconds\n", uptime_seconds);
    seq_printf(m, "-----------------------------------\n");
    seq_printf(m, "Total Button Hits: %lu\n", stats_ptr->buttons_pressed);
    seq_printf(m, "Data Packets:      %lu\n", stats_ptr->packets_received);

    return 0;
}

static ssize_t gamepad_proc_write(struct file *file, const char __user *ubuf, size_t count, loff_t *ppos) {
    char buf[16];
    size_t len = min(count, sizeof(buf) - 1);

    if (!stats_ptr) return -ENODEV;

    if (copy_from_user(buf, ubuf, len))
        return -EFAULT;

    buf[len] = '\0';

    // Command '0': Manual Disconnect
    if (buf[0] == '0') {
        stats_ptr->is_connected = 0;
        pr_warn("Admin Ops: Manual DISCONNECT via /proc\n");
    }
    // Command '1': Reset & Reconnect
    else if (buf[0] == '1') {
        stats_ptr->buttons_pressed = 0;
        stats_ptr->packets_received = 0;
        stats_ptr->is_connected = 1;
        pr_info("Admin Ops: Stats RESET and RECONNECT via /proc\n");
    }
    // Command '9': Emergency Stop
    else if (buf[0] == '9') {
        stats_ptr->is_halted = 1;
        pr_crit("Admin Ops: EMERGENCY STOP triggered!\n");
    }

    return count;
}

static long gamepad_proc_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
    if (!stats_ptr) return -ENODEV;

    switch (cmd) {
        case 0x100: // Example Command for resetting buttons
            stats_ptr->buttons_pressed = 0;
            return 0;
        default:
            return -EINVAL;
    }
}

static int gamepad_proc_open(struct inode *inode, struct file *file) {
   return single_open(file, gamepad_proc_show, NULL);
}

static const struct proc_ops admin_fops = {
    .proc_open    = gamepad_proc_open,
    .proc_read    = seq_read,
	.proc_write   = gamepad_proc_write,
	.proc_ioctl   = gamepad_proc_ioctl,
    .proc_lseek   = seq_lseek,
    .proc_release = single_release,
};

int admin_init(struct gamepad_stats *stats) {
    if (!stats) return -EINVAL;

    stats_ptr = stats; // Hook into the driver's live data
    driver_start_jiffies = jiffies;

    if (!proc_create("gamepad_stats", 0666, NULL, &admin_fops)) {
        pr_err("Admin Ops: Failed to create /proc/gamepad_stats\n");
        return -ENOMEM;
    }

    pr_info("Admin Ops: Interface Loaded\n");
    return 0;
}
EXPORT_SYMBOL(admin_init);

void admin_exit(void) {
    remove_proc_entry("gamepad_stats", NULL);
    stats_ptr = NULL;
    pr_info("Admin Ops: Interface Removed\n");
}
EXPORT_SYMBOL(admin_exit);