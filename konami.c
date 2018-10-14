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

// Don't do tail call optimizations, it breaks ftrace recursion handling
#pragma GCC optimize("-fno-optimize-sibling-calls")

/* Globals */

// Pointer to original exec system call handler.
// Signature based on <linux/syscalls.h>
static asmlinkage long
(*original_sys_execve)(const char __user *filename,
		const char __user *const __user *argv,
		const char __user *const __user *envp);

// ftrace options (includes handler)
static struct ftrace_ops execve_ops;

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
		return;

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

/* Module Setup */

// Initialization of module
int __init
init_KonamiModule(void)
{
	pr_debug("Konamo module initialized.\n");
	return 0;
}

// Exit of module
void __exit
exit_KonamiModule(void)
{
	pr_debug("Konami module exited.\n");
	return;
}

module_init(init_KonamiModule);
module_exit(exit_KonamiModule);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Up-Up-Down-Down-Left-Right-Left-Right-B-A-Enter, man pac");
