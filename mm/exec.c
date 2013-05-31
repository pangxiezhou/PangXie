/*
 * exec.c
 *
 *  Created on: 2013-5-22
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
#include "fs.h"
#include "mm.h"
#include "elf.h"


PUBLIC int sys_exec(const char* pathname, char * argv[])
{

	const char* inPathname = va2la(p_proc_ready->pid, (void*)pathname);
	char** p = argv = va2la(p_proc_ready->pid, (void*)argv);


	//prepare for process stack
	char arg_stack[PROC_ORIGIN_STACK];
	int stack_len = 0;

	while(*p++) {
		//assert(stack_len + 2 * sizeof(char*) < PROC_ORIGIN_STACK);
		stack_len += sizeof(char*);
	}

	*((int*)(&arg_stack[stack_len])) = 0;
	stack_len += sizeof(char*);
	//printl("Stack lenght %d \n", stack_len);
	char ** q = (char**)arg_stack;
	for (p = argv; *p != 0; p++) {
		*q++ = &arg_stack[stack_len];
		//printl("before Trans Paramerter addr %d \n",&arg_stack[stack_len] );
		//assert(stack_len + strlen(*p) + 1 < PROC_ORIGIN_STACK);
		strcpy(&arg_stack[stack_len], *p);
		stack_len += strlen(*p);
		arg_stack[stack_len] = 0;
		stack_len++;
	}


	//printl("pathname %s  \n", inPathname);
	int src = p_proc_ready->pid;
	kread(inPathname, mmbuf, 0, 10000);//just for echo fixme
	//printl(" file read success \n");
	Elf32_Ehdr* elf_hdr = (Elf32_Ehdr*)(mmbuf);
	int i;
	for (i = 0; i < elf_hdr->e_phnum; i++) {
		Elf32_Phdr* prog_hdr = (Elf32_Phdr*)(mmbuf + elf_hdr->e_phoff +
						(i * elf_hdr->e_phentsize));
		if (prog_hdr->p_type == PT_LOAD) {
			/*assert(prog_hdr->p_vaddr + prog_hdr->p_memsz <
				PROC_IMAGE_SIZE_DEFAULT);*/
			phys_copy((void*)va2la(src, (void*)prog_hdr->p_vaddr),
				  (void*)(mmbuf + prog_hdr->p_offset),
				  prog_hdr->p_filesz);
		}
	}
	u8 * orig_stack = (u8*)(PROC_IMAGE_SIZE_DEFAULT - PROC_ORIGIN_STACK);
	int delta = (int)orig_stack - (int)arg_stack;

	int argc = 0;
	if (stack_len) {	/* has args */
			char **q = (char**)arg_stack;
			for (; *q != 0; q++,argc++){
				//printl()
				*q += delta;
				//printl("Parameter %d addr %d \n",argc,*q);
			}

		}
	//printl("src pid %d  push eax  %d \n",src,(u32)orig_stack);
	phys_copy((void*)va2la(src, orig_stack),
			  (void*)(arg_stack),
			  stack_len);
	proc_table[src].regs.ecx = argc; /* argc */
	proc_table[src].regs.eax = (u32)orig_stack; /* argv */

	//printl("echo Entry %x \n",  elf_hdr->e_entry);
	proc_table[src].regs.eip = elf_hdr->e_entry; /* @see _start.asm */
	proc_table[src].regs.esp = PROC_IMAGE_SIZE_DEFAULT - PROC_ORIGIN_STACK;

	strcpy(proc_table[src].p_name, pathname);
	return (u32)orig_stack;

}
