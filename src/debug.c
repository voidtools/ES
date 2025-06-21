
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
// simple arrays

#include "es.h"

#ifdef _DEBUG

void debug_printf(ES_UTF8 *format,...)
{
	va_list argptr;
	wchar_buf_t wcbuf;
	DWORD num_written;

	va_start(argptr,format);
	wchar_buf_init(&wcbuf);

	wchar_buf_vprintf(&wcbuf,format,argptr);
	
	if (wcbuf.length_in_wchars <= ES_DWORD_MAX)
	{
		WriteConsole(GetStdHandle(STD_OUTPUT_HANDLE),wcbuf.buf,(DWORD)wcbuf.length_in_wchars,&num_written,NULL);
	}

	wchar_buf_kill(&wcbuf);
	va_end(argptr);
}

void debug_error_printf(const ES_UTF8 *format,...)
{
	va_list argptr;

	va_start(argptr,format);

	os_error_vprintf(format,argptr);

	va_end(argptr);
}

#endif

