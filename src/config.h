
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

typedef struct config_ini_s
{
	utf8_buf_t file_cbuf;
	ES_UTF8 *p;
	const ES_UTF8 *key;
	const ES_UTF8 *value;
	pool_t pool;
	struct _config_keyvalue_s *keyvalue_start;
	struct _config_keyvalue_s *keyvalue_last;
	
}config_ini_t;

BOOL config_get_filename(int is_appdata,int is_temp,wchar_buf_t *wcbuf);
void config_write_int(HANDLE file_handle,const ES_UTF8 *name,int value);
void config_write_empty(HANDLE file_handle,const ES_UTF8 *name);
void config_write_dword(HANDLE file_handle,const ES_UTF8 *name,DWORD value);
void config_write_uint64(HANDLE file_handle,const ES_UTF8 *name,ES_UINT64 value);
void config_write_string(HANDLE file_handle,const ES_UTF8 *name,const wchar_t *value);
BOOL config_read_string(config_ini_t *ini,const ES_UTF8 *name,wchar_buf_t *wcbuf);
int config_read_int(config_ini_t *ini,const ES_UTF8 *name,int default_value);
DWORD config_read_dword(config_ini_t *ini,const ES_UTF8 *name,DWORD default_value);
ES_UINT64 config_read_uint64(config_ini_t *ini,const ES_UTF8 *name,ES_UINT64 default_value);

BOOL config_ini_open(config_ini_t *ini,const wchar_t *filename,const char *lowercase_ascii_section);
void config_ini_close(config_ini_t *ini);
