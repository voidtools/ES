
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

// get a unicode character from p and store it in out_ch.
// advances p
// allows unpaired surrogates.
#define WCHAR_STRING_GET_CHAR(p,out_ch) \
if ((*p >= 0xD800) && (*p < 0xDC00) && (p[1] >= 0xDC00) && (p[1] < 0xE000)) \
{ \
	out_ch = 0x10000 + ((*p - 0xD800) << 10) + (p[1] - 0xDC00); \
	p += 2; \
} \
else \
{ \
	out_ch = *p; \
	p++; \
} 

int wchar_string_to_int(const wchar_t *s);
ES_UINT64 wchar_string_to_uint64(const wchar_t *s);
DWORD wchar_string_to_dword(const wchar_t *s);
SIZE_T wchar_string_get_length_in_wchars(const wchar_t *s);
SIZE_T wchar_string_get_highlighted_length(const wchar_t *s);
void wchar_string_copy_wchar_string_n(wchar_t *buf,const wchar_t *s,SIZE_T slength_in_wchars);
SIZE_T wchar_string_get_length_in_wchars_from_utf8_string(const ES_UTF8 *s);
wchar_t *wchar_string_copy_utf8_string(wchar_t *buf,const ES_UTF8 *s);
wchar_t *wchar_string_copy_lowercase_utf8_string(wchar_t *buf,const ES_UTF8 *s);
SIZE_T wchar_string_get_length_in_wchars_from_utf8_string_n(const ES_UTF8 *s,SIZE_T slength_in_bytes);
wchar_t *wchar_string_copy_utf8_string_n(wchar_t *buf,const ES_UTF8 *s,SIZE_T slength_in_bytes);
const wchar_t *wchar_string_skip_ws(const wchar_t *p);
wchar_t *wchar_string_alloc_wchar_string_n(const wchar_t *s,SIZE_T slength_in_wchars);
BOOL wchar_string_is_trailing_path_separator_n(const wchar_t *s,SIZE_T slength_in_wchars);
int wchar_string_get_path_separator_from_root(const wchar_t *s);
int wchar_string_compare(const wchar_t *a,const wchar_t *b);
const wchar_t *wchar_string_parse_list_item(const wchar_t *s,struct wchar_buf_s *out_wcbuf);
const wchar_t *wchar_string_parse_utf8_string(const wchar_t *s,const ES_UTF8 *search);
const wchar_t *wchar_string_parse_int(const wchar_t *s,int *out_value);
const wchar_t *wchar_string_parse_nocase_lowercase_ascii_string(const wchar_t *s,const char *lowercase_search);
void wchar_string_make_lowercase(wchar_t *s);
BOOL wchar_string_wildcard_exec(const wchar_t *filename,const wchar_t *wildcard_search);
