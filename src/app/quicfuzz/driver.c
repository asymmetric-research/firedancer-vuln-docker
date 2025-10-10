#include "driver.h"
#include "../../disco/metrics/fd_metrics.h"
#include "../../disco/net/fd_net_tile.h" /* fd_topos_net_tiles */
#include "../../disco/net/fd_net_tile.h"
#include "../fdctl/config.h"
#include "../fdctl/topology.h"

extern fd_topo_run_tile_t fd_tile_quic;
extern fd_topo_run_tile_t fd_tile_verify;
extern fd_topo_run_tile_t fd_tile_net;



FD_FN_CONST ulong
fd_drv_footprint( void ) {
  return sizeof(fd_drv_t);
}

FD_FN_CONST ulong
fd_drv_align( void ) {
  return alignof(fd_drv_t);
}


void *
fd_drv_new( void * shmem, fd_topo_run_tile_t ** tiles, fd_topo_obj_callbacks_t ** callbacks ) {
  fd_drv_t * drv = (fd_drv_t *)shmem;
  drv->tiles = tiles;
  drv->callbacks = callbacks;
  drv->config = (fd_config_t){0};
  return drv;
}

fd_drv_t *
fd_drv_join( void * shmem ) {
  return (fd_drv_t *) shmem;
}

void *
fd_drv_leave( fd_drv_t * drv ) {
  return (void *) drv;
}

void *
fd_drv_delete( void * shmem ) {
  // TODO dealoc obj mem
  return shmem;
}


void
fd_drv_publish_hook( fd_frag_meta_t const * mcache ) {
  FD_LOG_NOTICE(( "fd_drv_publish_hook received chunk of size %u", mcache->sz ));
  /* relay to another tile using the send function, validate data, or
     ignore */
}


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
    // run_tile->privileged_init( &drv->config.topo, topo_tile );
    run_tile->unprivileged_init( &drv->config.topo, topo_tile );
    // fd_metrics_register( topo_tile->metrics ); // TODO check if this is correct in a one thread world
  }
}

void
fd_topo_configure_tile( fd_topo_tile_t * tile,
                        fd_config_t *    config ) {
	FD_LOG_INFO(("TILE NAME %s", tile->name));
  if( FD_UNLIKELY( !strcmp( tile->name, "quic" ) ) ) {													
		tile->quic.reasm_cnt                      = config->tiles.quic.txn_reassembly_count;
		tile->quic.out_depth                      = config->tiles.verify.receive_buffer_size;
		tile->quic.max_concurrent_connections     = config->tiles.quic.max_concurrent_connections;
		tile->quic.max_concurrent_handshakes      = config->tiles.quic.max_concurrent_handshakes;
		tile->quic.quic_transaction_listen_port   = config->tiles.quic.quic_transaction_listen_port;
		tile->quic.idle_timeout_millis            = config->tiles.quic.idle_timeout_millis;
		tile->quic.ack_delay_millis               = config->tiles.quic.ack_delay_millis;
		tile->quic.retry                          = config->tiles.quic.retry;
		fd_cstr_fini( fd_cstr_append_cstr_safe( fd_cstr_init( tile->quic.key_log_path ), config->tiles.quic.ssl_key_log_file, sizeof(tile->quic.key_log_path) ) );
	}
  else if( FD_UNLIKELY( !strcmp( tile->name, "sock" ) ) ) {

  	tile->sock.net.bind_address = config->net.bind_address_parsed;

  	if( FD_UNLIKELY( config->net.socket.receive_buffer_size>INT_MAX ) ) FD_LOG_ERR(( "invalid [net.socket.receive_buffer_size]" ));
  	if( FD_UNLIKELY( config->net.socket.send_buffer_size   >INT_MAX ) ) FD_LOG_ERR(( "invalid [net.socket.send_buffer_size]" ));
  	tile->sock.so_rcvbuf = (int)config->net.socket.receive_buffer_size;
  	tile->sock.so_sndbuf = (int)config->net.socket.send_buffer_size   ;
	
	}
													
}

static void
isolated_quic_topo( config_t * config, fd_topo_obj_callbacks_t * callbacks[] ) {
  fd_topo_t * topo = &config->topo;

  fd_topob_new( &config->topo, config->name );

  ulong quic_tile_cnt   = 1; //config->layout.quic_tile_count;
  ulong net_tile_cnt    = 1; //config->layout.net_tile_count;  
	
	(void)quic_tile_cnt;
	(void)net_tile_cnt;
	(void)callbacks;
	
	fd_topob_wksp( topo, "sock" );
	fd_topob_wksp( topo, "quic"         );
  fd_topob_wksp( topo, "quic_verify"  );
  fd_topob_wksp( topo, "metric_in"    );	
	fd_topob_wksp( topo, "net_quic"    );	
	fd_topob_wksp( topo, "net_umem"    );

	fd_topob_wksp( topo, "void"    );


#define FOR(cnt) for( ulong i=0UL; i<cnt; i++ )  

	FD_LOG_INFO(("CONFIGURING QUIC TILE"));

	FOR(quic_tile_cnt)   fd_topob_tile( topo, "quic",    "quic",    "metric_in",  0, 0,        0 );

	// fd_topos_net_tiles( topo, net_tile_cnt, &config->net, 0, 0, 0, tile_to_cpu );
  FOR(net_tile_cnt) fd_topob_tile( topo, "sock", "sock", "metric_in",0, 0, 0 );
	
	FD_LOG_INFO(("CONFIGURING LINKS"));
	fd_topob_tile( topo, "void", "void", "metric_in",0, 0, 0 );


FOR(quic_tile_cnt)   fd_topob_link( topo, "quic_net",     "net_quic",     config->net.ingress_buffer_size,          FD_NET_MTU,                    1UL );
FOR(quic_tile_cnt)   fd_topob_link( topo, "quic_verify",  "quic_verify",  config->tiles.verify.receive_buffer_size, FD_TPU_REASM_MTU,              config->tiles.quic.txn_reassembly_count );

FOR(net_tile_cnt) fd_topos_net_rx_link( topo, "net_quic",   i, config->net.ingress_buffer_size );

FOR(quic_tile_cnt)  fd_topos_tile_in_net( topo,                          "metric_in", "quic_net",     i,            FD_TOPOB_UNRELIABLE, FD_TOPOB_POLLED ); /* No reliable consumers of networking fragments, may be dropped or overrun */


// FOR(quic_tile_cnt) for( ulong j=0UL; j<net_tile_cnt; j++ )
// 										fd_topob_tile_in(     topo, "quic",    i,            "metric_in", "net_quic",     j,            FD_TOPOB_UNRELIABLE, FD_TOPOB_POLLED ); /* No reliable consumers of networking fragments, may be dropped or overrun */

FOR(net_tile_cnt) for( ulong j=0UL; j<quic_tile_cnt; j++ )
											fd_topob_tile_in(    topo, "void",  i,            "metric_in", "quic_verify",  j,            FD_TOPOB_UNRELIABLE, FD_TOPOB_POLLED ); /* No reliable consumers, verify tiles may be overrun */											 										

FOR(quic_tile_cnt)  fd_topob_tile_out(    topo, "quic",    i,                         "quic_verify",  i                                                  );
FOR(quic_tile_cnt)  fd_topob_tile_out(    topo, "quic",    i,                         "quic_net",     i                                                  );


	FD_LOG_INFO(("CONFIGURING TILES %lu", topo->tile_cnt));
  for( ulong i=0UL; i<topo->tile_cnt; i++ ) fd_topo_configure_tile( &topo->tiles[ i ], config );

	FD_LOG_INFO(("FINISHING"));

  fd_topob_finish( topo, callbacks );
	config->topo = *topo;	

}

void
fd_drv_init( fd_drv_t * drv ) {
	(void)back_wksps;
	fd_config_t * config = &drv->config;
	isolated_quic_topo( config, drv->callbacks );	
	FD_LOG_INFO(("ISOLATED TOPO CREATED"));
	back_wksps( &config->topo, drv->callbacks );
	FD_LOG_INFO(( "tile cnt: %lu", config->topo.tile_cnt ));
	init_tiles( drv );
}

FD_FN_UNUSED void
fd_drv_housekeeping( fd_drv_t * drv,
                     char * tile_name,
										 int backpressured) {
	(void)backpressured;
	fd_topo_tile_t *     topo_tile = find_topo_tile( drv, tile_name );
	(void)topo_tile;

}






