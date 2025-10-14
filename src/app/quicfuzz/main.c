#include "driver.h"
#include <stdlib.h>
#include <sys/mman.h>
#include <errno.h>
#include "../platform/fd_file_util.h"


char const * FD_APP_NAME    = "fd_quiz_fuzz";
char const * FD_BINARY_NAME = "fd_quic_fuzz";

extern fd_topo_run_tile_t fd_tile_quic;
extern fd_topo_run_tile_t fd_tile_sock;
// extern fd_topo_run_tile_t fd_tile_verify;

extern fd_topo_obj_callbacks_t fd_obj_cb_tile;
extern fd_topo_obj_callbacks_t fd_obj_cb_mcache;
extern fd_topo_obj_callbacks_t fd_obj_cb_dcache;
extern fd_topo_obj_callbacks_t fd_obj_cb_metrics;

extern fd_topo_obj_callbacks_t fd_obj_cb_fseq;

configure_stage_t * STAGES[] = {
  &fd_cfg_stage_sysctl,
  NULL,
};


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
  // &fd_tile_verify,
  NULL
};

extern uchar const fdquic_default_config[];
extern ulong const fdquic_default_config_sz;
FD_IMPORT_BINARY( fdquic_default_config, "src/app/quicfuzz/config/default.toml" );

extern uchar const fdctl_default_config[];
extern ulong const fdctl_default_config_sz;
FD_IMPORT_BINARY( fdctl_default_config, "src/app/fdctl/config/default.toml" );


action_t * ACTIONS[] = { NULL };


int
main( int    argc,
      char** argv ) {
    FD_LOG_INFO(("START"));

    void * shmem = aligned_alloc( fd_drv_align(), fd_drv_footprint() );
    if( FD_UNLIKELY( !shmem ) ) FD_LOG_ERR(( "malloc failed" ));
    fd_drv_t * drv = fd_drv_join( fd_drv_new( shmem, TILES, CALLBACKS ) );
    if( FD_UNLIKELY( !drv ) ) FD_LOG_ERR(( "creating quic fuzz driver failed" ));
    const char * opt_user_config_path = fd_env_strip_cmdline_cstr(
      &argc,
      &argv,
      "--config",
      "FIREDANCER_CONFIG_TOML",
      NULL );
    char * user_config = NULL;
    ulong user_config_sz = 0UL;
    if( FD_LIKELY( opt_user_config_path ) ) {
      FD_LOG_INFO(("USER CONFIG"));
      user_config = fd_file_util_read_all( opt_user_config_path, &user_config_sz );
      if( FD_UNLIKELY( user_config==MAP_FAILED ) ) FD_LOG_ERR(( "failed to read user config file `%s` (%d-%s)", opt_user_config_path, errno, fd_io_strerror( errno ) ));
      fd_config_load( 0, 0, 1, (char const *)NULL, 0UL, NULL, NULL, 0UL, user_config, user_config_sz, opt_user_config_path, &drv->config );      
    } else {
      fd_config_load( 0, 0, 1, (char const *)fdquic_default_config, fdquic_default_config_sz, NULL, NULL, 0UL, NULL, 0UL, NULL, &drv->config );
    }


    FD_LOG_INFO(("NAME %s", drv->config.name ));
    FD_LOG_INFO(("user %s", drv->config.user ));
    fd_drv_init( drv );
    return 0;
}
