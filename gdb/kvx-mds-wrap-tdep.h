#ifndef _KVX_MDS_WRAP_H_
#define _KVX_MDS_WRAP_H_

int
kvx_num_pseudos_wrap (struct gdbarch *gdbarch);
const char *
kvx_pseudo_wrap_register_name (struct gdbarch *gdbarch, int regnr);
struct type *
kvx_pseudo_wrap_register_type (struct gdbarch *gdbarch, int regnr);
enum register_status
kvx_pseudo_wrap_register_read (struct gdbarch *gdbarch,
			       struct readable_regcache *regcache, int regnum,
			       gdb_byte *buf);
void
kvx_pseudo_wrap_register_write (struct gdbarch *gdbarch,
				struct regcache *regcache, int regnum,
				const gdb_byte *buf);

#endif // _KVX_MDS_WRAP_H_
