 /*
  * UAE - The Un*x Amiga Emulator
  *
  * Pro-Wizard glue code
  *
  * Copyright 2004 Toni Wilen
  */

#include "sysconfig.h"
#include "sysdeps.h"

#ifdef PROWIZARD

#include "uae.h"
#include "config.h"
#include "options.h"
#include "memory.h"
#include "autoconf.h"
#include "moduleripper.h"

static int got, canceled;

static void mc (uae_u8 *d, uaecptr s, int size)
{
    int i;

    for (i = 0; i < size; i++)
	d[i] = get_byte (s++);
}

#ifdef _WIN32
static LONG WINAPI ExceptionFilter( struct _EXCEPTION_POINTERS * pExceptionPointers, DWORD ec)
{
    return EXCEPTION_EXECUTE_HANDLER;
}
#endif

void moduleripper (void)
{
    int size;
    uae_u8 *buf, *p;

    size = currprefs.chipmem_size;
    size += currprefs.fastmem_size;
    size += currprefs.bogomem_size;
    buf = p = xmalloc (size);
    memcpy (p, chipmemory, currprefs.chipmem_size);
    p += currprefs.chipmem_size;
    mc (p, 0x200000, currprefs.fastmem_size);
    p += currprefs.fastmem_size;
    mc (p, 0xc00000, currprefs.bogomem_size);

    got = 0;
    canceled = 0;
#ifdef _WIN32
    __try {
#endif
	prowizard_search (buf, size);
#ifdef _WIN32
    } __except(ExceptionFilter(GetExceptionInformation(), GetExceptionCode())) {
	write_log("prowizard scan crashed\n");
    }
#endif
    if (!got)
	gui_message("No modules or compressed data found");
    else if (!canceled)
	gui_message("Search finished");
    free (buf);
}

FILE *moduleripper_fopen (const char *name, const char *mode)
{
    return fopen (name, mode);
}

FILE *moduleripper2_fopen (const char *name, const char *mode, const char *id)
{
    char msg[1000];
    int ret;

    if (canceled)
	return NULL;
    got++;
    sprintf (msg, "Module/packer found\n%s\nDo you want to save it?", id);
    ret = gui_message_multibutton (2, msg);
    if (ret < 0)
	canceled = 1;
    if (ret < 0 || ret != 1)
	return NULL;
    return fopen (name, mode);
}

#endif