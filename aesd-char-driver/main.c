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
#include <linux/slab.h>     // supposedly for kmalloc(), krealloc() (we also have krealloc() in kernel space)
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
    .write = aesd_write,
    .unlocked_ioctl = NULL,
    .open = aesd_open,
    .release = aesd_release,
};

char* temporary_command_buffer = NULL;
size_t current_temporary_buffer_size  = 0;

int aesd_open(struct inode *inode, struct file *filp)
{
    struct aesd_dev* dev;
     /**
     * TODO: handle open
     */
    PDEBUG("aesd_open");
    dev = container_of(inode->i_cdev, struct aesd_dev, cdev);
    filp->private_data = dev;
    PDEBUG("we retrieved the pointer to our aesd_device struct: %p, pointing to an area of %ld bytes", dev, sizeof(*dev));

    /* no idea if I really need to do this, but scull driver example dies, so...*/
    /* trim device length to 0 if opened as write only */
    //if ((filp->f_flags & O_ACCMODE) == O_WRONLY) {
    //    if (mutex_lock_interruptible(&dev->lock)) {
    //        return -ERESTARTSYS;
    //    }
    //    mutex_unlock(&dev->lock);
    //}
    return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
    PDEBUG("aesd_release");
    /**
     * TODO: handle release
     */
    return 0;
}

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

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
    //ssize_t retval = 0;
    char* buffer = NULL;
    struct aesd_buffer_entry* buffentry = NULL;
    struct aesd_dev* dev = filp->private_data;
    size_t idx = 0;
    size_t off = 0;

    PDEBUG("write %zu bytes with file offset %lld, but will ignore the offset", count, *f_pos);
    /**
     * TODO: handle write
     */

    /* 1. Let's check if incoming data contains a command terminator at some point... */
    while (idx + off < count) {
        if (buf[idx + off] == '\n') {
            PDEBUG("aesd_write: at least one command terminator found in this input chunk");
            /* At least one command terminator was found at position idx, so let's copy over that specific chunk into temporary buffer, add it to the circular buffer */
            /* and then resize the buffer to hold the remaining of the input (and then loop again just in case multiple command in same write)*/
            /* 1. Now let's use krealloc to allocate/reallocate that buffer to be able to also hold the current chunk of bytes being written */
            PDEBUG("Prior to reallocation, temporary_command_buffer was at %p with length %ld", temporary_command_buffer, current_temporary_buffer_size);
            buffer = krealloc(temporary_command_buffer, current_temporary_buffer_size + idx + 1, GFP_KERNEL);
            if (buffer) {
                temporary_command_buffer = buffer;
                PDEBUG("aesd_write: (re)allocated %ld bytes at %p", current_temporary_buffer_size + idx + 1, temporary_command_buffer);

                /* 2. If everything went fine with the allocation, let's copy the incoming buffer into our kernel space buffer*/
                memcpy(temporary_command_buffer + current_temporary_buffer_size, buf + off, idx + 1);
                PDEBUG("Copied %ld bytes from position %ld of input buffer to position %ld of internal buffer", idx + 1, off, current_temporary_buffer_size);
                current_temporary_buffer_size += idx + 1;

                /* 3. Now push the complete contents of the temporary_command_buffer into a new position of the circular buffer */
                /* TODO */
                buffentry = kmalloc(sizeof(struct aesd_buffer_entry), GFP_KERNEL);
                if (buffentry == NULL) {
                    printk(KERN_WARNING "Failed to allocate memory needed for the circular buffer entry to be added");
                    PDEBUG("Freeing temporary buffer and resetting allocation variables (within error handling branch)");
                    kfree(temporary_command_buffer);
                    temporary_command_buffer = NULL;
                    current_temporary_buffer_size = 0;
                    return -ENOMEM;
                } else {
                    /* Populate the allocated circular buffer entry */
                    buffentry->buffptr = temporary_command_buffer;
                    buffentry->size = current_temporary_buffer_size;
                    /* need to use filp to retrieve a pointer to the circular buffer */
                    aesd_circular_buffer_add_entry(dev->data, buffentry);
                    temporary_command_buffer = NULL;
                    current_temporary_buffer_size = 0;
                    /* now advance idx and off to keep looking for command terminators in the input buffer*/
                    off = idx + 1;
                    idx = 0;
                }
            }
            else {
                printk(KERN_WARNING "Could not allocate/extend buffer to hold incoming command");
                return -ENOMEM;
            }
        } else {
            PDEBUG("aesd_write: increasing idx value, was %ld", idx);
            idx++;
        }
    }
 

    if (idx && idx + off == count) {
        /* Got to the end of the buf and found not terminator, reallocate temporary buffer, copy over contents and exit */
        PDEBUG("aesd_write: no command terminator found in this input chunk");
        PDEBUG("aesd_write: prior to allocation, temporary_command_buffer was at %p with length %ld", temporary_command_buffer, current_temporary_buffer_size);
        /* 1. Now let's use krealloc to allocate/reallocate that buffer to be able to also hold the current chunk of bytes being written */
        buffer = krealloc(temporary_command_buffer, idx + current_temporary_buffer_size, GFP_KERNEL);
        if (buffer) {
            temporary_command_buffer = buffer;
            PDEBUG("aesd_write: (re)allocated %ld bytes at %p", idx + current_temporary_buffer_size, temporary_command_buffer);
            /* 2. If everything went fine with the allocation, let's copy the incoming buffer into our kernel space buffer*/
            memcpy(temporary_command_buffer + current_temporary_buffer_size, buf + off, idx);
            PDEBUG("Copied %ld bytes from position %ld of input buffer to position %ld of internal buffer", idx, off, current_temporary_buffer_size);
            current_temporary_buffer_size += idx;
        }
        else {
            printk(KERN_WARNING "Could not allocate/extend buffer to hold incoming command");
            return -ENOMEM;
        }
    }

    return count;
}



static int __init aesd_init_module(void)
{
    dev_t dev = 0;
    int result;
    
    PDEBUG("aesd_init_module: major=%d (module parameter provided)", aesd_char_major);

    temporary_command_buffer = NULL;
    current_temporary_buffer_size = 0;

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
    PDEBUG("aesd_init_module: allocated memory to hold the aesd_device structure at address %p (%ld bytes)", aesd_device, sizeof(struct aesd_dev));
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
    struct aesd_buffer_entry* entry = NULL;
    uint8_t index;
    dev_t devno = MKDEV(aesd_char_major, aesd_char_minor);

    PDEBUG("aesd_cleanup_module");

    cdev_del(&aesd_device->cdev);

    /**
     * TODO: cleanup AESD specific poritions here as necessary
     */
    if (aesd_device && aesd_device->data) {
        /* release memory for all circular buffer nodes */
        AESD_CIRCULAR_BUFFER_FOREACH(entry, aesd_device->data, index) {
            PDEBUG("Release entry with %ld bytes string", entry->size);
            /* the number of entries in the circular buffer is fixed, but it might be that not all entries have been */
            /* initialized, hence checking that the pointer are not NULL before freeing */
            /* THIS PART IS NOT YET WORKING GOOD... IT ONLY FREES 7 ENTRIES AND THEN SEGMENTS */
            if (entry && entry->buffptr) {
                kfree(entry->buffptr);
            }
            if (entry) {
                kfree(entry);
            }
        }
        kfree(aesd_device->data);
        PDEBUG("aesd_cleanup_module: released memory for the circular buffer");
    }

    if (aesd_device) {
        kfree(aesd_device);
        PDEBUG("aesd_cleanup_module: release memory for the aesd_device structure");
    }

    if (temporary_command_buffer) {
        kfree(temporary_command_buffer);
        PDEBUG("aesd_cleanup_module: freeing temporary command buffer");
    
    }

    unregister_chrdev_region(devno, 1);
    PDEBUG("aesd_cleanup_module: module is now removed from kernel");
}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
