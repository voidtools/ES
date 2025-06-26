
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
// wchar buffer

#include "es.h"

// initialize a wchar buffer.
// the default value is an empty string.
void wchar_buf_init(wchar_buf_t *wcbuf)
{
	wcbuf->buf = wcbuf->stack_buf;
	wcbuf->length_in_wchars = 0;
	wcbuf->size_in_wchars = WCHAR_BUF_STACK_SIZE;
	wcbuf->buf[0] = 0;
}

// kill a wchar buffer.
// returns any allocated memory back to the system.
void wchar_buf_kill(wchar_buf_t *wcbuf)
{
	if (wcbuf->buf != wcbuf->stack_buf)
	{
		mem_free(wcbuf->buf);
	}
}

// empty a wchar buffer.
// the wchar buffer is set to an empty string.
void wchar_buf_empty(wchar_buf_t *wcbuf)
{
	wcbuf->buf[0] = 0;
	wcbuf->length_in_wchars = 0;
}

// doesn't keep the existing text.
// doesn't set the text.
// doesn't set length.
// caller should set the text.
void wchar_buf_grow_size(wchar_buf_t *wcbuf,SIZE_T size_in_wchars)
{
	if (size_in_wchars <= wcbuf->size_in_wchars)
	{
		// already enough room.
	}
	else
	{
		wchar_buf_empty(wcbuf);

		wcbuf->buf = mem_alloc(safe_size_add(size_in_wchars,size_in_wchars));
		wcbuf->size_in_wchars = size_in_wchars;
	}
}

// doesn't keep the existing text.
// doesn't set the text, only sets the length.
// DOES set the length.
// caller should set the text.
void wchar_buf_grow_length(wchar_buf_t *wcbuf,SIZE_T length_in_wchars)
{
	wchar_buf_grow_size(wcbuf,safe_size_add(length_in_wchars,1));

	wcbuf->length_in_wchars = length_in_wchars;
}

// concatenate a simple wchar string with specified length.
void wchar_buf_cat_wchar_string_n(wchar_buf_t *wcbuf,const wchar_t *s,SIZE_T slength_in_wchars)
{
	SIZE_T size_in_wchars;
	SIZE_T length_in_wchars;
	
	size_in_wchars = wcbuf->length_in_wchars;
	size_in_wchars = safe_size_add(size_in_wchars,slength_in_wchars);
	size_in_wchars = safe_size_add_one(size_in_wchars);
	
	if (size_in_wchars > wcbuf->size_in_wchars)
	{
		SIZE_T new_size_in_wchars;
		wchar_t *new_buf;

		new_size_in_wchars = safe_size_mul_2(wcbuf->size_in_wchars);
		if (new_size_in_wchars < WCHAR_BUF_CAT_MIN_ALLOC_SIZE)
		{
			new_size_in_wchars = WCHAR_BUF_CAT_MIN_ALLOC_SIZE;
		}

		new_buf = mem_alloc(safe_size_mul_sizeof_wchar(new_size_in_wchars));
		
		// don't worry about the NULL terminator.
		// we write a new one below.
		os_copy_memory(new_buf,wcbuf->buf,wcbuf->length_in_wchars * 2);
		
		if (wcbuf->buf != wcbuf->stack_buf)
		{
			mem_free(wcbuf->buf);
		}

		wcbuf->size_in_wchars = new_size_in_wchars;
		wcbuf->buf = new_buf;
	}

	length_in_wchars = wcbuf->length_in_wchars;
	
	os_copy_memory(wcbuf->buf + length_in_wchars,s,slength_in_wchars * sizeof(wchar_t));
	
	length_in_wchars += slength_in_wchars;

	wcbuf->length_in_wchars = length_in_wchars;
	wcbuf->buf[length_in_wchars] = 0;
}

// concatenate a simple wchar string.
void wchar_buf_cat_wchar_string(wchar_buf_t *wcbuf,const wchar_t *s)
{
	wchar_buf_cat_wchar_string_n(wcbuf,s,wchar_string_get_length_in_wchars(s));
}

// concatenate a wchar character.
void wchar_buf_cat_wchar(wchar_buf_t *wcbuf,wchar_t ch)
{
	wchar_buf_cat_wchar_string_n(wcbuf,&ch,1);
}

// concatenate a UTF-8 string.
void wchar_buf_cat_utf8_string(wchar_buf_t *wcbuf,const ES_UTF8 *s)
{
	wchar_buf_t add_wcbuf;

	wchar_buf_init(&add_wcbuf);

	wchar_buf_copy_utf8_string(&add_wcbuf,s);

	wchar_buf_cat_wchar_string_n(wcbuf,add_wcbuf.buf,add_wcbuf.length_in_wchars);

	wchar_buf_kill(&add_wcbuf);
}

// copy a UTF-8 string.
void wchar_buf_copy_utf8_string(wchar_buf_t *wcbuf,const ES_UTF8 *s)
{
	wchar_buf_grow_length(wcbuf,wchar_string_get_length_in_wchars_from_utf8_string(s));

	wchar_string_copy_utf8_string(wcbuf->buf,s);
}

// copy a UTF-8 string.
void wchar_buf_copy_utf8_string_n(wchar_buf_t *wcbuf,const ES_UTF8 *s,SIZE_T slength_in_bytes)
{
	wchar_buf_grow_length(wcbuf,wchar_string_get_length_in_wchars_from_utf8_string_n(s,slength_in_bytes));

	wchar_string_copy_utf8_string_n(wcbuf->buf,s,slength_in_bytes);
}

// concatenate a simple wchar string with the specified length.
void wchar_buf_copy_wchar_string_n(wchar_buf_t *wcbuf,const wchar_t *s,SIZE_T slength_in_wchars)
{
	wchar_buf_grow_length(wcbuf,slength_in_wchars);
	
	os_copy_memory(wcbuf->buf,s,slength_in_wchars * 2);
	
	wcbuf->buf[slength_in_wchars] = 0;
}

// concatenate a simple wchar string.
void wchar_buf_copy_wchar_string(wchar_buf_t *wcbuf,const wchar_t *s)
{
	wchar_buf_copy_wchar_string_n(wcbuf,s,wchar_string_get_length_in_wchars(s));
}

// remove the file specification from the specified wcbuf.
void wchar_buf_remove_file_spec(wchar_buf_t *wcbuf)
{
	const wchar_t *p;
	const wchar_t *last;
	
	p = wcbuf->buf;
	last = NULL;
	
	while(*p)
	{
		if ((*p == '\\') || (*p == '/'))
		{
			// ignore trailing '\\'
			if (!p[1])
			{
				break;
			}
			
			last = p;
		}
		
		p++;
	}
	
	if (last)
	{
		SIZE_T length_in_wchars;
		
		length_in_wchars = last - wcbuf->buf;

		wcbuf->length_in_wchars = length_in_wchars;
		wcbuf->buf[length_in_wchars] = 0;
	}
}

// sprintf
void wchar_buf_printf(wchar_buf_t *wcbuf,const ES_UTF8 *format,...)
{
	va_list argptr;
		
	va_start(argptr,format);
	
	wchar_buf_vprintf(wcbuf,format,argptr);
	
	va_end(argptr);
}

// vsprintf
void wchar_buf_vprintf(wchar_buf_t *wcbuf,const ES_UTF8 *format,va_list argptr)
{
	utf8_buf_t cbuf;
	
	utf8_buf_init(&cbuf);

	utf8_buf_vprintf(&cbuf,format,argptr);
	
	wchar_buf_copy_utf8_string(wcbuf,cbuf.buf);

	utf8_buf_kill(&cbuf);
}

// cat printf
void wchar_buf_cat_printf(wchar_buf_t *wcbuf,const ES_UTF8 *format,...)
{
	va_list argptr;
		
	va_start(argptr,format);
	
	{
		utf8_buf_t cbuf;
		
		utf8_buf_init(&cbuf);
	
		utf8_buf_vprintf(&cbuf,format,argptr);
		
		wchar_buf_cat_utf8_string(wcbuf,cbuf.buf);

		utf8_buf_kill(&cbuf);
	}
	
	va_end(argptr);
}

void wchar_buf_cat_print_UINT64(wchar_buf_t *wcbuf,ES_UINT64 value)
{
	wchar_buf_cat_printf(wcbuf,"%I64u",value);
}

// combine a path and filename
// adds the correct path separate between path and filename if required.
// stores the result in out_wcbuf.
void wchar_buf_path_cat_filename(const wchar_t *path,const wchar_t *name,wchar_buf_t *out_wcbuf)
{
	wchar_buf_copy_wchar_string(out_wcbuf,path);

	wchar_buf_cat_path_separator(out_wcbuf);
	
	wchar_buf_cat_wchar_string(out_wcbuf,name);
}

// concatenate a path separator.
// does nothing if there's already a path separator.
// uses the correct path separator based on the root.
void wchar_buf_cat_path_separator(wchar_buf_t *wcbuf)
{
	if (wcbuf->length_in_wchars)
	{
		if (!wchar_string_is_trailing_path_separator_n(wcbuf->buf,wcbuf->length_in_wchars))
		{
			int path_separator;
			
			path_separator = wchar_string_get_path_separator_from_root(wcbuf->buf);

			wchar_buf_cat_wchar(wcbuf,path_separator);
		}
	}
}

// add a wchar string to a semicolon (;) delimited list.
void wchar_buf_cat_list_wchar_string_n(wchar_buf_t *wcbuf,const wchar_t *s,SIZE_T slength_in_wchars)
{
	if (wcbuf->length_in_wchars)
	{
		wchar_buf_cat_wchar(wcbuf,';');
	}
	
	wchar_buf_cat_wchar_string_n(wcbuf,s,slength_in_wchars);
}
