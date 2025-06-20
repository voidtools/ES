
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
// UTF-8 buffer

#include "es.h"

static ES_UTF8 *_utf8_buf_print_number(int base,int abase,ES_UTF8 *buf,ES_UTF8 *d,int sign,int paddingchar,SIZE_T padding,ES_UINT64 number);
static ES_UTF8 *_utf8_buf_get_vprintf(ES_UTF8 *buf,const ES_UTF8 *format,va_list argptr);

// Init a utf8 buffer to an empty string.
void utf8_buf_init(utf8_buf_t *cbuf)
{
	cbuf->buf = cbuf->stack_buf;
	cbuf->length_in_bytes = 0;
	cbuf->size_in_bytes = UTF8_BUF_STACK_SIZE;
	cbuf->buf[0] = 0;
}

// Kill the UTF-8 buffer, releasing any allocated memory back to the system.
void utf8_buf_kill(utf8_buf_t *cbuf)
{
	if (cbuf->buf != cbuf->stack_buf)
	{
		mem_free(cbuf->buf);
	}
}

// Empty the UTF-8 buffer, the buffer will be set to an empty string.
void utf8_buf_empty(utf8_buf_t *cbuf)
{
	cbuf->buf[0] = 0;
	cbuf->length_in_bytes = 0;
}

// doesn't keep the existing text.
// doesn't set the text, only sets the length.
void utf8_buf_grow_size(utf8_buf_t *cbuf,SIZE_T size_in_bytes)
{
	if (size_in_bytes <= cbuf->size_in_bytes)
	{
		// already enough room
	}
	else
	{
		utf8_buf_empty(cbuf);
		
		cbuf->buf = mem_alloc(size_in_bytes);
		cbuf->size_in_bytes = size_in_bytes;
	}
}

// doesn't keep the existing text.
// doesn't set the text, only sets the length.
void utf8_buf_grow_length(utf8_buf_t *cbuf,SIZE_T length_in_bytes)
{
	utf8_buf_grow_size(cbuf,safe_size_add(length_in_bytes,1));

	cbuf->length_in_bytes = length_in_bytes;
}

// copy a wide string into a UTF-8 buffer.
// returns TRUE on success. Otherwise FALSE if there's not enough memory.
void utf8_buf_copy_wchar_string(utf8_buf_t *cbuf,const wchar_t *ws)
{
	utf8_buf_grow_length(cbuf,(SIZE_T)utf8_string_copy_wchar_string(NULL,ws));

	utf8_string_copy_wchar_string(cbuf->buf,ws);
}

static ES_UTF8 *_utf8_buf_print_number(int base,int abase,ES_UTF8 *buf,ES_UTF8 *d,int sign,int paddingchar,SIZE_T padding,ES_UINT64 number)
{
	ES_UINT64 i;
	int dp;
	
	dp = 0;
	
	// get len
	i = number;
	if (i)
	{
loop1:	
		i /= base;
		dp++;
		
		if (i) goto loop1;
	}
	else
	{
		// 0
		dp = 1;
	}
	
	// do padding.
	if (paddingchar != '0')
	{
		if (padding > (SIZE_T)dp)
		{
			padding -= (SIZE_T)dp;

			if (buf)
			{
				while(padding)
				{
					*d++ = paddingchar;
					
					padding--;
				}
			}
			else
			{
				if (padding)
				{
					d = (void *)safe_size_add((SIZE_T)d,padding);
				}
			}
		}
	}

	// sign is left aligned.
	if (sign)
	{
		if (buf)
		{
			*d++ = '-';
		}
		else
		{
			d = (void *)safe_size_add_one((SIZE_T)d);
		}
		
		padding--;
	}
		
	// do padding.
	if (paddingchar == '0')
	{
		if (padding > (SIZE_T)dp)
		{
			padding -= (SIZE_T)dp;

			if (buf)
			{
				while(padding)
				{
					*d++ = paddingchar;
					
					padding--;
				}
			}
			else
			{
				if (padding)
				{
					d = (void *)safe_size_add((SIZE_T)d,padding);
				}
			}
		}
	}
		
	// write the number.
	if (buf)
	{
		i = number;
		if (i)
		{

loop2:
			dp--;
			
			if ((i % base) >= 10)
			{
				d[dp] = (int)(i % base) + abase - 10;
			}
			else
			{
				d[dp] = (int)(i % base) + '0';
			}
			
			d++;
				
			i /= base;
			dp--;
			
			if (i) goto loop2;
		}
		else
		{
			// 0
			*d++ = '0';
		}
	}
	else
	{
		// dp is a small number and is safe.
		d = (void *)safe_size_add((SIZE_T)d,dp);
	}

	return d;	
}

static ES_UTF8 *_utf8_buf_get_vprintf(ES_UTF8 *buf,const ES_UTF8 *format,va_list argptr)
{
	ES_UTF8 *d;
	const ES_UTF8 *p;
	SIZE_T padding;
	SIZE_T decimal_places;
	int paddingchar;
	
	d = buf;
	p = format;
	
	while(*p)
	{
		if (*p == '%')
		{
			p++;
			
			// [0padding] | [padding]
			
			// get padding char.
			if (*p == '0')
			{
				paddingchar = '0';
			}
			else
			{
				paddingchar = ' ';
			}

			// get padding 			
			padding = 0;
			
			while(*p)
			{
				if ((*p >= '0') && (*p <= '9'))
				{
					padding *= 10;
					padding += *p - '0';
				}
				else
				{
					break;
				}
				
				p++;
			}
			
			// decimal places.
			decimal_places = 6;
			
			if (*p == '.')
			{
				p++;
				
				decimal_places = 0;
				
				while(*p)
				{
					if ((*p >= '0') && (*p <= '9'))
					{
						decimal_places *= 10;
						decimal_places += *p - '0';
					}
					else
					{
						break;
					}
					
					p++;
				}
				
				if (decimal_places > 6)
				{
					decimal_places = 6;
				}
			}
			
			// calculate size..
			if (*p == '%')
			{
				if (buf)
				{
					*d++ = *p;
				}
				else
				{
					d = (void *)safe_size_add_one((SIZE_T)d);
				}
				
				p++;
			}
			else
			if (*p == 'd')
			{
				__int64 num;

				num = va_arg(argptr,int);
				
				// we use an int64 above so we can convert -2147483648 to 2147483648 (note that the largest value for an int is 2147483647)
				// if we used 32bits -(-2147483648) == -2147483648
				d = _utf8_buf_print_number(10,0,buf,d,(num < 0),paddingchar,padding,(num<0)?-num:num);
				
				p++;
			}
			else
			if (*p == 'u')
			{
				unsigned int num;

				num = va_arg(argptr,unsigned int);
				
				d = _utf8_buf_print_number(10,0,buf,d,0,paddingchar,padding,num);
				
				p++;
			}
			else
			if ((*p == 'z') && (p[1] == 'u'))
			{
				SIZE_T num;

				num = va_arg(argptr,SIZE_T);
				
				d = _utf8_buf_print_number(10,0,buf,d,0,paddingchar,padding,num);
				
				p+=2;
			}
			else
			if ((*p == 'z') && (p[1] == 'd'))
			{
				intptr_t num;

				num = va_arg(argptr,intptr_t);
				
				// == 0x80000000 (32bit)
				// special case for -0x80000000 == 0x80000000
				if ((SIZE_T)num == (SIZE_MAX>>1)+1)
				{
					d = _utf8_buf_print_number(10,0,buf,d,1,paddingchar,padding,(SIZE_MAX>>1)+1);
				}
				else
				{
					d = _utf8_buf_print_number(10,0,buf,d,(num<0),paddingchar,padding,(num<0)?-num:num);
				}
				
				p+=2;
			}
			else
			if ((*p == 'I') && (p[1] == '6') && (p[2] == '4') && (p[3] == 'd'))
			{
				__int64 num;

				num = va_arg(argptr,__int64);
				
				// special case for 0x8000000000000000UI64, since we cant remove the sign (eg: -9223372036854775808 to 9223372036854775808) because 9223372036854775807 is the largest signed __int64.
				if ((ES_UINT64)num == (ES_UINT64)0x8000000000000000UI64)
				{
					d = _utf8_buf_print_number(10,0,buf,d,1,paddingchar,padding,(ES_UINT64)0x8000000000000000UI64);
				}
				else
				{
					d = _utf8_buf_print_number(10,0,buf,d,(num<0),paddingchar,padding,(num<0)?-num:num);
				}
				
				p+=4;
			}
			else
			if ((*p == 'I') && (p[1] == '6') && (p[2] == '4') && (p[3] == 'u'))
			{
				ES_UINT64 num;

				num = va_arg(argptr,ES_UINT64);
				
				d = _utf8_buf_print_number(10,0,buf,d,0,paddingchar,padding,num);
				
				p+=4;
			}
			else
			if ((*p == 'I') && (p[1] == '6') && (p[2] == '4') && ((p[3] == 'x') || (p[3] == 'X')))
			{
				ES_UINT64 num;

				num = va_arg(argptr,ES_UINT64);
				
				d = _utf8_buf_print_number(16,p[3]+'A'-'X',buf,d,0,paddingchar,padding,num);
				
				p+=4;
			}
			else
			if ((*p == 'x') || (*p == 'X'))
			{
				unsigned int num;

				num = va_arg(argptr,unsigned int);
				
				d = _utf8_buf_print_number(16,*p+'A'-'X',buf,d,0,paddingchar,padding,num);
				
				p++;
			}
			else			
			if ((*p == 'p') || (*p == 'P'))
			{
				SIZE_T num;

				num = va_arg(argptr,SIZE_T);
				
				d = _utf8_buf_print_number(16,*p+'A'-'P',buf,d,0,'0',8<<(sizeof(SIZE_T)>>3),num);
				
#if SIZE_MAX == 0xffffffffffffffffui64
#elif SIZE_MAX == 0xffffffffui32
#else
#error unknown SIZE_MAX
#endif

				p++;
			}
			else
			if (*p == 's')
			{	
				const ES_UTF8 *str;
				
				str = va_arg(argptr,const ES_UTF8 *);
				
				if (buf)
				{
					while(*str)
					{
						*d++ = *str;
						
						str++;
					}
				}
				else
				{
					d = (void *)safe_size_add((SIZE_T)d,utf8_string_get_length_in_bytes(str));
				}
				
				p++;
			}
			else
			if (*p == 'S')
			{	
				const wchar_t *wstr;
				
				wstr = va_arg(argptr,const wchar_t *);
				
				if (buf)
				{
					d = utf8_string_copy_wchar_string(d,wstr);
				}
				else
				{
					d = (void *)safe_size_add((SIZE_T)d,(SIZE_T)utf8_string_copy_wchar_string(NULL,wstr));
				}
				
				p++;
			}
			else
			if (*p == 'c')
			{	
				int ch;
				
				// UTF-8 BYTE
				ch = va_arg(argptr,int);
				
				if (buf)
				{
					*d++ = ch;
				}
				else
				{
					d = (void *)safe_size_add_one((SIZE_T)d);
				}
				
				p++;
			}
			else
			if (*p == 'C')
			{	
				int ch;
				wchar_t wch[2];
				
				// ch MUST be ASCII
				ch = va_arg(argptr,int);

				wch[0] = (wchar_t)ch;
				wch[1] = 0;
				
				if (buf)
				{
					d = utf8_string_copy_wchar_string(d,wch);
				}
				else
				{
					d = (void *)safe_size_add((SIZE_T)d,(SIZE_T)utf8_string_copy_wchar_string(NULL,wch));
				}
				
				p++;
			}
			else
			{
				// ignore it.
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
				d = (void *)safe_size_add_one((SIZE_T)d);
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

void utf8_buf_vprintf(utf8_buf_t *cbuf,const ES_UTF8 *format,va_list argptr)
{
	utf8_buf_grow_length(cbuf,(SIZE_T)_utf8_buf_get_vprintf(NULL,format,argptr));
	
	_utf8_buf_get_vprintf(cbuf->buf,format,argptr);
}


void utf8_buf_printf(utf8_buf_t *cbuf,const ES_UTF8 *format,...)
{
	va_list argptr;
		
	va_start(argptr,format);
	
	utf8_buf_vprintf(cbuf,format,argptr);
	
	va_end(argptr);
}

