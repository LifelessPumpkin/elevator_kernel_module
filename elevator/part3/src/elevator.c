#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/kthread.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/limits.h>

#define ENTRY_NAME "elevator"
#define PERMS 0666
#define PARENT NULL
#define BUF_LEN 100

extern int (*STUB_start_elevator)(void);
extern int (*STUB_issue_request)(int,int,int);
extern int (*STUB_stop_elevator)(void);

static struct proc_dir_entry* proc_entry;
static char msg[BUF_LEN];
static int max_weight = 50;

static struct pet
{
    // This is the head of the linked list
    struct list_head list;
    int pet_type; // 0 = chihuahua, 1 = pug, 2 = pughuahua, 3 = doxen
    int weight; // 3, 14, 10, 16
    int starting_floor;
    int destination_floor;
};
static struct floor 
{
    struct list_head pets_waiting;
    struct mutex lock;
    bool elevator_at_floor;
};
static struct elevator 
{
    int current_floor;
    int num_of_pets;
    bool active;
    struct mutex lock;
    struct task_struct* thread;    
    struct list_head pet_list;

};

static int start_elevator(void);                                                    
static int issue_request(int start_floor, int destination_floor, int type);
static int stop_elevator(void); 
static void cleanup_elevator_list(struct elevator* pet_elevator);
static void cleanup_floor_list(struct floor* flo);
static int move_elevator_thread(void *data);
static void add_pet_to_floor(int type, int start_floor, int dest_floor);
static void add_pet_to_elevator(struct elevator* pet_elevator, struct floor* flo);
static bool look_for_request(void);
static int get_closest_request(void);
static bool dispense_pets_from_elevator(struct elevator* ele);


static struct elevator* pet_elevator = NULL;
static struct floor* floor1 = NULL;
static struct floor* floor2 = NULL;
static struct floor* floor3 = NULL;
static struct floor* floor4 = NULL;
static struct floor* floor5 = NULL;

static int start_elevator(void) {

    // If elevator is already active return 1
    if (pet_elevator && pet_elevator->active) {
        printk(KERN_INFO "Elevator already active\n");
        return 1;
    }
    
    // Allocate mem for elevator
    pet_elevator = kmalloc(sizeof(*pet_elevator), GFP_KERNEL);

    // Allocate mem for floors
    floor1 = kmalloc(sizeof(*floor1), GFP_KERNEL);
    floor2 = kmalloc(sizeof(*floor2), GFP_KERNEL);
    floor3 = kmalloc(sizeof(*floor3), GFP_KERNEL);
    floor4 = kmalloc(sizeof(*floor4), GFP_KERNEL);
    floor5 = kmalloc(sizeof(*floor5), GFP_KERNEL);

    
    // If it can't allocate memory return -ENOMEM
    if (!pet_elevator || !floor1 || !floor2 || !floor3 || !floor4 || !floor5) {
        printk(KERN_INFO "Couldn't allocate memory to run the elevator\n");
        return -ENOMEM;
    }
    
    pet_elevator->active = true;
    pet_elevator->current_floor = 1;
    mutex_init(&pet_elevator->lock);
    mutex_init(&floor1->lock);
    mutex_init(&floor2->lock);
    mutex_init(&floor3->lock);
    mutex_init(&floor4->lock);
    mutex_init(&floor5->lock);

    INIT_LIST_HEAD(&pet_elevator->pet_list);
    INIT_LIST_HEAD(&floor1->pets_waiting);
    INIT_LIST_HEAD(&floor2->pets_waiting);
    INIT_LIST_HEAD(&floor3->pets_waiting);
    INIT_LIST_HEAD(&floor4->pets_waiting);
    INIT_LIST_HEAD(&floor5->pets_waiting);

    pet_elevator->thread = kthread_run(move_elevator_thread,pet_elevator,"elevator_thread");
    if (IS_ERR(pet_elevator->thread)) {
        printk(KERN_ERR "Failed to create the elevator thread\n");
        kfree(pet_elevator);
        pet_elevator = NULL;
        // Its not letting me use -ERRORNUM
        return -ENOMEM;
    }

    // Start is successful, return 0
    return 0;
}

static int stop_elevator(void) {
    if (!pet_elevator || !pet_elevator->active) return 1;

    mutex_lock(&pet_elevator->lock);
    pet_elevator->active = false;
    mutex_unlock(&pet_elevator->lock);

    kthread_stop(pet_elevator->thread);
    printk(KERN_INFO "Elevator successfully stopped\n");

    cleanup_elevator_list(pet_elevator);
    cleanup_floor_list(floor1);
    cleanup_floor_list(floor2);
    cleanup_floor_list(floor3);
    cleanup_floor_list(floor4);
    cleanup_floor_list(floor5);

    kfree(pet_elevator);
    kfree(floor1);
    kfree(floor2);
    kfree(floor3);
    kfree(floor4);
    kfree(floor5);

    pet_elevator = NULL;
    floor1 = NULL;
    floor2 = NULL;
    floor3 = NULL;
    floor4 = NULL;
    floor5 = NULL;
    return 0;
}

static int issue_request(int start_floor, int dest_floor, int type) {
    if (start_floor < 1 || start_floor > 5) return 1;
    if (dest_floor < 1 || dest_floor > 5) return 1;
    if (type < 0 || type > 3) return 1;

    add_pet_to_floor(type,start_floor,dest_floor);
    
    return 0;    
}

static int move_elevator_thread(void *data) {
    struct elevator* pet_ele = data;

    while (!kthread_should_stop()) {
        mutex_lock(&pet_ele->lock);

        if (!list_empty(&pet_ele->pet_list)) {
            /* Try to drop off any pets for this floor. If we dispensed a pet
             * sleep for a bit to simulate unloading — but unlock the elevator
             * lock before sleeping to avoid blocking other callers.
             */
            if (dispense_pets_from_elevator(pet_ele)) {
                mutex_unlock(&pet_ele->lock);
                ssleep(1);
                continue;
            }

            if (list_empty(&pet_ele->pet_list)) {
                mutex_unlock(&pet_ele->lock);
                continue;
            }

            struct pet* next_pet = list_first_entry(&pet_ele->pet_list,struct pet,list);
            int destination = next_pet->destination_floor;

            if (pet_ele->current_floor == destination) {
                struct floor* current_floor = NULL;
                if (pet_elevator->current_floor == 1) current_floor = floor1;
                if (pet_elevator->current_floor == 2) current_floor = floor2;
                if (pet_elevator->current_floor == 3) current_floor = floor3;
                if (pet_elevator->current_floor == 4) current_floor = floor4;
                if (pet_elevator->current_floor == 5) current_floor = floor5;

                if (current_floor) {
                    /* add_pet_to_elevator() takes the floor lock internally. */
                    if (!list_empty(&current_floor->pets_waiting)) {
                        add_pet_to_elevator(pet_elevator, current_floor);
                    }
                }
                /* Finished loading on this floor — unlock and sleep to simulate
                 * the loading time so other threads can acquire the elevator lock.
                 */
                mutex_unlock(&pet_ele->lock);
                ssleep(1);
                continue;
            }
            else if (pet_ele->current_floor < destination) {
                printk(KERN_INFO "Moving up a floor!\n");
                pet_elevator->current_floor = pet_elevator->current_floor + 1;
                /* Unlock while "moving" so other ops aren't blocked. */
                mutex_unlock(&pet_ele->lock);
                ssleep(2);
                continue;
            }
            else if (pet_elevator->current_floor > destination) {
                printk(KERN_INFO "Moving down a floor!\n");
                pet_elevator->current_floor = pet_elevator->current_floor - 1;
                mutex_unlock(&pet_ele->lock);
                ssleep(2);
                continue;
            }
        }
        else {
            // Check if any floors has a request right now
            if (look_for_request() == false) {
                // There was no requests
                mutex_unlock(&pet_ele->lock);
                printk(KERN_INFO "No requests right now\n");
                msleep_interruptible(1000);
                continue;
            }
            int direction = get_closest_request();
            printk(KERN_INFO "There is a pet waiting on floor - %d \n",direction);
            // If already on the floor, pick up the pet
            // Else if there is a request on a floor, ie(cur=1,start=4)
            // Then start to move towards that floor
            if (pet_elevator->current_floor == direction) {
                if (pet_elevator->current_floor == 1) {
                    if (!list_empty(&floor1->pets_waiting)) {
                        add_pet_to_elevator(pet_elevator,floor1);
                    }
                }
                else if (pet_elevator->current_floor == 2) {
                    if (!list_empty(&floor2->pets_waiting)) {
                        add_pet_to_elevator(pet_elevator,floor2);
                    }
                }
                else if (pet_elevator->current_floor == 3) {
                    if (!list_empty(&floor3->pets_waiting)) {
                        add_pet_to_elevator(pet_elevator,floor3);
                    }
                }
                else if (pet_elevator->current_floor == 4) {
                    if (!list_empty(&floor4->pets_waiting)) {
                        add_pet_to_elevator(pet_elevator,floor4);
                    }
                }
                else if (pet_elevator->current_floor == 5) {
                    if (!list_empty(&floor5->pets_waiting)) {
                        add_pet_to_elevator(pet_elevator,floor5);
                    }
                }
                // Sleep for 1 second when loading/unloading
                ssleep(1);
            }
            else if (pet_elevator->current_floor < direction) {
                printk(KERN_INFO "Moving up a floor!\n");
                pet_elevator->current_floor = pet_elevator->current_floor + 1;
                // Elevator sleeps for 2 seconds when moving a floor
                ssleep(1);
            }
            else if (pet_elevator->current_floor > direction) {
                printk(KERN_INFO "Moving down a floor!\n");
                pet_elevator->current_floor = pet_elevator->current_floor - 1;
                ssleep(1);
            }

        }

        mutex_unlock(&pet_ele->lock);
    }

    printk(KERN_INFO "Elevator thread exiting\n");
    //Here I should make sure I service all the requests before actually stopping
    return 0;
}

static void cleanup_elevator_list(struct elevator* ele) {
    struct pet* entry, *next_entry;
    list_for_each_entry_safe(entry, next_entry, &ele->pet_list, list) {
        list_del(&entry->list);
        kfree(entry);
    }
}

static void cleanup_floor_list(struct floor* flo) {
    struct pet* entry, *next_entry;
    list_for_each_entry_safe(entry, next_entry, &flo->pets_waiting, list) {
        list_del(&entry->list);
        kfree(entry);
    }
}

static bool dispense_pets_from_elevator(struct elevator* ele) {
    struct pet* entry, *next_entry;
    bool pet_dispensed = false;
    list_for_each_entry_safe(entry, next_entry, &ele->pet_list, list) {
        if (entry->destination_floor != ele->current_floor) continue;
        printk(KERN_INFO "Pet type -> %d has reached it's destination floor -> %d",
        entry->pet_type, entry->destination_floor);
        list_del(&entry->list);
        kfree(entry);
        ele->num_of_pets = ele->num_of_pets - 1;
        pet_dispensed = true;
    }
    return pet_dispensed;
}

static bool look_for_request(void) {

    bool found = false;

    mutex_lock(&floor1->lock);
    mutex_lock(&floor2->lock);
    mutex_lock(&floor3->lock);
    mutex_lock(&floor4->lock);
    mutex_lock(&floor5->lock);

    if (!list_empty(&floor1->pets_waiting)) found = true;
    else if (!list_empty(&floor2->pets_waiting)) found = true;
    else if (!list_empty(&floor3->pets_waiting)) found = true;
    else if (!list_empty(&floor4->pets_waiting)) found = true;
    else if (!list_empty(&floor5->pets_waiting)) found = true;

    mutex_unlock(&floor1->lock);
    mutex_unlock(&floor2->lock);
    mutex_unlock(&floor3->lock);
    mutex_unlock(&floor4->lock);
    mutex_unlock(&floor5->lock);

    return found;
}

static int get_closest_request(void) {
    int closest_floor;
    int max = INT_MAX;
    mutex_lock(&floor1->lock);
    mutex_lock(&floor2->lock);
    mutex_lock(&floor3->lock);
    mutex_lock(&floor4->lock);
    mutex_lock(&floor5->lock);
    if (!list_empty(&floor1->pets_waiting)) {
        if (pet_elevator->current_floor == 1) {
            closest_floor = 1;
            max = 0;
        }
        else if (pet_elevator->current_floor > 1) {
            closest_floor = 1;
            max = pet_elevator->current_floor - 1;
        }
    }
    if (!list_empty(&floor2->pets_waiting)) {
        int temp;
        if (pet_elevator->current_floor == 2) {
            closest_floor = 2;
            temp = 0;
        }
        else if (pet_elevator->current_floor > 2 && pet_elevator->current_floor - 2 < max) {
            closest_floor = 2;
            temp = pet_elevator->current_floor - 2;
        }
        else if (pet_elevator->current_floor < 2 && 2 - pet_elevator->current_floor < max) {
            closest_floor = 2;
            temp = 2 - pet_elevator->current_floor;
        }
        max = temp;
    }
    if (!list_empty(&floor3->pets_waiting)) {
        int temp;
        if (pet_elevator->current_floor == 3) {
            closest_floor = 3;
            temp = 0;
        }
        else if (pet_elevator->current_floor > 3 && pet_elevator->current_floor - 3 < max) {
            closest_floor = 3;
            temp = pet_elevator->current_floor - 3;
        }
        else if (pet_elevator->current_floor < 3 && 3 - pet_elevator->current_floor < max) {
            closest_floor = 3;
            temp = 3 - pet_elevator->current_floor;
        }
        max = temp;
    }
    if (!list_empty(&floor4->pets_waiting)) {
        int temp;
        if (pet_elevator->current_floor == 4) {
            closest_floor = 4;
            temp = 0;
        }
        else if (pet_elevator->current_floor > 4 && pet_elevator->current_floor - 4 < max) {
            closest_floor = 4;
            temp = pet_elevator->current_floor - 3;
        }
        else if (pet_elevator->current_floor < 4 && 4 - pet_elevator->current_floor < max) {
            closest_floor = 4;
            temp = 4 - pet_elevator->current_floor;
        }
        max = temp;
    }
    if (!list_empty(&floor5->pets_waiting)) {
        int temp;
        if (pet_elevator->current_floor == 5) {
            closest_floor = 5;
            temp = 0;
        }
        else if (pet_elevator->current_floor < 5 && 5 - pet_elevator->current_floor < max) {
            closest_floor = 5;
            temp = 5 - pet_elevator->current_floor;
        }
        max = temp;
    }
    mutex_unlock(&floor1->lock);
    mutex_unlock(&floor2->lock);
    mutex_unlock(&floor3->lock);
    mutex_unlock(&floor4->lock);
    mutex_unlock(&floor5->lock);
    return closest_floor;

}

static void add_pet_to_elevator(struct elevator* pet_elevator, struct floor* flo) {
    
    mutex_lock(&flo->lock); 
    printk(KERN_INFO "Attempting to add to elevator");
    
    while (!list_empty(&flo->pets_waiting)) {
        struct pet* new_pet = list_first_entry(&flo->pets_waiting, struct pet, list);
        if (!new_pet) break;
        
        int current_weight = 0;
        struct pet* entry;
        list_for_each_entry(entry, &pet_elevator->pet_list, list) {
            current_weight += entry->weight;
        }
        
        if (pet_elevator->num_of_pets >= 5 || (current_weight + new_pet->weight > max_weight)) {
            printk(KERN_INFO "Elevator full or too heavy, cannot add pet.\n");
            break; 
        }

        list_del(&new_pet->list);
        printk(KERN_INFO "Successfully added pet to elevator\n");
        
        list_add_tail(&new_pet->list, &pet_elevator->pet_list);
        pet_elevator->num_of_pets += 1;
    }
    
    mutex_unlock(&flo->lock);
}

static void add_pet_to_floor(int type, int start_floor, int dest_floor) {
    struct pet* new_pet = kmalloc(sizeof(*new_pet),GFP_KERNEL);
    if (!new_pet)
        return;

    new_pet->destination_floor = dest_floor;
    new_pet->starting_floor = start_floor;

    new_pet->pet_type = type;
    if (type == 0) new_pet->weight = 3; // chihuahua
    if (type == 1) new_pet->weight = 14; // pug
    if (type == 2) new_pet->weight = 10; // pughuahua
    if (type == 3) new_pet->weight = 16; // doxen

    mutex_lock(&floor1->lock);
    mutex_lock(&floor2->lock);
    mutex_lock(&floor3->lock);
    mutex_lock(&floor4->lock);
    mutex_lock(&floor5->lock);
    if (start_floor == 1) list_add_tail(&new_pet->list,&floor1->pets_waiting);
    if (start_floor == 2) list_add_tail(&new_pet->list,&floor2->pets_waiting);
    if (start_floor == 3) list_add_tail(&new_pet->list,&floor3->pets_waiting);
    if (start_floor == 4) list_add_tail(&new_pet->list,&floor4->pets_waiting);
    if (start_floor == 5) list_add_tail(&new_pet->list,&floor5->pets_waiting);
    mutex_unlock(&floor1->lock);
    mutex_unlock(&floor2->lock);
    mutex_unlock(&floor3->lock);
    mutex_unlock(&floor4->lock);
    mutex_unlock(&floor5->lock);

    printk(KERN_INFO "Pet has been added to floor %d \n", start_floor);
}

static ssize_t procfile_read(struct file* file, char* ubuf, size_t count, loff_t *ppos) {
    int len;

    // Check if we are at the end of the file
    if (*ppos > 0) {
        return 0;
    }

    // else len = snprintf(msg, BUF_LEN, "current time: %lld.%09ld\n", my_timespec.tv_sec, my_timespec.tv_nsec);
    // Ensure the formatted string doesn't exceed buffer capacity
    if (len >= BUF_LEN) {
        len = BUF_LEN - 1;
        msg[len] = '\0';
    }

    if (copy_to_user(ubuf, msg, len + 1)) return -EFAULT;

    // Update the file position and return the number of bytes copied
    *ppos = len + 1;

    printk(KERN_INFO "proc_read: gave to user %s", msg);

    return len + 1;
}

static const struct proc_ops procfile_fops = {
    .proc_read = procfile_read,
    // .proc_write = procfile_write,
};

static int __init init_elevator(void) {
    printk(KERN_INFO "Loading elevator module\n");
    proc_entry = proc_create(ENTRY_NAME,PERMS,PARENT, &procfile_fops);
    if (proc_entry == NULL) return -ENOMEM;
    STUB_start_elevator = start_elevator;
    STUB_issue_request = issue_request;
    STUB_stop_elevator = stop_elevator;
    return 0;
}

static void __exit cleanup_elevator(void) {
    stop_elevator();
    printk(KERN_INFO "Unloading elevator module\n");
    proc_remove(proc_entry);
    printk(KERN_INFO "/proc/%s removed\n", ENTRY_NAME);
    STUB_start_elevator = NULL;
    STUB_issue_request = NULL;
    STUB_stop_elevator = NULL;
}

module_init(init_elevator);
module_exit(cleanup_elevator);

MODULE_AUTHOR("Logan Harmon");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("My elevator kernel module to service pets");
MODULE_VERSION("1.0");