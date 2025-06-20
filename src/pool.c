
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
// pool bump allocator

#include "es.h"

void pool_init(pool_t *buf)
{
	buf->p = buf->stack;
	buf->avail = POOL_STACK_SIZE;
	buf->chunk_start = NULL;
	buf->cur_alloc_size = POOL_STACK_SIZE;
}

void pool_empty(pool_t *buf)
{
	pool_kill(buf);
	pool_init(buf);
}
	
void pool_kill(pool_t *buf)
{
	pool_chunk_t *chunk;
	
	chunk = buf->chunk_start;
	
	while(chunk)
	{
		pool_chunk_t *next_chunk;
		
		next_chunk = chunk->next;

		mem_free(chunk);

		chunk = next_chunk;
	}
}

void *pool_alloc(pool_t *buf,SIZE_T size)
{
	void *p;
	
	if (size > buf->avail)
	{
		SIZE_T min_alloc_size;
		SIZE_T alloc_size;
		pool_chunk_t *chunk;
		
		alloc_size = buf->cur_alloc_size * 2;
		
		if (alloc_size > POOL_MAX_CHUNK_SIZE)
		{
			alloc_size = POOL_MAX_CHUNK_SIZE;
		}
		
		buf->cur_alloc_size = alloc_size;
		
		min_alloc_size = safe_size_add(sizeof(pool_chunk_t),size);
		
		if (alloc_size < min_alloc_size)
		{
			alloc_size = min_alloc_size;
		}
		
		chunk = mem_alloc(alloc_size);
		
		chunk->next = buf->chunk_start;
		buf->chunk_start = chunk;
		
		buf->p = POOL_CHUNK_DATA(chunk);
		buf->avail = alloc_size - sizeof(pool_chunk_t);
	}
	
	p = buf->p;

	buf->p += size;
	buf->avail -= size;

	return p;
}
