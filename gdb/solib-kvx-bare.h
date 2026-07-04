#ifndef _SOLIB_KVX_BARE_H_
#define _SOLIB_KVX_BARE_H_

#include "solib.h"
#include "inferior.h"

extern struct target_so_ops kvx_bare_solib_ops;

void
kvx_bare_solib_load_debug_info (void);
const char *
get_cluster_name (struct inferior *inf);
int
kvx_is_mmu_enabled (struct gdbarch *gdbarch, struct regcache *regs);
int
kvx_bare_get_core_range (CORE_ADDR *iter_ctx, CORE_ADDR *bottom,
			 CORE_ADDR *top);

#endif // _SOLIB_KVX_BARE_H_
