#include <sys/mman.h>
#include <fcntl.h>
#include <stdint.h>
#include <stddef.h>
#include "libafl_cov.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>

#include <sys/shm.h>
#include <sys/wait.h>
#include <sys/types.h>

static uint8_t *__afl_area_ptr = NULL;
static uint32_t __afl_map_size = MAX_EDGES_NUM;
static uint64_t previous_pc = 0;

__attribute__((no_instrument_function, noinline))
__attribute__((no_sanitize("coverage"))) void
__wrap___sanitizer_cov_trace_pc_guard_init(uint32_t *start, uint32_t *stop)
{
	static int already_initialized = 0;
	FD_LOG_DEBUG(("INIT CALLED\n"));
	if (start == stop || *start)
		return;
	if (already_initialized)
		return;
	already_initialized = 1;

	// Get the shared memory filename from environment
	char *shmem_name = getenv("__AFL_MMAP_SHM_NAME");
	if (!shmem_name)
	{
		FD_LOG_ERR(("[-] Error: __AFL_MMAP_SHM_NAME not set\n"));
	}

	// Open the POSIX shared memory object
	// Add leading slash for POSIX shm_open naming convention
	char shm_path[256];
	snprintf(shm_path, sizeof(shm_path), "/%s", shmem_name);

	int shm_fd = shm_open(shm_path, O_RDWR, 0600);
	if (shm_fd == -1)
	{
		FD_LOG_ERR(("[-] shm_open failed"));
	}

	// Map the shared memory
	__afl_area_ptr = (uint8_t *)mmap(
			NULL,
			__afl_map_size,
			PROT_READ | PROT_WRITE,
			MAP_SHARED,
			shm_fd,
			0);

	close(shm_fd); // Can close fd after mmap

	if (__afl_area_ptr == MAP_FAILED)
	{
		FD_LOG_ERR(("[-] mmap failed"));
	}

	FD_LOG_DEBUG(("[+] Mapped shared memory: %s (%u bytes)\n",
								shm_path, __afl_map_size));

	// Initialize guards with unique IDs
	uint32_t guard_id = 1;
	for (uint32_t *x = start; x < stop; x++)
	{
		*x = guard_id++;
	}

	FD_LOG_DEBUG(("[+] Initialized %u edge guards\n", guard_id - 1));
}

__attribute__((no_instrument_function, noinline))
__attribute__((no_sanitize("coverage"))) void
__wrap___sanitizer_cov_trace_pc_guard(uint32_t *guard)
{
	if (!*guard)
		return; // Duplicate the guard check.
						// If you set *guard to 0 this code will not be called again for this edge.
						// Now you can get the PC and do whatever you want:
						//   store it somewhere or symbolize it and print right away.
						// The values of `*guard` are as you set them in
						// __sanitizer_cov_trace_pc_guard_init and so you can make them consecutive
						// and use them to dereference an array or a bit vector.
	uint8_t *map = __afl_area_ptr;
	if (!map)
		return;

	/* compute an index from the PC pair (fast, no syscalls / stdlib calls) */
	uint64_t pc = (uint64_t)__builtin_return_address(0);
	uint64_t pos = (pc ^ (previous_pc >> 1)) % (__afl_map_size ? __afl_map_size : 1);
	previous_pc = pc;

	/* atomically increment the byte at map[pos] */
	/* Use __atomic_fetch_add for uint8_t - this is supported by GCC/Clang. */
	__atomic_fetch_add(&map[pos], 1u, __ATOMIC_RELAXED);
}
