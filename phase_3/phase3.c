#include <dirent.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define INPUT_LENGTH 16 * 1024
#define MAX_SEEDS 1000
#define COVERAGE_SIZE 65536
#define RAND_BYTE() (uint8_t)(rand() % 256)
#define TARGET_NAME "./pdftotext"

#define FORKSRV_FD 198
static int go_pipe[2]; // sends go signal
static int st_pipe[2]; // recieves status signal

static pid_t forkserver_pid;

static char input_file_path[512];
static char coverage_file_path[512];
static char crash_output_path[512];

// Shared memory for coverage (eliminates file I/O per test)
static char shm_name[64];
static uint8_t *shm_cov = NULL;

// Persistent fd for input file (eliminates open/close per test)
static int input_fd = -1;

struct Seed
{
  uint8_t seed_buffer[INPUT_LENGTH + 1];
  size_t size;
};

struct Seed seed_pool[MAX_SEEDS];
int num_seeds = 0;
uint8_t *edge_counters = NULL;
size_t edge_size = 0;

void cleanup_shared_memory(void)
{
  if (shm_cov)
  {
    munmap(shm_cov, sizeof(uint64_t) + COVERAGE_SIZE);
    shm_cov = NULL;
  }
  shm_unlink(shm_name);
}

void setup_shared_memory(void)
{
  snprintf(shm_name, sizeof(shm_name), "/fzcov_%d", getpid());

  int fd = shm_open(shm_name, O_CREAT | O_RDWR, 0600);
  if (fd < 0)
  {
    perror("shm_open");
    exit(1);
  }

  size_t total = sizeof(uint64_t) + COVERAGE_SIZE;
  if (ftruncate(fd, (off_t)total) != 0)
  {
    perror("ftruncate shm");
    exit(1);
  }

  shm_cov = mmap(NULL, total, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (shm_cov == MAP_FAILED)
  {
    perror("mmap shm");
    exit(1);
  }
  close(fd);
  memset(shm_cov, 0, total);

  setenv("FUZZER_SHM", shm_name, 1);
  atexit(cleanup_shared_memory);
}

void generate_input(uint8_t *input_buffer, size_t *size)
{
  *size = (size_t)(rand() % INPUT_LENGTH);
  for (size_t k = 0; k < *size; k++)
  {
    input_buffer[k] = RAND_BYTE();
  }
}

// Used for non-hot-path writes (crash saving)
void input_to_file(const char *filename, const uint8_t *input,
                   size_t num_bytes)
{
  FILE *f = fopen(filename, "wb");
  if (!f)
  {
    perror("Failed to open input file");
    exit(1);
  }
  fwrite(input, 1, num_bytes, f);
  fclose(f);
}

// Fast hot-path input write using persistent fd
static void write_input_fast(const uint8_t *input, size_t num_bytes)
{
  if (lseek(input_fd, 0, SEEK_SET) < 0)
  {
    perror("lseek input");
    exit(1);
  }
  ssize_t w = write(input_fd, input, num_bytes);
  if (w < 0 || (size_t)w != num_bytes)
  {
    perror("write input");
    exit(1);
  }
  if (ftruncate(input_fd, (off_t)num_bytes) != 0)
  {
    perror("ftruncate input");
    exit(1);
  }
}

void mutate_seed(uint8_t *new_input, const uint8_t *src, size_t size)
{
  memcpy(new_input, src, size);
  if (size > 0)
  {
    new_input[(size_t)(rand() % (int)size)] = RAND_BYTE();
  }
}

bool update_coverage(int *hit_count)
{
  // Read coverage directly from shared memory (no file I/O)
  uint64_t cov_size = *(uint64_t *)shm_cov;
  uint8_t *new_counters = shm_cov + sizeof(uint64_t);

  if (cov_size == 0)
  {
    fprintf(stderr, "No coverage data in shared memory\n");
    return false;
  }

  // First initialization of edge_counters
  if (edge_counters == NULL)
  {
    edge_size = (size_t)cov_size;
    edge_counters = malloc(edge_size);
    memcpy(edge_counters, new_counters, edge_size);
    return true;
  }

  // Update edge counters using 64-bit comparisons to skip zero chunks
  bool coverage_flag = false;
  int edges_hit = 0;

  size_t i = 0;
  size_t chunks = edge_size / sizeof(uint64_t);

  for (size_t c = 0; c < chunks; c++)
  {
    uint64_t new_qword, old_qword;
    memcpy(&new_qword, new_counters + i, sizeof(uint64_t));

    // Fast skip: no edges hit in this 8-byte chunk
    if (new_qword == 0)
    {
      i += 8;
      continue;
    }

    memcpy(&old_qword, edge_counters + i, sizeof(uint64_t));

    if (new_qword == old_qword)
    {
      // Same coverage, just count edges
      for (int j = 0; j < 8; j++)
      {
        if (new_counters[i + j] > 0)
          edges_hit++;
      }
      i += 8;
      continue;
    }

    // Coverage differs, process byte by byte
    for (int j = 0; j < 8; j++)
    {
      if (new_counters[i] > edge_counters[i])
      {
        edge_counters[i] = new_counters[i];
        coverage_flag = true;
      }
      if (new_counters[i] > 0)
        edges_hit++;
      i++;
    }
  }

  // Handle remaining bytes
  for (; i < edge_size; i++)
  {
    if (new_counters[i] > edge_counters[i])
    {
      edge_counters[i] = new_counters[i];
      coverage_flag = true;
    }
    if (new_counters[i] > 0)
      edges_hit++;
  }

  *hit_count = edges_hit;
  return coverage_flag;
}

void save_to_intresting(const uint8_t *input, int seed_num, size_t size)
{
  char filename[256];
  snprintf(filename, sizeof(filename), "interesting/seed_%d.pdf", seed_num);

  FILE *f = fopen(filename, "wb");
  if (f)
  {
    fwrite(input, 1, size, f);
    fclose(f);
  }
}

int run_target(const uint8_t *input, size_t size)
{
  // Write input using persistent fd (no open/close overhead)
  write_input_fast(input, size);

  uint32_t go = 0xDEADBEEF;
  if (write(go_pipe[1], &go, 4) != 4)
  {
    perror("write GO");
    exit(1);
  }

  uint32_t child_status;
  if (read(st_pipe[0], &child_status, 4) != 4)
  {
    perror("read STATUS");
    exit(1);
  }

  return (int)child_status;
}

void load_seeds(void)
{
  DIR *d;
  struct dirent *dir;
  d = opendir("seeds");
  if (!d)
  {
    printf("No seeds in directory.\n");
    return;
  }

  while ((dir = readdir(d)) != NULL)
  {
    if (dir->d_type == DT_REG && strstr(dir->d_name, ".pdf"))
    {
      char filepath[512];
      snprintf(filepath, sizeof(filepath), "%s/%s", "seeds", dir->d_name);

      FILE *f = fopen(filepath, "rb");
      if (f)
      {
        // Get file size
        fseek(f, 0, SEEK_END);
        long fsize_long = ftell(f);
        fseek(f, 0, SEEK_SET);

        if (fsize_long > 0 && fsize_long < INPUT_LENGTH)
        {
          size_t fsize = (size_t)fsize_long;
          uint8_t buffer[INPUT_LENGTH];
          size_t bytes_read = fread(buffer, 1, fsize, f);

          if (bytes_read == fsize)
          {
            run_target(buffer, bytes_read);
            int hit_count;
            if (update_coverage(&hit_count))
            {
              if (num_seeds < MAX_SEEDS)
              {
                memcpy(seed_pool[num_seeds].seed_buffer, buffer, bytes_read);
                seed_pool[num_seeds].size = bytes_read;
                num_seeds++;
                printf("Loaded seed: %s (%zu bytes)\n", dir->d_name, fsize);
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

void start_forkserver(const char *target_path)
{
  if (pipe(go_pipe) || pipe(st_pipe))
  {
    perror("fork server pipe setup");
    exit(1);
  }

  forkserver_pid = fork();
  if (forkserver_pid < 0)
  {
    perror("fork server start");
    exit(1);
  }

  if (forkserver_pid == 0)
  {
    dup2(go_pipe[0], FORKSRV_FD);
    dup2(st_pipe[1], FORKSRV_FD + 1);

    close(go_pipe[0]);
    close(go_pipe[1]);
    close(st_pipe[0]);
    close(st_pipe[1]);

    if (freopen("/dev/null", "w", stdout) == NULL)
    {
      perror("freopen stdout");
      exit(1);
    }
    if (freopen("/dev/null", "w", stderr) == NULL)
    {
      perror("freopen stderr");
      exit(1);
    }

    execl(target_path, target_path, NULL);
    perror("fork server exec");
    exit(1);
  }

  close(go_pipe[0]);
  close(st_pipe[1]);

  // wait for hello signal from forkserver
  uint32_t hello;
  if (read(st_pipe[0], &hello, 4) != 4)
  {
    perror("forkserver did not say hello");
    exit(1);
  }

  printf("Forkserver ready\n");
}

int main(void)
{
  char tmpdir[256];
  snprintf(tmpdir, sizeof(tmpdir), "/tmp/%d", getpid());

  if (mkdir(tmpdir, 0755) != 0)
  {
    exit(1);
  }

  snprintf(input_file_path, sizeof(input_file_path), "%s/exec_input.pdf",
           tmpdir);
  snprintf(coverage_file_path, sizeof(coverage_file_path), "%s/coverage.data",
           tmpdir);
  snprintf(crash_output_path, sizeof(crash_output_path),
           "%s/crashing_output.pdf", tmpdir);

  setenv("FUZZER_TMP", tmpdir, 1);
  printf("RAM disk working directory: %s\n", tmpdir);

  // Setup shared memory for coverage (before forkserver so env var is inherited)
  setup_shared_memory();

  // Open persistent fd for input file (avoids open/close per iteration)
  input_fd = open(input_file_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (input_fd < 0)
  {
    perror("open input fd");
    exit(1);
  }

  // Open log file
  FILE *f = fopen("log.txt", "w");
  if (f == NULL)
  {
    perror("fopen");
    return 1;
  }

  // Initialize randomizer with current time as seed
  srand((unsigned int)time(NULL));

  // Use monotonic clock for accurate sub-second timing
  struct timespec ts_start, ts_checkpoint;
  clock_gettime(CLOCK_MONOTONIC, &ts_start);
  ts_checkpoint = ts_start;

  // start forkserver
  start_forkserver(TARGET_NAME);

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
      struct timespec ts_now;
      clock_gettime(CLOCK_MONOTONIC, &ts_now);
      double elapsed = (double)(ts_now.tv_sec - ts_checkpoint.tv_sec) +
                        (double)(ts_now.tv_nsec - ts_checkpoint.tv_nsec) / 1e9;
      double iters_per_sec = (elapsed > 0) ? 1000.0 / elapsed : 0;
      ts_checkpoint = ts_now;

      int total_cov = 0;
      int unique_edges = 0;

      for (size_t i = 0; i < edge_size; i++)
      {
        total_cov += (int)(edge_counters[i]);
        if (edge_counters[i] > 0)
          unique_edges++;
      }

      printf("Iterations: %d | %.1f iter/s | Seeds: %d | Interesting: %zu | "
             "Cov: %d | Unique Edges: %d\n",
             iterations, iters_per_sec, num_seeds, interesting_cases, total_cov,
             unique_edges);
      fprintf(f,
              "Iterations: %d | %.1f iter/s | Seeds: %d | Interesting: %zu | "
              "Cov: %d | Unique Edges: %d\n",
              iterations, iters_per_sec, num_seeds, interesting_cases,
              total_cov, unique_edges);
      fprintf(f, "Average edges covered: %f / %zu\n", avg_edges_covered,
              edge_size);
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

      struct timespec ts_end;
      clock_gettime(CLOCK_MONOTONIC, &ts_end);
      double total_elapsed = (double)(ts_end.tv_sec - ts_start.tv_sec) +
                              (double)(ts_end.tv_nsec - ts_start.tv_nsec) / 1e9;

      printf("Elapsed time: %.1f seconds\n", total_elapsed);

      // Write summary to log
      fprintf(f, "Crash detected! Signal: %d\n", WTERMSIG(status));
      fprintf(f, "Crashing input (length %zu):\n", input_size);
      fprintf(f, "\n");
      fprintf(f, "Number of tests before crash: %d\n", iterations);
      fprintf(f, "Elapsed time: %.1f seconds\n", total_elapsed);
      fclose(f);

      // write save crash output
      input_to_file("crashing_output.pdf", input, input_size);

      if (edge_counters != NULL)
      {
        free(edge_counters);
      }

      close(input_fd);
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
    avg_edges_covered =
        avg_edges_covered +
        (((float)edges_covered - avg_edges_covered) / (float)iterations);
  }

  printf("Crashing input not found.\n");
  fprintf(f, "Crashing input not found.\n");
  fclose(f);

  if (edge_counters != NULL)
  {
    free(edge_counters);
  }

  close(input_fd);
  return 0;
}
