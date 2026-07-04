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
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "defs.h"
#include "frame-unwind.h"
#include "gdbcore.h"
#include "target-descriptions.h"
#include "user-regs.h"
#include "elf-bfd.h"
#include "inferior.h"
#include "gdbthread.h"
#include "arch-utils.h"
#include "objfiles.h"

extern "C" {
#include "opcodes/disassemble.h"
}
#include "elf/lvx.h"
#include "opcode/lvx.h"
extern "C" {
#include "opcodes/lvx-dis.h"
}
#include "lvx-common-tdep.h"

#define __NR_BREAKPOINT_PL0 4050
#define __NR_BREAKPOINT_JTAGISS 4054
#define REGNUM_STRUCT 15

enum lvx_frm_cache_reg_saved_loc_type
{
  LVX_FRM_CACHE_REG_LOC_REG,
  LVX_FRM_CACHE_REG_LOC_MEM,
  LVX_FRM_CACHE_REG_LOC_NONE,
};

struct lvx_frm_cache_reg_saved
{
  enum lvx_frm_cache_reg_saved_loc_type loc_type;
  int reg;
  CORE_ADDR offset;
};

struct lvx_frame_cache
{
  frame_info_ptr frame;

  struct lvx_frm_cache_reg_saved base;
  struct lvx_frm_cache_reg_saved ra;
  struct lvx_frm_cache_reg_saved fp;

  CORE_ADDR function_pc;
  CORE_ADDR frame_pc;
  CORE_ADDR sp_fp_offset;
  CORE_ADDR sp_entry_offset;

  int has_frame;
  int is_invalid;
};

enum LVX_ARCH lvx_current_arch = LVX_NUM_ARCHES;

struct op_list *branch_insns[LVX_NUM_ARCHES];
struct op_list *pcrel_insn[LVX_NUM_ARCHES];
uint32_t break_op[LVX_NUM_ARCHES];
uint32_t break_jtag_over_iss[LVX_NUM_ARCHES];
uint32_t nop_op[LVX_NUM_ARCHES];

enum LVX_ARCH
lvx_arch (void)
{
  const struct target_desc *desc = target_current_description ();
  bfd *exec_bfd;

  if (lvx_current_arch != LVX_NUM_ARCHES)
    return lvx_current_arch;

  exec_bfd = current_program_space->exec_bfd ();
  if (exec_bfd)
    {
      switch (elf_elfheader (exec_bfd)->e_flags & ELF_LVX_CORE_MASK)
	{
	case ELF_LVX_CORE_LVX_V1:
	  lvx_current_arch = LVX_1;
	  break;
	case ELF_LVX_CORE_LVX_V2:
	  lvx_current_arch = LVX_2;
	  break;
	default:
	  error (_ ("The LVX binary is compiled for an unknown core."));
	}
    }
  else if (desc && tdesc_architecture (desc))
    {
      const char *name = tdesc_architecture (desc)->printable_name;
      if (strstr (name, "lvx-1"))
	lvx_current_arch = LVX_1;
      else if (strstr (name, "lvx-2"))
	lvx_current_arch = LVX_2;
      else
	error ("unable to find the current lvx architecture.");
    }

  if (lvx_current_arch == LVX_NUM_ARCHES)
    return LVX_1;

  return lvx_current_arch;
}

int
get_lvx_arch (void)
{
  return lvx_arch ();
}

int
get_lvx_target_core_vers (int *target_ver, int *core_ver)
{
  int arch = lvx_arch ();

  switch (arch)
    {
    case LVX_1:
      *target_ver = 1;
      *core_ver = 1;
      break;
    case LVX_2:
      *target_ver = 1;
      *core_ver = 2;
      break;
    default:
      *target_ver = 0;
      *core_ver = 0;
      return 1;
    }

  return 0;
}

const char *
lvx_dummy_register_name (struct gdbarch *gdbarch, int regno)
{
  return "";
}

struct type *
lvx_dummy_register_type (struct gdbarch *gdbarch, int regno)
{
  return builtin_type (gdbarch)->builtin_int;
}

CORE_ADDR
lvx_adjust_breakpoint_address (struct gdbarch *gdbarch, CORE_ADDR bpaddr)
{
  gdb_byte syllab_buf[4];
  uint32_t syllab;
  enum bfd_endian order = gdbarch_byte_order_for_code (gdbarch);
  CORE_ADDR adjusted = bpaddr;
  int i = 0;

  adjusted &= ~3;
  if (adjusted == 0 || inferior_ptid == null_ptid
      || inferior_thread ()->state != THREAD_STOPPED)
    return adjusted;

  /* Look for the end of the bundle preceeding the requested address. */
  do
    {
      if (target_read_memory (adjusted - 4, syllab_buf, 4) != 0)
	return adjusted;

      syllab = extract_unsigned_integer (syllab_buf, 4, order);
    }
  while ((i++ < 7) && (syllab >> 31) && (adjusted -= 4));

  if (i == 8) /* Impossible for correct instructions */
    return bpaddr & ~3;

  return adjusted;
}

/* Return PC of first real instruction.  */
/* This function is nearly completely copied from
 * symtab.c:skip_prologue_using_lineinfo (expect the test in the loop at the
 * end.
 */
CORE_ADDR
lvx_skip_prologue (struct gdbarch *gdbarch, CORE_ADDR func_addr)
{
  CORE_ADDR func_start, func_end;
  struct linetable *l;
  struct symtab_and_line sal;
  struct compunit_symtab *cust;
  int ind, i, gcc_compiled = 0, other_compiled = 0;
  const char *producer;

  sal = find_pc_line (func_addr, 0);
  if (sal.symtab == NULL)
    return func_addr;

  cust = find_pc_compunit_symtab (func_addr);
  producer = cust->producer ();
  if (producer && strncmp (producer, "GNU C", 5) == 0)
    gcc_compiled = 1;
  else if (producer && strstr (producer, "clang"))
    other_compiled = 1;

  /* Give up if this symbol has no lineinfo table.  */
  l = sal.symtab->linetable ();
  if (l == NULL)
    return func_addr;

  /* Get the range for the function's PC values, or give up if we
     cannot, for some reason.  */
  if (!find_pc_partial_function (func_addr, NULL, &func_start, &func_end))
    return func_addr;

  /* Linetable entries are ordered by PC values, see the commentary in
     symtab.h where `struct linetable' is defined.  Thus, the first
     entry whose PC is in the range [FUNC_START..FUNC_END[ is the
     address we are looking for.  */
  if (gcc_compiled)
    {
      ind = 0;
      for (i = 0; i < l->nitems; i++)
	{
	  struct linetable_entry *item = &(l->item[i]);

	  if (item->line > 0 && func_start <= item->pc && item->pc < func_end)
	    {
	      if (ind == 0)
		++ind;
	      else
		return item->pc;
	    }
	}
    }
  else
    {
      ind = 0;
      for (i = 0; i < l->nitems; i++)
	{
	  struct linetable_entry *item = &(l->item[i]);

	  /* Don't use line numbers of zero, they mark special entries in
	    the table.  See the commentary on symtab.h before the
	    definition of struct linetable.  */
	  if (item->line > 0 && func_start <= item->pc && item->pc < func_end)
	    {
	      if (item->pc == func_start && (!other_compiled || ind++ == 0))
		continue;
	      return item->pc;
	    }
	}
    }

  return func_addr;
}

CORE_ADDR
lvx_unwind_pc (struct gdbarch *gdbarch, frame_info_ptr next_frame)
{
  return frame_unwind_register_unsigned (next_frame,
					 gdbarch_pc_regnum (gdbarch));
}

/* Initialize the cache struct of a frame */
static void
lvx_init_frame_cache (struct lvx_frame_cache *cache, frame_info_ptr frame)
{
  struct gdbarch *gdbarch = get_frame_arch (frame);

  cache->frame = frame;

  cache->function_pc = 0;
  cache->frame_pc = 0;
  cache->sp_fp_offset = 0;
  cache->sp_entry_offset = 0;

  /* at the function beginning, the previous frame PC is in RA */
  cache->ra.loc_type = LVX_FRM_CACHE_REG_LOC_REG;
  cache->ra.reg = user_reg_map_name_to_regnum (gdbarch, "ra", -1);
  cache->ra.offset = 0;

  /* the previous frame R12 is still in R12 */
  cache->base.loc_type = LVX_FRM_CACHE_REG_LOC_REG;
  cache->base.reg
    = user_reg_map_name_to_regnum (gdbarch, "r0", -1) + LVX_GPR_REG_SP;
  cache->base.offset = 0;

  /* the previous frame R14 is still in R14 */
  cache->fp.loc_type = LVX_FRM_CACHE_REG_LOC_REG;
  cache->fp.reg
    = user_reg_map_name_to_regnum (gdbarch, "r0", -1) + LVX_GPR_REG_FP;
  cache->fp.offset = 0;

  cache->is_invalid = 0;
  cache->has_frame = 0;
}

/* Allocate and initialize a frame cache */
static struct lvx_frame_cache *
lvx_alloc_frame_cache (frame_info_ptr frame)
{
  struct lvx_frame_cache *cache;

  cache = FRAME_OBSTACK_ZALLOC (struct lvx_frame_cache);
  lvx_init_frame_cache (cache, frame);

  return cache;
}

/* Like target_read_memory, but slightly different parameters */
static int
lvx_dis_asm_read_memory (bfd_vma memaddr, gdb_byte *myaddr, unsigned int len,
			 struct disassemble_info *info)
{
  return target_read_code (memaddr, myaddr, len);
}

#define NB_BUNDLES_PROL_MAX 10
#define NB_BUNDLES_EPIL_MAX 10
static void
lvx_scan_prologue_epilogue (frame_info_ptr frame, struct lvx_frame_cache *cache)
{
  struct gdbarch *gdbarch = get_frame_arch (frame);
  struct disassemble_info di;
  struct lvx_prologue_epilogue_bundle peb;
  int nb_bytes_bundle, idx_bundle, idx_insn, idx_insn1;
  CORE_ADDR crt_pc;
  int r0_regnum, sp_regnum, fp_regnum;
  int sp_restore_to_entry, ret_from_fc, sp_restore_from_fp;
  int end_scan = 0, sp_changed = 0, ra_in_gpr = 0, ra_in_mem = 0;
  int fp_in_mem = 0, fp_used = 0;

  /* get the PC of the current frame */
  cache->frame_pc = get_frame_pc (frame);

  /* get the function beginnig PC of the current frame */
  cache->function_pc = get_frame_func (frame);
  if (!cache->function_pc || cache->frame_pc < cache->function_pc)
    {
      cache->is_invalid = 1;
      return;
    }

  /* for the first bundle of the function, the frame cache is already correct */
  if (cache->frame_pc == cache->function_pc)
    return;

  /* get the required registers numbers */
  r0_regnum = user_reg_map_name_to_regnum (gdbarch, "r0", -1);
  sp_regnum = r0_regnum + LVX_GPR_REG_SP;
  fp_regnum = r0_regnum + LVX_GPR_REG_FP;

  /* init the disassemble_info struct */
  memset (&di, 0, sizeof (di));
  di.arch = gdbarch_bfd_arch_info (gdbarch)->arch;
  di.flavour = bfd_target_unknown_flavour;
  di.endian = gdbarch_byte_order (gdbarch);
  di.endian_code = gdbarch_byte_order_for_code (gdbarch);
  di.mach = gdbarch_bfd_arch_info (gdbarch)->mach;
  di.read_memory_func = lvx_dis_asm_read_memory;

  /* scan the prologue for instructions changing/saving R12, R14 and RA */
  /* the searched prologue instructions are:
     (1) addd $r12 = $r12, <res_stack> - taken into account only if (1) hasn't
     been parsed yet
     (2) get <gpr_reg> = $ra - only if not (2) and not (3)
     (3) sd <ofs>[$r12] = <gpr_reg> - only if (2) and not (3); sq, so supported
     (4) sd <ofs>[$r12] = $r14 - only if not (4); sq, so also supported
     (5) addd $r14 = $r12, <fp_ofs> - only if (1), (4) and not (5)
     (6) call, icall, goto, igoto, cb., ret - stop scanning */

  for (crt_pc = cache->function_pc, idx_bundle = 0;
       crt_pc < cache->frame_pc && idx_bundle < NB_BUNDLES_PROL_MAX;
       crt_pc += nb_bytes_bundle, idx_bundle++)
    {
      int insn_order[sizeof (peb.insn) / sizeof (peb.insn[0])];

      /* get the instructions of the current scanned bundle */
      nb_bytes_bundle = decode_prologue_epilogue_bundle (crt_pc, &di, &peb);
      if (nb_bytes_bundle == -1)
	{
	  cache->is_invalid = 1;
	  return;
	}

      if (!peb.nb_insn)
	continue;

      /* sort the bundle instructions by their type */
      for (idx_insn = 0; idx_insn < peb.nb_insn; idx_insn++)
	insn_order[idx_insn] = idx_insn;

      for (idx_insn = 0; idx_insn < peb.nb_insn - 1; idx_insn++)
	for (idx_insn1 = idx_insn + 1; idx_insn1 < peb.nb_insn; idx_insn1++)
	  if (peb.insn[insn_order[idx_insn]].insn_type
	      > peb.insn[insn_order[idx_insn1]].insn_type)
	    {
	      int tinstr_order = insn_order[idx_insn];
	      insn_order[idx_insn] = insn_order[idx_insn1];
	      insn_order[idx_insn1] = tinstr_order;
	    }

      /* update the cache struct fields based on the bundle instructions */
      for (idx_insn = 0; idx_insn < peb.nb_insn; idx_insn++)
	{
	  struct lvx_prologue_epilogue_insn *insn
	    = &peb.insn[insn_order[idx_insn]];
	  int store_nb_gprs = 0;

	  switch (insn->insn_type)
	    {
	    case LVX_PROL_EPIL_INSN_ADD_SP:
	      /* addd $r12 = $r12, <res_stack> -> <res_stack>: immediate */
	      if (!sp_changed)
		{
		  cache->base.loc_type = LVX_FRM_CACHE_REG_LOC_REG;
		  cache->base.reg = sp_regnum;
		  cache->base.offset = -insn->immediate;
		  cache->sp_entry_offset = cache->base.offset;
		  sp_changed = 1;
		}
	      break;
	    case LVX_PROL_EPIL_INSN_ADD_FP:
	      /* addd $r14 = $r12, <ofs> -> <ofs>: immediate */
	      if (sp_changed && fp_in_mem && !fp_used)
		{
		  cache->base.reg = fp_regnum;
		  cache->base.offset -= insn->immediate;
		  cache->sp_fp_offset = insn->immediate;
		  if (ra_in_mem)
		    {
		      cache->ra.reg = fp_regnum;
		      cache->ra.offset -= insn->immediate;
		    }
		  if (fp_in_mem)
		    {
		      cache->fp.reg = fp_regnum;
		      cache->fp.offset -= insn->immediate;
		    }
		  fp_used = 1;
		}
	      break;
	    case LVX_PROL_EPIL_INSN_GET_RA:
	      /* get <gpr_get_ra> = $ra -> <gpr_get_ra>: gpr_reg[0] */
	      if (!ra_in_gpr && !ra_in_mem)
		{
		  cache->ra.loc_type = LVX_FRM_CACHE_REG_LOC_REG;
		  cache->ra.reg = r0_regnum + insn->gpr_reg[0];
		  cache->ra.offset = 0;
		  ra_in_gpr = 1;
		}
	      break;
	    case LVX_PROL_EPIL_INSN_SD:
	      store_nb_gprs = 1;
	      break;
	    case LVX_PROL_EPIL_INSN_SQ:
	      store_nb_gprs = 2;
	      break;
	    case LVX_PROL_EPIL_INSN_SO:
	      store_nb_gprs = 4;
	      break;
	    case LVX_PROL_EPIL_INSN_RET:
	    case LVX_PROL_EPIL_INSN_GOTO:
	    case LVX_PROL_EPIL_INSN_IGOTO:
	    case LVX_PROL_EPIL_INSN_CB:
	    case LVX_PROL_EPIL_INSN_CALL:
	      end_scan = 1;
	      break;
	    default:
	      break;
	    } /* switch bundle instruction type */

	  /* invalid frame if there is a store into the stack without a previous
	     stack allocation */
	  if (store_nb_gprs && !sp_changed && insn->nb_gprs
	      && insn->gpr_reg[0] == LVX_GPR_REG_SP)
	    {
	      cache->is_invalid = 1;
	      return;
	    }

	  if (store_nb_gprs)
	    {
	      /* sd <ofs>[r12|r14] = <gpr_get_ra> -> ofs: immediate,
		 r12|r14: gpr_reg[0], <gpr_get_ra>: gpr_reg[1] */
	      if (ra_in_gpr && insn->nb_gprs == 2
		  && (insn->gpr_reg[0] == LVX_GPR_REG_SP
		      || insn->gpr_reg[0] == LVX_GPR_REG_FP)
		  && insn->gpr_reg[1] + r0_regnum <= cache->ra.reg
		  && insn->gpr_reg[1] + r0_regnum + store_nb_gprs
		       > cache->ra.reg)
		{
		  cache->ra.loc_type = LVX_FRM_CACHE_REG_LOC_MEM;
		  if (fp_used && insn->gpr_reg[0] == LVX_GPR_REG_SP)
		    {
		      cache->ra.offset
			= insn->immediate - cache->sp_fp_offset
			  + (cache->ra.reg - (insn->gpr_reg[1] + r0_regnum))
			      * 8;
		      cache->ra.reg = fp_regnum;
		    }
		  else
		    {
		      cache->ra.offset
			= insn->immediate
			  + (cache->ra.reg - (insn->gpr_reg[1] + r0_regnum))
			      * 8;
		      cache->ra.reg = r0_regnum + insn->gpr_reg[0];
		    }
		  ra_in_mem = 1;
		}

	      /* sd <ofs>[r12] = <r14> -> ofs: immediate,
		 r12: gpr_reg[0], r14: gpr_reg[1] */
	      if (!fp_in_mem && insn->gpr_reg[0] == LVX_GPR_REG_SP
		  && insn->gpr_reg[1] <= LVX_GPR_REG_FP
		  && insn->gpr_reg[1] + store_nb_gprs > LVX_GPR_REG_FP)
		{
		  cache->fp.loc_type = LVX_FRM_CACHE_REG_LOC_MEM;
		  cache->fp.reg = sp_regnum;
		  cache->fp.offset
		    = insn->immediate + (LVX_GPR_REG_FP - insn->gpr_reg[1]) * 8;
		  fp_in_mem = 1;
		}
	    }
	} /* foreach instruction in the current bundle */

      cache->has_frame
	= (sp_changed && ra_in_mem) || (!sp_changed && ra_in_gpr);

      /* stop scanning if a PC changing instruction was parsed or if the frame
	 cache struct is completely updated */
      if (end_scan || (fp_used && ra_in_mem))
	break;
    } /* foreach bundle in prologue */

  /* scan for the epilogue only for the top frame */
  if (frame_relative_level (frame) != 0)
    return;

  /* scan epilogue: search for ret and SP restoring instructions */
  /* the searched epilogue instructions are:
     (1) addd $r12 = $r12, <res_stack> - if not (1) and SP changed in prologue
     (2) addd $r12 = $r14, <offset> - if not (1) and FP is used
     (3) ret, goto <function> - epilogue found
     (4) call, icall, igoto, cb., goto <label_in_function> - stop scanning */
  end_scan = 0;
  ret_from_fc = 0;
  sp_restore_to_entry = 0;
  sp_restore_from_fp = 0;
  for (crt_pc = cache->frame_pc, idx_bundle = 0;
       idx_bundle < NB_BUNDLES_EPIL_MAX;
       crt_pc += nb_bytes_bundle, idx_bundle++)
    {
      nb_bytes_bundle = decode_prologue_epilogue_bundle (crt_pc, &di, &peb);
      if (nb_bytes_bundle == -1)
	{
	  cache->is_invalid = 1;
	  return;
	}

      if (!peb.nb_insn)
	continue;

      for (idx_insn = 0; idx_insn < peb.nb_insn; idx_insn++)
	{
	  struct lvx_prologue_epilogue_insn *insn = &peb.insn[idx_insn];

	  switch (insn->insn_type)
	    {
	    case LVX_PROL_EPIL_INSN_ADD_SP:
	      /* addd $r12 = $r12, <res_stack> -> <res_stack>: immediate */
	      if (sp_changed && !sp_restore_to_entry)
		sp_restore_to_entry = 1;
	      break;
	    case LVX_PROL_EPIL_INSN_RESTORE_SP_FROM_FP:
	      /* addd $r12 = $r14, <ofs> */
	      if (fp_used && !sp_restore_from_fp)
		sp_restore_from_fp = 1;
	      break;
	    case LVX_PROL_EPIL_INSN_RET:
	      ret_from_fc = 1;
	      end_scan = 1;
	      break;
	    case LVX_PROL_EPIL_INSN_GOTO:
	      /* check if goto jumps to outside this function */
	      if (get_pc_function_start (insn->immediate) != cache->function_pc
		  || insn->immediate == cache->function_pc)
		ret_from_fc = 1;
	      end_scan = 1;
	      break;
	    case LVX_PROL_EPIL_INSN_CB:
	    case LVX_PROL_EPIL_INSN_IGOTO:
	    case LVX_PROL_EPIL_INSN_CALL:
	      end_scan = 1;
	    default:
	      break;
	    }
	}

      if (end_scan)
	break;
    }

  /* the epilogue stack instructions have not been yet executed */
  if (!ret_from_fc || (!fp_used && sp_restore_to_entry)
      || (fp_used && sp_restore_from_fp))
    return;

  if (fp_used && sp_restore_to_entry)
    {
      /* sp was restored from fp, but not to its value at the function entry */
      cache->base.reg = sp_regnum;
      cache->base.offset += cache->sp_fp_offset;
      if (cache->ra.loc_type == LVX_FRM_CACHE_REG_LOC_MEM
	  && cache->ra.reg == fp_regnum)
	{
	  cache->ra.reg = sp_regnum;
	  cache->ra.offset += cache->sp_fp_offset;
	}
      if (cache->fp.loc_type == LVX_FRM_CACHE_REG_LOC_MEM
	  && cache->fp.reg == fp_regnum)
	{
	  cache->fp.reg = sp_regnum;
	  cache->fp.offset += cache->sp_fp_offset;
	}
    }
  else if (!sp_restore_to_entry)
    {
      /* sp was restored to its value at the function entry */
      cache->base.reg = sp_regnum;
      cache->base.offset = 0;
      if (cache->ra.loc_type == LVX_FRM_CACHE_REG_LOC_MEM)
	{
	  cache->ra.reg = sp_regnum;
	  cache->ra.offset += cache->sp_fp_offset - cache->sp_entry_offset;
	}
      if (cache->fp.loc_type == LVX_FRM_CACHE_REG_LOC_MEM)
	{
	  cache->fp.reg = sp_regnum;
	  cache->fp.offset += cache->sp_fp_offset - cache->sp_entry_offset;
	}
    }
}

/* Returns/computes a cache struct for the provided frame */
static struct lvx_frame_cache *
lvx_get_frame_cache (frame_info_ptr frame, void **this_cache)
{
  struct lvx_frame_cache *cache;

  if (*this_cache)
    return (struct lvx_frame_cache *) *this_cache;

  cache = lvx_alloc_frame_cache (frame);
  *this_cache = cache;

  lvx_scan_prologue_epilogue (frame, cache);

  return cache;
}

/* Returns the register of a frame based on the cache struct info */
static int
lvx_get_frame_cache_reg (frame_info_ptr frame,
			 struct lvx_frm_cache_reg_saved *rs, CORE_ADDR *v)
{
  CORE_ADDR t;
  struct gdbarch *gdbarch = get_frame_arch (frame);
  enum bfd_endian byte_order = gdbarch_byte_order (gdbarch);
  int r0_regnum = user_reg_map_name_to_regnum (gdbarch, "r0", -1);
  int gpr_reg_sz = register_size (gdbarch, r0_regnum);
  gdb_byte buf[gpr_reg_sz];

  switch (rs->loc_type)
    {
    case LVX_FRM_CACHE_REG_LOC_REG:
      try
	{
	  get_frame_register (frame, rs->reg, buf);
	}
      catch (...)
	{
	  return -1;
	}

      *v = extract_unsigned_integer (buf, gpr_reg_sz, byte_order);
      *v += rs->offset;
      break;
    case LVX_FRM_CACHE_REG_LOC_MEM:
      try
	{
	  get_frame_register (frame, rs->reg, buf);
	}
      catch (...)
	{
	  return -1;
	}

      t = extract_unsigned_integer (buf, gpr_reg_sz, byte_order);
      t += rs->offset;
      *v = 0;
      if (target_read_memory (t, (gdb_byte *) v, gpr_reg_sz) < 0)
	return -1;
      break;
    case LVX_FRM_CACHE_REG_LOC_NONE:
      return -1;
    default:
      break;
    }

  /* address registers should be multiple of 4 */
  if (*v & 3)
    return -1;

  return 0;
}

/* Given a GDB frame, determine the address of the calling function's
   frame.  This will be used to create a new GDB frame struct.  */
static void
lvx_frame_this_id (frame_info_ptr this_frame, void **this_cache,
		   struct frame_id *this_id)
{
  struct lvx_frame_cache *cache = lvx_get_frame_cache (this_frame, this_cache);
  CORE_ADDR base, ra;

  if (cache->is_invalid)
    return;

  /* mark the frame cache as invalid if the required registers cannot be
     obtained  */
  if (lvx_get_frame_cache_reg (this_frame, &cache->base, &base)
      || lvx_get_frame_cache_reg (this_frame, &cache->ra, &ra))
    {
      cache->is_invalid = 1;
      return;
    }

  *this_id = frame_id_build (base, cache->frame_pc);
}

/* Get the value of register regnum in the previous stack frame.  */
static struct value *
lvx_frame_prev_register (frame_info_ptr this_frame, void **this_cache,
			 int regnum)
{
  CORE_ADDR value;
  struct lvx_frame_cache *cache = lvx_get_frame_cache (this_frame, this_cache);
  struct gdbarch *gdbarch = get_frame_arch (this_frame);
  int pc_regnum = gdbarch_pc_regnum (gdbarch);
  int r0_regnum = user_reg_map_name_to_regnum (gdbarch, "r0", -1);
  int sp_regnum = r0_regnum + LVX_GPR_REG_SP;

  if (cache->is_invalid)
    return frame_unwind_got_optimized (this_frame, regnum);

  if (regnum == pc_regnum)
    {
      if (!lvx_get_frame_cache_reg (this_frame, &cache->ra, &value))
	return frame_unwind_got_constant (this_frame, regnum, value);
    }

  if (cache->frame_pc == cache->function_pc)
    return frame_unwind_got_register (this_frame, regnum, regnum);

  if (regnum == sp_regnum)
    {
      if (!lvx_get_frame_cache_reg (this_frame, &cache->base, &value))
	return frame_unwind_got_constant (this_frame, regnum, value);
    }

  if (regnum == r0_regnum + LVX_GPR_REG_FP)
    {
      if (!lvx_get_frame_cache_reg (this_frame, &cache->fp, &value))
	return frame_unwind_got_constant (this_frame, regnum, value);
    }

  return frame_unwind_got_optimized (this_frame, regnum);
}

const struct frame_unwind lvx_frame_unwind = {"lvx prologue",
					      NORMAL_FRAME,
					      default_frame_unwind_stop_reason,
					      lvx_frame_this_id,
					      lvx_frame_prev_register,
					      NULL,
					      default_frame_sniffer};

void
lvx_dwarf2_frame_init_reg (struct gdbarch *gdbarch, int regnum,
			   struct dwarf2_frame_state_reg *reg,
			   frame_info_ptr this_frame)
{
  if (regnum == gdbarch_pc_regnum (gdbarch))
    reg->how = DWARF2_FRAME_REG_RA;
  else if (regnum == gdbarch_sp_regnum (gdbarch))
    reg->how = DWARF2_FRAME_REG_CFA;
}

int
lvx_print_insn (bfd_vma pc, disassemble_info *di)
{
  int res;
  gdb_byte buf[4];

  res = print_insn_lvx (pc, di);

  target_read_memory (pc + res - 4, buf, 4);
  /* Check if the last syllab has the parallel bit set. If so mark
     the current instruction as having a delay slot.  This forces
     x/i to display the following instructions until the end of the
     bundle.  This is of course a lie, but it manages what we want
     to achieve: print the full bundle when invoking 'x/i $pc'.  */
  if (buf[3] & 0x80)
    {
      di->insn_info_valid = 1;
      di->branch_delay_insns = 1;
    }

  return res;
}

static CORE_ADDR
lvx_frame_align (struct gdbarch *gdbarch, CORE_ADDR addr)
{
  return addr & LVX_STACK_ALIGN_MASK;
}

struct frame_id
lvx_dummy_id (struct gdbarch *gdbarch, frame_info_ptr this_frame)
{
  CORE_ADDR sp;
  lvx_gdbarch_tdep *tdep = gdbarch_tdep<lvx_gdbarch_tdep> (gdbarch);

  // force read all registers to avoid writing all registers when returning the
  // dummy call
  try
    {
      get_frame_register_unsigned (this_frame, tdep->r0_regnum + 20);
    }
  catch (...)
    {
    }

  int sp_regnum = gdbarch_sp_regnum (gdbarch);
  CORE_ADDR pc = get_frame_pc (this_frame);
  sp = get_frame_register_unsigned (this_frame, sp_regnum);
  return frame_id_build (sp, pc);
}

CORE_ADDR
lvx_push_dummy_call (struct gdbarch *gdbarch, struct value *function,
		     struct regcache *regcache, CORE_ADDR bp_addr, int nargs,
		     struct value **args, CORE_ADDR sp,
		     function_call_return_method return_method,
		     CORE_ADDR struct_addr)
{
  lvx_gdbarch_tdep *tdep = gdbarch_tdep<lvx_gdbarch_tdep> (gdbarch);
  gdb_byte *argslotsbuf = NULL;
  unsigned int argslotsnb = 0;
  int i, n, r0_regnum, gpr_reg_sz, by_ref;
  const int params_regs = 12;
  const int by_ref_arg_min_nb_regs = 5;

  r0_regnum = user_reg_map_name_to_regnum (gdbarch, "r0", -1);
  gpr_reg_sz = register_size (gdbarch, r0_regnum);

  sp = lvx_frame_align (gdbarch, sp);

  /* Allocate arguments to the virtual argument slots  */
  for (i = 0; i < nargs; i++)
    {
      int newslots, typelen = value_enclosing_type (args[i])->length ();

      newslots = (typelen + gpr_reg_sz - 1) / gpr_reg_sz;
      by_ref = 0;
      if (newslots >= by_ref_arg_min_nb_regs)
	{
	  n = (newslots * gpr_reg_sz + LVX_STACK_ALIGN_BYTES - 1)
	      & LVX_STACK_ALIGN_MASK;
	  if (sp < n)
	    error (
	      _ ("Not enough place in stack to store argument %d (size %d)"), i,
	      typelen);
	  sp -= n;
	  write_memory (sp, value_contents (args[i]).data (), typelen);
	  newslots = 1;
	  by_ref = 1;
	}

      argslotsbuf = (gdb_byte *) xrealloc (argslotsbuf, (argslotsnb + newslots)
							  * gpr_reg_sz);
      memset (&argslotsbuf[argslotsnb * gpr_reg_sz], 0, newslots * gpr_reg_sz);
      if (by_ref)
	memcpy (&argslotsbuf[argslotsnb * gpr_reg_sz], &sp, sizeof (sp));
      else
	memcpy (&argslotsbuf[argslotsnb * gpr_reg_sz],
		value_contents (args[i]).data (), typelen);

      argslotsnb += newslots;
    }

  n = argslotsnb - params_regs;
  if (n > 0)
    {
      if (sp < n * gpr_reg_sz)
	error (_ ("Not enough place in stack to store arguments not passed "
		  "in registers"));

      sp -= n * gpr_reg_sz;
      write_memory (sp, argslotsbuf + params_regs * gpr_reg_sz, n * gpr_reg_sz);
    }

  n = argslotsnb > params_regs ? params_regs : argslotsnb;
  for (i = 0; i < n; i++)
    regcache->cooked_write (i + r0_regnum, &argslotsbuf[i * gpr_reg_sz]);

  if (return_method == return_method_hidden_param
      || return_method == return_method_struct)
    regcache_cooked_write_unsigned (regcache, r0_regnum + REGNUM_STRUCT,
				    struct_addr);

  regcache_cooked_write_unsigned (regcache, gdbarch_sp_regnum (gdbarch), sp);
  regcache_cooked_write_unsigned (regcache, tdep->ra_regnum, bp_addr);

  return sp;
}

static void
lvx_store_return_value (struct gdbarch *gdbarch, struct type *type,
			struct regcache *regcache, const gdb_byte *buf)
{
  int i, len = type->length ();
  int r0_regnum = user_reg_map_name_to_regnum (gdbarch, "r0", -1);
  int sz = register_size (gdbarch, r0_regnum);

  for (i = 0; len >= sz; i++, len -= sz)
    regcache->raw_write (i + r0_regnum, buf + i * sz);

  if (len > 0)
    {
      gdb_byte tmp[sz];

      memset (tmp, 0, sz);
      memcpy (tmp, buf + i * sz, len);
      regcache->raw_write (i + r0_regnum, tmp);
    }
}

static void
lvx_extract_return_value (struct gdbarch *gdbarch, struct type *type,
			  struct regcache *regcache, gdb_byte *buf)
{
  int i, len = type->length ();
  int r0_regnum = user_reg_map_name_to_regnum (gdbarch, "r0", -1);
  int sz = register_size (gdbarch, r0_regnum);

  for (i = 0; len >= sz; i++, len -= sz)
    regcache->raw_read (i + r0_regnum, buf + i * sz);

  if (len > 0)
    {
      gdb_byte tmp[sz];

      regcache->raw_read (i + r0_regnum, tmp);
      memcpy (buf + i * sz, tmp, len);
    }
}

enum return_value_convention
lvx_return_value (struct gdbarch *gdbarch, struct value *func_type,
		  struct type *type, struct regcache *regcache,
		  gdb_byte *readbuf, const gdb_byte *writebuf)
{
  if (type->length () > 32)
    {
      if (readbuf && regcache)
	{
	  int r0_regnum = user_reg_map_name_to_regnum (gdbarch, "r0", -1);
	  CORE_ADDR addr = 0;

	  regcache->raw_read (r0_regnum + REGNUM_STRUCT, &addr);
	  read_memory (addr, readbuf, type->length ());
	}

      return RETURN_VALUE_ABI_RETURNS_ADDRESS;
    }

  if (writebuf)
    lvx_store_return_value (gdbarch, type, regcache, writebuf);
  else if (readbuf)
    lvx_extract_return_value (gdbarch, type, regcache, readbuf);
  return RETURN_VALUE_REGISTER_CONVENTION;
}

int
lvx_get_longjmp_target (frame_info_ptr frame, CORE_ADDR *pc)
{
  /* R0 point to the jmpbuf, and RA is at offset 0xA0 in the buf */
  gdb_byte buf[sizeof (uint64_t)];
  CORE_ADDR r0;
  struct gdbarch *gdbarch = get_frame_arch (frame);
  enum bfd_endian byte_order = gdbarch_byte_order (gdbarch);

  get_frame_register (
    frame, user_reg_map_name_to_regnum (get_frame_arch (frame), "r0", -1), buf);
  r0 = extract_unsigned_integer (buf, sizeof (buf), byte_order);
  // 0xA0 = offset of RA in the jmp_buf struct
  if (target_read_memory (r0 + 0xA0, buf, sizeof (buf)))
    return 0;

  *pc = extract_unsigned_integer (buf, sizeof (buf), byte_order);
  return 1;
}

static void
add_op (struct op_list **list, struct lvx_opc *op)
{
  struct op_list *op_l = (struct op_list *) malloc (sizeof (struct op_list));

  op_l->op = op;
  op_l->next = *list;

  *list = op_l;
}

void
set_parse_compact_asm (bool v)
{
  parse_lvx_dis_option (v ? "compact-assembly" : "no-compact-assembly");
}

void
lvx_look_for_insns (void)
{
  int i;

  for (i = 0; i < LVX_NUM_ARCHES; ++i)
    {
      struct lvx_opc *op;

      switch (i)
	{
	case LVX_1:
	  op = lvx_v1_optab;
	  break;
	case LVX_2:
	  op = lvx_v2_optab;
	  break;
	default:
	  internal_error ("Unknown arch id.");
	}

      while (op->as_op[0])
	{
	  if (strcmp ("call", op->as_op) == 0)
	    add_op (&branch_insns[i], op);
	  else if (strcmp ("icall", op->as_op) == 0)
	    add_op (&branch_insns[i], op);
	  else if (strcmp ("goto", op->as_op) == 0)
	    add_op (&branch_insns[i], op);
	  else if (strcmp ("igoto", op->as_op) == 0)
	    add_op (&branch_insns[i], op);
	  else if (strcmp ("ret", op->as_op) == 0)
	    add_op (&branch_insns[i], op);
	  else if (strcmp ("rfe", op->as_op) == 0)
	    add_op (&branch_insns[i], op);
	  else if (strcmp ("pcrel", op->as_op) == 0)
	    add_op (&pcrel_insn[i], op);
	  else if (strcmp ("scall", op->as_op) == 0)
	    {
	      if (op->format && op->format[0] && op->format[0]->regs == NULL)
		{
		  break_jtag_over_iss[i]
		    = (op->codewords[0].opcode & op->codewords[0].mask)
		      | ((__NR_BREAKPOINT_JTAGISS
			  & ((1 << op->format[0]->bfield->size) - 1))
			 << op->format[0]->bfield->to_offset);
		  break_op[i]
		    = (op->codewords[0].opcode & op->codewords[0].mask)
		      | ((__NR_BREAKPOINT_PL0
			  & ((1 << op->format[0]->bfield->size) - 1))
			 << op->format[0]->bfield->to_offset);
		}
	      add_op (&branch_insns[i], op);
	    }
	  else if (strcmp ("cb", op->as_op) == 0)
	    add_op (&branch_insns[i], op);
	  else if (strcmp ("loopdo", op->as_op) == 0)
	    add_op (&branch_insns[i], op);
	  else if (strcmp ("loopgtz", op->as_op) == 0)
	    add_op (&branch_insns[i], op);
	  else if (strcmp ("loopnez", op->as_op) == 0)
	    add_op (&branch_insns[i], op);
	  else if (strcmp ("get", op->as_op) == 0)
	    add_op (&branch_insns[i], op);
	  else if (strcmp ("iget", op->as_op) == 0)
	    add_op (&branch_insns[i], op);
	  else if (strcmp ("nop", op->as_op) == 0)
	    nop_op[i] = op->codewords[0].opcode;

	  ++op;
	}
    }
}
