
void *os_copy_memory(void *dst,const void *src,SIZE_T size);
void *os_move_memory(void *dst,const void *src,SIZE_T size);
void *os_zero_memory(void *dst,SIZE_T size);
int os_replace_file(const wchar_t *old_name,const wchar_t *new_name);
HANDLE os_create_file(const wchar_t *filename);
HANDLE os_open_file(const wchar_t *filename);
void os_write_file_utf8_string(HANDLE file_handle,const ES_UTF8 *s);
void os_write_file_utf8_string_n(HANDLE file_handle,const ES_UTF8 *s,SIZE_T slength_in_bytes);
int os_get_module_file_name(HMODULE hmod,wchar_buf_t *wcbuf);
void os_get_full_path_name(const wchar_t *relative_path,wchar_buf_t *wcbuf);
