#include "driver.h"



/* Maybe similar to what initialize workspaces does, without
   following it closely */
static void
back_wksps( fd_topo_t * topo, fd_topo_obj_callbacks_t * callbacks[] ) {
  ulong keyswitch_obj_id = ULONG_MAX;
  for( ulong i=0UL; i<topo->obj_cnt; i++ ) {
    fd_topo_obj_t * obj = &topo->objs[ i ];
    fd_topo_obj_callbacks_t * cb = NULL;
    for( ulong j=0UL; callbacks[ j ]; j++ ) {
      if( FD_UNLIKELY( !strcmp( callbacks[ j ]->name, obj->name ) ) ) {
        cb = callbacks[ j ];
        break;
      }
    }
    ulong align = cb->align( topo, obj );

    obj->wksp_id = obj->id;
    topo->workspaces[ obj->wksp_id ].wksp = aligned_alloc( align, obj->footprint );
    obj->offset = 0UL;
    FD_LOG_NOTICE(( "obj %s %lu %lu %lu %lu", obj->name, obj->wksp_id, obj->footprint, obj->offset, align ));
    FD_LOG_NOTICE(( "wksp pointer %p", (void*)topo->workspaces[ obj->wksp_id ].wksp ));
    /* ~equivalent to fd_topo_wksp_new in a world of real workspaces */
    if( FD_UNLIKELY( cb->new ) ) { /* only saw this null for tiles */
      cb->new( topo, obj );
    }
    if( FD_UNLIKELY( 0== strcmp( obj->name, "keyswitch" ) ) ) {
      keyswitch_obj_id = obj->id;
    }
    // TODO add ASAN and MSAN poisoned memory before and after
  }

  /* The rest of this function an adoption of fd_topo_wksp_fill without
     the wksp id checks.  I haven't looked into why they are needed */
  for( ulong i=0UL; i<topo->link_cnt; i++ ) {
    fd_topo_link_t * link = &topo->links[ i ];
    link->mcache = fd_mcache_join( fd_topo_obj_laddr( topo, link->mcache_obj_id ) );
#ifdef FD_HAS_FUZZ /* TODO now basically everything needs FUZZ */
    link->mcache->hook = fd_drv_publish_hook;
#endif
    FD_TEST( link->mcache );
    /* only saw this false for tile code */
    if( FD_LIKELY( link->mtu ) ) {
      link->dcache = fd_dcache_join( fd_topo_obj_laddr( topo, link->dcache_obj_id ) );
      FD_TEST( link->dcache );
    }
  }

  for( ulong i=0UL; i<topo->tile_cnt; i++ ) {
    fd_topo_tile_t * tile = &topo->tiles[ i ];
    tile->keyswitch_obj_id = keyswitch_obj_id;

    tile->metrics = fd_metrics_join( fd_topo_obj_laddr( topo, tile->metrics_obj_id ) );
    FD_TEST( tile->metrics );

    for( ulong j=0UL; j<tile->in_cnt; j++ ) {
      tile->in_link_fseq[ j ] = fd_fseq_join( fd_topo_obj_laddr( topo, tile->in_link_fseq_obj_id[ j ] ) );
      FD_TEST( tile->in_link_fseq[ j ] );
    }
  }
}


static fd_topo_run_tile_t *
find_run_tile( fd_drv_t * drv, char * name ) {
  for( ulong i=0UL; drv->tiles[ i ]; i++ ) {
    if( 0==strcmp( name, drv->tiles[ i ]->name ) ) return drv->tiles[ i ];
  }
  FD_LOG_ERR(( "tile %s not found", name ));
}


static fd_topo_tile_t *
find_topo_tile( fd_drv_t * drv, char * name ) {
  for( ulong i=0UL; drv->config.topo.tile_cnt; i++ ) {
    if( 0==strcmp( name, drv->config.topo.tiles[ i ].name ) ) return &drv->config.topo.tiles[ i ];
  }
  FD_LOG_ERR(( "tile %s not found", name ));
}

static fd_topo_run_tile_t *
tile_topo_to_run( fd_drv_t * drv, fd_topo_tile_t * topo_tile ) {
  return find_run_tile( drv, topo_tile->name );
}



static void
init_tiles( fd_drv_t * drv ) {
  for( ulong i=0UL; i<drv->config.topo.tile_cnt; i++ ) {
    /* TODO Hack fix for shred_topo: move to isolated_shred_topo */
    // if( FD_UNLIKELY( 0==strcmp( drv->config.topo.tiles[i].name, "replay" ))) {
    //   continue;
    // }
    fd_topo_tile_t * topo_tile = &drv->config.topo.tiles[ i ];
    fd_topo_run_tile_t * run_tile = tile_topo_to_run( drv, topo_tile );
    run_tile->privileged_init( &drv->config.topo, topo_tile );
    run_tile->unprivileged_init( &drv->config.topo, topo_tile );
    // fd_metrics_register( topo_tile->metrics ); // TODO check if this is correct in a one thread world
  }
}

static void
isolated_quic_topo( config_t * config, fd_topo_obj_callbacks_t * callbacks[] ) {
  fd_topo_t * topo = &config->topo;
  fd_topob_new( &config->topo, config->name );
  ulong quic_tile_cnt   = config->layout.quic_tile_count;
  ulong net_tile_cnt    = config->layout.net_tile_count;  
  ulong verify_tile_cnt = config->layout.verify_tile_count;	
  fd_topob_wksp( topo, "quic"         );
  fd_topob_wksp( topo, "verify"       );
  fd_topob_wksp( topo, "net_send"     );
  fd_topob_wksp( topo, "net_quic"     );
  fd_topob_wksp( topo, "quic_verify"  );

#define FOR(cnt) for( ulong i=0UL; i<cnt; i++ )  
/**/                 fd_topob_link( topo, "send_net",     "net_send",     config->net.ingress_buffer_size,          FD_NET_MTU,                    2UL ); /* TODO: 2 is probably not correct, should be 1 */
FOR(quic_tile_cnt)   fd_topob_link( topo, "quic_net",     "net_quic",     config->net.ingress_buffer_size,          FD_NET_MTU,                    1UL );
FOR(quic_tile_cnt)   fd_topob_link( topo, "quic_verify",  "quic_verify",  config->tiles.verify.receive_buffer_size, FD_TPU_REASM_MTU,              config->tiles.quic.txn_reassembly_count );


FOR(net_tile_cnt) fd_topos_net_rx_link( topo, "net_send",   i, config->net.ingress_buffer_size );
FOR(net_tile_cnt) fd_topos_net_rx_link( topo, "net_quic",   i, config->net.ingress_buffer_size );


FOR(quic_tile_cnt) for( ulong j=0UL; j<net_tile_cnt; j++ )
										fd_topob_tile_in(     topo, "quic",    i,            "metric_in", "net_quic",     j,            FD_TOPOB_UNRELIABLE, FD_TOPOB_POLLED ); /* No reliable consumers of networking fragments, may be dropped or overrun */

FOR(verify_tile_cnt) for( ulong j=0UL; j<quic_tile_cnt; j++ )
											fd_topob_tile_in(    topo, "verify",  i,            "metric_in", "quic_verify",  j,            FD_TOPOB_UNRELIABLE, FD_TOPOB_POLLED );
											 										
/**/                 fd_topob_tile_out(   topo, "send",    0UL,                       "send_net",     0UL                                                );

FOR(quic_tile_cnt)  fd_topob_tile_out(    topo, "quic",    i,                         "quic_verify",  i                                                  );
FOR(quic_tile_cnt)  fd_topob_tile_out(    topo, "quic",    i,                         "quic_net",     i                                                  );



  for( ulong i=0UL; i<topo->tile_cnt; i++ ) fd_topo_configure_tile( &topo->tiles[ i ], config );

  FOR(net_tile_cnt) fd_topos_net_tile_finish( topo, i );
  fd_topob_finish( topo, callbacks );
}

void
fd_drv_init( fd_drv_t * drv,
             char* topo_name ) {
    fd_config_t * config = &drv->config;

    strcpy( config->name, "tile_quic_driver" );
    back_wksps( &config->topo, drv->callbacks );
    FD_LOG_NOTICE(( "tile cnt: %lu", config->topo.tile_cnt ));
    init_tiles( drv );
}
