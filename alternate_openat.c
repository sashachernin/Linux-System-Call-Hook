#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/unistd.h>
#include <linux/sched.h>
#include <linux/sysfs.h>
#include <linux/kobject.h>
#include <linux/kprobes.h>
#include <linux/ftrace.h>
#include <linux/string.h>
#include <linux/fs.h>
#include <linux/syscalls.h>
#include <linux/kallsyms.h>
#include <linux/limits.h>
#include <asm/uaccess.h>

MODULE_AUTHOR("Sasha");
MODULE_LICENSE("GPL");

#define HOOK(_name, _hook, _orig) \
{\
	.name = (_name), \
	.function = (_hook), \
	.original = (_orig), \
}

static char file[4096];
static char* editor = "gedit";

/*defining module parameter*/
module_param(editor, charp, 0);


/*
 * This function handles reads from /sys/kernel/opened_with_gedit/file
 * It takes pointer to KObject, its attributes and pointer to buffer
 * Returns count of bytes written to the buffer 
 */
static ssize_t file_show(struct kobject *kobj, struct kobj_attribute *attr,
			 char *buf)
{
    return sprintf(buf, "%s\n", file);
}

/*
 * This functions handles writes to /sys/kernel/openeed_with_gedit/file
 * It takes pointer to KObject, its attributes, pointer to the buffer and count of bytes written to it
 * Actually, this function does nothing and returns original 'count'
 */
static ssize_t file_store(struct kobject *kobj,
			  struct kobj_attribute *attr, const char *buf,
			  size_t count)
{
    return count;
}

/* /sys/kernel/opened_with_gedit/file attributes */
static struct kobj_attribute file_attribute =
__ATTR(file, 0644, file_show, file_store);

static struct attribute *attrs[] = {
    &file_attribute.attr,
    NULL,
};

static struct attribute_group attr_group = {
    .attrs = attrs,
};

/* SysFS KObject*/
static struct kobject *opened_with_gedit;

/* Table of syscalls */
void **sys_call_table = NULL;

/* KProbe structure used for retrieving 'kallsyms_lookup_name' function */
static struct kprobe kp = {
    .symbol_name = "kallsyms_lookup_name"
};

#define USE_FENTRY_OFFSET 0
#if !USE_FENTRY_OFFSET
#pragma GCC optimize("-fno-optimize-sibling-calls")
#endif

/* Structure for hooking with ftrace */
struct ftrace_hook {
    const char *name;
    void *function;
    void *original;
    unsigned long address;
    struct ftrace_ops ops;
};

/* 
 * This functions resolves symbol adderessed with kallsyms_lookup_name
 * It takes pointer to ftrace_hook stucture and fills its address field
 * Returns 0 on success and -ENOENT when symbol cant be resolved
 */
static int resolve_hook_address(struct ftrace_hook *hook)
{
    typedef unsigned long (*kallsyms_lookup_name_t)(const char *name);
    kallsyms_lookup_name_t kallsyms_lookup_name;
    register_kprobe(&kp);
    kallsyms_lookup_name = (kallsyms_lookup_name_t) kp.addr;
    unregister_kprobe(&kp);
    hook->address = kallsyms_lookup_name(hook->name);

    if (!hook->address) {
	printk(KERN_DEBUG "alternate_openat: unresolved symbol: %s\n",
	       hook->name);
	return -ENOENT;
    }
#if USE_FENTRY_OFFSET
    *((unsigned long *) hook->original) = hook->address + MCOUNT_INSN_SIZE;
#else
    *((unsigned long *) hook->original) = hook->address;
#endif
    return 0;
}

/*
 * This function being installed as hook->ops.func (look int install_hook below)
 * It takes Instruction Pointer, parent Instruction Pointer, pointer to ftrace_ops structure and pt_regs structure
 * It sets the proper Instruction Pointer into registers
 * Returns nothing
 */
static void notrace helper_ftrace_thunk(unsigned long ip,
					unsigned long parent_ip,
					struct ftrace_ops *ops,
					struct pt_regs *regs)
{
    struct ftrace_hook *hook = container_of(ops, struct ftrace_hook, ops);
#if USE_FENTRY_OFFSET
    regs->ip = (unsigned long) hook->function;
#else
    if (!within_module(parent_ip, THIS_MODULE))
	regs->ip = (unsigned long) hook->function;
#endif
}


/*
 * This function installs hook with kernel ftrace
 * It takes pointer to ftrace_hook structure, modifies it and passing its fields to kernel ftrace functions
 * Returns 0 on success and apporpriate error code on errors
 */
int install_hook(struct ftrace_hook *hook)
{
    int err;
    err = resolve_hook_address(hook);
    if (err)
	return err;

    hook->ops.func = helper_ftrace_thunk;
    hook->ops.flags = FTRACE_OPS_FL_SAVE_REGS
	| FTRACE_OPS_FL_RECURSION_SAFE | FTRACE_OPS_FL_IPMODIFY;

    err = ftrace_set_filter_ip(&hook->ops, hook->address, 0, 0);
    if (err) {
	return err;
    }

    err = register_ftrace_function(&hook->ops);
    if (err) {
	return err;
    }

    return 0;
}

/*
 * This function removes hook
 * It takes pointer to ftrace_hook structure
 * Returns nothing
 */
void remove_hook(struct ftrace_hook *hook)
{
    unregister_ftrace_function(&hook->ops);
    ftrace_set_filter_ip(&hook->ops, hook->address, 1, 0);
}

/*
 * This function loops over an array of ftrace_hook structures, passing it to 'install_hook'
 * It takes array of ftrace_hook structures and its size
 * Returns nothing
 */
int install_hooks(struct ftrace_hook *hooks, size_t count)
{
    int err;
    size_t i;

    for (i = 0; i < count; i++) {
	err = install_hook(&hooks[i]);
	if (err)
	    goto error;
    }
    return 0;

  error:
    while (i != 0) {
	remove_hook(&hooks[--i]);
    }
    return err;
}

/*
 * This function loops over an array of ftrace_hook structures, passing it to 'remove_hook'
 * It takes array of ftrace_hook structures and its size
 * Returns nothing
 */
void remove_hooks(struct ftrace_hook *hooks, size_t count)
{
    size_t i;

    for (i = 0; i < count; i++)
	remove_hook(&hooks[i]);
}

/* The original openat syscall */
static asmlinkage long (*original_openat)(const struct pt_regs *);

/* alternated openat syscall */
asmlinkage int hooked_openat(const struct pt_regs *regs)
{
	if (strcmp(current->comm, editor) == 0){
		strncpy_from_user(file, (char*)regs->si, 4096);
		sysfs_notify(opened_with_gedit, NULL, "file");
	}
    return original_openat(regs);
}

/* array of ftrace_hook structures */ 
static struct ftrace_hook hooks[] = {
    HOOK("__x64_sys_openat", hooked_openat, &original_openat),
};

/* This function called when we are loading module
 * It emits warning, sets up sysfs KObject and inserts alternated openat syscall
 * Returns 0 on success and appropiate error code on errors
 */
int init_module()
{
    printk(KERN_CRIT "This module is extremely dangerous\n");
    printk(KERN_CRIT "It alternates generic Linux system call\n");
    printk(KERN_CRIT
	   "Damages may be unacceptable. Use at your own risk\n");
    int err;
    err = install_hooks(hooks, ARRAY_SIZE(hooks));
    if (err)
	return err;
    opened_with_gedit =
	kobject_create_and_add("opened_with_gedit", kernel_kobj);
    if (!opened_with_gedit)
	return -ENOMEM;
    int retval = sysfs_create_group(opened_with_gedit, &attr_group);
    if (retval)
	kobject_put(opened_with_gedit);

    return retval;
}

/* This fuction calles when we are unloading module (e.g with rmmod)
 * It emits informational message, unregisters hooked openat syscall and removes sysfs KObject
 * Returns nothing
 */
void cleanup_module()
{
    printk(KERN_INFO "Restoring original openat syscall\n");
    remove_hooks(hooks, ARRAY_SIZE(hooks));
    kobject_put(opened_with_gedit);
}
