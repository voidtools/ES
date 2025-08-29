
//
// Copyright (C) 2025 voidtools / David Carpenter
// 
// Permission is hereby granted, free of charge, 
// to any person obtaining a copy of this software 
// and associated documentation files (the "Software"), 
// to deal in the Software without restriction, 
// including without limitation the rights to use, 
// copy, modify, merge, publish, distribute, sublicense, 
// and/or sell copies of the Software, and to permit 
// persons to whom the Software is furnished to do so, 
// subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be 
// included in all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, 
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES 
// OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. 
// IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, 
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, 
// TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE 
// SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//
// column widths
// we can set column widths for columns that are not shown. -useful for loading widths from the es.ini

#include "es.h"

static int _column_width_compare(const column_width_t *a,const void *property_id);

// these must be set in main()
pool_t *column_width_pool = NULL; // pool of column_width_t
array_t *column_width_array = NULL; // array of column_width_t sorted by property ID.

// compare two column widths by property ID.
static int _column_width_compare(const column_width_t *a,const void *property_id)
{
	if (a->property_id < (DWORD)(uintptr_t)property_id)
	{
		return -1;
	}

	if (a->property_id > (DWORD)(uintptr_t)property_id)
	{
		return 1;
	}
	
	return 0;
}

// set a column width
// adds a new column widths or replaces the existing column width.
void column_width_set(DWORD property_id,int width)
{
	column_width_t *column_width;
	SIZE_T insert_index;
	int sane_width;
	
	sane_width = width;
	
	// sanity.
	if (sane_width < 0) 
	{
		sane_width = 0;
	}
	
	if (sane_width > 65535) 
	{
		sane_width = 65535;
	}
	
	column_width = array_find_or_get_insertion_index(column_width_array,_column_width_compare,(const void *)(uintptr_t)property_id,&insert_index);
	if (column_width)
	{
		column_width->width = sane_width;
	}
	else
	{
		column_width = pool_alloc(column_width_pool,sizeof(column_width_t));
		
		column_width->property_id = property_id;
		column_width->width = sane_width;

		array_insert(column_width_array,insert_index,column_width);
	}
}

// remove an column width (if it exists)
// returns a pointer to the removed column width.
// returns NULL if not found.
column_width_t *column_width_find(DWORD property_id)
{
	return array_find(column_width_array,_column_width_compare,(const void *)(uintptr_t)property_id);
}

// returns a pointer to the found column width.
// returns NULL if not found.
column_width_t *column_width_remove(DWORD property_id)
{
	return array_remove(column_width_array,_column_width_compare,(const void *)(uintptr_t)property_id);
}

// get the width of a column.
// returns any defined width.
// if no width is defined, returns the default width.
int column_width_get(DWORD property_id)
{
	column_width_t *column_width;
	
	column_width = column_width_find(property_id);

	if (column_width)
	{
		return column_width->width;
	}
	
	return property_get_default_width(property_id);
}

// reset column widths.
void column_width_clear_all(void)
{
	array_empty(column_width_array);
	pool_empty(column_width_pool);
}

