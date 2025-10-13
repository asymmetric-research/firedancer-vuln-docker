#include "driver.h"
#include <stdlib.h>
#include "../shared/fd_action.h"
#include "../fdctl/config.h"
#include "../../util/fd_util.h"
#include "../../tango/fseq/fd_fseq.h"

char const * FD_APP_NAME    = "fd_quiz_fuzz";
char const * FD_BINARY_NAME = "fd_quic_fuzz";

extern fd_topo_run_tile_t fd_tile_quic;
extern fd_topo_run_tile_t fd_tile_sock;

extern fd_topo_obj_callbacks_t fd_obj_cb_tile;
extern fd_topo_obj_callbacks_t fd_obj_cb_mcache;
extern fd_topo_obj_callbacks_t fd_obj_cb_dcache;
extern fd_topo_obj_callbacks_t fd_obj_cb_metrics;

extern fd_topo_obj_callbacks_t fd_obj_cb_fseq;

fd_topo_obj_callbacks_t * CALLBACKS[] = {
    &fd_obj_cb_mcache,
    &fd_obj_cb_dcache,
    &fd_obj_cb_tile,
    &fd_obj_cb_metrics,    
    &fd_obj_cb_fseq,
    NULL,
};


fd_topo_run_tile_t * TILES[] = {
  &fd_tile_quic,
  &fd_tile_sock,
  NULL
};

// extern char const fdquic_default_config[1];
// extern ulong const fdquic_default_config_sz;
// FD_IMPORT_BINARY( fdquic_default_config, "src/app/quicfuzz/config/default.toml" );


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

    // fd_config_load( 0, 0, 1, (char const *)fdquic_default_config, fdquic_default_config_sz, NULL, NULL, 0UL, NULL, 0UL, NULL, &drv->config );	
    memset( &drv->config, 0, sizeof(config_t) );

    // fd_config_load_buf( &drv->config, ( char const * )fdquic_default_config, fdquic_default_config_sz, "" );
    // FD_LOG_INFO(("NAME %s", drv->config.name ));

    fd_drv_init( drv );
    return 0;
}
