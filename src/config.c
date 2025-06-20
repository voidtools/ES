
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
// Configuration

#include "es.h"

int config_get_filename(int is_temp,wchar_buf_t *wcbuf)
{
	if (os_get_module_file_name(NULL,wcbuf))
	{
		wchar_buf_remove_file_spec(wcbuf);
		
		#error could already have a trailing '\\'
		wchar_buf_cat_wchar(wcbuf,'\\');

		wchar_buf_cat_utf8_string(wcbuf,"es.ini");
		
		return 1;
	}

	return 0;
}

void config_write_int(HANDLE file_handle,const char *name,int value)
{
	utf8_buf_t cbuf;
	
	utf8_buf_init(&cbuf);
	
	utf8_buf_printf(&cbuf,"%u",value);
	
	os_write_file_utf8_string_n(file_handle,cbuf.buf,cbuf.length_in_bytes);

	utf8_buf_kill(&cbuf);
}

void config_write_string(HANDLE file_handle,const char *name,const wchar_t *value)
{
	utf8_buf_t cbuf;

	utf8_buf_init(&cbuf);

	utf8_buf_copy_wchar_string(&cbuf,value);

	os_write_file_utf8_string(file_handle,name);
	os_write_file_utf8_string(file_handle,"=");
	os_write_file_utf8_string_n(file_handle,cbuf.buf,cbuf.length_in_bytes);
	os_write_file_utf8_string(file_handle,"\r\n");

	utf8_buf_kill(&cbuf);
}

int config_read_string(const char *name,const char *filename,wchar_buf_t *wcbuf)
{
//TODO:
	/*
	char buf[ES_WSTRING_SIZE];
	
	if (GetPrivateProfileStringA("ES",name,"",buf,ES_WSTRING_SIZE,filename))
	{
		if (*buf)
		{
			wchar_buf_copy_utf8_string(wcbuf,buf);
			
			return 1;
		}
	}
	*/
	return 0;
}

int config_read_int(const char *filename,const char *name,int default_value)
{
	int ret;
	wchar_buf_t wcbuf;

	ret = default_value;
	wchar_buf_init(&wcbuf);
	
	if (config_read_string(name,filename,&wcbuf))
	{	
		if (wcbuf.length_in_wchars)
		{
			ret = wchar_string_to_int(wcbuf.buf);
		}
	}
	
	wchar_buf_kill(&wcbuf);

	return ret;
}
