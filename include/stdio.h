/*
 * stdio.h
 *
 *  Created on: 2013-5-21
 *      Author: bear
 */

#ifndef STDIO_H_
#define STDIO_H_
#include "type.h"
#define	O_CREAT		1
#define	O_RDWR		2
int     printf(const char *fmt, ...);
int     vsprintf(char *buf, const char *fmt, va_list args);
int	sprintf(char *buf, const char *fmt, ...);
int open(const char* pathname, int flags);
 int close(int fd);
 int writef(int fd, void* buf, int len);
int read(int fd, void* buf, int len);
int  deletef(const char* pathname);
int	fork		();

#endif /* STDIO_H_ */
