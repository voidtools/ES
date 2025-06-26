
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
// sorts

#include "es.h"

// pools must be set in main()
pool_t *secondary_sort_pool = NULL; // pool of secondary_sort_t
array_t *secondary_sort_array = NULL; // array of secondary_sort_t sorted by property id.
secondary_sort_t *secondary_sort_start = NULL;
secondary_sort_t *secondary_sort_last = NULL;

// compare two secondary sorts by property ID.
int secondary_sort_compare(const secondary_sort_t *a,const void *property_id)
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

// add a secondarry sort
// does nothing if the secondary sort already exists.
// returns the new secondary sort or the existing secondary sort.
secondary_sort_t *secondary_sort_add(DWORD property_id,int ascending)
{
	secondary_sort_t *sort;
	SIZE_T insert_index;
	
	sort = array_find_or_get_insertion_index(secondary_sort_array,secondary_sort_compare,(const void *)(uintptr_t)property_id,&insert_index);
	if (!sort)
	{
		// alloc
		sort = pool_alloc(secondary_sort_pool,sizeof(secondary_sort_t));
			
		// init
		sort->property_id = property_id;
		sort->ascending = ascending;
		
		// insert
		array_insert(secondary_sort_array,insert_index,sort);

		if (secondary_sort_start)
		{
			secondary_sort_last->next = sort;
		}
		else
		{
			secondary_sort_start = sort;
		}
		
		secondary_sort_last = sort;
		sort->next = NULL;
	}
	
	return sort;
}

void secondary_sort_clear_all(void)
{
	array_empty(secondary_sort_array);
	pool_empty(secondary_sort_pool);

	secondary_sort_start = NULL;
	secondary_sort_last = NULL;
}

