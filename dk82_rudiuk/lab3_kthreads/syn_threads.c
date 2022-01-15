#include <linux/module.h>       // required by all modules
#include <linux/kernel.h>       // required for sysinfo
#include <linux/init.h>         // used by module_init, module_exit macros
#include <linux/jiffies.h>      // where jiffies and its helpers reside
#include <linux/interrupt.h>    // tasklet
#include <linux/slab.h>         // memory allocation
#include <linux/kthread.h>      // threads
#include <linux/list.h>         // list
#include <linux/mutex.h>        // mutex
#include <linux/sched/task.h>

MODULE_DESCRIPTION("Threads and list");
MODULE_AUTHOR("thodnev & fastiA");
MODULE_VERSION("0.1");
MODULE_LICENSE("Dual MIT/GPL");		// this affects the kernel behavior

static LIST_HEAD(glob_var_list);

static struct task_struct **thread = NULL;
static unsigned long glob_var;

struct my_list_struct{
        struct list_head list;
        typeof(glob_var) glob_var_saved;
};

static unsigned long thread_num = 1;
static unsigned long thread_inccnt = 10;
static unsigned long thread_delay = 10;

module_param(thread_num, ulong, 0000);
MODULE_PARM_DESC(thread_num, "Amount of threads");
module_param(thread_inccnt, ulong, 0000);
MODULE_PARM_DESC(thread_inccnt, "How many times thread will increment glob_var");
module_param(thread_delay, ulong, 0000);
MODULE_PARM_DESC(thread_delay, "Delay between increment of glob_var");

DEFINE_MUTEX(list_mutex);
DEFINE_MUTEX(glob_var_mutex);

static int thread_func(void *param)
{
        unsigned long *glob_var_param = param;
        unsigned long temp_cnt = thread_inccnt;

        struct my_list_struct *new_node = kzalloc(sizeof(*new_node), GFP_KERNEL);
        if (NULL == new_node) {
                pr_err("Can`t allocate memory for new node of list\n");
                return -1;
        }

        INIT_LIST_HEAD(&new_node->list);

        while (!kthread_should_stop() && temp_cnt > 0) {
                mutex_lock(&glob_var_mutex);
                new_node->glob_var_saved = ++(*glob_var_param);
                mutex_unlock(&glob_var_mutex);
                temp_cnt--;
                schedule_timeout_uninterruptible(msecs_to_jiffies(thread_delay));
                pr_info("%s: thread glob_var: %lu\n", module_name(THIS_MODULE), new_node->glob_var_saved);
        }

        mutex_lock(&list_mutex);
        list_add_tail(&(new_node->list),&glob_var_list);
        mutex_unlock(&list_mutex);
        return 0;
}

static int __init my_module_init(void)
{
        int i;

        glob_var = 0;
        pr_info("%s started with: thread_num = %lu,\n"
                "\t\t   thread_inccnt = %lu,\n"
                "\t\t   thread_delay = %lu.\n",
                module_name(THIS_MODULE),thread_num,thread_inccnt, thread_delay);

        if (0 == thread_num) {
                pr_err("%s: thread num shouldn`t be 0\n", module_name(THIS_MODULE));
                return -1;
        }

        if (0 == thread_delay) {
                pr_warn("Thread delay is 0\n");
        }

        if (0 == thread_inccnt) {
                pr_warn("Thread increment count variable is 0\n");
        }

        thread = kmalloc(sizeof(**thread)*thread_num, GFP_KERNEL);

        for (i = 0; i < thread_num; i++) {

                thread[i] = kthread_run(thread_func, &glob_var, "thread[%i]", i);
                pr_info("%s: thread #%i connected\n",module_name(THIS_MODULE), i+1);

                if (IS_ERR(thread[i])) {
                        pr_err("Thread creation error %s\n", PTR_ERR(thread[i]) == -ENOMEM ? "ENOMEM" : "EINTR");
                        thread[i] = NULL;
                }

                get_task_struct(thread[i]);
        }
        return 0;
}

static void __exit my_module_exit(void)
{
        if (NULL == thread) {
                pr_warn("%s: memory for thread was not allocated\n", module_name(THIS_MODULE));
        }

        int i;
        for (i = 0; i < thread_num; i++) {
                kthread_stop(thread[i]);
                put_task_struct(thread[i]);
        }

        kfree(thread);
        struct my_list_struct *cursor, *next;

        i = 0;
        list_for_each_entry_safe (cursor, next, &glob_var_list, list) {
                pr_info("%s: node[%i] glob_var_saved = %lu\n",  module_name(THIS_MODULE), i, cursor->glob_var_saved);
                i++;
                list_del(&cursor->list);
                kfree(cursor);
        }

        return;
}

module_init(my_module_init);
module_exit(my_module_exit);
