int wchar_string_to_int(const wchar_t *s);
SIZE_T wchar_string_get_length_in_wchars(const wchar_t *s);
SIZE_T wchar_string_get_highlighted_length(const wchar_t *s);
void wchar_string_copy_wchar_string_n(wchar_t *buf,const wchar_t *s,SIZE_T slength_in_wchars);
wchar_t *wchar_string_copy_utf8_string(wchar_t *buf,const ES_UTF8 *s);
const wchar_t *wchar_string_skip_ws(const wchar_t *p);
wchar_t *wchar_string_alloc_wchar_string_n(const wchar_t *s,SIZE_T slength_in_wchars);
int wchar_string_is_trailing_path_separator_n(const wchar_t *s,SIZE_T slength_in_wchars);
int wchar_string_get_path_separator_from_root(const wchar_t *s);
