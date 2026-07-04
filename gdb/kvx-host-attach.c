/* Target-dependent code for the Kalray KVX for GDB, the GNU debugger.

   Copyright (C) 2010 Kalray

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

#include "defs.h"
#include <ctype.h>
#include <libgen.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/stat.h>
#include <unistd.h>
#include "gdbsupport/common-utils.h"
#include "arch-utils.h"
#include "dwarf2/loc.h"
#include "block.h"
#include "breakpoint.h"
#include "gdbcore.h"
#include "objfiles.h"
#include "observable.h"
#include "inferior.h"
#include "gdbcmd.h"
#include "progspace-and-thread.h"
#include "kvx-host-attach.h"
#include "kvx-attach.h"

#define KVX_HA_BP_NAME "mppa_rproc_gdb_boot"
#define KVX_HA_RUNNER_NAME "kvx-jtag-runner"

struct kvx_ha_host_bp_s
{
  uint64_t fw_path_addr;
  uint16_t fw_path_len;
  uint16_t board_id;
  uint16_t cluster_id;
} __attribute__ ((packed)) host_bp_v;

class kvx_ha_runner_proc
{
public:
  kvx_ha_runner_proc (int _pid, int _board_id)
  {
    pid = _pid;
    board_id = _board_id;
    next = NULL;
  }

  void add_to_list (void)
  {
    next = lst;
    lst = this;
  }

public:
  int pid;
  int board_id;
  struct kvx_ha_runner_proc *next;
  static kvx_ha_runner_proc *lst;
};

kvx_ha_runner_proc *kvx_ha_runner_proc::lst = NULL;
static bool kvx_ha_opt = false;

static char *
kvx_ha_get_gdb_dir (void)
{
  char path[PATH_MAX];
  int sz;

  sz = readlink ("/proc/self/exe", path, sizeof (path) - 1);
  if (sz < 0)
    {
      gdb_printf (
	_ ("Error %d (%s) occurred while trying to read the gdb path.\n"),
	errno, strerror (errno));
      return NULL;
    }
  path[sz] = 0;

  return strdup (dirname (path));
}

static int
kvx_ha_get_pcie_first_line (int board_id, const char *file, char *buf,
			    int buf_size)
{
  char path[128], *pc;
  int res, fd;

  *buf = 0;
  snprintf (path, sizeof (path), "/mppa/board%d/%s", board_id, file);
  if ((fd = open (path, O_RDONLY)) >= 0)
    {
      res = read (fd, buf, buf_size);
      close (fd);
      if (res < 0)
	return res;

      for (pc = buf; *pc; pc++)
	{
	  if (*pc == '\n')
	    {
	      *pc = 0;
	      break;
	    }
	}

      return 0;
    }

  return 1;
}

static char **
kvx_ha_get_runner_args (int board_id, int *argc)
{
  char *cmd = NULL, *gdb_dir = kvx_ha_get_gdb_dir ();
  char pcie_serial[100];
  char **ret_val = NULL;
  int runner_len, ret;
  struct stat st;

  if (!gdb_dir)
    return NULL;

  cmd = (char *) xmalloc (strlen (gdb_dir) + 200);
  sprintf (cmd, "%s/" KVX_HA_RUNNER_NAME, gdb_dir);
  runner_len = strlen (cmd);
  free (gdb_dir);

  if (stat (cmd, &st) < 0)
    {
      gdb_printf (_ ("Error: cannot find %s\n"), cmd);
      goto label_end;
    }

  *pcie_serial = 0;
  strcat (cmd + runner_len, " --scan >/dev/null 2>/dev/null");
  ret = system (cmd);
  if (WEXITSTATUS (ret) != 0)
    {
      if (kvx_ha_get_pcie_first_line (board_id, "serial", pcie_serial,
				      sizeof (pcie_serial) - 1))
	goto label_end;

      sprintf (cmd + runner_len,
	       " --scan --board-serial=%s >/dev/null",
	       pcie_serial);
      ret = system (cmd);
      if (WEXITSTATUS (ret) != 0)
	{
	  gdb_printf (_ ("Error executing %s\n"), cmd);
	  goto label_end;
	}
    }

  *argc = 1;
  if (*pcie_serial)
    (*argc)++;
  ret_val = (char **) xmalloc (sizeof (char *) * *argc);
  cmd[runner_len] = 0;
  ret_val[0] = strdup (cmd);
  if (*pcie_serial)
    {
      ret_val[1] = (char *) xmalloc (strlen (pcie_serial) + 100);
      sprintf (ret_val[1], "--board-serial=%s", pcie_serial);
    }

label_end:
  if (cmd)
    free (cmd);

  return ret_val;
}

static int
kvx_ha_start_runner (char **args, int *runner_pid)
{
  int pipefds[2], port = -1;

  if (pipe (pipefds))
    {
      gdb_printf (_ ("Error: cannot create pipe.\n"));
      return -1;
    }
  *runner_pid = fork ();

  if (*runner_pid < 0)
    {
      gdb_printf (_ ("Error: could not fork to launch the runner.\n"));
      return -1;
    }

  if (*runner_pid == 0)
    {
      /* Child */
      ::close (pipefds[0]);
      dup2 (pipefds[1], 500);
      ::close (pipefds[1]);

      setsid ();

      execvp (args[0], args);

      gdb_printf (_ ("Error: cannot run %s.\n"), args[0]);
      exit (1);
    }

  ::close (pipefds[1]);
  if (read (pipefds[0], &port, sizeof (port)) != sizeof (port))
    port = -1;
  ::close (pipefds[0]);
  if (port == -1)
    gdb_printf (_ ("Error reading the port from %s."), args[0]);

  return port;
}

static int
kvx_ha_read_fw_ags (enum bfd_endian byte_order, struct kvx_ha_host_bp_s *args,
		    char **fw_path)
{
  const struct block *blk = NULL;
  CORE_ADDR pc = 0, struct_addr = 0;
  frame_info_ptr frame = NULL;
  struct value *val = NULL;
  struct block_symbol blk_sym;
  struct stat st;

  // get the address of the function struct arg in memory
  memset (&blk_sym, 0, sizeof (blk_sym));
  frame = get_current_frame ();
  if (frame)
    pc = get_frame_pc (frame);
  if (pc)
    blk = block_for_pc (pc);
  if (blk)
    blk_sym = lookup_symbol ("gdb_info", blk, VAR_DOMAIN, nullptr);
  if (blk_sym.block && blk_sym.symbol)
    val = value_of_variable (blk_sym.symbol, blk_sym.block);
  if (val)
    struct_addr = extract_unsigned_integer (value_contents_all (val).data (), 8,
					    byte_order);
  if (!struct_addr)
    return 1;

  // read the firmware path
  read_memory (struct_addr, (gdb_byte *) args, sizeof (*args));
  if (!args->fw_path_addr || !args->fw_path_len
      || args->fw_path_len >= PATH_MAX)
    return 1;
  *fw_path = (char *) xmalloc (args->fw_path_len + 1);
  read_memory (args->fw_path_addr, (gdb_byte *) *fw_path, args->fw_path_len);
  (*fw_path)[args->fw_path_len] = 0;

  gdb_printf (_ ("Info: firmware: %s, board id: %u, cluster id: %u\n"),
	      *fw_path, args->board_id, args->cluster_id);

  if (stat (*fw_path, &st) < 0)
    {
      char *fw1
	= (char *) xmalloc (strlen ("/lib/firmware/") + args->fw_path_len + 1);

      sprintf (fw1, "/lib/firmware/%s", *fw_path);
      if (stat (fw1, &st) < 0)
	{
	  gdb_printf (_ ("Error: cannot find the firmware file %s or %s\n"),
		      *fw_path, fw1);
	  free (fw1);
	  return 1;
	}
      free (*fw_path);
      *fw_path = fw1;
    }

  return 0;
}

static char **
kvx_ha_get_runner_args (struct kvx_ha_host_bp_s *host_bp_v, char *fw_path)
{
  int i, nrunner_args;
  char **runner_args;

  // get the runner connection args
  runner_args = kvx_ha_get_runner_args (host_bp_v->board_id, &nrunner_args);
  if (!runner_args)
    return NULL;

  // add the rest of the runner args
  i = nrunner_args;
  nrunner_args += 4;
  runner_args
    = (char **) xrealloc (runner_args, (nrunner_args + 1) * sizeof (char *));
  runner_args[i] = (char *) xmalloc (strlen (fw_path) + 100);
  sprintf (runner_args[i], "--sym-file=Cluster%d:%s", host_bp_v->cluster_id,
	   fw_path); // + 1
  i++;
  runner_args[i++] = strdup ("--hot-attach");	  // + 2
  runner_args[i++] = strdup ("-D");		  // + 3
  runner_args[i++] = strdup ("--force-dsu-init"); // + 4
  runner_args[i] = NULL;

  return runner_args;
}

static int
kvx_ha_attach_to_runner (int port)
{
  inferior *orginf, *inf;
  char cmd[50];

  orginf = current_inferior ();

  scoped_restore_current_pspace_and_thread restore_pspace_thread;

  inf = add_inferior_with_spaces ();
  switch_to_inferior_and_push_target (inf, true, orginf);

  switch_to_inferior_no_thread (inf);

  sprintf (cmd, "attach-mppa %d", port);
  execute_command (cmd, 0);

  return 0;
}

static void
kvx_ha_on_bp_reached (struct gdbarch *gdbarch, struct bpstat *bs,
		      int print_frame)
{
  char *fw_path = NULL, **runner_args = NULL;
  int i, port, runner_pid;
  struct kvx_ha_host_bp_s host_bp_v;
  kvx_ha_runner_proc *rp;


  // get fw args
  if (kvx_ha_read_fw_ags (gdbarch_byte_order (gdbarch), &host_bp_v, &fw_path))
    goto label_end;

  // compute the runner args
  runner_args = kvx_ha_get_runner_args (&host_bp_v, fw_path);
  if (!runner_args)
    goto label_end;

  // start the runner
  port = kvx_ha_start_runner (runner_args, &runner_pid);
  for (i = 0;; i++)
    if (runner_args[i])
      free (runner_args[i]);
    else
      break;
  free (runner_args);
  if (port < 0)
    goto label_end;

  rp = new kvx_ha_runner_proc (runner_pid, host_bp_v.board_id);
  rp->add_to_list ();

  if (kvx_ha_attach_to_runner (port))
    goto label_end;

label_end:
  if (fw_path)
    free (fw_path);
}

static void
kvx_ha_cb_normal_stop (struct bpstat *bs, int print_frame)
{
  kvx_attach_normal_stop (KVX_HA_BP_NAME, kvx_ha_opt, &kvx_ha_on_bp_reached,
			  bfd_arch_i386, bs, print_frame);
}

static void
kvx_ha_cb_new_objfile (struct objfile *objf)
{
  kvx_attach_new_objfile (KVX_HA_BP_NAME, kvx_ha_opt, bfd_arch_i386, objf);
}

static void
kvx_ha_kill_runner (int pid)
{
  if (kill (pid, 0) != -1)
  {
    sleep (2);
    if (kill (pid, 0) != -1)
      kill (pid, 9);
  }
}

static void
set_ha_opt (const char *args, int from_tty, struct cmd_list_element *c)
{
  kvx_attach_set_opt (kvx_ha_opt, KVX_HA_BP_NAME, bfd_arch_i386);
}

void
kvx_ha_init (struct cmd_list_element **kal_set,
	     struct cmd_list_element **kal_show)
{
  add_setshow_boolean_cmd (
    "auto-host-attach", class_maintenance, &kvx_ha_opt,
    _ ("Set auto connect to MPPA when a host OpenCL program is executed."),
    _ ("Show auto connect to MPPA when a host OpenCL program is executed."),
    NULL, set_ha_opt, NULL, kal_set, kal_show);

  gdb::observers::new_objfile.attach (kvx_ha_cb_new_objfile, "kvx-opencl");
  gdb::observers::normal_stop.attach (kvx_ha_cb_normal_stop, "kvx-opencl");
}

__attribute__ ((destructor)) static void
kvx_ha_dtor (int exit_code)
{
  kvx_ha_runner_proc *q, *p = kvx_ha_runner_proc::lst;

  while (p)
    {
      kvx_ha_kill_runner (p->pid);

      q = p;
      p = p->next;
      free (q);
    }
}
