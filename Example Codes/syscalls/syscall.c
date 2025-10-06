#include <linux/linkage.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/syscalls.h>

// NOTE: ** this is just a demonstration of a system call, you should directly add the 3 needed calls not these unnecessary call!

// sys call stubs
int (*STUB_init_call)(void) = NULL;
int (*STUB_two_nums)(int, int) = NULL;
EXPORT_SYMBOL(STUB_init_call);
EXPORT_SYMBOL(STUB_two_nums);

// sys call wrappers
SYSCALL_DEFINE0(init_call) {    // change this to your system call name
    printk(KERN_NOTICE "Inside SYSCALL_DEFINE0 block. %s", __FUNCTION__);
    if(STUB_init_call != NULL)
        return STUB_init_call();
    else
        return -ENOSYS;
}

SYSCALL_DEFINE2(two_nums, int, first_num, int, second_num) { // change this to your system call name and parameters.
    printk(KERN_NOTICE "Inside SYSCALL_DEFINE3 block. %s: Your int are %d, %d\n", __FUNCTION__, first_num, second_num);
    if(STUB_two_nums != NULL)
        return STUB_two_nums(first_num, second_num);
    else
        return -ENOSYS;
}