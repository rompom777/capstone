CC=clang

COMMON_CFLAGS=-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wundef -Wstrict-prototypes
TEST_CFLAGS=$(COMMON_CFLAGS) -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -fno-optimize-sibling-calls
RELEASE_CFLAGS=$(COMMON_CFLAGS) -O2

fuzzer: phase3.c
	$(CC) phase3.c -o fuzzer $(RELEASE_CFLAGS)

fuzzer_test: phase3.c
	$(CC) phase3.c -o fuzzer_test $(TEST_CFLAGS)

clean:
	rm -f fuzzer fuzzer_test