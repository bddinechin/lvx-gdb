#include "defs.h"
#include <stdio.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include "inferior.h"
#include "remote.h"
#include "utils.h"
#include "gdbthread.h"
#include "gdbsupport/buildargv.h"
#include "kvx-target.h"
#include "kvx-dump-tlb.h"

#define ARRAY_SIZE(a) (sizeof (a) / sizeof ((a)[0]))
#define MSG_FROM_ARRAY(a, i)                                                   \
  ((i >= ARRAY_SIZE (a) || !a[i]) ? "[unknown]" : a[i])

#define MMU_JTLB_SETS 64
#define MMU_JTLB_WAYS 4
#define MMU_LTLB_SETS 1
#define MMU_LTLB_WAYS 16
#define MMC_SB_JTLB 0
#define MMC_SB_LTLB 1
#define TLB_ES_INVALID 0
#define MMC_E_MASK 0x80000000
#define MMC_PAR_MASK 0x40000000
#define TEH_PN_SHIFT 12
#define TEL_FN_SHIFT 12
#define VIRT_ADDR_BITS 41
#define PHYS_ADDR_BITS 40

union __attribute__ ((__packed__)) tel_s
{
  struct
  {
    uint64_t es : 2;  /* Entry Status */
    uint64_t cp : 2;  /* Cache Policy */
    uint64_t pa : 4;  /* Protection Attributes */
    uint64_t r : 2;   /* Reserved */
    uint64_t ps : 2;  /* Page Size */
    uint64_t fn : 28; /* Frame Number */
  } v1;
  struct
  {
    uint64_t es : 2;  /* Entry Status */
    uint64_t cp2 : 4; /* Cache Policy */
    uint64_t pa2 : 4; /* Protection Attributes */
    uint64_t ps : 2;  /* Page Size */
    uint64_t fn : 28; /* Frame Number */
  } v2;
  uint64_t value;
};

union __attribute__ ((__packed__)) teh_s
{
  struct
  {
    uint64_t asn : 9; /* Adress Space Number */
    uint64_t g : 1;   /* Global Indicator */
    uint64_t vs : 2;  /* Virtual Space */
    uint64_t pn : 29; /* Page Number */
  } _;
  uint64_t value;
};

struct tlb_entry_s
{
  union tel_s tel;
  union teh_s teh;
};

typedef void (*tlb_iteration_cb_fc) (struct tlb_entry_s *e, int tlb_type,
				     int iset, int iway, void *pvoid_opt,
				     int target_ver, int core_ver);

struct dump_tlb_opt_s
{
  int asn;
  int global;
  int valid_only;
};

struct lookup_addr_opt_s
{
  uint64_t addr;
  int virt;
  int asn;
};

static int
readtlb_entry (int tlb_type, int iset, int iway, struct tlb_entry_s *e,
	       uint64_t *mmc)
{
  char *buf = (char *) malloc (512);
  long size = 512;
  remote_target *rt = get_current_remote_target ();

  sprintf (buf, "qkalray.tlb:p%x.%lx:%d:%d:%d", inferior_ptid.pid (),
	   inferior_ptid.lwp (), tlb_type, iset, iway);
  putpkt (rt, buf);
  getpkt (rt, &buf, &size, 0);

  sscanf (buf, "%llx %llx %llx", (unsigned long long *) &e->tel.value,
	  (unsigned long long *) &e->teh.value, (unsigned long long *) mmc);

  return 0;
}

static const char *tel_es_msg[] = {
  [0] = "(invalid)   ",
  [1] = "(present)   ",
  [2] = "(modified)  ",
  [3] = "(a-modified)",
};

static const char *tel_ps_msg[] = {
  [0] = "(4KB)  ",
  [1] = "(64KB) ",
  [2] = "(2MB)  ",
  [3] = "(512MB)",
};

static uint64_t tel_ps_vals[] = {
  [0] = 4ULL * 1024,
  [1] = 64ULL * 1024,
  [2] = 2ULL * 1024 * 1024,
  [3] = 512ULL * 1024 * 1024,
};

static const char *tel_pa_msg[] = {
  [0] = "(U:NA/K:NA)  ",  [1] = "(U:NA/K:R)   ",  [2] = "(U:NA/K:RW)  ",
  [3] = "(U:NA/K:RX)  ",  [4] = "(U:NA/K:RWX) ",  [5] = "(U:R/K:R)    ",
  [6] = "(U:R/K:RW)   ",  [7] = "(U:R/K:RX)   ",  [8] = "(U:R/K:RWX)  ",
  [9] = "(U:RW/K:RW)  ",  [10] = "(U:RW/K:RWX) ", [11] = "(U:RX/K:RX)  ",
  [12] = "(U:RX/K:RWX) ", [13] = "(U:RWX/K:RWX)", [14] = "Res_14",
  [15] = "Res_15",
};

static const char *tel_cp_msg[] = {
  [0] = "(D:device/I:uncached)     ",
  [1] = "(D:uncached/I:uncached)   ",
  [2] = "(D:write_through/I:cached)",
  [3] = "(D:uncached/I:cached)     ",
};

static const char *tel_cp2_msg[] = {
  [0] = "(T:memory/L2ID:uncached/L1D:uncached/L1L2I:uncached)  ",
  [1] = "(T:memory/L2ID:uncached/L1D:uncached/L1L2I:cached)    ",
  [2] = "(T:memory/L2ID:uncached/L1D:cached/L1L2I:uncached)    ",
  [3] = "(T:memory/L2ID:uncached/L1D:cached/L1L2I:cached)      ",
  [4] = "(T:memory/L2ID:cached/L1D:uncached/L1L2I:uncached)    ",
  [5] = "(T:memory/L2ID:cached/L1D:uncached/L1L2I:cached)      ",
  [6] = "(T:memory/L2ID:cached/L1D:cached/L1L2I:uncached)      ",
  [7] = "(T:memory/L2ID:cached/L1D:cached/L1L2I:cached)        ",
  [8] = "(T:device/L2ID:uncached/L1D:uncached/L1L2I:uncached)  ",
  [9] = "(Res_9/L2ID:unpredict/L1D:unpredict/L1L2I:unpredict)  ",
  [10] = "(Res_10/L2ID:unpredict/L1D:unpredict/L1L2I:unpredict) ",
  [11] = "(Res_11/L2ID:unpredict/L1D:unpredict/L1L2I:unpredict) ",
  [12] = "(Res_12/L2ID:unpredict/L1D:unpredict/L1L2I:unpredict) ",
  [13] = "(Res_13/L2ID:unpredict/L1D:unpredict/L1L2I:unpredict) ",
  [14] = "(Res_14/L2ID:unpredict/L1D:unpredict/L1L2I:unpredict) ",
  [15] = "(Res_15/L2ID:unpredict/L1D:unpredict/L1L2I:unpredict) ",
};

static void
print_tlb_entry (struct tlb_entry_s *e, int tlb_type, int iset, int iway,
		 int target_ver, int core_ver)
{
  if (target_ver == 3 && core_ver == 1)
    {
      printf (
	"%s[s:%02d w:%02d]: PN:%09lx | FN:%09lx | PS:%lu %s | "
	"G:%lu | ASN:%03lu | VS:%02lu | PA:%02lu %s | CP:%lu %s | ES:%lu %s\n",
	(tlb_type == MMC_SB_JTLB) ? "JTLB" : "LTLB", iset, iway,
	(unsigned long) e->teh._.pn, (unsigned long) e->tel.v1.fn,
	(unsigned long) e->tel.v1.ps, MSG_FROM_ARRAY (tel_ps_msg, e->tel.v1.ps),
	(unsigned long) e->teh._.g, (unsigned long) e->teh._.asn,
	(unsigned long) e->teh._.vs, (unsigned long) e->tel.v1.pa,
	MSG_FROM_ARRAY (tel_pa_msg, e->tel.v1.pa), (unsigned long) e->tel.v1.cp,
	MSG_FROM_ARRAY (tel_cp_msg, e->tel.v1.cp), (unsigned long) e->tel.v1.es,
	MSG_FROM_ARRAY (tel_es_msg, e->tel.v1.es));
    }
  else
    {
      printf ("%s[s:%02d w:%02d]: PN:%09lx | FN:%09lx | PS:%lu %s | "
	      "G:%lu | ASN:%03lu | VS:%02lu | PA2:%02lu %s | CP2:%lu %s | "
	      "ES:%lu %s\n",
	      (tlb_type == MMC_SB_JTLB) ? "JTLB" : "LTLB", iset, iway,
	      (unsigned long) e->teh._.pn, (unsigned long) e->tel.v2.fn,
	      (unsigned long) e->tel.v2.ps,
	      MSG_FROM_ARRAY (tel_ps_msg, e->tel.v2.ps),
	      (unsigned long) e->teh._.g, (unsigned long) e->teh._.asn,
	      (unsigned long) e->teh._.vs, (unsigned long) e->tel.v2.pa2,
	      MSG_FROM_ARRAY (tel_pa_msg, e->tel.v2.pa2),
	      (unsigned long) e->tel.v2.cp2,
	      MSG_FROM_ARRAY (tel_cp2_msg, e->tel.v2.cp2),
	      (unsigned long) e->tel.v2.es,
	      MSG_FROM_ARRAY (tel_es_msg, e->tel.v2.es));
    }
}

static void
dump_tlb (struct tlb_entry_s *e, int tlb_type, int iset, int iway,
	  void *pvoid_opt, int target_ver, int core_ver)
{
  struct dump_tlb_opt_s *opt = (struct dump_tlb_opt_s *) pvoid_opt;
  uint64_t tel_es = (target_ver == 3 && core_ver == 1) ? e->tel.v1.es : e->tel.v2.es;

  if ((opt->asn == -1 || e->teh._.asn == opt->asn)
      && (!opt->global || e->teh._.g)
      && (!opt->valid_only || tel_es != TLB_ES_INVALID))
    {
      print_tlb_entry (e, tlb_type, iset, iway, target_ver, core_ver);
    }
}

static void
lookup_addr (struct tlb_entry_s *e, int tlb_type, int iset, int iway,
	     void *pvoid_opt, int target_ver, int core_ver)
{
  struct lookup_addr_opt_s *opt = (struct lookup_addr_opt_s *) pvoid_opt;
  uint64_t addr, ofs, translated_addr, tlb_begin;
  uint64_t tel_es, tel_ps, tel_fn;

  if (target_ver == 3 && core_ver == 1)
    {
      tel_es = e->tel.v1.es;
      tel_ps = e->tel.v1.ps;
      tel_fn = e->tel.v1.fn;
    }
  else
    {
      tel_es = e->tel.v2.es;
      tel_ps = e->tel.v2.ps;
      tel_fn = e->tel.v2.fn;
    }

  if (tel_es == TLB_ES_INVALID)
    return;

  if (!e->teh._.g && opt->asn != -1 && e->teh._.asn != opt->asn)
    return;

  if (opt->virt)
    {
      // virtual to physical translation
      tlb_begin = ((unsigned long long) e->teh._.pn) << TEH_PN_SHIFT;
      addr = opt->addr & ((1ULL << VIRT_ADDR_BITS) - 1);
      if (addr < tlb_begin || addr >= tlb_begin + tel_ps_vals[tel_ps])
	return;

      print_tlb_entry (e, tlb_type, iset, iway, target_ver, core_ver);
      ofs = opt->addr & (tel_ps_vals[tel_ps] - 1);
      translated_addr = (((unsigned long long) tel_fn) << TEL_FN_SHIFT) + ofs;
      printf ("\tvirtual address 0x%llx -> physical address 0x%llx\n\n",
	      (unsigned long long) opt->addr,
	      (unsigned long long) translated_addr);
    }
  else
    {
      // physical to virtual translation
      tlb_begin = ((unsigned long long) tel_fn) << TEL_FN_SHIFT;
      if (opt->addr < tlb_begin || opt->addr >= tlb_begin + tel_ps_vals[tel_ps])
	return;
      print_tlb_entry (e, tlb_type, iset, iway, target_ver, core_ver);
      ofs = opt->addr & (tel_ps_vals[tel_ps] - 1);
      translated_addr
	= (((unsigned long long) e->teh._.pn) << TEH_PN_SHIFT) + ofs;
      printf ("\tphysical address 0x%llx -> virtual address 0x%llx",
	      (unsigned long long) opt->addr,
	      (unsigned long long) translated_addr);
      if (translated_addr & (1ULL << (VIRT_ADDR_BITS - 1)))
	printf (" (0x%llx with sign extension)",
		translated_addr | (-1ULL << VIRT_ADDR_BITS));
      printf ("\n\n");
    }
}

static void
iterate_over_tlbs (int tlb_type, tlb_iteration_cb_fc cb, void *cb_options,
		   int target_ver, int core_ver)
{
  int no_sets, no_ways, iset, iway;
  struct tlb_entry_s e;
  uint64_t mmc;

  if (tlb_type == MMC_SB_JTLB)
    {
      no_sets = MMU_JTLB_SETS;
      no_ways = MMU_JTLB_WAYS;
    }
  else
    {
      no_sets = MMU_LTLB_SETS;
      no_ways = MMU_LTLB_WAYS;
    }

  for (iset = 0; iset < no_sets; iset++)
    for (iway = 0; iway < no_ways; iway++)
      {
	readtlb_entry (tlb_type, iset, iway, &e, &mmc);

	if (mmc & MMC_E_MASK)
	  {
	    printf (
	      "%s[s:%02d w:%02d]: error reading the entry (parity error: %s)\n",
	      (tlb_type == MMC_SB_JTLB) ? "JTLB" : "LTLB", iset, iway,
	      (mmc & MMC_PAR_MASK) ? "yes" : "no");
	  }
	else
	  {
	    (*cb) (&e, tlb_type, iset, iway, cb_options, target_ver, core_ver);
	  }
      }
}

void
mppa_dump_tlb_command (const char *args, int from_tty)
{
  int target_ver, core_ver;
  struct dump_tlb_opt_s opt = {.asn = -1, .global = 0, .valid_only = 0};
  int jtlb = -1, ltlb = -1;
  gdb_argv build_argv (args);
  char **targv, **argv = build_argv.get ();
  const char *asn_str = "--asn=";
  int i, argc = 0, len_asn_str = strlen (asn_str);

  if (inferior_ptid == null_ptid || inferior_thread ()->state == THREAD_RUNNING)
    {
      printf (_ ("Cannot dump the TLB without a stopped thread.\n"));
      return;
    }

  get_kvx_target_core_vers (&target_ver, &core_ver);

  if (argv)
    {
      for (targv = argv; *targv; targv++)
	;
      argc = targv - argv;

      if (argc && !*argv[argc - 1])
	argc--;
    }

  for (i = 0; i < argc; i++)
    {
      char *crt_arg = argv[i];

      if (!strcmp (crt_arg, "--jtlb"))
	{
	  jtlb = 1;
	  if (ltlb == -1)
	    ltlb = 0;
	}
      else if (!strcmp (crt_arg, "--ltlb"))
	{
	  ltlb = 1;
	  if (jtlb == -1)
	    jtlb = 0;
	}
      else if (!strcmp (crt_arg, "--valid-only"))
	opt.valid_only = 1;
      else if (!strcmp (crt_arg, "--global"))
	opt.global = 1;
      else if (!strncmp (crt_arg, asn_str, len_asn_str))
	{
	  char *endp, *sasn = crt_arg + len_asn_str;

	  opt.asn = strtol (sasn, &endp, 0);
	  if (endp == sasn || (*endp && *endp != ' '))
	    {
	      printf (_ ("The specified asn (%s) is invalid.\n"), sasn);
	      return;
	    }
	}
      else
	{
	  printf (_ ("Unknown option %s.\n"), crt_arg);
	  return;
	}
    }

  if (jtlb)
    iterate_over_tlbs (MMC_SB_JTLB, &dump_tlb, &opt, target_ver, core_ver);

  if (ltlb)
    iterate_over_tlbs (MMC_SB_LTLB, &dump_tlb, &opt, target_ver, core_ver);
}

void
mppa_lookup_addr_command (const char *args, int from_tty)
{
  int target_ver, core_ver;
  struct lookup_addr_opt_s opt = {0, 0, -1};
  int phys = 0;
  gdb_argv build_argv (args);
  char *endp, **targv, *saddr = NULL, **argv = build_argv.get ();
  const char *asn_str = "--asn=", *phys_str = "--phys=", *virt_str = "--virt=";
  int len_asn_str = strlen (asn_str), len_phys_str = strlen (phys_str),
      len_virt_str = strlen (virt_str);
  int i, argc = 0;

  if (inferior_ptid == null_ptid || inferior_thread ()->state == THREAD_RUNNING)
    {
      printf (_ ("Cannot lookup an address without a stopped thread.\n"));
      return;
    }

  get_kvx_target_core_vers (&target_ver, &core_ver);

  if (argv)
    {
      for (targv = argv; *targv; targv++)
	;
      argc = targv - argv;

      if (argc && !*argv[argc - 1])
	argc--;
    }

  for (i = 0; i < argc; i++)
    {
      char *crt_arg = argv[i];

      if (!strncmp (crt_arg, phys_str, len_phys_str))
	{
	  phys = 1;
	  saddr = crt_arg + len_phys_str;
	}
      else if (!strncmp (crt_arg, virt_str, len_virt_str))
	{
	  opt.virt = 1;
	  saddr = crt_arg + len_virt_str;
	}
      else if (!strncmp (crt_arg, asn_str, len_asn_str))
	{
	  char *sasn = crt_arg + len_asn_str;
	  opt.asn = strtol (sasn, &endp, 0);
	  if (endp == sasn || (*endp && *endp != ' '))
	    {
	      printf (_ ("The specified asn (%s) is invalid.\n"), sasn);
	      return;
	    }
	}
      else
	{
	  printf (_ ("Unknown option %s.\n"), crt_arg);
	  return;
	}
    }

  if (!(phys ^ opt.virt))
    {
      printf (_ ("Exactly one of the options %s and %s must be specified.\n"),
	      phys_str, virt_str);
      return;
    }

  opt.addr = strtoull (saddr, &endp, 0);
  if (endp == saddr || (*endp && *endp != ' '))
    {
      printf (_ ("The specified address (%s) is invalid.\n"), saddr);
      return;
    }

  iterate_over_tlbs (MMC_SB_JTLB, &lookup_addr, &opt, target_ver, core_ver);
  iterate_over_tlbs (MMC_SB_LTLB, &lookup_addr, &opt, target_ver, core_ver);
}
