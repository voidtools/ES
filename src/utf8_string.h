
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

ES_UTF8 *utf8_string_copy_wchar_string(ES_UTF8 *buf,const wchar_t *ws);
SIZE_T utf8_string_get_length_in_bytes(const ES_UTF8 *s);
const ES_UTF8 *utf8_string_parse_utf8_string(const ES_UTF8 *s,const ES_UTF8 *search);
int utf8_string_compare(const ES_UTF8 *a,const ES_UTF8 *b);
