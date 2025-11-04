#ifndef HEADER_FIRESTARTER_H
#define HEADER_FIRESTARTER_H

#if !FD_HAS_FIRESTARTER
#error "This target requires FD_HAS_FIRESTARTER"
#endif

#define FIRESTARTER_SHMEM 6

#include <stdint.h>
#include <sys/types.h>

#define HYPERCALL_MAGIC 0x4153524568797072

enum SHMEM_TYPE {
    FIRE_INPUT = 3,
    FIRE_COV = 4,
    FIRE_INIT = 5
};
typedef enum SHMEM_TYPE shm_mem_t;

inline void hypercall(uint64_t hypercall_num, uint64_t arg1, 
                             uint64_t arg2, uint64_t arg3, uint64_t arg4) {
    uint64_t rax_val = HYPERCALL_MAGIC;

    __asm__ __volatile__(
        "movq %1, %%r8\n\t"
        "movq %2, %%r9\n\t"
        "movq %3, %%r10\n\t"
        "movq %4, %%r11\n\t"
        "movq %5, %%r12\n\t"
        "int $3"
        : "+a" (rax_val)
        : "r" (hypercall_num),
          "r" (arg1),
          "r" (arg2),
          "r" (arg3),
          "r" (arg4)
        : "r8", "r9", "r10", "r11", "r12", "memory"
    );
}

inline void firestarter_full_snap(void) {
    hypercall(1, 0, 0, 0, 0);
}

inline void firestarter_diff_snap(void) {
    hypercall(2, 0, 0, 0, 0);
}

inline void firestarter_restore_snap(void)  {
    hypercall(3, 0, 0, 0, 0);
}

inline void firestarter_setup_shmem(void* ptr, uint64_t  size, shm_mem_t t) {
    hypercall(
        4,
        (uint64_t) ptr,
        (uint64_t) size,
        t,
        0
    );
}

inline void firestarter_mutate(void*ptr, size_t len, shm_mem_t t) {
    hypercall(5, (uint64_t) ptr, len, t, 0);
}

inline void firestarter_fuzzend(void) {
    hypercall(6, 0, 0, 0, 0);
}



void firestarter_init(void);
void firestarter_end(void);
#endif
