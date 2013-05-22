/*
 * config.h
 *
 *  Created on: 2013-5-18
 *      Author: bear
 */

#ifndef CONFIG_H_
#define CONFIG_H_

#define ROOT_DEV	48

#define	BOOT_PARAM_ADDR			0x900  /* physical address */
#define	BOOT_PARAM_MAGIC		0xB007 /* magic number */
#define	BI_MAG				0
#define	BI_MEM_SIZE			1
#define	BI_KERNEL_FILE			2

#define	INSTALL_START_SECT		0x8000
#define	INSTALL_NR_SECTS		0x800

#endif /* CONFIG_H_ */
