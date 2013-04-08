/*
 * Copyright (C) 2013 Sami Sorell
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
*/



#if !defined __KERNEL__  &&  !defined __MODULE__
#error This is intended as test module for runtrace application
#endif

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/timer.h>
#include <linux/proc_fs.h>
#include "../runtrace.h"


#define MODNAME "rt_mod_test"


static struct proc_dir_entry *proc_file;

struct priv
{
	int process;
	int cntr;
	// char buf[RT_MSG_MAX];
};


static int pnum;


static int 
proc_open(struct inode *const inode, struct file *const filp)
{
	struct priv *const priv = kmalloc(sizeof(struct priv), GFP_KERNEL);
	filp->private_data = priv;

	priv->process = pnum++;
	priv->cntr    = 0;

	return 0;
}

static int 
proc_release(struct inode *const inode, struct file *const filp)
{
	if (filp->private_data) {
		kfree(filp->private_data);
	}

	return 0;
}


static ssize_t
proc_write(struct file *filp, const char __user *buffer, size_t count, loff_t *const data)
{
	struct priv *const priv = (struct priv *) filp->private_data;
	
	// snprintf(priv->buf, sizeof(priv->buf), "Process %d, write %d, cpu %d", priv->process, priv->cntr++, smp_processor_id());
	// TP_FUNK(priv->buf);

	TP_FUNK_P("Process %d, write %d, cpu %d", priv->process, priv->cntr++, smp_processor_id());

	return count;
}


static struct file_operations const proc_fops = {
	.owner   = THIS_MODULE,
	.open    = proc_open,
	.release = proc_release,
	.write   = proc_write,
};


static void 
runtrace_test_exit(void)
{
	printk(KERN_INFO MODNAME ": Unloading module\n");

	remove_proc_entry(MODNAME, NULL);
	runtrace_exit();
}


static int 
runtrace_test_init(void)
{
	printk(KERN_INFO "Loading " MODNAME "\n");
	
	runtrace_init();

	proc_file = create_proc_entry(MODNAME, 0777, NULL);
	if (!proc_file) {
		printk(KERN_ERR MODNAME ": Failed to create proc entry '%s'\n", MODNAME);
		return -EPERM;
	}
	
	proc_file->proc_fops = &proc_fops;
	return 0;
}


module_init(runtrace_test_init);
module_exit(runtrace_test_exit);

MODULE_AUTHOR("Sami Sorell");
MODULE_DESCRIPTION("Testing utility for runtrace facility");
MODULE_LICENSE("GPL");
