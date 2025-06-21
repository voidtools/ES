
typedef struct es_column_color_s
{
	// EVERYTHING3_PROPERTY_ID_*
	DWORD property_id;
	
	// the SetConsoleTextAttribute color
	WORD color;
	
}column_color_t;

int column_color_compare(const column_color_t *a,const void *property_id);
void column_color_set(DWORD property_id,WORD color);
column_color_t *column_color_find(DWORD property_id);

extern pool_t *column_color_pool; // pool of column_color_t
extern array_t *column_color_array; // array of column_color_t
