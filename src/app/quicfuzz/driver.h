#ifndef DRIVER
#define DRIVER

#include "../../disco/topo/fd_topob.h"
#include "../shared/fd_config.h"
#include "../../disco/quic/fd_tpu.h"
#include "../shared/fd_config_file.h"
#include "../shared/fd_action.h"
#include "../fdctl/config.h"
#include "../../util/fd_util.h"
#include "../../tango/fseq/fd_fseq.h"
#include "../shared/commands/configure/configure.h"

struct fd_drv_private {
  fd_topo_run_tile_t **      tiles;
  fd_topo_obj_callbacks_t ** callbacks;
  fd_config_t                config;
  int                        is_firestarter;
};
typedef struct fd_drv_private fd_drv_t;

void
isolated_quic_topo( config_t * config  );

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


/* Use fd_link_permit_no_producers with links that do not have any
   producers.  This may be required in sub-topologies used for
   development and testing. */
FD_FN_UNUSED static ulong
fd_link_permit_no_producers( fd_topo_t * topo, char * link_name ) {
  ulong found = 0UL;
  for( ulong link_i = 0UL; link_i < topo->link_cnt; link_i++ ) {
    if( !strcmp( topo->links[ link_i ].name, link_name ) ) {
      topo->links[ link_i ].permit_no_producers = 1;
      found++;
    }
  }
  return found;
}


/* Use fd_link_permit_no_consumers with links that do not have any
   consumers.  This may be required in sub-topologies used for
   development and testing. */
FD_FN_UNUSED static ulong
fd_link_permit_no_consumers( fd_topo_t * topo, char * link_name ) {
  ulong found = 0UL;
  for( ulong link_i = 0UL; link_i < topo->link_cnt; link_i++ ) {
    if( !strcmp( topo->links[ link_i ].name, link_name ) ) {
      topo->links[ link_i ].permit_no_consumers = 1;
      found++;
      break;
    }
  }
  return found;
}

#endif
