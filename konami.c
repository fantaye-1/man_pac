/*
 * A kernel module -- TODO write what we do here
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/sched.h> // task_struct definition
#include <asm/unistd.h>
#include <linux/list.h>
#include <linux/init_task.h>

#ifndef __KERNEL__
#define __KERNEL__
#endif

// Initialization of module
int __init init_KonamiModule(void)
{
  printk("Konamo module initialized.\n");
  return 0;
}

// Exit of module
void __exit exit_KonamiModule(void)
{
  printk("Konami module exited.\n");
  return;
}

module_init(init_KonamiModule);
module_exit(exit_KonamiModule);

MODULE_LICENSE("MIT");
MODULE_DESCRIPTION("Up-Up-Down-Down-Left-Right-Left-Right-B-A-Enter, man pac");
