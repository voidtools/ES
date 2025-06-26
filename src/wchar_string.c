
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
// wchar strings

#include "es.h"

// get the length in wchars from a wchar string.
SIZE_T wchar_string_get_length_in_wchars(const wchar_t *s)
{
	const wchar_t *p;
	
	p = s;
	while(*p)
	{
		p++;
	}
	
	return (SIZE_T)(p - s);
}

// get the length in wchars from highlighted string
// skips over the '*' parts
SIZE_T wchar_string_get_highlighted_length(const wchar_t *s)
{
	const wchar_t *p;
	SIZE_T len;
	
	len = 0;
	p = s;
	while(*p)
	{
		if (*p == '*')
		{
			p++;
			
			if (*p == '*')
			{
				len++;
				p++;
			}
		}
		else
		{
			p++;
			len++;
		}
	}
	
	return len;
}

// convert a wchar string to a int.
int wchar_string_to_int(const wchar_t *s)
{
	const wchar_t *p;
	int value;
	
	p = s;
	value = 0;
	
	if ((*p == '0') && ((p[1] == 'x') || (p[1] == 'X')))
	{
		p += 2;
		
		while(*p)
		{
			if ((*p >= '0') && (*p <= '9'))
			{
				value *= 16;
				value += *p - '0';
			}
			else
			if ((*p >= 'A') && (*p <= 'F'))
			{
				value *= 16;
				value += *p - 'A' + 10;
			}
			else
			if ((*p >= 'a') && (*p <= 'f'))
			{
				value *= 16;
				value += *p - 'a' + 10;
			}
			else
			{
				break;
			}
			
			p++;
		}
	}
	else
	{
		int sign;
		
		sign = 1;
		
		if (*p == '-')
		{
			sign = -1;
			p++;
		}
	
		while(*p)
		{
			if (!((*p >= '0') && (*p <= '9')))
			{
				break;
			}
			
			value *= 10;
			value += *p - '0';
			p++;
		}
		
		if (sign < 0)
		{
			value = -value;
		}
	}
	
	return value;
}

// convert a wchar string to a dword.
DWORD wchar_string_to_dword(const wchar_t *s)
{
	return (DWORD)wchar_string_to_int(s);
}

// copy a wchar string to a wchar string buffer.
// caller MUST make sure there is enough room in buf.
void wchar_string_copy_wchar_string_n(wchar_t *buf,const wchar_t *s,SIZE_T slength_in_wchars)
{
	os_copy_memory(buf,s,slength_in_wchars * sizeof(wchar_t));
	buf[slength_in_wchars] = 0;
}

// Calculate the length of the specified utf8 string in wchars
// returns the length in wchars.
SIZE_T wchar_string_get_length_in_wchars_from_utf8_string(const ES_UTF8 *s)
{
	SIZE_T ret_len;
	const ES_UTF8 *p;
	
	p = s;
	ret_len = 0;
	
	while(*p)
	{
		if (*p & 0x80)
		{
			// p is UTF-8, decode it
			if (((*p & 0xE0) == 0xC0) && (p[1]))
			{
				// 2 byte UTF-8 to UTF-16
				ret_len = safe_size_add_one(ret_len);

				p += 2;
			}
			else
			if (((*p & 0xF0) == 0xE0) && (p[1]) && (p[2]))
			{
				// 3 byte UTF-8 to UTF-16
				ret_len = safe_size_add_one(ret_len);

				p += 3;
			}
			else
			if (((*p & 0xF8) == 0xF0) && (p[1]) && (p[2]) && (p[3]))
			{
				int c;
				
				c = ((*p & 0x07) << 18) | ((p[1] & 0x3f) << 12) | ((p[2] & 0x3f) << 6) | (p[3] & 0x3f);
				
				if (c >= 0x10000)
				{
					// surrogate.
					c -= 0x10000;
					
					ret_len = safe_size_add(ret_len,2);
				}
				else
				{
					// 4 byte UTF-8
					ret_len = safe_size_add_one(ret_len);
				}

				p += 4;
			}
			else
			{
				// invalid UTF-8.
				ret_len = safe_size_add_one(ret_len);
				
				p++;
			}
		}
		else
		{
			ret_len = safe_size_add_one(ret_len);
			
			p++;
		}
	}
	
	return ret_len;
}

// copy a UTF-8 into a wchar buffer
// caller needs to ensure there is enough room in the buffer.
// Use wchar_string_get_length_in_wchars_from_utf8_string to calculate the required length.
wchar_t *wchar_string_copy_utf8_string(wchar_t *buf,const ES_UTF8 *s)
{
	wchar_t *d;
	const ES_UTF8 *p;
	
	p = s;
	d = buf;
	
	while(*p)
	{
		if (*p & 0x80)
		{
			// p is UTF-8, decode it
			if (((*p & 0xE0) == 0xC0) && (p[1]))
			{
				// 2 byte UTF-8 to UTF-16
				*d++ = ((*p & 0x1f) << 6) | (p[1] & 0x3f);

				p += 2;
			}
			else
			if (((*p & 0xF0) == 0xE0) && (p[1]) && (p[2]))
			{
				// 3 byte UTF-8 to UTF-16
				*d++ = ((*p & 0x0f) << (12)) | ((p[1] & 0x3f) << 6) | (p[2] & 0x3f);

				p += 3;
			}
			else
			if (((*p & 0xF8) == 0xF0) && (p[1]) && (p[2]) && (p[3]))
			{
				int c;
				
				c = ((*p & 0x07) << 18) | ((p[1] & 0x3f) << 12) | ((p[2] & 0x3f) << 6) | (p[3] & 0x3f);
				
				if (c >= 0x10000)
				{
					// surrogate.
					c -= 0x10000;
					
					*d++ = 0xD800 + (c >> 10);
					*d++ = 0xDC00 + (c & 0x03FF);
				}
				else
				{
					// 4 byte UTF-8
					*d++ = c;
				}

				p += 4;
			}
			else
			{
				// invalid UTF-8.
				*d++ = 0xFFFD;
				
				p++;
			}
		}
		else
		{
			*d++ = *p;
			
			p++;
		}
	}
	
	*d = 0;

	return d;
}

// copy a UTF-8 into a wchar buffer
// caller needs to ensure there is enough room in the buffer.
// set buf to NULL to calculate the required length in bytes.
SIZE_T wchar_string_get_length_in_wchars_from_utf8_string_n(const ES_UTF8 *s,SIZE_T slength_in_bytes)
{
	SIZE_T ret_len;
	const ES_UTF8 *p;
	SIZE_T run;
	
	p = s;
	ret_len = 0;
	run = slength_in_bytes;
	
	while(run)
	{
		if (*p & 0x80)
		{
			// p is UTF-8, decode it
			if (((*p & 0xE0) == 0xC0) && (run >= 2))
			{
				// 2 byte UTF-8 to UTF-16
				ret_len = safe_size_add_one(ret_len);

				p += 2;
				run -= 2;
			}
			else
			if (((*p & 0xF0) == 0xE0) && (run >= 3))
			{
				// 3 byte UTF-8 to UTF-16
				ret_len = safe_size_add_one(ret_len);

				p += 3;
				run -= 3;
			}
			else
			if (((*p & 0xF8) == 0xF0) && (run >= 4))
			{
				int c;
				
				c = ((*p & 0x07) << 18) | ((p[1] & 0x3f) << 12) | ((p[2] & 0x3f) << 6) | (p[3] & 0x3f);
				
				if (c >= 0x10000)
				{
					// surrogate.
					c -= 0x10000;
					
					ret_len = safe_size_add(ret_len,2);
				}
				else
				{
					// 4 byte UTF-8
					ret_len = safe_size_add_one(ret_len);
				}

				p += 4;
				run -= 4;
			}
			else
			{
				// invalid UTF-8.
				ret_len = safe_size_add_one(ret_len);

				p++;
				run--;
			}
		}
		else
		{
			ret_len = safe_size_add_one(ret_len);
			
			p++;
			run--;
		}
	}

	return ret_len;
}

// copy a UTF-8 into a wchar buffer
// caller needs to ensure there is enough room in the buffer.
// set buf to NULL to calculate the required length in bytes.
wchar_t *wchar_string_copy_utf8_string_n(wchar_t *buf,const ES_UTF8 *s,SIZE_T slength_in_bytes)
{
	wchar_t *d;
	const ES_UTF8 *p;
	SIZE_T run;
	
	p = s;
	d = buf;
	run = slength_in_bytes;
	
	while(run)
	{
		if (*p & 0x80)
		{
			// p is UTF-8, decode it
			if (((*p & 0xE0) == 0xC0) && (run >= 2))
			{
				// 2 byte UTF-8 to UTF-16
				*d++ = ((*p & 0x1f) << 6) | (p[1] & 0x3f);

				p += 2;
				run -= 2;
			}
			else
			if (((*p & 0xF0) == 0xE0) && (run >= 3))
			{
				// 3 byte UTF-8 to UTF-16
				*d++ = ((*p & 0x0f) << (12)) | ((p[1] & 0x3f) << 6) | (p[2] & 0x3f);

				p += 3;
				run -= 3;
			}
			else
			if (((*p & 0xF8) == 0xF0) && (run >= 4))
			{
				int c;
				
				c = ((*p & 0x07) << 18) | ((p[1] & 0x3f) << 12) | ((p[2] & 0x3f) << 6) | (p[3] & 0x3f);
				
				if (c >= 0x10000)
				{
					// surrogate.
					c -= 0x10000;
					
					*d++ = 0xD800 + (c >> 10);
					*d++ = 0xDC00 + (c & 0x03FF);
				}
				else
				{
					// 4 byte UTF-8
					*d++ = c;
				}

				p += 4;
				run -= 4;
			}
			else
			{
				// invalid UTF-8.
				*d++ = 0xFFFD;
				
				p++;
				run--;
			}
		}
		else
		{
			*d++ = *p;
			
			p++;
			run--;
		}
	}
	
	*d = 0;

	return d;
}

// skip white spaces in s.
const wchar_t *wchar_string_skip_ws(const wchar_t *s)
{
	const wchar_t *p;
	
	p = s;
	
	while(*p)
	{
		if (!unicode_is_ascii_ws(*p))
		{
			break;
		}
		
		p++;
	}
	
	return p;
}

// allocate a copy of the specified string.
// the allocated string must be freed with mem_free.
wchar_t *wchar_string_alloc_wchar_string_n(const wchar_t *s,SIZE_T slength_in_wchars)
{
	SIZE_T size;
	wchar_t *p;
	
	size = safe_size_mul_sizeof_wchar(safe_size_add_one(slength_in_wchars));
	
	p = mem_alloc(size);
	
	os_copy_memory(p,s,slength_in_wchars * 2);
	
	p[slength_in_wchars] = 0;
	
	return p;
}

// matches '\\' or '/'
BOOL wchar_string_is_trailing_path_separator_n(const wchar_t *s,SIZE_T slength_in_wchars)
{
	if ((slength_in_wchars) && ((s[slength_in_wchars - 1] == '\\') || (s[slength_in_wchars - 1] == '/')))
	{
		return TRUE;
	}
	
	return FALSE;
}

// get the path separator character from the specified root path.
int wchar_string_get_path_separator_from_root(const wchar_t *s)
{
	const wchar_t *p;
	int is_colon;
	
	p = s;
	is_colon = 0;
	
	while(*p)
	{
		if (*p == '\\')
		{
			return '\\';
		}
		
		if (*p == '/')
		{
			return '/';
		}
		
		if (*p == ':')
		{
			is_colon = 1;
		}
	
		p++;
	}
	
	// no '\\' or '/'..
	// default to '\\';
	
	// Recycle Bin => '\\' (Recycle Bin\junk.txt)
	// Control Panel => '\\' (Control Panel\Display)
	// www.google.com => '\\' (no way to know if this is a virtual folder, use a scheme name, eg: https://)
	// http:www.google.com => '/'
	// file:C: => '/'
		
	if (is_colon)
	{
		// drive letter C:
		// or ::{guid}
		// assume there's no one-character scheme names.
		if ((*s) && (s[1] == ':'))
		{
			return '\\';
		}
		
		// scheme name: mailto: file: https: ftp:
		return '/';
	}

	// default to '\\'
	return '\\';
}

// case compare two wchar strings.
int wchar_string_compare(const wchar_t *a,const wchar_t *b)
{
	const wchar_t *p1;
	const wchar_t *p2;
	
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

// get an item from a semicolon (;) delimited list. (also allows ',' lists)
// returns the next item.
const wchar_t *wchar_string_parse_list_item(const wchar_t *s,wchar_buf_t *out_wcbuf)
{
	const wchar_t *start;
	const wchar_t *p;
	SIZE_T len;
	
	p = s;
	
	if (!*p)
	{
		return NULL;
	}
	
	start = p;
	
	for(;;)
	{
		if (!*p)
		{
			len = p - start;
			break;
		}
		
		if ((*p == ';') || (*p == ','))
		{
			len = p - start;
			p++;
			break;
		}
		
		p++;
	}
	
	wchar_buf_copy_wchar_string_n(out_wcbuf,start,len);
	
	return p;
}

// match a utf8_string in s.
// returns s AFTER the matched string.
// Otherwise, returns NULL if no match was found.
const wchar_t *wchar_string_parse_utf8_string(const wchar_t *s,const ES_UTF8 *search)
{
	const wchar_t *p1;
	const ES_UTF8 *p2;
	
	p1 = s;
	p2 = search;
	
	while(*p2)
	{
		int c2;
		
		UTF8_STRING_GET_CHAR(p2,c2);
		
		if (*p1 != c2)
		{
			return NULL;
		}
		
		p1++;
	}
	
	return p1;
}

// match an integer.
// returns s AFTER the matched integer and stores the value in out_value.
// Otherwise, returns NULL if no match was found.
const wchar_t *wchar_string_parse_int(const wchar_t *s,int *out_value)
{
	const wchar_t *p;
	int is_sign;
	int value;
	int got_digit;
	
	p = s;
	is_sign = 0;
	value = 0;
	got_digit = 0;
	
	if (*p == '-')
	{
		is_sign = 1;
		p++;
	}
	
	while(*p)
	{
		if ((*p >= '0') && (*p <= '9'))
		{
			value *= 10;
			value += *p - '0';
			got_digit = 1;
		}
		else
		{
			break;
		}
		
		p++;
	}
	
	if (!got_digit)
	{
		return NULL;
	}
	
	if (is_sign)
	{
		value = -value;
	}
	
	*out_value = value;
	
	return p;
}



