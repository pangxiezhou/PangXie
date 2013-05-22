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


PUBLIC void sys_exec(const char* pathname)
{

	const char* inPathname = va2la(p_proc_ready->pid, (void*)pathname);
	printl("pathname %s  \n", inPathname);
	int src = p_proc_ready->pid;
	kread(inPathname, mmbuf, 0, 8000);//just for echo fixme
	printl(" file read success \n");
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

	printl("echo Entry %x \n",  elf_hdr->e_entry);
	proc_table[src].regs.eip = elf_hdr->e_entry; /* @see _start.asm */
	proc_table[src].regs.esp = PROC_IMAGE_SIZE_DEFAULT - PROC_ORIGIN_STACK;

	strcpy(proc_table[src].p_name, pathname);

}
