#include "defs.h"

#include <string.h>

#include "gdbarch.h"
#include "gdbtypes.h"
#include "regcache.h"
#include "reggroups.h"
#include "user-regs.h"
#include "target-descriptions.h"
#include "kvx-common-tdep.h"
#include "kvx-mds-wrap-tdep.h"

struct rv_pseudo_desc
{
  const char *name;
  int component;
  const char *component_name;
};

static struct rv_pseudo_desc rv_pseudo_regs[]
  = {{"rv_x0", -1, NULL},   {"rv_x1", -1, "r1"},   {"rv_x2", -1, "r2"},
     {"rv_x3", -1, "r3"},   {"rv_x4", -1, "r4"},   {"rv_x5", -1, "r5"},
     {"rv_x6", -1, "r6"},   {"rv_x7", -1, "r7"},   {"rv_x8", -1, "r8"},
     {"rv_x9", -1, "r9"},   {"rv_x10", -1, "r10"}, {"rv_x11", -1, "r11"},
     {"rv_x12", -1, "r12"}, {"rv_x13", -1, "r13"}, {"rv_x14", -1, "r14"},
     {"rv_x15", -1, "r15"}, {"rv_x16", -1, "r16"}, {"rv_x17", -1, "r17"},
     {"rv_x18", -1, "r18"}, {"rv_x19", -1, "r19"}, {"rv_x20", -1, "r20"},
     {"rv_x21", -1, "r21"}, {"rv_x22", -1, "r22"}, {"rv_x23", -1, "r23"},
     {"rv_x24", -1, "r24"}, {"rv_x25", -1, "r25"}, {"rv_x26", -1, "r26"},
     {"rv_x27", -1, "r27"}, {"rv_x28", -1, "r28"}, {"rv_x29", -1, "r29"},
     {"rv_x30", -1, "r30"}, {"rv_x31", -1, "r31"}, {"rv_zero", -1, NULL},
     {"rv_ra", -1, "r1"},   {"rv_sp", -1, "r2"},   {"rv_gp", -1, "r3"},
     {"rv_tp", -1, "r4"},   {"rv_t0", -1, "r5"},   {"rv_t1", -1, "r6"},
     {"rv_t2", -1, "r7"},   {"rv_s0", -1, "r8"},   {"rv_fp", -1, "r8"},
     {"rv_s1", -1, "r9"},   {"rv_a0", -1, "r10"},  {"rv_a1", -1, "r11"},
     {"rv_a2", -1, "r12"},  {"rv_a3", -1, "r13"},  {"rv_a4", -1, "r14"},
     {"rv_a5", -1, "r15"},  {"rv_a6", -1, "r16"},  {"rv_a7", -1, "r17"},
     {"rv_s2", -1, "r18"},  {"rv_s3", -1, "r19"},  {"rv_s4", -1, "r20"},
     {"rv_s5", -1, "r21"},  {"rv_s6", -1, "r22"},  {"rv_s7", -1, "r23"},
     {"rv_s8", -1, "r24"},  {"rv_s9", -1, "r25"},  {"rv_s10", -1, "r26"},
     {"rv_s11", -1, "r27"}, {"rv_t3", -1, "r28"},  {"rv_t4", -1, "r29"},
     {"rv_t5", -1, "r30"},  {"rv_t6", -1, "r31"}};
static const int rv_num_pseudo_regs = ARRAY_SIZE (rv_pseudo_regs);

static const char *
find_tdesc_arch (struct gdbarch *gdbarch)
{
  const struct target_desc *tdesc;

  if (gdbarch == NULL)
    return "kv3-1";

  tdesc = gdbarch_target_desc (gdbarch);

  if (tdesc == NULL)
    return "kv3-1";

  if (tdesc_find_feature (tdesc, "eu.kalray.core.kv3-1"))
    return "kv3-1";
  if (tdesc_find_feature (tdesc, "eu.kalray.core.kv3-2"))
    return "kv3-2";
  if (tdesc_find_feature (tdesc, "eu.kalray.core.kv4-1"))
    return "kv4-1";

  return "kvx";
}

int
kvx_num_pseudos_wrap (struct gdbarch *gdbarch)
{
  const char *archname = find_tdesc_arch (gdbarch);
  int num_pseudos = kvx_num_pseudos (gdbarch);
  if (strcmp ("kv4-1", archname) == 0)
    num_pseudos += rv_num_pseudo_regs;

  return num_pseudos;
}

static int
kvx_get_rv_num_pseudo (struct gdbarch *gdbarch, int regnr)
{
  const char *archname = find_tdesc_arch (gdbarch);
  int pseudo_num;

  if (strcmp ("kv4-1", archname))
    return -1;

  pseudo_num = regnr - gdbarch_num_regs (gdbarch);
  return pseudo_num - kvx_num_pseudos (gdbarch);
}

const char *
kvx_pseudo_wrap_register_name (struct gdbarch *gdbarch, int regnr)
{
  int rv_num_pseudo = kvx_get_rv_num_pseudo (gdbarch, regnr);
  if (rv_num_pseudo < 0)
    return kvx_pseudo_register_name (gdbarch, regnr);

  if (rv_num_pseudo >= rv_num_pseudo_regs)
    return NULL;

  return rv_pseudo_regs[rv_num_pseudo].name;
}

struct type *
kvx_pseudo_wrap_register_type (struct gdbarch *gdbarch, int regnr)
{
  int rv_num_pseudo = kvx_get_rv_num_pseudo (gdbarch, regnr);
  if (rv_num_pseudo < 0)
    return kvx_pseudo_register_type (gdbarch, regnr);

  if (rv_num_pseudo >= rv_num_pseudo_regs)
    return NULL;

  return builtin_type (gdbarch)->builtin_long_long;
}

static void
kvx_init_pseudo_wrap_register (struct gdbarch *gdbarch,
			       struct rv_pseudo_desc *reg)
{
  reg->component
    = user_reg_map_name_to_regnum (gdbarch, reg->component_name, -1);
  if (reg->component < 0)
    error ("Can't find register '%s' for pseudo reg '%s'", reg->component_name,
	   reg->name);
}

enum register_status
kvx_pseudo_wrap_register_read (struct gdbarch *gdbarch,
			       struct readable_regcache *regcache, int regnum,
			       gdb_byte *buf)
{
  struct rv_pseudo_desc *reg;
  int rv_num_pseudo = kvx_get_rv_num_pseudo (gdbarch, regnum);
  if (rv_num_pseudo < 0)
    return kvx_pseudo_register_read (gdbarch, regcache, regnum, buf);

  if (rv_num_pseudo >= rv_num_pseudo_regs)
    error ("Register %i is not a pseudo register!", regnum);

  reg = &rv_pseudo_regs[rv_num_pseudo];
  if (!reg->component_name)
    {
      *(uint64_t *) buf = 0;
      return REG_VALID;
    }

  if (reg->component < 0)
    kvx_init_pseudo_wrap_register (gdbarch, reg);

  return regcache->raw_read (reg->component, buf);
}

void
kvx_pseudo_wrap_register_write (struct gdbarch *gdbarch,
				struct regcache *regcache, int regnum,
				const gdb_byte *buf)
{
  struct rv_pseudo_desc *reg;
  int rv_num_pseudo = kvx_get_rv_num_pseudo (gdbarch, regnum);
  if (rv_num_pseudo < 0)
    {
      kvx_pseudo_register_write (gdbarch, regcache, regnum, buf);
      return;
    }

  if (rv_num_pseudo >= rv_num_pseudo_regs)
    error ("Register %i is not a pseudo register!", regnum);

  reg = &rv_pseudo_regs[rv_num_pseudo];
  if (!reg->component_name)
    return;

  if (reg->component < 0)
    kvx_init_pseudo_wrap_register (gdbarch, reg);

  regcache->raw_write (reg->component, buf);
}
