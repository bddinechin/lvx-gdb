#ifndef _KVX_TARGET_H_
#define _KVX_TARGET_H_

struct kvx_inferior_data
{
  const char *cluster;
  CORE_ADDR gdb_os_init_done_addr;
  int booted;
  int sym_file_loaded;
  int cluster_break_on_spawn;
  int cluster_stop_all;
  int cluster_debug_ring;
  int unified;
  int os_init_done;
  uint32_t saved_os_init_done_syl;
  unsigned int intercept_trap;
};

extern int after_first_resume;
extern int wait_os_init_done;
extern bool opt_cont_os_init_done;
extern char cjtag_over_iss;

void
_initialize_kvx_target (void);
void
send_cluster_break_on_spawn (struct inferior *inf, int v);
void
send_intercept_trap (struct inferior *inf, unsigned int v);
void
send_cluster_stop_all (struct inferior *inf, int v);
void
send_cluster_debug_ring (struct inferior *inf, int v);
char
get_jtag_over_iss (void);
void
enable_ps_v64_at_boot (struct regcache *regs);
int
kvx_prepare_os_init_done (void);
int
sync_insert_remove_breakpoint (CORE_ADDR addr, int len, const gdb_byte *bp_op);
int
get_kvx_arch (void);
int get_kvx_target_core_vers (int *target_ver, int *core_ver);

kvx_inferior_data *
mppa_inferior_data (struct inferior *inf);

int
read_memory_no_dcache (uint64_t addr, unsigned char *gdb_byte, int len);
void
set_parse_compact_asm (bool v);

#endif //_KVX_TARGET_H_
