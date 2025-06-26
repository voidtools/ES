
#define COLUMN_FLAG_HIGHLIGHT	0x00000001

typedef struct column_s
{
	// order list.
	struct column_s *order_next;
	struct column_s *order_prev;
	
	// EVERYTHING3_PROPERTY_ID_*
	DWORD property_id;
	
	// zero or more COLUMN_FLAG_*
	DWORD flags;
	
}column_t;

int column_compare(const column_t *a,const void *property_id);
column_t *column_find(DWORD property_id);
void column_insert_order_at_end(column_t *column);
void column_insert_order_at_start(column_t *column);
void column_remove_order(column_t *column);
column_t *column_add(DWORD property_id);
void column_remove(DWORD property_id);
void column_clear_all(void);

extern pool_t *column_pool; // pool of column_t
extern array_t *column_array; // array of column_t
extern column_t *column_order_start; // column order
extern column_t *column_order_last;
