#include "defs.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include "gdbcore.h"
#include "remote.h"
#include "inferior.h"
#include "solist.h"
#include "objfiles.h"
#include "solib-kvx-bare.h"

struct target_so_ops kvx_bare_solib_ops;

#define DL_VERSION 2

struct mppa_dl_debug_s
{
  uint64_t version; /* Protocol version */
  uint64_t head;    /* Head of the shared object chain */
  uint64_t brk;	    /* Address of the breakpoint function */
  uint64_t valid;   /* The  shared object chain is valid */
} __attribute__ ((packed));

struct mppa_dl_debug_map_s
{
  uint64_t load_addr;	       /* Load address for the dynamic object */
  uint64_t file_name;	       /* File name of the dynamic object */
  uint64_t file_name_len;      /* File name length of the dynamic object */
  uint64_t text_sect_vma;      /* VMA of the .text section */
  uint64_t text_sect_size;     /* .text section size */
  uint64_t text_sect_checksum; /* The checksum value of the .text section */
  uint64_t next;	       /* Chain of loaded objects */
} __attribute__ ((packed));

/* KVX specific data */
struct kvx_lm_info : public lm_info_base
{
public:
  kvx_lm_info (CORE_ADDR addr) { this->lm_addr = addr; }

  kvx_lm_info (void) { this->lm_addr = 0; }

  kvx_lm_info (const kvx_lm_info &o) { this->lm_addr = o.lm_addr; }

public:
  CORE_ADDR lm_addr;
};

struct kvx_bare_solib_info
{
  CORE_ADDR brk;
  CORE_ADDR head_addr;
  CORE_ADDR valid_addr;
  int no_name_shown;
  struct so_list *last_head;
};

struct kvx_bare_solib_pspace_deleter
{
  void operator() (kvx_bare_solib_info *info) { xfree (info); }
};

/* Per-program-space data key. */
static const registry<program_space>::key<kvx_bare_solib_info,
					  kvx_bare_solib_pspace_deleter>
  kvx_bare_solib_pspace_data_key;

/* Get the current KVX data.  If none is found yet, add it now.  This
   function always returns a valid object */
static struct kvx_bare_solib_info *
kvx_bare_solib_get_info (void)
{
  struct kvx_bare_solib_info *info;

  info = kvx_bare_solib_pspace_data_key.get (current_program_space);
  if (info != NULL)
    return info;

  info = (struct kvx_bare_solib_info *) xzalloc (
    sizeof (struct kvx_bare_solib_info));
  kvx_bare_solib_pspace_data_key.set (current_program_space, info);

  return info;
}

void
kvx_bare_solib_load_debug_info (void)
{
  struct bound_minimal_symbol msym;
  CORE_ADDR mppa_dl_debug_addr;
  struct mppa_dl_debug_s dl_debug;
  struct kvx_bare_solib_info *info;
  struct obj_section *dl_debug_sect;

  info = kvx_bare_solib_get_info ();
  if (info->head_addr)
    return;

  msym = lookup_minimal_symbol ("mppa_dl_debug", NULL, NULL);
  if (msym.minsym == NULL)
    return;

  dl_debug_sect = msym.minsym->obj_section (msym.objfile);
  if (!dl_debug_sect || !dl_debug_sect->the_bfd_section)
    return;
  if (bfd_section_lma (dl_debug_sect->the_bfd_section)
      != bfd_section_vma (dl_debug_sect->the_bfd_section))
    {
      if (!kvx_is_mmu_enabled (NULL, NULL))
	return;
    }

  mppa_dl_debug_addr = msym.value_address ();

  if (target_read_memory (mppa_dl_debug_addr, (gdb_byte *) &dl_debug,
			  sizeof (struct mppa_dl_debug_s)))
    return;

  if (dl_debug.version != DL_VERSION)
    {
      warning ("solib-kvx: incorrect version %lld (expected %d)\n",
	       (unsigned long long) dl_debug.version, DL_VERSION);
      return;
    }

  info->brk = dl_debug.brk;
  info->head_addr
    = (CORE_ADDR) & ((struct mppa_dl_debug_s *) mppa_dl_debug_addr)->head;
  info->valid_addr
    = (CORE_ADDR) & ((struct mppa_dl_debug_s *) mppa_dl_debug_addr)->valid;

  create_solib_event_breakpoint (target_gdbarch (), info->brk);
}

/* The section table is built from bfd sections using bfd VMAs.
   Relocate these VMAs according to solib info */
static void
kvx_bare_solib_relocate_section_addresses (struct so_list *so,
					   struct target_section *sec)
{
  kvx_lm_info *li = (kvx_lm_info *) so->lm_info;

  sec->addr += li->lm_addr;
  sec->endaddr += li->lm_addr;

  // best effort to set addr_high/addr_low, used by 'info sharedlibary'.
  if (so->addr_high == 0)
    {
      so->addr_low = sec->addr;
      so->addr_high = sec->endaddr;
    }
  if (sec->endaddr > so->addr_high)
    so->addr_high = sec->endaddr;
  if (sec->addr < so->addr_low)
    so->addr_low = sec->addr;
}

static void
kvx_bare_solib_free_so (struct so_list *so)
{
  if (so->lm_info)
    {
      free (so->lm_info);
      so->lm_info = NULL;
    }
}

static void
kvx_bare_solib_clear_solib (void)
{
  struct kvx_bare_solib_info *info = kvx_bare_solib_get_info ();

  info->brk = 0;
  info->head_addr = 0;
  info->valid_addr = 0;
}

/* Shared library startup support.  See documentation in solib-svr4.c */
static void
kvx_bare_solib_solib_create_inferior_hook (int from_tty)
{
  kvx_bare_solib_clear_solib ();
  kvx_bare_solib_load_debug_info ();
}

static struct so_list *
kvx_bare_solib_dup_chain (struct so_list *head)
{
  struct so_list *dup_head = NULL, **p = &dup_head;

  for (; head; head = head->next)
    {
      *p = (struct so_list *) xzalloc (sizeof (struct so_list));
      memcpy (*p, head, sizeof (struct so_list));
      (*p)->lm_info = new kvx_lm_info (*(kvx_lm_info *) head->lm_info);
      (*p)->next = NULL;
      p = &(*p)->next;
    }

  return dup_head;
}

static void
kvx_bare_solib_free_chain (struct so_list *head)
{
  struct so_list *p;

  while (head)
    {
      p = head;
      head = head->next;

      kvx_bare_solib_free_so (p);
      xfree (p);
    }
}

static uint64_t
mppa_dl_checksum (const uint8_t *buf, int sz)
{
  uint64_t ret = 0;

  for (; sz; sz--, buf++)
    ret += *buf;

  return ret;
}

static int
kvx_bare_check_so (const char *file_name, struct mppa_dl_debug_map_s *mppadl)
{
  asection *text_sect;
  bfd *hbfd;
  uint64_t cs;
  int ret = 1;
  bfd_byte *text_data = NULL;
  gdb_bfd_ref_ptr abfd (solib_bfd_open (file_name));

  if (abfd == NULL)
    return 1;

  hbfd = abfd.get ();
  // check if the file is in format
  if (!bfd_check_format (hbfd, bfd_object))
    {
      printf_unfiltered ("Incompatible shared library file format %s\n",
			 file_name);
      goto label_end;
    }

  text_sect = bfd_get_section_by_name (hbfd, ".text");

  if (text_sect)
    {
      if (bfd_section_vma (text_sect) != mppadl->text_sect_vma)
	{
	  printf_unfiltered (
	    "The VMA (0x%lx) of the .text section of the shared library %s is "
	    "different from the VMA of the section loaded in the target memory "
	    "(0x%lx)\n",
	    bfd_section_vma (text_sect), file_name, mppadl->text_sect_vma);
	  goto label_end;
	}

      if (bfd_section_size (text_sect) != mppadl->text_sect_size)
	{
	  printf_unfiltered (
	    "The .text section size (0x%lx) of the shared library %s is "
	    "different from the size loaded in the target memory (0x%lx)\n",
	    bfd_section_size (text_sect), file_name, mppadl->text_sect_size);
	  goto label_end;
	}
    }

  if (!text_sect
      || !bfd_get_full_section_contents (hbfd, text_sect, &text_data))
    {
      printf_unfiltered (
	"Cannot get the .text section of the shared library %s\n", file_name);
      goto label_end;
    }

  cs = mppa_dl_checksum (text_data, mppadl->text_sect_size);
  if (cs != mppadl->text_sect_checksum)
    {
      printf_unfiltered (
	"The .text checksum of the shared library %s is different from the "
	"checksum of the section loaded in the target memory\n",
	file_name);
      goto label_end;
    }

  ret = 0;
label_end:
  if (text_data)
    free (text_data);

  return ret;
}

int
kvx_bare_get_core_range (CORE_ADDR *iter_ctx, CORE_ADDR *bottom, CORE_ADDR *top)
{
  if (!iter_ctx)
    return 1;

  struct mppa_dl_debug_map_s crt_debug_map;
  uint64_t crt_debug_map_addr = *iter_ctx;

  if (!crt_debug_map_addr)
    {
      if (inferior_ptid == null_ptid)
	return 1;

      struct kvx_bare_solib_info *info = kvx_bare_solib_get_info ();
      if (!info->brk || !info->head_addr || !info->valid_addr)
	{
	  kvx_bare_solib_load_debug_info ();
	  if (!info->brk || !info->head_addr || !info->valid_addr)
	    return 1;
	}

      uint64_t is_valid;
      if (target_read_memory (info->valid_addr, (gdb_byte *) &is_valid,
			      sizeof (is_valid))
	  || !is_valid)
	return 1;

      if (target_read_memory (info->head_addr, (gdb_byte *) &crt_debug_map_addr,
			      sizeof (crt_debug_map_addr)))
	return 1;
    }

  if (target_read_memory (crt_debug_map_addr, (gdb_byte *) &crt_debug_map,
			  sizeof (struct mppa_dl_debug_map_s)))
    return 1;

  *iter_ctx = crt_debug_map.next;

  *bottom = crt_debug_map_addr;
  *top = crt_debug_map_addr + sizeof (struct mppa_dl_debug_map_s);

  if (crt_debug_map.file_name < *bottom)
    *bottom = crt_debug_map.file_name;

  if (crt_debug_map.file_name + crt_debug_map.file_name_len > *top)
    *top = crt_debug_map.file_name + crt_debug_map.file_name_len;

  return 0;
}

/* Build a list of currently loaded shared objects.  See solib-svr4.c */
static struct so_list *
kvx_bare_solib_current_sos (void)
{
  struct kvx_bare_solib_info *info;
  struct mppa_dl_debug_map_s crt_debug_map;
  uint64_t crt_debug_map_addr, is_valid;
  char crt_file_name[SO_NAME_MAX_PATH_SIZE];
  struct so_list *sl;
  struct kvx_lm_info *li;
  struct so_list *head = NULL, **ptail = &head;
  int len_target_prefix = strlen (TARGET_SYSROOT_PREFIX);

  info = kvx_bare_solib_get_info ();
  if (!info->brk || !info->head_addr || !info->valid_addr)
    {
      kvx_bare_solib_load_debug_info ();
      if (!info->brk || !info->head_addr || !info->valid_addr)
	return NULL;
    }

  if (inferior_ptid == null_ptid)
    return NULL;

  if (target_read_memory (info->valid_addr, (gdb_byte *) &is_valid,
			  sizeof (is_valid)))
    return NULL;

  if (!is_valid)
    {
      if (!info->last_head)
	return NULL;

      return kvx_bare_solib_dup_chain (info->last_head);
    }
  kvx_bare_solib_free_chain (info->last_head);

  if (target_read_memory (info->head_addr, (gdb_byte *) &crt_debug_map_addr,
			  sizeof (crt_debug_map_addr)))
    return NULL;
  for (; crt_debug_map_addr; crt_debug_map_addr = crt_debug_map.next)
    {
      int len_search = 0;

      if (target_read_memory (crt_debug_map_addr, (gdb_byte *) &crt_debug_map,
			      sizeof (struct mppa_dl_debug_map_s)))
	break;

      if (!crt_debug_map.file_name || !crt_debug_map.file_name_len)
	{
	  if (!info->no_name_shown)
	    {
	      printf_unfiltered ("Inferior %d loaded a library without name.\n"
				 "Please add -Wl,-soname=<lib_file.so> to the "
				 "library link flags.\n",
				 inferior_ptid.pid ());
	      info->no_name_shown = 1;
	    }
	  continue;
	}

      if (!solib_search_path.empty ()
	  && startswith (solib_search_path.c_str (), TARGET_SYSROOT_PREFIX "/"))
	{
	  /* read the first char of the so path */
	  if (target_read_memory (crt_debug_map.file_name,
				  (gdb_byte *) crt_file_name, 1))
	    continue;

	  /* add solib-search-path only if the remote path is relative */
	  if (*crt_file_name != '/')
	    {
	      len_search = solib_search_path.length () - len_target_prefix;
	      if (len_search + 1 < SO_NAME_MAX_PATH_SIZE)
		strcpy (crt_file_name,
			solib_search_path.c_str () + len_target_prefix);
	      if (crt_file_name[len_search - 1] != '/')
		crt_file_name[len_search++] = '/';
	    }
	}

      if (crt_debug_map.file_name_len + len_search >= SO_NAME_MAX_PATH_SIZE)
	continue;

      if (target_read_memory (crt_debug_map.file_name,
			      (gdb_byte *) &crt_file_name[len_search],
			      crt_debug_map.file_name_len))
	continue;

      if (kvx_bare_check_so (crt_file_name, &crt_debug_map))
	continue;

      sl = (struct so_list *) xzalloc (sizeof (struct so_list));
      li = new kvx_lm_info (crt_debug_map.load_addr);
      sl->lm_info = li;
      strncpy (sl->so_original_name, crt_file_name, SO_NAME_MAX_PATH_SIZE);
      strncpy (sl->so_name, crt_file_name, SO_NAME_MAX_PATH_SIZE);

      *ptail = sl;
      ptail = &(*ptail)->next;
    }

  info->last_head = head;

  return kvx_bare_solib_dup_chain (head);
}

/* Return 1 if PC lies in the dynamic symbol resolution code of the run time
 * loader */
static int
kvx_bare_solib_in_dynsym_resolve_code (CORE_ADDR pc)
{
  return 0;
}

/*  Not used */
static int
kvx_bare_solib_open_symbol_file_object (int from_ttyp)
{
  return 0;
}

extern initialize_file_ftype _initialize_kvx_bare_solib;

void
_initialize_kvx_bare_solib ()
{
  kvx_bare_solib_ops.relocate_section_addresses
    = kvx_bare_solib_relocate_section_addresses;
  kvx_bare_solib_ops.free_so = kvx_bare_solib_free_so;
  kvx_bare_solib_ops.clear_solib = kvx_bare_solib_clear_solib;
  kvx_bare_solib_ops.solib_create_inferior_hook
    = kvx_bare_solib_solib_create_inferior_hook;
  kvx_bare_solib_ops.current_sos = kvx_bare_solib_current_sos;
  kvx_bare_solib_ops.open_symbol_file_object
    = kvx_bare_solib_open_symbol_file_object;
  kvx_bare_solib_ops.in_dynsym_resolve_code
    = kvx_bare_solib_in_dynsym_resolve_code;
  kvx_bare_solib_ops.bfd_open = solib_bfd_open;
}
