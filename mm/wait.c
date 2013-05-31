/*
 * wait.c
 *
 *  Created on: 2013-5-31
 *      Author: bear
 */

#include "type.h"
#include "config.h"
#include "const.h"
#include "protect.h"
#include "string.h"
#include "proc.h"
#include "tty.h"
#include "console.h"
#include "global.h"
#include "proto.h"
#include "mm.h"
PRIVATE void cleanup(struct proc * proc);
PUBLIC int  sys_wait()
{
	int pid = p_proc_ready->pid;

	int i;
	int children = 0;
	struct proc* p_proc = proc_table;
	for (i = 0; i < NR_TASKS + NR_PROCS; i++,p_proc++) {
		if (p_proc->p_parent == pid) {
			children++;
			//printl("Find a Chile Process \n");
			if (p_proc->p_flags & HANGING) {
				cleanup(p_proc);
				//printl("Return to Process child's Exit_status And This Process flags is %d\n",p_proc_ready->p_flags);
				//printl("Exit Status %d \n",p_proc->exit_status);
				return p_proc->exit_status;
			}
		}
	}

	if (children) {
		/* has children, but no child is HANGING */
		proc_table[pid].p_flags |= WAITING;
	}
	schedule();
	return 0;// never go here
}
PUBLIC void sys_exit(int status)
{
	int i;
	int pid = p_proc_ready->pid; /* PID of caller */
	int parent_pid = proc_table[pid].p_parent;
	struct proc * p = &proc_table[pid];
	//printl("free pid %d \n",pid);
	/* tell FS, see fs_exit() */
	free_mem(pid);

	p->exit_status = status;

	if (proc_table[parent_pid].p_flags & WAITING) { /* parent is waiting */
		proc_table[parent_pid].p_flags &= ~WAITING;
		proc_table[parent_pid].regs.eax = p->exit_status;
		cleanup(&proc_table[pid]);
	}
	else { /* parent is not waiting */
		//printl("Parent is not Waiting \n");
		proc_table[pid].p_flags |= HANGING;
	}
	schedule();

	/* if the proc has any child, make INIT the new parent */
/*	for (i = 0; i < NR_TASKS + NR_PROCS; i++) {
		if (proc_table[i].p_parent == pid) {  is a child
			proc_table[i].p_parent = INIT;
			if ((proc_table[INIT].p_flags & WAITING) &&
			    (proc_table[i].p_flags & HANGING)) {
				proc_table[INIT].p_flags &= ~WAITING;
				cleanup(&proc_table[i]);
			}
		}
	}*/
}

PRIVATE void cleanup(struct proc * proc)
{
	proc->p_flags = FREE_SLOT;
}
