#include <linux/init.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/random.h>

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Adds random animals to a list and uses proc to query the stats");

#define ENTRY_NAME "animal_list"
#define ENTRY_SIZE 1000
#define PERMS 0644
#define PARENT NULL
static struct proc_ops fops;

// static const struct proc_ops procfile_fops = {
//     .proc_read = procfile_read,
//     .proc_write = procfile_write,
// };
static char *message;
static int read_p;

struct animals {
	int total_cnt;
	int total_length;
	int total_weight;
	struct list_head list;
};

// declare an object animals with the same name as the structure type.
struct animals animals;

#define ANIMAL_HAMSTER 0
#define ANIMAL_CAT 1
#define ANIMAL_DOG 2

#define NUM_ANIMAL_TYPES 3
#define MAX_ANIMALS 50

typedef struct animal {
	int id;
	int length;
	int weight;
	const char *name;
	struct list_head list;
} Animal;

/********************************************************************/

int add_animal(int type) {
	int length;
	int weight;
	char *name;
	Animal *a;

	if (animals.total_cnt >= MAX_ANIMALS)
		return 0;

	switch (type) {
		case ANIMAL_HAMSTER:
			length = 2;
			weight = 1;
			name = "hamster";
			break;
		case ANIMAL_CAT:
			length = 18;
			weight = 160;
			name = "cat";
			break;
		case ANIMAL_DOG:
			length = 28;
			weight = 1200;
			name = "dog";
			break;
		default:
			return -1;
	}

	a = kmalloc(sizeof(Animal) * 1, __GFP_RECLAIM);
	if (a == NULL)
		return -ENOMEM;

	a->id = type;
	a->length = length;
	a->weight = weight;
	a->name = name;

	//list_add(&a->list, &animals.list); /* insert at front of list */
	list_add_tail(&a->list, &animals.list); /* insert at back of list */

	animals.total_cnt += 1;
	animals.total_length += length;
	animals.total_weight += weight;

	return 0;
}

int print_animals(void) {
	int i;
	Animal *a;
	struct list_head *temp;

	char *buf = kmalloc(sizeof(char) * 100, __GFP_RECLAIM);
	if (buf == NULL) {
		printk(KERN_WARNING "print_animals");
		return -ENOMEM;
	}

	/* init message buffer */
	strcpy(message, "");

	/* headers, print to temporary then append to message buffer */
	sprintf(buf, "Total count is: %d\n", animals.total_cnt);
	strcat(message, buf);
	sprintf(buf, "Total length is: %d\n", animals.total_length);
	strcat(message, buf);
	sprintf(buf, "Total weight is: %d\n", animals.total_weight);
	strcat(message, buf);
	sprintf(buf, "Animals seen:\n");
	strcat(message, buf);

	/* print entries */
	i = 0;
	//list_for_each_prev(temp, &animals.list) { /* backwards */
	list_for_each(temp, &animals.list) { /* forwards*/
		a = list_entry(temp, Animal, list);

		/* newline after every 5 entries */
		if (i % 5 == 0 && i > 0)
			strcat(message, "\n");

		sprintf(buf, "%s ", a->name);
		strcat(message, buf);

		i++;
	}

	/* trailing newline to separate file from commands */
	strcat(message, "\n");

	kfree(buf);
	return 0;
}

void delete_animals(int type) {
	struct list_head move_list;
	struct list_head *temp;
	struct list_head *dummy;
	int i;
	Animal *a;

	INIT_LIST_HEAD(&move_list);

	/* move items to a temporary list to illustrate movement */
	//list_for_each_prev_safe(temp, dummy, &animals.list) { /* backwards */
	list_for_each_safe(temp, dummy, &animals.list) { /* forwards */
		a = list_entry(temp, Animal, list);

		if (a->id == type) {
			//list_move(temp, &move_list); /* move to front of list */
			list_move_tail(temp, &move_list); /* move to back of list */
		}

	}

	/* print stats of list to syslog, entry version just as example (not needed here) */
	i = 0;
	//list_for_each_entry_reverse(a, &move_list, list) { /* backwards */
	list_for_each_entry(a, &move_list, list) { /* forwards */
		/* can access a directly e.g. a->id */
		i++;
	}
	printk(KERN_NOTICE "animal type %d had %d entries\n", type, i);

	/* free up memory allocation of Animals */
	//list_for_each_prev_safe(temp, dummy, &move_list) { /* backwards */
	list_for_each_safe(temp, dummy, &move_list) { /* forwards */
		a = list_entry(temp, Animal, list);
		list_del(temp);	/* removes entry from list */
		kfree(a);
	}
}

/********************************************************************/

int animal_proc_open(struct inode *sp_inode, struct file *sp_file) {
	read_p = 1;
	message = kmalloc(sizeof(char) * ENTRY_SIZE, __GFP_RECLAIM | __GFP_IO | __GFP_FS);
	if (message == NULL) {
		printk(KERN_WARNING "animal_proc_open");
		return -ENOMEM;
	}

	add_animal(get_random_long() % NUM_ANIMAL_TYPES);
	return print_animals();
}

ssize_t animal_proc_read(struct file *sp_file, char __user *buf, size_t size, loff_t *offset) {
	int len = strlen(message);

	read_p = !read_p;
	if (read_p)
		return 0;

	copy_to_user(buf, message, len);
	return len;
}

int animal_proc_release(struct inode *sp_inode, struct file *sp_file) {
	kfree(message);
	return 0;
}

/********************************************************************/

static int animal_init(void) {
	fops.proc_open = animal_proc_open;
	fops.proc_read = animal_proc_read;
	fops.proc_release = animal_proc_release;

	if (!proc_create(ENTRY_NAME, PERMS, NULL, &fops)) {
		printk(KERN_WARNING "animal_init\n");
		remove_proc_entry(ENTRY_NAME, NULL);
		return -ENOMEM;
	}

	animals.total_cnt = 0;
	animals.total_length = 0;
	animals.total_weight = 0;
	INIT_LIST_HEAD(&animals.list);

	return 0;
}
module_init(animal_init);

static void animal_exit(void) {
	delete_animals(ANIMAL_HAMSTER);
	delete_animals(ANIMAL_CAT);
	delete_animals(ANIMAL_DOG);
	remove_proc_entry(ENTRY_NAME, NULL);
}
module_exit(animal_exit);
