#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/kthread.h>
#include <linux/jiffies.h>

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

/* module parameters */
static int hard = 0;
module_param(hard, int, 0);
MODULE_PARM_DESC(hard, "Generate hard lock-up, default="
                 __MODULE_STRING(hard));

static int locksec = 60;
module_param(locksec, int, 0);
MODULE_PARM_DESC(hard, "Duration of lockup (seconds) , default="
                 __MODULE_STRING(locksec));


struct task_struct *task;
static spinlock_t spinlock;

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
 	stop = jiffies + msecs_to_jiffies(locksec*1000);
	while(!kthread_should_stop() && (elapsedms < locksec*1000)
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
	return 0;
}

int soft_lockup_task(void *arg)
{
	unsigned long stop;

	pr_info("jiffies=%u (ms)\n", jiffies_to_msecs(jiffies));
	stop = jiffies + msecs_to_jiffies(locksec*1000);

	spin_lock(&spinlock);

	while(!kthread_should_stop() && time_before(jiffies, stop)) {
		cpu_relax();
	}
	spin_unlock(&spinlock);


	pr_info("done, jiffies=%u (ms)\n",
	        jiffies_to_msecs(jiffies));

	task = NULL;
	return 0;
}

static int lockup_init(void)
{
	pr_info("Lockup duration = %d", locksec);
	spin_lock_init(&spinlock);
	if (hard) {
		pr_info("Starting hard-lockup-test kthread\n");
		task = kthread_run(&hard_lockup_task,NULL,"hard-lockup-test");
	} else {
		pr_info("Starting soft-lockup-test kthread\n");
		task = kthread_run(&soft_lockup_task,NULL,"soft-lockup-test");
	}

	return 0;
}

static void lockup_exit(void)
{
	if (task) {
		pr_info("Stopping lockup-test kthread\n");
		kthread_stop(task);
	}
	pr_info("done\n");
}

module_init(lockup_init);
module_exit(lockup_exit);

MODULE_AUTHOR("Steven Arnold");
MODULE_DESCRIPTION("Test module to generate CPU soft/hard lock-up");
MODULE_LICENSE("GPL v2");
