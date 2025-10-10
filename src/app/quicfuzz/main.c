#include "driver.h"
#include <stdlib.h>
#include "../shared/fd_action.h"

char const * FD_APP_NAME    = "fd_quiz_fuzz";
char const * FD_BINARY_NAME = "fd_quic_fuzz";

extern fd_topo_run_tile_t fd_tile_quic;
extern fd_topo_run_tile_t fd_tile_verify;
extern fd_topo_run_tile_t fd_tile_net;


extern fd_topo_obj_callbacks_t fd_obj_cb_mcache;
extern fd_topo_obj_callbacks_t fd_obj_cb_dcache;

fd_topo_obj_callbacks_t * CALLBACKS[] = {
    &fd_obj_cb_mcache,
    &fd_obj_cb_dcache,
    NULL,
};


fd_topo_run_tile_t * TILES[] = {
  &fd_tile_quic,
  &fd_tile_verify,
  &fd_tile_net,
  NULL
};


action_t * ACTIONS[] = { NULL };

int
main( int    argc,
      char** argv ) {

    void * shmem = aligned_alloc( fd_drv_align(), fd_drv_footprint() );
    if( FD_UNLIKELY( !shmem ) ) FD_LOG_ERR(( "malloc failed" ));
    fd_drv_t * drv = fd_drv_join( fd_drv_new( shmem, TILES, CALLBACKS ) );
    if( FD_UNLIKELY( !drv ) ) FD_LOG_ERR(( "creating tile fuzz driver failed" ));
    fd_drv_init( drv, argv[1] );
    return 0;
}
