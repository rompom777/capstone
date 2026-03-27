#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>

static uint8_t *counters_start = NULL;
static size_t counters_size = 0;

static int is_target_running = 0;

// Coverage State
void write_coverage_on_exit(void) {

    if (!counters_start) return;

    char path[256];
    const char *tmpdir = getenv("FUZZER_TMP");

    if (tmpdir) {
        snprintf(path, sizeof(path), "%s/coverage.data", tmpdir);
    } else {
        snprintf(path, sizeof(path), "coverage.data");
    }
    
    FILE *f = fopen(path, "wb");
    if (!f) {
        perror("Failed to write coverage");
        return;
    }

    
    uint64_t size_header = counters_size;
    fwrite(&size_header, sizeof(uint64_t), 1, f);
    fwrite(counters_start, 1, counters_size, f);
    
    fclose(f);

}

void __sanitizer_cov_8bit_counters_init(uint8_t *start, uint8_t *stop) {
    counters_start = start;
    counters_size = stop - start;

    atexit(write_coverage_on_exit);
    
}

void start_target_coverage(void){
    is_target_running = 1;
    if (counters_start){
        memset(counters_start, 0, counters_size);
    }
}

void stop_target_coverage(void){
    write_coverage_on_exit();
    is_target_running = 0;
}

extern int fuzzer_main(void);

int __wrap_main(int argc, char **argv) {
    return fuzzer_main();
}





