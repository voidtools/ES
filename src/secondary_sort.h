
typedef struct secondary_sort_s
{
	struct secondary_sort_s *next;
	
	// EVERYTHING3_PROPERTY_ID_*
	DWORD property_id;

	// <0 == descending
	// 0 == use default
	// >0 == ascending.
	int ascending;
	
}secondary_sort_t;

secondary_sort_t *secondary_sort_add(DWORD property_id,int ascending);
void secondary_sort_clear_all(void);

extern pool_t *secondary_sort_pool; // pool of secondary_sort_t
extern array_t *secondary_sort_array; // array of secondary_sort_t sorted by property id.
extern secondary_sort_t *secondary_sort_start; // sort order
extern secondary_sort_t *secondary_sort_last;
