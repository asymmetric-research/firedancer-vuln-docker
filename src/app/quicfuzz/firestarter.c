#if !FD_HAS_FIRESTARTER
#error "This target requires FD_HAS_FIRESTARTER"
#endif
#include "common.h"
#include "firestarter.h"
#include "shmem.h"

 
void firestarter_init(void){
    FD_LOG_WARNING(("!calling firestarter shmem"));
    firestarter_init_shmem();
    FD_LOG_WARNING(("!calling firestarter snap"));
    firestarter_full_snap();    
}

void firestarter_end(void) {
    FD_LOG_WARNING(("!calling firestarter firestarter_end"));
    firestarter_fuzzend();
}

