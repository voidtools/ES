
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
// simple arrays

#include "es.h"

// initialize an array.
// The initialized array will be empty.
void array_init(array_t *a)
{
	a->indexes = (void **)a->stack_buf;
	a->count = 0;
	a->allocated = ARRAY_STACK_SIZE / sizeof(void *);
}

// kill an initialized array
// Any allocated memory is returned to the system.
void array_kill(array_t *a)
{
	if (a->indexes != (void **)a->stack_buf)
	{
		mem_free(a->indexes);
	}
}

void array_empty(array_t *a)
{
	array_kill(a);
	array_init(a);
}

// returns a pointer to the found item.
// Otherwise, returns NULL and sets out_insertion_index.
// call array_insert with this index to add a new item.
void *array_find_or_get_insertion_index(const array_t *a,int (*compare)(const void *a,const void *b),const void *compare_data,SIZE_T *out_insertion_index)
{
	SIZE_T blo;
	SIZE_T bhi;
	SIZE_T best_insertion_index;
	void **array_base;
	
	// search for name insertion point.
	blo = 0;
	bhi = a->count;
	best_insertion_index = 0;
	array_base = a->indexes;

	while(blo < bhi)
	{
		SIZE_T bpos;
		int i;
		
		bpos = blo + ((bhi - blo) / 2);

		i = compare(array_base[bpos],compare_data);
		
		if (i > 0)
		{
			bhi = bpos;
		}
		else
		if (!i)
		{
			// already in the list!
			return array_base[bpos];
		}
		else
		{
			best_insertion_index = bpos + 1;
			blo = bpos + 1;
		}
	}
	
	*out_insertion_index = best_insertion_index;
	
	return NULL;
}

// returns a pointer to the found item.
// returns NULL if not found.
void *array_find(const array_t *a,int (*compare)(const void *a,const void *b),const void *compare_data)
{
	SIZE_T blo;
	SIZE_T bhi;
	void **array_base;
	
	// search for insertion point.
	blo = 0;
	bhi = a->count;
	array_base = a->indexes;

	while(blo < bhi)
	{
		SIZE_T bpos;
		int i;
		
		bpos = blo + ((bhi - blo) / 2);

		i = compare(array_base[bpos],compare_data);
		
		if (i > 0)
		{
			bhi = bpos;
		}
		else
		if (!i)
		{
			// already in the list!
			return array_base[bpos];
		}
		else
		{
			blo = bpos + 1;
		}
	}
	
	return NULL;
}

// insert the specified item at the specified index.
// call array_find_or_get_insertion_index to get the insertion position.
// insertion_index can be SIZE_MAX to add to the end.
void array_insert(array_t *a,SIZE_T insertion_index,void *item)
{
	SIZE_T new_count;
	SIZE_T validated_insertion_index;
	
	validated_insertion_index = insertion_index;
	if (validated_insertion_index > a->count)
	{
		validated_insertion_index = a->count;
	}
	
	new_count = safe_size_add(a->count,1);
	
	if (new_count <= a->allocated)
	{
		// make a hole in the array.
		os_move_memory(a->indexes + validated_insertion_index + 1,a->indexes + validated_insertion_index,(a->count - validated_insertion_index) * sizeof(void *));
		
		a->indexes[validated_insertion_index] = item;
	}
	else
	{
		SIZE_T new_allocated;
		void **new_indexes;
		
		new_allocated = safe_size_add(a->allocated,a->allocated);
		
		new_indexes = mem_alloc(safe_size_mul_sizeof_pointer(new_allocated));
		
		os_copy_memory(new_indexes,a->indexes,validated_insertion_index * sizeof(void *));
		
		new_indexes[validated_insertion_index] = item;
	
		os_copy_memory(new_indexes + validated_insertion_index + 1,a->indexes + validated_insertion_index,(a->count - validated_insertion_index) * sizeof(void *));
		
		// free old
		if (a->indexes != (void **)a->stack_buf)
		{
			mem_free(a->indexes);
		}
		
		a->indexes = new_indexes;
		a->allocated = new_allocated;
	}
	
	a->count++;
}

// returns a pointer to the item if found and removes it from the array.
// caller should free the returned item.
void *array_remove(array_t *a,int (*compare)(const void *a,const void *b),const void *compare_data)
{
	SIZE_T blo;
	SIZE_T bhi;
	void **array_base;
	
	// search for name insertion point.
	blo = 0;
	bhi = a->count;
	array_base = a->indexes;

	while(blo < bhi)
	{
		SIZE_T bpos;
		int i;
		
		bpos = blo + ((bhi - blo) / 2);

		i = compare(array_base[bpos],compare_data);
		
		if (i > 0)
		{
			bhi = bpos;
		}
		else
		if (!i)
		{
			void *item;
			
			item = a->indexes[bpos];
			
			os_move_memory(array_base + bpos,array_base + bpos + 1,(a->count - (bpos + 1)) * sizeof(void *));
			a->count--;
		
			return item;
		}
		else
		{
			blo = bpos + 1;
		}
	}
	
	return NULL;
}
