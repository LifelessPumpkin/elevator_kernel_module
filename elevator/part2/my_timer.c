#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/ktime.h>
#include <linux/time64.h>
#include <linux/string.h>

#define ENTRY_NAME "timer"
#define PERMS 0666
#define PARENT NULL
#define BUF_LEN 100

static struct proc_dir_entry* proc_entry;
static struct timespec64 prev_timespec;
static char msg[BUF_LEN];
static bool prev_set=false;
// static int procfs_buf_len;

static ssize_t procfile_read(struct file* file, char* ubuf, size_t count, loff_t *ppos) {
    struct timespec64 my_timespec;
    struct timespec64 new_timespec;
    int len;

    // Check if we are at the end of the file
    if (*ppos > 0) {
        return 0;
    }

    // Get the current time since linux epoch
    ktime_get_real_ts64(&my_timespec);

    // Get current time and set prev_time_called
    ktime_get_ts64(&new_timespec);

    // Format the timestamp into a string
    

    // If it's not equal to null, then perorm the subtraction and update len
    if (prev_set) {
        long long int seconds = new_timespec.tv_sec - prev_timespec.tv_sec;
        long int nanoseconds = new_timespec.tv_nsec - prev_timespec.tv_nsec;

        if (nanoseconds < 0) {
            nanoseconds += 1000000000;
            seconds--;
        }

        len = snprintf(msg, BUF_LEN, "current time: %lld.%09ld\nelapsed time: %lld.%09ld\n", 
            my_timespec.tv_sec, my_timespec.tv_nsec,seconds,nanoseconds);
    }
    else len = snprintf(msg, BUF_LEN, "current time: %lld.%09ld\n", my_timespec.tv_sec, my_timespec.tv_nsec);
    // Ensure the formatted string doesn't exceed buffer capacity
    if (len >= BUF_LEN) {
        len = BUF_LEN - 1;
        msg[len] = '\0';
    }

    // Copy the formatted string to user space
    if (copy_to_user(ubuf, msg, len + 1)) return -EFAULT;

    // Update the file position and return the number of bytes copied
    *ppos = len + 1;

    ktime_get_ts64(&prev_timespec);
    prev_set = true;

    printk(KERN_INFO "proc_read: gave to user %s", msg);

    return len + 1;
}

// static ssize_t procfile_write(struct file* file, const char* ubuf, size_t count, loff_t* ppos) {
//     printk(KERN_INFO "proc_write\n");
//     if (count > BUF_LEN)
//         procfs_buf_len = BUF_LEN;
//     else
//         procfs_buf_len = count;
//     if (copy_from_user(msg, ubuf, procfs_buf_len)) {
//         printk(KERN_WARNING "Failed to copy data from user space\n");
//         return -EFAULT;
//     }
//     printk(KERN_INFO "got from user: %s\n", msg);
//     return procfs_buf_len;
// }

static const struct proc_ops procfile_fops = {
    .proc_read = procfile_read,
    // .proc_write = procfile_write,
};

static int __init init_timer(void) {
    proc_entry = proc_create(ENTRY_NAME,PERMS,PARENT, &procfile_fops);
    if (proc_entry == NULL) return -ENOMEM;
    return 0;
}

static void __exit cleanup_timer(void) {
    proc_remove(proc_entry);
    printk(KERN_INFO "/proc/%s removed\n", ENTRY_NAME);
}

module_init(init_timer);
module_exit(cleanup_timer);

MODULE_AUTHOR("Logan Harmon");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("A simple timer module to display linux epoch");
MODULE_VERSION("1.0");