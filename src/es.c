
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

// TODO:
// search journal
// 
// [HIGH] vs2019 project and sln files. -move to vs subfolder.
// [HIGH] c# cmdlet for powershell.
// Maybe a configurable prompt, so the "current" (bottom-most) line, total count of results. -footer?
// Index Journal API
// export to clipboard (aka copy to clipboard)
// add a -max-path option. (limit paths to 259 chars -use shortpaths if we exceed.)
// add a -short-full-path option.
// -wait block until search results one result, combine with new-results-only:
// open to show summary information: total size and number of results (like -get-result-count)
// -export-m3u/-export-m3u8 need to use short paths for VLFNs. -No media players support VLFNs.
// ideally, -sort should apply the sort AFTER the search. -currently filters are overwriting the sort. -this needs to be done at the query level so we dont block with a sort. -add a sort-after parameter...
// add a -unquote command line option so we can handle powershell, eg: es.exe -unquote case:`"$x`" => es.exe -unquote "case:"abc 123"" => es.exe case:"abc 123"
// export results to environment variables -mostly useful for -n 1
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
// [23/72]:
// emulate LESS
// custom date/time format. eg: dd/MM/yy
// 1.1.0.30
// *-length=12m syntax.
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
// *number formatting -Use system number formatting (some localizations don't use , for the comma)
// *shouldn't we use environment variables for storing switches? like MORE? -registry does make it easy. -using ini now

// NOTES:
// ScrollConsoleScreenBuffer is unuseable because it fills invalidated areas, which causes unnecessary flickering.

#include "es.h"

#define _ES_COPYDATA_IPCTEST_QUERYCOMPLETEW			0
#define _ES_COPYDATA_IPCTEST_QUERYCOMPLETE2W		1

#define _ES_EXPORT_TYPE_NONE		0
#define _ES_EXPORT_TYPE_CSV			1
#define _ES_EXPORT_TYPE_EFU			2
#define _ES_EXPORT_TYPE_TXT			3
#define _ES_EXPORT_TYPE_M3U			4
#define _ES_EXPORT_TYPE_M3U8		5
#define _ES_EXPORT_TYPE_TSV			6
#define _ES_EXPORT_TYPE_JSON		7
#define _ES_EXPORT_TYPE_NOFORMAT	8 // same as NONE with no formatting.

#define _ES_MODE_SEARCH						0
#define _ES_MODE_GET_EVERYTHING_VERSION		1
#define _ES_MODE_LIST_PROPERTIES			2
#define _ES_MODE_GET_JOURNAL_ID				3
#define _ES_MODE_GET_JOURNAL_POS			4
#define _ES_MODE_READ_JOURNAL				5

#define _ES_EXPORT_BUF_SIZE			65536

#define _ES_PAUSE_TEXT				"ESC=Quit; Up,Down,Left,Right,Page Up,Page Down,Home,End=Scroll"
#define _ES_BLANK_PAUSE_TEXT		"                                                              "

#define _ES_MSGFLT_ALLOW			1

typedef struct _es_tagCHANGEFILTERSTRUCT_s
{
	DWORD cbSize;
	DWORD ExtStatus;
	
}_ES_CHANGEFILTERSTRUCT;

typedef struct _es_read_journal_s
{
	SIZE_T run;
	SIZE_T numitems;
	int state;
	
}_es_read_journal_t;

static int _es_main(void);
static void DECLSPEC_NORETURN _es_bad_switch_param(const char *format,...);
static void _es_console_fill(SIZE_T count,int ascii_ch);
static void _es_output_cell_write_console_wchar_string(const wchar_t *text,int is_highlighted);
static void _es_output_cell_utf8_string(const ES_UTF8 *text,int is_highlighted);
static void _es_output_cell_wchar_string(const wchar_t *text,int is_highlighted);
static void _es_export_write_data(const ES_UTF8 *text,SIZE_T length_in_bytes);
static void _es_export_write_wchar_string_n(const wchar_t *text,SIZE_T wlen);
static BOOL _es_get_locale_info(LCTYPE LCType,wchar_t out_wbuf[256]);
static void _es_format_number(ES_UINT64 number,int allow_digit_grouping,wchar_buf_t *out_wcbuf);
static void _es_format_fixed_q1k(ES_UINT64 number,int use_locale,int allow_digit_grouping,int is_signed,int allow_integer_representation,wchar_buf_t *out_wcbuf);
static void _es_format_fixed_q1m(ES_UINT64 number,int use_locale,int allow_digit_grouping,int is_signed,wchar_buf_t *out_wcbuf);
static void _es_format_dimensions(EVERYTHING3_DIMENSIONS *dimensions_value,wchar_buf_t *out_wcbuf);
static int _es_compare_list_items(const EVERYTHING_IPC_ITEM *a,const EVERYTHING_IPC_ITEM *b);
static void _es_output_cell_text_property_wchar_string(const wchar_t *value);
static void _es_output_cell_text_property_utf8_string(const ES_UTF8 *value);
static void _es_output_cell_text_property_utf8_string_n(const ES_UTF8 *value,SIZE_T length_in_bytes);
static void _es_output_cell_highlighted_text_property_wchar_string(const wchar_t *value);
static void _es_output_cell_highlighted_text_property_utf8_string(const ES_UTF8 *value);
static void _es_output_cell_unknown_property(void);
static void _es_output_cell_size_property(ES_UINT64 value);
static void _es_output_cell_filetime_property(ES_UINT64 value);
static void _es_output_cell_time_property(DWORD value);
static void _es_output_cell_date_property(DWORD value);
static void _es_output_cell_duration_property(ES_UINT64 value);
static void _es_output_cell_attribute_property(DWORD file_attributes);
static void _es_output_cell_number_property(ES_UINT64 value,ES_UINT64 empty_value,BOOL allow_digit_grouping);
static void _es_output_cell_formatted_number_property(const ES_UTF8 *text);
static void _es_output_cell_hex_number8_property(DWORD value);
static void _es_output_cell_hex_number16_property(ES_UINT64 value,ES_UINT64 empty_value);
static void _es_output_cell_hex_number32_property(EVERYTHING3_UINT128 *uint128_value);
static void _es_output_cell_khz_property(ES_UINT64 value,ES_UINT64 empty_value);
static void _es_output_cell_percent_property(BYTE value);
static void _es_output_cell_kbps_property(ES_UINT64 value,ES_UINT64 empty_value);
static void _es_output_cell_rating_property(BYTE value);
static void _es_output_cell_yesno_property(BYTE value);
static void _es_output_cell_fixed_q1k_property(ES_UINT64 value,ES_UINT64 empty_value,int is_signed);
static void _es_output_cell_fixed_q1m_property(ES_UINT64 value,ES_UINT64 empty_value,int is_signed);
static void _es_output_cell_f_stop_property(__int32 value);
static void _es_output_cell_exposure_time_property(__int32 value);
static void _es_output_cell_iso_speed_property(WORD value);
static void _es_output_cell_exposure_bias_property(__int32 value);
static void _es_output_cell_bcps_property(__int32 value);
static void _es_output_cell_small_number_property_with_suffix(ES_UINT64 value,ES_UINT64 empty_value,const ES_UTF8 *suffix);
static void _es_output_cell_dimensions_property(EVERYTHING3_DIMENSIONS *dimensions_value);
static void _es_output_cell_data_property(const BYTE *data,SIZE_T size);
static void _es_output_cell_separator(void);
static void _es_output_noncell_wchar_string(const wchar_t *text);
static void _es_output_noncell_wchar_string_n(const wchar_t *text,SIZE_T length_in_wchars);
static void _es_output_noncell_utf8_string(const ES_UTF8 *text);
static void _es_output_noncell_printf(const ES_UTF8 *text,...);
static void _es_output_header(void);
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
static ES_UINT64 _es_ipc2_calculate_total_size(EVERYTHING_IPC_LIST2 *list);
static void _es_output_ipc3_results(ipc3_result_list_t *result_list,SIZE_T index_start,SIZE_T count);
static LRESULT __stdcall _es_window_proc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam);
static void _es_help(void);
static HWND _es_find_ipc_window(void);
static const wchar_t *_es_parse_command_line_option_start(const wchar_t *s);
static BOOL _es_check_option_utf8_string(const wchar_t *param,const ES_UTF8 *s);
static void *_es_ipc2_get_column_data(EVERYTHING_IPC_LIST2 *list,SIZE_T index,DWORD property_id,DWORD property_highlight);
static void _es_format_size(ES_UINT64 size,int size_format,wchar_buf_t *wcbuf);
static void _es_format_filetime(ES_UINT64 filetime,wchar_buf_t *wcbuf);
static void _es_format_duration(ES_UINT64 filetime,wchar_buf_t *wcbuf);
static void _es_format_attributes(DWORD attributes,wchar_buf_t *wcbuf);
static const wchar_t *_es_parse_command_line_option_name(const wchar_t *argv,const ES_UTF8 *s);
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
static void _es_set_sort_list(const wchar_t *sort_list,int allow_old_column_ids,int die_on_bad_value);
static void _es_set_columns(const wchar_t *sort_list,int action,int allow_old_column_ids,BOOL die_on_bad_value);
static void _es_set_column_colors(const wchar_t *column_color_list,int action,BOOL die_on_bad_value);
static void _es_set_column_widths(const wchar_t *column_width_list,int action,BOOL die_on_bad_value);
static SIZE_T _es_highlighted_wchar_string_get_length_in_wchars(const wchar_t *s);
static void _es_add_standard_efu_columns(int size,int date_modified,int date_created,int attributes,int allow_reorder);
static column_t *_es_find_last_standard_efu_column(void);
static BOOL _es_ipc2_is_file_info_indexed(DWORD file_info_type);
static void _es_output_pause(DWORD ipc_version,const void *data);
static void _es_output_noncell_total_size(ES_UINT64 total_size);
static void _es_output_noncell_result_count(ES_UINT64 result_count);
static BOOL _es_resolve_sort_ascending(DWORD property_id,int ascending);
static int _es_list_properties_compare(const wchar_t *a,const wchar_t *b);
static void _es_format_clsid(const CLSID *clsid_value,utf8_buf_t *out_cbuf);
static void _es_output_footer(SIZE_T total_items,ES_UINT64 total_size);
static BOOL _es_read_journal_callback_proc(void *user_data,_ipc3_journal_change_t *change);
static ES_UINT64 _es_parse_date(const wchar_t *date_string);
static const wchar_t *_es_parse_date_word(const wchar_t *s,int digit_count,WORD *out_value);
static BOOL _es_should_allow_property_system(const wchar_t *name);
static DWORD _es_parse_journal_action_filter(const wchar_t *filter_list);
static ES_UINT64 _es_get_today_filetime(void);
static ES_UINT64 _es_get_today_offset_filetime(__int64 offset);
static ES_UINT64 _es_get_tomorrow_filetime(void);
static ES_UINT64 _es_get_yesterday_filetime(void);

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
static char _es_save_db = 0;
static BYTE _es_export_type = _ES_EXPORT_TYPE_NONE;
static HANDLE _es_export_file = INVALID_HANDLE_VALUE;
static BYTE *_es_export_buf = 0;
static BYTE *_es_export_p;
static DWORD _es_export_avail = 0;
static char _es_size_leading_zero = 0; // depreciated.
static char _es_run_count_leading_zero = 0; // depreciated
static char _es_digit_grouping = 1;
static char _es_locale_grouping = 3; // 0-9 or 32
static char _es_locale_lzero = 1; // leading zero
static char _es_locale_negnumber = 1; // negative number format, 1 == -1.1
static wchar_buf_t *_es_locale_thousand_wcbuf = NULL; // thousand separator ","
static wchar_buf_t *_es_locale_decimal_wcbuf = NULL; // decimal separator "."
static SIZE_T _es_offset = 0;
static SIZE_T _es_max_results = SIZE_MAX;
static DWORD _es_ret = ES_ERROR_SUCCESS; // the return code from main()
static const wchar_t *_es_command_line = 0;
static BYTE _es_command_line_was_eq = 0; // -switch=value
static BYTE _es_size_format = 1; // 0 = auto, 1=bytes, 2=kb
static BYTE _es_date_format = 0; // display/export (set on query) date/time format 0 = system format, 1=iso-8601 (as local time), 2=filetime in decimal, 3=iso-8601 (in utc)
static BYTE _es_display_date_format = 0; // display date/time format 0 = system format, 1=iso-8601 (as local time), 2=filetime in decimal, 3=iso-8601 (in utc)
static BYTE _es_export_date_format = 0; // export date/time format 0 = default, 1=iso-8601 (as local time), 2=filetime in decimal, 3=iso-8601 (in utc)
static CHAR_INFO *_es_output_cibuf = 0;
static int _es_output_cibuf_hscroll = 0;
static WORD _es_output_color = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
static int _es_output_cibuf_x = 0;
static int _es_output_cibuf_y = 0;
static int _es_max_wide = 0;
static int _es_console_window_wide = 80;
static int _es_console_window_high = 25;
static int _es_console_size_high = 25;
static int _es_console_window_x = 0;
static int _es_console_window_y = 0;
static char _es_pause = 0; 
static char _es_empty_search_help = 0;
static char _es_hide_empty_search_results = 0;
static char _es_save = 0;
static HWND _es_everything_hwnd = 0;
static char _es_get_result_count = 0; // 1 = show result count only
static char _es_get_total_size = 0; // 1 = calculate total result size, only display this total size and exit.
static char _es_no_result_error = 0; // 1 = set errorlevel if no results found.
static HANDLE _es_output_handle = 0; // current output console or file handle.
static UINT _es_cp = 0; // current code page
static char _es_output_is_char = 0; // default to file, unless we can get the console mode.
static WORD _es_default_attributes = 0x07; // grey text on black background.
static void *_es_run_history_data = 0; // run count command
static DWORD _es_run_history_count = 0; // run count command
static int _es_run_history_command = 0;
static SIZE_T _es_run_history_size = 0;
static char _es_header = 0; // 0 == resolve based on export type; >0 == show; <0 == hide.
static char _es_footer = 0; // 0 == resolve based on export type; >0 == show; <0 == hide.
static char _es_double_quote = 0; // always use double quotes for filenames.
static char _es_csv_double_quote = 1; // always use double quotes for CSV for consistancy.
static char _es_utf8_bom = 0;
static wchar_buf_t *_es_search_wcbuf = NULL;
static HWND _es_reply_hwnd = 0;
static char _es_loaded_appdata_ini = 0; // loaded settings from appdata, we should save to appdata.
static column_t *_es_output_column = NULL; // current output column
static SIZE_T _es_output_cell_overflow = 0;
static char _es_is_in_header = 0;
static char _es_no_default_filename_column = 0; // Don't automatically add the filename column.
static char _es_no_default_size_column = 0; // EFU only
static char _es_no_default_date_modified_column = 0; // EFU only
static char _es_no_default_date_created_column = 0; // EFU only
static char _es_no_default_attribute_column = 0; // EFU only
static char _es_folder_append_path_separator = 0; // append a trailing '\\' at the end of folder path and names. -resolve based on export type; >0 == show; <0 == hide. 
static char _es_newline_type = 0; // 0==CRLF, 1==LF, 2==NUL
static char _es_mode = _ES_MODE_SEARCH;
static ES_UINT64 _es_from_journal_id = ES_UINT64_MAX; // resume journal from this journal-id
static ES_UINT64 _es_from_change_id = ES_UINT64_MAX; // resume journal from this change-id
static ES_UINT64 _es_from_date = ES_UINT64_MAX; // resume journal from this change-date
static ES_UINT64 _es_to_journal_id = ES_UINT64_MAX; // end journal on this change-id (exclusive)
static ES_UINT64 _es_to_change_id = ES_UINT64_MAX; // end journal on this change-id (exclusive)
static ES_UINT64 _es_to_date = ES_UINT64_MAX; // end journal on this change-date (exclusive)
static char _es_from_now = FALSE; // resume journal from this change-id
static char _es_to_now = FALSE; // end journal on last change.
static DWORD _es_action_filter = 0xffffffff; // all filters.
static char _es_watch = FALSE; // watch for a change and trigger (exit) when filter matches.

wchar_buf_t *es_instance_name_wcbuf = NULL;
DWORD es_timeout = 0;
DWORD es_ipc_version = 0xffffffff; // allow all ipc versions
int es_pixels_to_characters_mul = 1; // default column widths in logical pixels to characters
int es_pixels_to_characters_div = 5; // default column widths in logical pixels to characters

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
	
	search_length_in_wchars = _es_search_wcbuf->length_in_wchars;
	
	// EVERYTHING_IPC_QUERY includes the NULL terminator.
	size = 	sizeof(EVERYTHING_IPC_QUERY);
	size = safe_size_add(size,safe_size_mul_sizeof_wchar(search_length_in_wchars));
	
	utf8_buf_grow_size(&cbuf,size);
	query = (EVERYTHING_IPC_QUERY *)cbuf.buf;

	if (_es_max_results <= ES_DWORD_MAX)
	{
		query->max_results = (DWORD)_es_max_results;
	}
	else
	{
		query->max_results = ES_DWORD_MAX;
	}

	query->offset = 0;
	query->reply_copydata_message = _ES_COPYDATA_IPCTEST_QUERYCOMPLETEW;
	query->search_flags = (_es_match_case?EVERYTHING_IPC_MATCHCASE:0) | (_es_match_diacritics?EVERYTHING_IPC_MATCHACCENTS:0) | (_es_match_whole_word?EVERYTHING_IPC_MATCHWHOLEWORD:0) | (_es_match_path?EVERYTHING_IPC_MATCHPATH:0);
	query->reply_hwnd = (DWORD)(uintptr_t)_es_reply_hwnd;
	os_copy_memory(query->search_string,_es_search_wcbuf->buf,(search_length_in_wchars+1)*sizeof(WCHAR));

	// it's not fatal if this is too large.
	// a different IPC method might succeed..
	if (size <= ES_DWORD_MAX)
	{
		COPYDATASTRUCT cds;
		
		cds.cbData = (DWORD)size;
		cds.dwData = EVERYTHING_IPC_COPYDATAQUERY;
		cds.lpData = query;

		if (SendMessage(_es_everything_hwnd,WM_COPYDATA,(WPARAM)_es_reply_hwnd,(LPARAM)&cds) == TRUE)
		{
			// we are committed to ipc1
			// if we are exporting as EFU, make sure the attribute column is shown.
			// but don't allow reordering.
			if (_es_export_type == _ES_EXPORT_TYPE_EFU)
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
	
	search_length_in_wchars = _es_search_wcbuf->length_in_wchars;
	
	size = sizeof(EVERYTHING_IPC_QUERY2);
	size = safe_size_add(size,safe_size_mul_sizeof_wchar(safe_size_add_one(search_length_in_wchars)));

	utf8_buf_grow_size(&cbuf,size);
	query = (EVERYTHING_IPC_QUERY2 *)cbuf.buf;

	if (_es_export_type == _ES_EXPORT_TYPE_EFU)
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
	
	if (_es_footer > 0)
	{
		request_flags |= EVERYTHING_IPC_QUERY2_REQUEST_SIZE;
	}
	
	if (_es_get_result_count)
	{
		request_flags = 0;
	}
	
	if (_es_get_total_size)
	{
		// we only want size.
		request_flags = EVERYTHING_IPC_QUERY2_REQUEST_SIZE;
	}
	
	if (_es_max_results <= ES_DWORD_MAX)
	{
		query->max_results = (DWORD)_es_max_results;
	}
	else
	{
		query->max_results = ES_DWORD_MAX;
	}

	if (_es_offset <= ES_DWORD_MAX)
	{
		query->offset = (DWORD)_es_offset;
	}
	else
	{
		query->offset = ES_DWORD_MAX;
	}

	query->reply_copydata_message = _ES_COPYDATA_IPCTEST_QUERYCOMPLETE2W;
	query->search_flags = (_es_match_case?EVERYTHING_IPC_MATCHCASE:0) | (_es_match_diacritics?EVERYTHING_IPC_MATCHACCENTS:0) | (_es_match_whole_word?EVERYTHING_IPC_MATCHWHOLEWORD:0) | (_es_match_path?EVERYTHING_IPC_MATCHPATH:0);
	query->reply_hwnd = (DWORD)(uintptr_t)_es_reply_hwnd;
	query->request_flags = request_flags;
	query->sort_type = _es_get_ipc_sort_type_from_property_id(_es_primary_sort_property_id,_es_primary_sort_ascending);
	os_copy_memory(query+1,_es_search_wcbuf->buf,(search_length_in_wchars + 1) * sizeof(WCHAR));

	// it's not fatal if this is too large.
	// a different IPC method might succeed..
	if (size <= ES_DWORD_MAX)
	{
		cds.cbData = (DWORD)size;
		cds.dwData = EVERYTHING_IPC_COPYDATA_QUERY2;
		cds.lpData = query;

		if (SendMessage(_es_everything_hwnd,WM_COPYDATA,(WPARAM)_es_reply_hwnd,(LPARAM)&cds) == TRUE)
		{
			// we are committed to ipc2
			// if we are exporting as EFU, make sure the attribute column is shown.
			// but don't allow reordering.
			if (_es_export_type == _ES_EXPORT_TYPE_EFU)
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
	
		utf8_buf_copy_wchar_string(&search_cbuf,_es_search_wcbuf->buf);

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

		if (_es_export_type == _ES_EXPORT_TYPE_EFU)
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

		if (_es_get_total_size)
		{
			search_flags |= IPC3_SEARCH_FLAG_TOTAL_SIZE;

			// we don't need any results.
			_es_max_results = 0;
			
			// we don't need any columns
			column_clear_all();
		}
		
		if (_es_footer > 0)
		{
			search_flags |= IPC3_SEARCH_FLAG_TOTAL_SIZE;
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
			packet_d = _es_copy_size_t(packet_d,_es_max_results);

			// primary sort
			
			packet_d = ipc3_copy_len_vlq(packet_d,search_sort_count);

			packet_d = _es_copy_dword(packet_d,_es_primary_sort_property_id);
			packet_d = _es_copy_dword(packet_d,_es_resolve_sort_ascending(_es_primary_sort_property_id,_es_primary_sort_ascending) ? 0 : IPC3_SEARCH_SORT_FLAG_DESCENDING);

			// secondary sort.
			
			{
				secondary_sort_t *secondary_sort;
				
				secondary_sort = secondary_sort_start;
				
				while(secondary_sort)
				{
					packet_d = _es_copy_dword(packet_d,secondary_sort->property_id);
					packet_d = _es_copy_dword(packet_d,_es_resolve_sort_ascending(secondary_sort->property_id,secondary_sort->ascending) ? 0 : IPC3_SEARCH_SORT_FLAG_DESCENDING);
					
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
							case PROPERTY_FORMAT_TEXT8:
							case PROPERTY_FORMAT_TEXT10:
							case PROPERTY_FORMAT_TEXT12:
							case PROPERTY_FORMAT_TEXT16:
							case PROPERTY_FORMAT_TEXT24:
							case PROPERTY_FORMAT_TEXT30:
							case PROPERTY_FORMAT_TEXT32:
							case PROPERTY_FORMAT_TEXT47:
							case PROPERTY_FORMAT_TEXT48:
							case PROPERTY_FORMAT_TEXT64:
							case PROPERTY_FORMAT_EXTENSION:
							case PROPERTY_FORMAT_FORMATED_TEXT8:
							case PROPERTY_FORMAT_FORMATED_TEXT12:
							case PROPERTY_FORMAT_FORMATED_TEXT16:
							case PROPERTY_FORMAT_FORMATED_TEXT24:
							case PROPERTY_FORMAT_FORMATED_TEXT32:
								property_request_flags |= IPC3_SEARCH_PROPERTY_REQUEST_FLAG_HIGHLIGHT;
								break;
						}
					}
					
					// these properties are normally integers.
					// but we don't want to have to deal with formatting them in ES.
					// so just request preformatted strings from Everything.
					// eg: file-signature: a integer value of 10 = image/png
					switch(property_get_format(column->property_id))
					{
						case PROPERTY_FORMAT_FORMATED_TEXT8:
						case PROPERTY_FORMAT_FORMATED_TEXT12:
						case PROPERTY_FORMAT_FORMATED_TEXT16:
						case PROPERTY_FORMAT_FORMATED_TEXT24:
						case PROPERTY_FORMAT_FORMATED_TEXT32:
							// maybe an option to export raw values?
							property_request_flags |= IPC3_SEARCH_PROPERTY_REQUEST_FLAG_FORMAT;
							break;
							
						case PROPERTY_FORMAT_NONE:
							if (_es_export_type == _ES_EXPORT_TYPE_NONE)
							{
								property_request_flags |= IPC3_SEARCH_PROPERTY_REQUEST_FLAG_FORMAT;
							}
							break;
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
				ipc3_stream_pool_t memory_stream;
				ipc3_result_list_t result_list;
				int got_memory_stream;
				
				got_memory_stream = 0;
				
				// we are committed to ipc3
				// if we are exporting as EFU, make sure the attribute column is shown.
				// but don't allow reordering.
				if (_es_export_type == _ES_EXPORT_TYPE_EFU)
				{
					_es_add_standard_efu_columns(0,0,0,1,0);
				}

				// initialize the stream that we will use to read the reply from the pipe.
				// stream will be x86 by default.
				ipc3_stream_pipe_init(&pipe_stream,pipe_handle);
				
				// setup our initial result list from the stream.
				// don't read any items yet.
				ipc3_result_list_init(&result_list,(ipc3_stream_t *)&pipe_stream);
				
				if (_es_get_result_count)
				{
					_es_output_noncell_result_count(result_list.folder_result_count + result_list.file_result_count);
				}
				else
				if (_es_get_total_size)
				{
					_es_output_noncell_total_size(result_list.total_result_size);
				}
				else
				{
					if (_es_pause)
					{
						// setup a pool stream.
						// we read the entire stream into memory as it gets accessed.
						// we store the stream position for each item index so we can quickly jump to a location.
						ipc3_stream_pool_init(&memory_stream,(ipc3_stream_t *)&pipe_stream);
						
						got_memory_stream = 1;
						
						// set the pool stream as the main stream.
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
						if (_es_header > 0)
						{
							total_item_count = safe_size_add_one(total_item_count);
						}
						
						if (_es_footer > 0)
						{
							total_item_count = safe_size_add_one(total_item_count);
						}
						
						_es_output_ipc3_results(&result_list,0,total_item_count);
					}
				}

				if (result_list.stream->is_error)
				{
					_es_ret = ES_ERROR_IPC_ERROR;
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

	// folders first.
	if (a->flags & EVERYTHING_IPC_FOLDER)
	{
		if (b->flags & EVERYTHING_IPC_FOLDER)
		{
			// both folders.
			// fall through.
		}
		else
		{
			// A<B
			return -1;
		}
	}
	else
	{
		if (b->flags & EVERYTHING_IPC_FOLDER)
		{
			// A>B
			return 1;
		}
		else
		{
			// both files
			// fall through.
		}
	}
	
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
	
	_es_flush_export_buffer();

	if (_es_export_file != INVALID_HANDLE_VALUE)
	{
		CloseHandle(_es_export_file);
		
		_es_export_file = INVALID_HANDLE_VALUE;
	}
	
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
			msg = "Everything IPC not found. Please make sure Everything is running.\r\n";
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

static void DECLSPEC_NORETURN _es_bad_switch_param(const char *format,...)
{
	va_list argptr;
	utf8_buf_t cbuf;

	va_start(argptr,format);
	utf8_buf_init(&cbuf);

	utf8_buf_vprintf(&cbuf,format,argptr);

	os_error_printf("Error %d: %s",ES_ERROR_EXPECTED_SWITCH_PARAMETER,cbuf.buf);

	utf8_buf_kill(&cbuf);
	va_end(argptr);

	ExitProcess(ES_ERROR_EXPECTED_SWITCH_PARAMETER);
}

static void _es_console_fill(SIZE_T count,int ascii_ch)
{
	if (_es_output_cibuf)
	{
		SIZE_T i;
		
		for(i=0;i<count;i++)
		{
			if ((int)i + _es_output_cibuf_x >= _es_console_window_wide)
			{
				break;
			}

			if ((int)i + _es_output_cibuf_x >= 0)
			{
				_es_output_cibuf[(int)i+_es_output_cibuf_x].Attributes = _es_output_color;
				_es_output_cibuf[(int)i+_es_output_cibuf_x].Char.UnicodeChar = ascii_ch;
			}
		}
		
		_es_output_cibuf_x += (int)count;
	}
	else
	if (_es_output_is_char)
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
			WriteConsole(_es_output_handle,wcbuf.buf,(DWORD)count,&numwritten,0);
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
			WriteFile(_es_output_handle,cbuf.buf,(DWORD)count,&numwritten,0);
		}
		
		utf8_buf_kill(&cbuf);
	}
}

// get the length in wchars from a wchar string.
static SIZE_T _es_highlighted_wchar_string_get_length_in_wchars(const wchar_t *s)
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
		length_in_wchars = _es_highlighted_wchar_string_get_length_in_wchars(text);
	}
	else
	{
		length_in_wchars = wchar_string_get_length_in_wchars(text);
	}
	
	is_right_aligned = property_is_right_aligned(_es_output_column->property_id);
	column_width = column_width_get(_es_output_column->property_id);
	column_color = column_color_find(_es_output_column->property_id);
	_es_output_color = _es_default_attributes;
	did_set_color = 0;
	
	// setup colors
	// pipe? console? cibuf?
	if (_es_output_cibuf)
	{
		_es_output_color = column_color ? column_color->color : _es_default_attributes;
	}
	else
	if (_es_output_is_char)
	{
		if (column_color)
		{
			_es_output_color = column_color->color;
			SetConsoleTextAttribute(_es_output_handle,column_color->color);

			did_set_color = 1;
		}
	}
		
	// don't fill with CSV/TSV/EFU/TXT/M3U
	if ((_es_export_type == _ES_EXPORT_TYPE_NONE) || (_es_export_type == _ES_EXPORT_TYPE_NOFORMAT))
	{
		if (_es_output_column != column_order_last)
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
					
					if (!_es_digit_grouping)
					{
						if (!_es_is_in_header)
						{
							if (_es_output_column->property_id == EVERYTHING3_PROPERTY_ID_SIZE)
							{
								if (_es_size_leading_zero)
								{
									fill_ch = '0';
								}
							}
							else
							if (_es_output_column->property_id == EVERYTHING3_PROPERTY_ID_RUN_COUNT)
							{
								if (_es_run_count_leading_zero)
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
	if (_es_output_cibuf)
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

				if ((int)cibuf_offset + _es_output_cibuf_x >= _es_console_window_wide)
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
						if ((int)i + (int)cibuf_offset + _es_output_cibuf_x >= _es_console_window_wide)
						{
							break;
						}

						if ((int)i + (int)cibuf_offset + _es_output_cibuf_x >= 0)
						{
							_es_output_cibuf[(int)i+(int)cibuf_offset+_es_output_cibuf_x].Attributes = is_in_highlight ? _es_highlight_color : _es_output_color;
							_es_output_cibuf[(int)i+(int)cibuf_offset+_es_output_cibuf_x].Char.UnicodeChar = start[i];
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
				if ((int)i + _es_output_cibuf_x >= _es_console_window_wide)
				{
					break;
				}

				if ((int)i + _es_output_cibuf_x >= 0)
				{
					_es_output_cibuf[(int)i+_es_output_cibuf_x].Attributes = _es_output_color;
					_es_output_cibuf[(int)i+_es_output_cibuf_x].Char.UnicodeChar = text[i];
				}
			}
		}
		
		_es_output_cibuf_x += (int)length_in_wchars;
	}
	else
	if (_es_output_is_char)
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

					SetConsoleTextAttribute(_es_output_handle,is_in_highlight ? _es_highlight_color : _es_output_color);
					did_set_color = 1;
		
					if (wlen <= ES_DWORD_MAX)
					{
						DWORD numwritten;
						
						WriteConsole(_es_output_handle,start,(DWORD)wlen,&numwritten,0);
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
			
				WriteConsole(_es_output_handle,text,(DWORD)length_in_wchars,&numwritten,0);
			}
		}
	}
	else
	{
		if (length_in_wchars <= INT_MAX)
		{
			int len;
			
			len = WideCharToMultiByte(_es_cp,0,text,(int)length_in_wchars,0,0,0,0);
			if (len)
			{
				DWORD numwritten;
				utf8_buf_t cbuf;

				utf8_buf_init(&cbuf);

				utf8_buf_grow_size(&cbuf,len);

				WideCharToMultiByte(_es_cp,0,text,(int)length_in_wchars,cbuf.buf,len,0,0);
				
				WriteFile(_es_output_handle,cbuf.buf,len,&numwritten,0);
				
				utf8_buf_kill(&cbuf);
			}
		}	
	}

	// don't fill with CSV/TSV/EFU/TXT/M3U
	if ((_es_export_type == _ES_EXPORT_TYPE_NONE) || (_es_export_type == _ES_EXPORT_TYPE_NOFORMAT))
	{
		if (_es_output_column != column_order_last)
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
		SetConsoleTextAttribute(_es_output_handle,_es_default_attributes);
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

		if (_es_export_type == _ES_EXPORT_TYPE_M3U)		
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
		if (_es_export_avail != _ES_EXPORT_BUF_SIZE)
		{
			if (_es_export_buf)
			{
				DWORD numwritten;
				
				if (WriteFile(_es_export_file,_es_export_buf,_ES_EXPORT_BUF_SIZE - _es_export_avail,&numwritten,0))
				{
					if (_ES_EXPORT_BUF_SIZE - _es_export_avail == numwritten)
					{
						ret = TRUE;
					}
				}
				
				_es_export_p = _es_export_buf;
				_es_export_avail = _ES_EXPORT_BUF_SIZE;
			}
		}	
	}
	
	return ret;
}

static void _es_output_cell_text_property_wchar_string(const wchar_t *value)
{
	if ((_es_export_type == _ES_EXPORT_TYPE_CSV) || (_es_export_type == _ES_EXPORT_TYPE_TSV))
	{
		_es_output_cell_csv_wchar_string_with_optional_quotes((_es_export_type == _ES_EXPORT_TYPE_CSV) ? _es_csv_double_quote : _es_double_quote,(_es_export_type == _ES_EXPORT_TYPE_CSV) ? ',' : '\t',value,0);
	}
	else
	if (_es_export_type == _ES_EXPORT_TYPE_EFU)
	{
		// always double quote.
		_es_output_cell_csv_wchar_string(value,0);
	}
	else
	if (_es_export_type == _ES_EXPORT_TYPE_JSON)
	{
		// always double quote.
		wchar_buf_t property_name_wcbuf;
		wchar_buf_t json_string_wcbuf;
		
		wchar_buf_init(&property_name_wcbuf);
		wchar_buf_init(&json_string_wcbuf);

		_es_get_nice_json_property_name(_es_output_column->property_id,&property_name_wcbuf);
		_es_escape_json_wchar_string(value,&json_string_wcbuf);
		
		_es_output_cell_printf(0,"\"%S\":\"%S\"",property_name_wcbuf.buf,json_string_wcbuf.buf);

		wchar_buf_kill(&json_string_wcbuf);
		wchar_buf_kill(&property_name_wcbuf);
	}
	else
	{
		if (_es_double_quote)
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

static void _es_output_cell_text_property_utf8_string(const ES_UTF8 *value)
{
	wchar_buf_t wcbuf;

	wchar_buf_init(&wcbuf);

	wchar_buf_copy_utf8_string(&wcbuf,value);

	_es_output_cell_text_property_wchar_string(wcbuf.buf);

	wchar_buf_kill(&wcbuf);
}							

static void _es_output_cell_highlighted_text_property_wchar_string(const wchar_t *value)
{
	if ((_es_export_type == _ES_EXPORT_TYPE_CSV) || (_es_export_type == _ES_EXPORT_TYPE_TSV))
	{
		_es_output_cell_csv_wchar_string_with_optional_quotes((_es_export_type == _ES_EXPORT_TYPE_CSV) ? _es_csv_double_quote : _es_double_quote,(_es_export_type == _ES_EXPORT_TYPE_CSV) ? ',' : '\t',value,1);
	}
	else
	if (_es_export_type == _ES_EXPORT_TYPE_EFU)
	{
		// always double quote.
		_es_output_cell_csv_wchar_string(value,1);
	}
	else
	if (_es_export_type == _ES_EXPORT_TYPE_JSON)
	{
		// always double quote.
		wchar_buf_t property_name_wcbuf;
		wchar_buf_t json_string_wcbuf;
		
		wchar_buf_init(&property_name_wcbuf);
		wchar_buf_init(&json_string_wcbuf);

		_es_get_nice_json_property_name(_es_output_column->property_id,&property_name_wcbuf);
		_es_escape_json_wchar_string(value,&json_string_wcbuf);
		
		_es_output_cell_printf(1,"\"%S\":\"%S\"",property_name_wcbuf.buf,json_string_wcbuf.buf);

		wchar_buf_kill(&json_string_wcbuf);
		wchar_buf_kill(&property_name_wcbuf);
	}
	else
	{
		if (_es_double_quote)
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
	if (_es_export_type == _ES_EXPORT_TYPE_JSON)
	{
		wchar_buf_t property_name_wcbuf;
		
		wchar_buf_init(&property_name_wcbuf);

		_es_get_nice_json_property_name(_es_output_column->property_id,&property_name_wcbuf);
		
		_es_output_cell_printf(0,"\"%S\":null",property_name_wcbuf.buf);

		wchar_buf_kill(&property_name_wcbuf);
	}
	else
	{
		// this will fill in the column with spaces to the correct column width.
		_es_output_cell_wchar_string(L"",0);
	}
}

static void _es_output_cell_size_property(ES_UINT64 value)
{
	if (_es_export_type == _ES_EXPORT_TYPE_JSON)
	{
		wchar_buf_t property_name_wcbuf;
		
		wchar_buf_init(&property_name_wcbuf);

		_es_get_nice_json_property_name(_es_output_column->property_id,&property_name_wcbuf);
		
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
		// empty.
		// this will fill in the column with spaces to the correct column width.
		_es_output_cell_printf(0,"");
	}
	else
	{
		if (_es_export_type == _ES_EXPORT_TYPE_NONE)
		{
			wchar_buf_t wcbuf;

			wchar_buf_init(&wcbuf);

			_es_format_size(value,_es_size_format,&wcbuf);
			
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
	if (_es_export_type == _ES_EXPORT_TYPE_JSON)
	{
		wchar_buf_t property_name_wcbuf;
		wchar_buf_t filetime_wcbuf;
		
		wchar_buf_init(&property_name_wcbuf);
		wchar_buf_init(&filetime_wcbuf);

		_es_get_nice_json_property_name(_es_output_column->property_id,&property_name_wcbuf);
		
		if (value == ES_UINT64_MAX)
		{
			_es_output_cell_printf(0,"\"%S\":null",property_name_wcbuf.buf);
		}
		else
		if (_es_date_format)
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
	if ((_es_export_type == _ES_EXPORT_TYPE_NONE) || (_es_date_format))
	{
		wchar_buf_t wcbuf;

		// format filetime if we specify a _es_date_format
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

static void _es_output_cell_time_property(DWORD value)
{
	if (_es_export_type == _ES_EXPORT_TYPE_JSON)
	{
		wchar_buf_t property_name_wcbuf;
		wchar_buf_t filetime_wcbuf;
		
		wchar_buf_init(&property_name_wcbuf);
		wchar_buf_init(&filetime_wcbuf);

		_es_get_nice_json_property_name(_es_output_column->property_id,&property_name_wcbuf);
		
		if (value == ES_DWORD_MAX)
		{
			_es_output_cell_printf(0,"\"%S\":null",property_name_wcbuf.buf);
		}
		else
		{
			_es_output_cell_printf(0,"\"%S\":%u",property_name_wcbuf.buf,value);
		}

		wchar_buf_kill(&filetime_wcbuf);
		wchar_buf_kill(&property_name_wcbuf);
	}
	else
	if (value == ES_DWORD_MAX)
	{
		// unknown filetime.
		// this will fill in the column with spaces to the correct column width.
		_es_output_cell_printf(0,"");
	}
	else
	if (_es_export_type == _ES_EXPORT_TYPE_NONE)
	{
		DWORD seconds;
		
		seconds = value / 1000;
		
		// HH:MM:SS
		_es_output_cell_printf(0,"%02u:%02u:%02u",seconds / 3600,(seconds / 60) % 60,seconds % 60);
	}
	else
	{
		// raw filetime.
		_es_output_cell_printf(0,"%u",value);
	}
}

static void _es_output_cell_date_property(DWORD value)
{
	if (_es_export_type == _ES_EXPORT_TYPE_JSON)
	{
		wchar_buf_t property_name_wcbuf;
		wchar_buf_t filetime_wcbuf;
		
		wchar_buf_init(&property_name_wcbuf);
		wchar_buf_init(&filetime_wcbuf);

		_es_get_nice_json_property_name(_es_output_column->property_id,&property_name_wcbuf);
		
		if (value == ES_DWORD_MAX)
		{
			_es_output_cell_printf(0,"\"%S\":null",property_name_wcbuf.buf);
		}
		else
		{
			_es_output_cell_printf(0,"\"%S\":%u",property_name_wcbuf.buf,value);
		}

		wchar_buf_kill(&filetime_wcbuf);
		wchar_buf_kill(&property_name_wcbuf);
	}
	else
	if (value == ES_DWORD_MAX)
	{
		// unknown filetime.
		// this will fill in the column with spaces to the correct column width.
		_es_output_cell_printf(0,"");
	}
	else
	if (_es_export_type == _ES_EXPORT_TYPE_NONE)
	{
		DWORD day;
		SYSTEMTIME st;
		
		os_zero_memory(&st,sizeof(SYSTEMTIME));
		
		day = value;
		st.wYear = (WORD)(day / (32*13));
		day -= (DWORD)st.wYear * (32*13);
		st.wMonth = (WORD)(day / (32));
		day -= (DWORD)st.wMonth * (32);
		st.wDay = (WORD)day;
		
		// HH:MM:SS
		_es_output_cell_printf(0,"%04d-%02d-%02d",st.wYear,st.wMonth,st.wDay);
	}
	else
	{
		// raw filetime.
		_es_output_cell_printf(0,"%u",value);
	}
}

static void _es_output_cell_duration_property(ES_UINT64 value)
{
	if (_es_export_type == _ES_EXPORT_TYPE_JSON)
	{
		wchar_buf_t property_name_wcbuf;
		
		wchar_buf_init(&property_name_wcbuf);

		_es_get_nice_json_property_name(_es_output_column->property_id,&property_name_wcbuf);
		
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
	if (_es_export_type == _ES_EXPORT_TYPE_NONE)
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

static void _es_output_cell_attribute_property(DWORD file_attributes)
{
	if (_es_export_type == _ES_EXPORT_TYPE_JSON)
	{
		wchar_buf_t property_name_wcbuf;
		
		wchar_buf_init(&property_name_wcbuf);

		_es_get_nice_json_property_name(_es_output_column->property_id,&property_name_wcbuf);
		
		if (file_attributes == INVALID_FILE_ATTRIBUTES)
		{
			_es_output_cell_printf(0,"\"%S\":null",property_name_wcbuf.buf);
		}
		else
		{
			_es_output_cell_printf(0,"\"%S\":0x%08X",property_name_wcbuf.buf,file_attributes);
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
	if (_es_export_type == _ES_EXPORT_TYPE_NONE)
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

// the goal here is to avoid grouping with NOGROUPING formats.
// avoid grouping when the number of digits is normally <= 4.
// for example: width and height.
// we never want to show 1,080 we want to show 1080
static void _es_output_cell_number_property(ES_UINT64 value,ES_UINT64 empty_value,BOOL allow_digit_grouping)
{
	if (_es_export_type == _ES_EXPORT_TYPE_JSON)
	{
		wchar_buf_t property_name_wcbuf;
		
		wchar_buf_init(&property_name_wcbuf);

		_es_get_nice_json_property_name(_es_output_column->property_id,&property_name_wcbuf);
		
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
	if (_es_export_type == _ES_EXPORT_TYPE_NONE)
	{
		wchar_buf_t wcbuf;

		wchar_buf_init(&wcbuf);

		_es_format_number(value,allow_digit_grouping,&wcbuf);
		
		_es_output_cell_wchar_string(wcbuf.buf,0);
		
		wchar_buf_kill(&wcbuf);
	}
	else
	{
		_es_output_cell_printf(0,"%I64u",value);
	}
}

// text must not contain any special characters. [a-zA-Z0-9\-] ONLY.
static void _es_output_cell_formatted_number_property(const ES_UTF8 *text)
{
	if (_es_export_type == _ES_EXPORT_TYPE_JSON)
	{
		wchar_buf_t property_name_wcbuf;
		
		wchar_buf_init(&property_name_wcbuf);

		_es_get_nice_json_property_name(_es_output_column->property_id,&property_name_wcbuf);
		
		_es_output_cell_printf(0,"\"%S\":%s",text);

		wchar_buf_kill(&property_name_wcbuf);
	}
	else
	{
		_es_output_cell_utf8_string(text,0);
	}
}

static void _es_output_cell_hex_number8_property(DWORD value)
{
	if (_es_export_type == _ES_EXPORT_TYPE_JSON)
	{
		wchar_buf_t property_name_wcbuf;
		
		wchar_buf_init(&property_name_wcbuf);

		_es_get_nice_json_property_name(_es_output_column->property_id,&property_name_wcbuf);
		
		if (value == ES_DWORD_MAX)
		{
			_es_output_cell_printf(0,"\"%S\":null",property_name_wcbuf.buf);
		}
		else
		{
			_es_output_cell_printf(0,"\"%S\":0x%08X",property_name_wcbuf.buf,value);
		}

		wchar_buf_kill(&property_name_wcbuf);
	}
	else
	if (value == ES_DWORD_MAX)
	{
		// empty.
		// this will fill in the column with spaces to the correct column width.
		_es_output_cell_printf(0,"");
	}
	else
	if (_es_export_type == _ES_EXPORT_TYPE_NONE)
	{
		wchar_buf_t wcbuf;

		wchar_buf_init(&wcbuf);
		
		wchar_buf_printf(&wcbuf,"0x%08X",value);
		
		_es_output_cell_wchar_string(wcbuf.buf,0);
		
		wchar_buf_kill(&wcbuf);
	}
	else
	{
		_es_output_cell_printf(0,"%u",value);
	}
}

static void _es_output_cell_hex_number16_property(ES_UINT64 value,ES_UINT64 empty_value)
{
	if (_es_export_type == _ES_EXPORT_TYPE_JSON)
	{
		wchar_buf_t property_name_wcbuf;
		
		wchar_buf_init(&property_name_wcbuf);

		_es_get_nice_json_property_name(_es_output_column->property_id,&property_name_wcbuf);
		
		if (value == empty_value)
		{
			_es_output_cell_printf(0,"\"%S\":null",property_name_wcbuf.buf);
		}
		else
		{
			_es_output_cell_printf(0,"\"%S\":0x%016I64X",property_name_wcbuf.buf,value);
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
	if (_es_export_type == _ES_EXPORT_TYPE_NONE)
	{
		wchar_buf_t wcbuf;

		wchar_buf_init(&wcbuf);
		
		wchar_buf_printf(&wcbuf,"0x%016I64X",value);
		
		_es_output_cell_wchar_string(wcbuf.buf,0);
		
		wchar_buf_kill(&wcbuf);
	}
	else
	{
		_es_output_cell_printf(0,"%I64u",value);
	}
}

static void _es_output_cell_hex_number32_property(EVERYTHING3_UINT128 *uint128_value)
{
	if (_es_export_type == _ES_EXPORT_TYPE_JSON)
	{
		wchar_buf_t property_name_wcbuf;
		
		wchar_buf_init(&property_name_wcbuf);

		_es_get_nice_json_property_name(_es_output_column->property_id,&property_name_wcbuf);
		
		if ((uint128_value->hi_uint64 == ES_UINT64_MAX) && (uint128_value->lo_uint64 == ES_UINT64_MAX))
		{
			_es_output_cell_printf(0,"\"%S\":null",property_name_wcbuf.buf);
		}
		else
		{
			if (uint128_value->hi_uint64)
			{
				_es_output_cell_printf(0,"\"%S\":0x%016I64X%016I64X",property_name_wcbuf.buf,uint128_value->hi_uint64,uint128_value->lo_uint64);
			}
			else
			{
				_es_output_cell_printf(0,"\"%S\":0x%016I64X",property_name_wcbuf.buf,uint128_value->lo_uint64);
			}
		}

		wchar_buf_kill(&property_name_wcbuf);
	}
	else
	if ((uint128_value->hi_uint64 == ES_UINT64_MAX) && (uint128_value->lo_uint64 == ES_UINT64_MAX))
	{
		// empty.
		// this will fill in the column with spaces to the correct column width.
		_es_output_cell_printf(0,"");
	}
	else
	{
		// just format as hex..
		if (uint128_value->hi_uint64)
		{
			_es_output_cell_printf(0,"0x%016I64X%016I64X",uint128_value->hi_uint64,uint128_value->lo_uint64);
		}
		else
		{
			_es_output_cell_printf(0,"0x%016I64X",uint128_value->lo_uint64);
		}
	}
}

static void _es_output_cell_kbps_property(ES_UINT64 value,ES_UINT64 empty_value)
{
	if (_es_export_type == _ES_EXPORT_TYPE_JSON)
	{
		wchar_buf_t property_name_wcbuf;
		
		wchar_buf_init(&property_name_wcbuf);

		_es_get_nice_json_property_name(_es_output_column->property_id,&property_name_wcbuf);
		
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
	if (_es_export_type == _ES_EXPORT_TYPE_NONE)
	{
		wchar_buf_t wcbuf;

		wchar_buf_init(&wcbuf);

		wchar_buf_printf(&wcbuf,"%I64u kbps",value / 1000);
		
		_es_output_cell_wchar_string(wcbuf.buf,0);
		
		wchar_buf_kill(&wcbuf);
	}
	else
	{
		_es_output_cell_printf(0,"%I64u",value);
	}
}

static void _es_output_cell_khz_property(ES_UINT64 value,ES_UINT64 empty_value)
{
	if (_es_export_type == _ES_EXPORT_TYPE_JSON)
	{
		wchar_buf_t property_name_wcbuf;
		
		wchar_buf_init(&property_name_wcbuf);

		_es_get_nice_json_property_name(_es_output_column->property_id,&property_name_wcbuf);
		
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
	if (_es_export_type == _ES_EXPORT_TYPE_NONE)
	{
		wchar_buf_t wcbuf;

		wchar_buf_init(&wcbuf);

		_es_format_fixed_q1k(value,1,0,0,1,&wcbuf);
		
		wchar_buf_cat_utf8_string(&wcbuf," kHz");
		
		_es_output_cell_wchar_string(wcbuf.buf,0);
		
		wchar_buf_kill(&wcbuf);
	}
	else
	{
		_es_output_cell_printf(0,"%I64u",value);
	}
}

static void _es_output_cell_percent_property(BYTE value)
{
	if (_es_export_type == _ES_EXPORT_TYPE_JSON)
	{
		wchar_buf_t property_name_wcbuf;
		
		wchar_buf_init(&property_name_wcbuf);

		_es_get_nice_json_property_name(_es_output_column->property_id,&property_name_wcbuf);
		
		if (value == ES_BYTE_MAX)
		{
			_es_output_cell_printf(0,"\"%S\":null",property_name_wcbuf.buf);
		}
		else
		{
			_es_output_cell_printf(0,"\"%S\":%u",value);
		}

		wchar_buf_kill(&property_name_wcbuf);
	}
	else
	if (value == ES_BYTE_MAX)
	{
		// empty.
		// this will fill in the column with spaces to the correct column width.
		_es_output_cell_printf(0,"");
	}
	else
	if (_es_export_type == _ES_EXPORT_TYPE_NONE)
	{
		wchar_buf_t wcbuf;

		wchar_buf_init(&wcbuf);

		wchar_buf_printf(&wcbuf,"%u%%",value);
		
		_es_output_cell_wchar_string(wcbuf.buf,0);
		
		wchar_buf_kill(&wcbuf);
	}
	else
	{
		_es_output_cell_printf(0,"%u",value);
	}
}

static void _es_output_cell_rating_property(BYTE value)
{
	if (_es_export_type == _ES_EXPORT_TYPE_JSON)
	{
		wchar_buf_t property_name_wcbuf;
		
		wchar_buf_init(&property_name_wcbuf);

		_es_get_nice_json_property_name(_es_output_column->property_id,&property_name_wcbuf);
		
		if (value == ES_BYTE_MAX)
		{
			_es_output_cell_printf(0,"\"%S\":null",property_name_wcbuf.buf);
		}
		else
		{
			_es_output_cell_printf(0,"\"%S\":%u",property_name_wcbuf.buf,(DWORD)value);
		}

		wchar_buf_kill(&property_name_wcbuf);
	}
	else
	if (value == ES_BYTE_MAX)
	{
		// empty.
		// this will fill in the column with spaces to the correct column width.
		_es_output_cell_printf(0,"");
	}
	else
	if (_es_export_type == _ES_EXPORT_TYPE_NONE)
	{
		wchar_buf_t wcbuf;

		wchar_buf_init(&wcbuf);

		if (value >= 1)
		{
			wchar_buf_cat_wchar(&wcbuf,'*');

			if (value >= 13)
			{
				wchar_buf_cat_wchar(&wcbuf,'*');

				if (value >= 38)
				{
					wchar_buf_cat_wchar(&wcbuf,'*');

					if (value >= 63)
					{
						wchar_buf_cat_wchar(&wcbuf,'*');

						if (value >= 88)
						{
							wchar_buf_cat_wchar(&wcbuf,'*');
						}
					}
				}
			}
		}
		
		_es_output_cell_wchar_string(wcbuf.buf,0);
		
		wchar_buf_kill(&wcbuf);
	}
	else
	{
		_es_output_cell_printf(0,"%u",(DWORD)value);
	}
}

static void _es_output_cell_yesno_property(BYTE value)
{
	if (_es_export_type == _ES_EXPORT_TYPE_JSON)
	{
		wchar_buf_t property_name_wcbuf;
		
		wchar_buf_init(&property_name_wcbuf);

		_es_get_nice_json_property_name(_es_output_column->property_id,&property_name_wcbuf);
		
		if (value == ES_BYTE_MAX)
		{
			_es_output_cell_printf(0,"\"%S\":null",property_name_wcbuf.buf);
		}
		else
		{
			_es_output_cell_printf(0,"\"%S\":%u",property_name_wcbuf.buf,(DWORD)value);
		}

		wchar_buf_kill(&property_name_wcbuf);
	}
	else
	if (value == ES_BYTE_MAX)
	{
		// empty.
		// this will fill in the column with spaces to the correct column width.
		_es_output_cell_printf(0,"");
	}
	else
	if (_es_export_type == _ES_EXPORT_TYPE_NONE)
	{
		_es_output_cell_printf(0,value ? "Yes" : "No");
	}
	else
	{
		_es_output_cell_printf(0,"%u",(DWORD)value);
	}
}

static void _es_output_cell_fixed_q1k_property(ES_UINT64 value,ES_UINT64 empty_value,int is_signed)
{
	if (_es_export_type == _ES_EXPORT_TYPE_JSON)
	{
		wchar_buf_t property_name_wcbuf;
		wchar_buf_t fixed_wcbuf;
		
		wchar_buf_init(&property_name_wcbuf);
		wchar_buf_init(&fixed_wcbuf);

		_es_get_nice_json_property_name(_es_output_column->property_id,&property_name_wcbuf);
		
		if (value == empty_value)
		{
			_es_output_cell_printf(0,"\"%S\":null",property_name_wcbuf.buf);
		}
		else
		{
			_es_format_fixed_q1k(value,0,0,is_signed,0,&fixed_wcbuf);
			
			_es_output_cell_printf(0,"\"%S\":%S",property_name_wcbuf.buf,fixed_wcbuf.buf);
		}

		wchar_buf_kill(&fixed_wcbuf);
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
	{
		wchar_buf_t wcbuf;

		wchar_buf_init(&wcbuf);

		_es_format_fixed_q1k(value,(_es_export_type == _ES_EXPORT_TYPE_NONE) ? 1 : 0,1,is_signed,0,&wcbuf);
		
		_es_output_cell_wchar_string(wcbuf.buf,0);
		
		wchar_buf_kill(&wcbuf);
	}
}

static void _es_output_cell_fixed_q1m_property(ES_UINT64 value,ES_UINT64 empty_value,int is_signed)
{
	if (_es_export_type == _ES_EXPORT_TYPE_JSON)
	{
		wchar_buf_t property_name_wcbuf;
		wchar_buf_t fixed_wcbuf;
		
		wchar_buf_init(&property_name_wcbuf);
		wchar_buf_init(&fixed_wcbuf);

		_es_get_nice_json_property_name(_es_output_column->property_id,&property_name_wcbuf);
		
		if (value == empty_value)
		{
			_es_output_cell_printf(0,"\"%S\":null",property_name_wcbuf.buf);
		}
		else
		{
			_es_format_fixed_q1m(value,0,0,is_signed,&fixed_wcbuf);
			
			_es_output_cell_printf(0,"\"%S\":%S",property_name_wcbuf.buf,fixed_wcbuf.buf);
		}

		wchar_buf_kill(&fixed_wcbuf);
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
	{
		wchar_buf_t wcbuf;

		wchar_buf_init(&wcbuf);

		_es_format_fixed_q1m(value,(_es_export_type == _ES_EXPORT_TYPE_NONE) ? 1 : 0,1,is_signed,&wcbuf);
		
		_es_output_cell_wchar_string(wcbuf.buf,0);
		
		wchar_buf_kill(&wcbuf);
	}
}

static void _es_output_cell_f_stop_property(__int32 value)
{
	if (_es_export_type == _ES_EXPORT_TYPE_JSON)
	{
		wchar_buf_t property_name_wcbuf;
		wchar_buf_t fixed_wcbuf;
		
		wchar_buf_init(&property_name_wcbuf);
		wchar_buf_init(&fixed_wcbuf);

		_es_get_nice_json_property_name(_es_output_column->property_id,&property_name_wcbuf);
		
		if (value == INT_MIN)
		{
			_es_output_cell_printf(0,"\"%S\":null",property_name_wcbuf.buf);
		}
		else
		{
			_es_format_fixed_q1k(value,0,0,1,0,&fixed_wcbuf);
			_es_output_cell_printf(0,"\"%S\":%S",property_name_wcbuf.buf,fixed_wcbuf.buf);
		}

		wchar_buf_kill(&fixed_wcbuf);
		wchar_buf_kill(&property_name_wcbuf);
	}
	else
	if (value == INT_MIN)
	{
		// empty.
		// this will fill in the column with spaces to the correct column width.
		_es_output_cell_printf(0,"");
	}
	else
	{
		wchar_buf_t wcbuf;
		wchar_buf_t fixed_wcbuf;

		wchar_buf_init(&wcbuf);
		wchar_buf_init(&fixed_wcbuf);

		_es_format_fixed_q1k(value,(_es_export_type == _ES_EXPORT_TYPE_NONE) ? 1 : 0,1,1,1,&fixed_wcbuf);
		
		if (_es_export_type == _ES_EXPORT_TYPE_NONE)
		{
			wchar_buf_printf(&wcbuf,"f/%S",fixed_wcbuf.buf);
		
			_es_output_cell_wchar_string(wcbuf.buf,0);
		}
		else
		{
			_es_output_cell_wchar_string(fixed_wcbuf.buf,0);
		}
		
		wchar_buf_kill(&fixed_wcbuf);
		wchar_buf_kill(&wcbuf);
	}
}

static void _es_output_cell_exposure_time_property(__int32 value)
{
	if (_es_export_type == _ES_EXPORT_TYPE_JSON)
	{
		wchar_buf_t property_name_wcbuf;
		wchar_buf_t fixed_wcbuf;
		
		wchar_buf_init(&property_name_wcbuf);
		wchar_buf_init(&fixed_wcbuf);

		_es_get_nice_json_property_name(_es_output_column->property_id,&property_name_wcbuf);
		
		if (value == INT_MIN)
		{
			_es_output_cell_printf(0,"\"%S\":null",property_name_wcbuf.buf);
		}
		else
		{
			_es_output_cell_printf(0,"\"%S\":%d",property_name_wcbuf.buf,value);
		}

		wchar_buf_kill(&fixed_wcbuf);
		wchar_buf_kill(&property_name_wcbuf);
	}
	else
	if (value == INT_MIN)
	{
		// empty.
		// this will fill in the column with spaces to the correct column width.
		_es_output_cell_printf(0,"");
	}
	else
	{
		wchar_buf_t wcbuf;

		wchar_buf_init(&wcbuf);

		if (_es_export_type == _ES_EXPORT_TYPE_NONE)
		{
			if (value >= 0)
			{
				_es_format_fixed_q1k(value,1,0,1,0,&wcbuf);
				wchar_buf_cat_printf(&wcbuf," sec");
			}
			else
			{
				wchar_buf_printf(&wcbuf,"1/%u sec",(DWORD)-value);
			}
		}
		else
		{
			wchar_buf_printf(&wcbuf,"%d",value);
		}
		
		_es_output_cell_wchar_string(wcbuf.buf,0);
		
		wchar_buf_kill(&wcbuf);
	}
}

static void _es_output_cell_iso_speed_property(WORD value)
{
	if (_es_export_type == _ES_EXPORT_TYPE_JSON)
	{
		wchar_buf_t property_name_wcbuf;
		wchar_buf_t fixed_wcbuf;
		
		wchar_buf_init(&property_name_wcbuf);
		wchar_buf_init(&fixed_wcbuf);

		_es_get_nice_json_property_name(_es_output_column->property_id,&property_name_wcbuf);
		
		if (value == ES_WORD_MAX)
		{
			_es_output_cell_printf(0,"\"%S\":null",property_name_wcbuf.buf);
		}
		else
		{
			_es_output_cell_printf(0,"\"%S\":%u",property_name_wcbuf.buf,(DWORD)value);
		}

		wchar_buf_kill(&fixed_wcbuf);
		wchar_buf_kill(&property_name_wcbuf);
	}
	else
	if (value == ES_WORD_MAX)
	{
		// empty.
		// this will fill in the column with spaces to the correct column width.
		_es_output_cell_printf(0,"");
	}
	else
	{
		wchar_buf_t wcbuf;

		wchar_buf_init(&wcbuf);

		if (_es_export_type == _ES_EXPORT_TYPE_NONE)
		{
			wchar_buf_cat_printf(&wcbuf,"ISO-%u",(DWORD)value);
		}
		else
		{
			wchar_buf_cat_printf(&wcbuf,"%u",(DWORD)value);
		}
		
		_es_output_cell_wchar_string(wcbuf.buf,0);
		
		wchar_buf_kill(&wcbuf);
	}
}

static void _es_output_cell_exposure_bias_property(__int32 value)
{
	if (_es_export_type == _ES_EXPORT_TYPE_JSON)
	{
		wchar_buf_t property_name_wcbuf;
		wchar_buf_t fixed_wcbuf;
		
		wchar_buf_init(&property_name_wcbuf);
		wchar_buf_init(&fixed_wcbuf);

		_es_get_nice_json_property_name(_es_output_column->property_id,&property_name_wcbuf);
		
		if (value == INT_MIN)
		{
			_es_output_cell_printf(0,"\"%S\":null",property_name_wcbuf.buf);
		}
		else
		{
			_es_format_fixed_q1k(value,0,0,1,0,&fixed_wcbuf);
			_es_output_cell_printf(0,"\"%S\":%S",property_name_wcbuf.buf,fixed_wcbuf.buf);
		}

		wchar_buf_kill(&fixed_wcbuf);
		wchar_buf_kill(&property_name_wcbuf);
	}
	else
	if (value == INT_MIN)
	{
		// empty.
		// this will fill in the column with spaces to the correct column width.
		_es_output_cell_printf(0,"");
	}
	else
	{
		wchar_buf_t wcbuf;
		wchar_buf_t fixed_wcbuf;

		wchar_buf_init(&wcbuf);
		wchar_buf_init(&fixed_wcbuf);

		_es_format_fixed_q1k(value,(_es_export_type == _ES_EXPORT_TYPE_NONE) ? 1 : 0,0,1,1,&fixed_wcbuf);
		
		if (_es_export_type == _ES_EXPORT_TYPE_NONE)
		{
			if (value > 0)
			{
				wchar_buf_cat_printf(&wcbuf,"+%S step",fixed_wcbuf.buf);
			}
			else
			{
				wchar_buf_printf(&wcbuf,"%S step",fixed_wcbuf.buf);
			}
			
			_es_output_cell_wchar_string(wcbuf.buf,0);
		}
		else
		{
			_es_output_cell_wchar_string(fixed_wcbuf.buf,0);
		}
		
		wchar_buf_kill(&fixed_wcbuf);
		wchar_buf_kill(&wcbuf);
	}
}

static void _es_output_cell_bcps_property(__int32 value)
{
	if (_es_export_type == _ES_EXPORT_TYPE_JSON)
	{
		wchar_buf_t property_name_wcbuf;
		wchar_buf_t fixed_wcbuf;
		
		wchar_buf_init(&property_name_wcbuf);
		wchar_buf_init(&fixed_wcbuf);

		_es_get_nice_json_property_name(_es_output_column->property_id,&property_name_wcbuf);
		
		if (value == INT_MIN)
		{
			_es_output_cell_printf(0,"\"%S\":null",property_name_wcbuf.buf);
		}
		else
		{
			_es_format_fixed_q1k(value,0,0,1,0,&fixed_wcbuf);
			_es_output_cell_printf(0,"\"%S\":%S",property_name_wcbuf.buf,fixed_wcbuf.buf);
		}

		wchar_buf_kill(&fixed_wcbuf);
		wchar_buf_kill(&property_name_wcbuf);
	}
	else
	if (value == INT_MIN)
	{
		// empty.
		// this will fill in the column with spaces to the correct column width.
		_es_output_cell_printf(0,"");
	}
	else
	{
		wchar_buf_t wcbuf;
		wchar_buf_t fixed_wcbuf;

		wchar_buf_init(&wcbuf);
		wchar_buf_init(&fixed_wcbuf);

		_es_format_fixed_q1k(value,(_es_export_type == _ES_EXPORT_TYPE_NONE) ? 1 : 0,1,1,1,&fixed_wcbuf);

		if (_es_export_type == _ES_EXPORT_TYPE_NONE)
		{
			wchar_buf_printf(&wcbuf,"%S bcps",fixed_wcbuf.buf);
			
			_es_output_cell_wchar_string(wcbuf.buf,0);
		}
		else
		{
			_es_output_cell_wchar_string(fixed_wcbuf.buf,0);
		}
		
		wchar_buf_kill(&fixed_wcbuf);
		wchar_buf_kill(&wcbuf);
	}
}

static void _es_output_cell_small_number_property_with_suffix(ES_UINT64 value,ES_UINT64 empty_value,const ES_UTF8 *suffix)
{
	if (_es_export_type == _ES_EXPORT_TYPE_JSON)
	{
		wchar_buf_t property_name_wcbuf;
		
		wchar_buf_init(&property_name_wcbuf);

		_es_get_nice_json_property_name(_es_output_column->property_id,&property_name_wcbuf);
		
		if (value == ES_WORD_MAX)
		{
			_es_output_cell_printf(0,"\"%S\":null",property_name_wcbuf.buf);
		}
		else
		{
			_es_output_cell_printf(0,"\"%S\":%u",value);
		}

		wchar_buf_kill(&property_name_wcbuf);
	}
	else
	if (value == ES_WORD_MAX)
	{
		// empty.
		// this will fill in the column with spaces to the correct column width.
		_es_output_cell_printf(0,"");
	}
	else
	{
		wchar_buf_t wcbuf;

		wchar_buf_init(&wcbuf);

		wchar_buf_printf(&wcbuf,"%u",value);
		
		if (_es_export_type == _ES_EXPORT_TYPE_NONE)
		{
			wchar_buf_cat_printf(&wcbuf," %s",suffix);
		}
		
		_es_output_cell_wchar_string(wcbuf.buf,0);
		
		wchar_buf_kill(&wcbuf);
	}
}

static void _es_output_cell_fixed_q1k_property_with_suffix(__int32 value,const ES_UTF8 *suffix)
{
	if (_es_export_type == _ES_EXPORT_TYPE_JSON)
	{
		wchar_buf_t property_name_wcbuf;
		wchar_buf_t fixed_wcbuf;
		
		wchar_buf_init(&property_name_wcbuf);
		wchar_buf_init(&fixed_wcbuf);

		_es_get_nice_json_property_name(_es_output_column->property_id,&property_name_wcbuf);
		
		if (value == INT_MIN)
		{
			_es_output_cell_printf(0,"\"%S\":null",property_name_wcbuf.buf);
		}
		else
		{
			_es_format_fixed_q1k(value,0,0,1,0,&fixed_wcbuf);
			_es_output_cell_printf(0,"\"%S\":%S",property_name_wcbuf.buf,fixed_wcbuf.buf);
		}

		wchar_buf_kill(&fixed_wcbuf);
		wchar_buf_kill(&property_name_wcbuf);
	}
	else
	if (value == INT_MIN)
	{
		// empty.
		// this will fill in the column with spaces to the correct column width.
		_es_output_cell_printf(0,"");
	}
	else
	{
		wchar_buf_t wcbuf;
		wchar_buf_t fixed_wcbuf;

		wchar_buf_init(&wcbuf);
		wchar_buf_init(&fixed_wcbuf);

		_es_format_fixed_q1k(value,(_es_export_type == _ES_EXPORT_TYPE_NONE) ? 1 : 0,1,1,1,&fixed_wcbuf);
		
		if (_es_export_type == _ES_EXPORT_TYPE_NONE)
		{
			wchar_buf_printf(&wcbuf,"%S %s",fixed_wcbuf.buf,suffix);

			_es_output_cell_wchar_string(wcbuf.buf,0);
		}
		else
		{
			_es_output_cell_wchar_string(fixed_wcbuf.buf,0);
		}
		
		wchar_buf_kill(&fixed_wcbuf);
		wchar_buf_kill(&wcbuf);
	}
}


static void _es_output_cell_dimensions_property(EVERYTHING3_DIMENSIONS *dimensions_value)
{
	wchar_buf_t wcbuf;

	wchar_buf_init(&wcbuf);

	_es_format_dimensions(dimensions_value,&wcbuf);
	
	if (_es_export_type == _ES_EXPORT_TYPE_JSON)
	{
		wchar_buf_t property_name_wcbuf;
		
		wchar_buf_init(&property_name_wcbuf);

		_es_get_nice_json_property_name(_es_output_column->property_id,&property_name_wcbuf);
		
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

	if (_es_export_type == _ES_EXPORT_TYPE_JSON)
	{
		wchar_buf_t property_name_wcbuf;
		
		wchar_buf_init(&property_name_wcbuf);

		_es_get_nice_json_property_name(_es_output_column->property_id,&property_name_wcbuf);
		
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
	if (_es_output_column != column_order_start)
	{
		const ES_UTF8 *separator_text;
	
		if ((_es_export_type == _ES_EXPORT_TYPE_CSV) || (_es_export_type == _ES_EXPORT_TYPE_TSV))
		{
			separator_text = (_es_export_type == _ES_EXPORT_TYPE_CSV) ? "," : "\t";
		}
		else
		if (_es_export_type == _ES_EXPORT_TYPE_EFU)
		{
			separator_text = ",";
		}
		else
		if (_es_export_type == _ES_EXPORT_TYPE_JSON)
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
		if (_es_output_cibuf)
		{
			SIZE_T i;
			SIZE_T ci_x;
			
			ci_x = 0;
		
			for(i=0;i<length_in_wchars;i++)
			{
				if ((int)ci_x + _es_output_cibuf_x >= _es_console_window_wide)
				{
					break;
				}
				
				if (text[i] == '\t')
				{
					if ((int)ci_x + _es_output_cibuf_x >= 0)
					{
						_es_output_cibuf[(int)ci_x+_es_output_cibuf_x].Attributes = _es_default_attributes;
						_es_output_cibuf[(int)ci_x+_es_output_cibuf_x].Char.UnicodeChar = ' ';
					}

					ci_x++;
					
					while (((int)ci_x + _es_output_cibuf_x + _es_output_cibuf_hscroll) & 7)
					{
						if ((int)ci_x + _es_output_cibuf_x < _es_console_window_wide)
						{
							if ((int)ci_x + _es_output_cibuf_x >= 0)
							{
								_es_output_cibuf[(int)ci_x+_es_output_cibuf_x].Attributes = _es_default_attributes;
								//_es_output_cibuf[(int)ci_x+_es_output_cibuf_x].Char.UnicodeChar = '0'+(((int)ci_x + _es_output_cibuf_x + _es_output_cibuf_hscroll) % 8);
								_es_output_cibuf[(int)ci_x+_es_output_cibuf_x].Char.UnicodeChar = ' ';
							}
						}
						
						ci_x++;
					}
				}
				else
				{
					if ((int)ci_x + _es_output_cibuf_x >= 0)
					{
						_es_output_cibuf[(int)ci_x+_es_output_cibuf_x].Attributes = _es_default_attributes;
						_es_output_cibuf[(int)ci_x+_es_output_cibuf_x].Char.UnicodeChar = text[i];
					}

					ci_x++;
				}
			}
			
			_es_output_cibuf_x += (int)ci_x;	
		}
		else
		{
			if (_es_output_is_char)
			{
				if (length_in_wchars <= ES_DWORD_MAX)
				{
					DWORD numwritten;
				
					WriteConsole(_es_output_handle,text,(DWORD)length_in_wchars,&numwritten,0);
				}
			}
			else
			{
				if (length_in_wchars <= INT_MAX)
				{
					int len;
					
					len = WideCharToMultiByte(_es_cp,0,text,(int)length_in_wchars,0,0,0,0);
					if (len)
					{
						DWORD numwritten;
						utf8_buf_t cbuf;

						utf8_buf_init(&cbuf);

						utf8_buf_grow_size(&cbuf,len);

						WideCharToMultiByte(_es_cp,0,text,(int)length_in_wchars,cbuf.buf,len,0,0);
						
						WriteFile(_es_output_handle,cbuf.buf,len,&numwritten,0);
						
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

static void _es_output_header(void)
{
	wchar_buf_t property_name_wcbuf;

	wchar_buf_init(&property_name_wcbuf);
	
	_es_is_in_header = 1;
	
	_es_output_line_begin(0);
	
	_es_output_column = column_order_start;
	
	while(_es_output_column)
	{
		_es_output_cell_separator();

		_es_get_nice_property_name(_es_output_column->property_id,&property_name_wcbuf);

		// no quotes
		// never highlight.
		_es_output_cell_wchar_string(property_name_wcbuf.buf,0);

		_es_output_column = _es_output_column->order_next;
	}

	_es_output_line_end(0);

	_es_is_in_header = 0;
	
	wchar_buf_kill(&property_name_wcbuf);
}

static void _es_output_page_begin(void)
{
	_es_output_cibuf_y = 0;	
}

static void _es_output_page_end(void)
{
}

static void _es_output_line_begin(int is_first)
{
	_es_output_cibuf_x = -_es_output_cibuf_hscroll;

	_es_output_column = column_order_start;
	
	if (_es_export_type == _ES_EXPORT_TYPE_JSON)
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
	if (_es_export_type == _ES_EXPORT_TYPE_JSON)
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
	
	if (_es_output_cibuf)
	{
		int wlen;

		// fill right side of screen
		
		if (_es_output_cibuf_x)
		{
			if (_es_output_cibuf_x + _es_output_cibuf_hscroll > _es_max_wide)
			{
				_es_max_wide = _es_output_cibuf_x + _es_output_cibuf_hscroll;
			}
		}

		wlen = _es_console_window_wide - _es_output_cibuf_x;
		
		if (wlen > 0)
		{
			int bufi;
			
			for(bufi=0;bufi<wlen;bufi++)
			{
				if (bufi + _es_output_cibuf_x >= _es_console_window_wide)
				{
					break;
				}

				if (bufi + _es_output_cibuf_x >= 0)
				{
					_es_output_cibuf[bufi+_es_output_cibuf_x].Attributes = _es_default_attributes;
					_es_output_cibuf[bufi+_es_output_cibuf_x].Char.UnicodeChar = ' ';
				}
			}
		}
		
		// draw buffer line.
		{
			COORD buf_size;
			COORD buf_pos;
			SMALL_RECT write_rect;
			
			buf_size.X = _es_console_window_wide;
			buf_size.Y = 1;
			
			buf_pos.X = 0;
			buf_pos.Y = 0;
			
			write_rect.Left = _es_console_window_x;
			write_rect.Top = _es_console_window_y + _es_output_cibuf_y;
			write_rect.Right = _es_console_window_x + _es_console_window_wide;
			write_rect.Bottom = _es_console_window_y + _es_output_cibuf_y + 1;
			
			WriteConsoleOutput(_es_output_handle,_es_output_cibuf,buf_size,buf_pos,&write_rect);
			
			_es_output_cibuf_x += wlen;				
		}
	
		// output nothing.
		_es_output_cibuf_y++;
	}
	else
	{
		switch(_es_newline_type)
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
	SIZE_T result_run;

	wchar_buf_init(&filename_wcbuf);

	everything_ipc_item = list->items + index_start;
	run = count;

	_es_output_page_begin();

	if (run)
	{
		// output header.
		if (_es_header > 0)
		{
			_es_output_header();
	
			// don't inc i.
			run--;
		}
	}
	
	result_run = run;
	
	if (_es_footer > 0)
	{
		if (result_run)
		{
			result_run--;
		}
	}
	
	if (result_run > list->numitems - index_start)
	{
		result_run = list->numitems - index_start;
	}
	
	run -= result_run;
	
	while(result_run)
	{
		_es_output_line_begin(everything_ipc_item == list->items);
		
		while(_es_output_column)
		{
			_es_output_cell_separator();
			
			switch(_es_output_column->property_id)
			{
				case EVERYTHING3_PROPERTY_ID_ATTRIBUTES:
					_es_output_cell_attribute_property((everything_ipc_item->flags & EVERYTHING_IPC_FOLDER) ? FILE_ATTRIBUTE_DIRECTORY : 0);
					break;
					
				case EVERYTHING3_PROPERTY_ID_PATH_AND_NAME:
					wchar_buf_path_cat_filename(EVERYTHING_IPC_ITEMPATH(list,everything_ipc_item),EVERYTHING_IPC_ITEMFILENAME(list,everything_ipc_item),&filename_wcbuf);

					if ((_es_folder_append_path_separator) && (everything_ipc_item->flags & EVERYTHING_IPC_FOLDER))
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
			
			_es_output_column = _es_output_column->order_next;
		}									
		
		everything_ipc_item++;
		result_run--;	

		_es_output_line_end(result_run ? 1 : 0);
	}

	if ((run) && (_es_footer > 0))
	{
		_es_output_footer(list->numitems,ES_UINT64_MAX);

		run--;
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
	SIZE_T result_run;

	wchar_buf_init(&column_wcbuf);			
	items = (EVERYTHING_IPC_ITEM2 *)(list + 1);

	i = index_start;
	run = count;

	_es_output_page_begin();
	
	if (run)
	{
		// output header.
		if (_es_header > 0)
		{
			_es_output_header();

			// don't inc i.
			run--;
		}
	}
	
	result_run = run;

	if (_es_footer > 0)
	{
		if (result_run)
		{
			result_run--;
		}
	}
		
	if (result_run > list->numitems - i)
	{
		result_run = list->numitems - i;
	}
	
	run -= result_run;
	
	while(result_run)
	{
		_es_output_line_begin(i == 0);
			
		while(_es_output_column)
		{
			void *data;
			
			_es_output_cell_separator();
			
			data = _es_ipc2_get_column_data(list,i,_es_output_column->property_id,_es_highlight);
			if (data)
			{
				switch(_es_output_column->property_id)
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

							if (_es_folder_append_path_separator)
							{
								if (_es_output_column->property_id == EVERYTHING3_PROPERTY_ID_PATH_AND_NAME)
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

							_es_output_cell_size_property(size);
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

							_es_output_cell_number_property(value,ES_DWORD_MAX,TRUE);
						}
						
						break;		
									
					default:
						_es_output_cell_unknown_property();
						break;
				}
			}
			else
			{
				switch(_es_output_column->property_id)
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
			
			_es_output_column = _es_output_column->order_next;
		}

		result_run--;
		i++;

		_es_output_line_end(result_run ? 1 : 0);
	}

	if ((run) && (_es_footer > 0))
	{
		ES_UINT64 total_size;
		
		total_size = _es_ipc2_calculate_total_size(list);
		
		_es_output_footer(list->numitems,total_size);

		run--;
	}

	_es_output_page_end();

	wchar_buf_kill(&column_wcbuf);			
}

// output a ipc2 list 
// count should include the header if shown
static void	_es_output_ipc3_results(ipc3_result_list_t *result_list,SIZE_T index_start,SIZE_T count)
{
	SIZE_T run;
	utf8_buf_t property_text_cbuf;
	int is_first_line;
	ipc3_stream_t *stream;
	SIZE_T property_request_count;
	ipc3_result_list_property_request_t *property_request_array;
	SIZE_T result_run;

	utf8_buf_init(&property_text_cbuf);
	
	run = count;
	stream = result_list->stream;
	property_request_count = result_list->property_request_count;
	property_request_array = (ipc3_result_list_property_request_t *)result_list->property_request_cbuf.buf;
	
	_es_output_page_begin();

	if (run)
	{
		// output header.
		if (_es_header > 0)
		{
			_es_output_header();

			// don't inc i.
			run--;
		}
	}
	
	result_run = run;

	if (_es_footer > 0)
	{
		if (result_run)
		{
			result_run--;
		}
	}
		
	// clip run.
	if (result_run > result_list->viewport_count - index_start)
	{
		result_run = result_list->viewport_count - index_start;
	}
	
	run -= result_run;
	
	is_first_line = 1;
	
	while(result_run)
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
			
			_es_output_column = column_order_start;
			
			property_request_run = property_request_count;
			property_request_p = property_request_array;
			
			while(_es_output_column)
			{
				_es_output_cell_separator();
				
				if ((property_request_run) && (_es_output_column->property_id == property_request_p->property_id))
				{
					if (property_request_p->flags & (IPC3_SEARCH_PROPERTY_REQUEST_FLAG_FORMAT|IPC3_SEARCH_PROPERTY_REQUEST_FLAG_HIGHLIGHT))
					{
						SIZE_T len;
						
						len = ipc3_stream_read_len_vlq(stream);
						
						utf8_buf_grow_length(&property_text_cbuf,len);
						
						ipc3_stream_read_data(stream,property_text_cbuf.buf,len);
						
						property_text_cbuf.buf[len] = 0;

						if (_es_folder_append_path_separator)
						{
							if (property_request_p->property_id == EVERYTHING3_PROPERTY_ID_PATH_AND_NAME)
							{
								if (item_flags & IPC3_RESULT_LIST_ITEM_FLAG_FOLDER)
								{
									utf8_buf_cat_path_separator(&property_text_cbuf);
								}
							}
						}
						
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
									
									if (_es_folder_append_path_separator)
									{
										if (property_request_p->property_id == EVERYTHING3_PROPERTY_ID_PATH_AND_NAME)
										{
											if (item_flags & IPC3_RESULT_LIST_ITEM_FLAG_FOLDER)
											{
												utf8_buf_cat_path_separator(&property_text_cbuf);
											}
										}
									}
								}
								
								switch(property_get_format(_es_output_column->property_id))
								{
									case PROPERTY_FORMAT_TEXT8:
									case PROPERTY_FORMAT_TEXT10:
									case PROPERTY_FORMAT_TEXT12:
									case PROPERTY_FORMAT_TEXT16:
									case PROPERTY_FORMAT_TEXT24:
									case PROPERTY_FORMAT_TEXT30:
									case PROPERTY_FORMAT_TEXT32:
									case PROPERTY_FORMAT_TEXT47:
									case PROPERTY_FORMAT_TEXT48:
									case PROPERTY_FORMAT_TEXT64:
									case PROPERTY_FORMAT_EXTENSION:
										_es_output_cell_text_property_utf8_string_n(property_text_cbuf.buf,property_text_cbuf.length_in_bytes);
										break;
										
									default:
										debug_error_printf("unhandled format %d for %d\n",property_get_format(_es_output_column->property_id),property_request_p->value_type);
										_es_output_cell_unknown_property();
										break;

								}

								break;

							case IPC3_PROPERTY_VALUE_TYPE_BYTE:
							case IPC3_PROPERTY_VALUE_TYPE_BYTE_GET_TEXT:

								{
									BYTE byte_value;

									ipc3_stream_read_data(stream,&byte_value,sizeof(BYTE));
								
									switch(property_get_format(_es_output_column->property_id))
									{
										case PROPERTY_FORMAT_NOGROUPING_NUMBER1:
										case PROPERTY_FORMAT_NOGROUPING_NUMBER2:
										case PROPERTY_FORMAT_NOGROUPING_NUMBER3:
										case PROPERTY_FORMAT_NOGROUPING_NUMBER4:
										case PROPERTY_FORMAT_NOGROUPING_NUMBER5:
											_es_output_cell_number_property(byte_value,ES_BYTE_MAX,FALSE);
											break;

										case PROPERTY_FORMAT_GROUPING_NUMBER2:
										case PROPERTY_FORMAT_GROUPING_NUMBER3:
										case PROPERTY_FORMAT_GROUPING_NUMBER4:
										case PROPERTY_FORMAT_GROUPING_NUMBER5:
										case PROPERTY_FORMAT_GROUPING_NUMBER6:
										case PROPERTY_FORMAT_GROUPING_NUMBER7:
											_es_output_cell_number_property(byte_value,ES_BYTE_MAX,TRUE);
											break;
											
										case PROPERTY_FORMAT_RATING:
											_es_output_cell_rating_property(byte_value);
											break;
												
										case PROPERTY_FORMAT_YESNO:
											_es_output_cell_yesno_property(byte_value);
											break;
											
										case PROPERTY_FORMAT_PERCENT:
											_es_output_cell_percent_property(byte_value);
											break;
											
										default:
											debug_error_printf("unhandled format %d for %d\n",property_get_format(_es_output_column->property_id),property_request_p->value_type);
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
									
									switch(property_get_format(_es_output_column->property_id))
									{
										case PROPERTY_FORMAT_NOGROUPING_NUMBER1:
										case PROPERTY_FORMAT_NOGROUPING_NUMBER2:
										case PROPERTY_FORMAT_NOGROUPING_NUMBER3:
										case PROPERTY_FORMAT_NOGROUPING_NUMBER4:
										case PROPERTY_FORMAT_NOGROUPING_NUMBER5:
											_es_output_cell_number_property(word_value,ES_WORD_MAX,FALSE);
											break;

										case PROPERTY_FORMAT_GROUPING_NUMBER2:
										case PROPERTY_FORMAT_GROUPING_NUMBER3:
										case PROPERTY_FORMAT_GROUPING_NUMBER4:
										case PROPERTY_FORMAT_GROUPING_NUMBER5:
										case PROPERTY_FORMAT_GROUPING_NUMBER6:
										case PROPERTY_FORMAT_GROUPING_NUMBER7:
											_es_output_cell_number_property(word_value,ES_WORD_MAX,TRUE);
											break;
											
										case PROPERTY_FORMAT_ISO_SPEED:
											_es_output_cell_iso_speed_property(word_value);
											break;
											
										case PROPERTY_FORMAT_35MM_FOCAL_LENGTH:
											_es_output_cell_small_number_property_with_suffix(word_value,ES_WORD_MAX,"mm");
											break;
											
										default:
											debug_error_printf("unhandled format %d for %d\n",property_get_format(_es_output_column->property_id),property_request_p->value_type);
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
									
									switch(property_get_format(_es_output_column->property_id))
									{
										case PROPERTY_FORMAT_ATTRIBUTES:
											_es_output_cell_attribute_property(dword_value);
											break;

										case PROPERTY_FORMAT_NOGROUPING_NUMBER1:
										case PROPERTY_FORMAT_NOGROUPING_NUMBER2:
										case PROPERTY_FORMAT_NOGROUPING_NUMBER3:
										case PROPERTY_FORMAT_NOGROUPING_NUMBER4:
										case PROPERTY_FORMAT_NOGROUPING_NUMBER5:
											_es_output_cell_number_property(dword_value,ES_DWORD_MAX,FALSE);
											break;

										case PROPERTY_FORMAT_GROUPING_NUMBER2:
										case PROPERTY_FORMAT_GROUPING_NUMBER3:
										case PROPERTY_FORMAT_GROUPING_NUMBER4:
										case PROPERTY_FORMAT_GROUPING_NUMBER5:
										case PROPERTY_FORMAT_GROUPING_NUMBER6:
										case PROPERTY_FORMAT_GROUPING_NUMBER7:
											_es_output_cell_number_property(dword_value,ES_DWORD_MAX,TRUE);
											break;
										
										case PROPERTY_FORMAT_HEX_NUMBER8:
											_es_output_cell_hex_number8_property(dword_value);
											break;
											
										case PROPERTY_FORMAT_KBPS:
											_es_output_cell_kbps_property(dword_value,ES_DWORD_MAX);
											break;
											
										case PROPERTY_FORMAT_KHZ:
											_es_output_cell_khz_property(dword_value,ES_DWORD_MAX);
											break;
											
										case PROPERTY_FORMAT_FIXED_Q1K:
											_es_output_cell_fixed_q1k_property(dword_value,ES_DWORD_MAX,0);
											break;
											
										case PROPERTY_FORMAT_TIME:
											_es_output_cell_time_property(dword_value);
											break;
										
										case PROPERTY_FORMAT_DATE:
											_es_output_cell_date_property(dword_value);
											break;
											
										default:
											debug_error_printf("unhandled format %d for %d\n",property_get_format(_es_output_column->property_id),property_request_p->value_type);
											_es_output_cell_unknown_property();
											break;
									}
								}
								
								break;
								
							case IPC3_PROPERTY_VALUE_TYPE_UINT64: 

								{
									ES_UINT64 uint64_value;

									ipc3_stream_read_data(stream,&uint64_value,sizeof(ES_UINT64));

									switch(property_get_format(_es_output_column->property_id))
									{
										case PROPERTY_FORMAT_SIZE:
										case PROPERTY_FORMAT_VOLUME_SIZE:
											_es_output_cell_size_property(uint64_value);
											break;

										case PROPERTY_FORMAT_FILETIME:
											_es_output_cell_filetime_property(uint64_value);
											break;
											
										case PROPERTY_FORMAT_DURATION:
											_es_output_cell_duration_property(uint64_value);
											break;
											
										case PROPERTY_FORMAT_NOGROUPING_NUMBER1:
										case PROPERTY_FORMAT_NOGROUPING_NUMBER2:
										case PROPERTY_FORMAT_NOGROUPING_NUMBER3:
										case PROPERTY_FORMAT_NOGROUPING_NUMBER4:
										case PROPERTY_FORMAT_NOGROUPING_NUMBER5:
											_es_output_cell_number_property(uint64_value,ES_UINT64_MAX,FALSE);
											break;

										case PROPERTY_FORMAT_GROUPING_NUMBER2:
										case PROPERTY_FORMAT_GROUPING_NUMBER3:
										case PROPERTY_FORMAT_GROUPING_NUMBER4:
										case PROPERTY_FORMAT_GROUPING_NUMBER5:
										case PROPERTY_FORMAT_GROUPING_NUMBER6:
										case PROPERTY_FORMAT_GROUPING_NUMBER7:
											_es_output_cell_number_property(uint64_value,ES_UINT64_MAX,TRUE);
											break;
											
										case PROPERTY_FORMAT_HEX_NUMBER16:
											_es_output_cell_hex_number16_property(uint64_value,ES_UINT64_MAX);
											break;

										default:
											debug_error_printf("unhandled format %d for %d\n",property_get_format(_es_output_column->property_id),property_request_p->value_type);
											_es_output_cell_unknown_property();
											break;
									}
								}
								
								break;
								
							case IPC3_PROPERTY_VALUE_TYPE_UINT128: 

								{
									EVERYTHING3_UINT128 uint128_value;

									ipc3_stream_read_data(stream,&uint128_value,sizeof(EVERYTHING3_UINT128));
									
									switch(property_get_format(_es_output_column->property_id))
									{
										case PROPERTY_FORMAT_HEX_NUMBER32:
											_es_output_cell_hex_number32_property(&uint128_value);
											break;

										default:
											debug_error_printf("unhandled format %d for %d\n",property_get_format(_es_output_column->property_id),property_request_p->value_type);
											_es_output_cell_unknown_property();
											break;
									}
								}
								
								break;
								
							case IPC3_PROPERTY_VALUE_TYPE_DIMENSIONS: 

								{
									EVERYTHING3_DIMENSIONS dimensions_value;

									ipc3_stream_read_data(stream,&dimensions_value,sizeof(EVERYTHING3_DIMENSIONS));

									switch(property_get_format(_es_output_column->property_id))
									{
										case PROPERTY_FORMAT_DIMENSIONS:
											_es_output_cell_dimensions_property(&dimensions_value);
											break;
											
										default:
											debug_error_printf("unhandled format %d for %d\n",property_get_format(_es_output_column->property_id),property_request_p->value_type);
											_es_output_cell_unknown_property();
											break;
									}
								}
								
								break;
								
							case IPC3_PROPERTY_VALUE_TYPE_SIZE_T:
							
								{
									SIZE_T size_t_value;
									
									size_t_value = ipc3_stream_read_size_t(stream);

									switch(property_get_format(_es_output_column->property_id))
									{	
										case PROPERTY_FORMAT_NOGROUPING_NUMBER1:
										case PROPERTY_FORMAT_NOGROUPING_NUMBER2:
										case PROPERTY_FORMAT_NOGROUPING_NUMBER3:
										case PROPERTY_FORMAT_NOGROUPING_NUMBER4:
										case PROPERTY_FORMAT_NOGROUPING_NUMBER5:
											_es_output_cell_number_property(size_t_value,SIZE_MAX,FALSE);
											break;
											
										case PROPERTY_FORMAT_GROUPING_NUMBER2:
										case PROPERTY_FORMAT_GROUPING_NUMBER3:
										case PROPERTY_FORMAT_GROUPING_NUMBER4:
										case PROPERTY_FORMAT_GROUPING_NUMBER5:
										case PROPERTY_FORMAT_GROUPING_NUMBER6:
										case PROPERTY_FORMAT_GROUPING_NUMBER7:
											_es_output_cell_number_property(size_t_value,SIZE_MAX,TRUE);
											break;

										case PROPERTY_FORMAT_HEX_NUMBER16:
											_es_output_cell_hex_number16_property(size_t_value,SIZE_MAX);
											break;
											
										default:
											debug_error_printf("unhandled format %d for %d\n",property_get_format(_es_output_column->property_id),property_request_p->value_type);
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
									
									switch(property_get_format(_es_output_column->property_id))
									{	
										case PROPERTY_FORMAT_FIXED_Q1K:
											_es_output_cell_fixed_q1k_property(int32_value,INT_MIN,1);
											break;
											
										case PROPERTY_FORMAT_FIXED_Q1M:
											_es_output_cell_fixed_q1m_property(int32_value,INT_MIN,1);
											break;
											
										case PROPERTY_FORMAT_F_STOP:
											_es_output_cell_f_stop_property(int32_value);
											break;
											
										case PROPERTY_FORMAT_EXPOSURE_TIME:
											_es_output_cell_exposure_time_property(int32_value);
											break;

										case PROPERTY_FORMAT_EXPOSURE_BIAS:
											_es_output_cell_exposure_bias_property(int32_value);
											break;
											
										case PROPERTY_FORMAT_FOCAL_LENGTH:
											_es_output_cell_fixed_q1k_property_with_suffix(int32_value,"mm");
											break;
											
										case PROPERTY_FORMAT_SUBJECT_DISTANCE:
											_es_output_cell_fixed_q1k_property_with_suffix(int32_value,"m");
											break;
											
										case PROPERTY_FORMAT_BCPS:	
											_es_output_cell_bcps_property(int32_value);
											break;
													
										case PROPERTY_FORMAT_ALTITUDE:
											_es_output_cell_fixed_q1k_property_with_suffix(int32_value,"m");
											break;
										
										case PROPERTY_FORMAT_SEC:
											_es_output_cell_fixed_q1k_property_with_suffix(int32_value,"sec");
											break;
										
										default:
											debug_error_printf("unhandled format %d for %d\n",property_get_format(_es_output_column->property_id),property_request_p->value_type);
											_es_output_cell_unknown_property();
											break;
									}
								}
								
								break;

							case IPC3_PROPERTY_VALUE_TYPE_BLOB8:

								{
									BYTE len;
									
									len = ipc3_stream_read_byte(stream);
									
									utf8_buf_grow_length(&property_text_cbuf,len);
									
									ipc3_stream_read_data(stream,property_text_cbuf.buf,len);
									
									property_text_cbuf.buf[len] = 0;
								
									switch(property_get_format(_es_output_column->property_id))
									{	
										case PROPERTY_FORMAT_DATA1:
										case PROPERTY_FORMAT_DATA2:
										case PROPERTY_FORMAT_DATA4:
										case PROPERTY_FORMAT_DATA8:
										case PROPERTY_FORMAT_DATA16:
										case PROPERTY_FORMAT_DATA20:
										case PROPERTY_FORMAT_DATA32:
										case PROPERTY_FORMAT_DATA48:
										case PROPERTY_FORMAT_DATA64:
										case PROPERTY_FORMAT_DATA128:
										case PROPERTY_FORMAT_DATA256:
										case PROPERTY_FORMAT_DATA512:
											_es_output_cell_data_property(property_text_cbuf.buf,property_text_cbuf.length_in_bytes);
											break;
											
										default:
											debug_error_printf("unhandled format %d for %d\n",property_get_format(_es_output_column->property_id),property_request_p->value_type);
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
									
									switch(property_get_format(_es_output_column->property_id))
									{	
										case PROPERTY_FORMAT_DATA1:
										case PROPERTY_FORMAT_DATA2:
										case PROPERTY_FORMAT_DATA4:
										case PROPERTY_FORMAT_DATA8:
										case PROPERTY_FORMAT_DATA16:
										case PROPERTY_FORMAT_DATA20:
										case PROPERTY_FORMAT_DATA32:
										case PROPERTY_FORMAT_DATA48:
										case PROPERTY_FORMAT_DATA64:
										case PROPERTY_FORMAT_DATA128:
										case PROPERTY_FORMAT_DATA256:
										case PROPERTY_FORMAT_DATA512:
											_es_output_cell_data_property(property_text_cbuf.buf,property_text_cbuf.length_in_bytes);
											break;
											
										default:
											debug_error_printf("unhandled format %d for %d\n",property_get_format(_es_output_column->property_id),property_request_p->value_type);
											_es_output_cell_unknown_property();
											break;
									}
								}

								break;

							case IPC3_PROPERTY_VALUE_TYPE_PROPVARIANT:
			
								{
									BYTE propvariant_type;

									propvariant_type =  ipc3_stream_read_byte(stream);
									
									switch(propvariant_type)
									{
										case EVERYTHING3_PROPERTY_VARIANT_TYPE_EMPTY:
										case EVERYTHING3_PROPERTY_VARIANT_TYPE_NULL:
											_es_output_cell_unknown_property();
											break;
										
										case EVERYTHING3_PROPERTY_VARIANT_TYPE_BYTE_UI1:
										
											{
												BYTE byte_value;

												ipc3_stream_read_data(stream,&byte_value,sizeof(BYTE));

												// there's no empty value.
												utf8_buf_printf(&property_text_cbuf,"%u",byte_value);
												_es_output_cell_formatted_number_property(property_text_cbuf.buf);
											}
											
											break;
											
										case EVERYTHING3_PROPERTY_VARIANT_TYPE_WORD_UI2:

											{
												WORD word_value;

												ipc3_stream_read_data(stream,&word_value,sizeof(WORD));

												// there's no empty value.
												utf8_buf_printf(&property_text_cbuf,"%u",word_value);
												_es_output_cell_formatted_number_property(property_text_cbuf.buf);
											}
											
											break;
											
										case EVERYTHING3_PROPERTY_VARIANT_TYPE_DWORD_UI4:
										case EVERYTHING3_PROPERTY_VARIANT_TYPE_DWORD_UINT:
											
											{
												DWORD dword_value;

												ipc3_stream_read_data(stream,&dword_value,sizeof(DWORD));

												// there's no empty value.
												utf8_buf_printf(&property_text_cbuf,"%u",dword_value);
												_es_output_cell_formatted_number_property(property_text_cbuf.buf);
											}
											
											break;

										case EVERYTHING3_PROPERTY_VARIANT_TYPE_UINT64_UI8:
										case EVERYTHING3_PROPERTY_VARIANT_TYPE_UINT64_FILETIME:
										
											{
												ES_UINT64 uint64_value;

												ipc3_stream_read_data(stream,&uint64_value,sizeof(ES_UINT64));

												if (propvariant_type == EVERYTHING3_PROPERTY_VARIANT_TYPE_UINT64_FILETIME)
												{
													_es_output_cell_filetime_property(uint64_value);
												}
												else
												{
													utf8_buf_printf(&property_text_cbuf,"%I64u",uint64_value);
													_es_output_cell_formatted_number_property(property_text_cbuf.buf);
												}
											}
											
											break;
											
										case EVERYTHING3_PROPERTY_VARIANT_TYPE_CHAR_I1:
											{
												char char_value;

												ipc3_stream_read_data(stream,&char_value,sizeof(char));

												// there's no empty value.
												utf8_buf_printf(&property_text_cbuf,"%d",char_value);
												_es_output_cell_formatted_number_property(property_text_cbuf.buf);
											}
											break;

										case EVERYTHING3_PROPERTY_VARIANT_TYPE_INT16_I2:
										case EVERYTHING3_PROPERTY_VARIANT_TYPE_INT16_BOOL:
											{
												__int16 int16_value;

												ipc3_stream_read_data(stream,&int16_value,sizeof(__int16));

												// there's no empty value.
												utf8_buf_printf(&property_text_cbuf,"%d",int16_value);
												_es_output_cell_formatted_number_property(property_text_cbuf.buf);
											}
											break;

										case EVERYTHING3_PROPERTY_VARIANT_TYPE_INT32_I4:
										case EVERYTHING3_PROPERTY_VARIANT_TYPE_INT32_INT:
										case EVERYTHING3_PROPERTY_VARIANT_TYPE_INT32_ERROR:
											{
												__int32 int32_value;

												ipc3_stream_read_data(stream,&int32_value,sizeof(__int32));

												// there's no empty value.
												utf8_buf_printf(&property_text_cbuf,"%d",int32_value);
												_es_output_cell_formatted_number_property(property_text_cbuf.buf);
											}
											break;

										case EVERYTHING3_PROPERTY_VARIANT_TYPE_INT64_I8:
										case EVERYTHING3_PROPERTY_VARIANT_TYPE_INT64_CY:
											{
												__int64 int64_value;

												ipc3_stream_read_data(stream,&int64_value,sizeof(__int64));

												// there's no empty value.
												utf8_buf_printf(&property_text_cbuf,"%I64d",int64_value);
												_es_output_cell_formatted_number_property(property_text_cbuf.buf);
											}
											break;

										case EVERYTHING3_PROPERTY_VARIANT_TYPE_FLOAT_R4:
											{
												float float_value;
												__int64 fixed_q1m_value;
												wchar_buf_t fixed_wcbuf;

												wchar_buf_init(&fixed_wcbuf);

												ipc3_stream_read_data(stream,&float_value,sizeof(float));

												fixed_q1m_value = (__int64)(float_value * 1000000);

												// there's no empty value.
												_es_format_fixed_q1m(fixed_q1m_value,0,0,1,&fixed_wcbuf);
												utf8_buf_copy_wchar_string(&property_text_cbuf,fixed_wcbuf.buf);
												_es_output_cell_formatted_number_property(property_text_cbuf.buf);

												wchar_buf_kill(&fixed_wcbuf);
											}
											break;
											
										case EVERYTHING3_PROPERTY_VARIANT_TYPE_DOUBLE_R8:
										case EVERYTHING3_PROPERTY_VARIANT_TYPE_DOUBLE_DATE:
										
											{
												double double_value;
												__int64 fixed_q1m_value;
												wchar_buf_t fixed_wcbuf;

												wchar_buf_init(&fixed_wcbuf);

												ipc3_stream_read_data(stream,&double_value,sizeof(double));

												fixed_q1m_value = (__int64)(double_value * 1000000);

												// there's no empty value.
												_es_format_fixed_q1m(fixed_q1m_value,0,0,1,&fixed_wcbuf);
												utf8_buf_copy_wchar_string(&property_text_cbuf,fixed_wcbuf.buf);
												_es_output_cell_formatted_number_property(property_text_cbuf.buf);

												wchar_buf_kill(&fixed_wcbuf);
											}
											break;

										case EVERYTHING3_PROPERTY_VARIANT_TYPE_POINTER_CLSID:

											{
												CLSID clsid_value;

												ipc3_stream_read_data(stream,&clsid_value,sizeof(CLSID));

												// there's no empty value.
												_es_format_clsid(&clsid_value,&property_text_cbuf);
												_es_output_cell_text_property_utf8_string_n(property_text_cbuf.buf,property_text_cbuf.length_in_bytes);
											}
											break;
											
										case EVERYTHING3_PROPERTY_VARIANT_TYPE_BLOB:
											
											{
												SIZE_T len;
												
												len = ipc3_stream_read_len_vlq(stream);
												
												utf8_buf_grow_length(&property_text_cbuf,len);
												
												ipc3_stream_read_data(stream,property_text_cbuf.buf,len);
												
												property_text_cbuf.buf[len] = 0;

												_es_output_cell_data_property(property_text_cbuf.buf,property_text_cbuf.length_in_bytes);
											}
											break;
											
										case EVERYTHING3_PROPERTY_VARIANT_TYPE_STRING_BSTR:
										case EVERYTHING3_PROPERTY_VARIANT_TYPE_STRING_LPWSTR:
										case EVERYTHING3_PROPERTY_VARIANT_TYPE_STRING_LPSTR:
										
											{
												SIZE_T len;
												
												len = ipc3_stream_read_len_vlq(stream);
												
												utf8_buf_grow_length(&property_text_cbuf,len);
												
												ipc3_stream_read_data(stream,property_text_cbuf.buf,len);
												
												property_text_cbuf.buf[len] = 0;

												_es_output_cell_text_property_utf8_string_n(property_text_cbuf.buf,property_text_cbuf.length_in_bytes);
											}

											break;

										case EVERYTHING3_PROPERTY_VARIANT_TYPE_ARRAY_BYTE_UI1:
										case EVERYTHING3_PROPERTY_VARIANT_TYPE_ARRAY_WORD_UI2:
										case EVERYTHING3_PROPERTY_VARIANT_TYPE_ARRAY_DWORD_UI4:
										case EVERYTHING3_PROPERTY_VARIANT_TYPE_ARRAY_UINT64_UI8:
										case EVERYTHING3_PROPERTY_VARIANT_TYPE_ARRAY_UINT64_FILETIME:
										case EVERYTHING3_PROPERTY_VARIANT_TYPE_ARRAY_CHAR_I1:
										case EVERYTHING3_PROPERTY_VARIANT_TYPE_ARRAY_INT16_I2:
										case EVERYTHING3_PROPERTY_VARIANT_TYPE_ARRAY_INT16_BOOL:
										case EVERYTHING3_PROPERTY_VARIANT_TYPE_ARRAY_INT32_I4:
										case EVERYTHING3_PROPERTY_VARIANT_TYPE_ARRAY_INT32_ERROR:
										case EVERYTHING3_PROPERTY_VARIANT_TYPE_ARRAY_INT64_I8:
										case EVERYTHING3_PROPERTY_VARIANT_TYPE_ARRAY_INT64_CY:
										case EVERYTHING3_PROPERTY_VARIANT_TYPE_ARRAY_FLOAT_R4:
										case EVERYTHING3_PROPERTY_VARIANT_TYPE_ARRAY_DOUBLE_R8:
										case EVERYTHING3_PROPERTY_VARIANT_TYPE_ARRAY_DOUBLE_DATE:
										case EVERYTHING3_PROPERTY_VARIANT_TYPE_ARRAY_CLSID:
										case EVERYTHING3_PROPERTY_VARIANT_TYPE_ARRAY_STRING_BSTR:
										case EVERYTHING3_PROPERTY_VARIANT_TYPE_ARRAY_STRING_LPWSTR:
										case EVERYTHING3_PROPERTY_VARIANT_TYPE_ARRAY_STRING_LPSTR:
											
											{
												SIZE_T array_count;
												utf8_buf_t string_cbuf;

												utf8_buf_init(&string_cbuf);
												
												array_count = ipc3_stream_read_len_vlq(stream);
												
												utf8_buf_empty(&property_text_cbuf);
												
												while(array_count)
												{
													if (property_text_cbuf.length_in_bytes)
													{
														utf8_buf_cat_byte(&property_text_cbuf,';');
													}
													
													switch(propvariant_type)
													{
														case EVERYTHING3_PROPERTY_VARIANT_TYPE_ARRAY_BYTE_UI1:
															
															{
																BYTE byte_value;
																byte_value = ipc3_stream_read_byte(stream);
																utf8_buf_printf(&string_cbuf,"%u",byte_value);
															}
															
															break;
															
														case EVERYTHING3_PROPERTY_VARIANT_TYPE_ARRAY_WORD_UI2:
															
															{
																WORD word_value;
	
																ipc3_stream_read_data(stream,&word_value,sizeof(WORD));

																// there's no empty value.
																utf8_buf_printf(&string_cbuf,"%u",word_value);
															}
															
															break;
															
														case EVERYTHING3_PROPERTY_VARIANT_TYPE_ARRAY_DWORD_UI4:
														
															{
																DWORD dword_value;
			
																ipc3_stream_read_data(stream,&dword_value,sizeof(DWORD));

																// there's no empty value.
																utf8_buf_printf(&string_cbuf,"%u",dword_value);
															}
															
															break;
															
														case EVERYTHING3_PROPERTY_VARIANT_TYPE_ARRAY_UINT64_UI8:
														case EVERYTHING3_PROPERTY_VARIANT_TYPE_ARRAY_UINT64_FILETIME:
														
															{
																ES_UINT64 uint64_value;
	
																ipc3_stream_read_data(stream,&uint64_value,sizeof(ES_UINT64));

																if ((propvariant_type == EVERYTHING3_PROPERTY_VARIANT_TYPE_ARRAY_UINT64_FILETIME) && (_es_date_format))
																{
																	wchar_buf_t string_wcbuf;

																	wchar_buf_init(&string_wcbuf);

																	_es_format_filetime(uint64_value,&string_wcbuf);
																	utf8_buf_copy_wchar_string(&string_cbuf,string_wcbuf.buf);

																	wchar_buf_kill(&string_wcbuf);
																}
																else
																{
																	// there's no empty value.
																	utf8_buf_printf(&string_cbuf,"%I64u",uint64_value);
																}
															}
															
															break;

														case EVERYTHING3_PROPERTY_VARIANT_TYPE_ARRAY_CHAR_I1:
														
															{
																char char_value;
																
																ipc3_stream_read_data(stream,&char_value,sizeof(char));
																
																utf8_buf_printf(&string_cbuf,"%d",char_value);
															}
															
															break;	

														case EVERYTHING3_PROPERTY_VARIANT_TYPE_ARRAY_INT16_I2:
														case EVERYTHING3_PROPERTY_VARIANT_TYPE_ARRAY_INT16_BOOL:

															{
																__int16 int16_value;
																
																ipc3_stream_read_data(stream,&int16_value,sizeof(__int16));
																
																utf8_buf_printf(&string_cbuf,"%d",int16_value);
															}
															
															break;
															
														case EVERYTHING3_PROPERTY_VARIANT_TYPE_ARRAY_INT32_I4:
														case EVERYTHING3_PROPERTY_VARIANT_TYPE_ARRAY_INT32_ERROR:

															{
																__int32 int32_value;
																
																ipc3_stream_read_data(stream,&int32_value,sizeof(__int32));
																
																utf8_buf_printf(&string_cbuf,"%d",int32_value);
															}
															
															break;
											
														case EVERYTHING3_PROPERTY_VARIANT_TYPE_ARRAY_INT64_I8:
														case EVERYTHING3_PROPERTY_VARIANT_TYPE_ARRAY_INT64_CY:
															
															{
																__int64 int64_value;
																
																ipc3_stream_read_data(stream,&int64_value,sizeof(__int64));
																
																utf8_buf_printf(&string_cbuf,"%I64d",int64_value);
															}
															
															break;
															
														case EVERYTHING3_PROPERTY_VARIANT_TYPE_ARRAY_FLOAT_R4:
															
															{
																float float_value;
																__int64 fixed_q1m_value;
																wchar_buf_t fixed_wcbuf;

																wchar_buf_init(&fixed_wcbuf);

																ipc3_stream_read_data(stream,&float_value,sizeof(float));

																fixed_q1m_value = (__int64)(float_value * 1000000);

																// there's no empty value.
																_es_format_fixed_q1m(fixed_q1m_value,0,0,1,&fixed_wcbuf);
																utf8_buf_copy_wchar_string(&string_cbuf,fixed_wcbuf.buf);

																wchar_buf_kill(&fixed_wcbuf);
															}
															
															break;
															
														case EVERYTHING3_PROPERTY_VARIANT_TYPE_ARRAY_DOUBLE_R8:
														case EVERYTHING3_PROPERTY_VARIANT_TYPE_ARRAY_DOUBLE_DATE:
															
															{
																double double_value;
																__int64 fixed_q1m_value;
																wchar_buf_t fixed_wcbuf;

																wchar_buf_init(&fixed_wcbuf);

																ipc3_stream_read_data(stream,&double_value,sizeof(double));

																fixed_q1m_value = (__int64)(double_value * 1000000);

																// there's no empty value.
																_es_format_fixed_q1m(fixed_q1m_value,0,0,1,&fixed_wcbuf);
																utf8_buf_copy_wchar_string(&string_cbuf,fixed_wcbuf.buf);

																wchar_buf_kill(&fixed_wcbuf);
															}
															
															break;
															
														case EVERYTHING3_PROPERTY_VARIANT_TYPE_ARRAY_CLSID:
															
															{
																CLSID clsid_value;

																ipc3_stream_read_data(stream,&clsid_value,sizeof(CLSID));

																// there's no empty value.
																_es_format_clsid(&clsid_value,&string_cbuf);
															}
															
															break;
																		
														case EVERYTHING3_PROPERTY_VARIANT_TYPE_ARRAY_STRING_BSTR:
														case EVERYTHING3_PROPERTY_VARIANT_TYPE_ARRAY_STRING_LPWSTR:
														case EVERYTHING3_PROPERTY_VARIANT_TYPE_ARRAY_STRING_LPSTR:

															{
																SIZE_T len;
																
																len = ipc3_stream_read_len_vlq(stream);
																	
																utf8_buf_grow_length(&string_cbuf,len);
																	
																ipc3_stream_read_data(stream,string_cbuf.buf,len);
																string_cbuf.buf[len] = 0;
															}
															
															break;
															
													}

													utf8_buf_cat_utf8_string_n(&property_text_cbuf,string_cbuf.buf,string_cbuf.length_in_bytes);

													array_count--;
												}
												
												_es_output_cell_text_property_utf8_string_n(property_text_cbuf.buf,property_text_cbuf.length_in_bytes);

												utf8_buf_kill(&string_cbuf);
											}
											
											break;
									
										default:
											// bad type;
											debug_error_printf("unhandled propvariant type %d\n",propvariant_type);
											_es_output_cell_unknown_property();
											break;
									}
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
					if (_es_output_column->property_id == EVERYTHING3_PROPERTY_ID_ATTRIBUTES)
					{
						// always output known attributes. (EFU export)
						_es_output_cell_attribute_property(item_flags & IPC3_RESULT_LIST_ITEM_FLAG_FOLDER ? FILE_ATTRIBUTE_DIRECTORY : 0);
					}
					else
					{
						_es_output_cell_unknown_property();
					}
				}
				
				_es_output_column = _es_output_column->order_next;
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

		result_run--;

		_es_output_line_end(result_run ? 1 : 0);

		is_first_line = 0;
	}
	
	if ((run) && (_es_footer > 0))
	{
		_es_output_footer(result_list->file_result_count+result_list->folder_result_count,result_list->total_result_size);

		run--;
	}

	_es_output_page_end();

	utf8_buf_kill(&property_text_cbuf);
}

static ES_UINT64 _es_ipc2_calculate_total_size(EVERYTHING_IPC_LIST2 *list)
{
	DWORD i;
	EVERYTHING_IPC_ITEM2 *items;
	ES_UINT64 total_size;
	SIZE_T run;
	
	items = (EVERYTHING_IPC_ITEM2 *)(list + 1);

	i = 0;
	total_size = 0;
	run = list->numitems;
	
	while(run)
	{
		void *data;
		
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

		i++;
		run--;
	}

	return total_size;
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
				case _ES_COPYDATA_IPCTEST_QUERYCOMPLETEW:

					if (_es_no_result_error)
					{
						if (((EVERYTHING_IPC_LIST *)cds->lpData)->totitems == 0)
						{
							_es_ret = ES_ERROR_NO_RESULTS;
						}
					}

					if (_es_get_result_count)
					{
						_es_output_noncell_result_count(((EVERYTHING_IPC_LIST *)cds->lpData)->totitems);
					}
					else
					if (_es_get_total_size)
					{
						// version 2 or later required.
						// IPC unavailable.
						_es_ret = ES_ERROR_NO_IPC;
					}
					else
					{
						EVERYTHING_IPC_LIST *list;
						utf8_buf_t sorted_list_cbuf;
						
						list = cds->lpData;
						utf8_buf_init(&sorted_list_cbuf);
						
						// sort by path.
						// sort folders first to be consistant with other sorts.
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
					
						if (_es_pause)
						{
							_es_output_pause(ES_IPC_VERSION_FLAG_IPC1,list);
						}
						else
						{
							SIZE_T total_lines;
							
							total_lines = list->numitems;
							if (_es_header > 0)
							{
								total_lines = safe_size_add_one(total_lines);
							}
							
							if (_es_footer > 0)
							{
								total_lines = safe_size_add_one(total_lines);
							}
							
							_es_output_ipc1_results(list,0,total_lines);
						}
						
						utf8_buf_kill(&sorted_list_cbuf);
					}
					
					PostQuitMessage(0);
					
					return TRUE;
					
				case _ES_COPYDATA_IPCTEST_QUERYCOMPLETE2W:
				{
					if (_es_no_result_error)
					{
						if (((EVERYTHING_IPC_LIST2 *)cds->lpData)->totitems == 0)
						{
							_es_ret = ES_ERROR_NO_RESULTS;
						}
					}
				
					if (_es_get_result_count)
					{
						_es_output_noncell_result_count(((EVERYTHING_IPC_LIST2 *)cds->lpData)->totitems);
					}
					else
					if (_es_get_total_size)
					{
						ES_UINT64 total_size;
						
						total_size = _es_ipc2_calculate_total_size((EVERYTHING_IPC_LIST2 *)cds->lpData);

						_es_output_noncell_total_size(total_size);
					}
					else
					if (_es_pause)
					{
						_es_output_pause(ES_IPC_VERSION_FLAG_IPC2,cds->lpData);
					}
					else
					{
						SIZE_T total_lines;
						
						total_lines = ((EVERYTHING_IPC_LIST2 *)cds->lpData)->numitems;
						if (_es_header > 0)
						{
							total_lines = safe_size_add_one(total_lines);
						}

						if (_es_footer > 0)
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
		"        attributes|filelist-filename|run-count|date-recently-changed|date-run|\r\n"
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
		"Journal options\r\n"
		"   -j, -journal [filename filter]\r\n"
		"        Show index journal changes.\r\n"
		"        Any journal option below also shows journal changes.\r\n"
		"        Wildcards are supported in the filename filter.\r\n"
		"        The whole final case-insensitive filename is matched.\r\n"
		"        Use a path separator to match full paths and names.\r\n"
		"   -get-journal-id\r\n"
		"        Return the current journal ID.\r\n"
		"   -get-journal-pos\r\n"
		"        Return the current journal ID and next change ID.\r\n"
		"   -action-filter <semicolon (;) delimited list of actions>\r\n"
		"        Show only changes with the specified actions:\r\n"
		"        folder-create;folder-delete;folder-rename;folder-move;folder-modify;\r\n"
		"        file-create;file-delete;file-rename;file-move;file-modify\r\n"
		"   -watch\r\n"
		"        Return when a match is found and display the journal position.\r\n"
		"\r\n"
		"   -from-journal-pos <journal-id> <change-id>\r\n"
		"   -from-journal-id <journal-id>\r\n"
		"   -from-change-id <change-id>\r\n"
		"        Show changes from the specified journal-id and change-id.\r\n"
		"   -from-date <date>\r\n"
		"        Show changes starting from the specified ISO-8601 date.\r\n"
		"   -from-yesterday\r\n"
		"        Show changes starting from the beginning of yesterday.\r\n"
		"   -from-today\r\n"
		"        Show changes starting from the beginning of today.\r\n"
		"   -from-now\r\n"
		"        Show changes starting from the current time.\r\n"
		"\r\n"
		"   -to-journal-pos <journal-id> <change-id>\r\n"
		"   -to-journal-id <journal-id>\r\n"
		"   -to-change-id <change-id>\r\n"
		"        Show changes until the specified journal-id and change-id (exclusive).\r\n"
		"   -to-date <date>\r\n"
		"        Show changes until the specified ISO-8601 date (exclusive).\r\n"
		"   -to-today\r\n"
		"        Show changes until the start of today (exclusive).\r\n"
		"   -to-tomorrow\r\n"
		"        Show changes until the start of tomorrow (exclusive).\r\n"
		"   -to-now\r\n"
		"        Show changes until the current time (exclusive).\r\n"
		"\r\n"
		"   -after-journal-pos <journal-id> <change-id>\r\n"
		"        Show changes after the specified journal-id and change-id.\r\n"
		"   -changed-today\r\n"
		"        Show changes from the start of today until the start of tomorrow.\r\n"
		"        Same as -from-today -to-tomorrow\r\n"
		"   -changed-yesterday\r\n"
		"        Show changes from the start of yesterday until the start of today.\r\n"
		"        Same as -from-yesterday -to-today\r\n"
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
		"   -add-columns <property-name;property-name2;...>\r\n"
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
		"   -set-column-colors <property-name=color;property-name2=color;...>\r\n"
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
		"   -set-column-widths <property-name=width;property-name2=width;...>\r\n"
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
		"   -no-folder-append-path-separator\r\n"
		"        Don't append a trailing path separator to folder paths.\r\n"
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
	wchar_buf_t local_instance_name_wcbuf;
	wchar_buf_t local_locale_thousand_wcbuf;
	wchar_buf_t local_locale_decimal_wcbuf;
	wchar_t *get_folder_size_filename;
	pool_t local_column_color_pool;
	pool_t local_column_width_pool;
	pool_t local_column_pool;
	pool_t local_secondary_sort_pool;
	pool_t local_property_unknown_pool;
	array_t local_column_color_array;
	array_t local_column_width_array;
	array_t local_column_array;
	array_t local_secondary_sort_array;
	array_t local_property_unknown_array;
	
	os_init();
	
	wchar_buf_init(&argv_wcbuf);
	wchar_buf_init(&search_wcbuf);
	wchar_buf_init(&filter_wcbuf);
	wchar_buf_init(&local_instance_name_wcbuf);
	wchar_buf_init(&local_locale_thousand_wcbuf);
	wchar_buf_init(&local_locale_decimal_wcbuf);
	pool_init(&local_column_color_pool);
	pool_init(&local_column_width_pool);
	pool_init(&local_column_pool);
	pool_init(&local_secondary_sort_pool);
	pool_init(&local_property_unknown_pool);
	array_init(&local_column_color_array);
	array_init(&local_column_width_array);
	array_init(&local_column_array);
	array_init(&local_secondary_sort_array);
	array_init(&local_property_unknown_array);
	
	get_folder_size_filename = NULL;
	es_instance_name_wcbuf = &local_instance_name_wcbuf;
	_es_locale_thousand_wcbuf = &local_locale_thousand_wcbuf;
	_es_locale_decimal_wcbuf = &local_locale_decimal_wcbuf;
	_es_search_wcbuf = &search_wcbuf;
	
	// I am trying to avoid bloating the exe size by using large global varibales.
	// instead I store most large buffers on the main stack.
	// ideally we should use a .bss section, but vs makes this painful and unportable.
	// typically, ES will make 0 allocations.
	column_color_pool = &local_column_color_pool;
	column_width_pool = &local_column_width_pool;
	column_pool = &local_column_pool;
	secondary_sort_pool = &local_secondary_sort_pool;
	property_unknown_pool = &local_property_unknown_pool;
	column_color_array = &local_column_color_array;
	column_width_array = &local_column_width_array;
	column_array = &local_column_array;
	secondary_sort_array = &local_secondary_sort_array;
	property_unknown_array = &local_property_unknown_array;
	
	perform_search = 1;
	
	_es_output_handle = GetStdHandle(STD_OUTPUT_HANDLE);
	
	_es_cp = GetConsoleCP();

	// get console info.
	
	{
		DWORD mode;

		if (GetConsoleMode(_es_output_handle,&mode))
		{
			CONSOLE_SCREEN_BUFFER_INFO csbi;
			
			if (GetConsoleScreenBufferInfo(_es_output_handle,&csbi))
			{
				_es_console_window_wide = csbi.srWindow.Right - csbi.srWindow.Left + 1;
				_es_console_window_high = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;

				_es_console_window_x = csbi.srWindow.Left;
				_es_console_window_y = csbi.dwCursorPosition.Y;
				
				if ((!_es_console_window_wide) || (!_es_console_window_high))
				{
					_es_console_window_wide = csbi.dwSize.X;
					_es_console_window_high = csbi.dwSize.Y;
				}
				
				_es_console_size_high = csbi.dwSize.Y;
				
				_es_default_attributes = csbi.wAttributes;
			}
		}

		if (GetFileType(_es_output_handle) == FILE_TYPE_CHAR)
		{
			_es_output_is_char = 1;
		}
	}
	
	// load locale settings.
	{
		wchar_t locale_info_wbuf[256];
		
		if (_es_get_locale_info(LOCALE_SGROUPING,locale_info_wbuf))
		{
			const wchar_t *p;
			
			_es_locale_grouping = 0;

			// handle:
			// 2;0
			// 3;0
			// 4;0
			// 3;2;0
			p = locale_info_wbuf;
			while(*p)
			{
				if (*p == '0')
				{
					break;
				}

				if ((*p >= '0') && (*p <= '9'))
				{
					_es_locale_grouping *= 10;
					_es_locale_grouping += *p - '0';
				}
				
				p++;
			}
		}
		
		if (_es_get_locale_info(LOCALE_ILZERO,locale_info_wbuf))
		{
			_es_locale_lzero = wchar_string_to_int(locale_info_wbuf);
		}

		if (_es_get_locale_info(LOCALE_INEGNUMBER,locale_info_wbuf))
		{
			_es_locale_negnumber = wchar_string_to_int(locale_info_wbuf);
		}

		if (_es_get_locale_info(LOCALE_STHOUSAND,locale_info_wbuf))
		{
			wchar_buf_copy_wchar_string(_es_locale_thousand_wcbuf,locale_info_wbuf);
		}
		else
		{
			// default to ","
			wchar_buf_copy_utf8_string(_es_locale_thousand_wcbuf,",");
		}

		if (_es_get_locale_info(LOCALE_SDECIMAL,locale_info_wbuf))
		{
			wchar_buf_copy_wchar_string(_es_locale_decimal_wcbuf,locale_info_wbuf);
		}
		else
		{
			// default to "."
			wchar_buf_copy_utf8_string(_es_locale_decimal_wcbuf,".");
		}
	}
	
	// load default settings.	
	_es_load_settings();

	_es_command_line = GetCommandLine();
	_es_command_line_was_eq = 0;
	
/*
	// code page test
	
	printf("CP %u\n",GetConsoleCP());
	printf("CP output %u\n",GetConsoleOutputCP());
	
	MessageBox(0,command_line,L"command line",MB_OK);
	return 0;
//	printf("command line %S\n",command_line);
*/
	// expect the executable name in the first argv.
	if (_es_command_line)
	{
		_es_get_argv(&argv_wcbuf);
	}
	
	if (_es_command_line)
	{
		_es_get_argv(&argv_wcbuf);
		
		if (_es_command_line)
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
				
				// too many if else statements breaks intellisense.
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"set-run-count"))
				{
					_es_expect_command_argv(&argv_wcbuf);
				
					_es_run_history_size = sizeof(EVERYTHING_IPC_RUN_HISTORY);
					_es_run_history_size = safe_size_add(_es_run_history_size,safe_size_mul_sizeof_wchar(safe_size_add_one(argv_wcbuf.length_in_wchars)));

					_es_run_history_data = mem_alloc(_es_run_history_size);

					wchar_string_copy_wchar_string_n((wchar_t *)(((EVERYTHING_IPC_RUN_HISTORY *)_es_run_history_data)+1),argv_wcbuf.buf,argv_wcbuf.length_in_wchars);
					
					_es_expect_command_argv(&argv_wcbuf);
					
					((EVERYTHING_IPC_RUN_HISTORY *)_es_run_history_data)->run_count = wchar_string_to_int(argv_wcbuf.buf);
					_es_run_history_command = EVERYTHING_IPC_COPYDATA_SET_RUN_COUNTW;
					
					goto next_argv;
				}

				if (_es_check_option_utf8_string(argv_wcbuf.buf,"inc-run-count"))
				{
					_es_expect_command_argv(&argv_wcbuf);
					
					_es_run_history_size = (wchar_string_get_length_in_wchars(argv_wcbuf.buf) + 1) * sizeof(wchar_t);
					_es_run_history_data = wchar_string_alloc_wchar_string_n(argv_wcbuf.buf,argv_wcbuf.length_in_wchars);
					_es_run_history_command = EVERYTHING_IPC_COPYDATA_INC_RUN_COUNTW;

					goto next_argv;
				}

				if (_es_check_option_utf8_string(argv_wcbuf.buf,"get-run-count"))
				{
					_es_expect_command_argv(&argv_wcbuf);

					_es_run_history_size = (wchar_string_get_length_in_wchars(argv_wcbuf.buf) + 1) * sizeof(wchar_t);
					_es_run_history_data = wchar_string_alloc_wchar_string_n(argv_wcbuf.buf,argv_wcbuf.length_in_wchars);
					_es_run_history_command = EVERYTHING_IPC_COPYDATA_GET_RUN_COUNTW;

					goto next_argv;
				}

				if (_es_check_option_utf8_string(argv_wcbuf.buf,"get-folder-size"))
				{
					_es_expect_command_argv(&argv_wcbuf);
					
					get_folder_size_filename = wchar_string_alloc_wchar_string_n(argv_wcbuf.buf,argv_wcbuf.length_in_wchars);

					goto next_argv;
				}

				if ((_es_check_option_utf8_string(argv_wcbuf.buf,"r")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"regex")))
				{
					_es_expect_argv(&argv_wcbuf);
					
					if (search_wcbuf.length_in_wchars)
					{
						wchar_buf_cat_wchar(&search_wcbuf,' ');
					}
							
					wchar_buf_cat_utf8_string(&search_wcbuf,"regex:");
					wchar_buf_cat_wchar_string_n(&search_wcbuf,argv_wcbuf.buf,argv_wcbuf.length_in_wchars);

					goto next_argv;
				}

				if ((_es_check_option_utf8_string(argv_wcbuf.buf,"i")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"case")))
				{
					_es_match_case = 1;

					goto next_argv;
				}

				if ((_es_check_option_utf8_string(argv_wcbuf.buf,"no-i")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"no-case")))
				{
					_es_match_case = 0;

					goto next_argv;
				}

				if ((_es_check_option_utf8_string(argv_wcbuf.buf,"a")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"diacritics")))
				{
					_es_match_diacritics = 1;

					goto next_argv;
				}

				if ((_es_check_option_utf8_string(argv_wcbuf.buf,"no-a")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"no-diacritics")))
				{
					_es_match_diacritics = 0;

					goto next_argv;
				}

				if (_es_check_option_utf8_string(argv_wcbuf.buf,"prefix"))
				{
					_es_match_prefix = 1;

					goto next_argv;
				}

				if (_es_check_option_utf8_string(argv_wcbuf.buf,"no-prefix"))
				{
					_es_match_prefix = 0;

					goto next_argv;
				}

				if (_es_check_option_utf8_string(argv_wcbuf.buf,"suffix"))
				{
					_es_match_suffix = 1;

					goto next_argv;
				}

				if (_es_check_option_utf8_string(argv_wcbuf.buf,"no-suffix"))
				{
					_es_match_suffix = 0;

					goto next_argv;
				}

				if ((_es_check_option_utf8_string(argv_wcbuf.buf,"ignore-punctuation")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"ignore-punc")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"no-punctuation")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"no-punc")))
				{
					_es_ignore_punctuation = 1;

					goto next_argv;
				}

				if ((_es_check_option_utf8_string(argv_wcbuf.buf,"no-ignore-punctuation")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"no-ignore-punc")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"punctuation")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"punc")))
				{
					_es_ignore_punctuation = 0;

					goto next_argv;
				}

				if ((_es_check_option_utf8_string(argv_wcbuf.buf,"ignore-white-space")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"ignore-ws")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"no-white-space")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"no-ws")))
				{
					_es_ignore_whitespace = 1;

					goto next_argv;
				}

				if ((_es_check_option_utf8_string(argv_wcbuf.buf,"no-ignore-white-space")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"no-ignore-ws")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"white-space")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"ws")))
				{
					_es_ignore_whitespace = 0;

					goto next_argv;
				}

				if (_es_check_option_utf8_string(argv_wcbuf.buf,"instance"))
				{
					_es_expect_command_argv(&argv_wcbuf);
					
					if (argv_wcbuf.length_in_wchars)
					{
						wchar_buf_copy_wchar_string_n(es_instance_name_wcbuf,argv_wcbuf.buf,argv_wcbuf.length_in_wchars);
					}

					goto next_argv;
				}

				if ((_es_check_option_utf8_string(argv_wcbuf.buf,"exit")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"quit")))
				{
					_es_exit_everything = 1;

					goto next_argv;
				}

				if ((_es_check_option_utf8_string(argv_wcbuf.buf,"re-index")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"re-build")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"update")))
				{
					es_reindex = 1;

					goto next_argv;
				}

				if (_es_check_option_utf8_string(argv_wcbuf.buf,"save-db"))
				{
					_es_save_db = 1;

					goto next_argv;
				}

				if (_es_check_option_utf8_string(argv_wcbuf.buf,"highlight-color"))
				{
					_es_expect_command_argv(&argv_wcbuf);
					
					_es_highlight_color = wchar_string_to_int(argv_wcbuf.buf);

					goto next_argv;
				}

				if (_es_check_option_utf8_string(argv_wcbuf.buf,"highlight"))
				{
					_es_highlight = 1;

					goto next_argv;
				}

				if (_es_check_option_utf8_string(argv_wcbuf.buf,"no-highlight"))
				{
					_es_highlight = 0;

					goto next_argv;
				}

				if (_es_check_option_utf8_string(argv_wcbuf.buf,"m3u"))
				{
					_es_export_type = _ES_EXPORT_TYPE_M3U;

					goto next_argv;
				}

				if (_es_check_option_utf8_string(argv_wcbuf.buf,"export-m3u"))
				{
					_es_expect_command_argv(&argv_wcbuf);
					
					_es_export_file = os_create_file(argv_wcbuf.buf);
					if (_es_export_file != INVALID_HANDLE_VALUE)
					{
						_es_export_type = _ES_EXPORT_TYPE_M3U;
					}
					else
					{
						es_fatal(ES_ERROR_CREATE_FILE);
					}

					goto next_argv;
				}

				if (_es_check_option_utf8_string(argv_wcbuf.buf,"m3u8"))
				{
					_es_export_type = _ES_EXPORT_TYPE_M3U8;

					goto next_argv;
				}

				if (_es_check_option_utf8_string(argv_wcbuf.buf,"export-m3u8"))
				{
					_es_expect_command_argv(&argv_wcbuf);
					
					_es_export_file = os_create_file(argv_wcbuf.buf);
					if (_es_export_file != INVALID_HANDLE_VALUE)
					{
						_es_export_type = _ES_EXPORT_TYPE_M3U8;
					}
					else
					{
						es_fatal(ES_ERROR_CREATE_FILE);
					}

					goto next_argv;
				}

				if (_es_check_option_utf8_string(argv_wcbuf.buf,"csv"))
				{
					_es_export_type = _ES_EXPORT_TYPE_CSV;

					goto next_argv;
				}

				if (_es_check_option_utf8_string(argv_wcbuf.buf,"tsv"))
				{
					_es_export_type = _ES_EXPORT_TYPE_TSV;

					goto next_argv;
				}

				if (_es_check_option_utf8_string(argv_wcbuf.buf,"json"))
				{
					_es_export_type = _ES_EXPORT_TYPE_JSON;

					goto next_argv;
				}

				if (_es_check_option_utf8_string(argv_wcbuf.buf,"no-format"))
				{
					_es_export_type = _ES_EXPORT_TYPE_NOFORMAT;

					goto next_argv;
				}

				if (_es_check_option_utf8_string(argv_wcbuf.buf,"export-csv"))
				{
					_es_expect_command_argv(&argv_wcbuf);
					
					_es_export_file = os_create_file(argv_wcbuf.buf);
					if (_es_export_file != INVALID_HANDLE_VALUE)
					{
						_es_export_type = _ES_EXPORT_TYPE_CSV;
					}
					else
					{
						es_fatal(ES_ERROR_CREATE_FILE);
					}

					goto next_argv;
				}

				if (_es_check_option_utf8_string(argv_wcbuf.buf,"export-tsv"))
				{
					_es_expect_command_argv(&argv_wcbuf);
					
					_es_export_file = os_create_file(argv_wcbuf.buf);
					if (_es_export_file != INVALID_HANDLE_VALUE)
					{
						_es_export_type = _ES_EXPORT_TYPE_TSV;
					}
					else
					{
						es_fatal(ES_ERROR_CREATE_FILE);
					}

					goto next_argv;
				}

				if (_es_check_option_utf8_string(argv_wcbuf.buf,"export-json"))
				{
					_es_expect_command_argv(&argv_wcbuf);
					
					_es_export_file = os_create_file(argv_wcbuf.buf);
					if (_es_export_file != INVALID_HANDLE_VALUE)
					{
						_es_export_type = _ES_EXPORT_TYPE_JSON;
					}
					else
					{
						es_fatal(ES_ERROR_CREATE_FILE);
					}

					goto next_argv;
				}

				if (_es_check_option_utf8_string(argv_wcbuf.buf,"efu"))
				{
					_es_export_type = _ES_EXPORT_TYPE_EFU;

					goto next_argv;
				}

				if (_es_check_option_utf8_string(argv_wcbuf.buf,"export-efu"))
				{
					_es_expect_command_argv(&argv_wcbuf);
					
					_es_export_file = os_create_file(argv_wcbuf.buf);
					if (_es_export_file != INVALID_HANDLE_VALUE)
					{
						_es_export_type = _ES_EXPORT_TYPE_EFU;
					}
					else
					{
						es_fatal(ES_ERROR_CREATE_FILE);
					}

					goto next_argv;
				}

				if (_es_check_option_utf8_string(argv_wcbuf.buf,"txt"))
				{
					_es_export_type = _ES_EXPORT_TYPE_TXT;

					goto next_argv;
				}

				if (_es_check_option_utf8_string(argv_wcbuf.buf,"export-txt"))
				{
					_es_expect_command_argv(&argv_wcbuf);
					
					_es_export_file = os_create_file(argv_wcbuf.buf);
					if (_es_export_file != INVALID_HANDLE_VALUE)
					{
						_es_export_type = _ES_EXPORT_TYPE_TXT;
					}
					else
					{
						es_fatal(ES_ERROR_CREATE_FILE);
					}

					goto next_argv;
				}

				if (_es_check_option_utf8_string(argv_wcbuf.buf,"cp"))
				{
					_es_expect_command_argv(&argv_wcbuf);
					
					_es_cp = wchar_string_to_int(argv_wcbuf.buf);

					goto next_argv;
				}

				if ((_es_check_option_utf8_string(argv_wcbuf.buf,"console-wide")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"console-width")))
				{
					int console_wide;
					CONSOLE_SCREEN_BUFFER_INFO csbi;
					
					_es_expect_command_argv(&argv_wcbuf);
					
					console_wide = wchar_string_to_int(argv_wcbuf.buf);
					
					if (GetConsoleScreenBufferInfo(_es_output_handle,&csbi))
					{
						COORD console_size;
						
						console_size.X = console_wide;
						console_size.Y = csbi.dwSize.Y;
						
						if (SetConsoleScreenBufferSize(_es_output_handle,console_size))
						{
							//_es_console_wide = console_size.X;
						}
					}

					goto next_argv;
				}

				if ((_es_check_option_utf8_string(argv_wcbuf.buf,"console-high")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"console-height")))
				{
					int console_high;
					CONSOLE_SCREEN_BUFFER_INFO csbi;
					
					_es_expect_command_argv(&argv_wcbuf);
					
					console_high = wchar_string_to_int(argv_wcbuf.buf);
					
					if (GetConsoleScreenBufferInfo(_es_output_handle,&csbi))
					{
						COORD console_size;
						
						console_size.X = csbi.dwSize.X;
						console_size.Y = console_high;
						
						if (SetConsoleScreenBufferSize(_es_output_handle,console_size))
						{
							//_es_console_high = console_size.X;
						}
					}

					goto next_argv;
				}

				if ((_es_check_option_utf8_string(argv_wcbuf.buf,"console-window-wide")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"console-window-width")))
				{
					int window_wide;
					CONSOLE_SCREEN_BUFFER_INFO csbi;
					
					_es_expect_command_argv(&argv_wcbuf);
					
					window_wide = wchar_string_to_int(argv_wcbuf.buf);
					
					if (GetConsoleScreenBufferInfo(_es_output_handle,&csbi))
					{
						SMALL_RECT small_rect;
						
						small_rect.Left = 0;
						small_rect.Top = 0;
						small_rect.Right = window_wide - 1;
						small_rect.Bottom = csbi.srWindow.Bottom - csbi.srWindow.Top;
						
						if (SetConsoleWindowInfo(_es_output_handle,TRUE,&small_rect))
						{
							_es_console_window_wide = window_wide;
						}
						else
						{
							debug_error_printf("SetConsoleWindowInfo failed %u\n",GetLastError());
						}
					}

					goto next_argv;
				}

				if ((_es_check_option_utf8_string(argv_wcbuf.buf,"console-window-high")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"console-window-height")))
				{
					int window_high;
					CONSOLE_SCREEN_BUFFER_INFO csbi;
					
					_es_expect_command_argv(&argv_wcbuf);
					
					window_high = wchar_string_to_int(argv_wcbuf.buf);
					
					if (GetConsoleScreenBufferInfo(_es_output_handle,&csbi))
					{
						SMALL_RECT small_rect;
						
						small_rect.Left = 0;
						small_rect.Top = 0;
						small_rect.Right = csbi.srWindow.Right - csbi.srWindow.Left;
						small_rect.Bottom = window_high - 1;
						
						if (SetConsoleWindowInfo(_es_output_handle,TRUE,&small_rect))
						{
							_es_console_window_high = window_high;
						}
						else
						{
							debug_error_printf("SetConsoleWindowInfo failed %u\n",GetLastError());
						}
					}

					goto next_argv;
				}

				if ((_es_check_option_utf8_string(argv_wcbuf.buf,"w")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"ww")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"whole-word")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"whole-words")))
				{
					_es_match_whole_word = 1;

					goto next_argv;
				}

				if ((_es_check_option_utf8_string(argv_wcbuf.buf,"no-w")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"no-ww")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"no-whole-word")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"no-whole-words")))
				{
					_es_match_whole_word = 0;

					goto next_argv;
				}

				if ((_es_check_option_utf8_string(argv_wcbuf.buf,"p")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"match-path")))
				{
					_es_match_path = 1;

					goto next_argv;
				}

				if ((_es_check_option_utf8_string(argv_wcbuf.buf,"no-p")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"no-match-path")))
				{
					_es_match_path = 0;

					goto next_argv;
				}

				if ((_es_check_option_utf8_string(argv_wcbuf.buf,"file-name-width")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"file-name-wide")))
				{
					_es_expect_command_argv(&argv_wcbuf);

					column_width_set(EVERYTHING3_PROPERTY_ID_PATH_AND_NAME,wchar_string_to_int(argv_wcbuf.buf));

					goto next_argv;
				}

				if ((_es_check_option_utf8_string(argv_wcbuf.buf,"name-width")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"name-wide")))
				{
					_es_expect_command_argv(&argv_wcbuf);
					
					column_width_set(EVERYTHING3_PROPERTY_ID_NAME,wchar_string_to_int(argv_wcbuf.buf));

					goto next_argv;
				}

				if ((_es_check_option_utf8_string(argv_wcbuf.buf,"path-width")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"path-wide")))
				{
					_es_expect_command_argv(&argv_wcbuf);
					
					column_width_set(EVERYTHING3_PROPERTY_ID_PATH,wchar_string_to_int(argv_wcbuf.buf));

					goto next_argv;
				}

				if ((_es_check_option_utf8_string(argv_wcbuf.buf,"extension-width")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"extension-wide")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"ext-width")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"ext-wide")))
				{
					_es_expect_command_argv(&argv_wcbuf);
					
					column_width_set(EVERYTHING3_PROPERTY_ID_EXTENSION,wchar_string_to_int(argv_wcbuf.buf));

					goto next_argv;
				}
				
				if ((_es_check_option_utf8_string(argv_wcbuf.buf,"size-width")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"size-wide")))
				{
					_es_expect_command_argv(&argv_wcbuf);
					
					column_width_set(EVERYTHING3_PROPERTY_ID_SIZE,wchar_string_to_int(argv_wcbuf.buf));

					goto next_argv;
				}
				
				if ((_es_check_option_utf8_string(argv_wcbuf.buf,"date-created-width")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"date-created-wide")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"dc-width")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"dc-wide")))
				{
					_es_expect_command_argv(&argv_wcbuf);
					
					column_width_set(EVERYTHING3_PROPERTY_ID_DATE_CREATED,wchar_string_to_int(argv_wcbuf.buf));

					goto next_argv;
				}
				
				if ((_es_check_option_utf8_string(argv_wcbuf.buf,"date-modified-width")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"date-modified-wide")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"dm-width")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"dm-wide")))
				{
					_es_expect_command_argv(&argv_wcbuf);
					
					column_width_set(EVERYTHING3_PROPERTY_ID_DATE_MODIFIED,wchar_string_to_int(argv_wcbuf.buf));

					goto next_argv;
				}
				
				if ((_es_check_option_utf8_string(argv_wcbuf.buf,"date-accessed-width")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"date-accessed-wide")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"da-width")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"da-wide")))
				{
					_es_expect_command_argv(&argv_wcbuf);
					
					column_width_set(EVERYTHING3_PROPERTY_ID_DATE_ACCESSED,wchar_string_to_int(argv_wcbuf.buf));

					goto next_argv;
				}
				
				if ((_es_check_option_utf8_string(argv_wcbuf.buf,"attributes-width")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"attributes-wide")))
				{
					_es_expect_command_argv(&argv_wcbuf);
					
					column_width_set(EVERYTHING3_PROPERTY_ID_ATTRIBUTES,wchar_string_to_int(argv_wcbuf.buf));

					goto next_argv;
				}
				
				if ((_es_check_option_utf8_string(argv_wcbuf.buf,"file-list-file-name-width")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"file-list-file-name-wide")))
				{
					_es_expect_command_argv(&argv_wcbuf);
					
					column_width_set(EVERYTHING3_PROPERTY_ID_FILE_LIST_NAME,wchar_string_to_int(argv_wcbuf.buf));

					goto next_argv;
				}
				
				if ((_es_check_option_utf8_string(argv_wcbuf.buf,"run-count-width")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"run-count-wide")))
				{
					_es_expect_command_argv(&argv_wcbuf);
					
					column_width_set(EVERYTHING3_PROPERTY_ID_RUN_COUNT,wchar_string_to_int(argv_wcbuf.buf));

					goto next_argv;
				}
				
				if ((_es_check_option_utf8_string(argv_wcbuf.buf,"date-run-width")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"date-run-wide")))
				{
					_es_expect_command_argv(&argv_wcbuf);
					
					column_width_set(EVERYTHING3_PROPERTY_ID_DATE_RUN,wchar_string_to_int(argv_wcbuf.buf));

					goto next_argv;
				}
				
				if ((_es_check_option_utf8_string(argv_wcbuf.buf,"date-recently-changed-width")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"date-recently-changed-wide")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"rc-width")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"rc-wide")))
				{
					_es_expect_command_argv(&argv_wcbuf);
					
					column_width_set(EVERYTHING3_PROPERTY_ID_DATE_RECENTLY_CHANGED,wchar_string_to_int(argv_wcbuf.buf));

					goto next_argv;
				}
				
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"size-leading-zero"))
				{
					_es_size_leading_zero = 1;

					goto next_argv;
				}
				
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"no-size-leading-zero"))
				{
					_es_size_leading_zero = 0;

					goto next_argv;
				}
				
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"run-count-leading-zero"))
				{
					_es_run_count_leading_zero = 1;

					goto next_argv;
				}
				
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"no-run-count-leading-zero"))
				{
					_es_run_count_leading_zero = 0;

					goto next_argv;
				}
				
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"no-digit-grouping"))
				{
					_es_digit_grouping = 0;

					goto next_argv;
				}
				
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"digit-grouping"))
				{
					_es_digit_grouping = 1;

					goto next_argv;
				}
				
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"locale-grouping"))
				{
					_es_expect_command_argv(&argv_wcbuf);
					
					_es_locale_grouping = wchar_string_to_int(argv_wcbuf.buf);

					goto next_argv;
				}
				
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"locale-thousand"))
				{
					_es_expect_command_argv(&argv_wcbuf);
					
					wchar_buf_copy_wchar_string_n(_es_locale_thousand_wcbuf,argv_wcbuf.buf,argv_wcbuf.length_in_wchars);

					goto next_argv;
				}
				
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"locale-decimal"))
				{
					_es_expect_command_argv(&argv_wcbuf);
					
					wchar_buf_copy_wchar_string_n(_es_locale_decimal_wcbuf,argv_wcbuf.buf,argv_wcbuf.length_in_wchars);

					goto next_argv;
				}
				
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"locale-lzero"))
				{
					_es_expect_command_argv(&argv_wcbuf);
					
					_es_locale_lzero = wchar_string_to_int(argv_wcbuf.buf);

					goto next_argv;
				}
				
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"locale-negnumber"))
				{
					_es_expect_command_argv(&argv_wcbuf);
					
					_es_locale_negnumber = wchar_string_to_int(argv_wcbuf.buf);

					goto next_argv;
				}
				
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"size-format"))
				{
					_es_expect_command_argv(&argv_wcbuf);
					
					_es_size_format = wchar_string_to_int(argv_wcbuf.buf);

					goto next_argv;
				}
				
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"date-format"))
				{
					_es_expect_command_argv(&argv_wcbuf);

					_es_display_date_format = wchar_string_to_int(argv_wcbuf.buf);

					goto next_argv;
				}
							
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"export-date-format"))
				{
					_es_expect_command_argv(&argv_wcbuf);

					_es_export_date_format = wchar_string_to_int(argv_wcbuf.buf);

					goto next_argv;
				}
							
				if ((_es_check_option_utf8_string(argv_wcbuf.buf,"pause")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"more")))
				{
					_es_pause = 1;

					goto next_argv;
				}
				
				if ((_es_check_option_utf8_string(argv_wcbuf.buf,"no-pause")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"no-more")))
				{
					_es_pause = 0;

					goto next_argv;
				}
				
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"empty-search-help"))
				{
					_es_empty_search_help = 1;
					_es_hide_empty_search_results = 0;

					goto next_argv;
				}
				
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"no-empty-search-help"))
				{
					_es_empty_search_help = 0;

					goto next_argv;
				}
				
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"hide-empty-search-results"))
				{
					_es_hide_empty_search_results = 1;
					_es_empty_search_help = 0;

					goto next_argv;
				}
				
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"no-hide-empty-search-results"))
				{
					_es_hide_empty_search_results = 0;

					goto next_argv;
				}
				
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"save-settings"))
				{
					_es_save = 1;

					goto next_argv;
				}
				
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

					goto next_argv;
				}
				
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

					goto next_argv;
				}
				
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

					goto next_argv;
				}
				
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"sort-ascending"))
				{
					_es_primary_sort_ascending = 1;

					goto next_argv;
				}
				
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"sort-descending"))
				{
					_es_primary_sort_ascending = -1;

					goto next_argv;
				}
				
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"sort"))
				{	
					_es_expect_command_argv(&argv_wcbuf);
					
					_es_set_sort_list(argv_wcbuf.buf,0,1);

					goto next_argv;
				}
				
				if ((_es_check_option_utf8_string(argv_wcbuf.buf,"columns")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"set-columns")))
				{	
					_es_expect_command_argv(&argv_wcbuf);
					
					_es_set_columns(argv_wcbuf.buf,0,0,TRUE);

					goto next_argv;
				}
				
				// don't pass addcolumns: to Everything
				// we might as well process these ourselfs..
				{
					const wchar_t *match_p;
					
					match_p = _es_parse_command_line_option_name(argv_wcbuf.buf,"columns:");
					
					if (match_p)
					{	
						_es_set_columns(match_p,0,0,TRUE);

						goto next_argv;
					}
				}
				
				if ((_es_check_option_utf8_string(argv_wcbuf.buf,"add-columns")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"add-column")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"add-col")))
				{	
					_es_expect_command_argv(&argv_wcbuf);
					
					_es_set_columns(argv_wcbuf.buf,1,0,TRUE);

					goto next_argv;
				}
				
				// don't pass addcolumns: to Everything
				// we might as well process these ourselfs..
				{
					const wchar_t *match_p;
					
					match_p = _es_parse_command_line_option_name(argv_wcbuf.buf,"add-columns:");
					if (!match_p)
					{
						match_p = _es_parse_command_line_option_name(argv_wcbuf.buf,"add-column:");

						if (!match_p)
						{
							match_p = _es_parse_command_line_option_name(argv_wcbuf.buf,"add-col:");
						}
					}
					
					if (match_p)
					{	
						_es_set_columns(match_p,1,0,TRUE);

						goto next_argv;
					}
				}
				
				if ((_es_check_option_utf8_string(argv_wcbuf.buf,"remove-columns")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"remove-column")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"remove-col")))
				{	
					_es_expect_command_argv(&argv_wcbuf);
					
					_es_set_columns(argv_wcbuf.buf,2,0,TRUE);

					goto next_argv;
				}
				
				// don't pass addcolumns: to Everything
				// we might as well process these ourselfs..
				{
					const wchar_t *match_p;
					
					match_p = _es_parse_command_line_option_name(argv_wcbuf.buf,"remove-columns:");
					if (!match_p)
					{
						match_p = _es_parse_command_line_option_name(argv_wcbuf.buf,"remove-column:");

						if (!match_p)
						{
							match_p = _es_parse_command_line_option_name(argv_wcbuf.buf,"remove-col:");
						}
					}
					
					if (match_p)
					{	
						_es_set_columns(match_p,2,0,TRUE);

						goto next_argv;
					}
				}
				
				if ((_es_check_option_utf8_string(argv_wcbuf.buf,"column-colors")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"set-column-colors")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"set-column-color")))
				{	
					_es_expect_command_argv(&argv_wcbuf);
					
					_es_set_column_colors(argv_wcbuf.buf,0,TRUE);

					goto next_argv;
				}
				
				if ((_es_check_option_utf8_string(argv_wcbuf.buf,"add-column-colors")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"add-column-color")))
				{	
					_es_expect_command_argv(&argv_wcbuf);
					
					_es_set_column_colors(argv_wcbuf.buf,1,TRUE);

					goto next_argv;
				}
				
				if ((_es_check_option_utf8_string(argv_wcbuf.buf,"remove-column-colors")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"remove-column-color")))
				{	
					_es_expect_command_argv(&argv_wcbuf);
					
					_es_set_column_colors(argv_wcbuf.buf,2,TRUE);

					goto next_argv;
				}
				
				if ((_es_check_option_utf8_string(argv_wcbuf.buf,"column-widths")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"set-column-widths")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"set-column-width")))
				{	
					_es_expect_command_argv(&argv_wcbuf);
					
					_es_set_column_widths(argv_wcbuf.buf,0,TRUE);

					goto next_argv;
				}
				
				if ((_es_check_option_utf8_string(argv_wcbuf.buf,"add-column-widths")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"add-column-width")))
				{	
					_es_expect_command_argv(&argv_wcbuf);
					
					_es_set_column_widths(argv_wcbuf.buf,1,TRUE);

					goto next_argv;
				}
				
				if ((_es_check_option_utf8_string(argv_wcbuf.buf,"remove-column-widths")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"remove-column-width")))
				{	
					_es_expect_command_argv(&argv_wcbuf);
					
					_es_set_column_widths(argv_wcbuf.buf,2,TRUE);

					goto next_argv;
				}
				
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"ipc1"))
				{	
					es_ipc_version = ES_IPC_VERSION_FLAG_IPC1; 

					goto next_argv;
				}
				
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"ipc2"))
				{	
					es_ipc_version = ES_IPC_VERSION_FLAG_IPC2; 

					goto next_argv;
				}
				
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"ipc3"))
				{	
					es_ipc_version = ES_IPC_VERSION_FLAG_IPC3; 

					goto next_argv;
				}
				
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"no-ipc1"))
				{	
					es_ipc_version &= (~ES_IPC_VERSION_FLAG_IPC1); 

					goto next_argv;
				}
				
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"no-ipc2"))
				{	
					es_ipc_version &= (~ES_IPC_VERSION_FLAG_IPC2); 

					goto next_argv;
				}
				
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"no-ipc3"))
				{	
					es_ipc_version &= (~ES_IPC_VERSION_FLAG_IPC3); 

					goto next_argv;
				}
				
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"header"))
				{	
					_es_header = 1; 

					goto next_argv;
				}
				
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"no-header"))
				{	
					_es_header = -1; 

					goto next_argv;
				}
				
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"footer"))
				{	
					_es_footer = 1; 

					goto next_argv;
				}
				
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"no-footer"))
				{	
					_es_footer = -1; 

					goto next_argv;
				}
				
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"double-quote"))
				{	
					_es_double_quote = 1; 
					_es_csv_double_quote = 1;

					goto next_argv;
				}
				
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"no-double-quote"))
				{	
					_es_double_quote = 0; 
					_es_csv_double_quote = 0;

					goto next_argv;
				}
				
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"version"))
				{	
					_es_output_noncell_utf8_string(VERSION_TEXT "\r\n");

					goto exit;
				}
				
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"get-everything-version"))
				{
					_es_mode = _ES_MODE_GET_EVERYTHING_VERSION;

					goto next_argv;
				}
				
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"list-properties"))
				{
					_es_mode = _ES_MODE_LIST_PROPERTIES;

					goto next_argv;
				}
				
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"utf8-bom"))
				{
					_es_utf8_bom = 1;

					goto next_argv;
				}
				
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"no-utf8-bom"))
				{
					_es_utf8_bom = 0;

					goto next_argv;
				}

				if (_es_check_option_utf8_string(argv_wcbuf.buf,"folder-append-path-separator"))
				{
					_es_folder_append_path_separator = 1;

					goto next_argv;
				}
				
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"no-folder-append-path-separator"))
				{
					_es_folder_append_path_separator = -1;

					goto next_argv;
				}
				
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"nul"))
				{
					_es_newline_type = 2;

					goto next_argv;
				}
				
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"lf"))
				{
					_es_newline_type = 1;

					goto next_argv;
				}
				
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"crlf"))
				{
					_es_newline_type = 0;

					goto next_argv;
				}
				
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"on"))
				{	
					_es_primary_sort_property_id = EVERYTHING3_PROPERTY_ID_NAME;
					_es_primary_sort_ascending = 1;
					secondary_sort_clear_all();

					goto next_argv;
				}
				
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"o-n"))
				{	
					_es_primary_sort_property_id = EVERYTHING3_PROPERTY_ID_NAME;
					_es_primary_sort_ascending = -1;
					secondary_sort_clear_all();

					goto next_argv;
				}
				
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"os"))
				{	
					_es_primary_sort_property_id = EVERYTHING3_PROPERTY_ID_SIZE;
					_es_primary_sort_ascending = 1;
					secondary_sort_clear_all();

					goto next_argv;
				}
				
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"o-s"))
				{	
					_es_primary_sort_property_id = EVERYTHING3_PROPERTY_ID_SIZE;
					_es_primary_sort_ascending = -1;
					secondary_sort_clear_all();

					goto next_argv;
				}
				
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"oe"))
				{	
					_es_primary_sort_property_id = EVERYTHING3_PROPERTY_ID_EXTENSION;
					_es_primary_sort_ascending = 1;
					secondary_sort_clear_all();

					goto next_argv;
				}
				
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"o-e"))
				{	
					_es_primary_sort_property_id = EVERYTHING3_PROPERTY_ID_EXTENSION;
					_es_primary_sort_ascending = -1;
					secondary_sort_clear_all();

					goto next_argv;
				}
				
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"od"))
				{	
					_es_primary_sort_property_id = EVERYTHING3_PROPERTY_ID_DATE_MODIFIED;
					_es_primary_sort_ascending = 1;
					secondary_sort_clear_all();

					goto next_argv;
				}
				
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"o-d"))
				{	
					_es_primary_sort_property_id = EVERYTHING3_PROPERTY_ID_DATE_MODIFIED;
					_es_primary_sort_ascending = -1;
					secondary_sort_clear_all();

					goto next_argv;
				}
				
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"s"))
				{
					_es_primary_sort_property_id = EVERYTHING3_PROPERTY_ID_PATH;
					_es_primary_sort_ascending = 1;
					secondary_sort_clear_all();

					goto next_argv;
				}
				
				if ((_es_check_option_utf8_string(argv_wcbuf.buf,"n")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"max-results")))
				{
					_es_expect_command_argv(&argv_wcbuf);
					
					_es_max_results = safe_size_from_uint64(wchar_string_to_uint64(argv_wcbuf.buf));

					goto next_argv;
				}
				
				if ((_es_check_option_utf8_string(argv_wcbuf.buf,"o")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"offset")))
				{
					_es_expect_command_argv(&argv_wcbuf);
					
					_es_offset = safe_size_from_uint64(wchar_string_to_uint64(argv_wcbuf.buf));

					goto next_argv;
				}
				
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"time-out"))
				{
					_es_expect_command_argv(&argv_wcbuf);
					
					es_timeout = (DWORD)wchar_string_to_int(argv_wcbuf.buf);

					goto next_argv;
				}
				
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"no-time-out"))
				{
					es_timeout = 0;

					goto next_argv;
				}
			
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"ad"))
				{	
					// add folder:
					_es_append_filter(&filter_wcbuf,"folder:");

					goto next_argv;
				}
				
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"a-d"))
				{	
					// add folder:
					_es_append_filter(&filter_wcbuf,"file:");

					goto next_argv;
				}
				
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"get-result-count"))
				{
					_es_get_result_count = 1;

					goto next_argv;
				}
				
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"get-total-size"))
				{
					_es_get_total_size = 1;

					goto next_argv;
				}
				
				if ((_es_check_option_utf8_string(argv_wcbuf.buf,"no-result-error")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"no-results-error")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"error-on-no-results")))
				{
					_es_no_result_error = 1;

					goto next_argv;
				}
				
				if ((_es_check_option_utf8_string(argv_wcbuf.buf,"q")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"search")))
				{
					// this removes quotes from the search string.
					_es_expect_command_argv(&argv_wcbuf);
							
					if (search_wcbuf.length_in_wchars)
					{
						wchar_buf_cat_wchar(&search_wcbuf,' ');
					}

					wchar_buf_cat_wchar_string(&search_wcbuf,argv_wcbuf.buf);

					goto next_argv;
				}
				
				if ((_es_check_option_utf8_string(argv_wcbuf.buf,"q*")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"search*")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"s*")))
				{
					// eat the rest.
					// this would do the same as a stop parsing switches command line option
					// like 7zip --
					// or powershell --%
					// this doesn't remove quotes.
					// -- will still parse quotes.
					_es_command_line = wchar_string_skip_ws(_es_command_line);
					_es_command_line_was_eq = 0;
							
					if (search_wcbuf.length_in_wchars)
					{
						wchar_buf_cat_wchar(&search_wcbuf,' ');
					}

					wchar_buf_cat_wchar_string(&search_wcbuf,_es_command_line);
					
					// we are done, break.
					break;
				}

				if ((_es_check_option_utf8_string(argv_wcbuf.buf,"j")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"journal")))
				{
					_es_mode = _ES_MODE_READ_JOURNAL;
					
					goto next_argv;
				}
				
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"get-journal-id"))
				{
					_es_mode = _ES_MODE_GET_JOURNAL_ID;
					
					goto next_argv;
				}
				
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"get-journal-pos"))
				{
					_es_mode = _ES_MODE_GET_JOURNAL_POS;
					
					goto next_argv;
				}
				
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"watch"))
				{
					_es_watch = TRUE;
					
					goto next_argv;
				}
				
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"from-journal-id"))
				{
					_es_expect_command_argv(&argv_wcbuf);
					
					_es_from_journal_id = wchar_string_to_uint64(argv_wcbuf.buf);
					
					if (!_es_mode)
					{
						_es_mode = _ES_MODE_READ_JOURNAL;
					}
					
					goto next_argv;
				}
				
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"from-change-id"))
				{
					_es_expect_command_argv(&argv_wcbuf);
					
					_es_from_change_id = wchar_string_to_uint64(argv_wcbuf.buf);
					
					// 1-based to 0-based.
					if (_es_from_change_id)
					{
						_es_from_change_id--;
					}
					else
					{
						// 0 == use first available.
						_es_from_change_id = ES_UINT64_MAX;
					}
					
					if (!_es_mode)
					{
						_es_mode = _ES_MODE_READ_JOURNAL;
					}
					
					goto next_argv;
				}
				
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"from-journal-pos"))
				{
					_es_expect_command_argv(&argv_wcbuf);
					
					_es_from_journal_id = wchar_string_to_uint64(argv_wcbuf.buf);
					
					_es_expect_command_argv(&argv_wcbuf);
					
					_es_from_change_id = wchar_string_to_uint64(argv_wcbuf.buf);
					
					// 1-based to 0-based.
					if (_es_from_change_id)
					{
						_es_from_change_id--;
					}
					else
					{
						// 0 == use first available.
						_es_from_change_id = ES_UINT64_MAX;
					}
					
					if (!_es_mode)
					{
						_es_mode = _ES_MODE_READ_JOURNAL;
					}
					
					goto next_argv;
				}
				
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"after-journal-pos"))
				{
					_es_expect_command_argv(&argv_wcbuf);
					
					_es_from_journal_id = wchar_string_to_uint64(argv_wcbuf.buf);
					
					_es_expect_command_argv(&argv_wcbuf);
					
					_es_from_change_id = wchar_string_to_uint64(argv_wcbuf.buf);
					
					// 1-based to 0-based.
					if (_es_from_change_id)
					{
						// convert to 0-based and add one
						// in other words, do nothing.
					}
					else
					{
						// change id must be valid.
						_es_bad_switch_param("Invalid change-id: 0\n");
					}
					
					if (!_es_mode)
					{
						_es_mode = _ES_MODE_READ_JOURNAL;
					}
					
					goto next_argv;
				}
								
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"from-date"))
				{
					_es_expect_command_argv(&argv_wcbuf);
					
					if (!_es_mode)
					{
						_es_mode = _ES_MODE_READ_JOURNAL;
					}
					
					_es_from_date = _es_parse_date(argv_wcbuf.buf);
					
					goto next_argv;
				}
				
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"from-yesterday"))
				{
					_es_from_date = _es_get_yesterday_filetime();
					
					if (!_es_mode)
					{
						_es_mode = _ES_MODE_READ_JOURNAL;
					}
					
					goto next_argv;
				}
				
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"from-today"))
				{
					_es_from_date = _es_get_today_filetime();
					
					if (!_es_mode)
					{
						_es_mode = _ES_MODE_READ_JOURNAL;
					}
					
					goto next_argv;
				}
				
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"from-now"))
				{
					_es_from_now = TRUE;
	
					if (!_es_mode)
					{
						_es_mode = _ES_MODE_READ_JOURNAL;
					}
					
					goto next_argv;
				}
				
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"to-journal-id"))
				{
					_es_expect_command_argv(&argv_wcbuf);
					
					_es_to_journal_id = wchar_string_to_uint64(argv_wcbuf.buf);
					
					if (!_es_mode)
					{
						_es_mode = _ES_MODE_READ_JOURNAL;
					}
					
					goto next_argv;
				}
				
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"to-change-id"))
				{
					_es_expect_command_argv(&argv_wcbuf);
					
					_es_to_change_id = wchar_string_to_uint64(argv_wcbuf.buf);
					
					// 1-based to 0-based.
					if (_es_to_change_id)
					{
						_es_to_change_id--;
					}
					
					if (!_es_mode)
					{
						_es_mode = _ES_MODE_READ_JOURNAL;
					}
					
					goto next_argv;
				}
				
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"to-journal-pos"))
				{
					_es_expect_command_argv(&argv_wcbuf);
					
					_es_to_journal_id = wchar_string_to_uint64(argv_wcbuf.buf);
					
					_es_expect_command_argv(&argv_wcbuf);
					
					_es_to_change_id = wchar_string_to_uint64(argv_wcbuf.buf);
					
					// 1-based to 0-based.
					if (_es_to_change_id)
					{
						_es_to_change_id--;
					}
					else
					{
						// 0 == use first available.
						_es_to_change_id = ES_UINT64_MAX;
					}
					
					if (!_es_mode)
					{
						_es_mode = _ES_MODE_READ_JOURNAL;
					}
					
					goto next_argv;
				}
				
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"to-date"))
				{
					_es_expect_command_argv(&argv_wcbuf);
					
					_es_to_date = _es_parse_date(argv_wcbuf.buf);
					
					if (!_es_mode)
					{
						_es_mode = _ES_MODE_READ_JOURNAL;
					}
					
					goto next_argv;
				}
				
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"to-now"))
				{
					_es_to_now = TRUE;
					
					if (!_es_mode)
					{
						_es_mode = _ES_MODE_READ_JOURNAL;
					}
					
					goto next_argv;
				}
				
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"to-today"))
				{
					_es_to_date = _es_get_today_filetime();
					
					if (!_es_mode)
					{
						_es_mode = _ES_MODE_READ_JOURNAL;
					}
					
					goto next_argv;
				}
				
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"to-tomorrow"))
				{
					_es_to_date = _es_get_tomorrow_filetime();
					
					if (!_es_mode)
					{
						_es_mode = _ES_MODE_READ_JOURNAL;
					}
					
					goto next_argv;
				}
				
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"action-filter"))
				{
					_es_expect_command_argv(&argv_wcbuf);
					
					_es_action_filter = _es_parse_journal_action_filter(argv_wcbuf.buf);
					
					if (!_es_mode)
					{
						_es_mode = _ES_MODE_READ_JOURNAL;
					}
					
					goto next_argv;
				}
				
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"changed-today"))
				{
					_es_from_date = _es_get_today_filetime();
					_es_to_date = _es_get_tomorrow_filetime();
					
					if (!_es_mode)
					{
						_es_mode = _ES_MODE_READ_JOURNAL;
					}
					
					goto next_argv;
				}
				
				if (_es_check_option_utf8_string(argv_wcbuf.buf,"changed-yesterday"))
				{
					_es_from_date = _es_get_yesterday_filetime();
					_es_to_date = _es_get_today_filetime();
					
					if (!_es_mode)
					{
						_es_mode = _ES_MODE_READ_JOURNAL;
					}
					
					goto next_argv;
				}
				
				if ((_es_check_option_utf8_string(argv_wcbuf.buf,"?")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"help")) || (_es_check_option_utf8_string(argv_wcbuf.buf,"h")))
				{
					// user requested help
					_es_help();
					
					goto exit;
				}
				
				if (_es_check_color_param(argv_wcbuf.buf,&property_id))
				{
					_es_expect_command_argv(&argv_wcbuf);
					
					column_color_set(property_id,wchar_string_to_int(argv_wcbuf.buf));

					goto next_argv;
				}

				if (_es_check_column_param(argv_wcbuf.buf))
				{
					goto next_argv;
				}				

				if (_es_check_sorts(argv_wcbuf.buf))
				{
					goto next_argv;
				}

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
								_es_bad_switch_param("Unknown attribute: %C\n",attrib_ch);
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
								_es_bad_switch_param("Unknown attribute: %C\n",attrib_ch);
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

					goto next_argv;
				}

				if ((argv_wcbuf.length_in_wchars) && (argv_wcbuf.buf[0] == '-') && (argv_wcbuf.buf[1] == '-'))
				{
					// no more switches.
					// treat the rest as a search.
					for(;;)
					{
						_es_get_argv(&argv_wcbuf);
						if (!_es_command_line)
						{
							break;
						}

						if (search_wcbuf.length_in_wchars)
						{
							wchar_buf_cat_wchar(&search_wcbuf,' ');
						}

						// copy append to search
						wchar_buf_cat_wchar_string(&search_wcbuf,argv_wcbuf.buf);
					}
					
					// we are done, break.
					break;
				}

				if ((argv_wcbuf.buf[0] == '-') && (!_es_is_literal_switch(argv_wcbuf.buf)))
				{
					// unknown command
					// allow /downloads to search for "\downloads" for now
					es_fatal(ES_ERROR_UNKNOWN_SWITCH);
				}

				// add as a search.
				if ((_es_mode == _ES_MODE_READ_JOURNAL) && (!argv_wcbuf.length_in_wchars))
				{
					// dont append empty searches.
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

next_argv:

				_es_get_argv(&argv_wcbuf);
				if (!_es_command_line)
				{
					break;
				}
			}
		}
	}
	
	// save settings.
	if (_es_save)
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
	
	// setup export
	if ((_es_export_type != _ES_EXPORT_TYPE_NONE) || (_es_export_type != _ES_EXPORT_TYPE_NOFORMAT))
	{
		// use export format, which might differ to the display format.
		// for display we typically want YYYY-MM-DD
		// for export we typically want raw filetimes.
		_es_date_format = _es_export_date_format;
		
		// disable pause
		if (_es_export_file != INVALID_HANDLE_VALUE)
		{
			_es_pause = 0;
		}
	
		// remove highlighting.
		if (_es_export_file != INVALID_HANDLE_VALUE)
		{
			_es_highlight = 0;
		}
		
		if (_es_export_file != INVALID_HANDLE_VALUE)
		{
			_es_export_buf = mem_alloc(_ES_EXPORT_BUF_SIZE);
			_es_export_p = _es_export_buf;
			_es_export_avail = _ES_EXPORT_BUF_SIZE;
		}
	}
	else
	{
		// use display format, which might differ to the export format.
		// for display we typically want YYYY-MM-DD
		// for export we typically want raw filetimes.
		_es_date_format = _es_display_date_format;
	}		

	// export BOM
	if (_es_utf8_bom)
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

	// resolve _es_folder_append_path_separator.
	// do this AFTER saving.
	if (!_es_folder_append_path_separator)
	{
		if (_es_export_type != _ES_EXPORT_TYPE_NONE)
		{
			if (_es_export_type != _ES_EXPORT_TYPE_NOFORMAT)
			{
				// export should use trailing '\\'.
				_es_folder_append_path_separator = 1;
			}
		}
	}
	else
	if (_es_folder_append_path_separator < 0)
	{	
		_es_folder_append_path_separator = 0;
	}
		
	// apply export formatting.
	if ((_es_export_type == _ES_EXPORT_TYPE_CSV) || (_es_export_type == _ES_EXPORT_TYPE_TSV))
	{
		if (!_es_header)
		{
			_es_header = 1;
		}

		_es_footer = -1;
	}
	else
	if (_es_export_type == _ES_EXPORT_TYPE_JSON)
	{
		// no header
		_es_header = -1;
		_es_footer = -1;
	}
	else
	if (_es_export_type == _ES_EXPORT_TYPE_EFU)
	{
		// add standard columns now.
		// we don't want to change columns once we have sent the IPC requests off.
		// this may resolve es_ipc_version
	
		if (!_es_header)
		{
			_es_header = 1;
		}
		
		_es_footer = -1;
	}
	else
	if ((_es_export_type == _ES_EXPORT_TYPE_TXT) || (_es_export_type == _ES_EXPORT_TYPE_M3U) || (_es_export_type == _ES_EXPORT_TYPE_M3U8))
	{
		if (_es_export_type == _ES_EXPORT_TYPE_M3U)		
		{
			_es_cp = CP_ACP;
		}

		if ((_es_mode == _ES_MODE_READ_JOURNAL) && (_es_export_type == _ES_EXPORT_TYPE_TXT))
		{
			// read journal
			// don't clear columns
		}
		else
		{
			// reset columns and force Filename.
			column_clear_all();
			column_add(EVERYTHING3_PROPERTY_ID_PATH_AND_NAME);
		}
		
		if (_es_export_type == _ES_EXPORT_TYPE_TXT)
		{
			// don't show header unless user wants it.
		}
		else
		{
			// never show header.
			_es_header = -1;
			_es_footer = -1;
		}
	}
	
	// get everything version
	switch(_es_mode)
	{
		case _ES_MODE_GET_EVERYTHING_VERSION:
		
			{
				_es_everything_hwnd = _es_find_ipc_window();
				
				if (_es_everything_hwnd)
				{
					int major;
					int minor;
					int revision;
					int build;

					// wait for DB_IS_LOADED so we don't get 0 results
					major = (int)SendMessage(_es_everything_hwnd,EVERYTHING_WM_IPC,EVERYTHING_IPC_GET_MAJOR_VERSION,0);
					minor = (int)SendMessage(_es_everything_hwnd,EVERYTHING_WM_IPC,EVERYTHING_IPC_GET_MINOR_VERSION,0);
					revision = (int)SendMessage(_es_everything_hwnd,EVERYTHING_WM_IPC,EVERYTHING_IPC_GET_REVISION,0);
					build = (int)SendMessage(_es_everything_hwnd,EVERYTHING_WM_IPC,EVERYTHING_IPC_GET_BUILD_NUMBER,0);
						
					_es_output_noncell_printf("%u.%u.%u.%u\r\n",major,minor,revision,build);
				}
				else
				{
					es_fatal(ES_ERROR_NO_IPC);
				}
				
				perform_search = 0;
			}
			
			break;	
	
		case _ES_MODE_LIST_PROPERTIES:
		
			{
				DWORD i;
				wchar_buf_t property_name_wcbuf;
				wchar_t **indexes;
				SIZE_T count;

				wchar_buf_init(&property_name_wcbuf);
				
				indexes = mem_alloc(safe_size_mul_sizeof_pointer(EVERYTHING3_PROPERTY_ID_BUILTIN_COUNT));
				count = 0;
	
				// setup
				for(i=0;i<EVERYTHING3_PROPERTY_ID_BUILTIN_COUNT;i++)
				{
					if (i != EVERYTHING3_PROPERTY_ID_SEPARATOR)
					{
						property_get_canonical_name(i,&property_name_wcbuf);
						
						indexes[count] = wchar_string_alloc_wchar_string_n(property_name_wcbuf.buf,property_name_wcbuf.length_in_wchars);
						count++;
					}
				}
				
				// sort
				os_sort(indexes,count,_es_list_properties_compare);
				
				// output
				for(i=0;i<count;i++)
				{
					_es_output_noncell_printf("%S\r\n",indexes[i]);
				}
				
				perform_search = 0;

				// free
				for(i=0;i<count;i++)
				{
					mem_free(indexes[i]);
				}

				mem_free(indexes);
				
				wchar_buf_kill(&property_name_wcbuf);
			}
			
			break;
			
		case _ES_MODE_GET_JOURNAL_ID:

			{
				ipc3_journal_info_t journal_info;
				
				if (ipc3_get_journal_info(&journal_info))
				{
					_es_output_noncell_printf("%I64u\r\n",journal_info.journal_id);
				}
				else
				{
					switch(GetLastError())
					{
						case ERROR_PIPE_NOT_CONNECTED:
							es_fatal(ES_ERROR_NO_IPC);
							break;

						default:
							es_fatal(ES_ERROR_IPC_ERROR);
							break;
					}
				}
			}
			
			perform_search = 0;

			break;
			
		case _ES_MODE_GET_JOURNAL_POS:

			{
				ipc3_journal_info_t journal_info;
				
				if (ipc3_get_journal_info(&journal_info))
				{
					_es_output_noncell_printf("%I64u %I64u\r\n",journal_info.journal_id,journal_info.next_change_id);
				}
				else
				{
					switch(GetLastError())
					{
						case ERROR_PIPE_NOT_CONNECTED:
							es_fatal(ES_ERROR_NO_IPC);
							break;

						default:
							es_fatal(ES_ERROR_IPC_ERROR);
							break;
					}
				}
			}
			
			perform_search = 0;

			break;
			
		case _ES_MODE_READ_JOURNAL:
		
			{
				ipc3_journal_info_t journal_info;
				
				property_load_read_journal_names();
				
				// column type is required.
				// if its not manually added, add the default columns.
				if (!column_find(EVERYTHING3_PROPERTY_ID_TYPE))
				{
					column_add(EVERYTHING3_PROPERTY_ID_PARENT_FILE_ID); // journal-id
					column_add(EVERYTHING3_PROPERTY_ID_FILE_ID); // change-id
					column_add(EVERYTHING3_PROPERTY_ID_DATE_CHANGED); // timestamp
					column_add(EVERYTHING3_PROPERTY_ID_TYPE); // action
					column_add(EVERYTHING3_PROPERTY_ID_PATH_AND_NAME); // old filename
					column_add(EVERYTHING3_PROPERTY_ID_FILE_LIST_PATH_AND_NAME); // new filename
					
					column_move_order_to_start(EVERYTHING3_PROPERTY_ID_FILE_LIST_PATH_AND_NAME);
					column_move_order_to_start(EVERYTHING3_PROPERTY_ID_PATH_AND_NAME);
					column_move_order_to_start(EVERYTHING3_PROPERTY_ID_TYPE);
					column_move_order_to_start(EVERYTHING3_PROPERTY_ID_DATE_INDEXED);
					column_move_order_to_start(EVERYTHING3_PROPERTY_ID_DATE_CHANGED);
					column_move_order_to_start(EVERYTHING3_PROPERTY_ID_FILE_ID);
					column_move_order_to_start(EVERYTHING3_PROPERTY_ID_PARENT_FILE_ID);
				}
				
				// adjust columns widths if they are not defined.
				// do this after saving settings above.
				
				// Journal ID
				if (!column_width_find(EVERYTHING3_PROPERTY_ID_PARENT_FILE_ID))
				{
					column_width_set(EVERYTHING3_PROPERTY_ID_PARENT_FILE_ID,18);
				}
				
				// Change ID
				if (!column_width_find(EVERYTHING3_PROPERTY_ID_FILE_ID))
				{
					column_width_set(EVERYTHING3_PROPERTY_ID_FILE_ID,4);
				}
				
				// Action
				if (!column_width_find(EVERYTHING3_PROPERTY_ID_TYPE))
				{
					column_width_set(EVERYTHING3_PROPERTY_ID_TYPE,13);
				}
				
				if (_es_from_change_id != ES_UINT64_MAX)
				{
					if (_es_from_journal_id == ES_UINT64_MAX)
					{
						// journal id MUST be specified.
						_es_bad_switch_param("Change ID specified without a Journal ID.\n");
					}
				}

				if (_es_to_change_id != ES_UINT64_MAX)
				{
					if (_es_to_journal_id == ES_UINT64_MAX)
					{
						// journal id MUST be specified.
						_es_bad_switch_param("Change ID specified without a Journal ID.\n");
					}
				}
				
				if (ipc3_get_journal_info(&journal_info))
				{
					DWORD flags;
					_es_read_journal_t read_journal;
					
					read_journal.run = _es_max_results;
					read_journal.numitems = 0;
					read_journal.state = 0;
					
					flags = 0;
					
					if ((column_find(EVERYTHING3_PROPERTY_ID_FILE_ID)) || (_es_to_change_id != ES_UINT64_MAX) || (_es_watch))
					{
						flags |= IPC3_READ_JOURNAL_FLAG_CHANGE_ID;
					}

					if ((column_find(EVERYTHING3_PROPERTY_ID_DATE_CHANGED)) || (_es_from_date != ES_UINT64_MAX) || (_es_to_date != ES_UINT64_MAX))
					{
						flags |= IPC3_READ_JOURNAL_FLAG_TIMESTAMP;
					}

					if (column_find(EVERYTHING3_PROPERTY_ID_DATE_INDEXED))
					{
						flags |= IPC3_READ_JOURNAL_FLAG_SOURCE_TIMESTAMP;
					}
						
					if (column_find(EVERYTHING3_PROPERTY_ID_DATE_RECENTLY_CHANGED))
					{
						flags |= IPC3_READ_JOURNAL_FLAG_OLD_PARENT_DATE_MODIFIED;
					}
						
					if (column_find(EVERYTHING3_PROPERTY_ID_PATH_AND_NAME))
					{
						flags |= IPC3_READ_JOURNAL_FLAG_OLD_PATH | IPC3_READ_JOURNAL_FLAG_OLD_NAME;
					}
					
					if (column_find(EVERYTHING3_PROPERTY_ID_SIZE))
					{
						flags |= IPC3_READ_JOURNAL_FLAG_SIZE;
					}
					
					if (column_find(EVERYTHING3_PROPERTY_ID_DATE_CREATED))
					{
						flags |= IPC3_READ_JOURNAL_FLAG_DATE_CREATED;
					}
						
					if (column_find(EVERYTHING3_PROPERTY_ID_DATE_MODIFIED))
					{
						flags |= IPC3_READ_JOURNAL_FLAG_DATE_MODIFIED;
					}
						
					if (column_find(EVERYTHING3_PROPERTY_ID_DATE_ACCESSED))
					{
						flags |= IPC3_READ_JOURNAL_FLAG_DATE_ACCESSED;
					}
						
					if (column_find(EVERYTHING3_PROPERTY_ID_ATTRIBUTES))
					{
						flags |= IPC3_READ_JOURNAL_FLAG_ATTRIBUTES;
					}
						
					if (column_find(EVERYTHING3_PROPERTY_ID_DATE_RUN))
					{
						flags |= IPC3_READ_JOURNAL_FLAG_NEW_PARENT_DATE_MODIFIED;
					}
						
					if (column_find(EVERYTHING3_PROPERTY_ID_FILE_LIST_PATH_AND_NAME))
					{
						flags |= IPC3_READ_JOURNAL_FLAG_NEW_PATH | IPC3_READ_JOURNAL_FLAG_NEW_NAME;
					}
					
					if (_es_from_journal_id != ES_UINT64_MAX)
					{
						journal_info.journal_id = _es_from_journal_id;
					}
					
					if (_es_to_now)
					{
						_es_to_journal_id = journal_info.journal_id;
						_es_to_change_id = journal_info.next_change_id;
					}
					
					if (_es_from_change_id != ES_UINT64_MAX)
					{
						journal_info.next_change_id = _es_from_change_id;
					}
					else
					if (_es_from_now)
					{
						// keep current.
					}
					else
					{
						// ES_UINT64_MAX == start from first available.
						journal_info.next_change_id = ES_UINT64_MAX;
					}
					
					wchar_string_make_lowercase(_es_search_wcbuf->buf);
					wchar_buf_fix_quotes(_es_search_wcbuf);

retry_read_journal:

					if (!ipc3_read_journal(journal_info.journal_id,journal_info.next_change_id,flags,&read_journal,_es_read_journal_callback_proc))
					{
						switch(GetLastError())
						{
							case ERROR_FILE_NOT_FOUND:
								
								// the journal was deteled while we were reading the journal?
								// if so, grab the new journal id and continue monitoring from the start of the new journal.
								// this will happen if Everything does a reindex.
								{
									ipc3_journal_info_t new_journal_info;
									
									if (ipc3_get_journal_info(&new_journal_info))
									{
										if (new_journal_info.journal_id != journal_info.journal_id)
										{
											// new journal ID.
											// use first available change id, which should really be 0.
											journal_info.journal_id = new_journal_info.journal_id;
											journal_info.next_change_id = ES_UINT64_MAX;
											
											goto retry_read_journal;
										}
									}
								}
								
								goto retry_read_journal;
								
							case ERROR_PIPE_NOT_CONNECTED:
								es_fatal(ES_ERROR_NO_IPC);
								break;
								
							case ERROR_CANCELLED:
								// no error, user cancelled.
								break;
								
							default:
								// ipc3_read_journal does not return.
								es_fatal(ES_ERROR_IPC_ERROR);
								break;
						}
					}

					if (_es_footer > 0)
					{
						_es_output_footer(read_journal.numitems,ES_UINT64_MAX);
					}
					
					_es_output_page_end();
				}
				else
				{
					switch(GetLastError())
					{
						case ERROR_PIPE_NOT_CONNECTED:
							es_fatal(ES_ERROR_NO_IPC);
							break;
							
						default:
							es_fatal(ES_ERROR_IPC_ERROR);
							break;
					}
				}
			}
		
			perform_search = 0;

			break;
	}
	
	// reindex?
	if (es_reindex)
	{
		_es_everything_hwnd = _es_find_ipc_window();
		
		if (_es_everything_hwnd)
		{
			SendMessage(_es_everything_hwnd,EVERYTHING_WM_IPC,EVERYTHING_IPC_REBUILD_DB,0);
			
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
	if (_es_run_history_command)
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
	if (_es_save_db)
	{
		_es_everything_hwnd = _es_find_ipc_window();
		
		if (_es_everything_hwnd)
		{
			SendMessage(_es_everything_hwnd,EVERYTHING_WM_IPC,EVERYTHING_IPC_SAVE_DB,0);
			
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
		_es_everything_hwnd = _es_find_ipc_window();
		
		if (_es_everything_hwnd)
		{
			DWORD dwProcessId;

			// wait for Everything to exit.
			if (GetWindowThreadProcessId(_es_everything_hwnd,&dwProcessId))
			{
				HANDLE h;
				
				h = OpenProcess(SYNCHRONIZE,FALSE,dwProcessId);
				if (h)
				{
					SendMessage(_es_everything_hwnd,WM_CLOSE,0,0);

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
		if ((!search_wcbuf.length_in_wchars) && (!filter_wcbuf.length_in_wchars) && (_es_max_results == 0xffffffff) && (!_es_get_result_count) && (!_es_get_total_size))
		{
			if ((_es_empty_search_help) && (_es_output_is_char))
			{
				// don't show help if we are redirecting to a file.
				_es_help();
				
				goto exit;
			}

			if (_es_hide_empty_search_results)
			{
				goto exit;
			}
		}
			
		// efu doesn't want name or path columns.
		if (_es_export_type == _ES_EXPORT_TYPE_EFU)
		{
			column_remove(EVERYTHING3_PROPERTY_ID_NAME);
			column_remove(EVERYTHING3_PROPERTY_ID_PATH);
		}
		
		// add filename column
		if (!_es_no_default_filename_column)
		{
			if (!column_find(EVERYTHING3_PROPERTY_ID_PATH_AND_NAME))
			{
				if (!column_find(EVERYTHING3_PROPERTY_ID_NAME))
				{
					if (!column_find(EVERYTHING3_PROPERTY_ID_PATH))
					{
						column_t *filename_column;
						
						filename_column = column_add(EVERYTHING3_PROPERTY_ID_PATH_AND_NAME);
						
						if (_es_export_type != _ES_EXPORT_TYPE_NONE)
						{
							if (_es_export_type != _ES_EXPORT_TYPE_NOFORMAT)
							{
								// move KEY to first column.
								column_remove_order(filename_column);
								column_insert_order_at_start(filename_column);
							}
						}
					}
				}
			}
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
		
		if (_es_get_result_count)
		{
			// no columns.
			column_clear_all();

			// don't show any results.
			_es_max_results = 0;
			
			// reset sort to name ascending.
			secondary_sort_clear_all();
			_es_primary_sort_property_id = EVERYTHING3_PROPERTY_ID_NAME;
			_es_primary_sort_ascending = 1;
		}

		if (_es_get_total_size)
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

		_es_everything_hwnd = _es_find_ipc_window();
		if (_es_everything_hwnd)
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

	if (_es_reply_hwnd)
	{
		DestroyWindow(_es_reply_hwnd);
	}

	secondary_sort_clear_all();
	column_clear_all();
	column_color_clear_all();
	column_width_clear_all();
	property_unknown_clear_all();

	if (_es_run_history_data)
	{
		mem_free(_es_run_history_data);
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
	
	if (_es_ret != ES_ERROR_SUCCESS)
	{
		es_fatal(_es_ret);
	}

	array_kill(&local_property_unknown_array);
	array_kill(&local_secondary_sort_array);
	array_kill(&local_column_array);
	array_kill(&local_column_width_array);
	array_kill(&local_column_color_array);
	pool_kill(&local_property_unknown_pool);
	pool_kill(&local_secondary_sort_pool);
	pool_kill(&local_column_pool);
	pool_kill(&local_column_width_pool);
	pool_kill(&local_column_color_pool);
	wchar_buf_kill(&local_locale_decimal_wcbuf);
	wchar_buf_kill(&local_locale_thousand_wcbuf);
	wchar_buf_kill(&local_instance_name_wcbuf);
	wchar_buf_kill(&filter_wcbuf);
	wchar_buf_kill(&search_wcbuf);
	wchar_buf_kill(&argv_wcbuf);

	os_kill();

	return ES_ERROR_SUCCESS;
}

int main(int argc,char **argv)
{
	return _es_main();
}

// get the window classname with the instance name if specified.
static void _es_get_window_classname(wchar_buf_t *wcbuf)
{
	wchar_buf_copy_utf8_string(wcbuf,EVERYTHING_IPC_WNDCLASSA);
	
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
	wchar_buf_t window_class_wcbuf;
	HWND ret;

	wchar_buf_init(&window_class_wcbuf);
	
	tickstart = GetTickCount();

	_es_get_window_classname(&window_class_wcbuf);

	ret = 0;
	
	for(;;)
	{
		HWND hwnd;
		DWORD tick;
		
		hwnd = FindWindow(window_class_wcbuf.buf,0);

		if (hwnd)
		{
			// wait for DB_IS_LOADED so we don't get 0 results
			if (es_timeout)
			{
				int major;
				int minor;
				int is_db_loaded;
				
				major = (int)SendMessage(_es_everything_hwnd,EVERYTHING_WM_IPC,EVERYTHING_IPC_GET_MAJOR_VERSION,0);
				minor = (int)SendMessage(_es_everything_hwnd,EVERYTHING_WM_IPC,EVERYTHING_IPC_GET_MINOR_VERSION,0);
				
				if (((major == 1) && (minor >= 4)) || (major > 1))
				{
					is_db_loaded = (int)SendMessage(_es_everything_hwnd,EVERYTHING_WM_IPC,EVERYTHING_IPC_IS_DB_LOADED,0);
					
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
		
		tick = GetTickCount();
		
		if (tick - tickstart > es_timeout)
		{
			// the everything window was not found.
			// we can optionally RegisterWindowMessage("EVERYTHING_IPC_CREATED") and 
			// wait for Everything to post this message to all top level windows when its up and running.
			es_fatal(ES_ERROR_NO_IPC);
		}

		// try again..
		Sleep(10);
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
			major = (int)SendMessage(_es_everything_hwnd,EVERYTHING_WM_IPC,EVERYTHING_IPC_GET_MAJOR_VERSION,0);
			minor = (int)SendMessage(_es_everything_hwnd,EVERYTHING_WM_IPC,EVERYTHING_IPC_GET_MINOR_VERSION,0);
			
			if (((major == 1) && (minor >= 4)) || (major > 1))
			{
				is_db_loaded = (int)SendMessage(_es_everything_hwnd,EVERYTHING_WM_IPC,EVERYTHING_IPC_IS_DB_LOADED,0);
				
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
			major = (int)SendMessage(_es_everything_hwnd,EVERYTHING_WM_IPC,EVERYTHING_IPC_GET_MAJOR_VERSION,0);
			minor = (int)SendMessage(_es_everything_hwnd,EVERYTHING_WM_IPC,EVERYTHING_IPC_GET_MINOR_VERSION,0);
			
			if (((major == 1) && (minor >= 4)) || (major > 1))
			{
				int is_busy;
				
				is_busy = (int)SendMessage(_es_everything_hwnd,EVERYTHING_WM_IPC,EVERYTHING_IPC_IS_DB_BUSY,0);
				
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
		argv_p = _es_parse_command_line_option_name(argv_p,s);
		if (argv_p)
		{
			if (!*argv_p)
			{
				return TRUE;
			}
		}
	}
	
	return FALSE;
}

const wchar_t *_es_parse_command_line_option_name(const wchar_t *param,const ES_UTF8 *s)
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
				return NULL;
			}
		}
	}
	
	return p1;
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

// format a size value.
// eg: 123 KB
static void _es_format_size(ES_UINT64 size,int size_format,wchar_buf_t *wcbuf)
{
	wchar_buf_empty(wcbuf);
	
	if (size != 0xffffffffffffffffI64)
	{
		if (size_format == 0)
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
		if (size_format == 2)
		{
			_es_format_number(((size) + 1023) / 1024,1,wcbuf);
			wchar_buf_cat_utf8_string(wcbuf," KB");
		}
		else
		if (size_format == 3)
		{
			_es_format_number(((size) + 1048575) / 1048576,1,wcbuf);
			wchar_buf_cat_utf8_string(wcbuf," MB");
		}
		else
		if (size_format == 4)
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


// format some attributes.
// eg: RASH
static void _es_format_attributes(DWORD attributes,wchar_buf_t *wcbuf)
{
	wchar_t *d;
	
	wchar_buf_grow_size(wcbuf,256);
	
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

// format a filetime.
static void _es_format_filetime(ES_UINT64 filetime,wchar_buf_t *wcbuf)
{
	wchar_buf_empty(wcbuf);
	
	if (filetime != 0xffffffffffffffffI64)
	{
		switch(_es_date_format)
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
				
				os_filetime_to_localtime(filetime,&st);
				
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
				os_filetime_to_localtime(filetime,&st);
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

// format a duration value.
// duration is in 100 nanoseconds resolution.
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

static BOOL _es_get_locale_info(LCTYPE LCType,wchar_t out_wbuf[256])
{
	if (GetLocaleInfo(LOCALE_USER_DEFAULT,LCType,out_wbuf,256))
	{
		return TRUE;
	}
	
	return FALSE;
}

// format a number.
// uses thousand separator.
static void _es_format_number(ES_UINT64 number,int allow_digit_grouping,wchar_buf_t *out_wcbuf)
{
	if (allow_digit_grouping)
	{
		wchar_buf_t number_wcbuf;
	    
		wchar_buf_init(&number_wcbuf);

		wchar_buf_printf(&number_wcbuf,"%I64u",number);

		wchar_buf_grow_size(out_wcbuf,256);
		wchar_buf_empty(out_wcbuf);

		if (out_wcbuf->size_in_wchars <= INT_MAX)
		{
			NUMBERFMT nfmt;
			int fmt_ret;
			
			nfmt.NumDigits = 0;
			nfmt.LeadingZero = _es_locale_lzero;
			nfmt.Grouping = _es_locale_grouping;
			nfmt.lpDecimalSep = _es_locale_decimal_wcbuf->buf;
			nfmt.lpThousandSep = _es_locale_thousand_wcbuf->buf;
			nfmt.NegativeOrder = _es_locale_negnumber;
			
			fmt_ret = GetNumberFormat(LOCALE_USER_DEFAULT,0,number_wcbuf.buf,&nfmt,out_wcbuf->buf,(int)out_wcbuf->size_in_wchars);
			if (fmt_ret)
			{
				out_wcbuf->length_in_wchars = fmt_ret - 1;
			}
			else
			{
				wchar_buf_empty(out_wcbuf);
			}
		}

		wchar_buf_kill(&number_wcbuf);
	}
	else
	{
		wchar_buf_printf(out_wcbuf,"%I64u",number);
	}
}

// format a fixed point number.
// uses thousand separator.
// trims trailing zeroes in the fraction part.
// always has one digit after the '.'
// always shows the '.'
static void _es_format_fixed_q1k(ES_UINT64 number,int use_locale,int allow_digit_grouping,int is_signed,int allow_integer_representation,wchar_buf_t *out_wcbuf)
{
	ES_UTF8 *d;
	ES_UTF8 buf[256];
	int fraction_part;
	ES_UINT64 int_part;
	int is_negative;
	int decimal_places;
	
	decimal_places = 3;
	is_negative = 0;
	if (is_signed)
	{
		if ((__int64)number < 0)
		{
			number = (ES_UINT64)-(__int64)number;
			is_negative = 1;
		}
	}
	
	int_part = number / 1000;
	fraction_part = (int)(number % 1000);
	
	d = buf + 256;
	*--d = 0;
	
	*--d = '0' + (fraction_part % 10);
	*--d = '0' + ((fraction_part / 10) % 10);
	*--d = '0' + (fraction_part / 100);
	
	{
		int i;
		ES_UTF8 *trailing_zero_p;
		int max_trim_decimal_places;
		
		trailing_zero_p = buf + 256 - 1 - 1;

		max_trim_decimal_places = ((use_locale) && (allow_integer_representation)) ? 3 : 2;

		for(i=0;i<max_trim_decimal_places;i++)
		{
			if (*trailing_zero_p != '0')
			{
				break;
			}

			*trailing_zero_p = 0;
			trailing_zero_p--;
			decimal_places--;
		}
	}
	
	if (decimal_places)
	{
		*--d = '.';
	}

	if (int_part)
	{
		ES_UINT64 i;
		
		i = int_part;
		
		while(i)
		{
			*--d = '0' + (int)(i % 10);
			
			i /= 10;
		}
	}
	else
	{
		*--d = '0';
	}	
	
	if (is_negative)
	{
		*--d = '-';
	}

	if (use_locale)
	{
		wchar_buf_grow_size(out_wcbuf,256);
		wchar_buf_empty(out_wcbuf);

		if (out_wcbuf->size_in_wchars <= INT_MAX)
		{
			wchar_buf_t number_wcbuf;
			NUMBERFMT nfmt;
			int fmt_ret;

			wchar_buf_init(&number_wcbuf);
			
			wchar_buf_copy_utf8_string(&number_wcbuf,d);

			nfmt.NumDigits = decimal_places;
			nfmt.LeadingZero = _es_locale_lzero;
			nfmt.Grouping = allow_digit_grouping ? _es_locale_grouping : 0;
			nfmt.lpDecimalSep = _es_locale_decimal_wcbuf->buf;
			nfmt.lpThousandSep = _es_locale_thousand_wcbuf->buf;
			nfmt.NegativeOrder = _es_locale_negnumber;
			
			fmt_ret = GetNumberFormat(LOCALE_USER_DEFAULT,0,number_wcbuf.buf,&nfmt,out_wcbuf->buf,(int)out_wcbuf->size_in_wchars);
			
			if (fmt_ret)
			{
				out_wcbuf->length_in_wchars = fmt_ret - 1;
			}
			else
			{
				wchar_buf_empty(out_wcbuf);
			}

			wchar_buf_kill(&number_wcbuf);
		}
	}
	else
	{
		wchar_buf_copy_utf8_string(out_wcbuf,d);
	}
}

// format a fixed point number.
// uses thousand separator.
// trims trailing zeroes in the fraction part.
// always has one digit after the '.'
// always shows the '.'
static void _es_format_fixed_q1m(ES_UINT64 number,int use_locale,int allow_digit_grouping,int is_signed,wchar_buf_t *out_wcbuf)
{
	ES_UTF8 *d;
	ES_UTF8 buf[256];
	int fraction_part;
	ES_UINT64 int_part;
	int is_negative;
	int decimal_places;
	
	decimal_places = 6;
	is_negative = 0;
	if (is_signed)
	{
		if ((__int64)number < 0)
		{
			number = (ES_UINT64)-(__int64)number;
			is_negative = 1;
		}
	}
	
	int_part = number / 1000000;
	fraction_part = (int)(number % 1000000);
	
	d = buf + 256;
	*--d = 0;
	
	*--d = '0' + (fraction_part % 10);
	*--d = '0' + ((fraction_part / 10) % 10);
	*--d = '0' + (fraction_part / 100) % 10;
	*--d = '0' + (fraction_part / 1000) % 10;
	*--d = '0' + (fraction_part / 10000) % 10;
	*--d = '0' + (fraction_part / 100000);
	
	{
		int i;
		ES_UTF8 *trailing_zero_p;
		
		trailing_zero_p = buf + 256 - 1 - 1;
		
		for(i=0;i<5;i++)
		{
			if (*trailing_zero_p != '0')
			{
				break;
			}

			*trailing_zero_p = 0;
			trailing_zero_p--;
			decimal_places--;
		}
	}
	
	*--d = '.';

	if (int_part)
	{
		ES_UINT64 i;
		
		i = int_part;
		
		while(i)
		{
			*--d = '0' + (int)(i % 10);
			
			i /= 10;
		}
	}
	else
	{
		*--d = '0';
	}	
	
	if (is_negative)
	{
		*--d = '-';
	}
	
	if (use_locale)
	{
		wchar_buf_grow_size(out_wcbuf,256);
		wchar_buf_empty(out_wcbuf);

		if (out_wcbuf->size_in_wchars <= INT_MAX)
		{
			wchar_buf_t number_wcbuf;
			NUMBERFMT nfmt;
			int fmt_ret;

			wchar_buf_init(&number_wcbuf);
			
			wchar_buf_copy_utf8_string(&number_wcbuf,d);

			nfmt.NumDigits = decimal_places;
			nfmt.LeadingZero = _es_locale_lzero;
			nfmt.Grouping = allow_digit_grouping ? _es_locale_grouping : 0;
			nfmt.lpDecimalSep = _es_locale_decimal_wcbuf->buf;
			nfmt.lpThousandSep = _es_locale_thousand_wcbuf->buf;
			nfmt.NegativeOrder = _es_locale_negnumber;
			
			fmt_ret = GetNumberFormat(LOCALE_USER_DEFAULT,0,number_wcbuf.buf,&nfmt,out_wcbuf->buf,(int)out_wcbuf->size_in_wchars);
			
			if (fmt_ret)
			{
				out_wcbuf->length_in_wchars = fmt_ret - 1;
			}
			else
			{
				wchar_buf_empty(out_wcbuf);
			}

			wchar_buf_kill(&number_wcbuf);
		}
	}
	else
	{
		wchar_buf_copy_utf8_string(out_wcbuf,d);
	}
}

// format a dimensions value.
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

// get a argument from the global _es_command_line.
// moves the global _es_command_line pointer forward.
// this function preserves quotes.
// so an arg in quotes: "-addcolumn" is treated as a search.
// use _es_get_command_argv to get a switch param that processes quotes.
static void _es_get_argv(wchar_buf_t *wcbuf)
{
	int pass;
	int inquote;
	wchar_t *d;
	const wchar_t *p;
	int was_quote;
	int was_eq;

	was_quote = 0;
	was_eq = 0;
	
	if (_es_command_line_was_eq)
	{
		// unhandled -switch=value
		es_fatal(ES_ERROR_UNKNOWN_SWITCH);
	}
	
	if (!*_es_command_line)
	{
		_es_command_line = NULL;
		_es_command_line_was_eq = was_eq;
		
		return;
	}
	
	d = 0;
	
	for(pass=0;pass<2;pass++)
	{
		const wchar_t *start;
		
		if (_es_command_line_was_eq)
		{
			p = _es_command_line;
		}
		else
		{
			p = wchar_string_skip_ws(_es_command_line);
		}
		
		start = p;
		
		inquote = 0;
		
		while(*p)
		{
			if ((!inquote) && (unicode_is_ascii_ws(*p)))
			{
				break;
			}
			else
			if ((!was_quote) && (*p == '=') && (*start == '-'))
			{
				was_eq = 1;
				
				p++;
				break;
			}
			else
			if ((*p == '"') && (p[1] == '"') && (p[2] == '"'))
			{
				if (pass)
				{
					*d++ = '&';
					*d++ = 'q';
					*d++ = 'u';
					*d++ = 'o';
					*d++ = 't';
					*d++ = ':';
				}
				else
				{
					d += 6;
				}
				
				p += 3;
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
				
				inquote = !inquote;
				was_quote = 1;
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
			break;
		}
		else
		{
			wchar_buf_grow_length(wcbuf,((SIZE_T)d) / sizeof(wchar_t));
			d = wcbuf->buf;
		}		
	}

	*d = 0;
	_es_command_line = p;
	_es_command_line_was_eq = was_eq;
}

// get an argument from the command line.
// throws a fatal error if there was no command line argument.
static void _es_expect_argv(wchar_buf_t *wcbuf)
{
	_es_get_argv(wcbuf);
	
	if (!_es_command_line)
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
	const wchar_t *p;
	
	// eq value can be empty.
	if (!_es_command_line_was_eq)
	{
		if (!*_es_command_line)
		{
			_es_command_line = NULL;
			_es_command_line_was_eq = 0;

			return;
		}
	}
	
	d = 0;
	
	for(pass=0;pass<2;pass++)
	{
		if (_es_command_line_was_eq)
		{
			p = _es_command_line;
		}
		else
		{
			p = wchar_string_skip_ws(_es_command_line);
		}
		
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
			break;
		}
		else
		{
			wchar_buf_grow_length(wcbuf,((SIZE_T)d) / sizeof(wchar_t));
			d = wcbuf->buf;
		}		
	}
	
	*d = 0;
	_es_command_line = p;
	_es_command_line_was_eq = 0;
}

// get an argument from the command line.
// throws a fatal error if there was no command line argument.
static void _es_expect_command_argv(wchar_buf_t *wcbuf)
{
	_es_get_command_argv(wcbuf);
	
	if (!_es_command_line)
	{
		es_fatal(ES_ERROR_EXPECTED_SWITCH_PARAMETER);
	}
}

// don't scroll on these keys.
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

// save the current configuration to the specified ini filename.
// returns TRUE if successful.
// returns FALSE on failure.
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
		config_write_int(file_handle,"size_leading_zero",_es_size_leading_zero);
		config_write_int(file_handle,"run_count_leading_zero",_es_run_count_leading_zero);
		config_write_int(file_handle,"digit_grouping",_es_digit_grouping);
		config_write_int(file_handle,"locale_grouping",_es_locale_grouping);
		config_write_int(file_handle,"locale_lzero",_es_locale_lzero);
		config_write_int(file_handle,"locale_negnumber",_es_locale_negnumber);
		config_write_string(file_handle,"locale_thousand",_es_locale_thousand_wcbuf->buf);
		config_write_string(file_handle,"locale_decimal",_es_locale_decimal_wcbuf->buf);
		config_write_uint64(file_handle,"offset",_es_offset);
		config_write_uint64(file_handle,"max_results",_es_max_results);
		config_write_int(file_handle,"header",_es_header);
		config_write_int(file_handle,"footer",_es_footer);
		config_write_dword(file_handle,"timeout",es_timeout);
		config_write_int(file_handle,"size_format",_es_size_format);
		config_write_int(file_handle,"date_format",_es_display_date_format);
		config_write_int(file_handle,"export_date_format",_es_export_date_format);
		config_write_int(file_handle,"pause",_es_pause);
		config_write_int(file_handle,"empty_search_help",_es_empty_search_help);
		config_write_int(file_handle,"hide_empty_search_results",_es_hide_empty_search_results);
		config_write_int(file_handle,"utf8_bom",_es_utf8_bom);
		config_write_int(file_handle,"folder_append_path_separator",_es_folder_append_path_separator);
		config_write_int(file_handle,"newline_type",_es_newline_type);
		config_write_int(file_handle,"pixels_to_characters_mul",es_pixels_to_characters_mul);
		config_write_int(file_handle,"pixels_to_characters_div",es_pixels_to_characters_div);
		
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

// save the current configuration to the specified location
// if is_appdata is TRUE: %APPDATA%\voidtools\es 
// if is_appdata is FALSE: same location as es.exe
// returns TRUE if successful.
// returns FALSE on failure.
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
	
	if (!_es_loaded_appdata_ini)
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

// load configuration from the specified filename.
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
				_es_set_sort_list(sort_list_wcbuf.buf,1,0);
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
		_es_size_leading_zero = config_read_int(&ini,"size_leading_zero",_es_size_leading_zero);
		_es_run_count_leading_zero = config_read_int(&ini,"run_count_leading_zero",_es_run_count_leading_zero);
		_es_digit_grouping = config_read_int(&ini,"digit_grouping",_es_digit_grouping);
		_es_locale_grouping = config_read_int(&ini,"locale_grouping",_es_locale_grouping);
		_es_locale_lzero = config_read_int(&ini,"locale_lzero",_es_locale_lzero);
		_es_locale_negnumber = config_read_int(&ini,"locale_negnumber",_es_locale_negnumber);
		config_read_string(&ini,"locale_thousand",_es_locale_thousand_wcbuf);
		config_read_string(&ini,"locale_decimal",_es_locale_decimal_wcbuf);
		_es_offset = safe_size_from_uint64(config_read_uint64(&ini,"offset",_es_offset));
		_es_max_results = safe_size_from_uint64(config_read_uint64(&ini,"max_results",_es_max_results));
		_es_header = config_read_int(&ini,"header",_es_header);
		_es_footer = config_read_int(&ini,"footer",_es_footer);
		es_timeout = config_read_dword(&ini,"timeout",es_timeout);
		_es_size_format = config_read_int(&ini,"size_format",_es_size_format);
		_es_display_date_format = config_read_int(&ini,"date_format",_es_display_date_format);
		_es_export_date_format = config_read_int(&ini,"export_date_format",_es_export_date_format);
		_es_pause = config_read_int(&ini,"pause",_es_pause);
		_es_empty_search_help = config_read_int(&ini,"empty_search_help",_es_empty_search_help);
		_es_hide_empty_search_results = config_read_int(&ini,"hide_empty_search_results",_es_hide_empty_search_results);
		_es_utf8_bom = config_read_int(&ini,"utf8_bom",_es_utf8_bom);
		_es_folder_append_path_separator = config_read_int(&ini,"folder_append_path_separator",_es_folder_append_path_separator);
		_es_newline_type = config_read_int(&ini,"newline_type",_es_newline_type);
		es_pixels_to_characters_mul = config_read_int(&ini,"pixels_to_characters_mul",es_pixels_to_characters_mul);
		es_pixels_to_characters_div = config_read_int(&ini,"pixels_to_characters_div",es_pixels_to_characters_div);

		// columns
		
		{
			wchar_buf_t column_list_wcbuf;

			wchar_buf_init(&column_list_wcbuf);
			
			if (config_read_string(&ini,"columns",&column_list_wcbuf))
			{
				_es_set_columns(column_list_wcbuf.buf,0,1,FALSE);
			}

			wchar_buf_kill(&column_list_wcbuf);
		}
		
		// column colors.

		{
			wchar_buf_t column_color_list_wcbuf;

			wchar_buf_init(&column_color_list_wcbuf);
			
			if (config_read_string(&ini,"column_colors",&column_color_list_wcbuf))
			{
				_es_set_column_colors(column_color_list_wcbuf.buf,0,FALSE);
			}

			wchar_buf_kill(&column_color_list_wcbuf);
		}
		
		// column widths.

		{
			wchar_buf_t column_width_list_wcbuf;

			wchar_buf_init(&column_width_list_wcbuf);
			
			if (config_read_string(&ini,"column_widths",&column_width_list_wcbuf))
			{
				_es_set_column_widths(column_width_list_wcbuf.buf,0,FALSE);
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
		_es_loaded_appdata_ini = 1;
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
	_es_everything_hwnd = _es_find_ipc_window();
	if (_es_everything_hwnd)
	{
		if (_es_run_history_size <= ES_DWORD_MAX)
		{
			COPYDATASTRUCT cds;
			DWORD run_count;

			cds.cbData = (DWORD)_es_run_history_size;
			cds.dwData = _es_run_history_command;
			cds.lpData = _es_run_history_data;
			
			run_count = (DWORD)SendMessage(_es_everything_hwnd,WM_COPYDATA,0,(LPARAM)&cds);

			if (_es_run_history_command == EVERYTHING_IPC_COPYDATA_GET_RUN_COUNTW)
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
			
			property_id = property_find(sort_name_wcbuf.buf,_es_should_allow_property_system(sort_name_wcbuf.buf));
			
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
		
		utf8_buf_init(&filename_cbuf);

		utf8_buf_copy_wchar_string(&filename_cbuf,filename);
		
		if (ipc3_ioctl_expect_output_size(pipe_handle,IPC3_COMMAND_GET_FOLDER_SIZE,filename_cbuf.buf,filename_cbuf.length_in_bytes,&folder_size,sizeof(ES_UINT64)))
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
			
			property_id = property_find(property_name_wcbuf.buf,_es_should_allow_property_system(property_name_wcbuf.buf));
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
		property_id = property_find(argv_p,_es_should_allow_property_system(argv_p));
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
				
				property_id = property_find(argv_p,_es_should_allow_property_system(argv_p));
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
	
	is_ascending = _es_resolve_sort_ascending(property_id,sort_ascending);
	
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
	if (!_es_reply_hwnd)
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
		
		_es_reply_hwnd = CreateWindow(TEXT("ES_IPC"),TEXT(""),0,0,0,0,0,0,0,GetModuleHandle(0),0);
		if (!_es_reply_hwnd)
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
					_es_pChangeWindowMessageFilterEx(_es_reply_hwnd,WM_COPYDATA,_ES_MSGFLT_ALLOW,0);
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
			if (*p == '.')
			{
				// the . in system.size
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
static void _es_set_sort_list(const wchar_t *sort_list,int allow_old_column_ids,int die_on_bad_sort)
{
	wchar_buf_t item_wcbuf;
	utf8_buf_t sort_name_cbuf;
	const wchar_t *sort_list_p;
	SIZE_T sort_count;
	
	wchar_buf_init(&item_wcbuf);
	utf8_buf_init(&sort_name_cbuf);
	
	secondary_sort_clear_all();

	sort_list_p = sort_list;
	sort_count = 0;
	
	for(;;)
	{
		sort_list_p = wchar_string_parse_list_item(sort_list_p,&item_wcbuf);
		if (!sort_list_p)
		{
			break;
		}
		
		if (item_wcbuf.length_in_wchars)
		{
			DWORD property_id;
			int ascending;

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
			
			property_id = property_find(item_wcbuf.buf,1);
			
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
				_es_bad_switch_param("Unknown sort: %S\n",item_wcbuf.buf);
			}
		}
	}

	utf8_buf_kill(&sort_name_cbuf);
	wchar_buf_kill(&item_wcbuf);
}

// action 0 == set columns
// action 1 == add columns
// action 2 == remove columns
static void _es_set_columns(const wchar_t *column_list,int action,int allow_old_column_ids,BOOL die_on_bad_value)
{
	wchar_buf_t item_wcbuf;
	const wchar_t *column_list_p;
	
	wchar_buf_init(&item_wcbuf);
	
	if (action == 0)
	{
		column_clear_all();	
	}
	
	column_list_p = column_list;
	
	for(;;)
	{
		column_list_p = wchar_string_parse_list_item(column_list_p,&item_wcbuf);
		if (!column_list_p)
		{
			break;
		}
	
		if (item_wcbuf.length_in_wchars)
		{
			DWORD property_id;

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
			
			property_id = property_find(item_wcbuf.buf,1);
			
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
								_es_no_default_filename_column = 1;
								break;

							case EVERYTHING3_PROPERTY_ID_SIZE:
								_es_no_default_size_column = 1;
								break;

							case EVERYTHING3_PROPERTY_ID_DATE_MODIFIED:
								_es_no_default_date_modified_column = 1;
								break;

							case EVERYTHING3_PROPERTY_ID_DATE_CREATED:
								_es_no_default_date_created_column = 1;
								break;

							case EVERYTHING3_PROPERTY_ID_ATTRIBUTES:
								_es_no_default_attribute_column = 1;
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
				_es_bad_switch_param("Unknown column: %S\n",item_wcbuf.buf);
			}
		}
	}

	wchar_buf_kill(&item_wcbuf);
}

// action 0 == set column colors
// action 1 == add column colors
// action 2 == remove column colors
static void _es_set_column_colors(const wchar_t *column_color_list,int action,BOOL die_on_bad_value)
{
	wchar_buf_t item_wcbuf;
	const wchar_t *column_color_list_p;
	int old_column_id;
	
	wchar_buf_init(&item_wcbuf);
	
	if (action == 0)
	{
		column_color_clear_all();	
	}
	
	column_color_list_p = column_color_list;
	old_column_id = 0;
	
	for(;;)
	{
		column_color_list_p = wchar_string_parse_list_item(column_color_list_p,&item_wcbuf);
		if (!column_color_list_p)
		{
			break;
		}
		
		if (item_wcbuf.length_in_wchars)
		{
			DWORD property_id;
			wchar_t *color_value;
			WORD color;

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
				property_id = property_find(item_wcbuf.buf,1);

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
				_es_bad_switch_param("Unknown color: %S",item_wcbuf.buf);
			}
		}

		old_column_id++;
	}

	wchar_buf_kill(&item_wcbuf);
}

// action 0 == set column widths
// action 1 == add column widths
// action 2 == remove column widths
static void _es_set_column_widths(const wchar_t *column_width_list,int action,BOOL die_on_bad_value)
{
	wchar_buf_t item_wcbuf;
	const wchar_t *column_width_list_p;
	int old_column_id;
	
	wchar_buf_init(&item_wcbuf);
	
	if (action == 0)
	{
		column_width_clear_all();	
	}
	
	column_width_list_p = column_width_list;
	old_column_id = 0;
	
	for(;;)
	{
		column_width_list_p = wchar_string_parse_list_item(column_width_list_p,&item_wcbuf);
		if (!column_width_list_p)
		{
			break;
		}
		
		if (item_wcbuf.length_in_wchars)
		{
			DWORD property_id;
			wchar_t *width_value;
			int width;

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
				property_id = property_find(item_wcbuf.buf,1);

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
				_es_bad_switch_param("Unknown width: %S",item_wcbuf.buf);
			}
		}
		
		old_column_id++;
	}

	wchar_buf_kill(&item_wcbuf);
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
		if (!_es_no_default_size_column)
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
		if (!_es_no_default_date_modified_column)
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
		if (!_es_no_default_date_created_column)
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
		if (!_es_no_default_attribute_column)
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
	if (SendMessage(_es_everything_hwnd,EVERYTHING_WM_IPC,EVERYTHING_IPC_IS_FILE_INFO_INDEXED,file_info_type))
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
	
	info_line = _es_console_window_high - 1;
	
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
	if (_es_header > 0)
	{
		total_lines = safe_size_add_one(total_lines);
	}
	
	if (_es_footer > 0)
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
		
		if (_es_console_window_y + info_line + 1 > _es_console_size_high)
		{
			for(i=0;i<(_es_console_window_y + info_line + 1) - _es_console_size_high;i++)
			{
				DWORD numwritten;
				
				WriteFile(_es_output_handle,"\r\n",2,&numwritten,0);
			}
										
			_es_console_window_y = _es_console_size_high - (int)(info_line + 1);
		}
	}
	
	std_input_handle = GetStdHandle(STD_INPUT_HANDLE);

	_es_output_cibuf = mem_alloc(_es_console_window_wide * sizeof(CHAR_INFO));
	
	// set cursor pos to the bottom of the screen
	{
		COORD cur_pos;
		cur_pos.X = _es_console_window_x;
		cur_pos.Y = _es_console_window_y + (int)info_line;
		
		SetConsoleCursorPosition(_es_output_handle,cur_pos);
	}

	// write out some basic usage at the bottom
	{
		DWORD numwritten;

		WriteFile(_es_output_handle,_ES_PAUSE_TEXT,(DWORD)utf8_string_get_length_in_bytes(_ES_PAUSE_TEXT),&numwritten,0);
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
				_es_output_ipc1_results((EVERYTHING_IPC_LIST *)data,start_index,_es_console_window_high-1);
				break;
				
			case ES_IPC_VERSION_FLAG_IPC2:
				_es_output_ipc2_results((EVERYTHING_IPC_LIST2 *)data,start_index,_es_console_window_high-1);
				break;	
				
			case ES_IPC_VERSION_FLAG_IPC3:
			
				// reposition the pool stream to the current start index.
				// if this isn't cached, we will read from the pipe stream, find the correct offset and cache the result.
				ipc3_result_list_seek_to_offset_from_index((ipc3_result_list_t *)data,start_index);
				
				_es_output_ipc3_results((ipc3_result_list_t *)data,start_index,_es_console_window_high-1);
				break;
		}
		
		// is everything is shown?
		if (_es_max_wide <= _es_console_window_wide)
		{
			if (total_lines < (SIZE_T)_es_console_window_high)
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
							if (total_lines < (SIZE_T)_es_console_window_high + 1)
							{
								goto exit;
							}
							if (start_index == total_lines - _es_console_window_high + 1)
							{
								goto exit;
							}
							start_index += 1;
							break;
						}
						else
						if (ir.Event.KeyEvent.wVirtualKeyCode == VK_SPACE)
						{
							if (total_lines < (SIZE_T)_es_console_window_high + 1)
							{
								goto exit;
							}
							
							if (start_index == total_lines - _es_console_window_high + 1)
							{
								goto exit;
							}
							
							start_index += 1;
							break;
						}
						else
						if (ir.Event.KeyEvent.wVirtualKeyCode == VK_RIGHT)
						{
							_es_output_cibuf_hscroll += 5;
							
							if (_es_max_wide > _es_console_window_wide)
							{
								if (_es_output_cibuf_hscroll > _es_max_wide - _es_console_window_wide)
								{
									_es_output_cibuf_hscroll = _es_max_wide - _es_console_window_wide;
								}
							}
							else
							{
								_es_output_cibuf_hscroll = 0;
							}
							break;
						}
						else
						if (ir.Event.KeyEvent.wVirtualKeyCode == VK_LEFT)
						{
							_es_output_cibuf_hscroll -= 5;
							if (_es_output_cibuf_hscroll < 0)
							{
								_es_output_cibuf_hscroll = 0;
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
							start_index -= _es_console_window_high - 1;
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
							start_index = (int)total_lines - _es_console_window_high + 1;
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
							start_index += _es_console_window_high - 1;
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
		if (total_lines > (SIZE_T)_es_console_window_high + 1)
		{
			if (start_index > (int)total_lines - _es_console_window_high + 1)
			{
				start_index = (int)total_lines - _es_console_window_high + 1;
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
		if (last_hscroll != _es_output_cibuf_hscroll)
		{
			last_hscroll = _es_output_cibuf_hscroll;
		}
		else
		{
			goto start;
		}
	}
	
exit:						
	
	// remove text.
	if (_es_output_cibuf)
	{
		// set cursor pos to the bottom of the screen
		{
			COORD cur_pos;
			cur_pos.X = _es_console_window_x;
			cur_pos.Y = _es_console_window_y + (int)info_line;
			
			SetConsoleCursorPosition(_es_output_handle,cur_pos);
		}

		// write out some basic usage at the bottom
		{
			DWORD numwritten;
	
			WriteFile(_es_output_handle,_ES_BLANK_PAUSE_TEXT,(DWORD)utf8_string_get_length_in_bytes(_ES_BLANK_PAUSE_TEXT),&numwritten,0);
		}
	
		// reset cursor pos.
		{
			COORD cur_pos;
			cur_pos.X = _es_console_window_x;
			cur_pos.Y = _es_console_window_y + (int)info_line;
			
			SetConsoleCursorPosition(_es_output_handle,cur_pos);
		}
		
		// free
		mem_free(_es_output_cibuf);
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

// if ascending is 0, use the default ascending order from the property id.
// returns TRUE if sort is ascending.
// Otherwise, returns FALSE.
static BOOL _es_resolve_sort_ascending(DWORD property_id,int ascending)
{
	if (ascending < 0)
	{
		return FALSE;
	}

	if (ascending > 0)
	{
		return TRUE;
	}
	
	// get default sort ascending.
	return property_get_default_sort_ascending(property_id);
}

static int _es_list_properties_compare(const wchar_t *a,const wchar_t *b)
{
	int cmp_ret;
	
	cmp_ret = CompareString(LOCALE_USER_DEFAULT,NORM_IGNORECASE,a,-1,b,-1);
	
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

	return wchar_string_compare(a,b);
}

// format a GUID to a windows {01234567-0123-0123-0123-012345670123} standard.
static void _es_format_clsid(const CLSID *clsid_value,utf8_buf_t *out_cbuf)
{
	utf8_buf_printf(out_cbuf,"{%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}",clsid_value->Data1,clsid_value->Data2,clsid_value->Data3,clsid_value->Data4[0],clsid_value->Data4[1],clsid_value->Data4[2],clsid_value->Data4[3],clsid_value->Data4[4],clsid_value->Data4[5],clsid_value->Data4[6],clsid_value->Data4[7]);
}

static void _es_output_footer(SIZE_T total_items,ES_UINT64 total_size)
{
	wchar_buf_t items_wcbuf;
	wchar_buf_t size_wcbuf;

	wchar_buf_init(&items_wcbuf);
	wchar_buf_init(&size_wcbuf);
	
	_es_is_in_header = 1;
	
	_es_output_line_begin(0);
	
	_es_format_number(total_items,1,&items_wcbuf);
	_es_format_size(total_size,0,&size_wcbuf);

	_es_output_noncell_printf("%S item%s%s%S",items_wcbuf.buf,total_items == 1 ? "" : "s",size_wcbuf.length_in_wchars ? " " : "",size_wcbuf.buf);
	
	_es_output_line_end(0);

	_es_is_in_header = 0;
	
	wchar_buf_kill(&size_wcbuf);
	wchar_buf_kill(&items_wcbuf);
}

static BOOL _es_read_journal_callback_proc(_es_read_journal_t *param,_ipc3_journal_change_t *change)
{
	BOOL is_first;
	BOOL is_last;
	
	is_last = FALSE;
	
	if (!param->state)
	{
		_es_output_page_begin();

		if (_es_header > 0)
		{
			_es_output_header();
		}
		
		param->state = 1;
	}
	
	if (param->run)
	{
		param->run--;
	
		if (!param->run)
		{
			is_last = TRUE;
		}
	}
	
	if (_es_to_journal_id != ES_UINT64_MAX)
	{
		if ((_es_to_journal_id == change->journal_id) && (_es_to_change_id != ES_UINT64_MAX) && (_es_to_change_id))
		{
			if (change->change_id > _es_to_change_id-1)
			{
				// already passed it?
				// dont show this change.
				return FALSE;
			}
			
			// was this the last entry?
			if (change->change_id == _es_to_change_id-1)
			{
				is_last = TRUE;
			}
		}
		else
		{
			if (change->journal_id >= _es_to_journal_id)
			{
				return FALSE;
			}
		}
	}
	
	// stop on to change date.
	if (_es_to_date != ES_UINT64_MAX)
	{
		if (change->timestamp >= _es_to_date)
		{
			return FALSE;
		}
	}
	
	// skip early unwanted dates.
	if (_es_from_date != ES_UINT64_MAX)
	{
		if (change->timestamp < _es_from_date)
		{
			return TRUE;
		}
	}
	
	if (!(_es_action_filter & (1 << change->type)))
	{
		// continue...
		return TRUE;
	}
	
	// check filename filter.
	if (*_es_search_wcbuf->buf)
	{
		BOOL cmpret;
		utf8_buf_t filename_cbuf;
		wchar_buf_t filename_wcbuf;

		utf8_buf_init(&filename_cbuf);
		wchar_buf_init(&filename_wcbuf);

//	debug_printf("OLD NAME '%S'\n",filename_wcbuf.buf);			

		if ((change->new_path_len) || (change->new_name_len))
		{
			// check the new name only.
			// we don't care about the existing filename.
			utf8_buf_path_cat_filename(change->new_path_len ? change->new_path : change->old_path,change->new_name_len ? change->new_name : change->old_name,&filename_cbuf);
			wchar_buf_copy_lowercase_utf8_string(&filename_wcbuf,filename_cbuf.buf);
			
//	debug_printf("NEW NAME '%S'\n",filename_wcbuf.buf);			
			
			cmpret = wchar_string_wildcard_exec(filename_wcbuf.buf,_es_search_wcbuf->buf);
		}
		else
		{
			utf8_buf_path_cat_filename(change->old_path,change->old_name,&filename_cbuf);
			wchar_buf_copy_lowercase_utf8_string(&filename_wcbuf,filename_cbuf.buf);
		
			cmpret = wchar_string_wildcard_exec(filename_wcbuf.buf,_es_search_wcbuf->buf);
		}
		
		wchar_buf_kill(&filename_wcbuf);
		utf8_buf_kill(&filename_cbuf);

		if (!cmpret)
		{
			// continue..
			return TRUE;
		}
	}
	
	if (_es_watch)
	{
		_es_output_noncell_printf("%I64u %I64u\r\n",change->journal_id,change->change_id);
		
		ExitProcess(0);
	}
	
	// did first.
	is_first = 0;
	if (param->state == 1)
	{
		is_first = 1;
		param->state = 2;
	}
	
	_es_output_line_begin(is_first);
	
	_es_output_column = column_order_start;
	
	while(_es_output_column)
	{
		_es_output_cell_separator();

		switch(_es_output_column->property_id)
		{
			case EVERYTHING3_PROPERTY_ID_PARENT_FILE_ID:
				_es_output_cell_number_property(change->journal_id,ES_UINT64_MAX,0);
				break;
				
			case EVERYTHING3_PROPERTY_ID_FILE_ID:
				// 1-based.
				_es_output_cell_number_property(change->change_id + 1,ES_UINT64_MAX,0);
				break;
			
			case EVERYTHING3_PROPERTY_ID_DATE_CHANGED:
				_es_output_cell_filetime_property(change->timestamp);
				break;
				
			case EVERYTHING3_PROPERTY_ID_DATE_INDEXED:
				_es_output_cell_filetime_property(change->source_timestamp);
				break;
				
			case EVERYTHING3_PROPERTY_ID_TYPE:

				// this should always be text.
				{
					const ES_UTF8 *type_text;
					
					type_text = "";
					
					switch(change->type)
					{
						case IPC3_JOURNAL_ITEM_TYPE_FOLDER_CREATE:
							type_text = "Folder Create";
							break;
							
						case IPC3_JOURNAL_ITEM_TYPE_FOLDER_DELETE:
							type_text = "Folder Delete";
							break;
							
						case IPC3_JOURNAL_ITEM_TYPE_FOLDER_RENAME:
							type_text = "Folder Rename";
							break;
							
						case IPC3_JOURNAL_ITEM_TYPE_FOLDER_MOVE:
							type_text = "Folder Move";
							break;
							
						case IPC3_JOURNAL_ITEM_TYPE_FOLDER_MODIFY:
							type_text = "Folder Modify";
							break;
							
						case IPC3_JOURNAL_ITEM_TYPE_FILE_CREATE:
							type_text = "File Create";
							break;
							
						case IPC3_JOURNAL_ITEM_TYPE_FILE_DELETE:
							type_text = "File Delete";
							break;
							
						case IPC3_JOURNAL_ITEM_TYPE_FILE_RENAME:
							type_text = "File Rename";
							break;
							
						case IPC3_JOURNAL_ITEM_TYPE_FILE_MOVE:
							type_text = "File Move";
							break;
							
						case IPC3_JOURNAL_ITEM_TYPE_FILE_MODIFY:
							type_text = "File Modify";
							break;
							
					}

					_es_output_cell_text_property_utf8_string(type_text);
				}
				
				break;
				
			case EVERYTHING3_PROPERTY_ID_PATH_AND_NAME:

				{
					utf8_buf_t filename_cbuf;

					utf8_buf_init(&filename_cbuf);

					utf8_buf_path_cat_filename(change->old_path,change->old_name,&filename_cbuf);
					
					// append slash
					if (_es_folder_append_path_separator)
					{
						if (ipc3_journal_action_is_folder(change->type))
						{
							utf8_buf_cat_path_separator(&filename_cbuf);
						}
					}

					_es_output_cell_text_property_utf8_string_n(filename_cbuf.buf,filename_cbuf.length_in_bytes);

					utf8_buf_kill(&filename_cbuf);
				}

				break;	
			
			case EVERYTHING3_PROPERTY_ID_DATE_RECENTLY_CHANGED:
				_es_output_cell_filetime_property(change->old_parent_date_modified);
				break;
				
			case EVERYTHING3_PROPERTY_ID_SIZE:
				_es_output_cell_size_property(change->size);
				break;
				
			case EVERYTHING3_PROPERTY_ID_DATE_CREATED:
				_es_output_cell_filetime_property(change->date_created);
				break;
				
			case EVERYTHING3_PROPERTY_ID_DATE_MODIFIED:
				_es_output_cell_filetime_property(change->date_modified);
				break;
				
			case EVERYTHING3_PROPERTY_ID_DATE_ACCESSED:
				_es_output_cell_filetime_property(change->date_accessed);
				break;
						
			case EVERYTHING3_PROPERTY_ID_ATTRIBUTES:
				_es_output_cell_attribute_property(change->attributes);
				break;
						
			case EVERYTHING3_PROPERTY_ID_DATE_RUN:
				_es_output_cell_filetime_property(change->new_parent_date_modified);
				break;
					
			case EVERYTHING3_PROPERTY_ID_FILE_LIST_PATH_AND_NAME:

				{
					utf8_buf_t filename_cbuf;

					utf8_buf_init(&filename_cbuf);

					utf8_buf_path_cat_filename(change->new_path,change->new_name,&filename_cbuf);
					
					// append slash
					if (_es_folder_append_path_separator)
					{
						if (ipc3_journal_action_is_folder(change->type))
						{
							utf8_buf_cat_path_separator(&filename_cbuf);
						}
					}

					_es_output_cell_text_property_utf8_string_n(filename_cbuf.buf,filename_cbuf.length_in_bytes);

					utf8_buf_kill(&filename_cbuf);
				}

				break;
						
			default:
				_es_output_cell_unknown_property();
				break;
		}

		_es_output_column = _es_output_column->order_next;
	}

	param->numitems++;

	// there's no way to know if this is the last line.
	_es_output_line_end(!is_last);
	
	if (is_last)
	{
		return FALSE;
	}
	
//	_es_output_noncell_printf("%I64u %I64u %u %s \\ %s\r\n",change->journal_id,change->change_id,change->type,);

	return TRUE;
}

static const wchar_t *_es_parse_date_word(const wchar_t *s,int digit_count,WORD *out_value)
{
	const wchar_t *p;
	WORD value;
	int digit_run;

	p = s;
	value = 0;
	digit_run = digit_count;
	
	while(digit_run)
	{
		if (!((*p >= '0') && (*p <= '9')))
		{
			if (value)
			{
				return p;
			}
			
			return NULL;
		}
		
		value *= 10;
		value += *p - '0';
	
		p++;
		digit_run--;
	}
	
	*out_value = value;
	
	return p;
}

// returns ES_UINT64_MAX on error.
// returns the date string as a FILETIME.
// throws a fatal error if date_string is bad.
static ES_UINT64 _es_parse_date(const wchar_t *date_string)
{
	SYSTEMTIME st;
	const wchar_t *p;
	
	os_zero_memory(&st,sizeof(SYSTEMTIME));
	
	// YYYY[-]MM[-]DD
	p = _es_parse_date_word(date_string,4,&st.wYear);
	if (p)
	{
		if (*p)
		{
			if (*p == '-')
			{
				p++;
			}

			p = _es_parse_date_word(p,2,&st.wMonth);
			if (p)
			{
				if (*p)
				{
					if (*p == '-')
					{
						p++;
					}
					
					p = _es_parse_date_word(p,2,&st.wDay);
					if (p)
					{
						if ((*p == 'T') || (*p == 't'))
						{
							p++;

							// HH[[:]mm[[:]ss]] time
							
							p = _es_parse_date_word(p,2,&st.wHour);
							if (!p)
							{
								goto bad_date;
							}
							
							if (*p)
							{
								if (*p == ':')
								{
									p++;
								}
								
								p = _es_parse_date_word(p,2,&st.wMinute);
								if (!p)
								{
									goto bad_date;
								}
									
								if (*p)
								{
									if (*p == ':')
									{
										p++;
									}
									
									p = _es_parse_date_word(p,2,&st.wSecond);
									if (!p)
									{
										goto bad_date;
									}
								}
							}
						}

						if (*p == 0)
						{
							return os_localtime_to_filetime(&st);
						}
					}
				}
			}
		}
	}

bad_date:
	
	_es_bad_switch_param("Unable to parse date: %S\n",date_string);
	
	return ES_UINT64_MAX;
}

// the "system." prefix will not conflict with any of our switches, so assume the user wants a property system property.
// eg: es.exe -system.size
static BOOL _es_should_allow_property_system(const wchar_t *name)
{
	if (wchar_string_parse_nocase_lowercase_ascii_string(name,"system."))
	{
		return TRUE;
	}
	
	return FALSE;
}

static DWORD _es_parse_journal_action_filter(const wchar_t *filter_list)
{
	wchar_buf_t item_wcbuf;
	const wchar_t *filter_p;
	DWORD action_filter;

	wchar_buf_init(&item_wcbuf);
	filter_p = filter_list;
	action_filter = 0;
	
	for(;;)
	{
		filter_p = wchar_buf_parse_list_item(filter_p,&item_wcbuf);
		if (!filter_p)
		{
			break;
		}
		
		if (item_wcbuf.length_in_wchars)
		{
			int action;
			
			action = ipc3_journal_item_type_from_name(item_wcbuf.buf);
			if (action)
			{
				action_filter |= (1 << action);
			}
			else
			{
				_es_bad_switch_param("Unknown action: '%S'\n",item_wcbuf.buf);
			}
		}
	}

	wchar_buf_kill(&item_wcbuf);
	
	return action_filter;
}

static ES_UINT64 _es_get_today_filetime(void)
{
	SYSTEMTIME now_local_st;
	SYSTEMTIME today_local_st;
	ES_UINT64 today_ft;

	// i just want to see changes from today and keep monitoring...
	GetLocalTime(&now_local_st);

	os_zero_memory(&today_local_st,sizeof(SYSTEMTIME));

	today_local_st.wYear = now_local_st.wYear;
	today_local_st.wMonth = now_local_st.wMonth;
	today_local_st.wDay = now_local_st.wDay;

	today_ft = os_localtime_to_filetime(&today_local_st);
	if (today_ft == ES_UINT64_MAX)
	{
		// system error.
		es_fatal(ES_ERROR_OUT_OF_MEMORY);
	}

	return today_ft;
}

static ES_UINT64 _es_get_today_offset_filetime(__int64 offset)
{
	SYSTEMTIME now_local_st;
	SYSTEMTIME today_local_st;
	SYSTEMTIME tomorrow_st;
	ES_UINT64 today_system_ft;
	ES_UINT64 tomorrow_system_ft;
	ES_UINT64 tomorrow_ft;
	
	GetLocalTime(&now_local_st);
	
	os_zero_memory(&today_local_st,sizeof(SYSTEMTIME));
	
	today_local_st.wYear = now_local_st.wYear;
	today_local_st.wMonth = now_local_st.wMonth;
	today_local_st.wDay = now_local_st.wDay;
	
	if (!SystemTimeToFileTime(&today_local_st,(FILETIME *)&today_system_ft))
	{
		// system error.
		es_fatal(ES_ERROR_OUT_OF_MEMORY);
	}

	tomorrow_system_ft = today_system_ft + offset;

	if (!FileTimeToSystemTime((FILETIME *)&tomorrow_system_ft,&tomorrow_st))
	{
		// system error.
		es_fatal(ES_ERROR_OUT_OF_MEMORY);
	}
	
	tomorrow_st.wHour = 0;
	tomorrow_st.wMinute = 0;
	tomorrow_st.wSecond = 0;
	tomorrow_st.wMilliseconds = 0;
	
	tomorrow_ft = os_localtime_to_filetime(&tomorrow_st);
	if (tomorrow_ft == ES_UINT64_MAX)
	{
		// system error.
		es_fatal(ES_ERROR_OUT_OF_MEMORY);
	}
	
	return tomorrow_ft;
}

static ES_UINT64 _es_get_tomorrow_filetime(void)
{
	return _es_get_today_offset_filetime(864000000000I64);
}

static ES_UINT64 _es_get_yesterday_filetime(void)
{
	return _es_get_today_offset_filetime(-864000000000I64);
}