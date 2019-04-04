#ifndef __SLAB_H__
#define __SLAB_H__

#define MEM_POOL_GROWING	0x00000001
#define MEM_POOL_PRIVATE	0x00000002

#define MAX_MEM_STRING			256

// always keep this a power of two
#define MEM_STRING_TABLE_COUNT	32

typedef enum
{
	NODE_TYPE_FREE = 0,
	NODE_TYPE_ALLOC
}_NodeType;

typedef enum {
	MEMPOOL_GENERAL_WORK = 0,
	MEMPOOL_GENERAL_STRING,
	MEMPOOL_GENERAL_PRIVATE,
	MEMPOOL_GENERAL_HUGE,
	NUMMEMPOOLGENERAL
} _MemPoolGeneral;

typedef enum {
	MEMPOOL_32 = 0,
	MEMPOOL_64,
	MEMPOOL_128,
	MEMPOOL_256,
	MEMPOOL_512,
	MEMPOOL_1K,
	MEMPOOL_2K,
	MEMPOOL_4K,
	MEMPOOL_8K,
	MEMPOOL_16K,
	MEMPOOL_32K,
	MEMPOOL_64K,
	MEMPOOL_128K,
	MEMPOOL_256K,
	MEMPOOL_512K,
	MEMPOOL_1M,
	NUMMEMPOOLSIZED
} _MemPoolSized;

typedef struct _MemNode
{
	union
	{
		// stack nodes will have this set to NULL
		struct _MemSlab	*slab;	// when allocated
		struct _MemNode	*next;	// when free
	};
	union
	{
		struct _MemNode	*hugeLink;	// when allocated and in huge pool
		struct _MemPool	*pool;		// when allocated and in normal pool
	};
	size_t				size;

/* #if defined(DEBUG)||defined(DEBUG_ASSERT) */
	_MemInfo			allocInfo;
	uint32				key;
/*
#endif
#ifdef DEBUG_ASSERT
*/
	struct
	{
		_MemInfo		lastAlloc;
		_MemInfo		info;
	}freeInfo;
	_MemSig				sig;
/* #endif */
	char				data[];
}_MemNode;

typedef struct
{
	XplLock				lock;
	_MemNode			*list;
	int					nodes;
}_MemList;

#ifdef NEW_LOCKS
typedef enum
{
	_RWWriteRequest = 0x01,
	_RWWriter		= 0x02
}_RWFlag;

typedef struct
{
	int				blocked;
	XplSemaphore	semaphore;
}Blocked;

typedef struct _RWLock
{
	XplLock			lock;
	_RWFlag			flags;
	int				readCount;
	Blocked			readers;
	Blocked			writers;
}_RWLock;
#endif	//NEW_LOCKS

typedef struct _MemSlab
{
	struct _MemSlab		*next;
	size_t				size;
	int					nodes;
	int					total;
	_MemList			freeList;
	_MemList			allocList;
	char				data[];
}_MemSlab;

typedef struct _MemString
{
	struct _MemString	*next;
	const char			*original;
	char				data[MAX_MEM_STRING];
}_MemString;

typedef struct
{
	XplLock				lock;
	_MemString			*list;
}_MemStringList;

typedef struct _MemPool
{
#ifdef NEW_LOCKS
	_RWLock				lock;
#else
	XplLock				lock;
#endif
	unsigned long		flags;
	int					slabs;	// contains number of huge allocs on the huge alloc pool
	int					threshold;
	size_t				size;
	int					pagesPerSlab;
	int					nodeSize;
	int					nodesPerSlab;
	int					advice;
	_MemSlab			*slabList;
	PoolEntryCB			prepareCB;
	PoolEntryCB			freeCB;
	PoolEntryCB			destroyCB;
	void				*clientData;
	const char			*identity;
	_MemStringList		stringHash[MEM_STRING_TABLE_COUNT];
}_MemPool;

typedef struct _MemChunk
{
	_MemSlab				*slab;
	_NodeType				type;
	_MemNode				*nodeList;
}_MemChunk;

typedef struct
{
	_MemPool			chunkPool;
	_MemPool			pool;
	XplBool				locked;
	XplBool				released;
}_PrivatePool;

#define MEM_REPORT		0x00000001
#define MEM_DUMP		0x00000002

typedef struct _MemWork
{
	struct _MemWork		*next;
	unsigned long		type;		// also: counter for collate
	void				*dataP;		// also: file for collate
	unsigned long		dataL;		// also: line for collate
}_MemWork;

typedef enum
{
	_MemLoaded,
	_MemInitialized
}_MemState;

typedef struct
{
	char				*consumer;
	int					useCount;
	unsigned long		pageSize;
	_MemState			state;
	XplThreadGroup		threadGroup;
	XplThread_			workThread;
	struct
	{
		XplLock			lock;
		_MemWork		*list;
		_MemWork		**tail;
	}work;
	union {
		_MemPool		pool[NUMMEMPOOLGENERAL+NUMMEMPOOLSIZED];
		struct {
			_MemPool	general[NUMMEMPOOLGENERAL];
			_MemPool	sized[NUMMEMPOOLSIZED];
		};
	};
	_MemPool			*workPool;
	_MemPool			*stringPool;
	_MemPool			*privatePool;
	_MemPool			*hugePool;

	_MemStringList		consumers; // A consumer list taken from 'string' pool
}_MemGlobals;

_MemGlobals _Mem;

// thread library shutdown
extern int XplThreadShutdown( void );

#endif

