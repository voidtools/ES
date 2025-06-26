
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
// UTF-8 strings

#include "es.h"

// calculate the utf8 length in bytes from the specified wchar string.
SIZE_T utf8_string_get_length_in_bytes_from_wchar_string(const wchar_t *ws)
{
	const wchar_t *p;
	SIZE_T ret_len;
	
	p = ws;
	ret_len = 0;
	
	while(*p)
	{
		int c;
		
		c = *p++;
		
		// surrogates
		if ((c >= 0xD800) && (c < 0xDC00))
		{
			if ((*p >= 0xDC00) && (*p < 0xE000))
			{
				c = 0x10000 + ((c - 0xD800) << 10) + (*p - 0xDC00);
				
				p++;
			}
		}
		
		if (c > 0xffff)
		{
			// 4 bytes
			ret_len = safe_size_add(ret_len,4);
		}
		else
		if (c > 0x7ff)
		{
			// 3 bytes
			ret_len = safe_size_add(ret_len,3);
		}
		else
		if (c > 0x7f)
		{
			// 2 bytes
			ret_len = safe_size_add(ret_len,2);
		}
		else
		{	
			// ascii
			ret_len = safe_size_add_one(ret_len);
		}
	}
	
	return ret_len;
}

// Copy a wchar string into a UTF-8 buffer.
// caller must ensure there is enough room in the buffer.
// use utf8_string_get_length_in_bytes_from_wchar_string to calculate length.
// Handles surrogates correctly.
ES_UTF8 *utf8_string_copy_wchar_string(ES_UTF8 *buf,const wchar_t *ws)
{
	const wchar_t *p;
	ES_UTF8 *d;
	
	p = ws;
	d = buf;
	
	while(*p)
	{
		int c;
		
		c = *p++;
		
		// surrogates
		if ((c >= 0xD800) && (c < 0xDC00))
		{
			if ((*p >= 0xDC00) && (*p < 0xE000))
			{
				c = 0x10000 + ((c - 0xD800) << 10) + (*p - 0xDC00);
				
				p++;
			}
		}
		
		if (c > 0xffff)
		{
			// 4 bytes
			*d++ = ((c >> 18) & 0x07) | 0xF0; // 11110xxx
			*d++ = ((c >> 12) & 0x3f) | 0x80; // 10xxxxxx
			*d++ = ((c >> 6) & 0x3f) | 0x80; // 10xxxxxx
			*d++ = (c & 0x3f) | 0x80; // 10xxxxxx
		}
		else
		if (c > 0x7ff)
		{
			// 3 bytes
			*d++ = ((c >> 12) & 0x0f) | 0xE0; // 1110xxxx
			*d++ = ((c >> 6) & 0x3f) | 0x80; // 10xxxxxx
			*d++ = (c & 0x3f) | 0x80; // 10xxxxxx
		}
		else
		if (c > 0x7f)
		{
			// 2 bytes
			*d++ = ((c >> 6) & 0x1f) | 0xC0; // 110xxxxx
			*d++ = (c & 0x3f) | 0x80; // 10xxxxxx
		}
		else
		{	
			// ascii
			*d++ = c;
		}
	}
	
	*d = 0;
	
	return d;
}

// return the length of the specified string in number of bytes.
SIZE_T utf8_string_get_length_in_bytes(const ES_UTF8 *s)
{
	const ES_UTF8 *p;
	
	p = s;
	
	while(*p)
	{
		p++;
	}
	
	return p - s;
}

// match a search string in s
// returns the position of s after matching search.
// returns NULL if it doesn't match.
const ES_UTF8 *utf8_string_parse_utf8_string(const ES_UTF8 *s,const ES_UTF8 *search)
{
	const ES_UTF8 *p1;
	const ES_UTF8 *p2;
	
	p1 = s;
	p2 = search;
	
	while(*p2)
	{
		if (*p1 != *p2)
		{
			return NULL;
		}
		
		p1++;
		p2++;
	}
	
	return p1;
}

// compare two UTF-8 strings
// returns -1 if a < b
// returns 1 if a > b
// returns 0 if a == b.
int utf8_string_compare(const ES_UTF8 *a,const ES_UTF8 *b)
{
	const ES_UTF8 *p1;
	const ES_UTF8 *p2;
	
	p1 = a;
	p2 = b;
	
	while(*p2)
	{
		if (!*p1)
		{
			// a < b
			return -1;
		}
		
		if (*p1 != *p2)
		{
			return *p1 - *p2;
		}
		
		p1++;
		p2++;
	}
	
	if (*p1)
	{
		// a > b
		return 1;
	}
	
	return 0;
}
