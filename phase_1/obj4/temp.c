#include <stdio.h>
#include <stdlib.h>

int main()
{
    system("strings target -n 3 -a -e s > temp.txt");
    FILE *f = fopen("temp.txt", "r");

    fclose(f);
}