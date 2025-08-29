
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

void os_init(void);
void os_kill(void);
void *os_copy_memory(void *dst,const void *src,SIZE_T size);
void *os_move_memory(void *dst,const void *src,SIZE_T size);
void *os_zero_memory(void *dst,SIZE_T size);
BOOL os_replace_file(const wchar_t *old_name,const wchar_t *new_name);
HANDLE os_create_file(const wchar_t *filename);
HANDLE os_open_file(const wchar_t *filename);
BOOL os_write_file_utf8_string(HANDLE file_handle,const ES_UTF8 *s);
BOOL os_write_file_utf8_string_n(HANDLE file_handle,const ES_UTF8 *s,SIZE_T slength_in_bytes);
BOOL os_get_module_file_name(HMODULE hmod,wchar_buf_t *out_wcbuf);
void os_expand_environment_variables(const wchar_t *s,wchar_buf_t *out_wcbuf);
void os_get_full_path_name(const wchar_t *relative_path,wchar_buf_t *out_wcbuf);
void os_get_expanded_full_path_name(const wchar_t *relative_path,wchar_buf_t *out_wcbuf);
void os_sort(void **indexes,SIZE_T count,int (*comp_proc)(const void *,const void *));
BOOL os_get_special_folder_path(int nFolder,wchar_buf_t *out_wcbuf);
BOOL os_get_appdata_path(wchar_buf_t *out_wcbuf);
void os_make_sure_path_to_file_exists(const wchar_t *filename);
void os_error_vprintf(const ES_UTF8 *format,va_list argptr);
void os_error_printf(const ES_UTF8 *format,...);
ES_UINT64 os_localtime_to_filetime(const SYSTEMTIME *localst);
BOOL os_filetime_to_localtime(ES_UINT64 ft,SYSTEMTIME *out_localst);
