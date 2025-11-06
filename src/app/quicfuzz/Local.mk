
$(call add-hdrs,driver.h)
$(call add-hdrs,shmem.h)

ifdef FD_HAS_FUZZ
LDFLAGS += -Wl,--wrap=__sanitizer_cov_trace_pc_guard_init
LDFLAGS += -Wl,--wrap=__sanitizer_cov_trace_pc_guard
$(call add-objs,libafl_cov,fd_quicfuzz)
endif

ifdef FD_HAS_FIRESTARTER
$(call add-hdrs,firestarter.h)
$(call add-objs,firestarter,fd_quicfuzz)
endif 


$(call add-objs,shmem,fd_quicfuzz)
$(call add-objs,driver,fd_quicfuzz)
$(call add-objs,config,fd_quicfuzz)



$(call make-bin,fd_quicfuzz,main,fd_quicfuzz fd_firedancer fddev_shared fdctl_shared fdctl_platform fd_discof fd_disco fd_choreo fd_flamenco fd_funk fd_quic fd_tls fd_reedsol fd_ballet fd_waltz fd_tango fd_util firedancer_version fd_fdctl)

