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
  Immediate_lvx_v1_csrnumber = 2,
  Immediate_lvx_v1_pcrel11s2 = 3,
  Immediate_lvx_v1_pcrel17s2 = 4,
  Immediate_lvx_v1_pcrel27s2 = 5,
  Immediate_lvx_v1_pcrel38s2 = 6,
  Immediate_lvx_v1_pcrel44s2 = 7,
  Immediate_lvx_v1_pcrel54s2 = 8,
  Immediate_lvx_v1_signed10 = 9,
  Immediate_lvx_v1_signed16 = 10,
  Immediate_lvx_v1_signed27 = 11,
  Immediate_lvx_v1_signed37 = 12,
  Immediate_lvx_v1_signed43 = 13,
  Immediate_lvx_v1_signed54 = 14,
  Immediate_lvx_v1_sysnumber = 15,
  Immediate_lvx_v1_unsigned6 = 16,
  Immediate_lvx_v1_wrapped32 = 17,
  Immediate_lvx_v1_wrapped64 = 18,
  Immediate_lvx_v1_wrapped8 = 19,
  Modifier_lvx_v1_accesses = 20,
  Modifier_lvx_v1_bcucond = 21,
  Modifier_lvx_v1_boolcas = 22,
  Modifier_lvx_v1_cachelev = 23,
  Modifier_lvx_v1_ccbcomp = 24,
  Modifier_lvx_v1_channel = 25,
  Modifier_lvx_v1_coherency = 26,
  Modifier_lvx_v1_conjugate = 27,
  Modifier_lvx_v1_doscale = 28,
  Modifier_lvx_v1_exunum = 29,
  Modifier_lvx_v1_floatcomp = 30,
  Modifier_lvx_v1_floatmode = 31,
  Modifier_lvx_v1_fnegate = 32,
  Modifier_lvx_v1_highmult = 33,
  Modifier_lvx_v1_hindex = 34,
  Modifier_lvx_v1_imultiply = 35,
  Modifier_lvx_v1_intcomp = 36,
  Modifier_lvx_v1_lanecond = 37,
  Modifier_lvx_v1_lanesize = 38,
  Modifier_lvx_v1_lanetodo = 39,
  Modifier_lvx_v1_mostsig = 40,
  Modifier_lvx_v1_oddlanes = 41,
  Modifier_lvx_v1_qindex = 42,
  Modifier_lvx_v1_realimag = 43,
  Modifier_lvx_v1_shuffleV = 44,
  Modifier_lvx_v1_shuffleX = 45,
  Modifier_lvx_v1_signextw = 46,
  Modifier_lvx_v1_speculate = 47,
  Modifier_lvx_v1_splat32 = 48,
  Modifier_lvx_v1_variant = 49,
  Modifier_lvx_v1_widemult = 50,
  Modifier_lvx_v1_ziplanes = 51,
  RegClass_lvx_v1_aloneReg = 52,
  RegClass_lvx_v1_buffer16Reg = 53,
  RegClass_lvx_v1_buffer2Reg = 54,
  RegClass_lvx_v1_buffer32Reg = 55,
  RegClass_lvx_v1_buffer4Reg = 56,
  RegClass_lvx_v1_buffer64Reg = 57,
  RegClass_lvx_v1_buffer8Reg = 58,
  RegClass_lvx_v1_floatReg = 59,
  RegClass_lvx_v1_mainReg = 60,
  RegClass_lvx_v1_mainRegPair = 61,
  RegClass_lvx_v1_onlyfxReg = 62,
  RegClass_lvx_v1_onlygetReg = 63,
  RegClass_lvx_v1_onlyraReg = 64,
  RegClass_lvx_v1_onlysetReg = 65,
  RegClass_lvx_v1_onlyswapReg = 66,
  RegClass_lvx_v1_pairedReg = 67,
  RegClass_lvx_v1_quadReg = 68,
  RegClass_lvx_v1_singleReg = 69,
  RegClass_lvx_v1_systemReg = 70,
  RegClass_lvx_v1_worddRegE = 71,
  RegClass_lvx_v1_worddRegO = 72,
  RegClass_lvx_v1_xworddReg = 73,
  RegClass_lvx_v1_xworddReg0M4 = 74,
  RegClass_lvx_v1_xworddReg1M4 = 75,
  RegClass_lvx_v1_xworddReg2M4 = 76,
  RegClass_lvx_v1_xworddReg3M4 = 77,
  RegClass_lvx_v1_xwordoReg = 78,
  RegClass_lvx_v1_xwordqReg = 79,
  RegClass_lvx_v1_xwordqRegE = 80,
  RegClass_lvx_v1_xwordqRegO = 81,
  RegClass_lvx_v1_xwordvReg = 82,
  RegClass_lvx_v1_xwordxReg = 83,
  Instruction_lvx_v1_abdd = 84,
  Instruction_lvx_v1_abdsd = 85,
  Instruction_lvx_v1_abdsw = 86,
  Instruction_lvx_v1_abdud = 87,
  Instruction_lvx_v1_abduw = 88,
  Instruction_lvx_v1_abdw = 89,
  Instruction_lvx_v1_absd = 90,
  Instruction_lvx_v1_abssd = 91,
  Instruction_lvx_v1_abssw = 92,
  Instruction_lvx_v1_absw = 93,
  Instruction_lvx_v1_acswapb = 94,
  Instruction_lvx_v1_acswapd = 95,
  Instruction_lvx_v1_acswaph = 96,
  Instruction_lvx_v1_acswapq = 97,
  Instruction_lvx_v1_acswapw = 98,
  Instruction_lvx_v1_addd = 99,
  Instruction_lvx_v1_addq = 100,
  Instruction_lvx_v1_addsd = 101,
  Instruction_lvx_v1_addsw = 102,
  Instruction_lvx_v1_addusd = 103,
  Instruction_lvx_v1_addusw = 104,
  Instruction_lvx_v1_addw = 105,
  Instruction_lvx_v1_addx16d = 106,
  Instruction_lvx_v1_addx16w = 107,
  Instruction_lvx_v1_addx2d = 108,
  Instruction_lvx_v1_addx2w = 109,
  Instruction_lvx_v1_addx32d = 110,
  Instruction_lvx_v1_addx32w = 111,
  Instruction_lvx_v1_addx4d = 112,
  Instruction_lvx_v1_addx4w = 113,
  Instruction_lvx_v1_addx64d = 114,
  Instruction_lvx_v1_addx64w = 115,
  Instruction_lvx_v1_addx8d = 116,
  Instruction_lvx_v1_addx8w = 117,
  Instruction_lvx_v1_aladdb = 118,
  Instruction_lvx_v1_aladdd = 119,
  Instruction_lvx_v1_aladdh = 120,
  Instruction_lvx_v1_aladdw = 121,
  Instruction_lvx_v1_alandb = 122,
  Instruction_lvx_v1_alandd = 123,
  Instruction_lvx_v1_alandh = 124,
  Instruction_lvx_v1_alandw = 125,
  Instruction_lvx_v1_alb = 126,
  Instruction_lvx_v1_alclrb = 127,
  Instruction_lvx_v1_alclrd = 128,
  Instruction_lvx_v1_alclrh = 129,
  Instruction_lvx_v1_alclrw = 130,
  Instruction_lvx_v1_ald = 131,
  Instruction_lvx_v1_aldusb = 132,
  Instruction_lvx_v1_aldusd = 133,
  Instruction_lvx_v1_aldush = 134,
  Instruction_lvx_v1_aldusw = 135,
  Instruction_lvx_v1_aleorb = 136,
  Instruction_lvx_v1_aleord = 137,
  Instruction_lvx_v1_aleorh = 138,
  Instruction_lvx_v1_aleorw = 139,
  Instruction_lvx_v1_alh = 140,
  Instruction_lvx_v1_aliorb = 141,
  Instruction_lvx_v1_aliord = 142,
  Instruction_lvx_v1_aliorh = 143,
  Instruction_lvx_v1_aliorw = 144,
  Instruction_lvx_v1_almaxb = 145,
  Instruction_lvx_v1_almaxd = 146,
  Instruction_lvx_v1_almaxh = 147,
  Instruction_lvx_v1_almaxub = 148,
  Instruction_lvx_v1_almaxud = 149,
  Instruction_lvx_v1_almaxuh = 150,
  Instruction_lvx_v1_almaxuw = 151,
  Instruction_lvx_v1_almaxw = 152,
  Instruction_lvx_v1_alminb = 153,
  Instruction_lvx_v1_almind = 154,
  Instruction_lvx_v1_alminh = 155,
  Instruction_lvx_v1_alminub = 156,
  Instruction_lvx_v1_alminud = 157,
  Instruction_lvx_v1_alminuh = 158,
  Instruction_lvx_v1_alminuw = 159,
  Instruction_lvx_v1_alminw = 160,
  Instruction_lvx_v1_alw = 161,
  Instruction_lvx_v1_andd = 162,
  Instruction_lvx_v1_andnd = 163,
  Instruction_lvx_v1_andnq = 164,
  Instruction_lvx_v1_andnw = 165,
  Instruction_lvx_v1_andq = 166,
  Instruction_lvx_v1_andw = 167,
  Instruction_lvx_v1_asaddb = 168,
  Instruction_lvx_v1_asaddd = 169,
  Instruction_lvx_v1_asaddh = 170,
  Instruction_lvx_v1_asaddw = 171,
  Instruction_lvx_v1_asandb = 172,
  Instruction_lvx_v1_asandd = 173,
  Instruction_lvx_v1_asandh = 174,
  Instruction_lvx_v1_asandw = 175,
  Instruction_lvx_v1_asb = 176,
  Instruction_lvx_v1_asd = 177,
  Instruction_lvx_v1_asdusb = 178,
  Instruction_lvx_v1_asdusd = 179,
  Instruction_lvx_v1_asdush = 180,
  Instruction_lvx_v1_asdusw = 181,
  Instruction_lvx_v1_aseorb = 182,
  Instruction_lvx_v1_aseord = 183,
  Instruction_lvx_v1_aseorh = 184,
  Instruction_lvx_v1_aseorw = 185,
  Instruction_lvx_v1_ash = 186,
  Instruction_lvx_v1_asiorb = 187,
  Instruction_lvx_v1_asiord = 188,
  Instruction_lvx_v1_asiorh = 189,
  Instruction_lvx_v1_asiorw = 190,
  Instruction_lvx_v1_asmaxb = 191,
  Instruction_lvx_v1_asmaxd = 192,
  Instruction_lvx_v1_asmaxh = 193,
  Instruction_lvx_v1_asmaxub = 194,
  Instruction_lvx_v1_asmaxud = 195,
  Instruction_lvx_v1_asmaxuh = 196,
  Instruction_lvx_v1_asmaxuw = 197,
  Instruction_lvx_v1_asmaxw = 198,
  Instruction_lvx_v1_asminb = 199,
  Instruction_lvx_v1_asmind = 200,
  Instruction_lvx_v1_asminh = 201,
  Instruction_lvx_v1_asminub = 202,
  Instruction_lvx_v1_asminud = 203,
  Instruction_lvx_v1_asminuh = 204,
  Instruction_lvx_v1_asminuw = 205,
  Instruction_lvx_v1_asminw = 206,
  Instruction_lvx_v1_asw = 207,
  Instruction_lvx_v1_aswapb = 208,
  Instruction_lvx_v1_aswapd = 209,
  Instruction_lvx_v1_aswaph = 210,
  Instruction_lvx_v1_aswapw = 211,
  Instruction_lvx_v1_avgruw = 212,
  Instruction_lvx_v1_avgrw = 213,
  Instruction_lvx_v1_avguw = 214,
  Instruction_lvx_v1_avgw = 215,
  Instruction_lvx_v1_await = 216,
  Instruction_lvx_v1_barrier = 217,
  Instruction_lvx_v1_blend = 218,
  Instruction_lvx_v1_break = 219,
  Instruction_lvx_v1_call = 220,
  Instruction_lvx_v1_callx = 221,
  Instruction_lvx_v1_catdq = 222,
  Instruction_lvx_v1_cb = 223,
  Instruction_lvx_v1_cbsd = 224,
  Instruction_lvx_v1_cbsw = 225,
  Instruction_lvx_v1_cbx = 226,
  Instruction_lvx_v1_ccb = 227,
  Instruction_lvx_v1_ccbx = 228,
  Instruction_lvx_v1_clsd = 229,
  Instruction_lvx_v1_clsw = 230,
  Instruction_lvx_v1_clzd = 231,
  Instruction_lvx_v1_clzw = 232,
  Instruction_lvx_v1_cmoved = 233,
  Instruction_lvx_v1_cmoveq = 234,
  Instruction_lvx_v1_compd = 235,
  Instruction_lvx_v1_compq = 236,
  Instruction_lvx_v1_compw = 237,
  Instruction_lvx_v1_copyd = 238,
  Instruction_lvx_v1_copyo = 239,
  Instruction_lvx_v1_copyq = 240,
  Instruction_lvx_v1_copyw = 241,
  Instruction_lvx_v1_crcbellw = 242,
  Instruction_lvx_v1_crcbelmw = 243,
  Instruction_lvx_v1_crclellw = 244,
  Instruction_lvx_v1_crclelmw = 245,
  Instruction_lvx_v1_csrrc = 246,
  Instruction_lvx_v1_csrrs = 247,
  Instruction_lvx_v1_csrrw = 248,
  Instruction_lvx_v1_ctzd = 249,
  Instruction_lvx_v1_ctzw = 250,
  Instruction_lvx_v1_d1inval = 251,
  Instruction_lvx_v1_dflushl = 252,
  Instruction_lvx_v1_dflushsw = 253,
  Instruction_lvx_v1_dinvall = 254,
  Instruction_lvx_v1_dinvalsw = 255,
  Instruction_lvx_v1_divmodd = 256,
  Instruction_lvx_v1_divmodud = 257,
  Instruction_lvx_v1_divmoduw = 258,
  Instruction_lvx_v1_divmodw = 259,
  Instruction_lvx_v1_dpurgel = 260,
  Instruction_lvx_v1_dpurgesw = 261,
  Instruction_lvx_v1_dtouchl = 262,
  Instruction_lvx_v1_eord = 263,
  Instruction_lvx_v1_eorq = 264,
  Instruction_lvx_v1_eorw = 265,
  Instruction_lvx_v1_errop = 266,
  Instruction_lvx_v1_extfs = 267,
  Instruction_lvx_v1_extfz = 268,
  Instruction_lvx_v1_fabsd = 269,
  Instruction_lvx_v1_fabsh = 270,
  Instruction_lvx_v1_fabsw = 271,
  Instruction_lvx_v1_fabswp = 272,
  Instruction_lvx_v1_faddd = 273,
  Instruction_lvx_v1_faddh = 274,
  Instruction_lvx_v1_faddw = 275,
  Instruction_lvx_v1_faddwc = 276,
  Instruction_lvx_v1_fclassd = 277,
  Instruction_lvx_v1_fclassh = 278,
  Instruction_lvx_v1_fclassw = 279,
  Instruction_lvx_v1_fclasswp = 280,
  Instruction_lvx_v1_fcompd = 281,
  Instruction_lvx_v1_fcomph = 282,
  Instruction_lvx_v1_fcompw = 283,
  Instruction_lvx_v1_fdivd = 284,
  Instruction_lvx_v1_fdivh = 285,
  Instruction_lvx_v1_fdivw = 286,
  Instruction_lvx_v1_fence = 287,
  Instruction_lvx_v1_ffmad = 288,
  Instruction_lvx_v1_ffmah = 289,
  Instruction_lvx_v1_ffmaw = 290,
  Instruction_lvx_v1_ffmawc = 291,
  Instruction_lvx_v1_ffmsd = 292,
  Instruction_lvx_v1_ffmsh = 293,
  Instruction_lvx_v1_ffmsw = 294,
  Instruction_lvx_v1_fixedd = 295,
  Instruction_lvx_v1_fixeddw = 296,
  Instruction_lvx_v1_fixedud = 297,
  Instruction_lvx_v1_fixedudw = 298,
  Instruction_lvx_v1_fixeduw = 299,
  Instruction_lvx_v1_fixeduwd = 300,
  Instruction_lvx_v1_fixeduwp = 301,
  Instruction_lvx_v1_fixedw = 302,
  Instruction_lvx_v1_fixedwd = 303,
  Instruction_lvx_v1_fixedwp = 304,
  Instruction_lvx_v1_floatd = 305,
  Instruction_lvx_v1_floatdw = 306,
  Instruction_lvx_v1_floatud = 307,
  Instruction_lvx_v1_floatudw = 308,
  Instruction_lvx_v1_floatuw = 309,
  Instruction_lvx_v1_floatuwd = 310,
  Instruction_lvx_v1_floatuwp = 311,
  Instruction_lvx_v1_floatw = 312,
  Instruction_lvx_v1_floatwd = 313,
  Instruction_lvx_v1_floatwp = 314,
  Instruction_lvx_v1_fmaxd = 315,
  Instruction_lvx_v1_fmaxh = 316,
  Instruction_lvx_v1_fmaxnd = 317,
  Instruction_lvx_v1_fmaxnh = 318,
  Instruction_lvx_v1_fmaxnw = 319,
  Instruction_lvx_v1_fmaxw = 320,
  Instruction_lvx_v1_fmind = 321,
  Instruction_lvx_v1_fminh = 322,
  Instruction_lvx_v1_fminnd = 323,
  Instruction_lvx_v1_fminnh = 324,
  Instruction_lvx_v1_fminnw = 325,
  Instruction_lvx_v1_fminw = 326,
  Instruction_lvx_v1_fmuld = 327,
  Instruction_lvx_v1_fmulh = 328,
  Instruction_lvx_v1_fmulw = 329,
  Instruction_lvx_v1_fmulwc = 330,
  Instruction_lvx_v1_fnarrowdw = 331,
  Instruction_lvx_v1_fnarrowwh = 332,
  Instruction_lvx_v1_fnegd = 333,
  Instruction_lvx_v1_fnegh = 334,
  Instruction_lvx_v1_fnegw = 335,
  Instruction_lvx_v1_fnegwp = 336,
  Instruction_lvx_v1_frcsr = 337,
  Instruction_lvx_v1_frflags = 338,
  Instruction_lvx_v1_frintd = 339,
  Instruction_lvx_v1_frinth = 340,
  Instruction_lvx_v1_frintw = 341,
  Instruction_lvx_v1_frrm = 342,
  Instruction_lvx_v1_fsbfd = 343,
  Instruction_lvx_v1_fsbfh = 344,
  Instruction_lvx_v1_fsbfw = 345,
  Instruction_lvx_v1_fsbfwc = 346,
  Instruction_lvx_v1_fscsr = 347,
  Instruction_lvx_v1_fsflags = 348,
  Instruction_lvx_v1_fsignd = 349,
  Instruction_lvx_v1_fsignh = 350,
  Instruction_lvx_v1_fsignmd = 351,
  Instruction_lvx_v1_fsignmh = 352,
  Instruction_lvx_v1_fsignmw = 353,
  Instruction_lvx_v1_fsignnd = 354,
  Instruction_lvx_v1_fsignnh = 355,
  Instruction_lvx_v1_fsignnw = 356,
  Instruction_lvx_v1_fsignw = 357,
  Instruction_lvx_v1_fsqrtd = 358,
  Instruction_lvx_v1_fsqrth = 359,
  Instruction_lvx_v1_fsqrtw = 360,
  Instruction_lvx_v1_fsrecd = 361,
  Instruction_lvx_v1_fsrecw = 362,
  Instruction_lvx_v1_fsrecwp = 363,
  Instruction_lvx_v1_fsrm = 364,
  Instruction_lvx_v1_fsrsrd = 365,
  Instruction_lvx_v1_fsrsrw = 366,
  Instruction_lvx_v1_fsrsrwp = 367,
  Instruction_lvx_v1_fwidenhw = 368,
  Instruction_lvx_v1_fwidenwd = 369,
  Instruction_lvx_v1_get = 370,
  Instruction_lvx_v1_goto = 371,
  Instruction_lvx_v1_gotox = 372,
  Instruction_lvx_v1_guard = 373,
  Instruction_lvx_v1_i1inval = 374,
  Instruction_lvx_v1_i1invals = 375,
  Instruction_lvx_v1_icall = 376,
  Instruction_lvx_v1_iget = 377,
  Instruction_lvx_v1_igoto = 378,
  Instruction_lvx_v1_insf = 379,
  Instruction_lvx_v1_iord = 380,
  Instruction_lvx_v1_iornd = 381,
  Instruction_lvx_v1_iornq = 382,
  Instruction_lvx_v1_iornw = 383,
  Instruction_lvx_v1_iorq = 384,
  Instruction_lvx_v1_iorw = 385,
  Instruction_lvx_v1_landd = 386,
  Instruction_lvx_v1_landw = 387,
  Instruction_lvx_v1_lbs = 388,
  Instruction_lvx_v1_lbz = 389,
  Instruction_lvx_v1_ld = 390,
  Instruction_lvx_v1_lhs = 391,
  Instruction_lvx_v1_lhz = 392,
  Instruction_lvx_v1_liord = 393,
  Instruction_lvx_v1_liorw = 394,
  Instruction_lvx_v1_lnandd = 395,
  Instruction_lvx_v1_lnandw = 396,
  Instruction_lvx_v1_lniord = 397,
  Instruction_lvx_v1_lniorw = 398,
  Instruction_lvx_v1_lo = 399,
  Instruction_lvx_v1_loopdo = 400,
  Instruction_lvx_v1_lq = 401,
  Instruction_lvx_v1_lws = 402,
  Instruction_lvx_v1_lwz = 403,
  Instruction_lvx_v1_maddd = 404,
  Instruction_lvx_v1_madddq = 405,
  Instruction_lvx_v1_madddt = 406,
  Instruction_lvx_v1_maddsudt = 407,
  Instruction_lvx_v1_maddsuwd = 408,
  Instruction_lvx_v1_maddudt = 409,
  Instruction_lvx_v1_madduwd = 410,
  Instruction_lvx_v1_maddw = 411,
  Instruction_lvx_v1_maddwd = 412,
  Instruction_lvx_v1_make = 413,
  Instruction_lvx_v1_maxd = 414,
  Instruction_lvx_v1_maxud = 415,
  Instruction_lvx_v1_maxuw = 416,
  Instruction_lvx_v1_maxw = 417,
  Instruction_lvx_v1_mind = 418,
  Instruction_lvx_v1_minud = 419,
  Instruction_lvx_v1_minuw = 420,
  Instruction_lvx_v1_minw = 421,
  Instruction_lvx_v1_msbfd = 422,
  Instruction_lvx_v1_msbfdq = 423,
  Instruction_lvx_v1_msbfdt = 424,
  Instruction_lvx_v1_msbfsudt = 425,
  Instruction_lvx_v1_msbfsuwd = 426,
  Instruction_lvx_v1_msbfudt = 427,
  Instruction_lvx_v1_msbfuwd = 428,
  Instruction_lvx_v1_msbfw = 429,
  Instruction_lvx_v1_msbfwd = 430,
  Instruction_lvx_v1_muld = 431,
  Instruction_lvx_v1_muldq = 432,
  Instruction_lvx_v1_muldt = 433,
  Instruction_lvx_v1_mulnd = 434,
  Instruction_lvx_v1_mulndq = 435,
  Instruction_lvx_v1_mulnw = 436,
  Instruction_lvx_v1_mulnwd = 437,
  Instruction_lvx_v1_mulsudt = 438,
  Instruction_lvx_v1_mulsuwd = 439,
  Instruction_lvx_v1_muludt = 440,
  Instruction_lvx_v1_muluwd = 441,
  Instruction_lvx_v1_mulw = 442,
  Instruction_lvx_v1_mulwd = 443,
  Instruction_lvx_v1_nandd = 444,
  Instruction_lvx_v1_nandq = 445,
  Instruction_lvx_v1_nandw = 446,
  Instruction_lvx_v1_negd = 447,
  Instruction_lvx_v1_negq = 448,
  Instruction_lvx_v1_negsd = 449,
  Instruction_lvx_v1_negsw = 450,
  Instruction_lvx_v1_negw = 451,
  Instruction_lvx_v1_neord = 452,
  Instruction_lvx_v1_neorq = 453,
  Instruction_lvx_v1_neorw = 454,
  Instruction_lvx_v1_niord = 455,
  Instruction_lvx_v1_niorq = 456,
  Instruction_lvx_v1_niorw = 457,
  Instruction_lvx_v1_nop = 458,
  Instruction_lvx_v1_notd = 459,
  Instruction_lvx_v1_notq = 460,
  Instruction_lvx_v1_notw = 461,
  Instruction_lvx_v1_pcrel = 462,
  Instruction_lvx_v1_ret = 463,
  Instruction_lvx_v1_rfe = 464,
  Instruction_lvx_v1_rold = 465,
  Instruction_lvx_v1_rolw = 466,
  Instruction_lvx_v1_rord = 467,
  Instruction_lvx_v1_rorw = 468,
  Instruction_lvx_v1_rswap = 469,
  Instruction_lvx_v1_sb = 470,
  Instruction_lvx_v1_sbfd = 471,
  Instruction_lvx_v1_sbfq = 472,
  Instruction_lvx_v1_sbfsd = 473,
  Instruction_lvx_v1_sbfsw = 474,
  Instruction_lvx_v1_sbfusd = 475,
  Instruction_lvx_v1_sbfusw = 476,
  Instruction_lvx_v1_sbfw = 477,
  Instruction_lvx_v1_sbmm8 = 478,
  Instruction_lvx_v1_sbmm8d = 479,
  Instruction_lvx_v1_sbmm8eord = 480,
  Instruction_lvx_v1_sbmmt8 = 481,
  Instruction_lvx_v1_sbmmt8d = 482,
  Instruction_lvx_v1_sbmmt8eord = 483,
  Instruction_lvx_v1_scall = 484,
  Instruction_lvx_v1_sd = 485,
  Instruction_lvx_v1_set = 486,
  Instruction_lvx_v1_sh = 487,
  Instruction_lvx_v1_signd = 488,
  Instruction_lvx_v1_signsd = 489,
  Instruction_lvx_v1_signsw = 490,
  Instruction_lvx_v1_signw = 491,
  Instruction_lvx_v1_sleep = 492,
  Instruction_lvx_v1_slld = 493,
  Instruction_lvx_v1_sllw = 494,
  Instruction_lvx_v1_slsd = 495,
  Instruction_lvx_v1_slsw = 496,
  Instruction_lvx_v1_slusd = 497,
  Instruction_lvx_v1_slusw = 498,
  Instruction_lvx_v1_so = 499,
  Instruction_lvx_v1_sq = 500,
  Instruction_lvx_v1_srad = 501,
  Instruction_lvx_v1_sraw = 502,
  Instruction_lvx_v1_srld = 503,
  Instruction_lvx_v1_srlw = 504,
  Instruction_lvx_v1_srsd = 505,
  Instruction_lvx_v1_srsw = 506,
  Instruction_lvx_v1_stop = 507,
  Instruction_lvx_v1_stsud = 508,
  Instruction_lvx_v1_stsuw = 509,
  Instruction_lvx_v1_sw = 510,
  Instruction_lvx_v1_sxbd = 511,
  Instruction_lvx_v1_sxhd = 512,
  Instruction_lvx_v1_sxwd = 513,
  Instruction_lvx_v1_syncgroup = 514,
  Instruction_lvx_v1_tlbdinval = 515,
  Instruction_lvx_v1_tlbiinval = 516,
  Instruction_lvx_v1_tlbprobe = 517,
  Instruction_lvx_v1_tlbread = 518,
  Instruction_lvx_v1_tlbwrite = 519,
  Instruction_lvx_v1_waitit = 520,
  Instruction_lvx_v1_wfxl = 521,
  Instruction_lvx_v1_wfxm = 522,
  Instruction_lvx_v1_zxbd = 523,
  Instruction_lvx_v1_zxhd = 524,
  Instruction_lvx_v1_zxwd = 525,
  Separator_lvx_v1_comma = 526,
  Separator_lvx_v1_equal = 527,
  Separator_lvx_v1_qmark = 528,
  Separator_lvx_v1_rsbracket = 529,
  Separator_lvx_v1_lsbracket = 530
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
  Immediate_lvx_v2_csrnumber = 2,
  Immediate_lvx_v2_pcrel11s2 = 3,
  Immediate_lvx_v2_pcrel17s2 = 4,
  Immediate_lvx_v2_pcrel27s2 = 5,
  Immediate_lvx_v2_pcrel38s2 = 6,
  Immediate_lvx_v2_pcrel44s2 = 7,
  Immediate_lvx_v2_pcrel54s2 = 8,
  Immediate_lvx_v2_signed10 = 9,
  Immediate_lvx_v2_signed16 = 10,
  Immediate_lvx_v2_signed27 = 11,
  Immediate_lvx_v2_signed37 = 12,
  Immediate_lvx_v2_signed43 = 13,
  Immediate_lvx_v2_signed54 = 14,
  Immediate_lvx_v2_sysnumber = 15,
  Immediate_lvx_v2_unsigned6 = 16,
  Immediate_lvx_v2_wrapped32 = 17,
  Immediate_lvx_v2_wrapped64 = 18,
  Immediate_lvx_v2_wrapped8 = 19,
  Modifier_lvx_v2_accesses = 20,
  Modifier_lvx_v2_bcucond = 21,
  Modifier_lvx_v2_boolcas = 22,
  Modifier_lvx_v2_cachelev = 23,
  Modifier_lvx_v2_ccbcomp = 24,
  Modifier_lvx_v2_channel = 25,
  Modifier_lvx_v2_coherency = 26,
  Modifier_lvx_v2_conjugate = 27,
  Modifier_lvx_v2_doscale = 28,
  Modifier_lvx_v2_exunum = 29,
  Modifier_lvx_v2_floatcomp = 30,
  Modifier_lvx_v2_floatmode = 31,
  Modifier_lvx_v2_fnegate = 32,
  Modifier_lvx_v2_highmult = 33,
  Modifier_lvx_v2_hindex = 34,
  Modifier_lvx_v2_imultiply = 35,
  Modifier_lvx_v2_intcomp = 36,
  Modifier_lvx_v2_lanecond = 37,
  Modifier_lvx_v2_lanesize = 38,
  Modifier_lvx_v2_lanetodo = 39,
  Modifier_lvx_v2_mostsig = 40,
  Modifier_lvx_v2_oddlanes = 41,
  Modifier_lvx_v2_qindex = 42,
  Modifier_lvx_v2_realimag = 43,
  Modifier_lvx_v2_shuffleV = 44,
  Modifier_lvx_v2_shuffleX = 45,
  Modifier_lvx_v2_signextw = 46,
  Modifier_lvx_v2_speculate = 47,
  Modifier_lvx_v2_splat32 = 48,
  Modifier_lvx_v2_variant = 49,
  Modifier_lvx_v2_widemult = 50,
  Modifier_lvx_v2_ziplanes = 51,
  RegClass_lvx_v2_aloneReg = 52,
  RegClass_lvx_v2_buffer16Reg = 53,
  RegClass_lvx_v2_buffer2Reg = 54,
  RegClass_lvx_v2_buffer32Reg = 55,
  RegClass_lvx_v2_buffer4Reg = 56,
  RegClass_lvx_v2_buffer64Reg = 57,
  RegClass_lvx_v2_buffer8Reg = 58,
  RegClass_lvx_v2_floatReg = 59,
  RegClass_lvx_v2_mainReg = 60,
  RegClass_lvx_v2_mainRegPair = 61,
  RegClass_lvx_v2_onlyfxReg = 62,
  RegClass_lvx_v2_onlygetReg = 63,
  RegClass_lvx_v2_onlyraReg = 64,
  RegClass_lvx_v2_onlysetReg = 65,
  RegClass_lvx_v2_onlyswapReg = 66,
  RegClass_lvx_v2_pairedReg = 67,
  RegClass_lvx_v2_quadReg = 68,
  RegClass_lvx_v2_singleReg = 69,
  RegClass_lvx_v2_systemReg = 70,
  RegClass_lvx_v2_worddRegE = 71,
  RegClass_lvx_v2_worddRegO = 72,
  RegClass_lvx_v2_xworddReg = 73,
  RegClass_lvx_v2_xworddReg0M4 = 74,
  RegClass_lvx_v2_xworddReg1M4 = 75,
  RegClass_lvx_v2_xworddReg2M4 = 76,
  RegClass_lvx_v2_xworddReg3M4 = 77,
  RegClass_lvx_v2_xwordoReg = 78,
  RegClass_lvx_v2_xwordqReg = 79,
  RegClass_lvx_v2_xwordqRegE = 80,
  RegClass_lvx_v2_xwordqRegO = 81,
  RegClass_lvx_v2_xwordvReg = 82,
  RegClass_lvx_v2_xwordxReg = 83,
  Instruction_lvx_v2_abdbx = 84,
  Instruction_lvx_v2_abdd = 85,
  Instruction_lvx_v2_abddp = 86,
  Instruction_lvx_v2_abdho = 87,
  Instruction_lvx_v2_abdsbx = 88,
  Instruction_lvx_v2_abdsd = 89,
  Instruction_lvx_v2_abdsdp = 90,
  Instruction_lvx_v2_abdsho = 91,
  Instruction_lvx_v2_abdsw = 92,
  Instruction_lvx_v2_abdswq = 93,
  Instruction_lvx_v2_abdubx = 94,
  Instruction_lvx_v2_abdud = 95,
  Instruction_lvx_v2_abdudp = 96,
  Instruction_lvx_v2_abduho = 97,
  Instruction_lvx_v2_abduw = 98,
  Instruction_lvx_v2_abduwq = 99,
  Instruction_lvx_v2_abdw = 100,
  Instruction_lvx_v2_abdwq = 101,
  Instruction_lvx_v2_absbx = 102,
  Instruction_lvx_v2_absd = 103,
  Instruction_lvx_v2_absdp = 104,
  Instruction_lvx_v2_absho = 105,
  Instruction_lvx_v2_abssbx = 106,
  Instruction_lvx_v2_abssd = 107,
  Instruction_lvx_v2_abssdp = 108,
  Instruction_lvx_v2_abssho = 109,
  Instruction_lvx_v2_abssw = 110,
  Instruction_lvx_v2_absswq = 111,
  Instruction_lvx_v2_absw = 112,
  Instruction_lvx_v2_abswq = 113,
  Instruction_lvx_v2_acswapb = 114,
  Instruction_lvx_v2_acswapd = 115,
  Instruction_lvx_v2_acswaph = 116,
  Instruction_lvx_v2_acswapq = 117,
  Instruction_lvx_v2_acswapw = 118,
  Instruction_lvx_v2_addbx = 119,
  Instruction_lvx_v2_addd = 120,
  Instruction_lvx_v2_adddp = 121,
  Instruction_lvx_v2_addho = 122,
  Instruction_lvx_v2_addq = 123,
  Instruction_lvx_v2_addsbx = 124,
  Instruction_lvx_v2_addsd = 125,
  Instruction_lvx_v2_addsdp = 126,
  Instruction_lvx_v2_addsho = 127,
  Instruction_lvx_v2_addsw = 128,
  Instruction_lvx_v2_addswq = 129,
  Instruction_lvx_v2_addusbx = 130,
  Instruction_lvx_v2_addusd = 131,
  Instruction_lvx_v2_addusdp = 132,
  Instruction_lvx_v2_addusho = 133,
  Instruction_lvx_v2_addusw = 134,
  Instruction_lvx_v2_adduswq = 135,
  Instruction_lvx_v2_addw = 136,
  Instruction_lvx_v2_addwq = 137,
  Instruction_lvx_v2_addx16bx = 138,
  Instruction_lvx_v2_addx16d = 139,
  Instruction_lvx_v2_addx16dp = 140,
  Instruction_lvx_v2_addx16ho = 141,
  Instruction_lvx_v2_addx16w = 142,
  Instruction_lvx_v2_addx16wq = 143,
  Instruction_lvx_v2_addx2bx = 144,
  Instruction_lvx_v2_addx2d = 145,
  Instruction_lvx_v2_addx2dp = 146,
  Instruction_lvx_v2_addx2ho = 147,
  Instruction_lvx_v2_addx2w = 148,
  Instruction_lvx_v2_addx2wq = 149,
  Instruction_lvx_v2_addx32d = 150,
  Instruction_lvx_v2_addx32w = 151,
  Instruction_lvx_v2_addx4bx = 152,
  Instruction_lvx_v2_addx4d = 153,
  Instruction_lvx_v2_addx4dp = 154,
  Instruction_lvx_v2_addx4ho = 155,
  Instruction_lvx_v2_addx4w = 156,
  Instruction_lvx_v2_addx4wq = 157,
  Instruction_lvx_v2_addx64d = 158,
  Instruction_lvx_v2_addx64w = 159,
  Instruction_lvx_v2_addx8bx = 160,
  Instruction_lvx_v2_addx8d = 161,
  Instruction_lvx_v2_addx8dp = 162,
  Instruction_lvx_v2_addx8ho = 163,
  Instruction_lvx_v2_addx8w = 164,
  Instruction_lvx_v2_addx8wq = 165,
  Instruction_lvx_v2_aladdb = 166,
  Instruction_lvx_v2_aladdd = 167,
  Instruction_lvx_v2_aladdh = 168,
  Instruction_lvx_v2_aladdw = 169,
  Instruction_lvx_v2_alandb = 170,
  Instruction_lvx_v2_alandd = 171,
  Instruction_lvx_v2_alandh = 172,
  Instruction_lvx_v2_alandw = 173,
  Instruction_lvx_v2_alb = 174,
  Instruction_lvx_v2_alclrb = 175,
  Instruction_lvx_v2_alclrd = 176,
  Instruction_lvx_v2_alclrh = 177,
  Instruction_lvx_v2_alclrw = 178,
  Instruction_lvx_v2_ald = 179,
  Instruction_lvx_v2_aldusb = 180,
  Instruction_lvx_v2_aldusd = 181,
  Instruction_lvx_v2_aldush = 182,
  Instruction_lvx_v2_aldusw = 183,
  Instruction_lvx_v2_aleorb = 184,
  Instruction_lvx_v2_aleord = 185,
  Instruction_lvx_v2_aleorh = 186,
  Instruction_lvx_v2_aleorw = 187,
  Instruction_lvx_v2_alh = 188,
  Instruction_lvx_v2_aliorb = 189,
  Instruction_lvx_v2_aliord = 190,
  Instruction_lvx_v2_aliorh = 191,
  Instruction_lvx_v2_aliorw = 192,
  Instruction_lvx_v2_almaxb = 193,
  Instruction_lvx_v2_almaxd = 194,
  Instruction_lvx_v2_almaxh = 195,
  Instruction_lvx_v2_almaxub = 196,
  Instruction_lvx_v2_almaxud = 197,
  Instruction_lvx_v2_almaxuh = 198,
  Instruction_lvx_v2_almaxuw = 199,
  Instruction_lvx_v2_almaxw = 200,
  Instruction_lvx_v2_alminb = 201,
  Instruction_lvx_v2_almind = 202,
  Instruction_lvx_v2_alminh = 203,
  Instruction_lvx_v2_alminub = 204,
  Instruction_lvx_v2_alminud = 205,
  Instruction_lvx_v2_alminuh = 206,
  Instruction_lvx_v2_alminuw = 207,
  Instruction_lvx_v2_alminw = 208,
  Instruction_lvx_v2_alw = 209,
  Instruction_lvx_v2_andd = 210,
  Instruction_lvx_v2_andnd = 211,
  Instruction_lvx_v2_andnq = 212,
  Instruction_lvx_v2_andnw = 213,
  Instruction_lvx_v2_andq = 214,
  Instruction_lvx_v2_andw = 215,
  Instruction_lvx_v2_asaddb = 216,
  Instruction_lvx_v2_asaddd = 217,
  Instruction_lvx_v2_asaddh = 218,
  Instruction_lvx_v2_asaddw = 219,
  Instruction_lvx_v2_asandb = 220,
  Instruction_lvx_v2_asandd = 221,
  Instruction_lvx_v2_asandh = 222,
  Instruction_lvx_v2_asandw = 223,
  Instruction_lvx_v2_asb = 224,
  Instruction_lvx_v2_asd = 225,
  Instruction_lvx_v2_asdusb = 226,
  Instruction_lvx_v2_asdusd = 227,
  Instruction_lvx_v2_asdush = 228,
  Instruction_lvx_v2_asdusw = 229,
  Instruction_lvx_v2_aseorb = 230,
  Instruction_lvx_v2_aseord = 231,
  Instruction_lvx_v2_aseorh = 232,
  Instruction_lvx_v2_aseorw = 233,
  Instruction_lvx_v2_ash = 234,
  Instruction_lvx_v2_asiorb = 235,
  Instruction_lvx_v2_asiord = 236,
  Instruction_lvx_v2_asiorh = 237,
  Instruction_lvx_v2_asiorw = 238,
  Instruction_lvx_v2_asmaxb = 239,
  Instruction_lvx_v2_asmaxd = 240,
  Instruction_lvx_v2_asmaxh = 241,
  Instruction_lvx_v2_asmaxub = 242,
  Instruction_lvx_v2_asmaxud = 243,
  Instruction_lvx_v2_asmaxuh = 244,
  Instruction_lvx_v2_asmaxuw = 245,
  Instruction_lvx_v2_asmaxw = 246,
  Instruction_lvx_v2_asminb = 247,
  Instruction_lvx_v2_asmind = 248,
  Instruction_lvx_v2_asminh = 249,
  Instruction_lvx_v2_asminub = 250,
  Instruction_lvx_v2_asminud = 251,
  Instruction_lvx_v2_asminuh = 252,
  Instruction_lvx_v2_asminuw = 253,
  Instruction_lvx_v2_asminw = 254,
  Instruction_lvx_v2_asw = 255,
  Instruction_lvx_v2_aswapb = 256,
  Instruction_lvx_v2_aswapd = 257,
  Instruction_lvx_v2_aswaph = 258,
  Instruction_lvx_v2_aswapw = 259,
  Instruction_lvx_v2_avgbx = 260,
  Instruction_lvx_v2_avgho = 261,
  Instruction_lvx_v2_avgrbx = 262,
  Instruction_lvx_v2_avgrho = 263,
  Instruction_lvx_v2_avgrubx = 264,
  Instruction_lvx_v2_avgruho = 265,
  Instruction_lvx_v2_avgruw = 266,
  Instruction_lvx_v2_avgruwq = 267,
  Instruction_lvx_v2_avgrw = 268,
  Instruction_lvx_v2_avgrwq = 269,
  Instruction_lvx_v2_avgubx = 270,
  Instruction_lvx_v2_avguho = 271,
  Instruction_lvx_v2_avguw = 272,
  Instruction_lvx_v2_avguwq = 273,
  Instruction_lvx_v2_avgw = 274,
  Instruction_lvx_v2_avgwq = 275,
  Instruction_lvx_v2_await = 276,
  Instruction_lvx_v2_barrier = 277,
  Instruction_lvx_v2_blend = 278,
  Instruction_lvx_v2_break = 279,
  Instruction_lvx_v2_call = 280,
  Instruction_lvx_v2_callx = 281,
  Instruction_lvx_v2_catdq = 282,
  Instruction_lvx_v2_cb = 283,
  Instruction_lvx_v2_cbsd = 284,
  Instruction_lvx_v2_cbsdp = 285,
  Instruction_lvx_v2_cbsho = 286,
  Instruction_lvx_v2_cbsw = 287,
  Instruction_lvx_v2_cbswq = 288,
  Instruction_lvx_v2_cbx = 289,
  Instruction_lvx_v2_ccb = 290,
  Instruction_lvx_v2_ccbx = 291,
  Instruction_lvx_v2_clsd = 292,
  Instruction_lvx_v2_clsdp = 293,
  Instruction_lvx_v2_clsho = 294,
  Instruction_lvx_v2_clsw = 295,
  Instruction_lvx_v2_clswq = 296,
  Instruction_lvx_v2_clzd = 297,
  Instruction_lvx_v2_clzdp = 298,
  Instruction_lvx_v2_clzho = 299,
  Instruction_lvx_v2_clzw = 300,
  Instruction_lvx_v2_clzwq = 301,
  Instruction_lvx_v2_cmovebx = 302,
  Instruction_lvx_v2_cmoved = 303,
  Instruction_lvx_v2_cmovedp = 304,
  Instruction_lvx_v2_cmoveho = 305,
  Instruction_lvx_v2_cmoveq = 306,
  Instruction_lvx_v2_cmovewq = 307,
  Instruction_lvx_v2_compbx = 308,
  Instruction_lvx_v2_compd = 309,
  Instruction_lvx_v2_compdp = 310,
  Instruction_lvx_v2_compho = 311,
  Instruction_lvx_v2_compnbx = 312,
  Instruction_lvx_v2_compndp = 313,
  Instruction_lvx_v2_compnho = 314,
  Instruction_lvx_v2_compnwq = 315,
  Instruction_lvx_v2_compq = 316,
  Instruction_lvx_v2_compw = 317,
  Instruction_lvx_v2_compwq = 318,
  Instruction_lvx_v2_copyd = 319,
  Instruction_lvx_v2_copyo = 320,
  Instruction_lvx_v2_copyq = 321,
  Instruction_lvx_v2_copyw = 322,
  Instruction_lvx_v2_crcbellw = 323,
  Instruction_lvx_v2_crcbelmw = 324,
  Instruction_lvx_v2_crclellw = 325,
  Instruction_lvx_v2_crclelmw = 326,
  Instruction_lvx_v2_csrrc = 327,
  Instruction_lvx_v2_csrrs = 328,
  Instruction_lvx_v2_csrrw = 329,
  Instruction_lvx_v2_ctzd = 330,
  Instruction_lvx_v2_ctzdp = 331,
  Instruction_lvx_v2_ctzho = 332,
  Instruction_lvx_v2_ctzw = 333,
  Instruction_lvx_v2_ctzwq = 334,
  Instruction_lvx_v2_d1inval = 335,
  Instruction_lvx_v2_dflushl = 336,
  Instruction_lvx_v2_dflushsw = 337,
  Instruction_lvx_v2_dinvall = 338,
  Instruction_lvx_v2_dinvalsw = 339,
  Instruction_lvx_v2_divmodd = 340,
  Instruction_lvx_v2_divmodud = 341,
  Instruction_lvx_v2_divmoduw = 342,
  Instruction_lvx_v2_divmodw = 343,
  Instruction_lvx_v2_dpurgel = 344,
  Instruction_lvx_v2_dpurgesw = 345,
  Instruction_lvx_v2_dtouchl = 346,
  Instruction_lvx_v2_eord = 347,
  Instruction_lvx_v2_eorq = 348,
  Instruction_lvx_v2_eorw = 349,
  Instruction_lvx_v2_errop = 350,
  Instruction_lvx_v2_extfs = 351,
  Instruction_lvx_v2_extfz = 352,
  Instruction_lvx_v2_extlqbho = 353,
  Instruction_lvx_v2_extlqhwq = 354,
  Instruction_lvx_v2_extlqnbx = 355,
  Instruction_lvx_v2_extlqwdp = 356,
  Instruction_lvx_v2_extlsbho = 357,
  Instruction_lvx_v2_extlshwq = 358,
  Instruction_lvx_v2_extlsnbx = 359,
  Instruction_lvx_v2_extlswdp = 360,
  Instruction_lvx_v2_extlzbho = 361,
  Instruction_lvx_v2_extlzhwq = 362,
  Instruction_lvx_v2_extlznbx = 363,
  Instruction_lvx_v2_extlzwdp = 364,
  Instruction_lvx_v2_fabsd = 365,
  Instruction_lvx_v2_fabsdp = 366,
  Instruction_lvx_v2_fabsh = 367,
  Instruction_lvx_v2_fabsho = 368,
  Instruction_lvx_v2_fabsw = 369,
  Instruction_lvx_v2_fabswp = 370,
  Instruction_lvx_v2_fabswq = 371,
  Instruction_lvx_v2_faddd = 372,
  Instruction_lvx_v2_fadddp = 373,
  Instruction_lvx_v2_faddh = 374,
  Instruction_lvx_v2_faddho = 375,
  Instruction_lvx_v2_faddw = 376,
  Instruction_lvx_v2_faddwc = 377,
  Instruction_lvx_v2_faddwq = 378,
  Instruction_lvx_v2_fclassd = 379,
  Instruction_lvx_v2_fclassh = 380,
  Instruction_lvx_v2_fclassw = 381,
  Instruction_lvx_v2_fclasswp = 382,
  Instruction_lvx_v2_fclasswq = 383,
  Instruction_lvx_v2_fcompd = 384,
  Instruction_lvx_v2_fcompdp = 385,
  Instruction_lvx_v2_fcomph = 386,
  Instruction_lvx_v2_fcompho = 387,
  Instruction_lvx_v2_fcompndp = 388,
  Instruction_lvx_v2_fcompnho = 389,
  Instruction_lvx_v2_fcompnwq = 390,
  Instruction_lvx_v2_fcompw = 391,
  Instruction_lvx_v2_fcompwq = 392,
  Instruction_lvx_v2_fdivd = 393,
  Instruction_lvx_v2_fdivh = 394,
  Instruction_lvx_v2_fdivw = 395,
  Instruction_lvx_v2_fence = 396,
  Instruction_lvx_v2_fextlhwq = 397,
  Instruction_lvx_v2_ffmad = 398,
  Instruction_lvx_v2_ffmadp = 399,
  Instruction_lvx_v2_ffmah = 400,
  Instruction_lvx_v2_ffmaho = 401,
  Instruction_lvx_v2_ffmaw = 402,
  Instruction_lvx_v2_ffmawc = 403,
  Instruction_lvx_v2_ffmawq = 404,
  Instruction_lvx_v2_ffmsd = 405,
  Instruction_lvx_v2_ffmsdp = 406,
  Instruction_lvx_v2_ffmsh = 407,
  Instruction_lvx_v2_ffmsho = 408,
  Instruction_lvx_v2_ffmsw = 409,
  Instruction_lvx_v2_ffmswq = 410,
  Instruction_lvx_v2_fixedd = 411,
  Instruction_lvx_v2_fixeddp = 412,
  Instruction_lvx_v2_fixeddw = 413,
  Instruction_lvx_v2_fixedho = 414,
  Instruction_lvx_v2_fixedud = 415,
  Instruction_lvx_v2_fixedudp = 416,
  Instruction_lvx_v2_fixedudw = 417,
  Instruction_lvx_v2_fixeduho = 418,
  Instruction_lvx_v2_fixeduw = 419,
  Instruction_lvx_v2_fixeduwd = 420,
  Instruction_lvx_v2_fixeduwp = 421,
  Instruction_lvx_v2_fixeduwq = 422,
  Instruction_lvx_v2_fixedw = 423,
  Instruction_lvx_v2_fixedwd = 424,
  Instruction_lvx_v2_fixedwp = 425,
  Instruction_lvx_v2_fixedwq = 426,
  Instruction_lvx_v2_floatd = 427,
  Instruction_lvx_v2_floatdp = 428,
  Instruction_lvx_v2_floatdw = 429,
  Instruction_lvx_v2_floatho = 430,
  Instruction_lvx_v2_floatud = 431,
  Instruction_lvx_v2_floatudp = 432,
  Instruction_lvx_v2_floatudw = 433,
  Instruction_lvx_v2_floatuho = 434,
  Instruction_lvx_v2_floatuw = 435,
  Instruction_lvx_v2_floatuwd = 436,
  Instruction_lvx_v2_floatuwp = 437,
  Instruction_lvx_v2_floatuwq = 438,
  Instruction_lvx_v2_floatw = 439,
  Instruction_lvx_v2_floatwd = 440,
  Instruction_lvx_v2_floatwp = 441,
  Instruction_lvx_v2_floatwq = 442,
  Instruction_lvx_v2_fmaxd = 443,
  Instruction_lvx_v2_fmaxdp = 444,
  Instruction_lvx_v2_fmaxh = 445,
  Instruction_lvx_v2_fmaxho = 446,
  Instruction_lvx_v2_fmaxnd = 447,
  Instruction_lvx_v2_fmaxndp = 448,
  Instruction_lvx_v2_fmaxnh = 449,
  Instruction_lvx_v2_fmaxnho = 450,
  Instruction_lvx_v2_fmaxnw = 451,
  Instruction_lvx_v2_fmaxnwq = 452,
  Instruction_lvx_v2_fmaxw = 453,
  Instruction_lvx_v2_fmaxwq = 454,
  Instruction_lvx_v2_fmind = 455,
  Instruction_lvx_v2_fmindp = 456,
  Instruction_lvx_v2_fminh = 457,
  Instruction_lvx_v2_fminho = 458,
  Instruction_lvx_v2_fminnd = 459,
  Instruction_lvx_v2_fminndp = 460,
  Instruction_lvx_v2_fminnh = 461,
  Instruction_lvx_v2_fminnho = 462,
  Instruction_lvx_v2_fminnw = 463,
  Instruction_lvx_v2_fminnwq = 464,
  Instruction_lvx_v2_fminw = 465,
  Instruction_lvx_v2_fminwq = 466,
  Instruction_lvx_v2_fmuld = 467,
  Instruction_lvx_v2_fmuldp = 468,
  Instruction_lvx_v2_fmulh = 469,
  Instruction_lvx_v2_fmulho = 470,
  Instruction_lvx_v2_fmulw = 471,
  Instruction_lvx_v2_fmulwc = 472,
  Instruction_lvx_v2_fmulwq = 473,
  Instruction_lvx_v2_fnarrowdw = 474,
  Instruction_lvx_v2_fnarrowdwq = 475,
  Instruction_lvx_v2_fnarrowwh = 476,
  Instruction_lvx_v2_fnarrowwhq = 477,
  Instruction_lvx_v2_fnegd = 478,
  Instruction_lvx_v2_fnegdp = 479,
  Instruction_lvx_v2_fnegh = 480,
  Instruction_lvx_v2_fnegho = 481,
  Instruction_lvx_v2_fnegw = 482,
  Instruction_lvx_v2_fnegwp = 483,
  Instruction_lvx_v2_fnegwq = 484,
  Instruction_lvx_v2_fractdwq = 485,
  Instruction_lvx_v2_fracthbx = 486,
  Instruction_lvx_v2_fractwho = 487,
  Instruction_lvx_v2_frcsr = 488,
  Instruction_lvx_v2_frflags = 489,
  Instruction_lvx_v2_frintd = 490,
  Instruction_lvx_v2_frinth = 491,
  Instruction_lvx_v2_frintw = 492,
  Instruction_lvx_v2_frrm = 493,
  Instruction_lvx_v2_fsbfd = 494,
  Instruction_lvx_v2_fsbfdp = 495,
  Instruction_lvx_v2_fsbfh = 496,
  Instruction_lvx_v2_fsbfho = 497,
  Instruction_lvx_v2_fsbfw = 498,
  Instruction_lvx_v2_fsbfwc = 499,
  Instruction_lvx_v2_fsbfwq = 500,
  Instruction_lvx_v2_fscsr = 501,
  Instruction_lvx_v2_fsflags = 502,
  Instruction_lvx_v2_fsignd = 503,
  Instruction_lvx_v2_fsigndp = 504,
  Instruction_lvx_v2_fsignh = 505,
  Instruction_lvx_v2_fsignho = 506,
  Instruction_lvx_v2_fsignmd = 507,
  Instruction_lvx_v2_fsignmdp = 508,
  Instruction_lvx_v2_fsignmh = 509,
  Instruction_lvx_v2_fsignmho = 510,
  Instruction_lvx_v2_fsignmw = 511,
  Instruction_lvx_v2_fsignmwq = 512,
  Instruction_lvx_v2_fsignnd = 513,
  Instruction_lvx_v2_fsignndp = 514,
  Instruction_lvx_v2_fsignnh = 515,
  Instruction_lvx_v2_fsignnho = 516,
  Instruction_lvx_v2_fsignnw = 517,
  Instruction_lvx_v2_fsignnwq = 518,
  Instruction_lvx_v2_fsignw = 519,
  Instruction_lvx_v2_fsignwq = 520,
  Instruction_lvx_v2_fsqrtd = 521,
  Instruction_lvx_v2_fsqrth = 522,
  Instruction_lvx_v2_fsqrtw = 523,
  Instruction_lvx_v2_fsrecd = 524,
  Instruction_lvx_v2_fsrecw = 525,
  Instruction_lvx_v2_fsrecwp = 526,
  Instruction_lvx_v2_fsrecwq = 527,
  Instruction_lvx_v2_fsrm = 528,
  Instruction_lvx_v2_fsrsrd = 529,
  Instruction_lvx_v2_fsrsrw = 530,
  Instruction_lvx_v2_fsrsrwp = 531,
  Instruction_lvx_v2_fsrsrwq = 532,
  Instruction_lvx_v2_fwidenhw = 533,
  Instruction_lvx_v2_fwidenhwq = 534,
  Instruction_lvx_v2_fwidenwd = 535,
  Instruction_lvx_v2_fwidenwdp = 536,
  Instruction_lvx_v2_get = 537,
  Instruction_lvx_v2_goto = 538,
  Instruction_lvx_v2_gotox = 539,
  Instruction_lvx_v2_guard = 540,
  Instruction_lvx_v2_i1inval = 541,
  Instruction_lvx_v2_i1invals = 542,
  Instruction_lvx_v2_icall = 543,
  Instruction_lvx_v2_iget = 544,
  Instruction_lvx_v2_igoto = 545,
  Instruction_lvx_v2_insf = 546,
  Instruction_lvx_v2_iord = 547,
  Instruction_lvx_v2_iornd = 548,
  Instruction_lvx_v2_iornq = 549,
  Instruction_lvx_v2_iornw = 550,
  Instruction_lvx_v2_iorq = 551,
  Instruction_lvx_v2_iorw = 552,
  Instruction_lvx_v2_landd = 553,
  Instruction_lvx_v2_landw = 554,
  Instruction_lvx_v2_lbs = 555,
  Instruction_lvx_v2_lbz = 556,
  Instruction_lvx_v2_ld = 557,
  Instruction_lvx_v2_lhs = 558,
  Instruction_lvx_v2_lhz = 559,
  Instruction_lvx_v2_liord = 560,
  Instruction_lvx_v2_liorw = 561,
  Instruction_lvx_v2_lnandd = 562,
  Instruction_lvx_v2_lnandw = 563,
  Instruction_lvx_v2_lniord = 564,
  Instruction_lvx_v2_lniorw = 565,
  Instruction_lvx_v2_lo = 566,
  Instruction_lvx_v2_loopdo = 567,
  Instruction_lvx_v2_lq = 568,
  Instruction_lvx_v2_lws = 569,
  Instruction_lvx_v2_lwz = 570,
  Instruction_lvx_v2_maddbho = 571,
  Instruction_lvx_v2_maddd = 572,
  Instruction_lvx_v2_madddp = 573,
  Instruction_lvx_v2_madddq = 574,
  Instruction_lvx_v2_madddt = 575,
  Instruction_lvx_v2_maddhwq = 576,
  Instruction_lvx_v2_maddsudt = 577,
  Instruction_lvx_v2_maddsuwd = 578,
  Instruction_lvx_v2_maddudt = 579,
  Instruction_lvx_v2_madduwd = 580,
  Instruction_lvx_v2_maddw = 581,
  Instruction_lvx_v2_maddwd = 582,
  Instruction_lvx_v2_maddwdp = 583,
  Instruction_lvx_v2_maddxbho = 584,
  Instruction_lvx_v2_maddxhwq = 585,
  Instruction_lvx_v2_maddxwdp = 586,
  Instruction_lvx_v2_make = 587,
  Instruction_lvx_v2_maxbx = 588,
  Instruction_lvx_v2_maxd = 589,
  Instruction_lvx_v2_maxdp = 590,
  Instruction_lvx_v2_maxho = 591,
  Instruction_lvx_v2_maxubx = 592,
  Instruction_lvx_v2_maxud = 593,
  Instruction_lvx_v2_maxudp = 594,
  Instruction_lvx_v2_maxuho = 595,
  Instruction_lvx_v2_maxuw = 596,
  Instruction_lvx_v2_maxuwq = 597,
  Instruction_lvx_v2_maxw = 598,
  Instruction_lvx_v2_maxwq = 599,
  Instruction_lvx_v2_minbx = 600,
  Instruction_lvx_v2_mind = 601,
  Instruction_lvx_v2_mindp = 602,
  Instruction_lvx_v2_minho = 603,
  Instruction_lvx_v2_minubx = 604,
  Instruction_lvx_v2_minud = 605,
  Instruction_lvx_v2_minudp = 606,
  Instruction_lvx_v2_minuho = 607,
  Instruction_lvx_v2_minuw = 608,
  Instruction_lvx_v2_minuwq = 609,
  Instruction_lvx_v2_minw = 610,
  Instruction_lvx_v2_minwq = 611,
  Instruction_lvx_v2_msbfbho = 612,
  Instruction_lvx_v2_msbfd = 613,
  Instruction_lvx_v2_msbfdp = 614,
  Instruction_lvx_v2_msbfdq = 615,
  Instruction_lvx_v2_msbfdt = 616,
  Instruction_lvx_v2_msbfhwq = 617,
  Instruction_lvx_v2_msbfsudt = 618,
  Instruction_lvx_v2_msbfsuwd = 619,
  Instruction_lvx_v2_msbfudt = 620,
  Instruction_lvx_v2_msbfuwd = 621,
  Instruction_lvx_v2_msbfw = 622,
  Instruction_lvx_v2_msbfwd = 623,
  Instruction_lvx_v2_msbfwdp = 624,
  Instruction_lvx_v2_msbfxbho = 625,
  Instruction_lvx_v2_msbfxhwq = 626,
  Instruction_lvx_v2_msbfxwdp = 627,
  Instruction_lvx_v2_mulbho = 628,
  Instruction_lvx_v2_muld = 629,
  Instruction_lvx_v2_muldp = 630,
  Instruction_lvx_v2_muldq = 631,
  Instruction_lvx_v2_muldt = 632,
  Instruction_lvx_v2_mulho = 633,
  Instruction_lvx_v2_mulhwq = 634,
  Instruction_lvx_v2_mulnbho = 635,
  Instruction_lvx_v2_mulnd = 636,
  Instruction_lvx_v2_mulndp = 637,
  Instruction_lvx_v2_mulndq = 638,
  Instruction_lvx_v2_mulnho = 639,
  Instruction_lvx_v2_mulnhwq = 640,
  Instruction_lvx_v2_mulnw = 641,
  Instruction_lvx_v2_mulnwd = 642,
  Instruction_lvx_v2_mulnwdp = 643,
  Instruction_lvx_v2_mulnwq = 644,
  Instruction_lvx_v2_mulnxbho = 645,
  Instruction_lvx_v2_mulnxhwq = 646,
  Instruction_lvx_v2_mulnxwdp = 647,
  Instruction_lvx_v2_mulsudt = 648,
  Instruction_lvx_v2_mulsuwd = 649,
  Instruction_lvx_v2_muludt = 650,
  Instruction_lvx_v2_muluwd = 651,
  Instruction_lvx_v2_mulw = 652,
  Instruction_lvx_v2_mulwd = 653,
  Instruction_lvx_v2_mulwdp = 654,
  Instruction_lvx_v2_mulwq = 655,
  Instruction_lvx_v2_mulxbho = 656,
  Instruction_lvx_v2_mulxhwq = 657,
  Instruction_lvx_v2_mulxwdp = 658,
  Instruction_lvx_v2_nandd = 659,
  Instruction_lvx_v2_nandq = 660,
  Instruction_lvx_v2_nandw = 661,
  Instruction_lvx_v2_negbx = 662,
  Instruction_lvx_v2_negd = 663,
  Instruction_lvx_v2_negdp = 664,
  Instruction_lvx_v2_negho = 665,
  Instruction_lvx_v2_negq = 666,
  Instruction_lvx_v2_negsbx = 667,
  Instruction_lvx_v2_negsd = 668,
  Instruction_lvx_v2_negsdp = 669,
  Instruction_lvx_v2_negsho = 670,
  Instruction_lvx_v2_negsw = 671,
  Instruction_lvx_v2_negswq = 672,
  Instruction_lvx_v2_negw = 673,
  Instruction_lvx_v2_negwq = 674,
  Instruction_lvx_v2_neord = 675,
  Instruction_lvx_v2_neorq = 676,
  Instruction_lvx_v2_neorw = 677,
  Instruction_lvx_v2_niord = 678,
  Instruction_lvx_v2_niorq = 679,
  Instruction_lvx_v2_niorw = 680,
  Instruction_lvx_v2_nop = 681,
  Instruction_lvx_v2_notd = 682,
  Instruction_lvx_v2_notq = 683,
  Instruction_lvx_v2_notw = 684,
  Instruction_lvx_v2_pcrel = 685,
  Instruction_lvx_v2_ret = 686,
  Instruction_lvx_v2_rfe = 687,
  Instruction_lvx_v2_rold = 688,
  Instruction_lvx_v2_rolw = 689,
  Instruction_lvx_v2_rolwq = 690,
  Instruction_lvx_v2_rord = 691,
  Instruction_lvx_v2_rorw = 692,
  Instruction_lvx_v2_rorwq = 693,
  Instruction_lvx_v2_rswap = 694,
  Instruction_lvx_v2_sb = 695,
  Instruction_lvx_v2_sbfbx = 696,
  Instruction_lvx_v2_sbfd = 697,
  Instruction_lvx_v2_sbfdp = 698,
  Instruction_lvx_v2_sbfho = 699,
  Instruction_lvx_v2_sbfq = 700,
  Instruction_lvx_v2_sbfsbx = 701,
  Instruction_lvx_v2_sbfsd = 702,
  Instruction_lvx_v2_sbfsdp = 703,
  Instruction_lvx_v2_sbfsho = 704,
  Instruction_lvx_v2_sbfsw = 705,
  Instruction_lvx_v2_sbfswq = 706,
  Instruction_lvx_v2_sbfusbx = 707,
  Instruction_lvx_v2_sbfusd = 708,
  Instruction_lvx_v2_sbfusdp = 709,
  Instruction_lvx_v2_sbfusho = 710,
  Instruction_lvx_v2_sbfusw = 711,
  Instruction_lvx_v2_sbfuswq = 712,
  Instruction_lvx_v2_sbfw = 713,
  Instruction_lvx_v2_sbfwq = 714,
  Instruction_lvx_v2_sbmm8 = 715,
  Instruction_lvx_v2_sbmm8d = 716,
  Instruction_lvx_v2_sbmm8dp = 717,
  Instruction_lvx_v2_sbmm8eord = 718,
  Instruction_lvx_v2_sbmm8eordp = 719,
  Instruction_lvx_v2_sbmmt8 = 720,
  Instruction_lvx_v2_sbmmt8d = 721,
  Instruction_lvx_v2_sbmmt8dp = 722,
  Instruction_lvx_v2_sbmmt8eord = 723,
  Instruction_lvx_v2_sbmmt8eordp = 724,
  Instruction_lvx_v2_scall = 725,
  Instruction_lvx_v2_sd = 726,
  Instruction_lvx_v2_set = 727,
  Instruction_lvx_v2_sh = 728,
  Instruction_lvx_v2_signbx = 729,
  Instruction_lvx_v2_signd = 730,
  Instruction_lvx_v2_signdp = 731,
  Instruction_lvx_v2_signho = 732,
  Instruction_lvx_v2_signsbx = 733,
  Instruction_lvx_v2_signsd = 734,
  Instruction_lvx_v2_signsdp = 735,
  Instruction_lvx_v2_signsho = 736,
  Instruction_lvx_v2_signsw = 737,
  Instruction_lvx_v2_signswq = 738,
  Instruction_lvx_v2_signw = 739,
  Instruction_lvx_v2_signwq = 740,
  Instruction_lvx_v2_sleep = 741,
  Instruction_lvx_v2_sllbx = 742,
  Instruction_lvx_v2_slld = 743,
  Instruction_lvx_v2_slldp = 744,
  Instruction_lvx_v2_sllho = 745,
  Instruction_lvx_v2_sllw = 746,
  Instruction_lvx_v2_sllwq = 747,
  Instruction_lvx_v2_slsbx = 748,
  Instruction_lvx_v2_slsd = 749,
  Instruction_lvx_v2_slsdp = 750,
  Instruction_lvx_v2_slsho = 751,
  Instruction_lvx_v2_slsw = 752,
  Instruction_lvx_v2_slswq = 753,
  Instruction_lvx_v2_slusbx = 754,
  Instruction_lvx_v2_slusd = 755,
  Instruction_lvx_v2_slusdp = 756,
  Instruction_lvx_v2_slusho = 757,
  Instruction_lvx_v2_slusw = 758,
  Instruction_lvx_v2_sluswq = 759,
  Instruction_lvx_v2_so = 760,
  Instruction_lvx_v2_sq = 761,
  Instruction_lvx_v2_srabx = 762,
  Instruction_lvx_v2_srad = 763,
  Instruction_lvx_v2_sradp = 764,
  Instruction_lvx_v2_sraho = 765,
  Instruction_lvx_v2_sraw = 766,
  Instruction_lvx_v2_srawq = 767,
  Instruction_lvx_v2_srlbx = 768,
  Instruction_lvx_v2_srld = 769,
  Instruction_lvx_v2_srldp = 770,
  Instruction_lvx_v2_srlho = 771,
  Instruction_lvx_v2_srlw = 772,
  Instruction_lvx_v2_srlwq = 773,
  Instruction_lvx_v2_srsbx = 774,
  Instruction_lvx_v2_srsd = 775,
  Instruction_lvx_v2_srsdp = 776,
  Instruction_lvx_v2_srsho = 777,
  Instruction_lvx_v2_srsw = 778,
  Instruction_lvx_v2_srswq = 779,
  Instruction_lvx_v2_stop = 780,
  Instruction_lvx_v2_stsud = 781,
  Instruction_lvx_v2_stsudp = 782,
  Instruction_lvx_v2_stsuho = 783,
  Instruction_lvx_v2_stsuw = 784,
  Instruction_lvx_v2_stsuwq = 785,
  Instruction_lvx_v2_sw = 786,
  Instruction_lvx_v2_sxbd = 787,
  Instruction_lvx_v2_sxhd = 788,
  Instruction_lvx_v2_sxwd = 789,
  Instruction_lvx_v2_syncgroup = 790,
  Instruction_lvx_v2_tlbdinval = 791,
  Instruction_lvx_v2_tlbiinval = 792,
  Instruction_lvx_v2_tlbprobe = 793,
  Instruction_lvx_v2_tlbread = 794,
  Instruction_lvx_v2_tlbwrite = 795,
  Instruction_lvx_v2_truncdwq = 796,
  Instruction_lvx_v2_trunchbx = 797,
  Instruction_lvx_v2_truncwho = 798,
  Instruction_lvx_v2_waitit = 799,
  Instruction_lvx_v2_wfxl = 800,
  Instruction_lvx_v2_wfxm = 801,
  Instruction_lvx_v2_widenqbho = 802,
  Instruction_lvx_v2_widenqhwq = 803,
  Instruction_lvx_v2_widenqwdp = 804,
  Instruction_lvx_v2_widensbho = 805,
  Instruction_lvx_v2_widenshwq = 806,
  Instruction_lvx_v2_widenswdp = 807,
  Instruction_lvx_v2_widenzbho = 808,
  Instruction_lvx_v2_widenzhwq = 809,
  Instruction_lvx_v2_widenzwdp = 810,
  Instruction_lvx_v2_xaccesso = 811,
  Instruction_lvx_v2_xaligno = 812,
  Instruction_lvx_v2_xcopyo = 813,
  Instruction_lvx_v2_xlo = 814,
  Instruction_lvx_v2_xmovefd = 815,
  Instruction_lvx_v2_xmovefo = 816,
  Instruction_lvx_v2_xmovefq = 817,
  Instruction_lvx_v2_xmovetd = 818,
  Instruction_lvx_v2_xmoveto = 819,
  Instruction_lvx_v2_xmovetq = 820,
  Instruction_lvx_v2_xplb = 821,
  Instruction_lvx_v2_xpld = 822,
  Instruction_lvx_v2_xplh = 823,
  Instruction_lvx_v2_xplo = 824,
  Instruction_lvx_v2_xplq = 825,
  Instruction_lvx_v2_xplw = 826,
  Instruction_lvx_v2_xso = 827,
  Instruction_lvx_v2_zxbd = 828,
  Instruction_lvx_v2_zxhd = 829,
  Instruction_lvx_v2_zxwd = 830,
  Separator_lvx_v2_comma = 831,
  Separator_lvx_v2_equal = 832,
  Separator_lvx_v2_qmark = 833,
  Separator_lvx_v2_rsbracket = 834,
  Separator_lvx_v2_lsbracket = 835
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
