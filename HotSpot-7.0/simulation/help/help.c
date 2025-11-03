
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

int main(int argc, char **argv)
{
    printf("%d", argc);
    for (int fk = 0; fk < sizeof(argv) / sizeof(argv[0]); fk++)
    {
        printf("%s", argv[fk]);
    }
    printf("\n");
}