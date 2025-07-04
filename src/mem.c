
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
// memory management.

#include "es.h"

// allocate some memory.
// throws a fatal error if there is not enough memory available.
// size should be <= 65536.
// use a try alloc for larger sizes.
// returns a pointer to the newly allocated memory.
// returned memory will be garbage and will need initializing.
// call mem_free to return the memory to the system.
// SIZE_MAX is not a valid size and will throw an error.
// use safe_size_* functions to perform safe size arithmetic.
void *mem_alloc(SIZE_T size)
{
	void *p;
	
	if (size == SIZE_MAX)
	{
		es_fatal(ES_ERROR_OUT_OF_MEMORY);
	}

	p = HeapAlloc(GetProcessHeap(),0,size);
	if (!p)
	{
		es_fatal(ES_ERROR_OUT_OF_MEMORY);
	}
	
	return p;
}

// same as mem_alloc, except can return NULL if there is not enought memory available.
void *mem_try_alloc(SIZE_T size)
{
	// SIZE_MAX is invalid.
	if (size == SIZE_MAX)
	{
		return NULL;
	}

	return HeapAlloc(GetProcessHeap(),0,size);
}

// return allocated memory to the system.
// ptr must be a return value from mem_alloc.
void mem_free(void *ptr)
{
	HeapFree(GetProcessHeap(),0,ptr);
}
