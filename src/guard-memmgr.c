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

size_t MemMgrPageSize				= (4 * 1024);

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
typedef struct {
	MemMgrPool					*pool;
	size_t						size;
	size_t						realsize;

	char						*file;
	unsigned long				line;

	void						*data;
} MemMgrEntry;

#ifdef HAVE_VALGRIND_H
# define _VGAccess(addr, size, rzSize, set)	VALGRIND_MALLOCLIKE_BLOCK((addr), (size), (rzSize), (set))
# define _VGNoAccess(addr, size, rzSize)	VALGRIND_FREELIKE_BLOCK((addr), (rzSize))
#else
# define _VGAccess(addr, size, rzSize, set)
# define _VGNoAccess(addr, size, rzSize)
#endif

EXPORT XplBool MemoryManagerOpen(const unsigned char *agentName)
{
#ifdef HAVE_GETPAGESIZE
	MemMgrPageSize = getpagesize();
#endif
	return(TRUE);
}

/*
	data points to a chunk of memory that is aligned with the end of a page.  If
	we subtract the size of the entry header and then find the next address that
	is the start of a page then we will have the entry header.
*/
#define getEntry(data)									\
	(MemMgrEntry *) (									\
		(((char *) data) - sizeof(MemMgrEntry)) -		\
														\
		(char *)((unsigned int)							\
			(((char *) data) - sizeof(MemMgrEntry))		\
		% MemMgrPageSize)								\
	)

#define UnlockEntry(d, entry)							\
{														\
	(entry) = getEntry((d));							\
	_VGAccess((entry), sizeof(MemMgrEntry), 0, TRUE);	\
}

#define LockEntry(entry)								\
{														\
	_VGNoAccess((entry), sizeof(MemMgrEntry), 0);		\
	(entry) = NULL;										\
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
	char			*r;
	char			c;

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

	entry = getEntry(data);

	DebugAssert(entry->data == data);
	for (c = 0, r = (char *)entry + sizeof(MemMgrEntry); r < (char *) data; r++, c++) {
		DebugAssert(*r == c);
	}
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
	_VGAccess((entry = getEntry(data)), sizeof(MemMgrEntry), 0, TRUE);

	size = entry->realsize;
	if (entry->pool && entry->pool->free) {
		entry->pool->free(data, entry->pool->data);
	}

	memset(data, 'F', entry->size);
	_VGNoAccess(data, entry->size, 0);

	memset(entry,	'F', sizeof(MemMgrEntry));
	_VGNoAccess(entry, sizeof(MemMgrEntry), 0);

	munmap(entry, size);
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

EXPORT void * _MemMalloc(size_t size, const char *file, const unsigned long line)
{
	MemMgrEntry		*entry;
	void			*data;
	char			*r;
	char			c;
	size_t			s;

	/*
		Determine the proper size to map so that the entry header and all of the
		data can fit into the mapped memory.

		An extra byte will be added to the size which will ensure that an extra
		page of virtual memory will be mapped to act as a guard page.

		The actual data will be aligned so that an overwrite will go into the
		guard page.
	*/
	s = (((sizeof(MemMgrEntry) + size) / MemMgrPageSize) + 1) * MemMgrPageSize;
printf("_MemMalloc() was called with a size of 0x%08zx (0x%08zx with the header) and we mapped 0x%08zx bytes\n",
		size, size + sizeof(MemMgrEntry), s);

	if (MAP_FAILED == (entry = mmap(NULL, s + 1, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0))) {
		return(NULL);
	}
	_VGAccess(entry, s, 0, FALSE);
	mprotect((char *)entry + s, 1, 0);

	data = entry->data	= (char *)entry + s - size;
	entry->realsize		= s + 1;

#if 0
printf("_MemMalloc() got entry:      %p\n", entry);
printf("_MemMalloc() returning data: %p\n", data);
printf("_MemMalloc() expect entry:   %p\n", (entry = getEntry(data)));

printf("_MemMalloc() entry size:     %zx\n", sizeof(MemMgrEntry));
printf("_MemMalloc() offset:         %zx\n", ((unsigned int) (((char *) data) - sizeof(MemMgrEntry))) % MemMgrPageSize);
#endif

	/*
		Wite a predictable pattern to the extra data between the entry header
		the actual data.  This data will be verified with MemAssert();
	*/
	for (c = 0, r = (char *)entry + sizeof(MemMgrEntry); r < (char *) entry->data; r++, c++) {
		*r = c;
	}

	/* Let valgrind know about the data area */
	_VGNoAccess(entry, s, 0);
	_VGAccess(data, size, 0, FALSE);

	MemUpdateOwner(data, file, line);
	memset(data, 'U', size);

	return(data);
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


