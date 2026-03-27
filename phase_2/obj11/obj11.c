#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/wait.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>

#define INPUT_LENGTH 16 * 1024
#define MAX_SEEDS 1000
#define COVERAGE_SIZE 65536
#define RAND_BYTE() (uint8_t)(rand() % 256)

extern int __real_main(int argc, char **argv);
extern void start_target_coverage(void);
extern void stop_target_coverage(void);

static char input_file_path[256];
static char coverage_file_path[256];
static char crash_output_path[256];

struct Seed
{
    uint8_t seed_buffer[INPUT_LENGTH + 1];
    size_t size;
};

struct Seed seed_pool[MAX_SEEDS];
int num_seeds = 0;
uint8_t *edge_counters = NULL;
size_t edge_size = 0;

// char generate_char()
// {
//     unsigned int index = (rand() % 62);
//     if (index < 10)
//     {
//         return (unsigned char)(index + 48);
//     }
//     else if (index < 36)
//     {
//         return (unsigned char)(index + 55);
//     }
//     else
//     {
//         return (unsigned char)(index + 61);
//     }
// }

void generate_input(uint8_t *input_buffer, size_t *size)
{
    *size = rand() % INPUT_LENGTH;
    for (size_t k = 0; k < *size; k++)
    {
        input_buffer[k] = RAND_BYTE();
    }
}

void input_to_file(const char *filename, const uint8_t *input, size_t num_bytes){
    FILE *f = fopen(filename, "wb");
    if (!f) {
        perror("Failed to open input file");
        exit(1);
    }
    fwrite(input, 1, num_bytes, f);
    fclose(f);

}

void mutate_seed(uint8_t *new_input, const uint8_t *src, size_t size)
{
    memcpy(new_input, src, size);
    new_input[rand() % size] = RAND_BYTE();
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

bool update_coverage(int *hit_count)
{
    //buffer for edge counters for new run
    static uint8_t temp_buffer[COVERAGE_SIZE]; 
    
    uint8_t *new_edge_counters = read_coverage_file_inplace(coverage_file_path, temp_buffer, edge_size);
        
    if (!new_edge_counters) {
        fprintf(stderr, "Error: failed to read coverage.data\n");
        return false;
    }
    
    // first initialization of edge_counters
    if (edge_counters == NULL) {
        size_t size;
        FILE *f = fopen(coverage_file_path, "rb");
        fread(&size, sizeof(uint64_t), 1, f);
        fclose(f);
        
        edge_counters = malloc(size);
        edge_size = size;
        memcpy(edge_counters, temp_buffer, size);
        return true;
    }
    
    // update edge counters to max count for each edge
    bool coverage_flag = false;
    int edges_hit = 0;
    for (size_t i = 0; i < edge_size; i++) {
        if (new_edge_counters[i] > edge_counters[i]) {
            edge_counters[i] = new_edge_counters[i];
            coverage_flag = true;
        }
        if (new_edge_counters[i] > 0) {
            edges_hit++;
        }
    }
    *hit_count = edges_hit;
    return coverage_flag;
}

void save_to_intresting(const uint8_t *input, int seed_num, size_t size){
    char filename[256];
    snprintf(filename, sizeof(filename), "interesting/seed_%d.pdf", seed_num);

    FILE *f = fopen(filename, "wb");
    if (f){
        fwrite(input, 1, size, f);
        fclose(f);
    }
}

int run_target(const uint8_t *input, size_t size) {
    
    input_to_file(input_file_path, input, size);
    //unlink(coverage_file_path);

    pid_t child = fork();
    if (child < 0)
    {
        perror("fork failed");
        exit(1);
    }

    if(child==0){
        freopen("/dev/null", "w", stderr);
        char *argv[] = {"pdftotext", input_file_path, NULL};
        start_target_coverage();
        int result = __real_main(2, argv);
        stop_target_coverage();  
        exit(result);
    }

    //parent waits for child to finish
    int status;
    if(waitpid(child, &status, 0) < 0){
        perror("waitpid failed");
        exit(1);
    }

    return status;
}

void load_seeds() {
    DIR *d;
    struct dirent *dir;
    d = opendir("seeds");
    if (!d) {
        printf("No seeds in directory.\n");
        return;
    }
    
    while ((dir = readdir(d)) != NULL) {
        if (dir->d_type == DT_REG && strstr(dir->d_name, ".pdf")) {
            char filepath[512];
            snprintf(filepath, sizeof(filepath), "%s/%s", "seeds", dir->d_name);
            
            FILE *f = fopen(filepath, "rb");
            if (f) {
                // Get file size
                fseek(f, 0, SEEK_END);
                long fsize = ftell(f);
                fseek(f, 0, SEEK_SET);
                
                // Check if file fits in our buffer
                if (fsize > 0 && fsize < INPUT_LENGTH) {
                    uint8_t buffer[INPUT_LENGTH];
                    size_t bytes_read = fread(buffer, 1, fsize, f);
                    
                    if (bytes_read == fsize) {
                        run_target(buffer, bytes_read); 
                        int hit_count;
                        if (update_coverage(&hit_count)) {
                            if (num_seeds < MAX_SEEDS) {
                                memcpy(seed_pool[num_seeds].seed_buffer, buffer, bytes_read);
                                seed_pool[num_seeds].size = bytes_read;
                                num_seeds++;
                                printf("Loaded seed: %s (%ld bytes)\n", dir->d_name, fsize);
                            }
                        }
                    }
                }
                fclose(f);
            }
        }
    }
    closedir(d);
    printf("Loaded %d seeds\n", num_seeds);
}

int fuzzer_main(void)
{
    char tmpdir[256];
    snprintf(tmpdir, sizeof(tmpdir), "/tmp/%d", getpid());

    if(mkdir(tmpdir,0755) != 0){
        exit(1);
    }

    snprintf(input_file_path, sizeof(input_file_path), "%s/exec_input.pdf", tmpdir);
    snprintf(coverage_file_path, sizeof(coverage_file_path), "%s/coverage.data", tmpdir);
    snprintf(crash_output_path,  sizeof(crash_output_path),  "%s/crashing_output.pdf", tmpdir);

    setenv("FUZZER_TMP", tmpdir, 1);
    printf("RAM disk working directory: %s\n", tmpdir);

    // Open log file
    FILE *f = fopen("log.txt", "w");

    // Initialize randomizer with current time as seed
    srand(time(NULL));
    time_t start = time(NULL);
    time_t last_checkpoint = start;   

    load_seeds();
    int iterations = 0;

    // 3 character string input
    uint8_t input[INPUT_LENGTH + 1];
    size_t input_size;
    size_t interesting_cases = 0;
    float avg_edges_covered = 0;

    while (true)
    {
        if (num_seeds == 0)
        {
            generate_input(input, &input_size);
        }
        else
        {
            int random_seed = rand() % num_seeds;
            input_size = seed_pool[random_seed].size;
            mutate_seed(input, seed_pool[random_seed].seed_buffer, input_size);
        }
        iterations++;
        // Print to console every 1k iterations
        if (iterations % 1000 == 0)
        {
            time_t now = time(NULL);
            double elapsed = difftime(now, last_checkpoint);
            double iters_per_sec = (elapsed > 0) ? 1000.0 / elapsed : 0;
            last_checkpoint = now;

            int total_cov = 0;
            int unique_edges = 0;

            for(size_t i = 0; i < edge_size; i++) {
                total_cov += (int)(edge_counters[i]);
            }

            for(size_t i=0; i < edge_size; i++){
                if(edge_counters[i] > 0){
                    unique_edges++;
                }
            }
            
            printf("Iterations: %d | %.1f iter/s | Seeds: %d | Interesting: %zu | Cov: %d | Unique Edges: %d\n",
               iterations, iters_per_sec, num_seeds, interesting_cases, total_cov, unique_edges);
            fprintf(f, "Iterations: %d | %.1f iter/s | Seeds: %d | Interesting: %zu | Cov: %d | Unique Edges: %d\n",
                iterations, iters_per_sec, num_seeds, interesting_cases, total_cov, unique_edges);
            fprintf(f, "Average edges covered: %f / %zu\n", avg_edges_covered, edge_size);
            fflush(stdout);
            fflush(f);
        }

        int status = run_target(input, input_size);

        if (WIFSIGNALED(status))
        {
            printf("Crash detected! Signal: %d\n", WTERMSIG(status));
            printf("Crashing input (length %zu):\n", input_size);
            printf("\n");
            printf("Number of tests before crash: %d\n", iterations);
            time_t end = time(NULL);

            printf("Elapsed time: %.0f seconds\n", difftime(end, start));

            // Write summary to log
            fprintf(f, "Crash detected! Signal: %d\n", WTERMSIG(status));
            fprintf(f, "Crashing input (length %zu):\n", input_size);
            fprintf(f, "\n");
            fprintf(f, "Number of tests before crash: %d\n", iterations);
            fprintf(f, "Elapsed time: %.0f seconds\n", difftime(end, start));
            fclose(f);
            
            //write save crash output
            input_to_file("crashing_output.pdf", input, input_size);

            if(edge_counters != NULL){
                free(edge_counters);
            }


            return 0; // STOP FUZZER
        }

        int edges_covered = 0;
        if (update_coverage(&edges_covered))
        {
            
            if (num_seeds < MAX_SEEDS)
            {
                memcpy(seed_pool[num_seeds].seed_buffer, input, input_size);
                seed_pool[num_seeds].size = input_size;
                num_seeds++;
            }
            interesting_cases++;
            save_to_intresting(input, num_seeds, input_size);
        }
        avg_edges_covered = avg_edges_covered + (((float)edges_covered - avg_edges_covered) / (float)iterations);
    }

    printf("Crashing input not found.\n");
    fprintf(f, "Crashing input not found.\n");
    fclose(f);

    if (edge_counters != NULL) {
        free(edge_counters);
    }

    return 0;
}