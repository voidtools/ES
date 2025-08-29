
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

typedef struct secondary_sort_s
{
	// next secondary sort.
	struct secondary_sort_s *next;
	
	// EVERYTHING3_PROPERTY_ID_*
	DWORD property_id;

	// <0 == descending
	// 0 == use default
	// >0 == ascending.
	int ascending;
	
}secondary_sort_t;

secondary_sort_t *secondary_sort_add(DWORD property_id,int ascending);
void secondary_sort_clear_all(void);

extern pool_t *secondary_sort_pool; // pool of secondary_sort_t
extern array_t *secondary_sort_array; // array of secondary_sort_t sorted by property id.
extern secondary_sort_t *secondary_sort_start; // sort order
extern secondary_sort_t *secondary_sort_last;
