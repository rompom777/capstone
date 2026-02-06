#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static uint8_t *counters_start = NULL;
static size_t counters_size = 0;

void write_coverage_on_exit(void) {
    if (!counters_start) return;
    
    FILE *f = fopen("coverage.data", "wb");
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



