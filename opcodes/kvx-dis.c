/* kvx-dis.c -- Kalray MPPA generic disassembler.
   Copyright (C) 2009-2023 Free Software Foundation, Inc.
   Contributed by Kalray SA.

   This file is part of the GNU opcodes library.

   This library is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3, or (at your option)
   any later version.

   It is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
   or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
   License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING3. If not,
   see <http://www.gnu.org/licenses/>.  */

#define STATIC_TABLE
#define DEFINE_TABLE

#include "sysdep.h"
#include "disassemble.h"
#include "libiberty.h"
#include "opintl.h"
#include <assert.h>
#include "elf-bfd.h"
#include "kvx-dis.h"

#include "elf/kvx.h"
#include "opcode/kvx.h"

#define FAIL(s) ((s) != NULL)

/* Begin synchronize with processor/kvx-family/BE/LAO/kvx-Bundle.c  */

#define KVX_PARALLEL_MASK (0x80000000)

/* Steering values for the KVX VLIW architecture.  */
typedef enum {
  Steering_BCU,
  Steering_LSU,
  Steering_MAU,
  Steering_ALU,
  Steering__
} enum_Steering;
typedef uint8_t Steering;
#define Steering_EXT Steering_MAU

/*
 * KV3_EXU enumeration.
 */
typedef enum {
  KV3_EXU_BCU,
  KV3_EXU_EXT,
  KV3_EXU_ALU0,
  KV3_EXU_ALU1,
  KV3_EXU_MAU,
  KV3_EXU_LSU,
  KV3_EXU__,
} enum_KV3_EXU;
typedef uint8_t KV3_EXU;

/*
 * KV4_EXU enumeration.
 */
typedef enum {
  KV4_EXU_BCU0,
  KV4_EXU_BCU1,
  KV4_EXU_ALU0,
  KV4_EXU_ALU1,
  KV4_EXU_LSU0,
  KV4_EXU_LSU1,
  KV4_EXU_EXT0,
  KV4_EXU_EXT1,
  KV4_EXU_EXT2,
  KV4_EXU_EXT3,
  KV4_EXU__,
} enum_KV4_EXU;
typedef uint8_t KV4_EXU;

static inline int
kvx_steering(uint32_t x)
{
  return (((x) & 0x60000000) >> 29);
}

static inline int
kvx_exu_tag(uint32_t x)
{
  return  (((x) & 0x18000000) >> 27);
}

static inline int
kvx_has_parallel_bit(uint32_t x)
{
  return (((x) & KVX_PARALLEL_MASK) == KVX_PARALLEL_MASK);
}

static inline int
kvx_is_nop_opcode(uint32_t x)
{
  return ((x)<<1) == 0xFFFFFFFE;
}

static inline int
kv3_is_tca_opcode(uint32_t x)
{
  unsigned major = ((x)>>24) & 0x1F;
  return (major > 1) && (major < 8);
}

/* End synchronize with processor/kvx-family/BE/LAO/kvx-Bundle.c  */

/* A raw instruction.  */
struct raw_insn
{
  uint32_t syllables[KVX_MAXSYLLABLES];
  short length;
  short read_size;
};

static uint32_t kvx_bundle_words[KVX_MAXBUNDLEWORDS];

static struct raw_insn kvx_bundle_insns[KVX_MAXBUNDLEISSUE];

/* An issued instruction.  */
struct issued_insn
{
  unsigned opcode;
  unsigned immx_words[2];
  bool immx_valid[2];
  char immx_count;
  char nb_syllables;
};

/* Option for "pretty printing", ie, not the usual little endian objdump output.  */
static int opt_pretty = 0;
/* Option for not emiting a new line between all bundles.  */
static int opt_compact_assembly = 0;
/* Option for displaying the conditional syntax.  */

void
parse_kvx_dis_option (const char *option)
{
  /* Try to match options that are simple flags.  */
  if (startswith (option, "pretty"))
    {
      opt_pretty = 1;
      return;
    }

  if (startswith (option, "compact-assembly"))
    {
      opt_compact_assembly = 1;
      return;
    }

  if (startswith (option, "no-compact-assembly"))
    {
      opt_compact_assembly = 0;
      return;
    }

  /* Invalid option.  */
  opcodes_error_handler (_("unrecognised disassembler option: %s"), option);
}

static void
parse_kvx_dis_options (const char *options)
{
  const char *option_end;

  if (options == NULL)
    return;

  while (*options != '\0')
    {
      /* Skip empty options.  */
      if (*options == ',')
	{
	  options++;
	  continue;
	}

      /* We know that *options is neither NUL or a comma.  */
      option_end = options + 1;
      while (*option_end != ',' && *option_end != '\0')
	option_end++;

      parse_kvx_dis_option (options);

      /* Go on to the next one.  If option_end points to a comma, it
	 will be skipped above.  */
      options = option_end;
    }
}

struct kvx_dis_env
{
  int kvx_arch_size;
  struct kvx_opc *opc_table;
  struct kvx_register *kvx_registers;
  const char ***kvx_modifiers;
  int *kvx_dec_registers;
  int *kvx_regfiles;
  unsigned int kvx_max_dec_registers;
  int initialized_p;
};

static struct kvx_dis_env env = {
  .kvx_arch_size = 0,
  .opc_table = NULL,
  .kvx_registers = NULL,
  .kvx_modifiers = NULL,
  .kvx_dec_registers = NULL,
  .kvx_regfiles = NULL,
  .initialized_p = 0,
  .kvx_max_dec_registers = 0
};

static void
kvx_dis_init (struct disassemble_info *info)
{
  env.kvx_arch_size = 32;
  switch (info->mach)
    {
    case bfd_mach_kv3_1_64:
      env.kvx_arch_size = 64;
      /* fallthrough */
    case bfd_mach_kv3_1_usr:
    case bfd_mach_kv3_1:
    default:
      env.opc_table = kv3_v1_optab;
      env.kvx_regfiles = kv3_v1_regfiles;
      env.kvx_registers = kv3_v1_registers;
      env.kvx_modifiers = kv3_v1_modifiers;
      env.kvx_dec_registers = kv3_v1_dec_registers;
      env.kvx_max_dec_registers = kv3_v1_regfiles[KV3_V1_REGFILE_DEC_REGISTERS];
      break;
    case bfd_mach_kv3_2_64:
      env.kvx_arch_size = 64;
      /* fallthrough */
    case bfd_mach_kv3_2_usr:
    case bfd_mach_kv3_2:
      env.opc_table = kv3_v2_optab;
      env.kvx_regfiles = kv3_v2_regfiles;
      env.kvx_registers = kv3_v2_registers;
      env.kvx_modifiers = kv3_v2_modifiers;
      env.kvx_dec_registers = kv3_v2_dec_registers;
      env.kvx_max_dec_registers = kv3_v2_regfiles[KV3_V2_REGFILE_DEC_REGISTERS];
      break;
    case bfd_mach_kv4_1_64:
      env.kvx_arch_size = 64;
      /* fallthrough */
    case bfd_mach_kv4_1_usr:
    case bfd_mach_kv4_1:
      env.opc_table = kv4_v1_optab;
      env.kvx_regfiles = kv4_v1_regfiles;
      env.kvx_registers = kv4_v1_registers;
      env.kvx_modifiers = kv4_v1_modifiers;
      env.kvx_dec_registers = kv4_v1_dec_registers;
      env.kvx_max_dec_registers = kv4_v1_regfiles[KV4_V1_REGFILE_DEC_REGISTERS];
      break;
    }

  if (info->disassembler_options)
    parse_kvx_dis_options (info->disassembler_options);

  env.initialized_p = 1;
}

static int
kv3_steer_bundle_insns (int word_cnt, int *_insn_cnt)
{
  /* Issue lanes taken.  */
  int bcu_taken = 0;
  int ext_taken = 0;
  int alu0_taken = 0;
  int alu1_taken = 0;
  int mau_taken = 0;
  int lsu_taken = 0;

  struct issued_insn issued_insns[KV3_EXU__];
  memset (issued_insns, 0, sizeof (issued_insns));

  if (word_cnt == 0)
    return FAIL("Bundle word count == 0");

  int index = 0;
  for (; index < word_cnt; index++)
    {
      uint32_t syllable = kvx_bundle_words[index];
      switch (kvx_steering (syllable))
	{
	case Steering_BCU:
	  /* BCU or EXT instruction.  */
	  if (index == 0)
	    {
	      if (kv3_is_tca_opcode (syllable))
		{
		  if (ext_taken)
		    return FAIL("Too many KV3_EXT instructions");

		  issued_insns[KV3_EXU_EXT].opcode = syllable;
		  issued_insns[KV3_EXU_EXT].nb_syllables = 1;
		  ext_taken = 1;
		}
	      else
		{
		  issued_insns[KV3_EXU_BCU].opcode = syllable;
		  issued_insns[KV3_EXU_BCU].nb_syllables = 1;
		  bcu_taken = 1;
		}
	    }
	  else
	    {
	      if (index == 1 && bcu_taken && kv3_is_tca_opcode (syllable))
		{
		  if (ext_taken)
		    return FAIL("Too many KV3_EXT instructions");

		  issued_insns[KV3_EXU_EXT].opcode = syllable;
		  issued_insns[KV3_EXU_EXT].nb_syllables = 1;
		  ext_taken = 1;
		}
	      else
		{
		  /* Not first syllable in bundle, IMMX.  */
		  struct issued_insn *issued_insn =
		    &(issued_insns[KV3_EXU_ALU0 + kvx_exu_tag (syllable)]);
		  int immx_count = issued_insn->immx_count;
		  if (immx_count > 1)
		    return FAIL("Too many IMMX syllables");

		  issued_insn->immx_words[immx_count] = syllable;
		  issued_insn->immx_valid[immx_count] = true;
		  issued_insn->nb_syllables++;
		  issued_insn->immx_count = immx_count + 1;
		}
	    }
	  break;

	case Steering_ALU:
	  if (alu0_taken == 0)
	    {
	      issued_insns[KV3_EXU_ALU0].opcode = syllable;
	      issued_insns[KV3_EXU_ALU0].nb_syllables = 1;
	      alu0_taken = 1;
	    }
	  else if (alu1_taken == 0)
	    {
	      issued_insns[KV3_EXU_ALU1].opcode = syllable;
	      issued_insns[KV3_EXU_ALU1].nb_syllables = 1;
	      alu1_taken = 1;
	    }
	  else if (mau_taken == 0)
	    {
	      issued_insns[KV3_EXU_MAU].opcode = syllable;
	      issued_insns[KV3_EXU_MAU].nb_syllables = 1;
	      mau_taken = 1;
	    }
	  else if (lsu_taken == 0)
	    {
	      issued_insns[KV3_EXU_LSU].opcode = syllable;
	      issued_insns[KV3_EXU_LSU].nb_syllables = 1;
	      lsu_taken = 1;
	    }
	  else if (!kvx_is_nop_opcode (syllable))
	    return FAIL("Too many ALU instructions");
	  break;

	case Steering_MAU:
	  if (mau_taken == 1)
	    return FAIL("Too many MAU instructions");

	  issued_insns[KV3_EXU_MAU].opcode = syllable;
	  issued_insns[KV3_EXU_MAU].nb_syllables = 1;
	  mau_taken = 1;
	  break;

	case Steering_LSU:
	  if (lsu_taken == 1)
	    return FAIL("Too many LSU instructions");

	  issued_insns[KV3_EXU_LSU].opcode = syllable;
	  issued_insns[KV3_EXU_LSU].nb_syllables = 1;
	  lsu_taken = 1;
	}

      if (!(kvx_has_parallel_bit (syllable)))
	break;
    }

  if (kvx_has_parallel_bit (kvx_bundle_words[index]))
    return FAIL("Bundle exceeds maximum size");

  /* Fill KVX_BUNDLE_INSNS and count read syllables.  */
  int insn_idx = 0;
  for (int exu = 0; exu < KV3_EXU__; exu++)
    {
      if (issued_insns[exu].nb_syllables)
	{
	  int syllable_idx = 0;

	  /* First copy the opcode.  */
	  kvx_bundle_insns[insn_idx].syllables[syllable_idx++] =
	      issued_insns[exu].opcode;
	  kvx_bundle_insns[insn_idx].length = 1;

	  /* Copy up to two immediate extension words.  */
	  for (int j = 0; j < 2; j++)
	    if (issued_insns[exu].immx_valid[j])
	      {
		kvx_bundle_insns[insn_idx].syllables[syllable_idx++] =
		    issued_insns[exu].immx_words[j];
		kvx_bundle_insns[insn_idx].length++;
	      }
	
	  kvx_bundle_insns[insn_idx].read_size =
	      kvx_bundle_insns[insn_idx].length * 4;

	  insn_idx++;
	}
    }

  *_insn_cnt = insn_idx;
  return 0;
}

static int
kv4_steer_bundle_insns (int word_cnt, int *_insn_cnt)
{
  /* Issue lanes in use.  */
  int bcu_inuse = 0;
  int alu_inuse = 0;
  int ext_inuse = 0;
  int lsu_inuse = 0;

  struct issued_insn issued_insns[KV4_EXU__];
  memset (issued_insns, 0, sizeof (issued_insns));

  if (word_cnt == 0)
    return FAIL("Bundle word_cnt == 0");

  int index = 0;
  for (; index < word_cnt; index++)
    {
      uint32_t syllable = kvx_bundle_words[index];
      switch (kvx_steering (syllable))
	{
	case Steering_BCU:
	  if (index == 0)
	    /* First BCU syllable in bundle issues in BCU0.  */
	    {
	      issued_insns[KV4_EXU_BCU0].opcode = syllable;
	      issued_insns[KV4_EXU_BCU0].nb_syllables = 1;
	      bcu_inuse++;
	    }
	  else if (index == 1 && bcu_inuse == 1)
	    /* Second BCU syllable in bundle following a BCU issued in BCU1.  */
	    {
	      if (kvx_exu_tag (syllable) == 0)
		/* Case of BCU with extended offset.  */
		{
		  issued_insns[KV4_EXU_BCU0].immx_words[0] = syllable;
		  issued_insns[KV4_EXU_BCU0].immx_valid[0] = true;
		  issued_insns[KV4_EXU_BCU0].immx_count = 1;
		  issued_insns[KV4_EXU_BCU0].nb_syllables = 2;
		}
	      else
		{
		  issued_insns[KV4_EXU_BCU1].opcode = syllable;
		  issued_insns[KV4_EXU_BCU1].nb_syllables = 1;
		}
	      bcu_inuse++;
	    }
	  else
	    {
	      /* Steering BCU and not first or second, must be IMMX.  */
	      int exu_tag = KV4_EXU_ALU0 + kvx_exu_tag(syllable);
	      struct issued_insn *issued_insn = &(issued_insns[exu_tag]);
	      int immx_count = issued_insn->immx_count;
	      if (immx_count > 1)
		return FAIL("Too many IMMX syllables");

	      issued_insn->immx_words[immx_count] = syllable;
	      issued_insn->immx_valid[immx_count] = true;
	      issued_insn->nb_syllables++;
	      issued_insn->immx_count = immx_count + 1;
	    }
	  break;

	case Steering_ALU:
	  if (alu_inuse == 0)
	    {
	      issued_insns[KV4_EXU_ALU0].opcode = syllable;
	      issued_insns[KV4_EXU_ALU0].nb_syllables = 1;
	      alu_inuse++;
	    }
	  else if (alu_inuse == 1)
	    {
	      issued_insns[KV4_EXU_ALU1].opcode = syllable;
	      issued_insns[KV4_EXU_ALU1].nb_syllables = 1;
	      alu_inuse++;
	    }
	  else if (lsu_inuse == 0)
	    {
	      issued_insns[KV4_EXU_LSU0].opcode = syllable;
	      issued_insns[KV4_EXU_LSU0].nb_syllables = 1;
	      lsu_inuse++;
	    }
	  else if (lsu_inuse == 1)
	    {
	      issued_insns[KV4_EXU_LSU1].opcode = syllable;
	      issued_insns[KV4_EXU_LSU1].nb_syllables = 1;
	      lsu_inuse++;
	    }
	  else
	    return FAIL("Too many ALU instructions");

	  break;

	case Steering_LSU:
	  if (lsu_inuse == 0)
	    {
	      issued_insns[KV4_EXU_LSU0].opcode = syllable;
	      issued_insns[KV4_EXU_LSU0].nb_syllables = 1;
	      lsu_inuse++;
	    }
	  else if (lsu_inuse == 1)
	    {
	      issued_insns[KV4_EXU_LSU1].opcode = syllable;
	      issued_insns[KV4_EXU_LSU1].nb_syllables = 1;
	      lsu_inuse++;
	    }
	  else
	    return FAIL("Too many LSU instructions");

	  break;

	case Steering_EXT:
	  if (ext_inuse == 0)
	    {
	      issued_insns[KV4_EXU_EXT0].opcode = syllable;
	      issued_insns[KV4_EXU_EXT0].nb_syllables = 1;
	      ext_inuse++;
	    }
	  else if (ext_inuse == 1)
	    {
	      issued_insns[KV4_EXU_EXT1].opcode = syllable;
	      issued_insns[KV4_EXU_EXT1].nb_syllables = 1;
	      ext_inuse++;
	    }
	  else if (ext_inuse == 2)
	    {
	      issued_insns[KV4_EXU_EXT2].opcode = syllable;
	      issued_insns[KV4_EXU_EXT2].nb_syllables = 1;
	      ext_inuse++;
	    }
	  else if (ext_inuse == 3)
	    {
	      issued_insns[KV4_EXU_EXT3].opcode = syllable;
	      issued_insns[KV4_EXU_EXT3].nb_syllables = 1;
	      ext_inuse++;
	    }
	  else
	    return FAIL("Too many KV4_EXT instructions");

	  break;
	}

      if (!(kvx_has_parallel_bit (syllable)))
	break;

    }

  if (kvx_has_parallel_bit (kvx_bundle_words[index]))
    return FAIL("Bundle exceeds maximum size");

  /* Fill KVX_BUNDLE_INSNS and count read syllables.  */
  int insn_idx = 0;
  for (int exu = 0; exu < KV4_EXU__; exu++)
    {
      if (issued_insns[exu].nb_syllables)
	{
	  int syllable_idx = 0;

	  /* First copy the opcode.  */
	  kvx_bundle_insns[insn_idx].syllables[syllable_idx++] =
	      issued_insns[exu].opcode;
	  kvx_bundle_insns[insn_idx].length = 1;

	  /* Copy up to two immediate extension words.  */
	  for (int j = 0; j < 2; j++)
	    if (issued_insns[exu].immx_valid[j])
	      {
		kvx_bundle_insns[insn_idx].syllables[syllable_idx++] =
		    issued_insns[exu].immx_words[j];
		kvx_bundle_insns[insn_idx].length++;
	      }

	  kvx_bundle_insns[insn_idx].read_size =
	      kvx_bundle_insns[insn_idx].length * 4;

	  insn_idx++;
	}
    }

  *_insn_cnt = insn_idx;
  return 0;
}

static int
kvx_steer_bundle_insns (struct disassemble_info *info,
		       int word_cnt, int *_insn_cnt)
{
  switch (info->mach)
    {
    case bfd_mach_kv3_1_64:
    case bfd_mach_kv3_1_usr:
    case bfd_mach_kv3_1:
    case bfd_mach_kv3_2_64:
    case bfd_mach_kv3_2_usr:
    case bfd_mach_kv3_2:
    default:
      return kv3_steer_bundle_insns (word_cnt, _insn_cnt);
      break;
    case bfd_mach_kv4_1_64:
    case bfd_mach_kv4_1_usr:
    case bfd_mach_kv4_1:
      return kv4_steer_bundle_insns (word_cnt, _insn_cnt);
      break;
    }
  return FAIL("Unknown machine architecture.");
}

struct decoded_insn
{
  /* The entry in the opc_table.  */
  struct kvx_opc *opc;
  /* The number of operands.  */
  int nb_ops;
  /* The operand type.  */
  struct
  {
    enum
    {
      CAT_REGISTER,
      CAT_MODIFIER,
      CAT_IMMEDIATE,
    } type;
    /* The operand value.  */
    uint64_t val;
    /* If it is an immediate, its sign.  */
    int sign;
    /* If it is an immediate, is it pc relative.  */
    int pcrel;
    /* The bit width of the operand.  */
    int width;
    /* If it is a modifier, the modifier category.
       An index in the modifier table.  */
    int mod_idx;
  } operands[KVX_MAXOPERANDS];
};

static int
decode_insn (bfd_vma memaddr, struct raw_insn *raw_insn, struct decoded_insn *res)
{

  unsigned kvx_opcode_keep_flags = env.kvx_arch_size == 32 ?
				    KVX_OPCODE_FLAG_MODE32 :
				    KVX_OPCODE_FLAG_MODE64;
  unsigned kvx_opcode_skip_flags = KVX_OPCODE_FLAG_RISCV;

  int found = 0;
  int idx = 0;
  for (struct kvx_opc *opc = env.opc_table;
       opc->as_op && (((char) opc->as_op[0]) != 0); opc++)
    {
      if (!(opc->codewords[0].flags & kvx_opcode_keep_flags)
	  || (opc->codewords[0].flags & kvx_opcode_skip_flags))
	continue;

      if (opc->wordcount != raw_insn->length)
	continue;

      int opcode_match = 1;
      for (int i = 0; i < opc->wordcount; i++)
	if ((opc->codewords[i].mask & raw_insn->syllables[i]) !=
	    opc->codewords[i].opcode)
	  opcode_match = 0;

      if (opcode_match)
	{
	  res->opc = opc;

	  for (int i = 0; opc->format[i]; i++)
	    {
	      struct kvx_bitfield *bf = opc->format[i]->bfield;
	      int bf_nb = opc->format[i]->bitfields;
	      int width = opc->format[i]->width;
	      int type = opc->format[i]->type;
	      const char *type_name = opc->format[i]->tname;
	      int flags = opc->format[i]->flags;
	      int shift = opc->format[i]->shift;
	      int bias = opc->format[i]->bias;
	      uint64_t value = 0;

	      for (int bf_idx = 0; bf_idx < bf_nb; bf_idx++)
		{
		  int insn_idx = (int) bf[bf_idx].to_offset / 32;
		  int to_offset = bf[bf_idx].to_offset % 32;
		  uint64_t encoded_value =
		    raw_insn->syllables[insn_idx] >> to_offset;
		  encoded_value &= (1LL << bf[bf_idx].size) - 1;
		  value |= encoded_value << bf[bf_idx].from_offset;
		}
	      if (flags & KVX_OPERAND_SIGNED)
		{
		  uint64_t signbit = 1LL << (width - 1);
		  value = (value ^ signbit) - signbit;
		}
	      value = (value << shift) + bias;

#define KVX_PRINT_REG(regfile,value) \
    if(env.kvx_regfiles[regfile]+value < env.kvx_max_dec_registers) { \
	res->operands[idx].val = env.kvx_dec_registers[env.kvx_regfiles[regfile]+value]; \
	res->operands[idx].type = CAT_REGISTER; \
	idx++; \
    } else { \
	res->operands[idx].val = ~0; \
	res->operands[idx].type = CAT_REGISTER; \
	idx++; \
    }

	      if (env.opc_table == kv3_v1_optab)
		{
		  switch (type)
		    {
		    case RegClass_kv3_v1_singleReg:
		      KVX_PRINT_REG (KV3_V1_REGFILE_DEC_GPR, value)
		      break;
		    case RegClass_kv3_v1_pairedReg:
		      KVX_PRINT_REG (KV3_V1_REGFILE_DEC_PGR, value)
		      break;
		    case RegClass_kv3_v1_quadReg:
		      KVX_PRINT_REG (KV3_V1_REGFILE_DEC_QGR, value)
		      break;
		    case RegClass_kv3_v1_systemReg:
		    case RegClass_kv3_v1_aloneReg:
		    case RegClass_kv3_v1_onlyraReg:
		    case RegClass_kv3_v1_onlygetReg:
		    case RegClass_kv3_v1_onlysetReg:
		    case RegClass_kv3_v1_onlyfxReg:
		    case RegClass_kv3_v1_onlyswapReg:
		      KVX_PRINT_REG (KV3_V1_REGFILE_DEC_SFR, value)
		      break;
		    case RegClass_kv3_v1_xworddReg0M4:
		    case RegClass_kv3_v1_xworddReg1M4:
		    case RegClass_kv3_v1_xworddReg2M4:
		    case RegClass_kv3_v1_xworddReg3M4:
		      KVX_PRINT_REG (KV3_V1_REGFILE_DEC_XCR, value)
		      break;
		    case RegClass_kv3_v1_xwordqRegE:
		    case RegClass_kv3_v1_xwordqRegO:
		    case RegClass_kv3_v1_xwordqReg0M4:
		    case RegClass_kv3_v1_xwordqReg1M4:
		    case RegClass_kv3_v1_xwordqReg2M4:
		    case RegClass_kv3_v1_xwordqReg3M4:
		      KVX_PRINT_REG (KV3_V1_REGFILE_DEC_XBR, value)
		      break;
		    case RegClass_kv3_v1_xwordoReg:
		    case RegClass_kv3_v1_xwordoRegE:
		    case RegClass_kv3_v1_xwordoRegO:
		      KVX_PRINT_REG (KV3_V1_REGFILE_DEC_XVR, value)
		      break;
		    case RegClass_kv3_v1_xwordxReg:
		      KVX_PRINT_REG (KV3_V1_REGFILE_DEC_XTR, value)
		      break;
		    case RegClass_kv3_v1_xwordvReg:
		      KVX_PRINT_REG (KV3_V1_REGFILE_DEC_XMR, value)
		      break;
		    case Immediate_kv3_v1_sysnumber:
		    case Immediate_kv3_v1_signed10:
		    case Immediate_kv3_v1_signed16:
		    case Immediate_kv3_v1_signed27:
		    case Immediate_kv3_v1_wrapped32:
		    case Immediate_kv3_v1_signed37:
		    case Immediate_kv3_v1_signed43:
		    case Immediate_kv3_v1_signed54:
		    case Immediate_kv3_v1_wrapped64:
		    case Immediate_kv3_v1_unsigned6:
		      res->operands[idx].val = value;
		      res->operands[idx].sign = flags & KVX_OPERAND_SIGNED;
		      res->operands[idx].width = width;
		      res->operands[idx].type = CAT_IMMEDIATE;
		      res->operands[idx].pcrel = 0;
		      idx++;
		      break;
		    case Immediate_kv3_v1_pcrel17:
		    case Immediate_kv3_v1_pcrel27:
		      res->operands[idx].val = value + memaddr;
		      res->operands[idx].sign = flags & KVX_OPERAND_SIGNED;
		      res->operands[idx].width = width;
		      res->operands[idx].type = CAT_IMMEDIATE;
		      res->operands[idx].pcrel = 1;
		      idx++;
		      break;
		    case Modifier_kv3_v1_column:
		    case Modifier_kv3_v1_comparison:
		    case Modifier_kv3_v1_doscale:
		    case Modifier_kv3_v1_exunum:
		    case Modifier_kv3_v1_floatcomp:
		    case Modifier_kv3_v1_qindex:
		    case Modifier_kv3_v1_rectify:
		    case Modifier_kv3_v1_rounding:
		    case Modifier_kv3_v1_roundint:
		    case Modifier_kv3_v1_saturate:
		    case Modifier_kv3_v1_scalarcond:
		    case Modifier_kv3_v1_silent:
		    case Modifier_kv3_v1_simdcond:
		    case Modifier_kv3_v1_speculate:
		    case Modifier_kv3_v1_splat32:
		    case Modifier_kv3_v1_variant:
		      {
			int sz = 0;
			int mod_idx = type - Modifier_kv3_v1_column;
			for (sz = 0; env.kvx_modifiers[mod_idx][sz]; ++sz);
			const char *mod = value < (unsigned) sz
			  ? env.kvx_modifiers[mod_idx][value] : NULL;
			if (!mod) goto retry;
			res->operands[idx].val = value;
			res->operands[idx].type = CAT_MODIFIER;
			res->operands[idx].mod_idx = mod_idx;
			idx++;
		      }
		      break;
		    default:
		      fprintf (stderr, "error: unexpected operand type (%s)\n",
			       type_name);
		      exit (-1);
		    };
		}
	      else if (env.opc_table == kv3_v2_optab)
		{
		  switch (type)
		    {
		    case RegClass_kv3_v2_singleReg:
		      KVX_PRINT_REG (KV3_V2_REGFILE_DEC_GPR, value)
		      break;
		    case RegClass_kv3_v2_pairedReg:
		      KVX_PRINT_REG (KV3_V2_REGFILE_DEC_PGR, value)
		      break;
		    case RegClass_kv3_v2_quadReg:
		      KVX_PRINT_REG (KV3_V2_REGFILE_DEC_QGR, value)
		      break;
		    case RegClass_kv3_v2_systemReg:
		    case RegClass_kv3_v2_aloneReg:
		    case RegClass_kv3_v2_onlyraReg:
		    case RegClass_kv3_v2_onlygetReg:
		    case RegClass_kv3_v2_onlysetReg:
		    case RegClass_kv3_v2_onlyfxReg:
		    case RegClass_kv3_v2_onlyswapReg:
		      KVX_PRINT_REG (KV3_V2_REGFILE_DEC_SFR, value)
		      break;
		    case RegClass_kv3_v2_xworddReg:
		    case RegClass_kv3_v2_xworddReg0M4:
		    case RegClass_kv3_v2_xworddReg1M4:
		    case RegClass_kv3_v2_xworddReg2M4:
		    case RegClass_kv3_v2_xworddReg3M4:
		      KVX_PRINT_REG (KV3_V2_REGFILE_DEC_XCR, value)
		      break;
		    case RegClass_kv3_v2_xwordqReg:
		    case RegClass_kv3_v2_xwordqRegE:
		    case RegClass_kv3_v2_xwordqRegO:
		      KVX_PRINT_REG (KV3_V2_REGFILE_DEC_XBR, value)
		      break;
		    case RegClass_kv3_v2_xwordoReg:
		      KVX_PRINT_REG (KV3_V2_REGFILE_DEC_XVR, value)
		      break;
		    case RegClass_kv3_v2_xwordxReg:
		      KVX_PRINT_REG (KV3_V2_REGFILE_DEC_XTR, value)
		      break;
		    case RegClass_kv3_v2_xwordvReg:
		      KVX_PRINT_REG (KV3_V2_REGFILE_DEC_XMR, value)
		      break;
		    case RegClass_kv3_v2_buffer2Reg:
		      KVX_PRINT_REG (KV3_V2_REGFILE_DEC_X2R, value)
		      break;
		    case RegClass_kv3_v2_buffer4Reg:
		      KVX_PRINT_REG (KV3_V2_REGFILE_DEC_X4R, value)
		      break;
		    case RegClass_kv3_v2_buffer8Reg:
		      KVX_PRINT_REG (KV3_V2_REGFILE_DEC_X8R, value)
		      break;
		    case RegClass_kv3_v2_buffer16Reg:
		      KVX_PRINT_REG (KV3_V2_REGFILE_DEC_X16R, value)
		      break;
		    case RegClass_kv3_v2_buffer32Reg:
		      KVX_PRINT_REG (KV3_V2_REGFILE_DEC_X32R, value)
		      break;
		    case RegClass_kv3_v2_buffer64Reg:
		      KVX_PRINT_REG (KV3_V2_REGFILE_DEC_X64R, value)
		      break;
		    case Immediate_kv3_v2_brknumber:
		    case Immediate_kv3_v2_sysnumber:
		    case Immediate_kv3_v2_signed10:
		    case Immediate_kv3_v2_signed16:
		    case Immediate_kv3_v2_signed27:
		    case Immediate_kv3_v2_wrapped32:
		    case Immediate_kv3_v2_signed37:
		    case Immediate_kv3_v2_signed43:
		    case Immediate_kv3_v2_signed54:
		    case Immediate_kv3_v2_wrapped64:
		    case Immediate_kv3_v2_unsigned6:
		      res->operands[idx].val = value;
		      res->operands[idx].sign = flags & KVX_OPERAND_SIGNED;
		      res->operands[idx].width = width;
		      res->operands[idx].type = CAT_IMMEDIATE;
		      res->operands[idx].pcrel = 0;
		      idx++;
		      break;
		    case Immediate_kv3_v2_pcrel27:
		    case Immediate_kv3_v2_pcrel17:
		      res->operands[idx].val = value + memaddr;
		      res->operands[idx].sign = flags & KVX_OPERAND_SIGNED;
		      res->operands[idx].width = width;
		      res->operands[idx].type = CAT_IMMEDIATE;
		      res->operands[idx].pcrel = 1;
		      idx++;
		      break;
		    case Modifier_kv3_v2_accesses:
		    case Modifier_kv3_v2_boolcas:
		    case Modifier_kv3_v2_cachelev:
		    case Modifier_kv3_v2_channel:
		    case Modifier_kv3_v2_coherency:
		    case Modifier_kv3_v2_comparison:
		    case Modifier_kv3_v2_conjugate:
		    case Modifier_kv3_v2_doscale:
		    case Modifier_kv3_v2_exunum:
		    case Modifier_kv3_v2_floatcomp:
		    case Modifier_kv3_v2_hindex:
		    case Modifier_kv3_v2_lsomask:
		    case Modifier_kv3_v2_lsumask:
		    case Modifier_kv3_v2_qindex:
		    case Modifier_kv3_v2_rounding:
		    case Modifier_kv3_v2_scalarcond:
		    case Modifier_kv3_v2_shuffleV:
		    case Modifier_kv3_v2_shuffleX:
		    case Modifier_kv3_v2_silent:
		    case Modifier_kv3_v2_simdcond:
		    case Modifier_kv3_v2_speculate:
		    case Modifier_kv3_v2_splat32:
		    case Modifier_kv3_v2_transpose:
		    case Modifier_kv3_v2_variant:
		      {
			int sz = 0;
			int mod_idx = type - Modifier_kv3_v2_accesses;
			for (sz = 0; env.kvx_modifiers[mod_idx][sz];
			     ++sz);
			const char *mod = value < (unsigned) sz
			  ? env.kvx_modifiers[mod_idx][value] : NULL;
			if (!mod) goto retry;
			res->operands[idx].val = value;
			res->operands[idx].type = CAT_MODIFIER;
			res->operands[idx].mod_idx = mod_idx;
			idx++;
		      };
		      break;
		    default:
		      fprintf (stderr, "error: unexpected operand type (%s)\n",
			       type_name);
		      exit (-1);
		    };
		}
	      else if (env.opc_table == kv4_v1_optab)
		{
		  switch (type)
		    {
		    case RegClass_kv4_v1_singleReg:
		      KVX_PRINT_REG (KV4_V1_REGFILE_DEC_GPR, value)
		      break;
		    case RegClass_kv4_v1_pairedReg:
		      KVX_PRINT_REG (KV4_V1_REGFILE_DEC_PGR, value)
		      break;
		    case RegClass_kv4_v1_quadReg:
		      KVX_PRINT_REG (KV4_V1_REGFILE_DEC_QGR, value)
		      break;
		    case RegClass_kv4_v1_systemReg:
		    case RegClass_kv4_v1_aloneReg:
		    case RegClass_kv4_v1_onlyraReg:
		    case RegClass_kv4_v1_onlygetReg:
		    case RegClass_kv4_v1_onlysetReg:
		    case RegClass_kv4_v1_onlyfxReg:
		    case RegClass_kv4_v1_onlyswapReg:
		      KVX_PRINT_REG (KV4_V1_REGFILE_DEC_SFR, value)
		      break;
		    case RegClass_kv4_v1_xworddReg:
		    case RegClass_kv4_v1_xworddReg0M4:
		    case RegClass_kv4_v1_xworddReg1M4:
		    case RegClass_kv4_v1_xworddReg2M4:
		    case RegClass_kv4_v1_xworddReg3M4:
		      KVX_PRINT_REG (KV4_V1_REGFILE_DEC_XCR, value)
		      break;
		    case RegClass_kv4_v1_xwordqReg:
		    case RegClass_kv4_v1_xwordqRegE:
		    case RegClass_kv4_v1_xwordqRegO:
		      KVX_PRINT_REG (KV4_V1_REGFILE_DEC_XBR, value)
		      break;
		    case RegClass_kv4_v1_xwordoReg:
		      KVX_PRINT_REG (KV4_V1_REGFILE_DEC_XVR, value)
		      break;
		    case RegClass_kv4_v1_xwordxReg:
		      KVX_PRINT_REG (KV4_V1_REGFILE_DEC_XTR, value)
		      break;
		    case RegClass_kv4_v1_xwordvReg:
		      KVX_PRINT_REG (KV4_V1_REGFILE_DEC_XMR, value)
		      break;
		    case RegClass_kv4_v1_buffer2Reg:
		      KVX_PRINT_REG (KV4_V1_REGFILE_DEC_X2R, value)
		      break;
		    case RegClass_kv4_v1_buffer4Reg:
		      KVX_PRINT_REG (KV4_V1_REGFILE_DEC_X4R, value)
		      break;
		    case RegClass_kv4_v1_buffer8Reg:
		      KVX_PRINT_REG (KV4_V1_REGFILE_DEC_X8R, value)
		      break;
		    case RegClass_kv4_v1_buffer16Reg:
		      KVX_PRINT_REG (KV4_V1_REGFILE_DEC_X16R, value)
		      break;
		    case RegClass_kv4_v1_buffer32Reg:
		      KVX_PRINT_REG (KV4_V1_REGFILE_DEC_X32R, value)
		      break;
		    case RegClass_kv4_v1_buffer64Reg:
		      KVX_PRINT_REG (KV4_V1_REGFILE_DEC_X64R, value)
		      break;
		    case RegClass_kv4_v1_mainReg:
		      KVX_PRINT_REG (KV4_V1_REGFILE_FIRST_RV_BIR, value)
		      break;
		    case RegClass_kv4_v1_floatReg:
		      KVX_PRINT_REG (KV4_V1_REGFILE_DEC_RV_FPR, value)
		      break;
		    case Immediate_kv4_v1_brknumber:
		    case Immediate_kv4_v1_sysnumber:
		    case Immediate_kv4_v1_signed10:
		    case Immediate_kv4_v1_signed12:
		    case Immediate_kv4_v1_signed16:
		    case Immediate_kv4_v1_signed20:
		    case Immediate_kv4_v1_signed27:
		    case Immediate_kv4_v1_wrapped32:
		    case Immediate_kv4_v1_signed37:
		    case Immediate_kv4_v1_signed43:
		    case Immediate_kv4_v1_signed54:
		    case Immediate_kv4_v1_wrapped64:
		    case Immediate_kv4_v1_unsigned6:
		      res->operands[idx].val = value;
		      res->operands[idx].sign = flags & KVX_OPERAND_SIGNED;
		      res->operands[idx].width = width;
		      res->operands[idx].type = CAT_IMMEDIATE;
		      res->operands[idx].pcrel = 0;
		      idx++;
		      break;
		    case Immediate_kv4_v1_pcrel12s1:
		    case Immediate_kv4_v1_pcrel11s2:
		    case Immediate_kv4_v1_pcrel17s2:
		    case Immediate_kv4_v1_pcrel20s1:
		    case Immediate_kv4_v1_pcrel27s2:
		    case Immediate_kv4_v1_pcrel38s2:
		    case Immediate_kv4_v1_pcrel44s2:
		    case Immediate_kv4_v1_pcrel54s2:
		      res->operands[idx].val = value + memaddr;
		      res->operands[idx].sign = flags & KVX_OPERAND_SIGNED;
		      res->operands[idx].width = width;
		      res->operands[idx].type = CAT_IMMEDIATE;
		      res->operands[idx].pcrel = 1;
		      idx++;
		      break;
		    case Modifier_kv4_v1_accesses:
		    case Modifier_kv4_v1_boolcas:
		    case Modifier_kv4_v1_cachelev:
		    case Modifier_kv4_v1_channel:
		    case Modifier_kv4_v1_coherency:
		    case Modifier_kv4_v1_conjugate:
		    case Modifier_kv4_v1_realimag:
		    case Modifier_kv4_v1_doscale:
		    case Modifier_kv4_v1_exunum:
		    case Modifier_kv4_v1_floatcomp:
		    case Modifier_kv4_v1_hindex:
		    case Modifier_kv4_v1_qindex:
		    case Modifier_kv4_v1_floatmode:
		    case Modifier_kv4_v1_ccbcomp:
		    case Modifier_kv4_v1_intcomp:
		    case Modifier_kv4_v1_bcucond:
		    case Modifier_kv4_v1_shuffleV:
		    case Modifier_kv4_v1_shuffleX:
		    case Modifier_kv4_v1_lanecond:
		    case Modifier_kv4_v1_speculate:
		    case Modifier_kv4_v1_splat32:
		    case Modifier_kv4_v1_variant:
		    case Modifier_kv4_v1_lanetodo:
		    case Modifier_kv4_v1_lanesize:
		    case Modifier_kv4_v1_signextw:
		      {
			int sz = 0;
			int mod_idx = type - Modifier_kv4_v1_accesses;
			for (sz = 0; env.kvx_modifiers[mod_idx][sz]; ++sz);
			const char *mod = value < (unsigned) sz
			  ? env.kvx_modifiers[mod_idx][value] : NULL;
			if (!mod) goto retry;
			res->operands[idx].val = value;
			res->operands[idx].type = CAT_MODIFIER;
			res->operands[idx].mod_idx = mod_idx;
			idx++;
		      }
		      break;
		    default:
		      fprintf (stderr, "error: unexpected operand type (%s)\n",
			       type_name);
		      exit (-1);
		    };
		}

#undef KVX_PRINT_REG
	    }

	  found = 1;
	  break;
	retry:;
	  idx = 0;
	  continue;
	}
    }
  res->nb_ops = idx;
  return found;
}

int
print_insn_kvx (bfd_vma memaddr, struct disassemble_info *info)
{
  static int insn_idx = 0;
  static int insn_cnt = 0;
  struct raw_insn *raw_insn;
  int readsofar = 0;
  int found = 0;
  int invalid_bundle = 0;

  if (!env.initialized_p)
    kvx_dis_init (info);

  /* Clear instruction information field.  */
  info->insn_info_valid = 0;
  info->branch_delay_insns = 0;
  info->data_size = 0;
  info->insn_type = dis_noninsn;
  info->target = 0;
  info->target2 = 0;

  /* Set line length.  */
  info->bytes_per_line = 16;


  /* If this is the beginning of the bundle, read BUNDLESIZE words and
     issue insns into KVX_BUNDLE_INSNS[].  */
  if (insn_idx == 0)
    {
      int word_cnt = 0;
      do
	{
	  int status;
	  assert (word_cnt < KVX_MAXBUNDLEWORDS);
	  status =
	    (*info->read_memory_func) (memaddr + 4 * word_cnt,
				       (bfd_byte *) (kvx_bundle_words +
						     word_cnt), 4, info);
	  if (status != 0)
	    {
	      (*info->memory_error_func) (status, memaddr + 4 * word_cnt,
					  info);
	      return -1;
	    }
	  word_cnt++;
	}
      while (kvx_has_parallel_bit (kvx_bundle_words[word_cnt - 1])
	     && word_cnt < KVX_MAXBUNDLEWORDS - 1);
      invalid_bundle = kvx_steer_bundle_insns (info, word_cnt, &insn_cnt);
    }

  assert (insn_idx < KVX_MAXBUNDLEISSUE);
  raw_insn = &(kvx_bundle_insns[insn_idx]);
  readsofar = raw_insn->read_size;
  insn_idx++;

  if (opt_pretty)
    {
      (*info->fprintf_func) (info->stream, "[ ");
      for (int i = 0; i < raw_insn->length; i++)
	(*info->fprintf_func) (info->stream, "%08x ", raw_insn->syllables[i]);
      (*info->fprintf_func) (info->stream, "] ");
    }

  struct decoded_insn dec;
  memset (&dec, 0, sizeof dec);
  if (!invalid_bundle && (found = decode_insn (memaddr, raw_insn, &dec)))
    {
      int ch;
      (*info->fprintf_func) (info->stream, "%s", dec.opc->as_op);
      const char *fmtp = dec.opc->fmtstring;
      for (int i = 0; i < dec.nb_ops; ++i)
	{
	  /* Print characters in the format string up to the following % or nul.  */
	  while ((ch = *fmtp) && ch != '%')
	    {
	      (*info->fprintf_func) (info->stream, "%c", ch);
	      fmtp++;
	    }

	  /* Skip past %s.  */
	  if (ch == '%')
	    {
	      ch = *fmtp++;
	      fmtp++;
	    }

	  switch (dec.operands[i].type)
	    {
	    case CAT_REGISTER:
	      (*info->fprintf_func) (info->stream, "%s",
				     env.kvx_registers[dec.operands[i].val].name);
	      break;
	    case CAT_MODIFIER:
	      {
		const char *mod = env.kvx_modifiers[dec.operands[i].mod_idx][dec.operands[i].val];
		(*info->fprintf_func) (info->stream, "%s", !mod || !strcmp (mod, ".") ? "" : mod);
	      }
	      break;
	    case CAT_IMMEDIATE:
	      {
		if (dec.operands[i].pcrel)
		  {
		    /* Fill in instruction information.  */
		    info->insn_info_valid = 1;
		    info->insn_type =
		      dec.operands[i].width ==
		      17 ? dis_condbranch : dis_branch;
		    info->target = dec.operands[i].val - 4 * (insn_idx - 1);

		    info->print_address_func (info->target, info);
		  }
		else if (dec.operands[i].sign)
		  {
		    if (dec.operands[i].width <= 32)
		      {
			(*info->fprintf_func) (info->stream, "%" PRId32 " (0x%" PRIx32 ")",
					       (int32_t) dec.operands[i].val,
					       (int32_t) dec.operands[i].val);
		      }
		    else
		      {
			(*info->fprintf_func) (info->stream, "%" PRId64 " (0x%" PRIx64 ")",
					       dec.operands[i].val,
					       dec.operands[i].val);
		      }
		  }
		else
		  {
		    if (dec.operands[i].width <= 32)
		      {
			(*info->fprintf_func) (info->stream, "%" PRIu32 " (0x%" PRIx32 ")",
					       (uint32_t) dec.operands[i].
					       val,
					       (uint32_t) dec.operands[i].
					       val);
		      }
		    else
		      {
			(*info->fprintf_func) (info->stream, "%" PRIu64 " (0x%" PRIx64 ")",
					       (uint64_t) dec.
					       operands[i].val,
					       (uint64_t) dec.
					       operands[i].val);
		      }
		  }
	      }
	      break;

	    default:
	      break;
	    }
	}

      while ((ch = *fmtp))
	{
	  (*info->fprintf_styled_func) (info->stream, dis_style_text, "%c",
					ch);
	  fmtp++;
	}
    }
  else
    {
      (*info->fprintf_func) (info->stream, "*** invalid opcode ***\n");
      insn_idx = 0;
      readsofar = 4;
    }

  if (found && (insn_idx == insn_cnt))
    {
      (*info->fprintf_func) (info->stream, ";;");
      if (!opt_compact_assembly)
	(*info->fprintf_func) (info->stream, "\n");
      insn_idx = 0;
    }

  return readsofar;
}

/* This function searches in the current bundle for the instructions required
   by unwinding. For prologue:
     (1) addd $r12 = $r12, <res_stack>
     (2) get <gpr_ra_reg> = $ra
     (3) sd <ofs>[$r12] = <gpr_ra_reg> or sq/so containing <gpr_ra_reg>
     (4) sd <ofs>[$r12] = $r14 or sq/so containing r14
     (5) addd $r14 = $r12, <fp_ofs> or copyd $r14 = $r12
	 The only difference seen between the code generated by gcc and clang
	 is the setting/resetting r14. gcc could also generate copyd $r14=$r12
	 instead of add addd $r14 = $r12, <ofs> when <ofs> is 0.
	 Vice-versa, <ofs> is not guaranteed to be 0 for clang, so, clang
	 could also generate addd instead of copyd
     (6) call, icall, goto, igoto, cb., ret
  For epilogue:
     (1) addd $r12 = $r12, <res_stack>
     (2) addd $r12 = $r14, <offset> or copyd $r12 = $r14
	 Same comment as prologue (5).
     (3) ret, goto
     (4) call, icall, igoto, cb.  */

int
decode_prologue_epilogue_bundle (bfd_vma memaddr,
				 struct disassemble_info *info,
				 struct kvx_prologue_epilogue_bundle *peb)
{
  int i, nb_insn, nb_syl;

  peb->nb_insn = 0;

  if (info->arch != bfd_arch_kvx)
    return -1;

  if (!env.initialized_p)
    kvx_dis_init (info);

  /* Read the bundle.  */
  nb_syl = 0;
  do
    {
      if (nb_syl >= KVX_MAXBUNDLEWORDS)
	return -1;
      if ((*info->read_memory_func) (memaddr + 4 * nb_syl,
				     (bfd_byte *) &kvx_bundle_words[nb_syl], 4,
				     info))
	return -1;
      nb_syl++;
    }
  while (kvx_has_parallel_bit (kvx_bundle_words[nb_syl - 1])
	 && nb_syl < KVX_MAXBUNDLEWORDS - 1);
  if (kvx_steer_bundle_insns (info, nb_syl, &nb_insn))
    return -1;

  /* Check for extension to right if this is not the end of bundle
     find the format of this raw_insn.  */
  for (int idx_insn = 0; idx_insn < nb_insn; idx_insn++)
    {
      struct raw_insn *raw_insn = &kvx_bundle_insns[idx_insn];
      int is_add = 0, is_get = 0, is_a_peb_insn = 0, is_copyd = 0;

      struct decoded_insn dec;
      memset (&dec, 0, sizeof dec);
      if (!decode_insn (memaddr, raw_insn, &dec))
	continue;

      const char *op_name = dec.opc->as_op;
      struct kvx_prologue_epilogue_insn *crt_peb_insn;

      crt_peb_insn = &peb->insn[peb->nb_insn];
      crt_peb_insn->nb_gprs = 0;

      if (!strcmp (op_name, "addd"))
	is_add = 1;
      else if (!strcmp (op_name, "copyd"))
	is_copyd = 1;
      else if (!strcmp (op_name, "get"))
	is_get = 1;
      else if (!strcmp (op_name, "sd"))
	{
	  crt_peb_insn->insn_type = KVX_PROL_EPIL_INSN_SD;
	  is_a_peb_insn = 1;
	}
      else if (!strcmp (op_name, "sq"))
	{
	  crt_peb_insn->insn_type = KVX_PROL_EPIL_INSN_SQ;
	  is_a_peb_insn = 1;
	}
      else if (!strcmp (op_name, "so"))
	{
	  crt_peb_insn->insn_type = KVX_PROL_EPIL_INSN_SO;
	  is_a_peb_insn = 1;
	}
      else if (!strcmp (op_name, "ret"))
	{
	  crt_peb_insn->insn_type = KVX_PROL_EPIL_INSN_RET;
	  is_a_peb_insn = 1;
	}
      else if (!strcmp (op_name, "goto"))
	{
	  crt_peb_insn->insn_type = KVX_PROL_EPIL_INSN_GOTO;
	  is_a_peb_insn = 1;
	}
      else if (!strcmp (op_name, "igoto"))
	{
	  crt_peb_insn->insn_type = KVX_PROL_EPIL_INSN_IGOTO;
	  is_a_peb_insn = 1;
	}
      else if (!strcmp (op_name, "call") || !strcmp (op_name, "icall"))
	{
	  crt_peb_insn->insn_type = KVX_PROL_EPIL_INSN_CALL;
	  is_a_peb_insn = 1;
	}
      else if (!strncmp (op_name, "cb", 2))
	{
	  crt_peb_insn->insn_type = KVX_PROL_EPIL_INSN_CB;
	  is_a_peb_insn = 1;
	}
      else
	continue;

      for (i = 0; dec.opc->format[i]; i++)
	{
	  struct kvx_operand *fmt = dec.opc->format[i];
	  struct kvx_bitfield *bf = fmt->bfield;
	  int bf_nb = fmt->bitfields;
	  int width = fmt->width;
	  int type = fmt->type;
	  int flags = fmt->flags;
	  int shift = fmt->shift;
	  int bias = fmt->bias;
	  uint64_t encoded_value, value = 0;

	  for (int bf_idx = 0; bf_idx < bf_nb; bf_idx++)
	    {
	      int insn_idx = (int) bf[bf_idx].to_offset / 32;
	      int to_offset = bf[bf_idx].to_offset % 32;
	      encoded_value = raw_insn->syllables[insn_idx] >> to_offset;
	      encoded_value &= (1LL << bf[bf_idx].size) - 1;
	      value |= encoded_value << bf[bf_idx].from_offset;
	    }
	  if (flags & KVX_OPERAND_SIGNED)
	    {
	      uint64_t signbit = 1LL << (width - 1);
	      value = (value ^ signbit) - signbit;
	    }
	  value = (value << shift) + bias;

#define chk_type(core_, val_) \
      (env.opc_table == core_ ##_optab && type == (val_))

	  if (   chk_type (kv3_v1, RegClass_kv3_v1_singleReg)
	      || chk_type (kv3_v2, RegClass_kv3_v2_singleReg)
	      || chk_type (kv4_v1, RegClass_kv4_v1_singleReg))
	    {
	      if (env.kvx_regfiles[KVX_REGFILE_DEC_GPR] + value
		  >= env.kvx_max_dec_registers)
		return -1;
	      if (is_add && i < 2)
		{
		  if (i == 0)
		    {
		      if (value == KVX_GPR_REG_SP)
			crt_peb_insn->insn_type = KVX_PROL_EPIL_INSN_ADD_SP;
		      else if (value == KVX_GPR_REG_FP)
			crt_peb_insn->insn_type = KVX_PROL_EPIL_INSN_ADD_FP;
		      else
			is_add = 0;
		    }
		  else if (i == 1)
		    {
		      if (value == KVX_GPR_REG_SP)
			is_a_peb_insn = 1;
		      else if (value == KVX_GPR_REG_FP
			       && crt_peb_insn->insn_type
			       == KVX_PROL_EPIL_INSN_ADD_SP)
			{
			  crt_peb_insn->insn_type
			    = KVX_PROL_EPIL_INSN_RESTORE_SP_FROM_FP;
			  is_a_peb_insn = 1;
			}
		      else
			is_add = 0;
		    }
		}
	      else if (is_copyd && i < 2)
		{
		  if (i == 0)
		    {
		      if (value == KVX_GPR_REG_FP)
			{
			  crt_peb_insn->insn_type = KVX_PROL_EPIL_INSN_ADD_FP;
			  crt_peb_insn->immediate = 0;
			}
		      else if (value == KVX_GPR_REG_SP)
			{
			  crt_peb_insn->insn_type
			    = KVX_PROL_EPIL_INSN_RESTORE_SP_FROM_FP;
			  crt_peb_insn->immediate = 0;
			}
		      else
			is_copyd = 0;
		    }
		  else if (i == 1)
		    {
		      if (value == KVX_GPR_REG_SP
			  && crt_peb_insn->insn_type
			  == KVX_PROL_EPIL_INSN_ADD_FP)
			is_a_peb_insn = 1;
		      else if (value == KVX_GPR_REG_FP
			       && crt_peb_insn->insn_type
			       == KVX_PROL_EPIL_INSN_RESTORE_SP_FROM_FP)
			is_a_peb_insn = 1;
		      else
			is_copyd = 0;
		    }
		}
	      else
		crt_peb_insn->gpr_reg[crt_peb_insn->nb_gprs++] = value;
	    }
	  else if (   chk_type (kv3_v1, RegClass_kv3_v1_pairedReg)
		   || chk_type (kv3_v2, RegClass_kv3_v2_pairedReg)
		   || chk_type (kv4_v1, RegClass_kv4_v1_pairedReg))
	    crt_peb_insn->gpr_reg[crt_peb_insn->nb_gprs++] = value * 2;
	  else if (   chk_type (kv3_v1, RegClass_kv3_v1_quadReg)
		   || chk_type (kv3_v2, RegClass_kv3_v2_quadReg)
		   || chk_type (kv4_v1, RegClass_kv4_v1_quadReg))
	    crt_peb_insn->gpr_reg[crt_peb_insn->nb_gprs++] = value * 4;
	  else if (   chk_type (kv3_v1, RegClass_kv3_v1_systemReg)
		   || chk_type (kv3_v2, RegClass_kv3_v2_systemReg)
		   || chk_type (kv4_v1, RegClass_kv4_v1_systemReg)
		   || chk_type (kv3_v1, RegClass_kv3_v1_aloneReg)
		   || chk_type (kv3_v2, RegClass_kv3_v2_aloneReg)
		   || chk_type (kv4_v1, RegClass_kv4_v1_aloneReg)
		   || chk_type (kv3_v1, RegClass_kv3_v1_onlyraReg)
		   || chk_type (kv3_v2, RegClass_kv3_v2_onlyraReg)
		   || chk_type (kv4_v1, RegClass_kv4_v1_onlygetReg)
		   || chk_type (kv3_v1, RegClass_kv3_v1_onlygetReg)
		   || chk_type (kv3_v2, RegClass_kv3_v2_onlygetReg)
		   || chk_type (kv4_v1, RegClass_kv4_v1_onlygetReg)
		   || chk_type (kv3_v1, RegClass_kv3_v1_onlysetReg)
		   || chk_type (kv3_v2, RegClass_kv3_v2_onlysetReg)
		   || chk_type (kv4_v1, RegClass_kv4_v1_onlysetReg)
		   || chk_type (kv3_v1, RegClass_kv3_v1_onlyfxReg)
		   || chk_type (kv3_v2, RegClass_kv3_v2_onlyfxReg)
		   || chk_type (kv4_v1, RegClass_kv4_v1_onlyfxReg))
	    {
	      if (env.kvx_regfiles[KVX_REGFILE_DEC_GPR] + value
		  >= env.kvx_max_dec_registers)
		return -1;
	      if (is_get && !strcmp (env.kvx_registers[env.kvx_dec_registers[env.kvx_regfiles[KVX_REGFILE_DEC_SFR] + value]].name, "$ra"))
		{
		  crt_peb_insn->insn_type = KVX_PROL_EPIL_INSN_GET_RA;
		  is_a_peb_insn = 1;
		}
	    }
	  else if (   chk_type (kv3_v1, RegClass_kv3_v1_xworddReg)
		   || chk_type (kv3_v2, RegClass_kv3_v2_xworddReg)
		   || chk_type (kv4_v1, RegClass_kv4_v1_xworddReg)
		   || chk_type (kv3_v1, RegClass_kv3_v1_xwordqReg)
		   || chk_type (kv3_v2, RegClass_kv3_v2_xwordqReg)
		   || chk_type (kv4_v1, RegClass_kv4_v1_xwordqReg)
		   || chk_type (kv3_v1, RegClass_kv3_v1_xwordoReg)
		   || chk_type (kv3_v2, RegClass_kv3_v2_xwordoReg)
		   || chk_type (kv4_v1, RegClass_kv4_v1_xwordoReg)
		   || chk_type (kv3_v1, RegClass_kv3_v1_xwordxReg)
		   || chk_type (kv3_v2, RegClass_kv3_v2_xwordxReg)
		   || chk_type (kv4_v1, RegClass_kv4_v1_xwordxReg)
		   || chk_type (kv3_v1, RegClass_kv3_v1_xwordvReg)
		   || chk_type (kv3_v2, RegClass_kv3_v2_xwordvReg)
		   || chk_type (kv4_v1, RegClass_kv4_v1_xwordvReg)
		   || chk_type (kv3_v1, Modifier_kv3_v1_scalarcond)
		   || chk_type (kv3_v1, Modifier_kv3_v1_column)
		   || chk_type (kv3_v1, Modifier_kv3_v1_comparison)
		   || chk_type (kv3_v1, Modifier_kv3_v1_doscale)
		   || chk_type (kv3_v1, Modifier_kv3_v1_exunum)
		   || chk_type (kv3_v1, Modifier_kv3_v1_floatcomp)
		   || chk_type (kv3_v1, Modifier_kv3_v1_qindex)
		   || chk_type (kv3_v1, Modifier_kv3_v1_rectify)
		   || chk_type (kv3_v1, Modifier_kv3_v1_rounding)
		   || chk_type (kv3_v1, Modifier_kv3_v1_roundint)
		   || chk_type (kv3_v1, Modifier_kv3_v1_saturate)
		   || chk_type (kv3_v1, Modifier_kv3_v1_scalarcond)
		   || chk_type (kv3_v1, Modifier_kv3_v1_silent)
		   || chk_type (kv3_v1, Modifier_kv3_v1_simdcond)
		   || chk_type (kv3_v1, Modifier_kv3_v1_speculate)
		   || chk_type (kv3_v1, Modifier_kv3_v1_splat32)
		   || chk_type (kv3_v1, Modifier_kv3_v1_variant)
		   || chk_type (kv3_v2, Modifier_kv3_v2_accesses)
		   || chk_type (kv3_v2, Modifier_kv3_v2_boolcas)
		   || chk_type (kv3_v2, Modifier_kv3_v2_cachelev)
		   || chk_type (kv3_v2, Modifier_kv3_v2_channel)
		   || chk_type (kv3_v2, Modifier_kv3_v2_coherency)
		   || chk_type (kv3_v2, Modifier_kv3_v2_comparison)
		   || chk_type (kv3_v2, Modifier_kv3_v2_conjugate)
		   || chk_type (kv3_v2, Modifier_kv3_v2_doscale)
		   || chk_type (kv3_v2, Modifier_kv3_v2_exunum)
		   || chk_type (kv3_v2, Modifier_kv3_v2_floatcomp)
		   || chk_type (kv3_v2, Modifier_kv3_v2_hindex)
		   || chk_type (kv3_v2, Modifier_kv3_v2_lsomask)
		   || chk_type (kv3_v2, Modifier_kv3_v2_lsumask)
		   || chk_type (kv3_v2, Modifier_kv3_v2_qindex)
		   || chk_type (kv3_v2, Modifier_kv3_v2_rounding)
		   || chk_type (kv3_v2, Modifier_kv3_v2_scalarcond)
		   || chk_type (kv3_v2, Modifier_kv3_v2_shuffleV)
		   || chk_type (kv3_v2, Modifier_kv3_v2_shuffleX)
		   || chk_type (kv3_v2, Modifier_kv3_v2_silent)
		   || chk_type (kv3_v2, Modifier_kv3_v2_simdcond)
		   || chk_type (kv3_v2, Modifier_kv3_v2_speculate)
		   || chk_type (kv3_v2, Modifier_kv3_v2_splat32)
		   || chk_type (kv3_v2, Modifier_kv3_v2_transpose)
		   || chk_type (kv3_v2, Modifier_kv3_v2_variant)
		   || chk_type (kv4_v1, Modifier_kv4_v1_accesses)
		   || chk_type (kv4_v1, Modifier_kv4_v1_boolcas)
		   || chk_type (kv4_v1, Modifier_kv4_v1_cachelev)
		   || chk_type (kv4_v1, Modifier_kv4_v1_channel)
		   || chk_type (kv4_v1, Modifier_kv4_v1_coherency)
		   || chk_type (kv4_v1, Modifier_kv4_v1_doscale)
		   || chk_type (kv4_v1, Modifier_kv4_v1_exunum)
		   || chk_type (kv4_v1, Modifier_kv4_v1_floatcomp)
		   || chk_type (kv4_v1, Modifier_kv4_v1_hindex)
		   || chk_type (kv4_v1, Modifier_kv4_v1_qindex)
		   || chk_type (kv4_v1, Modifier_kv4_v1_floatmode)
		   || chk_type (kv4_v1, Modifier_kv4_v1_ccbcomp)
		   || chk_type (kv4_v1, Modifier_kv4_v1_intcomp)
		   || chk_type (kv4_v1, Modifier_kv4_v1_bcucond)
		   || chk_type (kv4_v1, Modifier_kv4_v1_shuffleV)
		   || chk_type (kv4_v1, Modifier_kv4_v1_shuffleX)
		   || chk_type (kv4_v1, Modifier_kv4_v1_lanecond)
		   || chk_type (kv4_v1, Modifier_kv4_v1_speculate)
		   || chk_type (kv4_v1, Modifier_kv4_v1_splat32)
		   || chk_type (kv4_v1, Modifier_kv4_v1_variant)
		   || chk_type (kv4_v1, Modifier_kv4_v1_lanetodo)
		   || chk_type (kv4_v1, Modifier_kv4_v1_lanesize)
		   )
	    {
	      /* Do nothing.  */
	    }
	  else if (   chk_type (kv3_v1, Immediate_kv3_v1_sysnumber)
		   || chk_type (kv3_v2, Immediate_kv3_v2_sysnumber)
		   || chk_type (kv4_v1, Immediate_kv4_v1_sysnumber)
		   || chk_type (kv3_v2, Immediate_kv3_v2_wrapped8)
		   || chk_type (kv4_v1, Immediate_kv4_v1_wrapped8)
		   || chk_type (kv3_v1, Immediate_kv3_v1_signed10)
		   || chk_type (kv3_v2, Immediate_kv3_v2_signed10)
		   || chk_type (kv4_v1, Immediate_kv4_v1_signed10)
		   || chk_type (kv4_v1, Immediate_kv4_v1_signed12)
		   || chk_type (kv3_v1, Immediate_kv3_v1_signed16)
		   || chk_type (kv3_v2, Immediate_kv3_v2_signed16)
		   || chk_type (kv4_v1, Immediate_kv4_v1_signed16)
		   || chk_type (kv4_v1, Immediate_kv4_v1_signed20)
		   || chk_type (kv3_v1, Immediate_kv3_v1_signed27)
		   || chk_type (kv3_v2, Immediate_kv3_v2_signed27)
		   || chk_type (kv4_v1, Immediate_kv4_v1_signed27)
		   || chk_type (kv3_v1, Immediate_kv3_v1_wrapped32)
		   || chk_type (kv3_v2, Immediate_kv3_v2_wrapped32)
		   || chk_type (kv4_v1, Immediate_kv4_v1_wrapped32)
		   || chk_type (kv3_v1, Immediate_kv3_v1_signed37)
		   || chk_type (kv3_v2, Immediate_kv3_v2_signed37)
		   || chk_type (kv4_v1, Immediate_kv4_v1_signed37)
		   || chk_type (kv3_v1, Immediate_kv3_v1_signed43)
		   || chk_type (kv3_v2, Immediate_kv3_v2_signed43)
		   || chk_type (kv4_v1, Immediate_kv4_v1_signed43)
		   || chk_type (kv3_v1, Immediate_kv3_v1_signed54)
		   || chk_type (kv3_v2, Immediate_kv3_v2_signed54)
		   || chk_type (kv4_v1, Immediate_kv4_v1_signed54)
		   || chk_type (kv3_v1, Immediate_kv3_v1_wrapped64)
		   || chk_type (kv3_v2, Immediate_kv3_v2_wrapped64)
		   || chk_type (kv4_v1, Immediate_kv4_v1_wrapped64)
		   || chk_type (kv3_v1, Immediate_kv3_v1_unsigned6)
		   || chk_type (kv3_v2, Immediate_kv3_v2_unsigned6)
		   || chk_type (kv4_v1, Immediate_kv4_v1_unsigned6))
	    crt_peb_insn->immediate = value;
	  else if (   chk_type (kv3_v1, Immediate_kv3_v1_pcrel17)
		   || chk_type (kv3_v1, Immediate_kv3_v1_pcrel27)
		   || chk_type (kv3_v2, Immediate_kv3_v2_pcrel17)
		   || chk_type (kv3_v2, Immediate_kv3_v2_pcrel27)
		   || chk_type (kv4_v1, Immediate_kv4_v1_pcrel11s2)
		   || chk_type (kv4_v1, Immediate_kv4_v1_pcrel12s1)
		   || chk_type (kv4_v1, Immediate_kv4_v1_pcrel17s2)
		   || chk_type (kv4_v1, Immediate_kv4_v1_pcrel20s1)
		   || chk_type (kv4_v1, Immediate_kv4_v1_pcrel27s2)
		   || chk_type (kv4_v1, Immediate_kv4_v1_pcrel38s2)
		   || chk_type (kv4_v1, Immediate_kv4_v1_pcrel44s2)
		   || chk_type (kv4_v1, Immediate_kv4_v1_pcrel54s2))
	    crt_peb_insn->immediate = value + memaddr;
	  else
	    return -1;
	}

      if (is_a_peb_insn)
	peb->nb_insn++;
      continue;
    }

  return nb_syl * 4;
#undef chk_type
}

void
print_kvx_disassembler_options (FILE * stream)
{
  fprintf (stream, _("\n\
The following KVX specific disassembler options are supported for use\n\
with the -M switch (multiple options should be separated by commas):\n"));

  fprintf (stream, _("\n\
  pretty               Print 32-bit words in natural order corresponding to \
re-ordered instruction.\n"));

  fprintf (stream, _("\n\
  compact-assembly     Do not emit a new line between bundles of instructions.\
\n"));

  fprintf (stream, _("\n\
  no-compact-assembly  Emit a new line between bundles of instructions.\n"));

  fprintf (stream, _("\n"));
}
