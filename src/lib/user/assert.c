/* assert.c
 */

#include <assert.h>
#include <stdio.h>

void user_spin(char *filename, int line, const char *func_name,
                    const char *cond)
{
    printf("\n\n----------- user prog error! -----------\n");

    printf( "filename:  %s\n"
            "line:      %d\n"
            "function:  %s\n"
            "condition: %s\n",
            filename, line, func_name, cond);

    while(1)
        ;
}

