
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

#define UTF8_BUF_STACK_SIZE		MAX_PATH

// a dyanimcally sized UTF-8 string buffer
// has some stack space to avoid memory allocations.
typedef struct utf8_buf_s
{
	// pointer to UTF-8 string data.
	// buf is NULL terminated.
	ES_UTF8 *buf;

	// length of buf in bytes.
	// does not include the null terminator.
	SIZE_T length_in_bytes;

	// size of the buffer in bytes
	// includes room for the null terminator.
	SIZE_T size_in_bytes;

	// align stack_buf to 16 bytes.
	SIZE_T padding1;

	// some stack for us before we need to allocate memory from the system.
	ES_UTF8 stack_buf[UTF8_BUF_STACK_SIZE];
	
}utf8_buf_t;

void utf8_buf_init(utf8_buf_t *cbuf);
void utf8_buf_kill(utf8_buf_t *cbuf);
void utf8_buf_empty(utf8_buf_t *cbuf);
BOOL utf8_buf_try_grow_size(utf8_buf_t *cbuf,SIZE_T size_in_bytes);
void utf8_buf_grow_size(utf8_buf_t *cbuf,SIZE_T size_in_bytes);
void utf8_buf_grow_length(utf8_buf_t *cbuf,SIZE_T length_in_bytes);
void utf8_buf_copy_wchar_string(utf8_buf_t *cbuf,const wchar_t *ws);
void utf8_buf_vprintf(utf8_buf_t *cbuf,const ES_UTF8 *format,va_list argptr);
void utf8_buf_printf(utf8_buf_t *cbuf,const ES_UTF8 *format,...);
void utf8_buf_copy_utf8_string_n(utf8_buf_t *cbuf,const ES_UTF8 *s,SIZE_T length_in_bytes);
void utf8_buf_copy_utf8_string(utf8_buf_t *cbuf,const ES_UTF8 *s);
void utf8_buf_cat_utf8_string_n(utf8_buf_t *cbuf,const ES_UTF8 *s,SIZE_T slength_in_bytes);
void utf8_buf_cat_utf8_string(utf8_buf_t *cbuf,const ES_UTF8 *s);
void utf8_buf_cat_path_separator(utf8_buf_t *cbuf);
void utf8_buf_cat_byte(utf8_buf_t *cbuf,BYTE ch);
void utf8_buf_path_cat_filename(const ES_UTF8 *path,const ES_UTF8 *name,utf8_buf_t *out_cbuf);
