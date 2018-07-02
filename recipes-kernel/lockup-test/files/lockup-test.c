#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/kthread.h>
#include <linux/jiffies.h>
#include <linux/mutex.h>
#include <linux/sysfs.h>

/* module parameters */
#define HARD_DEFAULT (0)
static int hard = HARD_DEFAULT;
module_param(hard, int, 0);
MODULE_PARM_DESC(hard, "Generate hard lock-up, default="
                 __MODULE_STRING(HARD_DEFAULT));

#define DURATION_DEFAULT (60)
static int duration = DURATION_DEFAULT;
module_param(duration, int, 0);
MODULE_PARM_DESC(duration, "Duration of lockup (seconds) , default="
                 __MODULE_STRING(DURATION_DEFAULT));

#define ONCPU_DEFAULT (0)
static int oncpu = ONCPU_DEFAULT;
module_param(oncpu, int, 0);
MODULE_PARM_DESC(oncpu, "Generate lock-up on cpu, default="
                 __MODULE_STRING(ONCPU_DEFAULT));

struct task_struct *task;
static spinlock_t spinlock;
DEFINE_MUTEX(running);

int hard_lockup_task(void *arg)
{
	unsigned long flags, stop;
	unsigned long elapsedms, loops,loopsperms;

	pr_info("jiffies=%u (ms)\n", jiffies_to_msecs(jiffies));

	spin_lock(&spinlock);

	/* We can't rely on jiffies to be incremented with interrupts disabled.
	 * Do a rough calibration of loops per ms, and use a loop counter
	 * to end the lockup.
	 */
	loops = 0;
	stop = jiffies + msecs_to_jiffies(1000);
	while(!kthread_should_stop() && time_before(jiffies, stop)) {
		cpu_relax();
		loops++;
	}
	spin_unlock(&spinlock);

	loopsperms = loops / 1000;
	pr_info("loops per ms %lu\n",loopsperms);

	local_irq_save(flags);
	spin_lock(&spinlock);

	loops = elapsedms = 0;
	stop = jiffies + msecs_to_jiffies(duration*1000);
	while(!kthread_should_stop() && (elapsedms < duration*1000)
	      && time_before(jiffies, stop)) {
		cpu_relax();
		loops++;
		if (loops == loopsperms) {
			loops = 0;
			elapsedms++;
		}
	}
	spin_unlock(&spinlock);
	local_irq_restore(flags);

	pr_info("done, jiffies=%u (ms)\n",
	        jiffies_to_msecs(jiffies));

	task = NULL;
	mutex_unlock(&running);
	return 0;
}

int soft_lockup_task(void *arg)
{
	unsigned long stop;

	pr_info("jiffies=%u (ms)\n", jiffies_to_msecs(jiffies));
	stop = jiffies + msecs_to_jiffies(duration*1000);

	spin_lock(&spinlock);

	while(!kthread_should_stop() && time_before(jiffies, stop)) {
		cpu_relax();
	}
	spin_unlock(&spinlock);


	pr_info("done, jiffies=%u (ms)\n",
	        jiffies_to_msecs(jiffies));

	task = NULL;

	mutex_unlock(&running);

	return 0;
}

static ssize_t start_show(struct kobject *kobj, struct kobj_attribute *attr,
                      char *buf)
{
        return sprintf(buf, "%d\n", mutex_is_locked(&running));
}

static ssize_t start_store(struct kobject *kobj, struct kobj_attribute *attr,
			   const char *buf, size_t count)
{
	(void) buf;
	(void) kobj;
	(void) attr;

	if (mutex_trylock(&running) == 0) {
		return -EBUSY;
	}

	/* ignore data and start test */
	if (hard) {
		pr_info("Starting hard-lockup-test kthread\n");
		task = kthread_create(&hard_lockup_task,
				      NULL,
				      "hard-lockup-test");
		kthread_bind(task, oncpu);
		wake_up_process(task);
	} else {
		pr_info("Starting soft-lockup-test kthread\n");
		task = kthread_create(&soft_lockup_task,
				      NULL,
				      "soft-lockup-test");
		kthread_bind(task, oncpu);
		wake_up_process(task);
	}

        return count;
}

static struct kobject *lt_kobject;
static struct kobj_attribute start_attribute =
	__ATTR(start, 0660, start_show,  start_store);

static int lockup_init(void)
{
	int error;

	spin_lock_init(&spinlock);
	mutex_init(&running);

	lt_kobject = kobject_create_and_add("lockup-test", kernel_kobj);
	if(!lt_kobject) return -ENOMEM;

	error = sysfs_create_file(lt_kobject, &start_attribute.attr);
	if (error) {
		pr_err("failed to create the file /sys/kernel/lockup-test/start\n");
	}

	pr_info("Lockup duration = %d\n", duration);
	pr_info("Lockup on cpu %d\n", oncpu);
	pr_info("Write to /sys/kernel/lockup-test/start to trigger lock-up\n");

	return 0;
}

static void lockup_exit(void)
{
	if (task) {
		pr_info("Stopping lockup-test kthread\n");
		kthread_stop(task);
	}

	kobject_put(lt_kobject);

	pr_info("done\n");
}

module_init(lockup_init);
module_exit(lockup_exit);

MODULE_AUTHOR("Steven Arnold");
MODULE_DESCRIPTION("Test module to generate CPU soft/hard lock-up");
MODULE_LICENSE("GPL v2");
