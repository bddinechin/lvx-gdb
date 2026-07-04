/* LVX ELF support for BFD.

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

#ifndef _ELF_LVX_H
#define _ELF_LVX_H

#include "elf/reloc-macros.h"

START_RELOC_NUMBERS (elf_lvx_reloc_type)
    RELOC_NUMBER (R_LVX_NONE,                                  0)
    RELOC_NUMBER (R_LVX_16,                                    1)
    RELOC_NUMBER (R_LVX_32,                                    2)
    RELOC_NUMBER (R_LVX_64,                                    3)
    RELOC_NUMBER (R_LVX_S16_PCREL,                             4)
    RELOC_NUMBER (R_LVX_32_PCREL,                              5)
    RELOC_NUMBER (R_LVX_S37_PCREL_LO10,                        6)
    RELOC_NUMBER (R_LVX_S37_PCREL_UP27,                        7)
    RELOC_NUMBER (R_LVX_S43_PCREL_LO10,                        8)
    RELOC_NUMBER (R_LVX_S43_PCREL_UP27,                        9)
    RELOC_NUMBER (R_LVX_S43_PCREL_EX6,                        10)
    RELOC_NUMBER (R_LVX_S64_PCREL_LO10,                       11)
    RELOC_NUMBER (R_LVX_S64_PCREL_UP27,                       12)
    RELOC_NUMBER (R_LVX_S64_PCREL_EX27,                       13)
    RELOC_NUMBER (R_LVX_64_PCREL,                             14)
    RELOC_NUMBER (R_LVX_S16,                                  15)
    RELOC_NUMBER (R_LVX_S32_LO5,                              16)
    RELOC_NUMBER (R_LVX_S32_UP27,                             17)
    RELOC_NUMBER (R_LVX_S37_LO10,                             18)
    RELOC_NUMBER (R_LVX_S37_UP27,                             19)
    RELOC_NUMBER (R_LVX_S37_GOTOFF_LO10,                      20)
    RELOC_NUMBER (R_LVX_S37_GOTOFF_UP27,                      21)
    RELOC_NUMBER (R_LVX_S43_GOTOFF_LO10,                      22)
    RELOC_NUMBER (R_LVX_S43_GOTOFF_UP27,                      23)
    RELOC_NUMBER (R_LVX_S43_GOTOFF_EX6,                       24)
    RELOC_NUMBER (R_LVX_32_GOTOFF,                            25)
    RELOC_NUMBER (R_LVX_64_GOTOFF,                            26)
    RELOC_NUMBER (R_LVX_32_GOT,                               27)
    RELOC_NUMBER (R_LVX_S37_GOT_LO10,                         28)
    RELOC_NUMBER (R_LVX_S37_GOT_UP27,                         29)
    RELOC_NUMBER (R_LVX_S43_GOT_LO10,                         30)
    RELOC_NUMBER (R_LVX_S43_GOT_UP27,                         31)
    RELOC_NUMBER (R_LVX_S43_GOT_EX6,                          32)
    RELOC_NUMBER (R_LVX_64_GOT,                               33)
    RELOC_NUMBER (R_LVX_GLOB_DAT,                             34)
    RELOC_NUMBER (R_LVX_COPY,                                 35)
    RELOC_NUMBER (R_LVX_JMP_SLOT,                             36)
    RELOC_NUMBER (R_LVX_RELATIVE,                             37)
    RELOC_NUMBER (R_LVX_S43_LO10,                             38)
    RELOC_NUMBER (R_LVX_S43_UP27,                             39)
    RELOC_NUMBER (R_LVX_S43_EX6,                              40)
    RELOC_NUMBER (R_LVX_S64_LO10,                             41)
    RELOC_NUMBER (R_LVX_S64_UP27,                             42)
    RELOC_NUMBER (R_LVX_S64_EX27,                             43)
    RELOC_NUMBER (R_LVX_S37_GOTADDR_LO10,                     44)
    RELOC_NUMBER (R_LVX_S37_GOTADDR_UP27,                     45)
    RELOC_NUMBER (R_LVX_S43_GOTADDR_LO10,                     46)
    RELOC_NUMBER (R_LVX_S43_GOTADDR_UP27,                     47)
    RELOC_NUMBER (R_LVX_S43_GOTADDR_EX6,                      48)
    RELOC_NUMBER (R_LVX_S64_GOTADDR_LO10,                     49)
    RELOC_NUMBER (R_LVX_S64_GOTADDR_UP27,                     50)
    RELOC_NUMBER (R_LVX_S64_GOTADDR_EX27,                     51)
    RELOC_NUMBER (R_LVX_64_DTPMOD,                            52)
    RELOC_NUMBER (R_LVX_64_DTPOFF,                            53)
    RELOC_NUMBER (R_LVX_S37_TLS_DTPOFF_LO10,                  54)
    RELOC_NUMBER (R_LVX_S37_TLS_DTPOFF_UP27,                  55)
    RELOC_NUMBER (R_LVX_S43_TLS_DTPOFF_LO10,                  56)
    RELOC_NUMBER (R_LVX_S43_TLS_DTPOFF_UP27,                  57)
    RELOC_NUMBER (R_LVX_S43_TLS_DTPOFF_EX6,                   58)
    RELOC_NUMBER (R_LVX_S37_TLS_GD_LO10,                      59)
    RELOC_NUMBER (R_LVX_S37_TLS_GD_UP27,                      60)
    RELOC_NUMBER (R_LVX_S43_TLS_GD_LO10,                      61)
    RELOC_NUMBER (R_LVX_S43_TLS_GD_UP27,                      62)
    RELOC_NUMBER (R_LVX_S43_TLS_GD_EX6,                       63)
    RELOC_NUMBER (R_LVX_S37_TLS_LD_LO10,                      64)
    RELOC_NUMBER (R_LVX_S37_TLS_LD_UP27,                      65)
    RELOC_NUMBER (R_LVX_S43_TLS_LD_LO10,                      66)
    RELOC_NUMBER (R_LVX_S43_TLS_LD_UP27,                      67)
    RELOC_NUMBER (R_LVX_S43_TLS_LD_EX6,                       68)
    RELOC_NUMBER (R_LVX_64_TPOFF,                             69)
    RELOC_NUMBER (R_LVX_S37_TLS_IE_LO10,                      70)
    RELOC_NUMBER (R_LVX_S37_TLS_IE_UP27,                      71)
    RELOC_NUMBER (R_LVX_S43_TLS_IE_LO10,                      72)
    RELOC_NUMBER (R_LVX_S43_TLS_IE_UP27,                      73)
    RELOC_NUMBER (R_LVX_S43_TLS_IE_EX6,                       74)
    RELOC_NUMBER (R_LVX_S37_TLS_LE_LO10,                      75)
    RELOC_NUMBER (R_LVX_S37_TLS_LE_UP27,                      76)
    RELOC_NUMBER (R_LVX_S43_TLS_LE_LO10,                      77)
    RELOC_NUMBER (R_LVX_S43_TLS_LE_UP27,                      78)
    RELOC_NUMBER (R_LVX_S43_TLS_LE_EX6,                       79)
    RELOC_NUMBER (R_LVX_8,                                    80)
    RELOC_NUMBER (R_LVX_S11S2_PCREL,                          81)
    RELOC_NUMBER (R_LVX_S17S2_PCREL,                          82)
    RELOC_NUMBER (R_LVX_S27S2_PCREL,                          83)
    RELOC_NUMBER (R_LVX_S38S2_PCREL_LO11,                     84)
    RELOC_NUMBER (R_LVX_S38S2_PCREL_UP27,                     85)
    RELOC_NUMBER (R_LVX_S44S2_PCREL_LO17,                     86)
    RELOC_NUMBER (R_LVX_S44S2_PCREL_UP27,                     87)
    RELOC_NUMBER (R_LVX_S54S2_PCREL_LO27,                     88)
    RELOC_NUMBER (R_LVX_S54S2_PCREL_UP27,                     89)
END_RELOC_NUMBERS (R_LVX_end)

#include "lvx_elfids.h"

#endif
