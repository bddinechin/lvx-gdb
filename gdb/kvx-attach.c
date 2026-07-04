#include "kvx-attach.h"

#include "gdbcmd.h"
#include "inferior.h"
#include "progspace-and-thread.h"

void
kvx_attach_set_opt (bool opt, const char *bp_name, enum bfd_architecture arch)
{
  if (opt)
    {
      if (kvx_attach_search_bp (bp_name) > 0)
	return;
      if (!kvx_attach_search_fc_in_all_infs (bp_name, arch))
	return;

      kvx_attach_add_breakpoint (bp_name);
    }
}

void
kvx_attach_normal_stop (const char *bp_name, bool opt, bp_reached_cb cb,
			enum bfd_architecture exp_arch, struct bpstat *bs,
			int print_frame)
{
  struct gdbarch *gdbarch;

  if (!opt)
    return;

  // check that is our breakpoint
  if (!bs || !bs->breakpoint_at
      || !kvx_attach_bp_is_ours (bp_name, bs->breakpoint_at))
    return;

  // check the arch
  gdbarch = bs->breakpoint_at->gdbarch;
  if (!gdbarch || !gdbarch_bfd_arch_info (gdbarch)
      || gdbarch_bfd_arch_info (gdbarch)->arch != exp_arch)
    return;

  cb (gdbarch, bs, print_frame);

  kvx_attach_remove_bp (bp_name);
}

bool
kvx_attach_bp_is_ours (const char *bp_name, breakpoint *b)
{
  const char *name;
  location_spec *ls;

  if (!b)
    return false;

  ls = b->locspec.get ();
  if (!ls)
    return false;

  name = ls->to_string ();
  if (!name)
    return false;

  return !strcmp (name, bp_name) ? true : false;
}

int
kvx_attach_search_bp (const char *bp_name)
{
  for (breakpoint *b : all_breakpoints ())
    if (kvx_attach_bp_is_ours (bp_name, b))
      return b->number;

  return 0;
}

void
kvx_attach_remove_bp (const char *bp_name)
{
  char cmd[20];
  int bp = kvx_attach_search_bp (bp_name);

  if (bp > 0)
    {
      sprintf (cmd, "d %d", bp);
      execute_command (cmd, 0);
    }
}

void
kvx_attach_new_objfile (const char *bp_name, bool opt,
			enum bfd_architecture arch, struct objfile *objf)
{
  if (!opt)
    return;

  if (kvx_attach_search_bp (bp_name) > 0)
    return;

  if (kvx_attach_search_fc_in_objfile (bp_name, objf, arch))
    kvx_attach_add_breakpoint (bp_name);
}

bool
kvx_attach_search_fc_in_objfile (const char *function, struct objfile *objf,
				 enum bfd_architecture exp_arch)
{
  struct bound_minimal_symbol msym;
  struct gdbarch *gdbarch;

  if (!objf)
    return false;

  gdbarch = objf->arch ();
  if (!gdbarch || !gdbarch_bfd_arch_info (gdbarch)
      || gdbarch_bfd_arch_info (gdbarch)->arch != exp_arch)
    return false;

  msym = lookup_minimal_symbol (function, NULL, objf);
  if (msym.minsym == NULL)
    return false;

  return true;
}

void
kvx_attach_add_breakpoint (const char *bp_name)
{
  char buffer[1024];
  int len = snprintf (buffer, 1024, "b %s", bp_name);
  if (len < 0 || len >= 1024)
    {
      gdb_printf ("Error: cannot allocate buffer to add breakpoint in %s\n",
		  bp_name);
      return;
    }

  execute_command (buffer, 0);
}

bool
kvx_attach_search_fc_in_all_infs (const char *function,
				  enum bfd_architecture exp_arch)
{
  scoped_restore_current_pspace_and_thread restore_pspace_thread;

  for (inferior *inf : all_inferiors ())
    {
      if (!inf->pspace)
	continue;

      for (objfile *objf : inf->pspace->objfiles ())
	if (kvx_attach_search_fc_in_objfile (function, objf, exp_arch))
	  return true;

      for (struct so_list *so : inf->pspace->solibs ())
	if (kvx_attach_search_fc_in_objfile (function, so->objfile, exp_arch))
	  return true;
    }

  return false;
}
