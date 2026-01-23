#include <stdio.h>

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        printf("ERROR: must call the target with exactly one string command line argument.\n");
        return 1;
    }

    // 62 possible characters
    if (argv[1][0] == (char)0xF4 && (unsigned char)argv[1][1] == 0x42)
        __builtin_trap(); //*((int *)0) = 15;

    printf("No dice!\n");
    return 0;
}
