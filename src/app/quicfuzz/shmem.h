#ifndef HEADER_fd_src_app_quicfuzz_shmem_h
#define HEADER_fd_src_app_quicfuzz_shmem_h

#if FD_HAS_FIRESTARTER
#include "firestarter.h"


#define SHM_SIZE 4096
#define COV_SIZE 4096

void firestarter_init_shmem(void);

#endif
#endif
