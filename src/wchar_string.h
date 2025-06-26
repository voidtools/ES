
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
DWORD wchar_string_to_dword(const wchar_t *s);
SIZE_T wchar_string_get_length_in_wchars(const wchar_t *s);
SIZE_T wchar_string_get_highlighted_length(const wchar_t *s);
void wchar_string_copy_wchar_string_n(wchar_t *buf,const wchar_t *s,SIZE_T slength_in_wchars);
SIZE_T wchar_string_get_length_in_wchars_from_utf8_string(const ES_UTF8 *s);
wchar_t *wchar_string_copy_utf8_string(wchar_t *buf,const ES_UTF8 *s);
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
