
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

#define WCHAR_BUF_STACK_SIZE				MAX_PATH
#define WCHAR_BUF_CAT_MIN_ALLOC_SIZE		65536

// a dyanimcally sized wchar string
// has some stack space to avoid memory allocations.
typedef struct wchar_buf_s
{
	// pointer to wchar string data.
	// buf is NULL terminated.
	wchar_t *buf;
	
	// length of buf in wide chars.
	// does not include the null terminator.
	SIZE_T length_in_wchars;
	
	// size of the buffer in wide chars
	// includes room for the null terminator.
	SIZE_T size_in_wchars;
	
	// align stack_buf to 16 bytes.
	SIZE_T padding1;

	// some stack for us before we need to allocate memory from the system.
	wchar_t stack_buf[WCHAR_BUF_STACK_SIZE];
	
}wchar_buf_t;

void wchar_buf_init(wchar_buf_t *wcbuf);
void wchar_buf_kill(wchar_buf_t *wcbuf);
void wchar_buf_empty(wchar_buf_t *wcbuf);
void wchar_buf_grow_size(wchar_buf_t *wcbuf,SIZE_T length_in_wchars);
void wchar_buf_grow_length(wchar_buf_t *wcbuf,SIZE_T length_in_wchars);
void wchar_buf_cat_wchar_string(wchar_buf_t *wcbuf,const wchar_t *s);
void wchar_buf_cat_wchar_string_n(wchar_buf_t *wcbuf,const wchar_t *s,SIZE_T slength_in_wchars);
void wchar_buf_cat_wchar(wchar_buf_t *wcbuf,wchar_t ch);
void wchar_buf_copy_utf8_string(wchar_buf_t *wcbuf,const ES_UTF8 *s);
void wchar_buf_copy_utf8_string_n(wchar_buf_t *wcbuf,const ES_UTF8 *s,SIZE_T slength_in_bytes);
void wchar_buf_cat_utf8_string(wchar_buf_t *wcbuf,const ES_UTF8 *s);
void wchar_buf_copy_wchar_string_n(wchar_buf_t *wcbuf,const wchar_t *s,SIZE_T slength_in_wchars);
void wchar_buf_copy_wchar_string(wchar_buf_t *wcbuf,const wchar_t *s);
void wchar_buf_remove_file_spec(wchar_buf_t *wcbuf);
void wchar_buf_printf(wchar_buf_t *wcbuf,const ES_UTF8 *format,...);
void wchar_buf_vprintf(wchar_buf_t *wcbuf,const ES_UTF8 *format,va_list argptr);
void wchar_buf_cat_printf(wchar_buf_t *wcbuf,const ES_UTF8 *format,...);
void wchar_buf_cat_print_UINT64(wchar_buf_t *wcbuf,ES_UINT64 value);
void wchar_buf_path_cat_filename(const wchar_t *path,const wchar_t *name,wchar_buf_t *wcbuf);
void wchar_buf_cat_path_separator(wchar_buf_t *wcbuf);
void wchar_buf_cat_list_wchar_string_n(wchar_buf_t *wcbuf,const wchar_t *s,SIZE_T slength_in_wchars);
void wchar_buf_copy_lowercase_utf8_string(wchar_buf_t *wcbuf,const ES_UTF8 *s);
const wchar_t *wchar_buf_parse_list_item(const wchar_t *s,wchar_buf_t *out_wcbuf);
void wchar_buf_fix_quotes(wchar_buf_t *in_out_wcbuf);
