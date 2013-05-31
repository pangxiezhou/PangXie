
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
                            main.c
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
                                                    Forrest Yu, 2005
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

#include "type.h"
#include "const.h"
#include "protect.h"
#include "string.h"
#include "proc.h"
#include "tty.h"
#include "console.h"
#include "global.h"
#include "proto.h"
#include "fs.h"
#include "mm.h"

/*======================================================================*
                            kernel_main
 *======================================================================*/
PUBLIC int kernel_main()
{
	disp_str("-----\"kernel_main\" begins-----\n");

	TASK* p_task = task_table;
	PROCESS* p_proc = proc_table;
	char* p_task_stack = task_stack + STACK_SIZE_TOTAL - STACK_SIZE_INIT;
	//u16 selector_ldt = SELECTOR_LDT_FIRST;
	int i, j;
	u8 privilege;
	u8 rpl;
	int eflags;
	int ticks;
	for (i = 0; i < NR_TASKS + NR_PROCS; i++) {
		if (i >= NR_TASKS + NATIVE_PROCS) {
			p_proc->p_flags = FREE_SLOT;
			p_proc++;
			continue;
		}
		if (i < NR_TASKS) { /* 任务 */
			p_task = task_table + i;
			privilege = PRIVILEGE_TASK;
			rpl = RPL_TASK;
			eflags = 0x1202; /* IF=1, IOPL=1, bit 2 is always 1 */
			ticks = 15;
		} else { /* 用户进程 */
			p_task = user_proc_table + (i - NR_TASKS);
			privilege = PRIVILEGE_USER;
			rpl = RPL_USER;
			eflags = 0x202; /* IF=1, bit 2 is always 1 */
			ticks =5;
		}

		strcpy(p_proc->p_name, p_task->name);	// name of the process
		p_proc->pid = i;			// pid

		//p_proc->ldt_sel = selector_ldt;

		if (strcmp(p_proc->p_name, "INIT") != 0) {
			p_proc->ldts[INDEX_LDT_C] = gdt[SELECTOR_KERNEL_CS >> 3];
			p_proc->ldts[INDEX_LDT_RW] = gdt[SELECTOR_KERNEL_DS >> 3];

			/* change the DPLs */
			p_proc->ldts[INDEX_LDT_C].attr1 = DA_C | privilege << 5;
			p_proc->ldts[INDEX_LDT_RW].attr1 = DA_DRW | privilege << 5;
		} else { /* INIT process */
			unsigned int k_base;
			unsigned int k_limit;
			int ret = get_kernel_map(&k_base, &k_limit);

			//assert(ret == 0);
			init_descriptor(&p_proc->ldts[INDEX_LDT_C], 0, /* bytes before the entry point
			 * are useless (wasted) for the
			 * INIT process, doesn't matter
			 */
			(k_base + k_limit) >> LIMIT_4K_SHIFT,
					DA_32 | DA_LIMIT_4K | DA_C | privilege << 5);

			init_descriptor(&p_proc->ldts[INDEX_LDT_RW], 0, /* bytes before the entry point
			 * are useless (wasted) for the
			 * INIT process, doesn't matter
			 */
			(k_base + k_limit) >> LIMIT_4K_SHIFT,
					DA_32 | DA_LIMIT_4K | DA_DRW | privilege << 5);
		}
		p_proc->regs.cs = (0 & SA_RPL_MASK & SA_TI_MASK) | SA_TIL | rpl;
		p_proc->regs.ds = (8 & SA_RPL_MASK & SA_TI_MASK) | SA_TIL | rpl;
		p_proc->regs.es = (8 & SA_RPL_MASK & SA_TI_MASK) | SA_TIL | rpl;
		p_proc->regs.fs = (8 & SA_RPL_MASK & SA_TI_MASK) | SA_TIL | rpl;
		p_proc->regs.ss = (8 & SA_RPL_MASK & SA_TI_MASK) | SA_TIL | rpl;
		p_proc->regs.gs = (SELECTOR_KERNEL_GS & SA_RPL_MASK) | rpl;

		p_proc->regs.eip = (u32) p_task->initial_eip;
		p_proc->regs.esp = (u32) p_task_stack;
		p_proc->regs.eflags = eflags;
		p_proc->ticks = p_proc->priority =	ticks;
		p_proc->nr_tty = 0;
		p_proc->p_flags = 0;
		p_task_stack -= p_task->stacksize;
		p_proc++;
		p_task++;
		//p_proc->ticks = p_proc->priority = ticks;
		//selector_ldt += 1 << 3;
		//filesystem relate structure initialize
		for (j = 0; j < NR_FILES; j++)
			p_proc->filp[j] = 0;
	}

	/*proc_table[0].ticks = proc_table[0].priority = 15;
	proc_table[1].ticks = proc_table[1].priority = 5;
	proc_table[2].ticks = proc_table[2].priority = 5;
	proc_table[3].ticks = proc_table[3].priority = 5;
	proc_table[4].ticks = proc_table[4].priority = 5;*/

	/*proc_table[1].nr_tty = 0;
	proc_table[2].nr_tty = 0;
	proc_table[3].nr_tty = 0;
	proc_table[4].nr_tty = 0;*/

	k_reenter = 0;
	ticks = 0;

	p_proc_ready = proc_table;

	init_clock();
	//init_keyboard();
	init_tty();
	init_hd();
	init_fs();
	init_mm();
	restart();

	while (1) {
	}
}

/*======================================================================*
                               TestA
 *======================================================================*/
void TestA()
{
	/*int i = 0;
	int ret;
	int fd = open("/Test2",O_CREAT|O_RDWR);
	printf("process open file success %d \n",fd);
	u8 wbuf[5]={1,2,3,4,5};
	u8 rbuf[5];
	ret = writef(fd, (void*)wbuf,5);
	printf("Process write file Success %d\n", ret);
	for(i=0;i<5;i++)
			printf("%d",wbuf[i]);
		printf("\n");
	ret = close(fd);
	fd = open("/Test2",O_RDWR);
	ret = read(fd, rbuf, 5);
	printf("Process read file succcess %d\n",ret);
	for(i=0;i<5;i++)
		printf("%d",rbuf[i]);
	printf("\n");
	ret = close(fd);
	printf("Process close file success %d \n", ret);

	ret = deletef("/Test2");
	printf("Process delete file suceess %d \n",ret);*/
	while (1) {
		//printf("A");
		milli_delay(200);
	}
}
void Init()
{
	printf("Init Process \n");

	//printf("hehehe %d \n", 1);
	int pid = fork();

	//goin(pid);
	if(pid){
		printf("This parent \n");
		int exitStatus = bearwait();
		printf("wait Child exit status %d \n",exitStatus);
	}else
	{
		char* argv[2]={"Hello","World"};
		printf("This Child  \n");
		//bearexec("/echo",argv);
		bearexit(1);
	}
	//printf("Test before \n");
	//printf("Process pid   \n");
	//printf("Test After \n");
	/*if(pid){
		printf("This is Parent Process \n");
	}else{
		printf("This is Child Process  \n");
	}*/
	while(1){
			//printf("init \n");
			milli_delay(200);
		}
}
/*======================================================================*
                               TestB
 *======================================================================*/
void TestB()
{
	int i = 0x1000;
	while(1){
		//printf("B");
		milli_delay(200);
	}
}

/*======================================================================*
                               TestB
 *======================================================================*/
void TestC()
{
	int i = 0x2000;
	while(1){
		//printf("C");
		milli_delay(200);
	}
}
