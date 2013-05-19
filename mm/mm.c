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

PUBLIC int sys_fork()
{
	return 0;
}
