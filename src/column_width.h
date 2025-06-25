
typedef struct es_column_width_s
{
	// EVERYTHING3_PROPERTY_ID_*
	DWORD property_id;
	
	// the SetConsoleTextAttribute color
	int width;
	
}column_width_t;

int column_width_compare(const column_width_t *a,const void *property_id);
void column_width_set(DWORD property_id,int width);
column_width_t *column_width_find(DWORD property_id);
column_width_t *column_width_remove(DWORD property_id);
int column_width_get(DWORD property_id);
void column_width_clear_all(void);

extern pool_t *column_width_pool; // pool of column_width_t
extern array_t *column_width_array; // array of column_width_t
