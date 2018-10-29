/*
 * A kernel module -- TODO write what we do here
 *
 * Licensed under GPLv3.
 * Copyright (c) 2018 Abrham Fantaye, Chase Colman, Youngsoo Kang.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 */

// Module headers
#include <linux/kernel.h>
#include <linux/module.h>

// Process list headers
#include <linux/sched.h> // task_struct definition
#include <linux/list.h>
#include <linux/init_task.h>

// System call headers
#include <linux/kallsyms.h> // kallsysms_lookup_name
#include <linux/linkage.h>

// Key combo headers
#include <linux/interrupt.h> // IRQ for keyboard intercept
#include <linux/workqueue.h>

// Don't do tail call optimizations, it breaks ftrace recursion handling
#pragma GCC optimize("-fno-optimize-sibling-calls")

/* Data types */

// Wrapper for work containing scancode
struct kb_work {
	struct work_struct work; // work
	unsigned char scancode; // key scancode
};

/* Globals */

// Pointer to original exec system call handler.
// Signature based on <linux/syscalls.h>
static asmlinkage long
(*original_sys_execve)(const char __user *filename,
		const char __user *const __user *argv,
		const char __user *const __user *envp);

// ftrace options (includes handler)
static struct ftrace_ops execve_ops;

// work queue for key combo detection
static struct workqueue_struct *wq_keyboard;

static unsigned char kb_pos; // position in combo sequence

/* Exec System Call Wrapper */

// Wrapped sys_execve replacement
static asmlinkage long
sys_execve(const char __user *filename,
		const char __user *const __user *argv,
		const char __user *const __user *envp)
{
	// Replace `man pac` execution calls
	if (strcmp("/usr/bin/man", filename) == 0 && strcmp("pac", argv[1]) == 0)
	{
		return original_sys_execve("/tmp/manpac", argv, envp);
	}

	// Not calling `man pac`, pass through to orginal
	return original_sys_execve(filename, argv, envp);
}

// ftrace callback handler
static void notrace
ftrace_handler(unsigned long ip, unsigned long parent_ip,
		struct ftrace_ops *op, struct pt_regs *regs)
{
	// Prevent infinite recursion by checking if the caller's instruction pointer
	// is within this module before re-assigning the instruction pointer to our
	// wrapped implementation.
	if (within_module(parent_ip, THIS_MODULE))
	{
		return;
	}

	// Use wrapped version
	regs->ip = (unsigned long)&sys_execve;
}

// Installs the exec syscall wrapper using ftrace.
// Returns 0 on success, non-zero otherwise.
int
install_exec_wrapper(void)
{
	int error;

	// Find the original exec syscall handler
	unsigned long handler_addr = kallsyms_lookup_name("sys_execve");
	if (!handler_addr)
	{
		pr_debug("could not resolve sys_execve's symbol");
		return ENOENT;
	}

	// Setup pointer to original handler
	*((unsigned long *)original_sys_execve) = handler_addr;

	// Setup ftrace
	execve_ops.func = ftrace_handler;
	execve_ops.flags = FTRACE_OPS_FL_SAVE_REGS | // Need to modify IP register
		FTRACE_OPS_FL_IPMODIFY | // Modifies IP register
		FTRACE_OPS_FL_RECURSION_SAFE; // Recursion checked in handler

	error = ftrace_set_filter_ip(&execve_ops, handler_addr, 0, 0);
	if (error)
	{
		pr_debug("i - ftrace_set_filter_ip returned error: %d\n", error);
		return error;
	}

	error = register_ftrace_function(&execve_ops);
	if (error)
	{
		pr_debug("i - register_ftrace_function returned error: %d\n", error);
		ftrace_set_filter_ip(&execve_ops, handler_addr, 1, 0);
		return error;
	}

	return 0;
}

// Restores the original exec syscall
void
restore_exec(void)
{
	int error;
	unsigned long handler_addr;

	error = unregister_ftrace_function(&execve_ops);
	if (error)
	{
		pr_debug("r - unregister_ftrace_function returned error: %d\n", error);
	}

	handler_addr = (unsigned long)&original_sys_execve;
	error = ftrace_set_filter_ip(&execve_ops, handler_addr, 1, 0);
	if (error)
	{
		pr_debug("r - ftrace_set_filter_ip returned error: %d\n", error);
	}
}

/* Keyboard Combo Launcher */

// Work queue worker that handles key combo detection.
static void
kb_combo_handler(struct work_struct *work)
{
	struct kb_work *kb = container_of(work, struct kb_work, work);
	if ((kb->scancode & 0x80) == 0)
	{
		return; // Ignore key press, we only need releases
	}
	// Detection is mapped to Up-Up-Down-Down-Left-Right-Left-Right-B-A-Enter
	// All credit goes to Abraham for the initial implementation!
	switch (kb->scancode)
	{
		case 0xc8: // Released Up Arrow
			kb_pos = (kb_pos == 0 || kb_pos == 1) ? kb_pos + 1 : 0;
			pr_debug("Up Arrow: %d\n", kb_pos);
			break;
		case 0xcb: // Released Left Arrow
			kb_pos = (kb_pos == 4 || kb_pos == 6) ? kb_pos + 1 : 0;
			pr_debug("Left Arrow: %d\n", kb_pos);
			break;
		case 0xcd: // Released Right Arrow
			kb_pos = (kb_pos == 5 || kb_pos == 7) ? kb_pos + 1 : 0;
			pr_debug("Right Arrow: %d\n", kb_pos);
			break;
		case 0xd0: // Released Down Arrow
			kb_pos = (kb_pos == 2 || kb_pos == 3) ? kb_pos + 1 : 0;
			pr_debug("Down Arrow: %d\n", kb_pos);
			break;
		case 0x9e: // Released A
			kb_pos = kb_pos == 9 ? kb_pos + 1 : 0;
			pr_debug("A: %d\n", kb_pos);
			break;
		case 0xb0: // Released B
			kb_pos = kb_pos == 8 ? kb_pos + 1 : 0;
			pr_debug("B: %d\n", kb_pos);
			break;
		case 0x9c: // Released Enter
			kb_pos = kb_pos == 10 ? kb_pos + 1 : 0;
			pr_debug("Enter: %d\n", kb_pos);
			break;
		case 0xe0: // Ignore auxilary key
			break;
		default:
			kb_pos = 0; // Reset on invalid key
	}

	if (kb_pos == 11)
	{
		pr_debug("Konami Code Entered!\n");
	}
}

// Proxies the keyboard IRQ to a work queue. This prevents the handler from
// blocking the keyboard drivers in critical sections but still allows in-order
// combo detection.
irqreturn_t
kb_irq_handler(int irq, void *arg)
{
	struct kb_work *kb;

	kb = kmalloc(sizeof(struct kb_work), GFP_KERNEL);
	kb->scancode = inb(0x60); // Keyboard scancode is at I/O port 0x60

	INIT_WORK(&kb->work, kb_combo_handler);
	queue_work(wq_keyboard, &kb->work);

	return IRQ_HANDLED;
}

/* Module Setup */

// Initialization of module
int __init
init_KonamiModule(void)
{
	int error;

	pr_debug("Konamo module initialized.\n");
	// Create a single-thread work queue to handle keyboard IRQs without blocking
	wq_keyboard = create_singlethread_workqueue("KonamiKBCombo2018");
	// Keyboard's IRQ on x86* is 1, using IRQF_SHARED doesn't block other
	// interrupts. Handle for the request is kb_pos's static pointer.
	error = request_irq(1, kb_irq_handler, IRQF_SHARED, "KonamiKBIRQ", &kb_pos);
	if (error)
	{
		pr_debug("could not allocate IRQ handler: %d\n", error);
	}
	return error;
}

// Exit of module
void __exit
exit_KonamiModule(void)
{
	// Process remaining items in keyboard IRQ handler queue and cleanup
	flush_workqueue(wq_keyboard);
	destroy_workqueue(wq_keyboard);
	// Keyboard's IRQ on x86* is 1, free the handle located by kb_pos
	free_irq(1, &kb_pos);
	pr_debug("Konami module exited.\n");
	return;
}

module_init(init_KonamiModule);
module_exit(exit_KonamiModule);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Up-Up-Down-Down-Left-Right-Left-Right-B-A-Enter, man pac");
