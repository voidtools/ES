
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

// Everything command line interface via IPC.

// DONE:
// *-error-on-no-results alias
// *added manifest to disable the virtual store.
// *es.ini is now stored in %APPDATA%\voidtools\es\es.ini if es.exe location is not writeable.
// *output as JSON ./ES.exe | ConvertFrom-Json
// *trailing '\\' on folders.
// *exporting as EFU should not use localized dates (only CSV should do this). EFU should be raw FILETIMEs.
// *Show the sum of the size. -use -get-total-size
// *add an option to append a path separator for folders. -added -folder-append-path-separator
// *would be nice if it had -POSIX and -NUL switches to generate NUL for RG/GREP -added "-nul", "-crlf" and "-lf". -need clarification on -posix.
// *export as TSV to match Everything export.
// *inc-run-count/set-run-count should allow -instance
// *fix background highlight when a result spans multiple lines, eg: -highlight -highlight-color 0xa0 -appears to be fix with new cell outputing.
// *-save-db -save database to disk command line option. -already added
// *-rebuild -force a rebuild -already added
// *es sonic -name -path-column -name-color 13 -highlight // -name-color 13 doesn't work with -highlight ! -working fine now.
// *Gather file information if it is not indexed. -already added.
// *-get-size <filename> - expand -get-run-count to get other information for a single file? -what happens when the file doesn't exist? -added "-get-folder-size" -returns error code ES_ERROR_NO_RESULTS if not found.
//
// TODO:
// vs2019/vs2022 project and sln files. -move to vs subfolder.
// [HIGH] c# cmdlet for powershell.
// export to clipboard (aka copy to clipboard)
// add a -max-path option. (limit paths to 259 chars -use shortpaths if we exceed.)
// add a -short-full-path option.
// -wait block until search results one result, combine with new-results-only:
// open to show summary information: total size and number of results (like -get-result-count)
// -export-m3u/-export-m3u8 need to use short paths for VLFNs. -No media players support VLFNs.
// ideally, -sort should apply the sort AFTER the search. -currently filters are overwriting the sort. -this needs to be done at the query level so we dont block with a sort. -add a sort-after parameter...
// add a -unquote command line option so we can handle powershell, eg: es.exe -unquote case:`"$x`" => es.exe -unquote "case:"abc 123"" => es.exe case:"abc 123"
// export results to environment variables -mostly useful for -n 1
// Use system number formatting (some localizations don't use , for the comma)
// add a -no-config command line option to bypass loading of settings. -cant do this easily as we process command line options AFTER loading es.ini
// improve -pause when the window is resized.
// -v to display everything version?
// -path is not working with -r It is using the <"filter"> (-path is adding brackets and double quotes, neither of which regex understands)
// path ellipsis?
// when in pause:
//		w - wrap
//		n - number
//		/ - search
//		h - help
//		r - write
//		default "prompt", ":".
//		If you want help, :h.
//		Save the stream to a file, :r c:\tmp\out.txt
// Maybe a configurable prompt, so the "current" (bottom-most) line, total count of results. 
// [23/72]:
// emulate LESS
// *shouldn't we use environment variables for storing switches? like MORE? -registry does make it easy. -using ini now
// custom date/time format. eg: dd/MM/yy

// NOTES:
// ScrollConsoleScreenBuffer is unuseable because it fills invalidated areas, which causes unnecessary flickering.


#include "es.h"

#define ES_COPYDATA_IPCTEST_QUERYCOMPLETEW		0
#define ES_COPYDATA_IPCTEST_QUERYCOMPLETE2W		1

#define ES_EXPORT_TYPE_NONE			0
#define ES_EXPORT_TYPE_CSV			1
#define ES_EXPORT_TYPE_EFU			2
#define ES_EXPORT_TYPE_TXT			3
#define ES_EXPORT_TYPE_M3U			4
#define ES_EXPORT_TYPE_M3U8			5
#define ES_EXPORT_TYPE_TSV			6
#define ES_EXPORT_TYPE_JSON			7

#define ES_EXPORT_BUF_SIZE			65536

#define ES_PAUSE_TEXT				"ESC=Quit; Up,Down,Left,Right,Page Up,Page Down,Home,End=Scroll"
#define ES_BLANK_PAUSE_TEXT			"                                                              "

#define ES_IPC_VERSION_FLAG_IPC1	0x00000001 // Everything 1.3
#define ES_IPC_VERSION_FLAG_IPC2	0x00000002 // Everything 1.4
#define ES_IPC_VERSION_FLAG_IPC3	0x00000004 // Everything 1.5

#define _ES_MSGFLT_ALLOW			1

typedef struct _es_tagCHANGEFILTERSTRUCT 
{
	DWORD cbSize;
	DWORD ExtStatus;
}_ES_CHANGEFILTERSTRUCT;

static int _es_main(void);
static void _es_console_fill(SIZE_T count,int ascii_ch);
static void _es_output_cell_write_console_wchar_string(const wchar_t *text,int is_highlighted);
static void _es_output_cell_utf8_string(const ES_UTF8 *text,int is_highlighted);
static void _es_output_cell_wchar_string(const wchar_t *text,int is_highlighted);
static void _es_export_write_data(const ES_UTF8 *text,SIZE_T length_in_bytes);
static void _es_export_write_wchar_string_n(const wchar_t *text,SIZE_T wlen);
static void _es_format_number(ES_UINT64 number,int allow_digit_grouping,wchar_buf_t *out_wcbuf);
static void _es_format_dimensions(EVERYTHING3_DIMENSIONS *dimensions_value,wchar_buf_t *out_wcbuf);
static int _es_compare_list_items(const EVERYTHING_IPC_ITEM *a,const EVERYTHING_IPC_ITEM *b);
static void _es_output_cell_text_property_wchar_string(const wchar_t *value);
static void _es_output_cell_text_property_utf8_string_n(const ES_UTF8 *value,SIZE_T length_in_bytes);
static void _es_output_cell_highlighted_text_property_wchar_string(const wchar_t *value);
static void _es_output_cell_highlighted_text_property_utf8_string(const ES_UTF8 *value);
static void _es_output_cell_unknown_property(void);
static void _es_output_cell_size_property(int is_dir,ES_UINT64 value);
static void _es_output_cell_filetime_property(ES_UINT64 value);
static void _es_output_cell_duration_property(ES_UINT64 value);
static void _es_output_cell_attribute_property(DWORD file_attributes);
static void _es_output_cell_number_property(ES_UINT64 value,ES_UINT64 empty_value);
static void _es_output_cell_dimensions_property(EVERYTHING3_DIMENSIONS *dimensions_value);
static void _es_output_cell_data_property(const BYTE *data,SIZE_T size);
static void _es_output_cell_separator(void);
static void _es_output_noncell_wchar_string(const wchar_t *text);
static void _es_output_noncell_wchar_string_n(const wchar_t *text,SIZE_T length_in_wchars);
static void _es_output_noncell_utf8_string(const ES_UTF8 *text);
static void _es_output_noncell_printf(const ES_UTF8 *text,...);
static BOOL _es_output_header(void);
static void _es_output_line_begin(int is_first);
static void _es_output_line_end(int is_more);
static void _es_output_page_begin(void);
static void _es_output_page_end(void);
static void _es_output_cell_printf(int is_highlighted,ES_UTF8 *format,...);
static BOOL _es_ipc1_query(void);
static BOOL _es_ipc2_query(void);
static BOOL _es_ipc3_query(void);
static void _es_output_ipc1_results(EVERYTHING_IPC_LIST *list,SIZE_T index_start,SIZE_T count);
static void _es_output_ipc2_results(EVERYTHING_IPC_LIST2 *list,SIZE_T index_start,SIZE_T count);
static void _es_output_ipc2_total_size(EVERYTHING_IPC_LIST2 *list,int count);
static void _es_output_ipc3_results(ipc3_result_list_t *result_list,SIZE_T index_start,SIZE_T count);
static LRESULT __stdcall _es_window_proc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam);
static void _es_help(void);
static HWND _es_find_ipc_window(void);
static const wchar_t *_es_parse_command_line_option_start(const wchar_t *s);
static BOOL _es_check_option_utf8_string(const wchar_t *param,const ES_UTF8 *s);
static void *_es_ipc2_get_column_data(EVERYTHING_IPC_LIST2 *list,SIZE_T index,DWORD property_id,DWORD property_highlight);
static void _es_format_size(ES_UINT64 size,wchar_buf_t *wcbuf);
static void _es_format_filetime(ES_UINT64 filetime,wchar_buf_t *wcbuf);
static void _es_format_duration(ES_UINT64 filetime,wchar_buf_t *wcbuf);
static void _es_format_attributes(DWORD attributes,wchar_buf_t *wcbuf);
static int _es_filetime_to_localtime(SYSTEMTIME *localst,ES_UINT64 ft);
static BOOL _es_check_option_utf8_string_name(const wchar_t *argv,const ES_UTF8 *s);
static BOOL _es_flush_export_buffer(void);
static void _es_output_cell_csv_wchar_string(const wchar_t *s,int is_highlighted);
static void _es_output_cell_csv_wchar_string_with_optional_quotes(int is_always_double_quote,int separator_ch,const wchar_t *s,int is_highlighted);
static void _es_get_command_argv(wchar_buf_t *wcbuf);
static void _es_expect_command_argv(wchar_buf_t *wcbuf);
static void _es_get_argv(wchar_buf_t *wcbuf);
static void _es_expect_argv(wchar_buf_t *wcbuf);
static BOOL _es_is_valid_key(INPUT_RECORD *ir);
static BOOL _es_save_settings_with_filename(const wchar_t *filename);
static BOOL _es_save_settings_with_appdata(int is_appdata);
static BOOL _es_save_settings(void);
static void _es_load_settings(void);
static void _es_append_filter(wchar_buf_t *wcbuf,const ES_UTF8 *filter);
static void _es_do_run_history_command(void);
static BOOL _es_check_sorts(const wchar_t *argv);
static void _es_wait_for_db_loaded(void);
static void _es_wait_for_db_not_busy(void);
static BOOL _es_is_literal_switch(const wchar_t *s);
static BOOL _es_should_quote(int separator_ch,const wchar_t *s);
static BOOL _es_is_unbalanced_quotes(const wchar_t *s);
static BYTE *_es_copy_dword(BYTE *buf,DWORD value);
static BYTE *_es_copy_uint64(BYTE *buf,ES_UINT64 value);
static BYTE *_es_copy_size_t(BYTE *buf,SIZE_T value);
static BOOL _es_check_color_param(const wchar_t *argv,DWORD *out_property_id);
static BOOL _es_check_column_param(const wchar_t *argv);
static int _es_get_ipc_sort_type_from_property_id(DWORD property_id,int sort_ascending);
static void _es_get_window_classname(wchar_buf_t *wcbuf);
static void _es_get_folder_size(const wchar_t *filename);
static void _es_get_reply_window(void);
static BOOL _es_load_settings_with_filename(const wchar_t *filename);
static BOOL _es_load_settings_with_appdata(int is_appdata);
static void _es_get_nice_property_name(DWORD property_id,wchar_buf_t *out_wcbuf);
static void _es_get_nice_json_property_name(DWORD property_id,wchar_buf_t *out_wcbuf);
static void _es_escape_json_wchar_string(const wchar_t *s,wchar_buf_t *out_wcbuf);
static BOOL _es_set_sort_list(const wchar_t *sort_list,int allow_old_column_ids);
static BOOL _es_set_columns(const wchar_t *sort_list,int action,int allow_old_column_ids);
static BOOL _es_set_column_colors(const wchar_t *column_color_list,int action);
static BOOL _es_set_column_widths(const wchar_t *column_width_list,int action);
static SIZE_T __es_highlighted_wchar_string_get_length_in_wchars(const wchar_t *s);
static void _es_add_standard_efu_columns(int size,int date_modified,int date_created,int attributes,int allow_reorder);
static column_t *_es_find_last_standard_efu_column(void);
static BOOL _es_ipc2_is_file_info_indexed(DWORD file_info_type);
static void _es_output_pause(DWORD ipc_version,const void *data);
static void _es_output_noncell_total_size(ES_UINT64 total_size);
static void _es_output_noncell_result_count(ES_UINT64 result_count);

static DWORD _es_primary_sort_property_id = EVERYTHING3_PROPERTY_ID_NAME;
static char _es_primary_sort_ascending = 0; // 0 = default, >0 = ascending, <0 = descending
static const EVERYTHING_IPC_LIST *_es_sort_list;
static BOOL (WINAPI *_es_pChangeWindowMessageFilterEx)(HWND hWnd,UINT message,DWORD action,_ES_CHANGEFILTERSTRUCT *pChangeFilterStruct) = 0;
static int _es_highlight_color = FOREGROUND_GREEN|FOREGROUND_INTENSITY;
static char _es_highlight = 0;
static char _es_match_whole_word = 0;
static char _es_match_path = 0;
static char _es_match_case = 0;
static char _es_match_diacritics = 0;
static char _es_match_prefix = 0;
static char _es_match_suffix = 0;
static char _es_ignore_whitespace = 0;
static char _es_ignore_punctuation = 0;
static char _es_exit_everything = 0;
static char es_reindex = 0;
static char es_save_db = 0;
static BYTE _es_export_type = ES_EXPORT_TYPE_NONE;
static HANDLE _es_export_file = INVALID_HANDLE_VALUE;
static BYTE *_es_export_buf = 0;
static BYTE *_es_export_p;
static DWORD _es_export_avail = 0;
static char es_size_leading_zero = 0; // depreciated.
static char es_run_count_leading_zero = 0; // depreciated
static char es_digit_grouping = 1;
static SIZE_T es_offset = 0;
static SIZE_T es_max_results = SIZE_MAX;
static DWORD es_ret = ES_ERROR_SUCCESS;
static const wchar_t *es_command_line = 0;
static BYTE es_size_format = 1; // 0 = auto, 1=bytes, 2=kb
static BYTE es_date_format = 0; // display/export (set on query) date/time format 0 = system format, 1=iso-8601 (as local time), 2=filetime in decimal, 3=iso-8601 (in utc)
static BYTE es_display_date_format = 0; // display date/time format 0 = system format, 1=iso-8601 (as local time), 2=filetime in decimal, 3=iso-8601 (in utc)
static BYTE _es_export_date_format = 0; // export date/time format 0 = default, 1=iso-8601 (as local time), 2=filetime in decimal, 3=iso-8601 (in utc)
static CHAR_INFO *es_output_cibuf = 0;
static int es_output_cibuf_hscroll = 0;
static WORD es_output_color = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
static int es_output_cibuf_x = 0;
static int es_output_cibuf_y = 0;
static int es_max_wide = 0;
static int es_console_wide = 80;
static int es_console_high = 25;
static int es_console_size_high = 25;
static int es_console_window_x = 0;
static int es_console_window_y = 0;
static char es_pause = 0; 
static char es_empty_search_help = 0;
static char es_hide_empty_search_results = 0;
static char es_save = 0;
static HWND es_everything_hwnd = 0;
static DWORD es_timeout = 0;
static char es_get_result_count = 0; // 1 = show result count only
static char es_get_total_size = 0; // 1 = calculate total result size, only display this total size and exit.
static char es_no_result_error = 0; // 1 = set errorlevel if no results found.
static HANDLE es_output_handle = 0;
static UINT es_cp = 0; // current code page
static char es_output_is_char = 0; // default to file, unless we can get the console mode.
static WORD es_default_attributes = 0x07;
static void *es_run_history_data = 0; // run count command
static DWORD es_run_history_count = 0; // run count command
static int es_run_history_command = 0;
static SIZE_T es_run_history_size = 0;
static DWORD es_ipc_version = 0xffffffff; // allow all ipc versions
static char es_header = 0; // 0 == resolve based on export type; >0 == show; <0 == hide.
static char es_double_quote = 0; // always use double quotes for filenames.
static char es_csv_double_quote = 1; // always use double quotes for CSV for consistancy.
static char es_get_everything_version = 0;
static char es_utf8_bom = 0;
static wchar_buf_t *es_search_wcbuf = NULL;
static HWND es_reply_hwnd = 0;
static char es_loaded_appdata_ini = 0; // loaded settings from appdata, we should save to appdata.
static column_t *es_output_column = NULL; // current output column
static SIZE_T _es_output_cell_overflow = 0;
static char es_is_in_header = 0;
static char es_no_default_filename_column = 0;
static char es_no_default_size_column = 0; // EFU only
static char es_no_default_date_modified_column = 0; // EFU only
static char es_no_default_date_created_column = 0; // EFU only
static char es_no_default_attribute_column = 0; // EFU only
static char es_folder_append_path_separator = 1;
static char es_newline_type = 0; // 0==CRLF, 1==LF, 2==NUL

wchar_buf_t *es_instance_name_wcbuf = NULL;

// load unicode for windows 95/98
HMODULE LoadUnicowsProc(void);

extern FARPROC _PfnLoadUnicows = (FARPROC)&LoadUnicowsProc;

HMODULE LoadUnicowsProc(void)
{
	OSVERSIONINFOA osvi;
	
	// make sure we are win9x.
	// to prevent loading unicows.dll on NT as a securiry precausion.
	osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOA);
	if (GetVersionExA(&osvi))
	{
		if (osvi.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS)
		{
			return LoadLibraryA("unicows.dll");
		}
	}
	
	return NULL;
}

// query everything with search string
static BOOL _es_ipc1_query(void)
{
	EVERYTHING_IPC_QUERY *query;
	SIZE_T search_length_in_wchars;
	SIZE_T size;
	BOOL ret;
	utf8_buf_t cbuf;
	
	ret = FALSE;
	utf8_buf_init(&cbuf);
	
	_es_get_reply_window();
	
	search_length_in_wchars = es_search_wcbuf->length_in_wchars;
	
	// EVERYTHING_IPC_QUERY includes the NULL terminator.
	size = 	sizeof(EVERYTHING_IPC_QUERY);
	size = safe_size_add(size,safe_size_mul_sizeof_wchar(search_length_in_wchars));
	
	utf8_buf_grow_size(&cbuf,size);
	query = (EVERYTHING_IPC_QUERY *)cbuf.buf;

	if (es_max_results <= ES_DWORD_MAX)
	{
		query->max_results = (DWORD)es_max_results;
	}
	else
	{
		query->max_results = ES_DWORD_MAX;
	}

	query->offset = 0;
	query->reply_copydata_message = ES_COPYDATA_IPCTEST_QUERYCOMPLETEW;
	query->search_flags = (_es_match_case?EVERYTHING_IPC_MATCHCASE:0) | (_es_match_diacritics?EVERYTHING_IPC_MATCHACCENTS:0) | (_es_match_whole_word?EVERYTHING_IPC_MATCHWHOLEWORD:0) | (_es_match_path?EVERYTHING_IPC_MATCHPATH:0);
	query->reply_hwnd = (DWORD)(uintptr_t)es_reply_hwnd;
	os_copy_memory(query->search_string,es_search_wcbuf->buf,(search_length_in_wchars+1)*sizeof(WCHAR));

	// it's not fatal if this is too large.
	// a different IPC method might succeed..
	if (size <= ES_DWORD_MAX)
	{
		COPYDATASTRUCT cds;
		
		cds.cbData = (DWORD)size;
		cds.dwData = EVERYTHING_IPC_COPYDATAQUERY;
		cds.lpData = query;

		if (SendMessage(es_everything_hwnd,WM_COPYDATA,(WPARAM)es_reply_hwnd,(LPARAM)&cds) == TRUE)
		{
			// we are committed to ipc1
			// if we are exporting as EFU, make sure the attribute column is shown.
			// but don't allow reordering.
			if (_es_export_type == ES_EXPORT_TYPE_EFU)
			{
				_es_add_standard_efu_columns(0,0,0,1,0);
			}

			ret = TRUE;
		}
	}

	utf8_buf_kill(&cbuf);

	return ret;
}

// query everything with search string over IPC version2
static BOOL _es_ipc2_query(void)
{
	EVERYTHING_IPC_QUERY2 *query;
	SIZE_T search_length_in_wchars;
	SIZE_T size;
	COPYDATASTRUCT cds;
	BOOL ret;
	DWORD request_flags;
	utf8_buf_t cbuf;
	
	ret = FALSE;
	utf8_buf_init(&cbuf);

	_es_get_reply_window();
	
	search_length_in_wchars = es_search_wcbuf->length_in_wchars;
	
	size = sizeof(EVERYTHING_IPC_QUERY2);
	size = safe_size_add(size,safe_size_mul_sizeof_wchar(safe_size_add_one(search_length_in_wchars)));

	utf8_buf_grow_size(&cbuf,size);
	query = (EVERYTHING_IPC_QUERY2 *)cbuf.buf;

	if (_es_export_type == ES_EXPORT_TYPE_EFU)
	{
		int is_size_indexed;
		int is_date_modified_indexed;
		int is_date_created_indexed;
		int is_attributes_indexed;
		
		is_size_indexed = _es_ipc2_is_file_info_indexed(EVERYTHING_IPC_FILE_INFO_FILE_SIZE);
		is_date_modified_indexed = _es_ipc2_is_file_info_indexed(EVERYTHING_IPC_FILE_INFO_DATE_MODIFIED);
		is_date_created_indexed = _es_ipc2_is_file_info_indexed(EVERYTHING_IPC_FILE_INFO_DATE_CREATED);
		is_attributes_indexed = _es_ipc2_is_file_info_indexed(EVERYTHING_IPC_FILE_INFO_ATTRIBUTES);

		// this modifies columns for ALL ipc requests.
		// however, we only modify columns if we successfully retrieve an is-property-index request.
		// so this should be fine.
		_es_add_standard_efu_columns(is_size_indexed,is_date_modified_indexed,is_date_created_indexed,is_attributes_indexed,1);
	}

	request_flags = 0;
	
	{
		column_t *column;

		column = column_order_start;
		
		while(column)
		{
			switch(column->property_id)
			{
				case EVERYTHING3_PROPERTY_ID_PATH_AND_NAME:
					if (_es_highlight)
					{
						request_flags |= EVERYTHING_IPC_QUERY2_REQUEST_HIGHLIGHTED_FULL_PATH_AND_NAME;
					}
					else
					{
						request_flags |= EVERYTHING_IPC_QUERY2_REQUEST_FULL_PATH_AND_NAME;
					}
					break;
					
				case EVERYTHING3_PROPERTY_ID_NAME:
					if (_es_highlight)
					{
						request_flags |= EVERYTHING_IPC_QUERY2_REQUEST_HIGHLIGHTED_NAME;
					}
					else
					{
						request_flags |= EVERYTHING_IPC_QUERY2_REQUEST_NAME;
					}
					break;
					
				case EVERYTHING3_PROPERTY_ID_PATH:
					if (_es_highlight)
					{
						request_flags |= EVERYTHING_IPC_QUERY2_REQUEST_HIGHLIGHTED_PATH;
					}
					else
					{
						request_flags |= EVERYTHING_IPC_QUERY2_REQUEST_PATH;
					}
					break;
					
				case EVERYTHING3_PROPERTY_ID_EXTENSION:
					request_flags |= EVERYTHING_IPC_QUERY2_REQUEST_EXTENSION;
					break;

				case EVERYTHING3_PROPERTY_ID_SIZE:
					request_flags |= EVERYTHING_IPC_QUERY2_REQUEST_SIZE;
					break;

				case EVERYTHING3_PROPERTY_ID_DATE_CREATED:
					request_flags |= EVERYTHING_IPC_QUERY2_REQUEST_DATE_CREATED;
					break;

				case EVERYTHING3_PROPERTY_ID_DATE_MODIFIED:
					request_flags |= EVERYTHING_IPC_QUERY2_REQUEST_DATE_MODIFIED;
					break;

				case EVERYTHING3_PROPERTY_ID_DATE_ACCESSED:
					request_flags |= EVERYTHING_IPC_QUERY2_REQUEST_DATE_ACCESSED;
					break;

				case EVERYTHING3_PROPERTY_ID_ATTRIBUTES:
					request_flags |= EVERYTHING_IPC_QUERY2_REQUEST_ATTRIBUTES;
					break;

				case EVERYTHING3_PROPERTY_ID_FILE_LIST_NAME:
					request_flags |= EVERYTHING_IPC_QUERY2_REQUEST_FILE_LIST_FILE_NAME;
					break;

				case EVERYTHING3_PROPERTY_ID_RUN_COUNT:
					request_flags |= EVERYTHING_IPC_QUERY2_REQUEST_RUN_COUNT;
					break;

				case EVERYTHING3_PROPERTY_ID_DATE_RUN:
					request_flags |= EVERYTHING_IPC_QUERY2_REQUEST_DATE_RUN;
					break;

				case EVERYTHING3_PROPERTY_ID_DATE_RECENTLY_CHANGED:
					request_flags |= EVERYTHING_IPC_QUERY2_REQUEST_DATE_RECENTLY_CHANGED;
					break;
			}
			
			column = column->order_next;
		}
	}
	
	if (es_get_result_count)
	{
		request_flags = 0;
	}
	
	if (es_get_total_size)
	{
		// we only want size.
		request_flags = EVERYTHING_IPC_QUERY2_REQUEST_SIZE;
	}
	
	if (es_max_results <= ES_DWORD_MAX)
	{
		query->max_results = (DWORD)es_max_results;
	}
	else
	{
		query->max_results = ES_DWORD_MAX;
	}

	if (es_offset <= ES_DWORD_MAX)
	{
		query->offset = (DWORD)es_offset;
	}
	else
	{
		query->offset = ES_DWORD_MAX;
	}

	query->reply_copydata_message = ES_COPYDATA_IPCTEST_QUERYCOMPLETE2W;
	query->search_flags = (_es_match_case?EVERYTHING_IPC_MATCHCASE:0) | (_es_match_diacritics?EVERYTHING_IPC_MATCHACCENTS:0) | (_es_match_whole_word?EVERYTHING_IPC_MATCHWHOLEWORD:0) | (_es_match_path?EVERYTHING_IPC_MATCHPATH:0);
	query->reply_hwnd = (DWORD)(uintptr_t)es_reply_hwnd;
	query->request_flags = request_flags;
	query->sort_type = _es_get_ipc_sort_type_from_property_id(_es_primary_sort_property_id,_es_primary_sort_ascending);
	os_copy_memory(query+1,es_search_wcbuf->buf,(search_length_in_wchars + 1) * sizeof(WCHAR));

	// it's not fatal if this is too large.
	// a different IPC method might succeed..
	if (size <= ES_DWORD_MAX)
	{
		cds.cbData = (DWORD)size;
		cds.dwData = EVERYTHING_IPC_COPYDATA_QUERY2;
		cds.lpData = query;

		if (SendMessage(es_everything_hwnd,WM_COPYDATA,(WPARAM)es_reply_hwnd,(LPARAM)&cds) == TRUE)
		{
			// we are committed to ipc2
			// if we are exporting as EFU, make sure the attribute column is shown.
			// but don't allow reordering.
			if (_es_export_type == ES_EXPORT_TYPE_EFU)
			{
				_es_add_standard_efu_columns(0,0,0,1,0);
			}

			ret = TRUE;
		}
	}

	utf8_buf_kill(&cbuf);

	return ret;
}

static BOOL _es_ipc3_query(void)
{
	BOOL ret;
	HANDLE pipe_handle;
	
	ret = FALSE;

	pipe_handle = ipc3_connect_pipe();
	if (pipe_handle != INVALID_HANDLE_VALUE)
	{
		DWORD search_flags;
		utf8_buf_t search_cbuf;
		utf8_buf_t packet_cbuf;
		SIZE_T packet_size;
		SIZE_T search_sort_count;
		SIZE_T search_property_request_count;

		utf8_buf_init(&search_cbuf);
		utf8_buf_init(&packet_cbuf);
	
		utf8_buf_copy_wchar_string(&search_cbuf,es_search_wcbuf->buf);

#if SIZE_MAX == 0xFFFFFFFFFFFFFFFFUI64

		search_flags = IPC3_SEARCH_FLAG_64BIT;

#elif SIZE_MAX == 0xFFFFFFFF

		search_flags = 0;
	
#else
		#error unknown SIZE_MAX
#endif
		
		if (_es_match_case)
		{
			search_flags |= IPC3_SEARCH_FLAG_MATCH_CASE;
		}
		
		if (_es_match_diacritics)
		{
			search_flags |= IPC3_SEARCH_FLAG_MATCH_DIACRITICS;
		}
		
		if (_es_match_whole_word)
		{
			search_flags |= IPC3_SEARCH_FLAG_MATCH_WHOLEWORD;
		}
		
		if (_es_match_path)
		{
			search_flags |= IPC3_SEARCH_FLAG_MATCH_PATH;
		}
		
		if (_es_match_prefix)
		{
			search_flags |= IPC3_SEARCH_FLAG_MATCH_PREFIX;
		}
		
		if (_es_match_suffix)
		{
			search_flags |= IPC3_SEARCH_FLAG_MATCH_SUFFIX;
		}
		
		if (_es_ignore_punctuation)
		{
			search_flags |= IPC3_SEARCH_FLAG_IGNORE_PUNCTUATION;
		}
		
		if (_es_ignore_whitespace)
		{
			search_flags |= IPC3_SEARCH_FLAG_IGNORE_WHITESPACE;
		}

		if (_es_export_type == ES_EXPORT_TYPE_EFU)
		{
			int is_size_indexed;
			int is_date_modified_indexed;
			int is_date_created_indexed;
			int is_attributes_indexed;
			
			is_size_indexed = ipc3_is_property_indexed(pipe_handle,EVERYTHING3_PROPERTY_ID_SIZE);
			is_date_modified_indexed = ipc3_is_property_indexed(pipe_handle,EVERYTHING3_PROPERTY_ID_DATE_MODIFIED);
			is_date_created_indexed = ipc3_is_property_indexed(pipe_handle,EVERYTHING3_PROPERTY_ID_DATE_CREATED);
			is_attributes_indexed = ipc3_is_property_indexed(pipe_handle,EVERYTHING3_PROPERTY_ID_ATTRIBUTES);
			
			// this modifies columns for ALL ipc requests.
			// however, we only modify columns if we successfully retrieve an is-property-index request.
			// so this should be fine.
			_es_add_standard_efu_columns(is_size_indexed,is_date_modified_indexed,is_date_created_indexed,is_attributes_indexed,1);
		}

		if (es_get_total_size)
		{
			search_flags |= IPC3_SEARCH_FLAG_TOTAL_SIZE;

			// we don't need any results.
			es_max_results = 0;
			
			// we don't need any columns
			column_clear_all();
		}
		
		search_sort_count = 1 + secondary_sort_array->count;
		search_property_request_count = column_array->count;
		
		// search_flags
		packet_size = sizeof(DWORD);
		
		// search_len
		packet_size = safe_size_add(packet_size,(SIZE_T)ipc3_copy_len_vlq(NULL,search_cbuf.length_in_bytes));
		
		// search_text
		packet_size = safe_size_add(packet_size,search_cbuf.length_in_bytes);

		// view port offset
		packet_size = safe_size_add(packet_size,sizeof(SIZE_T));
		packet_size = safe_size_add(packet_size,sizeof(SIZE_T));

		// sort
		packet_size = safe_size_add(packet_size,(SIZE_T)ipc3_copy_len_vlq(NULL,search_sort_count));
		packet_size = safe_size_add(packet_size,search_sort_count * sizeof(ipc3_search_sort_t));

		// property request 
		packet_size = safe_size_add(packet_size,(SIZE_T)ipc3_copy_len_vlq(NULL,search_property_request_count));
		packet_size = safe_size_add(packet_size,search_property_request_count * sizeof(ipc3_search_property_request_t));
		
		// allocate packet.
		utf8_buf_grow_size(&packet_cbuf,packet_size);
		
		// write packet.
		{
			BYTE *packet_d;
			
			packet_d = (BYTE *)packet_cbuf.buf;
			
			// search flags
			packet_d = _es_copy_dword(packet_d,search_flags);
			
			// search text
			packet_d = ipc3_copy_len_vlq(packet_d,search_cbuf.length_in_bytes);
			packet_d = os_copy_memory(packet_d,search_cbuf.buf,search_cbuf.length_in_bytes);

			// viewport
			packet_d = _es_copy_size_t(packet_d,0);
			packet_d = _es_copy_size_t(packet_d,es_max_results);

			// primary sort
			
			packet_d = ipc3_copy_len_vlq(packet_d,search_sort_count);

			packet_d = _es_copy_dword(packet_d,_es_primary_sort_property_id);
			packet_d = _es_copy_dword(packet_d,_es_primary_sort_ascending ? 0 : IPC3_SEARCH_SORT_FLAG_DESCENDING);

			// secondary sort.
			
			{
				secondary_sort_t *secondary_sort;
				
				secondary_sort = secondary_sort_start;
				
				while(secondary_sort)
				{
					packet_d = _es_copy_dword(packet_d,secondary_sort->property_id);
					packet_d = _es_copy_dword(packet_d,secondary_sort->ascending ? 0 : IPC3_SEARCH_SORT_FLAG_DESCENDING);
					
					secondary_sort = secondary_sort->next;
				}
			}

			// property requests
			packet_d = ipc3_copy_len_vlq(packet_d,search_property_request_count);

			{
				column_t *column;
				
				column = column_order_start;
				
				while(column)
				{
					DWORD property_request_flags;
					
					property_request_flags = 0;
					
					if (_es_highlight)
					{	
						switch(property_get_format(column->property_id))
						{
							case PROPERTY_FORMAT_TEXT30:
							case PROPERTY_FORMAT_TEXT47:
							case PROPERTY_FORMAT_EXTENSION:
								property_request_flags |= IPC3_SEARCH_PROPERTY_REQUEST_FLAG_HIGHLIGHT;
								break;
						}
					}
					
					packet_d = _es_copy_dword(packet_d,column->property_id);
					packet_d = _es_copy_dword(packet_d,property_request_flags);
					
					column = column->order_next;
				}
			}

			DEBUG_ASSERT((packet_d - (BYTE *)packet_cbuf.buf) == packet_size);
			
			// send the search query packet
			if (ipc3_write_pipe_message(pipe_handle,IPC3_COMMAND_SEARCH,packet_cbuf.buf,packet_size))
			{
				ipc3_stream_pipe_t pipe_stream;
				ipc3_stream_memory_t memory_stream;
				ipc3_result_list_t result_list;
				int got_memory_stream;
				
				got_memory_stream = 0;
				
				// we are committed to ipc3
				// if we are exporting as EFU, make sure the attribute column is shown.
				// but don't allow reordering.
				if (_es_export_type == ES_EXPORT_TYPE_EFU)
				{
					_es_add_standard_efu_columns(0,0,0,1,0);
				}

				// initialize the stream that we will use to read the reply from the pipe.
				// stream will be x86 by default.
				ipc3_stream_pipe_init(&pipe_stream,pipe_handle);
				
				// setup our initial result list from the stream.
				// don't read any items yet.
				ipc3_result_list_init(&result_list,(ipc3_stream_t *)&pipe_stream);
				
				if (es_get_result_count)
				{
					_es_output_noncell_result_count(result_list.folder_result_count + result_list.file_result_count);
				}
				else
				if (es_get_total_size)
				{
					_es_output_noncell_total_size(result_list.total_result_size);
				}
				else
				{
					if (es_pause)
					{
						// setup a memory stream.
						// we read the entire stream into memory as it gets accessed.
						// we store the stream position for each item index so we can quickly jump to a location.
						ipc3_stream_memory_init(&memory_stream,(ipc3_stream_t *)&pipe_stream);
						
						got_memory_stream = 1;
						
						// set the memory stream as the main stream.
						result_list.stream = (ipc3_stream_t *)&memory_stream;
						
						// allocate index to stream offset array.
						// index_to_stream_offset_valid_count will still be zero.
						if (result_list.viewport_count)
						{
							result_list.index_to_stream_offset_array = mem_alloc(safe_size_mul(result_list.viewport_count,sizeof(SIZE_T)));
						}
						
						// output the pause stream.
						_es_output_pause(ES_IPC_VERSION_FLAG_IPC3,&result_list);
					}
					else
					{
						SIZE_T total_item_count;
						
						total_item_count = result_list.viewport_count;
						if (es_header > 0)
						{
							total_item_count = safe_size_add_one(total_item_count);
						}
						
						_es_output_ipc3_results(&result_list,0,total_item_count);
					}
				}

				if (result_list.stream->is_error)
				{
					es_ret = ES_ERROR_IPC_ERROR;
				}
					
				// don't try to process ipc2 or ipc1 if we sent the request successfully.
				ret = TRUE;
				
				if (got_memory_stream)
				{
					ipc3_stream_close((ipc3_stream_t *)&memory_stream);
				}

				ipc3_result_list_kill(&result_list);
				ipc3_stream_close((ipc3_stream_t *)&pipe_stream);
			}
		}
	
		utf8_buf_kill(&packet_cbuf);
		utf8_buf_kill(&search_cbuf);

		CloseHandle(pipe_handle);
	}

	return ret;
}

static int _es_compare_list_items(const EVERYTHING_IPC_ITEM *a,const EVERYTHING_IPC_ITEM *b)
{
	int cmp_ret;

	cmp_ret = CompareString(LOCALE_USER_DEFAULT,NORM_IGNORECASE,EVERYTHING_IPC_ITEMPATH(_es_sort_list,a),-1,EVERYTHING_IPC_ITEMPATH(_es_sort_list,b),-1);
	
//debug_printf("cmp %S %S %d\n",EVERYTHING_IPC_ITEMPATH(_es_sort_list,a),EVERYTHING_IPC_ITEMPATH(_es_sort_list,b),cmp_ret)	;

	if (cmp_ret)
	{
		if (cmp_ret == CSTR_LESS_THAN)
		{
			return -1;
		}
		else
		if (cmp_ret == CSTR_GREATER_THAN)
		{
			return 1;
		}
	}

	return wchar_string_compare(EVERYTHING_IPC_ITEMPATH(_es_sort_list,a),EVERYTHING_IPC_ITEMPATH(_es_sort_list,b));
}

void DECLSPEC_NORETURN es_fatal(int error_code)
{
	const char *msg;
	int show_help;
	
	msg = 0;
	show_help = 0;

	switch(error_code)
	{
		case ES_ERROR_REGISTER_WINDOW_CLASS:
			msg = "Failed to register window class.\r\n";
			break;
			
		case ES_ERROR_CREATE_WINDOW:
			msg = "Failed to create window.\r\n";
			break;
			
		case ES_ERROR_OUT_OF_MEMORY:
			msg = "Out of memory.\r\n";
			break;
			
		case ES_ERROR_EXPECTED_SWITCH_PARAMETER:
			msg = "Expected switch parameter.\r\n";
			// this error is permanent, show help:
			show_help = 1;
			break;
			
		case ES_ERROR_CREATE_FILE:
			msg = "Failed to create output file.\r\n";
			break;
			
		case ES_ERROR_UNKNOWN_SWITCH:
			msg = "Unknown switch.\r\n";
			// this error is permanent, show help:
			show_help = 1;
			break;
			
		case ES_ERROR_IPC_ERROR:
			msg = "Unable to send IPC message or bad IPC reply.\r\n";
			break;
			
		case ES_ERROR_NO_IPC:
			msg = "Everything IPC window not found. Please make sure Everything is running.\r\n";
			break;
			
		case ES_ERROR_NO_RESULTS:
			msg = "No results found.\r\n";
			break;
	}
	
	if (msg)
	{
		os_error_printf("Error %d: %s",error_code,msg);
	}
	
	if (show_help)
	{
		_es_help();
	}

	ExitProcess(error_code);
}

static void _es_console_fill(SIZE_T count,int ascii_ch)
{
	if (es_output_cibuf)
	{
		SIZE_T i;
		
		for(i=0;i<count;i++)
		{
			if ((int)i + es_output_cibuf_x >= es_console_wide)
			{
				break;
			}

			if ((int)i + es_output_cibuf_x >= 0)
			{
				es_output_cibuf[(int)i+es_output_cibuf_x].Attributes = es_output_color;
				es_output_cibuf[(int)i+es_output_cibuf_x].Char.UnicodeChar = ascii_ch;
			}
		}
		
		es_output_cibuf_x += (int)count;
	}
	else
	if (es_output_is_char)
	{
		wchar_buf_t wcbuf;
		wchar_t *d;
		DWORD numwritten;
		SIZE_T i;
		
		wchar_buf_init(&wcbuf);

		wchar_buf_grow_size(&wcbuf,count);

		d = wcbuf.buf;
		
		for(i=0;i<count;i++)
		{
			*d++ = ascii_ch;
		}

		if (count <= ES_DWORD_MAX)
		{
			WriteConsole(es_output_handle,wcbuf.buf,(DWORD)count,&numwritten,0);
		}
		
		wchar_buf_kill(&wcbuf);
	}
	else
	{
		utf8_buf_t cbuf;
		BYTE *d;
		DWORD numwritten;
		SIZE_T i;
		
		utf8_buf_init(&cbuf);

		utf8_buf_grow_size(&cbuf,count);

		d = cbuf.buf;
		
		for(i=0;i<count;i++)
		{
			*d++ = ascii_ch;
		}

		if (count <= ES_DWORD_MAX)
		{
			WriteFile(es_output_handle,cbuf.buf,(DWORD)count,&numwritten,0);
		}
		
		utf8_buf_kill(&cbuf);
	}
}

// get the length in wchars from a wchar string.
static SIZE_T __es_highlighted_wchar_string_get_length_in_wchars(const wchar_t *s)
{
	const wchar_t *p;
	SIZE_T len;
	
	len = 0;
	p = s;
	while(*p)
	{
		if (*p == '*')
		{
			if (p[1] == '*')
			{
				len++;
				p += 2;
				continue;	
			}
			
			p++;
			continue;
		}
		
		len++;
		p++;
	}
	
	return len;
}

// write out an entire cell to the console.
static void _es_output_cell_write_console_wchar_string(const wchar_t *text,int is_highlighted)
{
	SIZE_T length_in_wchars;
	int is_right_aligned;
	SIZE_T column_width;
	column_color_t *column_color;
	int did_set_color;
	
	if (is_highlighted)
	{
		length_in_wchars = __es_highlighted_wchar_string_get_length_in_wchars(text);
	}
	else
	{
		length_in_wchars = wchar_string_get_length_in_wchars(text);
	}
	
	is_right_aligned = property_is_right_aligned(es_output_column->property_id);
	column_width = column_width_get(es_output_column->property_id);
	column_color = column_color_find(es_output_column->property_id);
	did_set_color = 0;
	
	// setup colors
	// pipe? console? cibuf?
	if (es_output_cibuf)
	{
		es_output_color = column_color ? column_color->color : es_default_attributes;
	}
	else
	if (es_output_is_char)
	{
		if (column_color)
		{
			es_output_color = column_color->color;
			SetConsoleTextAttribute(es_output_handle,column_color->color);

			did_set_color = 1;
		}
	}
		
	// don't fill with CSV/TSV/EFU/TXT/M3U
	if (_es_export_type == ES_EXPORT_TYPE_NONE)
	{
		if (es_output_column != column_order_last)
		{
			if (is_right_aligned)
			{
				if (length_in_wchars < column_width)
				{
					SIZE_T fill_length;
					int fill_ch;
					
					fill_length = column_width - length_in_wchars;

					if (_es_output_cell_overflow)
					{
						if (fill_length > _es_output_cell_overflow)
						{
							fill_length -= _es_output_cell_overflow;
							_es_output_cell_overflow = 0;
						}
						else
						{
							_es_output_cell_overflow -= fill_length;
							fill_length = 0;
						}
					}
					
					fill_ch = ' ';
					
					if (!es_digit_grouping)
					{
						if (!es_is_in_header)
						{
							if (es_output_column->property_id == EVERYTHING3_PROPERTY_ID_SIZE)
							{
								if (es_size_leading_zero)
								{
									fill_ch = '0';
								}
							}
							else
							if (es_output_column->property_id == EVERYTHING3_PROPERTY_ID_RUN_COUNT)
							{
								if (es_run_count_leading_zero)
								{
									fill_ch = '0';
								}
							}
						}
					}
					
					_es_console_fill(fill_length,fill_ch);	
				}
			}
		}
	}

	// pipe? console? cibuf?
	if (es_output_cibuf)
	{
		if (is_highlighted)
		{
			const wchar_t *p;
			int is_in_highlight;
			SIZE_T cibuf_offset;
			
			p = text;
			is_in_highlight = 0;
			cibuf_offset = 0;
			
			while(*p)
			{
				const wchar_t *start;
				SIZE_T wlen;
				int is_highlight_change;

				if ((int)cibuf_offset + es_output_cibuf_x >= es_console_wide)
				{
					break;
				}
				
				start = p;
				
				is_highlight_change = 0;
				
				for(;;)
				{
					if (!*p)
					{
						wlen = p - start;
						break;
					}
					
					if (*p == '*')
					{
						if (p[1] == '*')
						{
							wlen = p + 1 - start;
							p += 2;
							break;
						}
						
						is_highlight_change = 1;
						wlen = p - start;
						p++;
						break;
					}
					
					p++;
				}

				{
					SIZE_T i;
					
					for(i=0;i<wlen;i++)
					{
						if ((int)i + (int)cibuf_offset + es_output_cibuf_x >= es_console_wide)
						{
							break;
						}

						if ((int)i + (int)cibuf_offset + es_output_cibuf_x >= 0)
						{
							es_output_cibuf[(int)i+(int)cibuf_offset+es_output_cibuf_x].Attributes = is_in_highlight ? _es_highlight_color : es_output_color;
							es_output_cibuf[(int)i+(int)cibuf_offset+es_output_cibuf_x].Char.UnicodeChar = start[i];
						}
					}
					
					cibuf_offset += wlen;
				}

				if (is_highlight_change)
				{
					is_in_highlight = !is_in_highlight;
				}
			}
		}
		else
		{
			SIZE_T i;
			
			for(i=0;i<length_in_wchars;i++)
			{
				if ((int)i + es_output_cibuf_x >= es_console_wide)
				{
					break;
				}

				if ((int)i + es_output_cibuf_x >= 0)
				{
					es_output_cibuf[(int)i+es_output_cibuf_x].Attributes = es_output_color;
					es_output_cibuf[(int)i+es_output_cibuf_x].Char.UnicodeChar = text[i];
				}
			}
		}
		
		es_output_cibuf_x += (int)length_in_wchars;
	}
	else
	if (es_output_is_char)
	{
		if (length_in_wchars <= ES_DWORD_MAX)
		{
			if (is_highlighted)
			{
				const wchar_t *p;
				int is_in_highlight;
				
				p = text;
				is_in_highlight = 0;
				
				while(*p)
				{
					const wchar_t *start;
					SIZE_T wlen;
					int is_highlight_change;
					
					start = p;
					
					is_highlight_change = 0;
					
					for(;;)
					{
						if (!*p)
						{
							wlen = p - start;
							break;
						}
						
						if (*p == '*')
						{
							if (p[1] == '*')
							{
								wlen = p + 1 - start;
								p += 2;
								break;
							}
							
							is_highlight_change = 1;
							wlen = p - start;
							p++;
							break;
						}
						
						p++;
					}

					SetConsoleTextAttribute(es_output_handle,is_in_highlight ? _es_highlight_color : es_output_color);
		
					if (wlen <= ES_DWORD_MAX)
					{
						DWORD numwritten;
						
						WriteConsole(es_output_handle,start,(DWORD)wlen,&numwritten,0);
					}

					if (is_highlight_change)
					{
						is_in_highlight = !is_in_highlight;
					}
				}
			}
			else
			{
				DWORD numwritten;
			
				WriteConsole(es_output_handle,text,(DWORD)length_in_wchars,&numwritten,0);
			}
		}
	}
	else
	{
		if (length_in_wchars <= INT_MAX)
		{
			int len;
			
			len = WideCharToMultiByte(es_cp,0,text,(int)length_in_wchars,0,0,0,0);
			if (len)
			{
				DWORD numwritten;
				utf8_buf_t cbuf;

				utf8_buf_init(&cbuf);

				utf8_buf_grow_size(&cbuf,len);

				WideCharToMultiByte(es_cp,0,text,(int)length_in_wchars,cbuf.buf,len,0,0);
				
				WriteFile(es_output_handle,cbuf.buf,len,&numwritten,0);
				
				utf8_buf_kill(&cbuf);
			}
		}	
	}

	// don't fill with CSV/TSV/EFU/TXT/M3U
	if (_es_export_type == ES_EXPORT_TYPE_NONE)
	{
		if (es_output_column != column_order_last)
		{
			if (!is_right_aligned)
			{
				if (length_in_wchars < column_width)
				{
					SIZE_T fill_length;
					
					fill_length = column_width - length_in_wchars;

					if (_es_output_cell_overflow)
					{
						if (fill_length > _es_output_cell_overflow)
						{
							fill_length -= _es_output_cell_overflow;
							_es_output_cell_overflow = 0;
						}
						else
						{
							_es_output_cell_overflow -= fill_length;
							fill_length = 0;
						}
					}
					
					_es_console_fill(fill_length,' ');
				}
			}
		}
	
		if (length_in_wchars > column_width)
		{
			_es_output_cell_overflow += length_in_wchars - column_width;
		}
	}
	
	// restore color
	if (did_set_color)
	{
		SetConsoleTextAttribute(es_output_handle,es_default_attributes);
	}
}

// write to the export buffer.
static void _es_export_write_data(const BYTE *data,SIZE_T length_in_bytes)
{
	const BYTE *p;
	SIZE_T run;
	
	p = data;
	run = length_in_bytes;
	
	while(run)
	{
		DWORD copy_size;
		
		if (!_es_export_avail)
		{
			if (!_es_flush_export_buffer())
			{
				break;
			}
		}
		
		if (run <= _es_export_avail)
		{
			copy_size = (DWORD)run;
		}
		else
		{
			copy_size = _es_export_avail;
		}

		os_copy_memory(_es_export_p,p,copy_size);
		_es_export_avail -= copy_size;
		_es_export_p += copy_size;
		
		p += copy_size;
		run -= copy_size;
	}
}

// write a wchar string to the export buffer.
static void _es_export_write_wchar_string_n(const wchar_t *text,SIZE_T wlen)
{
	if (wlen <= ES_DWORD_MAX)
	{
		int len;
		int cp;
		utf8_buf_t cbuf;
		
		cp = CP_UTF8;
		utf8_buf_init(&cbuf);

		if (_es_export_type == ES_EXPORT_TYPE_M3U)		
		{
			cp = CP_ACP;
		}
		
		len = WideCharToMultiByte(cp,0,text,(DWORD)wlen,0,0,0,0);
		
		utf8_buf_grow_size(&cbuf,len);

		len = WideCharToMultiByte(cp,0,text,(DWORD)wlen,cbuf.buf,len,0,0);
		
		if (wlen < 0)
		{
			// remove null from len.
			if (len) 
			{
				len--;
			}
		}
		
		_es_export_write_data(cbuf.buf,len);

		utf8_buf_kill(&cbuf);
	}
}

// write out a wchar string to an entire cell.
static void _es_output_cell_wchar_string(const wchar_t *text,int is_highlighted)
{
	if (_es_export_file != INVALID_HANDLE_VALUE)
	{
		_es_export_write_wchar_string_n(text,wchar_string_get_length_in_wchars(text));
	}
	else
	{
		_es_output_cell_write_console_wchar_string(text,is_highlighted);
	}
}

// write out a UTF-8 string to an entire cell.
static void _es_output_cell_utf8_string(const ES_UTF8 *text,int is_highlighted)
{
	wchar_buf_t wcbuf;

	wchar_buf_init(&wcbuf);

	wchar_buf_copy_utf8_string(&wcbuf,text);
	
	if (wcbuf.length_in_wchars <= INT_MAX)
	{
		_es_output_cell_wchar_string(wcbuf.buf,is_highlighted);
	}

	wchar_buf_kill(&wcbuf);
}

// write out a CSV wchar string to an entire cell.
static void _es_output_cell_csv_wchar_string(const wchar_t *s,int is_highlighted)
{
	wchar_buf_t wcbuf;
	const wchar_t *start;
	const wchar_t *p;
	
	wchar_buf_init(&wcbuf);

	wchar_buf_cat_wchar(&wcbuf,'"');
	
	start = s;
	p = s;
	
	while(*p)
	{
		if (*p == '"')
		{
			wchar_buf_cat_wchar_string_n(&wcbuf,start,p-start);

			// escape double quotes with double double quotes.
			wchar_buf_cat_wchar(&wcbuf,'"');
			wchar_buf_cat_wchar(&wcbuf,'"');
			
			start = p + 1;
		}
		
		p++;
	}

	wchar_buf_cat_wchar_string_n(&wcbuf,start,p-start);

	wchar_buf_cat_wchar(&wcbuf,'"');
	
	_es_output_cell_wchar_string(wcbuf.buf,is_highlighted);

	wchar_buf_kill(&wcbuf);
}

// should a TSV/CSV string value be quoted.
static BOOL _es_should_quote(int separator_ch,const wchar_t *s)
{
	const wchar_t *p;
	
	p = s;
	
	while(*p)
	{
		if ((*p == separator_ch) || (*p == '"') || (*p == '\r') || (*p == '\n'))
		{
			return TRUE;
		}
		
		p++;
	}
	
	return FALSE;
}

// write out a CSV wchar string to an entire cell.
// same as _es_output_cell_csv_wchar_string.
// but this version will only use double quotes if the text contains a separator or double quotes.
static void _es_output_cell_csv_wchar_string_with_optional_quotes(int is_always_double_quote,int separator_ch,const wchar_t *s,int is_highlighted)
{
	if (!is_always_double_quote)
	{
		if (!_es_should_quote(separator_ch,s))
		{
			// no quotes required..
			_es_output_cell_wchar_string(s,is_highlighted);
			
			return;
		}
	}

	// write with quotes..
	_es_output_cell_csv_wchar_string(s,is_highlighted);
}

// flush any unwritten data in the export buffer to disk.
static BOOL _es_flush_export_buffer(void)
{
	BOOL ret;
	
	ret = FALSE;
	
	if (_es_export_file != INVALID_HANDLE_VALUE)
	{
		if (_es_export_avail != ES_EXPORT_BUF_SIZE)
		{
			DWORD numwritten;
			
			if (WriteFile(_es_export_file,_es_export_buf,ES_EXPORT_BUF_SIZE - _es_export_avail,&numwritten,0))
			{
				if (ES_EXPORT_BUF_SIZE - _es_export_avail == numwritten)
				{
					ret = TRUE;
				}
			}
			
			_es_export_p = _es_export_buf;
			_es_export_avail = ES_EXPORT_BUF_SIZE;
		}	
	}
	
	return ret;
}

static void _es_output_cell_text_property_wchar_string(const wchar_t *value)
{
	if ((_es_export_type == ES_EXPORT_TYPE_CSV) || (_es_export_type == ES_EXPORT_TYPE_TSV))
	{
		_es_output_cell_csv_wchar_string_with_optional_quotes((_es_export_type == ES_EXPORT_TYPE_CSV) ? es_csv_double_quote : es_double_quote,(_es_export_type == ES_EXPORT_TYPE_CSV) ? ',' : '\t',value,0);
	}
	else
	if (_es_export_type == ES_EXPORT_TYPE_EFU)
	{
		// always double quote.
		_es_output_cell_csv_wchar_string(value,0);
	}
	else
	if (_es_export_type == ES_EXPORT_TYPE_JSON)
	{
		// always double quote.
		wchar_buf_t property_name_wcbuf;
		wchar_buf_t json_string_wcbuf;
		
		wchar_buf_init(&property_name_wcbuf);
		wchar_buf_init(&json_string_wcbuf);

		_es_get_nice_json_property_name(es_output_column->property_id,&property_name_wcbuf);
		_es_escape_json_wchar_string(value,&json_string_wcbuf);
		
		_es_output_cell_printf(0,"\"%S\":\"%S\"",property_name_wcbuf.buf,json_string_wcbuf.buf);

		wchar_buf_kill(&json_string_wcbuf);
		wchar_buf_kill(&property_name_wcbuf);
	}
	else
	{
		if (es_double_quote)
		{
			wchar_buf_t wcbuf;

			wchar_buf_init(&wcbuf);

			wchar_buf_printf(&wcbuf,"\"%S\"",value);

			_es_output_cell_wchar_string(wcbuf.buf,0);

			wchar_buf_kill(&wcbuf);
		}
		else
		{
			_es_output_cell_wchar_string(value,0);
		}
	}
}							

static void _es_output_cell_text_property_utf8_string_n(const ES_UTF8 *value,SIZE_T length_in_bytes)
{
	wchar_buf_t wcbuf;

	wchar_buf_init(&wcbuf);

	wchar_buf_copy_utf8_string_n(&wcbuf,value,length_in_bytes);

	_es_output_cell_text_property_wchar_string(wcbuf.buf);

	wchar_buf_kill(&wcbuf);
}							

static void _es_output_cell_highlighted_text_property_wchar_string(const wchar_t *value)
{
	if ((_es_export_type == ES_EXPORT_TYPE_CSV) || (_es_export_type == ES_EXPORT_TYPE_TSV))
	{
		_es_output_cell_csv_wchar_string_with_optional_quotes((_es_export_type == ES_EXPORT_TYPE_CSV) ? es_csv_double_quote : es_double_quote,(_es_export_type == ES_EXPORT_TYPE_CSV) ? ',' : '\t',value,1);
	}
	else
	if (_es_export_type == ES_EXPORT_TYPE_EFU)
	{
		// always double quote.
		_es_output_cell_csv_wchar_string(value,1);
	}
	else
	if (_es_export_type == ES_EXPORT_TYPE_JSON)
	{
		// always double quote.
		wchar_buf_t property_name_wcbuf;
		wchar_buf_t json_string_wcbuf;
		
		wchar_buf_init(&property_name_wcbuf);
		wchar_buf_init(&json_string_wcbuf);

		_es_get_nice_json_property_name(es_output_column->property_id,&property_name_wcbuf);
		_es_escape_json_wchar_string(value,&json_string_wcbuf);
		
		_es_output_cell_printf(1,"\"%S\":\"%S\"",property_name_wcbuf.buf,json_string_wcbuf.buf);

		wchar_buf_kill(&json_string_wcbuf);
		wchar_buf_kill(&property_name_wcbuf);
	}
	else
	{
		if (es_double_quote)
		{
			wchar_buf_t wcbuf;

			wchar_buf_init(&wcbuf);

			wchar_buf_printf(&wcbuf,"\"%S\"",value);

			_es_output_cell_wchar_string(wcbuf.buf,1);

			wchar_buf_kill(&wcbuf);
		}
		else
		{
			_es_output_cell_wchar_string(value,1);
		}
	}
}

static void _es_output_cell_highlighted_text_property_utf8_string(const ES_UTF8 *value)
{
	wchar_buf_t wcbuf;

	wchar_buf_init(&wcbuf);

	wchar_buf_copy_utf8_string(&wcbuf,value);

	_es_output_cell_highlighted_text_property_wchar_string(wcbuf.buf);

	wchar_buf_kill(&wcbuf);
}

static void _es_output_cell_unknown_property(void)
{	
	if (_es_export_type == ES_EXPORT_TYPE_JSON)
	{
		wchar_buf_t property_name_wcbuf;
		
		wchar_buf_init(&property_name_wcbuf);

		_es_get_nice_json_property_name(es_output_column->property_id,&property_name_wcbuf);
		
		_es_output_cell_printf(0,"\"%S\":null",property_name_wcbuf.buf);

		wchar_buf_kill(&property_name_wcbuf);
	}
	else
	{
		// this will fill in the column with spaces to the correct column width.
		_es_output_cell_wchar_string(L"",0);
	}
}

static void _es_output_cell_size_property(int is_dir,ES_UINT64 value)
{
	if (_es_export_type == ES_EXPORT_TYPE_JSON)
	{
		wchar_buf_t property_name_wcbuf;
		
		wchar_buf_init(&property_name_wcbuf);

		_es_get_nice_json_property_name(es_output_column->property_id,&property_name_wcbuf);
		
		if (value == ES_UINT64_MAX)
		{
			_es_output_cell_printf(0,"\"%S\":null",property_name_wcbuf.buf);
		}
		else
		{
			_es_output_cell_printf(0,"\"%S\":%I64u",property_name_wcbuf.buf,value);
		}

		wchar_buf_kill(&property_name_wcbuf);
	}
	else
	if (value == ES_UINT64_MAX)
	{
		// unknown size.
		if ((_es_export_type == ES_EXPORT_TYPE_NONE) && (is_dir))
		{
			_es_output_cell_printf(0,"<DIR>");
		}
		else
		{
			// empty.
			// this will fill in the column with spaces to the correct column width.
			_es_output_cell_printf(0,"");
		}
	}
	else
	{
		if (_es_export_type == ES_EXPORT_TYPE_NONE)
		{
			wchar_buf_t wcbuf;

			wchar_buf_init(&wcbuf);

			_es_format_size(value,&wcbuf);
			
			_es_output_cell_wchar_string(wcbuf.buf,0);
			
			wchar_buf_kill(&wcbuf);
		}
		else
		{
			// raw size.
			_es_output_cell_printf(0,"%I64u",value);
		}
	}
}

static void _es_output_cell_filetime_property(ES_UINT64 value)
{
	if (_es_export_type == ES_EXPORT_TYPE_JSON)
	{
		wchar_buf_t property_name_wcbuf;
		wchar_buf_t filetime_wcbuf;
		
		wchar_buf_init(&property_name_wcbuf);
		wchar_buf_init(&filetime_wcbuf);

		_es_get_nice_json_property_name(es_output_column->property_id,&property_name_wcbuf);
		
		if (value == ES_UINT64_MAX)
		{
			_es_output_cell_printf(0,"\"%S\":null",property_name_wcbuf.buf);
		}
		else
		if (es_date_format)
		{
			_es_format_filetime(value,&filetime_wcbuf);
		
			_es_output_cell_printf(0,"\"%S\":\"%S\"",property_name_wcbuf.buf,filetime_wcbuf.buf);
		}
		else
		{
			_es_output_cell_printf(0,"\"%S\":%I64u",property_name_wcbuf.buf,value);
		}

		wchar_buf_kill(&filetime_wcbuf);
		wchar_buf_kill(&property_name_wcbuf);
	}
	else
	if (value == ES_UINT64_MAX)
	{
		// unknown filetime.
		// this will fill in the column with spaces to the correct column width.
		_es_output_cell_printf(0,"");
	}
	else
	if ((_es_export_type == ES_EXPORT_TYPE_NONE) || (es_date_format))
	{
		wchar_buf_t wcbuf;

		// format filetime if we specify a es_date_format
		wchar_buf_init(&wcbuf);

		_es_format_filetime(value,&wcbuf);
		
		_es_output_cell_wchar_string(wcbuf.buf,0);
		
		wchar_buf_kill(&wcbuf);
	}
	else
	{
		// raw filetime.
		_es_output_cell_printf(0,"%I64u",value);
	}
}

static void _es_output_cell_duration_property(ES_UINT64 value)
{
	if (_es_export_type == ES_EXPORT_TYPE_JSON)
	{
		wchar_buf_t property_name_wcbuf;
		
		wchar_buf_init(&property_name_wcbuf);

		_es_get_nice_json_property_name(es_output_column->property_id,&property_name_wcbuf);
		
		if (value == ES_UINT64_MAX)
		{
			_es_output_cell_printf(0,"\"%S\":null",property_name_wcbuf.buf);
		}
		else
		{
			_es_output_cell_printf(0,"\"%S\":%I64u",property_name_wcbuf.buf,value);
		}

		wchar_buf_kill(&property_name_wcbuf);
	}
	else
	if (value == ES_UINT64_MAX)
	{
		// unknown duration
		// this will fill in the column with spaces to the correct column width.
		_es_output_cell_printf(0,"");
	}
	else
	{
		if (_es_export_type == ES_EXPORT_TYPE_NONE)
		{
			wchar_buf_t wcbuf;

			wchar_buf_init(&wcbuf);

			_es_format_duration(value,&wcbuf);
			
			_es_output_cell_wchar_string(wcbuf.buf,0);
			
			wchar_buf_kill(&wcbuf);
		}
		else
		{
			// raw filetime.
			_es_output_cell_printf(0,"%I64u",value);
		}
	}
}

static void _es_output_cell_attribute_property(DWORD file_attributes)
{
	if (_es_export_type == ES_EXPORT_TYPE_JSON)
	{
		wchar_buf_t property_name_wcbuf;
		
		wchar_buf_init(&property_name_wcbuf);

		_es_get_nice_json_property_name(es_output_column->property_id,&property_name_wcbuf);
		
		if (file_attributes == INVALID_FILE_ATTRIBUTES)
		{
			_es_output_cell_printf(0,"\"%S\":null",property_name_wcbuf.buf);
		}
		else
		{
			_es_output_cell_printf(0,"\"%S\":%u",property_name_wcbuf.buf,file_attributes);
		}

		wchar_buf_kill(&property_name_wcbuf);
	}
	else
	if (file_attributes == INVALID_FILE_ATTRIBUTES)
	{
		// empty.
		// this will fill in the column with spaces to the correct column width.
		_es_output_cell_printf(0,"");
	}
	else
	if (_es_export_type == ES_EXPORT_TYPE_NONE)
	{
		wchar_buf_t wcbuf;

		wchar_buf_init(&wcbuf);

		_es_format_attributes(file_attributes,&wcbuf);
		
		_es_output_cell_wchar_string(wcbuf.buf,0);
		
		wchar_buf_kill(&wcbuf);
	}
	else
	{
		// raw filetime.
		_es_output_cell_printf(0,"%u",file_attributes);
	}
	
}

static void _es_output_cell_number_property(ES_UINT64 value,ES_UINT64 empty_value)
{
	if (_es_export_type == ES_EXPORT_TYPE_JSON)
	{
		wchar_buf_t property_name_wcbuf;
		
		wchar_buf_init(&property_name_wcbuf);

		_es_get_nice_json_property_name(es_output_column->property_id,&property_name_wcbuf);
		
		if (value == empty_value)
		{
			_es_output_cell_printf(0,"\"%S\":null",property_name_wcbuf.buf);
		}
		else
		{
			_es_output_cell_printf(0,"\"%S\":%I64u",property_name_wcbuf.buf,value);
		}

		wchar_buf_kill(&property_name_wcbuf);
	}
	else
	if (value == empty_value)
	{
		// empty.
		// this will fill in the column with spaces to the correct column width.
		_es_output_cell_printf(0,"");
	}
	else
	if (_es_export_type == ES_EXPORT_TYPE_NONE)
	{
		wchar_buf_t wcbuf;
		int allow_digit_grouping;

		wchar_buf_init(&wcbuf);

		allow_digit_grouping = 0;
	
		switch(property_get_format(es_output_column->property_id))
		{
			case PROPERTY_FORMAT_NUMBER6:
			case PROPERTY_FORMAT_NUMBER7:
			case PROPERTY_FORMAT_NUMBER:
				allow_digit_grouping = 1;
				break;
		}
				
		_es_format_number(value,allow_digit_grouping,&wcbuf);
		
		_es_output_cell_wchar_string(wcbuf.buf,0);
		
		wchar_buf_kill(&wcbuf);
	}
	else
	{
		// raw filetime.
		_es_output_cell_printf(0,"%I64u",value);
	}
}

static void _es_output_cell_dimensions_property(EVERYTHING3_DIMENSIONS *dimensions_value)
{
	wchar_buf_t wcbuf;

	wchar_buf_init(&wcbuf);

	_es_format_dimensions(dimensions_value,&wcbuf);
	
	if (_es_export_type == ES_EXPORT_TYPE_JSON)
	{
		wchar_buf_t property_name_wcbuf;
		
		wchar_buf_init(&property_name_wcbuf);

		_es_get_nice_json_property_name(es_output_column->property_id,&property_name_wcbuf);
		
		// formatted dimensions doesn't need to be json escaped.
		_es_output_cell_printf(0,"\"%S\":\"%S\"",property_name_wcbuf.buf,wcbuf.buf);

		wchar_buf_kill(&property_name_wcbuf);
	}
	else
	{
		_es_output_cell_wchar_string(wcbuf.buf,0);
	}
	
	wchar_buf_kill(&wcbuf);
}

static void _es_output_cell_data_property(const BYTE *data,SIZE_T size)
{
	wchar_buf_t wcbuf;
	wchar_t *d;
	const BYTE *p;
	SIZE_T run;

	wchar_buf_init(&wcbuf);

	wchar_buf_grow_length(&wcbuf,safe_size_mul_2(size));
	
	p = data;
	d = wcbuf.buf;
	run = size;
	
	while(run)
	{
		*d++ = unicode_hex_char(*p >> 4);
		*d++ = unicode_hex_char(*p & 0x0f);
		
		p++;
		run--;
	}
	
	*d = 0;

	if (_es_export_type == ES_EXPORT_TYPE_JSON)
	{
		wchar_buf_t property_name_wcbuf;
		
		wchar_buf_init(&property_name_wcbuf);

		_es_get_nice_json_property_name(es_output_column->property_id,&property_name_wcbuf);
		
		// hex data text doesn't need to be escaped.
		_es_output_cell_printf(0,"\"%S\":\"%S\"",property_name_wcbuf.buf,wcbuf.buf);

		wchar_buf_kill(&property_name_wcbuf);
	}
	else
	{
		_es_output_cell_wchar_string(wcbuf.buf,0);
	}

	wchar_buf_kill(&wcbuf);
}

static void _es_output_cell_separator(void)
{
	if (es_output_column != column_order_start)
	{
		const ES_UTF8 *separator_text;
	
		if ((_es_export_type == ES_EXPORT_TYPE_CSV) || (_es_export_type == ES_EXPORT_TYPE_TSV))
		{
			separator_text = (_es_export_type == ES_EXPORT_TYPE_CSV) ? "," : "\t";
		}
		else
		if (_es_export_type == ES_EXPORT_TYPE_EFU)
		{
			separator_text = ",";
		}
		else
		if (_es_export_type == ES_EXPORT_TYPE_JSON)
		{
			separator_text = ",";
		}
		else
		{
			separator_text = " ";
		}
		
		_es_output_noncell_utf8_string(separator_text);
	}
}

static void _es_output_noncell_wchar_string(const wchar_t *text)
{
	_es_output_noncell_wchar_string_n(text,wchar_string_get_length_in_wchars(text));
}

static void _es_output_noncell_wchar_string_n(const wchar_t *text,SIZE_T length_in_wchars)
{
	if (_es_export_file != INVALID_HANDLE_VALUE)
	{
		_es_export_write_wchar_string_n(text,length_in_wchars);
	}
	else
	{
		if (es_output_cibuf)
		{
			SIZE_T i;
			SIZE_T ci_x;
			
			ci_x = 0;
		
			for(i=0;i<length_in_wchars;i++)
			{
				if ((int)ci_x + es_output_cibuf_x >= es_console_wide)
				{
					break;
				}
				
				if (text[i] == '\t')
				{
					if ((int)ci_x + es_output_cibuf_x >= 0)
					{
						es_output_cibuf[(int)ci_x+es_output_cibuf_x].Attributes = es_default_attributes;
						es_output_cibuf[(int)ci_x+es_output_cibuf_x].Char.UnicodeChar = ' ';
					}

					ci_x++;
					
					while (((int)ci_x + es_output_cibuf_x + es_output_cibuf_hscroll) & 7)
					{
						if ((int)ci_x + es_output_cibuf_x < es_console_wide)
						{
							if ((int)ci_x + es_output_cibuf_x >= 0)
							{
								es_output_cibuf[(int)ci_x+es_output_cibuf_x].Attributes = es_default_attributes;
								//es_output_cibuf[(int)ci_x+es_output_cibuf_x].Char.UnicodeChar = '0'+(((int)ci_x + es_output_cibuf_x + es_output_cibuf_hscroll) % 8);
								es_output_cibuf[(int)ci_x+es_output_cibuf_x].Char.UnicodeChar = ' ';
							}
						}
						
						ci_x++;
					}
				}
				else
				{
					if ((int)ci_x + es_output_cibuf_x >= 0)
					{
						es_output_cibuf[(int)ci_x+es_output_cibuf_x].Attributes = es_default_attributes;
						es_output_cibuf[(int)ci_x+es_output_cibuf_x].Char.UnicodeChar = text[i];
					}

					ci_x++;
				}
			}
			
			es_output_cibuf_x += (int)ci_x;	
		}
		else
		{
			if (es_output_is_char)
			{
				if (length_in_wchars <= ES_DWORD_MAX)
				{
					DWORD numwritten;
				
					WriteConsole(es_output_handle,text,(DWORD)length_in_wchars,&numwritten,0);
				}
			}
			else
			{
				if (length_in_wchars <= INT_MAX)
				{
					int len;
					
					len = WideCharToMultiByte(es_cp,0,text,(int)length_in_wchars,0,0,0,0);
					if (len)
					{
						DWORD numwritten;
						utf8_buf_t cbuf;

						utf8_buf_init(&cbuf);

						utf8_buf_grow_size(&cbuf,len);

						WideCharToMultiByte(es_cp,0,text,(int)length_in_wchars,cbuf.buf,len,0,0);
						
						WriteFile(es_output_handle,cbuf.buf,len,&numwritten,0);
						
						utf8_buf_kill(&cbuf);
					}
				}	
			}
		}
	}
}

static void _es_output_noncell_utf8_string(const ES_UTF8 *text)
{
	wchar_buf_t wcbuf;

	wchar_buf_init(&wcbuf);

	wchar_buf_copy_utf8_string(&wcbuf,text);

	_es_output_noncell_wchar_string(wcbuf.buf);

	wchar_buf_kill(&wcbuf);
}

static void _es_output_noncell_printf(const ES_UTF8 *format,...)
{
	va_list argptr;
	utf8_buf_t cbuf;

	va_start(argptr,format);
	utf8_buf_init(&cbuf);

	utf8_buf_vprintf(&cbuf,format,argptr);
	
	_es_output_noncell_utf8_string(cbuf.buf);

	utf8_buf_kill(&cbuf);
	va_end(argptr);
}

static BOOL _es_output_header(void)
{
	if (es_header > 0)
	{
		wchar_buf_t property_name_wcbuf;

		wchar_buf_init(&property_name_wcbuf);
		
		es_is_in_header = 1;
		
		_es_output_line_begin(0);
		
		es_output_column = column_order_start;
		
		while(es_output_column)
		{
			_es_output_cell_separator();

			_es_get_nice_property_name(es_output_column->property_id,&property_name_wcbuf);

			// no quotes
			// never highlight.
			_es_output_cell_wchar_string(property_name_wcbuf.buf,0);

			es_output_column = es_output_column->order_next;
		}

		_es_output_line_end(0);

		es_is_in_header = 0;
		
		wchar_buf_kill(&property_name_wcbuf);
		
		return TRUE;
	}
	
	return FALSE;
}

static void _es_output_page_begin(void)
{
	es_output_cibuf_y = 0;	
}

static void _es_output_page_end(void)
{
}

static void _es_output_line_begin(int is_first)
{
	es_output_cibuf_x = -es_output_cibuf_hscroll;

	es_output_column = column_order_start;
	
	if (_es_export_type == ES_EXPORT_TYPE_JSON)
	{
		if (is_first)
		{
			_es_output_noncell_printf("[");
		}
		
		_es_output_noncell_printf("{");
	}
}

static void _es_output_line_end(int is_more)
{
	if (_es_export_type == ES_EXPORT_TYPE_JSON)
	{
		_es_output_noncell_printf("}");

		if (is_more)
		{
			_es_output_noncell_printf(",");
		}
		else
		{
			_es_output_noncell_printf("]");
		}
	}
	
	if (es_output_cibuf)
	{
		int wlen;

		// fill right side of screen
		
		if (es_output_cibuf_x)
		{
			if (es_output_cibuf_x + es_output_cibuf_hscroll > es_max_wide)
			{
				es_max_wide = es_output_cibuf_x + es_output_cibuf_hscroll;
			}
		}

		wlen = es_console_wide - es_output_cibuf_x;
		
		if (wlen > 0)
		{
			int bufi;
			
			for(bufi=0;bufi<wlen;bufi++)
			{
				if (bufi + es_output_cibuf_x >= es_console_wide)
				{
					break;
				}

				if (bufi + es_output_cibuf_x >= 0)
				{
					es_output_cibuf[bufi+es_output_cibuf_x].Attributes = es_default_attributes;
					es_output_cibuf[bufi+es_output_cibuf_x].Char.UnicodeChar = ' ';
				}
			}
		}
		
		// draw buffer line.
		{
			COORD buf_size;
			COORD buf_pos;
			SMALL_RECT write_rect;
			
			buf_size.X = es_console_wide;
			buf_size.Y = 1;
			
			buf_pos.X = 0;
			buf_pos.Y = 0;
			
			write_rect.Left = es_console_window_x;
			write_rect.Top = es_console_window_y + es_output_cibuf_y;
			write_rect.Right = es_console_window_x + es_console_wide;
			write_rect.Bottom = es_console_window_y + es_output_cibuf_y + 1;
			
			WriteConsoleOutput(es_output_handle,es_output_cibuf,buf_size,buf_pos,&write_rect);
			
			es_output_cibuf_x += wlen;				
		}
	
		// output nothing.
		es_output_cibuf_y++;
	}
	else
	{
		switch(es_newline_type)
		{
			case 0:
				_es_output_noncell_printf("\r\n");
				break;
				
			case 1:
				_es_output_noncell_printf("\n");
				break;
				
			case 2:
				_es_output_noncell_wchar_string_n(L"\0",1);
				break;
		}
	}
	
	_es_output_cell_overflow = 0;
}

// output a cell with formatting.
static void _es_output_cell_printf(int is_highlighted,ES_UTF8 *format,...)
{
	va_list argptr;
	utf8_buf_t cbuf;

	va_start(argptr,format);
	utf8_buf_init(&cbuf);

	utf8_buf_vprintf(&cbuf,format,argptr);
	
	_es_output_cell_utf8_string(cbuf.buf,is_highlighted);

	utf8_buf_kill(&cbuf);
	va_end(argptr);
}

// output ipc1 results.
// can be called multiple times for pause mode.
// count should include the header if shown
static void _es_output_ipc1_results(EVERYTHING_IPC_LIST *list,SIZE_T index_start,SIZE_T count)
{
	SIZE_T run;
	wchar_buf_t filename_wcbuf;
	EVERYTHING_IPC_ITEM *everything_ipc_item;

	wchar_buf_init(&filename_wcbuf);

	everything_ipc_item = list->items + index_start;
	run = count;

	_es_output_page_begin();

	if (run)
	{
		// output header.
		if (_es_output_header())
		{
			// don't inc i.
			run--;
		}
	}
	
	while(run)
	{
		_es_output_line_begin(everything_ipc_item == list->items);
		
		while(es_output_column)
		{
			_es_output_cell_separator();
			
			switch(es_output_column->property_id)
			{
				case EVERYTHING3_PROPERTY_ID_ATTRIBUTES:
					_es_output_cell_attribute_property((everything_ipc_item->flags & EVERYTHING_IPC_FOLDER) ? FILE_ATTRIBUTE_DIRECTORY : 0);
					break;
					
				case EVERYTHING3_PROPERTY_ID_PATH_AND_NAME:
					wchar_buf_path_cat_filename(EVERYTHING_IPC_ITEMPATH(list,everything_ipc_item),EVERYTHING_IPC_ITEMFILENAME(list,everything_ipc_item),&filename_wcbuf);

					if ((es_folder_append_path_separator) && (everything_ipc_item->flags & EVERYTHING_IPC_FOLDER))
					{
						wchar_buf_cat_path_separator(&filename_wcbuf);
					}
					
					_es_output_cell_text_property_wchar_string(filename_wcbuf.buf);
					break;

				case EVERYTHING3_PROPERTY_ID_PATH:
					_es_output_cell_text_property_wchar_string(EVERYTHING_IPC_ITEMPATH(list,everything_ipc_item));
					break;
					
				case EVERYTHING3_PROPERTY_ID_NAME:
					_es_output_cell_text_property_wchar_string(EVERYTHING_IPC_ITEMFILENAME(list,everything_ipc_item));
					break;
												
				default:
					_es_output_cell_unknown_property();
					break;

			}
			
			es_output_column = es_output_column->order_next;
		}									
		
		everything_ipc_item++;
		run--;	

		_es_output_line_end(run ? 1 : 0);
	}

	_es_output_page_end();

	wchar_buf_kill(&filename_wcbuf);
}

// output a ipc2 list 
// count should include the header if shown
static void _es_output_ipc2_results(EVERYTHING_IPC_LIST2 *list,SIZE_T index_start,SIZE_T count)
{
	SIZE_T i;
	SIZE_T run;
	EVERYTHING_IPC_ITEM2 *items;
	wchar_buf_t column_wcbuf;

	wchar_buf_init(&column_wcbuf);			
	items = (EVERYTHING_IPC_ITEM2 *)(list + 1);

	i = index_start;
	run = count;

	_es_output_page_begin();
	
	if (run)
	{
		// output header.
		if (_es_output_header())
		{
			// don't inc i.
			run--;
		}
	}

	while(run)
	{
		if (i >= list->numitems)
		{
			break;
		}
	
		_es_output_line_begin(i == 0);
			
		while(es_output_column)
		{
			void *data;
			
			_es_output_cell_separator();
			
			data = _es_ipc2_get_column_data(list,i,es_output_column->property_id,_es_highlight);
			if (data)
			{
				switch(es_output_column->property_id)
				{
					case EVERYTHING3_PROPERTY_ID_NAME:
					case EVERYTHING3_PROPERTY_ID_PATH:
					case EVERYTHING3_PROPERTY_ID_PATH_AND_NAME:
					case EVERYTHING3_PROPERTY_ID_FILE_LIST_NAME:
					
						{
							DWORD length;
							
							// align length.					
							os_copy_memory(&length,data,sizeof(DWORD));
							
							// align text.
							wchar_buf_grow_length(&column_wcbuf,length);
							os_copy_memory(column_wcbuf.buf,((BYTE *)data) + sizeof(DWORD),length * sizeof(wchar_t));
							column_wcbuf.buf[length] = 0;

							if (es_folder_append_path_separator)
							{
								if (es_output_column->property_id == EVERYTHING3_PROPERTY_ID_PATH_AND_NAME)
								{
									if (items[i].flags & EVERYTHING_IPC_FOLDER)
									{
										wchar_buf_cat_path_separator(&column_wcbuf);
									}
								}
							}
														
							if (_es_highlight)
							{
								_es_output_cell_highlighted_text_property_wchar_string(column_wcbuf.buf);
							}
							else
							{
								_es_output_cell_text_property_wchar_string(column_wcbuf.buf);
							}
						}
						
						break;
						
					case EVERYTHING3_PROPERTY_ID_EXTENSION:
						{
							DWORD length;
												
							// align length.					
							os_copy_memory(&length,data,sizeof(DWORD));
							
							// align text.
							wchar_buf_grow_length(&column_wcbuf,length);
							os_copy_memory(column_wcbuf.buf,((BYTE *)data) + sizeof(DWORD),length * sizeof(wchar_t));
							column_wcbuf.buf[length] = 0;
							
							if (items[i].flags & EVERYTHING_IPC_FOLDER)
							{
								_es_output_cell_unknown_property();
							}
							else
							{
								_es_output_cell_text_property_wchar_string(column_wcbuf.buf);
							}
						}
						
						break;
						
					case EVERYTHING3_PROPERTY_ID_SIZE:

						{
							ES_UINT64 size;
							
							// align data.
							os_copy_memory(&size,data,sizeof(ES_UINT64));

							_es_output_cell_size_property((items[i].flags & EVERYTHING_IPC_FOLDER) ? 1 : 0,size);
						}
						
						break;

					case EVERYTHING3_PROPERTY_ID_DATE_MODIFIED:
					case EVERYTHING3_PROPERTY_ID_DATE_CREATED:
					case EVERYTHING3_PROPERTY_ID_DATE_ACCESSED:
					case EVERYTHING3_PROPERTY_ID_DATE_RUN:
					case EVERYTHING3_PROPERTY_ID_DATE_RECENTLY_CHANGED:
					
						{
							ES_UINT64 filetime;
							
							// align data.
							os_copy_memory(&filetime,data,sizeof(ES_UINT64));

							_es_output_cell_filetime_property(filetime);
						}
						
						break;

					case EVERYTHING3_PROPERTY_ID_ATTRIBUTES:

						{
							DWORD attributes;
							
							// align data.
							os_copy_memory(&attributes,data,sizeof(DWORD));

							_es_output_cell_attribute_property(attributes);
						}
						
						break;

					
					case EVERYTHING3_PROPERTY_ID_RUN_COUNT:
					
						{
							DWORD value;
							
							// align data.
							os_copy_memory(&value,data,sizeof(DWORD));

							_es_output_cell_number_property(value,ES_DWORD_MAX);
						}
						
						break;		
									
					default:
						_es_output_cell_unknown_property();
						break;
				}
			}
			else
			{
				switch(es_output_column->property_id)
				{
					case EVERYTHING3_PROPERTY_ID_ATTRIBUTES:

						// use what we have.
						_es_output_cell_attribute_property((items[i].flags & EVERYTHING_IPC_FOLDER) ? FILE_ATTRIBUTE_DIRECTORY : 0);
						
						break;

					default:
						_es_output_cell_unknown_property();
						break;
				}
			}
			
			es_output_column = es_output_column->order_next;
		}

		run--;
		i++;

		_es_output_line_end(run ? 1 : 0);
	}

	_es_output_page_end();

	wchar_buf_kill(&column_wcbuf);			
}

// output a ipc2 list 
// count should include the header if shown
static void	_es_output_ipc3_results(ipc3_result_list_t *result_list,SIZE_T index_start,SIZE_T count)
{
	SIZE_T viewport_run;
	utf8_buf_t property_text_cbuf;
	int is_first_line;
	SIZE_T index;
	ipc3_stream_t *stream;
	SIZE_T property_request_count;
	ipc3_result_list_property_request_t *property_request_array;

	utf8_buf_init(&property_text_cbuf);
	
	index = index_start;
	viewport_run = count;
	stream = result_list->stream;
	property_request_count = result_list->property_request_count;
	property_request_array = (ipc3_result_list_property_request_t *)result_list->property_request_cbuf.buf;
	
	_es_output_page_begin();

	if (viewport_run)
	{
		// output header.
		if (_es_output_header())
		{
			// don't inc i.
			viewport_run--;
		}
	}
	
	is_first_line = 1;
	
	while(viewport_run)
	{
		BYTE item_flags;
		
		_es_output_line_begin(is_first_line);
		
		item_flags = ipc3_stream_read_byte(stream);
		
		// read properties..
		// they will be in the same order as requested.
		// some could be missing if they don't exist.
		// so we might have more column_order_start than property_request_array.

		{
			SIZE_T property_request_run;
			const ipc3_result_list_property_request_t *property_request_p;
			
			es_output_column = column_order_start;
			
			property_request_run = property_request_count;
			property_request_p = property_request_array;
			
			while(es_output_column)
			{
				_es_output_cell_separator();
				
				if ((property_request_run) && (es_output_column->property_id == property_request_p->property_id))
				{
					if (property_request_p->flags & (IPC3_SEARCH_PROPERTY_REQUEST_FLAG_FORMAT|IPC3_SEARCH_PROPERTY_REQUEST_FLAG_HIGHLIGHT))
					{
						SIZE_T len;
						
						len = ipc3_stream_read_len_vlq(stream);
						
						utf8_buf_grow_length(&property_text_cbuf,len);
						
						ipc3_stream_read_data(stream,property_text_cbuf.buf,len);
						
						property_text_cbuf.buf[len] = 0;
						
						_es_output_cell_highlighted_text_property_utf8_string(property_text_cbuf.buf);
					}
					else
					{
						// add to total item size.
						switch(property_request_p->value_type)
						{
							case IPC3_PROPERTY_VALUE_TYPE_PSTRING: 
							case IPC3_PROPERTY_VALUE_TYPE_PSTRING_MULTISTRING: 
							case IPC3_PROPERTY_VALUE_TYPE_PSTRING_STRING_REFERENCE:
							case IPC3_PROPERTY_VALUE_TYPE_PSTRING_FOLDER_REFERENCE:
							case IPC3_PROPERTY_VALUE_TYPE_PSTRING_FILE_OR_FOLDER_REFERENCE:

								{
									SIZE_T len;
									
									len = ipc3_stream_read_len_vlq(stream);
									
									utf8_buf_grow_length(&property_text_cbuf,len);
									
									ipc3_stream_read_data(stream,property_text_cbuf.buf,len);
									
									property_text_cbuf.buf[len] = 0;
								}
								
								switch(property_get_format(es_output_column->property_id))
								{
									case PROPERTY_FORMAT_TEXT30:
									case PROPERTY_FORMAT_TEXT47:
									case PROPERTY_FORMAT_EXTENSION:
										_es_output_cell_text_property_utf8_string_n(property_text_cbuf.buf,property_text_cbuf.length_in_bytes);
										break;
										
									default:
										debug_error_printf("unhandled format %d for %d\n",property_get_format(es_output_column->property_id),property_request_p->value_type);
										_es_output_cell_unknown_property();
										break;

								}

								break;

							case IPC3_PROPERTY_VALUE_TYPE_BYTE:
							case IPC3_PROPERTY_VALUE_TYPE_BYTE_GET_TEXT:

								{
									BYTE byte_value;

									ipc3_stream_read_data(stream,&byte_value,sizeof(BYTE));
								
									switch(property_get_format(es_output_column->property_id))
									{
										case PROPERTY_FORMAT_NUMBER:
										case PROPERTY_FORMAT_NUMBER1:
										case PROPERTY_FORMAT_NUMBER2:
										case PROPERTY_FORMAT_NUMBER3:
										case PROPERTY_FORMAT_NUMBER4:
										case PROPERTY_FORMAT_NUMBER5:
										case PROPERTY_FORMAT_NUMBER6:
										case PROPERTY_FORMAT_NUMBER7:
										case PROPERTY_FORMAT_HEX_NUMBER:
											_es_output_cell_number_property(byte_value,ES_BYTE_MAX);
											break;
											
										default:
											debug_error_printf("unhandled format %d for %d\n",property_get_format(es_output_column->property_id),property_request_p->value_type);
											_es_output_cell_unknown_property();
											break;

									}
								}

								break;

							case IPC3_PROPERTY_VALUE_TYPE_WORD:
							case IPC3_PROPERTY_VALUE_TYPE_WORD_GET_TEXT:

								{
									WORD word_value;

									ipc3_stream_read_data(stream,&word_value,sizeof(WORD));
									
									switch(property_get_format(es_output_column->property_id))
									{
										case PROPERTY_FORMAT_NUMBER:
										case PROPERTY_FORMAT_NUMBER1:
										case PROPERTY_FORMAT_NUMBER2:
										case PROPERTY_FORMAT_NUMBER3:
										case PROPERTY_FORMAT_NUMBER4:
										case PROPERTY_FORMAT_NUMBER5:
										case PROPERTY_FORMAT_NUMBER6:
										case PROPERTY_FORMAT_NUMBER7:
										case PROPERTY_FORMAT_HEX_NUMBER:
											_es_output_cell_number_property(word_value,ES_WORD_MAX);
											break;
											
										default:
											debug_error_printf("unhandled format %d for %d\n",property_get_format(es_output_column->property_id),property_request_p->value_type);
											_es_output_cell_unknown_property();
											break;

									}
								}
								
								break;

							case IPC3_PROPERTY_VALUE_TYPE_DWORD: 
							case IPC3_PROPERTY_VALUE_TYPE_DWORD_FIXED_Q1K: 
							case IPC3_PROPERTY_VALUE_TYPE_DWORD_GET_TEXT: 

								{
									DWORD dword_value;

									ipc3_stream_read_data(stream,&dword_value,sizeof(DWORD));
									
									switch(property_get_format(es_output_column->property_id))
									{
										case PROPERTY_FORMAT_ATTRIBUTES:
											_es_output_cell_attribute_property(dword_value);
											break;

										case PROPERTY_FORMAT_NUMBER:
										case PROPERTY_FORMAT_NUMBER1:
										case PROPERTY_FORMAT_NUMBER2:
										case PROPERTY_FORMAT_NUMBER3:
										case PROPERTY_FORMAT_NUMBER4:
										case PROPERTY_FORMAT_NUMBER5:
										case PROPERTY_FORMAT_NUMBER6:
										case PROPERTY_FORMAT_NUMBER7:
										case PROPERTY_FORMAT_HEX_NUMBER:
											_es_output_cell_number_property(dword_value,ES_DWORD_MAX);
											break;
									}
								}
								
								break;
								
							case IPC3_PROPERTY_VALUE_TYPE_UINT64: 

								{
									ES_UINT64 uint64_value;

									ipc3_stream_read_data(stream,&uint64_value,sizeof(ES_UINT64));

									switch(property_get_format(es_output_column->property_id))
									{
										case PROPERTY_FORMAT_SIZE:
											_es_output_cell_size_property(item_flags & IPC3_RESULT_LIST_ITEM_FLAG_FOLDER ? 1 : 0,uint64_value);
											break;

										case PROPERTY_FORMAT_FILETIME:
											_es_output_cell_filetime_property(uint64_value);
											break;
											
										case PROPERTY_FORMAT_DURATION:
											_es_output_cell_duration_property(uint64_value);
											break;
											
										case PROPERTY_FORMAT_NUMBER:
										case PROPERTY_FORMAT_NUMBER1:
										case PROPERTY_FORMAT_NUMBER2:
										case PROPERTY_FORMAT_NUMBER3:
										case PROPERTY_FORMAT_NUMBER4:
										case PROPERTY_FORMAT_NUMBER5:
										case PROPERTY_FORMAT_NUMBER6:
										case PROPERTY_FORMAT_NUMBER7:
										case PROPERTY_FORMAT_HEX_NUMBER:
											_es_output_cell_number_property(uint64_value,ES_UINT64_MAX);
											break;
											
										default:
											debug_error_printf("unhandled format %d for %d\n",property_get_format(es_output_column->property_id),property_request_p->value_type);
											_es_output_cell_unknown_property();
											break;
									}
								}
								
								break;
								
							case IPC3_PROPERTY_VALUE_TYPE_UINT128: 

								{
									EVERYTHING3_UINT128 uint128_value;

									ipc3_stream_read_data(stream,&uint128_value,sizeof(EVERYTHING3_UINT128));
									
									switch(property_get_format(es_output_column->property_id))
									{
										default:
											debug_error_printf("unhandled format %d for %d\n",property_get_format(es_output_column->property_id),property_request_p->value_type);
											_es_output_cell_unknown_property();
											break;
									}
								}
								
								break;
								
							case IPC3_PROPERTY_VALUE_TYPE_DIMENSIONS: 

								{
									EVERYTHING3_DIMENSIONS dimensions_value;

									ipc3_stream_read_data(stream,&dimensions_value,sizeof(EVERYTHING3_DIMENSIONS));

									switch(property_get_format(es_output_column->property_id))
									{
										case PROPERTY_FORMAT_DIMENSIONS:
											_es_output_cell_dimensions_property(&dimensions_value);
											break;
											
										default:
											debug_error_printf("unhandled format %d for %d\n",property_get_format(es_output_column->property_id),property_request_p->value_type);
											_es_output_cell_unknown_property();
											break;
									}
								}
								
								break;
								
							case IPC3_PROPERTY_VALUE_TYPE_SIZE_T:
							
								{
									SIZE_T size_t_value;
									
									size_t_value = ipc3_stream_read_size_t(stream);

									switch(property_get_format(es_output_column->property_id))
									{	
										case PROPERTY_FORMAT_NUMBER:
										case PROPERTY_FORMAT_NUMBER1:
										case PROPERTY_FORMAT_NUMBER2:
										case PROPERTY_FORMAT_NUMBER3:
										case PROPERTY_FORMAT_NUMBER4:
										case PROPERTY_FORMAT_NUMBER5:
										case PROPERTY_FORMAT_NUMBER6:
										case PROPERTY_FORMAT_NUMBER7:
										case PROPERTY_FORMAT_HEX_NUMBER:
											_es_output_cell_number_property(size_t_value,SIZE_MAX);
											break;
											
										default:
											debug_error_printf("unhandled format %d for %d\n",property_get_format(es_output_column->property_id),property_request_p->value_type);
											_es_output_cell_unknown_property();
											break;
									}
								}
								break;
								
							case IPC3_PROPERTY_VALUE_TYPE_INT32_FIXED_Q1K: 
							case IPC3_PROPERTY_VALUE_TYPE_INT32_FIXED_Q1M: 

								{
									__int32 int32_value;

									ipc3_stream_read_data(stream,&int32_value,sizeof(__int32));
								}
								
								break;

							case IPC3_PROPERTY_VALUE_TYPE_BLOB8:

								{
									BYTE len;
									
									len = ipc3_stream_read_byte(stream);
									
									utf8_buf_grow_length(&property_text_cbuf,len);
									
									ipc3_stream_read_data(stream,property_text_cbuf.buf,len);
									
									property_text_cbuf.buf[len] = 0;
								
									switch(property_get_format(es_output_column->property_id))
									{	
										case PROPERTY_FORMAT_DATA1:
										case PROPERTY_FORMAT_DATA2:
										case PROPERTY_FORMAT_DATA4:
										case PROPERTY_FORMAT_DATA8:
										case PROPERTY_FORMAT_DATA16:
										case PROPERTY_FORMAT_DATA32:
										case PROPERTY_FORMAT_DATA64:
										case PROPERTY_FORMAT_DATA128:
										case PROPERTY_FORMAT_DATA256:
										case PROPERTY_FORMAT_DATA512:
											_es_output_cell_data_property(property_text_cbuf.buf,property_text_cbuf.length_in_bytes);
											break;
											
										default:
											debug_error_printf("unhandled format %d for %d\n",property_get_format(es_output_column->property_id),property_request_p->value_type);
											_es_output_cell_unknown_property();
											break;
									}
								}

								break;

							case IPC3_PROPERTY_VALUE_TYPE_BLOB16:

								{
									WORD len;
									
									len = ipc3_stream_read_word(stream);
									
									utf8_buf_grow_length(&property_text_cbuf,len);
									
									ipc3_stream_read_data(stream,property_text_cbuf.buf,len);
									
									property_text_cbuf.buf[len] = 0;
								}

								break;

								
							default:
								debug_error_printf("bad property value type %d\n",property_request_p->value_type);
								es_fatal(ES_ERROR_IPC_ERROR);
								break;
						}
					}

					property_request_p++;
					property_request_run--;
				}
				else
				{
					// empty
					if (es_output_column->property_id == EVERYTHING3_PROPERTY_ID_ATTRIBUTES)
					{
						// always output known attributes. (EFU export)
						_es_output_cell_attribute_property(item_flags & IPC3_RESULT_LIST_ITEM_FLAG_FOLDER ? FILE_ATTRIBUTE_DIRECTORY : 0);
					}
					else
					{
						_es_output_cell_unknown_property();
					}
				}
				
				es_output_column = es_output_column->order_next;
			}
			
			// read remaining pipe data.
			// we shouldn't any remaining data.
			// this should really be an error.
			while(property_request_run)
			{
				// skip it.
				if (property_request_p->flags & (IPC3_SEARCH_PROPERTY_REQUEST_FLAG_FORMAT|IPC3_SEARCH_PROPERTY_REQUEST_FLAG_HIGHLIGHT))
				{
					SIZE_T len;
					
					len = ipc3_stream_read_len_vlq(stream);
					
					ipc3_stream_skip(stream,len);
				}
				else
				{
					// add to total item size.
					switch(property_request_p->value_type)
					{
						case IPC3_PROPERTY_VALUE_TYPE_PSTRING: 
						case IPC3_PROPERTY_VALUE_TYPE_PSTRING_MULTISTRING: 
						case IPC3_PROPERTY_VALUE_TYPE_PSTRING_STRING_REFERENCE:
						case IPC3_PROPERTY_VALUE_TYPE_PSTRING_FOLDER_REFERENCE:
						case IPC3_PROPERTY_VALUE_TYPE_PSTRING_FILE_OR_FOLDER_REFERENCE:

							{
								SIZE_T len;
								
								len = ipc3_stream_read_len_vlq(stream);
								
								ipc3_stream_skip(stream,len);
							}

							break;

						case IPC3_PROPERTY_VALUE_TYPE_BYTE:
						case IPC3_PROPERTY_VALUE_TYPE_BYTE_GET_TEXT:

							ipc3_stream_skip(stream,sizeof(BYTE));

							break;

						case IPC3_PROPERTY_VALUE_TYPE_WORD:
						case IPC3_PROPERTY_VALUE_TYPE_WORD_GET_TEXT:

							ipc3_stream_skip(stream,sizeof(WORD));
							
							break;

						case IPC3_PROPERTY_VALUE_TYPE_DWORD: 
						case IPC3_PROPERTY_VALUE_TYPE_DWORD_FIXED_Q1K: 
						case IPC3_PROPERTY_VALUE_TYPE_DWORD_GET_TEXT: 

							ipc3_stream_skip(stream,sizeof(DWORD));
							
							break;
							
						case IPC3_PROPERTY_VALUE_TYPE_UINT64: 

							ipc3_stream_skip(stream,sizeof(ES_UINT64));
							
							break;
							
						case IPC3_PROPERTY_VALUE_TYPE_UINT128: 

							ipc3_stream_skip(stream,sizeof(EVERYTHING3_UINT128));
							
							break;
							
						case IPC3_PROPERTY_VALUE_TYPE_DIMENSIONS: 

							ipc3_stream_skip(stream,sizeof(EVERYTHING3_DIMENSIONS));
							
							break;
							
						case IPC3_PROPERTY_VALUE_TYPE_SIZE_T:
						
							ipc3_stream_read_size_t(stream);
							break;
							
						case IPC3_PROPERTY_VALUE_TYPE_INT32_FIXED_Q1K: 
						case IPC3_PROPERTY_VALUE_TYPE_INT32_FIXED_Q1M: 

							ipc3_stream_skip(stream,sizeof(__int32));
							
							break;

						case IPC3_PROPERTY_VALUE_TYPE_BLOB8:

							{
								BYTE len;
								
								len = ipc3_stream_read_byte(stream);
								
								ipc3_stream_skip(stream,len);
							}

							break;

						case IPC3_PROPERTY_VALUE_TYPE_BLOB16:

							{
								WORD len;
								
								len = ipc3_stream_read_word(stream);
								
								ipc3_stream_skip(stream,len);
							}

							break;

					}
				}
				
				property_request_p++;
				property_request_run--;
			}
		}

		viewport_run--;

		_es_output_line_end(viewport_run ? 1 : 0);

		is_first_line = 0;
	}

	_es_output_page_end();

	utf8_buf_kill(&property_text_cbuf);
}

static void _es_output_ipc2_total_size(EVERYTHING_IPC_LIST2 *list,int count)
{
	DWORD i;
	EVERYTHING_IPC_ITEM2 *items;
	ES_UINT64 total_size;
	
	items = (EVERYTHING_IPC_ITEM2 *)(list + 1);

	i = 0;
	total_size = 0;
	
	while(count)
	{
		void *data;
		
		if (i >= list->numitems)
		{
			break;
		}
	
		data = _es_ipc2_get_column_data(list,i,EVERYTHING3_PROPERTY_ID_SIZE,0);

		if (data)
		{
			// dont count folders
			if (!(items[i].flags & EVERYTHING_IPC_FOLDER))
			{
				ES_UINT64 size;
				
				// align data.
				os_copy_memory(&size,data,sizeof(ES_UINT64));
				
				total_size += size;
			}
		}

		count--;
		i++;
	}

	_es_output_noncell_total_size(total_size);
}

// custom window proc
static LRESULT __stdcall _es_window_proc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam)
{
	switch(msg)
	{
		case WM_COPYDATA:
		{
			COPYDATASTRUCT *cds = (COPYDATASTRUCT *)lParam;
			
			switch(cds->dwData)
			{
				case ES_COPYDATA_IPCTEST_QUERYCOMPLETEW:

					if (es_no_result_error)
					{
						if (((EVERYTHING_IPC_LIST *)cds->lpData)->totitems == 0)
						{
							es_ret = ES_ERROR_NO_RESULTS;
						}
					}

					if (es_get_result_count)
					{
						_es_output_noncell_result_count(((EVERYTHING_IPC_LIST *)cds->lpData)->totitems);
					}
					else
					if (es_get_total_size)
					{
						// version 2 or later required.
						// IPC unavailable.
						es_ret = ES_ERROR_NO_IPC;
					}
					else
					{
						EVERYTHING_IPC_LIST *list;
						utf8_buf_t sorted_list_cbuf;
						
						list = cds->lpData;
						utf8_buf_init(&sorted_list_cbuf);
						
						// sort by path.
						if (_es_primary_sort_property_id == EVERYTHING3_PROPERTY_ID_PATH)
						{
							utf8_buf_t indexes_cbuf;
							EVERYTHING_IPC_ITEM **indexes;
							EVERYTHING_IPC_ITEM **indexes_d;

							utf8_buf_init(&indexes_cbuf);

							// allocate indexes.
							utf8_buf_grow_size(&indexes_cbuf,safe_size_mul_sizeof_pointer(list->numitems));
							utf8_buf_grow_size(&sorted_list_cbuf,cds->cbData);
							
							// copy list header.
							os_copy_memory(sorted_list_cbuf.buf,list,sizeof(EVERYTHING_IPC_LIST) - sizeof(EVERYTHING_IPC_ITEM));
							
							// fill in indexes.
							indexes = (EVERYTHING_IPC_ITEM **)indexes_cbuf.buf;
							indexes_d = indexes;

							{
								DWORD i;
							
								for(i=0;i<list->numitems;i++)
								{
									*indexes_d++ = &list->items[i];
								}
							}
							
							// set _es_sort_list for es_compare_list_items.
							// we are single threaded, and _es_sort_list is read only.
							_es_sort_list = list;
							
							os_sort(indexes,list->numitems,_es_compare_list_items);
							
							// copy data.
							
							{
								const EVERYTHING_IPC_ITEM **indexes_p;
								DWORD run;
								EVERYTHING_IPC_ITEM *d;
								
								indexes_p = indexes;
								run = list->numitems;
								d = ((EVERYTHING_IPC_LIST *)sorted_list_cbuf.buf)->items;

								while(run)
								{
									os_copy_memory(d,*indexes_p,sizeof(EVERYTHING_IPC_ITEM));
									d++;
									
									indexes_p++;
									run--;
								}
								
								// copy filenames
								os_copy_memory(d,list->items + list->numitems,cds->cbData - (((BYTE *)d) - sorted_list_cbuf.buf));
							}
							
							// set our sorted list as the main list.
							list = (EVERYTHING_IPC_LIST *)sorted_list_cbuf.buf;
							
							utf8_buf_kill(&indexes_cbuf);
						}
					
						if (es_pause)
						{
							_es_output_pause(ES_IPC_VERSION_FLAG_IPC1,list);
						}
						else
						{
							SIZE_T total_lines;
							
							total_lines = list->numitems;
							if (es_header > 0)
							{
								total_lines = safe_size_add_one(total_lines);
							}
							
							_es_output_ipc1_results(list,0,total_lines);
						}
						
						utf8_buf_kill(&sorted_list_cbuf);
					}
					
					PostQuitMessage(0);
					
					return TRUE;
					
				case ES_COPYDATA_IPCTEST_QUERYCOMPLETE2W:
				{
					if (es_no_result_error)
					{
						if (((EVERYTHING_IPC_LIST2 *)cds->lpData)->totitems == 0)
						{
							es_ret = ES_ERROR_NO_RESULTS;
						}
					}
				
					if (es_get_result_count)
					{
						_es_output_noncell_result_count(((EVERYTHING_IPC_LIST2 *)cds->lpData)->totitems);
					}
					else
					if (es_get_total_size)
					{
						_es_output_ipc2_total_size((EVERYTHING_IPC_LIST2 *)cds->lpData,((EVERYTHING_IPC_LIST2 *)cds->lpData)->numitems);
					}
					else
					if (es_pause)
					{
						_es_output_pause(ES_IPC_VERSION_FLAG_IPC2,cds->lpData);
					}
					else
					{
						SIZE_T total_lines;
						
						total_lines = ((EVERYTHING_IPC_LIST2 *)cds->lpData)->numitems;
						if (es_header > 0)
						{
							total_lines = safe_size_add_one(total_lines);
						}

						_es_output_ipc2_results((EVERYTHING_IPC_LIST2 *)cds->lpData,0,total_lines);
					}
	
					PostQuitMessage(0);
				
					return TRUE;
				}
			}
			
			break;
		}
	}
	
	return DefWindowProc(hwnd,msg,wParam,lParam);
}

static void _es_help(void)
{
	// Help from NotNull
	_es_output_noncell_utf8_string(
		"ES " VERSION_TEXT "\r\n"
		"ES is a command line interface to search Everything from a command prompt.\r\n"
		"ES uses the Everything search syntax.\r\n"
		"\r\n"
		"Usage: es.exe [options] search text\r\n"
		"Example: ES  Everything ext:exe;ini \r\n"
		"\r\n"
		"\r\n"
		"Search options\r\n"
		"   -r <search>, -regex <search>\r\n"
		"        Search using regular expressions.\r\n"
		"   -i, -case\r\n"
		"        Match case.\r\n"
		"   -w, -ww, -whole-word, -whole-words\r\n"
		"        Match whole words.\r\n"
		"   -p, -match-path\r\n"
		"        Match full path and file name.\r\n"
		"   -a, -diacritics\r\n"
		"        Match diacritical marks.\r\n"
		"   -prefix\r\n"
		"        Match start of words.\r\n"
		"   -suffix\r\n"
		"        Match end of words.\r\n"
		"   -ignore-punctuation\r\n"
		"        Ignore punctuation in filenames.\r\n"
		"   -ignore-whitespace\r\n"
		"        Ignore whitespace in filenames.\r\n"
		"\r\n"
		"   -o <offset>, -offset <offset>\r\n"
		"        Show results starting from offset.\r\n"
		"   -n <num>, -max-results <num>\r\n"
		"        Limit the number of results shown to <num>.\r\n"
		"\r\n"
		"   -path <path>\r\n"
		"        Search for subfolders and files in path.\r\n"
		"   -parent-path <path>\r\n"
		"        Search for subfolders and files in the parent of path.\r\n"
		"   -parent <path>\r\n"
		"        Search for files with the specified parent path.\r\n"
		"\r\n"
		"   /ad\r\n"
		"        Folders only.\r\n"
		"   /a-d\r\n"
		"        Files only.\r\n"
		"   /a[RHSDAVNTPLCOIE]\r\n"
		"        DIR style attributes search.\r\n"
		"        R = Read only.\r\n"
		"        H = Hidden.\r\n"
		"        S = System.\r\n"
		"        D = Directory.\r\n"
		"        A = Archive.\r\n"
		"        V = Device.\r\n"
		"        X = No scrub data.\r\n"
		"        N = Normal.\r\n"
		"        T = Temporary.\r\n"
		"        P = Sparse file.\r\n"
		"        L = Reparse point.\r\n"
		"        C = Compressed.\r\n"
		"        O = Offline.\r\n"
		"        I = Not content indexed.\r\n"
		"        E = Encrypted.\r\n"
		"        U = Unpinned.\r\n"
		"        P = Pinned.\r\n"
		"        M = Recall on data access.\r\n"
		"        - = Prefix a flag with - to exclude.\r\n"
		"\r\n"
		"\r\n"
		"Sort options\r\n"
		"   -s\r\n"
		"        sort by full path.\r\n"
		"   -sort <name[-ascending|-descending]>\r\n"
		"        Set sort\r\n"
		"        name=name|path|size|extension|date-created|date-modified|date-accessed|\r\n"
		"        attributes|file-list-file-name|run-count|date-recently-changed|date-run|\r\n"
		"        <property-name>\r\n"
		"\r\n"
		"   /on, /o-n, /os, /o-s, /oe, /o-e, /od, /o-d\r\n"
		"        DIR style sorts.\r\n"
		"        N = Name.\r\n"
		"        S = Size.\r\n"
		"        E = Extension.\r\n"
		"        D = Date modified.\r\n"
		"        - = Sort in descending order.\r\n"
		"\r\n"
		"\r\n"
		"Display options\r\n"
		"   -name\r\n"
		"   -path-column\r\n"
		"   -full-path-and-name, -filename-column\r\n"
		"   -extension, -ext\r\n"
		"   -size\r\n"
		"   -date-created, -dc\r\n"
		"   -date-modified, -dm\r\n"
		"   -date-accessed, -da\r\n"
		"   -attributes, -attribs, -attrib\r\n"
		"   -file-list-file-name\r\n"
		"   -run-count\r\n"
		"   -date-run\r\n"
		"   -date-recently-changed, -rc\r\n"
		"   -<property-name>\r\n"
		"        Show the specified column.\r\n"
		"        \r\n"
		"   -highlight\r\n"
		"        Highlight results.\r\n"
		"   -highlight-color <color>\r\n"
		"        Highlight color 0x00-0xff.\r\n"
		"\r\n"
		"   -csv\r\n"
		"   -efu\r\n"
		"   -txt\r\n"
		"   -m3u\r\n"
		"   -m3u8\r\n"
		"   -tsv\r\n"
		"   -json\r\n"
		"        Change display format.\r\n"
		"\r\n"
		"   -size-format <format>\r\n"
		"        0=auto, 1=Bytes, 2=KB, 3=MB.\r\n"
		"   -date-format <format>, -export-date-format\r\n"
		"        0=auto, 1=ISO-8601, 2=FILETIME, 3=ISO-8601(UTC)\r\n"
		"\r\n"
		"   -filename-color <color>\r\n"
		"   -name-color <color>\r\n"
		"   -path-color <color>\r\n"
		"   -extension-color <color>\r\n"
		"   -size-color <color>\r\n"
		"   -date-created-color <color>, -dc-color <color>\r\n"
		"   -date-modified-color <color>, -dm-color <color>\r\n"
		"   -date-accessed-color <color>, -da-color <color>\r\n"
		"   -attributes-color <color>\r\n"
		"   -file-list-filename-color <color>\r\n"
		"   -run-count-color <color>\r\n"
		"   -date-run-color <color>\r\n"
		"   -date-recently-changed-color <color>, -rc-color <color>\r\n"
		"   -<property-name>-color <color>\r\n"
		"        Set the column color 0x00-0xff.\r\n"
		"\r\n"
		"   -filename-width <width>\r\n"
		"   -name-width <width>\r\n"
		"   -path-width <width>\r\n"
		"   -extension-width <width>\r\n"
		"   -size-width <width>\r\n"
		"   -date-created-width <width>, -dc-width <width>\r\n"
		"   -date-modified-width <width>, -dm-width <width>\r\n"
		"   -date-accessed-width <width>, -da-width <width>\r\n"
		"   -attributes-width <width>\r\n"
		"   -file-list-filename-width <width>\r\n"
		"   -run-count-width <width>\r\n"
		"   -date-run-width <width>\r\n"
		"   -date-recently-changed-width <width>, -rc-width <width>\r\n"
		"   -<property-name>-width <width>\r\n"
		"        Set the column width 0-65535.\r\n"
		"\r\n"
		"   -no-digit-grouping\r\n"
		"        Don't group numbers with commas.\r\n"
		"   -double-quote\r\n"
		"        Wrap paths and filenames with double quotes.\r\n"
		"\r\n"
		"\r\n"
		"Export options\r\n"
		"   -export-csv <out.csv>\r\n"
		"   -export-efu <out.efu>\r\n"
		"   -export-txt <out.txt>\r\n"
		"   -export-m3u <out.m3u>\r\n"
		"   -export-m3u8 <out.m3u8>\r\n"
		"   -export-tsv <out.txt>\r\n"
		"   -export-json <out.json>\r\n"
		"        Export to a file using the specified layout.\r\n"
		"   -no-header\r\n"
		"        Do not output a column header for CSV, EFU and TSV files.\r\n"
		"   -utf8-bom\r\n"
		"        Store a UTF-8 byte order mark at the start of the exported file.\r\n"
		"\r\n"
		"\r\n"
		"General options\r\n"
		"   -h, -help\r\n"
		"        Display this help.\r\n"
		"\r\n"
		"   -instance <name>\r\n"
		"        Connect to the unique Everything instance name.\r\n"
		"   -ipc1, -ipc2, -ipc3\r\n"
		"        Use IPC version 1, 2 or 3.\r\n"
		"   -pause, -more\r\n"
		"        Pause after each page of output.\r\n"
		"   -hide-empty-search-results\r\n"
		"        Don't show any results when there is no search.\r\n"
		"   -empty-search-help\r\n"
		"        Show help when no search is specified.\r\n"
		"   -timeout <milliseconds>\r\n"
		"        Timeout after the specified number of milliseconds to wait for\r\n"
		"        the Everything database to load before sending a query.\r\n"
		"        \r\n"
		"   -set-run-count <filename> <count>\r\n"
		"        Set the run count for the specified filename.\r\n"
		"   -inc-run-count <filename>\r\n"
		"        Increment the run count for the specified filename by one.\r\n"
		"   -get-run-count <filename>\r\n"
		"        Display the run count for the specified filename.\r\n"
		"   -get-result-count\r\n"
		"        Display the result count for the specified search.\r\n"
		"   -get-total-size\r\n"
		"        Display the total result size for the specified search.\r\n"
		"   -get-folder-size <filename>\r\n"
		"        Display the total folder size for the specified filename.\r\n"
		"   -save-settings, -clear-settings\r\n"
		"        Save or clear settings.\r\n"
		"   -version\r\n"
		"        Display ES major.minor.revision.build version and exit.\r\n"
		"   -get-everything-version\r\n"
		"        Display Everything major.minor.revision.build version and exit.\r\n"
		"   -exit\r\n"
		"        Exit Everything.\r\n"
		"        Returns after Everything process closes.\r\n"
		"   -save-db\r\n"
		"        Save the Everything database to disk.\r\n"
		"        Returns after saving completes.\r\n"
		"   -reindex\r\n"
		"        Force Everything to reindex.\r\n"
		"        Returns after indexing completes.\r\n"
		"   -no-result-error\r\n"
		"        Set the error level if no results are found.\r\n"
		"\r\n"
		"\r\n"
		"Notes \r\n"
		"    Internal -'s in options can be omitted, eg: -nodigitgrouping\r\n"
		"    Switches can also start with a /\r\n"
		"    Use double quotes to escape spaces and switches.\r\n"
		"    Switches can be disabled by prefixing them with no-, eg: -no-size.\r\n"
		"    Use a ^ prefix or wrap with double quotes (\") to escape \\ & | > < ^\r\n");
}

// main entry
static int _es_main(void)
{
	MSG msg;
	int ret;
	int perform_search;
	wchar_buf_t argv_wcbuf;
	wchar_buf_t search_wcbuf;
	wchar_buf_t filter_wcbuf;
	wchar_buf_t instance_name_wcbuf;
	wchar_t *get_folder_size_filename;
	pool_t local_column_color_pool;
	pool_t local_column_width_pool;
	pool_t local_column_pool;
	pool_t local_secondary_sort_pool;
	array_t local_column_color_array;
	array_t local_column_width_array;
	array_t local_column_array;
	array_t local_secondary_sort_array;
	
	wchar_buf_init(&argv_wcbuf);
	wchar_buf_init(&search_wcbuf);
	wchar_buf_init(&filter_wcbuf);
	wchar_buf_init(&instance_name_wcbuf);
	pool_init(&local_column_color_pool);
	pool_init(&local_column_width_pool);
	pool_init(&local_column_pool);
	pool_init(&local_secondary_sort_pool);
	array_init(&local_column_color_array);
	array_init(&local_column_width_array);
	array_init(&local_column_array);
	array_init(&local_secondary_sort_array);
	
	get_folder_size_filename = NULL;
	es_instance_name_wcbuf = &instance_name_wcbuf;
	es_search_wcbuf = &search_wcbuf;
	
	column_color_pool = &local_column_color_pool;
	column_width_pool = &local_column_width_pool;
	column_pool = &local_column_pool;
	secondary_sort_pool = &local_secondary_sort_pool;
	column_color_array = &local_column_color_array;
	column_width_array = &local_column_width_array;
	column_array = &local_column_array;
	secondary_sort_array = &local_secondary_sort_array;
	
	perform_search = 1;
	
	es_output_handle = GetStdHandle(STD_OUTPUT_HANDLE);
	
	es_cp = GetConsoleCP();

	// get console info.
	
	{
		DWORD mode;

		if (GetConsoleMode(es_output_handle,&mode))
		{
			CONSOLE_SCREEN_BUFFER_INFO csbi;
			
			if (GetConsoleScreenBufferInfo(es_output_handle,&csbi))
			{
				es_console_wide = csbi.srWindow.Right - csbi.srWindow.Left + 1;
				es_console_high = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;

				es_console_window_x = csbi.srWindow.Left;
				es_console_window_y = csbi.dwCursorPosition.Y;
				
				if ((!es_console_wide) || (!es_console_high))
				{
					es_console_wide = csbi.dwSize.X;
					es_console_high = csbi.dwSize.Y;
				}
				
				es_console_size_high = csbi.dwSize.Y;
				
				es_default_attributes = csbi.wAttributes;
			}
		}

		if (GetFileType(es_output_handle) == FILE_TYPE_CHAR)
		{
			es_output_is_char = 1;
		}
	}
			
	// load default settings.	
	_es_load_settings();
	
	es_command_line = GetCommandLine();
	
/*
	// code page test
	
	printf("CP %u\n",GetConsoleCP());
	printf("CP output %u\n",GetConsoleOutputCP());
	
	MessageBox(0,command_line,L"command line",MB_OK);
	return 0;
//	printf("command line %S\n",command_line);
*/
	// expect the executable name in the first argv.
	if (es_command_line)
	{
		_es_get_argv(&argv_wcbuf);
	}
	
	if (es_command_line)
	{
		_es_get_argv(&argv_wcbuf);
		
		if (es_command_line)
		{
			// I am often calling:
			// es -
			// to get help.
			// only allow - as literal when there's more search text.

			// this is too smart -user will not know why it works by itself and doesn't when other text is present.
			/*
			if (!*command_line)
			{
				// don't treat / as a switch
				// allow /downloads to search for "/downloads"
				if ((wcscmp(es_argv,L"-") == 0) || (wcsicmp(es_argv,L"--") == 0))
				{
					// only a single - or --
					// user requested help
					_es_help();
					
					goto exit;
				}
			}
*/
			for(;;)
			{
				DWORD property_id;
				
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"set-run-count"))
				{
					_es_expect_command_argv(&argv_wcbuf);
				
					es_run_history_size = sizeof(EVERYTHING_IPC_RUN_HISTORY);
					es_run_history_size = safe_size_add(es_run_history_size,safe_size_mul_sizeof_wchar(safe_size_add_one(argv_wcbuf.length_in_wchars)));

					es_run_history_data = mem_alloc(es_run_history_size);

					wchar_string_copy_wchar_string_n((wchar_t *)(((EVERYTHING_IPC_RUN_HISTORY *)es_run_history_data)+1),argv_wcbuf.buf,argv_wcbuf.length_in_wchars);
					
					_es_expect_command_argv(&argv_wcbuf);
					
					((EVERYTHING_IPC_RUN_HISTORY *)es_run_history_data)->run_count = wchar_string_to_int(argv_wcbuf.buf);
					es_run_history_command = EVERYTHING_IPC_COPYDATA_SET_RUN_COUNTW;
				}
				else			
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"inc-run-count"))
				{
					_es_expect_command_argv(&argv_wcbuf);
					
					es_run_history_size = (wchar_string_get_length_in_wchars(argv_wcbuf.buf) + 1) * sizeof(wchar_t);
					es_run_history_data = wchar_string_alloc_wchar_string_n(argv_wcbuf.buf,argv_wcbuf.length_in_wchars);
					es_run_history_command = EVERYTHING_IPC_COPYDATA_INC_RUN_COUNTW;
				}
				else			
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"get-run-count"))
				{
					_es_expect_command_argv(&argv_wcbuf);

					es_run_history_size = (wchar_string_get_length_in_wchars(argv_wcbuf.buf) + 1) * sizeof(wchar_t);
					es_run_history_data = wchar_string_alloc_wchar_string_n(argv_wcbuf.buf,argv_wcbuf.length_in_wchars);
					es_run_history_command = EVERYTHING_IPC_COPYDATA_GET_RUN_COUNTW;
				}
				else
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"get-folder-size"))
				{
					_es_expect_command_argv(&argv_wcbuf);
					
					get_folder_size_filename = wchar_string_alloc_wchar_string_n(argv_wcbuf.buf,argv_wcbuf.length_in_wchars);
				}
				else
				if ((_es_check_option_utf8_string(argv_wcbuf.buf,"r")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"regex")))
				{
					_es_expect_argv(&argv_wcbuf);
					
					if (search_wcbuf.length_in_wchars)
					{
						wchar_buf_cat_wchar(&search_wcbuf,' ');
					}
							
					wchar_buf_cat_utf8_string(&search_wcbuf,"regex:");
					wchar_buf_cat_wchar_string_n(&search_wcbuf,argv_wcbuf.buf,argv_wcbuf.length_in_wchars);
				}
				else
				if ((_es_check_option_utf8_string(argv_wcbuf.buf,"i")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"case")))
				{
					_es_match_case = 1;
				}
				else
				if ((_es_check_option_utf8_string(argv_wcbuf.buf,"no-i")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"no-case")))
				{
					_es_match_case = 0;
				}
				else
				if ((_es_check_option_utf8_string(argv_wcbuf.buf,"a")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"diacritics")))
				{
					_es_match_diacritics = 1;
				}
				else
				if ((_es_check_option_utf8_string(argv_wcbuf.buf,"no-a")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"no-diacritics")))
				{
					_es_match_diacritics = 0;
				}
				else
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"prefix"))
				{
					_es_match_prefix = 1;
				}
				else
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"no-prefix"))
				{
					_es_match_prefix = 0;
				}
				else
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"suffix"))
				{
					_es_match_suffix = 1;
				}
				else
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"no-suffix"))
				{
					_es_match_suffix = 0;
				}
				else
				if ((_es_check_option_utf8_string(argv_wcbuf.buf,"ignore-punctuation")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"ignore-punc")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"no-punctuation")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"no-punc")))
				{
					_es_ignore_punctuation = 1;
				}
				else
				if ((_es_check_option_utf8_string(argv_wcbuf.buf,"no-ignore-punctuation")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"no-ignore-punc")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"punctuation")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"punc")))
				{
					_es_ignore_punctuation = 0;
				}
				else
				if ((_es_check_option_utf8_string(argv_wcbuf.buf,"ignore-white-space")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"ignore-ws")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"no-white-space")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"no-ws")))
				{
					_es_ignore_whitespace = 1;
				}
				else
				if ((_es_check_option_utf8_string(argv_wcbuf.buf,"no-ignore-white-space")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"no-ignore-ws")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"white-space")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"ws")))
				{
					_es_ignore_whitespace = 0;
				}
				else
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"instance"))
				{
					_es_expect_command_argv(&argv_wcbuf);
					
					if (argv_wcbuf.length_in_wchars)
					{
						wchar_buf_copy_wchar_string_n(&instance_name_wcbuf,argv_wcbuf.buf,argv_wcbuf.length_in_wchars);
					}
				}
				else
				if ((_es_check_option_utf8_string(argv_wcbuf.buf,"exit")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"quit")))
				{
					_es_exit_everything = 1;
				}
				else
				if ((_es_check_option_utf8_string(argv_wcbuf.buf,"re-index")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"re-build")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"update")))
				{
					es_reindex = 1;
				}
				else
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"save-db"))
				{
					es_save_db = 1;
				}
				else
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"highlight-color"))
				{
					_es_expect_command_argv(&argv_wcbuf);
					
					_es_highlight_color = wchar_string_to_int(argv_wcbuf.buf);
				}
				else
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"highlight"))
				{
					_es_highlight = 1;
				}
				else			
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"no-highlight"))
				{
					_es_highlight = 0;
				}
				else
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"m3u"))
				{
					_es_export_type = ES_EXPORT_TYPE_M3U;
				}
				else				
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"export-m3u"))
				{
					_es_expect_command_argv(&argv_wcbuf);
					
					_es_export_file = os_create_file(argv_wcbuf.buf);
					if (_es_export_file != INVALID_HANDLE_VALUE)
					{
						_es_export_type = ES_EXPORT_TYPE_M3U;
					}
					else
					{
						es_fatal(ES_ERROR_CREATE_FILE);
					}
				}
				else					
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"m3u8"))
				{
					_es_export_type = ES_EXPORT_TYPE_M3U8;
				}
				else				
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"export-m3u8"))
				{
					_es_expect_command_argv(&argv_wcbuf);
					
					_es_export_file = os_create_file(argv_wcbuf.buf);
					if (_es_export_file != INVALID_HANDLE_VALUE)
					{
						_es_export_type = ES_EXPORT_TYPE_M3U8;
					}
					else
					{
						es_fatal(ES_ERROR_CREATE_FILE);
					}
				}
				else		
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"csv"))
				{
					_es_export_type = ES_EXPORT_TYPE_CSV;
				}
				else				
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"tsv"))
				{
					_es_export_type = ES_EXPORT_TYPE_TSV;
				}
				else				
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"json"))
				{
					_es_export_type = ES_EXPORT_TYPE_JSON;
				}
				else				
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"export-csv"))
				{
					_es_expect_command_argv(&argv_wcbuf);
					
					_es_export_file = os_create_file(argv_wcbuf.buf);
					if (_es_export_file != INVALID_HANDLE_VALUE)
					{
						_es_export_type = ES_EXPORT_TYPE_CSV;
					}
					else
					{
						es_fatal(ES_ERROR_CREATE_FILE);
					}
				}
				else		
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"export-tsv"))
				{
					_es_expect_command_argv(&argv_wcbuf);
					
					_es_export_file = os_create_file(argv_wcbuf.buf);
					if (_es_export_file != INVALID_HANDLE_VALUE)
					{
						_es_export_type = ES_EXPORT_TYPE_TSV;
					}
					else
					{
						es_fatal(ES_ERROR_CREATE_FILE);
					}
				}
				else		
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"export-json"))
				{
					_es_expect_command_argv(&argv_wcbuf);
					
					_es_export_file = os_create_file(argv_wcbuf.buf);
					if (_es_export_file != INVALID_HANDLE_VALUE)
					{
						_es_export_type = ES_EXPORT_TYPE_JSON;
					}
					else
					{
						es_fatal(ES_ERROR_CREATE_FILE);
					}
				}
				else		
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"efu"))
				{
					_es_export_type = ES_EXPORT_TYPE_EFU;
				}
				else		
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"export-efu"))
				{
					_es_expect_command_argv(&argv_wcbuf);
					
					_es_export_file = os_create_file(argv_wcbuf.buf);
					if (_es_export_file != INVALID_HANDLE_VALUE)
					{
						_es_export_type = ES_EXPORT_TYPE_EFU;
					}
					else
					{
						es_fatal(ES_ERROR_CREATE_FILE);
					}
				}
				else		
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"txt"))
				{
					_es_export_type = ES_EXPORT_TYPE_TXT;
				}
				else				
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"export-txt"))
				{
					_es_expect_command_argv(&argv_wcbuf);
					
					_es_export_file = os_create_file(argv_wcbuf.buf);
					if (_es_export_file != INVALID_HANDLE_VALUE)
					{
						_es_export_type = ES_EXPORT_TYPE_TXT;
					}
					else
					{
						es_fatal(ES_ERROR_CREATE_FILE);
					}
				}
				else
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"cp"))
				{
					_es_expect_command_argv(&argv_wcbuf);
					
					es_cp = wchar_string_to_int(argv_wcbuf.buf);
				}
				else
				if ((_es_check_option_utf8_string(argv_wcbuf.buf,"w")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"ww")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"whole-word")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"whole-words")))
				{
					_es_match_whole_word = 1;
				}
				else
				if ((_es_check_option_utf8_string(argv_wcbuf.buf,"no-w")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"no-ww")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"no-whole-word")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"no-whole-words")))
				{
					_es_match_whole_word = 0;
				}
				else
				if ((_es_check_option_utf8_string(argv_wcbuf.buf,"p")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"match-path")))
				{
					_es_match_path = 1;
				}
				else
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"no-p"))
				{
					_es_match_path = 0;
				}
				else
				if ((_es_check_option_utf8_string(argv_wcbuf.buf,"file-name-width")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"file-name-wide")))
				{
					_es_expect_command_argv(&argv_wcbuf);

					column_width_set(EVERYTHING3_PROPERTY_ID_PATH_AND_NAME,wchar_string_to_int(argv_wcbuf.buf));
				}
				else
				if ((_es_check_option_utf8_string(argv_wcbuf.buf,"name-width")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"name-wide")))
				{
					_es_expect_command_argv(&argv_wcbuf);
					
					column_width_set(EVERYTHING3_PROPERTY_ID_NAME,wchar_string_to_int(argv_wcbuf.buf));
				}
				else
				if ((_es_check_option_utf8_string(argv_wcbuf.buf,"path-width")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"path-wide")))
				{
					_es_expect_command_argv(&argv_wcbuf);
					
					column_width_set(EVERYTHING3_PROPERTY_ID_PATH,wchar_string_to_int(argv_wcbuf.buf));
				}
				else
				if ((_es_check_option_utf8_string(argv_wcbuf.buf,"extension-width")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"extension-wide")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"ext-width")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"ext-wide")))
				{
					_es_expect_command_argv(&argv_wcbuf);
					
					column_width_set(EVERYTHING3_PROPERTY_ID_EXTENSION,wchar_string_to_int(argv_wcbuf.buf));
				}
				else
				if ((_es_check_option_utf8_string(argv_wcbuf.buf,"size-width")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"size-wide")))
				{
					_es_expect_command_argv(&argv_wcbuf);
					
					column_width_set(EVERYTHING3_PROPERTY_ID_SIZE,wchar_string_to_int(argv_wcbuf.buf));
				}
				else
				if ((_es_check_option_utf8_string(argv_wcbuf.buf,"date-created-width")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"date-created-wide")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"dc-width")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"dc-wide")))
				{
					_es_expect_command_argv(&argv_wcbuf);
					
					column_width_set(EVERYTHING3_PROPERTY_ID_DATE_CREATED,wchar_string_to_int(argv_wcbuf.buf));
				}
				else
				if ((_es_check_option_utf8_string(argv_wcbuf.buf,"date-modified-width")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"date-modified-wide")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"dm-width")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"dm-wide")))
				{
					_es_expect_command_argv(&argv_wcbuf);
					
					column_width_set(EVERYTHING3_PROPERTY_ID_DATE_MODIFIED,wchar_string_to_int(argv_wcbuf.buf));
				}
				else
				if ((_es_check_option_utf8_string(argv_wcbuf.buf,"date-accessed-width")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"date-accessed-wide")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"da-width")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"da-wide")))
				{
					_es_expect_command_argv(&argv_wcbuf);
					
					column_width_set(EVERYTHING3_PROPERTY_ID_DATE_ACCESSED,wchar_string_to_int(argv_wcbuf.buf));
				}
				else
				if ((_es_check_option_utf8_string(argv_wcbuf.buf,"attributes-width")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"attributes-wide")))
				{
					_es_expect_command_argv(&argv_wcbuf);
					
					column_width_set(EVERYTHING3_PROPERTY_ID_ATTRIBUTES,wchar_string_to_int(argv_wcbuf.buf));
				}
				else
				if ((_es_check_option_utf8_string(argv_wcbuf.buf,"file-list-file-name-width")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"file-list-file-name-wide")))
				{
					_es_expect_command_argv(&argv_wcbuf);
					
					column_width_set(EVERYTHING3_PROPERTY_ID_FILE_LIST_NAME,wchar_string_to_int(argv_wcbuf.buf));
				}
				else
				if ((_es_check_option_utf8_string(argv_wcbuf.buf,"run-count-width")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"run-count-wide")))
				{
					_es_expect_command_argv(&argv_wcbuf);
					
					column_width_set(EVERYTHING3_PROPERTY_ID_RUN_COUNT,wchar_string_to_int(argv_wcbuf.buf));
				}
				else
				if ((_es_check_option_utf8_string(argv_wcbuf.buf,"date-run-width")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"date-run-wide")))
				{
					_es_expect_command_argv(&argv_wcbuf);
					
					column_width_set(EVERYTHING3_PROPERTY_ID_DATE_RUN,wchar_string_to_int(argv_wcbuf.buf));
				}
				else
				if ((_es_check_option_utf8_string(argv_wcbuf.buf,"date-recently-changed-width")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"date-recently-changed-wide")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"rc-width")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"rc-wide")))
				{
					_es_expect_command_argv(&argv_wcbuf);
					
					column_width_set(EVERYTHING3_PROPERTY_ID_DATE_RECENTLY_CHANGED,wchar_string_to_int(argv_wcbuf.buf));
				}
				else
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"size-leading-zero"))
				{
					es_size_leading_zero = 1;
				}
				else
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"no-size-leading-zero"))
				{
					es_size_leading_zero = 0;
				}
				else
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"run-count-leading-zero"))
				{
					es_run_count_leading_zero = 1;
				}
				else
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"no-run-count-leading-zero"))
				{
					es_run_count_leading_zero = 0;
				}
				else
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"no-digit-grouping"))
				{
					es_digit_grouping = 0;
				}
				else
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"digit-grouping"))
				{
					es_digit_grouping = 1;
				}
				else
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"size-format"))
				{
					_es_expect_command_argv(&argv_wcbuf);
					
					es_size_format = wchar_string_to_int(argv_wcbuf.buf);
				}
				else
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"date-format"))
				{
					_es_expect_command_argv(&argv_wcbuf);

					es_display_date_format = wchar_string_to_int(argv_wcbuf.buf);
				}
				else			
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"export-date-format"))
				{
					_es_expect_command_argv(&argv_wcbuf);

					_es_export_date_format = wchar_string_to_int(argv_wcbuf.buf);
				}
				else			
				if ((_es_check_option_utf8_string(argv_wcbuf.buf,"pause")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"more")))
				{
					es_pause = 1;
				}
				else
				if ((_es_check_option_utf8_string(argv_wcbuf.buf,"no-pause")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"no-more")))
				{
					es_pause = 0;
				}
				else
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"empty-search-help"))
				{
					es_empty_search_help = 1;
					es_hide_empty_search_results = 0;
				}
				else
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"no-empty-search-help"))
				{
					es_empty_search_help = 0;
				}
				else
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"hide-empty-search-results"))
				{
					es_hide_empty_search_results = 1;
					es_empty_search_help = 0;
				}
				else
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"no-hide-empty-search-results"))
				{
					es_hide_empty_search_results = 0;
				}
				else
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"save-settings"))
				{
					es_save = 1;
				}
				else
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"clear-settings"))
				{
					wchar_buf_t ini_filename_wcbuf;

					wchar_buf_init(&ini_filename_wcbuf);
					
					if (config_get_filename(0,0,&ini_filename_wcbuf))
					{
						DeleteFile(ini_filename_wcbuf.buf);

						_es_output_noncell_utf8_string("Settings saved.\r\n");
					}
					
					wchar_buf_kill(&ini_filename_wcbuf);
					
					goto exit;
				}
				else
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"path"))
				{
					wchar_buf_t path_wcbuf;

					// make sure we process this before _es_check_column_param
					// otherwise _es_check_column_param will eat -path.
					wchar_buf_init(&path_wcbuf);

					_es_expect_command_argv(&argv_wcbuf);
					
					// relative path.
					os_get_expanded_full_path_name(argv_wcbuf.buf,&path_wcbuf);
		
					if (filter_wcbuf.length_in_wchars)
					{
						wchar_buf_cat_wchar(&filter_wcbuf,' ');
					}

					wchar_buf_cat_utf8_string(&filter_wcbuf,"\"");
					wchar_buf_cat_wchar_string(&filter_wcbuf,path_wcbuf.buf);

					if (!wchar_string_is_trailing_path_separator_n(path_wcbuf.buf,path_wcbuf.length_in_wchars))
					{
						wchar_buf_cat_wchar(&filter_wcbuf,wchar_string_get_path_separator_from_root(path_wcbuf.buf));
					}
					
					wchar_buf_cat_utf8_string(&filter_wcbuf,"\"");

					wchar_buf_kill(&path_wcbuf);
				}
				else
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"parent-path"))
				{
					wchar_buf_t path_wcbuf;

					wchar_buf_init(&path_wcbuf);
				
					_es_expect_command_argv(&argv_wcbuf);
					
					// relative path.
					os_get_expanded_full_path_name(argv_wcbuf.buf,&path_wcbuf);
					wchar_buf_remove_file_spec(&path_wcbuf);
					
					if (filter_wcbuf.length_in_wchars)
					{
						wchar_buf_cat_wchar(&filter_wcbuf,' ');
					}

					wchar_buf_cat_utf8_string(&filter_wcbuf,"\"");
					wchar_buf_cat_wchar_string(&filter_wcbuf,path_wcbuf.buf);

					if (!wchar_string_is_trailing_path_separator_n(path_wcbuf.buf,path_wcbuf.length_in_wchars))
					{
						wchar_buf_cat_wchar(&filter_wcbuf,wchar_string_get_path_separator_from_root(path_wcbuf.buf));
					}

					wchar_buf_cat_utf8_string(&filter_wcbuf,"\"");

					wchar_buf_kill(&path_wcbuf);
				}
				else
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"parent"))
				{
					wchar_buf_t path_wcbuf;

					wchar_buf_init(&path_wcbuf);
					
					_es_expect_command_argv(&argv_wcbuf);
					
					// relative path.
					os_get_expanded_full_path_name(argv_wcbuf.buf,&path_wcbuf);
					
					if (filter_wcbuf.length_in_wchars)
					{
						wchar_buf_cat_wchar(&filter_wcbuf,' ');
					}

					wchar_buf_cat_utf8_string(&filter_wcbuf,"parent:\"");
					wchar_buf_cat_wchar_string(&filter_wcbuf,path_wcbuf.buf);

					if (!wchar_string_is_trailing_path_separator_n(path_wcbuf.buf,path_wcbuf.length_in_wchars))
					{
						wchar_buf_cat_wchar(&filter_wcbuf,wchar_string_get_path_separator_from_root(path_wcbuf.buf));
					}
					
					wchar_buf_cat_utf8_string(&filter_wcbuf,"\"");
					
					wchar_buf_kill(&path_wcbuf);
				}
				else
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"sort-ascending"))
				{
					_es_primary_sort_ascending = 1;
				}
				else
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"sort-descending"))
				{
					_es_primary_sort_ascending = -1;
				}
				else
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"sort"))
				{	
					_es_expect_command_argv(&argv_wcbuf);
					
					if (!_es_set_sort_list(argv_wcbuf.buf,0))
					{
						es_fatal(ES_ERROR_EXPECTED_SWITCH_PARAMETER);
					}
				}
				else
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"columns"))
				{	
					_es_expect_command_argv(&argv_wcbuf);
					
					if (!_es_set_columns(argv_wcbuf.buf,0,0))
					{
						es_fatal(ES_ERROR_EXPECTED_SWITCH_PARAMETER);
					}
				}
				else
				if ((_es_check_option_utf8_string(argv_wcbuf.buf,"add-columns")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"add-column")))
				{	
					_es_expect_command_argv(&argv_wcbuf);
					
					if (!_es_set_columns(argv_wcbuf.buf,1,0))
					{
						es_fatal(ES_ERROR_EXPECTED_SWITCH_PARAMETER);
					}
				}
				else
				if ((_es_check_option_utf8_string(argv_wcbuf.buf,"remove-columns")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"remove-column")))
				{	
					_es_expect_command_argv(&argv_wcbuf);
					
					if (!_es_set_columns(argv_wcbuf.buf,2,0))
					{
						es_fatal(ES_ERROR_EXPECTED_SWITCH_PARAMETER);
					}
				}
				else
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"column-colors"))
				{	
					_es_expect_command_argv(&argv_wcbuf);
					
					if (!_es_set_column_colors(argv_wcbuf.buf,0))
					{
						es_fatal(ES_ERROR_EXPECTED_SWITCH_PARAMETER);
					}
				}
				else
				if ((_es_check_option_utf8_string(argv_wcbuf.buf,"add-column-colors")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"add-column-color")))
				{	
					_es_expect_command_argv(&argv_wcbuf);
					
					if (!_es_set_column_colors(argv_wcbuf.buf,1))
					{
						es_fatal(ES_ERROR_EXPECTED_SWITCH_PARAMETER);
					}
				}
				else
				if ((_es_check_option_utf8_string(argv_wcbuf.buf,"remove-column-colors")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"remove-column-color")))
				{	
					_es_expect_command_argv(&argv_wcbuf);
					
					if (!_es_set_column_colors(argv_wcbuf.buf,2))
					{
						es_fatal(ES_ERROR_EXPECTED_SWITCH_PARAMETER);
					}
				}
				else
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"column-widths"))
				{	
					_es_expect_command_argv(&argv_wcbuf);
					
					if (!_es_set_column_widths(argv_wcbuf.buf,0))
					{
						es_fatal(ES_ERROR_EXPECTED_SWITCH_PARAMETER);
					}
				}
				else
				if ((_es_check_option_utf8_string(argv_wcbuf.buf,"add-column-widths")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"add-column-width")))
				{	
					_es_expect_command_argv(&argv_wcbuf);
					
					if (!_es_set_column_widths(argv_wcbuf.buf,1))
					{
						es_fatal(ES_ERROR_EXPECTED_SWITCH_PARAMETER);
					}
				}
				else
				if ((_es_check_option_utf8_string(argv_wcbuf.buf,"remove-column-widths")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"remove-column-width")))
				{	
					_es_expect_command_argv(&argv_wcbuf);
					
					if (!_es_set_column_widths(argv_wcbuf.buf,2))
					{
						es_fatal(ES_ERROR_EXPECTED_SWITCH_PARAMETER);
					}
				}
				else
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"ipc1"))
				{	
					es_ipc_version = ES_IPC_VERSION_FLAG_IPC1; 
				}
				else
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"ipc2"))
				{	
					es_ipc_version = ES_IPC_VERSION_FLAG_IPC2; 
				}
				else
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"ipc3"))
				{	
					es_ipc_version = ES_IPC_VERSION_FLAG_IPC3; 
				}
				else
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"header"))
				{	
					es_header = 1; 
				}
				else
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"no-header"))
				{	
					es_header = -1; 
				}
				else
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"double-quote"))
				{	
					es_double_quote = 1; 
					es_csv_double_quote = 1;
				}
				else
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"no-double-quote"))
				{	
					es_double_quote = 0; 
					es_csv_double_quote = 0;
				}
				else
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"version"))
				{	
					_es_output_noncell_utf8_string(VERSION_TEXT "\r\n");

					goto exit;
				}
				else
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"get-everything-version"))
				{
					es_get_everything_version = 1;
				}
				else
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"utf8-bom"))
				{
					es_utf8_bom = 1;
				}
				else
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"no-utf8-bom"))
				{
					es_utf8_bom = 0;
				}
				else
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"folder-append-path-separator"))
				{
					es_folder_append_path_separator = 1;
				}
				else
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"no-folder-append-path-separator"))
				{
					es_folder_append_path_separator = 0;
				}
				else
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"nul"))
				{
					es_newline_type = 2;
				}
				else
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"lf"))
				{
					es_newline_type = 1;
				}
				else
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"crlf"))
				{
					es_newline_type = 0;
				}
				else
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"on"))
				{	
					_es_primary_sort_property_id = EVERYTHING3_PROPERTY_ID_NAME;
					_es_primary_sort_ascending = 1;
					secondary_sort_clear_all();
				}
				else
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"o-n"))
				{	
					_es_primary_sort_property_id = EVERYTHING3_PROPERTY_ID_NAME;
					_es_primary_sort_ascending = -1;
					secondary_sort_clear_all();
				}
				else
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"os"))
				{	
					_es_primary_sort_property_id = EVERYTHING3_PROPERTY_ID_SIZE;
					_es_primary_sort_ascending = 1;
					secondary_sort_clear_all();
				}
				else
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"o-s"))
				{	
					_es_primary_sort_property_id = EVERYTHING3_PROPERTY_ID_SIZE;
					_es_primary_sort_ascending = -1;
					secondary_sort_clear_all();
				}
				else
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"oe"))
				{	
					_es_primary_sort_property_id = EVERYTHING3_PROPERTY_ID_EXTENSION;
					_es_primary_sort_ascending = 1;
					secondary_sort_clear_all();
				}
				else
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"o-e"))
				{	
					_es_primary_sort_property_id = EVERYTHING3_PROPERTY_ID_EXTENSION;
					_es_primary_sort_ascending = -1;
					secondary_sort_clear_all();
				}
				else
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"od"))
				{	
					_es_primary_sort_property_id = EVERYTHING3_PROPERTY_ID_DATE_MODIFIED;
					_es_primary_sort_ascending = 1;
					secondary_sort_clear_all();
				}
				else
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"o-d"))
				{	
					_es_primary_sort_property_id = EVERYTHING3_PROPERTY_ID_DATE_MODIFIED;
					_es_primary_sort_ascending = -1;
					secondary_sort_clear_all();
				}
				else
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"s"))
				{
					_es_primary_sort_property_id = EVERYTHING3_PROPERTY_ID_PATH;
					_es_primary_sort_ascending = 1;
					secondary_sort_clear_all();
				}
				else
				if ((_es_check_option_utf8_string(argv_wcbuf.buf,"n")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"max-results")))
				{
					_es_expect_command_argv(&argv_wcbuf);
					
					es_max_results = safe_size_from_uint64(wchar_string_to_uint64(argv_wcbuf.buf));
				}
				else
				if ((_es_check_option_utf8_string(argv_wcbuf.buf,"o")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"offset")))
				{
					_es_expect_command_argv(&argv_wcbuf);
					
					es_offset = safe_size_from_uint64(wchar_string_to_uint64(argv_wcbuf.buf));
				}
				else
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"time-out"))
				{
					_es_expect_command_argv(&argv_wcbuf);
					
					es_timeout = (DWORD)wchar_string_to_int(argv_wcbuf.buf);
				}
				else
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"no-time-out"))
				{
					es_timeout = 0;
				}
				else
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"ad"))
				{	
					// add folder:
					_es_append_filter(&filter_wcbuf,"folder:");
				}
				else
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"a-d"))
				{	
					// add folder:
					_es_append_filter(&filter_wcbuf,"file:");
				}
				else
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"get-result-count"))
				{
					es_get_result_count = 1;
				}
				else
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"get-total-size"))
				{
					es_get_total_size = 1;
				}
				else
				if ((_es_check_option_utf8_string(argv_wcbuf.buf,"no-result-error")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"no-results-error")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"error-on-no-results")))
				{
					es_no_result_error = 1;
				}
				else
				if ((_es_check_option_utf8_string(argv_wcbuf.buf,"q")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"search")))
				{
					// this removes quotes from the search string.
					_es_expect_command_argv(&argv_wcbuf);
							
					if (search_wcbuf.length_in_wchars)
					{
						wchar_buf_cat_wchar(&search_wcbuf,' ');
					}

					wchar_buf_cat_wchar_string(&search_wcbuf,argv_wcbuf.buf);
				}
				else
				if ((_es_check_option_utf8_string(argv_wcbuf.buf,"q*")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"search*")))
				{
					// eat the rest.
					// this would do the same as a stop parsing switches command line option
					// like 7zip --
					// or powershell --%
					// this doesn't remove quotes.
					es_command_line = wchar_string_skip_ws(es_command_line);
							
					if (search_wcbuf.length_in_wchars)
					{
						wchar_buf_cat_wchar(&search_wcbuf,' ');
					}

					wchar_buf_cat_wchar_string(&search_wcbuf,es_command_line);
					
					break;
				}
				else
			/*	if (_es_check_option_utf8_string(argv_wcbuf.buf,"%"))
				{	
					// this would conflict with powershell..
					for(;;)
					{
						const wchar_t *s;

						command_line = _es_get_argv(command_line);
						if (!command_line)
						{
							break;
						}
						
						if (search_wcbuf.length_in_wchars)
						{
							wchar_buf_cat_wchar(&search_wcbuf,' ');
						}


						// copy append to search
						s = argv_wcbuf.buf;
						while(*s)
						{
							if (d < e) *d++ = *s;
							s++;
						}
					}
					
					break;
				}
				else
			*/	
				if ((_es_check_option_utf8_string(argv_wcbuf.buf,"?")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"help")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"h")))
				{
					// user requested help
					_es_help();
					
					goto exit;
				}
				else
				if (_es_check_color_param(argv_wcbuf.buf,&property_id))
				{
					_es_expect_command_argv(&argv_wcbuf);
					
					column_color_set(property_id,wchar_string_to_int(argv_wcbuf.buf));
				}
				else
				if (_es_check_column_param(argv_wcbuf.buf))
				{
				}				
				else
				if (_es_check_sorts(argv_wcbuf.buf))
				{
				}
				else
				if (((argv_wcbuf.buf[0] == '-') || (argv_wcbuf.buf[0] == '/')) && (argv_wcbuf.buf[1] == 'a') && (argv_wcbuf.buf[2]))
				{
					const wchar_t *p;
					wchar_buf_t attrib_wcbuf;
					wchar_buf_t notattrib_wcbuf;

					// don't match -attrib
					// do this after adding columns.
					wchar_buf_init(&attrib_wcbuf);
					wchar_buf_init(&notattrib_wcbuf);
					p = argv_wcbuf.buf + 2;
					
					// handle only A-Za-z
					// and not (-)
					while(*p)
					{
						if ((*p == '-') && (p[1]))
						{
							int attrib_ch;
							
							attrib_ch = unicode_ascii_to_lower(p[1]);
							
							if ((attrib_ch >= 'a') && (attrib_ch <= 'z'))
							{
								wchar_buf_cat_wchar(&notattrib_wcbuf,attrib_ch);
							}
							else
							{
								es_fatal(ES_ERROR_EXPECTED_SWITCH_PARAMETER);
							}
							
							p += 2;
						}
						else
						{
							int attrib_ch;
							
							attrib_ch = unicode_ascii_to_lower(*p);
							
							if ((attrib_ch >= 'a') && (attrib_ch <= 'z'))
							{
								wchar_buf_cat_wchar(&notattrib_wcbuf,attrib_ch);
							}
							else
							{
								es_fatal(ES_ERROR_EXPECTED_SWITCH_PARAMETER);
							}
							
							wchar_buf_cat_wchar(&attrib_wcbuf,attrib_ch);
							
							p++;
						}
					}
					
					// copy append to search
					if (attrib_wcbuf.length_in_wchars)
					{
						if (search_wcbuf.length_in_wchars)
						{
							wchar_buf_cat_wchar(&search_wcbuf,' ');
						}

						wchar_buf_cat_utf8_string(&search_wcbuf,"attrib:");
						wchar_buf_cat_wchar_string_n(&search_wcbuf,attrib_wcbuf.buf,attrib_wcbuf.length_in_wchars);
					}

					// copy not append to search
					if (notattrib_wcbuf.length_in_wchars)
					{
						if (search_wcbuf.length_in_wchars)
						{
							wchar_buf_cat_wchar(&search_wcbuf,' ');
						}

						wchar_buf_cat_utf8_string(&search_wcbuf,"!attrib:");
						wchar_buf_cat_wchar_string_n(&search_wcbuf,notattrib_wcbuf.buf,notattrib_wcbuf.length_in_wchars);
					}

					wchar_buf_kill(&notattrib_wcbuf);
					wchar_buf_kill(&attrib_wcbuf);
				}
				else
				if ((argv_wcbuf.buf[0] == '-') && (!_es_is_literal_switch(argv_wcbuf.buf)))
				{
					// unknown command
					// allow /downloads to search for "\downloads" for now
					es_fatal(ES_ERROR_UNKNOWN_SWITCH);
				}
				else
				{
					if (search_wcbuf.length_in_wchars)
					{
						wchar_buf_cat_wchar(&search_wcbuf,' ');
					}

					// copy append to search
					wchar_buf_cat_wchar_string(&search_wcbuf,argv_wcbuf.buf);
				}

				_es_get_argv(&argv_wcbuf);
				if (!es_command_line)
				{
					break;
				}
			}
		}
	}
	
	// save settings.
	if (es_save)
	{
		if (_es_save_settings())
		{
			_es_output_noncell_utf8_string("Settings saved.\r\n");
		}
		else
		{
			es_fatal(ES_ERROR_CREATE_FILE);
		}
		
		perform_search = 0;
	}
	
	// get everything version
	if (es_get_everything_version)
	{
		es_everything_hwnd = _es_find_ipc_window();
		
		if (es_everything_hwnd)
		{
			int major;
			int minor;
			int revision;
			int build;

			// wait for DB_IS_LOADED so we don't get 0 results
			major = (int)SendMessage(es_everything_hwnd,EVERYTHING_WM_IPC,EVERYTHING_IPC_GET_MAJOR_VERSION,0);
			minor = (int)SendMessage(es_everything_hwnd,EVERYTHING_WM_IPC,EVERYTHING_IPC_GET_MINOR_VERSION,0);
			revision = (int)SendMessage(es_everything_hwnd,EVERYTHING_WM_IPC,EVERYTHING_IPC_GET_REVISION,0);
			build = (int)SendMessage(es_everything_hwnd,EVERYTHING_WM_IPC,EVERYTHING_IPC_GET_BUILD_NUMBER,0);
				
			_es_output_noncell_printf("%u.%u.%u.%u\r\n",major,minor,revision,build);
		}
		else
		{
			es_fatal(ES_ERROR_NO_IPC);
		}
		
		perform_search = 0;
	}
	
	// reindex?
	if (es_reindex)
	{
		es_everything_hwnd = _es_find_ipc_window();
		
		if (es_everything_hwnd)
		{
			SendMessage(es_everything_hwnd,EVERYTHING_WM_IPC,EVERYTHING_IPC_REBUILD_DB,0);
			
			// poll until db is available.
			_es_wait_for_db_loaded();
		}
		else
		{
			es_fatal(ES_ERROR_NO_IPC);
		}

		perform_search = 0;
	}
	
	// run history command
	if (es_run_history_command)
	{
		_es_do_run_history_command();

		perform_search = 0;
	}
	
	if (get_folder_size_filename)
	{
		_es_get_folder_size(get_folder_size_filename);
		
		mem_free(get_folder_size_filename);

		perform_search = 0;
	}
	
	// save db
	// do this after a reindex.
	if (es_save_db)
	{
		es_everything_hwnd = _es_find_ipc_window();
		
		if (es_everything_hwnd)
		{
			SendMessage(es_everything_hwnd,EVERYTHING_WM_IPC,EVERYTHING_IPC_SAVE_DB,0);
			
			// wait until not busy..
			_es_wait_for_db_not_busy();
		}
		else
		{
			es_fatal(ES_ERROR_NO_IPC);
		}

		perform_search = 0;
	}
	
	// Exit Everything?
	if (_es_exit_everything)
	{
		es_everything_hwnd = _es_find_ipc_window();
		
		if (es_everything_hwnd)
		{
			DWORD dwProcessId;

			// wait for Everything to exit.
			if (GetWindowThreadProcessId(es_everything_hwnd,&dwProcessId))
			{
				HANDLE h;
				
				h = OpenProcess(SYNCHRONIZE,FALSE,dwProcessId);
				if (h)
				{
					SendMessage(es_everything_hwnd,WM_CLOSE,0,0);

					WaitForSingleObject(h,es_timeout ? es_timeout : INFINITE);
				
					CloseHandle(h);
				}
				else
				{
					es_fatal(ES_ERROR_IPC_ERROR);
				}
			}
			else
			{
				es_fatal(ES_ERROR_IPC_ERROR);
			}
		}
		else
		{
			es_fatal(ES_ERROR_NO_IPC);
		}
		
		perform_search = 0;
	}
	
	if (perform_search)
	{
		// empty search?
		// if max results is set, treat the search as non-empty.
		// -useful if you want to see the top ten largest files etc..
		if ((!search_wcbuf.length_in_wchars) && (!filter_wcbuf.length_in_wchars) && (es_max_results == 0xffffffff) && (!es_get_result_count) && (!es_get_total_size))
		{
			if ((es_empty_search_help) && (es_output_is_char))
			{
				// don't show help if we are redirecting to a file.
				_es_help();
				
				goto exit;
			}

			if (es_hide_empty_search_results)
			{
				goto exit;
			}
		}
		
		// efu doesn't want name or path columns.
		if (_es_export_type == ES_EXPORT_TYPE_EFU)
		{
			column_remove(EVERYTHING3_PROPERTY_ID_NAME);
			column_remove(EVERYTHING3_PROPERTY_ID_PATH);
		}
		
		// add filename column
		if (!es_no_default_filename_column)
		{
			if (!column_find(EVERYTHING3_PROPERTY_ID_PATH_AND_NAME))
			{
				if (!column_find(EVERYTHING3_PROPERTY_ID_NAME))
				{
					if (!column_find(EVERYTHING3_PROPERTY_ID_PATH))
					{
						column_t *filename_column;
						
						filename_column = column_add(EVERYTHING3_PROPERTY_ID_PATH_AND_NAME);
						
						if (_es_export_type != ES_EXPORT_TYPE_NONE)
						{
							// move KEY to first column.
							column_remove_order(filename_column);
							column_insert_order_at_start(filename_column);
						}
					}
				}
			}
		}

		// write export headers
		// write header
		if (_es_export_type)
		{
			// use export format, which might differ to the display format.
			// for display we typically want YYYY-MM-DD
			// for export we typically want raw filetimes.
			es_date_format = _es_export_date_format;
			
			if (es_utf8_bom)
			{
				if (_es_export_file != INVALID_HANDLE_VALUE)
				{
					BYTE bom[3];
					DWORD numwritten;
					
					// 0xEF,0xBB,0xBF.
					bom[0] = 0xEF;
					bom[1] = 0xBB;
					bom[2] = 0xBF;
					
					WriteFile(_es_export_file,bom,3,&numwritten,0);
				}
			}
		
			// disable pause
			if (_es_export_file != INVALID_HANDLE_VALUE)
			{
				es_pause = 0;
			}
		
			// remove highlighting.

			if (_es_export_file != INVALID_HANDLE_VALUE)
			{
				_es_highlight = 0;
			}
			
			if (_es_export_file != INVALID_HANDLE_VALUE)
			{
				_es_export_buf = mem_alloc(ES_EXPORT_BUF_SIZE);
				_es_export_p = _es_export_buf;
				_es_export_avail = ES_EXPORT_BUF_SIZE;
			}
			
			if ((_es_export_type == ES_EXPORT_TYPE_CSV) || (_es_export_type == ES_EXPORT_TYPE_TSV))
			{
				if (!es_header)
				{
					es_header = 1;
				}
			}
			else
			if (_es_export_type == ES_EXPORT_TYPE_JSON)
			{
				// no header
				es_header = -1;
			}
			else
			if (_es_export_type == ES_EXPORT_TYPE_EFU)
			{
				// add standard columns now.
				// we don't want to change columns once we have sent the IPC requests off.
				// this may resolve es_ipc_version
			
				if (!es_header)
				{
					es_header = 1;
				}
			}
			else
			if ((_es_export_type == ES_EXPORT_TYPE_TXT) || (_es_export_type == ES_EXPORT_TYPE_M3U) || (_es_export_type == ES_EXPORT_TYPE_M3U8))
			{
				if (_es_export_type == ES_EXPORT_TYPE_M3U)		
				{
					es_cp = CP_ACP;
				}

				// reset columns and force Filename.
				column_clear_all();
				column_add(EVERYTHING3_PROPERTY_ID_PATH_AND_NAME);
				
				if (_es_export_type == ES_EXPORT_TYPE_TXT)
				{
					// don't show header unless user wants it.
				}
				else
				{
					// never show header.
					es_header = -1;
				}
			}
		}
		else
		{
			// use display format, which might differ to the export format.
			// for display we typically want YYYY-MM-DD
			// for export we typically want raw filetimes.
			es_date_format = es_display_date_format;
		}
		
		// fix search filter
		if (filter_wcbuf.length_in_wchars)
		{
			wchar_buf_t new_search_wcbuf;

			wchar_buf_init(&new_search_wcbuf);
			
			wchar_buf_cat_utf8_string(&new_search_wcbuf,"< ");

			wchar_buf_cat_wchar_string_n(&new_search_wcbuf,filter_wcbuf.buf,filter_wcbuf.length_in_wchars);
			wchar_buf_cat_utf8_string(&new_search_wcbuf," > < ");
			wchar_buf_cat_wchar_string_n(&new_search_wcbuf,search_wcbuf.buf,search_wcbuf.length_in_wchars);
			if (_es_is_unbalanced_quotes(new_search_wcbuf.buf))
			{
				// using a filter and a search without a trailing quote breaks the search
				// as it makes the trailing > literal.
				// add a terminating quote:
				wchar_buf_cat_utf8_string(&new_search_wcbuf,"\"");
			}

			wchar_buf_cat_utf8_string(&new_search_wcbuf," >");

			wchar_buf_copy_wchar_string_n(&search_wcbuf,new_search_wcbuf.buf,new_search_wcbuf.length_in_wchars);
			
			wchar_buf_kill(&new_search_wcbuf);
		}
		
		if (es_get_result_count)
		{
			// no columns.
			column_clear_all();

			// don't show any results.
			es_max_results = 0;
			
			// reset sort to name ascending.
			secondary_sort_clear_all();
			_es_primary_sort_property_id = EVERYTHING3_PROPERTY_ID_NAME;
			_es_primary_sort_ascending = 1;
		}

		if (es_get_total_size)
		{
			// just request size column.
			column_clear_all();
			column_add(EVERYTHING3_PROPERTY_ID_SIZE);
			
			// reset sort to name ascending.
			secondary_sort_clear_all();
			_es_primary_sort_property_id = EVERYTHING3_PROPERTY_ID_NAME;
			_es_primary_sort_ascending = 1;
		}

		if (es_ipc_version & ES_IPC_VERSION_FLAG_IPC3)
		{
			if (_es_ipc3_query()) 
			{
				// success
				// don't try other versions.
				// we dont need a message loop, exit..
				goto exit;
			}
		}

		es_everything_hwnd = _es_find_ipc_window();
		if (es_everything_hwnd)
		{
			if (es_ipc_version & ES_IPC_VERSION_FLAG_IPC2)
			{
				if (_es_ipc2_query()) 
				{
					// success
					// don't try version 1.
					goto query_sent;
				}
			}

			if (es_ipc_version & ES_IPC_VERSION_FLAG_IPC1)
			{
				if (_es_ipc1_query()) 
				{
					// success
					// don't try other versions.
					goto query_sent;
				}
			}

			es_fatal(ES_ERROR_IPC_ERROR);
		}

query_sent:


		// message pump
message_loop:

		// update windows
		if (PeekMessage(&msg,NULL,0,0,0)) 
		{
			ret = (int)GetMessage(&msg,0,0,0);
			if (ret <= 0) goto exit;

			// let windows handle it.
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}			
		else
		{
			WaitMessage();
		}
		
		goto message_loop;
	}

exit:

	if (es_reply_hwnd)
	{
		DestroyWindow(es_reply_hwnd);
	}

	secondary_sort_clear_all();
	column_clear_all();
	column_color_clear_all();
	column_width_clear_all();

	if (es_run_history_data)
	{
		mem_free(es_run_history_data);
	}

	_es_flush_export_buffer();
	
	if (_es_export_buf)
	{
		mem_free(_es_export_buf);
	}
	
	if (_es_export_file != INVALID_HANDLE_VALUE)
	{
		CloseHandle(_es_export_file);
	}
	
	if (es_ret != ES_ERROR_SUCCESS)
	{
		es_fatal(es_ret);
	}

	array_kill(&local_secondary_sort_array);
	array_kill(&local_column_array);
	array_kill(&local_column_width_array);
	array_kill(&local_column_color_array);
	pool_kill(&local_secondary_sort_pool);
	pool_kill(&local_column_pool);
	pool_kill(&local_column_width_pool);
	pool_kill(&local_column_color_pool);
	wchar_buf_kill(&instance_name_wcbuf);
	wchar_buf_kill(&filter_wcbuf);
	wchar_buf_kill(&search_wcbuf);
	wchar_buf_kill(&argv_wcbuf);

	return ES_ERROR_SUCCESS;
}

int main(int argc,char **argv)
{
	return _es_main();
}

// get the window classname with the instance name if specified.
static void _es_get_window_classname(wchar_buf_t *wcbuf)
{
	wchar_buf_copy_wchar_string(wcbuf,EVERYTHING_IPC_WNDCLASS);
	
	if (es_instance_name_wcbuf->length_in_wchars)
	{
		wchar_buf_cat_utf8_string(wcbuf,"_(");
		wchar_buf_cat_wchar_string_n(wcbuf,es_instance_name_wcbuf->buf,es_instance_name_wcbuf->length_in_wchars);
		wchar_buf_cat_utf8_string(wcbuf,")");
	}
}

// find the Everything IPC window
static HWND _es_find_ipc_window(void)
{
	DWORD tickstart;
	DWORD tick;
	wchar_buf_t window_class_wcbuf;
	HWND ret;

	wchar_buf_init(&window_class_wcbuf);
	
	tickstart = GetTickCount();

	_es_get_window_classname(&window_class_wcbuf);

	ret = 0;
	
	for(;;)
	{
		HWND hwnd;
		
		hwnd = FindWindow(window_class_wcbuf.buf,0);

		if (hwnd)
		{
			// wait for DB_IS_LOADED so we don't get 0 results
			if (es_timeout)
			{
				int major;
				int minor;
				int is_db_loaded;
				
				major = (int)SendMessage(es_everything_hwnd,EVERYTHING_WM_IPC,EVERYTHING_IPC_GET_MAJOR_VERSION,0);
				minor = (int)SendMessage(es_everything_hwnd,EVERYTHING_WM_IPC,EVERYTHING_IPC_GET_MINOR_VERSION,0);
				
				if (((major == 1) && (minor >= 4)) || (major > 1))
				{
					is_db_loaded = (int)SendMessage(es_everything_hwnd,EVERYTHING_WM_IPC,EVERYTHING_IPC_IS_DB_LOADED,0);
					
					if (!is_db_loaded)
					{
						goto wait;
					}
				}
			}
			
			ret = hwnd;
			break;
		}

wait:
		
		if (!es_timeout)
		{
			// the everything window was not found.
			// we can optionally RegisterWindowMessage("EVERYTHING_IPC_CREATED") and 
			// wait for Everything to post this message to all top level windows when its up and running.
			es_fatal(ES_ERROR_NO_IPC);
		}
		
		// try again..
		Sleep(10);
		
		tick = GetTickCount();
		
		if (tick - tickstart > es_timeout)
		{
			// the everything window was not found.
			// we can optionally RegisterWindowMessage("EVERYTHING_IPC_CREATED") and 
			// wait for Everything to post this message to all top level windows when its up and running.
			es_fatal(ES_ERROR_NO_IPC);
		}
	}
	
	wchar_buf_kill(&window_class_wcbuf);

	return ret;
}

// find the Everything IPC window
static void _es_wait_for_db_loaded(void)
{
	DWORD tickstart;
	wchar_buf_t window_class_wcbuf;

	wchar_buf_init(&window_class_wcbuf);
	
	tickstart = GetTickCount();
	
	_es_get_window_classname(&window_class_wcbuf);
	
	for(;;)
	{
		HWND hwnd;
		DWORD tick;
		
		hwnd = FindWindow(window_class_wcbuf.buf,0);

		if (hwnd)
		{
			int major;
			int minor;
			int is_db_loaded;
			
			// wait for DB_IS_LOADED so we don't get 0 results
			major = (int)SendMessage(es_everything_hwnd,EVERYTHING_WM_IPC,EVERYTHING_IPC_GET_MAJOR_VERSION,0);
			minor = (int)SendMessage(es_everything_hwnd,EVERYTHING_WM_IPC,EVERYTHING_IPC_GET_MINOR_VERSION,0);
			
			if (((major == 1) && (minor >= 4)) || (major > 1))
			{
				is_db_loaded = (int)SendMessage(es_everything_hwnd,EVERYTHING_WM_IPC,EVERYTHING_IPC_IS_DB_LOADED,0);
				
				if (is_db_loaded)
				{
					break;
				}
			}
			else
			{
				// can't wait
				break;
			}
		}
		else
		{
			// window was closed.
			break;
		}
		
		// try again..
		Sleep(10);
		
		tick = GetTickCount();
		
		if (es_timeout)
		{
			if (tick - tickstart > es_timeout)
			{
				// the everything window was not found.
				// we can optionally RegisterWindowMessage("EVERYTHING_IPC_CREATED") and 
				// wait for Everything to post this message to all top level windows when its up and running.
				es_fatal(ES_ERROR_NO_IPC);
			}
		}
	}
	
	wchar_buf_kill(&window_class_wcbuf);
}

// find the Everything IPC window
static void _es_wait_for_db_not_busy(void)
{
	DWORD tickstart;
	wchar_buf_t window_class_wcbuf;

	wchar_buf_init(&window_class_wcbuf);
	
	tickstart = GetTickCount();
	
	_es_get_window_classname(&window_class_wcbuf);
	
	for(;;)
	{
		HWND hwnd;
		DWORD tick;
		
		hwnd = FindWindow(window_class_wcbuf.buf,0);

		if (hwnd)
		{
			int major;
			int minor;
			
			// wait for DB_IS_LOADED so we don't get 0 results
			major = (int)SendMessage(es_everything_hwnd,EVERYTHING_WM_IPC,EVERYTHING_IPC_GET_MAJOR_VERSION,0);
			minor = (int)SendMessage(es_everything_hwnd,EVERYTHING_WM_IPC,EVERYTHING_IPC_GET_MINOR_VERSION,0);
			
			if (((major == 1) && (minor >= 4)) || (major > 1))
			{
				int is_busy;
				
				is_busy = (int)SendMessage(es_everything_hwnd,EVERYTHING_WM_IPC,EVERYTHING_IPC_IS_DB_BUSY,0);
				
				if (!is_busy)
				{
					break;
				}
			}
			else
			{
				// can't check.
				break;
			}
		}
		else
		{
			// window was closed.
			break;
		}
		
		// try again..
		Sleep(10);
		
		tick = GetTickCount();
		
		if (es_timeout)
		{
			if (tick - tickstart > es_timeout)
			{
				// the everything window was not found.
				// we can optionally RegisterWindowMessage("EVERYTHING_IPC_CREATED") and 
				// wait for Everything to post this message to all top level windows when its up and running.
				es_fatal(ES_ERROR_NO_IPC);
			}
		}
	}
	
	wchar_buf_kill(&window_class_wcbuf);
}

static const wchar_t *_es_parse_command_line_option_start(const wchar_t *s)
{
	const wchar_t *argv_p;
	
	argv_p = s;
	
	if ((*argv_p == '-') || (*argv_p == '/'))
	{
		argv_p++;
		
		// allow double -
		if (*argv_p == '-')
		{
			argv_p++;
		}
		
		return argv_p;
	}
	
	return NULL;
}

BOOL _es_check_option_utf8_string(const wchar_t *argv,const ES_UTF8 *s)
{
	const wchar_t *argv_p;
	
	argv_p = _es_parse_command_line_option_start(argv);
	if (argv_p)
	{
		return _es_check_option_utf8_string_name(argv_p,s);
	}
	
	return FALSE;
}

BOOL _es_check_option_utf8_string_name(const wchar_t *param,const ES_UTF8 *s)
{
	const wchar_t *p1;
	const ES_UTF8 *p2;
	
	p1 = param;
	p2 = s;
	
	while(*p2)
	{
		if (*p2 == '-')
		{
			if (*p1 == '-')
			{
				p1++;
			}
		
			p2++;
		}
		else
		{
			int c1;
			int c2;
			
			WCHAR_STRING_GET_CHAR(p1,c1);
			UTF8_STRING_GET_CHAR(p2,c2);
			
			c2 = unicode_ascii_to_lower(c2);
		
			if (c1 != c2)
			{
				return FALSE;
			}
		}
	}
	
	if (*p1)
	{
		return FALSE;
	}
	
	return TRUE;
}

// returned data is unaligned.
void *_es_ipc2_get_column_data(EVERYTHING_IPC_LIST2 *list,SIZE_T index,DWORD property_id,DWORD property_highlight)
{	
	BYTE *p;
	EVERYTHING_IPC_ITEM2 *items;
	
	items = (EVERYTHING_IPC_ITEM2 *)(list + 1);
	
	p = ((BYTE *)list) + items[index].data_offset;

	if (list->request_flags & EVERYTHING_IPC_QUERY2_REQUEST_NAME)
	{
		DWORD len;

		if ((property_id == EVERYTHING3_PROPERTY_ID_NAME) && (!property_highlight))
		{
			return p;
		}
		
		os_copy_memory(&len,p,sizeof(DWORD));
		p += sizeof(DWORD);
		
		p += (len + 1) * sizeof(wchar_t);
	}		
	
	if (list->request_flags & EVERYTHING_IPC_QUERY2_REQUEST_PATH)
	{
		DWORD len;
		
		if ((property_id == EVERYTHING3_PROPERTY_ID_PATH) && (!property_highlight))
		{
			return p;
		}
		
		os_copy_memory(&len,p,sizeof(DWORD));
		p += sizeof(DWORD);
		
		p += (len + 1) * sizeof(wchar_t);
	}
	
	if (list->request_flags & EVERYTHING_IPC_QUERY2_REQUEST_FULL_PATH_AND_NAME)
	{
		DWORD len;
		
		if ((property_id == EVERYTHING3_PROPERTY_ID_PATH_AND_NAME) && (!property_highlight))
		{
			return p;
		}
		
		os_copy_memory(&len,p,sizeof(DWORD));
		p += sizeof(DWORD);

		p += (len + 1) * sizeof(wchar_t);
	}
	
	if (list->request_flags & EVERYTHING_IPC_QUERY2_REQUEST_EXTENSION)
	{
		DWORD len;
		
		if (property_id == EVERYTHING3_PROPERTY_ID_EXTENSION)	
		{
			return p;
		}
		
		os_copy_memory(&len,p,sizeof(DWORD));
		p += sizeof(DWORD);
		
		p += (len + 1) * sizeof(wchar_t);
	}
	
	if (list->request_flags & EVERYTHING_IPC_QUERY2_REQUEST_SIZE)
	{
		if (property_id == EVERYTHING3_PROPERTY_ID_SIZE)	
		{
			return p;
		}
		
		p += sizeof(LARGE_INTEGER);
	}
	
	if (list->request_flags & EVERYTHING_IPC_QUERY2_REQUEST_DATE_CREATED)
	{
		if (property_id == EVERYTHING3_PROPERTY_ID_DATE_CREATED)	
		{
			return p;
		}
		
		p += sizeof(FILETIME);
	}
	
	if (list->request_flags & EVERYTHING_IPC_QUERY2_REQUEST_DATE_MODIFIED)
	{
		if (property_id == EVERYTHING3_PROPERTY_ID_DATE_MODIFIED)	
		{
			return p;
		}
		
		p += sizeof(FILETIME);
	}
	
	if (list->request_flags & EVERYTHING_IPC_QUERY2_REQUEST_DATE_ACCESSED)
	{
		if (property_id == EVERYTHING3_PROPERTY_ID_DATE_ACCESSED)	
		{
			return p;
		}
		
		p += sizeof(FILETIME);
	}
	
	if (list->request_flags & EVERYTHING_IPC_QUERY2_REQUEST_ATTRIBUTES)
	{
		if (property_id == EVERYTHING3_PROPERTY_ID_ATTRIBUTES)	
		{
			return p;
		}
		
		p += sizeof(DWORD);
	}
		
	if (list->request_flags & EVERYTHING_IPC_QUERY2_REQUEST_FILE_LIST_FILE_NAME)
	{
		DWORD len;
		
		if (property_id == EVERYTHING3_PROPERTY_ID_FILE_LIST_NAME)	
		{
			return p;
		}
		
		os_copy_memory(&len,p,sizeof(DWORD));
		p += sizeof(DWORD);
		
		p += (len + 1) * sizeof(wchar_t);
	}	
		
	if (list->request_flags & EVERYTHING_IPC_QUERY2_REQUEST_RUN_COUNT)
	{
		if (property_id == EVERYTHING3_PROPERTY_ID_RUN_COUNT)	
		{
			return p;
		}
		
		p += sizeof(DWORD);
	}	
	
	if (list->request_flags & EVERYTHING_IPC_QUERY2_REQUEST_DATE_RUN)
	{
		if (property_id == EVERYTHING3_PROPERTY_ID_DATE_RUN)	
		{
			return p;
		}
		
		p += sizeof(FILETIME);
	}		
	
	if (list->request_flags & EVERYTHING_IPC_QUERY2_REQUEST_DATE_RECENTLY_CHANGED)
	{
		if (property_id == EVERYTHING3_PROPERTY_ID_DATE_RECENTLY_CHANGED)	
		{
			return p;
		}
		
		p += sizeof(FILETIME);
	}	
	
	if (list->request_flags & EVERYTHING_IPC_QUERY2_REQUEST_HIGHLIGHTED_NAME)
	{
		DWORD len;
		
		if ((property_id == EVERYTHING3_PROPERTY_ID_NAME) && (property_highlight))
		{
			return p;
		}
		
		os_copy_memory(&len,p,sizeof(DWORD));
		p += sizeof(DWORD);
		
		p += (len + 1) * sizeof(wchar_t);
	}		
	
	if (list->request_flags & EVERYTHING_IPC_QUERY2_REQUEST_HIGHLIGHTED_PATH)
	{
		DWORD len;
		
		if ((property_id == EVERYTHING3_PROPERTY_ID_PATH) && (property_highlight))
		{
			return p;
		}
		
		os_copy_memory(&len,p,sizeof(DWORD));
		p += sizeof(DWORD);
		
		p += (len + 1) * sizeof(wchar_t);
	}
	
	if (list->request_flags & EVERYTHING_IPC_QUERY2_REQUEST_HIGHLIGHTED_FULL_PATH_AND_NAME)
	{
		DWORD len;
		
		if ((property_id == EVERYTHING3_PROPERTY_ID_PATH_AND_NAME) && (property_highlight))
		{
			return p;
		}
		
		os_copy_memory(&len,p,sizeof(DWORD));
		p += sizeof(DWORD);
		
		p += (len + 1) * sizeof(wchar_t);
	}			
	
	return 0;
}

static void _es_format_size(ES_UINT64 size,wchar_buf_t *wcbuf)
{
	wchar_buf_empty(wcbuf);
	
	if (size != 0xffffffffffffffffI64)
	{
		if (es_size_format == 0)
		{
			// auto size.
			if (size < 1000)
			{
				wchar_buf_printf(wcbuf,"%I64u",size);
				wchar_buf_cat_utf8_string(wcbuf,"  B");
			}
			else
			{
				const ES_UTF8 *suffix;
				
				// get suffix
				if (size / 1024I64 < 1000)
				{
					size = ((size * 100) ) / 1024;
					
					suffix = " KB";
				}
				else
				if (size / (1024I64*1024I64) < 1000)
				{
					size = ((size * 100) ) / 1048576;
					
					suffix = " MB";
				}
				else
				if (size / (1024I64*1024I64*1024I64) < 1000)
				{
					size = ((size * 100) ) / (1024I64*1024I64*1024I64);
					
					suffix = " GB";
				}
				else
				if (size / (1024I64*1024I64*1024I64*1024I64) < 1000)
				{
					size = ((size * 100) ) / (1024I64*1024I64*1024I64*1024I64);
					
					suffix = " TB";
				}
				else
				{
					size = ((size * 100) ) / (1024I64*1024I64*1024I64*1024I64*1024I64);
					
					suffix = " PB";
				}
				
				if (size == 0)
				{
					wchar_buf_cat_print_UINT64(wcbuf,size);
					wchar_buf_cat_utf8_string(wcbuf,suffix);
				}
				else
				if (size < 10)
				{
					// 0.0x
					wchar_buf_cat_utf8_string(wcbuf,"0.0");
					wchar_buf_cat_print_UINT64(wcbuf,size);
					wchar_buf_cat_utf8_string(wcbuf,suffix);
				}
				else
				if (size < 100)
				{
					// 0.xx
					wchar_buf_cat_utf8_string(wcbuf,"0.");
					wchar_buf_cat_print_UINT64(wcbuf,size);
					wchar_buf_cat_utf8_string(wcbuf,suffix);
				}
				else
				if (size < 1000)
				{
					// x.xx
					wchar_buf_cat_print_UINT64(wcbuf,size/100);
					wchar_buf_cat_utf8_string(wcbuf,".");
					if (size%100 < 10)
					{
						// leading zero
						wchar_buf_cat_utf8_string(wcbuf,"0");
					}
					wchar_buf_cat_print_UINT64(wcbuf,size%100);
					wchar_buf_cat_utf8_string(wcbuf,suffix);
				}
				else
				if (size < 10000)
				{
					// xx.x
					wchar_buf_cat_print_UINT64(wcbuf,size/100);
					wchar_buf_cat_utf8_string(wcbuf,".");
					wchar_buf_cat_print_UINT64(wcbuf,(size/10)%10);
					wchar_buf_cat_utf8_string(wcbuf,suffix);
				}
				else
				if (size < 100000)
				{
					// xxx
					wchar_buf_cat_print_UINT64(wcbuf,size/100);
					wchar_buf_cat_utf8_string(wcbuf,suffix);
				}
				else
				{
					// too big..
					_es_format_number(size/100,1,wcbuf);
					wchar_buf_cat_utf8_string(wcbuf,suffix);				
				}
			}
		}
		else
		if (es_size_format == 2)
		{
			_es_format_number(((size) + 1023) / 1024,1,wcbuf);
			wchar_buf_cat_utf8_string(wcbuf," KB");
		}
		else
		if (es_size_format == 3)
		{
			_es_format_number(((size) + 1048575) / 1048576,1,wcbuf);
			wchar_buf_cat_utf8_string(wcbuf," MB");
		}
		else
		if (es_size_format == 4)
		{
			_es_format_number(((size) + 1073741823) / 1073741824,1,wcbuf);
			wchar_buf_cat_utf8_string(wcbuf," GB");
		}
		else
		{
			_es_format_number(size,1,wcbuf);
		}
	}
}

static int _es_filetime_to_localtime(SYSTEMTIME *localst,ES_UINT64 ft)
{
	// try to convert with SystemTimeToTzSpecificLocalTime which will handle daylight savings correctly.
	{
		SYSTEMTIME utcst;
		
		if (FileTimeToSystemTime((FILETIME *)&ft,&utcst))
		{
			if (SystemTimeToTzSpecificLocalTime(NULL,&utcst,localst))
			{
				return 1;
			}
		}
	}
	
//	debug_color_printf(0xffff0000,"SystemTimeToTzSpecificLocalTime failed %d\n",GetLastError());
	
	// win9x: just convert normally.
	{
		FILETIME localft;
		
		if (FileTimeToLocalFileTime((FILETIME *)&ft,&localft))
		{
			if (FileTimeToSystemTime(&localft,localst))
			{
				return 1;
			}
		}
	}
	
	return 0;
}

static void _es_format_attributes(DWORD attributes,wchar_buf_t *wcbuf)
{
	wchar_t *d;
	
	d = wcbuf->buf;
	
	if (attributes & FILE_ATTRIBUTE_READONLY) *d++ = 'R';
	if (attributes & FILE_ATTRIBUTE_HIDDEN) *d++ = 'H';
	if (attributes & FILE_ATTRIBUTE_SYSTEM) *d++ = 'S';
	if (attributes & FILE_ATTRIBUTE_DIRECTORY) *d++ = 'D';
	if (attributes & FILE_ATTRIBUTE_ARCHIVE) *d++ = 'A';
	if (attributes & 0x8000) *d++ = 'V'; // FILE_ATTRIBUTE_INTEGRITY_STREAM
	if (attributes & 0x20000) *d++ = 'X'; // FILE_ATTRIBUTE_NO_SCRUB_DATA
	if (attributes & FILE_ATTRIBUTE_NORMAL) *d++ = 'N';
	if (attributes & FILE_ATTRIBUTE_TEMPORARY) *d++ = 'T';
//	if (attributes & FILE_ATTRIBUTE_SPARSE_FILE) *d++ = 'P';
	if (attributes & FILE_ATTRIBUTE_REPARSE_POINT) *d++ = 'L';
	if (attributes & FILE_ATTRIBUTE_COMPRESSED) *d++ = 'C';
	if (attributes & FILE_ATTRIBUTE_OFFLINE) *d++ = 'O';
	if (attributes & FILE_ATTRIBUTE_NOT_CONTENT_INDEXED) *d++ = 'I';
	if (attributes & FILE_ATTRIBUTE_ENCRYPTED) *d++ = 'E';
	if (attributes & 0x00100000) *d++ = 'U'; // FILE_ATTRIBUTE_UNPINNED; break;
	if (attributes & 0x00080000) *d++ = 'P'; // FILE_ATTRIBUTE_PINNED; break;
	if (attributes & 0x00400000) *d++ = 'M'; // FILE_ATTRIBUTE_RECALL_ON_DATA_ACCESS; break;

	wcbuf->length_in_wchars = d - wcbuf->buf;
	*d = 0;
}

static void _es_format_filetime(ES_UINT64 filetime,wchar_buf_t *wcbuf)
{
	wchar_buf_empty(wcbuf);
	
	if (filetime != 0xffffffffffffffffI64)
	{
		switch(es_date_format)
		{	
			default:
			case 0: // system format
			{
				wchar_t dmybuf[256];
				int dmyformat;
				SYSTEMTIME st;
				int val1;
				int val2;
				int val3;
								
				dmyformat = 1;

				if (GetLocaleInfoW(LOCALE_USER_DEFAULT,LOCALE_IDATE,dmybuf,256))
				{
					dmyformat = dmybuf[0] - '0';
				}
				
				_es_filetime_to_localtime(&st,filetime);
				
				switch(dmyformat)
				{
					case 0: val1 = st.wMonth; val2 = st.wDay; val3 = st.wYear; break; // Month-Day-Year
					default: val1 = st.wDay; val2 = st.wMonth; val3 = st.wYear; break; // Day-Month-Year
					case 2: val1 = st.wYear; val2 = st.wMonth; val3 = st.wDay; break; // Year-Month-Day
				}
				
				wchar_buf_printf(wcbuf,"%02d/%02d/%02d %02d:%02d:%02d",val1,val2,val3,st.wHour,st.wMinute,st.wSecond);
				break;
			}
				
			case 1: // ISO-8601
			{
				SYSTEMTIME st;
				_es_filetime_to_localtime(&st,filetime);
				wchar_buf_printf(wcbuf,"%04d-%02d-%02dT%02d:%02d:%02d",st.wYear,st.wMonth,st.wDay,st.wHour,st.wMinute,st.wSecond);
				break;
			}

			case 2: // raw filetime
				wchar_buf_printf(wcbuf,"%I64u",filetime);
				break;
				
			case 3: // ISO-8601 (UTC/Z)
			{
				SYSTEMTIME st;
				FileTimeToSystemTime((FILETIME *)&filetime,&st);
				wchar_buf_printf(wcbuf,"%04d-%02d-%02dT%02d:%02d:%02dZ",st.wYear,st.wMonth,st.wDay,st.wHour,st.wMinute,st.wSecond);
				break;
			}
		}
	}
}

static void _es_format_duration(ES_UINT64 duration,wchar_buf_t *wcbuf)
{
	wchar_buf_empty(wcbuf);
	
	if (duration != 0xffffffffffffffffI64)
	{
		ES_UINT64 total_seconds;
		ES_UINT64 days;
		ES_UINT64 hours;
		ES_UINT64 minutes;
		
		total_seconds = duration / 10000000;
		
		// "[d]:hh:mm:ss"
		
		days = total_seconds / 86400;

		// [d]
		if (days)
		{
			wchar_buf_cat_printf(wcbuf,"%I64u",days);

			total_seconds -= days * 86400;

			wchar_buf_cat_printf(wcbuf,":");
		}

		// hh
		hours = total_seconds / 3600;
		
		if (hours)
		{
			wchar_buf_cat_printf(wcbuf,"%02I64u",hours);

			total_seconds -= hours * 3600;

			wchar_buf_cat_printf(wcbuf,":");
		}

		// mm
		minutes = total_seconds / 60;
		
		wchar_buf_cat_printf(wcbuf,"%02I64u",minutes);

		total_seconds -= minutes * 60;

		wchar_buf_cat_printf(wcbuf,":");

		// ss
		wchar_buf_cat_printf(wcbuf,"%02I64u",total_seconds);
	}
}

static void _es_format_number(ES_UINT64 number,int allow_digit_grouping,wchar_buf_t *out_wcbuf)
{
	ES_UTF8 *d;
	int comma;
	ES_UTF8 buf[256];
	
	d = buf + 256;
	*--d = 0;
	comma = 0;

	if (number)
	{
		ES_UINT64 i;
		
		i = number;
		
		while(i)
		{
			if (comma >= 3)
			{
				if (allow_digit_grouping)
				{
					if (es_digit_grouping)
					{
						*--d = ',';
					}
				}
					
				comma = 0;
			}
		
			*--d = '0' + (int)(i % 10);
			
			i /= 10;
			
			comma++;
		}
	}
	else
	{
		*--d = '0';
	}	
	
	wchar_buf_copy_utf8_string(out_wcbuf,d);
}

static void _es_format_dimensions(EVERYTHING3_DIMENSIONS *dimensions_value,wchar_buf_t *out_wcbuf)
{
	if ((dimensions_value->width == ES_DWORD_MAX) && (dimensions_value->height == ES_DWORD_MAX))
	{
		wchar_buf_empty(out_wcbuf);
	}
	else
	{
		wchar_buf_printf(out_wcbuf,"%ux%u",dimensions_value->width,dimensions_value->height);
	}
}

static void _es_get_argv(wchar_buf_t *wcbuf)
{
	int pass;
	int inquote;
	wchar_t *d;
	
	if (!*es_command_line)
	{
		es_command_line = NULL;
		
		return;
	}
	
	d = 0;
	
	for(pass=0;pass<2;pass++)
	{
		const wchar_t *p;

		p = wchar_string_skip_ws(es_command_line);
		
		inquote = 0;
		
		while(*p)
		{
			if ((!inquote) && (unicode_is_ascii_ws(*p)))
			{
				break;
			}
			else
			if (*p == '"')
			{
				if (pass)
				{
					*d = '"';
				}
				
				d++;

				p++;

				// 3 quotes = 1 literal quote.
				if ((*p == '"') && (p[1] == '"'))
				{
					p += 2;
					
					if (pass)
					{
						*d = '"';
					}
					
					d++;
				}
				else
				{
					inquote = !inquote;
				}
			}
			else
			{
				if (pass) 
				{
					*d = *p;
				}

				d++;
				p++;
			}
		}
	
		if (pass) 
		{
			*d = 0;
			es_command_line = p;
			return;
		}
		else
		{
			wchar_buf_grow_length(wcbuf,((SIZE_T)d) / sizeof(wchar_t));
			d = wcbuf->buf;
		}		
	}
	
	es_command_line = NULL;
}

static void _es_expect_argv(wchar_buf_t *wcbuf)
{
	_es_get_argv(wcbuf);
	
	if (!es_command_line)
	{
		es_fatal(ES_ERROR_EXPECTED_SWITCH_PARAMETER);
	}
}

// like _es_get_argv, but we remove double quotes.
static void _es_get_command_argv(wchar_buf_t *wcbuf)
{
	int pass;
	int inquote;
	wchar_t *d;
	
	if (!*es_command_line)
	{
		es_command_line = NULL;

		return;
	}
	
	d = 0;
	
	for(pass=0;pass<2;pass++)
	{
		const wchar_t *p;

		p = wchar_string_skip_ws(es_command_line);
		
		inquote = 0;
		
		while(*p)
		{
			if ((!inquote) && (unicode_is_ascii_ws(*p)))
			{
				break;
			}
			else
			if (*p == '"')
			{
				p++;

				// 3 quotes = 1 literal quote.
				if ((*p == '"') && (p[1] == '"'))
				{
					p += 2;
					
					if (pass)
					{
						*d = '"';
					}
					
					d++;
				}
				else
				{
					inquote = !inquote;
				}
			}
			else
			{
				if (pass) 
				{
					*d = *p;
				}

				d++;
				p++;
			}
		}
	
		if (pass) 
		{
			*d = 0;
			es_command_line = p;
			return;
		}
		else
		{
			wchar_buf_grow_length(wcbuf,((SIZE_T)d) / sizeof(wchar_t));
			d = wcbuf->buf;
		}		
	}
	
	es_command_line = NULL;
}

static void _es_expect_command_argv(wchar_buf_t *wcbuf)
{
	_es_get_command_argv(wcbuf);
	
	if (!es_command_line)
	{
		es_fatal(ES_ERROR_EXPECTED_SWITCH_PARAMETER);
	}
}

BOOL _es_is_valid_key(INPUT_RECORD *ir)
{
	switch (ir->Event.KeyEvent.wVirtualKeyCode)
	{
		case 0x00: //  
		case 0x10: // SHIFT
		case 0x11: // CONTROL
		case 0x12: // ALT
		case 0x13: // PAUSE
		case 0x14: // CAPITAL
		case 0x90: // NUMLOCK
		case 0x91: // SCROLLLOCK
            return FALSE;
    }
    
    return TRUE;
}

static BOOL _es_save_settings_with_filename(const wchar_t *filename)
{
	BOOL ret;
	HANDLE file_handle;
	
	ret = FALSE;
			
	file_handle = os_create_file(filename);
	if (file_handle != INVALID_HANDLE_VALUE)
	{
		wchar_buf_t property_name_wcbuf;

		wchar_buf_init(&property_name_wcbuf);
		
		os_write_file_utf8_string(file_handle,"[ES]\r\n");
		
		{
			wchar_buf_t sort_list_wcbuf;
			secondary_sort_t *secondary_sort;

			wchar_buf_init(&sort_list_wcbuf);

			// primary sort.
			property_get_canonical_name(_es_primary_sort_property_id,&property_name_wcbuf);
			
			wchar_buf_cat_list_wchar_string_n(&sort_list_wcbuf,property_name_wcbuf.buf,property_name_wcbuf.length_in_wchars);
			
			// secondary sorts.
			secondary_sort = secondary_sort_start;
			while(secondary_sort)
			{
				property_get_canonical_name(secondary_sort->property_id,&property_name_wcbuf);
				
				wchar_buf_cat_list_wchar_string_n(&sort_list_wcbuf,property_name_wcbuf.buf,property_name_wcbuf.length_in_wchars);
				
				if (secondary_sort->ascending)
				{
					wchar_buf_cat_utf8_string(&sort_list_wcbuf,secondary_sort->ascending > 0 ? "-ascending" : "-descending");
				}
			
				secondary_sort = secondary_sort->next;
			}

			config_write_string(file_handle,"sort",sort_list_wcbuf.buf);

			wchar_buf_kill(&sort_list_wcbuf);
		}
		
		config_write_int(file_handle,"sort_ascending",_es_primary_sort_ascending);
		config_write_string(file_handle,"instance",es_instance_name_wcbuf->buf);
		config_write_int(file_handle,"highlight_color",_es_highlight_color);
		config_write_int(file_handle,"highlight",_es_highlight);
		config_write_int(file_handle,"match_whole_word",_es_match_whole_word);
		config_write_int(file_handle,"match_path",_es_match_path);
		config_write_int(file_handle,"match_case",_es_match_case);
		config_write_int(file_handle,"match_diacritics",_es_match_diacritics);
		config_write_int(file_handle,"size_leading_zero",es_size_leading_zero);
		config_write_int(file_handle,"run_count_leading_zero",es_run_count_leading_zero);
		config_write_int(file_handle,"digit_grouping",es_digit_grouping);
		config_write_uint64(file_handle,"offset",es_offset);
		config_write_uint64(file_handle,"max_results",es_max_results);
		config_write_dword(file_handle,"timeout",es_timeout);
		config_write_int(file_handle,"size_format",es_size_format);
		config_write_int(file_handle,"date_format",es_display_date_format);
		config_write_int(file_handle,"export_date_format",_es_export_date_format);
		config_write_int(file_handle,"pause",es_pause);
		config_write_int(file_handle,"empty_search_help",es_empty_search_help);
		config_write_int(file_handle,"hide_empty_search_results",es_hide_empty_search_results);
		config_write_int(file_handle,"utf8_bom",es_utf8_bom);
		config_write_int(file_handle,"folder_append_path_separator",es_folder_append_path_separator);
		config_write_int(file_handle,"es_newline_type",es_newline_type);
		
		// columns
		
		{
			wchar_buf_t column_list_wcbuf;
			column_t *column;

			wchar_buf_init(&column_list_wcbuf);
		
			column = column_order_start;
			while(column)
			{
				property_get_canonical_name(column->property_id,&property_name_wcbuf);
				
				wchar_buf_cat_list_wchar_string_n(&column_list_wcbuf,property_name_wcbuf.buf,property_name_wcbuf.length_in_wchars);
				
				column = column->order_next;
			}

			config_write_string(file_handle,"columns",column_list_wcbuf.buf);

			wchar_buf_kill(&column_list_wcbuf);
		}

		// column colors
		// size=14;date modified=12;...

		{
			wchar_buf_t column_color_list_wcbuf;
			const column_color_t **column_color_p;
			SIZE_T column_color_run;

			wchar_buf_init(&column_color_list_wcbuf);
		
			column_color_p = (const column_color_t **)column_color_array->indexes;
			column_color_run = column_color_array->count;
		
			while(column_color_run)
			{
				if (column_color_p != (const column_color_t **)column_color_array->indexes)
				{
					wchar_buf_cat_printf(&column_color_list_wcbuf,";");
				}
				
				property_get_canonical_name((*column_color_p)->property_id,&property_name_wcbuf);
				
				// property_name_wcbuf doesn't need escaping.
				wchar_buf_cat_printf(&column_color_list_wcbuf,"%S=%d",property_name_wcbuf.buf,(int)(*column_color_p)->color);

				column_color_p++;
				column_color_run--;
			}

			config_write_string(file_handle,"column_colors",column_color_list_wcbuf.buf);

			wchar_buf_kill(&column_color_list_wcbuf);
		}
	

		// column widths
		// size=9;date modified=12;name=100

		{
			wchar_buf_t column_width_list_wcbuf;
			const column_width_t **column_width_p;
			SIZE_T column_width_run;

			wchar_buf_init(&column_width_list_wcbuf);
		
			column_width_p = (const column_width_t **)column_width_array->indexes;
			column_width_run = column_width_array->count;
		
			while(column_width_run)
			{
				if (column_width_p != (const column_width_t **)column_width_array->indexes)
				{
					wchar_buf_cat_printf(&column_width_list_wcbuf,";");
				}
				
				property_get_canonical_name((*column_width_p)->property_id,&property_name_wcbuf);
				
				// property_name_wcbuf doesn't need escaping.
				wchar_buf_cat_printf(&column_width_list_wcbuf,"%S=%d",property_name_wcbuf.buf,(*column_width_p)->width);

				column_width_p++;
				column_width_run--;
			}

			config_write_string(file_handle,"column_widths",column_width_list_wcbuf.buf);

			wchar_buf_kill(&column_width_list_wcbuf);
		}
		
		ret = TRUE;
		
		wchar_buf_kill(&property_name_wcbuf);

		CloseHandle(file_handle);
	}
	
	return ret;
}

static BOOL _es_save_settings_with_appdata(int is_appdata)
{
	BOOL ret;
	wchar_buf_t ini_temp_filename_wcbuf;
	wchar_buf_t ini_filename_wcbuf;

	ret = FALSE;
	wchar_buf_init(&ini_temp_filename_wcbuf);
	wchar_buf_init(&ini_filename_wcbuf);
	
	// if we loaded from appdata ini, always save to appdata ini.
	// try to write to exe dir first..
	// if that fails (typically access denied), write to appdata.
	if (config_get_filename(is_appdata,1,&ini_temp_filename_wcbuf))
	{
		if (config_get_filename(is_appdata,0,&ini_filename_wcbuf))
		{
			if (is_appdata)
			{
				os_make_sure_path_to_file_exists(ini_temp_filename_wcbuf.buf);
			}
			
			if (_es_save_settings_with_filename(ini_temp_filename_wcbuf.buf))
			{
				if (os_replace_file(ini_temp_filename_wcbuf.buf,ini_filename_wcbuf.buf))
				{
					ret = TRUE;
				}
			}
		}
	}

	wchar_buf_kill(&ini_filename_wcbuf);
	wchar_buf_kill(&ini_temp_filename_wcbuf);	
	
	return ret;
}

// returns 1 if successful.
// returns 0 on failure and sets GetLastError();
static BOOL _es_save_settings(void)
{
	BOOL ret;

	ret = FALSE;
	
	// if we loaded from appdata ini, always save to appdata ini.
	// try to write to exe dir first..
	// if that fails (typically access denied), write to appdata.
	// 
	// our manifest uses:
	// <requestedExecutionLevel level="asInvoker" uiAccess="false"/>
	// which disables the virtual store.
	
	if (!es_loaded_appdata_ini)
	{
		if (_es_save_settings_with_appdata(0))
		{
			ret = TRUE;
			
			goto got_ret;
		}
	}

	if (_es_save_settings_with_appdata(1))
	{
		ret = TRUE;
	}

got_ret:

	return ret;
}

static BOOL _es_load_settings_with_filename(const wchar_t *filename)
{
	BOOL ret;
	config_ini_t ini;
	
	ret = FALSE;
	
	if (config_ini_open(&ini,filename,"ES"))
	{
		{
			wchar_buf_t sort_list_wcbuf;

			wchar_buf_init(&sort_list_wcbuf);
			
			if (config_read_string(&ini,"sort",&sort_list_wcbuf))
			{
				_es_set_sort_list(sort_list_wcbuf.buf,1);
			}

			wchar_buf_kill(&sort_list_wcbuf);
		}
		
		_es_primary_sort_ascending = config_read_int(&ini,"sort_ascending",_es_primary_sort_ascending);
		config_read_string(&ini,"instance",es_instance_name_wcbuf);
		_es_highlight_color = config_read_int(&ini,"highlight_color",_es_highlight_color);
		_es_highlight = config_read_int(&ini,"highlight",_es_highlight);
		_es_match_whole_word = config_read_int(&ini,"match_whole_word",_es_match_whole_word);
		_es_match_path = config_read_int(&ini,"match_path",_es_match_path);
		_es_match_case = config_read_int(&ini,"match_case",_es_match_case);
		_es_match_diacritics = config_read_int(&ini,"match_diacritics",_es_match_diacritics);
		es_size_leading_zero = config_read_int(&ini,"size_leading_zero",es_size_leading_zero);
		es_run_count_leading_zero = config_read_int(&ini,"run_count_leading_zero",es_run_count_leading_zero);
		es_digit_grouping = config_read_int(&ini,"digit_grouping",es_digit_grouping);
		es_offset = safe_size_from_uint64(config_read_uint64(&ini,"offset",es_offset));
		es_max_results = safe_size_from_uint64(config_read_uint64(&ini,"max_results",es_max_results));
		es_timeout = config_read_dword(&ini,"timeout",es_timeout);
		es_size_format = config_read_int(&ini,"size_format",es_size_format);
		es_display_date_format = config_read_int(&ini,"date_format",es_display_date_format);
		_es_export_date_format = config_read_int(&ini,"export_date_format",_es_export_date_format);
		es_pause = config_read_int(&ini,"pause",es_pause);
		es_empty_search_help = config_read_int(&ini,"empty_search_help",es_empty_search_help);
		es_hide_empty_search_results = config_read_int(&ini,"hide_empty_search_results",es_hide_empty_search_results);
		es_utf8_bom = config_read_int(&ini,"utf8_bom",es_utf8_bom);
		es_folder_append_path_separator = config_read_int(&ini,"folder_append_path_separator",es_folder_append_path_separator);
		es_newline_type = config_read_int(&ini,"newline_type",es_newline_type);
		
		// columns
		
		{
			wchar_buf_t column_list_wcbuf;

			wchar_buf_init(&column_list_wcbuf);
			
			if (config_read_string(&ini,"columns",&column_list_wcbuf))
			{
				_es_set_columns(column_list_wcbuf.buf,0,1);
			}

			wchar_buf_kill(&column_list_wcbuf);
		}
		
		// column colors.

		{
			wchar_buf_t column_color_list_wcbuf;

			wchar_buf_init(&column_color_list_wcbuf);
			
			if (config_read_string(&ini,"column_colors",&column_color_list_wcbuf))
			{
				_es_set_column_colors(column_color_list_wcbuf.buf,0);
			}

			wchar_buf_kill(&column_color_list_wcbuf);
		}
		
		// column widths.

		{
			wchar_buf_t column_width_list_wcbuf;

			wchar_buf_init(&column_width_list_wcbuf);
			
			if (config_read_string(&ini,"column_widths",&column_width_list_wcbuf))
			{
				_es_set_column_widths(column_width_list_wcbuf.buf,0);
			}

			wchar_buf_kill(&column_width_list_wcbuf);
		}
		
		config_ini_close(&ini);
	}
	
	return ret;
}

static BOOL _es_load_settings_with_appdata(int is_appdata)
{
	BOOL ret;
	wchar_buf_t ini_filename_wcbuf;

	ret = FALSE;
	wchar_buf_init(&ini_filename_wcbuf);
	
	if (config_get_filename(is_appdata,0,&ini_filename_wcbuf))
	{
		if (_es_load_settings_with_filename(ini_filename_wcbuf.buf))
		{
			ret = TRUE;
		}
	}

	wchar_buf_kill(&ini_filename_wcbuf);
	
	return ret;
}

static void _es_load_settings(void)
{
	_es_load_settings_with_appdata(0);
	
	if (_es_load_settings_with_appdata(1))
	{
		es_loaded_appdata_ini = 1;
	}
}

static void _es_append_filter(wchar_buf_t *wcbuf,const ES_UTF8 *filter)
{
	if (wcbuf->length_in_wchars)
	{
		wchar_buf_cat_wchar(wcbuf,' ');
	}
	
	wchar_buf_cat_utf8_string(wcbuf,filter);
}

static void _es_do_run_history_command(void)
{	
	es_everything_hwnd = _es_find_ipc_window();
	if (es_everything_hwnd)
	{
		if (es_run_history_size <= ES_DWORD_MAX)
		{
			COPYDATASTRUCT cds;
			DWORD run_count;

			cds.cbData = (DWORD)es_run_history_size;
			cds.dwData = es_run_history_command;
			cds.lpData = es_run_history_data;
			
			run_count = (DWORD)SendMessage(es_everything_hwnd,WM_COPYDATA,0,(LPARAM)&cds);

			if (es_run_history_command == EVERYTHING_IPC_COPYDATA_GET_RUN_COUNTW)
			{
				_es_output_noncell_printf("%u\r\n",run_count);
			}
		}
		else
		{
			es_fatal(ES_ERROR_OUT_OF_MEMORY);
		}
	}
	else
	{
		// the everything window was not found.
		// we can optionally RegisterWindowMessage("EVERYTHING_IPC_CREATED") and 
		// wait for Everything to post this message to all top level windows when its up and running.
		es_fatal(ES_ERROR_NO_IPC);
	}
}

// checks for -sort-size, -sort-size-ascending and -sort-size-descending
// depreciated.
// use -sort size -sort size-ascending -sort size-descending
static BOOL _es_check_sorts(const wchar_t *argv)
{
	BOOL ret;
	const wchar_t *argv_p;

	ret = FALSE;
	
	argv_p = _es_parse_command_line_option_start(argv);
	if (argv_p)
	{
		argv_p = wchar_string_parse_utf8_string(argv_p,"sort");
		if (argv_p)
		{
			wchar_buf_t sort_name_wcbuf;
			DWORD property_id;
			int ascending;

			wchar_buf_init(&sort_name_wcbuf);
			ascending = 0;
			
			if (*argv_p == '-')
			{
				argv_p++;
			}
			
			wchar_buf_copy_wchar_string(&sort_name_wcbuf,argv_p);
			
			{
				wchar_t *ascending_p;
				
				ascending_p = sort_name_wcbuf.buf;
				
				while(*ascending_p)
				{
					if (*ascending_p == '-')
					{
						const wchar_t *match_p;
						
						match_p = wchar_string_parse_utf8_string(ascending_p + 1,"ascending");
						if (match_p)
						{
							if (!*match_p)
							{
								ascending = 1;

								// truncate name
								*ascending_p = 0;
								break;
							}
						}

						match_p = wchar_string_parse_utf8_string(ascending_p + 1,"descending");
						if (match_p)
						{
							if (!*match_p)
							{
								ascending = -1;

								// truncate name
								*ascending_p = 0;
								break;
							}
						}
					}
					else
					if (*ascending_p == 'a')
					{
						const wchar_t *match_p;
						
						match_p = wchar_string_parse_utf8_string(ascending_p,"ascending");
						if (match_p)
						{
							if (!*match_p)
							{
								ascending = 1;

								// truncate name
								*ascending_p = 0;
								break;
							}
						}
					}	
					else
					if (*ascending_p == 'd')
					{
						const wchar_t *match_p;
						
						match_p = wchar_string_parse_utf8_string(ascending_p,"descending");
						if (match_p)
						{
							if (!*match_p)
							{
								ascending = 1;

								// truncate name
								*ascending_p = 0;
								break;
							}
						}
					}
				
					ascending_p++;
				}
			}
			
			property_id = property_find(sort_name_wcbuf.buf);
			
			if (property_id != EVERYTHING3_INVALID_PROPERTY_ID)
			{
				_es_primary_sort_property_id = property_id;
				
				if (ascending)
				{
					_es_primary_sort_ascending = ascending;
				}
				
				secondary_sort_clear_all();

				ret = TRUE;
			}
			
			wchar_buf_kill(&sort_name_wcbuf);
		}
	}

	return ret;
}

static BOOL _es_is_literal_switch(const wchar_t *s)
{
	const wchar_t *p;
	
	p = s;
	
	// skip -
	while(*p)
	{
		if (*p != '-')
		{
			break;
		}
		
		p++;
	}
	
	// expect end of switch.
	if (*p)
	{
		return FALSE;
	}
	
	return TRUE;
}

static BOOL _es_is_unbalanced_quotes(const wchar_t *s)
{
	const wchar_t *p;
	BOOL in_quote;
	
	p = s;
	in_quote = FALSE;
	
	while(*p)
	{
		if (*p == '"')
		{
			in_quote = !in_quote;
			p++;
			continue;
		}

		p++;
	}
	
	return in_quote;
}

static void _es_get_folder_size(const wchar_t *filename)
{
	HANDLE pipe_handle;
	
	pipe_handle = ipc3_connect_pipe();
	if (pipe_handle != INVALID_HANDLE_VALUE)
	{
		utf8_buf_t filename_cbuf;
		ES_UINT64 folder_size;
		SIZE_T numread;
		
		utf8_buf_init(&filename_cbuf);

		utf8_buf_copy_wchar_string(&filename_cbuf,filename);
		
		if (ipc3_ioctl(pipe_handle,IPC3_COMMAND_GET_FOLDER_SIZE,filename_cbuf.buf,filename_cbuf.length_in_bytes,&folder_size,sizeof(ES_UINT64),&numread))
		{
			if (numread == sizeof(ES_UINT64))
			{
				if (folder_size != ES_UINT64_MAX)
				{
					_es_output_noncell_printf("%I64u\r\n",folder_size);
				}
				else
				{
					// unknown
					es_fatal(ES_ERROR_NO_RESULTS);
				}
			}
			else
			{
				// bad read
				es_fatal(ES_ERROR_IPC_ERROR);
			}
		}
		else
		{
			// bad ioctrl
			es_fatal(ES_ERROR_IPC_ERROR);
		}

		utf8_buf_kill(&filename_cbuf);

		CloseHandle(pipe_handle);
	}
	else
	{
		// no IPC
		es_fatal(ES_ERROR_NO_IPC);
	}
}

static BYTE *_es_copy_dword(BYTE *buf,DWORD value)
{
	return os_copy_memory(buf,&value,sizeof(DWORD));
}

static BYTE *_es_copy_uint64(BYTE *buf,ES_UINT64 value)
{
	return os_copy_memory(buf,&value,sizeof(ES_UINT64));
}

static BYTE *_es_copy_size_t(BYTE *buf,SIZE_T value)
{
#if SIZE_MAX == 0xFFFFFFFFFFFFFFFFUI64
	
	return _es_copy_uint64(buf,(ES_UINT64)value);
	
#elif SIZE_MAX == 0xFFFFFFFF

	return _es_copy_dword(buf,(DWORD)value);
	
#else
	#error unknown SIZE_MAX
#endif

}

static BOOL _es_check_color_param(const wchar_t *argv,DWORD *out_property_id)
{
	BOOL ret;
	const wchar_t *argv_p;
	
	ret = FALSE;

	argv_p = _es_parse_command_line_option_start(argv);
	if (argv_p)
	{
		const wchar_t *property_name_start;
		wchar_buf_t property_name_wcbuf;

		property_name_start = argv_p;
		wchar_buf_init(&property_name_wcbuf);
	
		// look for trailing -color or color.
		// we don't have any property names ending with color.
		while(*argv_p)
		{
			if (*argv_p == '-')
			{
				const wchar_t *match_p;
				
				match_p = wchar_string_parse_utf8_string(argv_p + 1,"color");
				if (match_p)
				{
					if (!*match_p)
					{
						// found.
						wchar_buf_copy_wchar_string_n(&property_name_wcbuf,property_name_start,argv_p - property_name_start);
						
						break;
					}
				}
			}
			else
			if (*argv_p == 'c')
			{
				const wchar_t *match_p;
				
				match_p = wchar_string_parse_utf8_string(argv_p,"color");
				if (match_p)
				{
					if (!*match_p)
					{
						// found.
						wchar_buf_copy_wchar_string_n(&property_name_wcbuf,property_name_start,argv_p - property_name_start);
						
						break;
					}
				}
			}
			
			argv_p++;
		}

		if (property_name_wcbuf.length_in_wchars)
		{
			DWORD property_id;
			
			property_id = property_find(property_name_wcbuf.buf);
			if (property_id != EVERYTHING3_INVALID_PROPERTY_ID)
			{
				*out_property_id = property_id;
				
				ret = TRUE;
			}
		}

		wchar_buf_kill(&property_name_wcbuf);
	}

	return ret;
}

static BOOL _es_check_column_param(const wchar_t *argv)
{
	BOOL ret;
	DWORD property_id;
	const wchar_t *argv_p;

	ret = FALSE;

	argv_p = _es_parse_command_line_option_start(argv);
	if (argv_p)
	{
		property_id = property_find(argv_p);
		if (property_id != EVERYTHING3_INVALID_PROPERTY_ID)
		{
			ret = TRUE;
			
			column_add(property_id);
		}
		else
		{
			// try no-<property-name>
			argv_p = wchar_string_parse_utf8_string(argv_p,"no");
			if (argv_p)
			{
				if (*argv_p == '-')
				{
					argv_p++;
				}
				
				property_id = property_find(argv_p);
				if (property_id != EVERYTHING3_INVALID_PROPERTY_ID)
				{
					ret = TRUE;
					
					column_remove(property_id);
				}
			}
		}
	}

	return ret;
}

// sort_ascending >0 == ascending
// sort_ascending ==0 == default ascending
// sort_ascending <0 == descending
static int _es_get_ipc_sort_type_from_property_id(DWORD property_id,int sort_ascending)
{
	int is_ascending;
	
	is_ascending = 0;
	
	// resolve sort_ascending..
	if (sort_ascending)
	{
		// descending.
		if (sort_ascending > 0)
		{
			is_ascending = 1;
		}
	}
	else
	{
		if (property_get_default_sort_ascending(property_id))
		{
			is_ascending = 1;
		}
	}
	
	switch(property_id)
	{
		case EVERYTHING3_PROPERTY_ID_NAME:
			return is_ascending ? EVERYTHING_IPC_SORT_NAME_ASCENDING : EVERYTHING_IPC_SORT_NAME_DESCENDING;

		case EVERYTHING3_PROPERTY_ID_PATH:
			return is_ascending ? EVERYTHING_IPC_SORT_PATH_ASCENDING : EVERYTHING_IPC_SORT_PATH_DESCENDING;

		case EVERYTHING3_PROPERTY_ID_SIZE:
			return is_ascending ? EVERYTHING_IPC_SORT_SIZE_ASCENDING : EVERYTHING_IPC_SORT_SIZE_DESCENDING;

		case EVERYTHING3_PROPERTY_ID_EXTENSION:
			return is_ascending ? EVERYTHING_IPC_SORT_EXTENSION_ASCENDING : EVERYTHING_IPC_SORT_EXTENSION_DESCENDING;

		case EVERYTHING3_PROPERTY_ID_TYPE:
			return is_ascending ? EVERYTHING_IPC_SORT_TYPE_NAME_ASCENDING : EVERYTHING_IPC_SORT_TYPE_NAME_DESCENDING;

		case EVERYTHING3_PROPERTY_ID_DATE_CREATED:
			return is_ascending ? EVERYTHING_IPC_SORT_DATE_CREATED_ASCENDING : EVERYTHING_IPC_SORT_DATE_CREATED_DESCENDING;

		case EVERYTHING3_PROPERTY_ID_DATE_MODIFIED:
			return is_ascending ? EVERYTHING_IPC_SORT_DATE_MODIFIED_ASCENDING : EVERYTHING_IPC_SORT_DATE_MODIFIED_DESCENDING;

		case EVERYTHING3_PROPERTY_ID_ATTRIBUTES:
			return is_ascending ? EVERYTHING_IPC_SORT_ATTRIBUTES_ASCENDING : EVERYTHING_IPC_SORT_ATTRIBUTES_DESCENDING;

		case EVERYTHING3_PROPERTY_ID_FILE_LIST_NAME:
			return is_ascending ? EVERYTHING_IPC_SORT_FILE_LIST_FILENAME_ASCENDING : EVERYTHING_IPC_SORT_FILE_LIST_FILENAME_DESCENDING;

		case EVERYTHING3_PROPERTY_ID_RUN_COUNT:
			return is_ascending ? EVERYTHING_IPC_SORT_RUN_COUNT_ASCENDING : EVERYTHING_IPC_SORT_RUN_COUNT_DESCENDING;

		case EVERYTHING3_PROPERTY_ID_DATE_RECENTLY_CHANGED:
			return is_ascending ? EVERYTHING_IPC_SORT_DATE_RECENTLY_CHANGED_ASCENDING : EVERYTHING_IPC_SORT_DATE_RECENTLY_CHANGED_DESCENDING;

		case EVERYTHING3_PROPERTY_ID_DATE_ACCESSED:
			return is_ascending ? EVERYTHING_IPC_SORT_DATE_ACCESSED_ASCENDING : EVERYTHING_IPC_SORT_DATE_ACCESSED_DESCENDING;

		case EVERYTHING3_PROPERTY_ID_DATE_RUN:
			return is_ascending ? EVERYTHING_IPC_SORT_DATE_RUN_ASCENDING : EVERYTHING_IPC_SORT_DATE_RUN_DESCENDING;
	}
	
	return EVERYTHING_IPC_SORT_NAME_ASCENDING;
}

// only create the reply window once.
static void _es_get_reply_window(void)
{
	if (!es_reply_hwnd)
	{
		WNDCLASSEX wcex;
	
		os_zero_memory(&wcex,sizeof(wcex));
		wcex.cbSize = sizeof(wcex);
		
		if (!GetClassInfoEx(GetModuleHandle(NULL),TEXT("ES_IPC"),&wcex))
		{
			os_zero_memory(&wcex,sizeof(wcex));
			wcex.cbSize = sizeof(wcex);
			wcex.hInstance = GetModuleHandle(NULL);
			wcex.lpfnWndProc = _es_window_proc;
			wcex.lpszClassName = TEXT("ES_IPC");
			
			if (!RegisterClassEx(&wcex))
			{
				es_fatal(ES_ERROR_REGISTER_WINDOW_CLASS);
			}
		}
		
		es_reply_hwnd = CreateWindow(TEXT("ES_IPC"),TEXT(""),0,0,0,0,0,0,0,GetModuleHandle(0),0);
		if (!es_reply_hwnd)
		{
			es_fatal(ES_ERROR_CREATE_WINDOW);
		}

		// allow the everything window to send a reply if
		// the Everything window is running as normal user and we are running as admin.
		
		{
			HMODULE user32_hdll;

			user32_hdll = GetModuleHandleA("user32.dll");
			if (user32_hdll)
			{
				_es_pChangeWindowMessageFilterEx = (BOOL (WINAPI *)(HWND hWnd,UINT message,DWORD action,_ES_CHANGEFILTERSTRUCT *pChangeFilterStruct))GetProcAddress(user32_hdll,"ChangeWindowMessageFilterEx");

				if (_es_pChangeWindowMessageFilterEx)
				{
					_es_pChangeWindowMessageFilterEx(es_reply_hwnd,WM_COPYDATA,_ES_MSGFLT_ALLOW,0);
				}
			}
		}
	}
}

static void _es_get_nice_property_name(DWORD property_id,wchar_buf_t *out_wcbuf)
{
	property_get_canonical_name(property_id,out_wcbuf);
}

// property name without upper case and symbols converted to '_'
static void _es_get_nice_json_property_name(DWORD property_id,wchar_buf_t *out_wcbuf)
{
	wchar_buf_t property_name_wcbuf;

	wchar_buf_init(&property_name_wcbuf);
	
	wchar_buf_empty(out_wcbuf);

	_es_get_nice_property_name(property_id,&property_name_wcbuf);
	
	{
		const wchar_t *p;
		
		p = property_name_wcbuf.buf;
		
		while(*p)
		{
			if ((*p >= 'A') && (*p <= 'Z'))
			{
				wchar_buf_cat_wchar(out_wcbuf,*p - 'A' + 'a');
			}
			else
			if ((*p >= 'a') && (*p <= 'z'))
			{
				wchar_buf_cat_wchar(out_wcbuf,*p);
			}
			else
			if ((*p >= '0') && (*p <= '9'))
			{
				wchar_buf_cat_wchar(out_wcbuf,*p);
			}
			else
			if (*p == '-')
			{
				// ignore it.
			}
			else
			{
				wchar_buf_cat_wchar(out_wcbuf,'_');
			}
			
			p++;
		}
	}

	wchar_buf_kill(&property_name_wcbuf);
}

// escape special characters a json string 
static void _es_escape_json_wchar_string(const wchar_t *s,wchar_buf_t *out_wcbuf)
{
	const wchar_t *p;
	
	wchar_buf_empty(out_wcbuf);
	
	p = s;
	
	while(*p)
	{
		if ((*p == '\\') || (*p == '"'))
		{
			wchar_buf_cat_wchar(out_wcbuf,'\\');
			wchar_buf_cat_wchar(out_wcbuf,*p);
			
			p++;
		}
		else
		if (*p == '\r')
		{
			wchar_buf_cat_wchar(out_wcbuf,'\\');
			wchar_buf_cat_wchar(out_wcbuf,'r');
			
			p++;
		}
		else
		if (*p == '\n')
		{
			wchar_buf_cat_wchar(out_wcbuf,'\\');
			wchar_buf_cat_wchar(out_wcbuf,'n');
			
			p++;
		}
		else
		if (*p == '\t')
		{
			wchar_buf_cat_wchar(out_wcbuf,'\\');
			wchar_buf_cat_wchar(out_wcbuf,'t');
			
			p++;
		}
		else
		if (*p == '\b')
		{
			wchar_buf_cat_wchar(out_wcbuf,'\\');
			wchar_buf_cat_wchar(out_wcbuf,'b');
			
			p++;
		}
		else
		if (*p == '\f')
		{
			wchar_buf_cat_wchar(out_wcbuf,'\\');
			wchar_buf_cat_wchar(out_wcbuf,'f');
			
			p++;
		}
		else
		{
			wchar_buf_cat_wchar(out_wcbuf,*p);
			
			p++;
		}
	}
}

// set the sort to the semicolon delimited list of property canonical names.
// if allow_old_column_ids is true, parse integer column names as old column ids.
static BOOL _es_set_sort_list(const wchar_t *sort_list,int allow_old_column_ids)
{
	BOOL ret;
	wchar_buf_t item_wcbuf;
	utf8_buf_t sort_name_cbuf;
	const wchar_t *sort_list_p;
	SIZE_T sort_count;
	
	ret = TRUE;
	
	wchar_buf_init(&item_wcbuf);
	utf8_buf_init(&sort_name_cbuf);
	
	secondary_sort_clear_all();

	sort_list_p = sort_list;
	sort_count = 0;
	
	for(;;)
	{
		DWORD property_id;
		int ascending;

		sort_list_p = wchar_string_parse_list_item(sort_list_p,&item_wcbuf);
		if (!sort_list_p)
		{
			break;
		}
		
		ascending = 0;
		
		{
			wchar_t *ascending_p;
			
			ascending_p = item_wcbuf.buf;
			
			while(*ascending_p)
			{
				if (*ascending_p == '-')
				{
					const wchar_t *match_p;
					
					match_p = wchar_string_parse_utf8_string(ascending_p + 1,"ascending");
					if (match_p)
					{
						if (!*match_p)
						{
							ascending = 1;

							// truncate name
							*ascending_p = 0;
							break;
						}
					}

					match_p = wchar_string_parse_utf8_string(ascending_p + 1,"descending");
					if (match_p)
					{
						if (!*match_p)
						{
							ascending = -1;

							// truncate name
							*ascending_p = 0;
							break;
						}
					}
				}
			
				ascending_p++;
			}
		}
	
		// backwards compatibility.
		// sort=1
		// we want to sort by name.
		// check if item_wcbuf.buf is all digits.
		// if it is, treat it as an old column id.
		//
		// if allow_old_column_ids is disabled, "1" will match regmatch1
		if (allow_old_column_ids)
		{
			const wchar_t *old_column_ids_p;
			int old_column_id;
			
			old_column_ids_p = wchar_string_parse_int(item_wcbuf.buf,&old_column_id);
			if (old_column_ids_p)
			{
				// int only?
				if (!*old_column_ids_p)
				{
					// lookup old column id.
					property_id = property_id_from_old_column_id(old_column_id);
					if (property_id != EVERYTHING3_INVALID_PROPERTY_ID)
					{
						goto got_property_id;
					}
				}
			}
		}
		
		property_id = property_find(item_wcbuf.buf);
		
got_property_id:
		
		if (property_id != EVERYTHING3_INVALID_PROPERTY_ID)
		{
			if (sort_count)
			{
				if (property_id == _es_primary_sort_property_id)
				{
					// don't duplicate sorts
				}
				else
				{
					secondary_sort_add(property_id,ascending);
				}
			}
			else
			{
				_es_primary_sort_property_id = property_id;
				if (ascending)
				{
					// don't change unless it's specified.
					_es_primary_sort_ascending = ascending;
				}
			}
			
			sort_count++;
		}
		else
		{
			// set the rest of the properties.
			// this one will just be missing.
			// caller can fatal error if they expected valid properties.
			ret = FALSE;
		}
	}

	utf8_buf_kill(&sort_name_cbuf);
	wchar_buf_kill(&item_wcbuf);
	
	return ret;
}

// action 0 == set columns
// action 1 == add columns
// action 2 == remove columns
static BOOL _es_set_columns(const wchar_t *column_list,int action,int allow_old_column_ids)
{
	BOOL ret;
	wchar_buf_t item_wcbuf;
	const wchar_t *column_list_p;
	
	ret = TRUE;
	
	wchar_buf_init(&item_wcbuf);
	
	if (action == 0)
	{
		column_clear_all();	
	}
	
	column_list_p = column_list;
	
	for(;;)
	{
		DWORD property_id;

		column_list_p = wchar_string_parse_list_item(column_list_p,&item_wcbuf);
		if (!column_list_p)
		{
			break;
		}
	
		// backwards compatibility.
		// column=7
		// we want to show the size.
		// check if item_wcbuf.buf is all digits.
		// if it is, treat it as an old column id.
		//
		// if allow_old_column_ids is disabled, "1" will match regmatch1
		if (allow_old_column_ids)
		{
			const wchar_t *old_column_ids_p;
			int old_column_id;
			
			old_column_ids_p = wchar_string_parse_int(item_wcbuf.buf,&old_column_id);
			if (old_column_ids_p)
			{
				// int only?
				if (!*old_column_ids_p)
				{
					// lookup old column id.
					property_id = property_id_from_old_column_id(old_column_id);
					if (property_id != EVERYTHING3_INVALID_PROPERTY_ID)
					{
						goto got_property_id;
					}
				}
			}
		}
		
		property_id = property_find(item_wcbuf.buf);
		
got_property_id:
		
		if (property_id != EVERYTHING3_INVALID_PROPERTY_ID)
		{
			switch(action)
			{
				case 0:
				case 1:
					column_add(property_id);
					break;

				case 2:
				
					switch(property_id)
					{
						case EVERYTHING3_PROPERTY_ID_PATH_AND_NAME:
							// don't default to adding the filename column if it is not already shown.
							es_no_default_filename_column = 1;
							break;

						case EVERYTHING3_PROPERTY_ID_SIZE:
							es_no_default_size_column = 1;
							break;

						case EVERYTHING3_PROPERTY_ID_DATE_MODIFIED:
							es_no_default_date_modified_column = 1;
							break;

						case EVERYTHING3_PROPERTY_ID_DATE_CREATED:
							es_no_default_date_created_column = 1;
							break;

						case EVERYTHING3_PROPERTY_ID_ATTRIBUTES:
							es_no_default_attribute_column = 1;
							break;

					}
					
					column_remove(property_id);
					break;
			}
		}
		else
		{
			// set the rest of the properties.
			// this one will just be missing.
			// caller can fatal error if they expected valid properties.
			ret = FALSE;
		}
	}

	wchar_buf_kill(&item_wcbuf);
	
	return ret;
}

// action 0 == set column colors
// action 1 == add column colors
// action 2 == remove column colors
static BOOL _es_set_column_colors(const wchar_t *column_color_list,int action)
{
	BOOL ret;
	wchar_buf_t item_wcbuf;
	const wchar_t *column_color_list_p;
	int old_column_id;
	
	ret = TRUE;
	
	wchar_buf_init(&item_wcbuf);
	
	if (action == 0)
	{
		column_color_clear_all();	
	}
	
	column_color_list_p = column_color_list;
	old_column_id = 0;
	
	for(;;)
	{
		DWORD property_id;
		wchar_t *color_value;
		WORD color;

		column_color_list_p = wchar_string_parse_list_item(column_color_list_p,&item_wcbuf);
		if (!column_color_list_p)
		{
			break;
		}
		
		color_value = NULL;
	
		{
			wchar_t *keyvalue_p;
			
			keyvalue_p = item_wcbuf.buf;
			
			while(*keyvalue_p)
			{
				if (*keyvalue_p == '=')
				{
					// truncate name
					*keyvalue_p = 0;
					
					color_value = keyvalue_p + 1;
					
					break;
				}
			
				keyvalue_p++;
			}
		}
		
		property_id = EVERYTHING3_INVALID_PROPERTY_ID;
		
		if (color_value)
		{
			property_id = property_find(item_wcbuf.buf);

			color = wchar_string_to_int(color_value);
		}
		else
		{
			// no keyvalue pair, use old column id
			property_id = property_id_from_old_column_id(old_column_id);

			color = wchar_string_to_int(item_wcbuf.buf);
		}
		
		if (property_id != EVERYTHING3_INVALID_PROPERTY_ID)
		{
			switch(action)
			{
				case 0:
				case 1:
					column_color_set(property_id,color);
					break;

				case 2:
					column_color_remove(property_id);
					break;
			}
		}
		else
		{
			// set the rest of the properties.
			// this one will just be missing.
			// caller can fatal error if they expected valid properties.
			ret = FALSE;
		}
		
		old_column_id++;
	}

	wchar_buf_kill(&item_wcbuf);
	
	return ret;
}

// action 0 == set column widths
// action 1 == add column widths
// action 2 == remove column widths
static BOOL _es_set_column_widths(const wchar_t *column_width_list,int action)
{
	BOOL ret;
	wchar_buf_t item_wcbuf;
	const wchar_t *column_width_list_p;
	int old_column_id;
	
	ret = TRUE;
	
	wchar_buf_init(&item_wcbuf);
	
	if (action == 0)
	{
		column_width_clear_all();	
	}
	
	column_width_list_p = column_width_list;
	old_column_id = 0;
	
	for(;;)
	{
		DWORD property_id;
		wchar_t *width_value;
		int width;

		column_width_list_p = wchar_string_parse_list_item(column_width_list_p,&item_wcbuf);
		if (!column_width_list_p)
		{
			break;
		}
		
		width_value = NULL;
	
		{
			wchar_t *keyvalue_p;
			
			keyvalue_p = item_wcbuf.buf;
			
			while(*keyvalue_p)
			{
				if (*keyvalue_p == '=')
				{
					// truncate name
					*keyvalue_p = 0;
					
					width_value = keyvalue_p + 1;
					
					break;
				}
			
				keyvalue_p++;
			}
		}
		
		property_id = EVERYTHING3_INVALID_PROPERTY_ID;
		
		if (width_value)
		{
			property_id = property_find(item_wcbuf.buf);

			width = wchar_string_to_int(width_value);
		}
		else
		{
			// no keyvalue pair, use old column id
			property_id = property_id_from_old_column_id(old_column_id);

			width = wchar_string_to_int(item_wcbuf.buf);
		}
		
		if (property_id != EVERYTHING3_INVALID_PROPERTY_ID)
		{
			switch(action)
			{
				case 0:
				case 1:
					column_width_set(property_id,width);
					break;

				case 2:
					column_width_remove(property_id);
					break;
			}
		}
		else
		{
			// set the rest of the properties.
			// this one will just be missing.
			// caller can fatal error if they expected valid properties.
			ret = FALSE;
		}
		
		old_column_id++;
	}

	wchar_buf_kill(&item_wcbuf);
	
	return ret;
}

// find the last standard efu column
// returns NULL if none. (caller should insert at the start)
static column_t *_es_find_last_standard_efu_column(void)
{
	column_t *column;
	column_t *last;
	
	last = NULL;
	
	column = column_order_start;
	while(column)
	{
		switch(column->property_id)
		{
			case EVERYTHING3_PROPERTY_ID_NAME:
			case EVERYTHING3_PROPERTY_ID_PATH:
			case EVERYTHING3_PROPERTY_ID_PATH_AND_NAME:
			case EVERYTHING3_PROPERTY_ID_SIZE:
			case EVERYTHING3_PROPERTY_ID_DATE_MODIFIED:
			case EVERYTHING3_PROPERTY_ID_DATE_CREATED:
			case EVERYTHING3_PROPERTY_ID_ATTRIBUTES:
				last = column;
				break;
		}
		
		column = column->order_next;
	}
	
	return last;
}

// adds size, date-modified, date-create and attributes column. (if they are indexed)
// if a column is added, the columns are reordered to:
// Filename,Size,Date Modified,Date Created,Attributes
// 
// the FILE_ATTRIBUTE_DIRECTORY bit is always valid, so always add the attributes column.
// only do this AFTER sending a query to avoid gathering unindexed information.
//
// only attributes should be added when allow_reorder is 0.
// DO NOT reorder columns after sending a query.
// this would break ipc3 as we expect the properties in data stream to be in the requested order.
static void _es_add_standard_efu_columns(int size,int date_modified,int date_created,int attributes,int allow_reorder)
{
	column_t *path_and_name_column;
	column_t *size_column;
	column_t *date_modified_column;
	column_t *date_created_column;
	column_t *attribute_column;
	int changed;
	
	// Filename,Size,Date Modified,Date Created,Attributes
	path_and_name_column = column_find(EVERYTHING3_PROPERTY_ID_PATH_AND_NAME);
	size_column = column_find(EVERYTHING3_PROPERTY_ID_SIZE);
	date_modified_column = column_find(EVERYTHING3_PROPERTY_ID_DATE_MODIFIED);
	date_created_column = column_find(EVERYTHING3_PROPERTY_ID_DATE_CREATED);
	attribute_column = column_find(EVERYTHING3_PROPERTY_ID_ATTRIBUTES);
	changed = 0;

	if (size)
	{
		if (!es_no_default_size_column)
		{
			if (!size_column)
			{
				size_column = column_add(EVERYTHING3_PROPERTY_ID_SIZE);
				
				changed = 1;
			}
		}
	}

	if (date_modified)
	{
		if (!es_no_default_date_modified_column)
		{
			if (!date_modified_column)
			{
				date_modified_column = column_add(EVERYTHING3_PROPERTY_ID_DATE_MODIFIED);
				
				changed = 1;
			}
		}
	}

	if (date_created)
	{
		if (!es_no_default_date_created_column)
		{
			if (!date_created_column)
			{
				date_created_column = column_add(EVERYTHING3_PROPERTY_ID_DATE_CREATED);
				
				changed = 1;
			}
		}
	}

	// FILE_ATTRIBUTE_DIRECTORY is always available.
	if (attributes)
	{
		if (!es_no_default_attribute_column)
		{
			if (!attribute_column)
			{
				// add the attributes property to the start.
				// we re-add the other main columns below.
				attribute_column = column_add(EVERYTHING3_PROPERTY_ID_ATTRIBUTES);
				
				if (!allow_reorder)
				{
					column_remove_order(attribute_column);
					column_insert_after(attribute_column,_es_find_last_standard_efu_column());
				}
				
				changed = 1;
			}
		}
	}

	if (changed)
	{
		if (allow_reorder)
		{
			// reorder columns so we have:
			// Filename,Size,Date Modified,Date Created,Attributes,other-properties...
			// readd columns in backwards order.
			if (attribute_column)
			{
				column_remove_order(attribute_column);
				column_insert_order_at_start(attribute_column);
			}
			
			if (date_created_column)
			{
				column_remove_order(date_created_column);
				column_insert_order_at_start(date_created_column);
			}
			
			if (date_modified_column)
			{
				column_remove_order(date_modified_column);
				column_insert_order_at_start(date_modified_column);
			}
			
			if (size_column)
			{
				column_remove_order(size_column);
				column_insert_order_at_start(size_column);
			}

			if (path_and_name_column)
			{
				column_remove_order(path_and_name_column);
				column_insert_order_at_start(path_and_name_column);
			}
		}
	}
}

// is the requested EVERYTHING_IPC_FILE_INFO_* type indexed?
// returns TRUE if it is.
// Otherwise, returns FALSE.
// If unknown, returns FALSE.
static BOOL _es_ipc2_is_file_info_indexed(DWORD file_info_type)
{
	if (SendMessage(es_everything_hwnd,EVERYTHING_WM_IPC,EVERYTHING_IPC_IS_FILE_INFO_INDEXED,file_info_type))
	{
		return TRUE;
	}
	
	return FALSE;
}

// pause mode. (scrollable output)
// ipc_version should be a ES_IPC_VERSION_FLAG_* flag.
// data should be the corresponding ipc data.
static void _es_output_pause(DWORD ipc_version,const void *data)
{
	HANDLE std_input_handle;
	int start_index;
	int last_start_index;
	int last_hscroll;
	int info_line;
	SIZE_T total_lines;
	
	info_line = es_console_high - 1;
	
	total_lines = 0;
	
	switch(ipc_version)
	{
		case ES_IPC_VERSION_FLAG_IPC1:
			total_lines = ((EVERYTHING_IPC_LIST *)data)->numitems;
			break;

		case ES_IPC_VERSION_FLAG_IPC2:
			total_lines = ((EVERYTHING_IPC_LIST2 *)data)->numitems;
			break;
			
		case ES_IPC_VERSION_FLAG_IPC3:
			total_lines = ((ipc3_result_list_t *)data)->viewport_count;
			break;
	}
	
	// include the header in the total line count.
	if (es_header > 0)
	{
		total_lines = safe_size_add_one(total_lines);
	}

	if (info_line > safe_int_from_size(total_lines))
	{
		info_line = safe_int_from_size(total_lines);
	}
	
	// push lines above up.
	{
		int i;
		
		if (es_console_window_y + info_line + 1 > es_console_size_high)
		{
			for(i=0;i<(es_console_window_y + info_line + 1) - es_console_size_high;i++)
			{
				DWORD numwritten;
				
				WriteFile(es_output_handle,"\r\n",2,&numwritten,0);
			}
										
			es_console_window_y = es_console_size_high - (int)(info_line + 1);
		}
	}
	
	std_input_handle = GetStdHandle(STD_INPUT_HANDLE);

	es_output_cibuf = mem_alloc(es_console_wide * sizeof(CHAR_INFO));
	
	// set cursor pos to the bottom of the screen
	{
		COORD cur_pos;
		cur_pos.X = es_console_window_x;
		cur_pos.Y = es_console_window_y + (int)info_line;
		
		SetConsoleCursorPosition(es_output_handle,cur_pos);
	}

	// write out some basic usage at the bottom
	{
		DWORD numwritten;

		WriteFile(es_output_handle,ES_PAUSE_TEXT,(DWORD)utf8_string_get_length_in_bytes(ES_PAUSE_TEXT),&numwritten,0);
	}

	start_index = 0;
	last_start_index = 0;
	last_hscroll = 0;

	for(;;)
	{
		INPUT_RECORD ir;
		DWORD numread;

		switch(ipc_version)
		{
			case ES_IPC_VERSION_FLAG_IPC1:
				_es_output_ipc1_results((EVERYTHING_IPC_LIST *)data,start_index,es_console_high-1);
				break;
				
			case ES_IPC_VERSION_FLAG_IPC2:
				_es_output_ipc2_results((EVERYTHING_IPC_LIST2 *)data,start_index,es_console_high-1);
				break;	
				
			case ES_IPC_VERSION_FLAG_IPC3:
			
				// reposition the memory stream to the current start index.
				// if this isn't cached, we will read from the pipe stream, find the correct offset and cache the result.
				ipc3_result_list_seek_to_offset_from_index((ipc3_result_list_t *)data,start_index);
				
				_es_output_ipc3_results((ipc3_result_list_t *)data,start_index,es_console_high-1);
				break;
		}
		
		// is everything is shown?
		if (es_max_wide <= es_console_wide)
		{
			if (total_lines < (SIZE_T)es_console_high)
			{
				goto exit;
			}
		}
		
start:
		
		for(;;)
		{
			if (PeekConsoleInput(std_input_handle,&ir,1,&numread)) 
			{
				if (numread)
				{
					ReadConsoleInput(std_input_handle,&ir,1,&numread);

					if ((ir.EventType == KEY_EVENT) && (ir.Event.KeyEvent.bKeyDown))
					{
						if (ir.Event.KeyEvent.wVirtualKeyCode == VK_RETURN)
						{
							if (total_lines < (SIZE_T)es_console_high + 1)
							{
								goto exit;
							}
							if (start_index == total_lines - es_console_high + 1)
							{
								goto exit;
							}
							start_index += 1;
							break;
						}
						else
						if (ir.Event.KeyEvent.wVirtualKeyCode == VK_SPACE)
						{
							if (total_lines < (SIZE_T)es_console_high + 1)
							{
								goto exit;
							}
							
							if (start_index == total_lines - es_console_high + 1)
							{
								goto exit;
							}
							
							start_index += 1;
							break;
						}
						else
						if (ir.Event.KeyEvent.wVirtualKeyCode == VK_RIGHT)
						{
							es_output_cibuf_hscroll += 5;
							
							if (es_max_wide > es_console_wide)
							{
								if (es_output_cibuf_hscroll > es_max_wide - es_console_wide)
								{
									es_output_cibuf_hscroll = es_max_wide - es_console_wide;
								}
							}
							else
							{
								es_output_cibuf_hscroll = 0;
							}
							break;
						}
						else
						if (ir.Event.KeyEvent.wVirtualKeyCode == VK_LEFT)
						{
							es_output_cibuf_hscroll -= 5;
							if (es_output_cibuf_hscroll < 0)
							{
								es_output_cibuf_hscroll = 0;
							}
							break;
						}
						else
						if (ir.Event.KeyEvent.wVirtualKeyCode == VK_UP)
						{
							start_index -= 1;
							break;
						}
						else
						if (ir.Event.KeyEvent.wVirtualKeyCode == VK_DOWN)
						{
							start_index += 1;
							break;
						}
						else
						if (ir.Event.KeyEvent.wVirtualKeyCode == VK_PRIOR)
						{
							start_index -= es_console_high - 1;
							break;
						}
						else
						if (ir.Event.KeyEvent.wVirtualKeyCode == VK_HOME)
						{
							start_index = 0;
							break;
						}
						else
						if (ir.Event.KeyEvent.wVirtualKeyCode == VK_END)
						{
							start_index = (int)total_lines - es_console_high + 1;
							break;
						}
						else
						if ((ir.Event.KeyEvent.wVirtualKeyCode == 'Q') || (ir.Event.KeyEvent.wVirtualKeyCode == VK_ESCAPE))
						{
							goto exit;
						}
						else
						if (_es_is_valid_key(&ir))
						{
							start_index += es_console_high - 1;
							break;
						}
					}
				}
			}
	
			Sleep(1);
		}
		
		// clip start index.
		if (start_index < 0)
		{
			start_index = 0;
		}
		else
		if (total_lines > (SIZE_T)es_console_high + 1)
		{
			if (start_index > (int)total_lines - es_console_high + 1)
			{
				start_index = (int)total_lines - es_console_high + 1;
			}
		}
		else
		{
			start_index = 0;
		}
		
		if (last_start_index != start_index)
		{
			last_start_index = start_index;
		}
		else
		if (last_hscroll != es_output_cibuf_hscroll)
		{
			last_hscroll = es_output_cibuf_hscroll;
		}
		else
		{
			goto start;
		}
	}
	
exit:						
	
	// remove text.
	if (es_output_cibuf)
	{
		// set cursor pos to the bottom of the screen
		{
			COORD cur_pos;
			cur_pos.X = es_console_window_x;
			cur_pos.Y = es_console_window_y + (int)info_line;
			
			SetConsoleCursorPosition(es_output_handle,cur_pos);
		}

		// write out some basic usage at the bottom
		{
			DWORD numwritten;
	
			WriteFile(es_output_handle,ES_BLANK_PAUSE_TEXT,(DWORD)utf8_string_get_length_in_bytes(ES_BLANK_PAUSE_TEXT),&numwritten,0);
		}
	
		// reset cursor pos.
		{
			COORD cur_pos;
			cur_pos.X = es_console_window_x;
			cur_pos.Y = es_console_window_y + (int)info_line;
			
			SetConsoleCursorPosition(es_output_handle,cur_pos);
		}
		
		// free
		mem_free(es_output_cibuf);
	}								
}

static void _es_output_noncell_total_size(ES_UINT64 total_size)
{
	_es_output_noncell_printf("%I64u\r\n",total_size);
}

static void _es_output_noncell_result_count(ES_UINT64 result_count)
{
	_es_output_noncell_printf("%I64u\r\n",result_count);
}
