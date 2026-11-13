#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <sys/types.h>
#include <errno.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <ctype.h>

#include "../linux/glob.h"

#include "../qcommon/qcommon.h"

//===============================================================================

byte *membase;
int maxhunksize;
int curhunksize;

#ifdef __FreeBSD__
#define MMAP_ANON MAP_ANON
#else
#define MMAP_ANON MAP_ANONYMOUS
#endif

// Keep the public hunk pointer aligned for LP64 structures.
#define HUNK_HEADER_SIZE ((int)sizeof(void *))

void *Hunk_Begin (int maxsize, int precommit)
{
	// reserve a huge chunk of memory, but don't commit any yet
	maxhunksize = maxsize + HUNK_HEADER_SIZE;
	curhunksize = 0;
	membase = mmap(0, maxhunksize, PROT_READ|PROT_WRITE, 
		MAP_PRIVATE|MMAP_ANON, -1, 0);
	if (membase == NULL || membase == (byte *)-1)
		Sys_Error("unable to virtual allocate %d bytes", maxsize);

	*((int *)membase) = curhunksize;

	return membase + HUNK_HEADER_SIZE;
}

void *Hunk_Alloc (int size)
{
	byte *buf;

	// round to cacheline
	size = (size+31)&~31;
	if (curhunksize + size > maxhunksize)
		Sys_Error("Hunk_Alloc overflow");
	buf = membase + HUNK_HEADER_SIZE + curhunksize;
	curhunksize += size;
	return buf;
}

int Hunk_End (void)
{
#ifndef __FreeBSD__
	byte *n;

	n = mremap(membase, maxhunksize, curhunksize + HUNK_HEADER_SIZE, 0);
	if (n != membase)
		Sys_Error("Hunk_End:  Could not remap virtual block (%d)", errno);
	*((int *)membase) = curhunksize + HUNK_HEADER_SIZE;
#endif
	
	return curhunksize;
}

void Hunk_Free (void *base)
{
	byte *m;

	if (base) {
		m = ((byte *)base) - HUNK_HEADER_SIZE;
		if (munmap(m, *((int *)m)))
			Sys_Error("Hunk_Free: munmap failed (%d)", errno);
	}
}

//===============================================================================


/*
================
Sys_Milliseconds
================
*/
unsigned int curtime;
unsigned int Sys_Milliseconds (void)
{
	struct timeval tp;
	struct timezone tzp;
	static unsigned int		secbase;

	gettimeofday(&tp, &tzp);
	
	if (!secbase)
	{
		secbase = tp.tv_sec;
		return tp.tv_usec/1000;
	}

	curtime = (tp.tv_sec - secbase)*1000 + tp.tv_usec/1000;
	
	return curtime;
}

void Sys_DebugBreak (void)
{
        __asm ("int $3");
}

void Sys_Mkdir (char *path)
{
    mkdir (path, 0755);
}

void strlwr (char *s)
{
	while (*s) {
		*s = tolower(*s);
		s++;
	}
}

//============================================

static	char	findbase[MAX_OSPATH];
static	char	findpath[MAX_OSPATH];
static	char	findpattern[MAX_OSPATH];
static	DIR		*fdir;

static qboolean Sys_MakePath (char *dest, size_t destsize, const char *path, const char *name)
{
	int len;

	len = snprintf (dest, destsize, "%s/%s", path, name);
	return len >= 0 && (size_t)len < destsize;
}

static qboolean CompareAttributes(const char *path, const char *name,
	unsigned musthave, unsigned canthave )
{
	struct stat st;

// . and .. never match
	if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
		return false;

	if (!Sys_MakePath (findpath, sizeof(findpath), path, name))
		return false;

	if (stat(findpath, &st) == -1)
		return false; // shouldn't happen

	if ( ( st.st_mode & S_IFDIR ) && ( canthave & SFF_SUBDIR ) )
		return false;

	if ( ( musthave & SFF_SUBDIR ) && !( st.st_mode & S_IFDIR ) )
		return false;

	return true;
}

char *Sys_FindFirst (char *path, unsigned musthave, unsigned canhave)
{
	struct dirent *d;
	char *p;
	size_t len;

	if (fdir)
		Sys_Error ("Sys_BeginFind without close");

	len = strlen (path);
	if (len >= sizeof(findbase))
		return NULL;

//	COM_FilePath (path, findbase);
	memcpy (findbase, path, len + 1);

	if ((p = strrchr(findbase, '/')) != NULL) {
		*p = 0;
		Q_strncpy (findpattern, p + 1, sizeof(findpattern)-1);
	} else
		strcpy(findpattern, "*");

	if (strcmp(findpattern, "*.*") == 0)
		strcpy(findpattern, "*");
	
	if ((fdir = opendir(findbase)) == NULL)
		return NULL;
	while ((d = readdir(fdir)) != NULL) {
		if (!*findpattern || glob_match(findpattern, d->d_name)) {
//			if (*findpattern)
//				printf("%s matched %s\n", findpattern, d->d_name);
			if (CompareAttributes(findbase, d->d_name, musthave, canhave)) {
				return findpath;
			}
		}
	}
	return NULL;
}

char *Sys_FindNext (unsigned musthave, unsigned canhave)
{
	struct dirent *d;

	if (fdir == NULL)
		return NULL;
	while ((d = readdir(fdir)) != NULL) {
		if (!*findpattern || glob_match(findpattern, d->d_name)) {
//			if (*findpattern)
//				printf("%s matched %s\n", findpattern, d->d_name);
			if (CompareAttributes(findbase, d->d_name, musthave, canhave)) {
				return findpath;
			}
		}
	}
	return NULL;
}

void Sys_FindClose (void)
{
	if (fdir != NULL)
		closedir(fdir);
	fdir = NULL;
}


//============================================

