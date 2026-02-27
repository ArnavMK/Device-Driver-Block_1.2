#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#define DEVICE_NAME "rickroll"

// This is the missing piece:
MODULE_LICENSE("GPL");
MODULE_AUTHOR("MTG");
MODULE_DESCRIPTION("A basic Kernel Module");


static int dev_open (struct inode* inodep,struct file*);
static int dev_release(struct inode* inodep,struct file*);
static ssize_t dev_read(struct file*, char*, size_t, loff_t*);
static ssize_t dev_write(struct file*,const char*, size_t,loff_t*);

static struct file_operations fops = {
	.open = dev_open,
	.read = dev_read,
	.write = dev_write,
	.release = dev_release,
};

static int major;

static int __init rickroll_init(void) {
	major = register_chrdev(0,DEVICE_NAME,&fops);
	printk("Major Number: %d",major);
	if(major<0){
		printk(KERN_ALERT "Module load failed\n");
		return major;
	}
    printk(KERN_INFO "Module has been loaded\n");
    return 0;
}

static void __exit rickroll_exit(void) {
	unregister_chrdev(major,DEVICE_NAME);
    printk(KERN_INFO "Module has been unloaded\n");
}

static int dev_open(struct inode* inodep, struct file* fileptr){
	printk(KERN_INFO "Module device opened\n");
	return 0;
}
static ssize_t dev_write(struct file* fileptr, const char* buffer, size_t len,loff_t* offset){
	printk(KERN_INFO "Sorry, module is read only\n");
	return -EFAULT;
}

static int dev_release(struct inode* inodep, struct file* fileptr){
	printk(KERN_INFO "Module device closed\n");
	return 0;
}

static ssize_t dev_read(struct file* fileptr, char* buffer, size_t len,loff_t *offset) {
	int errors =0;
	char *message =  "Kernel Module";
	int messagelen = strlen(message);
	if(*offset>=messagelen){
		return 0;
	}
	errors = copy_to_user(buffer,message,messagelen);
	*offset += messagelen;
	return errors == 0 ? messagelen : -EFAULT;
}
module_init(rickroll_init);
module_exit(rickroll_exit);
