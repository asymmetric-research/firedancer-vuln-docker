#include "driver.h"
#include "../../disco/metrics/fd_metrics.h"
#include "../../disco/net/fd_net_tile.h" /* fd_topos_net_tiles */
#include "../../disco/topo/fd_cpu_topo.h"
#include "../fdctl/config.h"
#include "../fdctl/topology.h"
#include "../shared/commands/run/run.h"
#include "../../util/tile/fd_tile_private.h" /* fd_tile_private_cpus_parse */

#include <assert.h>
#include <unistd.h> /* pause */

#if !FD_HAS_HOSTED
#error "This target requires FD_HAS_HOSTED"
#endif

extern fd_topo_run_tile_t fd_tile_quic;
extern fd_topo_run_tile_t fd_tile_verify;
extern fd_topo_run_tile_t fd_tile_net;

fd_topo_run_tile_t
fdctl_tile_run( fd_topo_tile_t const * tile );

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

void
isolated_quic_topo( fd_drv_t * drv ) {
	fd_config_t * config = &drv->config;
  FD_LOG_INFO(("config name : %s", config->name));
  fd_topo_t * topo = &config->topo;

  // ulong tile_to_cpu[ FD_TILE_MAX ] = {0}; // required by net helpers
  ushort parsed_tile_to_cpu[ FD_TILE_MAX ];
  for( ulong i=0UL; i<FD_TILE_MAX; i++ ) parsed_tile_to_cpu[ i ] = USHORT_MAX;

  fd_topo_cpus_t cpus[1];
  fd_topo_cpus_init( cpus );

  ulong affinity_tile_cnt = 2UL;
  // if( FD_LIKELY( !is_auto_affinity ) ) affinity_tile_cnt = fd_tile_private_cpus_parse( affinity, parsed_tile_to_cpu );

  ulong tile_to_cpu[ FD_TILE_MAX ] = {0};
  for( ulong i=0UL; i<FD_TILE_MAX; i++ ) tile_to_cpu[ i ] = 0;

  for( ulong i=0UL; i<affinity_tile_cnt; i++ ) {
    if( FD_UNLIKELY( parsed_tile_to_cpu[ i ]!=USHORT_MAX && parsed_tile_to_cpu[ i ]>=cpus->cpu_cnt ) )
      FD_LOG_ERR(( "The CPU affinity string in the configuration file under [layout.affinity] specifies a CPU index of %hu, but the system "
                   "only has %lu CPUs. You should either change the CPU allocations in the affinity string, or increase the number of CPUs "
                   "in the system.",
                   parsed_tile_to_cpu[ i ], cpus->cpu_cnt ));
    tile_to_cpu[ i ] = fd_ulong_if( parsed_tile_to_cpu[ i ]==USHORT_MAX, ULONG_MAX, (ulong)parsed_tile_to_cpu[ i ] );
  }

  fd_topob_new( &config->topo, config->name );
  
  ulong quic_tile_cnt   = 1; 
  ulong net_tile_cnt    = 1;  
  
  // char const * affinity = config->layout.affinity;
  // int is_auto_affinity = !strcmp( affinity, "auto" );
	// if( FD_LIKELY( is_auto_affinity ) ) fd_topob_auto_layout( topo, 0 );

  fd_topob_wksp( topo, "metric_in" );

#define FOR(cnt) for( ulong i=0UL; i<cnt; i++ )  


  assert(strcmp("socket",config->net.provider) == 0);
  /**
   * FD helper for creating:
   * - wksp net_umem and sock
   * - sock tile
   */
  // fd_topos_net_tiles( topo, net_tile_cnt, &config->net, 1, 1, 1, tile_to_cpu );
  fd_topob_wksp( topo, "net_umem" );
  fd_topob_wksp( topo, "sock" );
  fd_topob_tile( topo, "sock", "sock", "metric_in",tile_to_cpu[ topo->tile_cnt ], 0, 0 );

  fd_topob_wksp( topo, "quic");
  fd_topob_wksp( topo, "quic_net");
FOR(quic_tile_cnt)   fd_topob_tile( topo, "quic","quic","metric_in", tile_to_cpu[ topo->tile_cnt ], 0,0 );
FOR(quic_tile_cnt) fd_topob_link( topo, "quic_net", "quic_net", config->net.ingress_buffer_size, FD_NET_MTU, 64 );
FOR(quic_tile_cnt) fd_topob_tile_out(    topo, "quic",i,"quic_net",  i);
/**
 * adds `quic_net` as link in to tile `sock`
 */
FOR(quic_tile_cnt)  fd_topos_tile_in_net( topo,"metric_in", "quic_net",i,FD_TOPOB_UNRELIABLE, FD_TOPOB_POLLED );

/**
 * creates link `net_quic` in wksp `net_umem` 
 * adds `net_quic` as tile `sock` out
 */
FOR(net_tile_cnt) fd_topos_net_rx_link( topo, "net_quic",i, config->net.ingress_buffer_size );
FOR(net_tile_cnt) fd_topob_tile_in( topo, "quic", i, "metric_in", "net_quic", i, FD_TOPOB_UNRELIABLE, FD_TOPOB_POLLED );

//quic link out to verify - use tricks to add link with no consumers
FOR(quic_tile_cnt) fd_topob_link( topo, "quic_verify", "quic", config->net.ingress_buffer_size, FD_NET_MTU, 64 );
fd_link_permit_no_consumers(topo, "quic_verify");
FOR(quic_tile_cnt) fd_topob_tile_out( topo, "quic",i,"quic_verify",  i);

//   fd_topob_wksp( topo, "verify");
// FOR(quic_tile_cnt)   fd_topob_tile( topo, "verify","verify","metric_in", tile_to_cpu[ topo->tile_cnt ], 0,0 );
// FOR(quic_tile_cnt) fd_topob_link( topo, "quic_verify", "quic", config->net.ingress_buffer_size, FD_NET_MTU, 64 );
// FOR(quic_tile_cnt) fd_topob_tile_out( topo, "quic",i,"quic_verify",  i);
// FOR(quic_tile_cnt) fd_topob_tile_in( topo, "verify", i, "metric_in", "quic_verify", i, FD_TOPOB_UNRELIABLE, FD_TOPOB_POLLED );


  for( ulong i=0UL; i<topo->tile_cnt; i++ ) fd_topo_configure_tile( &topo->tiles[ i ], config );

FOR(net_tile_cnt) fd_topos_net_tile_finish( topo, i );


  // fd_topob_auto_layout( topo, 0 );
  // topo->agave_affinity_cnt = 0;
  fd_topob_finish( topo, drv->callbacks );
  fd_topo_print_log( /* stdout */ 1, topo );
}

void
fd_drv_init( fd_drv_t * drv ) {
	
  fd_config_t* conf = &drv->config;

  char * shmem_args[ 3 ];
  /* pass in --shmem-path value from the config */
  shmem_args[ 0 ] = "--shmem-path";
  shmem_args[ 1 ] = conf->hugetlbfs.mount_path;
  shmem_args[ 2 ] = NULL;
  char ** argv = shmem_args;
  int     argc = 2;

  fd_shmem_private_boot( &argc, &argv );
  fd_log_private_boot  ( &argc, &argv );
  fd_tile_private_boot  ( &argc, &argv );

	isolated_quic_topo( drv);	
	FD_LOG_INFO(("ISOLATED TOPO CREATED"));
  configure_stage( &fd_cfg_stage_sysctl,CONFIGURE_CMD_INIT, conf );
  configure_stage( &fd_cfg_stage_hugetlbfs,CONFIGURE_CMD_INIT, conf );
  fdctl_check_configure( conf );
  initialize_workspaces(conf);
  initialize_stacks( conf );  
  fdctl_setup_netns( conf, 1 );  
  fd_topo_join_workspaces( &conf->topo, FD_SHMEM_JOIN_MODE_READ_WRITE );
  FD_LOG_INFO(( "tile cnt: %lu", conf->topo.tile_cnt ));
	// init_tiles( drv );  
  fd_topo_run_single_process( &drv->config.topo, 2, drv->config.uid,  drv->config.gid, fdctl_tile_run );
  for(;;) pause(); 
}






