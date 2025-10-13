#ifndef DRIVER
#define DRIVER

#include "../../disco/topo/fd_topob.h"
#include "../shared/fd_config.h"
#include "../../disco/quic/fd_tpu.h"

struct fd_drv_private {
  fd_topo_run_tile_t **      tiles;
  fd_topo_obj_callbacks_t ** callbacks;
  fd_config_t                config;
};
typedef struct fd_drv_private fd_drv_t;


void
fd_drv_init( fd_drv_t * drv );


ulong
fd_drv_footprint( void );

FD_FN_CONST ulong
fd_drv_align( void );

void *
fd_drv_new( void * shmem, fd_topo_run_tile_t ** tiles, fd_topo_obj_callbacks_t ** callbacks );

fd_drv_t *
fd_drv_join( void * shmem );

void *
fd_drv_leave( fd_drv_t * drv );

void *
fd_drv_delete( void * shmem );

void
fd_drv_publish_hook( fd_frag_meta_t const * mcache );


#endif
