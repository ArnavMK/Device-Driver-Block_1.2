#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/jiffies.h>
#include <linux/smp.h>
unsigned long driver_start_jiffies;


static int gamepad_proc_show(struct seq_file *m, void *v) {
	int cpu = raw_smp_processor_id();
	unsigned long uptime_seconds = (jiffies - INITIAL_JIFFIES_VAR) / HZ;

    seq_printf(m, "=== Xbox Driver Admin Dashboard ===\n");
    seq_printf(m, "System Status:     %s\n", mock_global_stats.is_halted ? "LOCKED (E-STOP)" : "OPERATIONAL");
    seq_printf(m, "Connection:        %s\n", mock_global_stats.is_connected ? "OK" : "DISCONNECTED");
    seq_printf(m, "-----------------------------------\n");
    seq_printf(m, "Active CPU Core:   %d\n", cpu);
    seq_printf(m, "Driver Uptime:     %lu seconds\n", uptime_seconds);
    seq_printf(m, "Memory Footprint:  %zu bytes\n", sizeof(struct gamepad_stats));
    seq_printf(m, "-----------------------------------\n");
    seq_printf(m, "Button Hits:       %lu\n", mock_global_stats.buttons_pressed);
    seq_printf(m, "Data Packets:      %lu\n", mock_global_stats.packets_received);

	return 0;
}

static ssize_t gamepad_proc_write(struct file *file, const char __user *ubuf, size_t count, loff_t *ppos) {
    char buf[11];

    /* 1. Safety Check: Prevent buffer overflow in Kernel Space */
    if (count > 10)
        return -EINVAL;

    /* 2. The Airlock: Securely move data from User Space to Kernel Space */
    if (copy_from_user(buf, ubuf, count))
        return -EFAULT;

    /* 3. Null Terminate: Ensure the string is safe for C logic */
    buf[count] = '\0';

    // Command '0': Simulate Hardware Disconnect
    if (buf[0] == '0') {
        global_stats.is_connected = 0;
        printk(KERN_WARNING "Admin Ops: Manual DISCONNECT triggered via /proc\n");
    }
    // Command '1': Reset Stats AND Reconnect
    else if (buf[0] == '1') {
        global_stats.buttons_pressed = 0;
        global_stats.packets_received = 0;
        global_stats.is_connected = 1;
        printk(KERN_INFO "Admin Ops: Stats RESET and RECONNECT triggered via /proc\n");
    }
    // Command '9': Emergency Stop (Mock example)
    else if (buf[0] == '9') {
        printk(KERN_CRIT "Admin Ops: EMERGENCY STOP command received!\n");
    }

    /* 5. Return count: Crucial to tell the Kernel the write is finished */
    return count;
}

static int gamepad_proc_open(struct inode *inode, struct file *file) {
   return single_open(file, gamepad_proc_show, NULL);
}

static const struct proc_ops admin_fops = {
    .proc_open    = gamepad_proc_open,
    .proc_read    = seq_read,
	.proc_write   = gamepad_proc_write,
    .proc_lseek   = seq_lseek,
    .proc_release = single_release,
};

int admin_init(void) {
	driver_start_jiffies = jiffies;
    proc_create("gamepad_stats", 0664, NULL, &admin_fops);
    printk(KERN_INFO "Admin Ops: Interface Loaded\n");
    return 0;
}

void admin_exit(void) {
    remove_proc_entry("gamepad_stats", NULL);
    printk(KERN_INFO "Admin Ops: Interface Removed\n");
}