
void *os_copy_memory(void *dst,const void *src,SIZE_T size);
void *os_move_memory(void *dst,const void *src,SIZE_T size);
void *os_zero_memory(void *dst,SIZE_T size);
BOOL os_replace_file(const wchar_t *old_name,const wchar_t *new_name);
HANDLE os_create_file(const wchar_t *filename);
HANDLE os_open_file(const wchar_t *filename);
BOOL os_write_file_utf8_string(HANDLE file_handle,const ES_UTF8 *s);
BOOL os_write_file_utf8_string_n(HANDLE file_handle,const ES_UTF8 *s,SIZE_T slength_in_bytes);
BOOL os_get_module_file_name(HMODULE hmod,wchar_buf_t *wcbuf);
void os_get_full_path_name(const wchar_t *relative_path,wchar_buf_t *wcbuf);
void os_sort(void **indexes,SIZE_T count,int (*comp_proc)(const void *,const void *));
BOOL os_get_special_folder_path(int nFolder,wchar_buf_t *out_wcbuf);
BOOL os_get_appdata_path(wchar_buf_t *out_wcbuf);
void os_make_sure_path_to_file_exists(const wchar_t *filename);
void os_error_vprintf(const ES_UTF8 *format,va_list argptr);
void os_error_printf(const ES_UTF8 *format,...);
