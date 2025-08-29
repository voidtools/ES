
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

#define PROPERTY_UNKNOWN_CANONICAL_NAME(unknown)	((ES_UTF8 *)(((property_unknown_t *)(unknown)) + 1))

// an unknown property.
typedef struct property_unknown_s
{
	SIZE_T canonical_name_len;
	DWORD property_id;
	int is_right_aligned;
	int is_sort_descending;
	
	// width in logical pixels.
	int default_width;
	
	// NULL terminated canonical name follows.
	// ES_UTF8 canonical_name[canonical_name_len + 1];
	
}property_unknown_t;

const property_unknown_t *property_unknown_find(DWORD property_id);
const property_unknown_t *property_unknown_get(DWORD property_id);
void property_unknown_clear_all(void);

extern pool_t *property_unknown_pool; // pool of property_unknown_t
extern array_t *property_unknown_array; // array of property_unknown_t
