/*
 * echo.c
 *
 *  Created on: 2013-5-21
 *      Author: bear
 */

#include "stdio.h"

int main(int argc, char * argv[])
{
	int i;


	printf("The First Application \n");
	for (i = 0; i < argc; i++)
				printf("%s%s", i == 0 ? "" : " ", argv[i]);
	printf("\n");
	while(1) ;
	return 0;
}
