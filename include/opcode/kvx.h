/* KVX assembler/disassembler support.

   Copyright (C) 2009-2024 Free Software Foundation, Inc.
   Contributed by Kalray SA.

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


#ifndef OPCODE_KVX_H
#define OPCODE_KVX_H

#define KVX_NUMCORES 3
#define KVX_MAXSYLLABLES 3
#define KVX_MAXOPERANDS 7
#define KVX_MAXBUNDLEISSUE 10
#define KVX_MAXBUNDLEWORDS 18


/*
 * The following macros are provided for compatibility with old
 * code.  They should not be used in new code.
 */

#define KV4_ACTIVATE_OFFSET	6
#define KV4_ACTIVATE_WIDTH	8
#define KV4_ACTIVATE_MASK	\
  (((1 << KV4_ACTIVATE_WIDTH) - 1) << KV4_ACTIVATE_OFFSET)
#define KV4_BCU_GUARD_OPCODE	0x0f800000
#define KV4_BCU_GUARD_MASK	0x7ffc0000
#define KV4_BCU_BLEND_OPCODE	0x0f840000
#define KV4_BCU_BLEND_MASK	0x7ffc0000

/***********************************************/
/*       DATA TYPES                            */
/***********************************************/

/*  Operand definition -- used in building     */
/*  format table                               */

enum kvx_rel {
  /* Absolute relocation. */
  KVX_REL_ABS,
  /* PC relative relocation. */
  KVX_REL_PC,
  /* GP relative relocation. */
  KVX_REL_GP,
  /* TP relative relocation. */
  KVX_REL_TP,
  /* GOT relative relocation. */
  KVX_REL_GOT,
  /* BASE load address relative relocation. */
  KVX_REL_BASE,
};

struct kvx_reloc {
  /* Size in bits. */
  int bitsize;
  /* Type of relative relocation. */
  enum kvx_rel relative;
  /* Number of BFD relocations. */
  int reloc_nb;
  /* List of BFD relocations. */
  unsigned int relocs[];
};

struct kvx_bitfield {
  /* Number of bits.  */
  int size;
  /* Offset in abstract value.  */
  int from_offset;
  /* Offset in encoded value.  */
  int to_offset;
};

struct kvx_operand {
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
  struct kvx_reloc **relocs;
  /* Number of given bitfields.  */
  int bitfields;
  /* Bitfields in most to least significant order.  */
  struct kvx_bitfield bfield[];
};

struct kvx_pseudo_relocs
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
     Enum values should match the kvx_arch_size var set by -m32
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
  struct kvx_reloc *kreloc;
};

typedef struct symbol symbolS;

struct pseudo_func
{
  const char *name;

  symbolS *sym;
  struct kvx_pseudo_relocs pseudo_relocs;
};

/* Flags for kvx_operand  */
#define KVX_OPERAND_SIGNED    1
#define KVX_OPERAND_CANEXTEND 2
#define KVX_OPERAND_BITMASK   4
#define KVX_OPERAND_WRAPPED   8

#define KVX_OPCODE_FLAG_UNDEF 0
#define KVX_OPCODE_FLAG_IMMX 1
#define KVX_OPCODE_FLAG_COND 2
#define KVX_OPCODE_FLAG_CALL 4
#define KVX_OPCODE_FLAG_LOAD 8
#define KVX_OPCODE_FLAG_STORE 16
#define KVX_OPCODE_FLAG_MODE32 32
#define KVX_OPCODE_FLAG_MODE64 64
#define KVX_OPCODE_FLAG_RISCV 128

/* Opcode definition.  */

struct kvx_codeword {
  /* The opcode.  */
  unsigned opcode;
  /* Disassembly mask.  */
  unsigned mask;
  /* Target dependent flags.  */
  unsigned flags;
};

struct kvx_opc {
  /* asm name */
  const char *as_op;
  /* 32 bits code words. */
  struct kvx_codeword codewords[KVX_MAXSYLLABLES];
  /* Number of words in codewords[].  */
  int wordcount;
  /* Bundling class.  */
  short bundling;
  /* Reservation class.  */
  short reservation;
  /* 0 terminated.  */
  struct kvx_operand *format[KVX_MAXOPERANDS + 1];
  /* Formating string.  */
  const char *fmtstring;
};

struct kvx_core_info {
  struct kvx_opc *optab;
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

struct kvx_register {
  int id;
  const char *name;
};

extern const int kv3_v1_reservation_table_cycles;
extern const int *kv3_v1_reservation_tables[];
extern const char *kv3_v1_resource_names[];

extern const int kv3_v1_resources[];
extern struct kvx_opc kv3_v1_optab[];
extern const struct kvx_core_info kv3_v1_core_info;
extern const int kv3_v2_reservation_table_cycles;
extern const int *kv3_v2_reservation_tables[];
extern const char *kv3_v2_resource_names[];

extern const int kv3_v2_resources[];
extern struct kvx_opc kv3_v2_optab[];
extern const struct kvx_core_info kv3_v2_core_info;
extern const int kv4_v1_reservation_table_cycles;
extern const int *kv4_v1_reservation_tables[];
extern const char *kv4_v1_resource_names[];

extern const int kv4_v1_resources[];
extern struct kvx_opc kv4_v1_optab[];
extern const struct kvx_core_info kv4_v1_core_info;
extern const struct kvx_core_info *kvx_core_info_table[];
extern const char ***kvx_modifiers_table[];
extern const struct kvx_register *kvx_registers_table[];
extern const int *kvx_regfiles_table[];
extern const int kvx_regfiles_size_table[];

#define KVX_REGFILE_FIRST_SFR 0
#define KVX_REGFILE_LAST_SFR 1
#define KVX_REGFILE_DEC_SFR 2
#define KVX_REGFILE_FIRST_GPR 3
#define KVX_REGFILE_LAST_GPR 4
#define KVX_REGFILE_DEC_GPR 5


#define KV3_V1_REGFILE_FIRST_SFR KVX_REGFILE_FIRST_SFR
#define KV3_V1_REGFILE_LAST_SFR KVX_REGFILE_LAST_SFR
#define KV3_V1_REGFILE_DEC_SFR KVX_REGFILE_DEC_SFR
#define KV3_V1_REGFILE_FIRST_GPR KVX_REGFILE_FIRST_GPR
#define KV3_V1_REGFILE_LAST_GPR KVX_REGFILE_LAST_GPR
#define KV3_V1_REGFILE_DEC_GPR KVX_REGFILE_DEC_GPR
#define KV3_V1_REGFILE_FIRST_PGR 6
#define KV3_V1_REGFILE_LAST_PGR 7
#define KV3_V1_REGFILE_DEC_PGR 8
#define KV3_V1_REGFILE_FIRST_QGR 9
#define KV3_V1_REGFILE_LAST_QGR 10
#define KV3_V1_REGFILE_DEC_QGR 11
#define KV3_V1_REGFILE_FIRST_X16R 12
#define KV3_V1_REGFILE_LAST_X16R 13
#define KV3_V1_REGFILE_DEC_X16R 14
#define KV3_V1_REGFILE_FIRST_X2R 15
#define KV3_V1_REGFILE_LAST_X2R 16
#define KV3_V1_REGFILE_DEC_X2R 17
#define KV3_V1_REGFILE_FIRST_X32R 18
#define KV3_V1_REGFILE_LAST_X32R 19
#define KV3_V1_REGFILE_DEC_X32R 20
#define KV3_V1_REGFILE_FIRST_X4R 21
#define KV3_V1_REGFILE_LAST_X4R 22
#define KV3_V1_REGFILE_DEC_X4R 23
#define KV3_V1_REGFILE_FIRST_X64R 24
#define KV3_V1_REGFILE_LAST_X64R 25
#define KV3_V1_REGFILE_DEC_X64R 26
#define KV3_V1_REGFILE_FIRST_X8R 27
#define KV3_V1_REGFILE_LAST_X8R 28
#define KV3_V1_REGFILE_DEC_X8R 29
#define KV3_V1_REGFILE_FIRST_XBR 30
#define KV3_V1_REGFILE_LAST_XBR 31
#define KV3_V1_REGFILE_DEC_XBR 32
#define KV3_V1_REGFILE_FIRST_XCR 33
#define KV3_V1_REGFILE_LAST_XCR 34
#define KV3_V1_REGFILE_DEC_XCR 35
#define KV3_V1_REGFILE_FIRST_XMR 36
#define KV3_V1_REGFILE_LAST_XMR 37
#define KV3_V1_REGFILE_DEC_XMR 38
#define KV3_V1_REGFILE_FIRST_XTR 39
#define KV3_V1_REGFILE_LAST_XTR 40
#define KV3_V1_REGFILE_DEC_XTR 41
#define KV3_V1_REGFILE_FIRST_XVR 42
#define KV3_V1_REGFILE_LAST_XVR 43
#define KV3_V1_REGFILE_DEC_XVR 44
#define KV3_V1_REGFILE_REGISTERS 45
#define KV3_V1_REGFILE_DEC_REGISTERS 46


extern int kv3_v1_regfiles[];
extern const char **kv3_v1_modifiers[];
extern struct kvx_register kv3_v1_registers[];

extern int kv3_v1_dec_registers[];

enum Method_kv3_v1_enum {
  Immediate_kv3_v1_pcrel17 = 1,
  Immediate_kv3_v1_pcrel27 = 2,
  Immediate_kv3_v1_signed10 = 3,
  Immediate_kv3_v1_signed16 = 4,
  Immediate_kv3_v1_signed27 = 5,
  Immediate_kv3_v1_signed37 = 6,
  Immediate_kv3_v1_signed43 = 7,
  Immediate_kv3_v1_signed54 = 8,
  Immediate_kv3_v1_sysnumber = 9,
  Immediate_kv3_v1_unsigned6 = 10,
  Immediate_kv3_v1_wrapped32 = 11,
  Immediate_kv3_v1_wrapped64 = 12,
  Modifier_kv3_v1_column = 13,
  Modifier_kv3_v1_comparison = 14,
  Modifier_kv3_v1_doscale = 15,
  Modifier_kv3_v1_exunum = 16,
  Modifier_kv3_v1_floatcomp = 17,
  Modifier_kv3_v1_qindex = 18,
  Modifier_kv3_v1_rectify = 19,
  Modifier_kv3_v1_rounding = 20,
  Modifier_kv3_v1_roundint = 21,
  Modifier_kv3_v1_saturate = 22,
  Modifier_kv3_v1_scalarcond = 23,
  Modifier_kv3_v1_silent = 24,
  Modifier_kv3_v1_simdcond = 25,
  Modifier_kv3_v1_speculate = 26,
  Modifier_kv3_v1_splat32 = 27,
  Modifier_kv3_v1_variant = 28,
  RegClass_kv3_v1_aloneReg = 29,
  RegClass_kv3_v1_buffer16Reg = 30,
  RegClass_kv3_v1_buffer2Reg = 31,
  RegClass_kv3_v1_buffer32Reg = 32,
  RegClass_kv3_v1_buffer4Reg = 33,
  RegClass_kv3_v1_buffer64Reg = 34,
  RegClass_kv3_v1_buffer8Reg = 35,
  RegClass_kv3_v1_onlyfxReg = 36,
  RegClass_kv3_v1_onlygetReg = 37,
  RegClass_kv3_v1_onlyraReg = 38,
  RegClass_kv3_v1_onlysetReg = 39,
  RegClass_kv3_v1_onlyswapReg = 40,
  RegClass_kv3_v1_pairedReg = 41,
  RegClass_kv3_v1_quadReg = 42,
  RegClass_kv3_v1_singleReg = 43,
  RegClass_kv3_v1_systemReg = 44,
  RegClass_kv3_v1_xworddReg = 45,
  RegClass_kv3_v1_xworddReg0M4 = 46,
  RegClass_kv3_v1_xworddReg1M4 = 47,
  RegClass_kv3_v1_xworddReg2M4 = 48,
  RegClass_kv3_v1_xworddReg3M4 = 49,
  RegClass_kv3_v1_xwordoReg = 50,
  RegClass_kv3_v1_xwordoRegE = 51,
  RegClass_kv3_v1_xwordoRegO = 52,
  RegClass_kv3_v1_xwordqReg = 53,
  RegClass_kv3_v1_xwordqReg0M4 = 54,
  RegClass_kv3_v1_xwordqReg1M4 = 55,
  RegClass_kv3_v1_xwordqReg2M4 = 56,
  RegClass_kv3_v1_xwordqReg3M4 = 57,
  RegClass_kv3_v1_xwordqRegE = 58,
  RegClass_kv3_v1_xwordqRegO = 59,
  RegClass_kv3_v1_xwordvReg = 60,
  RegClass_kv3_v1_xwordxReg = 61,
  Instruction_kv3_v1_abdd = 62,
  Instruction_kv3_v1_abdhq = 63,
  Instruction_kv3_v1_abdw = 64,
  Instruction_kv3_v1_abdwp = 65,
  Instruction_kv3_v1_absd = 66,
  Instruction_kv3_v1_abshq = 67,
  Instruction_kv3_v1_absw = 68,
  Instruction_kv3_v1_abswp = 69,
  Instruction_kv3_v1_acswapd = 70,
  Instruction_kv3_v1_acswapw = 71,
  Instruction_kv3_v1_addcd = 72,
  Instruction_kv3_v1_addcd_i = 73,
  Instruction_kv3_v1_addd = 74,
  Instruction_kv3_v1_addhcp_c = 75,
  Instruction_kv3_v1_addhq = 76,
  Instruction_kv3_v1_addsd = 77,
  Instruction_kv3_v1_addshq = 78,
  Instruction_kv3_v1_addsw = 79,
  Instruction_kv3_v1_addswp = 80,
  Instruction_kv3_v1_adduwd = 81,
  Instruction_kv3_v1_addw = 82,
  Instruction_kv3_v1_addwc_c = 83,
  Instruction_kv3_v1_addwd = 84,
  Instruction_kv3_v1_addwp = 85,
  Instruction_kv3_v1_addx16d = 86,
  Instruction_kv3_v1_addx16hq = 87,
  Instruction_kv3_v1_addx16uwd = 88,
  Instruction_kv3_v1_addx16w = 89,
  Instruction_kv3_v1_addx16wd = 90,
  Instruction_kv3_v1_addx16wp = 91,
  Instruction_kv3_v1_addx2d = 92,
  Instruction_kv3_v1_addx2hq = 93,
  Instruction_kv3_v1_addx2uwd = 94,
  Instruction_kv3_v1_addx2w = 95,
  Instruction_kv3_v1_addx2wd = 96,
  Instruction_kv3_v1_addx2wp = 97,
  Instruction_kv3_v1_addx4d = 98,
  Instruction_kv3_v1_addx4hq = 99,
  Instruction_kv3_v1_addx4uwd = 100,
  Instruction_kv3_v1_addx4w = 101,
  Instruction_kv3_v1_addx4wd = 102,
  Instruction_kv3_v1_addx4wp = 103,
  Instruction_kv3_v1_addx8d = 104,
  Instruction_kv3_v1_addx8hq = 105,
  Instruction_kv3_v1_addx8uwd = 106,
  Instruction_kv3_v1_addx8w = 107,
  Instruction_kv3_v1_addx8wd = 108,
  Instruction_kv3_v1_addx8wp = 109,
  Instruction_kv3_v1_aladdd = 110,
  Instruction_kv3_v1_aladdw = 111,
  Instruction_kv3_v1_alclrd = 112,
  Instruction_kv3_v1_alclrw = 113,
  Instruction_kv3_v1_aligno = 114,
  Instruction_kv3_v1_alignv = 115,
  Instruction_kv3_v1_andd = 116,
  Instruction_kv3_v1_andnd = 117,
  Instruction_kv3_v1_andnw = 118,
  Instruction_kv3_v1_andw = 119,
  Instruction_kv3_v1_avghq = 120,
  Instruction_kv3_v1_avgrhq = 121,
  Instruction_kv3_v1_avgruhq = 122,
  Instruction_kv3_v1_avgruw = 123,
  Instruction_kv3_v1_avgruwp = 124,
  Instruction_kv3_v1_avgrw = 125,
  Instruction_kv3_v1_avgrwp = 126,
  Instruction_kv3_v1_avguhq = 127,
  Instruction_kv3_v1_avguw = 128,
  Instruction_kv3_v1_avguwp = 129,
  Instruction_kv3_v1_avgw = 130,
  Instruction_kv3_v1_avgwp = 131,
  Instruction_kv3_v1_await = 132,
  Instruction_kv3_v1_barrier = 133,
  Instruction_kv3_v1_call = 134,
  Instruction_kv3_v1_cb = 135,
  Instruction_kv3_v1_cbsd = 136,
  Instruction_kv3_v1_cbsw = 137,
  Instruction_kv3_v1_cbswp = 138,
  Instruction_kv3_v1_clrf = 139,
  Instruction_kv3_v1_clsd = 140,
  Instruction_kv3_v1_clsw = 141,
  Instruction_kv3_v1_clswp = 142,
  Instruction_kv3_v1_clzd = 143,
  Instruction_kv3_v1_clzw = 144,
  Instruction_kv3_v1_clzwp = 145,
  Instruction_kv3_v1_cmoved = 146,
  Instruction_kv3_v1_cmovehq = 147,
  Instruction_kv3_v1_cmovewp = 148,
  Instruction_kv3_v1_cmuldt = 149,
  Instruction_kv3_v1_cmulghxdt = 150,
  Instruction_kv3_v1_cmulglxdt = 151,
  Instruction_kv3_v1_cmulgmxdt = 152,
  Instruction_kv3_v1_cmulxdt = 153,
  Instruction_kv3_v1_compd = 154,
  Instruction_kv3_v1_compnhq = 155,
  Instruction_kv3_v1_compnwp = 156,
  Instruction_kv3_v1_compuwd = 157,
  Instruction_kv3_v1_compw = 158,
  Instruction_kv3_v1_compwd = 159,
  Instruction_kv3_v1_convdhv0 = 160,
  Instruction_kv3_v1_convdhv1 = 161,
  Instruction_kv3_v1_convwbv0 = 162,
  Instruction_kv3_v1_convwbv1 = 163,
  Instruction_kv3_v1_convwbv2 = 164,
  Instruction_kv3_v1_convwbv3 = 165,
  Instruction_kv3_v1_copyd = 166,
  Instruction_kv3_v1_copyo = 167,
  Instruction_kv3_v1_copyq = 168,
  Instruction_kv3_v1_copyw = 169,
  Instruction_kv3_v1_crcbellw = 170,
  Instruction_kv3_v1_crcbelmw = 171,
  Instruction_kv3_v1_crclellw = 172,
  Instruction_kv3_v1_crclelmw = 173,
  Instruction_kv3_v1_ctzd = 174,
  Instruction_kv3_v1_ctzw = 175,
  Instruction_kv3_v1_ctzwp = 176,
  Instruction_kv3_v1_d1inval = 177,
  Instruction_kv3_v1_dinvall = 178,
  Instruction_kv3_v1_dot2suwd = 179,
  Instruction_kv3_v1_dot2suwdp = 180,
  Instruction_kv3_v1_dot2uwd = 181,
  Instruction_kv3_v1_dot2uwdp = 182,
  Instruction_kv3_v1_dot2w = 183,
  Instruction_kv3_v1_dot2wd = 184,
  Instruction_kv3_v1_dot2wdp = 185,
  Instruction_kv3_v1_dot2wzp = 186,
  Instruction_kv3_v1_dtouchl = 187,
  Instruction_kv3_v1_dzerol = 188,
  Instruction_kv3_v1_eord = 189,
  Instruction_kv3_v1_eorw = 190,
  Instruction_kv3_v1_errop = 191,
  Instruction_kv3_v1_extfs = 192,
  Instruction_kv3_v1_extfz = 193,
  Instruction_kv3_v1_fabsd = 194,
  Instruction_kv3_v1_fabshq = 195,
  Instruction_kv3_v1_fabsw = 196,
  Instruction_kv3_v1_fabswp = 197,
  Instruction_kv3_v1_faddd = 198,
  Instruction_kv3_v1_fadddc = 199,
  Instruction_kv3_v1_fadddc_c = 200,
  Instruction_kv3_v1_fadddp = 201,
  Instruction_kv3_v1_faddhq = 202,
  Instruction_kv3_v1_faddw = 203,
  Instruction_kv3_v1_faddwc = 204,
  Instruction_kv3_v1_faddwc_c = 205,
  Instruction_kv3_v1_faddwcp = 206,
  Instruction_kv3_v1_faddwcp_c = 207,
  Instruction_kv3_v1_faddwp = 208,
  Instruction_kv3_v1_faddwq = 209,
  Instruction_kv3_v1_fcdivd = 210,
  Instruction_kv3_v1_fcdivw = 211,
  Instruction_kv3_v1_fcdivwp = 212,
  Instruction_kv3_v1_fcompd = 213,
  Instruction_kv3_v1_fcompnhq = 214,
  Instruction_kv3_v1_fcompnwp = 215,
  Instruction_kv3_v1_fcompw = 216,
  Instruction_kv3_v1_fdot2w = 217,
  Instruction_kv3_v1_fdot2wd = 218,
  Instruction_kv3_v1_fdot2wdp = 219,
  Instruction_kv3_v1_fdot2wzp = 220,
  Instruction_kv3_v1_fence = 221,
  Instruction_kv3_v1_ffmad = 222,
  Instruction_kv3_v1_ffmahq = 223,
  Instruction_kv3_v1_ffmahw = 224,
  Instruction_kv3_v1_ffmahwq = 225,
  Instruction_kv3_v1_ffmaw = 226,
  Instruction_kv3_v1_ffmawd = 227,
  Instruction_kv3_v1_ffmawdp = 228,
  Instruction_kv3_v1_ffmawp = 229,
  Instruction_kv3_v1_ffmsd = 230,
  Instruction_kv3_v1_ffmshq = 231,
  Instruction_kv3_v1_ffmshw = 232,
  Instruction_kv3_v1_ffmshwq = 233,
  Instruction_kv3_v1_ffmsw = 234,
  Instruction_kv3_v1_ffmswd = 235,
  Instruction_kv3_v1_ffmswdp = 236,
  Instruction_kv3_v1_ffmswp = 237,
  Instruction_kv3_v1_fixedd = 238,
  Instruction_kv3_v1_fixedud = 239,
  Instruction_kv3_v1_fixeduw = 240,
  Instruction_kv3_v1_fixeduwp = 241,
  Instruction_kv3_v1_fixedw = 242,
  Instruction_kv3_v1_fixedwp = 243,
  Instruction_kv3_v1_floatd = 244,
  Instruction_kv3_v1_floatud = 245,
  Instruction_kv3_v1_floatuw = 246,
  Instruction_kv3_v1_floatuwp = 247,
  Instruction_kv3_v1_floatw = 248,
  Instruction_kv3_v1_floatwp = 249,
  Instruction_kv3_v1_fmaxd = 250,
  Instruction_kv3_v1_fmaxhq = 251,
  Instruction_kv3_v1_fmaxw = 252,
  Instruction_kv3_v1_fmaxwp = 253,
  Instruction_kv3_v1_fmind = 254,
  Instruction_kv3_v1_fminhq = 255,
  Instruction_kv3_v1_fminw = 256,
  Instruction_kv3_v1_fminwp = 257,
  Instruction_kv3_v1_fmm212w = 258,
  Instruction_kv3_v1_fmma212w = 259,
  Instruction_kv3_v1_fmma242hw0 = 260,
  Instruction_kv3_v1_fmma242hw1 = 261,
  Instruction_kv3_v1_fmma242hw2 = 262,
  Instruction_kv3_v1_fmma242hw3 = 263,
  Instruction_kv3_v1_fmms212w = 264,
  Instruction_kv3_v1_fmuld = 265,
  Instruction_kv3_v1_fmulhq = 266,
  Instruction_kv3_v1_fmulhw = 267,
  Instruction_kv3_v1_fmulhwq = 268,
  Instruction_kv3_v1_fmulw = 269,
  Instruction_kv3_v1_fmulwc = 270,
  Instruction_kv3_v1_fmulwc_c = 271,
  Instruction_kv3_v1_fmulwd = 272,
  Instruction_kv3_v1_fmulwdc = 273,
  Instruction_kv3_v1_fmulwdc_c = 274,
  Instruction_kv3_v1_fmulwdp = 275,
  Instruction_kv3_v1_fmulwp = 276,
  Instruction_kv3_v1_fmulwq = 277,
  Instruction_kv3_v1_fnarrow44wh = 278,
  Instruction_kv3_v1_fnarrowdw = 279,
  Instruction_kv3_v1_fnarrowdwp = 280,
  Instruction_kv3_v1_fnarrowwh = 281,
  Instruction_kv3_v1_fnarrowwhq = 282,
  Instruction_kv3_v1_fnegd = 283,
  Instruction_kv3_v1_fneghq = 284,
  Instruction_kv3_v1_fnegw = 285,
  Instruction_kv3_v1_fnegwp = 286,
  Instruction_kv3_v1_frecw = 287,
  Instruction_kv3_v1_frsrw = 288,
  Instruction_kv3_v1_fsbfd = 289,
  Instruction_kv3_v1_fsbfdc = 290,
  Instruction_kv3_v1_fsbfdc_c = 291,
  Instruction_kv3_v1_fsbfdp = 292,
  Instruction_kv3_v1_fsbfhq = 293,
  Instruction_kv3_v1_fsbfw = 294,
  Instruction_kv3_v1_fsbfwc = 295,
  Instruction_kv3_v1_fsbfwc_c = 296,
  Instruction_kv3_v1_fsbfwcp = 297,
  Instruction_kv3_v1_fsbfwcp_c = 298,
  Instruction_kv3_v1_fsbfwp = 299,
  Instruction_kv3_v1_fsbfwq = 300,
  Instruction_kv3_v1_fscalewv = 301,
  Instruction_kv3_v1_fsdivd = 302,
  Instruction_kv3_v1_fsdivw = 303,
  Instruction_kv3_v1_fsdivwp = 304,
  Instruction_kv3_v1_fsrecd = 305,
  Instruction_kv3_v1_fsrecw = 306,
  Instruction_kv3_v1_fsrecwp = 307,
  Instruction_kv3_v1_fsrsrd = 308,
  Instruction_kv3_v1_fsrsrw = 309,
  Instruction_kv3_v1_fsrsrwp = 310,
  Instruction_kv3_v1_fwidenlhw = 311,
  Instruction_kv3_v1_fwidenlhwp = 312,
  Instruction_kv3_v1_fwidenlwd = 313,
  Instruction_kv3_v1_fwidenmhw = 314,
  Instruction_kv3_v1_fwidenmhwp = 315,
  Instruction_kv3_v1_fwidenmwd = 316,
  Instruction_kv3_v1_get = 317,
  Instruction_kv3_v1_goto = 318,
  Instruction_kv3_v1_i1inval = 319,
  Instruction_kv3_v1_i1invals = 320,
  Instruction_kv3_v1_icall = 321,
  Instruction_kv3_v1_iget = 322,
  Instruction_kv3_v1_igoto = 323,
  Instruction_kv3_v1_insf = 324,
  Instruction_kv3_v1_iord = 325,
  Instruction_kv3_v1_iornd = 326,
  Instruction_kv3_v1_iornw = 327,
  Instruction_kv3_v1_iorw = 328,
  Instruction_kv3_v1_landd = 329,
  Instruction_kv3_v1_landhq = 330,
  Instruction_kv3_v1_landw = 331,
  Instruction_kv3_v1_landwp = 332,
  Instruction_kv3_v1_lbs = 333,
  Instruction_kv3_v1_lbz = 334,
  Instruction_kv3_v1_ld = 335,
  Instruction_kv3_v1_lhs = 336,
  Instruction_kv3_v1_lhz = 337,
  Instruction_kv3_v1_liord = 338,
  Instruction_kv3_v1_liorhq = 339,
  Instruction_kv3_v1_liorw = 340,
  Instruction_kv3_v1_liorwp = 341,
  Instruction_kv3_v1_lnandd = 342,
  Instruction_kv3_v1_lnandhq = 343,
  Instruction_kv3_v1_lnandw = 344,
  Instruction_kv3_v1_lnandwp = 345,
  Instruction_kv3_v1_lniord = 346,
  Instruction_kv3_v1_lniorhq = 347,
  Instruction_kv3_v1_lniorw = 348,
  Instruction_kv3_v1_lniorwp = 349,
  Instruction_kv3_v1_lnord = 350,
  Instruction_kv3_v1_lnorhq = 351,
  Instruction_kv3_v1_lnorw = 352,
  Instruction_kv3_v1_lnorwp = 353,
  Instruction_kv3_v1_lo = 354,
  Instruction_kv3_v1_loopdo = 355,
  Instruction_kv3_v1_lord = 356,
  Instruction_kv3_v1_lorhq = 357,
  Instruction_kv3_v1_lorw = 358,
  Instruction_kv3_v1_lorwp = 359,
  Instruction_kv3_v1_lq = 360,
  Instruction_kv3_v1_lws = 361,
  Instruction_kv3_v1_lwz = 362,
  Instruction_kv3_v1_maddd = 363,
  Instruction_kv3_v1_madddt = 364,
  Instruction_kv3_v1_maddhq = 365,
  Instruction_kv3_v1_maddhwq = 366,
  Instruction_kv3_v1_maddsudt = 367,
  Instruction_kv3_v1_maddsuhwq = 368,
  Instruction_kv3_v1_maddsuwd = 369,
  Instruction_kv3_v1_maddsuwdp = 370,
  Instruction_kv3_v1_maddudt = 371,
  Instruction_kv3_v1_madduhwq = 372,
  Instruction_kv3_v1_madduwd = 373,
  Instruction_kv3_v1_madduwdp = 374,
  Instruction_kv3_v1_madduzdt = 375,
  Instruction_kv3_v1_maddw = 376,
  Instruction_kv3_v1_maddwd = 377,
  Instruction_kv3_v1_maddwdp = 378,
  Instruction_kv3_v1_maddwp = 379,
  Instruction_kv3_v1_make = 380,
  Instruction_kv3_v1_maxd = 381,
  Instruction_kv3_v1_maxhq = 382,
  Instruction_kv3_v1_maxud = 383,
  Instruction_kv3_v1_maxuhq = 384,
  Instruction_kv3_v1_maxuw = 385,
  Instruction_kv3_v1_maxuwp = 386,
  Instruction_kv3_v1_maxw = 387,
  Instruction_kv3_v1_maxwp = 388,
  Instruction_kv3_v1_mind = 389,
  Instruction_kv3_v1_minhq = 390,
  Instruction_kv3_v1_minud = 391,
  Instruction_kv3_v1_minuhq = 392,
  Instruction_kv3_v1_minuw = 393,
  Instruction_kv3_v1_minuwp = 394,
  Instruction_kv3_v1_minw = 395,
  Instruction_kv3_v1_minwp = 396,
  Instruction_kv3_v1_mm212w = 397,
  Instruction_kv3_v1_mma212w = 398,
  Instruction_kv3_v1_mma444hbd0 = 399,
  Instruction_kv3_v1_mma444hbd1 = 400,
  Instruction_kv3_v1_mma444hd = 401,
  Instruction_kv3_v1_mma444suhbd0 = 402,
  Instruction_kv3_v1_mma444suhbd1 = 403,
  Instruction_kv3_v1_mma444suhd = 404,
  Instruction_kv3_v1_mma444uhbd0 = 405,
  Instruction_kv3_v1_mma444uhbd1 = 406,
  Instruction_kv3_v1_mma444uhd = 407,
  Instruction_kv3_v1_mma444ushbd0 = 408,
  Instruction_kv3_v1_mma444ushbd1 = 409,
  Instruction_kv3_v1_mma444ushd = 410,
  Instruction_kv3_v1_mms212w = 411,
  Instruction_kv3_v1_movetq = 412,
  Instruction_kv3_v1_msbfd = 413,
  Instruction_kv3_v1_msbfdt = 414,
  Instruction_kv3_v1_msbfhq = 415,
  Instruction_kv3_v1_msbfhwq = 416,
  Instruction_kv3_v1_msbfsudt = 417,
  Instruction_kv3_v1_msbfsuhwq = 418,
  Instruction_kv3_v1_msbfsuwd = 419,
  Instruction_kv3_v1_msbfsuwdp = 420,
  Instruction_kv3_v1_msbfudt = 421,
  Instruction_kv3_v1_msbfuhwq = 422,
  Instruction_kv3_v1_msbfuwd = 423,
  Instruction_kv3_v1_msbfuwdp = 424,
  Instruction_kv3_v1_msbfuzdt = 425,
  Instruction_kv3_v1_msbfw = 426,
  Instruction_kv3_v1_msbfwd = 427,
  Instruction_kv3_v1_msbfwdp = 428,
  Instruction_kv3_v1_msbfwp = 429,
  Instruction_kv3_v1_muld = 430,
  Instruction_kv3_v1_muldt = 431,
  Instruction_kv3_v1_mulhq = 432,
  Instruction_kv3_v1_mulhwq = 433,
  Instruction_kv3_v1_mulsudt = 434,
  Instruction_kv3_v1_mulsuhwq = 435,
  Instruction_kv3_v1_mulsuwd = 436,
  Instruction_kv3_v1_mulsuwdp = 437,
  Instruction_kv3_v1_muludt = 438,
  Instruction_kv3_v1_muluhwq = 439,
  Instruction_kv3_v1_muluwd = 440,
  Instruction_kv3_v1_muluwdp = 441,
  Instruction_kv3_v1_mulw = 442,
  Instruction_kv3_v1_mulwc = 443,
  Instruction_kv3_v1_mulwc_c = 444,
  Instruction_kv3_v1_mulwd = 445,
  Instruction_kv3_v1_mulwdc = 446,
  Instruction_kv3_v1_mulwdc_c = 447,
  Instruction_kv3_v1_mulwdp = 448,
  Instruction_kv3_v1_mulwp = 449,
  Instruction_kv3_v1_mulwq = 450,
  Instruction_kv3_v1_nandd = 451,
  Instruction_kv3_v1_nandw = 452,
  Instruction_kv3_v1_negd = 453,
  Instruction_kv3_v1_neghq = 454,
  Instruction_kv3_v1_negw = 455,
  Instruction_kv3_v1_negwp = 456,
  Instruction_kv3_v1_neord = 457,
  Instruction_kv3_v1_neorw = 458,
  Instruction_kv3_v1_niord = 459,
  Instruction_kv3_v1_niorw = 460,
  Instruction_kv3_v1_nop = 461,
  Instruction_kv3_v1_nord = 462,
  Instruction_kv3_v1_norw = 463,
  Instruction_kv3_v1_notd = 464,
  Instruction_kv3_v1_notw = 465,
  Instruction_kv3_v1_nxord = 466,
  Instruction_kv3_v1_nxorw = 467,
  Instruction_kv3_v1_ord = 468,
  Instruction_kv3_v1_ornd = 469,
  Instruction_kv3_v1_ornw = 470,
  Instruction_kv3_v1_orw = 471,
  Instruction_kv3_v1_pcrel = 472,
  Instruction_kv3_v1_ret = 473,
  Instruction_kv3_v1_rfe = 474,
  Instruction_kv3_v1_rolw = 475,
  Instruction_kv3_v1_rolwps = 476,
  Instruction_kv3_v1_rorw = 477,
  Instruction_kv3_v1_rorwps = 478,
  Instruction_kv3_v1_rswap = 479,
  Instruction_kv3_v1_satd = 480,
  Instruction_kv3_v1_satdh = 481,
  Instruction_kv3_v1_satdw = 482,
  Instruction_kv3_v1_sb = 483,
  Instruction_kv3_v1_sbfcd = 484,
  Instruction_kv3_v1_sbfcd_i = 485,
  Instruction_kv3_v1_sbfd = 486,
  Instruction_kv3_v1_sbfhcp_c = 487,
  Instruction_kv3_v1_sbfhq = 488,
  Instruction_kv3_v1_sbfsd = 489,
  Instruction_kv3_v1_sbfshq = 490,
  Instruction_kv3_v1_sbfsw = 491,
  Instruction_kv3_v1_sbfswp = 492,
  Instruction_kv3_v1_sbfuwd = 493,
  Instruction_kv3_v1_sbfw = 494,
  Instruction_kv3_v1_sbfwc_c = 495,
  Instruction_kv3_v1_sbfwd = 496,
  Instruction_kv3_v1_sbfwp = 497,
  Instruction_kv3_v1_sbfx16d = 498,
  Instruction_kv3_v1_sbfx16hq = 499,
  Instruction_kv3_v1_sbfx16uwd = 500,
  Instruction_kv3_v1_sbfx16w = 501,
  Instruction_kv3_v1_sbfx16wd = 502,
  Instruction_kv3_v1_sbfx16wp = 503,
  Instruction_kv3_v1_sbfx2d = 504,
  Instruction_kv3_v1_sbfx2hq = 505,
  Instruction_kv3_v1_sbfx2uwd = 506,
  Instruction_kv3_v1_sbfx2w = 507,
  Instruction_kv3_v1_sbfx2wd = 508,
  Instruction_kv3_v1_sbfx2wp = 509,
  Instruction_kv3_v1_sbfx4d = 510,
  Instruction_kv3_v1_sbfx4hq = 511,
  Instruction_kv3_v1_sbfx4uwd = 512,
  Instruction_kv3_v1_sbfx4w = 513,
  Instruction_kv3_v1_sbfx4wd = 514,
  Instruction_kv3_v1_sbfx4wp = 515,
  Instruction_kv3_v1_sbfx8d = 516,
  Instruction_kv3_v1_sbfx8hq = 517,
  Instruction_kv3_v1_sbfx8uwd = 518,
  Instruction_kv3_v1_sbfx8w = 519,
  Instruction_kv3_v1_sbfx8wd = 520,
  Instruction_kv3_v1_sbfx8wp = 521,
  Instruction_kv3_v1_sbmm8 = 522,
  Instruction_kv3_v1_sbmm8d = 523,
  Instruction_kv3_v1_sbmmt8 = 524,
  Instruction_kv3_v1_sbmmt8d = 525,
  Instruction_kv3_v1_scall = 526,
  Instruction_kv3_v1_sd = 527,
  Instruction_kv3_v1_set = 528,
  Instruction_kv3_v1_sh = 529,
  Instruction_kv3_v1_sleep = 530,
  Instruction_kv3_v1_slld = 531,
  Instruction_kv3_v1_sllhqs = 532,
  Instruction_kv3_v1_sllw = 533,
  Instruction_kv3_v1_sllwps = 534,
  Instruction_kv3_v1_slsd = 535,
  Instruction_kv3_v1_slshqs = 536,
  Instruction_kv3_v1_slsw = 537,
  Instruction_kv3_v1_slswps = 538,
  Instruction_kv3_v1_so = 539,
  Instruction_kv3_v1_sq = 540,
  Instruction_kv3_v1_srad = 541,
  Instruction_kv3_v1_srahqs = 542,
  Instruction_kv3_v1_sraw = 543,
  Instruction_kv3_v1_srawps = 544,
  Instruction_kv3_v1_srld = 545,
  Instruction_kv3_v1_srlhqs = 546,
  Instruction_kv3_v1_srlw = 547,
  Instruction_kv3_v1_srlwps = 548,
  Instruction_kv3_v1_srsd = 549,
  Instruction_kv3_v1_srshqs = 550,
  Instruction_kv3_v1_srsw = 551,
  Instruction_kv3_v1_srswps = 552,
  Instruction_kv3_v1_stop = 553,
  Instruction_kv3_v1_stsud = 554,
  Instruction_kv3_v1_stsuw = 555,
  Instruction_kv3_v1_sw = 556,
  Instruction_kv3_v1_sxbd = 557,
  Instruction_kv3_v1_sxhd = 558,
  Instruction_kv3_v1_sxlbhq = 559,
  Instruction_kv3_v1_sxlhwp = 560,
  Instruction_kv3_v1_sxmbhq = 561,
  Instruction_kv3_v1_sxmhwp = 562,
  Instruction_kv3_v1_sxwd = 563,
  Instruction_kv3_v1_syncgroup = 564,
  Instruction_kv3_v1_tlbdinval = 565,
  Instruction_kv3_v1_tlbiinval = 566,
  Instruction_kv3_v1_tlbprobe = 567,
  Instruction_kv3_v1_tlbread = 568,
  Instruction_kv3_v1_tlbwrite = 569,
  Instruction_kv3_v1_waitit = 570,
  Instruction_kv3_v1_wfxl = 571,
  Instruction_kv3_v1_wfxm = 572,
  Instruction_kv3_v1_xcopyo = 573,
  Instruction_kv3_v1_xlo = 574,
  Instruction_kv3_v1_xmma484bw = 575,
  Instruction_kv3_v1_xmma484subw = 576,
  Instruction_kv3_v1_xmma484ubw = 577,
  Instruction_kv3_v1_xmma484usbw = 578,
  Instruction_kv3_v1_xmovefo = 579,
  Instruction_kv3_v1_xmovetq = 580,
  Instruction_kv3_v1_xmt44d = 581,
  Instruction_kv3_v1_xord = 582,
  Instruction_kv3_v1_xorw = 583,
  Instruction_kv3_v1_xso = 584,
  Instruction_kv3_v1_zxbd = 585,
  Instruction_kv3_v1_zxhd = 586,
  Instruction_kv3_v1_zxwd = 587,
  Separator_kv3_v1_comma = 588,
  Separator_kv3_v1_equal = 589,
  Separator_kv3_v1_qmark = 590,
  Separator_kv3_v1_rsbracket = 591,
  Separator_kv3_v1_lsbracket = 592
};

typedef enum {
  Modifier_kv3_v1_exunum_ALU0=0,
  Modifier_kv3_v1_exunum_ALU1=1,
  Modifier_kv3_v1_exunum_MAU=2,
  Modifier_kv3_v1_exunum_LSU=3,
} Modifier_kv3_v1_exunum_values;


extern const char *mod_kv3_v1_exunum[];
extern const char *mod_kv3_v1_scalarcond[];
extern const char *mod_kv3_v1_simdcond[];
extern const char *mod_kv3_v1_comparison[];
extern const char *mod_kv3_v1_floatcomp[];
extern const char *mod_kv3_v1_rounding[];
extern const char *mod_kv3_v1_silent[];
extern const char *mod_kv3_v1_roundint[];
extern const char *mod_kv3_v1_saturate[];
extern const char *mod_kv3_v1_rectify[];
extern const char *mod_kv3_v1_variant[];
extern const char *mod_kv3_v1_speculate[];
extern const char *mod_kv3_v1_column[];
extern const char *mod_kv3_v1_doscale[];
extern const char *mod_kv3_v1_qindex[];
extern const char *mod_kv3_v1_splat32[];

typedef enum {
  Bundling_kv3_v1_ALL,
  Bundling_kv3_v1_BCU,
  Bundling_kv3_v1_EXT,
  Bundling_kv3_v1_FULL,
  Bundling_kv3_v1_FULL_X,
  Bundling_kv3_v1_FULL_Y,
  Bundling_kv3_v1_LITE,
  Bundling_kv3_v1_LITE_X,
  Bundling_kv3_v1_LITE_Y,
  Bundling_kv3_v1_MAU,
  Bundling_kv3_v1_MAU_X,
  Bundling_kv3_v1_MAU_Y,
  Bundling_kv3_v1_LSU,
  Bundling_kv3_v1_LSU_X,
  Bundling_kv3_v1_LSU_Y,
  Bundling_kv3_v1_TINY,
  Bundling_kv3_v1_TINY_X,
  Bundling_kv3_v1_TINY_Y,
  Bundling_kv3_v1_NOP,
} Bundling_kv3_v1;

static int ATTRIBUTE_UNUSED
kv3_v1_base_bundling(int bundling) {
  static int base_bundlings[] = {
    Bundling_kv3_v1_ALL,	// Bundling_kv3_v1_ALL
    Bundling_kv3_v1_BCU,	// Bundling_kv3_v1_BCU
    Bundling_kv3_v1_EXT,	// Bundling_kv3_v1_EXT
    Bundling_kv3_v1_FULL,	// Bundling_kv3_v1_FULL
    Bundling_kv3_v1_FULL,	// Bundling_kv3_v1_FULL_X
    Bundling_kv3_v1_FULL,	// Bundling_kv3_v1_FULL_Y
    Bundling_kv3_v1_LITE,	// Bundling_kv3_v1_LITE
    Bundling_kv3_v1_LITE,	// Bundling_kv3_v1_LITE_X
    Bundling_kv3_v1_LITE,	// Bundling_kv3_v1_LITE_Y
    Bundling_kv3_v1_MAU,	// Bundling_kv3_v1_MAU
    Bundling_kv3_v1_MAU,	// Bundling_kv3_v1_MAU_X
    Bundling_kv3_v1_MAU,	// Bundling_kv3_v1_MAU_Y
    Bundling_kv3_v1_LSU,	// Bundling_kv3_v1_LSU
    Bundling_kv3_v1_LSU,	// Bundling_kv3_v1_LSU_X
    Bundling_kv3_v1_LSU,	// Bundling_kv3_v1_LSU_Y
    Bundling_kv3_v1_TINY,	// Bundling_kv3_v1_TINY
    Bundling_kv3_v1_TINY,	// Bundling_kv3_v1_TINY_X
    Bundling_kv3_v1_TINY,	// Bundling_kv3_v1_TINY_Y
    Bundling_kv3_v1_NOP,	// Bundling_kv3_v1_NOP
  };
  return base_bundlings[bundling];
};

typedef enum {
  Resource_kv3_v1_ISSUE,
  Resource_kv3_v1_TINY,
  Resource_kv3_v1_LITE,
  Resource_kv3_v1_FULL,
  Resource_kv3_v1_LSU,
  Resource_kv3_v1_MAU,
  Resource_kv3_v1_BCU,
  Resource_kv3_v1_EXT,
  Resource_kv3_v1_AUXR,
  Resource_kv3_v1_AUXW,
  Resource_kv3_v1_XFER,
  Resource_kv3_v1_MEMW,
  Resource_kv3_v1_SR12,
  Resource_kv3_v1_SR13,
  Resource_kv3_v1_SR14,
  Resource_kv3_v1_SR15,
} Resource_kv3_v1;
#define kv3_v1_RESOURCE_COUNT 16

typedef enum {
  Reservation_kv3_v1_ALL,
  Reservation_kv3_v1_ALU_TINY,
  Reservation_kv3_v1_ALU_TINY_X,
  Reservation_kv3_v1_ALU_TINY_Y,
  Reservation_kv3_v1_ALU_TINY_CRRP,
  Reservation_kv3_v1_ALU_TINY_CRWL_CRWH,
  Reservation_kv3_v1_ALU_TINY_CRWL_CRWH_X,
  Reservation_kv3_v1_ALU_TINY_CRWL_CRWH_Y,
  Reservation_kv3_v1_ALU_TINY_CRRP_CRWL_CRWH,
  Reservation_kv3_v1_ALU_TINY_CRWL,
  Reservation_kv3_v1_ALU_TINY_CRWH,
  Reservation_kv3_v1_ALU_NOP,
  Reservation_kv3_v1_ALU_LITE,
  Reservation_kv3_v1_ALU_LITE_X,
  Reservation_kv3_v1_ALU_LITE_Y,
  Reservation_kv3_v1_ALU_LITE_CRWL,
  Reservation_kv3_v1_ALU_LITE_CRWH,
  Reservation_kv3_v1_ALU_FULL,
  Reservation_kv3_v1_ALU_FULL_X,
  Reservation_kv3_v1_ALU_FULL_Y,
  Reservation_kv3_v1_BCU,
  Reservation_kv3_v1_BCU_XFER,
  Reservation_kv3_v1_BCU_CRRP_CRWL_CRWH,
  Reservation_kv3_v1_BCU_TINY_AUXW_CRRP,
  Reservation_kv3_v1_BCU_TINY_TINY_MAU_XNOP,
  Reservation_kv3_v1_EXT,
  Reservation_kv3_v1_LSU,
  Reservation_kv3_v1_LSU_X,
  Reservation_kv3_v1_LSU_Y,
  Reservation_kv3_v1_LSU_CRRP,
  Reservation_kv3_v1_LSU_CRRP_X,
  Reservation_kv3_v1_LSU_CRRP_Y,
  Reservation_kv3_v1_LSU_AUXR,
  Reservation_kv3_v1_LSU_AUXR_X,
  Reservation_kv3_v1_LSU_AUXR_Y,
  Reservation_kv3_v1_LSU_AUXW,
  Reservation_kv3_v1_LSU_AUXW_X,
  Reservation_kv3_v1_LSU_AUXW_Y,
  Reservation_kv3_v1_LSU_AUXR_AUXW,
  Reservation_kv3_v1_LSU_AUXR_AUXW_X,
  Reservation_kv3_v1_LSU_AUXR_AUXW_Y,
  Reservation_kv3_v1_MAU,
  Reservation_kv3_v1_MAU_X,
  Reservation_kv3_v1_MAU_Y,
  Reservation_kv3_v1_MAU_AUXR,
  Reservation_kv3_v1_MAU_AUXR_X,
  Reservation_kv3_v1_MAU_AUXR_Y,
} Reservation_kv3_v1;

extern struct kvx_reloc kv3_v1_rel16_reloc;
extern struct kvx_reloc kv3_v1_rel32_reloc;
extern struct kvx_reloc kv3_v1_rel64_reloc;
extern struct kvx_reloc kv3_v1_pcrel_signed16_reloc;
extern struct kvx_reloc kv3_v1_pcrel17_reloc;
extern struct kvx_reloc kv3_v1_pcrel27_reloc;
extern struct kvx_reloc kv3_v1_pcrel32_reloc;
extern struct kvx_reloc kv3_v1_pcrel_signed37_reloc;
extern struct kvx_reloc kv3_v1_pcrel_signed43_reloc;
extern struct kvx_reloc kv3_v1_pcrel_signed64_reloc;
extern struct kvx_reloc kv3_v1_pcrel64_reloc;
extern struct kvx_reloc kv3_v1_signed16_reloc;
extern struct kvx_reloc kv3_v1_signed32_reloc;
extern struct kvx_reloc kv3_v1_signed37_reloc;
extern struct kvx_reloc kv3_v1_gotoff_signed37_reloc;
extern struct kvx_reloc kv3_v1_gotoff_signed43_reloc;
extern struct kvx_reloc kv3_v1_gotoff_32_reloc;
extern struct kvx_reloc kv3_v1_gotoff_64_reloc;
extern struct kvx_reloc kv3_v1_got_32_reloc;
extern struct kvx_reloc kv3_v1_got_signed37_reloc;
extern struct kvx_reloc kv3_v1_got_signed43_reloc;
extern struct kvx_reloc kv3_v1_got_64_reloc;
extern struct kvx_reloc kv3_v1_glob_dat_reloc;
extern struct kvx_reloc kv3_v1_copy_reloc;
extern struct kvx_reloc kv3_v1_jump_slot_reloc;
extern struct kvx_reloc kv3_v1_relative_reloc;
extern struct kvx_reloc kv3_v1_signed43_reloc;
extern struct kvx_reloc kv3_v1_signed64_reloc;
extern struct kvx_reloc kv3_v1_gotaddr_signed37_reloc;
extern struct kvx_reloc kv3_v1_gotaddr_signed43_reloc;
extern struct kvx_reloc kv3_v1_gotaddr_signed64_reloc;
extern struct kvx_reloc kv3_v1_dtpmod64_reloc;
extern struct kvx_reloc kv3_v1_dtpoff64_reloc;
extern struct kvx_reloc kv3_v1_dtpoff_signed37_reloc;
extern struct kvx_reloc kv3_v1_dtpoff_signed43_reloc;
extern struct kvx_reloc kv3_v1_tlsgd_signed37_reloc;
extern struct kvx_reloc kv3_v1_tlsgd_signed43_reloc;
extern struct kvx_reloc kv3_v1_tlsld_signed37_reloc;
extern struct kvx_reloc kv3_v1_tlsld_signed43_reloc;
extern struct kvx_reloc kv3_v1_tpoff64_reloc;
extern struct kvx_reloc kv3_v1_tlsie_signed37_reloc;
extern struct kvx_reloc kv3_v1_tlsie_signed43_reloc;
extern struct kvx_reloc kv3_v1_tlsle_signed37_reloc;
extern struct kvx_reloc kv3_v1_tlsle_signed43_reloc;
extern struct kvx_reloc kv3_v1_rel8_reloc;

#define KV3_V2_REGFILE_FIRST_SFR KVX_REGFILE_FIRST_SFR
#define KV3_V2_REGFILE_LAST_SFR KVX_REGFILE_LAST_SFR
#define KV3_V2_REGFILE_DEC_SFR KVX_REGFILE_DEC_SFR
#define KV3_V2_REGFILE_FIRST_GPR KVX_REGFILE_FIRST_GPR
#define KV3_V2_REGFILE_LAST_GPR KVX_REGFILE_LAST_GPR
#define KV3_V2_REGFILE_DEC_GPR KVX_REGFILE_DEC_GPR
#define KV3_V2_REGFILE_FIRST_PGR 6
#define KV3_V2_REGFILE_LAST_PGR 7
#define KV3_V2_REGFILE_DEC_PGR 8
#define KV3_V2_REGFILE_FIRST_QGR 9
#define KV3_V2_REGFILE_LAST_QGR 10
#define KV3_V2_REGFILE_DEC_QGR 11
#define KV3_V2_REGFILE_FIRST_X16R 12
#define KV3_V2_REGFILE_LAST_X16R 13
#define KV3_V2_REGFILE_DEC_X16R 14
#define KV3_V2_REGFILE_FIRST_X2R 15
#define KV3_V2_REGFILE_LAST_X2R 16
#define KV3_V2_REGFILE_DEC_X2R 17
#define KV3_V2_REGFILE_FIRST_X32R 18
#define KV3_V2_REGFILE_LAST_X32R 19
#define KV3_V2_REGFILE_DEC_X32R 20
#define KV3_V2_REGFILE_FIRST_X4R 21
#define KV3_V2_REGFILE_LAST_X4R 22
#define KV3_V2_REGFILE_DEC_X4R 23
#define KV3_V2_REGFILE_FIRST_X64R 24
#define KV3_V2_REGFILE_LAST_X64R 25
#define KV3_V2_REGFILE_DEC_X64R 26
#define KV3_V2_REGFILE_FIRST_X8R 27
#define KV3_V2_REGFILE_LAST_X8R 28
#define KV3_V2_REGFILE_DEC_X8R 29
#define KV3_V2_REGFILE_FIRST_XBR 30
#define KV3_V2_REGFILE_LAST_XBR 31
#define KV3_V2_REGFILE_DEC_XBR 32
#define KV3_V2_REGFILE_FIRST_XCR 33
#define KV3_V2_REGFILE_LAST_XCR 34
#define KV3_V2_REGFILE_DEC_XCR 35
#define KV3_V2_REGFILE_FIRST_XMR 36
#define KV3_V2_REGFILE_LAST_XMR 37
#define KV3_V2_REGFILE_DEC_XMR 38
#define KV3_V2_REGFILE_FIRST_XTR 39
#define KV3_V2_REGFILE_LAST_XTR 40
#define KV3_V2_REGFILE_DEC_XTR 41
#define KV3_V2_REGFILE_FIRST_XVR 42
#define KV3_V2_REGFILE_LAST_XVR 43
#define KV3_V2_REGFILE_DEC_XVR 44
#define KV3_V2_REGFILE_REGISTERS 45
#define KV3_V2_REGFILE_DEC_REGISTERS 46


extern int kv3_v2_regfiles[];
extern const char **kv3_v2_modifiers[];
extern struct kvx_register kv3_v2_registers[];

extern int kv3_v2_dec_registers[];

enum Method_kv3_v2_enum {
  Immediate_kv3_v2_brknumber = 1,
  Immediate_kv3_v2_pcrel17 = 2,
  Immediate_kv3_v2_pcrel27 = 3,
  Immediate_kv3_v2_signed10 = 4,
  Immediate_kv3_v2_signed16 = 5,
  Immediate_kv3_v2_signed27 = 6,
  Immediate_kv3_v2_signed37 = 7,
  Immediate_kv3_v2_signed43 = 8,
  Immediate_kv3_v2_signed54 = 9,
  Immediate_kv3_v2_sysnumber = 10,
  Immediate_kv3_v2_unsigned6 = 11,
  Immediate_kv3_v2_wrapped32 = 12,
  Immediate_kv3_v2_wrapped64 = 13,
  Immediate_kv3_v2_wrapped8 = 14,
  Modifier_kv3_v2_accesses = 15,
  Modifier_kv3_v2_boolcas = 16,
  Modifier_kv3_v2_cachelev = 17,
  Modifier_kv3_v2_channel = 18,
  Modifier_kv3_v2_coherency = 19,
  Modifier_kv3_v2_comparison = 20,
  Modifier_kv3_v2_conjugate = 21,
  Modifier_kv3_v2_doscale = 22,
  Modifier_kv3_v2_exunum = 23,
  Modifier_kv3_v2_floatcomp = 24,
  Modifier_kv3_v2_hindex = 25,
  Modifier_kv3_v2_lsomask = 26,
  Modifier_kv3_v2_lsumask = 27,
  Modifier_kv3_v2_qindex = 28,
  Modifier_kv3_v2_rounding = 29,
  Modifier_kv3_v2_scalarcond = 30,
  Modifier_kv3_v2_shuffleV = 31,
  Modifier_kv3_v2_shuffleX = 32,
  Modifier_kv3_v2_silent = 33,
  Modifier_kv3_v2_simdcond = 34,
  Modifier_kv3_v2_speculate = 35,
  Modifier_kv3_v2_splat32 = 36,
  Modifier_kv3_v2_transpose = 37,
  Modifier_kv3_v2_variant = 38,
  RegClass_kv3_v2_aloneReg = 39,
  RegClass_kv3_v2_buffer16Reg = 40,
  RegClass_kv3_v2_buffer2Reg = 41,
  RegClass_kv3_v2_buffer32Reg = 42,
  RegClass_kv3_v2_buffer4Reg = 43,
  RegClass_kv3_v2_buffer64Reg = 44,
  RegClass_kv3_v2_buffer8Reg = 45,
  RegClass_kv3_v2_onlyfxReg = 46,
  RegClass_kv3_v2_onlygetReg = 47,
  RegClass_kv3_v2_onlyraReg = 48,
  RegClass_kv3_v2_onlysetReg = 49,
  RegClass_kv3_v2_onlyswapReg = 50,
  RegClass_kv3_v2_pairedReg = 51,
  RegClass_kv3_v2_quadReg = 52,
  RegClass_kv3_v2_singleReg = 53,
  RegClass_kv3_v2_systemReg = 54,
  RegClass_kv3_v2_xworddReg = 55,
  RegClass_kv3_v2_xworddReg0M4 = 56,
  RegClass_kv3_v2_xworddReg1M4 = 57,
  RegClass_kv3_v2_xworddReg2M4 = 58,
  RegClass_kv3_v2_xworddReg3M4 = 59,
  RegClass_kv3_v2_xwordoReg = 60,
  RegClass_kv3_v2_xwordqReg = 61,
  RegClass_kv3_v2_xwordqRegE = 62,
  RegClass_kv3_v2_xwordqRegO = 63,
  RegClass_kv3_v2_xwordvReg = 64,
  RegClass_kv3_v2_xwordxReg = 65,
  Instruction_kv3_v2_abdbo = 66,
  Instruction_kv3_v2_abdd = 67,
  Instruction_kv3_v2_abdhq = 68,
  Instruction_kv3_v2_abdsbo = 69,
  Instruction_kv3_v2_abdsd = 70,
  Instruction_kv3_v2_abdshq = 71,
  Instruction_kv3_v2_abdsw = 72,
  Instruction_kv3_v2_abdswp = 73,
  Instruction_kv3_v2_abdubo = 74,
  Instruction_kv3_v2_abdud = 75,
  Instruction_kv3_v2_abduhq = 76,
  Instruction_kv3_v2_abduw = 77,
  Instruction_kv3_v2_abduwp = 78,
  Instruction_kv3_v2_abdw = 79,
  Instruction_kv3_v2_abdwp = 80,
  Instruction_kv3_v2_absbo = 81,
  Instruction_kv3_v2_absd = 82,
  Instruction_kv3_v2_abshq = 83,
  Instruction_kv3_v2_abssbo = 84,
  Instruction_kv3_v2_abssd = 85,
  Instruction_kv3_v2_absshq = 86,
  Instruction_kv3_v2_abssw = 87,
  Instruction_kv3_v2_absswp = 88,
  Instruction_kv3_v2_absw = 89,
  Instruction_kv3_v2_abswp = 90,
  Instruction_kv3_v2_acswapd = 91,
  Instruction_kv3_v2_acswapq = 92,
  Instruction_kv3_v2_acswapw = 93,
  Instruction_kv3_v2_addbo = 94,
  Instruction_kv3_v2_addcd = 95,
  Instruction_kv3_v2_addcd_i = 96,
  Instruction_kv3_v2_addd = 97,
  Instruction_kv3_v2_addhq = 98,
  Instruction_kv3_v2_addrbod = 99,
  Instruction_kv3_v2_addrhqd = 100,
  Instruction_kv3_v2_addrwpd = 101,
  Instruction_kv3_v2_addsbo = 102,
  Instruction_kv3_v2_addsd = 103,
  Instruction_kv3_v2_addshq = 104,
  Instruction_kv3_v2_addsw = 105,
  Instruction_kv3_v2_addswp = 106,
  Instruction_kv3_v2_addurbod = 107,
  Instruction_kv3_v2_addurhqd = 108,
  Instruction_kv3_v2_addurwpd = 109,
  Instruction_kv3_v2_addusbo = 110,
  Instruction_kv3_v2_addusd = 111,
  Instruction_kv3_v2_addushq = 112,
  Instruction_kv3_v2_addusw = 113,
  Instruction_kv3_v2_adduswp = 114,
  Instruction_kv3_v2_adduwd = 115,
  Instruction_kv3_v2_addw = 116,
  Instruction_kv3_v2_addwd = 117,
  Instruction_kv3_v2_addwp = 118,
  Instruction_kv3_v2_addx16bo = 119,
  Instruction_kv3_v2_addx16d = 120,
  Instruction_kv3_v2_addx16hq = 121,
  Instruction_kv3_v2_addx16uwd = 122,
  Instruction_kv3_v2_addx16w = 123,
  Instruction_kv3_v2_addx16wd = 124,
  Instruction_kv3_v2_addx16wp = 125,
  Instruction_kv3_v2_addx2bo = 126,
  Instruction_kv3_v2_addx2d = 127,
  Instruction_kv3_v2_addx2hq = 128,
  Instruction_kv3_v2_addx2uwd = 129,
  Instruction_kv3_v2_addx2w = 130,
  Instruction_kv3_v2_addx2wd = 131,
  Instruction_kv3_v2_addx2wp = 132,
  Instruction_kv3_v2_addx32d = 133,
  Instruction_kv3_v2_addx32uwd = 134,
  Instruction_kv3_v2_addx32w = 135,
  Instruction_kv3_v2_addx32wd = 136,
  Instruction_kv3_v2_addx4bo = 137,
  Instruction_kv3_v2_addx4d = 138,
  Instruction_kv3_v2_addx4hq = 139,
  Instruction_kv3_v2_addx4uwd = 140,
  Instruction_kv3_v2_addx4w = 141,
  Instruction_kv3_v2_addx4wd = 142,
  Instruction_kv3_v2_addx4wp = 143,
  Instruction_kv3_v2_addx64d = 144,
  Instruction_kv3_v2_addx64uwd = 145,
  Instruction_kv3_v2_addx64w = 146,
  Instruction_kv3_v2_addx64wd = 147,
  Instruction_kv3_v2_addx8bo = 148,
  Instruction_kv3_v2_addx8d = 149,
  Instruction_kv3_v2_addx8hq = 150,
  Instruction_kv3_v2_addx8uwd = 151,
  Instruction_kv3_v2_addx8w = 152,
  Instruction_kv3_v2_addx8wd = 153,
  Instruction_kv3_v2_addx8wp = 154,
  Instruction_kv3_v2_aladdd = 155,
  Instruction_kv3_v2_aladdw = 156,
  Instruction_kv3_v2_alclrd = 157,
  Instruction_kv3_v2_alclrw = 158,
  Instruction_kv3_v2_ald = 159,
  Instruction_kv3_v2_alw = 160,
  Instruction_kv3_v2_andd = 161,
  Instruction_kv3_v2_andnd = 162,
  Instruction_kv3_v2_andnw = 163,
  Instruction_kv3_v2_andrbod = 164,
  Instruction_kv3_v2_andrhqd = 165,
  Instruction_kv3_v2_andrwpd = 166,
  Instruction_kv3_v2_andw = 167,
  Instruction_kv3_v2_asd = 168,
  Instruction_kv3_v2_asw = 169,
  Instruction_kv3_v2_avgbo = 170,
  Instruction_kv3_v2_avghq = 171,
  Instruction_kv3_v2_avgrbo = 172,
  Instruction_kv3_v2_avgrhq = 173,
  Instruction_kv3_v2_avgrubo = 174,
  Instruction_kv3_v2_avgruhq = 175,
  Instruction_kv3_v2_avgruw = 176,
  Instruction_kv3_v2_avgruwp = 177,
  Instruction_kv3_v2_avgrw = 178,
  Instruction_kv3_v2_avgrwp = 179,
  Instruction_kv3_v2_avgubo = 180,
  Instruction_kv3_v2_avguhq = 181,
  Instruction_kv3_v2_avguw = 182,
  Instruction_kv3_v2_avguwp = 183,
  Instruction_kv3_v2_avgw = 184,
  Instruction_kv3_v2_avgwp = 185,
  Instruction_kv3_v2_await = 186,
  Instruction_kv3_v2_barrier = 187,
  Instruction_kv3_v2_break = 188,
  Instruction_kv3_v2_call = 189,
  Instruction_kv3_v2_cb = 190,
  Instruction_kv3_v2_cbsd = 191,
  Instruction_kv3_v2_cbsw = 192,
  Instruction_kv3_v2_cbswp = 193,
  Instruction_kv3_v2_clrf = 194,
  Instruction_kv3_v2_clsd = 195,
  Instruction_kv3_v2_clsw = 196,
  Instruction_kv3_v2_clswp = 197,
  Instruction_kv3_v2_clzd = 198,
  Instruction_kv3_v2_clzw = 199,
  Instruction_kv3_v2_clzwp = 200,
  Instruction_kv3_v2_cmovebo = 201,
  Instruction_kv3_v2_cmoved = 202,
  Instruction_kv3_v2_cmovehq = 203,
  Instruction_kv3_v2_cmovewp = 204,
  Instruction_kv3_v2_cmuldt = 205,
  Instruction_kv3_v2_cmulghxdt = 206,
  Instruction_kv3_v2_cmulglxdt = 207,
  Instruction_kv3_v2_cmulgmxdt = 208,
  Instruction_kv3_v2_cmulxdt = 209,
  Instruction_kv3_v2_compd = 210,
  Instruction_kv3_v2_compnbo = 211,
  Instruction_kv3_v2_compnd = 212,
  Instruction_kv3_v2_compnhq = 213,
  Instruction_kv3_v2_compnw = 214,
  Instruction_kv3_v2_compnwp = 215,
  Instruction_kv3_v2_compuwd = 216,
  Instruction_kv3_v2_compw = 217,
  Instruction_kv3_v2_compwd = 218,
  Instruction_kv3_v2_copyd = 219,
  Instruction_kv3_v2_copyo = 220,
  Instruction_kv3_v2_copyq = 221,
  Instruction_kv3_v2_copyw = 222,
  Instruction_kv3_v2_crcbellw = 223,
  Instruction_kv3_v2_crcbelmw = 224,
  Instruction_kv3_v2_crclellw = 225,
  Instruction_kv3_v2_crclelmw = 226,
  Instruction_kv3_v2_ctzd = 227,
  Instruction_kv3_v2_ctzw = 228,
  Instruction_kv3_v2_ctzwp = 229,
  Instruction_kv3_v2_d1inval = 230,
  Instruction_kv3_v2_dflushl = 231,
  Instruction_kv3_v2_dflushsw = 232,
  Instruction_kv3_v2_dinvall = 233,
  Instruction_kv3_v2_dinvalsw = 234,
  Instruction_kv3_v2_dot2suwd = 235,
  Instruction_kv3_v2_dot2suwdp = 236,
  Instruction_kv3_v2_dot2uwd = 237,
  Instruction_kv3_v2_dot2uwdp = 238,
  Instruction_kv3_v2_dot2w = 239,
  Instruction_kv3_v2_dot2wd = 240,
  Instruction_kv3_v2_dot2wdp = 241,
  Instruction_kv3_v2_dot2wzp = 242,
  Instruction_kv3_v2_dpurgel = 243,
  Instruction_kv3_v2_dpurgesw = 244,
  Instruction_kv3_v2_dtouchl = 245,
  Instruction_kv3_v2_eord = 246,
  Instruction_kv3_v2_eorrbod = 247,
  Instruction_kv3_v2_eorrhqd = 248,
  Instruction_kv3_v2_eorrwpd = 249,
  Instruction_kv3_v2_eorw = 250,
  Instruction_kv3_v2_errop = 251,
  Instruction_kv3_v2_extfs = 252,
  Instruction_kv3_v2_extfz = 253,
  Instruction_kv3_v2_fabsd = 254,
  Instruction_kv3_v2_fabshq = 255,
  Instruction_kv3_v2_fabsw = 256,
  Instruction_kv3_v2_fabswp = 257,
  Instruction_kv3_v2_faddd = 258,
  Instruction_kv3_v2_fadddc = 259,
  Instruction_kv3_v2_fadddc_c = 260,
  Instruction_kv3_v2_fadddp = 261,
  Instruction_kv3_v2_faddho = 262,
  Instruction_kv3_v2_faddhq = 263,
  Instruction_kv3_v2_faddw = 264,
  Instruction_kv3_v2_faddwc = 265,
  Instruction_kv3_v2_faddwc_c = 266,
  Instruction_kv3_v2_faddwcp = 267,
  Instruction_kv3_v2_faddwcp_c = 268,
  Instruction_kv3_v2_faddwp = 269,
  Instruction_kv3_v2_faddwq = 270,
  Instruction_kv3_v2_fcdivd = 271,
  Instruction_kv3_v2_fcdivw = 272,
  Instruction_kv3_v2_fcdivwp = 273,
  Instruction_kv3_v2_fcompd = 274,
  Instruction_kv3_v2_fcompnd = 275,
  Instruction_kv3_v2_fcompnhq = 276,
  Instruction_kv3_v2_fcompnw = 277,
  Instruction_kv3_v2_fcompnwp = 278,
  Instruction_kv3_v2_fcompw = 279,
  Instruction_kv3_v2_fdot2w = 280,
  Instruction_kv3_v2_fdot2wd = 281,
  Instruction_kv3_v2_fdot2wdp = 282,
  Instruction_kv3_v2_fdot2wzp = 283,
  Instruction_kv3_v2_fence = 284,
  Instruction_kv3_v2_ffdmasw = 285,
  Instruction_kv3_v2_ffdmaswp = 286,
  Instruction_kv3_v2_ffdmaswq = 287,
  Instruction_kv3_v2_ffdmaw = 288,
  Instruction_kv3_v2_ffdmawp = 289,
  Instruction_kv3_v2_ffdmawq = 290,
  Instruction_kv3_v2_ffdmdaw = 291,
  Instruction_kv3_v2_ffdmdawp = 292,
  Instruction_kv3_v2_ffdmdawq = 293,
  Instruction_kv3_v2_ffdmdsw = 294,
  Instruction_kv3_v2_ffdmdswp = 295,
  Instruction_kv3_v2_ffdmdswq = 296,
  Instruction_kv3_v2_ffdmsaw = 297,
  Instruction_kv3_v2_ffdmsawp = 298,
  Instruction_kv3_v2_ffdmsawq = 299,
  Instruction_kv3_v2_ffdmsw = 300,
  Instruction_kv3_v2_ffdmswp = 301,
  Instruction_kv3_v2_ffdmswq = 302,
  Instruction_kv3_v2_ffmad = 303,
  Instruction_kv3_v2_ffmaho = 304,
  Instruction_kv3_v2_ffmahq = 305,
  Instruction_kv3_v2_ffmahw = 306,
  Instruction_kv3_v2_ffmahwq = 307,
  Instruction_kv3_v2_ffmaw = 308,
  Instruction_kv3_v2_ffmawc = 309,
  Instruction_kv3_v2_ffmawcp = 310,
  Instruction_kv3_v2_ffmawd = 311,
  Instruction_kv3_v2_ffmawdp = 312,
  Instruction_kv3_v2_ffmawp = 313,
  Instruction_kv3_v2_ffmawq = 314,
  Instruction_kv3_v2_ffmsd = 315,
  Instruction_kv3_v2_ffmsho = 316,
  Instruction_kv3_v2_ffmshq = 317,
  Instruction_kv3_v2_ffmshw = 318,
  Instruction_kv3_v2_ffmshwq = 319,
  Instruction_kv3_v2_ffmsw = 320,
  Instruction_kv3_v2_ffmswc = 321,
  Instruction_kv3_v2_ffmswcp = 322,
  Instruction_kv3_v2_ffmswd = 323,
  Instruction_kv3_v2_ffmswdp = 324,
  Instruction_kv3_v2_ffmswp = 325,
  Instruction_kv3_v2_ffmswq = 326,
  Instruction_kv3_v2_fixedd = 327,
  Instruction_kv3_v2_fixedud = 328,
  Instruction_kv3_v2_fixeduw = 329,
  Instruction_kv3_v2_fixeduwp = 330,
  Instruction_kv3_v2_fixedw = 331,
  Instruction_kv3_v2_fixedwp = 332,
  Instruction_kv3_v2_floatd = 333,
  Instruction_kv3_v2_floatud = 334,
  Instruction_kv3_v2_floatuw = 335,
  Instruction_kv3_v2_floatuwp = 336,
  Instruction_kv3_v2_floatw = 337,
  Instruction_kv3_v2_floatwp = 338,
  Instruction_kv3_v2_fmaxd = 339,
  Instruction_kv3_v2_fmaxhq = 340,
  Instruction_kv3_v2_fmaxw = 341,
  Instruction_kv3_v2_fmaxwp = 342,
  Instruction_kv3_v2_fmind = 343,
  Instruction_kv3_v2_fminhq = 344,
  Instruction_kv3_v2_fminw = 345,
  Instruction_kv3_v2_fminwp = 346,
  Instruction_kv3_v2_fmm212w = 347,
  Instruction_kv3_v2_fmm222w = 348,
  Instruction_kv3_v2_fmma212w = 349,
  Instruction_kv3_v2_fmma222w = 350,
  Instruction_kv3_v2_fmms212w = 351,
  Instruction_kv3_v2_fmms222w = 352,
  Instruction_kv3_v2_fmuld = 353,
  Instruction_kv3_v2_fmulho = 354,
  Instruction_kv3_v2_fmulhq = 355,
  Instruction_kv3_v2_fmulhw = 356,
  Instruction_kv3_v2_fmulhwq = 357,
  Instruction_kv3_v2_fmulw = 358,
  Instruction_kv3_v2_fmulwc = 359,
  Instruction_kv3_v2_fmulwcp = 360,
  Instruction_kv3_v2_fmulwd = 361,
  Instruction_kv3_v2_fmulwdp = 362,
  Instruction_kv3_v2_fmulwp = 363,
  Instruction_kv3_v2_fmulwq = 364,
  Instruction_kv3_v2_fnarrowdw = 365,
  Instruction_kv3_v2_fnarrowdwp = 366,
  Instruction_kv3_v2_fnarrowwh = 367,
  Instruction_kv3_v2_fnarrowwhq = 368,
  Instruction_kv3_v2_fnegd = 369,
  Instruction_kv3_v2_fneghq = 370,
  Instruction_kv3_v2_fnegw = 371,
  Instruction_kv3_v2_fnegwp = 372,
  Instruction_kv3_v2_frecw = 373,
  Instruction_kv3_v2_frsrw = 374,
  Instruction_kv3_v2_fsbfd = 375,
  Instruction_kv3_v2_fsbfdc = 376,
  Instruction_kv3_v2_fsbfdc_c = 377,
  Instruction_kv3_v2_fsbfdp = 378,
  Instruction_kv3_v2_fsbfho = 379,
  Instruction_kv3_v2_fsbfhq = 380,
  Instruction_kv3_v2_fsbfw = 381,
  Instruction_kv3_v2_fsbfwc = 382,
  Instruction_kv3_v2_fsbfwc_c = 383,
  Instruction_kv3_v2_fsbfwcp = 384,
  Instruction_kv3_v2_fsbfwcp_c = 385,
  Instruction_kv3_v2_fsbfwp = 386,
  Instruction_kv3_v2_fsbfwq = 387,
  Instruction_kv3_v2_fsdivd = 388,
  Instruction_kv3_v2_fsdivw = 389,
  Instruction_kv3_v2_fsdivwp = 390,
  Instruction_kv3_v2_fsrecd = 391,
  Instruction_kv3_v2_fsrecw = 392,
  Instruction_kv3_v2_fsrecwp = 393,
  Instruction_kv3_v2_fsrsrd = 394,
  Instruction_kv3_v2_fsrsrw = 395,
  Instruction_kv3_v2_fsrsrwp = 396,
  Instruction_kv3_v2_fwidenlhw = 397,
  Instruction_kv3_v2_fwidenlhwp = 398,
  Instruction_kv3_v2_fwidenlwd = 399,
  Instruction_kv3_v2_fwidenmhw = 400,
  Instruction_kv3_v2_fwidenmhwp = 401,
  Instruction_kv3_v2_fwidenmwd = 402,
  Instruction_kv3_v2_get = 403,
  Instruction_kv3_v2_goto = 404,
  Instruction_kv3_v2_i1inval = 405,
  Instruction_kv3_v2_i1invals = 406,
  Instruction_kv3_v2_icall = 407,
  Instruction_kv3_v2_iget = 408,
  Instruction_kv3_v2_igoto = 409,
  Instruction_kv3_v2_insf = 410,
  Instruction_kv3_v2_iord = 411,
  Instruction_kv3_v2_iornd = 412,
  Instruction_kv3_v2_iornw = 413,
  Instruction_kv3_v2_iorrbod = 414,
  Instruction_kv3_v2_iorrhqd = 415,
  Instruction_kv3_v2_iorrwpd = 416,
  Instruction_kv3_v2_iorw = 417,
  Instruction_kv3_v2_landd = 418,
  Instruction_kv3_v2_landw = 419,
  Instruction_kv3_v2_lbs = 420,
  Instruction_kv3_v2_lbz = 421,
  Instruction_kv3_v2_ld = 422,
  Instruction_kv3_v2_lhs = 423,
  Instruction_kv3_v2_lhz = 424,
  Instruction_kv3_v2_liord = 425,
  Instruction_kv3_v2_liorw = 426,
  Instruction_kv3_v2_lnandd = 427,
  Instruction_kv3_v2_lnandw = 428,
  Instruction_kv3_v2_lniord = 429,
  Instruction_kv3_v2_lniorw = 430,
  Instruction_kv3_v2_lnord = 431,
  Instruction_kv3_v2_lnorw = 432,
  Instruction_kv3_v2_lo = 433,
  Instruction_kv3_v2_loopdo = 434,
  Instruction_kv3_v2_lord = 435,
  Instruction_kv3_v2_lorw = 436,
  Instruction_kv3_v2_lq = 437,
  Instruction_kv3_v2_lws = 438,
  Instruction_kv3_v2_lwz = 439,
  Instruction_kv3_v2_maddd = 440,
  Instruction_kv3_v2_madddt = 441,
  Instruction_kv3_v2_maddhq = 442,
  Instruction_kv3_v2_maddhwq = 443,
  Instruction_kv3_v2_maddmwq = 444,
  Instruction_kv3_v2_maddsudt = 445,
  Instruction_kv3_v2_maddsuhwq = 446,
  Instruction_kv3_v2_maddsumwq = 447,
  Instruction_kv3_v2_maddsuwd = 448,
  Instruction_kv3_v2_maddsuwdp = 449,
  Instruction_kv3_v2_maddudt = 450,
  Instruction_kv3_v2_madduhwq = 451,
  Instruction_kv3_v2_maddumwq = 452,
  Instruction_kv3_v2_madduwd = 453,
  Instruction_kv3_v2_madduwdp = 454,
  Instruction_kv3_v2_madduzdt = 455,
  Instruction_kv3_v2_maddw = 456,
  Instruction_kv3_v2_maddwd = 457,
  Instruction_kv3_v2_maddwdp = 458,
  Instruction_kv3_v2_maddwp = 459,
  Instruction_kv3_v2_maddwq = 460,
  Instruction_kv3_v2_make = 461,
  Instruction_kv3_v2_maxbo = 462,
  Instruction_kv3_v2_maxd = 463,
  Instruction_kv3_v2_maxhq = 464,
  Instruction_kv3_v2_maxrbod = 465,
  Instruction_kv3_v2_maxrhqd = 466,
  Instruction_kv3_v2_maxrwpd = 467,
  Instruction_kv3_v2_maxubo = 468,
  Instruction_kv3_v2_maxud = 469,
  Instruction_kv3_v2_maxuhq = 470,
  Instruction_kv3_v2_maxurbod = 471,
  Instruction_kv3_v2_maxurhqd = 472,
  Instruction_kv3_v2_maxurwpd = 473,
  Instruction_kv3_v2_maxuw = 474,
  Instruction_kv3_v2_maxuwp = 475,
  Instruction_kv3_v2_maxw = 476,
  Instruction_kv3_v2_maxwp = 477,
  Instruction_kv3_v2_minbo = 478,
  Instruction_kv3_v2_mind = 479,
  Instruction_kv3_v2_minhq = 480,
  Instruction_kv3_v2_minrbod = 481,
  Instruction_kv3_v2_minrhqd = 482,
  Instruction_kv3_v2_minrwpd = 483,
  Instruction_kv3_v2_minubo = 484,
  Instruction_kv3_v2_minud = 485,
  Instruction_kv3_v2_minuhq = 486,
  Instruction_kv3_v2_minurbod = 487,
  Instruction_kv3_v2_minurhqd = 488,
  Instruction_kv3_v2_minurwpd = 489,
  Instruction_kv3_v2_minuw = 490,
  Instruction_kv3_v2_minuwp = 491,
  Instruction_kv3_v2_minw = 492,
  Instruction_kv3_v2_minwp = 493,
  Instruction_kv3_v2_mm212w = 494,
  Instruction_kv3_v2_mma212w = 495,
  Instruction_kv3_v2_mms212w = 496,
  Instruction_kv3_v2_msbfd = 497,
  Instruction_kv3_v2_msbfdt = 498,
  Instruction_kv3_v2_msbfhq = 499,
  Instruction_kv3_v2_msbfhwq = 500,
  Instruction_kv3_v2_msbfmwq = 501,
  Instruction_kv3_v2_msbfsudt = 502,
  Instruction_kv3_v2_msbfsuhwq = 503,
  Instruction_kv3_v2_msbfsumwq = 504,
  Instruction_kv3_v2_msbfsuwd = 505,
  Instruction_kv3_v2_msbfsuwdp = 506,
  Instruction_kv3_v2_msbfudt = 507,
  Instruction_kv3_v2_msbfuhwq = 508,
  Instruction_kv3_v2_msbfumwq = 509,
  Instruction_kv3_v2_msbfuwd = 510,
  Instruction_kv3_v2_msbfuwdp = 511,
  Instruction_kv3_v2_msbfuzdt = 512,
  Instruction_kv3_v2_msbfw = 513,
  Instruction_kv3_v2_msbfwd = 514,
  Instruction_kv3_v2_msbfwdp = 515,
  Instruction_kv3_v2_msbfwp = 516,
  Instruction_kv3_v2_msbfwq = 517,
  Instruction_kv3_v2_muld = 518,
  Instruction_kv3_v2_muldt = 519,
  Instruction_kv3_v2_mulhq = 520,
  Instruction_kv3_v2_mulhwq = 521,
  Instruction_kv3_v2_mulmwq = 522,
  Instruction_kv3_v2_mulsudt = 523,
  Instruction_kv3_v2_mulsuhwq = 524,
  Instruction_kv3_v2_mulsumwq = 525,
  Instruction_kv3_v2_mulsuwd = 526,
  Instruction_kv3_v2_mulsuwdp = 527,
  Instruction_kv3_v2_muludt = 528,
  Instruction_kv3_v2_muluhwq = 529,
  Instruction_kv3_v2_mulumwq = 530,
  Instruction_kv3_v2_muluwd = 531,
  Instruction_kv3_v2_muluwdp = 532,
  Instruction_kv3_v2_mulw = 533,
  Instruction_kv3_v2_mulwd = 534,
  Instruction_kv3_v2_mulwdp = 535,
  Instruction_kv3_v2_mulwp = 536,
  Instruction_kv3_v2_mulwq = 537,
  Instruction_kv3_v2_nandd = 538,
  Instruction_kv3_v2_nandw = 539,
  Instruction_kv3_v2_negbo = 540,
  Instruction_kv3_v2_negd = 541,
  Instruction_kv3_v2_neghq = 542,
  Instruction_kv3_v2_negsbo = 543,
  Instruction_kv3_v2_negsd = 544,
  Instruction_kv3_v2_negshq = 545,
  Instruction_kv3_v2_negsw = 546,
  Instruction_kv3_v2_negswp = 547,
  Instruction_kv3_v2_negw = 548,
  Instruction_kv3_v2_negwp = 549,
  Instruction_kv3_v2_neord = 550,
  Instruction_kv3_v2_neorw = 551,
  Instruction_kv3_v2_niord = 552,
  Instruction_kv3_v2_niorw = 553,
  Instruction_kv3_v2_nop = 554,
  Instruction_kv3_v2_nord = 555,
  Instruction_kv3_v2_norw = 556,
  Instruction_kv3_v2_notd = 557,
  Instruction_kv3_v2_notw = 558,
  Instruction_kv3_v2_nxord = 559,
  Instruction_kv3_v2_nxorw = 560,
  Instruction_kv3_v2_ord = 561,
  Instruction_kv3_v2_ornd = 562,
  Instruction_kv3_v2_ornw = 563,
  Instruction_kv3_v2_orrbod = 564,
  Instruction_kv3_v2_orrhqd = 565,
  Instruction_kv3_v2_orrwpd = 566,
  Instruction_kv3_v2_orw = 567,
  Instruction_kv3_v2_pcrel = 568,
  Instruction_kv3_v2_ret = 569,
  Instruction_kv3_v2_rfe = 570,
  Instruction_kv3_v2_rolw = 571,
  Instruction_kv3_v2_rolwps = 572,
  Instruction_kv3_v2_rorw = 573,
  Instruction_kv3_v2_rorwps = 574,
  Instruction_kv3_v2_rswap = 575,
  Instruction_kv3_v2_sb = 576,
  Instruction_kv3_v2_sbfbo = 577,
  Instruction_kv3_v2_sbfcd = 578,
  Instruction_kv3_v2_sbfcd_i = 579,
  Instruction_kv3_v2_sbfd = 580,
  Instruction_kv3_v2_sbfhq = 581,
  Instruction_kv3_v2_sbfsbo = 582,
  Instruction_kv3_v2_sbfsd = 583,
  Instruction_kv3_v2_sbfshq = 584,
  Instruction_kv3_v2_sbfsw = 585,
  Instruction_kv3_v2_sbfswp = 586,
  Instruction_kv3_v2_sbfusbo = 587,
  Instruction_kv3_v2_sbfusd = 588,
  Instruction_kv3_v2_sbfushq = 589,
  Instruction_kv3_v2_sbfusw = 590,
  Instruction_kv3_v2_sbfuswp = 591,
  Instruction_kv3_v2_sbfuwd = 592,
  Instruction_kv3_v2_sbfw = 593,
  Instruction_kv3_v2_sbfwd = 594,
  Instruction_kv3_v2_sbfwp = 595,
  Instruction_kv3_v2_sbfx16bo = 596,
  Instruction_kv3_v2_sbfx16d = 597,
  Instruction_kv3_v2_sbfx16hq = 598,
  Instruction_kv3_v2_sbfx16uwd = 599,
  Instruction_kv3_v2_sbfx16w = 600,
  Instruction_kv3_v2_sbfx16wd = 601,
  Instruction_kv3_v2_sbfx16wp = 602,
  Instruction_kv3_v2_sbfx2bo = 603,
  Instruction_kv3_v2_sbfx2d = 604,
  Instruction_kv3_v2_sbfx2hq = 605,
  Instruction_kv3_v2_sbfx2uwd = 606,
  Instruction_kv3_v2_sbfx2w = 607,
  Instruction_kv3_v2_sbfx2wd = 608,
  Instruction_kv3_v2_sbfx2wp = 609,
  Instruction_kv3_v2_sbfx32d = 610,
  Instruction_kv3_v2_sbfx32uwd = 611,
  Instruction_kv3_v2_sbfx32w = 612,
  Instruction_kv3_v2_sbfx32wd = 613,
  Instruction_kv3_v2_sbfx4bo = 614,
  Instruction_kv3_v2_sbfx4d = 615,
  Instruction_kv3_v2_sbfx4hq = 616,
  Instruction_kv3_v2_sbfx4uwd = 617,
  Instruction_kv3_v2_sbfx4w = 618,
  Instruction_kv3_v2_sbfx4wd = 619,
  Instruction_kv3_v2_sbfx4wp = 620,
  Instruction_kv3_v2_sbfx64d = 621,
  Instruction_kv3_v2_sbfx64uwd = 622,
  Instruction_kv3_v2_sbfx64w = 623,
  Instruction_kv3_v2_sbfx64wd = 624,
  Instruction_kv3_v2_sbfx8bo = 625,
  Instruction_kv3_v2_sbfx8d = 626,
  Instruction_kv3_v2_sbfx8hq = 627,
  Instruction_kv3_v2_sbfx8uwd = 628,
  Instruction_kv3_v2_sbfx8w = 629,
  Instruction_kv3_v2_sbfx8wd = 630,
  Instruction_kv3_v2_sbfx8wp = 631,
  Instruction_kv3_v2_sbmm8 = 632,
  Instruction_kv3_v2_sbmm8d = 633,
  Instruction_kv3_v2_sbmmt8 = 634,
  Instruction_kv3_v2_sbmmt8d = 635,
  Instruction_kv3_v2_scall = 636,
  Instruction_kv3_v2_sd = 637,
  Instruction_kv3_v2_set = 638,
  Instruction_kv3_v2_sh = 639,
  Instruction_kv3_v2_sleep = 640,
  Instruction_kv3_v2_sllbos = 641,
  Instruction_kv3_v2_slld = 642,
  Instruction_kv3_v2_sllhqs = 643,
  Instruction_kv3_v2_sllw = 644,
  Instruction_kv3_v2_sllwps = 645,
  Instruction_kv3_v2_slsbos = 646,
  Instruction_kv3_v2_slsd = 647,
  Instruction_kv3_v2_slshqs = 648,
  Instruction_kv3_v2_slsw = 649,
  Instruction_kv3_v2_slswps = 650,
  Instruction_kv3_v2_slusbos = 651,
  Instruction_kv3_v2_slusd = 652,
  Instruction_kv3_v2_slushqs = 653,
  Instruction_kv3_v2_slusw = 654,
  Instruction_kv3_v2_sluswps = 655,
  Instruction_kv3_v2_so = 656,
  Instruction_kv3_v2_sq = 657,
  Instruction_kv3_v2_srabos = 658,
  Instruction_kv3_v2_srad = 659,
  Instruction_kv3_v2_srahqs = 660,
  Instruction_kv3_v2_sraw = 661,
  Instruction_kv3_v2_srawps = 662,
  Instruction_kv3_v2_srlbos = 663,
  Instruction_kv3_v2_srld = 664,
  Instruction_kv3_v2_srlhqs = 665,
  Instruction_kv3_v2_srlw = 666,
  Instruction_kv3_v2_srlwps = 667,
  Instruction_kv3_v2_srsbos = 668,
  Instruction_kv3_v2_srsd = 669,
  Instruction_kv3_v2_srshqs = 670,
  Instruction_kv3_v2_srsw = 671,
  Instruction_kv3_v2_srswps = 672,
  Instruction_kv3_v2_stop = 673,
  Instruction_kv3_v2_stsud = 674,
  Instruction_kv3_v2_stsuhq = 675,
  Instruction_kv3_v2_stsuw = 676,
  Instruction_kv3_v2_stsuwp = 677,
  Instruction_kv3_v2_sw = 678,
  Instruction_kv3_v2_sxbd = 679,
  Instruction_kv3_v2_sxhd = 680,
  Instruction_kv3_v2_sxlbhq = 681,
  Instruction_kv3_v2_sxlhwp = 682,
  Instruction_kv3_v2_sxmbhq = 683,
  Instruction_kv3_v2_sxmhwp = 684,
  Instruction_kv3_v2_sxwd = 685,
  Instruction_kv3_v2_syncgroup = 686,
  Instruction_kv3_v2_tlbdinval = 687,
  Instruction_kv3_v2_tlbiinval = 688,
  Instruction_kv3_v2_tlbprobe = 689,
  Instruction_kv3_v2_tlbread = 690,
  Instruction_kv3_v2_tlbwrite = 691,
  Instruction_kv3_v2_waitit = 692,
  Instruction_kv3_v2_wfxl = 693,
  Instruction_kv3_v2_wfxm = 694,
  Instruction_kv3_v2_xaccesso = 695,
  Instruction_kv3_v2_xaligno = 696,
  Instruction_kv3_v2_xandno = 697,
  Instruction_kv3_v2_xando = 698,
  Instruction_kv3_v2_xclampwo = 699,
  Instruction_kv3_v2_xcopyo = 700,
  Instruction_kv3_v2_xcopyv = 701,
  Instruction_kv3_v2_xcopyx = 702,
  Instruction_kv3_v2_xeoro = 703,
  Instruction_kv3_v2_xffma44hw = 704,
  Instruction_kv3_v2_xfmaxhx = 705,
  Instruction_kv3_v2_xfminhx = 706,
  Instruction_kv3_v2_xfmma484hw = 707,
  Instruction_kv3_v2_xfnarrow44wh = 708,
  Instruction_kv3_v2_xfscalewo = 709,
  Instruction_kv3_v2_xiorno = 710,
  Instruction_kv3_v2_xioro = 711,
  Instruction_kv3_v2_xlo = 712,
  Instruction_kv3_v2_xmadd44bw0 = 713,
  Instruction_kv3_v2_xmadd44bw1 = 714,
  Instruction_kv3_v2_xmaddifwo = 715,
  Instruction_kv3_v2_xmaddsu44bw0 = 716,
  Instruction_kv3_v2_xmaddsu44bw1 = 717,
  Instruction_kv3_v2_xmaddu44bw0 = 718,
  Instruction_kv3_v2_xmaddu44bw1 = 719,
  Instruction_kv3_v2_xmma4164bw = 720,
  Instruction_kv3_v2_xmma484bw = 721,
  Instruction_kv3_v2_xmmasu4164bw = 722,
  Instruction_kv3_v2_xmmasu484bw = 723,
  Instruction_kv3_v2_xmmau4164bw = 724,
  Instruction_kv3_v2_xmmau484bw = 725,
  Instruction_kv3_v2_xmmaus4164bw = 726,
  Instruction_kv3_v2_xmmaus484bw = 727,
  Instruction_kv3_v2_xmovefd = 728,
  Instruction_kv3_v2_xmovefo = 729,
  Instruction_kv3_v2_xmovefq = 730,
  Instruction_kv3_v2_xmovetd = 731,
  Instruction_kv3_v2_xmovetq = 732,
  Instruction_kv3_v2_xmsbfifwo = 733,
  Instruction_kv3_v2_xmt44d = 734,
  Instruction_kv3_v2_xnando = 735,
  Instruction_kv3_v2_xneoro = 736,
  Instruction_kv3_v2_xnioro = 737,
  Instruction_kv3_v2_xnoro = 738,
  Instruction_kv3_v2_xnxoro = 739,
  Instruction_kv3_v2_xord = 740,
  Instruction_kv3_v2_xorno = 741,
  Instruction_kv3_v2_xoro = 742,
  Instruction_kv3_v2_xorrbod = 743,
  Instruction_kv3_v2_xorrhqd = 744,
  Instruction_kv3_v2_xorrwpd = 745,
  Instruction_kv3_v2_xorw = 746,
  Instruction_kv3_v2_xplb = 747,
  Instruction_kv3_v2_xpld = 748,
  Instruction_kv3_v2_xplh = 749,
  Instruction_kv3_v2_xplo = 750,
  Instruction_kv3_v2_xplq = 751,
  Instruction_kv3_v2_xplw = 752,
  Instruction_kv3_v2_xrecvo = 753,
  Instruction_kv3_v2_xsbmm8dq = 754,
  Instruction_kv3_v2_xsbmmt8dq = 755,
  Instruction_kv3_v2_xsendo = 756,
  Instruction_kv3_v2_xsendrecvo = 757,
  Instruction_kv3_v2_xso = 758,
  Instruction_kv3_v2_xsplatdo = 759,
  Instruction_kv3_v2_xsplatov = 760,
  Instruction_kv3_v2_xsplatox = 761,
  Instruction_kv3_v2_xsx48bw = 762,
  Instruction_kv3_v2_xtrunc48wb = 763,
  Instruction_kv3_v2_xxoro = 764,
  Instruction_kv3_v2_xzx48bw = 765,
  Instruction_kv3_v2_zxbd = 766,
  Instruction_kv3_v2_zxhd = 767,
  Instruction_kv3_v2_zxlbhq = 768,
  Instruction_kv3_v2_zxlhwp = 769,
  Instruction_kv3_v2_zxmbhq = 770,
  Instruction_kv3_v2_zxmhwp = 771,
  Instruction_kv3_v2_zxwd = 772,
  Separator_kv3_v2_comma = 773,
  Separator_kv3_v2_equal = 774,
  Separator_kv3_v2_qmark = 775,
  Separator_kv3_v2_rsbracket = 776,
  Separator_kv3_v2_lsbracket = 777
};

typedef enum {
  Modifier_kv3_v2_exunum_ALU0=0,
  Modifier_kv3_v2_exunum_ALU1=1,
  Modifier_kv3_v2_exunum_MAU=2,
  Modifier_kv3_v2_exunum_LSU=3,
} Modifier_kv3_v2_exunum_values;


extern const char *mod_kv3_v2_exunum[];
extern const char *mod_kv3_v2_scalarcond[];
extern const char *mod_kv3_v2_lsomask[];
extern const char *mod_kv3_v2_lsumask[];
extern const char *mod_kv3_v2_simdcond[];
extern const char *mod_kv3_v2_comparison[];
extern const char *mod_kv3_v2_floatcomp[];
extern const char *mod_kv3_v2_rounding[];
extern const char *mod_kv3_v2_silent[];
extern const char *mod_kv3_v2_variant[];
extern const char *mod_kv3_v2_speculate[];
extern const char *mod_kv3_v2_doscale[];
extern const char *mod_kv3_v2_qindex[];
extern const char *mod_kv3_v2_hindex[];
extern const char *mod_kv3_v2_cachelev[];
extern const char *mod_kv3_v2_coherency[];
extern const char *mod_kv3_v2_boolcas[];
extern const char *mod_kv3_v2_accesses[];
extern const char *mod_kv3_v2_channel[];
extern const char *mod_kv3_v2_conjugate[];
extern const char *mod_kv3_v2_transpose[];
extern const char *mod_kv3_v2_shuffleV[];
extern const char *mod_kv3_v2_shuffleX[];
extern const char *mod_kv3_v2_splat32[];

typedef enum {
  Bundling_kv3_v2_ALL,
  Bundling_kv3_v2_BCU,
  Bundling_kv3_v2_EXT,
  Bundling_kv3_v2_FULL,
  Bundling_kv3_v2_FULL_X,
  Bundling_kv3_v2_FULL_Y,
  Bundling_kv3_v2_LITE,
  Bundling_kv3_v2_LITE_X,
  Bundling_kv3_v2_LITE_Y,
  Bundling_kv3_v2_MAU,
  Bundling_kv3_v2_MAU_X,
  Bundling_kv3_v2_MAU_Y,
  Bundling_kv3_v2_LSU,
  Bundling_kv3_v2_LSU_X,
  Bundling_kv3_v2_LSU_Y,
  Bundling_kv3_v2_TINY,
  Bundling_kv3_v2_TINY_X,
  Bundling_kv3_v2_TINY_Y,
  Bundling_kv3_v2_NOP,
} Bundling_kv3_v2;

static int ATTRIBUTE_UNUSED
kv3_v2_base_bundling(int bundling) {
  static int base_bundlings[] = {
    Bundling_kv3_v2_ALL,	// Bundling_kv3_v2_ALL
    Bundling_kv3_v2_BCU,	// Bundling_kv3_v2_BCU
    Bundling_kv3_v2_EXT,	// Bundling_kv3_v2_EXT
    Bundling_kv3_v2_FULL,	// Bundling_kv3_v2_FULL
    Bundling_kv3_v2_FULL,	// Bundling_kv3_v2_FULL_X
    Bundling_kv3_v2_FULL,	// Bundling_kv3_v2_FULL_Y
    Bundling_kv3_v2_LITE,	// Bundling_kv3_v2_LITE
    Bundling_kv3_v2_LITE,	// Bundling_kv3_v2_LITE_X
    Bundling_kv3_v2_LITE,	// Bundling_kv3_v2_LITE_Y
    Bundling_kv3_v2_MAU,	// Bundling_kv3_v2_MAU
    Bundling_kv3_v2_MAU,	// Bundling_kv3_v2_MAU_X
    Bundling_kv3_v2_MAU,	// Bundling_kv3_v2_MAU_Y
    Bundling_kv3_v2_LSU,	// Bundling_kv3_v2_LSU
    Bundling_kv3_v2_LSU,	// Bundling_kv3_v2_LSU_X
    Bundling_kv3_v2_LSU,	// Bundling_kv3_v2_LSU_Y
    Bundling_kv3_v2_TINY,	// Bundling_kv3_v2_TINY
    Bundling_kv3_v2_TINY,	// Bundling_kv3_v2_TINY_X
    Bundling_kv3_v2_TINY,	// Bundling_kv3_v2_TINY_Y
    Bundling_kv3_v2_NOP,	// Bundling_kv3_v2_NOP
  };
  return base_bundlings[bundling];
};

typedef enum {
  Resource_kv3_v2_ISSUE,
  Resource_kv3_v2_TINY,
  Resource_kv3_v2_LITE,
  Resource_kv3_v2_FULL,
  Resource_kv3_v2_LSU,
  Resource_kv3_v2_MAU,
  Resource_kv3_v2_BCU,
  Resource_kv3_v2_EXT,
  Resource_kv3_v2_AUXR,
  Resource_kv3_v2_AUXW,
  Resource_kv3_v2_XFER,
  Resource_kv3_v2_MEMW,
  Resource_kv3_v2_SR12,
  Resource_kv3_v2_SR13,
  Resource_kv3_v2_SR14,
  Resource_kv3_v2_SR15,
} Resource_kv3_v2;
#define kv3_v2_RESOURCE_COUNT 16

typedef enum {
  Reservation_kv3_v2_ALL,
  Reservation_kv3_v2_ALU_TINY,
  Reservation_kv3_v2_ALU_TINY_X,
  Reservation_kv3_v2_ALU_TINY_Y,
  Reservation_kv3_v2_ALU_TINY_CRRP,
  Reservation_kv3_v2_ALU_TINY_CRWL_CRWH,
  Reservation_kv3_v2_ALU_TINY_CRWL_CRWH_X,
  Reservation_kv3_v2_ALU_TINY_CRWL_CRWH_Y,
  Reservation_kv3_v2_ALU_TINY_CRRP_CRWL_CRWH,
  Reservation_kv3_v2_ALU_TINY_CRWL,
  Reservation_kv3_v2_ALU_TINY_CRWH,
  Reservation_kv3_v2_ALU_NOP,
  Reservation_kv3_v2_ALU_LITE,
  Reservation_kv3_v2_ALU_LITE_X,
  Reservation_kv3_v2_ALU_LITE_Y,
  Reservation_kv3_v2_ALU_LITE_CRWL,
  Reservation_kv3_v2_ALU_LITE_CRWH,
  Reservation_kv3_v2_ALU_FULL,
  Reservation_kv3_v2_ALU_FULL_X,
  Reservation_kv3_v2_ALU_FULL_Y,
  Reservation_kv3_v2_BCU,
  Reservation_kv3_v2_BCU_XFER,
  Reservation_kv3_v2_BCU_CRRP_CRWL_CRWH,
  Reservation_kv3_v2_BCU_TINY_AUXW_CRRP,
  Reservation_kv3_v2_BCU_TINY_TINY_MAU_XNOP,
  Reservation_kv3_v2_EXT,
  Reservation_kv3_v2_LSU,
  Reservation_kv3_v2_LSU_X,
  Reservation_kv3_v2_LSU_Y,
  Reservation_kv3_v2_LSU_CRRP,
  Reservation_kv3_v2_LSU_CRRP_X,
  Reservation_kv3_v2_LSU_CRRP_Y,
  Reservation_kv3_v2_LSU_AUXR,
  Reservation_kv3_v2_LSU_AUXR_X,
  Reservation_kv3_v2_LSU_AUXR_Y,
  Reservation_kv3_v2_LSU_AUXW,
  Reservation_kv3_v2_LSU_AUXW_X,
  Reservation_kv3_v2_LSU_AUXW_Y,
  Reservation_kv3_v2_LSU_AUXR_AUXW,
  Reservation_kv3_v2_LSU_AUXR_AUXW_X,
  Reservation_kv3_v2_LSU_AUXR_AUXW_Y,
  Reservation_kv3_v2_MAU,
  Reservation_kv3_v2_MAU_X,
  Reservation_kv3_v2_MAU_Y,
  Reservation_kv3_v2_MAU_AUXR,
  Reservation_kv3_v2_MAU_AUXR_X,
  Reservation_kv3_v2_MAU_AUXR_Y,
} Reservation_kv3_v2;

extern struct kvx_reloc kv3_v2_rel16_reloc;
extern struct kvx_reloc kv3_v2_rel32_reloc;
extern struct kvx_reloc kv3_v2_rel64_reloc;
extern struct kvx_reloc kv3_v2_pcrel_signed16_reloc;
extern struct kvx_reloc kv3_v2_pcrel17_reloc;
extern struct kvx_reloc kv3_v2_pcrel27_reloc;
extern struct kvx_reloc kv3_v2_pcrel32_reloc;
extern struct kvx_reloc kv3_v2_pcrel_signed37_reloc;
extern struct kvx_reloc kv3_v2_pcrel_signed43_reloc;
extern struct kvx_reloc kv3_v2_pcrel_signed64_reloc;
extern struct kvx_reloc kv3_v2_pcrel64_reloc;
extern struct kvx_reloc kv3_v2_signed16_reloc;
extern struct kvx_reloc kv3_v2_signed32_reloc;
extern struct kvx_reloc kv3_v2_signed37_reloc;
extern struct kvx_reloc kv3_v2_gotoff_signed37_reloc;
extern struct kvx_reloc kv3_v2_gotoff_signed43_reloc;
extern struct kvx_reloc kv3_v2_gotoff_32_reloc;
extern struct kvx_reloc kv3_v2_gotoff_64_reloc;
extern struct kvx_reloc kv3_v2_got_32_reloc;
extern struct kvx_reloc kv3_v2_got_signed37_reloc;
extern struct kvx_reloc kv3_v2_got_signed43_reloc;
extern struct kvx_reloc kv3_v2_got_64_reloc;
extern struct kvx_reloc kv3_v2_glob_dat_reloc;
extern struct kvx_reloc kv3_v2_copy_reloc;
extern struct kvx_reloc kv3_v2_jump_slot_reloc;
extern struct kvx_reloc kv3_v2_relative_reloc;
extern struct kvx_reloc kv3_v2_signed43_reloc;
extern struct kvx_reloc kv3_v2_signed64_reloc;
extern struct kvx_reloc kv3_v2_gotaddr_signed37_reloc;
extern struct kvx_reloc kv3_v2_gotaddr_signed43_reloc;
extern struct kvx_reloc kv3_v2_gotaddr_signed64_reloc;
extern struct kvx_reloc kv3_v2_dtpmod64_reloc;
extern struct kvx_reloc kv3_v2_dtpoff64_reloc;
extern struct kvx_reloc kv3_v2_dtpoff_signed37_reloc;
extern struct kvx_reloc kv3_v2_dtpoff_signed43_reloc;
extern struct kvx_reloc kv3_v2_tlsgd_signed37_reloc;
extern struct kvx_reloc kv3_v2_tlsgd_signed43_reloc;
extern struct kvx_reloc kv3_v2_tlsld_signed37_reloc;
extern struct kvx_reloc kv3_v2_tlsld_signed43_reloc;
extern struct kvx_reloc kv3_v2_tpoff64_reloc;
extern struct kvx_reloc kv3_v2_tlsie_signed37_reloc;
extern struct kvx_reloc kv3_v2_tlsie_signed43_reloc;
extern struct kvx_reloc kv3_v2_tlsle_signed37_reloc;
extern struct kvx_reloc kv3_v2_tlsle_signed43_reloc;
extern struct kvx_reloc kv3_v2_rel8_reloc;

#define KV4_V1_REGFILE_FIRST_SFR KVX_REGFILE_FIRST_SFR
#define KV4_V1_REGFILE_LAST_SFR KVX_REGFILE_LAST_SFR
#define KV4_V1_REGFILE_DEC_SFR KVX_REGFILE_DEC_SFR
#define KV4_V1_REGFILE_FIRST_GPR KVX_REGFILE_FIRST_GPR
#define KV4_V1_REGFILE_LAST_GPR KVX_REGFILE_LAST_GPR
#define KV4_V1_REGFILE_DEC_GPR KVX_REGFILE_DEC_GPR
#define KV4_V1_REGFILE_FIRST_CSR 6
#define KV4_V1_REGFILE_LAST_CSR 7
#define KV4_V1_REGFILE_DEC_CSR 8
#define KV4_V1_REGFILE_FIRST_PGR 9
#define KV4_V1_REGFILE_LAST_PGR 10
#define KV4_V1_REGFILE_DEC_PGR 11
#define KV4_V1_REGFILE_FIRST_QGR 12
#define KV4_V1_REGFILE_LAST_QGR 13
#define KV4_V1_REGFILE_DEC_QGR 14
#define KV4_V1_REGFILE_FIRST_RV_BIR 15
#define KV4_V1_REGFILE_LAST_RV_BIR 16
#define KV4_V1_REGFILE_DEC_RV_BIR 17
#define KV4_V1_REGFILE_FIRST_RV_BIRP 18
#define KV4_V1_REGFILE_LAST_RV_BIRP 19
#define KV4_V1_REGFILE_DEC_RV_BIRP 20
#define KV4_V1_REGFILE_FIRST_RV_FPR 21
#define KV4_V1_REGFILE_LAST_RV_FPR 22
#define KV4_V1_REGFILE_DEC_RV_FPR 23
#define KV4_V1_REGFILE_FIRST_X16R 24
#define KV4_V1_REGFILE_LAST_X16R 25
#define KV4_V1_REGFILE_DEC_X16R 26
#define KV4_V1_REGFILE_FIRST_X2R 27
#define KV4_V1_REGFILE_LAST_X2R 28
#define KV4_V1_REGFILE_DEC_X2R 29
#define KV4_V1_REGFILE_FIRST_X32R 30
#define KV4_V1_REGFILE_LAST_X32R 31
#define KV4_V1_REGFILE_DEC_X32R 32
#define KV4_V1_REGFILE_FIRST_X4R 33
#define KV4_V1_REGFILE_LAST_X4R 34
#define KV4_V1_REGFILE_DEC_X4R 35
#define KV4_V1_REGFILE_FIRST_X64R 36
#define KV4_V1_REGFILE_LAST_X64R 37
#define KV4_V1_REGFILE_DEC_X64R 38
#define KV4_V1_REGFILE_FIRST_X8R 39
#define KV4_V1_REGFILE_LAST_X8R 40
#define KV4_V1_REGFILE_DEC_X8R 41
#define KV4_V1_REGFILE_FIRST_XBR 42
#define KV4_V1_REGFILE_LAST_XBR 43
#define KV4_V1_REGFILE_DEC_XBR 44
#define KV4_V1_REGFILE_FIRST_XCR 45
#define KV4_V1_REGFILE_LAST_XCR 46
#define KV4_V1_REGFILE_DEC_XCR 47
#define KV4_V1_REGFILE_FIRST_XMR 48
#define KV4_V1_REGFILE_LAST_XMR 49
#define KV4_V1_REGFILE_DEC_XMR 50
#define KV4_V1_REGFILE_FIRST_XTR 51
#define KV4_V1_REGFILE_LAST_XTR 52
#define KV4_V1_REGFILE_DEC_XTR 53
#define KV4_V1_REGFILE_FIRST_XVR 54
#define KV4_V1_REGFILE_LAST_XVR 55
#define KV4_V1_REGFILE_DEC_XVR 56
#define KV4_V1_REGFILE_REGISTERS 57
#define KV4_V1_REGFILE_DEC_REGISTERS 58


extern int kv4_v1_regfiles[];
extern const char **kv4_v1_modifiers[];
extern struct kvx_register kv4_v1_registers[];

extern int kv4_v1_dec_registers[];

enum Method_kv4_v1_enum {
  Immediate_kv4_v1_brknumber = 1,
  Immediate_kv4_v1_pcrel11s2 = 2,
  Immediate_kv4_v1_pcrel12s1 = 3,
  Immediate_kv4_v1_pcrel17s2 = 4,
  Immediate_kv4_v1_pcrel20s1 = 5,
  Immediate_kv4_v1_pcrel27s2 = 6,
  Immediate_kv4_v1_pcrel38s2 = 7,
  Immediate_kv4_v1_pcrel44s2 = 8,
  Immediate_kv4_v1_pcrel54s2 = 9,
  Immediate_kv4_v1_signed10 = 10,
  Immediate_kv4_v1_signed12 = 11,
  Immediate_kv4_v1_signed16 = 12,
  Immediate_kv4_v1_signed20 = 13,
  Immediate_kv4_v1_signed27 = 14,
  Immediate_kv4_v1_signed37 = 15,
  Immediate_kv4_v1_signed43 = 16,
  Immediate_kv4_v1_signed54 = 17,
  Immediate_kv4_v1_signed6 = 18,
  Immediate_kv4_v1_sysnumber = 19,
  Immediate_kv4_v1_unsigned5 = 20,
  Immediate_kv4_v1_unsigned6 = 21,
  Immediate_kv4_v1_wrapped32 = 22,
  Immediate_kv4_v1_wrapped64 = 23,
  Immediate_kv4_v1_wrapped8 = 24,
  Modifier_kv4_v1_accesses = 25,
  Modifier_kv4_v1_acqrel = 26,
  Modifier_kv4_v1_bcucond = 27,
  Modifier_kv4_v1_boolcas = 28,
  Modifier_kv4_v1_cachelev = 29,
  Modifier_kv4_v1_ccbcomp = 30,
  Modifier_kv4_v1_channel = 31,
  Modifier_kv4_v1_coherency = 32,
  Modifier_kv4_v1_conjugate = 33,
  Modifier_kv4_v1_doscale = 34,
  Modifier_kv4_v1_exunum = 35,
  Modifier_kv4_v1_floatcomp = 36,
  Modifier_kv4_v1_floatmode = 37,
  Modifier_kv4_v1_fnegate = 38,
  Modifier_kv4_v1_froundmode = 39,
  Modifier_kv4_v1_highmult = 40,
  Modifier_kv4_v1_hindex = 41,
  Modifier_kv4_v1_imultiply = 42,
  Modifier_kv4_v1_intcomp = 43,
  Modifier_kv4_v1_lanecond = 44,
  Modifier_kv4_v1_lanesize = 45,
  Modifier_kv4_v1_lanetodo = 46,
  Modifier_kv4_v1_mostsig = 47,
  Modifier_kv4_v1_oddlanes = 48,
  Modifier_kv4_v1_ordering = 49,
  Modifier_kv4_v1_qindex = 50,
  Modifier_kv4_v1_realimag = 51,
  Modifier_kv4_v1_shuffleV = 52,
  Modifier_kv4_v1_shuffleX = 53,
  Modifier_kv4_v1_signextw = 54,
  Modifier_kv4_v1_speculate = 55,
  Modifier_kv4_v1_splat32 = 56,
  Modifier_kv4_v1_variant = 57,
  Modifier_kv4_v1_widemult = 58,
  Modifier_kv4_v1_ziplanes = 59,
  RegClass_kv4_v1_aloneReg = 60,
  RegClass_kv4_v1_buffer16Reg = 61,
  RegClass_kv4_v1_buffer2Reg = 62,
  RegClass_kv4_v1_buffer32Reg = 63,
  RegClass_kv4_v1_buffer4Reg = 64,
  RegClass_kv4_v1_buffer64Reg = 65,
  RegClass_kv4_v1_buffer8Reg = 66,
  RegClass_kv4_v1_csReg = 67,
  RegClass_kv4_v1_floatReg = 68,
  RegClass_kv4_v1_mainReg = 69,
  RegClass_kv4_v1_mainRegPair = 70,
  RegClass_kv4_v1_onlyfxReg = 71,
  RegClass_kv4_v1_onlygetReg = 72,
  RegClass_kv4_v1_onlyraReg = 73,
  RegClass_kv4_v1_onlysetReg = 74,
  RegClass_kv4_v1_onlyswapReg = 75,
  RegClass_kv4_v1_pairedReg = 76,
  RegClass_kv4_v1_quadReg = 77,
  RegClass_kv4_v1_singleReg = 78,
  RegClass_kv4_v1_systemReg = 79,
  RegClass_kv4_v1_worddRegE = 80,
  RegClass_kv4_v1_worddRegO = 81,
  RegClass_kv4_v1_xworddReg = 82,
  RegClass_kv4_v1_xworddReg0M4 = 83,
  RegClass_kv4_v1_xworddReg1M4 = 84,
  RegClass_kv4_v1_xworddReg2M4 = 85,
  RegClass_kv4_v1_xworddReg3M4 = 86,
  RegClass_kv4_v1_xwordoReg = 87,
  RegClass_kv4_v1_xwordqReg = 88,
  RegClass_kv4_v1_xwordqRegE = 89,
  RegClass_kv4_v1_xwordqRegO = 90,
  RegClass_kv4_v1_xwordvReg = 91,
  RegClass_kv4_v1_xwordxReg = 92,
  Instruction_kv4_v1_abdbo = 93,
  Instruction_kv4_v1_abdd = 94,
  Instruction_kv4_v1_abdhq = 95,
  Instruction_kv4_v1_abdsbo = 96,
  Instruction_kv4_v1_abdsd = 97,
  Instruction_kv4_v1_abdshq = 98,
  Instruction_kv4_v1_abdsw = 99,
  Instruction_kv4_v1_abdswp = 100,
  Instruction_kv4_v1_abdubo = 101,
  Instruction_kv4_v1_abdud = 102,
  Instruction_kv4_v1_abduhq = 103,
  Instruction_kv4_v1_abduw = 104,
  Instruction_kv4_v1_abduwp = 105,
  Instruction_kv4_v1_abdw = 106,
  Instruction_kv4_v1_abdwp = 107,
  Instruction_kv4_v1_absbo = 108,
  Instruction_kv4_v1_absd = 109,
  Instruction_kv4_v1_abshq = 110,
  Instruction_kv4_v1_abssbo = 111,
  Instruction_kv4_v1_abssd = 112,
  Instruction_kv4_v1_absshq = 113,
  Instruction_kv4_v1_abssw = 114,
  Instruction_kv4_v1_absswp = 115,
  Instruction_kv4_v1_absw = 116,
  Instruction_kv4_v1_abswp = 117,
  Instruction_kv4_v1_acswapb = 118,
  Instruction_kv4_v1_acswapd = 119,
  Instruction_kv4_v1_acswaph = 120,
  Instruction_kv4_v1_acswapq = 121,
  Instruction_kv4_v1_acswapw = 122,
  Instruction_kv4_v1_add = 123,
  Instruction_kv4_v1_add_uw = 124,
  Instruction_kv4_v1_addbo = 125,
  Instruction_kv4_v1_addd = 126,
  Instruction_kv4_v1_addhq = 127,
  Instruction_kv4_v1_addi = 128,
  Instruction_kv4_v1_addiw = 129,
  Instruction_kv4_v1_addq = 130,
  Instruction_kv4_v1_addsbo = 131,
  Instruction_kv4_v1_addsd = 132,
  Instruction_kv4_v1_addshq = 133,
  Instruction_kv4_v1_addsw = 134,
  Instruction_kv4_v1_addswp = 135,
  Instruction_kv4_v1_addusbo = 136,
  Instruction_kv4_v1_addusd = 137,
  Instruction_kv4_v1_addushq = 138,
  Instruction_kv4_v1_addusw = 139,
  Instruction_kv4_v1_adduswp = 140,
  Instruction_kv4_v1_addw = 141,
  Instruction_kv4_v1_addwp = 142,
  Instruction_kv4_v1_addx16bo = 143,
  Instruction_kv4_v1_addx16d = 144,
  Instruction_kv4_v1_addx16hq = 145,
  Instruction_kv4_v1_addx16w = 146,
  Instruction_kv4_v1_addx16wp = 147,
  Instruction_kv4_v1_addx2bo = 148,
  Instruction_kv4_v1_addx2d = 149,
  Instruction_kv4_v1_addx2hq = 150,
  Instruction_kv4_v1_addx2w = 151,
  Instruction_kv4_v1_addx2wp = 152,
  Instruction_kv4_v1_addx32d = 153,
  Instruction_kv4_v1_addx32w = 154,
  Instruction_kv4_v1_addx4bo = 155,
  Instruction_kv4_v1_addx4d = 156,
  Instruction_kv4_v1_addx4hq = 157,
  Instruction_kv4_v1_addx4w = 158,
  Instruction_kv4_v1_addx4wp = 159,
  Instruction_kv4_v1_addx64d = 160,
  Instruction_kv4_v1_addx64w = 161,
  Instruction_kv4_v1_addx8bo = 162,
  Instruction_kv4_v1_addx8d = 163,
  Instruction_kv4_v1_addx8hq = 164,
  Instruction_kv4_v1_addx8w = 165,
  Instruction_kv4_v1_addx8wp = 166,
  Instruction_kv4_v1_aladdb = 167,
  Instruction_kv4_v1_aladdd = 168,
  Instruction_kv4_v1_aladdh = 169,
  Instruction_kv4_v1_aladdw = 170,
  Instruction_kv4_v1_alandb = 171,
  Instruction_kv4_v1_alandd = 172,
  Instruction_kv4_v1_alandh = 173,
  Instruction_kv4_v1_alandw = 174,
  Instruction_kv4_v1_alb = 175,
  Instruction_kv4_v1_alclrb = 176,
  Instruction_kv4_v1_alclrd = 177,
  Instruction_kv4_v1_alclrh = 178,
  Instruction_kv4_v1_alclrw = 179,
  Instruction_kv4_v1_ald = 180,
  Instruction_kv4_v1_aldusb = 181,
  Instruction_kv4_v1_aldusd = 182,
  Instruction_kv4_v1_aldush = 183,
  Instruction_kv4_v1_aldusw = 184,
  Instruction_kv4_v1_aleorb = 185,
  Instruction_kv4_v1_aleord = 186,
  Instruction_kv4_v1_aleorh = 187,
  Instruction_kv4_v1_aleorw = 188,
  Instruction_kv4_v1_alh = 189,
  Instruction_kv4_v1_aliorb = 190,
  Instruction_kv4_v1_aliord = 191,
  Instruction_kv4_v1_aliorh = 192,
  Instruction_kv4_v1_aliorw = 193,
  Instruction_kv4_v1_almaxb = 194,
  Instruction_kv4_v1_almaxd = 195,
  Instruction_kv4_v1_almaxh = 196,
  Instruction_kv4_v1_almaxub = 197,
  Instruction_kv4_v1_almaxud = 198,
  Instruction_kv4_v1_almaxuh = 199,
  Instruction_kv4_v1_almaxuw = 200,
  Instruction_kv4_v1_almaxw = 201,
  Instruction_kv4_v1_alminb = 202,
  Instruction_kv4_v1_almind = 203,
  Instruction_kv4_v1_alminh = 204,
  Instruction_kv4_v1_alminub = 205,
  Instruction_kv4_v1_alminud = 206,
  Instruction_kv4_v1_alminuh = 207,
  Instruction_kv4_v1_alminuw = 208,
  Instruction_kv4_v1_alminw = 209,
  Instruction_kv4_v1_alw = 210,
  Instruction_kv4_v1_amoadd_b = 211,
  Instruction_kv4_v1_amoadd_d = 212,
  Instruction_kv4_v1_amoadd_h = 213,
  Instruction_kv4_v1_amoadd_w = 214,
  Instruction_kv4_v1_amoand_b = 215,
  Instruction_kv4_v1_amoand_d = 216,
  Instruction_kv4_v1_amoand_h = 217,
  Instruction_kv4_v1_amoand_w = 218,
  Instruction_kv4_v1_amocas_b = 219,
  Instruction_kv4_v1_amocas_d = 220,
  Instruction_kv4_v1_amocas_h = 221,
  Instruction_kv4_v1_amocas_q = 222,
  Instruction_kv4_v1_amocas_w = 223,
  Instruction_kv4_v1_amomax_b = 224,
  Instruction_kv4_v1_amomax_d = 225,
  Instruction_kv4_v1_amomax_h = 226,
  Instruction_kv4_v1_amomax_w = 227,
  Instruction_kv4_v1_amomaxu_b = 228,
  Instruction_kv4_v1_amomaxu_d = 229,
  Instruction_kv4_v1_amomaxu_h = 230,
  Instruction_kv4_v1_amomaxu_w = 231,
  Instruction_kv4_v1_amomin_b = 232,
  Instruction_kv4_v1_amomin_d = 233,
  Instruction_kv4_v1_amomin_h = 234,
  Instruction_kv4_v1_amomin_w = 235,
  Instruction_kv4_v1_amominu_b = 236,
  Instruction_kv4_v1_amominu_d = 237,
  Instruction_kv4_v1_amominu_h = 238,
  Instruction_kv4_v1_amominu_w = 239,
  Instruction_kv4_v1_amoor_b = 240,
  Instruction_kv4_v1_amoor_d = 241,
  Instruction_kv4_v1_amoor_h = 242,
  Instruction_kv4_v1_amoor_w = 243,
  Instruction_kv4_v1_amoswap_b = 244,
  Instruction_kv4_v1_amoswap_d = 245,
  Instruction_kv4_v1_amoswap_h = 246,
  Instruction_kv4_v1_amoswap_w = 247,
  Instruction_kv4_v1_amoxor_b = 248,
  Instruction_kv4_v1_amoxor_d = 249,
  Instruction_kv4_v1_amoxor_h = 250,
  Instruction_kv4_v1_amoxor_w = 251,
  Instruction_kv4_v1_and = 252,
  Instruction_kv4_v1_andd = 253,
  Instruction_kv4_v1_andi = 254,
  Instruction_kv4_v1_andn = 255,
  Instruction_kv4_v1_andnd = 256,
  Instruction_kv4_v1_andnq = 257,
  Instruction_kv4_v1_andnw = 258,
  Instruction_kv4_v1_andq = 259,
  Instruction_kv4_v1_andw = 260,
  Instruction_kv4_v1_asaddb = 261,
  Instruction_kv4_v1_asaddd = 262,
  Instruction_kv4_v1_asaddh = 263,
  Instruction_kv4_v1_asaddw = 264,
  Instruction_kv4_v1_asandb = 265,
  Instruction_kv4_v1_asandd = 266,
  Instruction_kv4_v1_asandh = 267,
  Instruction_kv4_v1_asandw = 268,
  Instruction_kv4_v1_asb = 269,
  Instruction_kv4_v1_asd = 270,
  Instruction_kv4_v1_asdusb = 271,
  Instruction_kv4_v1_asdusd = 272,
  Instruction_kv4_v1_asdush = 273,
  Instruction_kv4_v1_asdusw = 274,
  Instruction_kv4_v1_aseorb = 275,
  Instruction_kv4_v1_aseord = 276,
  Instruction_kv4_v1_aseorh = 277,
  Instruction_kv4_v1_aseorw = 278,
  Instruction_kv4_v1_ash = 279,
  Instruction_kv4_v1_asiorb = 280,
  Instruction_kv4_v1_asiord = 281,
  Instruction_kv4_v1_asiorh = 282,
  Instruction_kv4_v1_asiorw = 283,
  Instruction_kv4_v1_asmaxb = 284,
  Instruction_kv4_v1_asmaxd = 285,
  Instruction_kv4_v1_asmaxh = 286,
  Instruction_kv4_v1_asmaxub = 287,
  Instruction_kv4_v1_asmaxud = 288,
  Instruction_kv4_v1_asmaxuh = 289,
  Instruction_kv4_v1_asmaxuw = 290,
  Instruction_kv4_v1_asmaxw = 291,
  Instruction_kv4_v1_asminb = 292,
  Instruction_kv4_v1_asmind = 293,
  Instruction_kv4_v1_asminh = 294,
  Instruction_kv4_v1_asminub = 295,
  Instruction_kv4_v1_asminud = 296,
  Instruction_kv4_v1_asminuh = 297,
  Instruction_kv4_v1_asminuw = 298,
  Instruction_kv4_v1_asminw = 299,
  Instruction_kv4_v1_asw = 300,
  Instruction_kv4_v1_aswapb = 301,
  Instruction_kv4_v1_aswapd = 302,
  Instruction_kv4_v1_aswaph = 303,
  Instruction_kv4_v1_aswapw = 304,
  Instruction_kv4_v1_auipc = 305,
  Instruction_kv4_v1_avgbo = 306,
  Instruction_kv4_v1_avghq = 307,
  Instruction_kv4_v1_avgrbo = 308,
  Instruction_kv4_v1_avgrhq = 309,
  Instruction_kv4_v1_avgrubo = 310,
  Instruction_kv4_v1_avgruhq = 311,
  Instruction_kv4_v1_avgruw = 312,
  Instruction_kv4_v1_avgruwp = 313,
  Instruction_kv4_v1_avgrw = 314,
  Instruction_kv4_v1_avgrwp = 315,
  Instruction_kv4_v1_avgubo = 316,
  Instruction_kv4_v1_avguhq = 317,
  Instruction_kv4_v1_avguw = 318,
  Instruction_kv4_v1_avguwp = 319,
  Instruction_kv4_v1_avgw = 320,
  Instruction_kv4_v1_avgwp = 321,
  Instruction_kv4_v1_await = 322,
  Instruction_kv4_v1_barrier = 323,
  Instruction_kv4_v1_bclr = 324,
  Instruction_kv4_v1_bclri = 325,
  Instruction_kv4_v1_beq = 326,
  Instruction_kv4_v1_beqz = 327,
  Instruction_kv4_v1_bext = 328,
  Instruction_kv4_v1_bexti = 329,
  Instruction_kv4_v1_bge = 330,
  Instruction_kv4_v1_bgeu = 331,
  Instruction_kv4_v1_bgez = 332,
  Instruction_kv4_v1_bgtz = 333,
  Instruction_kv4_v1_binv = 334,
  Instruction_kv4_v1_binvi = 335,
  Instruction_kv4_v1_blend = 336,
  Instruction_kv4_v1_blez = 337,
  Instruction_kv4_v1_blt = 338,
  Instruction_kv4_v1_bltu = 339,
  Instruction_kv4_v1_bltz = 340,
  Instruction_kv4_v1_bne = 341,
  Instruction_kv4_v1_bnez = 342,
  Instruction_kv4_v1_break = 343,
  Instruction_kv4_v1_bset = 344,
  Instruction_kv4_v1_bseti = 345,
  Instruction_kv4_v1_call = 346,
  Instruction_kv4_v1_callx = 347,
  Instruction_kv4_v1_catdq = 348,
  Instruction_kv4_v1_cb = 349,
  Instruction_kv4_v1_cbo_clean = 350,
  Instruction_kv4_v1_cbo_flush = 351,
  Instruction_kv4_v1_cbo_inval = 352,
  Instruction_kv4_v1_cbo_zero = 353,
  Instruction_kv4_v1_cbsd = 354,
  Instruction_kv4_v1_cbshq = 355,
  Instruction_kv4_v1_cbsw = 356,
  Instruction_kv4_v1_cbswp = 357,
  Instruction_kv4_v1_cbx = 358,
  Instruction_kv4_v1_ccb = 359,
  Instruction_kv4_v1_ccbx = 360,
  Instruction_kv4_v1_clmul = 361,
  Instruction_kv4_v1_clmulh = 362,
  Instruction_kv4_v1_clmulr = 363,
  Instruction_kv4_v1_clsd = 364,
  Instruction_kv4_v1_clshq = 365,
  Instruction_kv4_v1_clsw = 366,
  Instruction_kv4_v1_clswp = 367,
  Instruction_kv4_v1_clz = 368,
  Instruction_kv4_v1_clzd = 369,
  Instruction_kv4_v1_clzhq = 370,
  Instruction_kv4_v1_clzw = 371,
  Instruction_kv4_v1_clzwp = 372,
  Instruction_kv4_v1_cmovebo = 373,
  Instruction_kv4_v1_cmoved = 374,
  Instruction_kv4_v1_cmovehq = 375,
  Instruction_kv4_v1_cmoveq = 376,
  Instruction_kv4_v1_cmovewp = 377,
  Instruction_kv4_v1_compbo = 378,
  Instruction_kv4_v1_compd = 379,
  Instruction_kv4_v1_comphq = 380,
  Instruction_kv4_v1_compnbo = 381,
  Instruction_kv4_v1_compnd = 382,
  Instruction_kv4_v1_compnhq = 383,
  Instruction_kv4_v1_compnwp = 384,
  Instruction_kv4_v1_compq = 385,
  Instruction_kv4_v1_compw = 386,
  Instruction_kv4_v1_compwp = 387,
  Instruction_kv4_v1_copyd = 388,
  Instruction_kv4_v1_copyo = 389,
  Instruction_kv4_v1_copyq = 390,
  Instruction_kv4_v1_copyw = 391,
  Instruction_kv4_v1_cpop = 392,
  Instruction_kv4_v1_cpopw = 393,
  Instruction_kv4_v1_crcbellw = 394,
  Instruction_kv4_v1_crcbelmw = 395,
  Instruction_kv4_v1_crclellw = 396,
  Instruction_kv4_v1_crclelmw = 397,
  Instruction_kv4_v1_csrr = 398,
  Instruction_kv4_v1_csrrc = 399,
  Instruction_kv4_v1_csrrci = 400,
  Instruction_kv4_v1_csrrs = 401,
  Instruction_kv4_v1_csrrsi = 402,
  Instruction_kv4_v1_csrrw = 403,
  Instruction_kv4_v1_csrrwi = 404,
  Instruction_kv4_v1_csrw = 405,
  Instruction_kv4_v1_ctz = 406,
  Instruction_kv4_v1_ctzd = 407,
  Instruction_kv4_v1_ctzhq = 408,
  Instruction_kv4_v1_ctzw = 409,
  Instruction_kv4_v1_ctzwp = 410,
  Instruction_kv4_v1_czero_eqz = 411,
  Instruction_kv4_v1_czero_nez = 412,
  Instruction_kv4_v1_d1inval = 413,
  Instruction_kv4_v1_dflushl = 414,
  Instruction_kv4_v1_dflushsw = 415,
  Instruction_kv4_v1_dinvall = 416,
  Instruction_kv4_v1_dinvalsw = 417,
  Instruction_kv4_v1_div = 418,
  Instruction_kv4_v1_divmodd = 419,
  Instruction_kv4_v1_divmodud = 420,
  Instruction_kv4_v1_divmoduw = 421,
  Instruction_kv4_v1_divmodw = 422,
  Instruction_kv4_v1_divu = 423,
  Instruction_kv4_v1_divuw = 424,
  Instruction_kv4_v1_divw = 425,
  Instruction_kv4_v1_dpurgel = 426,
  Instruction_kv4_v1_dpurgesw = 427,
  Instruction_kv4_v1_dtouchl = 428,
  Instruction_kv4_v1_ebreak = 429,
  Instruction_kv4_v1_ecall = 430,
  Instruction_kv4_v1_eord = 431,
  Instruction_kv4_v1_eorq = 432,
  Instruction_kv4_v1_eorw = 433,
  Instruction_kv4_v1_errop = 434,
  Instruction_kv4_v1_extfs = 435,
  Instruction_kv4_v1_extfz = 436,
  Instruction_kv4_v1_extlqbho = 437,
  Instruction_kv4_v1_extlqhwq = 438,
  Instruction_kv4_v1_extlqnbx = 439,
  Instruction_kv4_v1_extlqwdp = 440,
  Instruction_kv4_v1_extlsbho = 441,
  Instruction_kv4_v1_extlshwq = 442,
  Instruction_kv4_v1_extlsnbx = 443,
  Instruction_kv4_v1_extlswdp = 444,
  Instruction_kv4_v1_extlzbho = 445,
  Instruction_kv4_v1_extlzhwq = 446,
  Instruction_kv4_v1_extlznbx = 447,
  Instruction_kv4_v1_extlzwdp = 448,
  Instruction_kv4_v1_fabsd = 449,
  Instruction_kv4_v1_fabsh = 450,
  Instruction_kv4_v1_fabshq = 451,
  Instruction_kv4_v1_fabsw = 452,
  Instruction_kv4_v1_fabswp = 453,
  Instruction_kv4_v1_fadd_d = 454,
  Instruction_kv4_v1_fadd_s = 455,
  Instruction_kv4_v1_faddd = 456,
  Instruction_kv4_v1_faddh = 457,
  Instruction_kv4_v1_faddhq = 458,
  Instruction_kv4_v1_faddw = 459,
  Instruction_kv4_v1_faddwc = 460,
  Instruction_kv4_v1_faddwp = 461,
  Instruction_kv4_v1_fclass_d = 462,
  Instruction_kv4_v1_fclass_s = 463,
  Instruction_kv4_v1_fclassd = 464,
  Instruction_kv4_v1_fclassh = 465,
  Instruction_kv4_v1_fclasshq = 466,
  Instruction_kv4_v1_fclassw = 467,
  Instruction_kv4_v1_fclasswp = 468,
  Instruction_kv4_v1_fcompd = 469,
  Instruction_kv4_v1_fcomph = 470,
  Instruction_kv4_v1_fcomphq = 471,
  Instruction_kv4_v1_fcompnd = 472,
  Instruction_kv4_v1_fcompnhq = 473,
  Instruction_kv4_v1_fcompnwp = 474,
  Instruction_kv4_v1_fcompw = 475,
  Instruction_kv4_v1_fcompwp = 476,
  Instruction_kv4_v1_fcvt_d_l = 477,
  Instruction_kv4_v1_fcvt_d_lu = 478,
  Instruction_kv4_v1_fcvt_d_s = 479,
  Instruction_kv4_v1_fcvt_d_w = 480,
  Instruction_kv4_v1_fcvt_d_wu = 481,
  Instruction_kv4_v1_fcvt_l_d = 482,
  Instruction_kv4_v1_fcvt_l_s = 483,
  Instruction_kv4_v1_fcvt_lu_d = 484,
  Instruction_kv4_v1_fcvt_lu_s = 485,
  Instruction_kv4_v1_fcvt_s_d = 486,
  Instruction_kv4_v1_fcvt_s_l = 487,
  Instruction_kv4_v1_fcvt_s_lu = 488,
  Instruction_kv4_v1_fcvt_s_w = 489,
  Instruction_kv4_v1_fcvt_s_wu = 490,
  Instruction_kv4_v1_fcvt_w_d = 491,
  Instruction_kv4_v1_fcvt_w_s = 492,
  Instruction_kv4_v1_fcvt_wu_d = 493,
  Instruction_kv4_v1_fcvt_wu_s = 494,
  Instruction_kv4_v1_fdiv_d = 495,
  Instruction_kv4_v1_fdiv_s = 496,
  Instruction_kv4_v1_fdivd = 497,
  Instruction_kv4_v1_fdivh = 498,
  Instruction_kv4_v1_fdivw = 499,
  Instruction_kv4_v1_fence = 500,
  Instruction_kv4_v1_fence_i = 501,
  Instruction_kv4_v1_fence_mem = 502,
  Instruction_kv4_v1_feq_d = 503,
  Instruction_kv4_v1_feq_s = 504,
  Instruction_kv4_v1_fextlhwq = 505,
  Instruction_kv4_v1_ffmad = 506,
  Instruction_kv4_v1_ffmah = 507,
  Instruction_kv4_v1_ffmahq = 508,
  Instruction_kv4_v1_ffmaw = 509,
  Instruction_kv4_v1_ffmawc = 510,
  Instruction_kv4_v1_ffmawp = 511,
  Instruction_kv4_v1_ffmsd = 512,
  Instruction_kv4_v1_ffmsh = 513,
  Instruction_kv4_v1_ffmshq = 514,
  Instruction_kv4_v1_ffmsw = 515,
  Instruction_kv4_v1_ffmswp = 516,
  Instruction_kv4_v1_fixedd = 517,
  Instruction_kv4_v1_fixeddw = 518,
  Instruction_kv4_v1_fixedhq = 519,
  Instruction_kv4_v1_fixedud = 520,
  Instruction_kv4_v1_fixedudw = 521,
  Instruction_kv4_v1_fixeduhq = 522,
  Instruction_kv4_v1_fixeduw = 523,
  Instruction_kv4_v1_fixeduwd = 524,
  Instruction_kv4_v1_fixeduwp = 525,
  Instruction_kv4_v1_fixedw = 526,
  Instruction_kv4_v1_fixedwd = 527,
  Instruction_kv4_v1_fixedwp = 528,
  Instruction_kv4_v1_fld = 529,
  Instruction_kv4_v1_fle_d = 530,
  Instruction_kv4_v1_fle_s = 531,
  Instruction_kv4_v1_floatd = 532,
  Instruction_kv4_v1_floatdw = 533,
  Instruction_kv4_v1_floathq = 534,
  Instruction_kv4_v1_floatud = 535,
  Instruction_kv4_v1_floatudw = 536,
  Instruction_kv4_v1_floatuhq = 537,
  Instruction_kv4_v1_floatuw = 538,
  Instruction_kv4_v1_floatuwd = 539,
  Instruction_kv4_v1_floatuwp = 540,
  Instruction_kv4_v1_floatw = 541,
  Instruction_kv4_v1_floatwd = 542,
  Instruction_kv4_v1_floatwp = 543,
  Instruction_kv4_v1_flt_d = 544,
  Instruction_kv4_v1_flt_s = 545,
  Instruction_kv4_v1_flw = 546,
  Instruction_kv4_v1_fmadd_d = 547,
  Instruction_kv4_v1_fmadd_s = 548,
  Instruction_kv4_v1_fmax_d = 549,
  Instruction_kv4_v1_fmax_s = 550,
  Instruction_kv4_v1_fmaxd = 551,
  Instruction_kv4_v1_fmaxh = 552,
  Instruction_kv4_v1_fmaxhq = 553,
  Instruction_kv4_v1_fmaxnd = 554,
  Instruction_kv4_v1_fmaxnh = 555,
  Instruction_kv4_v1_fmaxnhq = 556,
  Instruction_kv4_v1_fmaxnw = 557,
  Instruction_kv4_v1_fmaxnwp = 558,
  Instruction_kv4_v1_fmaxw = 559,
  Instruction_kv4_v1_fmaxwp = 560,
  Instruction_kv4_v1_fmin_d = 561,
  Instruction_kv4_v1_fmin_s = 562,
  Instruction_kv4_v1_fmind = 563,
  Instruction_kv4_v1_fminh = 564,
  Instruction_kv4_v1_fminhq = 565,
  Instruction_kv4_v1_fminnd = 566,
  Instruction_kv4_v1_fminnh = 567,
  Instruction_kv4_v1_fminnhq = 568,
  Instruction_kv4_v1_fminnw = 569,
  Instruction_kv4_v1_fminnwp = 570,
  Instruction_kv4_v1_fminw = 571,
  Instruction_kv4_v1_fminwp = 572,
  Instruction_kv4_v1_fmsub_d = 573,
  Instruction_kv4_v1_fmsub_s = 574,
  Instruction_kv4_v1_fmul_d = 575,
  Instruction_kv4_v1_fmul_s = 576,
  Instruction_kv4_v1_fmuld = 577,
  Instruction_kv4_v1_fmulh = 578,
  Instruction_kv4_v1_fmulhq = 579,
  Instruction_kv4_v1_fmulw = 580,
  Instruction_kv4_v1_fmulwc = 581,
  Instruction_kv4_v1_fmulwp = 582,
  Instruction_kv4_v1_fmv_d_x = 583,
  Instruction_kv4_v1_fmv_w_x = 584,
  Instruction_kv4_v1_fmv_x_d = 585,
  Instruction_kv4_v1_fmv_x_w = 586,
  Instruction_kv4_v1_fnarrowdw = 587,
  Instruction_kv4_v1_fnarrowdwp = 588,
  Instruction_kv4_v1_fnarrowwh = 589,
  Instruction_kv4_v1_fnarrowwhq = 590,
  Instruction_kv4_v1_fnegd = 591,
  Instruction_kv4_v1_fnegh = 592,
  Instruction_kv4_v1_fneghq = 593,
  Instruction_kv4_v1_fnegw = 594,
  Instruction_kv4_v1_fnegwp = 595,
  Instruction_kv4_v1_fnmadd_d = 596,
  Instruction_kv4_v1_fnmadd_s = 597,
  Instruction_kv4_v1_fnmsub_d = 598,
  Instruction_kv4_v1_fnmsub_s = 599,
  Instruction_kv4_v1_fractdwp = 600,
  Instruction_kv4_v1_fracthbo = 601,
  Instruction_kv4_v1_fractwhq = 602,
  Instruction_kv4_v1_frintd = 603,
  Instruction_kv4_v1_frinth = 604,
  Instruction_kv4_v1_frintw = 605,
  Instruction_kv4_v1_fsbfd = 606,
  Instruction_kv4_v1_fsbfh = 607,
  Instruction_kv4_v1_fsbfhq = 608,
  Instruction_kv4_v1_fsbfw = 609,
  Instruction_kv4_v1_fsbfwc = 610,
  Instruction_kv4_v1_fsbfwp = 611,
  Instruction_kv4_v1_fsd = 612,
  Instruction_kv4_v1_fsgnj_d = 613,
  Instruction_kv4_v1_fsgnj_s = 614,
  Instruction_kv4_v1_fsgnjn_d = 615,
  Instruction_kv4_v1_fsgnjn_s = 616,
  Instruction_kv4_v1_fsgnjx_d = 617,
  Instruction_kv4_v1_fsgnjx_s = 618,
  Instruction_kv4_v1_fsignd = 619,
  Instruction_kv4_v1_fsignh = 620,
  Instruction_kv4_v1_fsignhq = 621,
  Instruction_kv4_v1_fsignmd = 622,
  Instruction_kv4_v1_fsignmh = 623,
  Instruction_kv4_v1_fsignmhq = 624,
  Instruction_kv4_v1_fsignmw = 625,
  Instruction_kv4_v1_fsignmwp = 626,
  Instruction_kv4_v1_fsignnd = 627,
  Instruction_kv4_v1_fsignnh = 628,
  Instruction_kv4_v1_fsignnhq = 629,
  Instruction_kv4_v1_fsignnw = 630,
  Instruction_kv4_v1_fsignnwp = 631,
  Instruction_kv4_v1_fsignw = 632,
  Instruction_kv4_v1_fsignwp = 633,
  Instruction_kv4_v1_fsqrt_d = 634,
  Instruction_kv4_v1_fsqrt_s = 635,
  Instruction_kv4_v1_fsqrtd = 636,
  Instruction_kv4_v1_fsqrth = 637,
  Instruction_kv4_v1_fsqrtw = 638,
  Instruction_kv4_v1_fsrecd = 639,
  Instruction_kv4_v1_fsrecw = 640,
  Instruction_kv4_v1_fsrecwp = 641,
  Instruction_kv4_v1_fsrsrd = 642,
  Instruction_kv4_v1_fsrsrw = 643,
  Instruction_kv4_v1_fsrsrwp = 644,
  Instruction_kv4_v1_fsub_d = 645,
  Instruction_kv4_v1_fsub_s = 646,
  Instruction_kv4_v1_fsw = 647,
  Instruction_kv4_v1_fwidenhw = 648,
  Instruction_kv4_v1_fwidenhwq = 649,
  Instruction_kv4_v1_fwidenwd = 650,
  Instruction_kv4_v1_get = 651,
  Instruction_kv4_v1_goto = 652,
  Instruction_kv4_v1_gotox = 653,
  Instruction_kv4_v1_guard = 654,
  Instruction_kv4_v1_i1inval = 655,
  Instruction_kv4_v1_i1invals = 656,
  Instruction_kv4_v1_icall = 657,
  Instruction_kv4_v1_iget = 658,
  Instruction_kv4_v1_igoto = 659,
  Instruction_kv4_v1_insf = 660,
  Instruction_kv4_v1_iord = 661,
  Instruction_kv4_v1_iornd = 662,
  Instruction_kv4_v1_iornq = 663,
  Instruction_kv4_v1_iornw = 664,
  Instruction_kv4_v1_iorq = 665,
  Instruction_kv4_v1_iorw = 666,
  Instruction_kv4_v1_j = 667,
  Instruction_kv4_v1_jal = 668,
  Instruction_kv4_v1_jalr = 669,
  Instruction_kv4_v1_jr = 670,
  Instruction_kv4_v1_kv_lq = 671,
  Instruction_kv4_v1_kv_sq = 672,
  Instruction_kv4_v1_landd = 673,
  Instruction_kv4_v1_landw = 674,
  Instruction_kv4_v1_lb = 675,
  Instruction_kv4_v1_lbs = 676,
  Instruction_kv4_v1_lbu = 677,
  Instruction_kv4_v1_lbz = 678,
  Instruction_kv4_v1_ld = 679,
  Instruction_kv4_v1_lh = 680,
  Instruction_kv4_v1_lhs = 681,
  Instruction_kv4_v1_lhu = 682,
  Instruction_kv4_v1_lhz = 683,
  Instruction_kv4_v1_li = 684,
  Instruction_kv4_v1_liord = 685,
  Instruction_kv4_v1_liorw = 686,
  Instruction_kv4_v1_lnandd = 687,
  Instruction_kv4_v1_lnandw = 688,
  Instruction_kv4_v1_lniord = 689,
  Instruction_kv4_v1_lniorw = 690,
  Instruction_kv4_v1_lo = 691,
  Instruction_kv4_v1_loopdo = 692,
  Instruction_kv4_v1_lq = 693,
  Instruction_kv4_v1_lr_d = 694,
  Instruction_kv4_v1_lr_w = 695,
  Instruction_kv4_v1_lui = 696,
  Instruction_kv4_v1_lw = 697,
  Instruction_kv4_v1_lws = 698,
  Instruction_kv4_v1_lwu = 699,
  Instruction_kv4_v1_lwz = 700,
  Instruction_kv4_v1_maddbho = 701,
  Instruction_kv4_v1_maddd = 702,
  Instruction_kv4_v1_madddq = 703,
  Instruction_kv4_v1_madddt = 704,
  Instruction_kv4_v1_maddhq = 705,
  Instruction_kv4_v1_maddhwq = 706,
  Instruction_kv4_v1_maddsudt = 707,
  Instruction_kv4_v1_maddsuwd = 708,
  Instruction_kv4_v1_maddudt = 709,
  Instruction_kv4_v1_madduwd = 710,
  Instruction_kv4_v1_maddw = 711,
  Instruction_kv4_v1_maddwd = 712,
  Instruction_kv4_v1_maddwdp = 713,
  Instruction_kv4_v1_maddwp = 714,
  Instruction_kv4_v1_maddxbho = 715,
  Instruction_kv4_v1_maddxhwq = 716,
  Instruction_kv4_v1_maddxwdp = 717,
  Instruction_kv4_v1_make = 718,
  Instruction_kv4_v1_max = 719,
  Instruction_kv4_v1_maxbo = 720,
  Instruction_kv4_v1_maxd = 721,
  Instruction_kv4_v1_maxhq = 722,
  Instruction_kv4_v1_maxu = 723,
  Instruction_kv4_v1_maxubo = 724,
  Instruction_kv4_v1_maxud = 725,
  Instruction_kv4_v1_maxuhq = 726,
  Instruction_kv4_v1_maxuw = 727,
  Instruction_kv4_v1_maxuwp = 728,
  Instruction_kv4_v1_maxw = 729,
  Instruction_kv4_v1_maxwp = 730,
  Instruction_kv4_v1_min = 731,
  Instruction_kv4_v1_minbo = 732,
  Instruction_kv4_v1_mind = 733,
  Instruction_kv4_v1_minhq = 734,
  Instruction_kv4_v1_minu = 735,
  Instruction_kv4_v1_minubo = 736,
  Instruction_kv4_v1_minud = 737,
  Instruction_kv4_v1_minuhq = 738,
  Instruction_kv4_v1_minuw = 739,
  Instruction_kv4_v1_minuwp = 740,
  Instruction_kv4_v1_minw = 741,
  Instruction_kv4_v1_minwp = 742,
  Instruction_kv4_v1_msbfbho = 743,
  Instruction_kv4_v1_msbfd = 744,
  Instruction_kv4_v1_msbfdq = 745,
  Instruction_kv4_v1_msbfdt = 746,
  Instruction_kv4_v1_msbfhq = 747,
  Instruction_kv4_v1_msbfhwq = 748,
  Instruction_kv4_v1_msbfsudt = 749,
  Instruction_kv4_v1_msbfsuwd = 750,
  Instruction_kv4_v1_msbfudt = 751,
  Instruction_kv4_v1_msbfuwd = 752,
  Instruction_kv4_v1_msbfw = 753,
  Instruction_kv4_v1_msbfwd = 754,
  Instruction_kv4_v1_msbfwdp = 755,
  Instruction_kv4_v1_msbfwp = 756,
  Instruction_kv4_v1_msbfxbho = 757,
  Instruction_kv4_v1_msbfxhwq = 758,
  Instruction_kv4_v1_msbfxwdp = 759,
  Instruction_kv4_v1_mul = 760,
  Instruction_kv4_v1_mulbho = 761,
  Instruction_kv4_v1_muld = 762,
  Instruction_kv4_v1_muldq = 763,
  Instruction_kv4_v1_muldt = 764,
  Instruction_kv4_v1_mulh = 765,
  Instruction_kv4_v1_mulhq = 766,
  Instruction_kv4_v1_mulhsu = 767,
  Instruction_kv4_v1_mulhu = 768,
  Instruction_kv4_v1_mulhwq = 769,
  Instruction_kv4_v1_mulnbho = 770,
  Instruction_kv4_v1_mulnd = 771,
  Instruction_kv4_v1_mulndq = 772,
  Instruction_kv4_v1_mulnhq = 773,
  Instruction_kv4_v1_mulnhwq = 774,
  Instruction_kv4_v1_mulnw = 775,
  Instruction_kv4_v1_mulnwd = 776,
  Instruction_kv4_v1_mulnwdp = 777,
  Instruction_kv4_v1_mulnwp = 778,
  Instruction_kv4_v1_mulnxbho = 779,
  Instruction_kv4_v1_mulnxhwq = 780,
  Instruction_kv4_v1_mulnxwdp = 781,
  Instruction_kv4_v1_mulsudt = 782,
  Instruction_kv4_v1_mulsuwd = 783,
  Instruction_kv4_v1_muludt = 784,
  Instruction_kv4_v1_muluwd = 785,
  Instruction_kv4_v1_mulw = 786,
  Instruction_kv4_v1_mulwd = 787,
  Instruction_kv4_v1_mulwdp = 788,
  Instruction_kv4_v1_mulwp = 789,
  Instruction_kv4_v1_mulxbho = 790,
  Instruction_kv4_v1_mulxhwq = 791,
  Instruction_kv4_v1_mulxwdp = 792,
  Instruction_kv4_v1_mv = 793,
  Instruction_kv4_v1_nandd = 794,
  Instruction_kv4_v1_nandq = 795,
  Instruction_kv4_v1_nandw = 796,
  Instruction_kv4_v1_neg = 797,
  Instruction_kv4_v1_negbo = 798,
  Instruction_kv4_v1_negd = 799,
  Instruction_kv4_v1_neghq = 800,
  Instruction_kv4_v1_negq = 801,
  Instruction_kv4_v1_negsbo = 802,
  Instruction_kv4_v1_negsd = 803,
  Instruction_kv4_v1_negshq = 804,
  Instruction_kv4_v1_negsw = 805,
  Instruction_kv4_v1_negswp = 806,
  Instruction_kv4_v1_negw = 807,
  Instruction_kv4_v1_negwp = 808,
  Instruction_kv4_v1_neord = 809,
  Instruction_kv4_v1_neorq = 810,
  Instruction_kv4_v1_neorw = 811,
  Instruction_kv4_v1_niord = 812,
  Instruction_kv4_v1_niorq = 813,
  Instruction_kv4_v1_niorw = 814,
  Instruction_kv4_v1_nop = 815,
  Instruction_kv4_v1_not = 816,
  Instruction_kv4_v1_notd = 817,
  Instruction_kv4_v1_notq = 818,
  Instruction_kv4_v1_notw = 819,
  Instruction_kv4_v1_or = 820,
  Instruction_kv4_v1_orc_b = 821,
  Instruction_kv4_v1_ori = 822,
  Instruction_kv4_v1_orn = 823,
  Instruction_kv4_v1_pcrel = 824,
  Instruction_kv4_v1_prefetch_i = 825,
  Instruction_kv4_v1_prefetch_r = 826,
  Instruction_kv4_v1_prefetch_w = 827,
  Instruction_kv4_v1_rem = 828,
  Instruction_kv4_v1_remu = 829,
  Instruction_kv4_v1_remuw = 830,
  Instruction_kv4_v1_remw = 831,
  Instruction_kv4_v1_ret = 832,
  Instruction_kv4_v1_rev8 = 833,
  Instruction_kv4_v1_rfe = 834,
  Instruction_kv4_v1_rol = 835,
  Instruction_kv4_v1_rold = 836,
  Instruction_kv4_v1_rolw = 837,
  Instruction_kv4_v1_rolwp = 838,
  Instruction_kv4_v1_ror = 839,
  Instruction_kv4_v1_rord = 840,
  Instruction_kv4_v1_rori = 841,
  Instruction_kv4_v1_roriw = 842,
  Instruction_kv4_v1_rorw = 843,
  Instruction_kv4_v1_rorwp = 844,
  Instruction_kv4_v1_rswap = 845,
  Instruction_kv4_v1_sb = 846,
  Instruction_kv4_v1_sbfbo = 847,
  Instruction_kv4_v1_sbfd = 848,
  Instruction_kv4_v1_sbfhq = 849,
  Instruction_kv4_v1_sbfq = 850,
  Instruction_kv4_v1_sbfsbo = 851,
  Instruction_kv4_v1_sbfsd = 852,
  Instruction_kv4_v1_sbfshq = 853,
  Instruction_kv4_v1_sbfsw = 854,
  Instruction_kv4_v1_sbfswp = 855,
  Instruction_kv4_v1_sbfusbo = 856,
  Instruction_kv4_v1_sbfusd = 857,
  Instruction_kv4_v1_sbfushq = 858,
  Instruction_kv4_v1_sbfusw = 859,
  Instruction_kv4_v1_sbfuswp = 860,
  Instruction_kv4_v1_sbfw = 861,
  Instruction_kv4_v1_sbfwp = 862,
  Instruction_kv4_v1_sbmm8 = 863,
  Instruction_kv4_v1_sbmm8d = 864,
  Instruction_kv4_v1_sbmm8eord = 865,
  Instruction_kv4_v1_sbmmt8 = 866,
  Instruction_kv4_v1_sbmmt8d = 867,
  Instruction_kv4_v1_sbmmt8eord = 868,
  Instruction_kv4_v1_sc_d = 869,
  Instruction_kv4_v1_sc_w = 870,
  Instruction_kv4_v1_scall = 871,
  Instruction_kv4_v1_sd = 872,
  Instruction_kv4_v1_seqz = 873,
  Instruction_kv4_v1_set = 874,
  Instruction_kv4_v1_sext_b = 875,
  Instruction_kv4_v1_sext_h = 876,
  Instruction_kv4_v1_sext_w = 877,
  Instruction_kv4_v1_sgtz = 878,
  Instruction_kv4_v1_sh = 879,
  Instruction_kv4_v1_sh1add = 880,
  Instruction_kv4_v1_sh1add_uw = 881,
  Instruction_kv4_v1_sh2add = 882,
  Instruction_kv4_v1_sh2add_uw = 883,
  Instruction_kv4_v1_sh3add = 884,
  Instruction_kv4_v1_sh3add_uw = 885,
  Instruction_kv4_v1_signbo = 886,
  Instruction_kv4_v1_signd = 887,
  Instruction_kv4_v1_signhq = 888,
  Instruction_kv4_v1_signsbo = 889,
  Instruction_kv4_v1_signsd = 890,
  Instruction_kv4_v1_signshq = 891,
  Instruction_kv4_v1_signsw = 892,
  Instruction_kv4_v1_signswp = 893,
  Instruction_kv4_v1_signw = 894,
  Instruction_kv4_v1_signwp = 895,
  Instruction_kv4_v1_sleep = 896,
  Instruction_kv4_v1_sll = 897,
  Instruction_kv4_v1_sllbo = 898,
  Instruction_kv4_v1_slld = 899,
  Instruction_kv4_v1_sllhq = 900,
  Instruction_kv4_v1_slli = 901,
  Instruction_kv4_v1_slli_uw = 902,
  Instruction_kv4_v1_slliw = 903,
  Instruction_kv4_v1_sllw = 904,
  Instruction_kv4_v1_sllwp = 905,
  Instruction_kv4_v1_slsbo = 906,
  Instruction_kv4_v1_slsd = 907,
  Instruction_kv4_v1_slshq = 908,
  Instruction_kv4_v1_slsw = 909,
  Instruction_kv4_v1_slswp = 910,
  Instruction_kv4_v1_slt = 911,
  Instruction_kv4_v1_slti = 912,
  Instruction_kv4_v1_sltiu = 913,
  Instruction_kv4_v1_sltu = 914,
  Instruction_kv4_v1_sltz = 915,
  Instruction_kv4_v1_slusbo = 916,
  Instruction_kv4_v1_slusd = 917,
  Instruction_kv4_v1_slushq = 918,
  Instruction_kv4_v1_slusw = 919,
  Instruction_kv4_v1_sluswp = 920,
  Instruction_kv4_v1_snez = 921,
  Instruction_kv4_v1_so = 922,
  Instruction_kv4_v1_sq = 923,
  Instruction_kv4_v1_sra = 924,
  Instruction_kv4_v1_srabo = 925,
  Instruction_kv4_v1_srad = 926,
  Instruction_kv4_v1_srahq = 927,
  Instruction_kv4_v1_srai = 928,
  Instruction_kv4_v1_sraiw = 929,
  Instruction_kv4_v1_sraw = 930,
  Instruction_kv4_v1_srawp = 931,
  Instruction_kv4_v1_srl = 932,
  Instruction_kv4_v1_srlbo = 933,
  Instruction_kv4_v1_srld = 934,
  Instruction_kv4_v1_srlhq = 935,
  Instruction_kv4_v1_srli = 936,
  Instruction_kv4_v1_srliw = 937,
  Instruction_kv4_v1_srlw = 938,
  Instruction_kv4_v1_srlwp = 939,
  Instruction_kv4_v1_srsbo = 940,
  Instruction_kv4_v1_srsd = 941,
  Instruction_kv4_v1_srshq = 942,
  Instruction_kv4_v1_srsw = 943,
  Instruction_kv4_v1_srswp = 944,
  Instruction_kv4_v1_stop = 945,
  Instruction_kv4_v1_stsud = 946,
  Instruction_kv4_v1_stsuhq = 947,
  Instruction_kv4_v1_stsuw = 948,
  Instruction_kv4_v1_stsuwp = 949,
  Instruction_kv4_v1_sub = 950,
  Instruction_kv4_v1_subw = 951,
  Instruction_kv4_v1_sw = 952,
  Instruction_kv4_v1_sxbd = 953,
  Instruction_kv4_v1_sxhd = 954,
  Instruction_kv4_v1_sxwd = 955,
  Instruction_kv4_v1_syncgroup = 956,
  Instruction_kv4_v1_tlbdinval = 957,
  Instruction_kv4_v1_tlbiinval = 958,
  Instruction_kv4_v1_tlbprobe = 959,
  Instruction_kv4_v1_tlbread = 960,
  Instruction_kv4_v1_tlbwrite = 961,
  Instruction_kv4_v1_truncdwp = 962,
  Instruction_kv4_v1_trunchbo = 963,
  Instruction_kv4_v1_truncwhq = 964,
  Instruction_kv4_v1_waitit = 965,
  Instruction_kv4_v1_wfxl = 966,
  Instruction_kv4_v1_wfxm = 967,
  Instruction_kv4_v1_widenqbho = 968,
  Instruction_kv4_v1_widenqhwq = 969,
  Instruction_kv4_v1_widenqwdp = 970,
  Instruction_kv4_v1_widensbho = 971,
  Instruction_kv4_v1_widenshwq = 972,
  Instruction_kv4_v1_widenswdp = 973,
  Instruction_kv4_v1_widenzbho = 974,
  Instruction_kv4_v1_widenzhwq = 975,
  Instruction_kv4_v1_widenzwdp = 976,
  Instruction_kv4_v1_xaccesso = 977,
  Instruction_kv4_v1_xaligno = 978,
  Instruction_kv4_v1_xcopyo = 979,
  Instruction_kv4_v1_xlo = 980,
  Instruction_kv4_v1_xmovefd = 981,
  Instruction_kv4_v1_xmovefo = 982,
  Instruction_kv4_v1_xmovefq = 983,
  Instruction_kv4_v1_xmovetd = 984,
  Instruction_kv4_v1_xmoveto = 985,
  Instruction_kv4_v1_xmovetq = 986,
  Instruction_kv4_v1_xnor = 987,
  Instruction_kv4_v1_xor = 988,
  Instruction_kv4_v1_xori = 989,
  Instruction_kv4_v1_xplb = 990,
  Instruction_kv4_v1_xpld = 991,
  Instruction_kv4_v1_xplh = 992,
  Instruction_kv4_v1_xplo = 993,
  Instruction_kv4_v1_xplq = 994,
  Instruction_kv4_v1_xplw = 995,
  Instruction_kv4_v1_xso = 996,
  Instruction_kv4_v1_zext_h = 997,
  Instruction_kv4_v1_zxbd = 998,
  Instruction_kv4_v1_zxhd = 999,
  Instruction_kv4_v1_zxwd = 1000,
  Separator_kv4_v1_comma = 1001,
  Separator_kv4_v1_equal = 1002,
  Separator_kv4_v1_qmark = 1003,
  Separator_kv4_v1_rsbracket = 1004,
  Separator_kv4_v1_lsbracket = 1005
};

typedef enum {
  Modifier_kv4_v1_exunum_ALU0=0,
  Modifier_kv4_v1_exunum_ALU1=1,
  Modifier_kv4_v1_exunum_LSU0=2,
  Modifier_kv4_v1_exunum_LSU1=3,
} Modifier_kv4_v1_exunum_values;


extern const char *mod_kv4_v1_exunum[];
extern const char *mod_kv4_v1_ccbcomp[];
extern const char *mod_kv4_v1_bcucond[];
extern const char *mod_kv4_v1_intcomp[];
extern const char *mod_kv4_v1_lanecond[];
extern const char *mod_kv4_v1_lanetodo[];
extern const char *mod_kv4_v1_lanesize[];
extern const char *mod_kv4_v1_floatcomp[];
extern const char *mod_kv4_v1_floatmode[];
extern const char *mod_kv4_v1_signextw[];
extern const char *mod_kv4_v1_highmult[];
extern const char *mod_kv4_v1_widemult[];
extern const char *mod_kv4_v1_mostsig[];
extern const char *mod_kv4_v1_oddlanes[];
extern const char *mod_kv4_v1_ziplanes[];
extern const char *mod_kv4_v1_fnegate[];
extern const char *mod_kv4_v1_variant[];
extern const char *mod_kv4_v1_speculate[];
extern const char *mod_kv4_v1_doscale[];
extern const char *mod_kv4_v1_qindex[];
extern const char *mod_kv4_v1_hindex[];
extern const char *mod_kv4_v1_cachelev[];
extern const char *mod_kv4_v1_coherency[];
extern const char *mod_kv4_v1_boolcas[];
extern const char *mod_kv4_v1_accesses[];
extern const char *mod_kv4_v1_channel[];
extern const char *mod_kv4_v1_conjugate[];
extern const char *mod_kv4_v1_imultiply[];
extern const char *mod_kv4_v1_realimag[];
extern const char *mod_kv4_v1_shuffleV[];
extern const char *mod_kv4_v1_shuffleX[];
extern const char *mod_kv4_v1_splat32[];
extern const char *mod_kv4_v1_ordering[];
extern const char *mod_kv4_v1_acqrel[];
extern const char *mod_kv4_v1_froundmode[];

typedef enum {
  Bundling_kv4_v1_ALL,
  Bundling_kv4_v1_BCU2,
  Bundling_kv4_v1_BCU2_X,
  Bundling_kv4_v1_BCU0,
  Bundling_kv4_v1_BCU,
  Bundling_kv4_v1_FULL,
  Bundling_kv4_v1_FULL_X,
  Bundling_kv4_v1_FULL_Y,
  Bundling_kv4_v1_LITE,
  Bundling_kv4_v1_LITE_X,
  Bundling_kv4_v1_LITE_Y,
  Bundling_kv4_v1_LSU0,
  Bundling_kv4_v1_LSU0_X,
  Bundling_kv4_v1_LSU0_Y,
  Bundling_kv4_v1_LSU,
  Bundling_kv4_v1_LSU_X,
  Bundling_kv4_v1_LSU_Y,
  Bundling_kv4_v1_TINY,
  Bundling_kv4_v1_TINY_X,
  Bundling_kv4_v1_TINY_Y,
  Bundling_kv4_v1_EXT,
  Bundling_kv4_v1_NOP,
} Bundling_kv4_v1;

static int ATTRIBUTE_UNUSED
kv4_v1_base_bundling(int bundling) {
  static int base_bundlings[] = {
    Bundling_kv4_v1_ALL,	// Bundling_kv4_v1_ALL
    Bundling_kv4_v1_BCU2,	// Bundling_kv4_v1_BCU2
    Bundling_kv4_v1_BCU2,	// Bundling_kv4_v1_BCU2_X
    Bundling_kv4_v1_BCU0,	// Bundling_kv4_v1_BCU0
    Bundling_kv4_v1_BCU,	// Bundling_kv4_v1_BCU
    Bundling_kv4_v1_FULL,	// Bundling_kv4_v1_FULL
    Bundling_kv4_v1_FULL,	// Bundling_kv4_v1_FULL_X
    Bundling_kv4_v1_FULL,	// Bundling_kv4_v1_FULL_Y
    Bundling_kv4_v1_LITE,	// Bundling_kv4_v1_LITE
    Bundling_kv4_v1_LITE,	// Bundling_kv4_v1_LITE_X
    Bundling_kv4_v1_LITE,	// Bundling_kv4_v1_LITE_Y
    Bundling_kv4_v1_LSU0,	// Bundling_kv4_v1_LSU0
    Bundling_kv4_v1_LSU0,	// Bundling_kv4_v1_LSU0_X
    Bundling_kv4_v1_LSU0,	// Bundling_kv4_v1_LSU0_Y
    Bundling_kv4_v1_LSU,	// Bundling_kv4_v1_LSU
    Bundling_kv4_v1_LSU,	// Bundling_kv4_v1_LSU_X
    Bundling_kv4_v1_LSU,	// Bundling_kv4_v1_LSU_Y
    Bundling_kv4_v1_TINY,	// Bundling_kv4_v1_TINY
    Bundling_kv4_v1_TINY,	// Bundling_kv4_v1_TINY_X
    Bundling_kv4_v1_TINY,	// Bundling_kv4_v1_TINY_Y
    Bundling_kv4_v1_EXT,	// Bundling_kv4_v1_EXT
    Bundling_kv4_v1_NOP,	// Bundling_kv4_v1_NOP
  };
  return base_bundlings[bundling];
};

typedef enum {
  Resource_kv4_v1_ISSUE,
  Resource_kv4_v1_TINY,
  Resource_kv4_v1_LITE,
  Resource_kv4_v1_FULL,
  Resource_kv4_v1_LSU,
  Resource_kv4_v1_MAU,
  Resource_kv4_v1_BCU,
  Resource_kv4_v1_EXT,
  Resource_kv4_v1_AUXR,
  Resource_kv4_v1_AUXW,
  Resource_kv4_v1_XFER,
  Resource_kv4_v1_MEMW,
  Resource_kv4_v1_SR12,
  Resource_kv4_v1_SR13,
  Resource_kv4_v1_SR14,
  Resource_kv4_v1_SR15,
} Resource_kv4_v1;
#define kv4_v1_RESOURCE_COUNT 16

typedef enum {
  Reservation_kv4_v1_ALL,
  Reservation_kv4_v1_ALU_TINY,
  Reservation_kv4_v1_ALU_TINY_X,
  Reservation_kv4_v1_ALU_TINY_Y,
  Reservation_kv4_v1_ALU_TINY_AUXR,
  Reservation_kv4_v1_ALU_LITE,
  Reservation_kv4_v1_ALU_LITE_X,
  Reservation_kv4_v1_ALU_LITE_Y,
  Reservation_kv4_v1_ALU_LITE_MISC,
  Reservation_kv4_v1_ALU_FULL,
  Reservation_kv4_v1_ALU_FULL_X,
  Reservation_kv4_v1_ALU_FULL_Y,
  Reservation_kv4_v1_BCU,
  Reservation_kv4_v1_BCU_BRRP,
  Reservation_kv4_v1_BCU_BRRP2,
  Reservation_kv4_v1_BCU2,
  Reservation_kv4_v1_BCU2_X,
  Reservation_kv4_v1_BCU_XFER,
  Reservation_kv4_v1_BCU_XFER_BRRP,
  Reservation_kv4_v1_BCU2_TINY_LSU,
  Reservation_kv4_v1_LSU,
  Reservation_kv4_v1_LSU_X,
  Reservation_kv4_v1_LSU_Y,
  Reservation_kv4_v1_LSU_MEMW_ACCR,
  Reservation_kv4_v1_LSU_MEMW_ACCR_X,
  Reservation_kv4_v1_LSU_MEMW_ACCR_Y,
  Reservation_kv4_v1_LSU2_MEMW,
  Reservation_kv4_v1_LSU2_MEMW_X,
  Reservation_kv4_v1_LSU2_MEMW_Y,
  Reservation_kv4_v1_LSU_AUXR,
  Reservation_kv4_v1_LSU_AUXR_X,
  Reservation_kv4_v1_LSU_AUXR_Y,
  Reservation_kv4_v1_LSU_MEMW_AUXR,
  Reservation_kv4_v1_LSU_MEMW_AUXR_X,
  Reservation_kv4_v1_LSU_MEMW_AUXR_Y,
  Reservation_kv4_v1_LSU2_MEMW_AUXR,
  Reservation_kv4_v1_LSU2_MEMW_AUXR_X,
  Reservation_kv4_v1_LSU2_MEMW_AUXR_Y,
  Reservation_kv4_v1_LSU_MEMW_AUXW,
  Reservation_kv4_v1_LSU_MEMW_AUXW_X,
  Reservation_kv4_v1_LSU_MEMW_AUXW_Y,
  Reservation_kv4_v1_LSU2_MEMW_AUXW,
  Reservation_kv4_v1_LSU2_MEMW_AUXW_X,
  Reservation_kv4_v1_LSU2_MEMW_AUXW_Y,
  Reservation_kv4_v1_LSU_AUXW,
  Reservation_kv4_v1_LSU_AUXW_X,
  Reservation_kv4_v1_LSU_AUXW_Y,
  Reservation_kv4_v1_LSU_AUXR_AUXW,
  Reservation_kv4_v1_LSU_AUXR_AUXW_X,
  Reservation_kv4_v1_LSU_AUXR_AUXW_Y,
  Reservation_kv4_v1_LSU2_MEMW_AUXR_AUXW,
  Reservation_kv4_v1_LSU2_MEMW_AUXR_AUXW_X,
  Reservation_kv4_v1_LSU2_MEMW_AUXR_AUXW_Y,
  Reservation_kv4_v1_MAU,
  Reservation_kv4_v1_EXT,
  Reservation_kv4_v1_EXT_COMP,
  Reservation_kv4_v1_EXT_MISC,
  Reservation_kv4_v1_EXT_MISC_AUXW,
} Reservation_kv4_v1;

extern struct kvx_reloc kv4_v1_rel16_reloc;
extern struct kvx_reloc kv4_v1_rel32_reloc;
extern struct kvx_reloc kv4_v1_rel64_reloc;
extern struct kvx_reloc kv4_v1_pcrel_signed16_reloc;
extern struct kvx_reloc kv4_v1_pcrel32_reloc;
extern struct kvx_reloc kv4_v1_pcrel_signed37_reloc;
extern struct kvx_reloc kv4_v1_pcrel_signed43_reloc;
extern struct kvx_reloc kv4_v1_pcrel_signed64_reloc;
extern struct kvx_reloc kv4_v1_pcrel64_reloc;
extern struct kvx_reloc kv4_v1_signed16_reloc;
extern struct kvx_reloc kv4_v1_signed32_reloc;
extern struct kvx_reloc kv4_v1_signed37_reloc;
extern struct kvx_reloc kv4_v1_gotoff_signed37_reloc;
extern struct kvx_reloc kv4_v1_gotoff_signed43_reloc;
extern struct kvx_reloc kv4_v1_gotoff_32_reloc;
extern struct kvx_reloc kv4_v1_gotoff_64_reloc;
extern struct kvx_reloc kv4_v1_got_32_reloc;
extern struct kvx_reloc kv4_v1_got_signed37_reloc;
extern struct kvx_reloc kv4_v1_got_signed43_reloc;
extern struct kvx_reloc kv4_v1_got_64_reloc;
extern struct kvx_reloc kv4_v1_glob_dat_reloc;
extern struct kvx_reloc kv4_v1_copy_reloc;
extern struct kvx_reloc kv4_v1_jump_slot_reloc;
extern struct kvx_reloc kv4_v1_relative_reloc;
extern struct kvx_reloc kv4_v1_signed43_reloc;
extern struct kvx_reloc kv4_v1_signed64_reloc;
extern struct kvx_reloc kv4_v1_gotaddr_signed37_reloc;
extern struct kvx_reloc kv4_v1_gotaddr_signed43_reloc;
extern struct kvx_reloc kv4_v1_gotaddr_signed64_reloc;
extern struct kvx_reloc kv4_v1_dtpmod64_reloc;
extern struct kvx_reloc kv4_v1_dtpoff64_reloc;
extern struct kvx_reloc kv4_v1_dtpoff_signed37_reloc;
extern struct kvx_reloc kv4_v1_dtpoff_signed43_reloc;
extern struct kvx_reloc kv4_v1_tlsgd_signed37_reloc;
extern struct kvx_reloc kv4_v1_tlsgd_signed43_reloc;
extern struct kvx_reloc kv4_v1_tlsld_signed37_reloc;
extern struct kvx_reloc kv4_v1_tlsld_signed43_reloc;
extern struct kvx_reloc kv4_v1_tpoff64_reloc;
extern struct kvx_reloc kv4_v1_tlsie_signed37_reloc;
extern struct kvx_reloc kv4_v1_tlsie_signed43_reloc;
extern struct kvx_reloc kv4_v1_tlsle_signed37_reloc;
extern struct kvx_reloc kv4_v1_tlsle_signed43_reloc;
extern struct kvx_reloc kv4_v1_rel8_reloc;
extern struct kvx_reloc kv4_v1_pcrel11s2_reloc;
extern struct kvx_reloc kv4_v1_pcrel17s2_reloc;
extern struct kvx_reloc kv4_v1_pcrel27s2_reloc;
extern struct kvx_reloc kv4_v1_pcrel38s2_reloc;
extern struct kvx_reloc kv4_v1_pcrel44s2_reloc;
extern struct kvx_reloc kv4_v1_pcrel54s2_reloc;


#endif /* OPCODE_KVX_H */
