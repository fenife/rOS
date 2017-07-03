/* prog_no_arg.c
 *
 * 外部的用户程序，加载到系统中执行
 */

#include "stdio.h"

int main(void) 
{
    printf("prog_no_arg from disk\n"); 
    while(1)
        ;
    return 0;
}

