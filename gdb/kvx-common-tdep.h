#ifndef _KVX_COMMON_TDEP_
#define _KVX_COMMON_TDEP_

#include "dwarf2/frame.h"
#include "gdbsupport/common-regcache.h"
#include "dis-asm.h"
#include "opcode/kvx.h"
#include "gdbtypes.h"
#include "gdbarch.h"

#define PS_ET_BIT 2
#define PS_MME_BIT 11
#define PS_V64_BIT 16
#define KVX_STACK_ALIGN_BYTES 8ULL
#define KVX_STACK_ALIGN_MASK ~(KVX_STACK_ALIGN_BYTES - 1)

#define RV_SP_REG 2
#define KVX_SP_REG 12
#define RV_SP_FROM_KVX_REG(kvx_regnum) (kvx_regnum - KVX_SP_REG + RV_SP_REG)

enum KVX_ARCH
{
  KVX_KV3_1,
  KVX_KV3_2,
  KVX_KV4_1,
  KVX_NUM_ARCHES
};

struct kvx_gdbarch_tdep : gdbarch_tdep_base
{
  int ev_regnum;
  int ls_regnum;
  int le_regnum;
  int lc_regnum;
  int ps_regnum;
  int ra_regnum;
  int cs_regnum;
  int spc_regnum;
  int syo_regnum;
  int ea_pl0_regnum;
  int es_pl0_regnum;
  int ev_pl0_regnum;
  int r0_regnum;
  int srf_offset;

  int local_regnum;
  struct type *uint256;
  struct type *uint512;
  struct type *uint1024;
};

struct op_list
{
  struct kvx_opc *op;
  struct op_list *next;
  int rv;
};

extern enum KVX_ARCH kvx_current_arch;
extern const struct frame_unwind kvx_frame_unwind;
extern struct op_list *branch_insns[KVX_NUM_ARCHES];
extern struct op_list *pcrel_insn[KVX_NUM_ARCHES];
extern uint32_t break_op[KVX_NUM_ARCHES];
extern uint32_t break_jtag_over_iss[KVX_NUM_ARCHES];
extern uint32_t nop_op[KVX_NUM_ARCHES];

enum KVX_ARCH
kvx_arch (void);
int
get_kvx_arch (void);
void
kvx_look_for_insns (void);
const char *
kvx_pseudo_register_name (struct gdbarch *gdbarch, int regnr);
struct type *
kvx_pseudo_register_type (struct gdbarch *gdbarch, int reg_nr);
int
kvx_pseudo_register_reggroup_p (struct gdbarch *gdbarch, int regnum,
				const struct reggroup *reggroup);
enum register_status
kvx_pseudo_register_read (struct gdbarch *gdbarch,
			  struct readable_regcache *regcache, int regnum,
			  gdb_byte *buf);
void
kvx_pseudo_register_write (struct gdbarch *gdbarch, struct regcache *regcache,
			   int regnum, const gdb_byte *buf);
int
kvx_dwarf2_reg_to_regnum (struct gdbarch *gdbarch, int reg);
int
kvx_num_pseudos (struct gdbarch *);
const char *
kvx_pc_name (struct gdbarch *);
const char *
kvx_sp_name (struct gdbarch *);
const char *
kvx_dummy_register_name (struct gdbarch *gdbarch, int regno);
struct type *
kvx_dummy_register_type (struct gdbarch *gdbarch, int regno);
void
kvx_dwarf2_frame_init_reg (struct gdbarch *gdbarch, int regnum,
			   struct dwarf2_frame_state_reg *reg,
			   frame_info_ptr this_frame);
enum return_value_convention
kvx_return_value (struct gdbarch *gdbarch, struct value *func_type,
		  struct type *type, struct regcache *regcache,
		  gdb_byte *readbuf, const gdb_byte *writebuf);
struct frame_id
kvx_dummy_id (struct gdbarch *gdbarch, frame_info_ptr this_frame);
CORE_ADDR
kvx_push_dummy_call (struct gdbarch *gdbarch, struct value *function,
		     struct regcache *regcache, CORE_ADDR bp_addr, int nargs,
		     struct value **args, CORE_ADDR sp,
		     function_call_return_method return_method,
		     CORE_ADDR struct_addr);
CORE_ADDR
kvx_skip_prologue (struct gdbarch *gdbarch, CORE_ADDR func_addr);
CORE_ADDR
kvx_unwind_pc (struct gdbarch *gdbarch, frame_info_ptr next_frame);
CORE_ADDR
kvx_adjust_breakpoint_address (struct gdbarch *gdbarch, CORE_ADDR bpaddr);
int
kvx_print_insn (bfd_vma pc, disassemble_info *di);
int
kvx_get_longjmp_target (frame_info_ptr frame, CORE_ADDR *pc);
void
set_parse_compact_asm (bool v);
int
get_kvx_target_core_vers (int *target_ver, int *core_ver);
int
kvx_is_vaddr_rv (CORE_ADDR vaddr, struct gdbarch **rv_gdbarch, bfd **rv_bfd);
int
kvx_crt_exe_is_rv_usr_only (void);
struct gdbarch *
kvx_get_rv_arch_from_elfs (void);
#endif
