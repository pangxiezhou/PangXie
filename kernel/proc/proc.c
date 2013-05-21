
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
                               proc.c
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
                                                    Forrest Yu, 2005
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

#include "type.h"
#include "const.h"
#include "protect.h"
#include "tty.h"
#include "console.h"
#include "string.h"
#include "proc.h"
#include "global.h"
#include "proto.h"

/*======================================================================*
                              schedule
 *======================================================================*/
PUBLIC void schedule()
{
	PROCESS* p;
	int	 greatest_ticks = 0;

	while (!greatest_ticks) {
		for (p = proc_table; p < proc_table+NR_TASKS+NR_PROCS; p++) {
			if (p->p_flags == 0) {
				if (p->ticks > greatest_ticks) {
					greatest_ticks = p->ticks;
					p_proc_ready = p;
				}
				//printl("Greatest Ticks %d \n",greatest_ticks);
			}
		}
		//printl("process Ready Name %s  eax :%d  Ldt Sele %d\n", p_proc_ready->p_name,p_proc_ready->regs.eax
		//		,p_proc_ready->ldt_sel);
		if (!greatest_ticks) {
			for(p=proc_table;p<proc_table+NR_TASKS+NR_PROCS;p++) {
				p->ticks = p->priority;
			}
		}
	}
}

/*======================================================================*
                           sys_get_ticks
 *======================================================================*/
PUBLIC int sys_get_ticks()
{
	return ticks;
}


PUBLIC int ldt_seg_linear(struct proc* p, int idx)
{
	DESCRIPTOR* d = &p->ldts[idx];

	return d->base_high << 24 | d->base_mid << 16 | d->base_low;
}

PUBLIC void* va2la(int pid, void* va)
{
	struct proc* p = &proc_table[pid];
	//printl("trans Pid %d \n",pid);
	u32 seg_base = ldt_seg_linear(p, INDEX_LDT_RW);
	//printl("segment Base %d, Virtual Addr %d \n",seg_base,va);
	u32 la = seg_base + (u32)va;
	return (void*)la;
}

