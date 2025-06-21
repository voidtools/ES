
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

void wchar_string_copy_wchar_string_n(wchar_t *buf,const wchar_t *s,SIZE_T slength_in_wchars)
{
	os_copy_memory(buf,s,slength_in_wchars * sizeof(wchar_t));
	buf[slength_in_wchars] = 0;
}

// don't need to use es_safe_size as the returned length is <= length of s.
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
				if (buf)
				{
					*d++ = ((*p & 0x1f) << 6) | (p[1] & 0x3f);
				}
				else
				{
					d++;
				}

				p += 2;
			}
			else
			if (((*p & 0xF0) == 0xE0) && (p[1]) && (p[2]))
			{
				// 3 byte UTF-8 to UTF-16
				if (buf)
				{
					*d++ = ((*p & 0x0f) << (12)) | ((p[1] & 0x3f) << 6) | (p[2] & 0x3f);
				}
				else
				{
					d++;
				}

				p += 3;
			}
			else
			if (((*p & 0xF8) == 0xF0) && (p[1]) && (p[2]) && (p[3]))
			{
				int c;
				
				c = ((*p & 0x07) << 18) | ((p[1] & 0x3f) << 12) | ((p[2] & 0x3f) << 6) | (p[3] & 0x3f);
				
				if (c > 0xFFFF)
				{
					// surrogate.
					c -= 0x10000;
					
					if (buf)
					{
						*d++ = 0xD800 + (c >> 10);
						*d++ = 0xDC00 + (c & 0x03FF);
					}
					else
					{
						d += 2;
					}
				}
				else
				{
					// 4 byte UTF-8
					if (buf)
					{
						*d++ = c;
					}
					else
					{
						d++;
					}
				}

				p += 4;
			}
			else
			{
				if (buf)
				{
					*d++ = 0;
				}
				else
				{
					d++;
				}
				
				p++;
			}
		}
		else
		{
			if (buf)
			{			
				*d++ = *p;
			}
			else
			{
				d++;
			}
			
			p++;
		}
	}
	
	if (buf)
	{			
		*d = 0;
	}

	return d;
}

const wchar_t *wchar_string_skip_ws(const wchar_t *p)
{
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
