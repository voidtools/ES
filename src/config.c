
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
// Configuration es.ini

#include "es.h"

// a keyvalue pair.
typedef struct _config_keyvalue_s
{
	// the next keyvalue pair
	// in reverse order (start == last line in es.ini)
	struct _config_keyvalue_s *next;
	
	// the key text
	const ES_UTF8 *key;
	
	// the value text.
	const ES_UTF8 *value;
	
}_config_keyvalue_t;

static BOOL _config_ini_get_line(config_ini_t *ini);
static BOOL _config_ini_find_next_keyvalue(config_ini_t *ini);
static _config_keyvalue_t *_config_keyvalue_find(config_ini_t *ini,const ES_UTF8 *key);

// get the es.ini filename
BOOL config_get_filename(int is_appdata,int is_temp,wchar_buf_t *wcbuf)
{
	if (is_appdata)
	{
		// "%APPDATA%\\voidtools\\es"
		if (os_get_appdata_path(wcbuf))
		{
			wchar_buf_cat_path_separator(wcbuf);
	
			wchar_buf_cat_utf8_string(wcbuf,"voidtools");
			
			wchar_buf_cat_path_separator(wcbuf);

			wchar_buf_cat_utf8_string(wcbuf,"es");
			
			wchar_buf_cat_path_separator(wcbuf);

			wchar_buf_cat_utf8_string(wcbuf,"es.ini");
			
			if (is_temp)
			{
				wchar_buf_cat_utf8_string(wcbuf,".tmp");
			}
			
			return TRUE;
		}
	}
	else
	{
		// same path as es.exe
		if (os_get_module_file_name(NULL,wcbuf))
		{
			wchar_buf_remove_file_spec(wcbuf);
			
			wchar_buf_cat_path_separator(wcbuf);

			wchar_buf_cat_utf8_string(wcbuf,"es.ini");
			
			if (is_temp)
			{
				wchar_buf_cat_utf8_string(wcbuf,".tmp");
			}
			
			return TRUE;
		}
	}

	return FALSE;
}

// write out a key=int-value pair to the opened es.ini
void config_write_int(HANDLE file_handle,const ES_UTF8 *name,int value)
{
	utf8_buf_t cbuf;
	
	utf8_buf_init(&cbuf);
	
	utf8_buf_printf(&cbuf,"%s=%d\r\n",name,value);
	
	os_write_file_utf8_string_n(file_handle,cbuf.buf,cbuf.length_in_bytes);

	utf8_buf_kill(&cbuf);
}

// write out a key=dword-value pair to the opened es.ini
void config_write_dword(HANDLE file_handle,const ES_UTF8 *name,DWORD value)
{
	utf8_buf_t cbuf;
	
	utf8_buf_init(&cbuf);
	
	utf8_buf_printf(&cbuf,"%s=%u\r\n",name,value);
	
	os_write_file_utf8_string_n(file_handle,cbuf.buf,cbuf.length_in_bytes);

	utf8_buf_kill(&cbuf);
}

// write out a key=dword-value pair to the opened es.ini
void config_write_uint64(HANDLE file_handle,const ES_UTF8 *name,ES_UINT64 value)
{
	utf8_buf_t cbuf;
	
	utf8_buf_init(&cbuf);
	
	utf8_buf_printf(&cbuf,"%s=%I64u\r\n",name,value);
	
	os_write_file_utf8_string_n(file_handle,cbuf.buf,cbuf.length_in_bytes);

	utf8_buf_kill(&cbuf);
}

// write out a key=string-value pair to the opened es.ini
void config_write_string(HANDLE file_handle,const ES_UTF8 *name,const wchar_t *value)
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

// read a string value with the specified name.
// returns TRUE if found and stores the value in wcbuf.
// Otherwise, returns FALSE if not found.
BOOL config_read_string(config_ini_t *ini,const ES_UTF8 *name,wchar_buf_t *wcbuf)
{
	_config_keyvalue_t *keyvalue;
							
	keyvalue = _config_keyvalue_find(ini,name);
	
	if (keyvalue)
	{
		wchar_buf_copy_utf8_string(wcbuf,keyvalue->value);
			
		return TRUE;
	}

	return FALSE;
}

// read an int-value with the specified name.
// returns the int-value if found.
// Otherwise, returns default_value
int config_read_int(config_ini_t *ini,const ES_UTF8 *name,int default_value)
{
	int ret;
	_config_keyvalue_t *keyvalue;
	
	ret = default_value;
	keyvalue = _config_keyvalue_find(ini,name);
	
	if (keyvalue)
	{
		if (*keyvalue->value)
		{
			wchar_buf_t wcbuf;

			wchar_buf_init(&wcbuf);
		
			wchar_buf_copy_utf8_string(&wcbuf,keyvalue->value);
		
			ret = wchar_string_to_int(wcbuf.buf);
		
			wchar_buf_kill(&wcbuf);
		}
	}

	return ret;
}

// read an dword-value with the specified name.
// returns the dword-value if found.
// Otherwise, returns default_value
DWORD config_read_dword(config_ini_t *ini,const ES_UTF8 *name,DWORD default_value)
{
	DWORD ret;
	_config_keyvalue_t *keyvalue;
	
	ret = default_value;
	keyvalue = _config_keyvalue_find(ini,name);
	
	if (keyvalue)
	{
		if (*keyvalue->value)
		{
			wchar_buf_t wcbuf;

			wchar_buf_init(&wcbuf);
		
			wchar_buf_copy_utf8_string(&wcbuf,keyvalue->value);
		
			ret = wchar_string_to_dword(wcbuf.buf);
		
			wchar_buf_kill(&wcbuf);
		}
	}

	return ret;
}

// read an uint64-value with the specified name.
// returns the uint64-value if found.
// Otherwise, returns default_value
ES_UINT64 config_read_uint64(config_ini_t *ini,const ES_UTF8 *name,ES_UINT64 default_value)
{
	ES_UINT64 ret;
	_config_keyvalue_t *keyvalue;
	
	ret = default_value;
	keyvalue = _config_keyvalue_find(ini,name);
	
	if (keyvalue)
	{
		if (*keyvalue->value)
		{
			wchar_buf_t wcbuf;

			wchar_buf_init(&wcbuf);
		
			wchar_buf_copy_utf8_string(&wcbuf,keyvalue->value);
		
			ret = wchar_string_to_uint64(wcbuf.buf);
		
			wchar_buf_kill(&wcbuf);
		}
	}

	return ret;
}

// read a line from the ini file.
// stores the found key/value pair in ini.
// ini->value will be set to NULL for comments, sections and if there's no =
// returns TRUE if data is available.
// returns FALSE if there's no more data.
static BOOL _config_ini_get_line(config_ini_t *ini)
{
	ES_UTF8 *p;
	
	if (!ini->p)
	{
		return FALSE;
	}
	
	p = ini->p;
	ini->key = p;
	ini->value = NULL;
	
	if (!*p)
	{
		return FALSE;
	}
	
	while(*p)
	{
		if ((*p == '\r') && (p[1] == '\n'))
		{
			*p = 0;
			
			p += 2;
			
			break;
		}
		
		if (*p == '\n')
		{
			*p = 0;
			
			p++;
			
			break;
		}
		
		if (*p == '=')
		{
			if (!ini->value)
			{
				*p = 0;
				
				p++;
				
				ini->value = p;
			}
		}
		
		p++;
	}
	
	ini->p = p;
	
	return TRUE;
}

// open an ini file, find the specified section and read all the keyvalue pairs.
// the keyvalue pairs are stored in ini.
BOOL config_ini_open(config_ini_t *ini,const wchar_t *filename,const ES_UTF8 *section)
{
	BOOL ret;
	HANDLE file_handle;
	
	ret = FALSE;
	file_handle = os_open_file(filename);
	
	if (file_handle != INVALID_HANDLE_VALUE)
	{
		DWORD size_lo;
		DWORD size_hi;
		
		size_lo = GetFileSize(file_handle,&size_hi);
		
		if ((size_lo != INVALID_FILE_SIZE) && (!size_hi))
		{
			DWORD numread;
			
			utf8_buf_init(&ini->file_cbuf);
			pool_init(&ini->pool);
			ini->keyvalue_start = NULL;
			ini->keyvalue_last = NULL;

			utf8_buf_grow_length(&ini->file_cbuf,size_lo);
			
			if (ReadFile(file_handle,ini->file_cbuf.buf,size_lo,&numread,NULL))
			{
				if (numread == size_lo)
				{
					ini->file_cbuf.buf[size_lo] = 0;
					ini->p = ini->file_cbuf.buf;
					
					// find section
					for(;;)
					{
						if (!_config_ini_get_line(ini))
						{
							break;
						}
						
						if (*ini->key == '[')
						{
							const ES_UTF8 *match_p;
							
							match_p = utf8_string_parse_utf8_string(ini->key + 1,section);
							if (match_p)
							{
								if (*match_p == ']')
								{
									ret = TRUE;
									
									// save values.
									while(_config_ini_find_next_keyvalue(ini))
									{
										_config_keyvalue_t *keyvalue;
										
										// alloc
										keyvalue = pool_alloc(&ini->pool,sizeof(_config_keyvalue_t));
										
										// init
										keyvalue->key = ini->key;
										keyvalue->value = ini->value;
										
										// insert at start, as we search from the start
										// this way the last added value will be returned first.
										keyvalue->next = ini->keyvalue_start;
										ini->keyvalue_start = keyvalue;
										
										if (!ini->keyvalue_last)
										{
											ini->keyvalue_last = keyvalue;
										}
									}
									
									break;
								}
							}
						}
					}
				}
			}
			
			if (!ret)
			{
				config_ini_close(ini);
			}
		}
		
		CloseHandle(file_handle);
	}
	
	return ret;
}

// find the next keyvalue pair.
// skips any ; comments
// returns TRUE if another keyvalue pair is found.
// returns FALSE if the section changes or if there's no more keyvalue pairs.
static BOOL _config_ini_find_next_keyvalue(config_ini_t *ini)
{
	for(;;)
	{
		if (!_config_ini_get_line(ini))
		{
			break;
		}
		
		// comment.
		if (*ini->key == ';')
		{
			continue;
		}
		
		// section
		if (*ini->key == '[')
		{
			ini->p = NULL;
			break;
		}
		
		// no value
		if (!ini->value)
		{
			continue;
		}
		
		return TRUE;
	}

	return FALSE;
}

// find a keyvalue pair by key name.
// returns a pointer to the keyvalue pair.
// returns NULL if not found.
static _config_keyvalue_t *_config_keyvalue_find(config_ini_t *ini,const ES_UTF8 *key)
{
	_config_keyvalue_t *keyvalue;
	
	keyvalue = ini->keyvalue_start;
	while(keyvalue)
	{
		const ES_UTF8 *match_p;
							
		match_p = utf8_string_parse_utf8_string(keyvalue->key,key);
		if (match_p)
		{
			if (!*match_p)
			{
				return keyvalue;
			}
		}
		
		keyvalue = keyvalue->next;
	}
	
	return NULL;
}

// close an ini file.
// returns any allocated memory to the system.
void config_ini_close(config_ini_t *ini)
{
	pool_kill(&ini->pool);
	utf8_buf_kill(&ini->file_cbuf);
}

