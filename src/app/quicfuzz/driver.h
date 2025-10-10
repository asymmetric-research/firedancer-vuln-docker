#pragma DRIVER

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
fd_drv_init( fd_drv_t * drv, char* topo_name );


