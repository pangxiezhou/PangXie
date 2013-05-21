/*
 * mm.h
 *
 *  Created on: 2013-5-19
 *      Author: bear
 */

#ifndef MM_H_
#define MM_H_

PUBLIC int	sys_fork();
PUBLIC void init_mm();
PUBLIC int alloc_mem(int pid, int memsize);
PUBLIC void sys_goin(int pid);
#endif /* MM_H_ */
