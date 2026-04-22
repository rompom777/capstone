#include <fcntl.h>
#include <setjmp.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

static uint8_t *counters_start = NULL;
static size_t counters_size = 0;

static int is_forkserver_parent = 0;
static char coverage_path[256];

// Shared memory for coverage (fast path)
static uint8_t *shm_ptr = NULL;

// Persistent mode: survive exit() calls via longjmp
#define PERSISTENT_ITERS 500
static jmp_buf exit_jmp;
static volatile int in_persistent_mode = 0;
static volatile int exit_status_val = 0;

extern void __real_exit(int status) __attribute__((noreturn));
extern int __real_main(int argc, char **argv);

void __wrap_exit(int status)
{
  if (in_persistent_mode)
  {
    exit_status_val = status;
    longjmp(exit_jmp, 1);
  }
  __real_exit(status);
}

static void write_coverage_to_shm(void)
{
  if (!shm_ptr || !counters_start)
    return;
  uint64_t size_header = counters_size;
  memcpy(shm_ptr, &size_header, sizeof(uint64_t));
  memcpy(shm_ptr + sizeof(uint64_t), counters_start, counters_size);
}

// Fallback file-based coverage (used when shm is unavailable)
void write_coverage_on_exit(void)
{
  if (is_forkserver_parent)
    return;
  if (!counters_start)
    return;

  if (shm_ptr)
  {
    write_coverage_to_shm();
    return;
  }

  FILE *f = fopen(coverage_path, "wb");
  if (!f)
    return;

  uint64_t size_header = counters_size;
  fwrite(&size_header, sizeof(uint64_t), 1, f);
  fwrite(counters_start, 1, counters_size, f);
  fclose(f);
}

void __sanitizer_cov_8bit_counters_init(uint8_t *start, const uint8_t *stop)
{
  counters_start = start;
  counters_size = (size_t)(stop - start);

  // Try to open shared memory created by the fuzzer
  const char *shm_name = getenv("FUZZER_SHM");
  if (shm_name)
  {
    int fd = shm_open(shm_name, O_RDWR, 0600);
    if (fd >= 0)
    {
      size_t total = sizeof(uint64_t) + counters_size;
      shm_ptr =
          mmap(NULL, total, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
      if (shm_ptr == MAP_FAILED)
        shm_ptr = NULL;
      close(fd);
    }
  }

  // Setup file-based fallback path
  const char *tmpdir = getenv("FUZZER_TMP");
  if (tmpdir)
    snprintf(coverage_path, sizeof(coverage_path), "%s/coverage.data", tmpdir);
  else
    snprintf(coverage_path, sizeof(coverage_path), "coverage.data");

  atexit(write_coverage_on_exit);
}

// Forkserver

#define FORKSRV_FD 198

__attribute__((constructor)) static void forkserver_init(void)
{
  printf("starting forkserver\n");

  if (fcntl(FORKSRV_FD, F_GETFD) == -1 ||
      fcntl(FORKSRV_FD + 1, F_GETFD) == -1)
    return;

  // Ignore SIGPIPE so broken pipe returns error instead of killing us
  signal(SIGPIPE, SIG_IGN);

  uint32_t hello = 0;
  if (write(FORKSRV_FD + 1, &hello, 4) != 4)
  {
    perror("hello from forkserver");
    _exit(1);
  }

  char input_path[256];
  const char *tmpdir = getenv("FUZZER_TMP");
  if (tmpdir)
    snprintf(input_path, sizeof(input_path), "%s/exec_input.pdf", tmpdir);
  else
    snprintf(input_path, sizeof(input_path), "exec_input.pdf");

  while (1)
  {
    // Wait for first go signal of this batch
    uint32_t go;
    if (read(FORKSRV_FD, &go, 4) != 4)
      _exit(1);

    if (counters_start)
      memset(counters_start, 0, counters_size);

    // Create pipes for persistent child communication
    int child_ctl[2], child_sts[2];
    if (pipe(child_ctl) || pipe(child_sts))
      _exit(1);

    pid_t child = fork();
    if (child < 0)
      _exit(1);

    if (child == 0)
    {
      // === PERSISTENT CHILD ===
      close(FORKSRV_FD);
      close(FORKSRV_FD + 1);
      close(child_ctl[1]);
      close(child_sts[0]);

      in_persistent_mode = 1;

      for (int iter = 0; iter < PERSISTENT_ITERS; iter++)
      {
        if (iter > 0)
        {
          // Wait for go signal for subsequent iterations
          uint32_t g;
          if (read(child_ctl[0], &g, 4) != 4)
            _exit(0);
          if (counters_start)
            memset(counters_start, 0, counters_size);
        }

        // Run target, catching exit() via longjmp
        int ret;
        if (setjmp(exit_jmp) == 0)
        {
          char *argv[] = {"xpdf", input_path, NULL};
          ret = __real_main(2, argv);
        }
        else
        {
          ret = exit_status_val;
        }

        // Write coverage to shared memory
        write_coverage_to_shm();

        // Send normal exit status (encoded like waitpid: exit code << 8)
        uint32_t status = (uint32_t)((ret & 0xff) << 8);
        if (write(child_sts[1], &status, 4) != 4)
          _exit(1);
      }
      _exit(0);
    }

    // === FORKSERVER PARENT ===
    is_forkserver_parent = 1;
    close(child_ctl[0]);
    close(child_sts[1]);

    int iter;
    for (iter = 0; iter < PERSISTENT_ITERS; iter++)
    {
      if (iter > 0)
      {
        // Wait for next go from fuzzer
        if (read(FORKSRV_FD, &go, 4) != 4)
          _exit(1);

        // Signal child to proceed
        uint32_t g = 1;
        if (write(child_ctl[1], &g, 4) != 4)
        {
          // Child died - get real status from waitpid
          int wstatus;
          waitpid(child, &wstatus, 0);
          uint32_t u_status = (uint32_t)wstatus;
          write(FORKSRV_FD + 1, &u_status, 4);
          goto next_batch;
        }
      }

      // Wait for child status
      uint32_t child_status;
      ssize_t r = read(child_sts[0], &child_status, 4);
      if (r != 4)
      {
        // Child crashed (signal) - get real status from waitpid
        int wstatus;
        waitpid(child, &wstatus, 0);
        uint32_t u_status = (uint32_t)wstatus;
        write(FORKSRV_FD + 1, &u_status, 4);
        goto next_batch;
      }

      // Relay status to fuzzer
      write(FORKSRV_FD + 1, &child_status, 4);
    }

    // Normal batch completion
    waitpid(child, NULL, 0);

  next_batch:
    close(child_ctl[1]);
    close(child_sts[0]);
    is_forkserver_parent = 0;
  }
}

int __wrap_main(int argc, char **argv) { return __real_main(argc, argv); }
