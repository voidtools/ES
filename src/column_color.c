
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
// column colors

#include "es.h"

// these must be set in main()
pool_t *column_color_pool = NULL; // pool of column_color_t
array_t *column_color_array = NULL; // array of column_color_t

int column_color_compare(const column_color_t *a,const void *property_id)
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

void column_color_set(DWORD property_id,WORD color)
{
	column_color_t *column_color;
	SIZE_T insert_index;
	
	column_color = array_find_or_get_insertion_index(column_color_array,column_color_compare,(const void *)(uintptr_t)property_id,&insert_index);
	if (column_color)
	{
		column_color->color = color;
	}
	else
	{
		column_color = pool_alloc(column_color_pool,sizeof(column_color_t));
		
		column_color->property_id = property_id;
		column_color->color = color;

		array_insert(column_color_array,insert_index,column_color);
	}
}

column_color_t *column_color_find(DWORD property_id)
{
	return array_find(column_color_array,column_color_compare,(const void *)(uintptr_t)property_id);
}

