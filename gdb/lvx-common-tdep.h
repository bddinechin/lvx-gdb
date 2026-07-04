#ifndef _LVX_COMMON_TDEP_
#define _LVX_COMMON_TDEP_

#include "dwarf2/frame.h"
#include "gdbsupport/common-regcache.h"
#include "dis-asm.h"
#include "opcode/lvx.h"
#include "gdbtypes.h"
#include "gdbarch.h"

#define PS_ET_BIT 2
#define PS_MME_BIT 11
#define PS_V64_BIT 16
#define LVX_STACK_ALIGN_BYTES 8ULL
#define LVX_STACK_ALIGN_MASK ~(LVX_STACK_ALIGN_BYTES - 1)

enum LVX_ARCH
{
  LVX_1,
  LVX_2,
  LVX_NUM_ARCHES
};

struct lvx_gdbarch_tdep : gdbarch_tdep_base
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
  struct lvx_opc *op;
  struct op_list *next;
};

extern enum LVX_ARCH lvx_current_arch;
extern const struct frame_unwind lvx_frame_unwind;
extern struct op_list *branch_insns[LVX_NUM_ARCHES];
extern struct op_list *pcrel_insn[LVX_NUM_ARCHES];
extern uint32_t break_op[LVX_NUM_ARCHES];
extern uint32_t break_jtag_over_iss[LVX_NUM_ARCHES];
extern uint32_t nop_op[LVX_NUM_ARCHES];

enum LVX_ARCH
lvx_arch (void);
int
get_lvx_arch (void);
void
lvx_look_for_insns (void);
const char *
lvx_pseudo_register_name (struct gdbarch *gdbarch, int regnr);
struct type *
lvx_pseudo_register_type (struct gdbarch *gdbarch, int reg_nr);
int
lvx_pseudo_register_reggroup_p (struct gdbarch *gdbarch, int regnum,
				 const struct reggroup *reggroup);
enum register_status
lvx_pseudo_register_read (struct gdbarch *gdbarch,
			   struct readable_regcache *regcache, int regnum,
			   gdb_byte *buf);
void
lvx_pseudo_register_write (struct gdbarch *gdbarch, struct regcache *regcache,
			    int regnum, const gdb_byte *buf);
int
lvx_dwarf2_reg_to_regnum (struct gdbarch *gdbarch, int reg);
int
lvx_num_pseudos (struct gdbarch *);
const char *
lvx_pc_name (struct gdbarch *);
const char *
lvx_sp_name (struct gdbarch *);
const char *
lvx_dummy_register_name (struct gdbarch *gdbarch, int regno);
struct type *
lvx_dummy_register_type (struct gdbarch *gdbarch, int regno);
void
lvx_dwarf2_frame_init_reg (struct gdbarch *gdbarch, int regnum,
			   struct dwarf2_frame_state_reg *reg,
			   frame_info_ptr this_frame);
enum return_value_convention
lvx_return_value (struct gdbarch *gdbarch, struct value *func_type,
		  struct type *type, struct regcache *regcache,
		  gdb_byte *readbuf, const gdb_byte *writebuf);
struct frame_id
lvx_dummy_id (struct gdbarch *gdbarch, frame_info_ptr this_frame);
CORE_ADDR
lvx_push_dummy_call (struct gdbarch *gdbarch, struct value *function,
		     struct regcache *regcache, CORE_ADDR bp_addr, int nargs,
		     struct value **args, CORE_ADDR sp,
		     function_call_return_method return_method,
		     CORE_ADDR struct_addr);
CORE_ADDR
lvx_skip_prologue (struct gdbarch *gdbarch, CORE_ADDR func_addr);
CORE_ADDR
lvx_unwind_pc (struct gdbarch *gdbarch, frame_info_ptr next_frame);
CORE_ADDR
lvx_adjust_breakpoint_address (struct gdbarch *gdbarch, CORE_ADDR bpaddr);
int
lvx_print_insn (bfd_vma pc, disassemble_info *di);
int
lvx_get_longjmp_target (frame_info_ptr frame, CORE_ADDR *pc);
void
set_parse_compact_asm (bool v);
int
get_lvx_target_core_vers (int *target_ver, int *core_ver);
#endif
