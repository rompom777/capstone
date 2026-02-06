#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/wait.h>
#include <time.h>
#include <string.h>
#include <unistd.h>

#define INPUT_LENGTH 3
#define MAX_SEEDS 100
#define COVERAGE_SIZE 65536

struct Seed
{
    char seed_buffer[INPUT_LENGTH + 1];
};

struct Seed seed_pool[MAX_SEEDS];
int num_seeds = 0;
uint8_t *edge_counters = NULL;
size_t edge_size = 0;

char generate_char()
{
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

uint8_t *read_coverage_file_inplace(const char *filename, uint8_t *buffer, size_t expected_size) {
    FILE *f = fopen(filename, "rb");
    if (!f) {
        perror("Failed to open coverage file");
        return NULL;
    }
    
    // Read size header
    uint64_t size;
    if (fread(&size, sizeof(uint64_t), 1, f) != 1) {
        fprintf(stderr, "Failed to read size header\n");
        fclose(f);
        return NULL;
    }
    
    // Verify size matches (should be constant across runs)
    if (expected_size > 0 && size != expected_size) {
        fprintf(stderr, "Size mismatch: expected %zu, got %lu\n", expected_size, size);
        fclose(f);
        return NULL;
    }
    
    // Read directly into provided buffer
    size_t bytes_read = fread(buffer, 1, size, f);
    
    if (bytes_read != size) {
        fprintf(stderr, "Short read: expected %lu, got %zu\n", size, bytes_read);
        fclose(f);
        return NULL;
    }
    
    fclose(f);
    return buffer;  // Success
}

bool update_coverage()
{
    //buffer for edge counters for new run
    static uint8_t temp_buffer[COVERAGE_SIZE];  
    
    uint8_t *new_edge_counters = read_coverage_file_inplace("coverage.data", temp_buffer, edge_size);
    
    if (!new_edge_counters) {
        fprintf(stderr, "Error: failed to read coverage.data\n");
        return false;
    }
    
    // first initialization of edge_counters
    if (edge_counters == NULL) {
        size_t size;
        FILE *f = fopen("coverage.data", "rb");
        fread(&size, sizeof(uint64_t), 1, f);
        fclose(f);
        
        edge_counters = malloc(size);
        edge_size = size;
        memcpy(edge_counters, temp_buffer, size);
        return true;
    }
    
    // update edge counters to max count for each edge
    bool coverage_flag = false;
    for (size_t i = 0; i < edge_size; i++) {
        if (new_edge_counters[i] > edge_counters[i]) {
            edge_counters[i] = new_edge_counters[i];
            coverage_flag = true;
        }
    }
    
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
            fprintf(f, "Iterations: %d Seeds: %d\n", iterations, num_seeds);
            fflush(stdout);
            fflush(f);
        }

        pid_t pid = fork();
        
        if (pid == -1)
        {
            perror("fork failed");
            continue;
        }
        
        if (pid == 0)
        {
            char *args[] = {"./target", input, NULL};
            execve("./target", args, NULL);
            
            perror("execve failed");
            _exit(1);
        }

        int status;
        waitpid(pid, &status, 0);

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
        unlink("coverage.data");
    }

    printf("Crashing input not found.\n");
    fprintf(f, "Crashing input not found.\n");
    fclose(f);

    if (edge_counters != NULL) {
        free(edge_counters);
    }

    return 0;
}