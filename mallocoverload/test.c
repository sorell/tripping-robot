#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    char *p = (char *) malloc(20);
    printf("p = %p\n", p);
    free(p);
    return 0;
}
