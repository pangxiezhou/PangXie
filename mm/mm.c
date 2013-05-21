/*
 * mm.c
 *
 *  Created on: 2013-5-19
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

PUBLIC void init_mm()
{
	struct boot_params bp;
	get_boot_params(&bp);

	memory_size = bp.mem_size;

		/* print memory size */
	printl("{MM} memsize:%dMB\n", memory_size / (1024 * 1024));

}



PUBLIC int alloc_mem(int pid, int memsize)
{
	//assert(pid >= (NR_TASKS + NATIVE_PROCS));
	if (memsize > PROC_IMAGE_SIZE_DEFAULT) {
		printl("unsupported memory request: %d. "
		      "(should be less than %d)",
		      memsize,
		      PROC_IMAGE_SIZE_DEFAULT);
	}

	int base = PROCS_BASE +
		(pid - (NR_TASKS + NATIVE_PROCS)) * PROC_IMAGE_SIZE_DEFAULT;

	if (base + memsize >= memory_size)
		printl("memory allocation failed. pid:%d", pid);

	return base;
}
