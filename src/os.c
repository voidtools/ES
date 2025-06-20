
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
// os dependant calls

#include "es.h"

// src or dst can be unaligned.
// returns the dst + size.
void *os_copy_memory(void *dst,const void *src,SIZE_T size)
{
	BYTE *d;
	
	d = dst;
	
	CopyMemory(d,src,size);
	
	return d + size;
}

// src or dst can overlap
// returns the dst + size.
void *os_move_memory(void *dst,const void *src,SIZE_T size)
{
	BYTE *d;
	
	d = dst;
	
	MoveMemory(d,src,size);
	
	return d + size;
}

// src or dst can be unaligned.
// returns the dst + size.
void *os_zero_memory(void *dst,SIZE_T size)
{
	BYTE *d;
	
	d = dst;
	
	ZeroMemory(d,size);
	
	return d + size;
}

int os_replace_file(const wchar_t *old_name,const wchar_t *new_name)
{
	int ret;
	
	ret = 0;
	
	// try MoveFileEx
	if (MoveFileExW(old_name,new_name,MOVEFILE_COPY_ALLOWED | MOVEFILE_REPLACE_EXISTING))
	{
		ret = 1;
	}
	
	return ret;
}

HANDLE os_create_file(const wchar_t *filename)
{
	return CreateFile(filename,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,0);
}

HANDLE os_open_file(const wchar_t *filename)
{
	return CreateFile(filename,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_FLAG_SEQUENTIAL_SCAN,0);
}

void os_write_file_utf8_string_n(HANDLE file_handle,const ES_UTF8 *s,SIZE_T slength_in_bytes)
{
	DWORD num_written;
	const ES_UTF8 *p;
	SIZE_T run;
	
	p = s;
	run = slength_in_bytes;
	
	while(run)
	{
		DWORD write_size;
		
		if (run < 65536)
		{
			write_size = (DWORD)run;
		}
		else
		{
			write_size = 65536;
		}
		
		if (!WriteFile(file_handle,s,write_size,&num_written,NULL))
		{
			break;
		}
		
		if (num_written != write_size)
		{
			break;
		}
		
		p += write_size;
		run -= write_size;
	}
}

void os_write_file_utf8_string(HANDLE file_handle,const ES_UTF8 *s)
{
	os_write_file_utf8_string_n(file_handle,s,utf8_string_get_length_in_bytes(s));
}

int os_get_module_file_name(HMODULE hmod,wchar_buf_t *wcbuf)
{
	for(;;)
	{
		DWORD size_in_wchars;
		DWORD gmfn_ret;
		SIZE_T new_size_in_wchars;
		
		if (wcbuf->size_in_wchars <= ES_DWORD_MAX)
		{
			size_in_wchars = (DWORD)wcbuf->size_in_wchars;
		}
		else
		{
			size_in_wchars = ES_DWORD_MAX;
		}
		
		gmfn_ret = GetModuleFileName(hmod,wcbuf->buf,size_in_wchars);
		if (!gmfn_ret)
		{
			break;
		}
		
		if (gmfn_ret < wcbuf->size_in_wchars)
		{
			wcbuf->length_in_wchars = gmfn_ret;
			
			return 1;
		}
		
		// already have the max size.
		if (size_in_wchars == ES_DWORD_MAX)
		{
			break;
		}
		
		new_size_in_wchars = wcbuf->size_in_wchars * 2;
		if (new_size_in_wchars < WCHAR_BUF_CAT_MIN_ALLOC_SIZE)
		{
			new_size_in_wchars = WCHAR_BUF_CAT_MIN_ALLOC_SIZE;
		}
		
		wchar_buf_grow_size(wcbuf,new_size_in_wchars);
	}
	
	return 0;
}

void os_get_full_path_name(const wchar_t *relative_path,wchar_buf_t *wcbuf)
{
	wchar_t *namepart;

	for(;;)
	{
		DWORD size;
		DWORD len;
		SIZE_T new_size_in_wchars;
		
		if (wcbuf->size_in_wchars <= ES_DWORD_MAX)
		{
			size = (DWORD)wcbuf->size_in_wchars;
		}
		else
		{
			size = ES_DWORD_MAX;
		}
		
		len = GetFullPathName(relative_path,size,wcbuf->buf,&namepart);
		
		if (!len)
		{
			break;
		}
		
		if (len < wcbuf->size_in_wchars)
		{
			wcbuf->length_in_wchars = len;
			
			return;
		}
		
		// already have the max size.
		if (size == ES_DWORD_MAX)
		{
			break;
		}
			
		new_size_in_wchars = wcbuf->size_in_wchars * 2;
		if (new_size_in_wchars < WCHAR_BUF_CAT_MIN_ALLOC_SIZE)
		{
			new_size_in_wchars = WCHAR_BUF_CAT_MIN_ALLOC_SIZE;
		}
		
		wchar_buf_grow_size(wcbuf,new_size_in_wchars);
	}
	
	// just use relative path..
	wchar_buf_copy_wchar_string(wcbuf,relative_path);
}
