
#define ARRAY_STACK_SIZE				256

// a simple array
typedef struct array_s
{
	// the base indexes.
	void **indexes;
	
	// the number of items in indexes.
	SIZE_T count;
	
	// the number of allocated items in indexes.
	// this will double in size each time the array grows.
	SIZE_T allocated;

	// ensure stack_buf is 16byte aligned.
	SIZE_T padding1;
	
	// small stack space for initial buffer to avoid allocation.
	BYTE stack_buf[ARRAY_STACK_SIZE];
	
}array_t;

void array_init(array_t *a);
void array_kill(array_t *a);
void array_empty(array_t *a);
void *array_find_or_get_insertion_index(const array_t *a,int (*compare)(const void *a,const void *b),const void *compare_data,SIZE_T *out_insertion_index);
void *array_find(const array_t *a,int (*compare)(const void *a,const void *b),const void *compare_data);
void array_insert(array_t *a,SIZE_T insertion_index,void *item);
void *array_remove(array_t *a,int (*compare)(const void *a,const void *b),const void *compare_data);
