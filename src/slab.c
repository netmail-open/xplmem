#define XPL_NO_WRAP	1

#include<xpllock.h>
#include<xplmem.h>
#include<xplthread.h>
#include<string.h>


#include"slab.h"

#define DebugAssert( arg )
#define CriticalAssert( arg )
#define XplParentOf( ptr, type, member )              (type *)((char *)ptr - offsetof( type, member ))

int TrackMemory = 0;

#define DUMP_INTERVAL	60 * 60 * 2
//#define DUMP_INTERVAL	30

#if 0
MemPoolConfig MemoryDefaultConfig = {
	{ "32",		2,		32,		0,		8,	MEMMGR_ADVICE_SMALL		},
	{ "64",		4,		54,		0,		8,  MEMMGR_ADVICE_SMALL		},
	{ "128",	8,		128,	0,		8,  MEMMGR_ADVICE_SMALL		},
	{ "256",	16,		256,	0,		8,  MEMMGR_ADVICE_SMALL		},
	{ "512",	32,		512,	0,		8,  MEMMGR_ADVICE_SMALL		},
	{ "1k",		32,		0,		128,	4,  MEMMGR_ADVICE_SMALL		},
	{ "2k",		64,		0,		128,	4,  MEMMGR_ADVICE_SMALL		},
	{ "4k",		128,	0,		128,	4,  MEMMGR_ADVICE_SMALL		},
	{ "8k",		128,	0,		64,		4,  MEMMGR_ADVICE_NORMAL	},
	{ "16k",	256,	0,		64,		4,	MEMMGR_ADVICE_NORMAL    },
	{ "32k",	256,	0,		32,		4,	MEMMGR_ADVICE_NORMAL    },
	{ "64k",	512,	0,		32,		4,	MEMMGR_ADVICE_NORMAL    },
	{ "128k",	512,	0,		16,		4,	MEMMGR_ADVICE_NORMAL    },
	{ "256k",	512,	0,		8,		4,	MEMMGR_ADVICE_NORMAL    },
	{ "512k",	512,	0,		4,		4,	MEMMGR_ADVICE_NORMAL    },
	{ "1m",		1024,	0,		4,		4,	MEMMGR_ADVICE_NORMAL    }
};
#else
// defaults for the memory manager, this can be overridden per application
// by using MemoryManagerOpenEx( "consumername", MemPoolConfig *myConfig )
// the configuration needs exactly 16 pools
MemPoolConfig MemoryDefaultConfig = {
	{ "32",		64,		32,		0,		8,	MEMMGR_ADVICE_SMALL		},
	{ "64",		32,		64,		0,		8,  MEMMGR_ADVICE_SMALL		},
	{ "128",	128,	128,	0,		8,  MEMMGR_ADVICE_SMALL		},
	{ "256",	64,		256,	0,		8,  MEMMGR_ADVICE_SMALL		},
	{ "512",	64,		512,	0,		8,  MEMMGR_ADVICE_SMALL		},
	{ "1k",		69,		0,		256,	4,  MEMMGR_ADVICE_SMALL		},
	{ "2k",		133,	0,		256,	4,  MEMMGR_ADVICE_SMALL		},
	{ "4k",		261,	0,		256,	4,  MEMMGR_ADVICE_SMALL		},
	{ "8k",		259,	0,		128,	4,  MEMMGR_ADVICE_NORMAL	},
	{ "16k",	515,	0,		128,	4,	MEMMGR_ADVICE_NORMAL    },
	{ "32k",	514,	0,		64,		4,	MEMMGR_ADVICE_NORMAL    },
	{ "64k",	1026,	0,		64,		4,	MEMMGR_ADVICE_NORMAL    },
	{ "128k",	4098,	0,		128,	4,	MEMMGR_ADVICE_NORMAL    },
	{ "256k",	1025,	0,		16,		4,	MEMMGR_ADVICE_NORMAL    },
	{ "512k",	1025,	0,		8,		4,	MEMMGR_ADVICE_NORMAL    },
	{ "1m",		2049,	0,		8,		4,	MEMMGR_ADVICE_NORMAL    }
};
#endif

#ifdef DEBUG_ASSERT
#define MEMNODEOVERHEAD (sizeof( _MemNode ) + sizeof( _MemSig ))
#else
#define MEMNODEOVERHEAD sizeof( _MemNode )
#endif

#ifdef DEBUG_ASSERT
static void __FoundDoubleFree(_MemNode *node, const char *file, const unsigned long line, const char *allocFile, const unsigned long allocLine)
{
	/*
		An allocations signature contained 'FrEeZoNe', which indicates that it
		has already been free'ed.  The file and line arguments point to the
		previous call to MemFree().
	*/
	CriticalAssert(0);
}

static void __FoundAlreadyFree(_MemNode *node, const char *file, const unsigned long line, const char *allocFile, const unsigned long allocLine)
{
	/*
		An allocations signature contained 'FrEeZoNe', which indicates that it
		has already been free'ed.  The file and line arguments point to the
		previous call to MemFree().
	*/
	CriticalAssert(0);
}

static void __FoundOverwrite(_MemNode *node, char *redzone, char R, char e, char D, char z, char O, char n, char E, char p)
{
	/*
		The end of the allocation has been overwritten.  The provided redzone
		argument would contain 'ReDzOnE.' if the allocation was intact.

		The remaining arguments should show at a glance which characters of the
		signature where overwritten.
	*/
	CriticalAssert(0);
}

static void __FoundUnderwrite(_MemNode *node, char *redzone, char R, char e, char D, char z, char O, char n, char E, char p)
{
	/*
		The end of the allocation has been underwritten.  The provided redzone
		argument would contain 'ReDzOnE.' if the allocation was intact.

		The remaining arguments should show at a glance which characters of the
		signature where overwritten.
	*/
	CriticalAssert(0);
}

static void __AllocationLocation(_MemNode *node, const char *file, const unsigned long line)
{
	int		i;
	_MemSig	*sig;

	for (i = 0; i < MEM_SIG_SIZE; i++) {
		if (MEMMGR_SIG_REDZ != node->sig.data[i].redz ||
			MEMMGR_SIG_ONE  != node->sig.data[i].one
		) {
			__FoundUnderwrite(node, node->sig.data[i].c,
				node->sig.data[i].c[0], node->sig.data[i].c[1],
				node->sig.data[i].c[2], node->sig.data[i].c[3],
				node->sig.data[i].c[4], node->sig.data[i].c[5],
				node->sig.data[i].c[6], node->sig.data[i].c[7]);

			return;
		}
	}

	sig = (_MemSig *)(node->data + node->size);

	for (i = 0; i < MEM_SIG_SIZE; i++) {
		if (MEMMGR_SIG_REDZ != sig->data[i].redz ||
			MEMMGR_SIG_ONE  != sig->data[i].one
		) {
			__FoundOverwrite(node, sig->data[i].c,
				sig->data[i].c[0], sig->data[i].c[1],
				sig->data[i].c[2], sig->data[i].c[3],
				sig->data[i].c[4], sig->data[i].c[5],
				sig->data[i].c[6], sig->data[i].c[7]);

			return;
		}
	}
}

static void __FreeLocation(_MemNode *node, const char *file, const unsigned long line)
{
	__AllocationLocation(node, node->allocInfo.file, node->allocInfo.line);
}

static void _AssertSig(_MemNode *node, XplBool infree)
{
	int		i;

	for (i = 0; i < MEM_SIG_SIZE; i++) {
		if (MEMMGR_SIG_REDZ != node->sig.data[i].redz ||
			MEMMGR_SIG_ONE  != node->sig.data[i].one
		) {
			if (MEMMGR_SIG_FREE == node->sig.data[i].redz &&
				MEMMGR_SIG_ZONE == node->sig.data[i].one
			) {
				if (infree) {
					__FoundDoubleFree(node, node->freeInfo.info.file, node->freeInfo.info.line, node->freeInfo.lastAlloc.file, node->freeInfo.lastAlloc.line);
				} else {
					__FoundAlreadyFree(node, node->freeInfo.info.file, node->freeInfo.info.line, node->freeInfo.lastAlloc.file, node->freeInfo.lastAlloc.line);
				}
				return;
			} else {
				__FreeLocation(node, node->allocInfo.file, node->allocInfo.line);
				return;
			}
		}
	}
}

static void _AssertTrail(_MemNode *node)
{
	int		i;
	_MemSig	*sig = (_MemSig *)(node->data + node->size);

	for (i = 0; i < MEM_SIG_SIZE; i++) {
		if (MEMMGR_SIG_REDZ != sig->data[i].redz ||
			MEMMGR_SIG_ONE  != sig->data[i].one
		) {
			__FreeLocation(node, node->allocInfo.file, node->allocInfo.line);
			return;
		}
	}
}

static void _AssertSigF( _MemNode *node )
{
	int		i;
	_MemSig	*sig = &node->sig;

	for (i = 0; i < MEM_SIG_SIZE; i++) {
		if (MEMMGR_SIG_FREE != sig->data[i].redz ||
			MEMMGR_SIG_ZONE != sig->data[i].one
		) {
			__FreeLocation(node, node->allocInfo.file, node->allocInfo.line);
			return;
		}
	}
}

static void _AssertTrailF( _MemNode *node )
{
	int		i;
	_MemSig	*sig = (_MemSig *)(node->data + node->size);

	for (i = 0; i < MEM_SIG_SIZE; i++) {
		if (MEMMGR_SIG_FREE != sig->data[i].redz ||
			MEMMGR_SIG_ZONE != sig->data[i].one
		) {
			__FreeLocation(node, node->allocInfo.file, node->allocInfo.line);
			return;
		}
	}
}

static void _AssertNodeInSlab( _MemSlab *slab, _MemNode *node )
{
	// node needs to be grater than or equal to slab->data and less than slab+slab->size
	// otherwise this node is not part of this slab
	if( node )
	{
		CriticalAssert(((char *)node >= slab->data) && ((char *)node < ((char *)slab + slab->size)));
	}
}
#else
#define _AssertNodeInSlab( s, n )
#define _AssertF( n )
#define _AssertTrailF( n)
#define _AssertSigF( n)
#endif

#ifdef HAVE_MMAN_H
/* use mmap */
#include <sys/mman.h>
#ifndef MAP_ANONYMOUS
# define MAP_ANONYMOUS MAP_ANON
#endif

static INLINE size_t _PageFit( size_t size )
{
	if( size & ( _Mem.pageSize - 1 ) )
	{
		size += _Mem.pageSize;
	}
	size &= ~( _Mem.pageSize - 1 );
	return size;
}
static void *SysAlloc( size_t size, size_t *actual, MemMgrAdvice advice )
{
	void *p;

	size = _PageFit( size );
	//TODO: use shmget() for MEMMGR_ADVICE_LARGE?
	if( MAP_FAILED != ( p = mmap( NULL, size, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0 ) ) )
	{
		if( actual )
		{
			*actual = size;
		}
		//TODO: This needs review.
		switch( advice )
		{
			case MEMMGR_ADVICE_LARGE:
				madvise( p, size, MADV_SEQUENTIAL );
				break;
			case MEMMGR_ADVICE_SMALL:
				madvise( p, size, MADV_RANDOM );
				break;
			case MEMMGR_ADVICE_NORMAL:
			default:
				break;			/* no action */
		}
		return p;
	}
	return NULL;
}

static void SysFree( void *addr, size_t size )
{
	size = _PageFit( size );
	munmap( addr, size );
}
#else
static void *SysAlloc( size_t size, size_t *actual, MemMgrAdvice advice )
{
	void *p;

	if( p = malloc( size ) )
	{
		if( actual )
		{
			*actual = size;
		}
#if 0			//TODO: take some action with advice here
		switch( advice )
		{
			case MEMMGR_ADVICE_LARGE:
			case MEMMGR_ADVICE_SMALL:
			case MEMMGR_ADVICE_NORMAL:
			default:
				break;			/* no action */
		}
#endif
		return p;
	}
	return NULL;
}
# define SysFree(addr, size)				free((addr))
#endif

static void _WorkSchedule( unsigned long type, void *dataP, unsigned long dataL );

static void _FreeSlab( _MemPool *pool, _MemSlab *slab )
{
	int	l;
	_MemNode	*node, *next;

	if( pool->freeCB )
	{
		for(l=0;l<slab->nodes;l++)
		{
			node = (_MemNode *)(slab->data + ( l * pool->nodeSize ));
			// if slab matches it is allocated
			if( node->slab == slab )
			{
				pool->freeCB( node->data, pool->clientData );
				node->next = slab->freeList.list;
				slab->freeList.list = node;
			}
		}
	}
	else
	{
		for(l=0;l<slab->nodes;l++)
		{
			node = (_MemNode *)(slab->data + ( l * pool->nodeSize ));
			// if slab matches it is allocated
			if( node->slab == slab )
			{
				node->next = slab->freeList.list;
				slab->freeList.list = node;
			}
		}
	}
	if( pool->destroyCB )
	{
		for(node=slab->freeList.list;node;node=next)
		{
			next = node->next;
			pool->destroyCB( node->data, pool->clientData );
		}
	}
	SysFree( slab, slab->size );
}

static void _ClearPool( _MemPool *pool )
{
	_MemSlab		*slab, *next;
	_MemString		*str, *strNext;
	int				l;

	slab = pool->slabList;
	pool->slabList = NULL;
	pool->slabs = 0;
	for(;slab;slab=next)
	{
		next = slab->next;
		_FreeSlab( pool, slab );
	}
	for(l=0;l<MEM_STRING_TABLE_COUNT;l++)
	{
		for(str=pool->stringHash[l].list;str;str=strNext)
		{
			strNext = str->next;
			MMFree( str, __FILE__, __LINE__ );
		}
		pool->stringHash[l].list = NULL;
	}
}

#ifdef DEBUG_MAX
static XplBool MarkFreeCB( void *ptr, void *client )
{
	_MemNode	*node;

	if( ptr )
	{
		node = XplParentOf( ptr, _MemNode, data );
		memset( ptr, 'F', node->size );
		return TRUE;
	}
	return FALSE;
}
#endif

EXPORT int ttyprintf( FILE *fp, const char *format, ... )
{
	va_list	args;
	int		bytes;

	if( !isatty( fileno( fp ) ) )
	{
		return 0;
	}

	va_start( args, format );
	bytes = vfprintf( fp, format, args );
	va_end( args );
	return bytes;
}

static int _NewSlab( _MemPool *pool )
{
	int			l;
	size_t		size;
	_MemSlab	*slab;
	char		*data;
	_MemNode	*node;

//	if( slab = (_MemSlab *)SysAlloc( ( ( pool->pagesPerSlab * ( 1 << pool->slabs ) ) + 1 ) * _Mem.pageSize, &size, pool->advice ) )
	if( slab = (_MemSlab *)SysAlloc( pool->pagesPerSlab * _Mem.pageSize, &size, pool->advice ) )
	{
#if DEBUG_SLABBER
		if( pool->threshold && ( pool->slabs > pool->threshold ) )
		{
			_WorkSchedule( MEM_REPORT, pool, 0 );
		}
#endif
		memset( slab, 0, size );
		slab->size = size;
		XplLockInit( &slab->freeList.lock );
		XplLockInit( &slab->allocList.lock );
		data = slab->data;
		slab->nodes = ( slab->size - sizeof( _MemSlab ) ) / pool->nodeSize;
		slab->total = slab->nodes;

		if( pool->prepareCB )
		{
			slab->freeList.list = NULL;
			slab->freeList.nodes = 0;
			slab->allocList.list = (_MemNode *)data;
			slab->allocList.nodes = slab->nodes;
		}
		else
		{
			slab->freeList.list = (_MemNode *)data;
			slab->freeList.nodes = slab->nodes;
			slab->allocList.list = NULL;
			slab->allocList.nodes = 0;
		}
		for(l=0;l<slab->nodes;l++)
		{
			node = (_MemNode *)(data + ( l * pool->nodeSize ));
//			memset( node, 0, pool->nodeSize );
			node->next = (_MemNode *)(data + ( ( l +  1 ) * pool->nodeSize));
			node->size = pool->size;
			_SetSigF( node );
			_SetTrailF( node );
		}
		l--;
		node = (_MemNode *)(data + ( l * pool->nodeSize) );
		node->next = NULL;
		slab->next = pool->slabList;
		pool->slabList = slab;
		pool->slabs++;
		if( pool->threshold && ( pool->slabs > pool->threshold ) )
		{
			ttyprintf( stderr, "Pool %s growing to %d slabs, new slab size: %ld\n", pool->identity, pool->slabs, (long) slab->size );
			if( pool->slabs > ( pool->threshold * 3 ) )
			{
				TrackMemory = 1;
				XplThreadSignal( _Mem.workThread, SIGINT, NULL );
			}
		}
		return 0;
	}
	return -1;
}

static _MemNode *_GetNode( _MemPool *pool, _MemSlab *slab, _MemList *nodeList, int nodes )
{
	_MemNode	*node;

	if( node = nodeList->list )
	{
		_AssertNodeInSlab( slab, node );
		if( 1 == nodes )
		{
			_AssertNodeInSlab( slab, node->next );
			_AssertSigF( node );
			_SetSig( node );
			_AssertTrailF( node );
			_SetTrail( node );
			node->pool = pool;
			nodeList->nodes--;
			nodeList->list = node->next;
			// node->next and node->slab are in the same union
			// next is populated when it is in a free list
			// slab is populated when it is out with the consumer
			// node->next = NULL;
			node->slab = slab;
		}
		else
		{
			// chunked mode
			_MemNode	**np;

			for(np=&nodeList->list;*np;np=&(*np)->next)
			{
				_AssertSigF( *np );
				_AssertTrailF( *np );
				_AssertNodeInSlab( slab, (*np)->next );
				(*np)->pool = pool;
				nodeList->nodes--;
				if( !--nodes )
				{
					break;
				}
			}
			nodeList->list = *np;
			// terminate the list of nodes
			*np = NULL;
			// the chunker will fix individual node->slab pointers as it gives nodes out
			// from the chunk
		}
		return node;
	}
	return NULL;
}

static _MemNode *_PoolAlloc( _MemPool *pool, int nodes, _MemSlab **slabP, _NodeType *type )
{
	_MemNode	*node;
	_MemSlab	*slab;

	for(;;)
	{
		for(slab=pool->slabList;slab;slab=slab->next)
		{
			// make sure all the slabs are full
			XplLockAcquire( &slab->freeList.lock );
			if( slab->freeList.nodes )
			{
				if( node = _GetNode( pool, slab, &slab->freeList, nodes ) )
				{
					XplLockRelease( &slab->freeList.lock );
					// let the chunker know the slab this set came from
					if( slabP ) *slabP = slab;
					if( type )	*type = NODE_TYPE_FREE;
					return node;
				}
			}
			XplLockRelease( &slab->freeList.lock );

			if( pool->prepareCB )
			{
				XplLockAcquire( &slab->allocList.lock );
				if( slab->allocList.nodes )
				{
					if( node = _GetNode( pool, slab, &slab->allocList, nodes ) )
					{
						XplLockRelease( &slab->allocList.lock );
						// let the chunker know the slab this set came from
						if( slabP ) *slabP = slab;
						if( type )	*type = NODE_TYPE_FREE;
						// prepare if not chunked
						if( 1 == nodes )
						{
							pool->prepareCB( node->data, pool->clientData );
						}
						return node;
					}
				}
				XplLockRelease( &slab->allocList.lock );
			}
		}
		// couldn't find a node, attempt to cut out a new slab
		XplLockAcquire( &pool->lock );
		if( pool->flags & MEM_POOL_GROWING )
		{
			// another thread is already growing the pool, wait for it to complete
			while( pool->flags & MEM_POOL_GROWING )
			{
				XplLockRelease( &pool->lock );
				XplDelay( 10 );
				XplLockAcquire( &pool->lock );
			}
		}
		else
		{
			// this thread is cutting out a new slab
			pool->flags |= MEM_POOL_GROWING;
			XplLockRelease( &pool->lock );
			_NewSlab( pool );
			XplLockAcquire( &pool->lock );
			pool->flags &= ~MEM_POOL_GROWING;
		}
		XplLockRelease( &pool->lock );
	}
}

static char *_MemStringLookup( _MemPool *pool, const char *data )
{
	unsigned long	hash;
	_MemString		*str;
	_MemNode		*node;

	if( data )
	{
		hash = ((unsigned long)data >> 8) & (MEM_STRING_TABLE_COUNT-1);
		for(str=pool->stringHash[hash].list;str;str=str->next)
		{
			if( str->original == data )
			{
				return str->data;
			}
		}
		node = _PoolAlloc( _Mem.stringPool, 1, NULL, NULL );
		str = (_MemString *)node->data;

		strncpy( str->data, data, MAX_MEM_STRING - 1 );
		str->original = data;
		XplLockAcquire( &pool->stringHash[hash].lock );
		str->next = pool->stringHash[hash].list;
		pool->stringHash[hash].list = str;
		XplLockRelease( &pool->stringHash[hash].lock );
		return str->data;
	}
	return "Unknown";
}

// implement LIFO mode also
static void _PoolFree( _MemSlab *slab, _MemNode *node )
{
	if( node->pool->freeCB )
	{
		node->pool->freeCB( &node->data, node->pool->clientData );
	}
	if( !( node->pool->flags & MEM_POOL_PRIVATE ) )
	{
		node->size = node->pool->size;
	}
	_SetSigF( node );
	_SetTrailF( node );

	XplLockAcquire( &slab->freeList.lock );
	node->pool = NULL;
	node->next = slab->freeList.list;
	slab->freeList.list = node;
	slab->freeList.nodes++;
	XplLockRelease( &slab->freeList.lock );
}

// implement LIFO mode also
static void _PoolDiscard( _MemNode *node )
{
	_MemSlab	*slab;

	// save slab pointer, it is unioned with next pointer
	slab = node->slab;
	XplLockAcquire( &slab->allocList.lock );
	node->pool = NULL;
	node->next = slab->allocList.list;
	slab->allocList.list = node;
	slab->allocList.nodes++;
	XplLockRelease( &slab->allocList.lock );
}

static void _PoolInit( _MemPool *pool, const char *identity, int pages, int size, int nodes, int threshold, MemMgrAdvice advice )
{
	int	l;

	if( pool )
	{
		memset( pool, 0, sizeof( _MemPool ) );
		pool->identity = identity;
		pool->threshold = threshold;
		pool->advice = advice;
#ifdef NEW_LOCKS
		_RWInit( &pool->lock );
#else
		XplLockInit( &pool->lock );
#endif
		for(l=0;l<MEM_STRING_TABLE_COUNT;l++)
		{
			XplLockInit( &pool->stringHash[l].lock );
			pool->stringHash[l].list = NULL;
		}
		pool->pagesPerSlab = pages;
		if( size )
		{
			// calculate nodeSize
			pool->size = size;
			pool->nodeSize = pool->size + MEMNODEOVERHEAD;
		}
		else
		{
			// calculate size
			pool->nodeSize = ( ( pool->pagesPerSlab * _Mem.pageSize ) - sizeof( _MemSlab ) ) / nodes;
			// align nodes to 32 byte
			pool->nodeSize &= 0xffffffe0;
			pool->size = pool->nodeSize - MEMNODEOVERHEAD;
		}
#ifdef DEBUG_MAX
		pool->prepareCB = ZeroMemoryCB;
		pool->freeCB = MarkFreeCB;
		pool->destroyCB = NULL;
#endif
	}
}

static void _NodeFree( _MemNode *node, const char *file, unsigned long line )
{
	_MemNode	**np;

	DebugAssert(0 == node->key);

	// huge alloc
	if( node->slab == _Mem.hugePool->slabList )
	{
		_SetFree( node, _MemStringLookup( _Mem.hugePool, file ), line );
		XplLockAcquire( &_Mem.hugePool->slabList->freeList.lock );
		for(np=&_Mem.hugePool->slabList->freeList.list;*np;np=&(*np)->hugeLink)
		{
			if( *np == node )
			{
				_AssertSig( node, TRUE );
				_AssertTrail( node );
				*np = node->hugeLink;
				_Mem.hugePool->slabs--;
				XplLockRelease( &_Mem.hugePool->slabList->freeList.lock );
				SysFree( node, node->size );
				return;
			}
		}
		XplLockRelease( &_Mem.hugePool->slabList->freeList.lock );
		// if debug asserts are disabled, this will NOP double free's
		CriticalAssert( 0 );	// Double Free
		_AssertSig( node, TRUE );
		_AssertTrail( node );
	}
	else
	{
		if( !node->pool )
		{
			// double free
			// if debug asserts are disabled, this will NOP double free's
			CriticalAssert( 0 );	// Double Free
			return;
		}
		_AssertNodeInSlab( node->slab, node );	// double free or invalid pointer
		_AssertSig( node, TRUE );
		_AssertTrail( node );
		_ClearTrail( node );

		_SetFree( node, _MemStringLookup( node->pool, file ), line );
		_PoolFree( node->slab, node );
	}
}

static int _DumpPool( FILE *fp, _MemPool *pool )
{
#if defined(DEBUG)||defined(DEBUG_ASSERT)
	int			dumped, l, totalNodes, freeNodes;
	_MemSlab	*slab;
	char		*data;
	_MemWork	*allocations, *a;
	_MemNode	*node;
	_MemNode	temp;

	dumped = 0;
// 	fprintf( fp, "----------============================================================----------\n" );
	fprintf( fp, "================================================================================\n" );
	fprintf( fp, "Pool          : %s\n" ,pool->identity );
	fprintf( fp, "Data size     : %ld\n", (long) pool->size );
	fprintf( fp, "Node size     : %ld\n", (long) pool->nodeSize );
	l = ( pool->pagesPerSlab * _Mem.pageSize ) / 1024;
	if( l < 16384 )
	{
		fprintf( fp, "Slab size     : %ld KB\n", (long) l );
	}
	else
	{
		fprintf( fp, "Slab size: %ld MB\n", (long) l / 1024 );
	}
	fprintf( fp, "================================================================================\n" );
	fprintf( fp, "Slab count    : %d\n", pool->slabs );

	if( pool->slabList )
	{
		allocations = NULL;
		totalNodes = freeNodes = 0;

		for(slab=pool->slabList,l=0;slab;slab=slab->next,l++)
		{
			totalNodes += slab->total;
			freeNodes += slab->freeList.nodes;
			freeNodes += slab->allocList.nodes;
		}
		fprintf( fp, "Total nodes   : %d\n", totalNodes );
		fprintf( fp, "Nodes in use  : %d\n", totalNodes - freeNodes );
		fprintf( fp, "Nodes free    : %d\n", freeNodes );
		fprintf( fp, "--------------------------------------------------------------------------------\n" );
//		fprintf( fp, "----------============================================================----------\n" );
		for(slab=pool->slabList;slab;slab=slab->next)
		{
			data = slab->data;
			for(l=0;l<slab->nodes;l++)
			{
				node = (_MemNode *)(data + ( l * pool->nodeSize ));
				memcpy( &temp, node, sizeof( _MemNode ) );
				if( slab == temp.slab )
				{
					for(a=allocations;a;a=a->next)
					{
						if( a->dataP == temp.allocInfo.file && a->dataL == temp.allocInfo.line )
						{
							a->type++;
							break;
						}
					}
					if( !a )
					{
						node = _PoolAlloc( _Mem.workPool, 1, NULL, NULL );
						a = (_MemWork *)node->data;
						memset( a, 0, sizeof( _MemWork ) );
						a->dataP = (void *)temp.allocInfo.file;
						a->dataL = temp.allocInfo.line;
						a->type = 1;
						a->next = allocations;
						allocations = a;
					}
				}
			}
		}
		if( allocations )
		{
			while( allocations )
			{
				a = allocations;
				allocations = a->next;
				fprintf( fp, "%10ld Nodes %s:%ld\n", a->type, (char *)a->dataP, a->dataL );
				node = XplParentOf( a, _MemNode, data );
				_PoolFree( node->slab, node );
				dumped++;
			}
		}
	}
	fprintf( fp, "\n" );
	return dumped;
#else
	return 0;
#endif
}

static int _DumpHugePool( FILE *fp, _MemPool *pool )
{
#if defined(DEBUG)||defined(DEBUG_ASSERT)
	int			dumped;
	_MemWork	*allocations, *a;
	_MemNode	*node, *track;

	allocations = NULL;
	if( pool->slabList && pool->slabs )
	{
		fprintf( fp, "----------============================================================----------\n" );
		fprintf( fp, "Pool: %s\n" ,pool->identity );
		fprintf( fp, "----------============================================================----------\n" );
		XplLockAcquire( &pool->slabList->freeList.lock );
		for(node=pool->slabList->freeList.list;node;node=node->hugeLink)
		{
			track = _PoolAlloc( _Mem.workPool, 1, NULL, NULL );
			a = (_MemWork *)track->data;
			memset( a, 0, sizeof( _MemWork ) );
			a->dataP = (void *)node->allocInfo.file;
			a->dataL = node->allocInfo.line;
			a->type = node->size;
			a->next = allocations;
			allocations = a;
		}
		XplLockRelease( &pool->slabList->freeList.lock );
	}
	dumped = 0;
	if( allocations )
	{
		while( allocations )
		{
			a = allocations;
			allocations = a->next;
			if( a->type < 16384 )
			{
				fprintf( fp, "%ldKB %s:%ld\n", a->type / 1024, (char *)a->dataP, a->dataL );
			}
			else
			{
				fprintf( fp, "%ldMB %s:%ld\n", a->type / (1024 * 1024), (char *)a->dataP, a->dataL );
			}
			node = XplParentOf( a, _MemNode, data );
			_PoolFree( node->slab, node );
			dumped++;
		}
	}
	return dumped;
#else
	return 0;
#endif
}

static int _ValidatePool( _MemPool *pool )
{
#if defined(DEBUG)||defined(DEBUG_ASSERT)
	_MemSlab	*slab;
	char		*data, *slabEnd;
	int			l;
	_MemNode	*node;

#ifdef NEW_LOCKS
	_RWWriteLock( &pool->lock );
#else
	XplLockAcquire( &pool->lock );
#endif
	for(slab=pool->slabList;slab;slab=slab->next)
	{
		data = slab->data;
		slabEnd = slab->data + (pool->pagesPerSlab * _Mem.pageSize);
		XplLockAcquire( &slab->allocList.lock );
		XplLockAcquire( &slab->freeList.lock );
		// validate allocList
		for( node = slab->allocList.list;node;node=node->next )
		{
			CriticalAssert( ( (char *)node >= data ) && ( (char *)node < slabEnd ) );
		}
		// validate freeList
		for( node = slab->freeList.list;node;node=node->next )
		{
			CriticalAssert( ( (char *)node >= data ) && ( (char *)node < slabEnd ) );
		}

		// validate all individual nodes
		for(l=0;l<slab->nodes;l++)
		{
			node = (_MemNode *)(data + ( l * pool->nodeSize ));
			if( slab == node->slab )
			{
				// node is allocated
				_AssertSig( node, TRUE );
				_AssertTrail( node );
			}
			else
			{
				// node is on one of the lists
				if( node->next )
				{
					// node is not the end of the list
					CriticalAssert( ( (char *)node->next >= data ) && ( (char *)node->next < slabEnd ) );
				}
#ifdef DEBUG_ASSERT
				_AssertSigF( node );
				_AssertTrailF( node );
#endif
			}
		}
		XplLockRelease( &slab->freeList.lock );
		XplLockRelease( &slab->allocList.lock );
	}
#ifdef NEW_LOCKS
	_RWWriteUnlock( &pool->lock );
#else
	XplLockRelease( &pool->lock );
#endif

#else
#endif
	return 0;
}

static void _MemoryDump( FILE *fp )
{
	int				l;
#ifdef DUMP_PRIVATE_POOL
	_MemSlab		*slab;
	char			*data;
	_MemNode		*node;
	_PrivatePool	*priv;
#endif

	for(l=0;l<NUMMEMPOOLSIZED;l++)
	{
		_DumpPool( fp, &_Mem.sized[l] );
	}
#ifdef DUMP_PRIVATE_POOL

	for(slab=_Mem.privatePool->slabList;slab;slab=slab->next)
	{
		data = slab->data;
		for(l=0;l<slab->nodes;l++)
		{
			node = (_MemNode *)(data + ( l * _Mem.privatePool->nodeSize ));
			if( slab == node->slab )
			{
				priv = (_PrivatePool *)node->data;
				XplLockAcquire( &_Mem.work.lock );
				if( !priv->released )
				{
					priv->locked = TRUE;
					XplLockRelease(&_Mem.work.lock);
					_DumpPool( fp, &priv->pool );
					_DumpPool( fp, &priv->chunkPool );
					XplLockAcquire( &_Mem.work.lock );
					priv->locked = FALSE;
				}
				XplLockRelease(&_Mem.work.lock);
			}
		}
	}
#endif
	_DumpHugePool( fp, _Mem.hugePool );
}

static void _WorkSchedule( unsigned long type, void *dataP, unsigned long dataL )
{
	_MemWork	*work;
	_MemNode	*node;

	node = _PoolAlloc( _Mem.workPool, 1, NULL, NULL );
	work = (_MemWork *)node->data;
	memset( work, 0, sizeof( _MemWork ) );
	work->type = type;
	work->dataP = dataP;
	work->dataL = dataL;
	XplLockAcquire( &_Mem.work.lock );
	*_Mem.work.tail = work;
	_Mem.work.tail = &work->next;
	XplLockRelease( &_Mem.work.lock );
	XplThreadSignal( _Mem.workThread, SIGINT, NULL );
}

static int _MemoryWorker( XplThread_ thread )
{
	_MemWork	*work;
	_MemPool	*pool;
	_MemNode	*node;
	FILE		*fp;
	time_t		now, next;
	int			index = 0;
	char		workFile[256];
	char		backFile[256];

	if( TrackMemory )
	{
		now = time( NULL );
		next = now + DUMP_INTERVAL;
	}
	else
	{
		next = (time_t)0;
	}
	sprintf( workFile, "Memory-%s-0.txt", _Mem.consumer );
	sprintf( backFile, "Memory-%s-0.bak", _Mem.consumer );
	unlink( backFile );
	rename( workFile, backFile );
	sprintf( workFile, "Memory-%s-1.txt", _Mem.consumer );
	sprintf( backFile, "Memory-%s-1.bak", _Mem.consumer );
	unlink( backFile );
	rename( workFile, backFile );
	sprintf( workFile, "Memory-%s-2.txt", _Mem.consumer );
	sprintf( backFile, "Memory-%s-2.bak", _Mem.consumer );
	unlink( backFile );
	rename( workFile, backFile );

	for(;;)
	{
		XplLockAcquire( &_Mem.work.lock );
		if( work = _Mem.work.list )
		{
			if( !( _Mem.work.list = work->next ) )
			{
				_Mem.work.tail = &_Mem.work.list;
			}
		}
		XplLockRelease( &_Mem.work.lock );
		if( work )
		{
			switch( work->type )
			{
				case MEM_REPORT:
					pool = (_MemPool *)work->dataP;
					sprintf( workFile, "Memory-%s-%s.txt", _Mem.consumer, pool->identity );
					if( fp = fopen( workFile, "w" ) )
					{
						_DumpPool( fp, pool );
						fclose( fp );

						ttyprintf( stderr, "MEMMGR: Report for pool %s saved to \"%s\"\n",
							pool->identity, workFile );
					}
					else
					{
						ttyprintf( stderr, "Could not open file \"%s\" for leak report.\n", workFile );
//						XplPError("slabber");
					}
					break;

				case MEM_DUMP:
					sprintf( workFile, "Memory-%s-%x.txt", _Mem.consumer, (unsigned int)time(NULL) );
					if( fp = fopen( workFile, "w" ) )
					{
						_MemoryDump( fp );
						fclose( fp );
						ttyprintf( stderr, "MEMMGR: Memory dump saved to \"%s\"\n", workFile );
					}
					break;

				default:
					break;
			}

			node = XplParentOf( work, _MemNode, data );
			_PoolFree( node->slab, node );
		}
		if( TrackMemory )
		{
			now = time( NULL );
			if( now > next )
			{
				next = now + DUMP_INTERVAL;
				if( index < 3 )
				{
					sprintf( workFile, "Memory-%s-%d.txt", _Mem.consumer, index );
				}
				else
				{
					sprintf( workFile, "Memory-%s-0.txt", _Mem.consumer );
					unlink( workFile );
					sprintf( backFile, "Memory-%s-1.txt", _Mem.consumer );
					rename( backFile, workFile );
					sprintf( workFile, "Memory-%s-2.txt", _Mem.consumer );
					rename( workFile, backFile );
				}
				index++;
				if( fp = fopen( workFile, "w" ) )
				{
					_MemoryDump( fp );
					fclose( fp );
					ttyprintf( stderr, "MEMMGR: (%s) Memory dump saved to \"%s\"\n", _Mem.consumer, workFile );
				}
			}
		}
		else
		{
			next = (time_t)0;
		}

		// add a second while in TrackMemory mode, so it will be ready when we wake up
		switch( XplThreadCatch( thread, NULL, (next) ? ((next - now) + 1) * 1000 : -1 ) )
		{
			case SIGTERM:
				return 0;

			case SIGINT:
				// we have work
				break;

			default:
				break;
		}
	}
}

#define INIT_POOL( pool, info ) _PoolInit( &_Mem.sized[pool], config->info.name, config->info.pages, config->info.size, config->info.nodes, config->info.depth, config->info.advice );

EXPORT XplBool ZeroMemoryCB( void *ptr, void *client )
{
	_MemNode	*node;

	if( ptr )
	{
		node = XplParentOf( ptr, _MemNode, data );
		memset( ptr, 0, node->size );
		return TRUE;
	}
	return FALSE;
}

static XplBool FreeChunkCB( void *ptr, void *client )
{
	_MemNode	**np;
	int			nodes;
	_MemChunk	*chunk = (_MemChunk *)ptr;

	if( chunk->nodeList )
	{
		// put the nodes back on the slab
		for(np=&chunk->nodeList->next,nodes=1;*np;np=&(*np)->next)
		{
			nodes++;
		}
		switch( chunk->type )
		{
			case NODE_TYPE_FREE:
				XplLockAcquire( &chunk->slab->freeList.lock );
				*np = chunk->slab->freeList.list;
				chunk->slab->freeList.list = chunk->nodeList;
				chunk->slab->freeList.nodes += nodes;
				XplLockRelease( &chunk->slab->freeList.lock );
				break;

			case NODE_TYPE_ALLOC:
				XplLockAcquire( &chunk->slab->allocList.lock );
				*np = chunk->slab->allocList.list;
				chunk->slab->allocList.list = chunk->nodeList;
				chunk->slab->allocList.nodes += nodes;
				XplLockRelease( &chunk->slab->allocList.lock );
				break;
		}
	}
	return TRUE;
}

static XplBool PreparePrivateCB( void *ptr, void *client )
{
	_PrivatePool	*priv = (_PrivatePool *)ptr;

	memset( priv, 0, sizeof( _PrivatePool ) );
	_PoolInit( &priv->chunkPool, "chunk", 1, sizeof( _MemChunk ), 0, 0, MEMMGR_ADVICE_SMALL );
	priv->chunkPool.flags |= MEM_POOL_PRIVATE;
	priv->chunkPool.prepareCB = ZeroMemoryCB;
	priv->chunkPool.freeCB = FreeChunkCB;

	return TRUE;
}

static XplBool FreePrivateCB( void *ptr, void *client )
{
	_PrivatePool	*priv = (_PrivatePool *)ptr;

	XplLockAcquire( &_Mem.work.lock );
	while( priv->locked)
	{
		XplLockRelease(&_Mem.work.lock);
		XplDelay( 10 );
		XplLockAcquire( &_Mem.work.lock );
	}
	priv->released = TRUE;
	XplLockRelease(&_Mem.work.lock);

	// destroy outstanding chunks
	_ClearPool( &priv->chunkPool );

	// destroy outstanding slabs
	_ClearPool( &priv->pool );
	return TRUE;
}

/*
    Consumers tracking
*/
static void AddConsumer(const char *consumer)
{
	_MemNode    *node;
    _MemString  *memstr;

    if (!consumer) // leave '""' as it is
    {
        consumer = "_";
    }

    node = _PoolAlloc( _Mem.stringPool, 1, NULL, NULL );
    memstr = (_MemString *)node->data;

    strncpy(memstr->data, consumer, MAX_MEM_STRING - 1);

    // Link into list
    XplLockAcquire(&_Mem.consumers.lock);

    memstr->next = _Mem.consumers.list;
    _Mem.consumers.list = memstr;

    XplLockRelease(&_Mem.consumers.lock);
}

// It would crash if given consumer is not found
static void RemoveConsumer(const char *consumer)
{
    _MemString  **memstrp, *memstr = NULL;

    if (!consumer) // leave '""' as it is
    {
        consumer = "_";
    }

    XplLockAcquire(&_Mem.consumers.lock);

    // Find consumer by name and unlink from list
    for (memstrp = &_Mem.consumers.list; *memstrp; memstrp = &(*memstrp)->next)
    {
        if (!stricmp((*memstrp)->data, consumer))
        {
            memstr = *memstrp;
            *memstrp = (*memstrp)->next;
            break;
        }
    }

    XplLockRelease(&_Mem.consumers.lock);

    // Free node if found, otherwise assert
    if( memstr )
    {
        MMFree(memstr, __FILE__, __LINE__);
    }
    else
	{
		ttyprintf( stderr, "MEMMGR: Consumer '%s' closed the memory manager too many times.\n", consumer );
		CriticalAssert( 0 );
    }
}

// End Consumers tracking

#ifdef DEBUG
#define MakeConsumerReport(c) _MakeConsumersReport((c))
static void _MakeConsumersReport( char *consumer )
{
	FILE		*f;
	int			dumped = 0;
	char		workFile[1024];

	strprintf( workFile, sizeof( workFile ), NULL, "MemoryLeaks-%s.txt", consumer );
	if( f = fopen( workFile, "w" ) )
	{
		dumped += _DumpPool( f, &_Mem.sized[MEMPOOL_32] );
		dumped += _DumpPool( f, &_Mem.sized[MEMPOOL_64] );
		dumped += _DumpPool( f, &_Mem.sized[MEMPOOL_128] );
		dumped += _DumpPool( f, &_Mem.sized[MEMPOOL_256] );
		dumped += _DumpPool( f, &_Mem.sized[MEMPOOL_512] );
		dumped += _DumpPool( f, &_Mem.sized[MEMPOOL_1K] );
		dumped += _DumpPool( f, &_Mem.sized[MEMPOOL_2K] );
		dumped += _DumpPool( f, &_Mem.sized[MEMPOOL_4K] );
		dumped += _DumpPool( f, &_Mem.sized[MEMPOOL_8K] );
		dumped += _DumpPool( f, &_Mem.sized[MEMPOOL_16K] );
		dumped += _DumpPool( f, &_Mem.sized[MEMPOOL_32K] );
		dumped += _DumpPool( f, &_Mem.sized[MEMPOOL_64K] );
		dumped += _DumpPool( f, &_Mem.sized[MEMPOOL_128K] );
		dumped += _DumpPool( f, &_Mem.sized[MEMPOOL_256K] );
		dumped += _DumpPool( f, &_Mem.sized[MEMPOOL_512K] );
		dumped += _DumpPool( f, &_Mem.sized[MEMPOOL_1M] );

		fclose(f);

		if( !dumped )
		{
			unlink( workFile );
		}
		else
		{
			ttyprintf( stderr, "MEMMGR: Memory leaks detected, report saved to %s\n", workFile );
		}
	}
}
#else
#define MakeConsumerReport(c)
#endif

EXPORT XplBool MMOpenEx( const char *consumer, MemPoolConfig *config, const char *file, unsigned long line )
{
	static volatile int initState = 0;

	/*
		Make stdout line buffered.  This is done here just because we need
		something that is called by everything.
	*/
#ifdef LINUX
	setlinebuf(stdout);
#endif

	if( !initState++ )
	{
		if( !config )
		{
			config = &MemoryDefaultConfig;
		}
		_Mem.pageSize = 0x00001000;
#ifdef HAVE_SYSCONF
		_Mem.pageSize = sysconf(_SC_PAGE_SIZE);
#endif
		XplLockInit( &_Mem.work.lock );
		_Mem.work.list = NULL;
		_Mem.work.tail = &_Mem.work.list;

		// create work pool
		_Mem.workPool = &_Mem.general[MEMPOOL_GENERAL_WORK];
		_PoolInit( _Mem.workPool,	"work",	1,	sizeof( _MemWork ),	0,	0, MEMMGR_ADVICE_SMALL );

		// create string pool
		_Mem.stringPool = &_Mem.general[MEMPOOL_GENERAL_STRING];
		_PoolInit( _Mem.stringPool,	"string",	16,	sizeof( _MemString ),	0,	0, MEMMGR_ADVICE_SMALL );

		// create private pool pool
		_Mem.privatePool = &_Mem.general[MEMPOOL_GENERAL_PRIVATE];
		_PoolInit( _Mem.privatePool,	"private",	4,	sizeof( _PrivatePool ),	0,	0, MEMMGR_ADVICE_SMALL );
		_Mem.privatePool->prepareCB = PreparePrivateCB;
		_Mem.privatePool->freeCB = FreePrivateCB;

		// create HUGE alloc pool, it is not slabbed
		// it's slabs freelist contains the alloc list
		_Mem.hugePool = &_Mem.general[MEMPOOL_GENERAL_HUGE];
		_PoolInit( _Mem.hugePool,	"huge", 1,	1,		1,	0, MEMMGR_ADVICE_LARGE );
		_Mem.hugePool->size = 0;
		_Mem.hugePool->nodeSize = 0;
		_Mem.hugePool->pagesPerSlab = 0;
		_Mem.hugePool->slabList = (_MemSlab *)malloc( sizeof( _MemSlab ) );
		memset( _Mem.hugePool->slabList, 0, sizeof( _MemSlab ) );
		XplLockInit( &_Mem.hugePool->slabList->freeList.lock );
		INIT_POOL( MEMPOOL_32,		info32 );
		INIT_POOL( MEMPOOL_64,		info64 );
		INIT_POOL( MEMPOOL_128,		info128 );
		INIT_POOL( MEMPOOL_256,		info256 );
		INIT_POOL( MEMPOOL_512,		info512 );
		INIT_POOL( MEMPOOL_1K,		info1k );
		INIT_POOL( MEMPOOL_2K,		info2k );
		INIT_POOL( MEMPOOL_4K,		info4k );
		INIT_POOL( MEMPOOL_8K,		info8k );
		INIT_POOL( MEMPOOL_16K,		info16k );
		INIT_POOL( MEMPOOL_32K,		info32k );
		INIT_POOL( MEMPOOL_64K,		info64k );
		INIT_POOL( MEMPOOL_128K,	info128k );
		INIT_POOL( MEMPOOL_256K,	info256k );
		INIT_POOL( MEMPOOL_512K,	info512k );
		INIT_POOL( MEMPOOL_1M,		info1m );

#if 0
		// static allocation for small pools
		_PoolInit( &_Mem.sized[MEMPOOL_32],		"32",	2,		32,		0,		8, MEMMGR_ADVICE_SMALL );
		_PoolInit( &_Mem.sized[MEMPOOL_64],		"64",	4,		64,		0,		8, MEMMGR_ADVICE_SMALL );
		_PoolInit( &_Mem.sized[MEMPOOL_128],	"128",	8,		128,	0,		8, MEMMGR_ADVICE_SMALL );
		_PoolInit( &_Mem.sized[MEMPOOL_256],	"256",	16,		256,	0,		8, MEMMGR_ADVICE_SMALL );
		_PoolInit( &_Mem.sized[MEMPOOL_512],	"512",	32,		512,	0,		8, MEMMGR_ADVICE_SMALL );
		// dynamic allocation
		_PoolInit( &_Mem.sized[MEMPOOL_32],		"32",	2,		0,		256,	8, MEMMGR_ADVICE_SMALL );
		_PoolInit( &_Mem.sized[MEMPOOL_64],		"64",	4,		0,		256,	8, MEMMGR_ADVICE_SMALL );
		_PoolInit( &_Mem.sized[MEMPOOL_128],	"128",	8,		0,		256,	8, MEMMGR_ADVICE_SMALL );
		_PoolInit( &_Mem.sized[MEMPOOL_256],	"256",	16,		0,		256,	8, MEMMGR_ADVICE_SMALL );
		_PoolInit( &_Mem.sized[MEMPOOL_512],	"512",	32,		0,		256,	8, MEMMGR_ADVICE_SMALL );

		_PoolInit( &_Mem.sized[MEMPOOL_1K],		"1k",	32,		0,		128,	4, MEMMGR_ADVICE_SMALL );
		_PoolInit( &_Mem.sized[MEMPOOL_2K],		"2k",	64,		0,		128,	4, MEMMGR_ADVICE_SMALL );
		_PoolInit( &_Mem.sized[MEMPOOL_4K],		"4k",	128,	0,		128,	4, MEMMGR_ADVICE_SMALL );
		_PoolInit( &_Mem.sized[MEMPOOL_8K],		"8k",	128,	0,		64,		4, MEMMGR_ADVICE_NORMAL );
		_PoolInit( &_Mem.sized[MEMPOOL_16K],	"16k",	256,	0,		64,		4, MEMMGR_ADVICE_NORMAL );
		_PoolInit( &_Mem.sized[MEMPOOL_32K],	"32k",	256,	0,		32,		4, MEMMGR_ADVICE_NORMAL );
		_PoolInit( &_Mem.sized[MEMPOOL_64K],	"64k",	512,	0,		32,		4, MEMMGR_ADVICE_NORMAL );
		_PoolInit( &_Mem.sized[MEMPOOL_128K],	"128k",	512,	0,		16,		4, MEMMGR_ADVICE_NORMAL );
		_PoolInit( &_Mem.sized[MEMPOOL_256K],	"256k",	512,	0,		8,		4, MEMMGR_ADVICE_NORMAL );
		_PoolInit( &_Mem.sized[MEMPOOL_512K],	"512k",	512,	0,		4,		4, MEMMGR_ADVICE_NORMAL );
		_PoolInit( &_Mem.sized[MEMPOOL_1M],		"1m",	1024,	0,		4,		4, MEMMGR_ADVICE_NORMAL );
#endif
		XplLockInit(&_Mem.consumers.lock );
		_Mem.consumers.list = NULL;

		XplLockAcquire( &_Mem.work.lock );
		_Mem.state = _MemInitialized;

		_Mem.threadGroup = XplThreadGroupCreate( "slabber" );
		XplThreadStart( _Mem.threadGroup, _MemoryWorker, NULL, &_Mem.workThread );

		_Mem.useCount = 0;	// incremented at bottom
		_Mem.consumer = MMStrdup( consumer, NULL, TRUE, __FILE__, __LINE__ );
		XplLockRelease( &_Mem.work.lock );
	}
	XplLockAcquire( &_Mem.work.lock );
	while( _MemInitialized != _Mem.state )
	{
		XplLockRelease( &_Mem.work.lock );
		XplDelay( 50 );
		XplLockAcquire( &_Mem.work.lock );
	}
	_Mem.useCount++;
	XplLockRelease( &_Mem.work.lock );
    // Consumers track
    AddConsumer(consumer);
	return TRUE;
}

EXPORT XplBool MMClose( const char *consumer, XplBool xplMain, const char *file, unsigned long line )
{
	char		memConsumer[256];
	_MemString	*str;

	CriticalAssert( _MemInitialized == _Mem.state );	// slabber not initialized

	// Consumers track
	RemoveConsumer(consumer);

	XplLockAcquire( &_Mem.work.lock );
	if( _Mem.consumer && !stricmp( consumer, _Mem.consumer ) )
	{
		XplLockAcquire( &_Mem.consumers.lock );
		if( _Mem.consumers.list )
		{
//			ttyprintf( stderr, "ERROR: Primary memory consumer closed with other consumers still active.\n" );
			for(str=_Mem.consumers.list;str;str=str->next)
			{
//				ttyprintf(stderr, "Consumer: '%s'\r\n", str->data);
			}
		}
		XplLockRelease( &_Mem.consumers.lock );
	}
	if( xplMain )
	{
		if( 1 != _Mem.useCount )
		{
			ttyprintf( stderr, "MEMMGR: Memory manager not closed by all consumers.\n" );
			CriticalAssert( _Mem.consumer );
			CriticalAssert( !strcmp( consumer, _Mem.consumer ) );
			strprintf( memConsumer, sizeof( memConsumer ), NULL, "%s", _Mem.consumer );
			MemRelease( &_Mem.consumer );

			for(str=_Mem.consumers.list;str;str=str->next)
			{
				ttyprintf( stderr, "MEMMGR: Consumer '%s' still has memory manager open.\n", str->data );
			}

			XplLockRelease( &_Mem.work.lock );

			XplThreadFree( &_Mem.workThread );
			XplThreadGroupDestroy( &_Mem.threadGroup );
			XplThreadShutdown();
			_Mem.state = _MemLoaded;
			// Consumers report
			MakeConsumerReport( memConsumer );

			return TRUE;
		}
	}
	if (!--_Mem.useCount)
	{
		CriticalAssert( _Mem.consumer );
		CriticalAssert( !strcmp( consumer, _Mem.consumer ) );
		strprintf( memConsumer, sizeof( memConsumer ), NULL, "%s", _Mem.consumer );
		MemRelease( &_Mem.consumer );

		XplLockRelease( &_Mem.work.lock );

		CriticalAssert( !_Mem.consumers.list );

		XplThreadFree( &_Mem.workThread );
		XplThreadGroupDestroy( &_Mem.threadGroup );
		XplThreadShutdown();
		_Mem.state = _MemLoaded;
		// Consumers report
		MakeConsumerReport( memConsumer );

		return TRUE;
	}
	XplLockRelease( &_Mem.work.lock );

	return TRUE;
}

EXPORT void MMUpdateOwner( void *ptr, const char *file, unsigned long line )
{
	_MemNode	*node;

	CriticalAssert( _MemInitialized == _Mem.state );	// slabber not initialized
	MMAssert( ptr );
	if( ptr )
	{
		node = XplParentOf( ptr, _MemNode, data );
		if( node->slab == _Mem.hugePool->slabList )
		{
			_SetAlloc( node, _MemStringLookup( _Mem.hugePool, file ), line );
		}
		else
		{
			_SetAlloc( node, _MemStringLookup( node->pool, file ), line );
		}
	}
}

EXPORT void MMLock( void *ptr, uint32 key )
{
	_MemNode	*node;

	CriticalAssert( _MemInitialized == _Mem.state );	// slabber not initialized
	MMAssert( ptr );
	if( ptr ) {
		node = XplParentOf( ptr, _MemNode, data );

		/* A node can not be locked if it has already been locked */
		DebugAssert(0 == node->key);

		node->key = key;
	}
}

EXPORT void MMUnlock( void *ptr, uint32 key )
{
	_MemNode	*node;

	CriticalAssert( _MemInitialized == _Mem.state );	// slabber not initialized
	MMAssert( ptr );
	if( ptr ) {
		node = XplParentOf( ptr, _MemNode, data );

		DebugAssert(key == node->key);

		node->key = 0;
	}
}

EXPORT void MMCopyOwner( void *dst, void *src )
{
#if defined(DEBUG)||defined(DEBUG_ASSERT)
	_MemNode	*nd, *ns;

	CriticalAssert( _MemInitialized == _Mem.state );	// slabber not initialized
	if( dst && src )
	{
		nd = XplParentOf( dst, _MemNode, data );
		ns = XplParentOf( src, _MemNode, data );
		// no lookup on copy
		_SetAlloc( nd, ns->allocInfo.file, ns->allocInfo.line );
	}
#endif
}

EXPORT void MMGetOwner( void *ptr, const char **file, unsigned long *line )
{
	CriticalAssert( _MemInitialized == _Mem.state );	// slabber not initialized
	if( file )
	{
#if defined(DEBUG)||defined(DEBUG_ASSERT)
		if( ptr )
		{
			_MemNode	*node;

			node = XplParentOf( ptr, _MemNode, data );
			*file = node->allocInfo.file;
			*line = node->allocInfo.line;
		}
		else
#endif
		{
			*file = "unknown";
			if( line )
			{
				*line = 0;
			}
		}
	}
}

EXPORT void MMAssert( void *ptr )
{
	_MemNode	*node;

	CriticalAssert( _MemInitialized == _Mem.state );	// slabber not initialized
	if( ptr )
	{
		node = XplParentOf( ptr, _MemNode, data );
		if( !MEM_SIG_SIZE || ( node->sig.data[0].redz != MEMMGR_SIG_WATCH ) || ( node->sig.data[0].one != MEMMGR_SIG_ING ) )
		{
			_AssertSig( node, FALSE );
		}
		DebugAssert( node->size > 0 );
		_AssertTrail( node );
	}
}

static void *_MMAlloc( size_t size, size_t *actual, XplBool wait, const char *file, unsigned long line )
{
	int			i;
	_MemNode	*node;

	// set i to NUMMEMPOOLSIZED to get rid of compiler warning
	CriticalAssert( ( 16 == ( i = NUMMEMPOOLSIZED ) ) );	// static memory pools modified

	// Catch negative (or > 50% of virtual space) allocation.
	if( ((ssize_t) size) < 0)
	{
		if( wait )
		{
			CriticalAssert( 0 );	/* wait-form allocation of negative size */
			abort();			/* wait can never return NULL */
		}
		return NULL;
	}

	i = 0;
	if( size > _Mem.sized[i + 7].size ) i += 8;
	if( size > _Mem.sized[i + 3].size ) i += 4;
	if( size > _Mem.sized[i + 1].size ) i += 2;
	if( size > _Mem.sized[i].size )
	{
		++i;
		if( size > _Mem.sized[i].size )
		{
			do {				// huge alloc path
				if( node = (_MemNode *)SysAlloc( size + MEMNODEOVERHEAD, actual, MEMMGR_ADVICE_LARGE ) )
				{
					memset( node, 0, sizeof( _MemNode ) );
					node->slab = _Mem.hugePool->slabList;
					if( actual )
					{
						*actual -= sizeof( _MemNode );
#ifdef DEBUG_ASSERT
						*actual -= sizeof( _MemSig );
#endif
						node->size = *actual;
					}
					else
					{
						node->size = size;
					}
					_SetSig( node );
					_SetTrail( node );
					_SetAlloc( node, _MemStringLookup( _Mem.hugePool, file ), line );
					if( _Mem.hugePool->prepareCB )
					{
						_Mem.hugePool->prepareCB( node->data, _Mem.hugePool->clientData );
					}
					XplLockAcquire( &_Mem.hugePool->slabList->freeList.lock );
					_Mem.hugePool->slabs++;
					node->hugeLink = _Mem.hugePool->slabList->freeList.list;
					_Mem.hugePool->slabList->freeList.list = node;
					XplLockRelease( &_Mem.hugePool->slabList->freeList.lock );
					return node->data;
				}
			}while( wait );
			return NULL;
		}
	}
								/* _PoolAlloc always succeeds */
	node = _PoolAlloc( &_Mem.sized[i], 1, NULL, NULL );
	if( actual )
	{
		node->size = *actual = _Mem.sized[i].size;
	}
	else
	{
		_ClearTrail( node );
		node->size = size;
	}
	_SetAlloc( node, _MemStringLookup( &_Mem.sized[i], file ), line );
	_SetTrail( node );
	return node->data;
}


EXPORT void *MMAllocEx( void *ptr, size_t size, size_t *actual, XplBool wait, XplBool zero, const char *file, unsigned long line )
{
	_MemNode	*node;
	char		*mem;
	char		*end;

	CriticalAssert( _MemInitialized == _Mem.state );	// slabber not initialized
	if( ptr )
	{
		// resizing within a node that is large enough to fit
		node = XplParentOf( ptr, _MemNode, data );
		_AssertSig( node, TRUE );
		_AssertTrail( node );
		if( ( node->slab != _Mem.hugePool->slabList ) && ( size <= node->pool->size ) )
		{
			end = node->data + node->size;
			_ClearTrail( node );
			if( actual )
			{
				node->size = *actual = node->pool->size;
			}
			else
			{
				node->size = size;
			}
			_SetTrail( node );
			if( zero )
			{
				memset( end, 0, (node->data + node->size) - end );
			}
			if( node->slab == _Mem.hugePool->slabList )
			{
				_SetAlloc( node, _MemStringLookup( _Mem.hugePool, file ), line );
			}
			else
			{
				_SetAlloc( node, _MemStringLookup( node->pool, file ), line );
			}
			return node->data;
		}
	}
	else
	{
		node = NULL;
	}

	if( mem = _MMAlloc( size, actual, wait, file, line ) )
	{
		if( actual )
		{
			size = *actual;
		}

		if( node )
		{
			if (node->key)
			{
				MMLock(mem, node->key);
				node->key = 0;
			}

			// was a realloc
			if( size > node->size )
			{
				memcpy( mem, node->data, node->size );
				if( zero )
				{
					memset( mem + node->size, 0, size - node->size );
				}
			}
			else
			{
				memcpy( mem, node->data, size );
			}
			_NodeFree( node, file, line );
		}
		else
		{
			// normal alloc
			if( zero )
			{
				memset( mem, 0, size );
			}
		}
		return mem;
	}
	return NULL;
}

EXPORT size_t MMSize( void *ptr )
{
	_MemNode	*node;

	CriticalAssert( _MemInitialized == _Mem.state );	// slabber not initialized
	if( ptr )
	{
		node = XplParentOf( ptr, _MemNode, data );
		if( node->size != node->pool->size )
		{
			_ClearTrail( node );
			node->size = node->pool->size;
			_SetTrail( node );
		}
		return node->size;
	}
	return 0;
}

EXPORT void *MMFree( void *ptr, const char *file, unsigned long line )
{
	_MemNode	*node;

	CriticalAssert( _MemInitialized == _Mem.state );	// slabber not initialized
	if( ptr )
	{
		node = XplParentOf( ptr, _MemNode, data );
		_NodeFree( node, file, line );
	}
	return NULL;
}

EXPORT char *MMDupEx( const char *src, size_t size, XplBool wait, const char *file, unsigned long line )
{
	char *mem;

	if( mem = MMAllocEx( NULL, size, NULL, wait, FALSE, file, line ) )
	{
		memcpy( mem, src, size );
	}
	return mem;
}

EXPORT char *MMStrdup( const char *string, size_t *actual, XplBool wait, const char *file, unsigned long line )
{
	char *mem;

	CriticalAssert( _MemInitialized == _Mem.state );	// slabber not initialized
	if( string )
	{
		if( mem = _MMAlloc( strlen( string ) + 1, actual, wait, file, line ) )
		{
			strcpy( mem, string );
		}
		return mem;
	}
	return NULL;
}

EXPORT char *MMStrndup( const char *string, size_t *actual, size_t maxSize, XplBool wait, const char *file, unsigned long line )
{
	char *mem;

	CriticalAssert( _MemInitialized == _Mem.state );	// slabber not initialized
	if( string )
	{
		if( strlen( string ) < maxSize )
		{
			maxSize = strlen( string );
		}
		if( mem = _MMAlloc( maxSize + 1, actual, wait, file, line ) )
		{
			strncpy( mem, string, maxSize );
			mem[maxSize] = '\0';
		}
		return mem;
	}
	return NULL;
}

EXPORT char *MMSprintf( const char *file, unsigned long line, const char *format, ... )
{
	char *buffer, *s;
	va_list	args;
	int size;
	size_t needed;

	CriticalAssert( _MemInitialized == _Mem.state );	// slabber not initialized
	if( buffer = _MMAlloc( 200, NULL, 0, file, line ) )
	{
		va_start( args, format );
		size = vstrprintf( buffer, 200, &needed, format, args );
		va_end( args );
		if( size < 200 )
		{
			return buffer;
		}
		s = buffer;
		if( buffer = MMAllocEx( buffer, needed+1, NULL, 0, 0, file, line ) )
		{
			va_start( args, format );
			size = vstrprintf( buffer, needed+1, NULL, format, args );
			va_end( args );
			return buffer;
		}
		MMFree( s, file, line );
	}
	return NULL;
}

EXPORT int MMAsprintf( char **ptr, const char *file, unsigned long line, const char *format, ... )
{
	char *buffer;
	va_list	args;
	int size;
	size_t needed;

	CriticalAssert( _MemInitialized == _Mem.state );	// slabber not initialized
	if( ptr )
	{
		if( buffer = _MMAlloc( 200, NULL, 0, file, line ) )
		{
			va_start( args, format );
			size = vstrprintf( buffer, 200, &needed, format, args );
			va_end( args );
			if( needed < 200 )
			{
				*ptr = buffer;
				return size;
			}
			if( *ptr = MMAllocEx( buffer, needed+1, NULL, 0, 0, file, line ) )
			{
				buffer = *ptr;

				va_start( args, format );
				size = vstrprintf( buffer, needed+1, NULL, format, args );
				va_end( args );
				return size;
			}
			MMFree( buffer, file, line );
		}
		*ptr = NULL;
	}
	return -1;
}

EXPORT int MMStrcatv( char **ptr, const char *file, unsigned long line, const char *format, va_list args )
{
	char *tmp;
	size_t	size, needed;
	va_list	argCopy;

	CriticalAssert( ptr );
	CriticalAssert( format );

	if( *ptr )
	{
		size = MMSize( *ptr );
	}
	else
	{
		if( !( *ptr = MMAllocEx( NULL, 64, &size, FALSE, TRUE, file, line ) ) )
		{
			return -1;
		}
		*(*ptr) = '\0';
	}

	va_copy( argCopy, args );
	vstrcatf( *ptr, size, &needed, format, argCopy );

	if( needed > size )
	{
		if( tmp = MMAllocEx( *ptr, needed, &size, FALSE, TRUE, file, line ) )
		{
			*ptr = tmp;

			va_copy( argCopy, args );
			vstrcatf( *ptr, size, &needed, format, argCopy );
			return 0;
		}
		return -1;
	}
	return 0;
}

EXPORT int MMStrcatf( char **ptr, const char *file, unsigned long line, const char *format, ... )
{
	int			r;
	va_list		args;

	va_start( args, format );
	r = MMStrcatv( ptr, file, line, format, args );
	va_end( args );

	return r;
}

EXPORT void MMGenerateReports( void )
{
	CriticalAssert( _MemInitialized == _Mem.state );	// slabber not initialized
	_WorkSchedule( MEM_DUMP, NULL, 0 );
}

EXPORT void MMDumpPools( FILE *fp )
{
	CriticalAssert( _MemInitialized == _Mem.state );	// slabber not initialized
	_MemoryDump( fp );
}

EXPORT const char *MMConsumer( void )
{
	CriticalAssert( _MemInitialized == _Mem.state );	// slabber not initialized
	return (const char *)_Mem.consumer;
}

EXPORT MemPool *MMPoolAlloc( const char *identity, size_t size, size_t nodesPerSlab, PoolEntryCB prepareCB, PoolEntryCB freeCB, PoolEntryCB destroyCB, void *clientData, const char *file, unsigned long line )
{
	_MemNode		*node;
	_PrivatePool	*priv;
	size_t			pagesPerSlab;

	CriticalAssert( _MemInitialized == _Mem.state );	// slabber not initialized
	pagesPerSlab = nodesPerSlab * ( size + MEMNODEOVERHEAD );
	pagesPerSlab = pagesPerSlab / _Mem.pageSize;
	pagesPerSlab++;

	node = _PoolAlloc( _Mem.privatePool, 1, NULL, NULL );
	_SetAlloc( node, _MemStringLookup( _Mem.privatePool, file ), line );

	priv = (_PrivatePool *)node->data;

	_PoolInit( &priv->pool, _MemStringLookup( _Mem.privatePool, identity ), pagesPerSlab, size, 0, 0, (size < 4096) ? MEMMGR_ADVICE_SMALL : MEMMGR_ADVICE_NORMAL );
	priv->pool.flags |= MEM_POOL_PRIVATE;
	priv->pool.prepareCB = prepareCB;
	priv->pool.freeCB = freeCB;
	priv->pool.destroyCB = destroyCB;
	priv->pool.clientData = clientData;
	return (MemPool *)priv;
}

EXPORT void MMPoolAbandon( MemPool *handle )
{
	// TODO: implement
	CriticalAssert( _MemInitialized == _Mem.state );	// slabber not initialized
}

EXPORT void *MMPoolGet( MemPool *handle, const char *file, unsigned long line )
{
	_MemNode		*node;
	_PrivatePool	*priv = (_PrivatePool *)handle;

	CriticalAssert( handle );
	CriticalAssert( _MemInitialized == _Mem.state );	// slabber not initialized
	if( priv )
	{
		node = _PoolAlloc( &priv->pool, 1, NULL, NULL );
		_SetAlloc( node, _MemStringLookup( &priv->pool, file ), line );
		return node->data;
	}
	return NULL;
}

EXPORT MemChunk *MMPoolGetChunk( MemPool *handle, size_t nodes, const char *file, unsigned long line )
{
	_MemNode		*node;
	_MemChunk		*chunk;
	_PrivatePool	*priv = (_PrivatePool *)handle;

	CriticalAssert( _MemInitialized == _Mem.state );	// slabber not initialized
	if( priv )
	{
		node = _PoolAlloc( &priv->chunkPool, 1, NULL, NULL );
		_SetAlloc( node, _MemStringLookup( &priv->pool, file ), line );
		chunk = (_MemChunk *)node->data;
		chunk->nodeList = _PoolAlloc( &priv->pool, nodes, &chunk->slab, &chunk->type );
		return (MemChunk *)chunk;
	}
	return NULL;
}

EXPORT void *MMChunkGet( MemChunk *chunk, const char *file, unsigned long line )
{
	_MemChunk		*c = (_MemChunk *)chunk;
	_MemNode		*node;

	CriticalAssert( _MemInitialized == _Mem.state );	// slabber not initialized
	if( c && c->nodeList )
	{
		node = c->nodeList;
		c->nodeList = node->next;
		node->slab = c->slab;
		_SetSig( node );
		_SetTrail( node );
		_SetAlloc( node, file, line );
		if( NODE_TYPE_ALLOC == c->type )
		{
			if( node->pool->prepareCB )
			{
				node->pool->prepareCB( node->data, node->pool->clientData );
			}
		}
		return node->data;
	}
	return NULL;
}

EXPORT int MMPoolValidate( MemPool *handle )
{
	_PrivatePool	*priv = (_PrivatePool *)handle;
	int				l;

	if( priv )
	{
		return _ValidatePool( &priv->pool );
	}
	else
	{
		for(l=0;l<NUMMEMPOOLSIZED;l++)
		{
			if( _ValidatePool( &_Mem.pool[l] ) )
			{
				return -1;
			}
		}
		return 0;
	}
	return -1;
}

EXPORT void MMTrackMemory( int on )
{
	TrackMemory = on;
}

EXPORT void MMWatch( void *ptr )
{
	_MemNode	*node;

	CriticalAssert( _MemInitialized == _Mem.state );	// slabber not initialized
	if( ptr )
	{
		node = XplParentOf( ptr, _MemNode, data );
		if ( MEM_SIG_SIZE ){
			_AssertSig( node, FALSE );
			node->sig.data[0].redz = MEMMGR_SIG_WATCH;
			node->sig.data[0].one = MEMMGR_SIG_ING;
		}
	}
}

EXPORT void MMUnwatch( void *ptr )
{
	_MemNode	*node;

	CriticalAssert( _MemInitialized == _Mem.state );	// slabber not initialized
	if( ptr )
	{
		node = XplParentOf( ptr, _MemNode, data );
		if ( MEM_SIG_SIZE ){
			CriticalAssert( node->sig.data[0].redz == MEMMGR_SIG_WATCH );
			CriticalAssert( node->sig.data[0].one == MEMMGR_SIG_ING );
			node->sig.data[0].redz = MEMMGR_SIG_REDZ;
			node->sig.data[0].one = MEMMGR_SIG_ONE;
		}
	}
}

EXPORT void MMDiscard_is_depricated( void *ptr, const char *file, unsigned long line )
{
	_MemNode	*node;

	CriticalAssert( _MemInitialized == _Mem.state );	// slabber not initialized
	if( ptr )
	{
		node = XplParentOf( ptr, _MemNode, data );
		CriticalAssert( node->slab );	// MMFree called on a stack buffer
		_AssertSig( node, TRUE );
		_AssertTrail( node );
		_SetFree( node, _MemStringLookup( node->pool, file ), line );
		_SetSigF( node );
		_SetTrailF( node );
		_PoolDiscard( node );
	}
}


EXPORT void MMPoolStats( MemPool *handle, MemStatistics *stats )
{
	CriticalAssert( _MemInitialized == _Mem.state );	// slabber not initialized
	if( stats )
	{
		memset( stats, 0, sizeof( MemStatistics ) );
	}
}

EXPORT MemStatistics *MMAllocStatistics( const char *file, unsigned long line )
{
	MemStatistics *stats;

	CriticalAssert( _MemInitialized == _Mem.state );	// slabber not initialized
	if( stats = _MMAlloc( sizeof( MemStatistics ), NULL, 0, file, line ) )
	{
		memset( stats, 0, sizeof( MemStatistics ) );
	}
	return stats;
}

EXPORT void MMFreeStatistics( MemStatistics *stats, const char *file, unsigned long line )
{
	CriticalAssert( _MemInitialized == _Mem.state );	// slabber not initialized
	if( stats )
	{
		MMFree( stats, file, line );
	}
}

EXPORT void MMPoolEnumerate( MemPool *handle, PoolEntryCB cb, void *clientData )
{
	CriticalAssert( _MemInitialized == _Mem.state );	// slabber not initialized
}

EXPORT void MMPoolEnumerateID( const char *identity, PoolEnumerateCB cb, void *clientData )
{
	CriticalAssert( _MemInitialized == _Mem.state );	// slabber not initialized
}

