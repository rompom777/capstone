#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/wait.h>
#include <time.h>
#include <string.h>

#define INPUT_LENGTH 3
#define MAX_SEEDS 100
#define COVERAGE_SIZE 65536

struct Seed
{
    char seed_buffer[INPUT_LENGTH + 1];
};

struct Seed seed_pool[MAX_SEEDS];
int num_seeds = 0;
uint32_t coverage_map[COVERAGE_SIZE];

char generate_char()
{
    unsigned char b;
    unsigned int index = (rand() % 62);
    if (index < 10)
    {
        return (unsigned char)(index + 48);
    }
    else if (index < 36)
    {
        return (unsigned char)(index + 55);
    }
    else
    {
        return (unsigned char)(index + 61);
    }
}

void generate_input(char *input_buffer)
{
    for (int k = 0; k < INPUT_LENGTH; k++)
    {
        input_buffer[k] = generate_char();
    }
    input_buffer[INPUT_LENGTH] = '\0';
}

void mutate_seed(char *new_input, const char *src)
{
    strcpy(new_input, src);
    new_input[rand() % INPUT_LENGTH] = generate_char();
}

bool update_coverage()
{
    FILE *f = fopen("coverage.data", "rb");

    bool coverage_flag = false;
    uint32_t edge_id;

    while (fread(&edge_id, sizeof(uint32_t), 1, f) == 1)
    {
        if (edge_id < COVERAGE_SIZE)
        {
            if (coverage_map[edge_id] == 0)
            {
                coverage_map[edge_id] = 1;
                coverage_flag = true;
            }
        }
    }

    fclose(f);
    return coverage_flag;
}

int main()
{
    // Open log file
    FILE *f = fopen("log.txt", "w");

    // Initialize randomizer with current time as seed
    srand(time(NULL));
    time_t start = time(NULL);

    int iterations = 0;

    // 3 character string input
    char input[INPUT_LENGTH + 1];

    while (true)
    {
        if (num_seeds == 0)
        {
            generate_input(input);
        }
        else
        {
            int random_seed = rand() % num_seeds;
            mutate_seed(input, seed_pool[random_seed].seed_buffer);
        }
        iterations++;
        // Print to console every 1k iterations
        if (iterations % 1000 == 0)
        {
            printf("Iterations: %d Seeds: %d\n", iterations, num_seeds);
            fprintf(f, "Iterations: %d Seeds: %d\n", iterations);
            fflush(stdout);
            fflush(f);
        }

        // Safe buffer writing
        char command[256];
        snprintf(command, sizeof(command), "./target '%s'", input);
        unlink("coverage.data");

        // Test the file and stop at crash
        int status = system(command);
        if (WIFSIGNALED(status) || WEXITSTATUS(status) >= 128)
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

        if (update_coverage())
        {
            if (num_seeds < MAX_SEEDS)
            {
                strcpy(seed_pool[num_seeds].seed_buffer, input);
                num_seeds++;
            }
        }
    }

    printf("Crashing input not found.\n");
    fprintf(f, "Crashing input not found.\n");
    fclose(f);

    return 0;
}