#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static uint8_t *counters_start = NULL;
static size_t counters_size = 0;

static int is_forkserver_parent = 0;
static char coverage_path[256];

// Coverage State
void write_coverage_on_exit(void) {

  if (is_forkserver_parent)
    return;
  if (!counters_start)
    return;

  FILE *f = fopen(coverage_path, "wb");
  if (!f) {
    perror("Failed to write coverage");
    return;
  }

  uint64_t size_header = counters_size;
  fwrite(&size_header, sizeof(uint64_t), 1, f);
  fwrite(counters_start, 1, counters_size, f);

  fclose(f);
}

void __sanitizer_cov_8bit_counters_init(uint8_t *start, const uint8_t *stop) {
  counters_start = start;
  counters_size = (size_t)(stop - start);

  const char *tmpdir = getenv("FUZZER_TMP");
  if (tmpdir) {
    snprintf(coverage_path, sizeof(coverage_path), "%s/coverage.data", tmpdir);
  } else {
    // fallback to current directory if env var not set
    snprintf(coverage_path, sizeof(coverage_path), "coverage.data");
  }

  atexit(write_coverage_on_exit);
}

// Forkserver

#define FORKSRV_FD 198 // read go on this fd, writes signal on FORKSRV_FD+1
#define INPUT "exec_input.pdf"

extern int __real_main(int argc, char **argv);

__attribute__((constructor)) static void forkserver_init(void) {

  printf("starting forkserver\n");

  if (fcntl(FORKSRV_FD, F_GETFD) == -1 ||
      fcntl(FORKSRV_FD + 1, F_GETFD) == -1) {
    return;
  }

  uint32_t hello = 0;
  if (write(FORKSRV_FD + 1, &hello, 4) != 4) {
    perror("hello from forkserver");
    _exit(1);
  }

  char input_path[256];
  const char *tmpdir = getenv("FUZZER_TMP");
  if (tmpdir) {
    snprintf(input_path, sizeof(input_path), "%s/exec_input.pdf", tmpdir);
  } else {
    snprintf(input_path, sizeof(input_path), "exec_input.pdf");
  }

  while (1) {

    // wait for go signal
    uint32_t go;
    if (read(FORKSRV_FD, &go, 4) != 4) {
      perror("get go signal");
      _exit(1);
    }

    if (counters_start) {
      memset(counters_start, 0, counters_size);
    }

    pid_t child = fork();
    if (child < 0) {
      perror("spawning target");
      _exit(1);
    }

    if (child == 0) {
      close(FORKSRV_FD);
      close(FORKSRV_FD + 1);
      // start target
      char *argv[] = {"xpdf", input_path, NULL};
      exit(__real_main(2, argv));
    }
    is_forkserver_parent = 1;

    int status;
    if (waitpid(child, &status, 0) < 0)
      _exit(1);

    uint32_t u_status = (uint32_t)status;
    if (write(FORKSRV_FD + 1, &u_status, 4) != 4)
      _exit(1);

    is_forkserver_parent = 0;
  }
}

int __wrap_main(int argc, char **argv) { return __real_main(argc, argv); }
