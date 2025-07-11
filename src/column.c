
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
array_t *column_array = NULL; // array of column_t sorted by property ID.
column_t *column_order_start = NULL; // column order
column_t *column_order_last = NULL;

// compare two columns, sorting by property ID.
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

// returns the column from the specified property id.
// returns NULL if not found.
column_t *column_find(DWORD property_id)
{
	return array_find(column_array,column_compare,(const void *)(uintptr_t)property_id);
}

// add the specified column to the end of the order list.
// the column MUST NOT be in the order list.
void column_insert_order_at_end(column_t *column)
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
	
// add the specified column to the start of the order list.
// the column MUST NOT be in the order list.
void column_insert_order_at_start(column_t *column)
{
	column->order_prev = NULL;
	column->order_next = column_order_start;
	column_order_start = column;
	
	if (column->order_next)
	{
		column->order_next->order_prev = column;
	}
	else
	{
		column_order_last = column;
	}
}

// remove the specified column from the order list.
// the column MUTS BE in the order list.
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

// add a new column.
// Does nothing if the column was already added.
// returns a pointer to the added column or existing column.
// The new column is added to the end of the order list.
// or, the existing column is moved to the end of the order list.
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
		
		array_insert(column_array,insert_index,column);
	}
	
	column_insert_order_at_end(column);
	
	return column;
}

// removes the specified column if it exists.
void column_remove(DWORD property_id)
{
	column_t *column;
	
	column = array_remove(column_array,column_compare,(const void *)(uintptr_t)property_id);
	if (column)
	{
		column_remove_order(column);
	}
}

// reset columns.
void column_clear_all(void)
{
	array_empty(column_array);
	pool_empty(column_pool);

	column_order_start = NULL;
	column_order_last = NULL;
}

// insert_after_column can be NULL to insert at the start.
// if insert_after_column is non-NULL it MUST be in the column order list.
// column MUST not be in the column order list.
void column_insert_after(column_t *column,column_t *insert_after_column)
{
	if (insert_after_column)
	{
		column->order_next = insert_after_column->order_next;
		column->order_prev = insert_after_column;
		insert_after_column->order_next = column;

		if (column->order_next)
		{
			column->order_next->order_prev = column;
		}
		else
		{
			column_order_last = column;
		}
	}
	else
	{
		column_insert_order_at_start(column);
	}
}