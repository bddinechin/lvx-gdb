/* lvx-dis.h -- Header file lvx-dis.c
   Copyright (C) 2009-2024 Free Software Foundation, Inc.
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

#ifndef _LVX_DIS_H_
#define _LVX_DIS_H_

#include "dis-asm.h"

#define LVX_GPR_REG_SP 12
#define LVX_GPR_REG_FP 14

enum lvx_prologue_epilogue_insn_type
{
  LVX_PROL_EPIL_INSN_SD,
  LVX_PROL_EPIL_INSN_SQ,
  LVX_PROL_EPIL_INSN_SO,
  LVX_PROL_EPIL_INSN_GET_RA,
  LVX_PROL_EPIL_INSN_ADD_FP,
  LVX_PROL_EPIL_INSN_ADD_SP,
  LVX_PROL_EPIL_INSN_RESTORE_SP_FROM_FP,
  LVX_PROL_EPIL_INSN_GOTO,
  LVX_PROL_EPIL_INSN_IGOTO,
  LVX_PROL_EPIL_INSN_CB,
  LVX_PROL_EPIL_INSN_RET,
  LVX_PROL_EPIL_INSN_CALL,
};

struct lvx_prologue_epilogue_insn
{
  enum lvx_prologue_epilogue_insn_type insn_type;
  uint64_t immediate;
  int gpr_reg[3];
  int nb_gprs;
};

struct lvx_prologue_epilogue_bundle
{
  struct lvx_prologue_epilogue_insn insn[6];
  int nb_insn;
};

int decode_prologue_epilogue_bundle (bfd_vma memaddr,
				     struct disassemble_info *info,
				     struct lvx_prologue_epilogue_bundle *pb);

void parse_lvx_dis_option (const char *option);

#endif
