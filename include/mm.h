/*
 * mm.h
 *
 *  Created on: 2013-5-19
 *      Author: bear
 */

#ifndef MM_H_
#define MM_H_

PUBLIC int	sys_fork();
PUBLIC int sys_exec(const char* pathname, char * argv[]);
PUBLIC void init_mm();
PUBLIC int alloc_mem(int pid, int memsize);
PUBLIC void sys_goin(int pid);
PUBLIC int  sys_wait();
PUBLIC void sys_exit(int status);
#endif /* MM_H_ */
