
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

#define ARRAY_STACK_SIZE				256

// a simple array
typedef struct array_s
{
	// the base indexes.
	// sorted by compare_proc
	void **indexes;
	
	// the number of items in indexes.
	SIZE_T count;
	
	// the number of allocated items in indexes.
	// this will double in size each time the array grows.
	SIZE_T allocated;

	// ensure stack_buf is 16byte aligned.
	SIZE_T padding1;
	
	// small stack space for initial buffer to avoid allocation.
	BYTE stack_buf[ARRAY_STACK_SIZE];
	
}array_t;

void array_init(array_t *a);
void array_kill(array_t *a);
void array_empty(array_t *a);
void *array_find_or_get_insertion_index(const array_t *a,int (*compare_proc)(const void *a,const void *b),const void *compare_data,SIZE_T *out_insertion_index);
void *array_find(const array_t *a,int (*compare_proc)(const void *a,const void *b),const void *compare_data);
void array_insert(array_t *a,SIZE_T insertion_index,void *item);
void *array_remove(array_t *a,int (*compare_proc)(const void *a,const void *b),const void *compare_data);
