 /*
  * UAE - The Un*x Amiga Emulator
  *
  * routines to handle compressed file automatically
  *
  * (c) 1996 Samuel Devulder, Tim Gunn
  *     2002-2004 Toni Wilen
  */

#include "sysconfig.h"
#include "sysdeps.h"

#include "config.h"
#include "options.h"
#include "zfile.h"
#include "unzip.h"
#include "disk.h"
#include "dms/pfile.h"
#include "gui.h"
#include "crc32.h"

#include <zlib.h>

struct zfile {
    char *name;
    char *zipname;
    FILE *f;
    uae_u8 *data;
    int size;
    int seek;
    int deleteafterclose;
    struct zfile *next;
    int writeskipbytes;
};

static struct zfile *zlist = 0;
int is_zlib;

#ifdef _WIN32
static HMODULE zlib;
#include "win32.h"
#endif

INFLATEINIT2 pinflateInit2;
INFLATEINIT pinflateInit;
INFLATEEND pinflateEnd;
INFLATE pinflate;
static DEFLATEINIT pdeflateInit;
static DEFLATEEND pdeflateEnd;
static DEFLATE pdeflate;
CRC32 pcrc32;
static int zlib_test (int, int);

static struct zfile *zfile_create (void)
{
    struct zfile *z;

    z = malloc (sizeof *z);
    if (!z)
	return 0;
    memset (z, 0, sizeof *z);
    z->next = zlist;
    zlist = z;
    return z;
}

static void zfile_free (struct zfile *f)
{
    if (f->f)
	fclose (f->f);
    if (f->deleteafterclose) {
	unlink (f->name);
	write_log ("deleted temporary file '%s'\n", f->name);
    }
    xfree (f->name);
    xfree (f->data);
    xfree (f);
}

void zfile_exit (void)
{
    struct zfile *l;
    while ((l = zlist)) {
	zlist = l->next;
	zfile_free (l);
    }
}

void zfile_fclose (struct zfile *f)
{
    struct zfile *pl = NULL;
    struct zfile *l  = zlist;
    struct zfile *nxt;

    if (!f)
	return;
    while (l != f) {
	if (l == 0) {
	    write_log ("zfile: tried to free already freed filehandle!\n");
	    return;
	}
	pl = l;
	l = l->next;
    }
    if (l) nxt = l->next;
    zfile_free (f);
    if (l == 0)
	return;
    if(!pl)
	zlist = nxt;
    else
	pl->next = nxt;
}

static uae_u8 exeheader[]={0x00,0x00,0x03,0xf3,0x00,0x00,0x00,0x00};
int zfile_gettype (struct zfile *z)
{
    uae_u8 buf[8];
    char *ext;
    
    if (!z)
	return ZFILE_UNKNOWN;
    ext = strrchr (z->name, '.');
    if (ext != NULL) {
	ext++;
	if (strcasecmp (ext, "adf") == 0)
	    return ZFILE_DISKIMAGE;
	if (strcasecmp (ext, "adz") == 0)
	    return ZFILE_DISKIMAGE;
	if (strcasecmp (ext, "roz") == 0)
	    return ZFILE_ROM;
	if (strcasecmp (ext, "ipf") == 0)
	    return ZFILE_DISKIMAGE;
	if (strcasecmp (ext, "fdi") == 0)
	    return ZFILE_DISKIMAGE;
	if (strcasecmp (ext, "uss") == 0)
	    return ZFILE_STATEFILE;
	if (strcasecmp (ext, "dms") == 0)
	    return ZFILE_DISKIMAGE;
	if (strcasecmp (ext, "rom") == 0)
	    return ZFILE_ROM;
	if (strcasecmp (ext, "key") == 0)
	    return ZFILE_KEY;
	if (strcasecmp (ext, "nvr") == 0)
	    return ZFILE_NVR;
	if (strcasecmp (ext, "uae") == 0)
	    return ZFILE_CONFIGURATION;
    }
    memset (buf, 0, sizeof (buf));
    zfile_fread (buf, 8, 1, z);
    zfile_fseek (z, -8, SEEK_CUR);
    if (!memcmp (buf, exeheader, sizeof(buf)))
	return ZFILE_DISKIMAGE;
    return ZFILE_UNKNOWN;
}

#if 0
#define TMP_PREFIX "uae_"

static struct zfile *createinputfile (struct zfile *z)
{
    FILE *f;
    struct zfile *z2;
    char *name;

    z2 = zfile_create ();
    if (!z->data) {
	z2->name = strdup (z->name);
	return z2;
    }
    name = tempnam (0, TMP_PREFIX);
    f = fopen (name, "wb");
    if (!f) return 0;
    write_log ("created temporary file '%s'\n", name);
    fwrite (z->data, z->size, 1, f);
    fclose (f);
    z2->name = name;
    z2->deleteafterclose = 1;
    return z2;
}

static struct zfile *createoutputfile (struct zfile *z)
{
    struct zfile *z2;
    char *name;

    name = tempnam (0, TMP_PREFIX);
    z2 = zfile_create ();
    z2->name = name;
    z2->deleteafterclose = 1;
    write_log ("allocated temporary file name '%s'\n", name);
    return z2;
}

/* we want to delete temporary files as early as possible */
static struct zfile *updateoutputfile (struct zfile *z)
{
    struct zfile *z2 = 0;
    int size;
    FILE *f = fopen (z->name, "rb");
    for (;;) {
	if (!f)
	    break;
        fseek (f, 0, SEEK_END);
	size = ftell (f);
	fseek (f, 0, SEEK_SET);
	if (!size)
	    break;
        z2 = zfile_fopen_empty (z->name, size);
	if (!z2)
	    break;
        fread (z2->data, size, 1, f);
	fclose (f);
	zfile_fclose (z);
	return z2;
    }
    if (f)
	fclose (f);
    zfile_fclose (z);
    zfile_fclose (z2);
    return 0;
}
#endif

static struct zfile *zuncompress (struct zfile *z);

static struct zfile *gunzip (struct zfile *z)
{
    uae_u8 header[2 + 1 + 1 + 4 + 1 + 1];
    z_stream zs;
    int i, size, ret, first;
    uae_u8 flags;
    long offset;
    char name[MAX_DPATH];
    uae_u8 buffer[8192];
    struct zfile *z2;
    uae_u8 b;

    if (!zlib_test (1, 0))
	return z;
    strcpy (name, z->name);
    memset (&zs, 0, sizeof (zs));
    memset (header, 0, sizeof (header));
    zfile_fread (header, sizeof (header), 1, z);
    flags = header[3];
    if (header[0] != 0x1f && header[1] != 0x8b)
	return z;
    if (flags & 2) /* multipart not supported */
	return z;
    if (flags & 32) /* encryption not supported */
	return z;
    if (flags & 4) { /* skip extra field */
        zfile_fread (&b, 1, 1, z);
	size = b;
	zfile_fread (&b, 1, 1, z);
	size |= b << 8;
	zfile_fseek (z, size + 2, SEEK_CUR);
    }
    if (flags & 8) { /* get original file name */
	i = 0;
	do {
	    zfile_fread (name + i, 1, 1, z);
	} while (name[i++]);
    }
    if (flags & 16) { /* skip comment */
	i = 0;
	do {
	    zfile_fread (&b, 1, 1, z);
	} while (b);
    }
    offset = zfile_ftell (z);
    zfile_fseek (z, -4, SEEK_END);
    zfile_fread (&b, 1, 1, z);
    size = b;
    zfile_fread (&b, 1, 1, z);
    size |= b << 8;
    zfile_fread (&b, 1, 1, z);
    size |= b << 16;
    zfile_fread (&b, 1, 1, z);
    size |= b << 24;
    if (size < 8 || size > 10000000) /* safety check */
	return z;
    zfile_fseek (z, offset, SEEK_SET);
    z2 = zfile_fopen_empty (name, size);
    if (!z2)
	return z;
    zs.next_out = z2->data;
    zs.avail_out = size;
    first = 1;
    do {
	zs.next_in = buffer;
	zs.avail_in = sizeof (buffer);
	zfile_fread (buffer, sizeof (buffer), 1, z);
	if (first) {
	    if (pinflateInit2 (&zs, -MAX_WBITS, ZLIB_VERSION, sizeof(z_stream)) != Z_OK)
		break;
	    first = 0;
	}
	ret = pinflate (&zs, 0);
    } while (ret == Z_OK);
    pinflateEnd (&zs);
    if (ret != Z_STREAM_END || first != 0) {
	zfile_fclose (z2);
	return z;
    }
    zfile_fclose (z);
    return z2;
}


static struct zfile *bunzip (const char *decompress, struct zfile *z)
{
    return z;
}

static struct zfile *lha (struct zfile *z)
{
    return z;
}

static struct zfile *dms (struct zfile *z)
{
    int ret;
    struct zfile *zo;
    
    zo = zfile_fopen_empty ("zipped.dms", 1760 * 512);
    if (!zo) return z;
    ret = DMS_Process_File (z, zo, CMD_UNPACK, OPT_VERBOSE, 0, 0);
    if (ret == NO_PROBLEM || ret == DMS_FILE_END) {
	zfile_fclose (z);
	return zo;
    }
    return z;
}

#if 0
static struct zfile *dms (struct zfile *z)
{
    char cmd[2048];
    struct zfile *zi = createinputfile (z);
    struct zfile *zo = createoutputfile (z);
    if (zi && zo) {
	sprintf(cmd, "xdms -q u \"%s\" +\"%s\"", zi->name, zo->name);
	execute_command (cmd);
    }
    zfile_fclose (zi);
    zfile_fclose (z);
    return updateoutputfile (zo);
}
#endif

static char *ignoreextensions[] = 
    { ".gif", ".jpg", ".png", ".xml", ".pdf", ".txt", 0 };
static char *diskimageextensions[] =
    { ".adf", ".adz", ".ipf", ".fdi", 0 };

static int isdiskimage (char *name)
{
    int i;

    i = 0;
    while (diskimageextensions[i]) {
	if (strlen (name) > 3 && !strcasecmp (name + strlen (name) - 4, diskimageextensions[i]))
	    return 1;
	i++;
    }
    return 0;
}

struct aa_FILETIME
{
    uae_u32 dwLowDateTime;
    uae_u32 dwHighDateTime;
};
struct aa_FileInArchiveInfo {
    int ArchiveHandle;
    uae_u64 CompressedFileSize;
    uae_u64 UncompressedFileSize;
    int attributes;
    int IsDir;
    struct aa_FILETIME LastWriteTime;
    char path[MAX_DPATH];
};

typedef int (__stdcall *aa_ReadCallback)(int StreamID, uae_u64 offset, uae_u32 count, void* buf, uae_u32 *processedSize);
typedef int (__stdcall *aa_WriteCallback)(int StreamID, uae_u32 count, const void *buf, uae_u32 *processedSize);
typedef int (CALLBACK *aa_pOpenArchive)(aa_ReadCallback function, int StreamID, uae_u64 FileSize, int ArchiveType, int *result);
typedef int (CALLBACK *aa_pGetFileCount)(int ArchiveHandle);
typedef int (CALLBACK *aa_pGetFileInfo)(int ArchiveHandle, int FileNum, struct aa_FileInArchiveInfo *FileInfo);
typedef int (CALLBACK *aa_pExtract)(int ArchiveHandle, int FileNum, int StreamID, aa_WriteCallback WriteFunc);
typedef int (CALLBACK *aa_pCloseArchive)(int ArchiveHandle);

static aa_pOpenArchive openArchive;
static aa_pGetFileCount getFileCount;
static aa_pGetFileInfo getFileInfo;
static aa_pExtract extract;
static aa_pCloseArchive closeArchive;

#ifdef _WIN32
#include <windows.h>
#include "win32.h"
static HMODULE arcacc_mod;

static void arcacc_free (void)
{
    if (arcacc_mod)
	FreeLibrary (arcacc_mod);
    arcacc_mod = NULL;
}

static int arcacc_init (void)
{
    if (arcacc_mod)
	return 1;
    arcacc_mod = WIN32_LoadLibrary ("archiveaccess.dll"); 
    if (!arcacc_mod) {
	arcacc_mod = WIN32_LoadLibrary ("archiveaccess-debug.dll");
	if (!arcacc_mod)
	    return 0;
    }
    openArchive = (aa_pOpenArchive) GetProcAddress (arcacc_mod, "openArchive"); 
    getFileCount = (aa_pGetFileCount) GetProcAddress (arcacc_mod, "getFileCount");
    getFileInfo = (aa_pGetFileInfo) GetProcAddress (arcacc_mod, "getFileInfo"); 
    extract = (aa_pExtract) GetProcAddress (arcacc_mod, "extract");
    closeArchive = (aa_pCloseArchive) GetProcAddress (arcacc_mod, "closeArchive");
    if (!openArchive || !getFileCount || !getFileInfo || !extract || !closeArchive) {
	arcacc_free ();
	return 0;
    }
    return 1;
}
#endif

#define ARCACC_STACKSIZE 10
static struct zfile *arcacc_stack[ARCACC_STACKSIZE];
static int arcacc_stackptr = -1;

static int arcacc_push (struct zfile *f)
{
    if (arcacc_stackptr == ARCACC_STACKSIZE - 1)
	return -1;
    arcacc_stackptr++;
    arcacc_stack[arcacc_stackptr] = f;
    return arcacc_stackptr;
}
static void arcacc_pop (void)
{
    arcacc_stackptr--;
}

static int __stdcall readCallback (int StreamID, uae_u64 offset, uae_u32 count, void *buf, uae_u32 *processedSize)
{
    struct zfile *f = arcacc_stack[StreamID];
    int ret;

    zfile_fseek (f, (long)offset, SEEK_SET);
    ret = zfile_fread (buf, 1, count, f);
    if (processedSize)
	*processedSize = ret;
    return 0;
}
int __stdcall writeCallback (int StreamID, uae_u32 count, const void *buf, uae_u32 *processedSize)
{
    struct zfile *f = arcacc_stack[StreamID];
    int ret;

    ret = zfile_fwrite ((void*)buf, 1, count, f);
    if (processedSize)
	*processedSize = ret;
    if (ret != count)
	return -1;
    return 0;
}

static struct zfile *arcacc_unpack (struct zfile *z, int type)
{
    int ah, status, i, f;
    char tmphist[MAX_DPATH];
    int first = 1;
    int we_have_file = 0;
    int size, id_r, id_w;
    struct zfile *zf;
    int skipsize = 0;

    tmphist[0] = 0;
    zf = 0;
    if (!arcacc_init ())
	return z;
    id_r = arcacc_push (z);
    zfile_fseek (z, 0, SEEK_END);
    size = zfile_ftell (z);
    zfile_fseek (z, 0, SEEK_SET);
    ah = openArchive (readCallback, id_r, z->size, type, &status);
    if (!status) {
	int fc = getFileCount (ah);
	int zipcnt = 0;
	for (f = 0; f < fc; f++) {
	    struct aa_FileInArchiveInfo fi;
	    char *name;

	    zipcnt++;
	    memset (&fi, 0, sizeof (fi));
	    getFileInfo (ah, f, &fi);
	    if (fi.IsDir)
		continue;

	    name = fi.path;
	    for (i = 0; ignoreextensions[i]; i++) {
		if (strlen(name) > strlen (ignoreextensions[i]) &&
		    !strcasecmp (ignoreextensions[i], name + strlen (name) - strlen (ignoreextensions[i])))
		    break;
	    }
	    if (!ignoreextensions[i]) {
		int select = 0;
		if (tmphist[0]) {
		    DISK_history_add (tmphist, -1);
		    tmphist[0] = 0;
		    first = 0;
		}
		if (first) {
		    if (isdiskimage (name))
			sprintf (tmphist,"%s/%s", z->name, name);
		} else {
		    sprintf (tmphist,"%s/%s", z->name, name);
		    DISK_history_add (tmphist, -1);
		    tmphist[0] = 0;
		}
		if (!z->zipname)
		    select = 1;
		if (z->zipname && !strcasecmp (z->zipname, name))
		    select = -1;
		if (z->zipname && z->zipname[0] == '#' && atol (z->zipname + 1) == zipcnt)
		    select = -1;
		if (select && !we_have_file) {

		    zf = zfile_fopen_empty (name, (int)fi.UncompressedFileSize);
		    if (zf) {
			int err;
			zf->writeskipbytes = skipsize;
			id_w = arcacc_push (zf);
			err = extract (ah, f, id_w, writeCallback);
			if (zf->seek != fi.UncompressedFileSize)
			    write_log ("%s unpack failed, got only %d bytes\n", name, zf->seek);
			if (zf->seek == fi.UncompressedFileSize && (select < 0 || zfile_gettype (zf)))
		    	    we_have_file = 1;
			 if (!we_have_file) {
			    zfile_fclose (zf);
			    zf = 0;
			}
			arcacc_pop ();
		    }
		}
	    }
	    if (type == 7) {
		if (fi.CompressedFileSize)
		    skipsize = 0;
		skipsize += (int)fi.UncompressedFileSize;
	    }
	}   
    }
    closeArchive (ah);
    arcacc_pop ();
    if (zf) {
	zfile_fclose (z);
	z = zf;
	zfile_fseek (z, 0, SEEK_SET);
    }
    return z;
}

static int zlib_test (int needzlib, int nomsg)
{
    static int zlibmsg;
    if (is_zlib)
	return 1;
#ifdef _WIN32
    zlib = WIN32_LoadLibrary ("zlib1.dll");
    if (zlib) {
	pinflateInit2 = (INFLATEINIT2)GetProcAddress (zlib, "inflateInit2_");
	pinflateInit = (INFLATEINIT)GetProcAddress (zlib, "inflateInit_");
	pinflate = (INFLATE)GetProcAddress (zlib, "inflate");
	pinflateEnd = (INFLATEEND)GetProcAddress (zlib, "inflateEnd");
	pdeflateInit = (DEFLATEINIT)GetProcAddress (zlib, "deflateInit_");
	pdeflate = (DEFLATE)GetProcAddress (zlib, "deflate");
	pdeflateEnd = (DEFLATEEND)GetProcAddress (zlib, "deflateEnd");
	pcrc32 = (CRC32)GetProcAddress (zlib, "crc32");
	is_zlib = 1;
	return 1;
    }
#endif
    if (!needzlib && arcacc_init ())
	return 1;
    if (zlibmsg || nomsg)
	return 0;
    zlibmsg = 1;
    notify_user (NUMSG_NOZLIB);
    return 0;
}

static struct zfile *unzip (struct zfile *z)
{
    unzFile uz;
    unz_file_info file_info;
    char filename_inzip[2048];
    struct zfile *zf;
    int err, zipcnt, select, i, we_have_file = 0;
    char tmphist[MAX_DPATH];
    int first = 1;

    if (!zlib_test (1, 0))
	return z;
    zf = 0;
    uz = unzOpen (z);
    if (!uz)
	return z;
    if (unzGoToFirstFile (uz) != UNZ_OK)
	return z;
    zipcnt = 1;
    tmphist[0] = 0;
    for (;;) {
	err = unzGetCurrentFileInfo(uz,&file_info,filename_inzip,sizeof(filename_inzip),NULL,0,NULL,0);
	if (err != UNZ_OK)
	    return z;
	if (file_info.uncompressed_size > 0) {
	    i = 0;
	    while (ignoreextensions[i]) {
		if (strlen(filename_inzip) > strlen (ignoreextensions[i]) &&
		    !strcasecmp (ignoreextensions[i], filename_inzip + strlen (filename_inzip) - strlen (ignoreextensions[i])))
		    break;
		i++;
	    }
	    if (!ignoreextensions[i]) {
		if (tmphist[0]) {
		    DISK_history_add (tmphist, -1);
		    tmphist[0] = 0;
		    first = 0;
		}
		if (first) {
		    if (isdiskimage (filename_inzip))
			sprintf (tmphist,"%s/%s", z->name, filename_inzip);
		} else {
		    sprintf (tmphist,"%s/%s", z->name, filename_inzip);
		    DISK_history_add (tmphist, -1);
		    tmphist[0] = 0;
		}
		select = 0;
		if (!z->zipname)
		    select = 1;
		if (z->zipname && !strcasecmp (z->zipname, filename_inzip))
		    select = -1;
		if (z->zipname && z->zipname[0] == '#' && atol (z->zipname + 1) == zipcnt)
		    select = -1;
		if (select && !we_have_file) {
		    int err = unzOpenCurrentFile (uz);
		    if (err == UNZ_OK) {
			zf = zfile_fopen_empty (filename_inzip, file_info.uncompressed_size);
			if (zf) {
			    err = unzReadCurrentFile  (uz, zf->data, file_info.uncompressed_size);
			    unzCloseCurrentFile (uz);
			    if (err == 0 || err == file_info.uncompressed_size) {
				zf = zuncompress (zf);
				if (select < 0 || zfile_gettype (zf)) {
		    		    we_have_file = 1;
				}
			    }
			}
			if (!we_have_file) {
			    zfile_fclose (zf);
			    zf = 0;
			}
		    }
		}
	    }
	}
	zipcnt++;
	err = unzGoToNextFile (uz);
	if (err != UNZ_OK)
	    break;
    }
    if (zf) {
	zfile_fclose (z);
	z = zf;
    }
    return z;
}

static char *plugins_7z[] = { "7z", "rar", "zip", NULL };
static char *plugins_7z_x[] = { "7z", "Rar!", "MK" };
static int plugins_7z_t[] = { 7, 12, 11 };

static int iszip (struct zfile *z)
{
    char *name = z->name;
    char *ext = strrchr (name, '.');
    uae_u8 header[4];
    int i;

    if (!ext)
	return 0;
    memset (header, 0, sizeof (header));
    zfile_fseek (z, 0, SEEK_SET);
    zfile_fread (header, sizeof (header), 1, z);
    zfile_fseek (z, 0, SEEK_SET);
    if (!strcasecmp (ext, ".zip") && header[0] == 'P' && header[1] == 'K' && zlib_test (1, 1))
	return -1;
    for (i = 0; plugins_7z[i]; i++) {
	if (plugins_7z_x[i] && !strcasecmp (ext + 1, plugins_7z[i]) &&
	    !memcmp (header, plugins_7z_x[i], strlen (plugins_7z_x[i])))
		return plugins_7z_t[i];
    }
    return 0;
}

static struct zfile *zuncompress (struct zfile *z)
{
    char *name = z->name;
    char *ext = strrchr (name, '.');
    uae_u8 header[4];
    int i;

    if (ext != NULL) {
	ext++;
	if (strcasecmp (ext, "zip") == 0 && zlib_test (1, 1))
	     return unzip (z);
	if (strcasecmp (ext, "gz") == 0)
	     return gunzip (z);
	if (strcasecmp (ext, "adz") == 0)
	     return gunzip (z);
	if (strcasecmp (ext, "roz") == 0)
	     return gunzip (z);
	if (strcasecmp (ext, "dms") == 0)
	     return dms (z);
	for (i = 0; plugins_7z[i]; i++) {
    	    if (strcasecmp (ext, plugins_7z[i]) == 0)
		return arcacc_unpack (z, plugins_7z_t[i]);
	}
	if (strcasecmp (ext, "lha") == 0
	    || strcasecmp (ext, "lzh") == 0)
	    return lha (z);
	memset (header, 0, sizeof (header));
	zfile_fseek (z, 0, SEEK_SET);
	zfile_fread (header, sizeof (header), 1, z);
	zfile_fseek (z, 0, SEEK_SET);
	if (header[0] == 0x1f && header[1] == 0x8b)
	    return gunzip (z);
	if (header[0] == 'P' && header[1] == 'K')
	    return unzip (z);
	if (header[0] == 'D' && header[1] == 'M' && header[2] == 'S' && header[3] == '!')
	    return dms (z);
    }
    return z;
}

static FILE *openzip (char *name, char *zippath)
{
    int i, j;
    char v;
    
    i = strlen (name) - 2;
    if (zippath)
	zippath[0] = 0;
    while (i > 0) {
	if (name[i] == '/' || name[i] == '\\' && i > 4) {
	    v = name[i];
	    name[i] = 0;
	    for (j = 0; plugins_7z[j]; j++) {
		int len = strlen (plugins_7z[j]);
		if (name[i - len - 1] == '.' && !strcasecmp (name + i - len, plugins_7z[j])) {
		    FILE *f = fopen (name, "rb");
		    if (f) {
			if (zippath)
			    strcpy (zippath, name + i + 1);
			return f;
		    }
		    break;
		}
	    }
	    name[i] = v;
	}
	i--;
    }
    return 0;	    
}

#ifdef SINGLEFILE
extern uae_u8 singlefile_data[];

static struct zfile *zfile_opensinglefile(struct zfile *l)
{
    uae_u8 *p = singlefile_data;
    int size, offset;
    char tmp[256], *s;

    strcpy (tmp, l->name);
    s = tmp + strlen (tmp) - 1;
    while (*s != 0 && *s != '/' && *s != '\\') s--;
    if (s > tmp)
	s++;
    write_log("loading from singlefile: '%s'\n", tmp);
    while (*p++);
    offset = (p[0] << 24)|(p[1] << 16)|(p[2] << 8)|(p[3] << 0);
    p += 4;
    for (;;) {
	size = (p[0] << 24)|(p[1] << 16)|(p[2] << 8)|(p[3] << 0);
	if (!size)
	    break;
	if (!strcmpi (tmp, p + 4)) {
	    l->data = singlefile_data + offset;
	    l->size = size;
	    write_log ("found, size %d\n", size);
	    return l;
	}
	offset += size;
	p += 4;
	p += strlen (p) + 1;
    }
    write_log ("not found\n");
    return 0;
}
#endif

static struct zfile *zfile_fopen_2 (const char *name, const char *mode)
{
    struct zfile *l;
    FILE *f;
    char zipname[1000];

    if( *name == '\0' )
        return NULL;
    l = zfile_create ();
    l->name = strdup (name);
#ifdef SINGLEFILE
    if (zfile_opensinglefile (l))
	return l;
#endif
    f = openzip (l->name, zipname);
    if (f) {
	if (strcmpi (mode, "rb")) {
	    zfile_fclose (l);
	    fclose (f);
	    return 0;
	}
	l->zipname = strdup (zipname);
    }
    if (!f) {
	f = fopen (name, mode);
	if (!f) {
	    zfile_fclose (l);
	    return 0;
	}
    }
    l->f = f;
    return l;
}

/* archiveaccess 7z-plugin compressed file scanner */
static int arcacc_zunzip (struct zfile *z, zfile_callback zc, void *user, int type)
{
    char tmp[MAX_DPATH], tmp2[2];
    struct zfile *zf;
    int err, fc, size, ah, status, i;
    int id_r, id_w;
    struct aa_FileInArchiveInfo fi;
    int skipsize = 0;

    memset (&fi, 0, sizeof (fi));
    if (!arcacc_init ())
	return 0;
    zf = 0;
    id_r = arcacc_push (z);
    if (id_r < 0)
	return 0;
    zfile_fseek (z, 0, SEEK_END);
    size = zfile_ftell (z);
    zfile_fseek (z, 0, SEEK_SET);
    ah = openArchive (readCallback, id_r, size, type, &status);
    if (status) {
	arcacc_pop ();
	return 0;
    }
    fc = getFileCount (ah);
    for (i = 0; i < fc; i++) {
	getFileInfo (ah, i, &fi);
	if (fi.IsDir)
	    continue;
	tmp2[0] = FSDB_DIR_SEPARATOR;
	tmp2[1] = 0;
	strcpy (tmp, z->name);
	strcat (tmp, tmp2);
	strcat (tmp, fi.path);
	zf = zfile_fopen_empty (tmp, (int)fi.UncompressedFileSize);
	if (zf) {
	    id_w = arcacc_push (zf);
	    if (id_w >= 0) {
		zf->writeskipbytes = skipsize;
		err = extract (ah, i, id_w, writeCallback);
		if (zf->seek == fi.UncompressedFileSize) {
		    zfile_fseek (zf, 0, SEEK_SET);
		    if (!zc (zf, user)) {
			closeArchive (ah);
			arcacc_pop ();
			zfile_fclose (zf);
			arcacc_pop ();
			return 0;
		    }
		}
		arcacc_pop ();
	    }
	    zfile_fclose (zf);
	}
        if (fi.CompressedFileSize)
	    skipsize = 0;
        skipsize += (int)fi.UncompressedFileSize;
    }
    closeArchive (ah);
    arcacc_pop ();
    return 1;
}

/* zip (zlib) scanning */
static int zunzip (struct zfile *z, zfile_callback zc, void *user)
{
    unzFile uz;
    unz_file_info file_info;
    char filename_inzip[MAX_DPATH];
    char tmp[MAX_DPATH], tmp2[2];
    struct zfile *zf;
    int err;

    if (!zlib_test (1, 0))
        return 0;
    zf = 0;
    uz = unzOpen (z);
    if (!uz)
        return 0;
    if (unzGoToFirstFile (uz) != UNZ_OK)
        return 0;
    for (;;) {
        err = unzGetCurrentFileInfo(uz, &file_info, filename_inzip, sizeof(filename_inzip), NULL, 0, NULL, 0);
        if (err != UNZ_OK)
	    return 0;
	if (file_info.uncompressed_size > 0) {
	    int err = unzOpenCurrentFile (uz);
    	    if (err == UNZ_OK) {
		tmp2[0] = FSDB_DIR_SEPARATOR;
		tmp2[1] = 0;
		strcpy (tmp, z->name);
		strcat (tmp, tmp2);
		strcat (tmp, filename_inzip);
		zf = zfile_fopen_empty (tmp, file_info.uncompressed_size);
		if (zf) {
		    err = unzReadCurrentFile  (uz, zf->data, file_info.uncompressed_size);
		    unzCloseCurrentFile (uz);
		    if (err == 0 || err == file_info.uncompressed_size) {
			zf = zuncompress (zf);
			if (!zc (zf, user)) {
			    zfile_fclose (zf);
			    return 0;
			}
		    }
		    zfile_fclose (zf);
		}
	    }
	}
	err = unzGoToNextFile (uz);
	if (err != UNZ_OK)
	    break;
    }
    return 1;
}


int zfile_zopen (const char *name, zfile_callback zc, void *user)
{
    struct zfile *l;
    int ztype;
    
    l = zfile_fopen_2 (name, "rb");
    if (!l)
	return 0;
    ztype = iszip (l);
    if (!ztype)
	zc (l, user);
    else if (ztype < 0)
	zunzip (l, zc, user);
    else
	arcacc_zunzip (l, zc, user, ztype);
    zfile_fclose (l);
    return 1;
}    

/*
 * fopen() for a compressed file
 */
struct zfile *zfile_fopen (const char *name, const char *mode)
{
    struct zfile *l;
    l = zfile_fopen_2 (name, mode);
    if (!l)
	return 0;
    l = zuncompress (l);
    return l;
}

int zfile_exists (const char *name)
{
    char fname[2000];
    FILE *f;

    if (strlen (name) == 0)
	return 0;
    strcpy (fname, name);
    f = openzip (fname, 0);
    if (!f)
	f = fopen(name,"rb");
    if (!f)
	return 0;
    fclose (f);
    return 1;
}

int zfile_iscompressed (struct zfile *z)
{
    return z->data ? 1 : 0;
}

struct zfile *zfile_fopen_empty (const char *name, int size)
{
    struct zfile *l;
    l = zfile_create ();
    l->name = strdup (name);
    l->data = malloc (size);
    l->size = size;
    memset (l->data, 0, size);
    return l;
}

long zfile_ftell (struct zfile *z)
{
    if (z->data)
	return z->seek;
    return ftell (z->f);
}

int zfile_fseek (struct zfile *z, long offset, int mode)
{
    if (z->data) {
	int old = z->seek;
	switch (mode)
	{
	    case SEEK_SET:
	    z->seek = offset;
	    break;
	    case SEEK_CUR:
	    z->seek += offset;
	    break;
	    case SEEK_END:
	    z->seek = z->size - offset;
	    break;
	}
	if (z->seek < 0) z->seek = 0;
	if (z->seek > z->size) z->seek = z->size;
	return old;
    }
    return fseek (z->f, offset, mode);
}

size_t zfile_fread  (void *b, size_t l1, size_t l2,struct zfile *z)
{
    long len = l1 * l2;
    if (z->data) {
	if (z->seek + len > z->size) {
	    len = z->size - z->seek;
	    if (len < 0)
		len = 0;
	}
	memcpy (b, z->data + z->seek, len);
	z->seek += len;
	return len;
    }
    return fread (b, l1, l2, z->f);
}

size_t zfile_fputs (struct zfile *z, char *s)
{
    return zfile_fwrite (s, strlen (s), 1, z);
}

size_t zfile_fwrite  (void *b, size_t l1, size_t l2, struct zfile *z)
{
    long len = l1 * l2;
    
    if (z->writeskipbytes) {
	z->writeskipbytes -= len;
	if (z->writeskipbytes >= 0)
	    return len;
	len = -z->writeskipbytes;
	z->writeskipbytes = 0;
    }
    if (z->data) {
	if (z->seek + len > z->size) {
	    len = z->size - z->seek;
	    if (len < 0)
		len = 0;
	}
	memcpy (z->data + z->seek, b, len);
	z->seek += len;
	return len;
    }
    return fwrite (b, l1, l2, z->f);
}

int zfile_zuncompress (void *dst, int dstsize, struct zfile *src, int srcsize)
{
    z_stream zs;
    int v;
    uae_u8 inbuf[4096];
    int incnt;

    if (!zlib_test (1, 0))
	return 0;
    memset (&zs, 0, sizeof(zs));
    if (pinflateInit (&zs, ZLIB_VERSION, sizeof(z_stream)) != Z_OK)
	return 0;
    zs.next_out = dst;
    zs.avail_out = dstsize;
    incnt = 0;
    v = Z_OK;
    while (v == Z_OK && zs.avail_out > 0) {
	if (zs.avail_in == 0) {
	    int left = srcsize - incnt;
	    if (left == 0)
		break;
	    if (left > sizeof (inbuf)) left = sizeof (inbuf);
	    zs.next_in = inbuf;
	    zs.avail_in = zfile_fread (inbuf, 1, left, src);
	    incnt += left;
	}
	v = pinflate (&zs, 0);
    }
    pinflateEnd (&zs);
    return 0;
}

int zfile_zcompress (struct zfile *f, void *src, int size)
{
    int v;
    z_stream zs;
    uae_u8 outbuf[4096];

    if (!is_zlib)
	return 0;
    memset (&zs, 0, sizeof (zs));
    if (pdeflateInit (&zs, Z_DEFAULT_COMPRESSION, ZLIB_VERSION, sizeof(z_stream)) != Z_OK)
	return 0;
    zs.next_in = src;
    zs.avail_in = size;
    v = Z_OK;
    while (v == Z_OK) {
	zs.next_out = outbuf;
        zs.avail_out = sizeof (outbuf);
	v = pdeflate(&zs, Z_NO_FLUSH | Z_FINISH);
	if (sizeof(outbuf) - zs.avail_out > 0)
	    zfile_fwrite (outbuf, 1, sizeof (outbuf) - zs.avail_out, f);
    }
    pdeflateEnd(&zs);
    return zs.total_out;
}

char *zfile_getname (struct zfile *f)
{
    return f->name;
}

uae_u32 zfile_crc32 (struct zfile *f)
{
    uae_u8 *p;
    int pos, size;
    uae_u32 crc;

    if (!f)
	return 0;
    if (f->data)
	return get_crc32 (f->data, f->size);
    pos = zfile_ftell (f);
    zfile_fseek (f, 0, SEEK_END);
    size = zfile_ftell (f);
    p = xmalloc (size);
    if (!p)
	return 0;
    memset (p, 0, size);
    zfile_fseek (f, 0, SEEK_SET);
    zfile_fread (p, 1, size, f);
    zfile_fseek (f, pos, SEEK_SET);        
    crc = get_crc32 (p, size);
    xfree (p);
    return crc;
}

