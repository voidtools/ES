
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
#include <shlobj.h> // SHGetSpecialFolderLocation

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

// replaces the old file with a new file.
// the operation should be atomic.
// However, this is no longer true as the NTFS rename can get flushed to disk before the actual data.
// for ES.ini this will have to do.
int os_replace_file(const wchar_t *old_name,const wchar_t *new_name)
{
	BOOL ret;
	
	ret = FALSE;
	
	// try MoveFileEx
	if (MoveFileExW(old_name,new_name,MOVEFILE_COPY_ALLOWED | MOVEFILE_REPLACE_EXISTING))
	{
		ret = TRUE;
	}
	else
	{
		DWORD last_error;
		
		last_error = GetLastError();
		
		debug_error_printf("MoveFileEx failed %u\n",last_error);
		
		// if it failed, check to see if it was implemented.
		if (last_error == ERROR_CALL_NOT_IMPLEMENTED) 
		{
			// delete old file
			DeleteFile(new_name);
				
			if (MoveFileW(old_name,new_name))
			{
				// success
				ret = TRUE;
			}
			else
			{
				debug_error_printf("MoveFile failed %u\n",GetLastError());
			}
		}
	}
	
	return ret;
}

// create a new empty file.
// returns INVALID_HANDLE_VALUE if unable to create the new file.
HANDLE os_create_file(const wchar_t *filename)
{
	HANDLE file_handle;

	file_handle = CreateFile(filename,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,0);
	
	if (file_handle == INVALID_HANDLE_VALUE)
	{
		debug_error_printf("CreateFile error %u: failed to create %S\n",GetLastError(),filename);
	}
	
	return file_handle;
}

// open an existing file.
// returns INVALID_HANDLE_VALUE if not found.
HANDLE os_open_file(const wchar_t *filename)
{
	HANDLE file_handle;

	file_handle = CreateFile(filename,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_FLAG_SEQUENTIAL_SCAN,0);
	
	if (file_handle == INVALID_HANDLE_VALUE)
	{
		debug_error_printf("CreateFile error %u: failed to open %S\n",GetLastError(),filename);
	}
	
	return file_handle;
}

// write out a UTF-8 string with the specified length to a file.
BOOL os_write_file_utf8_string_n(HANDLE file_handle,const ES_UTF8 *s,SIZE_T slength_in_bytes)
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
			return FALSE;
		}
		
		if (num_written != write_size)
		{
			return FALSE;
		}
		
		p += write_size;
		run -= write_size;
	}
	
	return TRUE;
}

// write out a UTF-8 string to a file.
BOOL os_write_file_utf8_string(HANDLE file_handle,const ES_UTF8 *s)
{
	return os_write_file_utf8_string_n(file_handle,s,utf8_string_get_length_in_bytes(s));
}

// get the module filename, eg: "C:\\Windows\\System32\\ES.exe"
// stores the filename in out_wcbuf.
BOOL os_get_module_file_name(HMODULE hmod,wchar_buf_t *out_wcbuf)
{
	for(;;)
	{
		DWORD size_in_wchars;
		DWORD gmfn_ret;
		SIZE_T new_size_in_wchars;
		
		if (out_wcbuf->size_in_wchars <= ES_DWORD_MAX)
		{
			size_in_wchars = (DWORD)out_wcbuf->size_in_wchars;
		}
		else
		{
			size_in_wchars = ES_DWORD_MAX;
		}
		
		gmfn_ret = GetModuleFileName(hmod,out_wcbuf->buf,size_in_wchars);
		if (!gmfn_ret)
		{
			break;
		}
		
		if (gmfn_ret < size_in_wchars)
		{
			out_wcbuf->length_in_wchars = gmfn_ret;
			
			return TRUE;
		}
		
		// already have the max size.
		if (size_in_wchars == ES_DWORD_MAX)
		{
			break;
		}
		
		new_size_in_wchars = safe_size_mul_2(out_wcbuf->size_in_wchars);
		if (new_size_in_wchars < WCHAR_BUF_CAT_MIN_ALLOC_SIZE)
		{
			new_size_in_wchars = WCHAR_BUF_CAT_MIN_ALLOC_SIZE;
		}
		
		wchar_buf_grow_size(out_wcbuf,new_size_in_wchars);
	}
	
	return FALSE;
}

// expand environment variables.
// stores the expanded variables in out_wcbuf.
void os_expand_environment_variables(const wchar_t *s,wchar_buf_t *out_wcbuf)
{
	for(;;)
	{
		DWORD size;
		DWORD len;
		SIZE_T new_size_in_wchars;
		
		if (out_wcbuf->size_in_wchars <= ES_DWORD_MAX)
		{
			size = (DWORD)out_wcbuf->size_in_wchars;
		}
		else
		{
			size = ES_DWORD_MAX;
		}
		
		len = ExpandEnvironmentStrings(s,out_wcbuf->buf,size);
		
		if (!len)
		{
			break;
		}
		
		if (len <= size)
		{
			out_wcbuf->length_in_wchars = len - 1;
			
			return;
		}
		
		// already have the max size.
		if (size == ES_DWORD_MAX)
		{
			break;
		}
			
		new_size_in_wchars = safe_size_mul_2(out_wcbuf->size_in_wchars);
		if (new_size_in_wchars < WCHAR_BUF_CAT_MIN_ALLOC_SIZE)
		{
			new_size_in_wchars = WCHAR_BUF_CAT_MIN_ALLOC_SIZE;
		}
		
		wchar_buf_grow_size(out_wcbuf,new_size_in_wchars);
	}
	
	// just use un-expanded path..
	wchar_buf_copy_wchar_string(out_wcbuf,s);
}

// get the full path from the specified path
// expands relative paths to an absolute path.
void os_get_full_path_name(const wchar_t *relative_path,wchar_buf_t *out_wcbuf)
{
	for(;;)
	{
		DWORD size;
		DWORD len;
		SIZE_T new_size_in_wchars;
		wchar_t *namepart;
		
		if (out_wcbuf->size_in_wchars <= ES_DWORD_MAX)
		{
			size = (DWORD)out_wcbuf->size_in_wchars;
		}
		else
		{
			size = ES_DWORD_MAX;
		}
		
		len = GetFullPathName(relative_path,size,out_wcbuf->buf,&namepart);
		
		if (!len)
		{
			break;
		}
		
		if (len < size)
		{
			out_wcbuf->length_in_wchars = len;
			
			return;
		}
		
		// already have the max size.
		if (size == ES_DWORD_MAX)
		{
			break;
		}
			
		new_size_in_wchars = safe_size_mul_2(out_wcbuf->size_in_wchars);
		if (new_size_in_wchars < WCHAR_BUF_CAT_MIN_ALLOC_SIZE)
		{
			new_size_in_wchars = WCHAR_BUF_CAT_MIN_ALLOC_SIZE;
		}
		
		wchar_buf_grow_size(out_wcbuf,new_size_in_wchars);
	}
	
	// just use relative path..
	wchar_buf_copy_wchar_string(out_wcbuf,relative_path);
}

// get the expanded full path from the specified path
// expands relative paths to an absolute path.
// expands environment variables.
void os_get_expanded_full_path_name(const wchar_t *relative_path,wchar_buf_t *out_wcbuf)
{
	wchar_buf_t expanded_wcbuf;

	wchar_buf_init(&expanded_wcbuf);

	os_expand_environment_variables(relative_path,&expanded_wcbuf);
	
	os_get_full_path_name(expanded_wcbuf.buf,out_wcbuf);
	
	wchar_buf_kill(&expanded_wcbuf);
}

// merge left and right sorted arrays into one.
static void _os_sort_merge(void **dst,void **left,SIZE_T left_count,void **right,SIZE_T right_count,int (*comp_proc)(const void *,const void *))
{
	void **d;
	void **l;
	void **r;
	SIZE_T lrun;
	SIZE_T rrun;
	
	d = dst;
	l = left;
	lrun = left_count;
	r = right;
	rrun = right_count;
	
	for(;;)
	{
		// find lowest
		if (comp_proc(*l,*r) <= 0)
		{
			*d++ = *l;
			lrun--;

			if (!lrun)		
			{
				// copy the rest of right array
				os_copy_memory(d,r,rrun * sizeof(void *));
				break;
			}

			l++;
		}
		else
		{
			*d++ = *r;
			rrun--;

			if (!rrun)		
			{
				// copy the rest of left array
				os_copy_memory(d,l,lrun * sizeof(void *));
				break;
			}

			r++;
		}
	}
}

// split array into two, sort and merge.
static void _os_sort_split(void **dst,void **src,SIZE_T count,int (*comp_proc)(const void *,const void *))
{
	SIZE_T mid;
	
	if (count == 1)
	{
		// already done
		dst[0] = src[0];
		
		return;
	}
	
	mid = count / 2;
	
	// dst contains a copy of the array 
	// and it doesn't matter what order it is in.
	_os_sort_split(src,dst,mid,comp_proc);
	_os_sort_split(src+mid,dst+mid,count - mid,comp_proc);

	_os_sort_merge(dst,src,mid,src+mid,count - mid,comp_proc);	
}

// sort indexes using merge sort.
void os_sort(void **indexes,SIZE_T count,int (*comp_proc)(const void *,const void *))
{
	if (count < 2)
	{
		// already sorted.
	}
	else
	{
		SIZE_T temp_size;
		pool_t temp_pool;
		void **temp;

		pool_init(&temp_pool);
		
		temp_size = safe_size_mul_sizeof_pointer(count);
		temp = pool_alloc(&temp_pool,temp_size);

		os_copy_memory(temp,indexes,temp_size);

		_os_sort_split(indexes,temp,count,comp_proc);
			
		pool_kill(&temp_pool);
	}
}

// get a known folder path.
BOOL os_get_special_folder_path(int nFolder,wchar_buf_t *out_wcbuf)
{
	ITEMIDLIST *pidl;
	BOOL ret;
	
	ret = FALSE;
	
	if (SUCCEEDED(SHGetSpecialFolderLocation(0,nFolder,&pidl)))
	{
		// buffer size should be "at least MAX_PATH"...
		wchar_buf_grow_size(out_wcbuf,MAX_PATH);
		
		if (SHGetPathFromIDList(pidl,out_wcbuf->buf))
		{
			out_wcbuf->length_in_wchars = wchar_string_get_length_in_wchars(out_wcbuf->buf);
			
			ret = TRUE;
		}

		ILFree(pidl);
	}
	
	return ret;
}

// get the appdata path and store the path in out_wcbuf.
BOOL os_get_appdata_path(wchar_buf_t *out_wcbuf)
{
	return os_get_special_folder_path(CSIDL_APPDATA,out_wcbuf);
}

// makes sure the path to filename exists.
void os_make_sure_path_to_file_exists(const wchar_t *filename)
{
	wchar_buf_t path_wcbuf;
	
	wchar_buf_init(&path_wcbuf);

	wchar_buf_copy_wchar_string(&path_wcbuf,filename);

	wchar_buf_remove_file_spec(&path_wcbuf);
	
	// try to create each subpath.
	// we don't care if it fails.
	// caller will try to create filename after this and 
	// that will return ERROR_PATH_NOT_FOUND.
	{
		wchar_t *p;
		
		p = path_wcbuf.buf;
		
		while(*p == '\\')
		{
			p++;
		}
		
		while(*p)
		{
			wchar_t *restore_p;
			
			restore_p = NULL;
			
			while(*p)
			{
				if (*p == '\\')
				{
					restore_p = p;
					*p = 0;
					p++;
					
					break;
				}
				
				p++;
			}
			
			// debug_printf("make path %S\n",path_wcbuf.buf);	
			CreateDirectory(path_wcbuf.buf,NULL);
			
			if (restore_p)
			{
				*restore_p = '\\';
			}
		}
	}

	wchar_buf_kill(&path_wcbuf);
}

// print an error message to the console.
void os_error_vprintf(const ES_UTF8 *format,va_list argptr)
{
	utf8_buf_t cbuf;
	HANDLE error_handle;

	utf8_buf_init(&cbuf);
		
	utf8_buf_vprintf(&cbuf,format,argptr);
	
	error_handle = GetStdHandle(STD_ERROR_HANDLE);

	if (GetFileType(error_handle) == FILE_TYPE_CHAR)
	{
		wchar_buf_t msg_wcbuf;
		DWORD num_written;

		wchar_buf_init(&msg_wcbuf);

		wchar_buf_copy_utf8_string(&msg_wcbuf,cbuf.buf);

		if (msg_wcbuf.length_in_wchars <= ES_DWORD_MAX)
		{
			CONSOLE_SCREEN_BUFFER_INFO csbi;
			int got_old_color;
			
			got_old_color = 0;
			
			if (GetConsoleScreenBufferInfo(error_handle,&csbi))
			{
				SetConsoleTextAttribute(error_handle,FOREGROUND_RED|FOREGROUND_INTENSITY);
				
				got_old_color = 1;	
			}
	
			WriteConsole(error_handle,msg_wcbuf.buf,(DWORD)msg_wcbuf.length_in_wchars,&num_written,NULL);

			if (got_old_color)
			{
				SetConsoleTextAttribute(error_handle,csbi.wAttributes);
			}
		}

		wchar_buf_kill(&msg_wcbuf);
	}
	else
	{
		os_write_file_utf8_string_n(error_handle,cbuf.buf,cbuf.length_in_bytes);
	}

	utf8_buf_kill(&cbuf);
}

// print an error message to the console.
void os_error_printf(const ES_UTF8 *format,...)
{
	va_list argptr;

	va_start(argptr,format);

	os_error_vprintf(format,argptr);

	va_end(argptr);
}

