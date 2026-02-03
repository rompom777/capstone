#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <time.h>
#include <string.h>

int main()
{
    // Open log file
    FILE *f = fopen("log.txt", "w");

    // Initialize randomizer with current time as seed
    srand(time(NULL));
    time_t start = time(NULL);

    // Run for length 3
    int iterations = 0;
    const int INPUT_LENGTH = 3;
    const int ITERATIONS = 1000000;

    // 3 character string input
    char input[INPUT_LENGTH + 1];

    // Repeat "ITERATIONS" number of times
    for (int i = 0; i < ITERATIONS; i++)
    {
        // Construct the input byte-by-byte
        for (int k = 0; k < INPUT_LENGTH; k++)
        {
            unsigned char b;
            unsigned int index = (rand() % 62);
            if (index < 10)
            {
                b = (unsigned char)(index + 48);
            }
            else if (index < 36)
            {
                b = (unsigned char)(index + 65);
            }
            else
            {
                b = (unsigned char)(index + 97);
            }
            input[k] = b;
        }
        input[INPUT_LENGTH] = '\0';

        iterations++;
        // Print to console every 1k iterations
        if (iterations % 1000 == 0)
        {
            printf("%d/%d\n", iterations, ITERATIONS);
            fprintf(f, "%d/%d\n", iterations, ITERATIONS);
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
            printf("Crashing input (length %d):\n", INPUT_LENGTH);
            for (int k = 0; k < INPUT_LENGTH; k++)
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
            fprintf(f, "Crashing input (length %d):\n", INPUT_LENGTH);
            for (int k = 0; k < INPUT_LENGTH; k++)
            {
                fprintf(f, "\\x%02X", (unsigned char)input[k]);
            }
            fprintf(f, "\n");
            fprintf(f, "Number of tests before crash: %d\n", iterations);
            fprintf(f, "Elapsed time: %.0f seconds\n", difftime(end, start));
            fclose(f);
            return 0; // STOP FUZZER
        }
    }

    printf("Crashing input not found.\n");
    fprintf(f, "Crashing input not found.\n");
    fclose(f);

    return 0;
}