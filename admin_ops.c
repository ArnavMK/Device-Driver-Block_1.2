#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

/* --- MOCK SECTION: Delete this when gamepad.h is ready --- */
struct gamepad_stats {
    unsigned long buttons_pressed;
    unsigned long packets_received;
    int is_connected;
};

// Create a local "fake" version of the stats to test with
struct gamepad_stats mock_global_stats = { .buttons_pressed = 42, .packets_received = 100, .is_connected = 1 };
#define GAMEPAD_IOCTL_RESET_STATS 101
/* --- END MOCK SECTION --- */



static int gamepad_proc_show(struct seq_file *m, void *v) {
    seq_printf(m, "=== Xbox Driver Admin Dashboard ===\n");
    seq_printf(m, "Connection: %s\n", mock_global_stats.is_connected ? "OK" : "DISCONNECTED");
    seq_printf(m, "Button Hits: %lu\n", mock_global_stats.buttons_pressed);
    seq_printf(m, "Data Packets: %lu\n", mock_global_stats.packets_received);
    return 0;
}

static int gamepad_proc_open(struct inode *inode, struct file *file) {
    return single_open(file, gamepad_proc_show, NULL);
}

// These are the standard "hooks" for a /proc file
static const struct proc_ops admin_fops = {
    .proc_open    = gamepad_proc_open,
    .proc_read    = seq_read,
    .proc_lseek   = seq_lseek,
    .proc_release = single_release,
};

int admin_init(void) {
    // This creates /proc/gamepad_stats
    proc_create("gamepad_stats", 0444, NULL, &admin_fops);
    printk(KERN_INFO "Admin Ops: Interface Loaded\n");
    return 0;
}

void admin_exit(void) {
    remove_proc_entry("gamepad_stats", NULL);
    printk(KERN_INFO "Admin Ops: Interface Removed\n");
}