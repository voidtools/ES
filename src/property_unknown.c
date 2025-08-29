
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
// Unknown properties
// property system properties.

#include "es.h"

static int _property_unknown_compare(const property_unknown_t *a,const void *property_id);

// these must be set in main()
pool_t *property_unknown_pool = NULL; // pool of property_unknown_t
array_t *property_unknown_array = NULL; // array of property_unknown_t sorted by property ID.

// compare two property_unknown properties by property ID.
static int _property_unknown_compare(const property_unknown_t *a,const void *property_id)
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

const property_unknown_t *property_unknown_find(DWORD property_id)
{
	return array_find(property_unknown_array,_property_unknown_compare,(const void *)(uintptr_t)property_id);
}

// property_id >= EVERYTHING3_PROPERTY_ID_BUILTIN_COUNT
const property_unknown_t *property_unknown_get(DWORD property_id)
{
	property_unknown_t *property_unknown;
	SIZE_T insert_position;
	
	property_unknown = array_find_or_get_insertion_index(property_unknown_array,_property_unknown_compare,(const void *)(uintptr_t)property_id,&insert_position);
	if (!property_unknown)
	{
		utf8_buf_t property_canonical_name_cbuf;
		
		utf8_buf_init(&property_canonical_name_cbuf);
		
		if (ipc3_get_property_canonical_name(property_id,&property_canonical_name_cbuf))
		{
			SIZE_T unknown_size;
				
			unknown_size = sizeof(property_unknown_t);
			unknown_size = safe_size_add(unknown_size,safe_size_add_one(property_canonical_name_cbuf.length_in_bytes));
			
			property_unknown = pool_alloc(property_unknown_pool,unknown_size);

			property_unknown->canonical_name_len = property_canonical_name_cbuf.length_in_bytes;
			property_unknown->property_id = property_id;
			property_unknown->is_right_aligned = ipc3_is_property_right_aligned(property_id);
			property_unknown->is_sort_descending = ipc3_is_property_sort_descending(property_id);
			property_unknown->default_width = ipc3_get_property_default_width(property_id);
			
			utf8_string_copy_utf8_string_n(PROPERTY_UNKNOWN_CANONICAL_NAME(property_unknown),property_canonical_name_cbuf.buf,property_canonical_name_cbuf.length_in_bytes);
			
			array_insert(property_unknown_array,insert_position,property_unknown);
		}

		utf8_buf_kill(&property_canonical_name_cbuf);
	}
	
	return property_unknown;
}

void property_unknown_clear_all(void)
{
	array_empty(property_unknown_array);
	pool_empty(property_unknown_pool);
}
