#ifndef _SOLIB_LVX_BARE_H_
#define _SOLIB_LVX_BARE_H_

#include "solib.h"
#include "inferior.h"

extern struct target_so_ops lvx_bare_solib_ops;

void
lvx_bare_solib_load_debug_info (void);
const char *
get_cluster_name (struct inferior *inf);
int
lvx_is_mmu_enabled (struct gdbarch *gdbarch, struct regcache *regs);
int
lvx_bare_get_core_range (CORE_ADDR *iter_ctx, CORE_ADDR *bottom,
			 CORE_ADDR *top);

#endif // _SOLIB_LVX_BARE_H_
