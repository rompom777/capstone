#include "test_gen.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>

void fuzzer()
{
    // Open log file
    FILE *f = fopen("log.txt", "w");

    // Initialize randomizer with current time as seed
    srand(time(NULL));
    time_t start = time(NULL);

    // Run for lengths 1 - 128
    int iterations = 0;
    const int MAX_LENGTH = 128;
    const int ITERATIONS_PER_LENGTH = 1000000;
    int total_tests = MAX_LENGTH * ITERATIONS_PER_LENGTH;

    // Randomized length testing instead of in-order progression
    for (int i = 0; i < total_tests; i++)
    {
        int curr_size = 1 + (rand() % MAX_LENGTH);
        // Current test case to input
        char input[curr_size + 1];

        // Construct the input byte-by-byte
        for (int k = 0; k < curr_size; k++)
        {
            unsigned char b = (unsigned char)(rand() % 256);
            while (b == 0x00 || b == 0x27)
            {
                b = (unsigned char)(rand() % 256);
            }
            input[k] = b;
        }
        input[curr_size] = '\0';

        iterations++;
        // Print to console every 10k iterations
        if (iterations % 10000 == 0)
        {
            printf("%d/%d\n", iterations, total_tests);
            fprintf(f, "%d/%d\n", iterations, total_tests);
            fflush(stdout);
            fflush(f);
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
            printf("Crashing input (length %d):\n", curr_size);
            for (int k = 0; k < curr_size; k++)
            {
                printf("\\x%02X", (unsigned char)input[k]);
            }
            printf("\n");
            printf("Number of tests before crash: %d\n", iterations);
            time_t end = time(NULL);

            printf("Elapsed time: %.0f seconds\n", difftime(end, start));

            // Write summary to log
            // FILE *f = fopen("log.txt", "w");
            fprintf(f, "Crash detected! Signal: %d\n", sig);
            fprintf(f, "Crashing input (length %d):\n", curr_size);
            for (int k = 0; k < curr_size; k++)
            {
                fprintf(f, "\\x%02X", (unsigned char)input[k]);
            }
            fprintf(f, "\n");
            fprintf(f, "Number of tests before crash: %d\n", iterations);
            fprintf(f, "Elapsed time: %.0f seconds\n", difftime(end, start));
            fclose(f);
            return; // STOP FUZZER
        }
    }

    // for (int i = 1; i <= MAX_LENGTH; i++)
    // {
    //     // 10,000 iterations for each length
    //     for (int j = 0; j < ITERATIONS_PER_LENGTH; j++)
    //     {
    //         // Current test case to input
    //         char input[i + 1];
    //         // Construct the input byte-by-byte
    //         for (int k = 0; k < i; k++)
    //         {
    //             unsigned char b = (unsigned char)(rand() % 256);
    //             while (b == 0x00 || b == 0x27)
    //             {
    //                 b = (unsigned char)(rand() % 256);
    //             }
    //             input[k] = b;
    //         }
    //         input[i] = '\0';
    //         iterations++;
    //         // Print to console every 10k iterations
    //         if (iterations % 10000 == 0)
    //         {
    //             printf("%d/%d\n", iterations, total_tests);
    //             fprintf(f, "%d/%d\n", iterations, total_tests);
    //             fflush(stdout);
    //             fflush(f);
    //         }

    //         // Safe buffer writing
    //         char command[256];
    //         snprintf(command, sizeof(command), "./target '%s'", input);

    //         // Test the file and stop at crash
    //         int status = system(command);
    //         if (WIFSIGNALED(status))
    //         {
    //             int sig = WTERMSIG(status);
    //             printf("Crash detected! Signal: %d\n", sig);
    //             printf("Crashing input (length %d):\n", i);
    //             for (int k = 0; k < i; k++)
    //             {
    //                 printf("\\x%02X", (unsigned char)input[k]);
    //             }
    //             printf("\n");
    //             printf("Number of tests before crash: %d\n", iterations);
    //             time_t end = time(NULL);

    //             printf("Elapsed time: %.0f seconds\n", difftime(end, start));

    //             // Write summary to log
    //             // FILE *f = fopen("log.txt", "w");
    //             fprintf(f, "Crash detected! Signal: %d\n", sig);
    //             fprintf(f, "Crashing input (length %d):\n", i);
    //             for (int k = 0; k < i; k++)
    //             {
    //                 fprintf(f, "\\x%02X", (unsigned char)input[k]);
    //             }
    //             fprintf(f, "\n");
    //             fprintf(f, "Number of tests before crash: %d\n", iterations);
    //             fprintf(f, "Elapsed time: %.0f seconds\n", difftime(end, start));
    //             fclose(f);
    //             return; // STOP FUZZER
    //         }
    //     }
    // }
    // Write to console and log that testing was unsuccessful
    // FILE *f = fopen("log.txt", "w");
    printf("Crashing input not found.\n");
    fprintf(f, "Crashing input not found.\n");
    fclose(f);
}