/* Target-dependent code for the Kalray KVX for GDB, the GNU debugger.

   Copyright (C) 2010 Kalray

   This file is part of GDB.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#include "defs.h"
#include <ctype.h>
#include "arch-utils.h"
#include "cli/cli-decode.h"
#include "event-top.h"
#include "frame-unwind.h"
#include "gdbcmd.h"
#include "gdbcore.h"
#include "regset.h"
#include "gcore.h"
#include "gcore-elf.h"
#include "progspace-and-thread.h"
#include "objfiles.h"
#include "reggroups.h"
#include "target-descriptions.h"
#include "remote.h"
#include "gdbsupport/event-loop.h"
#include "gdbsupport/rsp-low.h"
#include "observable.h"
#include "elf-bfd.h"
#include "source.h"

#include "kvx-target.h"
#include "gdbthread.h"
#include "kvx-common-tdep.h"
#include "kvx-mds-wrap-tdep.h"
#include "solib-kvx-bare.h"

#define RV_OPCODE_MASK 0x7F
#define RV_OPCODE_JAL 0x6F
#define RV_OPCODE_JALR 0x67
#define RV_OPCODE_BRANCH 0x63
#define RV_OPCODE_ECALL 0x73
#define RV_OPCODE_AUIPC 0x17

#define RV_USER_ONLY_STACK_SIZE (8 * 1024)
#define RV_USER_ONLY_ARGS_AREA_SIZE 1024
#define RV_USER_ONLY_DISPLACED_AREA_SZ 64

#define RV_BKP_KIND 0x10
#define BKP_LEN_FROM_BKP_KIND(_kind) ((_kind) & ~RV_BKP_KIND)
#define RV_MODE_FROM_BKP_KIND(_kind) (((_kind) & RV_BKP_KIND) != 0)
#define MAKE_BKP_KIND(_len, _rv_mode) ((_len) | ((_rv_mode) ? RV_BKP_KIND : 0))

static bool displaced_busy = false;
CORE_ADDR dummy_rv_bp_addr = -1;
static int dummy_rv_next = 0;

struct kvx_step_inferior_data
{
  kvx_step_inferior_data ()
  {
    step_pad_area = 0;
    step_pad_area_lma = 0;
    has_step_pad_area_p = 0;
    has_step_pad_area_lma_p = 0;
  }

  gdb::optional<displaced_step_buffers> disp_step_bufs;
  CORE_ADDR step_pad_area;
  CORE_ADDR step_pad_area_lma;
  int has_step_pad_area_p;
  int has_step_pad_area_lma_p;
};

struct kvx_displaced_step
{
  /* Take into account that ALUs might have extensions. */
  uint32_t insn_words[8];
  int num_insn_words;

  unsigned branchy : 1;
  unsigned scall_jump : 1;
  unsigned rewrite_RA : 1;
  unsigned rewrite_LE : 1;
  unsigned rewrite_reg : 2;
  unsigned riscv : 1;

  int has_pcrel;
  int pcrel_reg;
  int execute_inplace;

  /* The destination address when the branch is taken. */
  uint64_t dest;
  uint64_t reg_org;
  int reg;
};

struct kvx_step_inf_data_deleter
{
  void operator() (kvx_step_inferior_data *inf_data) { delete inf_data; }
};

static const registry<inferior>::key<kvx_step_inferior_data,
				     kvx_step_inf_data_deleter>
  kvx_step_inf_data_key;

static kvx_step_inferior_data *
kvx_inferior_data (struct inferior *inf)
{
  kvx_step_inferior_data *res = kvx_step_inf_data_key.get (inf);
  if (!res)
    {
      res = new kvx_step_inferior_data;
      kvx_step_inf_data_key.set (inf, res);
    }

  return res;
}

static CORE_ADDR
kvx_fetch_tls_load_module_address (struct objfile *objfile)
{
  struct regcache *regs = get_current_regcache ();
  ULONGEST val;
  kvx_gdbarch_tdep *tdep = gdbarch_tdep<kvx_gdbarch_tdep> (target_gdbarch ());

  regcache_raw_read_unsigned (regs, tdep->local_regnum, &val);
  return val;
}

int
kvx_is_mmu_enabled (struct gdbarch *gdbarch, struct regcache *regs)
{
  ULONGEST ps;
  kvx_gdbarch_tdep *tdep;

  if (gdbarch == NULL)
    gdbarch = target_gdbarch ();

  tdep = gdbarch_tdep<kvx_gdbarch_tdep> (gdbarch);

  if (regs == NULL)
    regs = get_current_regcache ();

  regcache_raw_read_unsigned (regs, tdep->ps_regnum, &ps);
  return (ps & (1 << PS_MME_BIT)) != 0;
}

void
enable_ps_v64_at_boot (struct regcache *regs)
{
  kvx_gdbarch_tdep *tdep;
  struct gdbarch *gdbarch;
  ULONGEST ps;
  bfd *exec_bfd;

  if (regcache_read_pc (regs) != 0)
    return;

  exec_bfd = current_program_space->exec_bfd ();
  if (exec_bfd && elf_elfheader (exec_bfd)->e_ident[EI_CLASS] != ELFCLASS64)
    return;

  gdbarch = target_gdbarch ();
  if (!gdbarch)
    return;
  tdep = gdbarch_tdep<kvx_gdbarch_tdep> (gdbarch);
  if (!tdep)
    return;

  regcache_raw_read_unsigned (regs, tdep->ps_regnum, &ps);

  // enable the 64bit mode
  if ((ps & (1 << PS_V64_BIT)) == 0)
    {
      ps |= (1 << PS_V64_BIT);
      regcache_raw_write_unsigned (regs, tdep->ps_regnum, ps);
    }
}

static displaced_step_prepare_status
kvx_displaced_step_prepare (struct gdbarch *gdbarch, thread_info *thread,
			    CORE_ADDR &displaced_pc)
{
  kvx_step_inferior_data *data = kvx_inferior_data (current_inferior ());

  if (displaced_busy)
    return DISPLACED_STEP_PREPARE_STATUS_UNAVAILABLE;
  displaced_busy = true;

  if (!data->has_step_pad_area_p)
    {
      struct bound_minimal_symbol msym;
      int rv_user_only = kvx_crt_exe_is_rv_usr_only ();
      const char *sym_name = rv_user_only ? "_end" : "_debug_start";

      msym = lookup_minimal_symbol (sym_name, NULL, NULL);
      if (msym.minsym == NULL)
	{
	  gdb_printf (_ ("Can not locate a suitable step pad area."));
	  return DISPLACED_STEP_PREPARE_STATUS_CANT;
	}
      if (msym.value_address () % 4)
	warning ("Step pad area is not 4-byte aligned.");
      data->step_pad_area = (msym.value_address () + 3) & ~0x3;
      data->has_step_pad_area_p = 1;

      if (rv_user_only)
	{
	  data->has_step_pad_area_lma_p = 1;
	  data->step_pad_area += RV_USER_ONLY_STACK_SIZE
				 + RV_USER_ONLY_ARGS_AREA_SIZE
				 - RV_USER_ONLY_DISPLACED_AREA_SZ;
	  data->step_pad_area_lma = data->step_pad_area;
	}
      else
	{
	  msym = lookup_minimal_symbol ("_debug_start_lma", NULL, NULL);
	  if (msym.minsym != NULL)
	    {
	      if (msym.value_address () % 4)
		warning ("Physical step pad area is not 4-byte aligned.");
	      data->step_pad_area_lma = (msym.value_address () + 3) & ~0x3;
	      data->has_step_pad_area_lma_p = 1;
	    }
	}
    }

  std::vector<CORE_ADDR> buffers;
  if (data->has_step_pad_area_lma_p && kvx_is_mmu_enabled (gdbarch, NULL))
    buffers.push_back (data->step_pad_area_lma);
  else
    buffers.push_back (data->step_pad_area);

  data->disp_step_bufs.emplace (buffers);

  return data->disp_step_bufs->prepare (thread, displaced_pc);
}

static bool
kvx_displaced_step_hw_singlestep (struct gdbarch *gdbarch)
{
  return true;
}

static displaced_step_finish_status
kvx_displaced_step_finish (gdbarch *arch, thread_info *thread, gdb_signal sig)
{
  kvx_step_inferior_data *data = kvx_inferior_data (current_inferior ());
  displaced_step_finish_status ret;

  ret = data->disp_step_bufs->finish (arch, thread, sig);
  displaced_busy = false;

  return ret;
}

static uint64_t
extract_mds_bitfield (struct kvx_opc *op, uint32_t syllab, int bitfield, int sign)
{
  struct kvx_bitfield *bfield;
  uint64_t res;

  bfield = &op->format[bitfield]->bfield[0];
  res = ((uint64_t) syllab >> bfield->to_offset) & ((1ULL << bfield->size) - 1);

  if (sign && (res & (1ULL << (bfield->size - 1))))
    res |= (0xffffffffffffffffULL << bfield->size);

  return res;
}

static uint64_t
extract_mds_multi_bitfield (struct kvx_opc *op, uint32_t syllab, int op_idx,
			    int sign)
{
  uint64_t res = 0;
  kvx_operand *o = op->format[op_idx];

  for (int i = 0; i < o->bitfields; i++)
    {
      struct kvx_bitfield *bfield = &o->bfield[i];
      res |= (((uint64_t) syllab >> bfield->to_offset)
	      & ((1ULL << bfield->size) - 1))
	     << bfield->from_offset;
    }

  if (sign && (res & (1ULL << (o->width - 1))))
    res |= (0xffffffffffffffffULL << o->width);

  res <<= o->shift;

  return res;
}

static void
patch_mds_bitfield (struct kvx_opc *op, uint32_t *syllab, int bitfield, int value)
{
  struct kvx_bitfield *bfield;
  uint32_t mask;

  bfield = &op->format[bitfield]->bfield[0];
  mask = ~(((1 << bfield->size) - 1) << bfield->to_offset);
  *syllab &= mask;
  *syllab |= (value << bfield->to_offset) & ~mask;
}

static void
patch_mds_multi_bitfield (struct kvx_opc *op, uint32_t *syllab, int op_idx,
			  int value)
{
  kvx_operand *o = op->format[op_idx];

  value >>= o->shift;
  for (int i = 0; i < o->bitfields; i++)
    {
      struct kvx_bitfield *bfield = &o->bfield[i];
      uint32_t mask = (1 << bfield->size) - 1;
      *syllab &= ~(mask << bfield->to_offset);
      *syllab |= ((value >> bfield->from_offset) & mask) << bfield->to_offset;
    }
}

void
send_cluster_break_on_spawn (struct inferior *inf, int v)
{
  char *buf;
  long size = 256;
  remote_target *rt = get_current_remote_target ();

  buf = (char *) malloc (size);
  sprintf (buf, "Qkalray.break_on_spawn:%dp%x.1", v, inf->pid);
  putpkt (rt, buf);
  getpkt (rt, &buf, &size, 0);
  free (buf);
}

void
send_intercept_trap (struct inferior *inf, unsigned int v)
{
  char *buf;
  long size = 256;
  remote_target *rt = get_current_remote_target ();

  buf = (char *) malloc (size);
  sprintf (buf, "Qkalray.intercept_hto_mask:%04xp%x.1", v, inf->pid);
  putpkt (rt, buf);
  getpkt (rt, &buf, &size, 0);
  if (!strcmp (buf, "KO"))
    printf (_ ("Error setting the trap intercepting mask.\n"));

  free (buf);
}

void
send_cluster_stop_all (struct inferior *inf, int v)
{
  char *buf;
  long size = 256;
  remote_target *rt = get_current_remote_target ();

  buf = (char *) malloc (size);
  sprintf (buf, "Qkalray.stop_all:%dp%x.1", v, inf->pid);
  putpkt (rt, buf);
  getpkt (rt, &buf, &size, 0);
  free (buf);
}

void
send_cluster_debug_ring (struct inferior *inf, int v)
{
  char *buf;
  long size = 256;
  remote_target *rt = get_current_remote_target ();

  buf = (char *) malloc (size);
  sprintf (buf, "Qkalray.cluster_debug_ring:%dp%x.1", v, inf->pid);
  putpkt (rt, buf);
  getpkt (rt, &buf, &size, 0);
  free (buf);
}

static void
kvx_enable_hbkp_on_cpu (CORE_ADDR addr, int enable)
{
  char *buf;
  long size = 256;
  remote_target *rt = get_current_remote_target ();

  buf = (char *) malloc (size);
  sprintf (buf, "Qkalray.enable_hbkp_on_cpu:0x%" PRIx64 ",%d", addr, enable);
  putpkt (rt, buf);
  getpkt (rt, &buf, &size, 0);
  free (buf);
}

int
read_memory_no_dcache (uint64_t addr, gdb_byte *user_buf, int len)
{
  char *buf;
  long size;
  int decoded_bytes;
  remote_target *rt = get_current_remote_target ();

  set_general_thread (rt, inferior_ptid);

  size = 256;
  buf = (char *) malloc (size);
  sprintf (buf, "qkalray.mem_no_cache:%" PRIx64 ",%d", addr, len);
  putpkt (rt, buf);
  getpkt (rt, &buf, &size, 0);

  if (buf[0] == 'E' && isxdigit (buf[1]) && isxdigit (buf[2]) && !buf[3])
    {
      free (buf);
      return 0;
    }

  decoded_bytes = hex2bin (buf, user_buf, len);
  free (buf);

  return decoded_bytes == len;
}

char
get_jtag_over_iss (void)
{
  char ret, *buf = (char *) malloc (256);
  long size = 256;
  remote_target *rt = get_current_remote_target ();

  strcpy (buf, "qkalray.jtag_over_iss");
  putpkt (rt, buf);
  getpkt (rt, &buf, &size, 0);
  ret = *buf;
  free (buf);

  return ret;
}

static void
patch_bcu_instruction (struct gdbarch *gdbarch, CORE_ADDR from, CORE_ADDR to,
		       struct regcache *regs, struct kvx_displaced_step *dsc,
		       int riscv_en)
{
  struct op_list *insn = branch_insns[kvx_arch ()];
  kvx_gdbarch_tdep *tdep = gdbarch_tdep<kvx_gdbarch_tdep> (gdbarch);

  if (debug_displaced)
    gdb_printf (_ ("displaced: Looking at BCU instruction\n"));

  /* In order to limit side effects, we patch every instruction or
     register in order to make branches target the step pad. This
     way we have a simple way to check if branches were taken or
     not. 'scalls' are an exception to this rule, because we don't
     want to change the 'ev' register that might influence other
     things than syscalls.
   */

  while (insn)
    {
      struct kvx_opc *op = insn->op;

      if (insn->rv != riscv_en
	  || (dsc->insn_words[0] & op->codewords[0].mask)
	       != op->codewords[0].opcode)
	{
	  insn = insn->next;
	  continue;
	}

      if (debug_displaced)
	gdb_printf (_ ("displaced: found branchy BCU insn: %s\n"), op->as_op);

      dsc->branchy = 1;
      dsc->riscv = riscv_en;

      if (strcmp ("call", op->as_op) == 0)
	{
	  dsc->rewrite_RA = 1;
	  dsc->dest
	    = from + extract_mds_bitfield (op, dsc->insn_words[0], 0, 1) * 4;
	  patch_mds_bitfield (op, &dsc->insn_words[0], 0, 0);
	}
      else if (strcmp ("jal", op->as_op) == 0)
	{
	  if (extract_mds_bitfield (op, dsc->insn_words[0], 0, 0)) // not J
	    dsc->rewrite_RA = 1;
	  dsc->dest
	    = from + extract_mds_multi_bitfield (op, dsc->insn_words[0], 1, 1);
	  patch_mds_multi_bitfield (op, &dsc->insn_words[0], 1, 0);
	}
      else if (strcmp ("goto", op->as_op) == 0)
	{
	  dsc->dest
	    = from + extract_mds_bitfield (op, dsc->insn_words[0], 0, 1) * 4;
	  patch_mds_bitfield (op, &dsc->insn_words[0], 0, 0);
	}
      else if (strcmp ("cb", op->as_op) == 0)
	{
	  dsc->dest
	    = from + extract_mds_bitfield (op, dsc->insn_words[0], 2, 1) * 4;
	  patch_mds_bitfield (op, &dsc->insn_words[0], 2, 0);
	}
      else if (!strcmp ("beq", op->as_op) || !strcmp ("bne", op->as_op)
	       || !strcmp ("blt", op->as_op) || !strcmp ("bltu", op->as_op)
	       || !strcmp ("bge", op->as_op) || !strcmp ("bgeu", op->as_op))
	{
	  dsc->dest
	    = from + extract_mds_multi_bitfield (op, dsc->insn_words[0], 2, 1);
	  patch_mds_multi_bitfield (op, &dsc->insn_words[0], 2, 0);
	}
      else if (strcmp ("icall", op->as_op) == 0)
	{
	  ULONGEST reg_value;

	  dsc->rewrite_RA = 1;
	  dsc->rewrite_reg = 1;
	  dsc->reg = extract_mds_bitfield (op, dsc->insn_words[0], 0, 0)
		     + tdep->r0_regnum;
	  regcache_raw_read_unsigned (regs, dsc->reg, &reg_value);
	  dsc->dest = reg_value;
	  regcache_raw_write_unsigned (regs, dsc->reg, to);
	}
      else if (strcmp ("jalr", op->as_op) == 0)
	{
	  ULONGEST reg_value;

	  if (extract_mds_bitfield (op, dsc->insn_words[0], 0,
				    0)) // not RET or JR
	    dsc->rewrite_RA = 1;
	  dsc->rewrite_reg = 2;
	  dsc->reg = extract_mds_bitfield (op, dsc->insn_words[0], 1, 0)
		     + tdep->r0_regnum;
	  regcache_raw_read_unsigned (regs, dsc->reg, &reg_value);
	  dsc->reg_org = reg_value;
	  dsc->dest
	    = reg_value + extract_mds_bitfield (op, dsc->insn_words[0], 2, 1);
	  dsc->dest &= ~1ULL;
	  patch_mds_bitfield (op, &dsc->insn_words[0], 2, 0);
	  regcache_raw_write_unsigned (regs, dsc->reg, to);
	}
      else if (strcmp ("igoto", op->as_op) == 0)
	{
	  ULONGEST reg_value;

	  dsc->rewrite_reg = 1;
	  dsc->reg = extract_mds_bitfield (op, dsc->insn_words[0], 0, 0)
		     + tdep->r0_regnum;
	  regcache_raw_read_unsigned (regs, dsc->reg, &reg_value);
	  dsc->dest = reg_value;
	  regcache_raw_write_unsigned (regs, dsc->reg, to);
	}
      else if (strcmp ("ret", op->as_op) == 0)
	{
	  ULONGEST reg_value;

	  dsc->rewrite_reg = 1;
	  dsc->reg = tdep->ra_regnum;
	  regcache_raw_read_unsigned (regs, dsc->reg, &reg_value);
	  dsc->dest = reg_value;
	  regcache_raw_write_unsigned (regs, dsc->reg, to);
	}
      else if (strcmp ("rfe", op->as_op) == 0)
	{
	  ULONGEST reg_value;

	  dsc->rewrite_reg = 1;
	  dsc->reg = tdep->spc_regnum;
	  regcache_raw_read_unsigned (regs, dsc->reg, &reg_value);
	  dsc->dest = reg_value;
	  regcache_raw_write_unsigned (regs, dsc->reg, to);
	}
      else if (strcmp ("scall", op->as_op) == 0)
	{
	  ULONGEST syo, ev, ps;
	  int scall_no, scall_slice, target_pl, crt_pl;

	  dsc->scall_jump = 1;
	  scall_no = extract_mds_bitfield (op, dsc->insn_words[0], 0, 0);
	  scall_slice = scall_no / 1024;
	  regcache_raw_read_unsigned (regs, tdep->syo_regnum, &syo);
	  target_pl = (syo >> (scall_slice * 2)) & 3;
	  regcache_raw_read_unsigned (regs, tdep->ps_regnum, &ps);
	  crt_pl = ps & 3;
	  if (crt_pl < target_pl)
	    target_pl = crt_pl;
	  regcache_raw_read_unsigned (regs, tdep->ev_pl0_regnum + target_pl,
				      &ev);
	  dsc->dest = (ev & ~0xFFULL) + 0x40 * 3;
	}
      else if (strcmp ("ecall", op->as_op) == 0)
	{
	  ULONGEST syo, ev, ps;
	  int scall_no, scall_slice, target_pl, crt_pl;

	  dsc->scall_jump = 1;
	  scall_no = 0;
	  scall_slice = scall_no / 1024;
	  regcache_raw_read_unsigned (regs, tdep->syo_regnum, &syo);
	  target_pl = (syo >> (scall_slice * 2)) & 3;
	  regcache_raw_read_unsigned (regs, tdep->ps_regnum, &ps);
	  crt_pl = ps & 3;
	  if (crt_pl < target_pl)
	    target_pl = crt_pl;
	  regcache_raw_read_unsigned (regs, tdep->ev_pl0_regnum + target_pl,
				      &ev);
	  dsc->dest = (ev & ~0xFFULL) + 0x40 * 3;
	}
      else if (strcmp ("loopdo", op->as_op) == 0
	       || strcmp ("loopgtz", op->as_op) == 0
	       || strcmp ("loopnez", op->as_op) == 0)
	{
	  dsc->rewrite_LE = 1;
	  dsc->dest
	    = from + extract_mds_bitfield (op, dsc->insn_words[0], 1, 1) * 4;
	  patch_mds_bitfield (op, &dsc->insn_words[0], 1, 0);
	}
      else if (strcmp ("get", op->as_op) == 0 && op->format[1])
	{
	  /* get version with immediate */
	  if (extract_mds_bitfield (op, dsc->insn_words[0], 1, 0)
	      != (gdbarch_pc_regnum (gdbarch) - tdep->srf_offset))
	    break;

	  dsc->branchy = 0;
	  dsc->rewrite_reg = 1;
	  dsc->reg
	    = (extract_mds_bitfield (op, dsc->insn_words[0], 0, 0) & 0x3F)
	      + tdep->r0_regnum;
	  dsc->dest = from;
	}
      else if (strcmp ("iget", op->as_op) == 0)
	{
	  ULONGEST reg, srf_reg;

	  /* indirect get instruction */
	  reg = (extract_mds_bitfield (op, dsc->insn_words[0], 0, 0) & 0x3F)
		+ tdep->r0_regnum;
	  regcache_raw_read_unsigned (regs, reg, &srf_reg);

	  if (srf_reg != (gdbarch_pc_regnum (gdbarch) - tdep->srf_offset))
	    break;

	  dsc->branchy = 0;
	  dsc->rewrite_reg = 1;
	  dsc->reg = reg;
	  dsc->dest = from;
	}
      else
	{
	  internal_error ("Unknwon BCU insn");
	}

      break;
    }
}

static ULONGEST
kvx_get_reg_field_value (struct regcache *regs, int regnum,
			 const char *field_name)
{
  struct gdbarch *gdbarch = regs->arch ();
  struct type *reg_type = register_type (gdbarch, regnum);
  ULONGEST reg_value;
  int i;

  regcache_raw_read_unsigned (regs, regnum, &reg_value);
  for (i = 0; i < reg_type->num_fields (); i++)
    {
      struct field &crt_field = reg_type->field (i);
      if (strcmp (field_name, crt_field.name ()))
	continue;
      reg_value >>= crt_field.loc_bitpos ();
      reg_value &= ((1 << FIELD_BITSIZE (crt_field)) - 1);
      return reg_value;
    }

  internal_error ("KVX regnum %d does not have the bitfield %s", regnum,
		  field_name);
}

static displaced_step_copy_insn_closure_up
kvx_displaced_step_copy_insn (struct gdbarch *gdbarch, CORE_ADDR from,
			      CORE_ADDR to, struct regcache *regs)
{
  size_t len = sizeof (kvx_displaced_step);
  std::unique_ptr<buf_displaced_step_copy_insn_closure> closure (
    new buf_displaced_step_copy_insn_closure (len));
  struct kvx_displaced_step *dsc
    = (struct kvx_displaced_step *) (void *) closure->buf.data ();
  kvx_gdbarch_tdep *tdep = gdbarch_tdep<kvx_gdbarch_tdep> (gdbarch);
  int i;
  int riscv_en = (kvx_arch () == KVX_KV4_1
		  && kvx_get_reg_field_value (regs, tdep->ps_regnum, "rv"));

  if (debug_displaced)
    gdb_printf (_ ("displaced: copying from %s\n"), paddress (gdbarch, from));

  memset (dsc, 0, sizeof (*dsc));
  try
    {
      do
	{
	  read_memory (from + dsc->num_insn_words * 4,
		       (gdb_byte *) (dsc->insn_words + dsc->num_insn_words), 4);

	  if (riscv_en)
	    {
	      dsc->num_insn_words++;
	      break;
	    }
	}
      while (dsc->insn_words[dsc->num_insn_words++] & (1 << 31));
    }
  catch (const gdb_exception_error &ex)
    {
      int mme = kvx_is_mmu_enabled (gdbarch, regs);
      int hbp = hardware_breakpoint_inserted_here_p (regs->aspace (), from);

      if (!mme || !hbp
	  || (ex.error != MEMORY_ERROR && ex.error != NOT_SUPPORTED_ERROR))
	throw;

      if (debug_displaced)
	gdb_printf (_ ("displaced: stepping inplace a hardware breakpoint "
		       "on an unmapped address\n"));

      kvx_enable_hbkp_on_cpu (from, 0);

      dsc->execute_inplace = 1;
      return displaced_step_copy_insn_closure_up (closure.release ());
    }

  if (debug_displaced)
    {
      int idx_words;
      gdb_printf (_ ("displaced: copied a %i word(s)\n"), dsc->num_insn_words);
      for (idx_words = 0; idx_words < dsc->num_insn_words; idx_words++)
	gdb_printf (_ ("displaced: insn[%i] = %08x\n"), idx_words,
		    dsc->insn_words[idx_words]);
    }

  dsc->insn_words[0]
    = extract_unsigned_integer ((const gdb_byte *) dsc->insn_words, 4,
				gdbarch_byte_order (gdbarch));
  if (riscv_en)
    {
      uint8_t opc = dsc->insn_words[0] & RV_OPCODE_MASK;
      if (opc == RV_OPCODE_JAL || opc == RV_OPCODE_JALR
	  || opc == RV_OPCODE_BRANCH || opc == RV_OPCODE_ECALL)
	patch_bcu_instruction (gdbarch, from, to, regs, dsc, riscv_en);
    }
  else if (((dsc->insn_words[0] >> 29) & 0x3) == 0)
    patch_bcu_instruction (gdbarch, from, to, regs, dsc, riscv_en);

  // pcrel instruction - the first syllable after the branch instructions
  for (i = 0; i < dsc->num_insn_words; i++)
    {
      uint32_t crt_word = dsc->insn_words[i];

      if ((riscv_en && (crt_word & RV_OPCODE_MASK) == RV_OPCODE_AUIPC)
	  || (!riscv_en && ((crt_word >> 29) & 0x3) != 0))
	{
	  struct op_list *insn = pcrel_insn[kvx_arch ()];

	  if (!riscv_en && ((crt_word >> 29) & 0x3) != 0x3)
	    break;

	  while (insn)
	    {
	      struct kvx_opc *op = insn->op;

	      if ((crt_word & op->codewords[0].mask) != op->codewords[0].opcode)
		{
		  insn = insn->next;
		  continue;
		}

	      if (strcmp ("pcrel", op->as_op) == 0)
		{
		  dsc->has_pcrel = 1;
		  dsc->pcrel_reg = extract_mds_bitfield (op, crt_word, 0, 0);
		}
	      else if (strcmp ("auipc", op->as_op) == 0)
		{
		  dsc->riscv = riscv_en;
		  dsc->has_pcrel = 1;
		  dsc->pcrel_reg = extract_mds_bitfield (op, crt_word, 0, 0);
		}

	      if (debug_displaced)
		gdb_printf (
		  _ ("displaced: found pcrel insn: destination register r%d\n"),
		  dsc->pcrel_reg);
	      break;
	    }

	  break;
	}
    }
  store_unsigned_integer ((gdb_byte *) dsc->insn_words, 4,
			  gdbarch_byte_order (gdbarch), dsc->insn_words[0]);

  write_memory (to, (gdb_byte *) dsc->insn_words, dsc->num_insn_words * 4);

  return displaced_step_copy_insn_closure_up (closure.release ());
}

static void
kvx_displaced_step_fixup (struct gdbarch *gdbarch,
			  displaced_step_copy_insn_closure *closure_,
			  CORE_ADDR from, CORE_ADDR to, struct regcache *regs)
{
  ULONGEST ps, lc, le, pc, pcrel_reg;
  buf_displaced_step_copy_insn_closure *closure
    = (buf_displaced_step_copy_insn_closure *) closure_;
  struct kvx_displaced_step *dsc
    = (struct kvx_displaced_step *) (void *) closure->buf.data ();
  kvx_gdbarch_tdep *tdep = gdbarch_tdep<kvx_gdbarch_tdep> (gdbarch);
  int branched = 0;
  int exception = 0;

  if (debug_displaced)
    gdb_printf (_ ("displaced: Fixup\n"));

  regcache_raw_read_unsigned (regs, tdep->ps_regnum, &ps);
  pc = regcache_read_pc (regs);
  if (debug_displaced)
    gdb_printf (_ ("displaced: new pc %s\n"), paddress (gdbarch, pc));

  if (dsc->execute_inplace)
    {
      kvx_enable_hbkp_on_cpu (from, 1);
      if (debug_displaced)
	gdb_printf (_ ("displaced: inplace executed finished\n"));
      return;
    }

  if (dsc->has_pcrel)
    {
      regcache_raw_read_unsigned (regs, dsc->pcrel_reg + tdep->r0_regnum,
				  &pcrel_reg);
      pcrel_reg += from - to;
      if (debug_displaced)
	gdb_printf (_ ("displaced: pcrel patch register r%d to 0x%" PRIx64
		       "\n"),
		    dsc->pcrel_reg, pcrel_reg);
      regcache_raw_write_unsigned (regs, dsc->pcrel_reg + tdep->r0_regnum,
				   pcrel_reg);
    }

  if (pc - to == dsc->num_insn_words * 4)
    {
      pc = from + (pc - to);
      if (debug_displaced)
	gdb_printf (_ ("displaced: Didn't branch\n"));
    }
  else
    {
      ULONGEST spc;

      /* We branched. */
      branched = 1;
      if (debug_displaced)
	gdb_printf (_ ("displaced: we branched (predicted dest: %" PRIx64
		       ") \n"),
		    dsc->dest);
      if (dsc->branchy && (pc == to || (dsc->scall_jump && pc == dsc->dest)))
	{
	  /* The branchy instruction jumped to its destination. */
	  pc = dsc->dest;

	  /* Rewrite RA only if the brach executed correctly. */
	  if (dsc->rewrite_RA)
	    {
	      regcache_raw_write_unsigned (regs,
					   dsc->riscv ? (tdep->r0_regnum + 1)
						      : tdep->ra_regnum,
					   from + dsc->num_insn_words * 4);
	      if (debug_displaced)
		gdb_printf (_ ("displaced: rewrite RA\n"));
	    }

	  if (dsc->scall_jump)
	    {
	      regcache_raw_write_unsigned (regs, tdep->spc_regnum,
					   from + dsc->num_insn_words * 4);
	      if (debug_displaced)
		gdb_printf (_ ("displaced: rewrite SPC\n"));
	    }
	}
      else
	{
	  // Uh oh... seems we've taken some exceptional condition. This means
	  // interrupt or H/W trap
	  regcache_raw_read_unsigned (regs, tdep->spc_regnum, &spc);
	  if (debug_displaced)
	    gdb_printf (_ ("displaced: trapped SPC=%lx\n"),
			(unsigned long) spc);
	  gdb_assert (spc == to);
	  spc = from;
	  regcache_raw_write_unsigned (regs, tdep->spc_regnum, spc);
	  exception = 1;
	}
    }

  /* Rewrite a patched reg unconditionnaly */
  if (dsc->rewrite_reg)
    {
      regcache_raw_write_unsigned (regs, dsc->reg,
				   dsc->rewrite_reg == 1 ? dsc->dest
							 : dsc->reg_org);
      if (debug_displaced)
	gdb_printf (_ ("displaced: rewrite %i with %" PRIx64 "\n"), dsc->reg,
		    dsc->dest);
    }

  if (((ps >> 5) & 1) /* HLE */)
    {
      /* The loop setup is done only if H/W loops are actually
	 enabled. */
      if (!exception && dsc->rewrite_LE)
	{
	  regcache_raw_write_unsigned (regs, tdep->le_regnum, dsc->dest);
	  regcache_raw_write_unsigned (regs, tdep->ls_regnum,
				       from + dsc->num_insn_words * 4);
	  if (debug_displaced)
	    gdb_printf (_ ("displaced: rewrite LE\n"));
	}

      if (!branched)
	{
	  regcache_raw_read_unsigned (regs, tdep->le_regnum, &le);
	  if (debug_displaced)
	    gdb_printf (_ ("displaced: active loop pc(%" PRIx64 ") le(%" PRIx64
			   ")\n"),
			pc, le);

	  if (pc == le)
	    {
	      if (debug_displaced)
		gdb_printf (_ ("displaced: at loop end\n"));
	      regcache_raw_read_unsigned (regs, tdep->lc_regnum, &lc);
	      if (lc - 1 == 0)
		regcache_raw_read_unsigned (regs, tdep->le_regnum, &pc);
	      else
		regcache_raw_read_unsigned (regs, tdep->ls_regnum, &pc);
	      if (lc != 0)
		regcache_raw_write_unsigned (regs, tdep->lc_regnum, lc - 1);
	    }
	}
    }

  regcache_write_pc (regs, pc);
  if (debug_displaced)
    gdb_printf (_ ("displaced: writing PC %s\n"), paddress (gdbarch, pc));
}

static int
kvx_register_reggroup_p (struct gdbarch *gdbarch, int regnum,
			 const struct reggroup *group)
{
  if (gdbarch_register_name (gdbarch, regnum) == NULL
      || *gdbarch_register_name (gdbarch, regnum) == '\0')
    return 0;

  if ((group == save_reggroup || group == restore_reggroup
       || group == all_reggroup)
      && strncmp (gdbarch_register_name (gdbarch, regnum), "oce", 3) == 0)
    return 0;

  return default_register_reggroup_p (gdbarch, regnum, group);
}

static int
kvx_bare_breakpoint_kind_from_pc (struct gdbarch *gdbarch, CORE_ADDR *pcptr)
{
  struct gdbarch *rv_gdbarch = NULL;
  int rv_mode = kvx_is_vaddr_rv (*pcptr, &rv_gdbarch, NULL) || dummy_rv_next;
  int len = 4;

  if (rv_mode)
    {
      int riscv_breakpoint_kind_from_pc (struct gdbarch * gdbarch,
					 CORE_ADDR * pcptr);

      if (!rv_gdbarch)
	rv_gdbarch = kvx_get_rv_arch_from_elfs ();

      if (dummy_rv_next)
	dummy_rv_bp_addr = *pcptr;
      len = riscv_breakpoint_kind_from_pc (rv_gdbarch, pcptr);
    }

  return MAKE_BKP_KIND (len, rv_mode);
}

static const gdb_byte *
kvx_bare_get_breakpoint (int *len)
{
  *len = 4;
  if (cjtag_over_iss == 'o')
    {
      if (!break_jtag_over_iss[kvx_arch ()])
	error (
	  "Cannot find the scall instruction for the current architecture.");
      return (gdb_byte *) &break_jtag_over_iss[kvx_arch ()];
    }

  if (!break_op[kvx_arch ()])
    error ("Cannot find the break instruction for the current architecture.");

  return (gdb_byte *) &break_op[kvx_arch ()];
}

static const gdb_byte *
kvx_bare_sw_breakpoint_from_kind (struct gdbarch *gdbarch, int kind, int *size)
{
  bool rv_mode = RV_MODE_FROM_BKP_KIND (kind);
  int len = BKP_LEN_FROM_BKP_KIND (kind);

  if (rv_mode)
    {
      const gdb_byte *riscv_sw_breakpoint_from_kind (struct gdbarch * gdbarch,
						     int kind, int *size);

      return riscv_sw_breakpoint_from_kind (kvx_get_rv_arch_from_elfs (), len,
					    size);
    }
  else if (len == 4)
    return kvx_bare_get_breakpoint (size);

  error (_ ("Invalid software breakpoint kind %x"), kind);
}

static void
kvx_timeout_os_init_done_cb (void *arg)
{
  *(int *) arg = 1;
}

int
kvx_prepare_os_init_done (void)
{
  static int s_cnt_wait_os_init_done = 0;
  CORE_ADDR gdb_os_init_done_addr;
  uint32_t saved_os_init_done_syl;
  struct bound_minimal_symbol msym;
  const gdb_byte *bp_opcode;
  struct inferior *inf;
  struct kvx_inferior_data *data;
  struct regcache *rc;
  int bp_len;
  process_stratum_target *proc_target
    = (process_stratum_target *) get_current_remote_target ();

  inf = current_inferior ();
  if (!inf || !proc_target)
    return 0;

  // continue to gdb_os_init_done only for the clusters by spawned by runner
  // and only if the "kalray cont_os_init_done" option is on
  if (!opt_cont_os_init_done)
    return 0;

  // continue to gdb_os_init_done only after reboot (pc = 0)
  rc = get_thread_regcache (proc_target, inferior_ptid);
  if (regcache_read_pc (rc) != 0)
    return 0;

  // search the gdb_os_init_done symbol
  msym = lookup_minimal_symbol ("gdb_os_init_done", NULL, NULL);
  if (!msym.minsym)
    return 0;
  gdb_os_init_done_addr = msym.value_address ();
  if (!gdb_os_init_done_addr)
    return 0;

  // add the breakpoint opcode at the gdb_os_init_done address
  // w/o adding a breakpoint in gdb. This prevents the gdb breakpoints
  // counter increment
  bp_opcode = gdbarch_breakpoint_from_pc (target_gdbarch (),
					  &gdb_os_init_done_addr, &bp_len);
  gdb_assert (bp_len == 4);
  read_memory (gdb_os_init_done_addr, (gdb_byte *) &saved_os_init_done_syl,
	       bp_len);
  write_memory (gdb_os_init_done_addr, bp_opcode, bp_len);

  // save the info to the inferior data
  wait_os_init_done++;
  data = mppa_inferior_data (inf);
  data->gdb_os_init_done_addr = gdb_os_init_done_addr;
  data->saved_os_init_done_syl = saved_os_init_done_syl;
  data->os_init_done = ++s_cnt_wait_os_init_done;

  return 1;
}

static void
kvx_inferior_created (struct inferior *inf)
{
  int timedout, timer_id;
  struct thread_info *thread;
  thread_suspend_state ss;
  const int timeout_ms_os_init_done = 1000;
  process_stratum_target *proc_target
    = (process_stratum_target *) get_current_remote_target ();

  kvx_current_arch = KVX_NUM_ARCHES;

  if (!inf || inf->pid != 1)
    return;

  if (after_first_resume || !kvx_prepare_os_init_done ())
    return;

  // save the initial stopped thread waitstatus
  thread = find_thread_ptid (proc_target, inferior_ptid);
  thread->save_suspend_to (ss);

  // continue to gdb_os_init_done
  continue_1 (0);

  // wait to arrive at gdb_os_init_done (or any other stop reason)
  // with any thread of the current inferior
  async_disable_stdin ();
  // avoid printing the stop info, it will be printed later by
  // process_initial_stop_replies
  inf->control.stop_soon = STOP_QUIETLY;
  timedout = 0;
  // if for the execution does not arrive at gdb_os_init_done, avoid
  // gdb stalling by creating a timer
  timer_id = create_timer (timeout_ms_os_init_done,
			   &kvx_timeout_os_init_done_cb, &timedout);
  while (gdb_do_one_event () >= 0)
    {
      // update stopped threads list
      update_thread_list ();
      finish_thread_state (proc_target, minus_one_ptid);
      // search a stopped thread in the current inferior
      thread = any_live_thread_of_inferior (inf);
      if (thread->state == THREAD_STOPPED)
	{
	  // set the initial stopped thread waitstatus
	  thread->restore_suspend_from (ss);
	  break;
	}

      if (timedout)
	break;
    }
  if (!timedout)
    delete_timer (timer_id);
  // restore the gdb state
  inf->control.stop_soon = NO_STOP_QUIETLY;
  async_enable_stdin ();
}

static CORE_ADDR
kvx_push_dummy_code (struct gdbarch *gdbarch, CORE_ADDR sp, CORE_ADDR funcaddr,
		     struct value **args, int nargs, struct type *value_type,
		     CORE_ADDR *real_pc, CORE_ADDR *bp_addr,
		     struct regcache *regcache)
{
  int i, sz;
  uint32_t nop = nop_op[kvx_arch ()];

  struct gdbarch *rv_gdbarch = NULL;
  if (kvx_is_vaddr_rv (funcaddr, &rv_gdbarch, NULL))
    {
      CORE_ADDR
      riscv_push_dummy_code (struct gdbarch * gdbarch, CORE_ADDR sp,
			     CORE_ADDR funaddr, struct value * *args, int nargs,
			     struct type *value_type, CORE_ADDR *real_pc,
			     CORE_ADDR *bp_addr, struct regcache *regcache);
      return riscv_push_dummy_code (rv_gdbarch, sp, funcaddr, args, nargs,
				    value_type, real_pc, bp_addr, regcache);
    }

  // allocate space for a breakpoint and a nop, keep the stack aligned
  sp &= KVX_STACK_ALIGN_MASK;
  sz = (2 * sizeof (nop) + KVX_STACK_ALIGN_BYTES - 1) & KVX_STACK_ALIGN_MASK;
  if (sp < sz)
    {
      error (_ ("Cannot call yet a function from gdb prompt because the stack "
		"pointer is not set yet (sp=0x%" PRIx64 ")"),
	     sp);
    }
  sp -= sz;

  // write NOPs on the reserved stack place
  for (i = 0; i < sz / sizeof (nop); i++)
    write_memory (sp + i * sizeof (nop), (gdb_byte *) &nop, sizeof (nop));

  // the breakpoint will be on the second NOP (beginning from the lowest
  // address) when the breakpoint will be inserted, it will search the end of
  // the previous bundle (bit parallel 0) so, it will find our first unparallel
  // NOP
  *bp_addr = sp + sizeof (nop);

  // inferior resumes at the function entry point
  *real_pc = funcaddr;

  return sp;
}

int
sync_insert_remove_breakpoint (CORE_ADDR addr, int len, const gdb_byte *pvalue)
{
  char *buf;
  long size = 256;
  uint32_t value = 0;
  remote_target *rt = get_current_remote_target ();

  if (len == 4)
    value = *(uint32_t *) pvalue;
  else if (len == 2)
    value = *(uint16_t *) pvalue;
  else
    error (_ ("Invalid breakpoint length %d"), len);

  buf = (char *) malloc (size);
  sprintf (buf, "Qkalray.unified_write:%" PRIx64 ",%d:%" PRIx64, addr, len,
	   (unsigned long) value);
  putpkt (rt, buf);
  getpkt (rt, &buf, &size, 0);
  free (buf);

  return 0;
}

static int
kvx_memory_insert_breakpoint (struct gdbarch *gdbarch,
			      struct bp_target_info *bp_tgt)
{
  int ret = default_memory_insert_breakpoint (gdbarch, bp_tgt);

  if (!ret)
    {
      int len = 0;
      CORE_ADDR pc = bp_tgt->placed_address;
      const gdb_byte *bp = gdbarch_breakpoint_from_pc (gdbarch, &pc, &len);

      sync_insert_remove_breakpoint (bp_tgt->placed_address, len, bp);
    }

  return ret;
}

static int
kvx_memory_remove_breakpoint (struct gdbarch *gdbarch,
			      struct bp_target_info *bp_tgt)
{
  int ret = default_memory_remove_breakpoint (gdbarch, bp_tgt);

  if (!ret)
    {
      sync_insert_remove_breakpoint (bp_tgt->placed_address,
				     BKP_LEN_FROM_BKP_KIND (bp_tgt->kind),
				     bp_tgt->shadow_contents);
    }

  return ret;
}

static void
kvx_write_pc (struct regcache *regcache, CORE_ADDR pc)
{
  struct kvx_displaced_step *dsc = nullptr;
  struct gdbarch *gdbarch = target_gdbarch ();
  if (gdbarch_displaced_step_copy_insn_closure_by_addr_p (gdbarch))
    {
      buf_displaced_step_copy_insn_closure *closure
	= (buf_displaced_step_copy_insn_closure *)
	  gdbarch_displaced_step_copy_insn_closure_by_addr (gdbarch,
							    current_inferior (),
							    pc);

      dsc = (struct kvx_displaced_step *) (void *) closure->buf.data ();
    }

  if (dsc && dsc->execute_inplace)
    {
      if (debug_displaced)
	gdb_printf (_ ("The PC was not written because the displace "
		       "will be executed inplace\n"));

      return;
    }

  regcache_cooked_write_unsigned (regcache,
				  gdbarch_pc_regnum (regcache->arch ()), pc);
}

static int
kvx_core_addr_lessthan (CORE_ADDR lhs, CORE_ADDR rhs)
{
  int i;
  frame_info_ptr frame;
  CORE_ADDR pc, fp;
  struct minimal_symbol *sym;
  const char *name = NULL;
  static const char *fcs[]
    = {"__cos_asm_kernel_debug_handler", "__cos_asm_exception_handler",
       "__cos_asm_kernel_syscall_handler", "__cos_asm_interrupt_handler",
       "_start"};

  if (lhs >= rhs)
    return 0;

  for (frame = get_current_frame (); frame; frame = get_prev_frame (frame))
    {
      fp = get_frame_base (frame);
      if (rhs == fp)
	{
	  pc = get_frame_pc (frame);
	  if (!pc)
	    break;
	  sym = lookup_minimal_symbol_by_pc (pc).minsym;
	  if (sym)
	    name = sym->linkage_name ();
	  if (!name)
	    break;
	  for (i = 0; i < sizeof (fcs) / sizeof (fcs[0]); i++)
	    if (!strcmp (name, fcs[i]))
	      return 0;
	  break;
	}
    }

  return 1;
}

/* Fetch the siginfo data for the specified thread, if it exists.  If
   there is no data, or we could not read it, return an empty
   buffer.  */

static gdb::byte_vector
kvx_get_siginfo_data (thread_info *thread, struct gdbarch *gdbarch)
{
  struct type *siginfo_type;
  LONGEST bytes_read;

  if (!gdbarch_get_siginfo_type_p (gdbarch))
    return gdb::byte_vector ();

  scoped_restore_current_thread save_current_thread;
  switch_to_thread (thread);

  siginfo_type = gdbarch_get_siginfo_type (gdbarch);

  gdb::byte_vector buf (siginfo_type->length ());

  bytes_read = target_read (current_inferior ()->top_target (),
			    TARGET_OBJECT_SIGNAL_INFO, NULL, buf.data (), 0,
			    siginfo_type->length ());
  if (bytes_read != siginfo_type->length ())
    buf.clear ();

  return buf;
}

struct kvx_corefile_thread_data
{
  kvx_corefile_thread_data (struct gdbarch *gdbarch, bfd *obfd,
			    gdb::unique_xmalloc_ptr<char> &note_data,
			    int *note_size, gdb_signal stop_signal)
    : gdbarch (gdbarch), obfd (obfd), note_data (note_data),
      note_size (note_size), stop_signal (stop_signal)
  {}

  struct gdbarch *gdbarch;
  bfd *obfd;
  gdb::unique_xmalloc_ptr<char> &note_data;
  int *note_size;
  enum gdb_signal stop_signal;
};

static void
kvx_corefile_thread (struct thread_info *info,
		     struct kvx_corefile_thread_data *args)
{
  gcore_elf_build_thread_register_notes (args->gdbarch, info, args->stop_signal,
					 args->obfd, &args->note_data,
					 args->note_size);

  /* Don't return anything if we got no register information above,
     such a core file is useless.  */
  if (args->note_data != NULL)
    {
      gdb::byte_vector siginfo_data
	= kvx_get_siginfo_data (info, args->gdbarch);
      if (!siginfo_data.empty ())
	args->note_data.reset (
	  elfcore_write_note (args->obfd, args->note_data.release (),
			      args->note_size, "CORE", NT_SIGINFO,
			      siginfo_data.data (), siginfo_data.size ()));
    }
}

static struct regcache_map_entry kvx_gregmap[] = {{0, 0, 0}, {0}};

static const struct regset kvx_gregset
  = {kvx_gregmap, regcache_supply_regset, regcache_collect_regset};

static void
kvx_iterate_over_regset_sections (struct gdbarch *gdbarch,
				  iterate_over_regset_sections_cb *cb,
				  void *cb_data,
				  const struct regcache *regcache)
{
  int nb_regs = gdbarch_num_regs (gdbarch);
  int sz = nb_regs * 8;

  kvx_gregmap[0].count = nb_regs;

  cb (".reg", sz, sz, &kvx_gregset, NULL, cb_data);
}

static int
kvx_derive_thread_stack_segment (thread_info *thr, bfd_vma *bottom,
				 bfd_vma *top)
{
  scoped_restore_current_pspace_and_thread restore_pspace_thread;
  frame_info_ptr fi, tmp_fi;

  switch_to_thread (thr);

  /* Can't succeed without stack and registers.  */
  if (!target_has_stack () || !target_has_registers ())
    return 0;

  /* Can't succeed without current frame.  */
  fi = get_current_frame ();
  if (fi == NULL)
    return 0;

  /* Save frame pointer of TOS frame.  */
  *bottom = get_frame_base (fi);
  /* If current stack pointer is more "inner", use that instead.  */
  if (gdbarch_inner_than (get_frame_arch (fi), get_frame_sp (fi), *bottom))
    *bottom = get_frame_sp (fi);

  /* Find prev-most frame.  */
  while ((tmp_fi = get_prev_frame (fi)) != NULL)
    fi = tmp_fi;

  /* Save frame pointer of prev-most frame.  */
  *top = get_frame_base (fi);
  if (*top == 0)
    *top = get_frame_sp (fi);

  if (*bottom > *top || *top - *bottom > 64 * 1024)
    *bottom = *top - 64 * 1024;

  return 1;
}

static int
kvx_gcore_add_load_section (bfd *obfd, CORE_ADDR bottom, CORE_ADDR top)
{
  asection *osec
    = bfd_make_section_anyway_with_flags (obfd, "load",
					  SEC_ALLOC | SEC_HAS_CONTENTS
					    | SEC_LOAD | SEC_DATA);
  if (osec == NULL)
    {
      warning (_ ("Couldn't make gcore segment: %s"),
	       bfd_errmsg (bfd_get_error ()));
      return 1;
    }
  uint64_t size = top - bottom;
  if (info_verbose)
    {
      gdb_printf ("Save stack segment, %s bytes at %s\n", plongest (size),
		  paddress (target_gdbarch (), bottom));
    }

  bfd_set_section_size (osec, size);
  bfd_set_section_vma (osec, bottom);
  bfd_set_section_lma (osec, 0);

  return 0;
}

static gdb::unique_xmalloc_ptr<char>
kvx_make_corefile_notes (struct gdbarch *gdbarch, bfd *obfd, int *note_size)
{
  gdb::unique_xmalloc_ptr<char> note_data;

  /* Update the thread list */
  try
    {
      update_thread_list ();
    }
  catch (const gdb_exception_error &e)
    {
      exception_print (gdb_stderr, e);
    }

  /* Copy the core version from the elf bfd to the core bfd */
  if (current_program_space)
    {
      bfd *ebfd = current_program_space->exec_bfd ();
      if (ebfd)
	elf_elfheader (obfd)->e_flags = elf_elfheader (ebfd)->e_flags;
    }

  /* Like the kernel, prefer dumping the signaled thread first.
     "First thread" is what tools use to infer the signaled
     thread.  */
  kvx_corefile_thread_data thread_args (gdbarch, obfd, note_data, note_size,
					GDB_SIGNAL_0);
  thread_info *signalled_thr = NULL;
  for (thread_info *thr : current_inferior ()->non_exited_threads ())
    if (thr->stop_signal () != GDB_SIGNAL_0
	&& thr->stop_signal () != GDB_SIGNAL_INT)
      {
	signalled_thr = thr;
	thread_args.stop_signal = thr->stop_signal ();
	kvx_corefile_thread (signalled_thr, &thread_args);
	break;
      }

  for (thread_info *thr : current_inferior ()->non_exited_threads ())
    {
      if (thr->state != THREAD_STOPPED)
	continue;

      /* Dump thread registers */
      if (thr != signalled_thr)
	kvx_corefile_thread (thr, &thread_args);

      /* Dump thread stack */
      bfd_vma stack_bottom = 0, stack_top = 0;
      if (kvx_derive_thread_stack_segment (thr, &stack_bottom, &stack_top))
	if (kvx_gcore_add_load_section (obfd, stack_bottom, stack_top))
	  return NULL;
    }

  /* Dump mppadl linked list memory */
  CORE_ADDR iter_ctx = 0, mppadl_bottom = 0, mppadl_top = 0;
  while (!kvx_bare_get_core_range (&iter_ctx, &mppadl_bottom, &mppadl_top)
	 && mppadl_bottom && mppadl_top - mppadl_bottom <= 2 * 1024 * 1024)
    {
      if (kvx_gcore_add_load_section (obfd, mppadl_bottom, mppadl_top))
	return NULL;

      if (!iter_ctx)
	break;
    }

  if (!note_data)
    return NULL;

  /* Target description.  */
  gcore_elf_make_tdesc_note (obfd, &note_data, note_size);

  return note_data;
}

static int
kvx_find_and_open_solib (const char *in_pathname, unsigned o_flags,
			 gdb::unique_xmalloc_ptr<char> *temp_pathname)
{
	return openp (current_inferior ()->environment.get
		      ("KALRAY_LD_LIBRARY_PATH"),
		      OPF_TRY_CWD_FIRST | OPF_RETURN_REALPATH, in_pathname,
		      o_flags, temp_pathname);
}

static CORE_ADDR
kvx_unwind_sp_rv (struct gdbarch *gdbarch, frame_info_ptr next_frame)
{
  int sp_regnum = gdbarch_sp_regnum (gdbarch);
  if (kvx_is_vaddr_rv (get_frame_pc (next_frame), NULL, NULL))
    sp_regnum = RV_SP_FROM_KVX_REG (sp_regnum);
  return frame_unwind_register_unsigned (next_frame, sp_regnum);
}

static void
kvx_on_inferior_call_pre (ptid_t thread, CORE_ADDR address)
{
  struct regcache *regs = get_current_regcache ();
  kvx_gdbarch_tdep *tdep = gdbarch_tdep<kvx_gdbarch_tdep> (regs->arch ());
  int riscv_en = (kvx_arch () == KVX_KV4_1
		  && kvx_get_reg_field_value (regs, tdep->ps_regnum, "rv"));
  int rv_addr = kvx_is_vaddr_rv (address, NULL, NULL);

  dummy_rv_bp_addr = -1;
  dummy_rv_next = 0;

  if (riscv_en ^ rv_addr)
    {
      error ("Cannot call a %s function from the %s cpu mode!",
	     rv_addr ? "riscv" : "kvx", riscv_en ? "riscv" : "kvx");
    }
  dummy_rv_next = rv_addr;
}

static void
kvx_on_inferior_call_post (ptid_t thread, CORE_ADDR address)
{
  dummy_rv_next = 0;
}

static struct gdbarch *
kvx_gdbarch_init (struct gdbarch_info info, struct gdbarch_list *arches)
{
  struct gdbarch *gdbarch;
  kvx_gdbarch_tdep *tdep;
  const struct target_desc *tdesc;
  tdesc_arch_data_up tdesc_data;
  int i;
  int has_pc = -1, has_sp = -1, has_le = -1, has_ls = -1, has_ps = -1;
  int has_ev = -1, has_lc = -1, has_local = -1, has_ra = -1, has_spc = -1;
  int has_ea_pl0 = -1, has_es_pl0 = -1, has_syo = -1, has_ev_pl0 = -1;
  int has_r0 = -1;

  static const char kvx_ev_name[] = "ev";
  static const char kvx_lc_name[] = "lc";
  static const char kvx_ls_name[] = "ls";
  static const char kvx_le_name[] = "le";
  static const char kvx_ps_name[] = "ps";
  static const char kvx_ra_name[] = "ra";
  static const char kvx_spc_name[] = "spc";
  static const char kvx_local_name[] = "r13";
  static const char kvx_ea_pl0_name[] = "ea_pl0";
  static const char kvx_es_pl0_name[] = "es_pl0";
  static const char kvx_syo_name[] = "syo";
  static const char kvx_ev_pl0_name[] = "ev_pl0";
  static const char kvx_r0_name[] = "r0";

  const char *pc_name;
  const char *sp_name;

  if (inferior_ptid == null_ptid)
    {
      static int non_stop_set = 0;
      char set_non_stop_cmd[] = "set non-stop";
      if (!non_stop_set)
	{
	  non_stop_set = 1;
	  execute_command (set_non_stop_cmd, 0);
	}
    }

  /* If there is already a candidate, use it.  */
  arches = gdbarch_list_lookup_by_info (arches, &info);
  if (arches != NULL)
    return arches->gdbarch;

  tdep = (kvx_gdbarch_tdep *) xzalloc (sizeof (kvx_gdbarch_tdep));
  gdbarch = gdbarch_alloc (&info, tdep);

  pc_name = kvx_pc_name (gdbarch);
  sp_name = kvx_sp_name (gdbarch);

  /* This could (should?) be extracted from MDS */
  set_gdbarch_short_bit (gdbarch, 16);
  set_gdbarch_int_bit (gdbarch, 32);
  set_gdbarch_long_bit (gdbarch,
			gdbarch_bfd_arch_info (gdbarch)->bits_per_address);
  set_gdbarch_long_long_bit (gdbarch, 64);
  set_gdbarch_float_bit (gdbarch, 32);
  set_gdbarch_double_bit (gdbarch, 64);
  set_gdbarch_long_double_bit (gdbarch, 64);
  set_gdbarch_ptr_bit (gdbarch,
		       gdbarch_bfd_arch_info (gdbarch)->bits_per_address);
  set_gdbarch_memory_insert_breakpoint (gdbarch, kvx_memory_insert_breakpoint);
  set_gdbarch_memory_remove_breakpoint (gdbarch, kvx_memory_remove_breakpoint);
  set_gdbarch_write_pc (gdbarch, kvx_write_pc);

  /* Get the kvx target description from INFO.  */
  tdesc = info.target_desc;
  if (tdesc_has_registers (tdesc))
    {
      set_gdbarch_num_regs (gdbarch, 0);
      tdesc_data = tdesc_data_alloc ();
      tdesc_use_registers (gdbarch, tdesc, std::move (tdesc_data));

      for (i = 0; i < gdbarch_num_regs (gdbarch); ++i)
	{
	  if (strcmp (tdesc_register_name (gdbarch, i), kvx_r0_name) == 0)
	    has_r0 = i;
	  else if (strcmp (tdesc_register_name (gdbarch, i), pc_name) == 0)
	    has_pc = i;
	  else if (strcmp (tdesc_register_name (gdbarch, i), sp_name) == 0)
	    has_sp = i;
	  else if (strcmp (tdesc_register_name (gdbarch, i), kvx_le_name) == 0)
	    has_le = i;
	  else if (strcmp (tdesc_register_name (gdbarch, i), kvx_ls_name) == 0)
	    has_ls = i;
	  else if (strcmp (tdesc_register_name (gdbarch, i), kvx_ps_name) == 0)
	    has_ps = i;
	  else if (strcmp (tdesc_register_name (gdbarch, i), kvx_lc_name) == 0)
	    has_lc = i;
	  else if (strcmp (tdesc_register_name (gdbarch, i), kvx_local_name)
		   == 0)
	    has_local = i;
	  else if (strcmp (tdesc_register_name (gdbarch, i), kvx_ra_name) == 0)
	    has_ra = i;
	  else if (strcmp (tdesc_register_name (gdbarch, i), kvx_spc_name) == 0)
	    has_spc = i;
	  else if (strcmp (tdesc_register_name (gdbarch, i), kvx_ev_name) == 0)
	    has_ev = i;
	  else if (strcmp (tdesc_register_name (gdbarch, i), kvx_ea_pl0_name)
		   == 0)
	    has_ea_pl0 = i;
	  else if (strcmp (tdesc_register_name (gdbarch, i), kvx_es_pl0_name)
		   == 0)
	    has_es_pl0 = i;
	  else if (strcmp (tdesc_register_name (gdbarch, i), kvx_syo_name) == 0)
	    has_syo = i;
	  else if (strcmp (tdesc_register_name (gdbarch, i), kvx_ev_pl0_name)
		   == 0)
	    has_ev_pl0 = i;
	}

      if (has_r0 < 0)
	error ("There's no '%s' register!", kvx_r0_name);
      if (has_pc < 0)
	error ("There's no '%s' register!", pc_name);
      if (has_sp < 0)
	error ("There's no '%s' register!", sp_name);
      if (has_le < 0)
	error ("There's no '%s' register!", kvx_le_name);
      if (has_ls < 0)
	error ("There's no '%s' register!", kvx_ls_name);
      if (has_lc < 0)
	error ("There's no '%s' register!", kvx_lc_name);
      if (has_ps < 0)
	error ("There's no '%s' register!", kvx_ps_name);
      if (has_local < 0)
	error ("There's no '%s' register!", kvx_local_name);
      if (has_ra < 0)
	error ("There's no '%s' register!", kvx_ra_name);
      if (has_spc < 0)
	error ("There's no '%s' register!", kvx_spc_name);
      if (has_ev < 0)
	error ("There's no '%s' register!", kvx_ev_name);
      if (has_ea_pl0 < 0)
	error ("There's no '%s' register!", kvx_ea_pl0_name);
      if (has_es_pl0 < 0)
	error ("There's no '%s' register!", kvx_es_pl0_name);
      if (has_syo < 0)
	error ("There's no '%s' register!", kvx_syo_name);
      if (has_ev_pl0 < 0)
	error ("There's no '%s' register!", kvx_ev_pl0_name);

      tdep->r0_regnum = has_r0;
      {
	extern int rv_regnum_ofs;
	rv_regnum_ofs = has_r0;
      }
      tdep->ev_regnum = has_ev;
      tdep->le_regnum = has_le;
      tdep->ls_regnum = has_ls;
      tdep->lc_regnum = has_lc;
      tdep->ps_regnum = has_ps;
      tdep->ra_regnum = has_ra;
      tdep->spc_regnum = has_spc;
      tdep->local_regnum = has_local;
      tdep->ea_pl0_regnum = has_ea_pl0;
      tdep->es_pl0_regnum = has_es_pl0;
      tdep->ev_pl0_regnum = has_ev_pl0;
      tdep->syo_regnum = has_syo;
      tdep->uint256 = arch_integer_type (gdbarch, 256, 0, "uint256_t");
      tdep->uint512 = arch_integer_type (gdbarch, 512, 0, "uint512_t");
      tdep->uint1024 = arch_integer_type (gdbarch, 1024, 0, "uint1024_t");

      if (has_r0 == 0)
	tdep->srf_offset = has_pc;
      else
	tdep->srf_offset = has_pc;

      set_gdbarch_pc_regnum (gdbarch, has_pc);
      set_gdbarch_sp_regnum (gdbarch, has_sp);
    }
  else
    {
      set_gdbarch_num_regs (gdbarch, 1);
      set_gdbarch_register_name (gdbarch, kvx_dummy_register_name);
      set_gdbarch_register_type (gdbarch, kvx_dummy_register_type);
    }

  set_gdbarch_register_reggroup_p (gdbarch, kvx_register_reggroup_p);

  set_gdbarch_num_pseudo_regs (gdbarch, kvx_num_pseudos_wrap (gdbarch));
  set_tdesc_pseudo_register_name (gdbarch, kvx_pseudo_wrap_register_name);
  set_tdesc_pseudo_register_type (gdbarch, kvx_pseudo_wrap_register_type);
  set_tdesc_pseudo_register_reggroup_p (gdbarch,
					kvx_pseudo_register_reggroup_p);

  set_gdbarch_pseudo_register_read (gdbarch, kvx_pseudo_wrap_register_read);
  set_gdbarch_pseudo_register_write (gdbarch, kvx_pseudo_wrap_register_write);
  set_gdbarch_dwarf2_reg_to_regnum (gdbarch, kvx_dwarf2_reg_to_regnum);
  dwarf2_frame_set_init_reg (gdbarch, kvx_dwarf2_frame_init_reg);

  set_gdbarch_return_value (gdbarch, kvx_return_value);
  set_gdbarch_push_dummy_call (gdbarch, kvx_push_dummy_call);
  set_gdbarch_dummy_id (gdbarch, kvx_dummy_id);

  set_gdbarch_unwind_sp (gdbarch, kvx_unwind_sp_rv);
  set_gdbarch_call_dummy_location (gdbarch, ON_STACK);
  set_gdbarch_push_dummy_code (gdbarch, kvx_push_dummy_code);

  set_gdbarch_skip_prologue (gdbarch, kvx_skip_prologue);
  set_gdbarch_unwind_pc (gdbarch, kvx_unwind_pc);
  dwarf2_append_unwinders (gdbarch);
  frame_unwind_append_unwinder (gdbarch, &kvx_frame_unwind);

  set_gdbarch_fetch_tls_load_module_address (gdbarch,
					     kvx_fetch_tls_load_module_address);

  set_gdbarch_breakpoint_kind_from_pc (gdbarch,
				       kvx_bare_breakpoint_kind_from_pc);
  set_gdbarch_sw_breakpoint_from_kind (gdbarch,
				       kvx_bare_sw_breakpoint_from_kind);
  set_gdbarch_adjust_breakpoint_address (gdbarch,
					 kvx_adjust_breakpoint_address);
  /* Settings that should be unnecessary.  */
  set_gdbarch_inner_than (gdbarch, kvx_core_addr_lessthan);
  set_gdbarch_print_insn (gdbarch, kvx_print_insn);

  /* Displaced stepping */
  set_gdbarch_displaced_step_copy_insn (gdbarch, kvx_displaced_step_copy_insn);
  set_gdbarch_displaced_step_fixup (gdbarch, kvx_displaced_step_fixup);
  set_gdbarch_displaced_step_prepare (gdbarch, kvx_displaced_step_prepare);
  set_gdbarch_displaced_step_hw_singlestep (gdbarch,
					    kvx_displaced_step_hw_singlestep);
  set_gdbarch_displaced_step_finish (gdbarch, kvx_displaced_step_finish);
  set_gdbarch_max_insn_length (gdbarch, 8 * 4);

  set_gdbarch_get_longjmp_target (gdbarch, kvx_get_longjmp_target);

  if (tdesc_has_registers (tdesc))
    {
      kvx_bare_solib_ops.find_and_open_solib = kvx_find_and_open_solib;
      set_gdbarch_so_ops (gdbarch, &kvx_bare_solib_ops);
    }

  set_gdbarch_make_corefile_notes (gdbarch, kvx_make_corefile_notes);
  set_gdbarch_iterate_over_regset_sections (gdbarch,
					    kvx_iterate_over_regset_sections);

  return gdbarch;
}

// extern initialize_file_ftype _initialize_kvx_tdep;
void
_initialize_kvx_tdep ();
void
_initialize_kvx_tdep ()
{
  kvx_look_for_insns ();
  gdbarch_register (bfd_arch_kvx, kvx_gdbarch_init);

  gdb::observers::inferior_created.attach (kvx_inferior_created, "kvx-tdep");
  gdb::observers::inferior_call_pre.attach (kvx_on_inferior_call_pre,
					    "kvx-tdep");
  gdb::observers::inferior_call_post.attach (kvx_on_inferior_call_post,
					     "kvx-tdep");
}
