#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/syscalls.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/kallsyms.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kwai Liu");
MODULE_DESCRIPTION("Rickroll module");

#define KPROBE_LOOKUP 1
#include <linux/kprobes.h>
static struct kprobe kp = {
    .symbol_name = "kallsyms_lookup_name"
};

static char *rickroll_filename = "/home/bandit/Music/Rick Astley - Never Gonna Give You Up.mp3";

/*
 * Set up a module parameter for the filename. The arguments are variable name,
 * type, and permissions The third argument is the permissions for the parameter
 * file in sysfs, something like
 *
 * /sys/module/<module_name>/parameters/<parameter_name>
 *
 * We're setting it writeable by root so it can be modified without reloading
 * the module.
 */
module_param(rickroll_filename, charp, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(rickroll_filename, "The location of the rick roll file");


/*
 * cr0 is an x86 control register, and the bit we're twiddling here controls
 * the write protection. We need to do this because the area of memory
 * containing the system call table is write protected, and modifying it would
 * case a protection fault.
 */
#define DISABLE_WRITE_PROTECTION (write_cr0(read_cr0() & (~ 0x10000)))
#define ENABLE_WRITE_PROTECTION (write_cr0(read_cr0() | 0x10000))


static int rickroll_open(const char __user *filename, int flags, umode_t mode);

asmlinkage long (*original_sys_open)(const char __user *, int, umode_t);
asmlinkage unsigned long **sys_call_table;


static int __init rickroll_init(void)
{
    if(!rickroll_filename) {
	printk(KERN_ERR "No rick roll filename given.");
	return -EINVAL;  /* invalid argument */
    }

	#ifdef KPROBE_LOOKUP
    typedef unsigned long (*kallsyms_lookup_name_t)(const char *name);
    kallsyms_lookup_name_t kallsyms_lookup_name;
    register_kprobe(&kp);
    kallsyms_lookup_name = (kallsyms_lookup_name_t) kp.addr;
    unregister_kprobe(&kp);
	#endif

    sys_call_table = kallsyms_lookup_name("sys_call_table");

    if(!sys_call_table) {
	printk(KERN_ERR "Couldn't find sys_call_table.\n");
	return -EPERM;  /* operation not permitted; couldn't find general error */
    }

    /*
     * Replace the entry for open with our own function. We save the location
     * of the real sys_open so we can put it back when we're unloaded.
     */
    DISABLE_WRITE_PROTECTION;
    original_sys_open = (void *) sys_call_table[__NR_open];
    sys_call_table[__NR_open] = (unsigned long *) rickroll_open;
    ENABLE_WRITE_PROTECTION;

    printk(KERN_INFO "Never gonna give you up!\n");
    return 0;  /* zero indicates success */
}


/*
 * Our replacement for sys_open, which forwards to the real sys_open unless the
 * file name ends with .mp3, in which case it opens the rick roll file instead.
 */
static int rickroll_open(const char __user *filename, int flags, umode_t mode)
{
    int len = strlen(filename);

    /* See if we should hijack the open */
    if(strcmp(filename + len - 4, ".mp3")) {
		/* Just pass through to the real sys_open if the extension isn't .mp3 */
		return (*original_sys_open)(filename, flags, mode);
    } else {
		long fd;

		/*
		 * Read the filename from user space into kernel memory.
		 */

		unsigned long user_filename_len = strnlen_user(filename, 4096);
		char kernel_filename[user_filename_len + 1];
		const char *const_kernel_filename = kernel_filename;
		copy_from_user(kernel_filename, filename, user_filename_len);
		kernel_filename[user_filename_len] = '\0';

		/*
		 * Open the rickroll file instead.
		 */
		fd = (*original_sys_open)("/home/bandit/Music/Rick Astley - Never Gonna Give You Up.mp3", flags, mode);

		/*
		 * Copy the rickroll filename back to user space.
		 */
		copy_to_user(filename, const_kernel_filename, strlen(kernel_filename));

		return fd;
    }
}


static void __exit rickroll_cleanup(void)
{
    printk(KERN_INFO "Ok, now we're gonna give you up. Sorry.\n");

    /* Restore the original sys_open in the table */
    DISABLE_WRITE_PROTECTION;
    sys_call_table[__NR_open] = (unsigned long *) original_sys_open;
    ENABLE_WRITE_PROTECTION;
}


module_init(rickroll_init);
module_exit(rickroll_cleanup);
