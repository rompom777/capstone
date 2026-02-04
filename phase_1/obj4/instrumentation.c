#include <stdint.h>
#include <stdio.h>
#include <sanitizer/coverage_interface.h>

void __sanitizer_cov_trace_pc_guard_init(uint32_t *start, uint32_t *stop) {
    static uint64_t N;  // Counter for the guards.
    if (start == stop || *start) return;  // Initialize only once.
    
    for (uint32_t *x = start; x < stop; x++){
        *x = ++N;  // Guards should start from 1.
    }
}

void __sanitizer_cov_trace_pc_guard(uint32_t *guard) {
    if (!*guard) return;  // Duplicate the guard check.

    FILE *f = fopen("coverage.data", "ab");
    if(f){
        fwrite(guard, sizeof(uint32_t), 1, f);
        fclose(f);
    }
}