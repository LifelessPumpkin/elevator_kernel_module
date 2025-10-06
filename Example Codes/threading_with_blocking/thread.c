#include <linux/init.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/mutex.h>

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Thread example illustrating different blocking commands");

#define ENTRY_NAME "thread_example"
#define ENTRY_SIZE 20
#define PERMS 0644
#define PARENT NULL
static struct proc_ops fops;

static char *message;
static int read_p;

struct thread_parameter {
	int id;
	int cnt;
	struct task_struct *kthread;
};

struct thread_parameter thread1;

/******************************************************************************/

int thread_run(void *data) {
	struct thread_parameter *parm = data;

	while (!kthread_should_stop()) {
		ssleep(1);
		parm->cnt++;
	}

	return 0;
}

void thread_init_parameter(struct thread_parameter *parm) {
	static int id = 1;

	parm->id = id++;
	parm->cnt = 0;
	parm->kthread = kthread_run(thread_run, parm, "thread example %d", parm->id);
}

/******************************************************************************/

int thread_proc_open(struct inode *sp_inode, struct file *sp_file) {
	read_p = 1;

	message = kmalloc(sizeof(char) * ENTRY_SIZE, __GFP_RECLAIM | __GFP_IO | __GFP_FS);
	if (message == NULL) {
		printk(KERN_WARNING "hello_proc_open");
		return -ENOMEM;
	}

	sprintf(message, "Thread %d has blocked %d times\n", thread1.id, thread1.cnt);
	return 0;
}

ssize_t thread_proc_read(struct file *sp_file, char __user *buf, size_t size, loff_t *offset) {
	int len = strlen(message);

	read_p = !read_p;
	if (read_p)
		return 0;

	copy_to_user(buf, message, len);
	return len;
}

int thread_proc_release(struct inode *sp_inode, struct file *sp_file) {
	kfree(message);
	return 0;
}

/******************************************************************************/

static int thread_init(void) {
	fops.proc_open = thread_proc_open;
	fops.proc_read = thread_proc_read;
	fops.proc_release = thread_proc_release;

	if (!proc_create(ENTRY_NAME, PERMS, NULL, &fops)) {
		printk(KERN_WARNING "thread_init");
		remove_proc_entry(ENTRY_NAME, NULL);
		return -ENOMEM;
	}

	thread_init_parameter(&thread1);
	if (IS_ERR(thread1.kthread)) {
		printk(KERN_WARNING "error spawning thread");
		remove_proc_entry(ENTRY_NAME, NULL);
		return PTR_ERR(thread1.kthread);
	}

	return 0;
}
module_init(thread_init);

static void thread_exit(void) {
	kthread_stop(thread1.kthread);
	remove_proc_entry(ENTRY_NAME, NULL);
	printk(KERN_NOTICE "Removing /proc/%s\n", ENTRY_NAME);
}
module_exit(thread_exit);
