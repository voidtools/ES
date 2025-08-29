
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
#define UTF8_STRING_GET_CHAR(p,out_ch) \
if (!(*p & 0x80)) \
{ \
	out_ch = *p; \
	p++;\
} \
else \
{ \
	if (((*p & 0xE0) == 0xC0) && (p[1])) \
	{ \
		/* 2 byte UTF-8 */ \
		out_ch = ((*p & 0x1f) << 6) | (p[1] & 0x3f); \
		p += 2; \
	} \
	else \
	if (((*p & 0xF0) == 0xE0) && (p[1]) && (p[2])) \
	{ \
		/* 3 byte UTF-8 */ \
		out_ch = ((*p & 0x0f) << (12)) | ((p[1] & 0x3f) << 6) | (p[2] & 0x3f); \
		p += 3; \
	} \
	else \
	if (((*p & 0xF8) == 0xF0) && (p[1]) && (p[2]) && (p[3])) \
	{ \
		/* 4 byte UTF-8 */ \
		out_ch = ((*p & 0x07) << 18) | ((p[1] & 0x3f) << 12) | ((p[2] & 0x3f) << 6) | (p[3] & 0x3f); \
		p += 4; \
	} \
	else \
	{ \
		/* bad UTF-8 */ \
		out_ch = 0; \
		p++; \
	} \
}

SIZE_T utf8_string_get_length_in_bytes_from_wchar_string(const wchar_t *ws);
ES_UTF8 *utf8_string_copy_wchar_string(ES_UTF8 *buf,const wchar_t *ws);
SIZE_T utf8_string_get_length_in_bytes(const ES_UTF8 *s);
const ES_UTF8 *utf8_string_parse_utf8_string(const ES_UTF8 *s,const ES_UTF8 *search);
int utf8_string_compare(const ES_UTF8 *a,const ES_UTF8 *b);
BOOL utf8_string_is_trailing_path_separator_n(const ES_UTF8 *s,SIZE_T slength_in_bytes);
int utf8_string_get_path_separator_from_root(const ES_UTF8 *s);
void utf8_string_copy_utf8_string_n(ES_UTF8 *d,ES_UTF8 *s,SIZE_T slength_in_bytes);
