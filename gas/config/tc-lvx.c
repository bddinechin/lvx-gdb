/* tc-lvx.c -- Assemble for the LVX ISA

   Copyright (C) 2009-2024 Free Software Foundation, Inc.
   Contributed by Kalray SA.

   This file is part of GAS.

   GAS is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the license, or
   (at your option) any later version.

   GAS is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING3. If not,
   see <http://www.gnu.org/licenses/>.  */

#include "as.h"
#include "obstack.h"
#include "subsegs.h"
#include "tc-lvx.h"
#include "libiberty.h"

#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>

#ifdef OBJ_ELF
#include "elf/lvx.h"
#include "dwarf2dbg.h"
#include "dw2gencfi.h"
#endif

static void supported_cores (char buf[], size_t buflen);

#define NELEMS(a) ((int) (sizeof (a)/sizeof ((a)[0])))

#define STREQ(x,y) !strcmp(((x) ? (x) : ""), ((y) ? (y) : ""))
#define STRNEQ(x,y,n) !strncmp(((x) ? (x) : ""), ((y) ? (y) : ""),(n))

/* The LVX_PARALLEL_BIT is set to 0 when a syllable is the last in a bundle.  */
#define LVX_PARALLEL_BIT (1u << 31)

/* Enumeration of LVX execution units */
enum LVX_EXU
  { LVX_EXU_BCU0, LVX_EXU_BCU1, LVX_EXU_ALU0, LVX_EXU_ALU1, LVX_EXU_LSU0, LVX_EXU_LSU1,
    LVX_EXU_EXT0, LVX_EXU_EXT1, LVX_EXU_EXT2, LVX_EXU_EXT3, LVX_EXU__ };


int size_type_function = 1;

struct lvx_as_env env = {
  .params = {
    .abi = ELF_LVX_ABI_UNDEF,
    .osabi = ELFOSABI_NONE,
    .core = -1,
    .core_set = 0,
    .abi_set = 0,
    .osabi_set = 0,
    .pic_flags = 0,
  },
  .opts = {
    .march = NULL,
    .check_resource_usage = 1,
    .dump_table = 0,
    .dump_insn = 0,
    .diagnostics = 1,
    .more = 1,
    .allow_all_sfr = 0
  },
  .stcall_info = {
    .found = false,
    .frag = NULL,
    .seg = 0,
    .frag_fix = 0
  },
};

/* Default lvx_registers array.  */
const struct lvx_register *lvx_registers = NULL;
/* Default lvx_modifiers array.  */
const char ***lvx_modifiers = NULL;
/* Default lvx_regfiles array.  */
/* Default lvx_regfiles array and its size.  */
const int *lvx_regfiles = NULL;
int lvx_regfiles_size = 0;
/* Default values used if no assume directive is given.  */
const struct lvx_core_info *lvx_core_info = NULL;

/***********************************************/
/*    Generic Globals for GAS                  */
/***********************************************/

const char comment_chars[]        = "#";
const char line_comment_chars[]   = "#";
const char line_separator_chars[] = ";";
const char EXP_CHARS[]            = "eE";
const char FLT_CHARS[]            = "dD";
const int md_short_jump_size      = 0;
const int md_long_jump_size       = 0;

/***********************************************/
/*           Local Types                       */
/***********************************************/

/* A fix up record.  */
struct lvx_fixup
{
  /* The expression used.  */
  expressionS exp;
  /* The place in the frag where this goes.  */
  int where;
  /* The relocation.  */
  bfd_reloc_code_real_type reloc;
};

/* A single assembled instruction, also used for IMMX syllables.  */
struct lvx_insn
{
  /* Opcode table entry for this insn.  */
  const struct lvx_opc *opdef;
  /* Raw instruction words (syllables).  */
  uint32_t words[LVX_MAXSYLLABLES];	
  /* Execution unit where issued.  */
  int8_t lvx_exu;
  /* Position in reordered bubdle.  */
  int8_t insn_pos;
  /* Index of first immx in lvx_immx_buffer.  */
  int8_t immx0_idx;
  /* Index of second immx in lvx_immx_buffer.  */
  int8_t immx1_idx;
  /* Base Bundling class used to sort instructions in bundle.  */
  uint8_t bundling;
  /* Assembler source order to sort instructions in bundle.  */
  uint8_t order;
  /* Number of instruction words (syllables).  */
  int8_t length;
  /* The number of fixups [0,2].  */
  int8_t nfixups;
  /* The actual fixups.  */
  struct lvx_fixup fixups[2];
};

/* Instruction comparison function for qsort.  */
static int
lvx_insn_compare (const void *a, const void *b)
{
  struct lvx_insn *insn_a = *(struct lvx_insn **) a;
  struct lvx_insn *insn_b = *(struct lvx_insn **) b;
  int bundling_a = insn_a->bundling;
  int bundling_b = insn_b->bundling;
  int order_a = insn_a->order;
  int order_b = insn_b->order;
  /* Lower bundling first.  */
  if (bundling_a != bundling_b)
    return bundling_a - bundling_b;
  /* Lower order first.  */
  return order_a - order_b;
}

static int (*lvx_base_bundling) (int bundling) = NULL;
static void (*lvx_reorder_bundle) (struct lvx_insn *insns[], struct lvx_insn *issued_insns[]) = NULL;
static void (*lvx_dump_opc) (struct lvx_opc *opc) = NULL;
static bool lvx_insn_pcrel;

static struct lvx_insn lvx_insn_buffer[LVX_MAXBUNDLEISSUE + 1];
static int lvx_insn_cnt = 0;
static struct lvx_insn lvx_immx_buffer[LVX_MAXOPERANDS];
static int lvx_immx_cnt = 0;

static inline void
lvx_insn_add_fixup (struct lvx_insn *insn, bfd_reloc_code_real_type reloc,
		    expressionS exp)
{
  int nfixups = insn->nfixups;
  if (nfixups >= NELEMS (insn->fixups))
    as_fatal ("[lvx_insn_add_fixup] insn with more than two fixups");

  insn->fixups[nfixups].reloc = reloc;
  insn->fixups[nfixups].exp = exp;
  insn->fixups[nfixups].where = 0;
  insn->nfixups++;
}

static inline struct lvx_insn *
lvx_insn_add_immx (struct lvx_insn *insn)
{
  int32_t word = 0;
  if (insn->immx0_idx < 0)
    {
      insn->immx0_idx = lvx_immx_cnt;
      word = insn->words[1];
    }
  else if (insn->immx1_idx < 0)
    {
      insn->immx1_idx = lvx_immx_cnt;
      word = insn->words[2];
    }
  else
    as_fatal ("[lvx_insn_add_immx] insn with more than two IMMX syllables");

  struct lvx_insn *insn_immx = lvx_immx_buffer + lvx_immx_cnt++;
  if (lvx_immx_cnt >= NELEMS (lvx_immx_buffer))
    as_fatal ("[lvx_insn_add_immx] max number of IMMX exceeded: %d", lvx_immx_cnt);

  insn_immx->words[0] = word;
  insn_immx->length = 1;
  insn->length--;

  return insn_immx;
}

static void set_byte_counter (asection * sec, int value);
static void
set_byte_counter (asection * sec, int value)
{
  sec->target_index = value;
}

static int
get_byte_counter (asection * sec)
{
  return sec->target_index;
}

const char *
lvx_target_format (void)
{
  return "elf64-lvx";
}

/****************************************************/
/*  ASSEMBLER Pseudo-ops.  Some of this just        */
/*  extends the default definitions                 */
/*  others are LVX specific                          */
/****************************************************/

static void lvx_check_resources (int);
static void lvx_proc (int start);
static void lvx_endp (int start);
static void lvx_type (int start);

static void
lvx_cfi_offset (int start)
{
  (void) start;
  offsetT offset;
  int reg1;


  if (frchain_now->frch_cfi_data == NULL)
    {
      as_bad (_("CFI instruction used without previous .cfi_startproc"));
      ignore_rest_of_line ();
      return;
    }

  if (env.stcall_info.found)
    {
      /* If the last address was not at the current PC, advance to current.  */
      if (symbol_get_frag (frchain_now->frch_cfi_data->last_address)
	    != env.stcall_info.frag
	  || (S_GET_VALUE (frchain_now->frch_cfi_data->last_address)
	    != env.stcall_info.frag_fix))
      cfi_add_advance_loc (symbol_temp_new (env.stcall_info.seg,
					    env.stcall_info.frag,
					    env.stcall_info.frag_fix));
    }
  else
    {
      /* If the last address was not at the current PC, advance to current.  */
      if (symbol_get_frag (frchain_now->frch_cfi_data->last_address) != frag_now
	  || (S_GET_VALUE (frchain_now->frch_cfi_data->last_address)
	    != frag_now_fix ()))
	cfi_add_advance_loc (symbol_temp_new_now ());
    }

  expressionS exp = { 0 };
  SKIP_WHITESPACE ();
  expression (&exp);

  switch (exp.X_op)
    {
    case O_register:
    case O_constant:
      reg1 = exp.X_add_number;
      break;

    default:
      reg1 = -1;
      break;
    }

  if (reg1 < 0)
    {
      as_bad (_("bad register expression"));
      reg1 = 0;
    }

  SKIP_WHITESPACE ();
  if (*input_line_pointer == ',')
    input_line_pointer++;
  else
    as_bad (_("missing separator"));

  offset = get_absolute_expression ();
  cfi_add_CFA_offset (reg1, offset);

  demand_empty_rest_of_line ();
}

const pseudo_typeS md_pseudo_table[] = {
  /* Override default 2-bytes.  */
  { "word",             cons,                  4 },

  /* LVX specific pseudo.  */
  { "dword",            cons,                  8 },

  /* Override align directives to have a boundary as argument
     (and not the power of two as in p2align).  */
  { "align",            s_align_bytes,         0 },

  { "checkresources",   lvx_check_resources,   1 },
  { "nocheckresources", lvx_check_resources,   0 },

  { "proc",             lvx_proc,              1 },
  { "endp",             lvx_endp,              0 },

  { "type",             lvx_type,              0 },

#ifdef OBJ_ELF
  { "file",             dwarf2_directive_file, 0 },
  { "loc",              dwarf2_directive_loc,  0 },
  { "cfi_offset",       lvx_cfi_offset,        0 },
#endif
  { NULL,               0,                     0 }
};


static int inside_bundle = 0;

/* Stores the labels inside bundles (typically debug labels) that need
   to be postponed to the next bundle.  */
struct label_fix
{
  struct label_fix *next;
  symbolS *sym;
} *label_fixes = 0;

/*****************************************************/
/*   OPTIONS PROCESSING                              */
/*****************************************************/

const char *md_shortopts = "hV";	/* Catted to std short options.  */

/* Added to std long options.  */

#define OPTION_MARCH                 (OPTION_MD_BASE + 0)
#define OPTION_CHECK_RESOURCES       (OPTION_MD_BASE + 1)
#define OPTION_NO_CHECK_RESOURCES    (OPTION_MD_BASE + 2)
#define OPTION_DUMP_TABLE            (OPTION_MD_BASE + 3)
#define OPTION_PIC                   (OPTION_MD_BASE + 4)
#define OPTION_BIGPIC                (OPTION_MD_BASE + 5)
#define OPTION_NOPIC                 (OPTION_MD_BASE + 6)
#define OPTION_DUMPINSN              (OPTION_MD_BASE + 8)
#define OPTION_ALL_SFR               (OPTION_MD_BASE + 9)
#define OPTION_DIAGNOSTICS           (OPTION_MD_BASE + 10)
#define OPTION_NO_DIAGNOSTICS        (OPTION_MD_BASE + 11)
#define OPTION_MORE                  (OPTION_MD_BASE + 12)
#define OPTION_NO_MORE               (OPTION_MD_BASE + 13)

struct option md_longopts[] = {
  { "march",                 required_argument, NULL, OPTION_MARCH                 },
  { "check-resources",       no_argument,       NULL, OPTION_CHECK_RESOURCES       },
  { "no-check-resources",    no_argument,       NULL, OPTION_NO_CHECK_RESOURCES    },
  { "dump-table",            no_argument,       NULL, OPTION_DUMP_TABLE            },
  { "mpic",                  no_argument,       NULL, OPTION_PIC                   },
  { "mPIC",                  no_argument,       NULL, OPTION_BIGPIC                },
  { "mnopic",                no_argument,       NULL, OPTION_NOPIC                 },
  { "dump-insn",             no_argument,       NULL, OPTION_DUMPINSN              },
  { "all-sfr",               no_argument,       NULL, OPTION_ALL_SFR               },
  { "diagnostics",           no_argument,       NULL, OPTION_DIAGNOSTICS           },
  { "no-diagnostics",        no_argument,       NULL, OPTION_NO_DIAGNOSTICS        },
  { "more",                  no_argument,       NULL, OPTION_MORE                  },
  { "no-more",               no_argument,       NULL, OPTION_NO_MORE               },
  { NULL,                    no_argument,       NULL, 0                            }
};

size_t md_longopts_size = sizeof (md_longopts);

int
md_parse_option (int c, const char *arg ATTRIBUTE_UNUSED)
{
  int find_core = 0;

  switch (c)
    {
    case 'h':
      md_show_usage (stdout);
      exit (EXIT_SUCCESS);
      break;

      /* -V: SVR4 argument to print version ID.  */
    case 'V':
      print_version_id ();
      exit (EXIT_SUCCESS);
      break;
    case OPTION_MARCH:
      env.opts.march = strdup (arg);
      for (int i = 0; i < LVX_NUMCORES && !find_core; ++i)
	    if (!strcasecmp (env.opts.march, lvx_core_info_table[i]->name))
	      {
		lvx_core_info = lvx_core_info_table[i];
		lvx_registers = lvx_registers_table[i];
		lvx_modifiers = lvx_modifiers_table[i];
		lvx_regfiles = lvx_regfiles_table[i];
		lvx_regfiles_size = lvx_regfiles_size_table[i];

		find_core = 1;
		break;
	      }
      if (!find_core)
	{
	  char buf[100];
	  supported_cores (buf, sizeof (buf));
	  as_fatal ("[md_parse_option] "
		    "specified arch is not supported [%s]", buf);
	}
      break;
    case OPTION_CHECK_RESOURCES:
      env.opts.check_resource_usage = 1;
      break;
    case OPTION_NO_CHECK_RESOURCES:
      env.opts.check_resource_usage = 0;
      break;
    case OPTION_DUMP_TABLE:
      env.opts.dump_table = 1;
      break;
    case OPTION_DUMPINSN:
      env.opts.dump_insn = 1;
      break;
    case OPTION_ALL_SFR:
      env.opts.allow_all_sfr = 1;
      break;
    case OPTION_DIAGNOSTICS:
      env.opts.diagnostics = 1;
      break;
    case OPTION_NO_DIAGNOSTICS:
      env.opts.diagnostics = 0;
      break;
    case OPTION_MORE:
      env.opts.more = 1;
      break;
    case OPTION_NO_MORE:
      env.opts.more = 0;
      break;
    case OPTION_PIC:
    case OPTION_BIGPIC:
      env.params.pic_flags |= ELF_LVX_ABI_PIC_BIT;
      break;
    case OPTION_NOPIC:
      env.params.pic_flags &= ~(ELF_LVX_ABI_PIC_BIT);
      break;
    default:
      return 0;
    }
  return 1;
}

void
md_show_usage (FILE * stream)
{
  char buf[100];
  supported_cores (buf, sizeof (buf));

  fprintf (stream, "\n"
"LVX specific options:\n\n"
"  --check-resources\t Perform minimal resource checking\n"
"  --march [%s]\t Select architecture\n"
"  -V \t\t\t Print assembler version number\n\n"
"  The options -M, --mri and -f are not supported in this assembler.\n", buf);
}

/**************************************************/
/*              UTILITIES                         */
/**************************************************/

/*
 * Read a value from to the object file
 */

static valueT md_chars_to_number (char *buf, int n);
valueT
md_chars_to_number (char *buf, int n)
{
  valueT val = 0;

  if (n > (int) sizeof (val) || n <= 0)
    abort ();

  while (n--)
    {
      val <<= 8;
      val |= (buf[n] & 0xff);
    }

  return val;
}

/* Returns the corresponding pseudo function matching SYM and to be
   used for data section.  */
static struct pseudo_func *
lvx_get_pseudo_func_data_scn (symbolS * sym)
{
  for (int i = 0; i < lvx_core_info->nb_pseudo_funcs; i++)
    if (sym == lvx_core_info->pseudo_funcs[i].sym
	&& lvx_core_info->pseudo_funcs[i].pseudo_relocs.single != BFD_RELOC_UNUSED)
	return &lvx_core_info->pseudo_funcs[i];
  return NULL;
}

/* Returns the corresponding pseudo function matching SYM and operand
   format OPND.  */
static struct pseudo_func *
lvx_get_pseudo_func2 (symbolS *sym, struct lvx_operand * opnd)
{
  for (int i = 0; i < lvx_core_info->nb_pseudo_funcs; i++)
    if (sym == lvx_core_info->pseudo_funcs[i].sym)
      for (int rel_idx = 0; rel_idx < opnd->reloc_nb; rel_idx++)
	if (opnd->relocs[rel_idx] == lvx_core_info->pseudo_funcs[i].pseudo_relocs.kreloc
	    && lvx_core_info->pseudo_funcs[i].pseudo_relocs.avail_modes != PSEUDO_32_ONLY)
	  return &lvx_core_info->pseudo_funcs[i];

  return NULL;
}

static void
supported_cores (char buf[], size_t buflen)
{
  buf[0] = '\0';
  for (int i = 0; i < LVX_NUMCORES; i++)
    {
      if (buf[0] == '\0')
	strcpy (buf, lvx_core_info_table[i]->name);
      else
	if ((strlen (buf) + 1 + strlen (lvx_core_info_table[i]->name) + 1) < buflen)
	  {
	    strcat (buf, "|");
	    strcat (buf, lvx_core_info_table[i]->name);
	  }
    }
}

/***************************************************/
/*   ASSEMBLE AN INSTRUCTION                       */
/***************************************************/

/*
 * Insert ARG into the operand described by OPDEF in instruction INSN
 * Returns 1 if the immediate extension (IMMX) has been
 * handled along with relocation, 0 if not.
 */
static bool
insert_operand (struct lvx_insn *insn, struct lvx_operand *opnd,
		struct token_list *tok)
{
  uint64_t op = 0;
  struct lvx_bitfield *bfields = opnd->bfield;
  int bf_nb = opnd->bitfields;
  struct lvx_insn *insn_immx = 0;

  if (opnd->width == 0)
    return 0;

  /* Try to resolve the value.  */

  switch (tok->category)
    {
    case CAT_REGISTER:
      op = S_GET_VALUE (str_hash_find (env.reg_hash, tok->tok));
      op -= opnd->bias;
      op >>= opnd->shift;
      break;

    case CAT_MODIFIER:
      op = tok->val;
      op -= opnd->bias;
      op >>= opnd->shift;
      break;

    case CAT_IMMEDIATE:
      {
	char *ilp_save = input_line_pointer;
	input_line_pointer = tok->tok;
	expressionS exp = { 0 };
	expression (&exp);
	input_line_pointer = ilp_save;

	/* We are dealing with a pseudo-function.  */
	if (tok->tok[0] == '@')
	  {
	    if (insn->nfixups == 0)
	      {
		expressionS reloc_arg;
		reloc_arg = exp;
		reloc_arg.X_op = O_symbol;
		struct pseudo_func *pf =
		  lvx_get_pseudo_func2 (exp.X_op_symbol, opnd);
		/* S64 uses LO10/UP27/EX27 format (3 words), with one reloc in each word (3).  */
		/* S43 uses LO10/EX6/UP27 format (2 words), with 2 relocs in main word and 1 in extra word.  */
		/* S37 uses LO10/UP27 format (2 words), with one reloc in each word (2).  */

		/* Beware that LVX_IMMX_BUFFER must be filled in the same order as relocs should be emitted.  */

		if (pf->pseudo_relocs.reloc_type == S64_LO10_UP27_EX27
		    || pf->pseudo_relocs.reloc_type == S43_LO10_UP27_EX6
		    || pf->pseudo_relocs.reloc_type == S37_LO10_UP27)
		  {
		    lvx_insn_add_fixup (insn, pf->pseudo_relocs.reloc_lo10, reloc_arg);
		    insn_immx = lvx_insn_add_immx (insn);
		    lvx_insn_add_fixup (insn_immx, pf->pseudo_relocs.reloc_up27, reloc_arg);
		  }
		else if (pf->pseudo_relocs.reloc_type == S32_LO5_UP27)
		  {
		    lvx_insn_add_fixup (insn, pf->pseudo_relocs.reloc_lo5, reloc_arg);
		    insn_immx = lvx_insn_add_immx (insn);
		    lvx_insn_add_fixup (insn_immx, pf->pseudo_relocs.reloc_up27, reloc_arg);
		  }
		else if (pf->pseudo_relocs.reloc_type == S16)
		  {
		    lvx_insn_add_fixup (insn, pf->pseudo_relocs.single, reloc_arg);
		  }
		else
		  as_fatal ("[insert_operand] unexpected pseudo-function");

		if (pf->pseudo_relocs.reloc_type == S64_LO10_UP27_EX27)
		  {
		    insn_immx = lvx_insn_add_immx (insn);
		    lvx_insn_add_fixup (insn_immx, pf->pseudo_relocs.reloc_ex, reloc_arg);
		  }
		else if (pf->pseudo_relocs.reloc_type == S43_LO10_UP27_EX6)
		  {
		    lvx_insn_add_fixup (insn, pf->pseudo_relocs.reloc_ex, reloc_arg);
		  }
	      }
	  }
	else
	  {
	    if (exp.X_op == O_constant)
	      {
		/* This is a immediate: either a regular immediate, or an
		   immediate that was saved in a variable through `.equ'.  */
		uint64_t sval = (int64_t) tok->val;
		op = opnd->flags & LVX_OPERAND_SIGNED ? sval : tok->val;
		if (op % (1 << opnd->shift))
		  as_fatal ("[insert_operand] immediate of %s should be a multiple of %d.\n",
			   insn->opdef->as_op, 1 << opnd->shift);
		op >>= opnd->shift;
	      }
	    else if (exp.X_op == O_subtract)
	      as_fatal ("[insert_operand] O_subtract not supported.");
	    else
	      {
		/* This is a symbol which needs a relocation.  */
		if (insn->nfixups != 0)
		  as_fatal ("[insert_operand] no room for fixup ");

		if (ELF_LVX_IS_LVX (env.params.core))
		  switch (opnd->type)
		    {
		      case Immediate_lvx_v1_signed10:
			lvx_insn_add_fixup (insn, BFD_RELOC_LVX_S37_LO10, exp);
			break;
		      case Immediate_lvx_v1_signed37:
			lvx_insn_add_fixup (insn, BFD_RELOC_LVX_S37_LO10, exp);
			insn_immx = lvx_insn_add_immx (insn);
			lvx_insn_add_fixup (insn_immx, BFD_RELOC_LVX_S37_UP27, exp);
			break;
		      case Immediate_lvx_v1_signed43:
			lvx_insn_add_fixup (insn, BFD_RELOC_LVX_S43_LO10, exp);
			lvx_insn_add_fixup (insn, BFD_RELOC_LVX_S43_EX6, exp);
			insn_immx = lvx_insn_add_immx (insn);
			lvx_insn_add_fixup (insn_immx, BFD_RELOC_LVX_S43_UP27, exp);
			break;
		      case Immediate_lvx_v1_wrapped32:
			lvx_insn_add_fixup (insn, BFD_RELOC_LVX_S32_LO5, exp);
			insn_immx = lvx_insn_add_immx (insn);
			lvx_insn_add_fixup (insn_immx, BFD_RELOC_LVX_S32_UP27, exp);
			break;
		      case Immediate_lvx_v1_wrapped64:
			lvx_insn_add_fixup (insn, BFD_RELOC_LVX_S64_LO10, exp);
			insn_immx = lvx_insn_add_immx (insn);
			lvx_insn_add_fixup (insn_immx, BFD_RELOC_LVX_S64_UP27, exp);
			insn_immx = lvx_insn_add_immx (insn);
			lvx_insn_add_fixup (insn_immx, BFD_RELOC_LVX_S64_EX27, exp);
			break;
		      case Immediate_lvx_v1_pcrel11s2:
			lvx_insn_add_fixup (insn, BFD_RELOC_LVX_S11S2_PCREL, exp);
			break;
		      case Immediate_lvx_v1_pcrel17s2:
			lvx_insn_add_fixup (insn, BFD_RELOC_LVX_S17S2_PCREL, exp);
			break;
		      case Immediate_lvx_v1_pcrel27s2:
			lvx_insn_add_fixup (insn, BFD_RELOC_LVX_S27S2_PCREL, exp);
			break;
		      case Immediate_lvx_v1_pcrel38s2:
			lvx_insn_add_fixup (insn, BFD_RELOC_LVX_S38S2_PCREL_LO11, exp);
			insn_immx = lvx_insn_add_immx (insn);
			lvx_insn_add_fixup (insn_immx, BFD_RELOC_LVX_S38S2_PCREL_UP27, exp);
			break;
		      case Immediate_lvx_v1_pcrel44s2:
			lvx_insn_add_fixup (insn, BFD_RELOC_LVX_S44S2_PCREL_LO17, exp);
			insn_immx = lvx_insn_add_immx (insn);
			lvx_insn_add_fixup (insn_immx, BFD_RELOC_LVX_S44S2_PCREL_UP27, exp);
			break;
		      case Immediate_lvx_v1_pcrel54s2:
			lvx_insn_add_fixup (insn, BFD_RELOC_LVX_S54S2_PCREL_LO27, exp);
			insn_immx = lvx_insn_add_immx (insn);
			lvx_insn_add_fixup (insn_immx, BFD_RELOC_LVX_S54S2_PCREL_UP27, exp);
			break;
		      default:
		      as_fatal ("[insert_operand] don't know how to generate a fixup record");
		    }

		return insn_immx != 0;
	      }
	  }
      }
      break;

    default:
      break;
    }

  for (int bf_idx = 0; bf_idx < bf_nb; bf_idx++)
    {
      uint64_t value =
	((uint64_t) op >> bfields[bf_idx].from_offset);
      int j = 0;
      int to_offset = bfields[bf_idx].to_offset;
      value &= (1LL << bfields[bf_idx].size) - 1;
      j = to_offset / 32;
      to_offset = to_offset % 32;
      insn->words[j] |= (value << to_offset) & 0xffffffff;
    }

  return insn_immx != 0;
}

/* Emit an instruction from the instruction array into the object
 * file. INSN points to an element of the instruction array. STOPFLAG
 * is true if this is the last instruction in the bundle.
 *
 * Only handles main syllables of bundle. Immediate extensions are
 * handled by insert_operand.
 */
static void
lvx_emit_insn (struct lvx_insn *insn, int fixup_pos, int stopflag)
{
  /* If we are listing, attach frag to previous line.  */
  if (listing)
    listing_prev_line ();

  /* Update text size for lane parity checking.  */
  set_byte_counter (now_seg, (get_byte_counter (now_seg) + (insn->length * 4)));

  /* Allocate space in the fragment.  */
  char *f = frag_more (insn->length * 4);

  if (env.stcall_info.found
      && insn->opdef && *insn->opdef->as_op == 'c')
    {
      env.stcall_info.seg = now_seg;
      env.stcall_info.frag = frag_now;
      env.stcall_info.frag_fix = frag_now_fix ();
    }

  /* Spit out syllables.  */
  for (int i = 0; i < insn->length; i++)
    {
      unsigned syllable = insn->words[i];

      /* Handle bundle parallel bit.  */ ;
      if ((i == insn->length - 1) && stopflag)
	syllable &= ~LVX_PARALLEL_BIT;
      else
	syllable |= LVX_PARALLEL_BIT;

      /* Emit the instruction syllable.  */
      md_number_to_chars (f + (i * 4), syllable, 4);
    }

  /* Generate fixup records.  */
  for (int i = 0; i < insn->nfixups; i++)
    {
      int size, pcrel;
      reloc_howto_type *reloc_howto =
	bfd_reloc_type_lookup (stdoutput, insn->fixups[i].reloc);
      assert (reloc_howto);
      size = bfd_get_reloc_size (reloc_howto);
      pcrel = reloc_howto->pc_relative;

      /* In case the PCREL relocation is not for the first insn in the
	 bundle, we have to offset it.  LVX relies on the insn PC (not
	 the bundle PC as in older architectures).
	 This difference is taken care of when setting the FIXUP_POS.  */
      if (pcrel && fixup_pos > 0)
	insn->fixups[i].exp.X_add_number += fixup_pos * 4;

      fixS *fixup = fix_new_exp (frag_now,
				 f - frag_now->fr_literal +
				 insn->fixups[i].where,
				 size,
				 &(insn->fixups[i].exp),
				 pcrel,
				 insn->fixups[i].reloc);
      /*
       * Set this bit so that large value can still be
       * handled. Without it, assembler will fail in fixup_segment
       * when it checks there is enough bits to store the value. As we
       * usually split our reloc across different words, it may think
       * that 4 bytes are not enough for large value. This simply
       * skips the tests.
       */
      fixup->fx_no_overflow = 1;
    }
}


/* Called for any expression that can not be recognized.  When the
 * function is called, `input_line_pointer' will point to the start of
 * the expression.  */
/* FIXME: Should be done by the parser.  */
void
md_operand (expressionS * e)
{
  size_t len;
  int ch, i;

  switch (*input_line_pointer)
    {
    case '@':
      /* Find what relocation pseudo-function we're dealing with.  */
      ch = *++input_line_pointer;
      for (i = 0; i < lvx_core_info->nb_pseudo_funcs; ++i)
	if (lvx_core_info->pseudo_funcs[i].name && lvx_core_info->pseudo_funcs[i].name[0] == ch)
	  {
	    len = strlen (lvx_core_info->pseudo_funcs[i].name);
	    if (strncmp (lvx_core_info->pseudo_funcs[i].name + 1,
			 input_line_pointer + 1, len - 1) == 0
		&& !is_part_of_name (input_line_pointer[len]))
	      {
		input_line_pointer += len;
		break;
	      }
	  }
      SKIP_WHITESPACE ();
      if (*input_line_pointer != '(')
	{
	  as_bad ("expected '('");
	  goto err;
	}
      /* Skip '('.  */
      ++input_line_pointer;
      if (!lvx_core_info->pseudo_funcs[i].pseudo_relocs.has_no_arg)
	expression (e);
      if (*input_line_pointer++ != ')')
	{
	  as_bad ("missing ')'");
	  goto err;
	}
      if (!lvx_core_info->pseudo_funcs[i].pseudo_relocs.has_no_arg)
	{
	  if (e->X_op != O_symbol)
	    as_fatal ("[md_operand] illegal combination of relocation functions");
	}
      /* Make sure gas doesn't get rid of local symbols that are used
	 in relocs.  */
      e->X_op = O_pseudo_fixup;
      e->X_op_symbol = lvx_core_info->pseudo_funcs[i].sym;
      break;

    default:
      break;
    }
  return;

err:
  ignore_rest_of_line ();
}

/*
 * Return the LVX EXU tag for an IMMX syllable.
 */
static inline int
lvx_exu_tag (uint32_t x)
{
  return  (((x) & 0x18000000) >> 27);
}

/*
 * Given a set of operands and a matching instruction, assemble it into INSN.
 * Possibly update LVX_IMMX_BUFFER and LVX_IMMX_CNT.
 */
static void
assemble_insn (const struct lvx_opc *opdef, struct token_list *tok, struct lvx_insn *insn)
{
  struct lvx_operand **format = (struct lvx_operand **) opdef->format;

  /* Complete INSN initialization, which has been zeroed already.  */
  insn->opdef = opdef;
  insn->immx0_idx = insn->immx1_idx = -1;
  insn->bundling = (*lvx_base_bundling) (insn->opdef->bundling);
  for (int i = 0; i < opdef->wordcount; i++)
    {
      insn->words[i] = opdef->codewords[i].opcode;
      insn->length++;
    }

  bool immx_ready = 0;
  struct token_list *tok_ = tok;
  while (tok_)
    {
      immx_ready |= insert_operand (insn, *format, tok_);
      while ((tok_ = tok_->next) && tok_->category == CAT_SEPARATOR);
      format++;
    }

  /* Handle IMMX if insert_operand did not already take care of it.  */
  if (!immx_ready)
    {
      for (int i = 1; i < opdef->wordcount; i++)
	{
	  if (opdef->codewords[i].flags & LVX_OPCODE_FLAG_IMMX)
	    {
	      lvx_insn_add_immx (insn);
	    }
	}
    }
}

/*
 * Assemble a token list into LVX_INSN_BUFFER[] and update LVX_INSN_CNT.
 * Calls assemble_insn() which may update LVX_IMMX_BUFFER and LVX_IMMX_CNT.
 */
static void
assemble_tokens (struct token_list *tok_list)
{
  assert (tok_list != NULL);
  char *opcode = tok_list->tok;

  struct token_list *toks = tok_list->next;
  while (toks && toks->category == CAT_SEPARATOR)
    toks = toks->next;

  /* Find the lvx_opc requested by the instruction.  */
  struct lvx_opc *opc = NULL;
  struct lvx_opc *try_opc = str_hash_find (env.opcode_hash, opcode);
  for (; !strcmp(opcode, try_opc->as_op); try_opc++)
    {
      if (try_opc->codewords[0].flags & LVX_OPCODE_FLAG_RISCV)
	continue;

      struct token_list *toks_ = toks;
      int is_tiny = (try_opc->bundling == (int)Bundling_lvx_v1_TINY
		     || try_opc->bundling == (int)Bundling_lvx_v1_TINY_X
		     || try_opc->bundling == (int)Bundling_lvx_v1_TINY_Y);
      for (int i = 0 ; toks_ && try_opc->format[i]
	   && (toks_->class_id == try_opc->format[i]->type
	       || (is_tiny
		   && toks_->category == CAT_REGISTER
		   && try_opc->format[i]->reg_nb > 0
		   && try_opc->format[i]->regs != NULL
		   && ({ unsigned _rn = (unsigned)S_GET_VALUE(str_hash_find(env.reg_hash, toks_->tok));
			 _rn < (unsigned)try_opc->format[i]->reg_nb
			 && try_opc->format[i]->regs[_rn]; }))) ;)
	{
	  toks_ = toks_->next;
	  while (toks_ && toks_->category == CAT_SEPARATOR)
	    toks_ = toks_->next;
	  i += 1;
	}

      if (!toks_)
	{
	  opc = try_opc;
	  break;
	}

    }
  assert (opc != NULL);

  /* Ensure there is room in LVX_INSN_BUFFER then assemble a new insn.  */
  if (lvx_insn_cnt >= NELEMS (lvx_insn_buffer))
    as_fatal ("[assemble_tokens] bundle instruction buffer overflow");
  assemble_insn (opc, toks, lvx_insn_buffer + lvx_insn_cnt);
  lvx_insn_cnt++;
}

/*
 * Write in buf at most buf_size.
 * Returns the number of writen characters.
 */
static int ATTRIBUTE_UNUSED
insn_syntax (struct lvx_opc * op, char *buf, int buf_size)
{
  int chars = snprintf (buf, buf_size, "%s ", op->as_op);
  const char *fmtp = op->fmtstring;
  char ch = 0;

  for (int i = 0; op->format[i]; i++)
    {
      int type = op->format[i]->type;
      const char *type_name = TOKEN_NAME (type);
      int offset = 0;

      for (int j = 0 ; type_name[j] ; ++j)
	if (type_name[j] == '_')
	  offset = j + 1;

      /* Print characters in the format string up to the following * % or null.  */
      while ((chars < buf_size) && (ch = *fmtp) && ch != '%')
	{
	  buf[chars++] = ch;
	  fmtp++;
	}

      /* Skip past %s.  */
      if (ch == '%')
	{
	  ch = *fmtp++;
	  fmtp++;
	}

      chars += snprintf (&buf[chars], buf_size - chars, "%s", type_name + offset);
    }

  /* Print trailing characters in the format string, if any.  */
  while ((chars < buf_size) && (ch = *fmtp))
    {
      buf[chars++] = ch;
      fmtp++;
    }

  if (chars < buf_size)
    buf[chars++] = '\0';
  else
    buf[buf_size - 1] = '\0';

  return chars;
}

#define ASM_CHARS_MAX (71)


static void
lvx_v1_dump_opc (struct lvx_opc * op ATTRIBUTE_UNUSED)
{
  char asm_str[ASM_CHARS_MAX];
  int chars = insn_syntax (op, asm_str, ASM_CHARS_MAX);
  const char *insn_type = "UNKNOWN";
  const char *insn_mode = "";

  for (int i = chars - 1; i < ASM_CHARS_MAX - 1; i++)
    asm_str[i] = '-';

  switch ((int) op->bundling)
    {
    case Bundling_lvx_v1_ALL:
      insn_type = "ALL  ";
      break;
    case Bundling_lvx_v1_BCU2_X:
    case Bundling_lvx_v1_BCU2:
    case Bundling_lvx_v1_BCU0:
    case Bundling_lvx_v1_BCU:
      insn_type = "BCU  ";
      break;
    case Bundling_lvx_v1_FULL:
    case Bundling_lvx_v1_FULL_X:
    case Bundling_lvx_v1_FULL_Y:
      insn_type = "FULL ";
      break;
    case Bundling_lvx_v1_LITE:
    case Bundling_lvx_v1_LITE_X:
    case Bundling_lvx_v1_LITE_Y:
      insn_type = "LITE ";
      break;
    case Bundling_lvx_v1_TINY:
    case Bundling_lvx_v1_TINY_X:
    case Bundling_lvx_v1_TINY_Y:
      insn_type = "TINY ";
      break;
    case Bundling_lvx_v1_LSU0:
    case Bundling_lvx_v1_LSU0_X:
    case Bundling_lvx_v1_LSU0_Y:
    case Bundling_lvx_v1_LSU:
    case Bundling_lvx_v1_LSU_X:
    case Bundling_lvx_v1_LSU_Y:
      insn_type = "LSU  ";
      break;
    case Bundling_lvx_v1_EXT:
      insn_type = "EXT  ";
      break;
    case Bundling_lvx_v1_NOP:
      insn_type = "NOP  ";
      break;
    default:
      as_fatal ("[lvx_dump_opc] unhandled bundling class %d", op->bundling);
    }

  if (op->codewords[0].flags & LVX_OPCODE_FLAG_MODE64
      && op->codewords[0].flags & LVX_OPCODE_FLAG_MODE32)
    insn_mode = "32 and 64";
  else if (op->codewords[0].flags & LVX_OPCODE_FLAG_MODE64)
    insn_mode = "64";
  else if (op->codewords[0].flags & LVX_OPCODE_FLAG_MODE32)
    insn_mode = "32";
  else
    as_fatal ("[lvx_dump_opc] unknown instruction mode.");

  printf ("%s | syllables: %d | type: %s | mode: %s bits\n",
	  asm_str, op->wordcount, insn_type, insn_mode);
}


/* Record CCB with IMMX, which will be first in LVX_IMMX_BUFFER.  */
static struct lvx_insn *lvx_bcux_insn;

/* Record the BCU opcode words to commonalize GUARD and BLEND instructions.  */
static unsigned lvx_bcu0_word, lvx_bcu1_word;

/*
 * Fill the ISSUED_INSNS array indexed by EXU with the BUNDLE_INSNS.
 * Reorder the BUNDLE_INSNS array and update the LVX_IMMX_BUFFER entries.
 */
static void
lvx_v1_reorder_bundle (struct lvx_insn *bundle_insns[], struct lvx_insn *issued_insns[])
{
  lvx_bcux_insn = 0;
  lvx_bcu0_word = lvx_bcu1_word = 0;
  unsigned bundle_flags = 0;
  for (int i = 0; i < lvx_insn_cnt; i++)
    {
      struct lvx_insn *insn = bundle_insns[i];
      const struct lvx_opc *opdef = insn->opdef;
      bundle_flags |= opdef->codewords[0].flags;

      int tag = -1, exu = -1;
      switch (opdef->bundling)
	{
	case Bundling_lvx_v1_ALL:
	  if (lvx_insn_cnt > 1)
	    as_fatal ("too many instructions in a singleton bundle");
	  issued_insns[exu = 0] = insn;
	  break;
	case Bundling_lvx_v1_BCU2_X:
	  assert (lvx_immx_cnt > 0);
	  lvx_bcux_insn = insn;
	  // Fall-through.
	case Bundling_lvx_v1_BCU2:
	  if (!issued_insns[LVX_EXU_BCU0] && !issued_insns[LVX_EXU_BCU1])
	    {
	      issued_insns[exu = LVX_EXU_BCU0] = issued_insns[LVX_EXU_BCU1] = insn;
	      lvx_bcu0_word = lvx_bcu1_word = insn->words[0];
	    }
	  else
	    as_fatal ("more than two BCU2 instructions in bundle");
	  break;
	case Bundling_lvx_v1_BCU0:
	  if (!issued_insns[LVX_EXU_BCU0])
	    {
	      issued_insns[exu = LVX_EXU_BCU0] = insn;
	      lvx_bcu0_word = insn->words[0];
	    }
	  else
	    as_fatal ("more than one BCU0 instruction in bundle");
	  break;
	case Bundling_lvx_v1_BCU:
	  if (!issued_insns[LVX_EXU_BCU0])
	    {
	      issued_insns[exu = LVX_EXU_BCU0] = insn;
	      lvx_bcu0_word = insn->words[0];
	    }
	  else if (!issued_insns[LVX_EXU_BCU1])
	    {
	      issued_insns[exu = LVX_EXU_BCU1] = insn;
	      lvx_bcu1_word = insn->words[0];
	    }
	  else
	    as_fatal ("more than two BCU instructions in bundle");
	  break;
	case Bundling_lvx_v1_FULL:
	case Bundling_lvx_v1_FULL_X:
	case Bundling_lvx_v1_FULL_Y:
	  if (!issued_insns[LVX_EXU_ALU0])
	    {
	      issued_insns[exu = LVX_EXU_ALU0] = insn;
	      tag = Modifier_lvx_v1_exunum_ALU0;
	    }
	  else
	    as_fatal ("more than one ALU FULL instruction in bundle");
	  break;
	case Bundling_lvx_v1_LITE:
	case Bundling_lvx_v1_LITE_X:
	case Bundling_lvx_v1_LITE_Y:
	  if (!issued_insns[LVX_EXU_ALU0])
	    {
	      issued_insns[exu = LVX_EXU_ALU0] = insn;
	      tag = Modifier_lvx_v1_exunum_ALU0;
	    }
	  else if (!issued_insns[LVX_EXU_ALU1])
	    {
	      issued_insns[exu = LVX_EXU_ALU1] = insn;
	      tag = Modifier_lvx_v1_exunum_ALU1;
	    }
	  else
	    as_fatal ("too many ALU FULL or LITE instructions in bundle");
	  break;
	case Bundling_lvx_v1_LSU0:
	case Bundling_lvx_v1_LSU0_X:
	case Bundling_lvx_v1_LSU0_Y:
	  if (!issued_insns[LVX_EXU_LSU0])
	    {
	      issued_insns[exu = LVX_EXU_LSU0] = insn;
	      tag = Modifier_lvx_v1_exunum_LSU0;
	    }
	  else
	    as_fatal ("more than one LSU0 instruction in bundle");
	  break;
	case Bundling_lvx_v1_LSU:
	case Bundling_lvx_v1_LSU_X:
	case Bundling_lvx_v1_LSU_Y:
	  if (!issued_insns[LVX_EXU_LSU0])
	    {
	      issued_insns[exu = LVX_EXU_LSU0] = insn;
	      tag = Modifier_lvx_v1_exunum_LSU0;
	    }
	  else if (!issued_insns[LVX_EXU_LSU1])
	    {
	      issued_insns[exu = LVX_EXU_LSU1] = insn;
	      tag = Modifier_lvx_v1_exunum_LSU1;
	    }
	  else
	    as_fatal ("more than two LSU instructions in bundle");
	  break;
	case Bundling_lvx_v1_TINY:
	case Bundling_lvx_v1_TINY_X:
	case Bundling_lvx_v1_TINY_Y:
	case Bundling_lvx_v1_NOP:
	  if (!issued_insns[LVX_EXU_ALU0])
	    {
	      issued_insns[exu = LVX_EXU_ALU0] = insn;
	      tag = Modifier_lvx_v1_exunum_ALU0;
	    }
	  else if (!issued_insns[LVX_EXU_ALU1])
	    {
	      issued_insns[exu = LVX_EXU_ALU1] = insn;
	      tag = Modifier_lvx_v1_exunum_ALU1;
	    }
	  else if (!issued_insns[LVX_EXU_LSU0])
	    {
	      issued_insns[exu = LVX_EXU_LSU0] = insn;
	      tag = Modifier_lvx_v1_exunum_LSU0;
	    }
	  else if (!issued_insns[LVX_EXU_LSU1])
	    {
	      issued_insns[exu = LVX_EXU_LSU1] = insn;
	      tag = Modifier_lvx_v1_exunum_LSU1;
	    }
	  else
	    as_fatal ("too many ALU instructions in bundle");
	  break;
	case Bundling_lvx_v1_EXT:
	  if (!issued_insns[LVX_EXU_EXT0])
	    issued_insns[exu = LVX_EXU_EXT0] = insn;
	  else if (!issued_insns[LVX_EXU_EXT1])
	    issued_insns[exu = LVX_EXU_EXT1] = insn;
	  else if (!issued_insns[LVX_EXU_EXT2])
	    issued_insns[exu = LVX_EXU_EXT2] = insn;
	  else if (!issued_insns[LVX_EXU_EXT3])
	    issued_insns[exu = LVX_EXU_EXT3] = insn;
	  else
	    as_fatal ("more than four EXT instructions in bundle");
	  break;
	default:
	  as_fatal ("unhandled Bundling class %d", opdef->bundling);
	}
      insn->lvx_exu = exu;
      assert (exu >= 0);

      if (tag >= 0)
	{
	  if (issued_insns[exu]->immx0_idx >= 0)
	    {
	      int immx_idx = issued_insns[exu]->immx0_idx;
	      lvx_immx_buffer[immx_idx].words[0] |= (tag << 27);
	      lvx_immx_buffer[immx_idx].lvx_exu = exu;
	    }
	  if (issued_insns[exu]->immx1_idx >= 0)
	    {
	      int immx_idx = issued_insns[exu]->immx1_idx;
	      lvx_immx_buffer[immx_idx].words[0] |= (tag << 27);
	      lvx_immx_buffer[immx_idx].lvx_exu = exu;
	    }
	}
    }

  /* Fill BUNDLE_INSNS with valid instructions in enum LVX_EXU order.  */
  int insn_cnt = 0;
  if (issued_insns[LVX_EXU_BCU0])
    bundle_insns[insn_cnt++] = issued_insns[LVX_EXU_BCU0];

  if (issued_insns[LVX_EXU_BCU1] == issued_insns[LVX_EXU_BCU0])
    /* Case of BCU0 with its immediate extension in BCU1.  */
    ;
  else if (issued_insns[LVX_EXU_BCU1])
    bundle_insns[insn_cnt++] = issued_insns[LVX_EXU_BCU1];

  for (int exu = LVX_EXU_ALU0; exu < LVX_EXU__; exu++)
    if (issued_insns[exu])
      bundle_insns[insn_cnt++] = issued_insns[exu];

  if (insn_cnt != lvx_insn_cnt)
    as_fatal ("mismatch between bundled and issued instructions");

  env.stcall_info.found = (bundle_flags & LVX_OPCODE_FLAG_STORE)
			  && (bundle_flags & LVX_OPCODE_FLAG_CALL);
}

/* BCU part of a conditional instruction.  */
struct lvx_cond {
  /* A pointer to the target insn within lvx_insn_buffer.  */
  struct lvx_insn *target_insn;
  /* The conditional instruction as text.  */
  char line_prefix[32];
};

static bool
lvx_cond_insn_merge (int target_exu,
		     struct lvx_insn *bundle_insns[],
		     struct lvx_insn *issued_insns[])
{
  struct lvx_insn *insn = bundle_insns[lvx_insn_cnt - 1];
  const struct lvx_opc *opdef = insn->opdef;

  if (!(opdef->codewords[0].flags & LVX_OPCODE_FLAG_COND))
    as_fatal ("[lvx_cond_insn_merge] unrecognized conditional instruction prefix.");

  /* Try merging with an already issued BCU instruction.  */
  unsigned insn_word = insn->words[0];
  unsigned insn_activate =
      1 << (target_exu - LVX_EXU_ALU0 + LVX_ACTIVATE_OFFSET);
  if (!((insn_word ^ lvx_bcu0_word) & ~LVX_ACTIVATE_MASK))
    {
      issued_insns[LVX_EXU_BCU0]->words[0] |= insn_activate;
      lvx_bcu0_word |= insn_activate;
      return true;
    }
  if (!((insn_word ^ lvx_bcu1_word) & ~LVX_ACTIVATE_MASK))
    {
      issued_insns[LVX_EXU_BCU1]->words[0] |= insn_activate;
      lvx_bcu1_word |= insn_activate;
      return true;
    }

  /* No merging, insert the new BCU instruction into ISSUED_INSNS[].  */
  int insn_idx = -1;
  if (!issued_insns[LVX_EXU_BCU0])
    {
      issued_insns[LVX_EXU_BCU0] = insn;
      lvx_bcu0_word = insn_word;
      insn_idx = 0;
    }
  else if (!issued_insns[LVX_EXU_BCU1])
    {
      issued_insns[LVX_EXU_BCU1] = insn;
      lvx_bcu1_word = insn_word;
      insn_idx = 1;
    }
  else
    as_fatal ("[lvx_cond_insn_merge] no BCU available to merge conditional.");

  /* Insert the new BCU instruction into BUNDLE_INSNS[].  */
  for (int i = lvx_insn_cnt - 2 ; i >= insn_idx; i--)
    bundle_insns[i + 1] = bundle_insns[i];
  bundle_insns[insn_idx] = insn;

  return false;
}

/* Stack of BCU insns for conditional execution.  */
static struct {
  struct lvx_cond cells[LVX_EXU__];
  int count;
} lvx_cond_stack[1];

static inline void
lvx_cond_stack_reset (void)
{
  memset (lvx_cond_stack, 0, sizeof (lvx_cond_stack));
}

static inline int
lvx_cond_stack_count (void)
{
  return lvx_cond_stack->count;
}

static inline struct lvx_cond *
lvx_cond_stack_access (int index)
{
  return lvx_cond_stack->cells + index;
}

static struct lvx_cond *
lvx_cond_stack_push (void)
{
  if (lvx_cond_stack->count >= LVX_EXU__)
    as_fatal ("[lvx_cond_stack_push] lvx_cond_stack overflow.");

  return lvx_cond_stack->cells + lvx_cond_stack->count++;
}

static void
lvx_check_resource_usage (struct lvx_insn *bundle_insns[])
{
  const int reservation_table_len =
      (lvx_core_info->reservation_table_cycles * lvx_core_info->resource_count);
  const int *resources = lvx_core_info->resources;
  int *resources_used = malloc (reservation_table_len * sizeof (int));
  memset (resources_used, 0, reservation_table_len * sizeof (int));

  for (int i = 0; i < lvx_insn_cnt; i++)
    {
      int insn_reservation = bundle_insns[i]->opdef->reservation;
      const int *reservation_table = lvx_core_info->reservation_tables[insn_reservation];
      for (int j = 0; j < reservation_table_len; j++)
	resources_used[j] += reservation_table[j];
    }

  for (int i = 0; i < lvx_core_info->reservation_table_cycles; i++)
    {
      for (int j = 0; j < lvx_core_info->resource_count; j++)
	if (resources_used[(i * lvx_core_info->resource_count) + j] > resources[j])
	  {
	    int v = resources_used[(i * lvx_core_info->resource_count) + j];
	    free (resources_used);
	    as_fatal ("resource %s over-used in bundle: %d used, %d available",
		lvx_core_info->resource_names[j], v, resources[j]);
	  }
    }

  free (resources_used);
}

/*
 * Called by core to assemble a single line.
 */
void
md_assemble (char *line)
{
#define TOK_FROM_STR(str) \
  { .insn = (str), .begin = 0, .end = 0, .class_id = -1, .val = 0 }

  char *line_cursor = line;

  if (get_byte_counter (now_seg) & 3)
    as_fatal ("code segment not word aligned in md_assemble");

  while (line_cursor && line_cursor[0] && (line_cursor[0] == ' '))
    line_cursor++;

  /* The ;; was converted to "be" by line hook.  Here we look for the bundle end
   and actually output any instructions in bundle.  */
  /* Also we need to implement the stop bit check for bundle end.  */
  if (strncmp (line_cursor, "be", 2) == 0)
    {
      inside_bundle = 0;
      //int sec_align = bfd_get_section_alignment(stdoutput, now_seg);
      struct lvx_insn *bundle_insns[NELEMS (lvx_insn_buffer)];
      struct lvx_insn *issued_insns[LVX_EXU__];
      memset (issued_insns, 0, sizeof (issued_insns));

#ifdef OBJ_ELF
      /* Emit Dwarf debug line information.  */
      dwarf2_emit_insn (0);
#endif

      /* Fill BUNDLE_INSNS[], compute lvx_insn ORDER and count bundle words.  */
      int word_cnt = 0;
      for (int j = 0; j < lvx_insn_cnt; j++)
	{
	  lvx_insn_buffer[j].order = j;
	  bundle_insns[j] = &lvx_insn_buffer[j];
	  word_cnt += lvx_insn_buffer[j].length;
	}
      if (word_cnt + lvx_immx_cnt > LVX_MAXBUNDLEWORDS)
	as_fatal ("bundle has too many syllables: %d instead of %d",
		  word_cnt + lvx_immx_cnt, LVX_MAXBUNDLEWORDS);

      /* Issue insns into ISSUED_INSNS[] and reorder BUNDLE_INSNS[].  */
      qsort (bundle_insns, lvx_insn_cnt, sizeof (struct lvx_insn *), lvx_insn_compare);
      (*lvx_reorder_bundle) (bundle_insns, issued_insns);

      if (lvx_cond_stack_count ())
	/* Assemble the guard or blend line_prefix of conditional insns.  */
	{
	  inside_bundle = 1;
	  for (int i = 0; i < lvx_cond_stack_count (); i++)
	    {
	      struct lvx_cond *cond = lvx_cond_stack_access (i);
	      int len = strlen (cond->line_prefix);
	      int target_exu = cond->target_insn->lvx_exu;
	      unsigned exu_mask = 1 << (target_exu - LVX_EXU_ALU0);
	      sprintf (cond->line_prefix + len, " %d", exu_mask);

	      struct token_s gob_tok = TOK_FROM_STR (cond->line_prefix);
	      struct token_list *gob_tok_lst = parse (gob_tok);
	      assemble_tokens (gob_tok_lst);
	      free_token_list (gob_tok_lst);

	      bundle_insns[lvx_insn_cnt - 1] = &lvx_insn_buffer[lvx_insn_cnt - 1];
	      lvx_insn_cnt -= lvx_cond_insn_merge (target_exu, bundle_insns, issued_insns);
	    }

	    lvx_cond_stack_reset ();
	    inside_bundle = 0;
	}

      if (env.opts.check_resource_usage)
	lvx_check_resource_usage (bundle_insns);

      /* Compute EMIT_CNT the total number of calls to lvx_emit_insn().  */
      int emit_cnt = lvx_insn_cnt;
      for (int i = 0; i < lvx_immx_cnt; i++)
	if (lvx_immx_buffer[i].length)
	  emit_cnt++;

      /* The ordering of the insns has been set correctly in bundle_insns.  */
      bool bcux_seen = false;
      int bcux_immx_idx = -1;
      int8_t exu2pos[LVX_EXU__];
      memset(exu2pos, -1, sizeof(exu2pos));
      for (int insn_pos = 0; insn_pos < lvx_insn_cnt; insn_pos++)
	{
	  struct lvx_insn *insn = bundle_insns[insn_pos];
	  int fixup_pos = lvx_insn_pcrel ? 0 : insn_pos;
	  lvx_emit_insn (insn, fixup_pos, !--emit_cnt);
	  assert ((unsigned)insn->lvx_exu < LVX_EXU__);
	  exu2pos[insn->lvx_exu] = insn_pos + bcux_seen;
	  if (insn == lvx_bcux_insn)
	    {
	      bcux_immx_idx = insn->immx0_idx;
	      lvx_emit_insn (&lvx_immx_buffer[bcux_immx_idx],
			     fixup_pos + 1, !--emit_cnt);
	      bcux_seen = true;
	    }
	}

      /* Emit the IMMX syllables, ordering them by EXU tags, 0 to 3.  */
      int immx_pos = lvx_insn_cnt;
      for (int tag = 0; tag < 4; tag++)
	for (int j = 0; j < lvx_immx_cnt; j++)
	  {
	    if (j == bcux_immx_idx)
	      continue;

	    struct lvx_insn *immx = &lvx_immx_buffer[j];
	    if (lvx_exu_tag (immx->words[0]) == tag)
	      {
		assert ((unsigned)immx->lvx_exu < LVX_EXU__);
		int insn_pos = exu2pos[immx->lvx_exu];
		assert (insn_pos >= 0);
		int fixup_pos = lvx_insn_pcrel ? immx_pos - insn_pos : immx_pos;
		lvx_emit_insn (immx, fixup_pos, !--emit_cnt);
		immx_pos++;
	      }
	  }

      /* A debug label that appears in the middle of a bundle
	 should rather be attached to the next bundle.
	 This is because usually these labels point to the first
	 instruction where some condition is met.
	 If the label isn't handled this way it will be attached
	 to the current bundle which is wrong as the corresponding
	 instruction wasn't executed yet.  */
      while (label_fixes)
	{
	  struct label_fix *fix = label_fixes;
	  label_fixes = fix->next;
	  symbol_set_value_now (fix->sym);
	  free (fix);
	}

      /* Reset global state for the next bundle.  */
      memset (lvx_insn_buffer, 0, sizeof (lvx_insn_buffer));
      memset (lvx_immx_buffer, 0, sizeof (lvx_immx_buffer));
      lvx_insn_cnt = lvx_immx_cnt = 0;

      return;
    }

  int cond_len = 0;
  if (env.params.core == ELF_LVX_CORE_LVX_V1
      && (cond_len = insn_cond_len (line)))
    {
      struct lvx_cond *cond = lvx_cond_stack_push ();
      cond->target_insn = lvx_insn_buffer + lvx_insn_cnt;
      strncpy (cond->line_prefix, line, cond_len);
      line_cursor += cond_len;
    }

  char *buf = NULL;
  sscanf (line_cursor, "%m[^\n]", &buf);
  struct token_s my_tok = TOK_FROM_STR (buf);
  struct token_list *tok_lst = parse (my_tok);
  free (buf);

  if (!tok_lst)
    return;

  /* Skip opcode.  */
  line_cursor += strlen (tok_lst->tok);

  inside_bundle = 1;
  assemble_tokens (tok_lst);
  free_token_list (tok_lst);

#undef TOK_FROM_STR
}

static void
lvx_set_cpu (void)
{
  if (!lvx_core_info)
    lvx_core_info = &lvx_v1_core_info;

  if (!lvx_registers)
    lvx_registers = lvx_v1_registers;

  if (!lvx_regfiles) {
    lvx_regfiles = lvx_v1_regfiles;
    lvx_regfiles_size = LVX_V1_REGFILE_REGISTERS;
  }

  if (!lvx_modifiers)
    lvx_modifiers = lvx_v1_modifiers;

  if (env.params.core == -1)
      env.params.core = lvx_core_info->elf_core;

  int lvx_bfd_mach;

  switch (lvx_core_info->elf_core)
    {
    case ELF_LVX_CORE_LVX_V1:
      lvx_bfd_mach = bfd_mach_lvx_v1_64;
      lvx_base_bundling = lvx_v1_base_bundling;
      lvx_reorder_bundle = lvx_v1_reorder_bundle;
      lvx_dump_opc = lvx_v1_dump_opc;
      lvx_insn_pcrel = true;
      setup (ELF_LVX_CORE_LVX_V1);
      break;
    case ELF_LVX_CORE_LVX_V2:
      lvx_bfd_mach = bfd_mach_lvx_v2_64;
      lvx_base_bundling = lvx_v1_base_bundling;
      lvx_reorder_bundle = lvx_v1_reorder_bundle;
      lvx_dump_opc = lvx_v1_dump_opc;
      lvx_insn_pcrel = true;
      setup (ELF_LVX_CORE_LVX_V2);
      break;
    default:
      as_fatal ("unknown elf core: 0x%x", lvx_core_info->elf_core);
    }

  if (!bfd_set_arch_mach (stdoutput, TARGET_ARCH, lvx_bfd_mach))
    as_warn (_("could not set architecture and machine"));
}

static int
lvx_opc_compare (const void *a, const void *b)
{
  const struct lvx_opc *opa = (const struct lvx_opc *) a;
  const struct lvx_opc *opb = (const struct lvx_opc *) b;
  int res = strcmp (opa->as_op, opb->as_op);

  if (res)
    return res;
  else
    {
      for (int i = 0; opa->format[i] && opb->format[i]; ++i)
	if (opa->format[i]->width != opb->format[i]->width)
	  return opa->format[i]->width - opb->format[i]->width;
      return 0;
    }
}

/***************************************************/
/*    INITIALIZE ASSEMBLER                         */
/***************************************************/

static int
print_hash (void **slot, void *arg ATTRIBUTE_UNUSED)
{
  string_tuple_t *tuple = *((string_tuple_t **) slot);
  printf ("%s\n", tuple->key);
  return 1;
}

static void
declare_register (const char *name, int number)
{
  symbolS *regS = symbol_create (name, reg_section,
				 &zero_address_frag, number);

  if (str_hash_insert (env.reg_hash, S_GET_NAME (regS), regS, 0) != NULL)
    as_fatal (_("duplicate %s"), name);
}

void
md_begin ()
{
  lvx_set_cpu ();

  /*
   * Declare register names with symbols
   */

  env.reg_hash = str_htab_create ();

  for (int i = 0; i < lvx_regfiles[lvx_regfiles_size]; i++)
    declare_register (lvx_registers[i].name, lvx_registers[i].id);

  /* Sort optab, so that identical mnemonics appear consecutively.  */
  {
    int nel;
    for (nel = 0; !STREQ ("", lvx_core_info->optab[nel].as_op); nel++)
      ;
    qsort (lvx_core_info->optab, nel, sizeof (lvx_core_info->optab[0]),
	   lvx_opc_compare);
  }

  /* The '?' is an operand separator.  */
  lex_type['?'] = 0;

  /* Create the opcode hash table      */
  /* Each name should appear only once.  */

  env.opcode_hash = str_htab_create ();
  env.reloc_hash = str_htab_create ();

  {
    struct lvx_opc *opc;
    const char *name = 0;
    for (opc = lvx_core_info->optab; !(STREQ ("", opc->as_op)); opc++)
      {
	/* Enter in hash table if this is a new name.  */
	if (!(STREQ (name, opc->as_op)))
	  {
	    name = opc->as_op;
	    if (str_hash_insert (env.opcode_hash, name, opc, 0))
	      as_fatal ("can't hash opcode `%s'", name);
	  }


	for (int i = 0 ; opc->format[i] ; ++i)
	  {
	    const char *reloc_name = TOKEN_NAME (opc->format[i]->type);
	    void *relocs = opc->format[i]->relocs;
	    if (opc->format[i]->relocs[0] != 0
		&& !str_hash_find (env.reloc_hash, reloc_name))
	      if (str_hash_insert (env.reloc_hash, reloc_name, relocs, 0))
		  as_fatal ("can't hash type `%s'", reloc_name);
	  }
      }
  }

  if (env.opts.dump_table)
    {
      htab_traverse (env.opcode_hash, print_hash, NULL);
      exit (0);
    }

  if (env.opts.dump_insn)
    {
      for (struct lvx_opc *opc = lvx_core_info->optab; !(STREQ ("", opc->as_op)); opc++)
	(*lvx_dump_opc) (opc);
      exit (0);
    }

  /* Here we enforce the minimum section alignment.  Remember, in
   * the linker we can make the boudaries between the linked sections
   * on larger boundaries.  The text segment is aligned to long words
   * because of the odd/even constraint on immediate extensions
   */

  bfd_set_section_alignment (text_section, 3);	/* -- 8 bytes  */
  bfd_set_section_alignment (data_section, 2);	/* -- 4 bytes  */
  bfd_set_section_alignment (bss_section, 2);	/* -- 4 bytes  */
  subseg_set (text_section, 0);

#define MKSYM(symb) \
  symbol_create (symb, undefined_section, &zero_address_frag, 0);

  symbolS *gotoff_sym   = MKSYM (".<gotoff>");
  symbolS *got_sym      = MKSYM (".<got>");
  symbolS *plt_sym      = MKSYM (".<plt>");
  symbolS *tlsgd_sym    = MKSYM (".<tlsgd>");
  symbolS *tlsie_sym    = MKSYM (".<tlsie>");
  symbolS *tlsle_sym    = MKSYM (".<tlsle>");
  symbolS *tlsld_sym    = MKSYM (".<tlsld>");
  symbolS *dtpoff_sym   = MKSYM (".<dtpoff>");
  symbolS *plt64_sym    = MKSYM (".<plt64>");
  symbolS *gotaddr_sym  = MKSYM (".<gotaddr>");
  symbolS *pcrel16_sym  = MKSYM (".<pcrel16>");
  symbolS *pcrel_sym    = MKSYM (".<pcrel>");
  symbolS *signed32_sym = MKSYM (".<signed32>");

#undef MK_SYMBOL

  for (int i = 0; i < lvx_core_info->nb_pseudo_funcs; ++i)
    {
#define PSEUDO_FUNC_IS(pf) \
  !strcmp (lvx_core_info->pseudo_funcs[i].name, (pf))

      symbolS *sym;
      if (PSEUDO_FUNC_IS ("gotoff"))
	sym = gotoff_sym;
      else if (PSEUDO_FUNC_IS ("got"))
	sym = got_sym;
      else if (PSEUDO_FUNC_IS ("plt"))
	sym = plt_sym;
      else if (PSEUDO_FUNC_IS ("tlsgd"))
	sym = tlsgd_sym;
      else if (PSEUDO_FUNC_IS ("tlsle"))
	sym = tlsle_sym;
      else if (PSEUDO_FUNC_IS ("tlsld"))
	sym = tlsld_sym;
      else if (PSEUDO_FUNC_IS ("dtpoff"))
	sym = dtpoff_sym;
      else if (PSEUDO_FUNC_IS ("tlsie"))
	sym = tlsie_sym;
      else if (PSEUDO_FUNC_IS ("plt64"))
	sym = plt64_sym;
      else if (PSEUDO_FUNC_IS ("pcrel16"))
	sym = pcrel16_sym;
      else if (PSEUDO_FUNC_IS ("pcrel"))
	sym = pcrel_sym;
      else if (PSEUDO_FUNC_IS ("gotaddr"))
	sym = gotaddr_sym;
      else if (PSEUDO_FUNC_IS ("signed32"))
	sym = signed32_sym;
      else
	as_fatal ("unknown pseudo func `%s'",
	    lvx_core_info->pseudo_funcs[i].name);

      lvx_core_info->pseudo_funcs[i].sym = sym;
    }
#undef PSEUDO_FUNC_IS
}

/***************************************************/
/*          ASSEMBLER CLEANUP STUFF                */
/***************************************************/

/* Return non-zero if the indicated VALUE has overflowed the maximum
   range expressible by a signed number with the indicated number of
   BITS.

   This is from tc-aarch64.c
*/

static bfd_boolean
signed_overflow (offsetT value, unsigned bits)
{
  offsetT lim;
  if (bits >= sizeof (offsetT) * 8)
    return FALSE;
  lim = (offsetT) 1 << (bits - 1);
  return (value < -lim || value >= lim);
}

/***************************************************/
/*          ASSEMBLER FIXUP STUFF                  */
/***************************************************/

void
md_apply_fix (fixS * fixP, valueT * valueP, segT segmentP ATTRIBUTE_UNUSED)
{
  char *const fixpos = fixP->fx_frag->fr_literal + fixP->fx_where;
  valueT value = *valueP;
  valueT image;
  arelent *rel;

  rel = (arelent *) xmalloc (sizeof (arelent));

  rel->howto = bfd_reloc_type_lookup (stdoutput, fixP->fx_r_type);
  if (rel->howto == NULL)
    {
      as_fatal
	("[md_apply_fix] unsupported relocation type (can't find howto)");
    }

  /* Note whether this will delete the relocation.  */
  if (fixP->fx_addsy == NULL && fixP->fx_pcrel == 0)
    fixP->fx_done = 1;

  if (fixP->fx_size > 0)
    image = md_chars_to_number (fixpos, fixP->fx_size);
  else
    image = 0;
  if (fixP->fx_addsy != NULL)
    {
      switch (fixP->fx_r_type)
	{
	case BFD_RELOC_LVX_S37_TLS_LE_UP27:
	case BFD_RELOC_LVX_S37_TLS_LE_LO10:

	case BFD_RELOC_LVX_S43_TLS_LE_EX6:
	case BFD_RELOC_LVX_S43_TLS_LE_UP27:
	case BFD_RELOC_LVX_S43_TLS_LE_LO10:

	case BFD_RELOC_LVX_S37_TLS_GD_LO10:
	case BFD_RELOC_LVX_S37_TLS_GD_UP27:

	case BFD_RELOC_LVX_S43_TLS_GD_LO10:
	case BFD_RELOC_LVX_S43_TLS_GD_UP27:
	case BFD_RELOC_LVX_S43_TLS_GD_EX6:

	case BFD_RELOC_LVX_S37_TLS_IE_LO10:
	case BFD_RELOC_LVX_S37_TLS_IE_UP27:

	case BFD_RELOC_LVX_S43_TLS_IE_LO10:
	case BFD_RELOC_LVX_S43_TLS_IE_UP27:
	case BFD_RELOC_LVX_S43_TLS_IE_EX6:

	case BFD_RELOC_LVX_S37_TLS_LD_LO10:
	case BFD_RELOC_LVX_S37_TLS_LD_UP27:

	case BFD_RELOC_LVX_S43_TLS_LD_LO10:
	case BFD_RELOC_LVX_S43_TLS_LD_UP27:
	case BFD_RELOC_LVX_S43_TLS_LD_EX6:

	  S_SET_THREAD_LOCAL (fixP->fx_addsy);
	  break;
	default:
	  break;
	}
    }

  /* If relocation has been marked for deletion, apply remaining changes.  */
  if (fixP->fx_done)
    {
      switch (fixP->fx_r_type)
	{
	case BFD_RELOC_8:
	case BFD_RELOC_16:
	case BFD_RELOC_32:
	case BFD_RELOC_64:

	case BFD_RELOC_LVX_GLOB_DAT:
	case BFD_RELOC_LVX_32_GOT:
	case BFD_RELOC_LVX_64_GOT:
	case BFD_RELOC_LVX_64_GOTOFF:
	case BFD_RELOC_LVX_32_GOTOFF:
	  image = value;
	  md_number_to_chars (fixpos, image, fixP->fx_size);
	  break;

	case BFD_RELOC_LVX_S11S2_PCREL:
	  if (signed_overflow (value, 11 + 2))
	    as_bad_where (fixP->fx_file, fixP->fx_line,
			  _("branch out of range"));
	  goto pcrel_common;

	case BFD_RELOC_LVX_S17S2_PCREL:
	  if (signed_overflow (value, 17 + 2))
	    as_bad_where (fixP->fx_file, fixP->fx_line,
			  _("branch out of range"));
	  goto pcrel_common;

	case BFD_RELOC_LVX_S27S2_PCREL:
	  if (signed_overflow (value, 27 + 2))
	    as_bad_where (fixP->fx_file, fixP->fx_line,
			  _("branch out of range"));
	  goto pcrel_common;

	case BFD_RELOC_LVX_S38S2_PCREL_LO11:
	case BFD_RELOC_LVX_S38S2_PCREL_UP27:
	  if (signed_overflow (value, 11 + 27 + 2))
	    as_bad_where (fixP->fx_file, fixP->fx_line,
			  _("signed38 PCREL value out of range"));
	  goto pcrel_common;

	case BFD_RELOC_LVX_S44S2_PCREL_LO17:
	case BFD_RELOC_LVX_S44S2_PCREL_UP27:
	  if (signed_overflow (value, 17 + 27 + 2))
	    as_bad_where (fixP->fx_file, fixP->fx_line,
			  _("signed44 PCREL value out of range"));
	  goto pcrel_common;

	case BFD_RELOC_LVX_S54S2_PCREL_LO27:
	case BFD_RELOC_LVX_S54S2_PCREL_UP27:
	  if (signed_overflow (value, 27 + 27 + 2))
	    as_bad_where (fixP->fx_file, fixP->fx_line,
			  _("signed54 PCREL value out of range"));
	  goto pcrel_common;

	case BFD_RELOC_LVX_S16_PCREL:
	  if (signed_overflow (value, 16))
	    as_bad_where (fixP->fx_file, fixP->fx_line,
			  _("signed16 PCREL value out of range"));
	  goto pcrel_common;

	case BFD_RELOC_LVX_S43_PCREL_LO10:
	case BFD_RELOC_LVX_S43_PCREL_UP27:
	case BFD_RELOC_LVX_S43_PCREL_EX6:
	  if (signed_overflow (value, 10 + 27 + 6))
	    as_bad_where (fixP->fx_file, fixP->fx_line,
			  _("signed43 PCREL value out of range"));
	  goto pcrel_common;

	case BFD_RELOC_LVX_S37_PCREL_LO10:
	case BFD_RELOC_LVX_S37_PCREL_UP27:
	  if (signed_overflow (value, 10 + 27))
	    as_bad_where (fixP->fx_file, fixP->fx_line,
			  _("signed37 PCREL value out of range"));
	  goto pcrel_common;

	case BFD_RELOC_LVX_S64_PCREL_LO10:
	case BFD_RELOC_LVX_S64_PCREL_UP27:
	case BFD_RELOC_LVX_S64_PCREL_EX27:

	pcrel_common:
	  if (fixP->fx_pcrel || fixP->fx_addsy)
	    return;
	  value =
	    (((value >> rel->howto->rightshift) << rel->howto->bitpos) & rel->
	     howto->dst_mask);
	  image = (image & ~(rel->howto->dst_mask)) | value;
	  md_number_to_chars (fixpos, image, fixP->fx_size);
	  break;

	case BFD_RELOC_LVX_S64_GOTADDR_LO10:
	case BFD_RELOC_LVX_S64_GOTADDR_UP27:
	case BFD_RELOC_LVX_S64_GOTADDR_EX27:

	case BFD_RELOC_LVX_S43_GOTADDR_LO10:
	case BFD_RELOC_LVX_S43_GOTADDR_UP27:
	case BFD_RELOC_LVX_S43_GOTADDR_EX6:

	case BFD_RELOC_LVX_S37_GOTADDR_LO10:
	case BFD_RELOC_LVX_S37_GOTADDR_UP27:
	  value = 0;
	  /* Fallthrough.  */

	case BFD_RELOC_LVX_S32_UP27:

	case BFD_RELOC_LVX_S37_UP27:

	case BFD_RELOC_LVX_S43_UP27:

	case BFD_RELOC_LVX_S64_UP27:
	case BFD_RELOC_LVX_S64_EX27:
	case BFD_RELOC_LVX_S64_LO10:

	case BFD_RELOC_LVX_S43_TLS_LE_UP27:
	case BFD_RELOC_LVX_S43_TLS_LE_EX6:

	case BFD_RELOC_LVX_S37_TLS_LE_UP27:

	case BFD_RELOC_LVX_S37_GOTOFF_UP27:

	case BFD_RELOC_LVX_S43_GOTOFF_UP27:
	case BFD_RELOC_LVX_S43_GOTOFF_EX6:

	case BFD_RELOC_LVX_S43_GOT_UP27:
	case BFD_RELOC_LVX_S43_GOT_EX6:

	case BFD_RELOC_LVX_S37_GOT_UP27:

	case BFD_RELOC_LVX_S32_LO5:
	case BFD_RELOC_LVX_S37_LO10:

	case BFD_RELOC_LVX_S43_LO10:
	case BFD_RELOC_LVX_S43_EX6:

	case BFD_RELOC_LVX_S43_TLS_LE_LO10:
	case BFD_RELOC_LVX_S37_TLS_LE_LO10:

	case BFD_RELOC_LVX_S37_GOTOFF_LO10:
	case BFD_RELOC_LVX_S43_GOTOFF_LO10:

	case BFD_RELOC_LVX_S43_GOT_LO10:
	case BFD_RELOC_LVX_S37_GOT_LO10:

	default:
	  as_fatal ("[md_apply_fix]:"
		    "unsupported relocation type (type not handled : %d)",
		    fixP->fx_r_type);
	}
    }
  xfree (rel);
}

/*
 * Warning: Can be called only in fixup_segment() after fx_addsy field
 * has been updated by calling symbol_get_value_expression(...->X_add_symbol)
 */
int
lvx_validate_sub_fix (fixS * fixP)
{
  segT add_symbol_segment, sub_symbol_segment;

  switch (fixP->fx_r_type)
    {
    case BFD_RELOC_8:
    case BFD_RELOC_16:
    case BFD_RELOC_32:
      if (fixP->fx_addsy != NULL)
	add_symbol_segment = S_GET_SEGMENT (fixP->fx_addsy);
      else
	return 0;
      if (fixP->fx_subsy != NULL)
	sub_symbol_segment = S_GET_SEGMENT (fixP->fx_subsy);
      else
	return 0;

      if ((strcmp (S_GET_NAME (fixP->fx_addsy),
		   S_GET_NAME (fixP->fx_subsy)) == 0) &&
	  (add_symbol_segment == sub_symbol_segment))
	return 1;
      break;
    default:
      break;
    }

  return 0;
}

/* Called whenever some data item (not an instruction) needs a fixup.  */
void
lvx_cons_fix_new (fragS * f, int where, int nbytes, expressionS * exp,
		  bfd_reloc_code_real_type code)
{
  if (exp->X_op == O_pseudo_fixup)
    {
      exp->X_op = O_symbol;
      struct pseudo_func *pf =
	lvx_get_pseudo_func_data_scn (exp->X_op_symbol);
      assert (pf != NULL);
      code = pf->pseudo_relocs.single;

      if (code == BFD_RELOC_UNUSED)
	as_fatal ("[lvx_cons_fix_new] unsupported relocation");
    }
  else
    {
      switch (nbytes)
	{
	case 1:
	  code = BFD_RELOC_8;
	  break;
	case 2:
	  code = BFD_RELOC_16;
	  break;
	case 4:
	  code = BFD_RELOC_32;
	  break;
	case 8:
	  code = BFD_RELOC_64;
	  break;
	default:
	  as_fatal ("unsupported BFD relocation size %u", nbytes);
	  break;
	}
    }
  fix_new_exp (f, where, nbytes, exp, 0, code);
}

/*
 * generate a relocation record
 */

arelent *
tc_gen_reloc (asection * sec ATTRIBUTE_UNUSED, fixS * fixp)
{
  arelent *reloc;
  bfd_reloc_code_real_type code;

  reloc = (arelent *) xmalloc (sizeof (arelent));

  reloc->sym_ptr_ptr = (asymbol **) xmalloc (sizeof (asymbol *));
  *reloc->sym_ptr_ptr = symbol_get_bfdsym (fixp->fx_addsy);
  reloc->address = fixp->fx_frag->fr_address + fixp->fx_where;

  code = fixp->fx_r_type;
  if (code == BFD_RELOC_32 && fixp->fx_pcrel)
    code = BFD_RELOC_32_PCREL;
  reloc->howto = bfd_reloc_type_lookup (stdoutput, code);

  if (reloc->howto == NULL)
    {
      as_bad_where (fixp->fx_file, fixp->fx_line,
		    "cannot represent `%s' relocation in object file",
		    bfd_get_reloc_code_name (code));
      return NULL;
    }

//  if (!fixp->fx_pcrel != !reloc->howto->pc_relative)
//    {
//      as_fatal ("internal error? cannot generate `%s' relocation",
//		bfd_get_reloc_code_name (code));
//    }
//  assert (!fixp->fx_pcrel == !reloc->howto->pc_relative);

  reloc->addend = fixp->fx_offset;

  /*
   * Ohhh, this is ugly.  The problem is that if this is a local global
   * symbol, the relocation will entirely be performed at link time, not
   * at assembly time.  bfd_perform_reloc doesn't know about this sort
   * of thing, and as a result we need to fake it out here.
   */

  /* GD I'm not sure what this is used for in the lvx case but it sure
     messes up the relocs when emit_all_relocs is used as they are not
     resolved with respect to a global sysmbol (e.g. .text), and hence
     they are ALWAYS resolved at link time.  */
  /* FIXME FIXME                                                       */

  /* clarkes: 030827:  This code (and the other half of the fix in write.c)
   * have caused problems with the PIC relocations.
   * The root problem is that bfd_install_relocation adds in to the reloc
   * addend the section offset of a symbol defined in the current object.
   * This causes problems on numerous other targets too, and there are
   * several different methods used to get around it:
   *   1.  In tc_gen_reloc, subtract off the value that bfd_install_relocation
   *       added.  That is what we do here, and it is also done the
   *       same way for alpha.
   *   2.  In md_apply_fix, subtract off the value that bfd_install_relocation
   *       will add.  This is done on SH (non-ELF) and sparc targets.
   *   3.  In the howto structure for the relocations, specify a
   *       special function that does not return bfd_reloc_continue.
   *       This causes bfd_install_relocaion to terminate before it
   *       adds in the symbol offset.  This is done on SH ELF targets.
   *       Note that on ST200 we specify bfd_elf_generic_reloc as
   *       the special function.  This will return bfd_reloc_continue
   *       only in some circumstances, but in particular if the reloc
   *       is marked as partial_inplace in the bfd howto structure, then
   *       bfd_elf_generic_reloc will return bfd_reloc_continue.
   *       Some ST200 relocations are marked as partial_inplace
   *       (this is an error in my opinion because ST200 always uses
   *       a separate addend), but some are not.  The PIC relocations
   *       are not marked as partial_inplace, so for them,
   *       bfd_elf_generic_reloc returns bfd_reloc_ok, and the addend
   *       is not modified by bfd_install_relocation.   The relocations
   *       R_LVX_16 and R_LVX_32 are marked partial_inplace, and so for
   *       these we need to correct the addend.
   * In the code below, the condition in the emit_all_relocs branch
   * (now moved to write.c) is the inverse of the condition that
   * bfd_elf_generic_reloc uses to short-circuit the code in
   * bfd_install_relocation that modifies the addend.  The condition
   * in the else branch match the condition used in the alpha version
   * of tc_gen_reloc (see tc-alpha.c).
   * I do not know why we need to use different conditions in these
   * two branches, it seems to me that the condition should be the same
   * whether or not emit_all_relocs is true.
   * I also do not understand why it was necessary to move the emit_all_relocs
   * condition to write.c.
   */

  if (S_IS_EXTERNAL (fixp->fx_addsy) &&
      !S_IS_COMMON (fixp->fx_addsy) && reloc->howto->partial_inplace)
    reloc->addend -= symbol_get_bfdsym (fixp->fx_addsy)->value;

  return reloc;
}

/* Round up segment to appropriate boundary.  */

valueT
md_section_align (asection * seg ATTRIBUTE_UNUSED, valueT size)
{
#ifndef OBJ_ELF
  /* This is not right for ELF; a.out wants it, and COFF will force
   * the alignment anyways.  */
  int align = bfd_get_section_alignment (stdoutput, seg);
  valueT mask = ((valueT) 1 << align) - 1;
  return (size + mask) & ~mask;
#else
  return size;
#endif
}

int
md_estimate_size_before_relax (register fragS * fragP ATTRIBUTE_UNUSED,
			       segT segtype ATTRIBUTE_UNUSED)
{
  as_fatal ("estimate_size_before_relax called");
}

void
md_convert_frag (bfd * abfd ATTRIBUTE_UNUSED,
		 asection * sec ATTRIBUTE_UNUSED,
		 fragS * fragp ATTRIBUTE_UNUSED)
{
  as_fatal ("lvx convert_frag");
}

symbolS *
md_undefined_symbol (char *name ATTRIBUTE_UNUSED)
{
  return 0;
}

const char *
md_atof (int type ATTRIBUTE_UNUSED,
	 char *litp ATTRIBUTE_UNUSED, int *sizep ATTRIBUTE_UNUSED)
{
  return ieee_md_atof (type, litp, sizep, TARGET_BYTES_BIG_ENDIAN);
}

/*
 * calculate the base for a pcrel fixup
 * -- for relocation, we might need to add addend ?
 */

long
md_pcrel_from (fixS * fixP)
{
  return (fixP->fx_where + fixP->fx_frag->fr_address);
}

/**************************************************************/
/*   Hooks into standard processing -- we hook into label     */
/*   handling code to detect double ':' and we hook before    */
/*   a line of code is processed to do some simple sed style  */
/*   edits.                                                   */
/**************************************************************/

static symbolS *last_proc_sym = NULL;
static int update_last_proc_sym = 0;

void
lvx_frob_label (symbolS *sym)
{
  if (update_last_proc_sym)
    {
      last_proc_sym = sym;
      update_last_proc_sym = 0;
    }

  if (inside_bundle)
    {
      struct label_fix *fix;
      fix = malloc (sizeof (*fix));
      fix->next = label_fixes;
      fix->sym = sym;
      label_fixes = fix;
    }

  dwarf2_emit_label (sym);
}

void
lvx_check_label (symbolS *sym)
{
  /* Labels followed by a second semi-colon are considered external symbols.  */
  if (*input_line_pointer == ':')
    {
      S_SET_EXTERNAL (sym);
      input_line_pointer++;
    }
}

/* Emit single bundle nop. This is needed by .nop asm directive
 * Have to manage end of bundle done usually by start_line_hook
 * using BE pseudo op
 */
void
lvx_emit_single_noop (void)
{
  char *nop;
  char *end_of_bundle;

  if (asprintf (&nop, "nop") < 0)
    as_fatal ("%s", xstrerror (errno));

  if (asprintf (&end_of_bundle, "be") < 0)
    as_fatal ("%s", xstrerror (errno));

  char *saved_ilp = input_line_pointer;
  md_assemble (nop);
  md_assemble (end_of_bundle);
  input_line_pointer = saved_ilp;
  free (nop);
  free (end_of_bundle);
}

/*  Edit out some syntactic sugar that confuses GAS.
    input_line_pointer is guaranteed to point to the
    the current line but may include text from following
    lines.  Thus, '\n' must be scanned for as well as '\0'.  */

void
lvx_md_start_line_hook (void)
{
  char *t;

  for (t = input_line_pointer; t && t[0] == ' '; t++);

  /* Detect illegal syntax patterns:
   * - two bundle ends on the same line: ;; ;;
   * - illegal token: ;;;
   */
  if (t && (t[0] == ';') && (t[1] == ';'))
    {
      char *tmp_t;
      bool newline_seen = false;

      if (t[2] == ';')
	as_fatal ("syntax error: Illegal ;;; token");

      tmp_t = t + 2;

      while (tmp_t && tmp_t[0])
	{
	  while (tmp_t && tmp_t[0] &&
		 ((tmp_t[0] == ' ') || (tmp_t[0] == '\n')))
	    {
	      if (tmp_t[0] == '\n')
		newline_seen = true;
	      tmp_t++;
	    }
	  if (tmp_t[0] == ';' && tmp_t[1] == ';')
	    {
	      /* if there's no newline between the two bundle stops
	       * then raise a syntax error now, otherwise a strange error
	       * message from read.c will be raised: "junk at end of line..."
	       */
	      if (tmp_t[2] == ';')
		as_fatal ("syntax error: Illegal ;;; token");

	      if (!newline_seen)
		  as_fatal ("syntax error: More than one bundle stop on a line");
	      newline_seen = false;	/* Reset.  */

	      /* This is an empty bundle, transform it into an
	       * empty statement.  */
	      tmp_t[0] = ';';
	      tmp_t[1] = ' ';

	      tmp_t += 2;
	    }
	  else
	    break;
	}
    }

  /* Check for bundle end.
     We transform these into a special opcode BE
     because gas has ';' hardwired as a statement end.  */
  if (t && (t[0] == ';') && (t[1] == ';'))
    {
      t[0] = 'B';
      t[1] = 'E';
      return;
    }
}

static void
lvx_check_resources (int f)
{
  env.opts.check_resource_usage = f;
}

/* Called before write_object_file.  */
void
lvx_end (void)
{
  int newflags;

  if (!env.params.core_set)
    env.params.core = lvx_core_info->elf_core;

  /* (pp) the flags must be set at once.  */
  newflags = env.params.core | env.params.abi | env.params.pic_flags;

  newflags |= ELF_LVX_ABI_64B_ADDR_BIT;

  bfd_set_private_flags (stdoutput, newflags);

  cleanup ();

  if (inside_bundle && lvx_insn_cnt != 0)
    as_bad ("unexpected end-of-file while processing a bundle."
	    "  Please check that ;; is on its own line.");
}

static void
lvx_type (int start ATTRIBUTE_UNUSED)
{
  char *name;
  char c;
  int type;
  char *typename = NULL;
  symbolS *sym;
  elf_symbol_type *elfsym;

  c = get_symbol_name (&name);
  sym = symbol_find_or_make (name);
  elfsym = (elf_symbol_type *) symbol_get_bfdsym (sym);
  *input_line_pointer = c;

  if (!*S_GET_NAME (sym))
    as_bad (_("missing symbol name in directive"));

  SKIP_WHITESPACE ();

  /* When the symbol is enclosed by double quotes, the input line pointer ends
     up on the closing double quote.  Skip it.  */
  if (*input_line_pointer == '"')
    ++input_line_pointer;

  SKIP_WHITESPACE ();
  if (*input_line_pointer == ',')
    ++input_line_pointer;
  SKIP_WHITESPACE ();

  if (*input_line_pointer == '#'
      || *input_line_pointer == '@'
      || *input_line_pointer == '"' || *input_line_pointer == '%')
    ++input_line_pointer;

  /* typename = input_line_pointer;  */
  /* c = get_symbol_end();  */
  c = get_symbol_name (&typename);

  type = 0;
  if (strcmp (typename, "function") == 0
      || strcmp (typename, "STT_FUNC") == 0)
    type = BSF_FUNCTION;
  else if (strcmp (typename, "object") == 0
	   || strcmp (typename, "STT_OBJECT") == 0)
    type = BSF_OBJECT;
  else if (strcmp (typename, "tls_object") == 0
	   || strcmp (typename, "STT_TLS") == 0)
    type = BSF_OBJECT | BSF_THREAD_LOCAL;
  else if (strcmp (typename, "common") == 0
	   || strcmp (typename, "STT_COMMON") == 0)
    type = BSF_ELF_COMMON;
  else if (strcmp (typename, "gnu_unique_object") == 0
	   || strcmp (typename, "STB_GNU_UNIQUE") == 0)
    {
      elf_tdata (stdoutput)->has_gnu_osabi |= elf_gnu_osabi_unique;
      type = BSF_OBJECT | BSF_GNU_UNIQUE;
    }
  else if (strcmp (typename, "notype") == 0
	   || strcmp (typename, "STT_NOTYPE") == 0)
    ;
#ifdef md_elf_symbol_type
  else if ((type = md_elf_symbol_type (typename, sym, elfsym)) != -1)
    ;
#endif
  else
    as_bad (_("unrecognized symbol type \"%s\""), typename);

  *input_line_pointer = c;

  if (*input_line_pointer == '"')
    ++input_line_pointer;

  elfsym->symbol.flags |= type;
  symbol_get_bfdsym (sym)->flags |= type;

  demand_empty_rest_of_line ();
}

#define ENDPROCEXTENSION	"$endproc"
#define MINUSEXPR		".-"

static int proc_endp_status = 0;

static void
lvx_endp (int start ATTRIBUTE_UNUSED)
{
  char c;
  char *name;

  if (inside_bundle)
    as_warn (".endp directive inside a bundle.");
  /* Function name is optionnal and is ignored  */
  /* There may be several names separated by commas...  */
  while (1)
    {
      SKIP_WHITESPACE ();
      c = get_symbol_name (&name);
      (void) restore_line_pointer (c);
      SKIP_WHITESPACE ();
      if (*input_line_pointer != ',')
	break;
      ++input_line_pointer;
    }
  demand_empty_rest_of_line ();

  if (!proc_endp_status)
    {
      as_warn (".endp directive doesn't follow .proc -- ignoring ");
      return;
    }

  proc_endp_status = 0;

  /* TB begin : add BSF_FUNCTION attribute to last_proc_sym symbol.  */
  if (size_type_function)
    {
      if (!last_proc_sym)
	{
	  as_bad ("cannot set function attributes (bad symbol)");
	  return;
	}

      /*    last_proc_sym->symbol.flags |= BSF_FUNCTION;  */
      symbol_get_bfdsym (last_proc_sym)->flags |= BSF_FUNCTION;
      /* Add .size funcname,.-funcname in order to add size
	 attribute to the current function.  */
      {
	const int newdirective_sz =
	  strlen (S_GET_NAME (last_proc_sym)) + strlen (MINUSEXPR) + 1;
	char *newdirective = malloc (newdirective_sz);
	char *savep = input_line_pointer;
	expressionS exp;

	memset (newdirective, 0, newdirective_sz);

	/* BUILD :".-funcname" expression.  */
	strcat (newdirective, MINUSEXPR);
	strcat (newdirective, S_GET_NAME (last_proc_sym));
	input_line_pointer = newdirective;
	expression (&exp);

	if (exp.X_op == O_constant)
	  {
	    S_SET_SIZE (last_proc_sym, exp.X_add_number);
	    if (symbol_get_obj (last_proc_sym)->size)
	      {
		xfree (symbol_get_obj (last_proc_sym)->size);
		symbol_get_obj (last_proc_sym)->size = NULL;
	      }
	  }
	else
	  {
	    symbol_get_obj (last_proc_sym)->size =
	      (expressionS *) xmalloc (sizeof (expressionS));
	    *symbol_get_obj (last_proc_sym)->size = exp;
	  }

	/* Just restore the real input pointer.  */
	input_line_pointer = savep;
	free (newdirective);
      }
    }

  last_proc_sym = NULL;
}

static void
lvx_proc (int start ATTRIBUTE_UNUSED)
{
  char c;
  char *name;
  /* There may be several names separated by commas...  */
  while (1)
    {
      SKIP_WHITESPACE ();
      c = get_symbol_name (&name);
      (void) restore_line_pointer (c);

      SKIP_WHITESPACE ();
      if (*input_line_pointer != ',')
	break;
      ++input_line_pointer;
    }
  demand_empty_rest_of_line ();

  if (proc_endp_status)
    {
      as_warn (".proc follows .proc -- ignoring");
      return;
    }

  proc_endp_status = 1;

  /* This code emit a global symbol to mark the end of each function
     the symbol emitted has a name formed by the original function name
     concatenated with $endproc so if _foo is a function name the symbol
     marking the end of it is _foo$endproc.  */
  /* It is also required for generation of .size directive in lvx_endp().  */

  if (size_type_function)
    update_last_proc_sym = 1;
}

int
lvx_force_reloc (fixS * fixP)
{
  symbolS *sym;
  asection *symsec;

  if (generic_force_reloc (fixP))
    return 1;

  switch (fixP->fx_r_type)
    {
    case BFD_RELOC_LVX_32_GOTOFF:
    case BFD_RELOC_LVX_S37_GOTOFF_UP27:
    case BFD_RELOC_LVX_S37_GOTOFF_LO10:

    case BFD_RELOC_LVX_64_GOTOFF:
    case BFD_RELOC_LVX_S43_GOTOFF_UP27:
    case BFD_RELOC_LVX_S43_GOTOFF_LO10:
    case BFD_RELOC_LVX_S43_GOTOFF_EX6:

    case BFD_RELOC_LVX_32_GOT:
    case BFD_RELOC_LVX_64_GOT:
    case BFD_RELOC_LVX_S37_GOT_UP27:
    case BFD_RELOC_LVX_S37_GOT_LO10:

    case BFD_RELOC_LVX_GLOB_DAT:
      return 1;
    default:
      return 0;
    }

  sym = fixP->fx_addsy;
  if (sym)
    {
      symsec = S_GET_SEGMENT (sym);
      /* if (bfd_is_abs_section (symsec)) return 0;  */
      if (!SEG_NORMAL (symsec))
	return 0;
    }
  return 1;
}

int
lvx_force_reloc_sub_same (fixS * fixP, segT sec)
{
  symbolS *sym;
  asection *symsec;
  const char *sec_name = NULL;

  if (generic_force_reloc (fixP))
    return 1;

  switch (fixP->fx_r_type)
    {
    case BFD_RELOC_LVX_32_GOTOFF:
    case BFD_RELOC_LVX_S37_GOTOFF_UP27:
    case BFD_RELOC_LVX_S37_GOTOFF_LO10:

    case BFD_RELOC_LVX_64_GOTOFF:
    case BFD_RELOC_LVX_S43_GOTOFF_UP27:
    case BFD_RELOC_LVX_S43_GOTOFF_LO10:
    case BFD_RELOC_LVX_S43_GOTOFF_EX6:

    case BFD_RELOC_LVX_32_GOT:
    case BFD_RELOC_LVX_64_GOT:
    case BFD_RELOC_LVX_S37_GOT_UP27:
    case BFD_RELOC_LVX_S37_GOT_LO10:

    case BFD_RELOC_LVX_S37_LO10:
    case BFD_RELOC_LVX_S37_UP27:

    case BFD_RELOC_LVX_GLOB_DAT:
      return 1;

    default:
      return 0;
    }

  sym = fixP->fx_addsy;
  if (sym)
    {
      symsec = S_GET_SEGMENT (sym);
      /* if (bfd_is_abs_section (symsec)) return 0;  */
      if (!SEG_NORMAL (symsec))
	return 0;

      /*
       * for .debug_arrange, .debug_frame, .eh_frame sections, containing
       * expressions of the form "sym2 - sym1 + addend", solve them even when
       * --emit-all-relocs is set. Otherwise, a relocation on two symbols
       * is necessary and fails at elf level. Binopt should not be impacted by
       * the resolution of this relocatable expression on symbols inside a
       * function.
       */
      sec_name = segment_name (sec);
      if ((strcmp (sec_name, ".eh_frame") == 0) ||
	  (strcmp (sec_name, ".except_table") == 0) ||
	  (strncmp (sec_name, ".debug_", sizeof (".debug_")) == 0))
	return 0;
    }
  return 1;
}

/* Implement HANDLE_ALIGN.  */

static void
lvx_make_nops (char *buf, bfd_vma bytes)
{
  bfd_vma i = 0;
  unsigned int j;

  static unsigned int nop_single = 0;

  if (!nop_single)
    {
      const struct lvx_opc *opcode =
	(struct lvx_opc *) str_hash_find (env.opcode_hash, "nop");

      if (opcode == NULL)
	as_fatal ("could not find opcode for 'nop' during padding");

      nop_single = opcode->codewords[0].opcode;
    }

  /* LVX instructions are always 4-bytes aligned. If we are at a position
     that is not 4 bytes aligned, it means this is not part of an instruction,
     so it is safe to use a zero byte for padding.  */

  for (j = bytes % 4; j > 0; j--)
    buf[i++] = 0;

  for (j = 0; j < (bytes - i); j += 4)
    {
      unsigned nop = nop_single;

      // nop has bundle end only if #4 nop or last padding nop.
      // Sets the parallel bit when neither conditions are matched.
      // 4*4 = biggest nop bundle we can get
      // 12 = offset when writting the last nop possible in a 4 nops bundle
      // bytes-i-4 = offset for the last 4-words in the padding
      if (j % (4 * 4) != 12 && j != (bytes - i - 4))
	nop |= LVX_PARALLEL_BIT;

      memcpy (buf + i + j, &nop, sizeof (nop));
    }
}

/* Pads code section with bundle of nops when possible, 0 if not.  */
void
lvx_handle_align (fragS *fragP)
{
  switch (fragP->fr_type)
    {
    case rs_align_code:
      {
	bfd_signed_vma bytes = (fragP->fr_next->fr_address
				- fragP->fr_address - fragP->fr_fix);
	char *p = fragP->fr_literal + fragP->fr_fix;

	if (bytes <= 0)
	  break;

	/* Insert zeros or nops to get 4 byte alignment.  */
	lvx_make_nops (p, bytes);
	fragP->fr_fix += bytes;
      }
      break;

    default:
      break;
    }
}
/*
 * This is just used for debugging
 */

ATTRIBUTE_UNUSED
static void
print_operand (expressionS * e, FILE * out)
{
  if (e)
    {
      switch (e->X_op)
	{
	case O_register:
	  fprintf (out, "%s", lvx_registers[e->X_add_number].name);
	  break;

	case O_constant:
	  if (e->X_add_symbol)
	    {
	      if (e->X_add_number)
		fprintf (out, "(%s + %d)", S_GET_NAME (e->X_add_symbol),
			 (int) e->X_add_number);
	      else
		fprintf (out, "%s", S_GET_NAME (e->X_add_symbol));
	    }
	  else
	    fprintf (out, "%d", (int) e->X_add_number);
	  break;

	case O_symbol:
	  if (e->X_add_symbol)
	    {
	      if (e->X_add_number)
		fprintf (out, "(%s + %d)", S_GET_NAME (e->X_add_symbol),
			 (int) e->X_add_number);
	      else
		fprintf (out, "%s", S_GET_NAME (e->X_add_symbol));
	    }
	  else
	    fprintf (out, "%d", (int) e->X_add_number);
	  break;

	default:
	  fprintf (out, "o,ptype-%d", e->X_op);
	}
    }
}

void
lvx_cfi_frame_initial_instructions (void)
{
  cfi_add_CFA_def_cfa (LVX_SP_REGNO, 0);
}

int
lvx_regname_to_dw2regnum (const char *regname)
{
  unsigned int regnum = -1;
  const char *p;
  char *q;

  if (regname[0] == 'r')
    {
      p = regname + 1;
      regnum = strtoul (p, &q, 10);
      if (p == q || *q || regnum >= 64)
	return -1;
    }
  return regnum;
}

#undef DEFINE_STACK
