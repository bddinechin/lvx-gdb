/* First line, to ensure we load the POSIX version of basename(). */
#include <libgen.h>

#include "kvx-attach.h"

#include <unistd.h>

#include <cstdint>
#include <string>
#include <vector>

#include "gdbcmd.h"
#include "gdbcore.h"
#include "observable.h"
#include "remote.h"
#include "user-regs.h"

#include "kvx-rv-debug.h"

static bool kvx_ra_opt = false;

#define KVX_RA_BP_NAME "runner_load_done"

#define RUNNER_LOAD_DATA_MAGIC 0x10ADDA7A
#define RUNNER_LOAD_DATA_VERSION 1
#define RUNNER_RV_PATH_MAX 1000

struct runner_load_data_s
{
  uint32_t magic;
  uint32_t version;
  uint8_t ignore_path;
  char rv_elf_path[RUNNER_RV_PATH_MAX];
  uint64_t elf_ptr;
  uint64_t elf_size;
} __attribute__ ((packed));

struct kvx_runner_filenames : public std::vector<std::string>
{
  ~kvx_runner_filenames ()
  {
    for (std::string const &s : *this)
      unlink (s.c_str ());
  }
} runner_filenames;

static void
kvx_ra_on_bp_reached (struct gdbarch *gdbarch, struct bpstat *bs,
		      int print_frame)
{
  struct regcache *rc = get_current_regcache ();
  int r0_num = user_reg_map_name_to_regnum (gdbarch, "r0", -1);
  LONGEST r0 = regcache_raw_get_signed (rc, r0_num);

  struct runner_load_data_s load_data;
  size_t load_data_size = sizeof (load_data);
  memset (&load_data, 0, load_data_size);
  read_memory ((CORE_ADDR) r0, (gdb_byte *) &load_data, load_data_size);

  if (load_data.magic != RUNNER_LOAD_DATA_MAGIC)
    {
      gdb_printf ("Error: incorrect runner_load_data magic, expected %d got "
		  "%d\n",
		  RUNNER_LOAD_DATA_MAGIC, load_data.magic);
      return;
    }

  if (load_data.version != RUNNER_LOAD_DATA_VERSION)
    {
      gdb_printf ("Error: incorrect runner_load_data version, expected %d got "
		  "%d\n",
		  RUNNER_LOAD_DATA_VERSION, load_data.version);
      return;
    }

  const size_t local_filename_len = 1024;
  char local_filename[local_filename_len];

  {
    char buffer[RUNNER_RV_PATH_MAX];
    strcpy (buffer, load_data.rv_elf_path);
    char *base = basename (buffer);
    snprintf (local_filename, local_filename_len,
	      "/tmp/kvx-gdb_rv_runner_%d_%s", getpid (), base);
  }

  if (load_data.ignore_path)
    {
      FILE *file = fopen (local_filename, "wb");
      if (!file)
	{
	  gdb_printf (
	    "Error: unable to open temporary file to write RV elf: %s\n",
	    strerror (errno));
	  return;
	}

      char *buffer = (char *) xmalloc (load_data.elf_size);
      read_memory ((CORE_ADDR) load_data.elf_ptr, (gdb_byte *) buffer,
		   load_data.elf_size);
      size_t res = fwrite (buffer, load_data.elf_size, 1, file);
      free (buffer);
      fclose (file);

      if (res != load_data.elf_size)
	{
	  gdb_printf ("Error: unable to write RV ELF to a temporary file\n");
	  return;
	}
    }
  else
    {
      remote_file_get (load_data.rv_elf_path, local_filename, 0);
    }

  char cmd_buffer[local_filename_len + 100];
  snprintf (cmd_buffer, local_filename_len + 100, "add-symbol-file %s",
	    local_filename);
  execute_command (cmd_buffer, 0);

  runner_filenames.push_back (std::string (local_filename));
}

static void
kvx_ra_cb_normal_stop (struct bpstat *bs, int print_frame)
{
  kvx_attach_normal_stop (KVX_RA_BP_NAME, kvx_ra_opt, &kvx_ra_on_bp_reached,
			  bfd_arch_kvx, bs, print_frame);
}

static void
kvx_ra_cb_new_objfile (struct objfile *objf)
{
  kvx_attach_new_objfile (KVX_RA_BP_NAME, kvx_ra_opt, bfd_arch_kvx, objf);
}

static void
set_ra_opt (const char *args, int from_tty, struct cmd_list_element *c)
{
  kvx_attach_set_opt (kvx_ra_opt, KVX_RA_BP_NAME, bfd_arch_kvx);
}

void
kvx_ra_init (struct cmd_list_element **kal_set,
	     struct cmd_list_element **kal_show)
{
  add_setshow_boolean_cmd (
    "auto-rv-debug", class_maintenance, &kvx_ra_opt,
    _ ("Set add Risc-V symbols and pause when the Risc-V runner is executed."),
    _ ("Show add Risc-V symbols and pause when the Risc-V runner is executed."),
    NULL, set_ra_opt, NULL, kal_set, kal_show);

  gdb::observers::new_objfile.attach (kvx_ra_cb_new_objfile, "rvx-runner");
  gdb::observers::normal_stop.attach (kvx_ra_cb_normal_stop, "rvx-runner");
}
