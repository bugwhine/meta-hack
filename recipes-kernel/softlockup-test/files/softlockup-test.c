#include<linux/kernel.h>
#include<linux/module.h>
#include<linux/kthread.h>

struct task_struct *task0;
static spinlock_t spinlock;

int task(void *arg)
{
    printk(KERN_INFO "%s:%d\n",__func__,__LINE__);

    spin_lock(&spinlock);
    while(!kthread_should_stop()) {
	cpu_relax();
    }
    spin_unlock(&spinlock);

    return 0;
}

static int softlockup_init(void)
{
    printk(KERN_INFO "%s:%d\n",__func__,__LINE__);

    spin_lock_init(&spinlock);
    task0 = kthread_run(&task,NULL,"softlockup_thread");

    return 0;
}

static void softlockup_exit(void)
{
    printk(KERN_INFO "%s:%d\n",__func__,__LINE__);
    kthread_stop(task0);
}

module_init(softlockup_init);
module_exit(softlockup_exit);

MODULE_AUTHOR("Saiyam Doshi");
MODULE_DESCRIPTION("Test module to generate CPU soft lock-up/panic");
MODULE_LICENSE("GPL v2");
