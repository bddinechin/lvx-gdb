#ifndef KVX_ATTACH_H
#define KVX_ATTACH_H

#include "defs.h"
#include "gdbsupport/common-utils.h"

#include "arch-utils.h"
#include "breakpoint.h"
#include "objfiles.h"

typedef void (*bp_reached_cb) (struct gdbarch *, struct bpstat *, int);

extern void
kvx_attach_set_opt (bool opt, const char *bp_name, enum bfd_architecture arch);
extern void
kvx_attach_new_objfile (const char *bp_name, bool opt,
			enum bfd_architecture arch, struct objfile *objf);
extern void
kvx_attach_normal_stop (const char *bp_name, bool opt, bp_reached_cb cb,
			enum bfd_architecture exp_arch, struct bpstat *bs,
			int print_frame);
extern bool
kvx_attach_bp_is_ours (const char *bp_name, breakpoint *b);
extern int
kvx_attach_search_bp (const char *bp_name);
extern void
kvx_attach_remove_bp (const char *bp_name);
extern bool
kvx_attach_search_fc_in_objfile (const char *function, struct objfile *objf,
				 enum bfd_architecture arch);
extern void
kvx_attach_add_breakpoint (const char *bp_name);
extern bool
kvx_attach_search_fc_in_all_infs (const char *function,
				  enum bfd_architecture arch);

#endif /* KVX_ATTACH_H */
