/* Kalray KVX GNU/Linux native support.

   Copyright (C) 2007-2020 Free Software Foundation, Inc.

   This file is part of GDB.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program. If not, see <http://www.gnu.org/licenses/>. */

#include "defs.h"
#include "gdbarch.h"
#include "inferior.h"
#include "regcache.h"
#include <elf.h>
#include <sys/ptrace.h>
#include "linux-nat.h"
#include "kvx-linux-nat.h"
#include "gregset.h"
#include "gdb_proc_service.h"

struct kvx_linux_inf_data
{
  /* Hardware breakpoints for this process.  */
  struct kvx_linux_hw_pt bpts[MAX_BPTS];
  /* Hardware watchpoints for this process.  */
  struct kvx_linux_hw_pt wpts[MAX_WPTS];
};

struct kvx_linux_inf_data_deleter
{
  void operator() (kvx_linux_inf_data *inf_data) { xfree (inf_data); }
};

static const registry<inferior>::key<kvx_linux_inf_data,
				     kvx_linux_inf_data_deleter>
  kvx_linux_inf_data_key;
static struct kvx_linux_hw_pt_cap_s kvx_linux_hw_pt_cap;

class kvx_linux_nat_target final : public linux_nat_target
{
public:
  virtual void fetch_registers (struct regcache *, int) override;
  virtual void store_registers (struct regcache *, int) override;

  virtual int can_use_hw_breakpoint (enum bptype, int, int) override;
  virtual int insert_hw_breakpoint (struct gdbarch *,
				    struct bp_target_info *) override;
  virtual int remove_hw_breakpoint (struct gdbarch *,
				    struct bp_target_info *) override;

  virtual int insert_watchpoint (CORE_ADDR, int, enum target_hw_bp_type,
				 struct expression *) override;
  virtual int remove_watchpoint (CORE_ADDR, int, enum target_hw_bp_type,
				 struct expression *) override;
  virtual bool stopped_by_watchpoint (void) override;
  virtual bool stopped_data_address (CORE_ADDR *) override;

  /* defer to common nat/ code.  */
  virtual void low_new_thread (struct lwp_info *lp) override;
  virtual void low_delete_thread (struct arch_lwp_info *lp) override;
  virtual void low_prepare_to_resume (struct lwp_info *lp) override;

public:
  /* custom methods */
  void init_hwbp_cap (int pid);
  kvx_linux_inf_data *get_inferior_data (int pid);
  int get_hw_point_type_info (int pid, int type, struct kvx_linux_hw_pt **pts,
			      int *count);
  int insert_hw_point (CORE_ADDR addr, int len, int type, int krn_wp_type);
  int remove_hw_point (CORE_ADDR addr, int len, int type);
  struct arch_lwp_info *get_thread_info (struct lwp_info *lwp);
};

static kvx_linux_nat_target the_kvx_linux_nat_target;

/* Query Hardware Breakpoint information for the target we are attached to */
void
kvx_linux_nat_target::init_hwbp_cap (int pid)
{
  uint64_t val[2];

  if (ptrace ((enum __ptrace_request) PTRACE_GET_HW_PT_REGS, pid, 0, val) < 0)
    return;

  kvx_linux_hw_pt_cap.bp_count = val[0];
  kvx_linux_hw_pt_cap.wp_count = val[1];

  if (kvx_linux_hw_pt_cap.wp_count > MAX_WPTS)
    internal_error ("Unsupported number of watchpoints");
  if (kvx_linux_hw_pt_cap.bp_count > MAX_BPTS)
    internal_error ("Unsupported number of breakpoints");
}

kvx_linux_inf_data *
kvx_linux_nat_target::get_inferior_data (int pid)
{
  kvx_linux_inf_data *res;
  struct inferior *inf = find_inferior_pid (this, pid);

  if (!inf)
    return NULL;

  res = kvx_linux_inf_data_key.get (inf);
  if (!res)
    {
      res = (kvx_linux_inf_data *) xcalloc (1, sizeof (kvx_linux_inf_data));
      kvx_linux_inf_data_key.set (inf, res);

      this->init_hwbp_cap (pid);
    }

  return res;
}

void
fill_gregset (const struct regcache *regcache, gdb_gregset_t *rset, int regnum)
{
  int i;

  if (regnum != -1)
    {
      regcache->raw_collect (regnum, &(*rset)[regnum]);
      return;
    }

  for (i = R_R0; i < NB_GREGSET_REGS; i++)
    if (regcache->get_register_status (i) == REG_VALID)
      regcache->raw_collect (i, &(*rset)[i]);
}

static void
supply_gregset_reg (struct regcache *regcache, const gdb_gregset_t *rset)
{
  int i;

  for (i = R_R0; i < NB_GREGSET_REGS; i++)
    regcache->raw_supply (i, &(*rset)[i]);
}

void
supply_gregset (struct regcache *regcache, const gdb_gregset_t *gregsetp)
{
  supply_gregset_reg (regcache, gregsetp);
}

void
fill_fpregset (const struct regcache *regcache, gdb_fpregset_t *fpregsetp,
	       int regnum)
{
  /* Not implemented yet */
}

void
supply_fpregset (struct regcache *regcache, const gdb_fpregset_t *fpregsetp)
{
  /* Not implemented yet */
}

static void
kvx_get_gregs (int tid, gdb_gregset_t *regs)
{
  struct iovec iovec;

  iovec.iov_base = regs;
  iovec.iov_len = sizeof (*regs);
  if (ptrace (PTRACE_GETREGSET, tid, NT_PRSTATUS, &iovec) < 0)
    {
      perror_with_name (_ ("Couldn't get general registers"));
      return;
    }
}

/* Fetch greg-register(s) from process/thread TID
   and store value(s) in GDB's register array. */
void
kvx_linux_nat_target::fetch_registers (struct regcache *regcache, int regnum)
{
  int tid = regcache->ptid ().lwp ();
  gdb_gregset_t regs;

  kvx_get_gregs (tid, &regs);
  supply_gregset_reg (regcache, &regs);
}

/* Store greg-register(s) in GDB's register
   array into the process/thread specified by TID. */
void
kvx_linux_nat_target::store_registers (struct regcache *regcache, int regnum)
{
  gdb_gregset_t regs;
  struct iovec iovec;
  int tid = regcache->ptid ().lwp ();

  kvx_get_gregs (tid, &regs);

  fill_gregset (regcache, &regs, regnum);

  iovec.iov_base = &regs;
  iovec.iov_len = sizeof (regs);
  if (ptrace (PTRACE_SETREGSET, tid, NT_PRSTATUS, &iovec) < 0)
    {
      perror_with_name (_ ("Couldn't write general registers"));
      return;
    }
}

#if defined(__kvxarch_kv3_2)
static int
can_use_hw_breakpoint_cb (struct breakpoint *b, void *d)
{
  int *cb_par = (int *) d;

  if (!cb_par || b->enable_state != bp_enabled || b->type == cb_par[1])
    return 0;

  if (b->type == bp_hardware_watchpoint || b->type == bp_read_watchpoint
      || b->type == bp_access_watchpoint)
    cb_par[0]++;

  return 0;
}
#endif

int
kvx_linux_nat_target::can_use_hw_breakpoint (enum bptype type, int cnt,
					     int othertype)
{
  /* Software watchpoint always possible */
  if (type == bp_watchpoint)
    return 1;

#if defined(__kvxarch_kv3_1)
  if (type == bp_hardware_breakpoint)
    return (cnt <= 2) ? 1 : -1;
  else if (type == bp_hardware_watchpoint)
    return (cnt <= 1) ? 1 : -1;
#elif defined(__kvxarch_kv3_2)
  if (type == bp_hardware_breakpoint || type == bp_hardware_watchpoint
      || type == bp_access_watchpoint || type == bp_read_watchpoint)
    {
      int no_wb, cb_par[2] = {0, type};

      no_wb = 2;
      if (type == bp_hardware_breakpoint || othertype == 0)
	return (cnt <= no_wb) ? 1 : -1;

      /* Count the number of hardware watchpoints of other types*/
      for (breakpoint *b : all_breakpoints ())
	can_use_hw_breakpoint_cb (b, cb_par);

      return (cb_par[0] + cnt <= no_wb) ? 1 : -1;
    }
#else
#error Unsupported arch
#endif

  return 0;
}

static PTRACE_TYPE_ARG3
compute_ptrace_addr_arg (int cmd, int type, int idx, int krn_wp_type)
{
  uint64_t ret = ((uint64_t) cmd) | (type << 2) | (krn_wp_type << 3);
  ret |= (idx << 5);
  return (PTRACE_TYPE_ARG3) ret;
}

static uint64_t *
compute_ptrace_data_arg (CORE_ADDR addr, int len, int enable)
{
  static uint64_t data[2];

  data[0] = addr;
  data[1] = enable | (len << 1);

  return data;
}

int
kvx_linux_nat_target::get_hw_point_type_info (int pid, int type,
					      struct kvx_linux_hw_pt **pts,
					      int *count)
{
  kvx_linux_inf_data *data;

  *pts = NULL;
  *count = 0;

  data = this->get_inferior_data (pid);
  if (!data)
    return -1;

  if (type == HW_BKP_TYPE)
    {
      *pts = data->bpts;
      *count = kvx_linux_hw_pt_cap.bp_count;
    }
  else
    {
      *pts = data->wpts;
      *count = kvx_linux_hw_pt_cap.wp_count;
    }

  return 0;
}

struct arch_lwp_info *
kvx_linux_nat_target::get_thread_info (struct lwp_info *lwp)
{
  int i;
  struct arch_lwp_info *info = lwp_arch_private_info (lwp);

  if (info)
    return info;

  info = XCNEW (struct arch_lwp_info);

  for (i = 0; i < MAX_BPTS; i++)
    info->bpts_changed[i] = 1;
  for (i = 0; i < MAX_WPTS; i++)
    info->wpts_changed[i] = 1;

  lwp_set_arch_private_info (lwp, info);

  return info;
}

static struct update_registers_data cb_data_cb;
static int
kvx_update_thread_inf_cb (struct lwp_info *lwp)
{
  struct arch_lwp_info *info = the_kvx_linux_nat_target.get_thread_info (lwp);

  /* The actual update is done later just before resuming the lwp,
  we just mark that the registers need updating. */
  if (cb_data_cb.type == HW_BKP_TYPE)
    info->bpts_changed[cb_data_cb.i] = 1;
  else
    info->wpts_changed[cb_data_cb.i] = 1;

  /* If the lwp isn't stopped, force it to momentarily pause, so
     we can update its breakpoint registers. */
  if (!lwp_is_stopped (lwp))
    linux_stop_lwp (lwp);

  return 0;
}

int
kvx_linux_nat_target::insert_hw_point (CORE_ADDR addr, int len, int type,
				       int krn_wp_type)
{
  struct kvx_linux_hw_pt *pts;
  int i, count;
  int pid = inferior_ptid.pid ();
  int tid = inferior_ptid.lwp ();

  if (this->get_hw_point_type_info (pid, type, &pts, &count))
    return -1;

  for (i = 0; i < count; i++)
    {
      if (!pts[i].enabled)
	{
	  cb_data_cb.type = type;
	  cb_data_cb.i = i;

	  if (ptrace ((enum __ptrace_request) PTRACE_SET_HW_PT_REGS, tid,
		      compute_ptrace_addr_arg (HW_PT_CMD_SET_RESERVE, type, i,
					       krn_wp_type),
		      compute_ptrace_data_arg (addr, len, 1)))
	    continue;

	  pts[i].address = addr;
	  pts[i].size = len;
	  pts[i].krn_wp_type = krn_wp_type;
	  pts[i].enabled = 1;
	  pts[i].used = 1;

	  iterate_over_lwps (ptid_t (pid), kvx_update_thread_inf_cb);

	  return 0;
	}
    }

  /* We're out of hardware resources */
  return -1;
}

/* Insert a hardware-assisted breakpoint at BP_TGT->reqstd_address.
   Return 0 on success, -1 on failure. */
int
kvx_linux_nat_target::insert_hw_breakpoint (struct gdbarch *gdbarch,
					    struct bp_target_info *bp_tgt)
{
  CORE_ADDR addr;
  int len;

  addr = bp_tgt->reqstd_address;
  gdbarch_breakpoint_from_pc (gdbarch, &addr, &len);
  bp_tgt->placed_address = addr;
  bp_tgt->length = len;

  return this->insert_hw_point (addr, len, HW_BKP_TYPE, 0);
}

/* Insert a watchpoint to watch a memory region which starts at
   address ADDR and whose length is LEN bytes. Watch memory accesses
   of the type TYPE. Return 0 on success, -1 on failure. */
int
kvx_linux_nat_target::insert_watchpoint (CORE_ADDR addr, int len,
					 enum target_hw_bp_type type,
					 struct expression *cond)
{
  int krn_wp_type = 0;

  if (type == hw_write)
    krn_wp_type = KRN_HW_BREAKPOINT_W;
#if !defined(__kvxarch_kv3_1)
  else if (type == hw_read)
    krn_wp_type = KRN_HW_BREAKPOINT_R;
  else if (type == hw_access)
    krn_wp_type = KRN_HW_BREAKPOINT_RW;
#endif
  else
    return -1;

  return this->insert_hw_point (addr, len, HW_WP_TYPE, krn_wp_type);
}

int
kvx_linux_nat_target::remove_hw_point (CORE_ADDR addr, int len, int type)
{
  struct kvx_linux_hw_pt *pts;
  int i, found, count;
  int pid = inferior_ptid.pid ();

  if (this->get_hw_point_type_info (pid, type, &pts, &count))
    return -1;

  found = 0;
  for (i = 0; i < count; i++)
    {
      if (!pts[i].enabled)
	continue;

      if (pts[i].address == addr && pts[i].size == len)
	{
	  cb_data_cb.type = type;
	  cb_data_cb.i = i;
	  pts[i].enabled = 0;
	  iterate_over_lwps (ptid_t (pid), kvx_update_thread_inf_cb);
	  found = 1;
	}
    }

  if (found)
    return 0;

  return -1;
}

/* Remove a hardware-assisted breakpoint at BP_TGT->placed_address.
   Return 0 on success, -1 on failure. */
int
kvx_linux_nat_target::remove_hw_breakpoint (struct gdbarch *gdbarch,
					    struct bp_target_info *bp_tgt)
{
  return this->remove_hw_point (bp_tgt->placed_address, bp_tgt->length,
				HW_BKP_TYPE);
}

/* Implement the "to_remove_watchpoint" target_ops method.
   Remove a watchpoint that watched the memory region which starts at
   address ADDR, whose length is LEN bytes, and for accesses of the
   type TYPE. Return 0 on success, -1 on failure. */
int
kvx_linux_nat_target::remove_watchpoint (CORE_ADDR addr, int len,
					 enum target_hw_bp_type type,
					 struct expression *cond)
{
  return this->remove_hw_point (addr, len, HW_WP_TYPE);
}

bool
kvx_linux_nat_target::stopped_data_address (CORE_ADDR *addr_p)
{
  siginfo_t siginfo;
  int tid = inferior_ptid.lwp ();

  /* We must be able to set hardware watchpoints. */
  if (kvx_linux_hw_pt_cap.wp_count == 0)
    return false;

  /* Retrieve siginfo. */
  errno = 0;
  ptrace (PTRACE_GETSIGINFO, tid, 0, &siginfo);
  if (errno != 0)
    return false;

  /* This must be a hardware breakpoint. */
  if (siginfo.si_signo != SIGTRAP || (siginfo.si_code & 0xffff) != TRAP_HWBKPT)
    return false;

  /* If we are in a positive slot then we're looking at a breakpoint and not
     a watchpoint. */
  if (hw_pt_trap_is_bkp (siginfo.si_errno))
    return false;

  *addr_p = (CORE_ADDR) (uintptr_t) siginfo.si_addr;

  return true;
}

/* Return whether current thread is stopped due to a watchpoint. */
bool
kvx_linux_nat_target::stopped_by_watchpoint (void)
{
  CORE_ADDR addr;

  return this->stopped_data_address (&addr);
}

/* Function to call when a new thread is detected. */
void
kvx_linux_nat_target::low_new_thread (struct lwp_info *lwp)
{
  this->get_thread_info (lwp);
}

void
kvx_linux_nat_target::low_delete_thread (struct arch_lwp_info *lp)
{
  xfree (lp);
}

/* Called when resuming a thread LWP.
   The hardware debug registers are updated when there is any change. */
void
kvx_linux_nat_target::low_prepare_to_resume (struct lwp_info *lwp)
{
  struct arch_lwp_info *info = this->get_thread_info (lwp);
  int i, j, ret;
  int tid = lwp->ptid.lwp ();
  int pid = lwp->ptid.pid ();
  int hw_points_types[] = {HW_BKP_TYPE, HW_WP_TYPE};
  int nb_hw_points_types
    = sizeof (hw_points_types) / sizeof (hw_points_types[0]);

  for (i = 0; i < nb_hw_points_types; i++)
    {
      struct kvx_linux_hw_pt *pts, *pt;
      int count, type = hw_points_types[i];
      char *changed = !i ? info->bpts_changed : info->wpts_changed;

      if (this->get_hw_point_type_info (pid, type, &pts, &count))
	break;

      for (j = 0; j < count; j++)
	{
	  pt = &pts[j];
	  if (changed[j] && pt->used)
	    {
	      errno = 0;
	      ret = ptrace ((enum __ptrace_request) PTRACE_SET_HW_PT_REGS, tid,
			    compute_ptrace_addr_arg (HW_PT_CMD_SET_ENABLE, type,
						     j, pt->krn_wp_type),
			    compute_ptrace_data_arg (pt->address, pt->size,
						     pt->enabled));
	      if (ret < 0)
		perror_with_name ("Error setting hardware breakpoint");
	    }

	  changed[j] = 0;
	}
    }
}

/* Called by libthread_db. */
ps_err_e
ps_get_thread_area (struct ps_prochandle *ph, lwpid_t lwpid, int idx,
		    void **base)
{
  gdb_gregset_t regs;

  kvx_get_gregs (lwpid, &regs);

  /* IDX is the bias from the thread pointer to the beginning of the
   thread descriptor.  It has to be subtracted due to implementation
   quirks in libthread_db. */
  *base = (void *) (uintptr_t) (regs[R_TLS] - idx - KVX_TLS_TCB_SIZE);

  return PS_OK;
}

void
_initialize_kvx_linux_nat ();
void
_initialize_kvx_linux_nat ()
{
  /* Register the target.  */
  linux_target = &the_kvx_linux_nat_target;
  add_inf_child_target (&the_kvx_linux_nat_target);
}
