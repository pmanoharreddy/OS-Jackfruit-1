#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/device.h>
#include <linux/slab.h>

#include "monitor_ioctl.h"

#define DEVICE_NAME "container_monitor"
#define CLASS_NAME  "monitor"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Student");
MODULE_DESCRIPTION("Simple Container Memory Monitor");

static int major;
static struct class*  monitor_class  = NULL;
static struct device* monitor_device = NULL;

struct container_info {
    pid_t pid;
    unsigned long soft;
    unsigned long hard;
    char id[32];
};

static struct container_info current;

/* ================= IOCTL ================= */

static long monitor_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    struct monitor_request req;

    if (copy_from_user(&req, (void __user *)arg, sizeof(req)))
        return -EFAULT;

    switch (cmd) {

    case MONITOR_REGISTER:
        current.pid = req.pid;
        current.soft = req.soft_limit_bytes;
        current.hard = req.hard_limit_bytes;
        strncpy(current.id, req.container_id, sizeof(current.id));

        printk(KERN_INFO "[monitor] Registered container %s (pid=%d)\n",
               current.id, current.pid);

        printk(KERN_INFO "[monitor] Soft=%lu Hard=%lu\n",
               current.soft, current.hard);

        break;

    case MONITOR_UNREGISTER:
        printk(KERN_INFO "[monitor] Unregistered container %s\n",
               req.container_id);
        break;

    default:
        return -EINVAL;
    }

    return 0;
}

/* ================= FILE OPS ================= */

static int dev_open(struct inode *inodep, struct file *filep) {
    return 0;
}

static int dev_release(struct inode *inodep, struct file *filep) {
    return 0;
}

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = dev_open,
    .release = dev_release,
    .unlocked_ioctl = monitor_ioctl,
};

/* ================= INIT ================= */

static int __init monitor_init(void)
{
    printk(KERN_INFO "[monitor] Initializing...\n");

    major = register_chrdev(0, DEVICE_NAME, &fops);

    monitor_class = class_create(THIS_MODULE, CLASS_NAME);
    monitor_device = device_create(monitor_class, NULL,
                                   MKDEV(major, 0), NULL,
                                   DEVICE_NAME);

    printk(KERN_INFO "[monitor] Device ready: /dev/%s\n", DEVICE_NAME);
    return 0;
}

/* ================= EXIT ================= */

static void __exit monitor_exit(void)
{
    device_destroy(monitor_class, MKDEV(major, 0));
    class_unregister(monitor_class);
    class_destroy(monitor_class);
    unregister_chrdev(major, DEVICE_NAME);

    printk(KERN_INFO "[monitor] Exiting...\n");
}

module_init(monitor_init);
module_exit(monitor_exit);
