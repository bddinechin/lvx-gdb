/* LVX assembler/disassembler support.

   Copyright (C) 2009-2024 Free Software Foundation, Inc.
   Contributed by Liesme Tech.

   This file is part of GNU Binutils.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the license, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING3. If not,
   see <http://www.gnu.org/licenses/>.  */


#ifndef OPCODE_LVX_H
#define OPCODE_LVX_H

#define LVX_NUMCORES 2
#define LVX_MAXSYLLABLES 3
#define LVX_MAXOPERANDS 7
#define LVX_MAXBUNDLEISSUE 10
#define LVX_MAXBUNDLEWORDS 18


/*
 * The following macros are provided for compatibility with old
 * code.  They should not be used in new code.
 */

#define LVX_ACTIVATE_OFFSET	6
#define LVX_ACTIVATE_WIDTH	8
#define LVX_ACTIVATE_MASK	\
  (((1 << LVX_ACTIVATE_WIDTH) - 1) << LVX_ACTIVATE_OFFSET)
#define LVX_BCU_GUARD_OPCODE	0x0f800000
#define LVX_BCU_GUARD_MASK	0x7ffc0000
#define LVX_BCU_BLEND_OPCODE	0x0f840000
#define LVX_BCU_BLEND_MASK	0x7ffc0000

/***********************************************/
/*       DATA TYPES                            */
/***********************************************/

/*  Operand definition -- used in building     */
/*  format table                               */

enum lvx_rel {
  /* Absolute relocation. */
  LVX_REL_ABS,
  /* PC relative relocation. */
  LVX_REL_PC,
  /* GP relative relocation. */
  LVX_REL_GP,
  /* TP relative relocation. */
  LVX_REL_TP,
  /* GOT relative relocation. */
  LVX_REL_GOT,
  /* BASE load address relative relocation. */
  LVX_REL_BASE,
};

struct lvx_reloc {
  /* Size in bits. */
  int bitsize;
  /* Type of relative relocation. */
  enum lvx_rel relative;
  /* Number of BFD relocations. */
  int reloc_nb;
  /* List of BFD relocations. */
  unsigned int relocs[];
};

struct lvx_bitfield {
  /* Number of bits.  */
  int size;
  /* Offset in abstract value.  */
  int from_offset;
  /* Offset in encoded value.  */
  int to_offset;
};

struct lvx_operand {
  /* Operand type name.  */
  const char *tname;
  /* Type of operand.  */
  int type;
  /* Width of the operand. */
  int width;
  /* Encoded value shift. */
  int shift;
  /* Encoded value bias.  */
  int bias;
  /* Can be SIGNED|CANEXTEND|BITMASK|WRAPPED.  */
  int flags;
  /* Number of registers.  */
  int reg_nb;
  /* Valid registers for this operand (if no register get null pointer).  */
  int *regs;
  /* Number of relocations.  */
  int reloc_nb;
  /* List of relocations that can be applied to this operand.  */
  struct lvx_reloc **relocs;
  /* Number of given bitfields.  */
  int bitfields;
  /* Bitfields in most to least significant order.  */
  struct lvx_bitfield bfield[];
};

struct lvx_pseudo_relocs
{
  enum
  {
    S32_LO5_UP27,
    S37_LO10_UP27,
    S43_LO10_UP27_EX6,
    S64_LO10_UP27_EX27,
    S16,
    S32,
    S64,
  } reloc_type;

  int bitsize;

  /* Used when pseudo func should expand to different relocations
     based on the 32/64 bits mode.
     Enum values should match the lvx_arch_size var set by -m32
   */
  enum
  {
    PSEUDO_ALL = 0,
    PSEUDO_32_ONLY = 32,
    PSEUDO_64_ONLY = 64,
  } avail_modes;

  /* set to 1 when pseudo func does not take an argument */
  int has_no_arg;

  bfd_reloc_code_real_type reloc_lo5, reloc_lo10, reloc_up27, reloc_ex;
  bfd_reloc_code_real_type single;
  struct lvx_reloc *kreloc;
};

typedef struct symbol symbolS;

struct pseudo_func
{
  const char *name;

  symbolS *sym;
  struct lvx_pseudo_relocs pseudo_relocs;
};

/* Flags for lvx_operand  */
#define LVX_OPERAND_SIGNED    1
#define LVX_OPERAND_CANEXTEND 2
#define LVX_OPERAND_BITMASK   4
#define LVX_OPERAND_WRAPPED   8

#define LVX_OPCODE_FLAG_UNDEF 0
#define LVX_OPCODE_FLAG_IMMX 1
#define LVX_OPCODE_FLAG_COND 2
#define LVX_OPCODE_FLAG_CALL 4
#define LVX_OPCODE_FLAG_LOAD 8
#define LVX_OPCODE_FLAG_STORE 16
#define LVX_OPCODE_FLAG_MODE32 32
#define LVX_OPCODE_FLAG_MODE64 64
#define LVX_OPCODE_FLAG_RISCV 128

/* Opcode definition.  */

struct lvx_codeword {
  /* The opcode.  */
  unsigned opcode;
  /* Disassembly mask.  */
  unsigned mask;
  /* Target dependent flags.  */
  unsigned flags;
};

struct lvx_opc {
  /* asm name */
  const char *as_op;
  /* 32 bits code words. */
  struct lvx_codeword codewords[LVX_MAXSYLLABLES];
  /* Number of words in codewords[].  */
  int wordcount;
  /* Bundling class.  */
  short bundling;
  /* Reservation class.  */
  short reservation;
  /* 0 terminated.  */
  struct lvx_operand *format[LVX_MAXOPERANDS + 1];
  /* Formating string.  */
  const char *fmtstring;
};

struct lvx_core_info {
  struct lvx_opc *optab;
  const char *name;
  const int *resources;
  int elf_core;
  struct pseudo_func *pseudo_funcs;
  int nb_pseudo_funcs;
  int **reservation_tables;
  int reservation_table_cycles;
  int resource_count;
  char **resource_names;
};

struct lvx_register {
  int id;
  const char *name;
};

extern const int lvx_v1_reservation_table_cycles;
extern const int *lvx_v1_reservation_tables[];
extern const char *lvx_v1_resource_names[];

extern const int lvx_v1_resources[];
extern struct lvx_opc lvx_v1_optab[];
extern const struct lvx_core_info lvx_v1_core_info;
extern const int lvx_v2_reservation_table_cycles;
extern const int *lvx_v2_reservation_tables[];
extern const char *lvx_v2_resource_names[];

extern const int lvx_v2_resources[];
extern struct lvx_opc lvx_v2_optab[];
extern const struct lvx_core_info lvx_v2_core_info;
extern const struct lvx_core_info *lvx_core_info_table[];
extern const char ***lvx_modifiers_table[];
extern const struct lvx_register *lvx_registers_table[];
extern const int *lvx_regfiles_table[];
extern const int lvx_regfiles_size_table[];

#define LVX_REGFILE_FIRST_SFR 0
#define LVX_REGFILE_LAST_SFR 1
#define LVX_REGFILE_DEC_SFR 2
#define LVX_REGFILE_FIRST_GPR 3
#define LVX_REGFILE_LAST_GPR 4
#define LVX_REGFILE_DEC_GPR 5


#define LVX_V1_REGFILE_FIRST_SFR LVX_REGFILE_FIRST_SFR
#define LVX_V1_REGFILE_LAST_SFR LVX_REGFILE_LAST_SFR
#define LVX_V1_REGFILE_DEC_SFR LVX_REGFILE_DEC_SFR
#define LVX_V1_REGFILE_FIRST_GPR LVX_REGFILE_FIRST_GPR
#define LVX_V1_REGFILE_LAST_GPR LVX_REGFILE_LAST_GPR
#define LVX_V1_REGFILE_DEC_GPR LVX_REGFILE_DEC_GPR
#define LVX_V1_REGFILE_FIRST_PGR 6
#define LVX_V1_REGFILE_LAST_PGR 7
#define LVX_V1_REGFILE_DEC_PGR 8
#define LVX_V1_REGFILE_FIRST_QGR 9
#define LVX_V1_REGFILE_LAST_QGR 10
#define LVX_V1_REGFILE_DEC_QGR 11
#define LVX_V1_REGFILE_FIRST_RV_BIR 12
#define LVX_V1_REGFILE_LAST_RV_BIR 13
#define LVX_V1_REGFILE_DEC_RV_BIR 14
#define LVX_V1_REGFILE_FIRST_RV_BIRP 15
#define LVX_V1_REGFILE_LAST_RV_BIRP 16
#define LVX_V1_REGFILE_DEC_RV_BIRP 17
#define LVX_V1_REGFILE_FIRST_RV_FPR 18
#define LVX_V1_REGFILE_LAST_RV_FPR 19
#define LVX_V1_REGFILE_DEC_RV_FPR 20
#define LVX_V1_REGFILE_FIRST_X16R 21
#define LVX_V1_REGFILE_LAST_X16R 22
#define LVX_V1_REGFILE_DEC_X16R 23
#define LVX_V1_REGFILE_FIRST_X2R 24
#define LVX_V1_REGFILE_LAST_X2R 25
#define LVX_V1_REGFILE_DEC_X2R 26
#define LVX_V1_REGFILE_FIRST_X32R 27
#define LVX_V1_REGFILE_LAST_X32R 28
#define LVX_V1_REGFILE_DEC_X32R 29
#define LVX_V1_REGFILE_FIRST_X4R 30
#define LVX_V1_REGFILE_LAST_X4R 31
#define LVX_V1_REGFILE_DEC_X4R 32
#define LVX_V1_REGFILE_FIRST_X64R 33
#define LVX_V1_REGFILE_LAST_X64R 34
#define LVX_V1_REGFILE_DEC_X64R 35
#define LVX_V1_REGFILE_FIRST_X8R 36
#define LVX_V1_REGFILE_LAST_X8R 37
#define LVX_V1_REGFILE_DEC_X8R 38
#define LVX_V1_REGFILE_FIRST_XBR 39
#define LVX_V1_REGFILE_LAST_XBR 40
#define LVX_V1_REGFILE_DEC_XBR 41
#define LVX_V1_REGFILE_FIRST_XCR 42
#define LVX_V1_REGFILE_LAST_XCR 43
#define LVX_V1_REGFILE_DEC_XCR 44
#define LVX_V1_REGFILE_FIRST_XMR 45
#define LVX_V1_REGFILE_LAST_XMR 46
#define LVX_V1_REGFILE_DEC_XMR 47
#define LVX_V1_REGFILE_FIRST_XTR 48
#define LVX_V1_REGFILE_LAST_XTR 49
#define LVX_V1_REGFILE_DEC_XTR 50
#define LVX_V1_REGFILE_FIRST_XVR 51
#define LVX_V1_REGFILE_LAST_XVR 52
#define LVX_V1_REGFILE_DEC_XVR 53
#define LVX_V1_REGFILE_REGISTERS 54
#define LVX_V1_REGFILE_DEC_REGISTERS 55


extern int lvx_v1_regfiles[];
extern const char **lvx_v1_modifiers[];
extern struct lvx_register lvx_v1_registers[];

extern int lvx_v1_dec_registers[];

enum Method_lvx_v1_enum {
  Immediate_lvx_v1_brknumber = 1,
  Immediate_lvx_v1_pcrel11s2 = 2,
  Immediate_lvx_v1_pcrel17s2 = 3,
  Immediate_lvx_v1_pcrel27s2 = 4,
  Immediate_lvx_v1_pcrel38s2 = 5,
  Immediate_lvx_v1_pcrel44s2 = 6,
  Immediate_lvx_v1_pcrel54s2 = 7,
  Immediate_lvx_v1_signed10 = 8,
  Immediate_lvx_v1_signed16 = 9,
  Immediate_lvx_v1_signed27 = 10,
  Immediate_lvx_v1_signed37 = 11,
  Immediate_lvx_v1_signed43 = 12,
  Immediate_lvx_v1_signed54 = 13,
  Immediate_lvx_v1_sysnumber = 14,
  Immediate_lvx_v1_unsigned6 = 15,
  Immediate_lvx_v1_wrapped32 = 16,
  Immediate_lvx_v1_wrapped64 = 17,
  Immediate_lvx_v1_wrapped8 = 18,
  Modifier_lvx_v1_accesses = 19,
  Modifier_lvx_v1_bcucond = 20,
  Modifier_lvx_v1_boolcas = 21,
  Modifier_lvx_v1_cachelev = 22,
  Modifier_lvx_v1_ccbcomp = 23,
  Modifier_lvx_v1_channel = 24,
  Modifier_lvx_v1_coherency = 25,
  Modifier_lvx_v1_conjugate = 26,
  Modifier_lvx_v1_doscale = 27,
  Modifier_lvx_v1_exunum = 28,
  Modifier_lvx_v1_floatcomp = 29,
  Modifier_lvx_v1_floatmode = 30,
  Modifier_lvx_v1_fnegate = 31,
  Modifier_lvx_v1_highmult = 32,
  Modifier_lvx_v1_hindex = 33,
  Modifier_lvx_v1_imultiply = 34,
  Modifier_lvx_v1_intcomp = 35,
  Modifier_lvx_v1_lanecond = 36,
  Modifier_lvx_v1_lanesize = 37,
  Modifier_lvx_v1_lanetodo = 38,
  Modifier_lvx_v1_mostsig = 39,
  Modifier_lvx_v1_oddlanes = 40,
  Modifier_lvx_v1_qindex = 41,
  Modifier_lvx_v1_realimag = 42,
  Modifier_lvx_v1_shuffleV = 43,
  Modifier_lvx_v1_shuffleX = 44,
  Modifier_lvx_v1_signextw = 45,
  Modifier_lvx_v1_speculate = 46,
  Modifier_lvx_v1_splat32 = 47,
  Modifier_lvx_v1_variant = 48,
  Modifier_lvx_v1_widemult = 49,
  Modifier_lvx_v1_ziplanes = 50,
  RegClass_lvx_v1_aloneReg = 51,
  RegClass_lvx_v1_buffer16Reg = 52,
  RegClass_lvx_v1_buffer2Reg = 53,
  RegClass_lvx_v1_buffer32Reg = 54,
  RegClass_lvx_v1_buffer4Reg = 55,
  RegClass_lvx_v1_buffer64Reg = 56,
  RegClass_lvx_v1_buffer8Reg = 57,
  RegClass_lvx_v1_floatReg = 58,
  RegClass_lvx_v1_mainReg = 59,
  RegClass_lvx_v1_mainRegPair = 60,
  RegClass_lvx_v1_onlyfxReg = 61,
  RegClass_lvx_v1_onlygetReg = 62,
  RegClass_lvx_v1_onlyraReg = 63,
  RegClass_lvx_v1_onlysetReg = 64,
  RegClass_lvx_v1_onlyswapReg = 65,
  RegClass_lvx_v1_pairedReg = 66,
  RegClass_lvx_v1_quadReg = 67,
  RegClass_lvx_v1_singleReg = 68,
  RegClass_lvx_v1_systemReg = 69,
  RegClass_lvx_v1_worddRegE = 70,
  RegClass_lvx_v1_worddRegO = 71,
  RegClass_lvx_v1_xworddReg = 72,
  RegClass_lvx_v1_xworddReg0M4 = 73,
  RegClass_lvx_v1_xworddReg1M4 = 74,
  RegClass_lvx_v1_xworddReg2M4 = 75,
  RegClass_lvx_v1_xworddReg3M4 = 76,
  RegClass_lvx_v1_xwordoReg = 77,
  RegClass_lvx_v1_xwordqReg = 78,
  RegClass_lvx_v1_xwordqRegE = 79,
  RegClass_lvx_v1_xwordqRegO = 80,
  RegClass_lvx_v1_xwordvReg = 81,
  RegClass_lvx_v1_xwordxReg = 82,
  Instruction_lvx_v1_abdd = 83,
  Instruction_lvx_v1_abdsd = 84,
  Instruction_lvx_v1_abdsw = 85,
  Instruction_lvx_v1_abdud = 86,
  Instruction_lvx_v1_abduw = 87,
  Instruction_lvx_v1_abdw = 88,
  Instruction_lvx_v1_absd = 89,
  Instruction_lvx_v1_abssd = 90,
  Instruction_lvx_v1_abssw = 91,
  Instruction_lvx_v1_absw = 92,
  Instruction_lvx_v1_acswapb = 93,
  Instruction_lvx_v1_acswapd = 94,
  Instruction_lvx_v1_acswaph = 95,
  Instruction_lvx_v1_acswapq = 96,
  Instruction_lvx_v1_acswapw = 97,
  Instruction_lvx_v1_addd = 98,
  Instruction_lvx_v1_addq = 99,
  Instruction_lvx_v1_addsd = 100,
  Instruction_lvx_v1_addsw = 101,
  Instruction_lvx_v1_addusd = 102,
  Instruction_lvx_v1_addusw = 103,
  Instruction_lvx_v1_addw = 104,
  Instruction_lvx_v1_addx16d = 105,
  Instruction_lvx_v1_addx16w = 106,
  Instruction_lvx_v1_addx2d = 107,
  Instruction_lvx_v1_addx2w = 108,
  Instruction_lvx_v1_addx32d = 109,
  Instruction_lvx_v1_addx32w = 110,
  Instruction_lvx_v1_addx4d = 111,
  Instruction_lvx_v1_addx4w = 112,
  Instruction_lvx_v1_addx64d = 113,
  Instruction_lvx_v1_addx64w = 114,
  Instruction_lvx_v1_addx8d = 115,
  Instruction_lvx_v1_addx8w = 116,
  Instruction_lvx_v1_aladdb = 117,
  Instruction_lvx_v1_aladdd = 118,
  Instruction_lvx_v1_aladdh = 119,
  Instruction_lvx_v1_aladdw = 120,
  Instruction_lvx_v1_alandb = 121,
  Instruction_lvx_v1_alandd = 122,
  Instruction_lvx_v1_alandh = 123,
  Instruction_lvx_v1_alandw = 124,
  Instruction_lvx_v1_alb = 125,
  Instruction_lvx_v1_alclrb = 126,
  Instruction_lvx_v1_alclrd = 127,
  Instruction_lvx_v1_alclrh = 128,
  Instruction_lvx_v1_alclrw = 129,
  Instruction_lvx_v1_ald = 130,
  Instruction_lvx_v1_aldusb = 131,
  Instruction_lvx_v1_aldusd = 132,
  Instruction_lvx_v1_aldush = 133,
  Instruction_lvx_v1_aldusw = 134,
  Instruction_lvx_v1_aleorb = 135,
  Instruction_lvx_v1_aleord = 136,
  Instruction_lvx_v1_aleorh = 137,
  Instruction_lvx_v1_aleorw = 138,
  Instruction_lvx_v1_alh = 139,
  Instruction_lvx_v1_aliorb = 140,
  Instruction_lvx_v1_aliord = 141,
  Instruction_lvx_v1_aliorh = 142,
  Instruction_lvx_v1_aliorw = 143,
  Instruction_lvx_v1_almaxb = 144,
  Instruction_lvx_v1_almaxd = 145,
  Instruction_lvx_v1_almaxh = 146,
  Instruction_lvx_v1_almaxub = 147,
  Instruction_lvx_v1_almaxud = 148,
  Instruction_lvx_v1_almaxuh = 149,
  Instruction_lvx_v1_almaxuw = 150,
  Instruction_lvx_v1_almaxw = 151,
  Instruction_lvx_v1_alminb = 152,
  Instruction_lvx_v1_almind = 153,
  Instruction_lvx_v1_alminh = 154,
  Instruction_lvx_v1_alminub = 155,
  Instruction_lvx_v1_alminud = 156,
  Instruction_lvx_v1_alminuh = 157,
  Instruction_lvx_v1_alminuw = 158,
  Instruction_lvx_v1_alminw = 159,
  Instruction_lvx_v1_alw = 160,
  Instruction_lvx_v1_andd = 161,
  Instruction_lvx_v1_andnd = 162,
  Instruction_lvx_v1_andnq = 163,
  Instruction_lvx_v1_andnw = 164,
  Instruction_lvx_v1_andq = 165,
  Instruction_lvx_v1_andw = 166,
  Instruction_lvx_v1_asaddb = 167,
  Instruction_lvx_v1_asaddd = 168,
  Instruction_lvx_v1_asaddh = 169,
  Instruction_lvx_v1_asaddw = 170,
  Instruction_lvx_v1_asandb = 171,
  Instruction_lvx_v1_asandd = 172,
  Instruction_lvx_v1_asandh = 173,
  Instruction_lvx_v1_asandw = 174,
  Instruction_lvx_v1_asb = 175,
  Instruction_lvx_v1_asd = 176,
  Instruction_lvx_v1_asdusb = 177,
  Instruction_lvx_v1_asdusd = 178,
  Instruction_lvx_v1_asdush = 179,
  Instruction_lvx_v1_asdusw = 180,
  Instruction_lvx_v1_aseorb = 181,
  Instruction_lvx_v1_aseord = 182,
  Instruction_lvx_v1_aseorh = 183,
  Instruction_lvx_v1_aseorw = 184,
  Instruction_lvx_v1_ash = 185,
  Instruction_lvx_v1_asiorb = 186,
  Instruction_lvx_v1_asiord = 187,
  Instruction_lvx_v1_asiorh = 188,
  Instruction_lvx_v1_asiorw = 189,
  Instruction_lvx_v1_asmaxb = 190,
  Instruction_lvx_v1_asmaxd = 191,
  Instruction_lvx_v1_asmaxh = 192,
  Instruction_lvx_v1_asmaxub = 193,
  Instruction_lvx_v1_asmaxud = 194,
  Instruction_lvx_v1_asmaxuh = 195,
  Instruction_lvx_v1_asmaxuw = 196,
  Instruction_lvx_v1_asmaxw = 197,
  Instruction_lvx_v1_asminb = 198,
  Instruction_lvx_v1_asmind = 199,
  Instruction_lvx_v1_asminh = 200,
  Instruction_lvx_v1_asminub = 201,
  Instruction_lvx_v1_asminud = 202,
  Instruction_lvx_v1_asminuh = 203,
  Instruction_lvx_v1_asminuw = 204,
  Instruction_lvx_v1_asminw = 205,
  Instruction_lvx_v1_asw = 206,
  Instruction_lvx_v1_aswapb = 207,
  Instruction_lvx_v1_aswapd = 208,
  Instruction_lvx_v1_aswaph = 209,
  Instruction_lvx_v1_aswapw = 210,
  Instruction_lvx_v1_avgruw = 211,
  Instruction_lvx_v1_avgrw = 212,
  Instruction_lvx_v1_avguw = 213,
  Instruction_lvx_v1_avgw = 214,
  Instruction_lvx_v1_await = 215,
  Instruction_lvx_v1_barrier = 216,
  Instruction_lvx_v1_blend = 217,
  Instruction_lvx_v1_break = 218,
  Instruction_lvx_v1_call = 219,
  Instruction_lvx_v1_callx = 220,
  Instruction_lvx_v1_catdq = 221,
  Instruction_lvx_v1_cb = 222,
  Instruction_lvx_v1_cbsd = 223,
  Instruction_lvx_v1_cbsw = 224,
  Instruction_lvx_v1_cbx = 225,
  Instruction_lvx_v1_ccb = 226,
  Instruction_lvx_v1_ccbx = 227,
  Instruction_lvx_v1_clsd = 228,
  Instruction_lvx_v1_clsw = 229,
  Instruction_lvx_v1_clzd = 230,
  Instruction_lvx_v1_clzw = 231,
  Instruction_lvx_v1_cmoved = 232,
  Instruction_lvx_v1_cmoveq = 233,
  Instruction_lvx_v1_compd = 234,
  Instruction_lvx_v1_compq = 235,
  Instruction_lvx_v1_compw = 236,
  Instruction_lvx_v1_copyd = 237,
  Instruction_lvx_v1_copyo = 238,
  Instruction_lvx_v1_copyq = 239,
  Instruction_lvx_v1_copyw = 240,
  Instruction_lvx_v1_crcbellw = 241,
  Instruction_lvx_v1_crcbelmw = 242,
  Instruction_lvx_v1_crclellw = 243,
  Instruction_lvx_v1_crclelmw = 244,
  Instruction_lvx_v1_ctzd = 245,
  Instruction_lvx_v1_ctzw = 246,
  Instruction_lvx_v1_d1inval = 247,
  Instruction_lvx_v1_dflushl = 248,
  Instruction_lvx_v1_dflushsw = 249,
  Instruction_lvx_v1_dinvall = 250,
  Instruction_lvx_v1_dinvalsw = 251,
  Instruction_lvx_v1_divmodd = 252,
  Instruction_lvx_v1_divmodud = 253,
  Instruction_lvx_v1_divmoduw = 254,
  Instruction_lvx_v1_divmodw = 255,
  Instruction_lvx_v1_dpurgel = 256,
  Instruction_lvx_v1_dpurgesw = 257,
  Instruction_lvx_v1_dtouchl = 258,
  Instruction_lvx_v1_eord = 259,
  Instruction_lvx_v1_eorq = 260,
  Instruction_lvx_v1_eorw = 261,
  Instruction_lvx_v1_errop = 262,
  Instruction_lvx_v1_extfs = 263,
  Instruction_lvx_v1_extfz = 264,
  Instruction_lvx_v1_fabsd = 265,
  Instruction_lvx_v1_fabsh = 266,
  Instruction_lvx_v1_fabsw = 267,
  Instruction_lvx_v1_fabswp = 268,
  Instruction_lvx_v1_faddd = 269,
  Instruction_lvx_v1_faddh = 270,
  Instruction_lvx_v1_faddw = 271,
  Instruction_lvx_v1_faddwc = 272,
  Instruction_lvx_v1_fclassd = 273,
  Instruction_lvx_v1_fclassh = 274,
  Instruction_lvx_v1_fclassw = 275,
  Instruction_lvx_v1_fclasswp = 276,
  Instruction_lvx_v1_fcompd = 277,
  Instruction_lvx_v1_fcomph = 278,
  Instruction_lvx_v1_fcompw = 279,
  Instruction_lvx_v1_fdivd = 280,
  Instruction_lvx_v1_fdivh = 281,
  Instruction_lvx_v1_fdivw = 282,
  Instruction_lvx_v1_fence = 283,
  Instruction_lvx_v1_ffmad = 284,
  Instruction_lvx_v1_ffmah = 285,
  Instruction_lvx_v1_ffmaw = 286,
  Instruction_lvx_v1_ffmawc = 287,
  Instruction_lvx_v1_ffmsd = 288,
  Instruction_lvx_v1_ffmsh = 289,
  Instruction_lvx_v1_ffmsw = 290,
  Instruction_lvx_v1_fixedd = 291,
  Instruction_lvx_v1_fixeddw = 292,
  Instruction_lvx_v1_fixedud = 293,
  Instruction_lvx_v1_fixedudw = 294,
  Instruction_lvx_v1_fixeduw = 295,
  Instruction_lvx_v1_fixeduwd = 296,
  Instruction_lvx_v1_fixeduwp = 297,
  Instruction_lvx_v1_fixedw = 298,
  Instruction_lvx_v1_fixedwd = 299,
  Instruction_lvx_v1_fixedwp = 300,
  Instruction_lvx_v1_floatd = 301,
  Instruction_lvx_v1_floatdw = 302,
  Instruction_lvx_v1_floatud = 303,
  Instruction_lvx_v1_floatudw = 304,
  Instruction_lvx_v1_floatuw = 305,
  Instruction_lvx_v1_floatuwd = 306,
  Instruction_lvx_v1_floatuwp = 307,
  Instruction_lvx_v1_floatw = 308,
  Instruction_lvx_v1_floatwd = 309,
  Instruction_lvx_v1_floatwp = 310,
  Instruction_lvx_v1_fmaxd = 311,
  Instruction_lvx_v1_fmaxh = 312,
  Instruction_lvx_v1_fmaxnd = 313,
  Instruction_lvx_v1_fmaxnh = 314,
  Instruction_lvx_v1_fmaxnw = 315,
  Instruction_lvx_v1_fmaxw = 316,
  Instruction_lvx_v1_fmind = 317,
  Instruction_lvx_v1_fminh = 318,
  Instruction_lvx_v1_fminnd = 319,
  Instruction_lvx_v1_fminnh = 320,
  Instruction_lvx_v1_fminnw = 321,
  Instruction_lvx_v1_fminw = 322,
  Instruction_lvx_v1_fmuld = 323,
  Instruction_lvx_v1_fmulh = 324,
  Instruction_lvx_v1_fmulw = 325,
  Instruction_lvx_v1_fmulwc = 326,
  Instruction_lvx_v1_fnarrowdw = 327,
  Instruction_lvx_v1_fnarrowwh = 328,
  Instruction_lvx_v1_fnegd = 329,
  Instruction_lvx_v1_fnegh = 330,
  Instruction_lvx_v1_fnegw = 331,
  Instruction_lvx_v1_fnegwp = 332,
  Instruction_lvx_v1_frintd = 333,
  Instruction_lvx_v1_frinth = 334,
  Instruction_lvx_v1_frintw = 335,
  Instruction_lvx_v1_fsbfd = 336,
  Instruction_lvx_v1_fsbfh = 337,
  Instruction_lvx_v1_fsbfw = 338,
  Instruction_lvx_v1_fsbfwc = 339,
  Instruction_lvx_v1_fsignd = 340,
  Instruction_lvx_v1_fsignh = 341,
  Instruction_lvx_v1_fsignmd = 342,
  Instruction_lvx_v1_fsignmh = 343,
  Instruction_lvx_v1_fsignmw = 344,
  Instruction_lvx_v1_fsignnd = 345,
  Instruction_lvx_v1_fsignnh = 346,
  Instruction_lvx_v1_fsignnw = 347,
  Instruction_lvx_v1_fsignw = 348,
  Instruction_lvx_v1_fsqrtd = 349,
  Instruction_lvx_v1_fsqrth = 350,
  Instruction_lvx_v1_fsqrtw = 351,
  Instruction_lvx_v1_fsrecd = 352,
  Instruction_lvx_v1_fsrecw = 353,
  Instruction_lvx_v1_fsrecwp = 354,
  Instruction_lvx_v1_fsrsrd = 355,
  Instruction_lvx_v1_fsrsrw = 356,
  Instruction_lvx_v1_fsrsrwp = 357,
  Instruction_lvx_v1_fwidenhw = 358,
  Instruction_lvx_v1_fwidenwd = 359,
  Instruction_lvx_v1_get = 360,
  Instruction_lvx_v1_goto = 361,
  Instruction_lvx_v1_gotox = 362,
  Instruction_lvx_v1_guard = 363,
  Instruction_lvx_v1_i1inval = 364,
  Instruction_lvx_v1_i1invals = 365,
  Instruction_lvx_v1_icall = 366,
  Instruction_lvx_v1_iget = 367,
  Instruction_lvx_v1_igoto = 368,
  Instruction_lvx_v1_insf = 369,
  Instruction_lvx_v1_iord = 370,
  Instruction_lvx_v1_iornd = 371,
  Instruction_lvx_v1_iornq = 372,
  Instruction_lvx_v1_iornw = 373,
  Instruction_lvx_v1_iorq = 374,
  Instruction_lvx_v1_iorw = 375,
  Instruction_lvx_v1_landd = 376,
  Instruction_lvx_v1_landw = 377,
  Instruction_lvx_v1_lbs = 378,
  Instruction_lvx_v1_lbz = 379,
  Instruction_lvx_v1_ld = 380,
  Instruction_lvx_v1_lhs = 381,
  Instruction_lvx_v1_lhz = 382,
  Instruction_lvx_v1_liord = 383,
  Instruction_lvx_v1_liorw = 384,
  Instruction_lvx_v1_lnandd = 385,
  Instruction_lvx_v1_lnandw = 386,
  Instruction_lvx_v1_lniord = 387,
  Instruction_lvx_v1_lniorw = 388,
  Instruction_lvx_v1_lo = 389,
  Instruction_lvx_v1_loopdo = 390,
  Instruction_lvx_v1_lq = 391,
  Instruction_lvx_v1_lws = 392,
  Instruction_lvx_v1_lwz = 393,
  Instruction_lvx_v1_maddd = 394,
  Instruction_lvx_v1_madddq = 395,
  Instruction_lvx_v1_madddt = 396,
  Instruction_lvx_v1_maddsudt = 397,
  Instruction_lvx_v1_maddsuwd = 398,
  Instruction_lvx_v1_maddudt = 399,
  Instruction_lvx_v1_madduwd = 400,
  Instruction_lvx_v1_maddw = 401,
  Instruction_lvx_v1_maddwd = 402,
  Instruction_lvx_v1_make = 403,
  Instruction_lvx_v1_maxd = 404,
  Instruction_lvx_v1_maxud = 405,
  Instruction_lvx_v1_maxuw = 406,
  Instruction_lvx_v1_maxw = 407,
  Instruction_lvx_v1_mind = 408,
  Instruction_lvx_v1_minud = 409,
  Instruction_lvx_v1_minuw = 410,
  Instruction_lvx_v1_minw = 411,
  Instruction_lvx_v1_msbfd = 412,
  Instruction_lvx_v1_msbfdq = 413,
  Instruction_lvx_v1_msbfdt = 414,
  Instruction_lvx_v1_msbfsudt = 415,
  Instruction_lvx_v1_msbfsuwd = 416,
  Instruction_lvx_v1_msbfudt = 417,
  Instruction_lvx_v1_msbfuwd = 418,
  Instruction_lvx_v1_msbfw = 419,
  Instruction_lvx_v1_msbfwd = 420,
  Instruction_lvx_v1_muld = 421,
  Instruction_lvx_v1_muldq = 422,
  Instruction_lvx_v1_muldt = 423,
  Instruction_lvx_v1_mulnd = 424,
  Instruction_lvx_v1_mulndq = 425,
  Instruction_lvx_v1_mulnw = 426,
  Instruction_lvx_v1_mulnwd = 427,
  Instruction_lvx_v1_mulsudt = 428,
  Instruction_lvx_v1_mulsuwd = 429,
  Instruction_lvx_v1_muludt = 430,
  Instruction_lvx_v1_muluwd = 431,
  Instruction_lvx_v1_mulw = 432,
  Instruction_lvx_v1_mulwd = 433,
  Instruction_lvx_v1_nandd = 434,
  Instruction_lvx_v1_nandq = 435,
  Instruction_lvx_v1_nandw = 436,
  Instruction_lvx_v1_negd = 437,
  Instruction_lvx_v1_negq = 438,
  Instruction_lvx_v1_negsd = 439,
  Instruction_lvx_v1_negsw = 440,
  Instruction_lvx_v1_negw = 441,
  Instruction_lvx_v1_neord = 442,
  Instruction_lvx_v1_neorq = 443,
  Instruction_lvx_v1_neorw = 444,
  Instruction_lvx_v1_niord = 445,
  Instruction_lvx_v1_niorq = 446,
  Instruction_lvx_v1_niorw = 447,
  Instruction_lvx_v1_nop = 448,
  Instruction_lvx_v1_notd = 449,
  Instruction_lvx_v1_notq = 450,
  Instruction_lvx_v1_notw = 451,
  Instruction_lvx_v1_pcrel = 452,
  Instruction_lvx_v1_ret = 453,
  Instruction_lvx_v1_rfe = 454,
  Instruction_lvx_v1_rold = 455,
  Instruction_lvx_v1_rolw = 456,
  Instruction_lvx_v1_rord = 457,
  Instruction_lvx_v1_rorw = 458,
  Instruction_lvx_v1_rswap = 459,
  Instruction_lvx_v1_sb = 460,
  Instruction_lvx_v1_sbfd = 461,
  Instruction_lvx_v1_sbfq = 462,
  Instruction_lvx_v1_sbfsd = 463,
  Instruction_lvx_v1_sbfsw = 464,
  Instruction_lvx_v1_sbfusd = 465,
  Instruction_lvx_v1_sbfusw = 466,
  Instruction_lvx_v1_sbfw = 467,
  Instruction_lvx_v1_sbmm8 = 468,
  Instruction_lvx_v1_sbmm8d = 469,
  Instruction_lvx_v1_sbmm8eord = 470,
  Instruction_lvx_v1_sbmmt8 = 471,
  Instruction_lvx_v1_sbmmt8d = 472,
  Instruction_lvx_v1_sbmmt8eord = 473,
  Instruction_lvx_v1_scall = 474,
  Instruction_lvx_v1_sd = 475,
  Instruction_lvx_v1_set = 476,
  Instruction_lvx_v1_sh = 477,
  Instruction_lvx_v1_signd = 478,
  Instruction_lvx_v1_signsd = 479,
  Instruction_lvx_v1_signsw = 480,
  Instruction_lvx_v1_signw = 481,
  Instruction_lvx_v1_sleep = 482,
  Instruction_lvx_v1_slld = 483,
  Instruction_lvx_v1_sllw = 484,
  Instruction_lvx_v1_slsd = 485,
  Instruction_lvx_v1_slsw = 486,
  Instruction_lvx_v1_slusd = 487,
  Instruction_lvx_v1_slusw = 488,
  Instruction_lvx_v1_so = 489,
  Instruction_lvx_v1_sq = 490,
  Instruction_lvx_v1_srad = 491,
  Instruction_lvx_v1_sraw = 492,
  Instruction_lvx_v1_srld = 493,
  Instruction_lvx_v1_srlw = 494,
  Instruction_lvx_v1_srsd = 495,
  Instruction_lvx_v1_srsw = 496,
  Instruction_lvx_v1_stop = 497,
  Instruction_lvx_v1_stsud = 498,
  Instruction_lvx_v1_stsuw = 499,
  Instruction_lvx_v1_sw = 500,
  Instruction_lvx_v1_sxbd = 501,
  Instruction_lvx_v1_sxhd = 502,
  Instruction_lvx_v1_sxwd = 503,
  Instruction_lvx_v1_syncgroup = 504,
  Instruction_lvx_v1_tlbdinval = 505,
  Instruction_lvx_v1_tlbiinval = 506,
  Instruction_lvx_v1_tlbprobe = 507,
  Instruction_lvx_v1_tlbread = 508,
  Instruction_lvx_v1_tlbwrite = 509,
  Instruction_lvx_v1_waitit = 510,
  Instruction_lvx_v1_wfxl = 511,
  Instruction_lvx_v1_wfxm = 512,
  Instruction_lvx_v1_zxbd = 513,
  Instruction_lvx_v1_zxhd = 514,
  Instruction_lvx_v1_zxwd = 515,
  Separator_lvx_v1_comma = 516,
  Separator_lvx_v1_equal = 517,
  Separator_lvx_v1_qmark = 518,
  Separator_lvx_v1_rsbracket = 519,
  Separator_lvx_v1_lsbracket = 520
};

typedef enum {
  Modifier_lvx_v1_exunum_ALU0=0,
  Modifier_lvx_v1_exunum_ALU1=1,
  Modifier_lvx_v1_exunum_LSU0=2,
  Modifier_lvx_v1_exunum_LSU1=3,
} Modifier_lvx_v1_exunum_values;


extern const char *mod_lvx_v1_exunum[];
extern const char *mod_lvx_v1_ccbcomp[];
extern const char *mod_lvx_v1_bcucond[];
extern const char *mod_lvx_v1_intcomp[];
extern const char *mod_lvx_v1_lanecond[];
extern const char *mod_lvx_v1_lanetodo[];
extern const char *mod_lvx_v1_lanesize[];
extern const char *mod_lvx_v1_floatcomp[];
extern const char *mod_lvx_v1_floatmode[];
extern const char *mod_lvx_v1_signextw[];
extern const char *mod_lvx_v1_highmult[];
extern const char *mod_lvx_v1_widemult[];
extern const char *mod_lvx_v1_mostsig[];
extern const char *mod_lvx_v1_oddlanes[];
extern const char *mod_lvx_v1_ziplanes[];
extern const char *mod_lvx_v1_fnegate[];
extern const char *mod_lvx_v1_variant[];
extern const char *mod_lvx_v1_speculate[];
extern const char *mod_lvx_v1_doscale[];
extern const char *mod_lvx_v1_qindex[];
extern const char *mod_lvx_v1_hindex[];
extern const char *mod_lvx_v1_cachelev[];
extern const char *mod_lvx_v1_coherency[];
extern const char *mod_lvx_v1_boolcas[];
extern const char *mod_lvx_v1_accesses[];
extern const char *mod_lvx_v1_channel[];
extern const char *mod_lvx_v1_conjugate[];
extern const char *mod_lvx_v1_imultiply[];
extern const char *mod_lvx_v1_realimag[];
extern const char *mod_lvx_v1_shuffleV[];
extern const char *mod_lvx_v1_shuffleX[];
extern const char *mod_lvx_v1_splat32[];

typedef enum {
  Bundling_lvx_v1_ALL,
  Bundling_lvx_v1_BCU2,
  Bundling_lvx_v1_BCU2_X,
  Bundling_lvx_v1_BCU0,
  Bundling_lvx_v1_BCU,
  Bundling_lvx_v1_FULL,
  Bundling_lvx_v1_FULL_X,
  Bundling_lvx_v1_FULL_Y,
  Bundling_lvx_v1_LITE,
  Bundling_lvx_v1_LITE_X,
  Bundling_lvx_v1_LITE_Y,
  Bundling_lvx_v1_LSU0,
  Bundling_lvx_v1_LSU0_X,
  Bundling_lvx_v1_LSU0_Y,
  Bundling_lvx_v1_LSU,
  Bundling_lvx_v1_LSU_X,
  Bundling_lvx_v1_LSU_Y,
  Bundling_lvx_v1_TINY,
  Bundling_lvx_v1_TINY_X,
  Bundling_lvx_v1_TINY_Y,
  Bundling_lvx_v1_EXT,
  Bundling_lvx_v1_NOP,
} Bundling_lvx_v1;

static int ATTRIBUTE_UNUSED
lvx_v1_base_bundling(int bundling) {
  static int base_bundlings[] = {
    Bundling_lvx_v1_ALL,	// Bundling_lvx_v1_ALL
    Bundling_lvx_v1_BCU2,	// Bundling_lvx_v1_BCU2
    Bundling_lvx_v1_BCU2,	// Bundling_lvx_v1_BCU2_X
    Bundling_lvx_v1_BCU0,	// Bundling_lvx_v1_BCU0
    Bundling_lvx_v1_BCU,	// Bundling_lvx_v1_BCU
    Bundling_lvx_v1_FULL,	// Bundling_lvx_v1_FULL
    Bundling_lvx_v1_FULL,	// Bundling_lvx_v1_FULL_X
    Bundling_lvx_v1_FULL,	// Bundling_lvx_v1_FULL_Y
    Bundling_lvx_v1_LITE,	// Bundling_lvx_v1_LITE
    Bundling_lvx_v1_LITE,	// Bundling_lvx_v1_LITE_X
    Bundling_lvx_v1_LITE,	// Bundling_lvx_v1_LITE_Y
    Bundling_lvx_v1_LSU0,	// Bundling_lvx_v1_LSU0
    Bundling_lvx_v1_LSU0,	// Bundling_lvx_v1_LSU0_X
    Bundling_lvx_v1_LSU0,	// Bundling_lvx_v1_LSU0_Y
    Bundling_lvx_v1_LSU,	// Bundling_lvx_v1_LSU
    Bundling_lvx_v1_LSU,	// Bundling_lvx_v1_LSU_X
    Bundling_lvx_v1_LSU,	// Bundling_lvx_v1_LSU_Y
    Bundling_lvx_v1_TINY,	// Bundling_lvx_v1_TINY
    Bundling_lvx_v1_TINY,	// Bundling_lvx_v1_TINY_X
    Bundling_lvx_v1_TINY,	// Bundling_lvx_v1_TINY_Y
    Bundling_lvx_v1_EXT,	// Bundling_lvx_v1_EXT
    Bundling_lvx_v1_NOP,	// Bundling_lvx_v1_NOP
  };
  return base_bundlings[bundling];
};

typedef enum {
  Resource_lvx_v1_ISSUE,
  Resource_lvx_v1_TINY,
  Resource_lvx_v1_LITE,
  Resource_lvx_v1_FULL,
  Resource_lvx_v1_LSU,
  Resource_lvx_v1_MAU,
  Resource_lvx_v1_BCU,
  Resource_lvx_v1_EXT,
  Resource_lvx_v1_AUXR,
  Resource_lvx_v1_AUXW,
  Resource_lvx_v1_XFER,
  Resource_lvx_v1_MEMW,
  Resource_lvx_v1_SR12,
  Resource_lvx_v1_SR13,
  Resource_lvx_v1_SR14,
  Resource_lvx_v1_SR15,
} Resource_lvx_v1;
#define lvx_v1_RESOURCE_COUNT 16

typedef enum {
  Reservation_lvx_v1_ALL,
  Reservation_lvx_v1_ALU_TINY,
  Reservation_lvx_v1_ALU_TINY_X,
  Reservation_lvx_v1_ALU_TINY_Y,
  Reservation_lvx_v1_ALU_TINY_AUXR,
  Reservation_lvx_v1_ALU_LITE,
  Reservation_lvx_v1_ALU_LITE_X,
  Reservation_lvx_v1_ALU_LITE_Y,
  Reservation_lvx_v1_ALU_LITE_MISC,
  Reservation_lvx_v1_ALU_FULL,
  Reservation_lvx_v1_ALU_FULL_X,
  Reservation_lvx_v1_ALU_FULL_Y,
  Reservation_lvx_v1_BCU,
  Reservation_lvx_v1_BCU_BRRP,
  Reservation_lvx_v1_BCU_BRRP2,
  Reservation_lvx_v1_BCU2,
  Reservation_lvx_v1_BCU2_X,
  Reservation_lvx_v1_BCU_XFER,
  Reservation_lvx_v1_BCU_XFER_BRRP,
  Reservation_lvx_v1_BCU2_TINY_LSU,
  Reservation_lvx_v1_LSU,
  Reservation_lvx_v1_LSU_X,
  Reservation_lvx_v1_LSU_Y,
  Reservation_lvx_v1_LSU_MEMW_ACCR,
  Reservation_lvx_v1_LSU_MEMW_ACCR_X,
  Reservation_lvx_v1_LSU_MEMW_ACCR_Y,
  Reservation_lvx_v1_LSU2_MEMW,
  Reservation_lvx_v1_LSU2_MEMW_X,
  Reservation_lvx_v1_LSU2_MEMW_Y,
  Reservation_lvx_v1_LSU_AUXR,
  Reservation_lvx_v1_LSU_AUXR_X,
  Reservation_lvx_v1_LSU_AUXR_Y,
  Reservation_lvx_v1_LSU_MEMW_AUXR,
  Reservation_lvx_v1_LSU_MEMW_AUXR_X,
  Reservation_lvx_v1_LSU_MEMW_AUXR_Y,
  Reservation_lvx_v1_LSU2_MEMW_AUXR,
  Reservation_lvx_v1_LSU2_MEMW_AUXR_X,
  Reservation_lvx_v1_LSU2_MEMW_AUXR_Y,
  Reservation_lvx_v1_LSU_MEMW_AUXW,
  Reservation_lvx_v1_LSU_MEMW_AUXW_X,
  Reservation_lvx_v1_LSU_MEMW_AUXW_Y,
  Reservation_lvx_v1_LSU2_MEMW_AUXW,
  Reservation_lvx_v1_LSU2_MEMW_AUXW_X,
  Reservation_lvx_v1_LSU2_MEMW_AUXW_Y,
  Reservation_lvx_v1_LSU_AUXW,
  Reservation_lvx_v1_LSU_AUXW_X,
  Reservation_lvx_v1_LSU_AUXW_Y,
  Reservation_lvx_v1_LSU_AUXR_AUXW,
  Reservation_lvx_v1_LSU_AUXR_AUXW_X,
  Reservation_lvx_v1_LSU_AUXR_AUXW_Y,
  Reservation_lvx_v1_LSU2_MEMW_AUXR_AUXW,
  Reservation_lvx_v1_LSU2_MEMW_AUXR_AUXW_X,
  Reservation_lvx_v1_LSU2_MEMW_AUXR_AUXW_Y,
  Reservation_lvx_v1_MAU,
  Reservation_lvx_v1_EXT,
  Reservation_lvx_v1_EXT_COMP,
  Reservation_lvx_v1_EXT_MISC,
  Reservation_lvx_v1_EXT_MISC_AUXW,
} Reservation_lvx_v1;

extern struct lvx_reloc lvx_v1_rel16_reloc;
extern struct lvx_reloc lvx_v1_rel32_reloc;
extern struct lvx_reloc lvx_v1_rel64_reloc;
extern struct lvx_reloc lvx_v1_pcrel_signed16_reloc;
extern struct lvx_reloc lvx_v1_pcrel32_reloc;
extern struct lvx_reloc lvx_v1_pcrel_signed37_reloc;
extern struct lvx_reloc lvx_v1_pcrel_signed43_reloc;
extern struct lvx_reloc lvx_v1_pcrel_signed64_reloc;
extern struct lvx_reloc lvx_v1_pcrel64_reloc;
extern struct lvx_reloc lvx_v1_signed16_reloc;
extern struct lvx_reloc lvx_v1_signed32_reloc;
extern struct lvx_reloc lvx_v1_signed37_reloc;
extern struct lvx_reloc lvx_v1_gotoff_signed37_reloc;
extern struct lvx_reloc lvx_v1_gotoff_signed43_reloc;
extern struct lvx_reloc lvx_v1_gotoff_32_reloc;
extern struct lvx_reloc lvx_v1_gotoff_64_reloc;
extern struct lvx_reloc lvx_v1_got_32_reloc;
extern struct lvx_reloc lvx_v1_got_signed37_reloc;
extern struct lvx_reloc lvx_v1_got_signed43_reloc;
extern struct lvx_reloc lvx_v1_got_64_reloc;
extern struct lvx_reloc lvx_v1_glob_dat_reloc;
extern struct lvx_reloc lvx_v1_copy_reloc;
extern struct lvx_reloc lvx_v1_jump_slot_reloc;
extern struct lvx_reloc lvx_v1_relative_reloc;
extern struct lvx_reloc lvx_v1_signed43_reloc;
extern struct lvx_reloc lvx_v1_signed64_reloc;
extern struct lvx_reloc lvx_v1_gotaddr_signed37_reloc;
extern struct lvx_reloc lvx_v1_gotaddr_signed43_reloc;
extern struct lvx_reloc lvx_v1_gotaddr_signed64_reloc;
extern struct lvx_reloc lvx_v1_dtpmod64_reloc;
extern struct lvx_reloc lvx_v1_dtpoff64_reloc;
extern struct lvx_reloc lvx_v1_dtpoff_signed37_reloc;
extern struct lvx_reloc lvx_v1_dtpoff_signed43_reloc;
extern struct lvx_reloc lvx_v1_tlsgd_signed37_reloc;
extern struct lvx_reloc lvx_v1_tlsgd_signed43_reloc;
extern struct lvx_reloc lvx_v1_tlsld_signed37_reloc;
extern struct lvx_reloc lvx_v1_tlsld_signed43_reloc;
extern struct lvx_reloc lvx_v1_tpoff64_reloc;
extern struct lvx_reloc lvx_v1_tlsie_signed37_reloc;
extern struct lvx_reloc lvx_v1_tlsie_signed43_reloc;
extern struct lvx_reloc lvx_v1_tlsle_signed37_reloc;
extern struct lvx_reloc lvx_v1_tlsle_signed43_reloc;
extern struct lvx_reloc lvx_v1_rel8_reloc;
extern struct lvx_reloc lvx_v1_pcrel11s2_reloc;
extern struct lvx_reloc lvx_v1_pcrel17s2_reloc;
extern struct lvx_reloc lvx_v1_pcrel27s2_reloc;
extern struct lvx_reloc lvx_v1_pcrel38s2_reloc;
extern struct lvx_reloc lvx_v1_pcrel44s2_reloc;
extern struct lvx_reloc lvx_v1_pcrel54s2_reloc;

#define LVX_V2_REGFILE_FIRST_SFR LVX_REGFILE_FIRST_SFR
#define LVX_V2_REGFILE_LAST_SFR LVX_REGFILE_LAST_SFR
#define LVX_V2_REGFILE_DEC_SFR LVX_REGFILE_DEC_SFR
#define LVX_V2_REGFILE_FIRST_GPR LVX_REGFILE_FIRST_GPR
#define LVX_V2_REGFILE_LAST_GPR LVX_REGFILE_LAST_GPR
#define LVX_V2_REGFILE_DEC_GPR LVX_REGFILE_DEC_GPR
#define LVX_V2_REGFILE_FIRST_PGR 6
#define LVX_V2_REGFILE_LAST_PGR 7
#define LVX_V2_REGFILE_DEC_PGR 8
#define LVX_V2_REGFILE_FIRST_QGR 9
#define LVX_V2_REGFILE_LAST_QGR 10
#define LVX_V2_REGFILE_DEC_QGR 11
#define LVX_V2_REGFILE_FIRST_RV_BIR 12
#define LVX_V2_REGFILE_LAST_RV_BIR 13
#define LVX_V2_REGFILE_DEC_RV_BIR 14
#define LVX_V2_REGFILE_FIRST_RV_BIRP 15
#define LVX_V2_REGFILE_LAST_RV_BIRP 16
#define LVX_V2_REGFILE_DEC_RV_BIRP 17
#define LVX_V2_REGFILE_FIRST_RV_FPR 18
#define LVX_V2_REGFILE_LAST_RV_FPR 19
#define LVX_V2_REGFILE_DEC_RV_FPR 20
#define LVX_V2_REGFILE_FIRST_X16R 21
#define LVX_V2_REGFILE_LAST_X16R 22
#define LVX_V2_REGFILE_DEC_X16R 23
#define LVX_V2_REGFILE_FIRST_X2R 24
#define LVX_V2_REGFILE_LAST_X2R 25
#define LVX_V2_REGFILE_DEC_X2R 26
#define LVX_V2_REGFILE_FIRST_X32R 27
#define LVX_V2_REGFILE_LAST_X32R 28
#define LVX_V2_REGFILE_DEC_X32R 29
#define LVX_V2_REGFILE_FIRST_X4R 30
#define LVX_V2_REGFILE_LAST_X4R 31
#define LVX_V2_REGFILE_DEC_X4R 32
#define LVX_V2_REGFILE_FIRST_X64R 33
#define LVX_V2_REGFILE_LAST_X64R 34
#define LVX_V2_REGFILE_DEC_X64R 35
#define LVX_V2_REGFILE_FIRST_X8R 36
#define LVX_V2_REGFILE_LAST_X8R 37
#define LVX_V2_REGFILE_DEC_X8R 38
#define LVX_V2_REGFILE_FIRST_XBR 39
#define LVX_V2_REGFILE_LAST_XBR 40
#define LVX_V2_REGFILE_DEC_XBR 41
#define LVX_V2_REGFILE_FIRST_XCR 42
#define LVX_V2_REGFILE_LAST_XCR 43
#define LVX_V2_REGFILE_DEC_XCR 44
#define LVX_V2_REGFILE_FIRST_XMR 45
#define LVX_V2_REGFILE_LAST_XMR 46
#define LVX_V2_REGFILE_DEC_XMR 47
#define LVX_V2_REGFILE_FIRST_XTR 48
#define LVX_V2_REGFILE_LAST_XTR 49
#define LVX_V2_REGFILE_DEC_XTR 50
#define LVX_V2_REGFILE_FIRST_XVR 51
#define LVX_V2_REGFILE_LAST_XVR 52
#define LVX_V2_REGFILE_DEC_XVR 53
#define LVX_V2_REGFILE_REGISTERS 54
#define LVX_V2_REGFILE_DEC_REGISTERS 55


extern int lvx_v2_regfiles[];
extern const char **lvx_v2_modifiers[];
extern struct lvx_register lvx_v2_registers[];

extern int lvx_v2_dec_registers[];

enum Method_lvx_v2_enum {
  Immediate_lvx_v2_brknumber = 1,
  Immediate_lvx_v2_pcrel11s2 = 2,
  Immediate_lvx_v2_pcrel17s2 = 3,
  Immediate_lvx_v2_pcrel27s2 = 4,
  Immediate_lvx_v2_pcrel38s2 = 5,
  Immediate_lvx_v2_pcrel44s2 = 6,
  Immediate_lvx_v2_pcrel54s2 = 7,
  Immediate_lvx_v2_signed10 = 8,
  Immediate_lvx_v2_signed16 = 9,
  Immediate_lvx_v2_signed27 = 10,
  Immediate_lvx_v2_signed37 = 11,
  Immediate_lvx_v2_signed43 = 12,
  Immediate_lvx_v2_signed54 = 13,
  Immediate_lvx_v2_sysnumber = 14,
  Immediate_lvx_v2_unsigned6 = 15,
  Immediate_lvx_v2_wrapped32 = 16,
  Immediate_lvx_v2_wrapped64 = 17,
  Immediate_lvx_v2_wrapped8 = 18,
  Modifier_lvx_v2_accesses = 19,
  Modifier_lvx_v2_bcucond = 20,
  Modifier_lvx_v2_boolcas = 21,
  Modifier_lvx_v2_cachelev = 22,
  Modifier_lvx_v2_ccbcomp = 23,
  Modifier_lvx_v2_channel = 24,
  Modifier_lvx_v2_coherency = 25,
  Modifier_lvx_v2_conjugate = 26,
  Modifier_lvx_v2_doscale = 27,
  Modifier_lvx_v2_exunum = 28,
  Modifier_lvx_v2_floatcomp = 29,
  Modifier_lvx_v2_floatmode = 30,
  Modifier_lvx_v2_fnegate = 31,
  Modifier_lvx_v2_highmult = 32,
  Modifier_lvx_v2_hindex = 33,
  Modifier_lvx_v2_imultiply = 34,
  Modifier_lvx_v2_intcomp = 35,
  Modifier_lvx_v2_lanecond = 36,
  Modifier_lvx_v2_lanesize = 37,
  Modifier_lvx_v2_lanetodo = 38,
  Modifier_lvx_v2_mostsig = 39,
  Modifier_lvx_v2_oddlanes = 40,
  Modifier_lvx_v2_qindex = 41,
  Modifier_lvx_v2_realimag = 42,
  Modifier_lvx_v2_shuffleV = 43,
  Modifier_lvx_v2_shuffleX = 44,
  Modifier_lvx_v2_signextw = 45,
  Modifier_lvx_v2_speculate = 46,
  Modifier_lvx_v2_splat32 = 47,
  Modifier_lvx_v2_variant = 48,
  Modifier_lvx_v2_widemult = 49,
  Modifier_lvx_v2_ziplanes = 50,
  RegClass_lvx_v2_aloneReg = 51,
  RegClass_lvx_v2_buffer16Reg = 52,
  RegClass_lvx_v2_buffer2Reg = 53,
  RegClass_lvx_v2_buffer32Reg = 54,
  RegClass_lvx_v2_buffer4Reg = 55,
  RegClass_lvx_v2_buffer64Reg = 56,
  RegClass_lvx_v2_buffer8Reg = 57,
  RegClass_lvx_v2_floatReg = 58,
  RegClass_lvx_v2_mainReg = 59,
  RegClass_lvx_v2_mainRegPair = 60,
  RegClass_lvx_v2_onlyfxReg = 61,
  RegClass_lvx_v2_onlygetReg = 62,
  RegClass_lvx_v2_onlyraReg = 63,
  RegClass_lvx_v2_onlysetReg = 64,
  RegClass_lvx_v2_onlyswapReg = 65,
  RegClass_lvx_v2_pairedReg = 66,
  RegClass_lvx_v2_quadReg = 67,
  RegClass_lvx_v2_singleReg = 68,
  RegClass_lvx_v2_systemReg = 69,
  RegClass_lvx_v2_worddRegE = 70,
  RegClass_lvx_v2_worddRegO = 71,
  RegClass_lvx_v2_xworddReg = 72,
  RegClass_lvx_v2_xworddReg0M4 = 73,
  RegClass_lvx_v2_xworddReg1M4 = 74,
  RegClass_lvx_v2_xworddReg2M4 = 75,
  RegClass_lvx_v2_xworddReg3M4 = 76,
  RegClass_lvx_v2_xwordoReg = 77,
  RegClass_lvx_v2_xwordqReg = 78,
  RegClass_lvx_v2_xwordqRegE = 79,
  RegClass_lvx_v2_xwordqRegO = 80,
  RegClass_lvx_v2_xwordvReg = 81,
  RegClass_lvx_v2_xwordxReg = 82,
  Instruction_lvx_v2_abdbx = 83,
  Instruction_lvx_v2_abdd = 84,
  Instruction_lvx_v2_abddp = 85,
  Instruction_lvx_v2_abdho = 86,
  Instruction_lvx_v2_abdsbx = 87,
  Instruction_lvx_v2_abdsd = 88,
  Instruction_lvx_v2_abdsdp = 89,
  Instruction_lvx_v2_abdsho = 90,
  Instruction_lvx_v2_abdsw = 91,
  Instruction_lvx_v2_abdswq = 92,
  Instruction_lvx_v2_abdubx = 93,
  Instruction_lvx_v2_abdud = 94,
  Instruction_lvx_v2_abdudp = 95,
  Instruction_lvx_v2_abduho = 96,
  Instruction_lvx_v2_abduw = 97,
  Instruction_lvx_v2_abduwq = 98,
  Instruction_lvx_v2_abdw = 99,
  Instruction_lvx_v2_abdwq = 100,
  Instruction_lvx_v2_absbx = 101,
  Instruction_lvx_v2_absd = 102,
  Instruction_lvx_v2_absdp = 103,
  Instruction_lvx_v2_absho = 104,
  Instruction_lvx_v2_abssbx = 105,
  Instruction_lvx_v2_abssd = 106,
  Instruction_lvx_v2_abssdp = 107,
  Instruction_lvx_v2_abssho = 108,
  Instruction_lvx_v2_abssw = 109,
  Instruction_lvx_v2_absswq = 110,
  Instruction_lvx_v2_absw = 111,
  Instruction_lvx_v2_abswq = 112,
  Instruction_lvx_v2_acswapb = 113,
  Instruction_lvx_v2_acswapd = 114,
  Instruction_lvx_v2_acswaph = 115,
  Instruction_lvx_v2_acswapq = 116,
  Instruction_lvx_v2_acswapw = 117,
  Instruction_lvx_v2_addbx = 118,
  Instruction_lvx_v2_addd = 119,
  Instruction_lvx_v2_adddp = 120,
  Instruction_lvx_v2_addho = 121,
  Instruction_lvx_v2_addq = 122,
  Instruction_lvx_v2_addsbx = 123,
  Instruction_lvx_v2_addsd = 124,
  Instruction_lvx_v2_addsdp = 125,
  Instruction_lvx_v2_addsho = 126,
  Instruction_lvx_v2_addsw = 127,
  Instruction_lvx_v2_addswq = 128,
  Instruction_lvx_v2_addusbx = 129,
  Instruction_lvx_v2_addusd = 130,
  Instruction_lvx_v2_addusdp = 131,
  Instruction_lvx_v2_addusho = 132,
  Instruction_lvx_v2_addusw = 133,
  Instruction_lvx_v2_adduswq = 134,
  Instruction_lvx_v2_addw = 135,
  Instruction_lvx_v2_addwq = 136,
  Instruction_lvx_v2_addx16bx = 137,
  Instruction_lvx_v2_addx16d = 138,
  Instruction_lvx_v2_addx16dp = 139,
  Instruction_lvx_v2_addx16ho = 140,
  Instruction_lvx_v2_addx16w = 141,
  Instruction_lvx_v2_addx16wq = 142,
  Instruction_lvx_v2_addx2bx = 143,
  Instruction_lvx_v2_addx2d = 144,
  Instruction_lvx_v2_addx2dp = 145,
  Instruction_lvx_v2_addx2ho = 146,
  Instruction_lvx_v2_addx2w = 147,
  Instruction_lvx_v2_addx2wq = 148,
  Instruction_lvx_v2_addx32d = 149,
  Instruction_lvx_v2_addx32w = 150,
  Instruction_lvx_v2_addx4bx = 151,
  Instruction_lvx_v2_addx4d = 152,
  Instruction_lvx_v2_addx4dp = 153,
  Instruction_lvx_v2_addx4ho = 154,
  Instruction_lvx_v2_addx4w = 155,
  Instruction_lvx_v2_addx4wq = 156,
  Instruction_lvx_v2_addx64d = 157,
  Instruction_lvx_v2_addx64w = 158,
  Instruction_lvx_v2_addx8bx = 159,
  Instruction_lvx_v2_addx8d = 160,
  Instruction_lvx_v2_addx8dp = 161,
  Instruction_lvx_v2_addx8ho = 162,
  Instruction_lvx_v2_addx8w = 163,
  Instruction_lvx_v2_addx8wq = 164,
  Instruction_lvx_v2_aladdb = 165,
  Instruction_lvx_v2_aladdd = 166,
  Instruction_lvx_v2_aladdh = 167,
  Instruction_lvx_v2_aladdw = 168,
  Instruction_lvx_v2_alandb = 169,
  Instruction_lvx_v2_alandd = 170,
  Instruction_lvx_v2_alandh = 171,
  Instruction_lvx_v2_alandw = 172,
  Instruction_lvx_v2_alb = 173,
  Instruction_lvx_v2_alclrb = 174,
  Instruction_lvx_v2_alclrd = 175,
  Instruction_lvx_v2_alclrh = 176,
  Instruction_lvx_v2_alclrw = 177,
  Instruction_lvx_v2_ald = 178,
  Instruction_lvx_v2_aldusb = 179,
  Instruction_lvx_v2_aldusd = 180,
  Instruction_lvx_v2_aldush = 181,
  Instruction_lvx_v2_aldusw = 182,
  Instruction_lvx_v2_aleorb = 183,
  Instruction_lvx_v2_aleord = 184,
  Instruction_lvx_v2_aleorh = 185,
  Instruction_lvx_v2_aleorw = 186,
  Instruction_lvx_v2_alh = 187,
  Instruction_lvx_v2_aliorb = 188,
  Instruction_lvx_v2_aliord = 189,
  Instruction_lvx_v2_aliorh = 190,
  Instruction_lvx_v2_aliorw = 191,
  Instruction_lvx_v2_almaxb = 192,
  Instruction_lvx_v2_almaxd = 193,
  Instruction_lvx_v2_almaxh = 194,
  Instruction_lvx_v2_almaxub = 195,
  Instruction_lvx_v2_almaxud = 196,
  Instruction_lvx_v2_almaxuh = 197,
  Instruction_lvx_v2_almaxuw = 198,
  Instruction_lvx_v2_almaxw = 199,
  Instruction_lvx_v2_alminb = 200,
  Instruction_lvx_v2_almind = 201,
  Instruction_lvx_v2_alminh = 202,
  Instruction_lvx_v2_alminub = 203,
  Instruction_lvx_v2_alminud = 204,
  Instruction_lvx_v2_alminuh = 205,
  Instruction_lvx_v2_alminuw = 206,
  Instruction_lvx_v2_alminw = 207,
  Instruction_lvx_v2_alw = 208,
  Instruction_lvx_v2_andd = 209,
  Instruction_lvx_v2_andnd = 210,
  Instruction_lvx_v2_andnq = 211,
  Instruction_lvx_v2_andnw = 212,
  Instruction_lvx_v2_andq = 213,
  Instruction_lvx_v2_andw = 214,
  Instruction_lvx_v2_asaddb = 215,
  Instruction_lvx_v2_asaddd = 216,
  Instruction_lvx_v2_asaddh = 217,
  Instruction_lvx_v2_asaddw = 218,
  Instruction_lvx_v2_asandb = 219,
  Instruction_lvx_v2_asandd = 220,
  Instruction_lvx_v2_asandh = 221,
  Instruction_lvx_v2_asandw = 222,
  Instruction_lvx_v2_asb = 223,
  Instruction_lvx_v2_asd = 224,
  Instruction_lvx_v2_asdusb = 225,
  Instruction_lvx_v2_asdusd = 226,
  Instruction_lvx_v2_asdush = 227,
  Instruction_lvx_v2_asdusw = 228,
  Instruction_lvx_v2_aseorb = 229,
  Instruction_lvx_v2_aseord = 230,
  Instruction_lvx_v2_aseorh = 231,
  Instruction_lvx_v2_aseorw = 232,
  Instruction_lvx_v2_ash = 233,
  Instruction_lvx_v2_asiorb = 234,
  Instruction_lvx_v2_asiord = 235,
  Instruction_lvx_v2_asiorh = 236,
  Instruction_lvx_v2_asiorw = 237,
  Instruction_lvx_v2_asmaxb = 238,
  Instruction_lvx_v2_asmaxd = 239,
  Instruction_lvx_v2_asmaxh = 240,
  Instruction_lvx_v2_asmaxub = 241,
  Instruction_lvx_v2_asmaxud = 242,
  Instruction_lvx_v2_asmaxuh = 243,
  Instruction_lvx_v2_asmaxuw = 244,
  Instruction_lvx_v2_asmaxw = 245,
  Instruction_lvx_v2_asminb = 246,
  Instruction_lvx_v2_asmind = 247,
  Instruction_lvx_v2_asminh = 248,
  Instruction_lvx_v2_asminub = 249,
  Instruction_lvx_v2_asminud = 250,
  Instruction_lvx_v2_asminuh = 251,
  Instruction_lvx_v2_asminuw = 252,
  Instruction_lvx_v2_asminw = 253,
  Instruction_lvx_v2_asw = 254,
  Instruction_lvx_v2_aswapb = 255,
  Instruction_lvx_v2_aswapd = 256,
  Instruction_lvx_v2_aswaph = 257,
  Instruction_lvx_v2_aswapw = 258,
  Instruction_lvx_v2_avgbx = 259,
  Instruction_lvx_v2_avgho = 260,
  Instruction_lvx_v2_avgrbx = 261,
  Instruction_lvx_v2_avgrho = 262,
  Instruction_lvx_v2_avgrubx = 263,
  Instruction_lvx_v2_avgruho = 264,
  Instruction_lvx_v2_avgruw = 265,
  Instruction_lvx_v2_avgruwq = 266,
  Instruction_lvx_v2_avgrw = 267,
  Instruction_lvx_v2_avgrwq = 268,
  Instruction_lvx_v2_avgubx = 269,
  Instruction_lvx_v2_avguho = 270,
  Instruction_lvx_v2_avguw = 271,
  Instruction_lvx_v2_avguwq = 272,
  Instruction_lvx_v2_avgw = 273,
  Instruction_lvx_v2_avgwq = 274,
  Instruction_lvx_v2_await = 275,
  Instruction_lvx_v2_barrier = 276,
  Instruction_lvx_v2_blend = 277,
  Instruction_lvx_v2_break = 278,
  Instruction_lvx_v2_call = 279,
  Instruction_lvx_v2_callx = 280,
  Instruction_lvx_v2_catdq = 281,
  Instruction_lvx_v2_cb = 282,
  Instruction_lvx_v2_cbsd = 283,
  Instruction_lvx_v2_cbsdp = 284,
  Instruction_lvx_v2_cbsho = 285,
  Instruction_lvx_v2_cbsw = 286,
  Instruction_lvx_v2_cbswq = 287,
  Instruction_lvx_v2_cbx = 288,
  Instruction_lvx_v2_ccb = 289,
  Instruction_lvx_v2_ccbx = 290,
  Instruction_lvx_v2_clsd = 291,
  Instruction_lvx_v2_clsdp = 292,
  Instruction_lvx_v2_clsho = 293,
  Instruction_lvx_v2_clsw = 294,
  Instruction_lvx_v2_clswq = 295,
  Instruction_lvx_v2_clzd = 296,
  Instruction_lvx_v2_clzdp = 297,
  Instruction_lvx_v2_clzho = 298,
  Instruction_lvx_v2_clzw = 299,
  Instruction_lvx_v2_clzwq = 300,
  Instruction_lvx_v2_cmovebx = 301,
  Instruction_lvx_v2_cmoved = 302,
  Instruction_lvx_v2_cmovedp = 303,
  Instruction_lvx_v2_cmoveho = 304,
  Instruction_lvx_v2_cmoveq = 305,
  Instruction_lvx_v2_cmovewq = 306,
  Instruction_lvx_v2_compbx = 307,
  Instruction_lvx_v2_compd = 308,
  Instruction_lvx_v2_compdp = 309,
  Instruction_lvx_v2_compho = 310,
  Instruction_lvx_v2_compnbx = 311,
  Instruction_lvx_v2_compndp = 312,
  Instruction_lvx_v2_compnho = 313,
  Instruction_lvx_v2_compnwq = 314,
  Instruction_lvx_v2_compq = 315,
  Instruction_lvx_v2_compw = 316,
  Instruction_lvx_v2_compwq = 317,
  Instruction_lvx_v2_copyd = 318,
  Instruction_lvx_v2_copyo = 319,
  Instruction_lvx_v2_copyq = 320,
  Instruction_lvx_v2_copyw = 321,
  Instruction_lvx_v2_crcbellw = 322,
  Instruction_lvx_v2_crcbelmw = 323,
  Instruction_lvx_v2_crclellw = 324,
  Instruction_lvx_v2_crclelmw = 325,
  Instruction_lvx_v2_ctzd = 326,
  Instruction_lvx_v2_ctzdp = 327,
  Instruction_lvx_v2_ctzho = 328,
  Instruction_lvx_v2_ctzw = 329,
  Instruction_lvx_v2_ctzwq = 330,
  Instruction_lvx_v2_d1inval = 331,
  Instruction_lvx_v2_dflushl = 332,
  Instruction_lvx_v2_dflushsw = 333,
  Instruction_lvx_v2_dinvall = 334,
  Instruction_lvx_v2_dinvalsw = 335,
  Instruction_lvx_v2_divmodd = 336,
  Instruction_lvx_v2_divmodud = 337,
  Instruction_lvx_v2_divmoduw = 338,
  Instruction_lvx_v2_divmodw = 339,
  Instruction_lvx_v2_dpurgel = 340,
  Instruction_lvx_v2_dpurgesw = 341,
  Instruction_lvx_v2_dtouchl = 342,
  Instruction_lvx_v2_eord = 343,
  Instruction_lvx_v2_eorq = 344,
  Instruction_lvx_v2_eorw = 345,
  Instruction_lvx_v2_errop = 346,
  Instruction_lvx_v2_extfs = 347,
  Instruction_lvx_v2_extfz = 348,
  Instruction_lvx_v2_extlqbho = 349,
  Instruction_lvx_v2_extlqhwq = 350,
  Instruction_lvx_v2_extlqnbx = 351,
  Instruction_lvx_v2_extlqwdp = 352,
  Instruction_lvx_v2_extlsbho = 353,
  Instruction_lvx_v2_extlshwq = 354,
  Instruction_lvx_v2_extlsnbx = 355,
  Instruction_lvx_v2_extlswdp = 356,
  Instruction_lvx_v2_extlzbho = 357,
  Instruction_lvx_v2_extlzhwq = 358,
  Instruction_lvx_v2_extlznbx = 359,
  Instruction_lvx_v2_extlzwdp = 360,
  Instruction_lvx_v2_fabsd = 361,
  Instruction_lvx_v2_fabsdp = 362,
  Instruction_lvx_v2_fabsh = 363,
  Instruction_lvx_v2_fabsho = 364,
  Instruction_lvx_v2_fabsw = 365,
  Instruction_lvx_v2_fabswp = 366,
  Instruction_lvx_v2_fabswq = 367,
  Instruction_lvx_v2_faddd = 368,
  Instruction_lvx_v2_fadddp = 369,
  Instruction_lvx_v2_faddh = 370,
  Instruction_lvx_v2_faddho = 371,
  Instruction_lvx_v2_faddw = 372,
  Instruction_lvx_v2_faddwc = 373,
  Instruction_lvx_v2_faddwq = 374,
  Instruction_lvx_v2_fclassd = 375,
  Instruction_lvx_v2_fclassh = 376,
  Instruction_lvx_v2_fclassw = 377,
  Instruction_lvx_v2_fclasswp = 378,
  Instruction_lvx_v2_fclasswq = 379,
  Instruction_lvx_v2_fcompd = 380,
  Instruction_lvx_v2_fcompdp = 381,
  Instruction_lvx_v2_fcomph = 382,
  Instruction_lvx_v2_fcompho = 383,
  Instruction_lvx_v2_fcompndp = 384,
  Instruction_lvx_v2_fcompnho = 385,
  Instruction_lvx_v2_fcompnwq = 386,
  Instruction_lvx_v2_fcompw = 387,
  Instruction_lvx_v2_fcompwq = 388,
  Instruction_lvx_v2_fdivd = 389,
  Instruction_lvx_v2_fdivh = 390,
  Instruction_lvx_v2_fdivw = 391,
  Instruction_lvx_v2_fence = 392,
  Instruction_lvx_v2_fextlhwq = 393,
  Instruction_lvx_v2_ffmad = 394,
  Instruction_lvx_v2_ffmadp = 395,
  Instruction_lvx_v2_ffmah = 396,
  Instruction_lvx_v2_ffmaho = 397,
  Instruction_lvx_v2_ffmaw = 398,
  Instruction_lvx_v2_ffmawc = 399,
  Instruction_lvx_v2_ffmawq = 400,
  Instruction_lvx_v2_ffmsd = 401,
  Instruction_lvx_v2_ffmsdp = 402,
  Instruction_lvx_v2_ffmsh = 403,
  Instruction_lvx_v2_ffmsho = 404,
  Instruction_lvx_v2_ffmsw = 405,
  Instruction_lvx_v2_ffmswq = 406,
  Instruction_lvx_v2_fixedd = 407,
  Instruction_lvx_v2_fixeddp = 408,
  Instruction_lvx_v2_fixeddw = 409,
  Instruction_lvx_v2_fixedho = 410,
  Instruction_lvx_v2_fixedud = 411,
  Instruction_lvx_v2_fixedudp = 412,
  Instruction_lvx_v2_fixedudw = 413,
  Instruction_lvx_v2_fixeduho = 414,
  Instruction_lvx_v2_fixeduw = 415,
  Instruction_lvx_v2_fixeduwd = 416,
  Instruction_lvx_v2_fixeduwp = 417,
  Instruction_lvx_v2_fixeduwq = 418,
  Instruction_lvx_v2_fixedw = 419,
  Instruction_lvx_v2_fixedwd = 420,
  Instruction_lvx_v2_fixedwp = 421,
  Instruction_lvx_v2_fixedwq = 422,
  Instruction_lvx_v2_floatd = 423,
  Instruction_lvx_v2_floatdp = 424,
  Instruction_lvx_v2_floatdw = 425,
  Instruction_lvx_v2_floatho = 426,
  Instruction_lvx_v2_floatud = 427,
  Instruction_lvx_v2_floatudp = 428,
  Instruction_lvx_v2_floatudw = 429,
  Instruction_lvx_v2_floatuho = 430,
  Instruction_lvx_v2_floatuw = 431,
  Instruction_lvx_v2_floatuwd = 432,
  Instruction_lvx_v2_floatuwp = 433,
  Instruction_lvx_v2_floatuwq = 434,
  Instruction_lvx_v2_floatw = 435,
  Instruction_lvx_v2_floatwd = 436,
  Instruction_lvx_v2_floatwp = 437,
  Instruction_lvx_v2_floatwq = 438,
  Instruction_lvx_v2_fmaxd = 439,
  Instruction_lvx_v2_fmaxdp = 440,
  Instruction_lvx_v2_fmaxh = 441,
  Instruction_lvx_v2_fmaxho = 442,
  Instruction_lvx_v2_fmaxnd = 443,
  Instruction_lvx_v2_fmaxndp = 444,
  Instruction_lvx_v2_fmaxnh = 445,
  Instruction_lvx_v2_fmaxnho = 446,
  Instruction_lvx_v2_fmaxnw = 447,
  Instruction_lvx_v2_fmaxnwq = 448,
  Instruction_lvx_v2_fmaxw = 449,
  Instruction_lvx_v2_fmaxwq = 450,
  Instruction_lvx_v2_fmind = 451,
  Instruction_lvx_v2_fmindp = 452,
  Instruction_lvx_v2_fminh = 453,
  Instruction_lvx_v2_fminho = 454,
  Instruction_lvx_v2_fminnd = 455,
  Instruction_lvx_v2_fminndp = 456,
  Instruction_lvx_v2_fminnh = 457,
  Instruction_lvx_v2_fminnho = 458,
  Instruction_lvx_v2_fminnw = 459,
  Instruction_lvx_v2_fminnwq = 460,
  Instruction_lvx_v2_fminw = 461,
  Instruction_lvx_v2_fminwq = 462,
  Instruction_lvx_v2_fmuld = 463,
  Instruction_lvx_v2_fmuldp = 464,
  Instruction_lvx_v2_fmulh = 465,
  Instruction_lvx_v2_fmulho = 466,
  Instruction_lvx_v2_fmulw = 467,
  Instruction_lvx_v2_fmulwc = 468,
  Instruction_lvx_v2_fmulwq = 469,
  Instruction_lvx_v2_fnarrowdw = 470,
  Instruction_lvx_v2_fnarrowdwq = 471,
  Instruction_lvx_v2_fnarrowwh = 472,
  Instruction_lvx_v2_fnarrowwhq = 473,
  Instruction_lvx_v2_fnegd = 474,
  Instruction_lvx_v2_fnegdp = 475,
  Instruction_lvx_v2_fnegh = 476,
  Instruction_lvx_v2_fnegho = 477,
  Instruction_lvx_v2_fnegw = 478,
  Instruction_lvx_v2_fnegwp = 479,
  Instruction_lvx_v2_fnegwq = 480,
  Instruction_lvx_v2_fractdwq = 481,
  Instruction_lvx_v2_fracthbx = 482,
  Instruction_lvx_v2_fractwho = 483,
  Instruction_lvx_v2_frintd = 484,
  Instruction_lvx_v2_frinth = 485,
  Instruction_lvx_v2_frintw = 486,
  Instruction_lvx_v2_fsbfd = 487,
  Instruction_lvx_v2_fsbfdp = 488,
  Instruction_lvx_v2_fsbfh = 489,
  Instruction_lvx_v2_fsbfho = 490,
  Instruction_lvx_v2_fsbfw = 491,
  Instruction_lvx_v2_fsbfwc = 492,
  Instruction_lvx_v2_fsbfwq = 493,
  Instruction_lvx_v2_fsignd = 494,
  Instruction_lvx_v2_fsigndp = 495,
  Instruction_lvx_v2_fsignh = 496,
  Instruction_lvx_v2_fsignho = 497,
  Instruction_lvx_v2_fsignmd = 498,
  Instruction_lvx_v2_fsignmdp = 499,
  Instruction_lvx_v2_fsignmh = 500,
  Instruction_lvx_v2_fsignmho = 501,
  Instruction_lvx_v2_fsignmw = 502,
  Instruction_lvx_v2_fsignmwq = 503,
  Instruction_lvx_v2_fsignnd = 504,
  Instruction_lvx_v2_fsignndp = 505,
  Instruction_lvx_v2_fsignnh = 506,
  Instruction_lvx_v2_fsignnho = 507,
  Instruction_lvx_v2_fsignnw = 508,
  Instruction_lvx_v2_fsignnwq = 509,
  Instruction_lvx_v2_fsignw = 510,
  Instruction_lvx_v2_fsignwq = 511,
  Instruction_lvx_v2_fsqrtd = 512,
  Instruction_lvx_v2_fsqrth = 513,
  Instruction_lvx_v2_fsqrtw = 514,
  Instruction_lvx_v2_fsrecd = 515,
  Instruction_lvx_v2_fsrecw = 516,
  Instruction_lvx_v2_fsrecwp = 517,
  Instruction_lvx_v2_fsrecwq = 518,
  Instruction_lvx_v2_fsrsrd = 519,
  Instruction_lvx_v2_fsrsrw = 520,
  Instruction_lvx_v2_fsrsrwp = 521,
  Instruction_lvx_v2_fsrsrwq = 522,
  Instruction_lvx_v2_fwidenhw = 523,
  Instruction_lvx_v2_fwidenhwq = 524,
  Instruction_lvx_v2_fwidenwd = 525,
  Instruction_lvx_v2_fwidenwdp = 526,
  Instruction_lvx_v2_get = 527,
  Instruction_lvx_v2_goto = 528,
  Instruction_lvx_v2_gotox = 529,
  Instruction_lvx_v2_guard = 530,
  Instruction_lvx_v2_i1inval = 531,
  Instruction_lvx_v2_i1invals = 532,
  Instruction_lvx_v2_icall = 533,
  Instruction_lvx_v2_iget = 534,
  Instruction_lvx_v2_igoto = 535,
  Instruction_lvx_v2_insf = 536,
  Instruction_lvx_v2_iord = 537,
  Instruction_lvx_v2_iornd = 538,
  Instruction_lvx_v2_iornq = 539,
  Instruction_lvx_v2_iornw = 540,
  Instruction_lvx_v2_iorq = 541,
  Instruction_lvx_v2_iorw = 542,
  Instruction_lvx_v2_landd = 543,
  Instruction_lvx_v2_landw = 544,
  Instruction_lvx_v2_lbs = 545,
  Instruction_lvx_v2_lbz = 546,
  Instruction_lvx_v2_ld = 547,
  Instruction_lvx_v2_lhs = 548,
  Instruction_lvx_v2_lhz = 549,
  Instruction_lvx_v2_liord = 550,
  Instruction_lvx_v2_liorw = 551,
  Instruction_lvx_v2_lnandd = 552,
  Instruction_lvx_v2_lnandw = 553,
  Instruction_lvx_v2_lniord = 554,
  Instruction_lvx_v2_lniorw = 555,
  Instruction_lvx_v2_lo = 556,
  Instruction_lvx_v2_loopdo = 557,
  Instruction_lvx_v2_lq = 558,
  Instruction_lvx_v2_lws = 559,
  Instruction_lvx_v2_lwz = 560,
  Instruction_lvx_v2_maddbho = 561,
  Instruction_lvx_v2_maddd = 562,
  Instruction_lvx_v2_madddp = 563,
  Instruction_lvx_v2_madddq = 564,
  Instruction_lvx_v2_madddt = 565,
  Instruction_lvx_v2_maddhwq = 566,
  Instruction_lvx_v2_maddsudt = 567,
  Instruction_lvx_v2_maddsuwd = 568,
  Instruction_lvx_v2_maddudt = 569,
  Instruction_lvx_v2_madduwd = 570,
  Instruction_lvx_v2_maddw = 571,
  Instruction_lvx_v2_maddwd = 572,
  Instruction_lvx_v2_maddwdp = 573,
  Instruction_lvx_v2_maddxbho = 574,
  Instruction_lvx_v2_maddxhwq = 575,
  Instruction_lvx_v2_maddxwdp = 576,
  Instruction_lvx_v2_make = 577,
  Instruction_lvx_v2_maxbx = 578,
  Instruction_lvx_v2_maxd = 579,
  Instruction_lvx_v2_maxdp = 580,
  Instruction_lvx_v2_maxho = 581,
  Instruction_lvx_v2_maxubx = 582,
  Instruction_lvx_v2_maxud = 583,
  Instruction_lvx_v2_maxudp = 584,
  Instruction_lvx_v2_maxuho = 585,
  Instruction_lvx_v2_maxuw = 586,
  Instruction_lvx_v2_maxuwq = 587,
  Instruction_lvx_v2_maxw = 588,
  Instruction_lvx_v2_maxwq = 589,
  Instruction_lvx_v2_minbx = 590,
  Instruction_lvx_v2_mind = 591,
  Instruction_lvx_v2_mindp = 592,
  Instruction_lvx_v2_minho = 593,
  Instruction_lvx_v2_minubx = 594,
  Instruction_lvx_v2_minud = 595,
  Instruction_lvx_v2_minudp = 596,
  Instruction_lvx_v2_minuho = 597,
  Instruction_lvx_v2_minuw = 598,
  Instruction_lvx_v2_minuwq = 599,
  Instruction_lvx_v2_minw = 600,
  Instruction_lvx_v2_minwq = 601,
  Instruction_lvx_v2_msbfbho = 602,
  Instruction_lvx_v2_msbfd = 603,
  Instruction_lvx_v2_msbfdp = 604,
  Instruction_lvx_v2_msbfdq = 605,
  Instruction_lvx_v2_msbfdt = 606,
  Instruction_lvx_v2_msbfhwq = 607,
  Instruction_lvx_v2_msbfsudt = 608,
  Instruction_lvx_v2_msbfsuwd = 609,
  Instruction_lvx_v2_msbfudt = 610,
  Instruction_lvx_v2_msbfuwd = 611,
  Instruction_lvx_v2_msbfw = 612,
  Instruction_lvx_v2_msbfwd = 613,
  Instruction_lvx_v2_msbfwdp = 614,
  Instruction_lvx_v2_msbfxbho = 615,
  Instruction_lvx_v2_msbfxhwq = 616,
  Instruction_lvx_v2_msbfxwdp = 617,
  Instruction_lvx_v2_mulbho = 618,
  Instruction_lvx_v2_muld = 619,
  Instruction_lvx_v2_muldp = 620,
  Instruction_lvx_v2_muldq = 621,
  Instruction_lvx_v2_muldt = 622,
  Instruction_lvx_v2_mulho = 623,
  Instruction_lvx_v2_mulhwq = 624,
  Instruction_lvx_v2_mulnbho = 625,
  Instruction_lvx_v2_mulnd = 626,
  Instruction_lvx_v2_mulndp = 627,
  Instruction_lvx_v2_mulndq = 628,
  Instruction_lvx_v2_mulnho = 629,
  Instruction_lvx_v2_mulnhwq = 630,
  Instruction_lvx_v2_mulnw = 631,
  Instruction_lvx_v2_mulnwd = 632,
  Instruction_lvx_v2_mulnwdp = 633,
  Instruction_lvx_v2_mulnwq = 634,
  Instruction_lvx_v2_mulnxbho = 635,
  Instruction_lvx_v2_mulnxhwq = 636,
  Instruction_lvx_v2_mulnxwdp = 637,
  Instruction_lvx_v2_mulsudt = 638,
  Instruction_lvx_v2_mulsuwd = 639,
  Instruction_lvx_v2_muludt = 640,
  Instruction_lvx_v2_muluwd = 641,
  Instruction_lvx_v2_mulw = 642,
  Instruction_lvx_v2_mulwd = 643,
  Instruction_lvx_v2_mulwdp = 644,
  Instruction_lvx_v2_mulwq = 645,
  Instruction_lvx_v2_mulxbho = 646,
  Instruction_lvx_v2_mulxhwq = 647,
  Instruction_lvx_v2_mulxwdp = 648,
  Instruction_lvx_v2_nandd = 649,
  Instruction_lvx_v2_nandq = 650,
  Instruction_lvx_v2_nandw = 651,
  Instruction_lvx_v2_negbx = 652,
  Instruction_lvx_v2_negd = 653,
  Instruction_lvx_v2_negdp = 654,
  Instruction_lvx_v2_negho = 655,
  Instruction_lvx_v2_negq = 656,
  Instruction_lvx_v2_negsbx = 657,
  Instruction_lvx_v2_negsd = 658,
  Instruction_lvx_v2_negsdp = 659,
  Instruction_lvx_v2_negsho = 660,
  Instruction_lvx_v2_negsw = 661,
  Instruction_lvx_v2_negswq = 662,
  Instruction_lvx_v2_negw = 663,
  Instruction_lvx_v2_negwq = 664,
  Instruction_lvx_v2_neord = 665,
  Instruction_lvx_v2_neorq = 666,
  Instruction_lvx_v2_neorw = 667,
  Instruction_lvx_v2_niord = 668,
  Instruction_lvx_v2_niorq = 669,
  Instruction_lvx_v2_niorw = 670,
  Instruction_lvx_v2_nop = 671,
  Instruction_lvx_v2_notd = 672,
  Instruction_lvx_v2_notq = 673,
  Instruction_lvx_v2_notw = 674,
  Instruction_lvx_v2_pcrel = 675,
  Instruction_lvx_v2_ret = 676,
  Instruction_lvx_v2_rfe = 677,
  Instruction_lvx_v2_rold = 678,
  Instruction_lvx_v2_rolw = 679,
  Instruction_lvx_v2_rolwq = 680,
  Instruction_lvx_v2_rord = 681,
  Instruction_lvx_v2_rorw = 682,
  Instruction_lvx_v2_rorwq = 683,
  Instruction_lvx_v2_rswap = 684,
  Instruction_lvx_v2_sb = 685,
  Instruction_lvx_v2_sbfbx = 686,
  Instruction_lvx_v2_sbfd = 687,
  Instruction_lvx_v2_sbfdp = 688,
  Instruction_lvx_v2_sbfho = 689,
  Instruction_lvx_v2_sbfq = 690,
  Instruction_lvx_v2_sbfsbx = 691,
  Instruction_lvx_v2_sbfsd = 692,
  Instruction_lvx_v2_sbfsdp = 693,
  Instruction_lvx_v2_sbfsho = 694,
  Instruction_lvx_v2_sbfsw = 695,
  Instruction_lvx_v2_sbfswq = 696,
  Instruction_lvx_v2_sbfusbx = 697,
  Instruction_lvx_v2_sbfusd = 698,
  Instruction_lvx_v2_sbfusdp = 699,
  Instruction_lvx_v2_sbfusho = 700,
  Instruction_lvx_v2_sbfusw = 701,
  Instruction_lvx_v2_sbfuswq = 702,
  Instruction_lvx_v2_sbfw = 703,
  Instruction_lvx_v2_sbfwq = 704,
  Instruction_lvx_v2_sbmm8 = 705,
  Instruction_lvx_v2_sbmm8d = 706,
  Instruction_lvx_v2_sbmm8dp = 707,
  Instruction_lvx_v2_sbmm8eord = 708,
  Instruction_lvx_v2_sbmm8eordp = 709,
  Instruction_lvx_v2_sbmmt8 = 710,
  Instruction_lvx_v2_sbmmt8d = 711,
  Instruction_lvx_v2_sbmmt8dp = 712,
  Instruction_lvx_v2_sbmmt8eord = 713,
  Instruction_lvx_v2_sbmmt8eordp = 714,
  Instruction_lvx_v2_scall = 715,
  Instruction_lvx_v2_sd = 716,
  Instruction_lvx_v2_set = 717,
  Instruction_lvx_v2_sh = 718,
  Instruction_lvx_v2_signbx = 719,
  Instruction_lvx_v2_signd = 720,
  Instruction_lvx_v2_signdp = 721,
  Instruction_lvx_v2_signho = 722,
  Instruction_lvx_v2_signsbx = 723,
  Instruction_lvx_v2_signsd = 724,
  Instruction_lvx_v2_signsdp = 725,
  Instruction_lvx_v2_signsho = 726,
  Instruction_lvx_v2_signsw = 727,
  Instruction_lvx_v2_signswq = 728,
  Instruction_lvx_v2_signw = 729,
  Instruction_lvx_v2_signwq = 730,
  Instruction_lvx_v2_sleep = 731,
  Instruction_lvx_v2_sllbx = 732,
  Instruction_lvx_v2_slld = 733,
  Instruction_lvx_v2_slldp = 734,
  Instruction_lvx_v2_sllho = 735,
  Instruction_lvx_v2_sllw = 736,
  Instruction_lvx_v2_sllwq = 737,
  Instruction_lvx_v2_slsbx = 738,
  Instruction_lvx_v2_slsd = 739,
  Instruction_lvx_v2_slsdp = 740,
  Instruction_lvx_v2_slsho = 741,
  Instruction_lvx_v2_slsw = 742,
  Instruction_lvx_v2_slswq = 743,
  Instruction_lvx_v2_slusbx = 744,
  Instruction_lvx_v2_slusd = 745,
  Instruction_lvx_v2_slusdp = 746,
  Instruction_lvx_v2_slusho = 747,
  Instruction_lvx_v2_slusw = 748,
  Instruction_lvx_v2_sluswq = 749,
  Instruction_lvx_v2_so = 750,
  Instruction_lvx_v2_sq = 751,
  Instruction_lvx_v2_srabx = 752,
  Instruction_lvx_v2_srad = 753,
  Instruction_lvx_v2_sradp = 754,
  Instruction_lvx_v2_sraho = 755,
  Instruction_lvx_v2_sraw = 756,
  Instruction_lvx_v2_srawq = 757,
  Instruction_lvx_v2_srlbx = 758,
  Instruction_lvx_v2_srld = 759,
  Instruction_lvx_v2_srldp = 760,
  Instruction_lvx_v2_srlho = 761,
  Instruction_lvx_v2_srlw = 762,
  Instruction_lvx_v2_srlwq = 763,
  Instruction_lvx_v2_srsbx = 764,
  Instruction_lvx_v2_srsd = 765,
  Instruction_lvx_v2_srsdp = 766,
  Instruction_lvx_v2_srsho = 767,
  Instruction_lvx_v2_srsw = 768,
  Instruction_lvx_v2_srswq = 769,
  Instruction_lvx_v2_stop = 770,
  Instruction_lvx_v2_stsud = 771,
  Instruction_lvx_v2_stsudp = 772,
  Instruction_lvx_v2_stsuho = 773,
  Instruction_lvx_v2_stsuw = 774,
  Instruction_lvx_v2_stsuwq = 775,
  Instruction_lvx_v2_sw = 776,
  Instruction_lvx_v2_sxbd = 777,
  Instruction_lvx_v2_sxhd = 778,
  Instruction_lvx_v2_sxwd = 779,
  Instruction_lvx_v2_syncgroup = 780,
  Instruction_lvx_v2_tlbdinval = 781,
  Instruction_lvx_v2_tlbiinval = 782,
  Instruction_lvx_v2_tlbprobe = 783,
  Instruction_lvx_v2_tlbread = 784,
  Instruction_lvx_v2_tlbwrite = 785,
  Instruction_lvx_v2_truncdwq = 786,
  Instruction_lvx_v2_trunchbx = 787,
  Instruction_lvx_v2_truncwho = 788,
  Instruction_lvx_v2_waitit = 789,
  Instruction_lvx_v2_wfxl = 790,
  Instruction_lvx_v2_wfxm = 791,
  Instruction_lvx_v2_widenqbho = 792,
  Instruction_lvx_v2_widenqhwq = 793,
  Instruction_lvx_v2_widenqwdp = 794,
  Instruction_lvx_v2_widensbho = 795,
  Instruction_lvx_v2_widenshwq = 796,
  Instruction_lvx_v2_widenswdp = 797,
  Instruction_lvx_v2_widenzbho = 798,
  Instruction_lvx_v2_widenzhwq = 799,
  Instruction_lvx_v2_widenzwdp = 800,
  Instruction_lvx_v2_xaccesso = 801,
  Instruction_lvx_v2_xaligno = 802,
  Instruction_lvx_v2_xcopyo = 803,
  Instruction_lvx_v2_xlo = 804,
  Instruction_lvx_v2_xmovefd = 805,
  Instruction_lvx_v2_xmovefo = 806,
  Instruction_lvx_v2_xmovefq = 807,
  Instruction_lvx_v2_xmovetd = 808,
  Instruction_lvx_v2_xmoveto = 809,
  Instruction_lvx_v2_xmovetq = 810,
  Instruction_lvx_v2_xplb = 811,
  Instruction_lvx_v2_xpld = 812,
  Instruction_lvx_v2_xplh = 813,
  Instruction_lvx_v2_xplo = 814,
  Instruction_lvx_v2_xplq = 815,
  Instruction_lvx_v2_xplw = 816,
  Instruction_lvx_v2_xso = 817,
  Instruction_lvx_v2_zxbd = 818,
  Instruction_lvx_v2_zxhd = 819,
  Instruction_lvx_v2_zxwd = 820,
  Separator_lvx_v2_comma = 821,
  Separator_lvx_v2_equal = 822,
  Separator_lvx_v2_qmark = 823,
  Separator_lvx_v2_rsbracket = 824,
  Separator_lvx_v2_lsbracket = 825
};

typedef enum {
  Modifier_lvx_v2_exunum_ALU0=0,
  Modifier_lvx_v2_exunum_ALU1=1,
  Modifier_lvx_v2_exunum_LSU0=2,
  Modifier_lvx_v2_exunum_LSU1=3,
} Modifier_lvx_v2_exunum_values;


extern const char *mod_lvx_v2_exunum[];
extern const char *mod_lvx_v2_ccbcomp[];
extern const char *mod_lvx_v2_bcucond[];
extern const char *mod_lvx_v2_intcomp[];
extern const char *mod_lvx_v2_lanecond[];
extern const char *mod_lvx_v2_lanetodo[];
extern const char *mod_lvx_v2_lanesize[];
extern const char *mod_lvx_v2_floatcomp[];
extern const char *mod_lvx_v2_floatmode[];
extern const char *mod_lvx_v2_signextw[];
extern const char *mod_lvx_v2_highmult[];
extern const char *mod_lvx_v2_widemult[];
extern const char *mod_lvx_v2_mostsig[];
extern const char *mod_lvx_v2_oddlanes[];
extern const char *mod_lvx_v2_ziplanes[];
extern const char *mod_lvx_v2_fnegate[];
extern const char *mod_lvx_v2_variant[];
extern const char *mod_lvx_v2_speculate[];
extern const char *mod_lvx_v2_doscale[];
extern const char *mod_lvx_v2_qindex[];
extern const char *mod_lvx_v2_hindex[];
extern const char *mod_lvx_v2_cachelev[];
extern const char *mod_lvx_v2_coherency[];
extern const char *mod_lvx_v2_boolcas[];
extern const char *mod_lvx_v2_accesses[];
extern const char *mod_lvx_v2_channel[];
extern const char *mod_lvx_v2_conjugate[];
extern const char *mod_lvx_v2_imultiply[];
extern const char *mod_lvx_v2_realimag[];
extern const char *mod_lvx_v2_shuffleV[];
extern const char *mod_lvx_v2_shuffleX[];
extern const char *mod_lvx_v2_splat32[];

typedef enum {
  Bundling_lvx_v2_ALL,
  Bundling_lvx_v2_BCU2,
  Bundling_lvx_v2_BCU2_X,
  Bundling_lvx_v2_BCU0,
  Bundling_lvx_v2_BCU,
  Bundling_lvx_v2_FULL,
  Bundling_lvx_v2_FULL_X,
  Bundling_lvx_v2_FULL_Y,
  Bundling_lvx_v2_LITE,
  Bundling_lvx_v2_LITE_X,
  Bundling_lvx_v2_LITE_Y,
  Bundling_lvx_v2_LSU0,
  Bundling_lvx_v2_LSU0_X,
  Bundling_lvx_v2_LSU0_Y,
  Bundling_lvx_v2_LSU,
  Bundling_lvx_v2_LSU_X,
  Bundling_lvx_v2_LSU_Y,
  Bundling_lvx_v2_TINY,
  Bundling_lvx_v2_TINY_X,
  Bundling_lvx_v2_TINY_Y,
  Bundling_lvx_v2_EXT,
  Bundling_lvx_v2_NOP,
} Bundling_lvx_v2;

static int ATTRIBUTE_UNUSED
lvx_v2_base_bundling(int bundling) {
  static int base_bundlings[] = {
    Bundling_lvx_v2_ALL,	// Bundling_lvx_v2_ALL
    Bundling_lvx_v2_BCU2,	// Bundling_lvx_v2_BCU2
    Bundling_lvx_v2_BCU2,	// Bundling_lvx_v2_BCU2_X
    Bundling_lvx_v2_BCU0,	// Bundling_lvx_v2_BCU0
    Bundling_lvx_v2_BCU,	// Bundling_lvx_v2_BCU
    Bundling_lvx_v2_FULL,	// Bundling_lvx_v2_FULL
    Bundling_lvx_v2_FULL,	// Bundling_lvx_v2_FULL_X
    Bundling_lvx_v2_FULL,	// Bundling_lvx_v2_FULL_Y
    Bundling_lvx_v2_LITE,	// Bundling_lvx_v2_LITE
    Bundling_lvx_v2_LITE,	// Bundling_lvx_v2_LITE_X
    Bundling_lvx_v2_LITE,	// Bundling_lvx_v2_LITE_Y
    Bundling_lvx_v2_LSU0,	// Bundling_lvx_v2_LSU0
    Bundling_lvx_v2_LSU0,	// Bundling_lvx_v2_LSU0_X
    Bundling_lvx_v2_LSU0,	// Bundling_lvx_v2_LSU0_Y
    Bundling_lvx_v2_LSU,	// Bundling_lvx_v2_LSU
    Bundling_lvx_v2_LSU,	// Bundling_lvx_v2_LSU_X
    Bundling_lvx_v2_LSU,	// Bundling_lvx_v2_LSU_Y
    Bundling_lvx_v2_TINY,	// Bundling_lvx_v2_TINY
    Bundling_lvx_v2_TINY,	// Bundling_lvx_v2_TINY_X
    Bundling_lvx_v2_TINY,	// Bundling_lvx_v2_TINY_Y
    Bundling_lvx_v2_EXT,	// Bundling_lvx_v2_EXT
    Bundling_lvx_v2_NOP,	// Bundling_lvx_v2_NOP
  };
  return base_bundlings[bundling];
};

typedef enum {
  Resource_lvx_v2_ISSUE,
  Resource_lvx_v2_TINY,
  Resource_lvx_v2_LITE,
  Resource_lvx_v2_FULL,
  Resource_lvx_v2_LSU,
  Resource_lvx_v2_MAU,
  Resource_lvx_v2_BCU,
  Resource_lvx_v2_EXT,
  Resource_lvx_v2_AUXR,
  Resource_lvx_v2_AUXW,
  Resource_lvx_v2_XFER,
  Resource_lvx_v2_MEMW,
  Resource_lvx_v2_SR12,
  Resource_lvx_v2_SR13,
  Resource_lvx_v2_SR14,
  Resource_lvx_v2_SR15,
} Resource_lvx_v2;
#define lvx_v2_RESOURCE_COUNT 16

typedef enum {
  Reservation_lvx_v2_ALL,
  Reservation_lvx_v2_ALU_TINY,
  Reservation_lvx_v2_ALU_TINY_X,
  Reservation_lvx_v2_ALU_TINY_Y,
  Reservation_lvx_v2_ALU_TINY_AUXR,
  Reservation_lvx_v2_ALU_LITE,
  Reservation_lvx_v2_ALU_LITE_X,
  Reservation_lvx_v2_ALU_LITE_Y,
  Reservation_lvx_v2_ALU_LITE_MISC,
  Reservation_lvx_v2_ALU_FULL,
  Reservation_lvx_v2_ALU_FULL_X,
  Reservation_lvx_v2_ALU_FULL_Y,
  Reservation_lvx_v2_BCU,
  Reservation_lvx_v2_BCU_BRRP,
  Reservation_lvx_v2_BCU_BRRP2,
  Reservation_lvx_v2_BCU2,
  Reservation_lvx_v2_BCU2_X,
  Reservation_lvx_v2_BCU_XFER,
  Reservation_lvx_v2_BCU_XFER_BRRP,
  Reservation_lvx_v2_BCU2_TINY_LSU,
  Reservation_lvx_v2_LSU,
  Reservation_lvx_v2_LSU_X,
  Reservation_lvx_v2_LSU_Y,
  Reservation_lvx_v2_LSU_MEMW_ACCR,
  Reservation_lvx_v2_LSU_MEMW_ACCR_X,
  Reservation_lvx_v2_LSU_MEMW_ACCR_Y,
  Reservation_lvx_v2_LSU2_MEMW,
  Reservation_lvx_v2_LSU2_MEMW_X,
  Reservation_lvx_v2_LSU2_MEMW_Y,
  Reservation_lvx_v2_LSU_AUXR,
  Reservation_lvx_v2_LSU_AUXR_X,
  Reservation_lvx_v2_LSU_AUXR_Y,
  Reservation_lvx_v2_LSU_MEMW_AUXR,
  Reservation_lvx_v2_LSU_MEMW_AUXR_X,
  Reservation_lvx_v2_LSU_MEMW_AUXR_Y,
  Reservation_lvx_v2_LSU2_MEMW_AUXR,
  Reservation_lvx_v2_LSU2_MEMW_AUXR_X,
  Reservation_lvx_v2_LSU2_MEMW_AUXR_Y,
  Reservation_lvx_v2_LSU_MEMW_AUXW,
  Reservation_lvx_v2_LSU_MEMW_AUXW_X,
  Reservation_lvx_v2_LSU_MEMW_AUXW_Y,
  Reservation_lvx_v2_LSU2_MEMW_AUXW,
  Reservation_lvx_v2_LSU2_MEMW_AUXW_X,
  Reservation_lvx_v2_LSU2_MEMW_AUXW_Y,
  Reservation_lvx_v2_LSU_AUXW,
  Reservation_lvx_v2_LSU_AUXW_X,
  Reservation_lvx_v2_LSU_AUXW_Y,
  Reservation_lvx_v2_LSU_AUXR_AUXW,
  Reservation_lvx_v2_LSU_AUXR_AUXW_X,
  Reservation_lvx_v2_LSU_AUXR_AUXW_Y,
  Reservation_lvx_v2_LSU2_MEMW_AUXR_AUXW,
  Reservation_lvx_v2_LSU2_MEMW_AUXR_AUXW_X,
  Reservation_lvx_v2_LSU2_MEMW_AUXR_AUXW_Y,
  Reservation_lvx_v2_MAU,
  Reservation_lvx_v2_EXT,
  Reservation_lvx_v2_EXT_COMP,
  Reservation_lvx_v2_EXT_MISC,
  Reservation_lvx_v2_EXT_MISC_AUXW,
} Reservation_lvx_v2;

extern struct lvx_reloc lvx_v2_rel16_reloc;
extern struct lvx_reloc lvx_v2_rel32_reloc;
extern struct lvx_reloc lvx_v2_rel64_reloc;
extern struct lvx_reloc lvx_v2_pcrel_signed16_reloc;
extern struct lvx_reloc lvx_v2_pcrel32_reloc;
extern struct lvx_reloc lvx_v2_pcrel_signed37_reloc;
extern struct lvx_reloc lvx_v2_pcrel_signed43_reloc;
extern struct lvx_reloc lvx_v2_pcrel_signed64_reloc;
extern struct lvx_reloc lvx_v2_pcrel64_reloc;
extern struct lvx_reloc lvx_v2_signed16_reloc;
extern struct lvx_reloc lvx_v2_signed32_reloc;
extern struct lvx_reloc lvx_v2_signed37_reloc;
extern struct lvx_reloc lvx_v2_gotoff_signed37_reloc;
extern struct lvx_reloc lvx_v2_gotoff_signed43_reloc;
extern struct lvx_reloc lvx_v2_gotoff_32_reloc;
extern struct lvx_reloc lvx_v2_gotoff_64_reloc;
extern struct lvx_reloc lvx_v2_got_32_reloc;
extern struct lvx_reloc lvx_v2_got_signed37_reloc;
extern struct lvx_reloc lvx_v2_got_signed43_reloc;
extern struct lvx_reloc lvx_v2_got_64_reloc;
extern struct lvx_reloc lvx_v2_glob_dat_reloc;
extern struct lvx_reloc lvx_v2_copy_reloc;
extern struct lvx_reloc lvx_v2_jump_slot_reloc;
extern struct lvx_reloc lvx_v2_relative_reloc;
extern struct lvx_reloc lvx_v2_signed43_reloc;
extern struct lvx_reloc lvx_v2_signed64_reloc;
extern struct lvx_reloc lvx_v2_gotaddr_signed37_reloc;
extern struct lvx_reloc lvx_v2_gotaddr_signed43_reloc;
extern struct lvx_reloc lvx_v2_gotaddr_signed64_reloc;
extern struct lvx_reloc lvx_v2_dtpmod64_reloc;
extern struct lvx_reloc lvx_v2_dtpoff64_reloc;
extern struct lvx_reloc lvx_v2_dtpoff_signed37_reloc;
extern struct lvx_reloc lvx_v2_dtpoff_signed43_reloc;
extern struct lvx_reloc lvx_v2_tlsgd_signed37_reloc;
extern struct lvx_reloc lvx_v2_tlsgd_signed43_reloc;
extern struct lvx_reloc lvx_v2_tlsld_signed37_reloc;
extern struct lvx_reloc lvx_v2_tlsld_signed43_reloc;
extern struct lvx_reloc lvx_v2_tpoff64_reloc;
extern struct lvx_reloc lvx_v2_tlsie_signed37_reloc;
extern struct lvx_reloc lvx_v2_tlsie_signed43_reloc;
extern struct lvx_reloc lvx_v2_tlsle_signed37_reloc;
extern struct lvx_reloc lvx_v2_tlsle_signed43_reloc;
extern struct lvx_reloc lvx_v2_rel8_reloc;
extern struct lvx_reloc lvx_v2_pcrel11s2_reloc;
extern struct lvx_reloc lvx_v2_pcrel17s2_reloc;
extern struct lvx_reloc lvx_v2_pcrel27s2_reloc;
extern struct lvx_reloc lvx_v2_pcrel38s2_reloc;
extern struct lvx_reloc lvx_v2_pcrel44s2_reloc;
extern struct lvx_reloc lvx_v2_pcrel54s2_reloc;


#endif /* OPCODE_LVX_H */
