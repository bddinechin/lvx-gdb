/* THIS FILE IS GENERATED.  -*- buffer-read-only: t -*- vi:set ro:
  Original: kvx-linux.xml */

#include "defs.h"
#include "osabi.h"
#include "target-descriptions.h"

struct target_desc *tdesc_kvx_linux;
static void
initialize_tdesc_kvx_linux (void)
{
  target_desc_up result = allocate_target_description ();
  set_tdesc_architecture (result.get (), bfd_scan_arch ("kvx:kv3-1"));

  struct tdesc_feature *feature;

  feature = tdesc_create_feature (result.get (), "eu.kalray.core.kv3-1");
  tdesc_type_with_fields *type_with_fields;
  type_with_fields = tdesc_create_flags (feature, "cs_type", 8);
  tdesc_add_flag (type_with_fields, 0, "ic");
  tdesc_add_flag (type_with_fields, 1, "io");
  tdesc_add_flag (type_with_fields, 2, "dz");
  tdesc_add_flag (type_with_fields, 3, "ov");
  tdesc_add_flag (type_with_fields, 4, "un");
  tdesc_add_flag (type_with_fields, 5, "in");
  tdesc_add_flag (type_with_fields, 9, "xio");
  tdesc_add_flag (type_with_fields, 10, "xdz");
  tdesc_add_flag (type_with_fields, 11, "xov");
  tdesc_add_flag (type_with_fields, 12, "xun");
  tdesc_add_flag (type_with_fields, 13, "xin");
  tdesc_add_bitfield (type_with_fields, "rm", 16, 17);
  tdesc_add_bitfield (type_with_fields, "xrm", 20, 21);
  tdesc_add_flag (type_with_fields, 24, "xmf");
  tdesc_add_bitfield (type_with_fields, "cc", 32, 47);
  tdesc_add_bitfield (type_with_fields, "xdrop", 48, 53);
  tdesc_add_bitfield (type_with_fields, "xpow2", 54, 59);

  tdesc_create_reg (feature, "r0", 0, 1, NULL, 64, "int64");
  tdesc_create_reg (feature, "r1", 1, 1, NULL, 64, "int64");
  tdesc_create_reg (feature, "r2", 2, 1, NULL, 64, "int64");
  tdesc_create_reg (feature, "r3", 3, 1, NULL, 64, "int64");
  tdesc_create_reg (feature, "r4", 4, 1, NULL, 64, "int64");
  tdesc_create_reg (feature, "r5", 5, 1, NULL, 64, "int64");
  tdesc_create_reg (feature, "r6", 6, 1, NULL, 64, "int64");
  tdesc_create_reg (feature, "r7", 7, 1, NULL, 64, "int64");
  tdesc_create_reg (feature, "r8", 8, 1, NULL, 64, "int64");
  tdesc_create_reg (feature, "r9", 9, 1, NULL, 64, "int64");
  tdesc_create_reg (feature, "r10", 10, 1, NULL, 64, "int64");
  tdesc_create_reg (feature, "r11", 11, 1, NULL, 64, "int64");
  tdesc_create_reg (feature, "r12", 12, 1, NULL, 64, "int64");
  tdesc_create_reg (feature, "r13", 13, 1, NULL, 64, "int64");
  tdesc_create_reg (feature, "r14", 14, 1, NULL, 64, "int64");
  tdesc_create_reg (feature, "r15", 15, 1, NULL, 64, "int64");
  tdesc_create_reg (feature, "r16", 16, 1, NULL, 64, "int64");
  tdesc_create_reg (feature, "r17", 17, 1, NULL, 64, "int64");
  tdesc_create_reg (feature, "r18", 18, 1, NULL, 64, "int64");
  tdesc_create_reg (feature, "r19", 19, 1, NULL, 64, "int64");
  tdesc_create_reg (feature, "r20", 20, 1, NULL, 64, "int64");
  tdesc_create_reg (feature, "r21", 21, 1, NULL, 64, "int64");
  tdesc_create_reg (feature, "r22", 22, 1, NULL, 64, "int64");
  tdesc_create_reg (feature, "r23", 23, 1, NULL, 64, "int64");
  tdesc_create_reg (feature, "r24", 24, 1, NULL, 64, "int64");
  tdesc_create_reg (feature, "r25", 25, 1, NULL, 64, "int64");
  tdesc_create_reg (feature, "r26", 26, 1, NULL, 64, "int64");
  tdesc_create_reg (feature, "r27", 27, 1, NULL, 64, "int64");
  tdesc_create_reg (feature, "r28", 28, 1, NULL, 64, "int64");
  tdesc_create_reg (feature, "r29", 29, 1, NULL, 64, "int64");
  tdesc_create_reg (feature, "r30", 30, 1, NULL, 64, "int64");
  tdesc_create_reg (feature, "r31", 31, 1, NULL, 64, "int64");
  tdesc_create_reg (feature, "r32", 32, 1, NULL, 64, "int64");
  tdesc_create_reg (feature, "r33", 33, 1, NULL, 64, "int64");
  tdesc_create_reg (feature, "r34", 34, 1, NULL, 64, "int64");
  tdesc_create_reg (feature, "r35", 35, 1, NULL, 64, "int64");
  tdesc_create_reg (feature, "r36", 36, 1, NULL, 64, "int64");
  tdesc_create_reg (feature, "r37", 37, 1, NULL, 64, "int64");
  tdesc_create_reg (feature, "r38", 38, 1, NULL, 64, "int64");
  tdesc_create_reg (feature, "r39", 39, 1, NULL, 64, "int64");
  tdesc_create_reg (feature, "r40", 40, 1, NULL, 64, "int64");
  tdesc_create_reg (feature, "r41", 41, 1, NULL, 64, "int64");
  tdesc_create_reg (feature, "r42", 42, 1, NULL, 64, "int64");
  tdesc_create_reg (feature, "r43", 43, 1, NULL, 64, "int64");
  tdesc_create_reg (feature, "r44", 44, 1, NULL, 64, "int64");
  tdesc_create_reg (feature, "r45", 45, 1, NULL, 64, "int64");
  tdesc_create_reg (feature, "r46", 46, 1, NULL, 64, "int64");
  tdesc_create_reg (feature, "r47", 47, 1, NULL, 64, "int64");
  tdesc_create_reg (feature, "r48", 48, 1, NULL, 64, "int64");
  tdesc_create_reg (feature, "r49", 49, 1, NULL, 64, "int64");
  tdesc_create_reg (feature, "r50", 50, 1, NULL, 64, "int64");
  tdesc_create_reg (feature, "r51", 51, 1, NULL, 64, "int64");
  tdesc_create_reg (feature, "r52", 52, 1, NULL, 64, "int64");
  tdesc_create_reg (feature, "r53", 53, 1, NULL, 64, "int64");
  tdesc_create_reg (feature, "r54", 54, 1, NULL, 64, "int64");
  tdesc_create_reg (feature, "r55", 55, 1, NULL, 64, "int64");
  tdesc_create_reg (feature, "r56", 56, 1, NULL, 64, "int64");
  tdesc_create_reg (feature, "r57", 57, 1, NULL, 64, "int64");
  tdesc_create_reg (feature, "r58", 58, 1, NULL, 64, "int64");
  tdesc_create_reg (feature, "r59", 59, 1, NULL, 64, "int64");
  tdesc_create_reg (feature, "r60", 60, 1, NULL, 64, "int64");
  tdesc_create_reg (feature, "r61", 61, 1, NULL, 64, "int64");
  tdesc_create_reg (feature, "r62", 62, 1, NULL, 64, "int64");
  tdesc_create_reg (feature, "r63", 63, 1, NULL, 64, "int64");
  tdesc_create_reg (feature, "lc", 64, 1, NULL, 64, "int64");
  tdesc_create_reg (feature, "le", 65, 1, NULL, 64, "code_ptr");
  tdesc_create_reg (feature, "ls", 66, 1, NULL, 64, "code_ptr");
  tdesc_create_reg (feature, "ra", 67, 1, NULL, 64, "code_ptr");
  tdesc_create_reg (feature, "cs", 68, 1, NULL, 64, "cs_type");
  tdesc_create_reg (feature, "pc", 69, 1, NULL, 64, "code_ptr");

  tdesc_kvx_linux = result.release ();
}
