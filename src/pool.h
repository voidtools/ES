
#define POOL_STACK_SIZE				256
#define POOL_MAX_CHUNK_SIZE			65536

#define POOL_CHUNK_DATA(chunk)		((BYTE *)(((pool_chunk_t *)(chunk)) + 1))

// a chunk of memory.
typedef struct pool_chunk_s
{
	union
	{
		// the next pool in the list.
		struct pool_chunk_s *next;
		
		// ensure 8-byte alignment.
		ES_UINT64 alignment;
	};
	
	// data follows.
	// BYTE data[];
	
}pool_chunk_t;

// a simple pool bump allocator
typedef struct pool_s
{
	// current position
	BYTE *p;
	
	// bytes available at position
	SIZE_T avail;
	
	// chunk list.
	pool_chunk_t *chunk_start;
	
	// current chunk size.
	SIZE_T cur_alloc_size;
	
	// some stack space.
	// make sure this is 16-byte aligned.
	BYTE stack[POOL_STACK_SIZE];
	
}pool_t;

void pool_init(pool_t *buf);
void pool_empty(pool_t *buf);
void pool_kill(pool_t *buf);
void *pool_alloc(pool_t *buf,SIZE_T size);
