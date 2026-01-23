#include "test_gen.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>

void fuzzer()
{
    // Initialize randomizer with current time as seed
    srand(time(NULL));

    // Run for lengths 1 - 64
    int iterations = 0;
    const int MAX_LENGTH = 128;
    const int ITERATIONS_PER_LENGTH = 100000;
    int total_tests = MAX_LENGTH * ITERATIONS_PER_LENGTH;

    for (int i = 1; i <= MAX_LENGTH; i++)
    {
        // 10,000 iterations for each length
        for (int j = 0; j < ITERATIONS_PER_LENGTH; j++)
        {
            // Current test case to input
            char input[i + 1];
            // Construct the input byte-by-byte
            for (int k = 0; k < i; k++)
            {
                unsigned char b = (unsigned char)(rand() % 256);
                while (b == 0x00 || b == 0x27)
                {
                    b = (unsigned char)(rand() % 256);
                }
                input[k] = b;
            }
            input[i] = '\0';
            iterations++;
            if (iterations % 1000 == 0)
            {
                printf("%d/%d\n", iterations, total_tests);
                fflush(stdout);
            }

            // Safe buffer writing
            char command[256];
            snprintf(command, sizeof(command), "./target '%s'", input);

            // Test the file and stop at crash
            int status = system(command);
            if (WIFSIGNALED(status))
            {
                int sig = WTERMSIG(status);
                printf("Crash detected! Signal: %d\n", sig);
                printf("Crashing input (length %d):\n", i);
                for (int k = 0; k < i; k++)
                {
                    printf("\\x%02X", (unsigned char)input[k]);
                }
                printf("\n");
                return; // STOP FUZZER
            }
        }
    }
    printf("Crashing input not found.\n");
}