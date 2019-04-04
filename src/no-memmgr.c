#include <memmgr-config.h>
#define XPL_NO_WRAP	1
#include <xpl.h>
#include <ctype.h>
#include <time.h>

#ifdef HAVE_VALGRIND_H
#include <valgrind/valgrind.h>
#endif

#ifdef HAVE_MMAN_H
/* use mmap */
#include <sys/mman.h>
#endif

/* Red Zone Signature */
#define MEMMGR_SIG_REDZ			0x7a446552 // 'zDeR'
#define MEMMGR_SIG_ONE			0x2e456e4f // '.EnO'

int MemMgrPageSize				= (4 * 1024);

/*
	In order to emulate the memory manager pool interfaces we need to keep info
	about the 'pool' (It isn't really a pool here) for the callbacks, and each
	allocation needs to have a small pointer back to the pool.

	This will ensure that the callbacks are used as expected with the MemMgr
	pools, but will not actually pool anything.  Each entry will be freed when
	returned.
*/
typedef struct {
	size_t						size;

	PoolEntryCB					alloc;
	PoolEntryCB					free;

	void						*data;
} MemMgrPool;

/*
	In debug builds the entry->sig and the entry's tail will contain the text
	"ReDzOnE." which can be clearly seen in the debugger.

	This signature is verified on any MemFree() or MemRealloc().  The signature
	check can also be forced by a consumer by calling MemAssert().
*/
#ifdef DEBUG
typedef struct {
	union {
		struct {
			unsigned long		redz;
			unsigned long		one;
		} l;

		char					c[8];
	};
} MemMgrSig;
#define SIG_SIZE	sizeof(MemMgrSig)
#else
#define SIG_SIZE	0
#endif



typedef struct {
	MemMgrPool					*pool;
	size_t						size;

	char						*file;
	unsigned long				line;

#ifdef DEBUG
	MemMgrSig					sig;
#endif
	char						data[];
} MemMgrEntry;

/*
	============================================================================
	Platform specific functions
	============================================================================
*/
#if 1
#ifdef HAVE_MMAN_H
# undef HAVE_MMAN_H
#endif
#endif

#ifdef HAVE_MMAN_H
# define SysAlloc(size)						mmap(NULL, (size), PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0)
# define SysFree(addr, size)				munmap((addr), (size))
# define SysBadAddr							MAP_FAILED
#else
# define SysAlloc(size)						malloc((size))
# define SysFree(addr, size)				free((addr))
# define SysBadAddr							NULL
#endif

#ifdef HAVE_VALGRIND_H
# define _VGAccess(addr, size, rzSize, set)	VALGRIND_MALLOCLIKE_BLOCK((addr), (size), (rzSize), (set))
# define _VGNoAccess(addr, size, rzSize)	VALGRIND_FREELIKE_BLOCK((addr), (rzSize))
#else
# define _VGAccess(addr, size, rzSize, set)
# define _VGNoAccess(addr, size, rzSize)
#endif

#ifdef HAVE_MMAN_H
# define _MMAccess(addr, size, rzSize)		mprotect((addr), (size) + (rzSize), PROT_READ|PROT_WRITE)
# define _MMNoAccess(addr, size, rzSize)	mprotect((addr), (size) + (rzSize), 0)
#else
# define _MMAccess(addr, size, rzSize)
# define _MMNoAccess(addr, size, rzSize)
#endif

#define SysAccess(addr, size, rzSize, set)	{ _VGAccess((addr), (size), (rzSize), (set)); _MMAccess((addr), (size), (rzSize)); }
#define SysNoAccess(addr, size, rzSize)		{ _VGNoAccess((addr), (size), (rzSize)); _MMNoAccess((addr), (size), (rzSize)); }


/*
	============================================================================
	End platform specific functions
	============================================================================
*/
#define UnlockEntry(d, entry)										\
{																	\
	(entry) = XplParentOf((d), MemMgrEntry, data);					\
	SysAccess((entry), sizeof(MemMgrEntry) - SIG_SIZE, 0, TRUE);	\
}

#define LockEntry(entry)											\
{																	\
	_VGNoAccess((entry), sizeof(MemMgrEntry) - SIG_SIZE, 0);		\
	(entry) = NULL;													\
}


EXPORT XplBool MemoryManagerOpen(const unsigned char *agentName)
{
#ifdef HAVE_SYSCONF
	MemMgrPageSize = sysconf(_SC_PAGE_SIZE);
#endif
	return(TRUE);
}

EXPORT XplBool MemoryManagerClose(const unsigned char *agentName)
{
	return(TRUE);
}

EXPORT void MemPoolEnumerate(unsigned char *name, PoolEnumerateCB cb, void *data)
{
	return;
}

EXPORT void MemPrivatePoolEnumerate(void *pool, PoolEntryCB cb, void *data)
{
	return;
}

EXPORT void MemPrivatePoolStatistics(void *PoolHandle, MemStatistics *PoolStats)
{
	memset(PoolStats, 0, sizeof(MemStatistics));
}

EXPORT MemStatistics * _MemAllocStatistics(const char *file, const unsigned long line)
{
	MemStatistics	*stats;

	if ((stats = _MemMalloc(sizeof(MemStatistics), file, line))) {
		memset(stats, 0, sizeof(MemStatistics));
	}

	return(stats);
}

#ifdef DEBUG
EXPORT void MemAssert(void *data)
{
	MemMgrEntry		*entry;
	MemMgrSig		*tail;

#ifdef HAVE_VALGRIND_H
	if (RUNNING_ON_VALGRIND) {
		/*
			MemAssert is pointless when running with valgrind because it will
			detect any overwrites when they occur, and it would detect the check
			as an invalid read.
		*/
		return;
	}
#endif

	if (!data) {
		return;
	}

	UnlockEntry(data, entry);
	DebugAssert(entry->sig.l.redz	== MEMMGR_SIG_REDZ	&&
				entry->sig.l.one	== MEMMGR_SIG_ONE);

	tail = (MemMgrSig *) (((char *)data) + entry->size);

	DebugAssert(tail->l.redz		== MEMMGR_SIG_REDZ	&&
				tail->l.one			== MEMMGR_SIG_ONE);
	LockEntry(entry);
}
#endif

EXPORT void MemFree(void *data)
{
	MemMgrEntry		*entry;
	size_t			size;

	if (!data) {
		return;
	}
	MemAssert(data);

	UnlockEntry(data, entry);
	size = entry->size;

	if (entry->pool && entry->pool->free) {
		entry->pool->free(data, entry->pool->data);
	}

#ifdef DEBUG
# ifdef HAVE_VALGRIND_H
	if (!RUNNING_ON_VALGRIND) {
# endif
		memset(entry, 'F', sizeof(MemMgrEntry) + size + SIG_SIZE);
# ifdef HAVE_VALGRIND_H
	}
# endif
#endif

	_VGNoAccess(data, size, SIG_SIZE);
	SysNoAccess(entry, sizeof(MemMgrEntry) - SIG_SIZE, 0);

	SysFree(entry, size);
}

EXPORT void MemRelease(void **Source)
{
	MemFree(*Source);
	*Source = NULL;
}

EXPORT void MemCopyOwner(void *dest, void *source)
{
	MemMgrEntry		*entry;

	if (!source || !dest) {
		return;
	}
	MemAssert(dest);
	MemAssert(source);

	UnlockEntry(source, entry);
	MemUpdateOwner(dest, entry->file, entry->line);
	LockEntry(entry);
}

EXPORT void MemUpdateOwner(void *data, const char *file, const unsigned int line)
{
	MemMgrEntry		*entry;

	if (!data || !file) {
		return;
	}
	MemAssert(data);

	UnlockEntry(data, entry);

	entry->file = (char *) file;
	entry->line = (unsigned long) line;

	LockEntry(entry);
}

static void *_MMAlloc(size_t size, size_t *actual, XplBool wait, XplBool zero, const char *file, const unsigned long line)
{
	MemMgrEntry		*entry;
#ifdef DEBUG
	MemMgrSig		*tail;
#endif
	void			*data;

	do
	{
		if (SysBadAddr == (entry = SysAlloc(sizeof(MemMgrEntry) + size + sizeof(MemMgrSig)))) {
			if( !wait )
			{
				return(NULL);
			}
		}
	}while( wait );

	/* Allow access to the entire allocation to setup the red zones */
	_VGAccess(entry, sizeof(MemMgrEntry) + size + sizeof(MemMgrSig), 0, FALSE);

	entry->size			= size;
	if( actual )
	{
		*actual = size;
	}
	entry->pool			= NULL;
	data				= &entry->data;
#ifdef DEBUG
	tail				= (MemMgrSig *)(((char *)&entry->data) + size);
	tail->l.redz		= MEMMGR_SIG_REDZ;
	tail->l.one			= MEMMGR_SIG_ONE;
	entry->sig.l.redz	= MEMMGR_SIG_REDZ;
	entry->sig.l.one	= MEMMGR_SIG_ONE;
#endif

	/* We no longer need access to the entire allocation.  */
	_VGNoAccess(entry, sizeof(MemMgrEntry) + size + SIG_SIZE, 0);
	_VGAccess(data, size, SIG_SIZE, FALSE);

	MemUpdateOwner(data, file, line);
	memset(data, (zero) ? 0 : 'U', size);

	return(data);
}

EXPORT void *MMAllocEx(void *data, size_t size, size_t *actual, XplBool wait, XplBool zero, const char *file, const unsigned long line)
{
	MemMgrEntry		*entry;
	void			*result;

	if (!(result = _MemMalloc(size, actual, wait, zero, file, line))) {
		return(NULL);
	}

	if (data) {
		UnlockEntry(data, entry);
		memcpy(result, data, min(entry->size, size));
		LockEntry(entry);

		MemFree(data);
	}

	MemAssert(result);
	return(result);
}

EXPORT void * _MemCalloc(size_t count, size_t size, const char *file, const unsigned long line)
{
	void *data;

	if ((data = _MemMalloc(count * size, file, line))) {
		memset(data, 0, count * size);
	}

	return(data);
}

EXPORT void * _MemPrivatePoolAlloc(unsigned char *identity, size_t size, unsigned int minentries, unsigned int maxentries, XplBool dynamic, XplBool temp, PoolEntryCB alloc, PoolEntryCB free, void *data, const char *file, const unsigned long line)
{
	MemMgrPool	*pool;

	if ((pool = _MemMalloc(sizeof(MemMgrPool), file, line))) {
		pool->size	= size;
		pool->alloc	= alloc;
		pool->free	= free;
		pool->data	= data;
	}

	return(pool);
}

EXPORT void * _MemPrivatePoolGetEntry(void *handle, const char *file, const unsigned long line)
{
	MemMgrPool		*pool;
	void			*data;
	MemMgrEntry		*entry;

	if (!(pool = (MemMgrPool *) handle)) {
		return(NULL);
	}

	if (!(data = _MemMalloc(pool->size, file, line))) {
		return(NULL);
	}

	UnlockEntry(data, entry);
	entry->pool = pool;
	LockEntry(entry);

	if (pool->alloc && !pool->alloc(data, pool->data)) {
		MemFree(data);
		return(NULL);
	}

	return(data);
}

EXPORT void * _MemMallocWait(size_t size, const char *file, const unsigned long line)
{
	void	*result;

	if (!size) {
		/* Always allocate at least 1 byte */
		size = 1;
	}

	while (!(result = MemMalloc(size))) {
		fputs("Can't allocate memory!  Retrying in 5 seconds\n", stderr);
		XplDelay(5000);
	}

	return(result);
}

/*
	Force MemRealloc() to allocate a new value every time so that the pointer
	will always move in order to try and catch any code using it wrong.
*/
EXPORT void * _MemRealloc(void *data, size_t size, const char *file, const unsigned long line)
{
	MemMgrEntry		*entry;
	void			*result;

	if (!(result = _MemMalloc(size, file, line))) {
		return(NULL);
	}

	if (data) {
		UnlockEntry(data, entry);
		memcpy(result, data, min(entry->size, size));
		LockEntry(entry);

		MemFree(data);
	}

	MemAssert(result);
	return(result);
}

EXPORT void _MemReallocWait(void **data, size_t size, const char *file, const unsigned long line)
{
	void	*result;

	if (!size) {
		/* Always allocate at least 1 byte */
		size = 1;
	}

	while (!(result = _MemRealloc(*data, size, file, line))) {
		fputs("Can't reallocate memory!  Retrying in 5 seconds\n", stderr);
		XplDelay(5000);
	}

	*data = result;
}

EXPORT char * _MemStrndup(const char *string, size_t length, const char *file, const unsigned long line)
{
	char	*result;

	if (!string || !(result = _MemMalloc(length + 1, file, line))) {
		return(NULL);
	}

	strncpy(result, string, length);
	result[length] = '\0';

	return(result);
}

EXPORT char * _MemStrndupWait(const char *string, size_t length, const char *file, const unsigned long line)
{
	char	*result = NULL;

	while (!(result = _MemStrndup(string, length, file, line))) {
		fputs("Can't duplicate string in memory!  Retrying in 5 seconds\n", stderr);
		XplDelay(5000);
	}

	return(result);
}

EXPORT char * _MemStrdup(const char *string, const char *file, const unsigned long line)
{
	if (!string) {
		return(NULL);
	}

	return(_MemStrndup(string, strlen(string), file, line));
}

EXPORT char * _MemStrdupWait(const char *string, const char *file, const unsigned long line)
{
	if (!string) {
		return(NULL);
	}

	return(_MemStrndupWait(string, strlen(string), file, line));
}

EXPORT int _MemAsprintf(char **ret, const char *file, const unsigned long line, const char *format, ...)
{
	va_list		args;
	int			result;
	size_t		needed;
	char		*tmp;

	va_start(args, format);
	result = vstrprintf(NULL, 0, &needed, format, args);
	va_end(args);

	if (!(*ret = _MemMalloc(needed+1, file, line))) {
		return(-1);
	}

	va_start(args, format);
	result = vstrprintf(*ret, needed+1, NULL, format, args);
	va_end(args);

	return(result);
}


