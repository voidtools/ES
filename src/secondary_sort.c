
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
secondary_sort_t *secondary_sort_start = NULL;
secondary_sort_t *secondary_sort_last = NULL;
SIZE_T secondary_sort_count = 0;

void secondary_sort_add(DWORD property_id,int ascending)
{
	secondary_sort_t *sort;

	sort = pool_alloc(secondary_sort_pool,sizeof(secondary_sort_t));
		
	sort->property_id = property_id;
	sort->ascending = ascending;

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
	secondary_sort_count++;
}

void secondary_sort_clear_all(void)
{
	pool_empty(secondary_sort_pool);

	secondary_sort_start = NULL;
	secondary_sort_last = NULL;
	secondary_sort_count = 0;
}

