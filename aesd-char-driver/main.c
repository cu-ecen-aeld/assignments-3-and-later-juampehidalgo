/**
 * @file aesdchar.c
 * @brief Functions and data related to the AESD char driver implementation
 *
 * Based on the implementation of the "scull" device driver, found in
 * Linux Device Drivers example code.
 *
 * @author Dan Walkes
 * @date 2019-10-22
 * @copyright Copyright (c) 2019
 *
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>

#include <linux/kernel.h>   // supposedly for printk()
#include <linux/types.h>    // dev_t
#include <linux/errno.h>    // error codes
#include <linux/fs.h>       // file_operations
#include <linux/slab.h>     // supposedly for kmalloc()
#include <linux/fcntl.h>    // O_ACCMODE

#include "aesdchar.h"

static int aesd_char_major = AESD_CHAR_MAJOR;
static int aesd_char_minor = 0;
static int aesd_char_num_devs = AESD_CHAR_NUM_DEVS;

module_param(aesd_char_major, int, S_IRUGO);
module_param(aesd_char_minor, int, S_IRUGO);
module_param(aesd_char_num_devs, int, S_IRUGO);

MODULE_AUTHOR("Juan Pedro Hidalgo Cuevas"); /** TODO: fill in your name **/
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev* aesd_device;       // allocated and initialized in the init method
struct file_operations aesd_fops = {
    .owner = THIS_MODULE,
    .llseek = NULL,
    .read = NULL,
    .write = NULL,
    .unlocked_ioctl = NULL,
    .open = NULL,
    .release = NULL,
};
// int aesd_open(struct inode *inode, struct file *filp)
// {
//     struct aesd_dev* dev;

//     /**
//      * TODO: handle open
//      */

//     dev = container_of(inode->i_cdev, struct aesd_dev, cdev);
//     filp->private_data = dev;

//     /* no idea if I really need to do this, but scull driver example dies, so...*/
//     /* trim device length to 0 if opened as write only */
//     if ((filp->f_flags & O_ACCMODE) == O_WRONLY) {
//         if (mutex_lock_interruptible(&dev->lock)) {
//             return -ERESTARTSYS;
//         }
//         mutex_unlock(&dev->lock);
//     }
//     return 0;
// }

// int aesd_release(struct inode *inode, struct file *filp)
// {
//     PDEBUG("release");
//     /**
//      * TODO: handle release
//      */
//     return 0;
// }

// ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
//                 loff_t *f_pos)
// {
//     ssize_t retval = 0;
//     PDEBUG("read %zu bytes with offset %lld",count,*f_pos);
//     /**
//      * TODO: handle read
//      */
//     return retval;
// }

// ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
//                 loff_t *f_pos)
// {
//     ssize_t retval = -ENOMEM;
//     PDEBUG("write %zu bytes with offset %lld",count,*f_pos);
//     /**
//      * TODO: handle write
//      */
//     return retval;
// }
// struct file_operations aesd_fops = {
//     .owner =    THIS_MODULE,
//     .read =     aesd_read,
//     .write =    aesd_write,
//     .open =     aesd_open,
//     .release =  aesd_release,
// };

// static int aesd_setup_cdev(struct aesd_dev *dev)
// {
//     int err, devno = MKDEV(aesd_major, aesd_minor);

//     cdev_init(&dev->cdev, &aesd_fops);
//     dev->cdev.owner = THIS_MODULE;
//     dev->cdev.ops = &aesd_fops;
//     err = cdev_add (&dev->cdev, devno, 1, 1);
//     if (err) {
//         printk(KERN_ERR "Error %d adding aesd cdev", err);
//     }
//     return err;
// }



static int __init aesd_init_module(void)
{
    dev_t dev = 0;
    int result;
    
    PDEBUG("aesd_init_module: major=%d (module parameter provided)", aesd_char_major);

    /* this comes directly from scull code, that allows to pass major as parameter */
    if (aesd_char_major) {
        PDEBUG("aesd_init_module: on major provided as module parameter branch");
        dev = MKDEV(aesd_char_major, aesd_char_minor);
        result = register_chrdev_region(dev, 1, "aesdchar");
    } else {
        PDEBUG("aesd_init_module: on major dynamically allocated branch");
        result = alloc_chrdev_region(&dev, aesd_char_minor, 1, "aesdchar");
        aesd_char_major = MAJOR(dev);
    }

    if (result < 0) {
        printk(KERN_WARNING "Can't get major %d\n", aesd_char_major);
        return result;
    }

    PDEBUG("aesd_init_module: major in use is %d", aesd_char_major);

    /**
     * TODO: initialize the AESD specific portion of the device
     */
    /* We need to allocate memory for the circular buffer structure in the cdev struct */

    aesd_device = kmalloc(sizeof(struct aesd_dev), GFP_KERNEL);
    if (aesd_device == NULL) {
        printk(KERN_WARNING "Failed to allocate kernel memory to hold aesd_dev structure");
        result = -ENOMEM;
        goto failure;
    }
    PDEBUG("aesd_init_module: allocated memory to hold the aesd_device structure (%ld bytes)", sizeof(struct aesd_dev));
    memset(aesd_device, 0, sizeof(struct aesd_dev));
    mutex_init(&aesd_device->lock);
    aesd_device->data = kmalloc(sizeof(struct aesd_circular_buffer), GFP_KERNEL);
    if (aesd_device->data == NULL) {
        printk(KERN_WARNING "Failed to allocate kernel memory to hold circular buffer within aesd device structure");
        result = -ENOMEM;
        goto failure;
    }
    PDEBUG("aesd_init_module: allocated memory to hold the circular buffer structure (%ld bytes)", sizeof(struct aesd_circular_buffer));
    aesd_circular_buffer_init(aesd_device->data);

    cdev_init(&aesd_device->cdev, &aesd_fops);
    aesd_device->cdev.owner = THIS_MODULE;
    result = cdev_add(&aesd_device->cdev, MKDEV(aesd_char_major, 0), 1);

    if( result ) {
        printk(KERN_WARNING "Failed to initialize cdev structure");
        goto failure;
    }
    PDEBUG("aesd_init_module: module is now active (cdev has been added)");
    return result;

failure:
    aesd_cleanup_module();
    return result;
}

static void aesd_cleanup_module(void)
{
    dev_t devno = MKDEV(aesd_char_major, aesd_char_minor);

    PDEBUG("aesd_cleanup_module");

    cdev_del(&aesd_device->cdev);

    /**
     * TODO: cleanup AESD specific poritions here as necessary
     */
    if (aesd_device && aesd_device->data) {
        /* release memory for all circular buffer nodes */
        kfree(aesd_device->data);
        PDEBUG("aesd_cleanup_module: released memory for the circular buffer");
    }

    if (aesd_device) {
        kfree(aesd_device);
        PDEBUG("aesd_cleanup_module: release memory for the aesd_device structure");
    }

    unregister_chrdev_region(devno, 1);
    PDEBUG("aesd_cleanup_module: module is now removed from kernel");
}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
