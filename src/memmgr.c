#include <memmgr-config.h>

#ifdef MEMMGR_TYPE_SLABBER
# include "slab.c"
#elif defined(MEMMGR_TYPE_NONE)
# include "no-memmgr.c"
#elif defined(MEMMGR_TYPE_GUARD)
# include "guard-memmgr.c"
#else
# error "No memory manager selected"
#endif
