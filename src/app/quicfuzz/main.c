#include "driver.h"
#include <stdlib.h>
#include "../shared/fd_action.h"
#include "../fdctl/config.h"
#include "../../util/fd_util.h"

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

extern uchar const fdquic_default_config[1];
extern ulong const fdquic_default_config_sz;
FD_IMPORT_BINARY( fdquic_default_config, "src/app/quicfuzz/config/default.toml" );


action_t * ACTIONS[] = { NULL };

int
main( int    argc,
      char** argv ) {
    (void)argc;
    (void)argv;

    void * shmem = aligned_alloc( fd_drv_align(), fd_drv_footprint() );
    if( FD_UNLIKELY( !shmem ) ) FD_LOG_ERR(( "malloc failed" ));
    fd_drv_t * drv = fd_drv_join( fd_drv_new( shmem, TILES, CALLBACKS ) );
    if( FD_UNLIKELY( !drv ) ) FD_LOG_ERR(( "creating quic fuzz driver failed" ));
    // static config_t config[1];
	fd_config_load( 0, 0, 1, (char const *)fdquic_default_config, fdquic_default_config_sz, NULL, NULL, 0UL, NULL, 0UL, NULL, &drv->config );	

    fd_drv_init( drv );
    return 0;
}
