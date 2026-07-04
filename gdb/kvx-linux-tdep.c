/* Target-dependent code for the Kalray KVX for GDB, the GNU debugger.

   Copyright (C) 2020 Kalray

   This file is part of GDB.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program. If not, see <http://www.gnu.org/licenses/>. */

#include "defs.h"

#include <libgen.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>
#include <stdlib.h>

#include "inferior.h"
#include "osabi.h"
#include "regset.h"
#include "linux-tdep.h"
#include "solib-svr4.h"
#include "symtab.h"
#include "gdbthread.h"
#include "top.h"
#include "gdbarch.h"
#include "target-descriptions.h"
#include "frame-unwind.h"
#include "arch-utils.h"
#include "observable.h"
#include "cli/cli-cmds.h"
#include "remote.h"
#include "cli/cli-decode.h"
#include "../gdbsupport/common-exceptions.h"
#include "kvx-common-tdep.h"
#include "features/kvx-linux.c"
#include "trad-frame.h"
#include "tramp-frame.h"

char *sysroot_path = NULL;
uint32_t breakpoint_linux = 0x0;

#define OFS_R0_IN_UCONTEXT 0x28
#define OFS_RA_IN_UCONTEXT 0x240
#define OFS_SPC_IN_UCONTEXT 0x250

static void
kvx_linux_sigframe_init (const struct tramp_frame *self,
			 frame_info_ptr this_frame,
			 struct trad_frame_cache *this_cache, CORE_ADDR func)
{
  struct gdbarch *gdbarch = get_frame_arch (this_frame);
  int i, pc_regnum = gdbarch_pc_regnum (gdbarch);
  kvx_gdbarch_tdep *tdep = gdbarch_tdep<kvx_gdbarch_tdep> (gdbarch);
  CORE_ADDR sp = get_frame_register_unsigned (this_frame, tdep->r0_regnum + 12);
  CORE_ADDR uc = get_frame_register_unsigned (this_frame, tdep->r0_regnum + 18);

  for (i = 0; i < 64; i++)
    {
      trad_frame_set_reg_addr (this_cache, tdep->r0_regnum + i,
			       uc + OFS_R0_IN_UCONTEXT + i * sizeof (CORE_ADDR));
    }

  trad_frame_set_reg_addr (this_cache, tdep->ra_regnum,
			   uc + OFS_RA_IN_UCONTEXT);
  trad_frame_set_reg_addr (this_cache, pc_regnum,
			   uc + OFS_SPC_IN_UCONTEXT);

  trad_frame_set_id (this_cache, frame_id_build (sp, func));
}

static const struct tramp_frame kvx_linux_rt_sigframe = {
  SIGTRAMP_FRAME,
  4,
  {
    {0x6018008b, ULONGEST_MAX}, /* make $r6 = 139 */
    {0x0fe40006, ULONGEST_MAX}, /* scall $r6 */
    {TRAMP_SENTINEL_INSN, ULONGEST_MAX}
  },
  kvx_linux_sigframe_init
};

static const gdb_byte *
kvx_linux_breakpoint_from_pc (struct gdbarch *gdbarch, CORE_ADDR *pc, int *len)
{
  *len = 4;
  return (gdb_byte *) &breakpoint_linux;
}

static int
kvx_linux_breakpoint_kind_from_pc (struct gdbarch *gdbarch, CORE_ADDR *pcptr)
{
  return 4;
}

static const gdb_byte *
kvx_linux_sw_breakpoint_from_kind (struct gdbarch *gdbarch, int kind, int *size)
{
  if (kind == 4)
    return kvx_linux_breakpoint_from_pc (gdbarch, NULL, size);

  error (_ ("Invalid software breakpoint kind %d (expected 4)"), kind);
}

static struct gdbarch *
kvx_gdbarch_init (struct gdbarch_info info, struct gdbarch_list *arches)
{
  struct gdbarch *gdbarch;
  kvx_gdbarch_tdep *tdep;
  const struct target_desc *tdesc = info.target_desc;
  int i;
  int has_lc = -1, has_le = -1, has_ls = -1, has_cs = -1;
  int has_ra = -1, has_pc = -1, has_sp = -1, has_local = -1, has_r0 = -1;

  static const char kvx_r0_name[] = "r0";
  static const char kvx_lc_name[] = "lc";
  static const char kvx_le_name[] = "le";
  static const char kvx_ls_name[] = "ls";
  static const char kvx_ra_name[] = "ra";
  static const char kvx_cs_name[] = "cs";
  static const char kvx_local_name[] = "r13";

  const char *pc_name;
  const char *sp_name;

  /* If there is already a candidate, use it. */
  arches = gdbarch_list_lookup_by_info (arches, &info);
  if (arches != NULL)
    return arches->gdbarch;
  if (tdesc == NULL)
    tdesc = tdesc_kvx_linux;

  tdep = (kvx_gdbarch_tdep *) xzalloc (sizeof (kvx_gdbarch_tdep));
  gdbarch = gdbarch_alloc (&info, tdep);

  pc_name = kvx_pc_name (gdbarch);
  sp_name = kvx_sp_name (gdbarch);

  /* This could (should?) be extracted from MDS */
  set_gdbarch_short_bit (gdbarch, 16);
  set_gdbarch_int_bit (gdbarch, 32);
  set_gdbarch_long_bit (gdbarch, 64);
  set_gdbarch_long_long_bit (gdbarch, 64);
  set_gdbarch_float_bit (gdbarch, 32);
  set_gdbarch_double_bit (gdbarch, 64);
  set_gdbarch_long_double_bit (gdbarch, 64);
  set_gdbarch_ptr_bit (gdbarch, 64);

  /* Get the kvx target description from INFO. */
  if (tdesc_has_registers (tdesc))
    {
      set_gdbarch_num_regs (gdbarch, 0);
      tdesc_arch_data_up tdesc_data = tdesc_data_alloc ();
      tdesc_use_registers (gdbarch, tdesc, std::move (tdesc_data));

      for (i = 0; i < gdbarch_num_regs (gdbarch); ++i)
	{
	  if (strcmp (tdesc_register_name (gdbarch, i), kvx_r0_name) == 0)
	    has_r0 = i;
	  else if (strcmp (tdesc_register_name (gdbarch, i), kvx_lc_name) == 0)
	    has_lc = i;
	  else if (strcmp (tdesc_register_name (gdbarch, i), kvx_le_name) == 0)
	    has_le = i;
	  else if (strcmp (tdesc_register_name (gdbarch, i), kvx_ls_name) == 0)
	    has_ls = i;
	  else if (strcmp (tdesc_register_name (gdbarch, i), kvx_ra_name) == 0)
	    has_ra = i;
	  else if (strcmp (tdesc_register_name (gdbarch, i), kvx_cs_name) == 0)
	    has_cs = i;
	  if (strcmp (tdesc_register_name (gdbarch, i), pc_name) == 0)
	    has_pc = i;
	  else if (strcmp (tdesc_register_name (gdbarch, i), sp_name) == 0)
	    has_sp = i;
	  else if (strcmp (tdesc_register_name (gdbarch, i), kvx_local_name)
		   == 0)
	    has_local = i;
	}

      if (has_r0 < 0)
	error ("There's no '%s' register!", kvx_r0_name);
      if (has_lc < 0)
	error ("There's no '%s' register!", kvx_lc_name);
      if (has_le < 0)
	error ("There's no '%s' register!", kvx_le_name);
      if (has_ls < 0)
	error ("There's no '%s' register!", kvx_ls_name);
      if (has_ra < 0)
	error ("There's no '%s' register!", kvx_ra_name);
      if (has_pc < 0)
	error ("There's no '%s' register!", pc_name);
      if (has_sp < 0)
	error ("There's no '%s' register!", sp_name);
      if (has_local < 0)
	error ("There's no '%s' register!", kvx_local_name);

      tdep->r0_regnum = has_r0;
      tdep->lc_regnum = has_lc;
      tdep->le_regnum = has_le;
      tdep->ls_regnum = has_ls;
      tdep->ra_regnum = has_ra;
      tdep->cs_regnum = has_cs;
      tdep->local_regnum = has_local;
      set_gdbarch_pc_regnum (gdbarch, has_pc);
      set_gdbarch_sp_regnum (gdbarch, has_sp);

      tdep->uint256 = arch_integer_type (gdbarch, 256, 0, "uint256_t");
      tdep->uint512 = arch_integer_type (gdbarch, 512, 0, "uint512_t");
      tdep->uint1024 = arch_integer_type (gdbarch, 1024, 0, "uint1024_t");
    }
  else
    {
      set_gdbarch_num_regs (gdbarch, 1);
      set_gdbarch_register_name (gdbarch, kvx_dummy_register_name);
      set_gdbarch_register_type (gdbarch, kvx_dummy_register_type);
    }

  set_gdbarch_num_pseudo_regs (gdbarch, kvx_num_pseudos (gdbarch));

  set_tdesc_pseudo_register_name (gdbarch, kvx_pseudo_register_name);
  set_tdesc_pseudo_register_type (gdbarch, kvx_pseudo_register_type);
  set_tdesc_pseudo_register_reggroup_p (gdbarch,
					kvx_pseudo_register_reggroup_p);

  set_gdbarch_pseudo_register_read (gdbarch, kvx_pseudo_register_read);
  set_gdbarch_pseudo_register_write (gdbarch, kvx_pseudo_register_write);
  set_gdbarch_dwarf2_reg_to_regnum (gdbarch, kvx_dwarf2_reg_to_regnum);
  dwarf2_frame_set_init_reg (gdbarch, kvx_dwarf2_frame_init_reg);

  set_gdbarch_return_value (gdbarch, kvx_return_value);
  set_gdbarch_push_dummy_call (gdbarch, kvx_push_dummy_call);
  set_gdbarch_dummy_id (gdbarch, kvx_dummy_id);

  set_gdbarch_skip_prologue (gdbarch, kvx_skip_prologue);
  set_gdbarch_unwind_pc (gdbarch, kvx_unwind_pc);
  dwarf2_append_unwinders (gdbarch);
  frame_unwind_append_unwinder (gdbarch, &kvx_frame_unwind);

  set_gdbarch_breakpoint_from_pc (gdbarch, kvx_linux_breakpoint_from_pc);
  set_gdbarch_breakpoint_kind_from_pc (gdbarch,
				       kvx_linux_breakpoint_kind_from_pc);
  set_gdbarch_sw_breakpoint_from_kind (gdbarch,
				       kvx_linux_sw_breakpoint_from_kind);
  set_gdbarch_adjust_breakpoint_address (gdbarch,
					 kvx_adjust_breakpoint_address);
  /* Settings that should be unnecessary. */
  set_gdbarch_inner_than (gdbarch, core_addr_lessthan);
  set_gdbarch_print_insn (gdbarch, kvx_print_insn);

  set_gdbarch_max_insn_length (gdbarch, 8 * 4);

  set_gdbarch_get_longjmp_target (gdbarch, kvx_get_longjmp_target);

  if (tdesc_has_registers (tdesc) && info.bfd_arch_info)
    {
      unsigned long bfd_mach = info.bfd_arch_info->mach;

      /* Hook in the ABI-specific overrides, if they have been registered. */
      if (bfd_mach == bfd_mach_kv3_1_64 || bfd_mach == bfd_mach_kv3_1_usr
	  || bfd_mach == bfd_mach_kv3_2_64 || bfd_mach == bfd_mach_kv3_2_usr)
	gdbarch_init_osabi (info, gdbarch);
    }

  return gdbarch;
}

#ifndef __KVX__
static void
attach_user_command (const char *args, int from_tty)
{
  static const char *syntax
    = "Syntax: attach-user <comm> <path_in_initrd_kvx_linux_program>\n";
  const char *pargs;
  char file_dir[PATH_MAX];
  std::string kvx_comm, comm, file, cmd;
  struct stat vstat;
  int ret;

  if (inferior_ptid != null_ptid)
    {
      fprintf (stderr, "Gdb already attached!\n");
      return;
    }

  pargs = args;
  kvx_comm = comm = extract_arg (&pargs);
  if (comm.empty ())
    {
      fprintf (stderr, "Error: the comm was not specified!\n%s", syntax);
      return;
    }
  file = extract_arg (&pargs);
  if (file.empty ())
    {
      fprintf (stderr, "Error: the KVX Linux program was not specified!\n%s",
	       syntax);
      return;
    }
  if (stat (file.c_str (), &vstat) || S_ISDIR (vstat.st_mode)
      || realpath (file.c_str (), file_dir) == NULL)
    {
      fprintf (stderr, "Error: Cannot find file %s!\n", file.c_str ());
      return;
    }

  execute_command ("set pagination off", 0);

  // set file
  cmd = "file " + file;
  execute_command (cmd.c_str (), 0);

  // set sysroot
  if (sysroot_path)
    {
      free (sysroot_path);
      sysroot_path = NULL;
    }
  strcpy (file_dir, dirname (file_dir));
  while (*file_dir && (file_dir[0] != '/' || file_dir[1] != 0))
    {
      cmd = std::string (file_dir) + "/etc/inittab";
      if (stat (cmd.c_str (), &vstat) == 0)
	{
	  // found the root of kvx filesystem
	  sysroot_path = file_dir;
	  cmd = std::string ("set sysroot ") + file_dir;
	  fprintf (stderr, "Set sysroot to %s\n", file_dir);
	  execute_command (cmd.c_str (), 0);
	  break;
	}

      strcpy (file_dir, dirname (file_dir));
    }

  ret = 0;
  try
    {
      cmd = "target remote " + comm;
      execute_command (cmd.c_str (), 0);
    }
  catch (const gdb_exception_error &ex)
    {
      printf ("Error while trying to connect (%s).\n", ex.what ());
      ret = -1;
    }

  if (ret)
    return;

  signal_stop_update (GDB_SIGNAL_REALTIME_32, 0);
  signal_print_update (GDB_SIGNAL_REALTIME_32, 0);
  signal_pass_update (GDB_SIGNAL_REALTIME_32, 1);
  signal_stop_update (GDB_SIGNAL_REALTIME_33, 0);
  signal_print_update (GDB_SIGNAL_REALTIME_33, 0);
  signal_pass_update (GDB_SIGNAL_REALTIME_33, 1);

  fprintf (stderr, "Attached to KVX linux user debug using %s.\n",
	   kvx_comm.c_str ());
}
#endif

static void
kvx_inferior_created (struct inferior *inf)
{
  kvx_current_arch = KVX_NUM_ARCHES;
}

/* Core file and register set support.  */
#define KVX_LINUX_SIZEOF_GREGSET ((64 + 6) * sizeof (uint64_t))

static void
kvx_linux_supply_collect_gregset (struct regcache *rc, int regnum,
				  const void *gregs_buf, int to_supply)
{
  uint64_t *p = (uint64_t *) gregs_buf;
  gdbarch *gdbarch = rc->arch ();
  kvx_gdbarch_tdep *tdep = gdbarch_tdep<kvx_gdbarch_tdep> (gdbarch);
  int pc_regnum = gdbarch_pc_regnum (gdbarch);
  const int sfrs[] = {tdep->lc_regnum, tdep->le_regnum, tdep->ls_regnum,
		      tdep->ra_regnum, tdep->cs_regnum, pc_regnum};
  const int no_sfrs = sizeof (sfrs) / sizeof (sfrs[0]);
  int i, regno;

  for (regno = tdep->r0_regnum; regno < tdep->r0_regnum + 64; regno++)
    if (regnum == -1 || regnum == regno)
      {
	if (to_supply)
	  rc->raw_supply (regno, p++);
	else
	  rc->raw_collect (regno, p++);
      }

  for (i = 0; i < no_sfrs; i++)
    {
      regno = sfrs[i];
      if (regnum == -1 || regnum == regno)
	{
	  if (to_supply)
	    rc->raw_supply (regno, p++);
	  else
	    rc->raw_collect (regno, p++);
	}
    }
}

static void
kvx_linux_supply_gregset (const struct regset *regset, struct regcache *rc,
			  int regnum, const void *gregs_buf, size_t len)
{
  kvx_linux_supply_collect_gregset (rc, regnum, gregs_buf, 1);
}

static void
kvx_linux_collect_gregset (const struct regset *regset,
			   const struct regcache *rc, int regnum,
			   void *gregs_buf, size_t len)
{
  kvx_linux_supply_collect_gregset ((struct regcache *) rc, regnum, gregs_buf,
				    0);
}

static const struct regset kvx_linux_gregset
  = {NULL, kvx_linux_supply_gregset, kvx_linux_collect_gregset};

/* Iterate over core file register note sections.  */

static void
kvx_linux_iterate_over_regset_sections (struct gdbarch *gdbarch,
					iterate_over_regset_sections_cb *cb,
					void *cb_data,
					const struct regcache *regcache)
{
  cb (".reg", KVX_LINUX_SIZEOF_GREGSET, KVX_LINUX_SIZEOF_GREGSET,
      &kvx_linux_gregset, NULL, cb_data);
}

/* Determine target description from core file.  */

static const struct target_desc *
kvx_linux_core_read_description (struct gdbarch *gdbarch,
				 struct target_ops *target, bfd *abfd)
{
  const struct target_desc *tdesc = gdbarch_target_desc (gdbarch);

  return tdesc;
}

/* OS specific initialization of gdbarch. */
static void
kvx_linux_init_abi (struct gdbarch_info info, struct gdbarch *gdbarch)
{
  linux_init_abi (info, gdbarch, 0);

  tramp_frame_prepend_unwinder (gdbarch, &kvx_linux_rt_sigframe);

  set_solib_svr4_fetch_link_map_offsets (gdbarch,
					 svr4_lp64_fetch_link_map_offsets);

  /* Enable TLS support. */
  set_gdbarch_fetch_tls_load_module_address (gdbarch,
					     svr4_fetch_objfile_link_map);

  /* Core file support.  */
  set_gdbarch_iterate_over_regset_sections (
    gdbarch, kvx_linux_iterate_over_regset_sections);
  set_gdbarch_core_read_description (gdbarch, kvx_linux_core_read_description);
}

static void
add_kvx_commands (void)
{
#ifndef __KVX__
  add_com (
    "attach-user", class_run, attach_user_command,
    "Connect to gdbserver running on MPPA.\n"
    "Usage is 'attach-user <comm> [<path_in_initrd_kvx_linux_program>]'.");
#endif
}

int
htab_eq_string (const void *a, const void *b) __attribute__ ((weak));
int
htab_eq_string (const void *a, const void *b)
{
  return strcmp ((const char *) a, (const char *) b) == 0;
}

extern initialize_file_ftype _initialize_kvx_linux_tdep;
void
_initialize_kvx_linux_tdep ()
{
  gdbarch_register_osabi (bfd_arch_kvx, bfd_mach_kv3_1_64, GDB_OSABI_LINUX,
			  kvx_linux_init_abi);
  gdbarch_register_osabi (bfd_arch_kvx, bfd_mach_kv3_1_usr, GDB_OSABI_LINUX,
			  kvx_linux_init_abi);
  gdbarch_register_osabi (bfd_arch_kvx, bfd_mach_kv3_2_64, GDB_OSABI_LINUX,
			  kvx_linux_init_abi);
  gdbarch_register_osabi (bfd_arch_kvx, bfd_mach_kv3_2_usr, GDB_OSABI_LINUX,
			  kvx_linux_init_abi);
  add_kvx_commands ();
}

extern initialize_file_ftype _initialize_kvx_tdep;
void
_initialize_kvx_tdep ()
{
  kvx_look_for_insns ();
  gdbarch_register (bfd_arch_kvx, kvx_gdbarch_init, NULL);
  gdb::observers::inferior_created.attach (kvx_inferior_created,
					   "kvx-linux-tdep");

  initialize_tdesc_kvx_linux ();
}
