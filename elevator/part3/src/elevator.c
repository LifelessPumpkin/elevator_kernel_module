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
#define BUF_LEN 2048

extern int (*STUB_start_elevator)(void);
extern int (*STUB_issue_request)(int,int,int);
extern int (*STUB_stop_elevator)(void);

static struct proc_dir_entry* proc_entry;
static char msg[BUF_LEN];
static int max_weight = 50;

struct pet
{
    struct list_head list;
    int pet_type; // 0 = chihuahua, 1 = pug, 2 = pughuahua, 3 = doxen
    int weight; // 3, 14, 10, 16
    int starting_floor;
    int destination_floor;
};
struct floor 
{
    struct list_head pets_waiting;
    struct mutex lock;
    bool elevator_at_floor;
};
enum elevator_state {
    ELEVATOR_OFFLINE = 0,
    ELEVATOR_IDLE,
    ELEVATOR_LOADING,
    ELEVATOR_UP,
    ELEVATOR_DOWN,
};
struct elevator 
{
    int current_floor;
    int num_of_pets;
    enum elevator_state state;
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
static struct floor* floors[5];

static int pets_serviced = 0;

static int start_elevator(void) {

    if (pet_elevator && pet_elevator->state != ELEVATOR_OFFLINE) {
        // printk(KERN_INFO "Elevator already active\n");
        return 1;
    }

    pet_elevator = kmalloc(sizeof(*pet_elevator), GFP_KERNEL);
    int i;
    for (i = 0; i < 5; ++i) {
        floors[i] = kmalloc(sizeof(*floors[i]), GFP_KERNEL);
        if (!floors[i]) break;
    }
    
    if (!pet_elevator || i < 5) {
        printk(KERN_INFO "Couldn't allocate memory to run the elevator\n");
        if (pet_elevator) {
            kfree(pet_elevator);
            pet_elevator = NULL;
        }
        while (--i >= 0) {
            kfree(floors[i]);
            floors[i] = NULL;
        }
        return -ENOMEM;
    }
    
    pet_elevator->state = ELEVATOR_IDLE;
    pet_elevator->current_floor = 1;
    mutex_init(&pet_elevator->lock);
    for (i = 0; i < 5; ++i)
        mutex_init(&floors[i]->lock);

    INIT_LIST_HEAD(&pet_elevator->pet_list);
    for (i = 0; i < 5; ++i)
        INIT_LIST_HEAD(&floors[i]->pets_waiting);

    pet_elevator->num_of_pets = 0;
    for (i = 0; i < 5; ++i)
        floors[i]->elevator_at_floor = false;

    pet_elevator->thread = kthread_run(move_elevator_thread,pet_elevator,"elevator_thread");
        if (IS_ERR(pet_elevator->thread)) {
        printk(KERN_ERR "Failed to create the elevator thread\n");
        if (pet_elevator) { kfree(pet_elevator); pet_elevator = NULL; }
        for (i = 0; i < 5; ++i) {
            if (floors[i]) { kfree(floors[i]); floors[i] = NULL; }
        }
        return -ENOMEM;
    }
    return 0;
}

static int stop_elevator(void) {
    int i;
    if (!pet_elevator || pet_elevator->state == ELEVATOR_OFFLINE) return 1;

    mutex_lock(&pet_elevator->lock);
    pet_elevator->state = ELEVATOR_OFFLINE;
    mutex_unlock(&pet_elevator->lock);

    kthread_stop(pet_elevator->thread);
    // printk(KERN_INFO "Elevator successfully stopped\n");

    cleanup_elevator_list(pet_elevator);
    for (i = 0; i < 5; ++i)
        cleanup_floor_list(floors[i]);

    kfree(pet_elevator);
    for (i = 0; i < 5; ++i) {
        kfree(floors[i]);
        floors[i] = NULL;
    }
    pet_elevator = NULL;
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

            struct floor* current_floor = NULL;
            if (pet_elevator->current_floor >= 1 && pet_elevator->current_floor <= 5)
                current_floor = floors[pet_elevator->current_floor - 1];
            if (current_floor) {
                if (!list_empty(&current_floor->pets_waiting))
                    add_pet_to_elevator(pet_elevator, current_floor);
            }

            if (pet_ele->current_floor == destination) {
                
                mutex_unlock(&pet_ele->lock);
                ssleep(1);
                continue;
            }
            else if (pet_ele->current_floor < destination) {
                // printk(KERN_INFO "Moving up a floor!\n");
                pet_elevator->current_floor = pet_elevator->current_floor + 1;
                mutex_unlock(&pet_ele->lock);
                ssleep(2);
                continue;
            }
            else if (pet_elevator->current_floor > destination) {
                // printk(KERN_INFO "Moving down a floor!\n");
                pet_elevator->current_floor = pet_elevator->current_floor - 1;
                mutex_unlock(&pet_ele->lock);
                ssleep(2);
                continue;
            }
        }
        else {
            if (look_for_request() == false) {
                mutex_unlock(&pet_ele->lock);
                // printk(KERN_INFO "No requests right now\n");
                msleep_interruptible(1000);
                continue;
            }
            int direction = get_closest_request();
            // printk(KERN_INFO "There is a pet waiting on floor - %d \n",direction);
            if (pet_elevator->current_floor == direction) {
                if (pet_elevator->current_floor >= 1 && pet_elevator->current_floor <= 5) {
                    struct floor *f = floors[pet_elevator->current_floor - 1];
                    if (f && !list_empty(&f->pets_waiting)){
                        add_pet_to_elevator(pet_elevator, f);
                        ssleep(1);
                    }
                }
            }
            else if (pet_elevator->current_floor < direction) {
                // printk(KERN_INFO "Moving up a floor!\n");
                pet_elevator->current_floor = pet_elevator->current_floor + 1;
                ssleep(2);
            }
            else if (pet_elevator->current_floor > direction) {
                // printk(KERN_INFO "Moving down a floor!\n");
                pet_elevator->current_floor = pet_elevator->current_floor - 1;
                ssleep(2);
            }

        }

        mutex_unlock(&pet_ele->lock);
    }

    mutex_lock(&pet_ele->lock);
    while (!list_empty(&pet_ele->pet_list)) {
        if (dispense_pets_from_elevator(pet_ele)) {
            mutex_unlock(&pet_ele->lock);
            ssleep(1);
            mutex_lock(&pet_ele->lock);
            continue;
        }

        struct pet* next_pet = list_first_entry(&pet_ele->pet_list, struct pet, list);
        int destination = next_pet->destination_floor;

        if (pet_ele->current_floor == destination) {
            mutex_unlock(&pet_ele->lock);
            ssleep(1);
            mutex_lock(&pet_ele->lock);
            continue;
        } else if (pet_ele->current_floor < destination) {
            // printk(KERN_INFO "Moving up a floor!\n");
            pet_ele->current_floor = pet_ele->current_floor + 1;
            mutex_unlock(&pet_ele->lock);
            ssleep(2);
            mutex_lock(&pet_ele->lock);
            continue;
        } else {
            // printk(KERN_INFO "Moving down a floor!\n");
            pet_ele->current_floor = pet_ele->current_floor - 1;
            mutex_unlock(&pet_ele->lock);
            ssleep(2);
            mutex_lock(&pet_ele->lock);
            continue;
        }
    }
    mutex_unlock(&pet_ele->lock);
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
    mutex_lock(&flo->lock);
    list_for_each_entry_safe(entry, next_entry, &flo->pets_waiting, list) {
        list_del(&entry->list);
        kfree(entry);
    }
    mutex_unlock(&flo->lock);
}

static bool dispense_pets_from_elevator(struct elevator* ele) {
    struct pet* entry, *next_entry;
    bool pet_dispensed = false;
    list_for_each_entry_safe(entry, next_entry, &ele->pet_list, list) {
        if (entry->destination_floor != ele->current_floor) continue;
    ele->state = ELEVATOR_LOADING;
    // printk(KERN_INFO "Pet type -> %d has reached its destination floor -> %d\n",
    //        entry->pet_type, entry->destination_floor);
        list_del(&entry->list);
        kfree(entry);
        ele->num_of_pets = ele->num_of_pets - 1;
        pets_serviced++;
        pet_dispensed = true;
    }
    return pet_dispensed;
}

static bool look_for_request(void) {

    bool found = false;

    int i;
    for (i = 0; i < 5; ++i)
        mutex_lock(&floors[i]->lock);

    for (i = 0; i < 5; ++i) {
        if (!list_empty(&floors[i]->pets_waiting)) { found = true; break; }
    }

    for (i = 0; i < 5; ++i)
        mutex_unlock(&floors[i]->lock);

    return found;
}

static int get_closest_request(void) {
    int closest_floor = pet_elevator->current_floor;
    int best_dist = INT_MAX;
    int dist;

    int i;
    for (i = 0; i < 5; ++i)
        mutex_lock(&floors[i]->lock);

    for (i = 0; i < 5; ++i) {
        if (!list_empty(&floors[i]->pets_waiting)) {
            int floor_no = i + 1;
            dist = (pet_elevator->current_floor > floor_no) ? (pet_elevator->current_floor - floor_no) : (floor_no - pet_elevator->current_floor);
            if (dist < best_dist) { best_dist = dist; closest_floor = floor_no; }
        }
    }

    for (i = 0; i < 5; ++i)
        mutex_unlock(&floors[i]->lock);
    return closest_floor;

}

static void add_pet_to_elevator(struct elevator* pet_elevator, struct floor* flo) {
    
    mutex_lock(&flo->lock); 
    
    while (!list_empty(&flo->pets_waiting)) {
        struct pet* new_pet = list_first_entry(&flo->pets_waiting, struct pet, list);
        if (!new_pet) break;
        
        int current_weight = 0;
        struct pet* entry;
        list_for_each_entry(entry, &pet_elevator->pet_list, list) {
            current_weight += entry->weight;
        }
        
        if (pet_elevator->num_of_pets >= 5 || (current_weight + new_pet->weight > max_weight)) {
            // printk(KERN_INFO "Elevator full or too heavy, cannot add pet.\n");
            break; 
        }

        list_del(&new_pet->list);
        // printk(KERN_INFO "Successfully added pet to elevator\n");
        
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

    int i;
    for (i = 0; i < 5; ++i)
        mutex_lock(&floors[i]->lock);
    if (start_floor >= 1 && start_floor <= 5)
        list_add_tail(&new_pet->list, &floors[start_floor-1]->pets_waiting);
    for (i = 0; i < 5; ++i)
        mutex_unlock(&floors[i]->lock);

    // printk(KERN_INFO "Pet has been added to floor %d \n", start_floor);
}

static ssize_t procfile_read(struct file* file, char* ubuf, size_t count, loff_t *ppos) {
    int len = 0;
    int i;

    if (*ppos > 0) return 0;

    int cur_floor = 0;
    int cur_num = 0;
    int cur_load = 0;
    char *elev_pets = kmalloc(512, GFP_KERNEL);
    if (!elev_pets) return -ENOMEM;
    elev_pets[0] = '\0';

    if (!pet_elevator || pet_elevator->state == ELEVATOR_OFFLINE) {
        len += scnprintf(msg + len, BUF_LEN - len, "Elevator state: OFFLINE\n");
        len += scnprintf(msg + len, BUF_LEN - len, "Current floor: N/A\n");
        len += scnprintf(msg + len, BUF_LEN - len, "Current load: 0 lbs\n");
        len += scnprintf(msg + len, BUF_LEN - len, "Elevator status:\n\n");
    } else {
        mutex_lock(&pet_elevator->lock);
        cur_floor = pet_elevator->current_floor;
        cur_num = pet_elevator->num_of_pets;

        {
            struct pet* e;
            int off = 0;
            for (e = list_first_entry_or_null(&pet_elevator->pet_list, struct pet, list);
                 e != NULL;
                 e = list_is_head(&e->list, &pet_elevator->pet_list) ? NULL : list_first_entry_or_null(&e->list, struct pet, list)) {
                break;
            }
            off = 0;
            list_for_each_entry(e, &pet_elevator->pet_list, list) {
                char t = (e->pet_type == 0) ? 'C' : (e->pet_type == 1) ? 'P' : (e->pet_type == 2) ? 'H' : 'D';
                off += scnprintf(elev_pets + off, 512 - off, "%c%d ", t, e->destination_floor);
                cur_load += e->weight;
                if (off >= 512 - 16) break;
            }
            if (off > 0 && elev_pets[off-1] == ' ') elev_pets[off-1] = '\0';
            else elev_pets[off] = '\0';
        }
        mutex_unlock(&pet_elevator->lock);

        bool loading = false;
        (void)loading;
    }

    int total_waiting = 0;
    bool current_floor_has_waiters = false;
    struct pet *entry, *next_entry;
    char (*floor_lines)[512] = kmalloc_array(5, 512, GFP_KERNEL);
    if (!floor_lines) { kfree(elev_pets); return -ENOMEM; }
    for (i = 0; i < 5; ++i) floor_lines[i][0] = '\0';

    int floor_counts[5] = {0,0,0,0,0};
    for (i = 5; i >= 1; --i) {
    struct floor* flo = floors[i-1];
        int count = 0;
        int off = 0;
        if (!flo) continue;
        mutex_lock(&flo->lock);
        list_for_each_entry_safe(entry, next_entry, &flo->pets_waiting, list) {
            count++;
            char t = (entry->pet_type == 0) ? 'C' : (entry->pet_type == 1) ? 'P' : (entry->pet_type == 2) ? 'H' : 'D';
            off += scnprintf(floor_lines[i-1] + off, sizeof(floor_lines[i-1]) - off, "%c%d ", t, entry->destination_floor);
            if (off >= (int)sizeof(floor_lines[i-1]) - 8) break;
        }
        mutex_unlock(&flo->lock);
        if (off > 0 && floor_lines[i-1][off-1] == ' ') floor_lines[i-1][off-1] = '\0';
        total_waiting += count;
        floor_counts[i-1] = count;
        if (pet_elevator && pet_elevator->state != ELEVATOR_OFFLINE && pet_elevator->current_floor == i && count > 0) current_floor_has_waiters = true;
    }

    char state_buf[32] = "IDLE";
    if (!pet_elevator || pet_elevator->state == ELEVATOR_OFFLINE) {
        strcpy(state_buf, "OFFLINE");
    } else {
        bool has_dest_here = false;
        int cur_floor_local = pet_elevator->current_floor;
        mutex_lock(&pet_elevator->lock);
        list_for_each_entry(entry, &pet_elevator->pet_list, list) {
            if (entry->destination_floor == cur_floor_local) { has_dest_here = true; break; }
        }
        mutex_unlock(&pet_elevator->lock);
        if (has_dest_here || current_floor_has_waiters) strcpy(state_buf, "LOADING");
        else if (pet_elevator->num_of_pets == 0 && total_waiting == 0) strcpy(state_buf, "IDLE");
        else {
            int direction = 0;
            mutex_lock(&pet_elevator->lock);
            if (!list_empty(&pet_elevator->pet_list)) {
                struct pet* first = list_first_entry(&pet_elevator->pet_list, struct pet, list);
                if (first->destination_floor > pet_elevator->current_floor) direction = 1;
                else if (first->destination_floor < pet_elevator->current_floor) direction = -1;
            }
            mutex_unlock(&pet_elevator->lock);
            if (direction == 0 && total_waiting > 0) {
                int closest = get_closest_request();
                if (closest > pet_elevator->current_floor) direction = 1;
                else if (closest < pet_elevator->current_floor) direction = -1;
            }
            if (direction == 1) strcpy(state_buf, "UP");
            else if (direction == -1) strcpy(state_buf, "DOWN");
            else strcpy(state_buf, "IDLE");
        }
    }

    cur_load = 0;
    elev_pets[0] = '\0';
    if (pet_elevator && pet_elevator->state != ELEVATOR_OFFLINE) {
        mutex_lock(&pet_elevator->lock);
        list_for_each_entry(entry, &pet_elevator->pet_list, list) {
            cur_load += entry->weight;
        }
        {
            int off = 0;
            list_for_each_entry(entry, &pet_elevator->pet_list, list) {
                char t = (entry->pet_type == 0) ? 'C' : (entry->pet_type == 1) ? 'P' : (entry->pet_type == 2) ? 'H' : 'D';
                off += scnprintf(elev_pets + off, 512 - off, "%c%d ", t, entry->destination_floor);
                if (off >= 512 - 8) break;
            }
            if (off > 0 && elev_pets[off-1] == ' ') elev_pets[off-1] = '\0';
            else elev_pets[off] = '\0';
        }
        cur_num = pet_elevator->num_of_pets;
        cur_floor = pet_elevator->current_floor;
        mutex_unlock(&pet_elevator->lock);
    }

    len = 0; 
    len += scnprintf(msg + len, BUF_LEN - len, "Elevator state: %s\n", state_buf);
    if (pet_elevator && pet_elevator->state != ELEVATOR_OFFLINE)
        len += scnprintf(msg + len, BUF_LEN - len, "Current floor: %d\n", pet_elevator->current_floor);
    else
        len += scnprintf(msg + len, BUF_LEN - len, "Current floor: N/A\n");
    len += scnprintf(msg + len, BUF_LEN - len, "Current load: %d lbs\n", cur_load);
    len += scnprintf(msg + len, BUF_LEN - len, "Elevator status: %s\n\n", elev_pets[0] ? elev_pets : "");

    for (i = 5; i >= 1; --i) {
    bool here = (pet_elevator && pet_elevator->state != ELEVATOR_OFFLINE && pet_elevator->current_floor == i);
        len += scnprintf(msg + len, BUF_LEN - len, "[%s] Floor %d: %d%s%s\n",
                         here ? "*" : " ", i,
                         floor_counts[i-1],
                         (floor_lines[i-1][0] ? " " : ""),
                         (floor_lines[i-1][0] ? floor_lines[i-1] : ""));
    }

    len += scnprintf(msg + len, BUF_LEN - len, "\nNumber of pets: %d\n", cur_num);
    len += scnprintf(msg + len, BUF_LEN - len, "Number of pets waiting: %d\n", total_waiting);
    len += scnprintf(msg + len, BUF_LEN - len, "Number of pets serviced: %d\n", pets_serviced);

    if (len >= BUF_LEN) len = BUF_LEN - 1;
    if (copy_to_user(ubuf, msg, len + 1)) {
        kfree(elev_pets);
        kfree(floor_lines);
        return -EFAULT;
    }
    *ppos = len + 1;
    kfree(elev_pets);
    kfree(floor_lines);
    return len + 1;
}

static const struct proc_ops procfile_fops = {
    .proc_read = procfile_read,
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