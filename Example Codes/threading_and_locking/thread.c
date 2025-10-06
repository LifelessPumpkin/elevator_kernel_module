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
MODULE_DESCRIPTION("Thread example illustrating race conditions");

#define ENTRY_NAME "thread_example"
#define ENTRY_SIZE 10000
#define PERMS 0644
#define PARENT NULL
static struct proc_ops fops;

static char *message;
static int read_p;

#define CNT_SIZE 20
struct thread_parameter {
	int id;
	int cnt[CNT_SIZE];
	struct task_struct *kthread;
	struct mutex mutex;
};

struct thread_parameter thread1;

/******************************************************************************/

int thread_run(void *data) {
	int i;
	struct thread_parameter *parm = data;

	while (!kthread_should_stop()) {
		if (mutex_lock_interruptible(&parm->mutex) == 0) {
			for (i = 0; i < CNT_SIZE; i++)
				parm->cnt[i]++;
			mutex_unlock(&parm->mutex);
		}

	}

	return 0;
}

void thread_init_parameter(struct thread_parameter *parm) {
	static int id = 1;
	int i;

	parm->id = id++;
	for (i = 0; i < CNT_SIZE; i++)
		parm->cnt[i] = 0;
	mutex_init(&parm->mutex);
	parm->kthread = kthread_run(thread_run, parm, "thread example %d", parm->id);
}

/******************************************************************************/

int thread_proc_open(struct inode *sp_inode, struct file *sp_file) {
	int i;
	char *buf;
	read_p = 1;

	buf = kmalloc(sizeof(char) * ENTRY_SIZE, __GFP_RECLAIM | __GFP_IO | __GFP_FS);
	message = kmalloc(sizeof(char) * ENTRY_SIZE, __GFP_RECLAIM | __GFP_IO | __GFP_FS);
	if (message == NULL) {
		printk(KERN_WARNING "hello_proc_open");
		return -ENOMEM;
	}

	sprintf(message, "Thread %d has cycled this many times:\n", thread1.id);
	if (mutex_lock_interruptible(&thread1.mutex) == 0) {
		for (i = 0; i < CNT_SIZE; i++) {
			sprintf(buf, "%d\n", thread1.cnt[i]);
			strcat(message, buf);
		}
		mutex_unlock(&thread1.mutex);
	}
	else {
		for (i = 0; i < CNT_SIZE; i++) {
			sprintf(buf, "%d\n", -1);
			strcat(message, buf);
		}
	}

	strcat(message, "\n");

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
	mutex_destroy(&thread1.mutex);
	printk(KERN_NOTICE "Removing /proc/%s\n", ENTRY_NAME);
}
module_exit(thread_exit);
