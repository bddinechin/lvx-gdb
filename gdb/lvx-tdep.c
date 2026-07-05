/* Target-dependent code for LVX for GDB, the GNU debugger.

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

/* This is a Tier-1 port of KVX's kvx-tdep.c: it covers gdbarch registration,
   the register model, and prologue/frame analysis. Functionality that needs
   a live target (displaced stepping, the Kalray JTAG-runner/ISS remote
   protocol, corefile generation, TLS, COS inferior-startup sequencing) is
   intentionally not ported yet -- there is no LVX ISS or hardware, and no
   RISC-V dual-mode (KV4's "rv" bit) is assumed to exist for LVX. See
   /home/bd3/LVX/lvx-gdb/CLAUDE.md and the porting plan for the full list of
   deferred KVX files. */

#include "defs.h"
#include "arch-utils.h"
#include "frame-unwind.h"
#include "gdbcore.h"
#include "objfiles.h"
#include "reggroups.h"
#include "target-descriptions.h"
#include "inferior.h"
#include "gdbthread.h"
#include "source.h"
#include "gdbcmd.h"

#include "lvx-common-tdep.h"
#include "solib-lvx-bare.h"

int
lvx_is_mmu_enabled (struct gdbarch *gdbarch, struct regcache *regs)
{
  ULONGEST ps;
  lvx_gdbarch_tdep *tdep;

  if (gdbarch == NULL)
    gdbarch = target_gdbarch ();

  tdep = gdbarch_tdep<lvx_gdbarch_tdep> (gdbarch);

  if (regs == NULL)
    regs = get_current_regcache ();

  regcache_raw_read_unsigned (regs, tdep->ps_regnum, &ps);
  return (ps & (1 << PS_MME_BIT)) != 0;
}

static int
lvx_register_reggroup_p (struct gdbarch *gdbarch, int regnum,
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

static const gdb_byte *
lvx_get_breakpoint (int *len)
{
  *len = 4;
  if (!break_op[lvx_arch ()])
    error ("Cannot find the break instruction for the current architecture.");

  return (gdb_byte *) &break_op[lvx_arch ()];
}

static int
lvx_breakpoint_kind_from_pc (struct gdbarch *gdbarch, CORE_ADDR *pcptr)
{
  return 4;
}

static const gdb_byte *
lvx_sw_breakpoint_from_kind (struct gdbarch *gdbarch, int kind, int *size)
{
  if (kind == 4)
    return lvx_get_breakpoint (size);

  error (_ ("Invalid software breakpoint kind %x"), kind);
}

static CORE_ADDR
lvx_push_dummy_code (struct gdbarch *gdbarch, CORE_ADDR sp, CORE_ADDR funcaddr,
		     struct value **args, int nargs, struct type *value_type,
		     CORE_ADDR *real_pc, CORE_ADDR *bp_addr,
		     struct regcache *regcache)
{
  int i, sz;
  uint32_t nop = nop_op[lvx_arch ()];

  /* allocate space for a breakpoint and a nop, keep the stack aligned */
  sp &= LVX_STACK_ALIGN_MASK;
  sz = (2 * sizeof (nop) + LVX_STACK_ALIGN_BYTES - 1) & LVX_STACK_ALIGN_MASK;
  if (sp < sz)
    {
      error (_ ("Cannot call yet a function from gdb prompt because the stack "
		"pointer is not set yet (sp=0x%" PRIx64 ")"),
	     sp);
    }
  sp -= sz;

  /* write NOPs on the reserved stack place */
  for (i = 0; i < sz / sizeof (nop); i++)
    write_memory (sp + i * sizeof (nop), (gdb_byte *) &nop, sizeof (nop));

  /* the breakpoint will be on the second NOP (beginning from the lowest
     address) when the breakpoint will be inserted, it will search the end of
     the previous bundle (bit parallel 0) so, it will find our first unparallel
     NOP */
  *bp_addr = sp + sizeof (nop);

  /* inferior resumes at the function entry point */
  *real_pc = funcaddr;

  return sp;
}

static int
lvx_find_and_open_solib (const char *in_pathname, unsigned o_flags,
			 gdb::unique_xmalloc_ptr<char> *temp_pathname)
{
  return openp (current_inferior ()->environment.get ("LVX_LD_LIBRARY_PATH"),
		OPF_TRY_CWD_FIRST | OPF_RETURN_REALPATH, in_pathname, o_flags,
		temp_pathname);
}

static struct gdbarch *
lvx_gdbarch_init (struct gdbarch_info info, struct gdbarch_list *arches)
{
  struct gdbarch *gdbarch;
  lvx_gdbarch_tdep *tdep;
  const struct target_desc *tdesc;
  tdesc_arch_data_up tdesc_data;
  int i;
  int has_pc = -1, has_sp = -1, has_le = -1, has_ls = -1, has_ps = -1;
  int has_ev = -1, has_lc = -1, has_local = -1, has_ra = -1, has_spc = -1;
  int has_ea_pl0 = -1, has_es_pl0 = -1, has_syo = -1, has_ev_pl0 = -1;
  int has_r0 = -1;

  static const char lvx_ev_name[] = "ev";
  static const char lvx_lc_name[] = "lc";
  static const char lvx_ls_name[] = "ls";
  static const char lvx_le_name[] = "le";
  static const char lvx_ps_name[] = "ps";
  static const char lvx_ra_name[] = "ra";
  static const char lvx_spc_name[] = "spc";
  static const char lvx_local_name[] = "r13";
  static const char lvx_ea_pl0_name[] = "ea_pl0";
  static const char lvx_es_pl0_name[] = "es_pl0";
  static const char lvx_syo_name[] = "syo";
  static const char lvx_ev_pl0_name[] = "ev_pl0";
  static const char lvx_r0_name[] = "r0";

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

  tdep = (lvx_gdbarch_tdep *) xzalloc (sizeof (lvx_gdbarch_tdep));
  gdbarch = gdbarch_alloc (&info, tdep);

  pc_name = lvx_pc_name (gdbarch);
  sp_name = lvx_sp_name (gdbarch);

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

  /* Get the lvx target description from INFO.  */
  tdesc = info.target_desc;
  if (tdesc_has_registers (tdesc))
    {
      set_gdbarch_num_regs (gdbarch, 0);
      tdesc_data = tdesc_data_alloc ();
      tdesc_use_registers (gdbarch, tdesc, std::move (tdesc_data));

      for (i = 0; i < gdbarch_num_regs (gdbarch); ++i)
	{
	  if (strcmp (tdesc_register_name (gdbarch, i), lvx_r0_name) == 0)
	    has_r0 = i;
	  else if (strcmp (tdesc_register_name (gdbarch, i), pc_name) == 0)
	    has_pc = i;
	  else if (strcmp (tdesc_register_name (gdbarch, i), sp_name) == 0)
	    has_sp = i;
	  else if (strcmp (tdesc_register_name (gdbarch, i), lvx_le_name) == 0)
	    has_le = i;
	  else if (strcmp (tdesc_register_name (gdbarch, i), lvx_ls_name) == 0)
	    has_ls = i;
	  else if (strcmp (tdesc_register_name (gdbarch, i), lvx_ps_name) == 0)
	    has_ps = i;
	  else if (strcmp (tdesc_register_name (gdbarch, i), lvx_lc_name) == 0)
	    has_lc = i;
	  else if (strcmp (tdesc_register_name (gdbarch, i), lvx_local_name)
		   == 0)
	    has_local = i;
	  else if (strcmp (tdesc_register_name (gdbarch, i), lvx_ra_name) == 0)
	    has_ra = i;
	  else if (strcmp (tdesc_register_name (gdbarch, i), lvx_spc_name) == 0)
	    has_spc = i;
	  else if (strcmp (tdesc_register_name (gdbarch, i), lvx_ev_name) == 0)
	    has_ev = i;
	  else if (strcmp (tdesc_register_name (gdbarch, i), lvx_ea_pl0_name)
		   == 0)
	    has_ea_pl0 = i;
	  else if (strcmp (tdesc_register_name (gdbarch, i), lvx_es_pl0_name)
		   == 0)
	    has_es_pl0 = i;
	  else if (strcmp (tdesc_register_name (gdbarch, i), lvx_syo_name) == 0)
	    has_syo = i;
	  else if (strcmp (tdesc_register_name (gdbarch, i), lvx_ev_pl0_name)
		   == 0)
	    has_ev_pl0 = i;
	}

      if (has_r0 < 0)
	error ("There's no '%s' register!", lvx_r0_name);
      if (has_pc < 0)
	error ("There's no '%s' register!", pc_name);
      if (has_sp < 0)
	error ("There's no '%s' register!", sp_name);
      if (has_le < 0)
	error ("There's no '%s' register!", lvx_le_name);
      if (has_ls < 0)
	error ("There's no '%s' register!", lvx_ls_name);
      if (has_lc < 0)
	error ("There's no '%s' register!", lvx_lc_name);
      if (has_ps < 0)
	error ("There's no '%s' register!", lvx_ps_name);
      if (has_local < 0)
	error ("There's no '%s' register!", lvx_local_name);
      if (has_ra < 0)
	error ("There's no '%s' register!", lvx_ra_name);
      if (has_spc < 0)
	error ("There's no '%s' register!", lvx_spc_name);
      if (has_ev < 0)
	error ("There's no '%s' register!", lvx_ev_name);
      if (has_ea_pl0 < 0)
	error ("There's no '%s' register!", lvx_ea_pl0_name);
      if (has_es_pl0 < 0)
	error ("There's no '%s' register!", lvx_es_pl0_name);
      if (has_syo < 0)
	error ("There's no '%s' register!", lvx_syo_name);
      if (has_ev_pl0 < 0)
	error ("There's no '%s' register!", lvx_ev_pl0_name);

      tdep->r0_regnum = has_r0;
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
      tdep->srf_offset = has_pc;

      set_gdbarch_pc_regnum (gdbarch, has_pc);
      set_gdbarch_sp_regnum (gdbarch, has_sp);
    }
  else
    {
      set_gdbarch_num_regs (gdbarch, 1);
      set_gdbarch_register_name (gdbarch, lvx_dummy_register_name);
      set_gdbarch_register_type (gdbarch, lvx_dummy_register_type);
    }

  set_gdbarch_register_reggroup_p (gdbarch, lvx_register_reggroup_p);

  set_gdbarch_num_pseudo_regs (gdbarch, lvx_num_pseudos (gdbarch));
  set_tdesc_pseudo_register_name (gdbarch, lvx_pseudo_register_name);
  set_tdesc_pseudo_register_type (gdbarch, lvx_pseudo_register_type);
  set_tdesc_pseudo_register_reggroup_p (gdbarch,
					lvx_pseudo_register_reggroup_p);

  set_gdbarch_pseudo_register_read (gdbarch, lvx_pseudo_register_read);
  set_gdbarch_pseudo_register_write (gdbarch, lvx_pseudo_register_write);
  set_gdbarch_dwarf2_reg_to_regnum (gdbarch, lvx_dwarf2_reg_to_regnum);
  dwarf2_frame_set_init_reg (gdbarch, lvx_dwarf2_frame_init_reg);

  set_gdbarch_return_value (gdbarch, lvx_return_value);
  set_gdbarch_push_dummy_call (gdbarch, lvx_push_dummy_call);
  set_gdbarch_dummy_id (gdbarch, lvx_dummy_id);

  set_gdbarch_call_dummy_location (gdbarch, ON_STACK);
  set_gdbarch_push_dummy_code (gdbarch, lvx_push_dummy_code);

  set_gdbarch_skip_prologue (gdbarch, lvx_skip_prologue);
  set_gdbarch_unwind_pc (gdbarch, lvx_unwind_pc);
  dwarf2_append_unwinders (gdbarch);
  frame_unwind_append_unwinder (gdbarch, &lvx_frame_unwind);

  set_gdbarch_breakpoint_kind_from_pc (gdbarch, lvx_breakpoint_kind_from_pc);
  set_gdbarch_sw_breakpoint_from_kind (gdbarch, lvx_sw_breakpoint_from_kind);
  set_gdbarch_adjust_breakpoint_address (gdbarch,
					 lvx_adjust_breakpoint_address);
  set_gdbarch_print_insn (gdbarch, lvx_print_insn);
  set_gdbarch_max_insn_length (gdbarch, 8 * 4);
  set_gdbarch_inner_than (gdbarch, core_addr_lessthan);

  set_gdbarch_get_longjmp_target (gdbarch, lvx_get_longjmp_target);

  if (tdesc_has_registers (tdesc))
    {
      lvx_bare_solib_ops.find_and_open_solib = lvx_find_and_open_solib;
      set_gdbarch_so_ops (gdbarch, &lvx_bare_solib_ops);
    }

  return gdbarch;
}

void
_initialize_lvx_tdep ();
void
_initialize_lvx_tdep ()
{
  lvx_look_for_insns ();
  gdbarch_register (bfd_arch_lvx, lvx_gdbarch_init);
}
