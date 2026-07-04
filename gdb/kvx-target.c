/* Target ops to connect to the KVX simulator.

   Copyright (C) 2010, Kalray
 */

#include "defs.h"
#include <assert.h>
#include <libgen.h>
#include <limits.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "target.h"
#include "environ.h"
#include "gdbcmd.h"
#include "gdbcore.h"
#include "gdbthread.h"
#include "inferior.h"
#include "infrun.h"
#include "osdata.h"
#include "main.h"
#include "symfile.h"
#include "top.h"
#include "arch-utils.h"
#include "cli/cli-decode.h"
#include "cli/cli-setshow.h"
#include "cli/cli-utils.h"
#include "event-top.h"
#include "regcache.h"
#include "../gdbsupport/event-loop.h"
#include "location.h"
#include "regcache.h"
#include "objfiles.h"
#include "thread-fsm.h"
#include "interps.h"
#include "progspace-and-thread.h"
#include "remote.h"

#include "elf/kvx.h"
#include "elf-bfd.h"
#include "elf/kvx.h"
#include "kvx-target.h"
#include "kvx-exception-info.h"
#include "kvx-dump-tlb.h"
#include "solib-kvx-bare.h"
#include "observable.h"
#include "../gdbsupport/ptid.h"
#include "bfd-in2.h"
#include "kvx-host-attach.h"
#include "kvx-rv-debug.h"

#ifndef MAX
#define MAX(a, b) ((a < b) ? (b) : (a))
#endif

extern int print_stopped_thread;

static bool sigint_already_sent = false;
static struct target_info kvx_target_info = {
  "mppa",		     // shortname
  "Kalray MPPA connection",  // longname
  "Connect to a Kalray MPPA" // doc
};
static bool use_all_stop = false;

static int
kvx_get_inf_arch (inferior *inf)
{
  struct gdbarch *gdbarch = NULL;
  const struct bfd_arch_info *arch_info = NULL;

  if (inf == NULL)
    inf = current_inferior ();

  if (inf)
    gdbarch = inf->gdbarch;
  if (gdbarch)
    arch_info = gdbarch_bfd_arch_info (gdbarch);

  if (!arch_info)
    return bfd_arch_unknown;

  return arch_info->arch;
}

struct kvx_target final : public target_ops
{
public:
  virtual strata stratum (void) const override { return arch_stratum; }

  virtual const target_info &info (void) const override
  {
    return kvx_target_info;
  }

  virtual bool can_create_inferior (void) override
  {
    inferior *inf = current_inferior ();
    int arch = kvx_get_inf_arch (inf);

    if (inf && arch != bfd_arch_unknown && arch != bfd_arch_kvx)
      {
	if (inf->target_is_pushed (this))
	  inf->unpush_target (this);

	return false;
      }

    return true;
  }

  virtual bool filesystem_is_local (void) override;

  virtual bool can_run (void) override { return false; }

  virtual bool can_attach (void) override { return false; }

  virtual void attach (const char *args, int from_tty) override;

  virtual void create_inferior (const char *exec_file, const std::string &args,
				char **env, int from_tty) override;

  virtual void mourn_inferior (void) override;

  virtual ptid_t wait (ptid_t ptid, struct target_waitstatus *status,
		       target_wait_flags options)
    TARGET_DEFAULT_FUNC (default_target_wait) override;

  virtual void resume (ptid_t ptid,
		       int TARGET_DEBUG_PRINTER (target_debug_print_step) step,
		       enum gdb_signal signal) override;

  virtual bool supports_non_stop (void) override { return !use_all_stop; }

  virtual bool can_async_p (void) override { return target_async_permitted; }

  virtual std::string pid_to_str (ptid_t ptid) override;

  virtual const char *extra_thread_info (thread_info *tp) override;

  virtual void fetch_registers (struct regcache *rc, int regnum) override;

  virtual int region_ok_for_hw_watchpoint (CORE_ADDR addr, int len) override
  {
    return 1;
  }

  static int can_use_hw_breakpoint_cb (struct breakpoint *b, void *d)
  {
    int *cb_par = (int *) d;

    if (!cb_par || b->enable_state != bp_enabled || b->type == cb_par[1])
      return 0;

    if (b->type == bp_hardware_watchpoint || b->type == bp_read_watchpoint
	|| b->type == bp_access_watchpoint)
      cb_par[0]++;

    return 0;
  }

  virtual int can_use_hw_breakpoint (enum bptype type, int cnt, int ot) override
  {
    int no_wb, cb_par[2] = {0, type};

    /* Software watchpoint always possible */
    if (type == bp_watchpoint)
      return 1;

    if (get_kvx_arch () == 0)
      {
	/* Coolidge V1 */
	no_wb = 1;
	if (type == bp_hardware_breakpoint || type == bp_hardware_watchpoint)
	  return (cnt <= no_wb) ? 1 : -1;

	return 0;
      }

    /* Coolidge V2 */
    if (type != bp_hardware_breakpoint && type != bp_hardware_watchpoint
	&& type != bp_access_watchpoint && type != bp_read_watchpoint)
      return 0;

    no_wb = 2;
    if (type == bp_hardware_breakpoint || ot == 0)
      return (cnt <= no_wb) ? 1 : -1;

    /* Count the number of hardware watchpoints of other types*/
    for (breakpoint *b : all_breakpoints ())
      can_use_hw_breakpoint_cb (b, cb_par);

    return (cb_par[0] + cnt <= no_wb) ? 1 : -1;
  }

  virtual void interrupt (void) override
  {
    if (inferior_ptid == null_ptid)
      return;

    if (sigint_already_sent)
      {
	sigint_already_sent = false;
	if (query (_ ("Interrupted while waiting for the program.\n"
		      "Give up waiting? ")))
	  quit ();
      }

    sigint_already_sent = true;
    this->beneath ()->stop (inferior_ptid);
  }

  virtual enum target_xfer_status
  xfer_partial (enum target_object object, const char *annex, gdb_byte *readbuf,
		const gdb_byte *writebuf, ULONGEST offset, ULONGEST len,
		ULONGEST *xfered_len) override;
};

static struct kvx_target the_kvx_target;
static std::string da_options;

static struct cmd_list_element *kalray_set_cmdlist;
static struct cmd_list_element *kalray_set_traps_cmdlist;
static struct cmd_list_element *kalray_show_traps_cmdlist;
static struct cmd_list_element *kalray_show_cmdlist;
static const char *simulation_vehicles[] = {"kvx-cluster", NULL};
static const char *simulation_vehicle = simulation_vehicles[0];
static const char *sopts_cluster_stop_all[]
  = {"none", "jtag", "break_mask", NULL};
static const char *sopts_cluster_debug_ring[] = {"all", "current", NULL};
static const char *sopt_cluster_debug_ring = sopts_cluster_debug_ring[0];
static const char *sopt_cluster_stop_all = sopts_cluster_stop_all[0];
static const char *traps_name[] = {"opcode",
				   "misalign",
				   "psys",
				   "dsys",
				   "double_ecc",
				   "single_ecc",
				   "nomap",
				   "protection",
				   "write_to_clean",
				   "atomic_to_clean",
				   "double_exception",
				   "vsfr",
				   "pl_owner"};
static int no_traps_name = sizeof (traps_name) / sizeof (traps_name[0]);
char cjtag_over_iss = 'n';
bool opt_break_on_spawn = false;
bool opt_cont_os_init_done = true;
bool opt_compact_asm = true;
int opt_cluster_stop_all = 0;
int in_attach_mppa = 0;
int new_attach_requested = 0;
static pid_t server_pid;
int after_first_resume = 0;
bool opt_trap = false;
int wait_os_init_done = 0;

struct kvx_inf_data_deleter
{
  void operator() (kvx_inferior_data *inf_data) { xfree (inf_data); }
};

static const registry<inferior>::key<kvx_inferior_data, kvx_inf_data_deleter>
  kvx_inf_data_key;

static bool
in_sync_execution (void)
{
  for (ui *ui : all_uis ())
    {
      if (ui->prompt_state == PROMPT_BLOCKED)
	return true;
    }

  return false;
}

static void
kvx_push_arch_stratum (struct target_ops *ops, int from_tty)
{
  inferior *inf = current_inferior ();
  int arch = kvx_get_inf_arch (inf);

  if (arch != bfd_arch_unknown && arch != bfd_arch_kvx)
    return;

  if (inf && !inf->target_is_pushed (&the_kvx_target))
    inf->push_target (&the_kvx_target);
}

static void
kvx_inf_added (struct inferior *inf)
{
  int arch = kvx_get_inf_arch (inf);

  if (arch != bfd_arch_unknown && arch != bfd_arch_kvx)
    return;

  if (!inf->target_is_pushed (&the_kvx_target))
    inf->push_target (&the_kvx_target);
}

static void
kvx_target_new_thread (struct thread_info *t)
{
  if (inferior_ptid != null_ptid)
    current_inferior ()->control.stop_soon = STOP_QUIETLY;

  kvx_push_arch_stratum (NULL, 0);
}

static void
kvx_target_attach (struct target_ops *ops, const char *args, int from_tty)
{
  const char tar_remote_str[] = "target extended-remote";
  const gdb::observers::token new_thread_token;
  int print_thread_events_save = print_thread_events;
  char *args_copy, *host, *port, *tar_remote_cmd;
  bool retry_connection = false;

  if (!args)
    args = "";

  args_copy = strdup (args);
  port = strchr (args_copy, ':');
  if (port)
    {
      *port = 0;
      port++;
      host = args_copy;
    }
  else
    {
      port = args_copy;
      host = (char *) "";
    }

  if (!*host || !strcmp (host, "127.0.0.1") || !strcmp (host, "localhost"))
    execute_command ("set sysroot /", 0);

  tar_remote_cmd = (char *) malloc (strlen (host) + strlen (port)
				    + strlen (tar_remote_str) + 4);
  parse_pid_to_attach (port);

  print_thread_events = 0;
  sprintf (tar_remote_cmd, "%s %s:%s", tar_remote_str, host, port);
  free (args_copy);

  /* Load the real debug target by issuing 'target remote'. Of
     course things aren't that simple because it's not meant to be
     used that way. One issue is that connecting to the gdb_stub
     will emit MI stop notifications and will print the initial
     frame at the connection point. The observer
     makes infrun believe that we're in a series of steps and thus
     inhibits the emission of the new_thread observer notification
     that prints the initial MI *stopped message.
   */
  gdb::observers::new_thread.attach (kvx_target_new_thread, new_thread_token,
				     "kvx");
  async_disable_stdin ();

  do
    {
      try
	{
	  execute_command (tar_remote_cmd, 0);
	  retry_connection = false;
	}
      catch (gdb_exception &ex)
	{
	  const char *prefix_non_stop = "Non-stop mode requested";
	  if (!retry_connection
	      && !strncmp (ex.message->c_str (), prefix_non_stop,
			   strlen (prefix_non_stop)))
	    {
	      execute_command ("set non-stop 0", 0);
	      retry_connection = true;
	      use_all_stop = true;
	      opt_cont_os_init_done = false;
	    }
	  else
	    {
	      gdb::observers::new_thread.detach (new_thread_token);
	      free (tar_remote_cmd);
	      print_thread_events = print_thread_events_save;

	      throw;
	    }
	}
    }
  while (retry_connection);

  free (tar_remote_cmd);

  /* We need to tell the debugger to fake a synchronous
     command. This has already been done at the upper level when the
     main loop executes the "run" command, but the execute_command
     call we did just above reseted to async handling when it
     terminated.
   */
  async_disable_stdin ();
  kvx_push_arch_stratum (NULL, 0);

  /* Remove hacks*/
  gdb::observers::new_thread.detach (new_thread_token);
  current_inferior ()->control.stop_soon = NO_STOP_QUIETLY;
  print_thread_events = print_thread_events_save;
}

void
kvx_target::create_inferior (const char *exec_file, const std::string &args,
			     char **env, int from_tty)
{
  char set_non_stop_cmd[] = "set non-stop";
  char set_pagination_off_cmd[] = "set pagination off";
  gdb_argv build_argv (args.c_str ());
  char **argv_args = build_argv.get ();
  gdb_argv build_da_args (da_options.c_str ());
  char **da_args = build_da_args.get ();
  char **stub_args;
  char **arg;
  int nb_args = 0, nb_da_args = 0;
  int pipefds[2];
  int no_march = 0;
  int core;
  int argidx = 0;
  int saved_async_execution = !in_sync_execution ();

  if (exec_file == NULL)
    error (_ ("No executable file specified.\nUse the \"file\" or "
	      "\"exec-file\" command."));

  kvx_push_arch_stratum (NULL, 0);
  if (!use_all_stop)
    execute_command (set_non_stop_cmd, 0);
  execute_command (set_pagination_off_cmd, 0);

  arg = argv_args;
  while (arg && *arg++)
    nb_args++;
  if (nb_args && !*argv_args[nb_args - 1])
    nb_args--;

  arg = da_args;
  while (arg && *arg++)
    nb_da_args++;

  stub_args = (char **) xmalloc ((nb_args + nb_da_args + 7) * sizeof (char *));
  stub_args[argidx++] = (char *) simulation_vehicle;

  core = (elf_elfheader (current_program_space->exec_bfd ())->e_flags
	  & ELF_KVX_CORE_MASK);

  if (nb_da_args && strlen (da_options.c_str ()))
    {
      arg = da_args;
      while (*arg)
	{
	  if (strncmp (*arg, "--march=", 8) == 0)
	    no_march = 1;

	  stub_args[argidx++] = *arg++;
	}
    }

  if (!no_march)
    {
      switch (core)
	{
	case ELF_KVX_CORE_KV3_1:
	  stub_args[argidx++] = (char *) "--march=kv3-1";
	  break;
	case ELF_KVX_CORE_KV3_2:
	  stub_args[argidx++] = (char *) "--march=kv3-2";
	  break;
	case ELF_KVX_CORE_KV4_1:
	  stub_args[argidx++] = (char *) "--march=kv4-1";
	  break;
	default:
	  error (_ ("The KVX binary is compiled for an unknown core."));
	}
    }

  stub_args[argidx++] = (char *) "--gdb";
  stub_args[argidx++] = (char *) "--";
  stub_args[argidx++] = (char *) exec_file;
  if (nb_args)
    {
      memcpy (stub_args + argidx, argv_args, nb_args * sizeof (char *));
      argidx += nb_args;
    }
  stub_args[argidx++] = NULL;

  /* Check that we didn't overflow the allocation above. */
  gdb_assert (argidx <= nb_args + nb_da_args + 7);

  if (server_pid != 0)
    {
      ::kill (server_pid, 9);
      waitpid (server_pid, NULL, 0);
    }

  opt_cont_os_init_done = 0;
  if (pipe (pipefds))
    error ("Cannot create pipe.");
  server_pid = fork ();

  if (server_pid < 0)
    error ("Couldn't fork to launch the server.");

  if (server_pid == 0)
    {
      char path[PATH_MAX];
      char tmp[PATH_MAX] = {0};
      char *dir;

      ::close (pipefds[0]);
      dup2 (pipefds[1], 500);
      ::close (pipefds[1]);

      setsid ();

      /* Child */
      if (env)
	environ = env;
      execvp (simulation_vehicle, stub_args);

      /* Not in PATH */
      if (readlink ("/proc/self/exe", tmp, PATH_MAX) != -1)
	{
	  dir = dirname (tmp);
	  snprintf (path, PATH_MAX, "%s/%s", dir, simulation_vehicle);
	  execvp (path, stub_args);
	}

      printf_unfiltered ("Could not find %s in the PATH\n", simulation_vehicle);
      exit (1);
    }
  else
    {
      int port = -1;
      char cmd_port[20];

      free (stub_args);

      ::close (pipefds[1]);
      if (read (pipefds[0], &port, sizeof (port)) != sizeof (port))
	port = -1;
      ::close (pipefds[0]);

      if (port > 0)
	{
	  sprintf (cmd_port, "%i", port);
	  print_stopped_thread = 0;
	  kvx_target_attach (this, cmd_port, from_tty);
	  print_stopped_thread = 1;
	}

      if (saved_async_execution)
	async_enable_stdin ();

      if (port == -1)
	error (_ ("Error reading the port from %s."), simulation_vehicle);
    }
}

static kvx_inferior_data *
mppa_init_inferior_data (struct inferior *inf)
{
  kvx_inferior_data *data
    = (kvx_inferior_data *) xcalloc (1, sizeof (kvx_inferior_data));
  char *endptr;
  std::unique_ptr<osdata> od;
  int idx_items;

  data->cluster_break_on_spawn = 0;
  kvx_inf_data_key.set (inf, data);

  /* Cluster name */
  data->cluster = "Cluster ?";

  od = get_osdata (NULL);

  for (idx_items = 0; idx_items < od->items.size (); idx_items++)
    {
      osdata_item &item = od->items[idx_items];
      unsigned long pid
	= strtoul (get_osdata_column (item, "pid")->c_str (), &endptr, 10);
      const char *cluster = get_osdata_column (item, "cluster")->c_str ();
      const char *unified = get_osdata_column (item, "unified")->c_str ();

      if (pid != inf->pid)
	continue;

      data->cluster = strdup (cluster);
      data->unified = unified && !strcmp (unified, "yes");
    }

  return data;
}

kvx_inferior_data *
mppa_inferior_data (struct inferior *inf)
{
  kvx_inferior_data *data = (kvx_inferior_data *) kvx_inf_data_key.get (inf);

  if (!data)
    data = mppa_init_inferior_data (inf);

  return data;
}

const char *
get_cluster_name (struct inferior *inf)
{
  kvx_inferior_data *data = mppa_inferior_data (inf);
  if (!data || !data->cluster)
    return "[cluster unknown]";

  return data->cluster;
}

void
kvx_target::mourn_inferior (void)
{
  struct target_ops *remote_target = this->beneath ();
  inferior *crt_inf = current_inferior ();

  /* Force disconnect even if we are in extended mode */
  for (inferior &inf : inferior_list)
    {
      if (inf.target_at (process_stratum) != remote_target)
	continue;

      set_current_inferior (&inf);
      set_current_program_space (inf.pspace);
      remote_target->mourn_inferior ();
      if (this->beneath () == remote_target)
	inf.unpush_target (remote_target);
      if (inf.target_is_pushed (this))
	inf.unpush_target (this);
    }
  set_current_inferior (crt_inf);
  set_current_program_space (crt_inf->pspace);
  crt_inf->push_target (this);

  if (server_pid)
    {
      ::kill (server_pid, 9);
      server_pid = 0;
    }
}

void
kvx_target_open (const char *name, int from_tty);
void
kvx_target_open (const char *name, int from_tty)
{}

void
kvx_target_close (struct target_ops *ops);
void
kvx_target_close (struct target_ops *ops)
{}

std::string
kvx_target::pid_to_str (ptid_t ptid)
{
  struct thread_info *thread = NULL;
  process_stratum_target *proc_target;
  const char *name;
  std::string ret;

  if (!ptid.lwp ())
    return this->beneath ()->pid_to_str (ptid);

  proc_target = (process_stratum_target *) get_current_remote_target ();
  if (proc_target)
    thread = find_thread_ptid (proc_target, ptid);
  if (!thread)
    return string_printf (_ ("Thread %ld"), ptid.lwp ());

  ret = string_printf ("Thread %d.%d", thread->inf->num, thread->per_inf_num);
  name = proc_target->thread_name (thread);
  if (name)
    {
      ret += " \"";
      ret += name;
      ret += "\"";
    }
  return ret;
}

const char *
kvx_target::extra_thread_info (struct thread_info *tp)
{
  return NULL;
}

static int
mppa_mark_clusters_booted (struct inferior *inf, void *_ptid)
{
  struct thread_info *thread;
  ptid_t *ptid = (ptid_t *) _ptid;

  if (!inf->pid)
    return 0;

  thread = any_live_thread_of_inferior (inf);

  /* Newer GDBs mark the thread as running before passing it to
     target_resume. However, if we are resuming the thread, it must
     have been stooped before... */
  if (thread
      && (thread->state == THREAD_STOPPED || thread->ptid.matches (*ptid)))
    mppa_inferior_data (inf)->booted = 1;

  return 0;
}

void
kvx_target::resume (ptid_t ptid, int step, enum gdb_signal signal)
{
  if (!after_first_resume && !wait_os_init_done)
    {
      after_first_resume = 1;
      for (inferior *inf : all_inferiors ())
	mppa_mark_clusters_booted (inf, &ptid);
    }

  return this->beneath ()->resume (ptid, step, signal);
}

static void
kvx_change_file (int pid, const char *file_path, const char *cluster_name)
{
  cjtag_over_iss = get_jtag_over_iss ();

  try
    {
      exec_file_locate_attach (pid, 0, 0);
      if (cjtag_over_iss == 'i')
	{
	  const char *soc = "default";
	  char path[PATH_MAX], *dn;
	  int sz = readlink ("/proc/self/exe", path, sizeof (path) - 1);
	  int len_path, nb_bytes, target_ver, core_ver;
	  path[sz] = 0;
	  dn = dirname (path);
	  if (dn != path)
	    strcpy (path, dn);
	  dn = dirname (path);
	  if (dn != path)
	    strcpy (path, dn);

	  get_kvx_target_core_vers (&target_ver, &core_ver);
	  if (target_ver == 3 && core_ver == 2)
	    soc = "mppa3_80_2";
	  else if (target_ver == 4 && core_ver == 1)
	    soc = "dpa_1";
	  len_path = strlen (path);
	  nb_bytes
	    = snprintf (path + len_path, sizeof (path) - len_path,
			"/lib/kalray-oce/kvx/kv%d_v%d_%s_debug_handlers.u",
			target_ver, core_ver, soc);

	  if (nb_bytes >= sizeof (path) - len_path)
	    fprintf (stderr, "Error: the debug handlers path is too long\n");
	  else if (access (path, R_OK))
	    fprintf (stderr, "Warning: cannot find the debug handlers at %s\n",
		     path);
	  else
	    {
	      if (!cluster_name)
		cluster_name = "[unknown cluster name]";
	      fprintf (stderr,
		       "Info: adding %s debug handler symbols from %s\n",
		       cluster_name, path);
	      symbol_file_add (path, 0, NULL, OBJF_USERLOADED | OBJF_SHARED);
	    }
	}

      kvx_bare_solib_load_debug_info ();
    }
  catch (...)
    {
      // exception
    }
}

static void
kvx_new_inferiors_cb (void *arg)
{
  int idx_items, found_new;
  char attach_cmd[25], *endptr;
  struct inferior *inf;
  std::unique_ptr<osdata> od;
  process_stratum_target *proc_target
    = (process_stratum_target *) get_current_remote_target ();

  do
    {
      in_attach_mppa = 1;
      new_attach_requested = 0;
      found_new = 0;

      scoped_restore_current_pspace_and_thread restore_pspace_thread;

      od = get_osdata (NULL);
      for (idx_items = 0; idx_items < od->items.size (); idx_items++)
	{
	  osdata_item &item1 = od->items[idx_items];
	  unsigned long pid
	    = strtoul (get_osdata_column (item1, "pid")->c_str (), &endptr, 10);

	  if (find_inferior_pid (proc_target, pid))
	    continue;

	  found_new = 1;
	  inf = add_inferior_with_spaces ();
	  set_current_inferior (inf);
	  switch_to_no_thread ();
	  set_current_program_space (inf->pspace);
	  sprintf (attach_cmd, "attach %li&", pid);
	  execute_command (attach_cmd, 0);
	  inf->control.stop_soon = NO_STOP_QUIETLY;
	  inf->removable = 1;
	}

      in_attach_mppa = 0;
    }
  while (new_attach_requested || found_new);
}

void
custom_notification_cb (const char *arg);
void
custom_notification_cb (const char *arg)
{
  if (remote_timeout < 10)
    remote_timeout = 10;
  if (in_attach_mppa)
    new_attach_requested = 1;
  else
    create_timer (0, &kvx_new_inferiors_cb, NULL);
}

static void
change_thread_cb (void *p)
{
  struct inferior *inf = (struct inferior *) p;
  struct thread_info *th;

  if (inferior_thread ()->state == THREAD_STOPPED)
    return;

  th = any_live_thread_of_inferior (inf);
  if (!th || th->state != THREAD_STOPPED)
    return;

  switch_to_thread (th);
}

struct os_init_done_fsm : public thread_fsm
{
  os_init_done_fsm (struct interp *interp) : thread_fsm (interp) {}

  virtual bool should_stop (struct thread_info *thread) override
  {
    process_stratum_target *proc_target
      = (process_stratum_target *) get_current_remote_target ();
    struct regcache *rc = get_thread_regcache (proc_target, inferior_ptid);

    return regcache_read_pc (rc) != 0;
  }

  virtual enum async_reply_reason do_async_reply_reason (void) override
  {
    return EXEC_ASYNC_LOCATION_REACHED;
  }
};

ptid_t
kvx_target::wait (ptid_t ptid, struct target_waitstatus *status,
		  target_wait_flags options)
{
  process_stratum_target *proc_target
    = (process_stratum_target *) get_current_remote_target ();
  struct target_ops *remote_target = this->beneath ();
  ptid_t res;
  struct inferior *inferior;
  kvx_inferior_data *data;
  struct regcache *rc;
  int idx_items, booted;

  res = remote_target->wait (ptid, status, options);

  inferior = find_inferior_pid (proc_target, res.pid ());
  if (!inferior || !find_thread_ptid (proc_target, res))
    return res;

  sigint_already_sent = false;

  data = mppa_inferior_data (inferior);
  booted = data->booted;
  if (!booted)
    {
      char *endptr;
      std::unique_ptr<osdata> od;

      od = get_osdata (NULL);

      for (idx_items = 0; idx_items < od->items.size (); idx_items++)
	{
	  osdata_item &item = od->items[idx_items];
	  unsigned long pid
	    = strtoul (get_osdata_column (item, "pid")->c_str (), &endptr, 10);
	  const char *file = get_osdata_column (item, "command")->c_str ();

	  if (pid != res.pid ())
	    continue;

	  data = mppa_inferior_data (inferior);
	  data->booted = 1;

	  if (file && file[0] != 0 && !data->sym_file_loaded)
	    {
	      ptid_t save_ptid = inferior_ptid;
	      data->sym_file_loaded = 1;
	      switch_to_thread (proc_target, res);
	      kvx_change_file (pid, file, data->cluster);
	      if (save_ptid != null_ptid)
		switch_to_thread (proc_target, save_ptid);
	    }

	  if (!after_first_resume)
	    status->set_stopped (GDB_SIGNAL_TRAP);
	  else
	    {
	      if (data->cluster_break_on_spawn)
		status->set_stopped (GDB_SIGNAL_TRAP);
	      else
		status->set_spurious ();
	    }
	  break;
	}
    }

  // restore the first syllab of gdb_os_init_done
  if (data->os_init_done)
    {
      int crt_os_init_done = data->os_init_done;

      wait_os_init_done--;
      data->os_init_done = 0;
      rc = get_thread_regcache (proc_target, res);
      if (regcache_read_pc (rc) == data->gdb_os_init_done_addr)
	{
	  ptid_t save_ptid = inferior_ptid;

	  switch_to_thread (proc_target, res);
	  write_memory (data->gdb_os_init_done_addr,
			(bfd_byte *) &data->saved_os_init_done_syl, 4);
	  sync_insert_remove_breakpoint (
	    data->gdb_os_init_done_addr, 4,
	    (const gdb_byte *) &data->saved_os_init_done_syl);
	  kvx_bare_solib_load_debug_info ();
	  if (after_first_resume)
	    status->set_spurious ();
	  if (save_ptid != null_ptid)
	    switch_to_thread (proc_target, save_ptid);
	}

      // change the thread to stopped one after continue to
      // gdb_os_init_done
      if (crt_os_init_done == 1)
	create_timer (0, &change_thread_cb, inferior);
    }
  else
    {
      if (after_first_resume && !booted)
	{
	  if (status->kind () == TARGET_WAITKIND_SPURIOUS)
	    {
	      ptid_t save_ptid = inferior_ptid;

	      switch_to_thread (proc_target, res);
	      kvx_prepare_os_init_done ();
	      if (save_ptid != null_ptid)
		switch_to_thread (proc_target, save_ptid);
	    }
	}
      // for the first inferior, the continue to os_init_done
      // is done from kvx_inferior_created
      if (!after_first_resume && opt_cont_os_init_done && res.pid () != 1)
	{
	  struct thread_info *tp;
	  ptid_t save_ptid = inferior_ptid;

	  switch_to_thread (proc_target, res);

	  tp = inferior_thread ();
	  if (tp && !tp->thread_fsm () && kvx_prepare_os_init_done ())
	    {
	      tp->set_thread_fsm (std::unique_ptr<thread_fsm> (
		new os_init_done_fsm (command_interp ())));
	    }

	  if (save_ptid != null_ptid)
	    switch_to_thread (proc_target, save_ptid);
	}
    }

  return res;
}

bool
kvx_target::filesystem_is_local (void)
{
  struct process_stratum_target *ps;
  inferior *inf = current_inferior ();
  const char *scon = NULL, *sport;

  if (!inf)
    return true;
  ps = inf->process_target ();
  if (!ps)
    return true;
  scon = ps->connection_string ();
  if (!scon)
    return true;

  sport = strchr (scon, ':');
  if (!sport)
    return true;

  if (sport == scon || !strncmp (scon, "127.0.0.1", 9)
      || !strncmp (scon, "localhost", 9))
    return true;

  return false;
}

void
kvx_target::attach (const char *args, int from_tty)
{
  return this->beneath ()->attach (args, from_tty);
}

enum target_xfer_status
kvx_target::xfer_partial (enum target_object object, const char *annex,
			  gdb_byte *readbuf, const gdb_byte *writebuf,
			  ULONGEST offset, ULONGEST len, ULONGEST *xfered_len)
{
  if (!this->beneath ())
    error (_ ("Don't know how to xfer.  Try \"help target\"."));

  return this->beneath ()->xfer_partial (object, annex, readbuf, writebuf,
					 offset, len, xfered_len);
}

void
kvx_target::fetch_registers (struct regcache *rc, int regnum)
{
  // don't use current_inferior () & current_inferior_
  // our caller (regcache_raw_read) changes only inferior_ptid

  // get the registers of the current thread (CPU) in the usual way
  this->beneath ()->fetch_registers (rc, regnum);

  enable_ps_v64_at_boot (rc);
}

static void
set_kalray_cmd (const char *args, int from_tty)
{
  help_list (kalray_set_cmdlist, "set kalray ", all_commands, gdb_stdout);
}

static void
show_kalray_cmd (const char *args, int from_tty)
{
  help_list (kalray_show_cmdlist, "show kalray ", all_commands, gdb_stdout);
}

static void
set_kalray_traps_cmd (const char *args, int from_tty)
{
  help_list (kalray_set_traps_cmdlist, "show kalray intercept-traps ",
	     all_commands, gdb_stdout);
}

static void
show_kalray_traps_cmd (const char *args, int from_tty)
{
  help_list (kalray_show_traps_cmdlist, "show kalray intercept-traps ",
	     all_commands, gdb_stdout);
}

static void
set_cluster_break_on_spawn (const char *args, int from_tty,
			    struct cmd_list_element *c)
{
  struct inferior *inf;
  kvx_inferior_data *data;

  if (inferior_ptid == null_ptid)
    error (_ ("Cannot set break on reset without a selected thread."));

  inf = current_inferior ();
  data = mppa_inferior_data (inf);
  data->cluster_break_on_spawn = opt_break_on_spawn;
  send_cluster_break_on_spawn (inf, opt_break_on_spawn);
}

static void
show_cluster_break_on_spawn (struct ui_file *file, int from_tty,
			     struct cmd_list_element *c, const char *value)
{
  kvx_inferior_data *data;
  process_stratum_target *proc_target
    = (process_stratum_target *) get_current_remote_target ();

  if (inferior_ptid == null_ptid)
    {
      printf (
	_ ("Cannot show break on reset without a live selected thread.\n"));
      return;
    }

  data = mppa_inferior_data (
    find_inferior_pid (proc_target, inferior_ptid.pid ()));
  gdb_printf (file, "The cluster break on reset is %d.\n",
	      data->cluster_break_on_spawn);
}

static void
show_cont_os_init_done (struct ui_file *file, int from_tty,
			struct cmd_list_element *c, const char *value)
{
  gdb_printf (file, "Continue until OS initialization is done is %d.\n",
	      opt_cont_os_init_done);
}

static void
show_compact_asm (struct ui_file *file, int from_tty,
		  struct cmd_list_element *c, const char *value)
{
  gdb_printf (file, "Compact assembly is %d.\n", opt_compact_asm);
}

static void
set_compact_asm (const char *args, int from_tty, struct cmd_list_element *c)
{
  set_parse_compact_asm (opt_compact_asm);
}

static int
get_trap_index (const char *name)
{
  int i;

  for (i = 0; i < no_traps_name; i++)
    if (!strcmp (traps_name[i], name))
      return i;

  return -1;
}

static void
set_intercept_trap (const char *args, int from_tty, struct cmd_list_element *c)
{
  struct inferior *inf;
  kvx_inferior_data *data;
  int trap_idx;

  trap_idx = get_trap_index (c->name);
  if (trap_idx < 0)
    error (_ ("Invalid trap name %s."), c->name);

  if (inferior_ptid == null_ptid)
    error (_ ("Cannot set intercept trap without a selected thread."));

  inf = current_inferior ();
  data = mppa_inferior_data (inf);
  data->intercept_trap &= ~(1 << trap_idx);
  data->intercept_trap |= opt_trap << trap_idx;

  send_intercept_trap (inf, data->intercept_trap);
}

static void
show_intercept_trap (struct ui_file *file, int from_tty,
		     struct cmd_list_element *c, const char *value)
{
  kvx_inferior_data *data;
  int trap_idx;
  process_stratum_target *proc_target
    = (process_stratum_target *) get_current_remote_target ();

  trap_idx = get_trap_index (c->name);
  if (trap_idx < 0)
    error (_ ("Invalid trap name %s."), c->name);

  if (inferior_ptid == null_ptid)
    {
      printf (
	_ ("Cannot show intercept trap without a live selected thread.\n"));
      return;
    }

  data = mppa_inferior_data (
    find_inferior_pid (proc_target, inferior_ptid.pid ()));
  gdb_printf (file, "Intercept %s trap is %s.\n", c->name,
	      ((data->intercept_trap >> trap_idx) & 1) ? "on" : "off");
}

static void
set_intercept_trap_mask (const char *args, int from_tty,
			 struct cmd_list_element *c)
{
  struct inferior *inf;
  kvx_inferior_data *data;

  if (inferior_ptid == null_ptid)
    error (_ ("Cannot set intercept trap without a selected thread."));

  opt_trap &= (1 << no_traps_name) - 1;

  inf = current_inferior ();
  data = mppa_inferior_data (inf);
  data->intercept_trap = opt_trap;

  send_intercept_trap (inf, data->intercept_trap);
}

static void
show_intercept_trap_mask (struct ui_file *file, int from_tty,
			  struct cmd_list_element *c, const char *value)
{
  kvx_inferior_data *data;
  process_stratum_target *proc_target
    = (process_stratum_target *) get_current_remote_target ();

  if (inferior_ptid == null_ptid)
    {
      printf (
	_ ("Cannot show intercept trap without a live selected thread.\n"));
      return;
    }

  data = mppa_inferior_data (
    find_inferior_pid (proc_target, inferior_ptid.pid ()));
  gdb_printf (file, "Intercept trap mask is 0x%04x.\n", data->intercept_trap);
}

static int
str_idx_in_opts (const char *s, const char **opts)
{
  int i;
  for (i = 0; opts[i]; i++)
    if (!strcmp (s, opts[i]))
      return i;
  return -1;
}

static void
set_cluster_stop_all (const char *args, int from_tty,
		      struct cmd_list_element *c)
{
  struct inferior *inf;
  kvx_inferior_data *data;

  if (inferior_ptid == null_ptid)
    error (_ ("Cannot set stop all cluster CPUs without a selected thread."));

  inf = current_inferior ();
  data = mppa_inferior_data (inf);
  data->cluster_stop_all
    = str_idx_in_opts (sopt_cluster_stop_all, sopts_cluster_stop_all);
  send_cluster_stop_all (inf, data->cluster_stop_all);
}

static void
show_cluster_stop_all (struct ui_file *file, int from_tty,
		       struct cmd_list_element *c, const char *value)
{
  kvx_inferior_data *data;
  process_stratum_target *proc_target
    = (process_stratum_target *) get_current_remote_target ();

  if (inferior_ptid == null_ptid)
    {
      printf (_ (
	"Cannot show stop all cluster CPUs without a live selected thread.\n"));
      return;
    }

  data = mppa_inferior_data (
    find_inferior_pid (proc_target, inferior_ptid.pid ()));
  gdb_printf (file, "Stop all cluster CPUs is %s.\n",
	      sopts_cluster_stop_all[data->cluster_stop_all]);
}

static void
set_cluster_debug_ring (const char *args, int from_tty,
			struct cmd_list_element *c)
{
  struct inferior *inf;
  kvx_inferior_data *data;

  if (inferior_ptid == null_ptid)
    error (_ ("Cannot set cluster debug ring without a selected thread."));

  inf = current_inferior ();
  data = mppa_inferior_data (inf);
  data->cluster_debug_ring
    = str_idx_in_opts (sopt_cluster_debug_ring, sopts_cluster_debug_ring);
  send_cluster_debug_ring (inf, data->cluster_debug_ring);
}

static void
show_cluster_debug_ring (struct ui_file *file, int from_tty,
			 struct cmd_list_element *c, const char *value)
{
  kvx_inferior_data *data;
  process_stratum_target *proc_target
    = (process_stratum_target *) get_current_remote_target ();

  if (inferior_ptid == null_ptid)
    {
      printf (
	_ ("Cannot show cluster debug ring without a selected thread.\n"));
      return;
    }

  data = mppa_inferior_data (
    find_inferior_pid (proc_target, inferior_ptid.pid ()));
  gdb_printf (file, "Cluster debug ring is %s.\n",
	      sopts_cluster_debug_ring[data->cluster_debug_ring]);
}

static void
kvx_force_inf_arch (inferior *inf)
{
  int arch;

  if (inf == NULL)
    inf = current_inferior ();

  arch = kvx_get_inf_arch (inf);

  if (arch != bfd_arch_unknown && arch != bfd_arch_kvx)
    {
      execute_command ("target exec", 0);
      execute_command ("file", 0);
      kvx_push_arch_stratum (NULL, 0);
    }
}

static void
attach_mppa_command (const char *args, int from_tty)
{
  char set_non_stop_cmd[] = "set non-stop";
  char set_pagination_off_cmd[] = "set pagination off";
  std::unique_ptr<osdata> od;
  int idx_items, cur_pid, bstopped, bcur_inf_stopped, new_attached;
  struct inferior *cur_inf;
  ptid_t cur_ptid, stopped_ptid;
  bool saved_async_execution = !in_sync_execution ();
  process_stratum_target *proc_target;

  dont_repeat ();

  print_thread_events = 0;
  after_first_resume = 0;
  in_attach_mppa = 1;

  kvx_push_arch_stratum (NULL, 0);
  if (!use_all_stop)
    execute_command (set_non_stop_cmd, 0);
  execute_command (set_pagination_off_cmd, 0);

  kvx_force_inf_arch (NULL);

  kvx_target_attach (&the_kvx_target, args, from_tty);
  cjtag_over_iss = get_jtag_over_iss ();

  bstopped = 0;
  bcur_inf_stopped = 0;
  od = get_osdata (NULL);

  cur_inf = current_inferior ();
  cur_ptid = inferior_ptid;
  cur_pid = cur_inf->pid;
  proc_target = (process_stratum_target *) get_current_remote_target ();

  for (new_attached = 1; new_attached;)
    {
      new_attached = 0;
      for (idx_items = 0; idx_items < od->items.size (); idx_items++)
	{
	  char *endptr, attach_cmd[25];
	  struct inferior *inf;
	  osdata_item &item = od->items[idx_items];
	  unsigned long pid
	    = strtoul (get_osdata_column (item, "pid")->c_str (), &endptr, 10);
	  bool saved_print_inferior_events = print_inferior_events;
	  bool saved_commit_resumed_state;
	  thread_info *first_inf_thread;

	  if (pid == cur_inf->pid || find_inferior_pid (proc_target, pid))
	    continue;

	  sprintf (attach_cmd, "%li", pid);
	  print_inferior_events = false;
	  proc_target->attach (attach_cmd, 0);
	  kvx_push_arch_stratum (NULL, 0);
	  print_inferior_events = saved_print_inferior_events;
	  inf = current_inferior ();

	  // send stop for the RM and initialize the inf (solib etc.)
	  inf->needs_setup = 1;
	  first_inf_thread = first_thread_of_inferior (inf);
	  saved_commit_resumed_state = proc_target->commit_resumed_state;
	  proc_target->commit_resumed_state = false;
	  notice_new_inferior (first_inf_thread,
			       first_inf_thread->state == THREAD_RUNNING, 0);
	  proc_target->commit_resumed_state = saved_commit_resumed_state;

	  inf->control.stop_soon = NO_STOP_QUIETLY;
	  inf->removable = 1;
	}

      bstopped = 0;
      bcur_inf_stopped = 0;
      od = get_osdata (NULL);

      for (idx_items = 0; idx_items < od->items.size (); idx_items++)
	{
	  struct inferior *inf;
	  char *endptr;
	  struct thread_info *live_th;
	  osdata_item &item1 = od->items[idx_items];
	  unsigned long pid
	    = strtoul (get_osdata_column (item1, "pid")->c_str (), &endptr, 10);
	  const char *file = get_osdata_column (item1, "command")->c_str ();
	  const char *running = get_osdata_column (item1, "running")->c_str ();

	  if (strcmp (running, "yes"))
	    continue;

	  inf = find_inferior_pid (proc_target, pid);
	  if (!inf) // new cluster not attached yet
	    {
	      new_attached = 1;
	      continue;
	    }

	  live_th = any_live_thread_of_inferior (inf);
	  if (live_th && !live_th->stop_requested)
	    set_stop_requested (proc_target, live_th->ptid, 1);

	  if (pid == cur_pid)
	    bcur_inf_stopped = 1;

	  if (!bstopped)
	    {
	      bstopped = 1;
	      stopped_ptid = any_live_thread_of_inferior (inf)->ptid;
	    }

	  if (file && file[0])
	    {
	      kvx_inferior_data *data;

	      switch_to_thread (any_live_thread_of_inferior (inf));
	      data = mppa_inferior_data (current_inferior ());
	      if (!data->sym_file_loaded)
		{
		  data->sym_file_loaded = 1;
		  kvx_change_file (pid, file, data->cluster);
		}
	    }
	}
    }

  if (!bstopped)
    async_enable_stdin ();

  if (!bstopped || bcur_inf_stopped)
    switch_to_thread (proc_target, cur_ptid);
  else
    switch_to_thread (proc_target, stopped_ptid);

  /* This is a hack to populate the dwarf mapping tables now that we
     have the architecture at hand. Having these tables initialized
     from the debug reader routines will break as objfile_arch won't
     have the register descriptions */
  gdbarch_dwarf2_reg_to_regnum (get_current_arch (), 0);

  if (saved_async_execution)
    async_enable_stdin ();

  in_attach_mppa = 0;
  if (new_attach_requested)
    kvx_new_inferiors_cb (NULL);
}

static void
run_mppa_command (const char *args, int from_tty)
{
  char set_non_stop_cmd[] = "set non-stop";
  char set_pagination_off_cmd[] = "set pagination off";
  char run_cmd[] = "run";

  dont_repeat ();

  kvx_push_arch_stratum (NULL, 0);
  if (!use_all_stop)
    execute_command (set_non_stop_cmd, 0);
  execute_command (set_pagination_off_cmd, 0);
  execute_command (run_cmd, 0);
}

void
_initialize_kvx_target ()
{
  int i;

  simulation_vehicle = simulation_vehicles[0];
  cjtag_over_iss = 'n';

  print_stopped_thread = 1;

  add_prefix_cmd ("kalray", class_maintenance, set_kalray_cmd,
		  _ ("Kalray specific variables\n            Configure various "
		     "Kalray specific variables."),
		  &kalray_set_cmdlist, 0 /* allow-unknown */, &setlist);

  add_prefix_cmd ("kalray", class_maintenance, show_kalray_cmd,
		  _ ("Kalray specific variables\n            Configure various "
		     "Kalray specific variables."),
		  &kalray_show_cmdlist, 0 /* allow-unknown */, &showlist);

  add_setshow_string_noescape_cmd (
    "debug_agent_options", class_maintenance, &da_options,
    _ ("Set the options passed to the debug agent."),
    _ ("Show the options passed to the debug agent."), NULL, NULL, NULL,
    &kalray_set_cmdlist, &kalray_show_cmdlist);

  add_setshow_enum_cmd ("simulation_vehicle", class_maintenance,
			simulation_vehicles, &simulation_vehicle,
			_ ("Set the simulation vehicle to use for execution."),
			_ ("Show the simulation vehicle to use for execution."),
			NULL, NULL, NULL, &kalray_set_cmdlist,
			&kalray_show_cmdlist);

  add_setshow_boolean_cmd ("break_on_spawn", class_maintenance,
			   &opt_break_on_spawn, _ ("Set break on reset."),
			   _ ("Show break on reset."), NULL,
			   set_cluster_break_on_spawn,
			   show_cluster_break_on_spawn, &kalray_set_cmdlist,
			   &kalray_show_cmdlist);

  add_setshow_boolean_cmd ("cont_os_init_done", class_maintenance,
			   &opt_cont_os_init_done,
			   _ ("Set continue until OS initialization is done."),
			   _ ("Show continue until OS initialization is done."),
			   NULL, NULL, show_cont_os_init_done,
			   &kalray_set_cmdlist, &kalray_show_cmdlist);

  add_com ("attach-mppa", class_run, attach_mppa_command,
	   _ ("Connect to a MPPA TLM platform and start debugging it.\nUsage "
	      "is `attach-mppa PORT'."));

  add_com ("mppa-cpu-status", class_run, mppa_cpu_status_command,
	   _ ("Show information of the current processor."));

  add_com (
    "mppa-cpu-debug-sfr-regs", class_run, mppa_cpu_debug_sfr_regs_command,
    _ ("Read the current processor debug SFR registers from the magic bus"));

  add_setshow_enum_cmd ("stop-all", class_maintenance, sopts_cluster_stop_all,
			&sopt_cluster_stop_all, _ ("Set stop all cluster CPUs"),
			_ ("Show stop all cluster CPUs"), NULL,
			set_cluster_stop_all, show_cluster_stop_all,
			&kalray_set_cmdlist, &kalray_show_cmdlist);

  add_setshow_enum_cmd ("cluster-debug-ring", class_maintenance,
			sopts_cluster_debug_ring, &sopt_cluster_debug_ring,
			_ ("Set cluster debug ring"),
			_ ("Show cluster debug ring"), NULL,
			set_cluster_debug_ring, show_cluster_debug_ring,
			&kalray_set_cmdlist, &kalray_show_cmdlist);

  add_setshow_boolean_cmd ("compact-asm", class_maintenance, &opt_compact_asm,
			   _ ("Set compact assembly."),
			   _ ("Show compact assembly."), NULL, set_compact_asm,
			   show_compact_asm, &kalray_set_cmdlist,
			   &kalray_show_cmdlist);

  add_prefix_cmd (
    "intercept-traps", class_maintenance, set_kalray_traps_cmd,
    _ ("Configure what traps should be intercepted for the current cluster."),
    &kalray_set_traps_cmdlist, 0 /* allow-unknown */, &kalray_set_cmdlist);

  add_prefix_cmd (
    "intercept-traps", class_maintenance, show_kalray_traps_cmd,
    _ ("Configure what traps should be intercepted for the current cluster."),
    &kalray_show_traps_cmdlist, 0 /* allow-unknown */, &kalray_show_cmdlist);

  for (i = 0; i < no_traps_name; i++)
    {
      char trap_set_doc[100], trap_show_doc[100];

      sprintf (trap_set_doc, _ ("Set intercept %s trap."), traps_name[i]);
      sprintf (trap_show_doc, _ ("Show intercept %s trap."), traps_name[i]);
      add_setshow_boolean_cmd (traps_name[i], class_maintenance, &opt_trap,
			       trap_set_doc, trap_show_doc, NULL,
			       set_intercept_trap, show_intercept_trap,
			       &kalray_set_traps_cmdlist,
			       &kalray_show_traps_cmdlist);
    }
  add_setshow_uinteger_cmd ("mask", class_maintenance,
			    (unsigned int *) &opt_trap,
			    _ ("Set intercept traps mask."),
			    _ ("Show intercept traps mask."), NULL,
			    set_intercept_trap_mask, show_intercept_trap_mask,
			    &kalray_set_traps_cmdlist,
			    &kalray_show_traps_cmdlist);

  add_com ("run-mppa", class_run, run_mppa_command,
	   _ ("Connect to a MPPA TLM platform and start debugging it."));

  add_com ("mppa-dump-tlb", class_run, mppa_dump_tlb_command,
	   _ ("Dump TLB. Syntax:\nmppa-dump-tlb [--jtlb] [--ltlb] "
	      "[--valid-only] [--global] [--asn=<asn>]\nIf none of "
	      "--jtlb and --ltlb are given, the both are dumped."));

  add_com ("mppa-lookup-addr", class_run, mppa_lookup_addr_command,
	   _ ("Translate virtual address to/from physical address using the "
	      "TLB entries. "
	      "Syntax:\nmppa-lookup-addr --phys=<addr>|--virt=<addr> "
	      "[--asn=<asn>]\nIf --asn is not specified, "
	      "display all matching entries."));

  kvx_ha_init (&kalray_set_cmdlist, &kalray_show_cmdlist);
  kvx_ra_init (&kalray_set_cmdlist, &kalray_show_cmdlist);

  gdb::observers::inferior_created.attach (kvx_inf_added, "kvx");
  gdb::observers::inferior_added.attach (kvx_inf_added, "kvx");
}
