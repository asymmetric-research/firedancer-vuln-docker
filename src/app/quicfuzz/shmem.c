
#if FD_HAS_FIRESTARTER
#include "shmem.h"
#include <string.h>

char shmem[SHM_SIZE];
char shmem_cov[COV_SIZE];
void firestarter_init_shmem(void) {
    memset(shmem, 0, SHM_SIZE);
    memset(shmem_cov, 0, SHM_SIZE);
    firestarter_setup_shmem(shmem, SHM_SIZE , FIRE_INIT);
    firestarter_setup_shmem(shmem_cov, COV_SIZE, FIRE_COV);
}


#endif

void compilermyfriend(void){;;}
