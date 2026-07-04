/* GNU/Linux/KVX specific low level interface, for the remote server for GDB.
   Copyright 2007-2012 Free Software Foundation, Inc.
   Copyright 2013-14 Kalray

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
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#include "server.h"
#include "linux-low.h"
#include "regdef.h"
#include "tdesc.h"

#include <elf.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/procfs.h>
#include <sys/ptrace.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "kvx-linux-nat.h"

/* Per-process arch-specific data we want to keep.  */
struct arch_process_info
{
  /* Hardware breakpoints for this process.  */
  kvx_linux_hw_pt bpts[MAX_BPTS];
  /* Hardware watchpoints for this process.  */
  kvx_linux_hw_pt wpts[MAX_WPTS];
};

static const char *xmltarget_kvx_linux = 0;
kvx_linux_hw_pt_cap_s kvx_linux_hw_pt_cap;

void init_registers_kvx (void);
extern const target_desc *tdesc_kvx;

static void
kvx_fill_gregset (regcache *regcache, void *buf)
{
  elf_greg_t *rset = (elf_greg_t *) buf;
  const target_desc *tdesc = regcache->tdesc;
  int r0_regnum;
  char *ptr;
  int i;

  // R0 - R63
  r0_regnum = find_regno (tdesc, "r0");
  ptr = (char *) &rset[R_R0];

  for (i = r0_regnum; i < r0_regnum + 64; i++)
    {
      collect_register (regcache, i, ptr);
      ptr += register_size (tdesc, i);
    }

  collect_register_by_name (regcache, "lc", (char *) &rset[R_LC]);
  collect_register_by_name (regcache, "le", (char *) &rset[R_LE]);
  collect_register_by_name (regcache, "ls", (char *) &rset[R_LS]);
  collect_register_by_name (regcache, "ra", (char *) &rset[R_RA]);
  collect_register_by_name (regcache, "cs", (char *) &rset[R_CS]);
  collect_register_by_name (regcache, "pc", (char *) &rset[R_PC]);
}

static void
kvx_store_gregset (regcache *regcache, const void *buf)
{
  const elf_greg_t *rset = (const elf_greg_t *) buf;
  const target_desc *tdesc = regcache->tdesc;
  int r0_regnum;
  char *ptr;
  int i;

  // R0 - R63
  r0_regnum = find_regno (tdesc, "r0");
  ptr = (char *) &rset[R_R0];

  for (i = r0_regnum; i < r0_regnum + 64; i++)
    {
      supply_register (regcache, i, ptr);
      ptr += register_size (tdesc, i);
    }

  supply_register_by_name (regcache, "lc", (char *) &rset[R_LC]);
  supply_register_by_name (regcache, "le", (char *) &rset[R_LE]);
  supply_register_by_name (regcache, "ls", (char *) &rset[R_LS]);
  supply_register_by_name (regcache, "ra", (char *) &rset[R_RA]);
  supply_register_by_name (regcache, "cs", (char *) &rset[R_CS]);
  supply_register_by_name (regcache, "pc", (char *) &rset[R_PC]);
}

regset_info kvx_regsets[]
  = {{PTRACE_GETREGSET, PTRACE_SETREGSET, NT_PRSTATUS, sizeof (elf_gregset_t),
      GENERAL_REGS, kvx_fill_gregset, kvx_store_gregset},
     NULL_REGSET};

static /*__attribute__((naked)) */ void
kvx_breakpoint_opcode (void)
{
  __asm("set $vsfr0=$r63\n;;\n");
}

static const gdb_byte *kvx_breakpoint
  = (gdb_byte *) (void *) &kvx_breakpoint_opcode;
#define kvx_breakpoint_len 4

static regsets_info kvx_regsets_info = {
  kvx_regsets, /* regsets */
  0,	       /* num_regsets */
  NULL,	       /* disabled_regsets */
};

static regs_info vregs_info = {NULL, /* regset_bitmap */
			       NULL, /* usrregs_info */
			       &kvx_regsets_info};

/* Query Hardware Breakpoint information for the target we are attached to */
static void
kvx_linux_init_hwbp_cap (int pid)
{
  uint64_t val[2];

  if (ptrace (PTRACE_GET_HW_PT_REGS, pid, 0, val) < 0)
    return;

  kvx_linux_hw_pt_cap.bp_count = val[0];
  kvx_linux_hw_pt_cap.wp_count = val[1];

  if (kvx_linux_hw_pt_cap.wp_count > MAX_WPTS)
    internal_error ("Unsupported number of watchpoints");
  if (kvx_linux_hw_pt_cap.bp_count > MAX_BPTS)
    internal_error ("Unsupported number of breakpoints");
}

static void
update_registers_callback (thread_info *thread, update_registers_data *data)
{
  lwp_info *lwp = get_thread_lwp (thread);

  /* The actual update is done later just before resuming the lwp,
  we just mark that the registers need updating.  */
  if (data->type == raw_bkpt_type_hw)
    lwp->arch_private->bpts_changed[data->i] = 1;
  else
    lwp->arch_private->wpts_changed[data->i] = 1;

  /* If the lwp isn't stopped, force it to momentarily pause, so
     we can update its breakpoint registers.  */
  if (!lwp->stopped)
    linux_stop_lwp (lwp);
}

static PTRACE_TYPE_ARG3
compute_ptrace_addr_arg (int cmd, int type, int idx, int krn_wp_type)
{
  uint64_t ret = ((uint64_t) cmd) | (type << 2) | (krn_wp_type << 3);
  ret |= (idx << 5);
  return (PTRACE_TYPE_ARG3) ret;
}

static uint64_t *
compute_ptrace_data_arg (kvx_linux_hw_pt *p)
{
  static uint64_t data[2];

  data[0] = p->address;
  data[1] = p->enabled | (p->size << 1);

  return data;
}

class kvx_target : public linux_process_target
{
public:
  const regs_info *get_regs_info (void) override;
  int breakpoint_kind_from_pc (CORE_ADDR *pcptr) override;
  const gdb_byte *sw_breakpoint_from_kind (int kind, int *size) override;
  bool supports_z_point_type (char z_type) override;
  bool supports_hardware_single_step (void) override;

protected:
  void low_arch_setup (void) override;
  CORE_ADDR low_get_pc (regcache *regcache) override;
  void low_set_pc (regcache *regcache, CORE_ADDR newpc) override;
  bool low_breakpoint_at (CORE_ADDR pc) override;
  int low_insert_point (raw_bkpt_type type, CORE_ADDR addr, int size,
			raw_breakpoint *bp) override;
  int low_remove_point (raw_bkpt_type type, CORE_ADDR addr, int size,
			raw_breakpoint *bp) override;
  bool low_stopped_by_watchpoint (void) override;
  CORE_ADDR low_stopped_data_address (void) override;
  arch_process_info *low_new_process (void) override;
  void low_delete_process (arch_process_info *info) override;
  void low_new_thread (lwp_info *) override;
  void low_delete_thread (arch_lwp_info *) override;
  void low_new_fork (process_info *parent, process_info *child) override;
  void low_prepare_to_resume (lwp_info *lwp) override;
  bool low_cannot_fetch_register (int regno) override;
  bool low_cannot_store_register (int regno) override;
  bool low_supports_breakpoints () override;

public:
  int low_get_thread_area (int lwpid, CORE_ADDR *addrp) override;
};

/* The singleton target ops object.  */
static kvx_target the_kvx_target;

const regs_info *
kvx_target::get_regs_info (void)
{
  return &vregs_info;
}

void
kvx_target::low_arch_setup (void)
{
  int pid = lwpid_of (current_thread);

  current_process ()->tdesc = tdesc_kvx;
  kvx_linux_init_hwbp_cap (pid);
}

CORE_ADDR
kvx_target::low_get_pc (regcache *regcache)
{
  unsigned long pc;

  collect_register_by_name (regcache, "pc", &pc);
  return pc;
}

void
kvx_target::low_set_pc (regcache *regcache, CORE_ADDR pc)
{
  unsigned long newpc = pc;
  supply_register_by_name (regcache, "pc", &newpc);
}

int
kvx_target::breakpoint_kind_from_pc (CORE_ADDR *pcptr)
{
  return 4;
}

const gdb_byte *
kvx_target::sw_breakpoint_from_kind (int kind, int *size)
{
  *size = kvx_breakpoint_len;
  return kvx_breakpoint;
}

bool
kvx_target::low_breakpoint_at (CORE_ADDR where)
{
  unsigned long insn;

  the_target->read_memory (where, (unsigned char *) &insn, kvx_breakpoint_len);
  return memcmp ((char *) &insn, kvx_breakpoint, kvx_breakpoint_len) == 0;
}

bool
kvx_target::supports_z_point_type (char z_type)
{
  switch (z_type)
    {
    case Z_PACKET_SW_BP:
    case Z_PACKET_HW_BP:
    case Z_PACKET_WRITE_WP:
#if !defined(__kvxarch_kv3_1)
    case Z_PACKET_READ_WP:
    case Z_PACKET_ACCESS_WP:
#endif
      return true;
    }

  return false;
}

int
kvx_target::low_insert_point (raw_bkpt_type type, CORE_ADDR addr, int size,
			      raw_breakpoint *bp)
{
  process_info *proc;
  arch_process_info *proc_info;
  kvx_linux_hw_pt *pts;
  int tid, pid, i, count;

  if (type == raw_bkpt_type_sw)
    return insert_memory_breakpoint (bp);

  tid = lwpid_of (current_thread);
  proc = current_process ();
  pid = pid_of (proc);
  proc_info = proc->priv->arch_private;
  if (type == raw_bkpt_type_hw)
    {
      /* breakpoint */
      count = kvx_linux_hw_pt_cap.bp_count;
      pts = proc_info->bpts;
    }
  else
    {
      count = kvx_linux_hw_pt_cap.wp_count;
      pts = proc_info->wpts;
    }

  for (i = 0; i < count; i++)
    {
      if (!pts[i].enabled)
	{
	  update_registers_data data = {type, i};
	  int krn_wp_type = 0;

	  if (type == raw_bkpt_type_write_wp)
	    krn_wp_type = KRN_HW_BREAKPOINT_W;
	  else if (type == raw_bkpt_type_read_wp)
	    krn_wp_type = KRN_HW_BREAKPOINT_R;
	  else if (type == raw_bkpt_type_access_wp)
	    krn_wp_type = KRN_HW_BREAKPOINT_RW;

	  if (ptrace (PTRACE_SET_HW_PT_REGS, tid,
		      compute_ptrace_addr_arg (HW_PT_CMD_SET_RESERVE,
					       (type == raw_bkpt_type_hw)
						 ? HW_BKP_TYPE
						 : HW_WP_TYPE,
					       i, krn_wp_type),
		      compute_ptrace_data_arg (&pts[i])))
	    continue;

	  pts[i].address = addr;
	  pts[i].size = size;
	  pts[i].krn_wp_type = krn_wp_type;
	  pts[i].enabled = 1;

	  for_each_thread (pid, [&] (thread_info *thread) {
	    update_registers_callback (thread, &data);
	  });

	  return 0;
	}
    }

  /* We're out of watchpoints.  */
  return -1;
}

int
kvx_target::low_remove_point (raw_bkpt_type type, CORE_ADDR addr, int size,
			      raw_breakpoint *bp)
{
  process_info *proc;
  kvx_linux_hw_pt *pts;
  int i, count, pid;

  if (type == raw_bkpt_type_sw)
    return remove_memory_breakpoint (bp);

  proc = current_process ();
  if (type == raw_bkpt_type_hw)
    {
      /* breakpoint */
      count = kvx_linux_hw_pt_cap.bp_count;
      pts = proc->priv->arch_private->bpts;
    }
  else
    {
      count = kvx_linux_hw_pt_cap.wp_count;
      pts = proc->priv->arch_private->wpts;
    }

  pid = pid_of (proc);
  for (i = 0; i < count; i++)
    {
      if (pts[i].address == addr && pts[i].size == size && pts[i].enabled == 1)
	{
	  update_registers_data data = {type, i};
	  pts[i].enabled = 0;

	  for_each_thread (pid, [&] (thread_info *thread) {
	    update_registers_callback (thread, &data);
	  });

	  return 0;
	}
    }

  /* No watchpoint matched.  */
  return -1;
}

/* Return whether current thread is stopped due to a watchpoint.  */
bool
kvx_target::low_stopped_by_watchpoint (void)
{
  lwp_info *lwp = get_thread_lwp (current_thread);
  int pid = lwpid_of (current_thread);
  siginfo_t siginfo;

  /* We must be able to set hardware watchpoints.  */
  if (kvx_linux_hw_pt_cap.wp_count == 0)
    return false;

  /* Retrieve siginfo.  */
  errno = 0;
  ptrace (PTRACE_GETSIGINFO, pid, 0, &siginfo);
  if (errno != 0)
    return false;

  /* This must be a hardware breakpoint.  */
  if (siginfo.si_signo != SIGTRAP || (siginfo.si_code & 0xffff) != TRAP_HWBKPT)
    return false;

  /* If we are in a positive slot then we're looking at a breakpoint and not
     a watchpoint.  */
  if (hw_pt_trap_is_bkp (siginfo.si_errno))
    return false;

  /* Cache stopped data address for use by kvx_stopped_data_address.  */
  lwp->arch_private->stopped_data_address
    = (CORE_ADDR) (uintptr_t) siginfo.si_addr;

  return true;
}

/* Return data address that triggered watchpoint.  Called only if
   kvx_stopped_by_watchpoint returned true.  */
CORE_ADDR
kvx_target::low_stopped_data_address (void)
{
  lwp_info *lwp = get_thread_lwp (current_thread);
  return lwp->arch_private->stopped_data_address;
}

/* Called when a new process is created.  */
arch_process_info *
kvx_target::low_new_process (void)
{
  arch_process_info *info = XCNEW (struct arch_process_info);
  return info;
}

void
kvx_target::low_delete_process (arch_process_info *info)
{
  xfree (info);
}

/* Called when a new thread is detected.  */
void
kvx_target::low_new_thread (lwp_info *lwp)
{
  arch_lwp_info *info = XCNEW (arch_lwp_info);
  int i;

  for (i = 0; i < MAX_BPTS; i++)
    info->bpts_changed[i] = 1;
  for (i = 0; i < MAX_WPTS; i++)
    info->wpts_changed[i] = 1;

  lwp->arch_private = info;
}

void
kvx_target::low_delete_thread (arch_lwp_info *arch_lwp)
{
  xfree (arch_lwp);
}

void
kvx_target::low_new_fork (process_info *parent, process_info *child)
{
  arch_process_info *parent_proc_info;
  arch_process_info *child_proc_info;
  lwp_info *child_lwp;
  arch_lwp_info *child_lwp_info;
  int i;

  /* These are allocated by linux_add_process.  */
  gdb_assert (parent->priv != NULL && parent->priv->arch_private != NULL);
  gdb_assert (child->priv != NULL && child->priv->arch_private != NULL);

  parent_proc_info = parent->priv->arch_private;
  child_proc_info = child->priv->arch_private;

  /* Linux kernel before 2.6.33 commit
     72f674d203cd230426437cdcf7dd6f681dad8b0d
     will inherit hardware debug registers from parent
     on fork/vfork/clone.  Newer Linux kernels create such tasks with
     zeroed debug registers.

     GDB core assumes the child inherits the watchpoints/hw
     breakpoints of the parent, and will remove them all from the
     forked off process.  Copy the debug registers mirrors into the
     new process so that all breakpoints and watchpoints can be
     removed together.  The debug registers mirror will become zeroed
     in the end before detaching the forked off process, thus making
     this compatible with older Linux kernels too.  */

  *child_proc_info = *parent_proc_info;

  /* Mark all the hardware breakpoints and watchpoints as changed to
     make sure that the registers will be updated.  */
  child_lwp = find_lwp_pid (ptid_t (child->pid));
  child_lwp_info = child_lwp->arch_private;
  for (i = 0; i < MAX_BPTS; i++)
    child_lwp_info->bpts_changed[i] = 1;
  for (i = 0; i < MAX_WPTS; i++)
    child_lwp_info->wpts_changed[i] = 1;
}

/* Called when resuming a thread.
   If the debug regs have changed, update the thread's copies.  */
void
kvx_target::low_prepare_to_resume (lwp_info *lwp)
{
  thread_info *thread = get_lwp_thread (lwp);
  int pid = lwpid_of (thread);
  process_info *proc = find_process_pid (pid_of (thread));
  arch_process_info *proc_info = proc->priv->arch_private;
  arch_lwp_info *lwp_info = lwp->arch_private;
  struct kvx_linux_hw_pt *pt;
  int i, ret;

  for (i = 0; i < kvx_linux_hw_pt_cap.bp_count; i++)
    {
      if (lwp_info->bpts_changed[i])
	{
	  pt = &proc_info->bpts[i];
	  errno = 0;
	  ret = ptrace (PTRACE_SET_HW_PT_REGS, pid,
			compute_ptrace_addr_arg (HW_PT_CMD_SET_ENABLE,
						 HW_BKP_TYPE, i, 0),
			compute_ptrace_data_arg (pt));
	  if (ret < 0)
	    perror_with_name ("Error setting breakpoint");

	  lwp_info->bpts_changed[i] = 0;
	}
    }

  for (i = 0; i < kvx_linux_hw_pt_cap.wp_count; i++)
    {
      if (lwp_info->wpts_changed[i])
	{
	  pt = &proc_info->wpts[i];
	  errno = 0;
	  ret
	    = ptrace (PTRACE_SET_HW_PT_REGS, pid,
		      compute_ptrace_addr_arg (HW_PT_CMD_SET_ENABLE, HW_WP_TYPE,
					       i, pt->krn_wp_type),
		      compute_ptrace_data_arg (pt));
	  if (ret < 0)
	    perror_with_name ("Error setting watchpoint");

	  lwp_info->wpts_changed[i] = 0;
	}
    }
}

/* Support for hardware single step.  */
bool
kvx_target::supports_hardware_single_step (void)
{
  return true;
}

int
kvx_target::low_get_thread_area (int lwpid, CORE_ADDR *addrp)
{
  uint64_t regs[NB_GREGSET_REGS];
  iovec iovec;

  iovec.iov_base = &regs;
  iovec.iov_len = sizeof (regs);
  if (ptrace (PTRACE_GETREGSET, lwpid, NT_PRSTATUS, &iovec) < 0)
    {
      perror_with_name (_ ("Couldn't get general registers"));
      return -1;
    }

  *addrp = regs[R_TLS];
  return 0;
}

/* Fetch the thread-local storage pointer for libthread_db.  */
ps_err_e
ps_get_thread_area (ps_prochandle *ph, lwpid_t lwpid, int idx, void **base)
{
  CORE_ADDR addr;

  if (the_kvx_target.low_get_thread_area (lwpid, &addr))
    return PS_ERR;

  *base = (void *) (uintptr_t) (addr - idx - KVX_TLS_TCB_SIZE);

  return PS_OK;
}

bool
kvx_target::low_cannot_fetch_register (int regno)
{
  return false;
}

bool
kvx_target::low_cannot_store_register (int regno)
{
  return false;
}

bool
kvx_target::low_supports_breakpoints ()
{
  return true;
}

static char *
create_xml (void)
{
  char *ret, *buf, reg_type[50];
  const struct gdb::reg *r;
  int i, nr, pos;

  nr = tdesc_kvx->reg_defs.size ();
  buf = (char *) malloc (nr * 150 + 1000);
  strcpy (buf, "@<target><architecture>kvx:kv3-1</architecture>"
	       "<feature name=\"eu.kalray.core.kv3-1\">");
  pos = strlen (buf);

  for (i = 0; i < nr; i++)
    {
      r = &tdesc_kvx->reg_defs[i];
      if (!strcmp (r->name, "cs"))
	{
	  pos += sprintf (buf + pos,
			  "<struct id=\"cs_type\" size=\"8\">\n"
			  " <field name=\"ic\" start=\"0\" end=\"0\" />"
			  " <field name=\"io\" start=\"1\" end=\"1\" />"
			  " <field name=\"dz\" start=\"2\" end=\"2\" />"
			  " <field name=\"ov\" start=\"3\" end=\"3\" />"
			  " <field name=\"un\" start=\"4\" end=\"4\" />"
			  " <field name=\"in\" start=\"5\" end=\"5\" />"
			  " <field name=\"xio\" start=\"9\" end=\"9\" />"
			  " <field name=\"xdz\" start=\"10\" end=\"10\" />"
			  " <field name=\"xov\" start=\"11\" end=\"11\" />"
			  " <field name=\"xun\" start=\"12\" end=\"12\" />"
			  " <field name=\"xin\" start=\"13\" end=\"13\" />"
			  " <field name=\"rm\" start=\"16\" end=\"17\" />"
			  " <field name=\"xrm\" start=\"20\" end=\"21\" />"
			  " <field name=\"xmf\" start=\"24\" end=\"24\" />"
			  " <field name=\"cc\" start=\"32\" end=\"47\" />"
			  " <field name=\"xdrop\" start=\"48\" end=\"53\" />"
			  " <field name=\"xpow2\" start=\"54\" end=\"59\" />"
			  "</struct>");
	  strcpy (reg_type, "cs_type");
	}
      else if (!strcmp (r->name, "pc") || !strcmp (r->name, "ra"))
	strcpy (reg_type, "code_ptr");
      else
	sprintf (reg_type, "int%d", r->size);

      pos += sprintf (
	buf + pos,
	"<reg name=\"%s\" regnum=\"%d\" bitsize=\"%d\" type=\"%s\"/>", r->name,
	i, r->size, reg_type);
    }

  strcpy (buf + pos, "</feature></target>");

  ret = strdup (buf);
  free (buf);

  return ret;
}

/* The linux target ops object.  */
linux_process_target *the_linux_target = &the_kvx_target;

void
initialize_low_arch (void)
{
  /* Initialize the Linux target descriptions.  */
  init_registers_kvx ();

  if (!tdesc_kvx->xmltarget)
    {
      xmltarget_kvx_linux = create_xml ();
      ((target_desc *) tdesc_kvx)->xmltarget = xmltarget_kvx_linux;
    }

  initialize_regsets_info (&kvx_regsets_info);
}
