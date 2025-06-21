
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
// Columns

#include "es.h"

// these must be set in main()
pool_t *column_pool = NULL; // pool of column_t
array_t *column_array = NULL; // array of column_t
column_t *column_order_start = NULL; // column order
column_t *column_order_last = NULL;

int column_compare(const column_t *a,const void *property_id)
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

// returns SIZE_MAX if not found.
column_t *column_find(DWORD property_id)
{
	return array_find(column_array,column_compare,(const void *)(uintptr_t)property_id);
}

void column_insert_order(column_t *column)
{
	if (column_order_start)
	{
		column_order_last->order_next = column;
		column->order_prev = column_order_last;
	}
	else
	{
		column_order_start = column;
		column->order_prev = NULL;
	}
	
	column->order_next = NULL;
	column_order_last = column;
}
		
void column_remove_order(column_t *column)
{
	if (column_order_start == column)
	{
		column_order_start = column->order_next;
	}
	else
	{
		column->order_prev->order_next = column->order_next;
	}
	
	if (column_order_last == column)
	{
		column_order_last = column->order_prev;
	}
	else
	{
		column->order_next->order_prev = column->order_prev;
	}
}
		
column_t *column_add(DWORD property_id)
{
	column_t *column;
	SIZE_T insert_index;
	
	column = array_find_or_get_insertion_index(column_array,column_compare,(const void *)(uintptr_t)property_id,&insert_index);
	if (column)
	{
		column_remove_order(column);
	}
	else
	{
		column = pool_alloc(column_pool,sizeof(column_t));
		
		column->property_id = property_id;
		column->flags = 0;
		
		array_insert(column_array,insert_index,column);
	}
	
	column_insert_order(column);
	
	return column;
}

void column_remove(DWORD property_id)
{
	column_t *column;
	
	column = array_remove(column_array,column_compare,(const void *)(uintptr_t)property_id);
	if (column)
	{
		column_remove_order(column);
	}
}

void column_clear_all(void)
{
	array_empty(column_array);
	pool_empty(column_pool);

	column_order_start = NULL;
	column_order_last = NULL;
}

