
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
//
// TODO:
// [HIGH] c# cmdlet for powershell.
// [HIGH] output as JSON ./ES.exe | ConvertFrom-Json
// [BUG] exporting as EFU should not use localized dates (only CSV should do this). EFU should be raw FILETIMEs.
// add a -max-path option. (limit paths to 259 chars -use shortpaths if we exceed.)
// add a -short-full-path option.
// -wait block until search results one result, combine with new-results-only:
// open to show summary information: total size and number of results (like -get-result-count)
// -export-m3u/-export-m3u8 need to use short paths for VLFNs. -No media players support VLFNs.
// ideally, -sort should apply the sort AFTER the search. -currently filters are overwriting the sort. -this needs to be done at the query level so we dont block with a sort. -add a sort-after parameter...
// export to clipboard (aka copy to clipboard)
// add a -unquote command line option so we can handle powershell, eg: es.exe -unquote case:`"$x`" => es.exe -unquote "case:"abc 123"" => es.exe case:"abc 123"
// export results to environment variables -mostly useful for -n 1
// Use system number formatting (some localizations don't use , for the comma)
// Show the sum of the size.
// add an option to append a path separator for folders.
// add a -no-config command line option to bypass loading of settings.
// would be nice if it had -POSIX and -NUL switches to generate NUL for RG/GREP
// export as TSV to match Everything export.
// -save-db -save database to disk command line option.
// -rebuild -force a rebuild
// inc-run-count/set-run-count should allow -instance
// fix background highlight when a result spans multiple lines, eg: -highlight -highlight-color 0xa0
// es sonic -name -path-column -name-color 13 -highlight // -name-color 13 doesn't work with -highlight !
// improve -pause when the window is resized.
// -v to display everything version?
// -path is not working with -r It is using the <"filter"> (-path is adding brackets and double quotes, neither of which regex understands)
// Gather file information if it is not indexed.
// path ellipsis?
// -get-size <filename> - expand -get-run-count to get other information for a single file? -what happens when the file doesn't exist?
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
// custom date/time format.

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

#define ES_EXPORT_BUF_SIZE			65536

#define ES_PAUSE_TEXT		"ESC=Quit; Up,Down,Left,Right,Page Up,Page Down,Home,End=Scroll"
#define ES_BLANK_PAUSE_TEXT	"                                                              "

#define ES_IPC_VERSION_FLAG_IPC1	0x00000001
#define ES_IPC_VERSION_FLAG_IPC2	0x00000002
#define ES_IPC_VERSION_FLAG_IPC3	0x00000004

// IPC pipe commands.
#define _EVERYTHING3_COMMAND_GET_IPC_PIPE_VERSION			0
#define _EVERYTHING3_COMMAND_GET_MAJOR_VERSION				1
#define _EVERYTHING3_COMMAND_GET_MINOR_VERSION				2
#define _EVERYTHING3_COMMAND_GET_REVISION					3
#define _EVERYTHING3_COMMAND_GET_BUILD_NUMBER				4
#define _EVERYTHING3_COMMAND_GET_TARGET_MACHINE				5
#define _EVERYTHING3_COMMAND_FIND_PROPERTY_FROM_NAME		6
#define _EVERYTHING3_COMMAND_SEARCH							7
#define _EVERYTHING3_COMMAND_IS_DB_LOADED					8
#define _EVERYTHING3_COMMAND_IS_PROPERTY_INDEXED			9
#define _EVERYTHING3_COMMAND_IS_PROPERTY_FAST_SORT			10
#define _EVERYTHING3_COMMAND_GET_PROPERTY_NAME				11
#define _EVERYTHING3_COMMAND_GET_PROPERTY_CANONICAL_NAME	12
#define _EVERYTHING3_COMMAND_GET_PROPERTY_TYPE				13
#define _EVERYTHING3_COMMAND_IS_RESULT_CHANGE				14
#define _EVERYTHING3_COMMAND_GET_RUN_COUNT					15
#define _EVERYTHING3_COMMAND_SET_RUN_COUNT					16
#define _EVERYTHING3_COMMAND_INC_RUN_COUNT					17
#define _EVERYTHING3_COMMAND_GET_FOLDER_SIZE				18
#define _EVERYTHING3_COMMAND_GET_FILE_ATTRIBUTES			19
#define _EVERYTHING3_COMMAND_GET_FILE_ATTRIBUTES_EX			20
#define _EVERYTHING3_COMMAND_GET_FIND_FIRST_FILE			21
#define _EVERYTHING3_COMMAND_GET_RESULTS					22
#define _EVERYTHING3_COMMAND_SORT							23
#define _EVERYTHING3_COMMAND_WAIT_FOR_RESULT_CHANGE			24
#define _EVERYTHING3_COMMAND_CANCEL							25

// IPC pipe responses
#define _EVERYTHING3_RESPONSE_OK_MORE_DATA					100 // expect another repsonse.
#define _EVERYTHING3_RESPONSE_OK							200 // reply data depending on request.
#define _EVERYTHING3_RESPONSE_ERROR_BAD_REQUEST				400
#define _EVERYTHING3_RESPONSE_ERROR_CANCELLED				401 // another requested was made while processing...
#define _EVERYTHING3_RESPONSE_ERROR_NOT_FOUND				404
#define _EVERYTHING3_RESPONSE_ERROR_OUT_OF_MEMORY			500
#define _EVERYTHING3_RESPONSE_ERROR_INVALID_COMMAND			501

#define _EVERYTHING3_SEARCH_FLAG_MATCH_CASE					0x00000001	// match case
#define _EVERYTHING3_SEARCH_FLAG_MATCH_WHOLEWORD			0x00000002	// match whole word
#define _EVERYTHING3_SEARCH_FLAG_MATCH_PATH					0x00000004	// include paths in search
#define _EVERYTHING3_SEARCH_FLAG_REGEX						0x00000008	// enable regex
#define _EVERYTHING3_SEARCH_FLAG_MATCH_DIACRITICS			0x00000010	// match diacritic marks
#define _EVERYTHING3_SEARCH_FLAG_MATCH_PREFIX				0x00000020	// match prefix (Everything 1.5)
#define _EVERYTHING3_SEARCH_FLAG_MATCH_SUFFIX				0x00000040	// match suffix (Everything 1.5)
#define _EVERYTHING3_SEARCH_FLAG_IGNORE_PUNCTUATION			0x00000080	// ignore punctuation (Everything 1.5)
#define _EVERYTHING3_SEARCH_FLAG_IGNORE_WHITESPACE			0x00000100	// ignore white-space (Everything 1.5)
#define _EVERYTHING3_SEARCH_FLAG_FOLDERS_FIRST_ASCENDING	0x00000000	// folders first when sort ascending
#define _EVERYTHING3_SEARCH_FLAG_FOLDERS_FIRST_ALWAYS		0x00000200	// folders first
#define _EVERYTHING3_SEARCH_FLAG_FOLDERS_FIRST_NEVER		0x00000400	// folders last 
#define _EVERYTHING3_SEARCH_FLAG_FOLDERS_FIRST_DESCENDING	0x00000600	// folders first when sort descending
#define _EVERYTHING3_SEARCH_FLAG_TOTAL_SIZE					0x00000800	// calculate total size
#define _EVERYTHING3_SEARCH_FLAG_HIDE_RESULT_OMISSIONS		0x00001000	// hide omitted results
#define _EVERYTHING3_SEARCH_FLAG_SORT_MIX					0x00002000	// mix file and folder results
#define _EVERYTHING3_SEARCH_FLAG_64BIT						0x00004000	// SIZE_T is 64bits. Otherwise, 32bits.
#define _EVERYTHING3_SEARCH_FLAG_FORCE						0x00008000	// Force a research, even when the search state doesn't change.

#define _EVERYTHING3_MESSAGE_DATA(msg)						((void *)(((_everything3_message_t *)(msg)) + 1))

#define _EVERYTHING3_SEARCH_PROPERTY_REQUEST_FLAG_FORMAT	0x00000001
#define _EVERYTHING3_SEARCH_PROPERTY_REQUEST_FLAG_HIGHLIGHT	0x00000002

#define _EVERYTHING3_RESULT_LIST_ITEM_FLAG_FOLDER			0x01
#define _EVERYTHING3_RESULT_LIST_ITEM_FLAG_ROOT				0x02

// Everything3_GetResultListPropertyRequestValueType()
#define	EVERYTHING3_PROPERTY_VALUE_TYPE_NULL				0
#define	EVERYTHING3_PROPERTY_VALUE_TYPE_BYTE				1 // Everything3_GetResultPropertyBYTE
#define	EVERYTHING3_PROPERTY_VALUE_TYPE_WORD				2 // Everything3_GetResultPropertyWORD
#define	EVERYTHING3_PROPERTY_VALUE_TYPE_DWORD				3 // Everything3_GetResultPropertyDWORD
#define	EVERYTHING3_PROPERTY_VALUE_TYPE_DWORD_FIXED_Q1K		4 // Everything3_GetResultPropertyDWORD / 1000
#define	EVERYTHING3_PROPERTY_VALUE_TYPE_UINT64				5 // Everything3_GetResultPropertyUINT64
#define	EVERYTHING3_PROPERTY_VALUE_TYPE_UINT128				6 // Everything3_GetResultPropertyUINT128
#define	EVERYTHING3_PROPERTY_VALUE_TYPE_DIMENSIONS			7 // Everything3_GetResultPropertyDIMENSIONS
#define	EVERYTHING3_PROPERTY_VALUE_TYPE_PSTRING				8 // Everything3_GetResultPropertyText
#define	EVERYTHING3_PROPERTY_VALUE_TYPE_PSTRING_MULTISTRING	9 // Everything3_GetResultPropertyText
#define	EVERYTHING3_PROPERTY_VALUE_TYPE_PSTRING_STRING_REFERENCE	10 // Everything3_GetResultPropertyText
#define	EVERYTHING3_PROPERTY_VALUE_TYPE_SIZE_T				11 // Everything3_GetResultPropertySIZE_T
#define	EVERYTHING3_PROPERTY_VALUE_TYPE_INT32_FIXED_Q1K		12 // Everything3_GetResultPropertyINT32 / 1000
#define	EVERYTHING3_PROPERTY_VALUE_TYPE_INT32_FIXED_Q1M		13 // Everything3_GetResultPropertyINT32 / 1000000
#define	EVERYTHING3_PROPERTY_VALUE_TYPE_PSTRING_FOLDER_REFERENCE	14 // Everything3_GetResultPropertyText
#define	EVERYTHING3_PROPERTY_VALUE_TYPE_PSTRING_FILE_OR_FOLDER_REFERENCE	15 // Everything3_GetResultPropertyText
#define	EVERYTHING3_PROPERTY_VALUE_TYPE_BLOB8				16 // Everything3_GetResultPropertyBlob
#define	EVERYTHING3_PROPERTY_VALUE_TYPE_DWORD_GET_TEXT		17 // Everything3_GetResultPropertyDWORD
#define	EVERYTHING3_PROPERTY_VALUE_TYPE_WORD_GET_TEXT		18 // Everything3_GetResultPropertyWORD
#define	EVERYTHING3_PROPERTY_VALUE_TYPE_BLOB16				19 // Everything3_GetResultPropertyBlob
#define	EVERYTHING3_PROPERTY_VALUE_TYPE_BYTE_GET_TEXT		20 // Everything3_GetResultPropertyBYTE
#define	EVERYTHING3_PROPERTY_VALUE_TYPE_PROPVARIANT			21 // Everything3_GetResultPropertyPropVariant

#define _ES_MSGFLT_ALLOW		1

typedef struct _es_tagCHANGEFILTERSTRUCT 
{
	DWORD cbSize;
	DWORD ExtStatus;
}_ES_CHANGEFILTERSTRUCT;

// IPC pipe message
typedef struct _everything3_message_s
{
	DWORD code; // _EVERYTHING3_COMMAND_* or _EVERYTHING3_RESPONSE_*
	DWORD size; // excludes header size.
	
	// data follows
	// BYTE data[size];
	
}_everything3_message_t;

// recv stream.
typedef struct _everything3_stream_s
{
	HANDLE pipe_handle;
	BYTE *buf;
	BYTE *p;
	SIZE_T avail;
	int is_error;
	int got_last;
	int is_64bit;
		
}_everything3_stream_t;

typedef struct _everything3_result_list_property_request_s
{
	DWORD property_id;
	// one or more of _EVERYTHING3_SEARCH_PROPERTY_REQUEST_FLAG_*
	DWORD flags;
	DWORD value_type;
	
}_everything3_result_list_property_request_t;

static int _es_main(void);
static void es_write_wchar_string(const wchar_t *text);
static void es_write_utf8_string(const ES_UTF8 *text);
void es_fwrite_utf8_string(const ES_UTF8 *text);
void es_fwrite_wchar_string(const wchar_t *text);
void es_fwrite_n(const wchar_t *text,int wlen);
void es_write_wchar(wchar_t ch);
void es_write_highlighted(const wchar_t *text);
void es_write_UINT64(ES_UINT64 value);
void es_write_dword(DWORD value);
void es_format_number(ES_UINT64 number,wchar_buf_t *wcbuf);
static BOOL es_sendquery(void);
static BOOL es_sendquery2(void);
static BOOL es_sendquery3(void);
int es_compare_list_items(const EVERYTHING_IPC_ITEM *a,const EVERYTHING_IPC_ITEM *b);
static void es_write_result_count(DWORD result_count);
static void es_output_text_property(const wchar_t *value);
static void es_output_attributes(DWORD file_attributes);
static void es_output_separator(void);
static void es_output_newline(void);
void es_listresultsW(EVERYTHING_IPC_LIST *list);
void es_listresults2W(EVERYTHING_IPC_LIST2 *list,int index_start,int count);
void es_write_total_size(EVERYTHING_IPC_LIST2 *list,int count);
LRESULT __stdcall es_window_proc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam);
void es_help(void);
HWND es_find_ipc_window(void);
int es_check_option_utf8_string(const wchar_t *param,const ES_UTF8 *s);
int es_check_option_wchar_string(const wchar_t *param,const wchar_t *s);
void *es_get_column_data(EVERYTHING_IPC_LIST2 *list,int index,DWORD property_id,DWORD property_highlight);
void es_format_dir(wchar_buf_t *wcbuf);
void es_format_size(ES_UINT64 size,wchar_buf_t *wcbuf);
void es_format_filetime(ES_UINT64 filetime,wchar_buf_t *wcbuf);
void es_format_attributes(DWORD attributes,wchar_buf_t *wcbuf);
int es_filetime_to_localtime(SYSTEMTIME *localst,ES_UINT64 ft);
void es_format_leading_space(wchar_t *buf,int size,int ch);
void es_space_to_width(wchar_t *buf,int wide);
void es_format_run_count(DWORD run_count,wchar_buf_t *wcbuf);
int es_check_option_wchar_string_name(const wchar_t *argv,const wchar_t *s);
int es_check_param_ascii(const wchar_t *s,const char *search);
void es_flush(void);
void es_fwrite_csv_string(const wchar_t *s);
void es_fwrite_csv_string_with_optional_quotes(int is_always_double_quote,int separator_ch,const wchar_t *s);
void es_get_command_argv(wchar_buf_t *wcbuf);
void es_expect_command_argv(wchar_buf_t *wcbuf);
void es_get_argv(wchar_buf_t *wcbuf);
void es_expect_argv(wchar_buf_t *wcbuf);
void es_fill(int count,int utf8ch);
int es_is_valid_key(INPUT_RECORD *ir);
static BOOL _es_save_settings_with_filename(const wchar_t *filename);
static BOOL _es_save_settings_with_appdata(int is_appdata);
static BOOL _es_save_settings(void);
static void _es_load_settings(void);
void es_append_filter(wchar_buf_t *wcbuf,const wchar_t *filter);
void es_do_run_history_command(void);
static BOOL es_check_sorts(const wchar_t *argv);
static void es_wait_for_db_loaded(void);
static void es_wait_for_db_not_busy(void);
static int es_is_literal_switch(const wchar_t *s);
static int _es_should_quote(int separator_ch,const wchar_t *s);
static int _es_is_unbalanced_quotes(const wchar_t *s);
static void es_get_pipe_name(wchar_buf_t *wcbuf);
static BYTE *es_copy_len_vlq(BYTE *buf,SIZE_T value);
static BYTE *es_copy_dword(BYTE *buf,DWORD value);
static BYTE *es_copy_uint64(BYTE *buf,ES_UINT64 value);
static BYTE *es_copy_size_t(BYTE *buf,SIZE_T value);
static BOOL es_write_pipe_data(HANDLE h,const void *in_data,SIZE_T in_size);
static BOOL es_write_pipe_message(HANDLE h,DWORD code,const void *in_data,SIZE_T in_size);
static void _everything3_stream_read_data(_everything3_stream_t *stream,void *data,SIZE_T size);
static BYTE _everything3_stream_read_byte(_everything3_stream_t *stream);
static WORD _everything3_stream_read_word(_everything3_stream_t *stream);
static DWORD _everything3_stream_read_dword(_everything3_stream_t *stream);
static ES_UINT64 _everything3_stream_read_uint64(_everything3_stream_t *stream);
static SIZE_T _everything3_stream_read_size_t(_everything3_stream_t *stream);
static SIZE_T _everything3_stream_read_len_vlq(_everything3_stream_t *stream);
static BOOL es_read_pipe(HANDLE pipe_handle,void *buf,SIZE_T buf_size);
static BOOL es_skip_pipe(HANDLE pipe_handle,SIZE_T buf_size);
static int es_property_name_to_id_compare(const char *a,const char *name);
//static DWORD es_property_id_from_name(const char *name);
static BOOL es_column_is_right_aligned(DWORD property_id);
static BOOL es_check_color_param(const wchar_t *argv,int *out_basic_property_name_i);
static BOOL es_check_column_param(const wchar_t *argv);
static int es_get_ipc_sort_type_from_property_id(DWORD property_id,int sort_ascending);
static void es_column_color_clear_all(void);
static void es_column_width_clear_all(void);
static void es_get_window_classname(wchar_buf_t *wcbuf);
static HANDLE es_connect_ipc_pipe(void);
static void es_get_folder_size(const wchar_t *filename);
static void es_get_reply_window(void);
static BOOL _es_load_settings_with_filename(const wchar_t *filename);
static BOOL _es_load_settings_with_appdata(int is_appdata);

static DWORD es_sort_property_id = EVERYTHING3_PROPERTY_ID_NAME;
static char es_sort_ascending = 0; // 0 = default, >0 = ascending, <0 = descending
static const EVERYTHING_IPC_LIST *es_sort_list;
static BOOL (WINAPI *es_pChangeWindowMessageFilterEx)(HWND hWnd,UINT message,DWORD action,_ES_CHANGEFILTERSTRUCT *pChangeFilterStruct) = 0;
static int es_highlight_color = FOREGROUND_GREEN|FOREGROUND_INTENSITY;
static char es_highlight = 0;
static char es_match_whole_word = 0;
static char es_match_path = 0;
static char es_match_case = 0;
static char es_match_diacritics = 0;
static char es_exit_everything = 0;
static char es_reindex = 0;
static char es_save_db = 0;
static BYTE es_export_type = ES_EXPORT_TYPE_NONE;
static HANDLE es_export_file = INVALID_HANDLE_VALUE;
static BYTE *es_export_buf = 0;
static BYTE *es_export_p;
static int es_export_remaining = 0;
static char es_size_leading_zero = 0;
static char es_run_count_leading_zero = 0;
static char es_digit_grouping = 1;
static DWORD es_offset = 0;
static DWORD es_max_results = EVERYTHING_IPC_ALLRESULTS;
static DWORD es_ret = ES_ERROR_SUCCESS;
static const wchar_t *es_command_line = 0;
static WORD es_last_color;
static char es_is_last_color = 0;
static BYTE es_size_format = 1; // 0 = auto, 1=bytes, 2=kb
static BYTE es_date_format = 0; // 0 = system format, 1=iso-8601 (as local time), 2=filetime in decimal, 3=iso-8601 (in utc)
static CHAR_INFO *es_cibuf = 0;
static int es_max_wide = 0;
static int es_console_wide = 80;
static int es_console_high = 25;
static int es_console_size_high = 25;
static int es_console_window_x = 0;
static int es_console_window_y = 0;
static char es_pause = 0; 
static int es_cibuf_attributes = 0x07;
static int es_cibuf_hscroll = 0;
static int es_cibuf_x = 0;
static int es_cibuf_y = 0;
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
static char es_header = 1;
static char es_double_quote = 0; // always use double quotes for filenames.
static char es_csv_double_quote = 1; // always use double quotes for CSV for consistancy.
static char es_get_everything_version = 0;
static char es_utf8_bom = 0;
static wchar_buf_t *es_instance_name_wcbuf = NULL;
static wchar_buf_t *es_search_wcbuf = NULL;
static HWND es_reply_hwnd = 0;
static char es_loaded_appdata_ini = 0; // loaded settings from appdata, we should save to appdata.

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
static BOOL es_sendquery(void)
{
	EVERYTHING_IPC_QUERY *query;
	SIZE_T search_length_in_wchars;
	SIZE_T size;
	BOOL ret;
	utf8_buf_t cbuf;
	
	ret = FALSE;
	utf8_buf_init(&cbuf);
	
	es_get_reply_window();
	
	search_length_in_wchars = es_search_wcbuf->length_in_wchars;
	
	// EVERYTHING_IPC_QUERY includes the NULL terminator.
	size = 	sizeof(EVERYTHING_IPC_QUERY);
	size = safe_size_add(size,safe_size_mul_sizeof_wchar(search_length_in_wchars));
	
	utf8_buf_grow_size(&cbuf,size);
	query = (EVERYTHING_IPC_QUERY *)cbuf.buf;

	if (es_get_result_count)
	{
		es_max_results = 0;
	}

	query->max_results = es_max_results;
	query->offset = 0;
	query->reply_copydata_message = ES_COPYDATA_IPCTEST_QUERYCOMPLETEW;
	query->search_flags = (es_match_case?EVERYTHING_IPC_MATCHCASE:0) | (es_match_diacritics?EVERYTHING_IPC_MATCHACCENTS:0) | (es_match_whole_word?EVERYTHING_IPC_MATCHWHOLEWORD:0) | (es_match_path?EVERYTHING_IPC_MATCHPATH:0);
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
			ret = TRUE;
		}
	}

	utf8_buf_kill(&cbuf);

	return ret;
}

// query everything with search string
static BOOL es_sendquery2(void)
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

	es_get_reply_window();
	
	search_length_in_wchars = es_search_wcbuf->length_in_wchars;
	
	size = sizeof(EVERYTHING_IPC_QUERY2);
	size = safe_size_add(size,safe_size_mul_sizeof_wchar(safe_size_add_one(search_length_in_wchars)));

	utf8_buf_grow_size(&cbuf,size);
	query = (EVERYTHING_IPC_QUERY2 *)cbuf.buf;

	request_flags = 0;
	
	{
		column_t *column;

		column = column_order_start;
		
		while(column)
		{
			switch(column->property_id)
			{
				case EVERYTHING3_PROPERTY_ID_FULL_PATH:
					if (column->flags & COLUMN_FLAG_HIGHLIGHT)
					{
						request_flags |= EVERYTHING_IPC_QUERY2_REQUEST_HIGHLIGHTED_FULL_PATH_AND_NAME;
					}
					else
					{
						request_flags |= EVERYTHING_IPC_QUERY2_REQUEST_FULL_PATH_AND_NAME;
					}
					break;
					
				case EVERYTHING3_PROPERTY_ID_NAME:
					if (column->flags & COLUMN_FLAG_HIGHLIGHT)
					{
						request_flags |= EVERYTHING_IPC_QUERY2_REQUEST_HIGHLIGHTED_NAME;
					}
					else
					{
						request_flags |= EVERYTHING_IPC_QUERY2_REQUEST_NAME;
					}
					break;
					
				case EVERYTHING3_PROPERTY_ID_PATH:
					if (column->flags & COLUMN_FLAG_HIGHLIGHT)
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

				case EVERYTHING3_PROPERTY_ID_FILE_LIST_FILENAME:
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
		es_max_results = 0;
		request_flags = 0;
	}
	
	if (es_get_total_size)
	{
		// we only want size.
		request_flags = EVERYTHING_IPC_QUERY2_REQUEST_SIZE;
		es_max_results = EVERYTHING_IPC_ALLRESULTS;
	}
	
	query->max_results = es_max_results;
	query->offset = es_offset;
	query->reply_copydata_message = ES_COPYDATA_IPCTEST_QUERYCOMPLETE2W;
	query->search_flags = (es_match_case?EVERYTHING_IPC_MATCHCASE:0) | (es_match_diacritics?EVERYTHING_IPC_MATCHACCENTS:0) | (es_match_whole_word?EVERYTHING_IPC_MATCHWHOLEWORD:0) | (es_match_path?EVERYTHING_IPC_MATCHPATH:0);
	query->reply_hwnd = (DWORD)(uintptr_t)es_reply_hwnd;
	query->request_flags = request_flags;
	query->sort_type = es_get_ipc_sort_type_from_property_id(es_sort_property_id,es_sort_ascending);
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
			ret = TRUE;
		}
	}

	utf8_buf_kill(&cbuf);

	return ret;
}

static BOOL es_sendquery3(void)
{
	BOOL ret;
	HANDLE pipe_handle;
	
	ret = FALSE;

	pipe_handle = es_connect_ipc_pipe();
	if (pipe_handle != INVALID_HANDLE_VALUE)
	{
		DWORD search_flags;
		utf8_buf_t search_cbuf;
		BYTE *packet_data;
		SIZE_T packet_size;
		SIZE_T sort_count;
		SIZE_T property_request_count;

		utf8_buf_init(&search_cbuf);
	
		utf8_buf_copy_wchar_string(&search_cbuf,es_search_wcbuf->buf);

//TODO:
/*
			if (es_export_type == ES_EXPORT_TYPE_EFU)
			{
				// get indexed file info column for exporting.
				if (SendMessage(es_everything_hwnd,EVERYTHING_WM_IPC,EVERYTHING_IPC_IS_FILE_INFO_INDEXED,EVERYTHING_IPC_FILE_INFO_FILE_SIZE))
				{
					column_add(EVERYTHING3_PROPERTY_ID_SIZE);
				}
				
				if (SendMessage(es_everything_hwnd,EVERYTHING_WM_IPC,EVERYTHING_IPC_IS_FILE_INFO_INDEXED,EVERYTHING_IPC_FILE_INFO_DATE_MODIFIED))
				{
					column_add(EVERYTHING3_PROPERTY_ID_DATE_MODIFIED);
				}
				
				if (SendMessage(es_everything_hwnd,EVERYTHING_WM_IPC,EVERYTHING_IPC_IS_FILE_INFO_INDEXED,EVERYTHING_IPC_FILE_INFO_DATE_CREATED))
				{
					column_add(EVERYTHING3_PROPERTY_ID_DATE_CREATED);
				}
				
				if (SendMessage(es_everything_hwnd,EVERYTHING_WM_IPC,EVERYTHING_IPC_IS_FILE_INFO_INDEXED,EVERYTHING_IPC_FILE_INFO_ATTRIBUTES))
				{
					column_add(EVERYTHING3_PROPERTY_ID_ATTRIBUTES);
				}
			}
	*/
	
#if SIZE_MAX == 0xFFFFFFFFFFFFFFFFUI64

		search_flags = _EVERYTHING3_SEARCH_FLAG_64BIT;

#elif SIZE_MAX == 0xFFFFFFFF

		search_flags = 0;
	
#else
		#error unknown SIZE_MAX
#endif
		
		if (es_match_case)
		{
			search_flags |= _EVERYTHING3_SEARCH_FLAG_MATCH_CASE;
		}
		
		sort_count = 0;
		property_request_count = 0;
		
		// search_flags
		packet_size = sizeof(DWORD);
		
		// search_len
		packet_size = safe_size_add(packet_size,(SIZE_T)es_copy_len_vlq(NULL,search_cbuf.length_in_bytes));
		
		// search_text
		packet_size = safe_size_add(packet_size,search_cbuf.length_in_bytes);

		// view port offset
		packet_size = safe_size_add(packet_size,sizeof(SIZE_T));
		packet_size = safe_size_add(packet_size,sizeof(SIZE_T));

		// sort
		packet_size = safe_size_add(packet_size,(SIZE_T)es_copy_len_vlq(NULL,sort_count));
//		packet_size = safe_size_add(packet_size,sort_count * sizeof(es_search_sort_t));

		// property request 
		packet_size = safe_size_add(packet_size,(SIZE_T)es_copy_len_vlq(NULL,property_request_count));
//		packet_size = safe_size_add(packet_size,property_request_count * sizeof(es_search_property_request_t));
		
		packet_data = mem_alloc(packet_size);
		if (packet_data)
		{
			BYTE *packet_d;
			
			packet_d = packet_data;
			
			// search flags
			packet_d = es_copy_dword(packet_d,search_flags);
			
			// search text
			packet_d = es_copy_len_vlq(packet_d,search_cbuf.length_in_bytes);
			packet_d = os_copy_memory(packet_d,search_cbuf.buf,search_cbuf.length_in_bytes);

			// viewport
			packet_d = es_copy_size_t(packet_d,0);
			packet_d = es_copy_size_t(packet_d,es_max_results);

			// sort
			
			packet_d = es_copy_len_vlq(packet_d,sort_count);
/*
			{
				es_search_sort_t *sort_p;
				SIZE_T sort_run;
				
				sort_p = search_state->sort_array;
				sort_run = search_state->sort_count;
				
				while(sort_run)
				{
					packet_d = es_copy_dword(packet_d,sort_p->property_id);
					packet_d = es_copy_dword(packet_d,sort_p->flags);
					
					sort_p++;
					sort_run--;
				}
			}
*/
			// property requests

			packet_d = es_copy_len_vlq(packet_d,property_request_count);
/*
			{
				es_search_property_request_t *property_request_p;
				SIZE_T property_request_run;
				
				property_request_p = search_state->property_request_array;
				property_request_run = search_state->property_request_count;
				
				while(property_request_run)
				{
					packet_d = es_copy_dword(packet_d,property_request_p->property_id);
					packet_d = es_copy_dword(packet_d,property_request_p->flags);
					
					property_request_p++;
					property_request_run--;
				}
			}
*/
			DEBUG_ASSERT((packet_d - packet_data) == packet_size);
			
			if (es_write_pipe_message(pipe_handle,_EVERYTHING3_COMMAND_SEARCH,packet_data,packet_size))
			{
				_everything3_stream_t stream;
				SIZE_T size_t_size;
				ES_UINT64 total_result_size;
				DWORD valid_flags;
				SIZE_T folder_result_count;
				SIZE_T file_result_count;
				SIZE_T viewport_offset;
				SIZE_T viewport_count;
				SIZE_T sort_count;
				SIZE_T property_request_count;
				_everything3_result_list_property_request_t *property_request_array;
				
				total_result_size = ES_UINT64_MAX;
				property_request_array = NULL;
				
				stream.pipe_handle = pipe_handle;
				stream.buf = NULL;
				stream.p = NULL;
				stream.avail = 0;
				stream.is_error = 0;
				stream.got_last = 0;
				stream.is_64bit = 0;
				size_t_size = sizeof(DWORD);
				
				valid_flags = _everything3_stream_read_dword(&stream);

				if (valid_flags & _EVERYTHING3_SEARCH_FLAG_64BIT)
				{
					stream.is_64bit = 1;
					size_t_size = sizeof(ES_UINT64);
				}

				// result counts
				folder_result_count = _everything3_stream_read_size_t(&stream);
				file_result_count = _everything3_stream_read_size_t(&stream);
				
				// total size.
				if (valid_flags & _EVERYTHING3_SEARCH_FLAG_TOTAL_SIZE)
				{
					total_result_size = _everything3_stream_read_uint64(&stream);
				}
				
				// viewport
				viewport_offset = _everything3_stream_read_size_t(&stream);
				viewport_count = _everything3_stream_read_size_t(&stream);
				
				// sort
				sort_count = _everything3_stream_read_len_vlq(&stream);
				
				if (sort_count)
				{
					SIZE_T sort_run;
					
					sort_run = sort_count;
					
					while(sort_run)
					{
						DWORD sort_property_id;
						DWORD sort_flags;
						
						sort_property_id = _everything3_stream_read_dword(&stream);
						sort_flags = _everything3_stream_read_dword(&stream);
						
						sort_run--;
					}
				}

				// property requests
				property_request_count = _everything3_stream_read_len_vlq(&stream);
				
				if (property_request_count)
				{
					SIZE_T property_request_size;
					
					property_request_size = property_request_count;
					
					DEBUG_ASSERT(sizeof(_everything3_result_list_property_request_t) <= 12);

					property_request_size = safe_size_add(property_request_size,property_request_size); // x2
					property_request_size = safe_size_add(property_request_size,property_request_size); // x4
					property_request_size = safe_size_add(property_request_size,property_request_size); // x8
					property_request_size = safe_size_add(property_request_size,property_request_size); // x16 (over allocate)
					
					property_request_array = mem_alloc(property_request_size);
					
					{
						SIZE_T property_request_run;
						_everything3_result_list_property_request_t *property_request_d;
						
						property_request_run = property_request_count;
						property_request_d = property_request_array;
						
						while(property_request_run)
						{
							property_request_d->property_id = _everything3_stream_read_dword(&stream);
							property_request_d->flags = _everything3_stream_read_dword(&stream);
							property_request_d->value_type = _everything3_stream_read_byte(&stream);
							
							property_request_d++;
							property_request_run--;
						}
					}
				}
				
				// read items
				
				if (viewport_count)
				{
					SIZE_T viewport_run;
					utf8_buf_t property_text_cbuf;

					utf8_buf_init(&property_text_cbuf);
					
					viewport_run = viewport_count;
					
					while(viewport_run)
					{
						BYTE item_flags;
						
						item_flags = _everything3_stream_read_byte(&stream);
						
						// read properties..
						{
							SIZE_T property_request_run;
							const _everything3_result_list_property_request_t *property_request_p;
							
							property_request_run = property_request_count;
							property_request_p = property_request_array;
							
							while(property_request_run)
							{
								if (property_request_p->flags & (_EVERYTHING3_SEARCH_PROPERTY_REQUEST_FLAG_FORMAT|_EVERYTHING3_SEARCH_PROPERTY_REQUEST_FLAG_HIGHLIGHT))
								{
									SIZE_T len;
									
									len = _everything3_stream_read_len_vlq(&stream);
									
									utf8_buf_grow_length(&property_text_cbuf,len);
									
									_everything3_stream_read_data(&stream,property_text_cbuf.buf,len);
									
									property_text_cbuf.buf[len] = 0;
								}
								else
								{
									// add to total item size.
									switch(property_request_p->value_type)
									{
										case EVERYTHING3_PROPERTY_VALUE_TYPE_PSTRING: 
										case EVERYTHING3_PROPERTY_VALUE_TYPE_PSTRING_MULTISTRING: 
										case EVERYTHING3_PROPERTY_VALUE_TYPE_PSTRING_STRING_REFERENCE:
										case EVERYTHING3_PROPERTY_VALUE_TYPE_PSTRING_FOLDER_REFERENCE:
										case EVERYTHING3_PROPERTY_VALUE_TYPE_PSTRING_FILE_OR_FOLDER_REFERENCE:

											{
												SIZE_T len;
												
												len = _everything3_stream_read_len_vlq(&stream);
												
												utf8_buf_grow_length(&property_text_cbuf,len);
												
												_everything3_stream_read_data(&stream,property_text_cbuf.buf,len);
												
												property_text_cbuf.buf[len] = 0;
												
												debug_printf("ITEM PROPERTY %s\n",property_text_cbuf.buf);
											}

											break;

										case EVERYTHING3_PROPERTY_VALUE_TYPE_BYTE:
										case EVERYTHING3_PROPERTY_VALUE_TYPE_BYTE_GET_TEXT:

											{
												BYTE byte_value;

												_everything3_stream_read_data(&stream,&byte_value,sizeof(BYTE));
											}

											break;

										case EVERYTHING3_PROPERTY_VALUE_TYPE_WORD:
										case EVERYTHING3_PROPERTY_VALUE_TYPE_WORD_GET_TEXT:

											{
												WORD word_value;

												_everything3_stream_read_data(&stream,&word_value,sizeof(WORD));
											}
											
											break;

										case EVERYTHING3_PROPERTY_VALUE_TYPE_DWORD: 
										case EVERYTHING3_PROPERTY_VALUE_TYPE_DWORD_FIXED_Q1K: 
										case EVERYTHING3_PROPERTY_VALUE_TYPE_DWORD_GET_TEXT: 

											{
												DWORD dword_value;

												_everything3_stream_read_data(&stream,&dword_value,sizeof(DWORD));
											}
											
											break;
											
										case EVERYTHING3_PROPERTY_VALUE_TYPE_UINT64: 

											{
												ES_UINT64 uint64_value;

												_everything3_stream_read_data(&stream,&uint64_value,sizeof(ES_UINT64));
											}
											
											break;
											
										case EVERYTHING3_PROPERTY_VALUE_TYPE_UINT128: 

											{
												EVERYTHING3_UINT128 uint128_value;

												_everything3_stream_read_data(&stream,&uint128_value,sizeof(EVERYTHING3_UINT128));
											}
											
											break;
											
										case EVERYTHING3_PROPERTY_VALUE_TYPE_DIMENSIONS: 

											{
												EVERYTHING3_DIMENSIONS dimensions_value;

												_everything3_stream_read_data(&stream,&dimensions_value,sizeof(EVERYTHING3_DIMENSIONS));
											}
											
											break;
											
										case EVERYTHING3_PROPERTY_VALUE_TYPE_SIZE_T:
										
											{
												SIZE_T size_t_value;
												
												size_t_value = _everything3_stream_read_size_t(&stream);
											}
											break;
											
										case EVERYTHING3_PROPERTY_VALUE_TYPE_INT32_FIXED_Q1K: 
										case EVERYTHING3_PROPERTY_VALUE_TYPE_INT32_FIXED_Q1M: 

											{
												__int32 int32_value;

												_everything3_stream_read_data(&stream,&int32_value,sizeof(__int32));
											}
											
											break;

										case EVERYTHING3_PROPERTY_VALUE_TYPE_BLOB8:

											{
												BYTE len;
												
												len = _everything3_stream_read_byte(&stream);
												
												utf8_buf_grow_length(&property_text_cbuf,len);
												
												_everything3_stream_read_data(&stream,property_text_cbuf.buf,len);
												
												property_text_cbuf.buf[len] = 0;
											}

											break;

										case EVERYTHING3_PROPERTY_VALUE_TYPE_BLOB16:

											{
												WORD len;
												
												len = _everything3_stream_read_word(&stream);
												
												utf8_buf_grow_length(&property_text_cbuf,len);
												
												_everything3_stream_read_data(&stream,property_text_cbuf.buf,len);
												
												property_text_cbuf.buf[len] = 0;
											}

											break;

									}
								}
								
								property_request_p++;
								property_request_run--;
							}
						}
						
						viewport_run--;
					}

					utf8_buf_kill(&property_text_cbuf);
				}
				
				if (!stream.is_error)
				{
					debug_printf("OK\n");
					
					ret = TRUE;
				}
				
				if (property_request_array)
				{
					mem_free(property_request_array);
				}
				
				if (stream.buf)
				{
					mem_free(stream.buf);
				}
			}
			
			mem_free(packet_data);
		}
	
		utf8_buf_kill(&search_cbuf);

		CloseHandle(pipe_handle);
	}

	return ret;
}

int es_compare_list_items(const EVERYTHING_IPC_ITEM *a,const EVERYTHING_IPC_ITEM *b)
{
	int cmp_ret;

	cmp_ret = CompareString(LOCALE_USER_DEFAULT,NORM_IGNORECASE,EVERYTHING_IPC_ITEMPATH(es_sort_list,a),-1,EVERYTHING_IPC_ITEMPATH(es_sort_list,b),-1);
	
//debug_printf("cmp %S %S %d\n",EVERYTHING_IPC_ITEMPATH(es_sort_list,a),EVERYTHING_IPC_ITEMPATH(es_sort_list,b),cmp_ret)	;

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

	return wchar_string_compare(EVERYTHING_IPC_ITEMPATH(es_sort_list,a),EVERYTHING_IPC_ITEMPATH(es_sort_list,b));
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
			
		case ES_ERROR_SEND_MESSAGE:
			msg = "Unable to send IPC message.\r\n";
			break;
			
		case ES_ERROR_IPC:
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
		es_help();
	}

	ExitProcess(error_code);
}

static void es_write_wchar_string(const wchar_t *text)
{
	SIZE_T length_in_wchars;
	
	length_in_wchars = wchar_string_get_length_in_wchars(text);
	
	if (es_cibuf)
	{
		SIZE_T i;
		
		for(i=0;i<length_in_wchars;i++)
		{
			if ((int)i + es_cibuf_x >= es_console_wide)
			{
				break;
			}

			if (i + es_cibuf_x >= 0)
			{
				es_cibuf[i+es_cibuf_x].Attributes = es_cibuf_attributes;
				es_cibuf[i+es_cibuf_x].Char.UnicodeChar = text[i];
			}
		}
		
		es_cibuf_x += (int)length_in_wchars;
	}
	else
	{
		// pipe?
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

void es_fill(int count,int utf8ch)
{
	if (es_cibuf)
	{
		int i;
		
		for(i=0;i<count;i++)
		{
			if (i + es_cibuf_x >= es_console_wide)
			{
				break;
			}

			if (i + es_cibuf_x >= 0)
			{
				es_cibuf[i+es_cibuf_x].Attributes = es_cibuf_attributes;
				es_cibuf[i+es_cibuf_x].Char.UnicodeChar = utf8ch;
			}
		}
		
		es_cibuf_x += count;
	}
	else
	{
		utf8_buf_t cbuf;
		BYTE *p;
		DWORD numwritten;
		int i;
		
		utf8_buf_init(&cbuf);

		utf8_buf_grow_size(&cbuf,count);

		p = cbuf.buf;
		
		for(i=0;i<count;i++)
		{
			*p++ = utf8ch;
		}

 		WriteFile(es_output_handle,cbuf.buf,count,&numwritten,0);
		
		utf8_buf_kill(&cbuf);
	}
}

void es_write_wchar(wchar_t ch)
{
	wchar_t buf[2];
	
	buf[0] = ch;
	buf[1] = 0;
	
	es_fwrite_n(buf,1);
}

// set wlen to -1 for null terminated text.
void es_fwrite_n(const wchar_t *text,int wlen)
{
	if (wlen)
	{
		if (es_export_file != INVALID_HANDLE_VALUE)
		{
			int len;
			DWORD numwritten;
			int cp;
			utf8_buf_t cbuf;
			
			cp = CP_UTF8;
			utf8_buf_init(&cbuf);

			if (es_export_type == ES_EXPORT_TYPE_M3U)		
			{
				cp = CP_ACP;
			}
			
			len = WideCharToMultiByte(cp,0,text,wlen,0,0,0,0);
			
			utf8_buf_grow_size(&cbuf,len);

			len = WideCharToMultiByte(cp,0,text,wlen,cbuf.buf,len,0,0);
			
			if (wlen < 0)
			{
				// remove null from len.
				if (len) 
				{
					len--;
				}
			}
			
			if (len > es_export_remaining)
			{
				es_flush();
				
				if (len >= ES_EXPORT_BUF_SIZE)
				{
					WriteFile(es_export_file,cbuf.buf,len,&numwritten,0);
					
					goto copied;
				}
			}

			es_export_remaining -= len;
			os_copy_memory(es_export_p,cbuf.buf,len);
			es_export_p += len;

	copied:
			
			utf8_buf_kill(&cbuf);
		}
		else
		{
			es_write_wchar_string(text);
		}
	}
}

void es_fwrite_wchar_string(const wchar_t *text)
{
	es_fwrite_n(text,-1);
}

void es_fwrite_utf8_string(const ES_UTF8 *text)
{
	wchar_buf_t wcbuf;

	wchar_buf_init(&wcbuf);
	
	if (wcbuf.length_in_wchars <= INT_MAX)
	{
		es_fwrite_n(wcbuf.buf,(int)wcbuf.length_in_wchars);
	}

	wchar_buf_kill(&wcbuf);
}

void es_fwrite_csv_string(const wchar_t *s)
{
	const wchar_t *start;
	const wchar_t *p;
	
	es_fwrite_utf8_string("\"");
	
	// escape double quotes with double double quotes.
	start = s;
	p = s;
	
	while(*p)
	{
		if (*p == '"')
		{
			es_fwrite_n(start,(int)(p-start));

			// escape double quotes with double double quotes.
			es_fwrite_utf8_string("\"");
			es_fwrite_utf8_string("\"");
			
			start = p + 1;
		}
		
		p++;
	}

	es_fwrite_n(start,(int)(p-start));
	
	es_fwrite_utf8_string("\"");
}

// should a TSV/CSV string value be quoted.
static int _es_should_quote(int separator_ch,const wchar_t *s)
{
	const wchar_t *p;
	
	p = s;
	
	while(*p)
	{
		if ((*p == separator_ch) || (*p == '"') || (*p == '\r') || (*p == '\n'))
		{
			return 1;
		}
		
		p++;
	}
	
	return 0;
}

void es_fwrite_csv_string_with_optional_quotes(int is_always_double_quote,int separator_ch,const wchar_t *s)
{
	if (!is_always_double_quote)
	{
		if (!_es_should_quote(separator_ch,s))
		{
			// no quotes required..
			es_fwrite_wchar_string(s);
			
			return;
		}
	}

	// write with quotes..
	es_fwrite_csv_string(s);
}

void es_flush(void)
{
	if (es_export_file != INVALID_HANDLE_VALUE)
	{
		if (es_export_remaining != ES_EXPORT_BUF_SIZE)
		{
			DWORD numwritten;
			
			WriteFile(es_export_file,es_export_buf,ES_EXPORT_BUF_SIZE - es_export_remaining,&numwritten,0);
			
			es_export_p = es_export_buf;
			es_export_remaining = ES_EXPORT_BUF_SIZE;
		}	
	}
}

void es_write_highlighted(const wchar_t *text)
{
	if (es_cibuf)
	{
		const wchar_t *p;
		int highlighted;
		
		highlighted = 0;
		
		p = text;
		while(*p)
		{
			int i;
			const wchar_t *start;
			int toggle_highlight;
			int wlen;
			
			start = p;
			toggle_highlight = 0;
			
			for(;;)
			{
				if (!*p)
				{
					wlen = (int)(p-start);

					break;
				}
				
				if (*p == '*')
				{
					if (p[1] == '*')
					{
						// include one literal *
						wlen = (int)(p+1-start);
						p+=2;
						break;
					}
					
					wlen = (int)(p-start);
					p++;
					toggle_highlight = 1;
					break;
				}
				
				p++;
			}
		
			for(i=0;i<wlen;i++)
			{
				if (i + es_cibuf_x >= es_console_wide)
				{
					break;
				}

				if (i + es_cibuf_x >= 0)
				{
					es_cibuf[i+es_cibuf_x].Attributes = es_cibuf_attributes;
					es_cibuf[i+es_cibuf_x].Char.UnicodeChar = start[i];
				}
			}
			
			es_cibuf_x += wlen;

			// skip *				
			if (toggle_highlight)
			{
				highlighted = !highlighted;

				if (highlighted)
				{
					es_cibuf_attributes = es_highlight_color;
				}
				else
				{
					if (es_is_last_color)
					{
						es_cibuf_attributes = es_last_color;
					}
					else
					{
						es_cibuf_attributes = es_default_attributes;
					}
				}				
			}			
		}
	}
	else
	{
		const wchar_t *p;
		int highlighted;
		int attributes;
		
		if (es_is_last_color)
		{
			attributes = es_last_color;
		}
		else
		{
			attributes = es_default_attributes;
		}
		
		highlighted = 0;
		
		p = text;
		while(*p)
		{
			const wchar_t *start;
			int toggle_highlight;
			int wlen;
			
			start = p;
			toggle_highlight = 0;
			
			for(;;)
			{
				if (!*p)
				{
					wlen = (int)(p-start);

					break;
				}
				
				if (*p == '*')
				{
					if (p[1] == '*')
					{
						// include one literal *
						wlen = (int)(p+1-start);
						p+=2;
						break;
					}
					
					wlen = (int)(p-start);
					p++;
					toggle_highlight = 1;
					break;
				}
				
				p++;
			}
		
			if (es_output_is_char)
			{
				DWORD numwritten;
				
				WriteConsole(es_output_handle,start,wlen,&numwritten,0);
			}
			else
			{
				int mblen;
				
				mblen = WideCharToMultiByte(es_cp,0,start,wlen,0,0,0,0);
				if (mblen)
				{
					DWORD numwritten;
					utf8_buf_t cbuf;
					
					utf8_buf_init(&cbuf);

					utf8_buf_grow_size(&cbuf,mblen);
					
					WideCharToMultiByte(es_cp,0,start,wlen,cbuf.buf,mblen,0,0);
					
					WriteFile(es_output_handle,cbuf.buf,mblen,&numwritten,0);
					
					utf8_buf_kill(&cbuf);
				}			
			}
			
			if (toggle_highlight)
			{
				highlighted = !highlighted;

				if (highlighted)
				{
					es_cibuf_attributes = es_highlight_color;
					SetConsoleTextAttribute(es_output_handle,es_highlight_color);
				}
				else
				{
					es_cibuf_attributes = attributes;
					SetConsoleTextAttribute(es_output_handle,attributes);
				}
			}
		}
	}
}

void es_write_UINT64(ES_UINT64 value)
{
	wchar_t buf[256];
	wchar_t *d;
	
	d = buf + 256;
	*--d = 0;

	if (value)
	{
		ES_UINT64 i;
		
		i = value;
		
		while(i)
		{
			*--d = (wchar_t)('0' + (i % 10));
			
			i /= 10;
		}
	}
	else
	{
		*--d = '0';
	}	
	
	es_write_wchar_string(d);
}

void es_write_dword(DWORD value)
{
	es_write_UINT64(value);
}

static void es_write_result_count(DWORD result_count)
{
	es_write_dword(result_count);
	es_write_utf8_string("\r\n");
}

static void es_output_text_property(const wchar_t *value)
{
	if ((es_export_type == ES_EXPORT_TYPE_CSV) || (es_export_type == ES_EXPORT_TYPE_TSV))
	{
		es_fwrite_csv_string_with_optional_quotes((es_export_type == ES_EXPORT_TYPE_CSV) ? es_csv_double_quote : es_double_quote,(es_export_type == ES_EXPORT_TYPE_CSV) ? ',' : '\t',value);
	}
	else
	{
		es_write_wchar_string(value);
	}
}

static void es_output_attributes(DWORD file_attributes)
{
//TODO:
	es_write_dword(file_attributes);
//	es_write_wchar_string(value);
}

static void es_output_separator(void)
{
	if ((es_export_type == ES_EXPORT_TYPE_CSV) || (es_export_type == ES_EXPORT_TYPE_TSV))
	{
		es_write_wchar((es_export_type == ES_EXPORT_TYPE_CSV) ? ',' : '\t');
	}
}

static void es_output_newline(void)
{
	es_write_utf8_string("\r\n");
}

void es_listresultsW(EVERYTHING_IPC_LIST *list)
{
	if (es_no_result_error)
	{
		if (list->totitems == 0)
		{
			es_ret = ES_ERROR_NO_RESULTS;
		}
	}

	if (es_get_result_count)
	{
		es_write_result_count(list->totitems);
	}
	else
	if (es_get_total_size)
	{
		// version 2 IPC unavailable.
		es_ret = ES_ERROR_IPC;
	}
	else
	{
		if (es_sort_property_id == EVERYTHING3_PROPERTY_ID_PATH)
		{
			utf8_buf_t indexes_cbuf;
			EVERYTHING_IPC_ITEM **indexes;
			EVERYTHING_IPC_ITEM **indexes_d;

			utf8_buf_init(&indexes_cbuf);

			// allocate indexes.
			utf8_buf_grow_size(&indexes_cbuf,safe_size_mul_sizeof_pointer(list->numitems));
			
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
			
			// set es_sort_list for es_compare_list_items.
			// we are single threaded, and es_sort_list is read only.
			es_sort_list = list;
			
			os_sort(indexes,list->numitems,es_compare_list_items);
			
			{
				const EVERYTHING_IPC_ITEM **indexes_p;
				DWORD run;
				wchar_buf_t filename_wcbuf;
				
				wchar_buf_init(&filename_wcbuf);
				indexes_p = indexes;
				run = list->numitems;

				while(run)
				{
					column_t *column;
					int done_first;
					
					done_first = 0;
					column = column_order_start;
					
					while(column)
					{
						if (done_first)
						{
							es_output_separator();
						}
						else
						{
							done_first = 1;
						}
						
						switch(column->property_id)
						{
							case EVERYTHING3_PROPERTY_ID_ATTRIBUTES:
								es_output_attributes(((*indexes_p)->flags & EVERYTHING_IPC_FOLDER) ? FILE_ATTRIBUTE_DIRECTORY : 0);
								break;
								
							case EVERYTHING3_PROPERTY_ID_FULL_PATH:
								wchar_buf_path_cat_filename(EVERYTHING_IPC_ITEMPATH(list,*indexes_p),EVERYTHING_IPC_ITEMFILENAME(list,*indexes_p),&filename_wcbuf);
								es_output_text_property(filename_wcbuf.buf);
								break;

							case EVERYTHING3_PROPERTY_ID_PATH:
								es_output_text_property(EVERYTHING_IPC_ITEMPATH(list,*indexes_p));
								break;
								
							case EVERYTHING3_PROPERTY_ID_NAME:
								es_output_text_property(EVERYTHING_IPC_ITEMFILENAME(list,*indexes_p));
								break;
						}
						
						column = column->order_next;
					}									
					
					es_output_newline();
					
					indexes_p++;
					run--;
				}

				wchar_buf_kill(&filename_wcbuf);
			}
			
			utf8_buf_kill(&indexes_cbuf);
		}
		else
		{
			DWORD i;
			wchar_buf_t filename_wcbuf;
				
			wchar_buf_init(&filename_wcbuf);
			
			for(i=0;i<list->numitems;i++)
			{
				column_t *column;
				int done_first;
				
				done_first = 0;
				column = column_order_start;
				
				while(column)
				{
					if (done_first)
					{
						es_output_separator();
					}
					else
					{
						done_first = 1;
					}
					
					switch(column->property_id)
					{
						case EVERYTHING3_PROPERTY_ID_ATTRIBUTES:
							es_output_attributes((list->items[i].flags & EVERYTHING_IPC_FOLDER) ? FILE_ATTRIBUTE_DIRECTORY : 0);
							break;
							
						case EVERYTHING3_PROPERTY_ID_FULL_PATH:
							wchar_buf_path_cat_filename(EVERYTHING_IPC_ITEMPATH(list,&list->items[i]),EVERYTHING_IPC_ITEMFILENAME(list,&list->items[i]),&filename_wcbuf);
							es_output_text_property(filename_wcbuf.buf);
							break;

						case EVERYTHING3_PROPERTY_ID_PATH:
							es_output_text_property(EVERYTHING_IPC_ITEMPATH(list,&list->items[i]));
							break;
							
						case EVERYTHING3_PROPERTY_ID_NAME:
							es_output_text_property(EVERYTHING_IPC_ITEMFILENAME(list,&list->items[i]));
							break;
					}
					
					column = column->order_next;
				}									
				
				es_output_newline();
				
			}
			
			wchar_buf_kill(&filename_wcbuf);
		}
	}
	
	PostQuitMessage(0);
}

void es_listresults2W(EVERYTHING_IPC_LIST2 *list,int index_start,int count)
{
	DWORD i;
	EVERYTHING_IPC_ITEM2 *items;
	wchar_buf_t column_wcbuf;

	wchar_buf_init(&column_wcbuf);			
	items = (EVERYTHING_IPC_ITEM2 *)(list + 1);

	es_cibuf_y = 0;
	i = index_start;
	
	while(count)
	{
		if (i >= list->numitems)
		{
			break;
		}
	
		es_cibuf_x = -es_cibuf_hscroll;
		
		if ((es_export_type == ES_EXPORT_TYPE_CSV) || (es_export_type == ES_EXPORT_TYPE_TSV))
		{
			column_t *column;
			int csv_double_quote;
			int separator_ch;
			int done_first;
			
			// Like Excel, avoid quoting unless we really need to with TSV
			// CSV should always quote for consistancy.
			csv_double_quote = (es_export_type == ES_EXPORT_TYPE_CSV) ? es_csv_double_quote : es_double_quote;
			
			separator_ch = (es_export_type == ES_EXPORT_TYPE_CSV) ? ',' : '\t';
			
			done_first = 0;
			column = column_order_start;
			
			while(column)
			{
				void *data;
				
				if (done_first)
				{
					es_write_wchar(separator_ch);
				}
				else
				{
					done_first = 1;
				}
				
				data = es_get_column_data(list,i,column->property_id,0);
				if (data)
				{
					switch(column->property_id)
					{
						case EVERYTHING3_PROPERTY_ID_NAME:
						case EVERYTHING3_PROPERTY_ID_PATH:
						case EVERYTHING3_PROPERTY_ID_FULL_PATH:
						case EVERYTHING3_PROPERTY_ID_EXTENSION:
						case EVERYTHING3_PROPERTY_ID_FILE_LIST_FILENAME:
							es_fwrite_csv_string_with_optional_quotes(csv_double_quote,separator_ch,(wchar_t *)((char *)data+sizeof(DWORD)));
							break;
							
						case EVERYTHING3_PROPERTY_ID_SIZE:

							{
								if ((items[i].flags & EVERYTHING_IPC_FOLDER) && (*(ES_UINT64 *)data == 0xffffffffffffffffI64))
								{
								}
								else
								{
									wchar_buf_printf(&column_wcbuf,"%I64u",*(ES_UINT64 *)data);
									es_fwrite_wchar_string(column_wcbuf.buf);
								}
							}
							
							break;

						case EVERYTHING3_PROPERTY_ID_DATE_MODIFIED:
						case EVERYTHING3_PROPERTY_ID_DATE_CREATED:
						case EVERYTHING3_PROPERTY_ID_DATE_ACCESSED:
						case EVERYTHING3_PROPERTY_ID_DATE_RUN:
						case EVERYTHING3_PROPERTY_ID_DATE_RECENTLY_CHANGED:
						
							{
								es_format_filetime(*(ES_UINT64 *)data,&column_wcbuf);
								es_fwrite_wchar_string(column_wcbuf.buf);
							}

							break;

						case EVERYTHING3_PROPERTY_ID_ATTRIBUTES:

							{
								es_format_attributes(*(DWORD *)data,&column_wcbuf);
								es_fwrite_wchar_string(column_wcbuf.buf);
							}
							
							break;

						
						case EVERYTHING3_PROPERTY_ID_RUN_COUNT:
						
							{
								wchar_buf_print_UINT64(&column_wcbuf,*(DWORD *)data);
								es_fwrite_wchar_string(column_wcbuf.buf);
							}	

							break;
					}
				}
				
				column = column->order_next;
			}

			es_fwrite_utf8_string("\r\n");
		}
		else
		if (es_export_type == ES_EXPORT_TYPE_EFU)
		{
			void *data;
				
			// Filename
			data = es_get_column_data(list,i,EVERYTHING3_PROPERTY_ID_FULL_PATH,0);
			if (data)
			{
				es_fwrite_csv_string((wchar_t *)((char *)data+sizeof(DWORD)));
			}

			es_fwrite_utf8_string(",");
			
			// size
			data = es_get_column_data(list,i,EVERYTHING3_PROPERTY_ID_SIZE,0);
			if (data)
			{
				if (*(ES_UINT64 *)data != 0xffffffffffffffffI64)
				{
					wchar_buf_print_UINT64(&column_wcbuf,*(ES_UINT64 *)data);
					es_fwrite_wchar_string(column_wcbuf.buf);
				}
			}

			es_fwrite_utf8_string(",");

			// date modified
			data = es_get_column_data(list,i,EVERYTHING3_PROPERTY_ID_DATE_MODIFIED,0);
			if (data)
			{
				if ((*(ES_UINT64 *)data != 0xffffffffffffffffI64))
				{
					wchar_buf_print_UINT64(&column_wcbuf,*(ES_UINT64 *)data);
					es_fwrite_wchar_string(column_wcbuf.buf);
				}
			}

			es_fwrite_utf8_string(",");

			// date created
			data = es_get_column_data(list,i,EVERYTHING3_PROPERTY_ID_DATE_CREATED,0);
			if (data)
			{
				if ((*(ES_UINT64 *)data != 0xffffffffffffffffI64))
				{
					wchar_buf_print_UINT64(&column_wcbuf,*(ES_UINT64 *)data);
					es_fwrite_wchar_string(column_wcbuf.buf);
				}
			}

			es_fwrite_utf8_string(",");

			// date created
			data = es_get_column_data(list,i,EVERYTHING3_PROPERTY_ID_ATTRIBUTES,0);
			if (data)
			{
				wchar_buf_print_UINT64(&column_wcbuf,*(DWORD *)data);
				es_fwrite_wchar_string(column_wcbuf.buf);
			}
			else
			{
				DWORD file_attributes;
				
				// add file/folder attributes.
				if (items[i].flags & EVERYTHING_IPC_FOLDER)
				{
					file_attributes = FILE_ATTRIBUTE_DIRECTORY;
				}
				else
				{
					file_attributes = 0;
				}

				wchar_buf_print_UINT64(&column_wcbuf,file_attributes);
				es_fwrite_wchar_string(column_wcbuf.buf);
			}

			es_fwrite_utf8_string("\r\n");
		}
		else
		if ((es_export_type == ES_EXPORT_TYPE_TXT) || (es_export_type == ES_EXPORT_TYPE_M3U) || (es_export_type == ES_EXPORT_TYPE_M3U8))
		{
			void *data;
			
			data = es_get_column_data(list,i,EVERYTHING3_PROPERTY_ID_FULL_PATH,0);
			
			if (es_double_quote)
			{
				es_fwrite_utf8_string("\"");
			}
			
			if (data)
			{
				es_fwrite_wchar_string((wchar_t *)((char *)data+sizeof(DWORD)));
			}

			if (es_double_quote)
			{
				es_fwrite_utf8_string("\"");
			}
			
			es_fwrite_utf8_string("\r\n");
		}
		else
		{
			column_t *column;
			int did_set_color;
			int tot_column_text_len;
			int tot_column_width;
			int done_first;
			
			tot_column_text_len = 0;
			tot_column_width = 0;
			done_first = 0;
			
			column = column_order_start;

			while(column)
			{
				void *data;
				const wchar_t *column_text;
				int column_is_highlighted;
				int column_is_double_quote;
				
				if (done_first)
				{
					es_write_utf8_string(" ");
					tot_column_text_len++;
					tot_column_width++;
				}
				else
				{
					done_first = 1;
				}
				
				data = es_get_column_data(list,i,column->property_id,column->flags & COLUMN_FLAG_HIGHLIGHT);

				es_is_last_color = 0;
				did_set_color = 0;
				
				column_text = L"";
				column_is_highlighted = 0;

				if (column->flags & COLUMN_FLAG_HIGHLIGHT)
				{
					column_is_highlighted = 1;
				}

				column_is_double_quote = 0;

				if (data)
				{
					if (es_output_is_char)
					{
						column_color_t *column_color;
						
						column_color = column_color_find(column->property_id);
						if (column_color)
						{
							es_cibuf_attributes = column_color->color;
							SetConsoleTextAttribute(es_output_handle,es_cibuf_attributes);

							es_last_color = es_cibuf_attributes;
							es_is_last_color = 1;

							did_set_color = 1;
						}
					}

					switch(column->property_id)
					{
						case EVERYTHING3_PROPERTY_ID_NAME:

							column_text = (wchar_t *)((char *)data+sizeof(DWORD));
							column_is_double_quote = es_double_quote;

							break;
							
						case EVERYTHING3_PROPERTY_ID_PATH:

							column_text = (wchar_t *)((char *)data+sizeof(DWORD));
							column_is_double_quote = es_double_quote;

							break;
							
						case EVERYTHING3_PROPERTY_ID_FULL_PATH:

							column_text = (wchar_t *)((char *)data+sizeof(DWORD));
							column_is_double_quote = es_double_quote;

							break;
							
						case EVERYTHING3_PROPERTY_ID_EXTENSION:

							column_text = (wchar_t *)((char *)data+sizeof(DWORD));
							break;
							
						case EVERYTHING3_PROPERTY_ID_SIZE:
							
							if ((items[i].flags & EVERYTHING_IPC_FOLDER) && (*(ES_UINT64 *)data == 0xffffffffffffffffI64))
							{
								es_format_dir(&column_wcbuf);
							}
							else
							{
								es_format_size(*(ES_UINT64 *)data,&column_wcbuf);
							}

							column_text = column_wcbuf.buf;

							break;

						case EVERYTHING3_PROPERTY_ID_DATE_CREATED:

							es_format_filetime(*(ES_UINT64 *)data,&column_wcbuf);

							column_text = column_wcbuf.buf;
							break;
						
						case EVERYTHING3_PROPERTY_ID_DATE_MODIFIED:

							es_format_filetime(*(ES_UINT64 *)data,&column_wcbuf);
							column_text = column_wcbuf.buf;
							break;
						
						case EVERYTHING3_PROPERTY_ID_DATE_ACCESSED:

							es_format_filetime(*(ES_UINT64 *)data,&column_wcbuf);
							column_text = column_wcbuf.buf;
							break;
						
						case EVERYTHING3_PROPERTY_ID_ATTRIBUTES:

							es_format_attributes(*(DWORD *)data,&column_wcbuf);
							column_text = column_wcbuf.buf;
							break;

						case EVERYTHING3_PROPERTY_ID_FILE_LIST_FILENAME:

							column_text = (wchar_t *)((char *)data+sizeof(DWORD));
							break;
						
						case EVERYTHING3_PROPERTY_ID_RUN_COUNT:

							es_format_run_count(*(DWORD *)data,&column_wcbuf);
							column_text = column_wcbuf.buf;
							break;
											
						case EVERYTHING3_PROPERTY_ID_DATE_RUN:

							es_format_filetime(*(ES_UINT64 *)data,&column_wcbuf);
							column_text = column_wcbuf.buf;
							break;
															
						case EVERYTHING3_PROPERTY_ID_DATE_RECENTLY_CHANGED:

							es_format_filetime(*(ES_UINT64 *)data,&column_wcbuf);
							column_text = column_wcbuf.buf;
							break;
					}

					//es_column_widths
					
					{
						int column_text_len;
						int fill;
						int column_width;
						int is_right_aligned;
						
						is_right_aligned = es_column_is_right_aligned(column->property_id);
						
						if (is_right_aligned)
						{
							// check for right aligned dir
							// and make it left aligned.
							if (*column_text == '<')
							{
								is_right_aligned = 0;
							}
						}
						
						if (column_is_highlighted)
						{
							column_text_len = safe_int_from_size(wchar_string_get_highlighted_length(column_text));
						}
						else
						{
							column_text_len = safe_int_from_size(wchar_string_get_length_in_wchars(column_text));
						}
						
						if (column_is_double_quote)
						{
							column_text_len += 2;
						}
						
						column_width = column_width_get(column->property_id);
						
						// left fill
						if (is_right_aligned)
						{
							if (tot_column_width + column_width > (tot_column_text_len + column_text_len))
							{
								int fillch;
								
								fill = tot_column_width + column_width - (tot_column_text_len + column_text_len);

								fillch = ' ';
								
								if (!es_digit_grouping)
								{
									if (column->property_id == EVERYTHING3_PROPERTY_ID_SIZE)
									{
										fillch = es_size_leading_zero ? '0' : ' ';
									}
									else
									if (column->property_id == EVERYTHING3_PROPERTY_ID_RUN_COUNT)
									{
										fillch = es_run_count_leading_zero ? '0' : ' ';
									}
								}
							
								// left spaces.
								es_fill(fill,fillch);
								tot_column_text_len += fill;
							}
						}

						if (column_is_double_quote)
						{
							es_write_utf8_string("\"");
						}

						// write text.
						if (column_is_highlighted)
						{
							es_write_highlighted(column_text);
						}
						else
						{
							es_write_wchar_string(column_text);
						}
				
						if (column_is_double_quote)
						{
							es_write_utf8_string("\"");
						}

						// right fill
						if (!is_right_aligned)
						{
							if (tot_column_width + column_width > (tot_column_text_len + column_text_len))
							{
								fill = tot_column_width + column_width - (tot_column_text_len + column_text_len);

								// dont right fill last column.
								if (column != column_order_last)
								{
									// right spaces.
									es_fill(fill,' ');
									tot_column_text_len += fill;
								}
							}
						}
							
									
						tot_column_text_len += column_text_len;
						tot_column_width += column_width;
					}

					// restore color
					if (did_set_color)
					{
						es_cibuf_attributes = es_default_attributes;
						SetConsoleTextAttribute(es_output_handle,es_default_attributes);
					}
				}
				
				column = column->order_next;
			}
			
			if (es_pause)
			{
				// fill right side of screen
				if (es_cibuf)
				{
					int wlen;
					
					if (es_cibuf_x)
					{
						if (es_cibuf_x + es_cibuf_hscroll > es_max_wide)
						{
							es_max_wide = es_cibuf_x + es_cibuf_hscroll;
						}
					}
			
					wlen = es_console_wide - es_cibuf_x;
					
					if (wlen > 0)
					{
						int bufi;
						
						for(bufi=0;bufi<wlen;bufi++)
						{
							if (bufi + es_cibuf_x >= es_console_wide)
							{
								break;
							}

							if (bufi + es_cibuf_x >= 0)
							{
								es_cibuf[bufi+es_cibuf_x].Attributes = es_cibuf_attributes;
								es_cibuf[bufi+es_cibuf_x].Char.UnicodeChar = ' ';
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
						write_rect.Top = es_console_window_y + es_cibuf_y;
						write_rect.Right = es_console_window_x + es_console_wide;
						write_rect.Bottom = es_console_window_y + es_cibuf_y + 1;
						
						WriteConsoleOutput(es_output_handle,es_cibuf,buf_size,buf_pos,&write_rect);
						
						es_cibuf_x += wlen;				
					}
				}			
			}
			else
			{
				es_write_utf8_string("\r\n");
			}
		}

		es_cibuf_y++;
		count--;
		i++;
	}

	wchar_buf_kill(&column_wcbuf);			
}

void es_write_total_size(EVERYTHING_IPC_LIST2 *list,int count)
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
	
		data = es_get_column_data(list,i,EVERYTHING3_PROPERTY_ID_SIZE,0);

		if (data)
		{
			// dont count folders
			if (!(items[i].flags & EVERYTHING_IPC_FOLDER))
			{
				total_size += *(ES_UINT64 *)data;
			}
		}

		count--;
		i++;
	}

	es_write_UINT64(total_size);
	es_write_utf8_string("\r\n");
}

// custom window proc
LRESULT __stdcall es_window_proc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam)
{
	switch(msg)
	{
		case WM_COPYDATA:
		{
			COPYDATASTRUCT *cds = (COPYDATASTRUCT *)lParam;
			
			switch(cds->dwData)
			{
				case ES_COPYDATA_IPCTEST_QUERYCOMPLETEW:
					es_listresultsW((EVERYTHING_IPC_LIST *)cds->lpData);
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
						es_write_result_count(((EVERYTHING_IPC_LIST2 *)cds->lpData)->totitems);
					}
					else
					if (es_get_total_size)
					{
						es_write_total_size((EVERYTHING_IPC_LIST2 *)cds->lpData,((EVERYTHING_IPC_LIST2 *)cds->lpData)->numitems);
					}
					else
					if ((int)((EVERYTHING_IPC_LIST2 *)cds->lpData)->numitems)
					{
						if (es_pause)
						{
							HANDLE std_input_handle;
							int start_index;
							int last_start_index;
							int last_hscroll;
							int info_line;
							
							info_line = es_console_high - 1;
							
							if (info_line > (int)((EVERYTHING_IPC_LIST2 *)cds->lpData)->numitems)
							{
								info_line = (int)((EVERYTHING_IPC_LIST2 *)cds->lpData)->numitems;
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
																
									es_console_window_y = es_console_size_high - (info_line + 1);
								}
							}
							
							std_input_handle = GetStdHandle(STD_INPUT_HANDLE);

							es_cibuf = mem_alloc(es_console_wide * sizeof(CHAR_INFO));
							
							// set cursor pos to the bottom of the screen
							{
								COORD cur_pos;
								cur_pos.X = es_console_window_x;
								cur_pos.Y = es_console_window_y + info_line;
								
								SetConsoleCursorPosition(es_output_handle,cur_pos);
							}

							// write out some basic usage at the bottom
							{
								DWORD numwritten;
						
								WriteFile(es_output_handle,ES_PAUSE_TEXT,(int)strlen(ES_PAUSE_TEXT),&numwritten,0);
							}
						
							start_index = 0;
							last_start_index = 0;
							last_hscroll = 0;
						
							for(;;)
							{
								INPUT_RECORD ir;
								DWORD numread;

								es_listresults2W((EVERYTHING_IPC_LIST2 *)cds->lpData,start_index,es_console_high-1);
								
								// is everything is shown?
								if (es_max_wide <= es_console_wide)
								{
									if (((int)((EVERYTHING_IPC_LIST2 *)cds->lpData)->numitems) < es_console_high)
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
													if ((int)((EVERYTHING_IPC_LIST2 *)cds->lpData)->numitems < es_console_high + 1)
													{
														goto exit;
													}
													if (start_index == (int)((EVERYTHING_IPC_LIST2 *)cds->lpData)->numitems - es_console_high + 1)
													{
														goto exit;
													}
													start_index += 1;
													break;
												}
												else
												if (ir.Event.KeyEvent.wVirtualKeyCode == VK_SPACE)
												{
													if ((int)((EVERYTHING_IPC_LIST2 *)cds->lpData)->numitems < es_console_high + 1)
													{
														goto exit;
													}
													
													if (start_index == (int)((EVERYTHING_IPC_LIST2 *)cds->lpData)->numitems - es_console_high + 1)
													{
														goto exit;
													}
													
													// fall through
												}
												else
												if (ir.Event.KeyEvent.wVirtualKeyCode == VK_RIGHT)
												{
													es_cibuf_hscroll += 5;
													
													if (es_max_wide > es_console_wide)
													{
														if (es_cibuf_hscroll > es_max_wide - es_console_wide)
														{
															es_cibuf_hscroll = es_max_wide - es_console_wide;
														}
													}
													else
													{
														es_cibuf_hscroll = 0;
													}
													break;
												}
												else
												if (ir.Event.KeyEvent.wVirtualKeyCode == VK_LEFT)
												{
													es_cibuf_hscroll -= 5;
													if (es_cibuf_hscroll < 0)
													{
														es_cibuf_hscroll = 0;
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
													start_index = ((EVERYTHING_IPC_LIST2 *)cds->lpData)->numitems - es_console_high + 1;
													break;
												}
												else
												if ((ir.Event.KeyEvent.wVirtualKeyCode == 'Q') || (ir.Event.KeyEvent.wVirtualKeyCode == VK_ESCAPE))
												{
													goto exit;
												}
												else
												if (es_is_valid_key(&ir))
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
								if ((int)((EVERYTHING_IPC_LIST2 *)cds->lpData)->numitems > es_console_high + 1)
								{
									if (start_index > (int)((EVERYTHING_IPC_LIST2 *)cds->lpData)->numitems - es_console_high + 1)
									{
										start_index = ((EVERYTHING_IPC_LIST2 *)cds->lpData)->numitems - es_console_high + 1;
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
								if (last_hscroll != es_cibuf_hscroll)
								{
									last_hscroll = es_cibuf_hscroll;
								}
								else
								{
									goto start;
								}
							}
							
	exit:						
							
							// remove text.
							if (es_cibuf)
							{
								// set cursor pos to the bottom of the screen
								{
									COORD cur_pos;
									cur_pos.X = es_console_window_x;
									cur_pos.Y = es_console_window_y + info_line;
									
									SetConsoleCursorPosition(es_output_handle,cur_pos);
								}

								// write out some basic usage at the bottom
								{
									DWORD numwritten;
							
									WriteFile(es_output_handle,ES_BLANK_PAUSE_TEXT,(int)strlen(ES_BLANK_PAUSE_TEXT),&numwritten,0);
								}
							
								// reset cursor pos.
								{
									COORD cur_pos;
									cur_pos.X = es_console_window_x;
									cur_pos.Y = es_console_window_y + info_line;
									
									SetConsoleCursorPosition(es_output_handle,cur_pos);
								}
								
								// free
								mem_free(es_cibuf);
							}								
						}
						else
						{
							es_listresults2W((EVERYTHING_IPC_LIST2 *)cds->lpData,0,((EVERYTHING_IPC_LIST2 *)cds->lpData)->numitems);
						}
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

void es_help(void)
{
	// Help from NotNull
	es_write_utf8_string("ES " VERSION_TEXT "\r\n");
	es_write_utf8_string("ES is a command line interface to search Everything from a command prompt.\r\n");
	es_write_utf8_string("ES uses the Everything search syntax.\r\n");
	es_write_utf8_string("\r\n");
	es_write_utf8_string("Usage: es.exe [options] search text\r\n");
	es_write_utf8_string("Example: ES  Everything ext:exe;ini \r\n");
	es_write_utf8_string("\r\n");
	es_write_utf8_string("\r\n");
	es_write_utf8_string("Search options\r\n");
	es_write_utf8_string("   -r <search>, -regex <search>\r\n");
	es_write_utf8_string("        Search using regular expressions.\r\n");
	es_write_utf8_string("   -i, -case\r\n");
	es_write_utf8_string("        Match case.\r\n");
	es_write_utf8_string("   -w, -ww, -whole-word, -whole-words\r\n");
	es_write_utf8_string("        Match whole words.\r\n");
	es_write_utf8_string("   -p, -match-path\r\n");
	es_write_utf8_string("        Match full path and file name.\r\n");
	es_write_utf8_string("   -a, -diacritics\r\n");
	es_write_utf8_string("        Match diacritical marks.\r\n");
	es_write_utf8_string("\r\n");
	es_write_utf8_string("   -o <offset>, -offset <offset>\r\n");
	es_write_utf8_string("        Show results starting from offset.\r\n");
	es_write_utf8_string("   -n <num>, -max-results <num>\r\n");
	es_write_utf8_string("        Limit the number of results shown to <num>.\r\n");
	es_write_utf8_string("\r\n");
	es_write_utf8_string("   -path <path>\r\n");
	es_write_utf8_string("        Search for subfolders and files in path.\r\n");
	es_write_utf8_string("   -parent-path <path>\r\n");
	es_write_utf8_string("        Search for subfolders and files in the parent of path.\r\n");
	es_write_utf8_string("   -parent <path>\r\n");
	es_write_utf8_string("        Search for files with the specified parent path.\r\n");
	es_write_utf8_string("\r\n");
	es_write_utf8_string("   /ad\r\n");
	es_write_utf8_string("        Folders only.\r\n");
	es_write_utf8_string("   /a-d\r\n");
	es_write_utf8_string("        Files only.\r\n");
	es_write_utf8_string("   /a[RHSDAVNTPLCOIE]\r\n");
	es_write_utf8_string("        DIR style attributes search.\r\n");
	es_write_utf8_string("        R = Read only.\r\n");
	es_write_utf8_string("        H = Hidden.\r\n");
	es_write_utf8_string("        S = System.\r\n");
	es_write_utf8_string("        D = Directory.\r\n");
	es_write_utf8_string("        A = Archive.\r\n");
	es_write_utf8_string("        V = Device.\r\n");
	es_write_utf8_string("        N = Normal.\r\n");
	es_write_utf8_string("        T = Temporary.\r\n");
	es_write_utf8_string("        P = Sparse file.\r\n");
	es_write_utf8_string("        L = Reparse point.\r\n");
	es_write_utf8_string("        C = Compressed.\r\n");
	es_write_utf8_string("        O = Offline.\r\n");
	es_write_utf8_string("        I = Not content indexed.\r\n");
	es_write_utf8_string("        E = Encrypted.\r\n");
	es_write_utf8_string("        - = Prefix a flag with - to exclude.\r\n");
	es_write_utf8_string("\r\n");
	es_write_utf8_string("\r\n");
	es_write_utf8_string("Sort options\r\n");
	es_write_utf8_string("   -s\r\n");
	es_write_utf8_string("        sort by full path.\r\n");
	es_write_utf8_string("   -sort <name[-ascending|-descending]>\r\n");
	es_write_utf8_string("        Set sort\r\n");
	es_write_utf8_string("        name=name|path|size|extension|date-created|date-modified|date-accessed|\r\n");
	es_write_utf8_string("        attributes|file-list-file-name|run-count|date-recently-changed|date-run\r\n");
	es_write_utf8_string("   -sort-ascending, -sort-descending\r\n");
	es_write_utf8_string("        Set sort order\r\n");
	es_write_utf8_string("\r\n");
	es_write_utf8_string("   /on, /o-n, /os, /o-s, /oe, /o-e, /od, /o-d\r\n");
	es_write_utf8_string("        DIR style sorts.\r\n");
	es_write_utf8_string("        N = Name.\r\n");
	es_write_utf8_string("        S = Size.\r\n");
	es_write_utf8_string("        E = Extension.\r\n");
	es_write_utf8_string("        D = Date modified.\r\n");
	es_write_utf8_string("        - = Sort in descending order.\r\n");
	es_write_utf8_string("\r\n");
	es_write_utf8_string("\r\n");
	es_write_utf8_string("Display options\r\n");
	es_write_utf8_string("   -name\r\n");
	es_write_utf8_string("   -path-column\r\n");
	es_write_utf8_string("   -full-path-and-name, -filename-column\r\n");
	es_write_utf8_string("   -extension, -ext\r\n");
	es_write_utf8_string("   -size\r\n");
	es_write_utf8_string("   -date-created, -dc\r\n");
	es_write_utf8_string("   -date-modified, -dm\r\n");
	es_write_utf8_string("   -date-accessed, -da\r\n");
	es_write_utf8_string("   -attributes, -attribs, -attrib\r\n");
	es_write_utf8_string("   -file-list-file-name\r\n");
	es_write_utf8_string("   -run-count\r\n");
	es_write_utf8_string("   -date-run\r\n");
	es_write_utf8_string("   -date-recently-changed, -rc\r\n");
	es_write_utf8_string("        Show the specified column.\r\n");
	es_write_utf8_string("        \r\n");
	es_write_utf8_string("   -highlight\r\n");
	es_write_utf8_string("        Highlight results.\r\n");
	es_write_utf8_string("   -highlight-color <color>\r\n");
	es_write_utf8_string("        Highlight color 0-255.\r\n");
	es_write_utf8_string("\r\n");
	es_write_utf8_string("   -csv\r\n");
	es_write_utf8_string("   -efu\r\n");
	es_write_utf8_string("   -txt\r\n");
	es_write_utf8_string("   -m3u\r\n");
	es_write_utf8_string("   -m3u8\r\n");
	es_write_utf8_string("   -tsv\r\n");
	es_write_utf8_string("        Change display format.\r\n");
	es_write_utf8_string("\r\n");
	es_write_utf8_string("   -size-format <format>\r\n");
	es_write_utf8_string("        0=auto, 1=Bytes, 2=KB, 3=MB.\r\n");
	es_write_utf8_string("   -date-format <format>\r\n");
	es_write_utf8_string("        0=auto, 1=ISO-8601, 2=FILETIME, 3=ISO-8601(UTC)\r\n");
	es_write_utf8_string("\r\n");
	es_write_utf8_string("   -filename-color <color>\r\n");
	es_write_utf8_string("   -name-color <color>\r\n");
	es_write_utf8_string("   -path-color <color>\r\n");
	es_write_utf8_string("   -extension-color <color>\r\n");
	es_write_utf8_string("   -size-color <color>\r\n");
	es_write_utf8_string("   -date-created-color <color>, -dc-color <color>\r\n");
	es_write_utf8_string("   -date-modified-color <color>, -dm-color <color>\r\n");
	es_write_utf8_string("   -date-accessed-color <color>, -da-color <color>\r\n");
	es_write_utf8_string("   -attributes-color <color>\r\n");
	es_write_utf8_string("   -file-list-filename-color <color>\r\n");
	es_write_utf8_string("   -run-count-color <color>\r\n");
	es_write_utf8_string("   -date-run-color <color>\r\n");
	es_write_utf8_string("   -date-recently-changed-color <color>, -rc-color <color>\r\n");
	es_write_utf8_string("        Set the column color 0-255.\r\n");
	es_write_utf8_string("\r\n");
	es_write_utf8_string("   -filename-width <width>\r\n");
	es_write_utf8_string("   -name-width <width>\r\n");
	es_write_utf8_string("   -path-width <width>\r\n");
	es_write_utf8_string("   -extension-width <width>\r\n");
	es_write_utf8_string("   -size-width <width>\r\n");
	es_write_utf8_string("   -date-created-width <width>, -dc-width <width>\r\n");
	es_write_utf8_string("   -date-modified-width <width>, -dm-width <width>\r\n");
	es_write_utf8_string("   -date-accessed-width <width>, -da-width <width>\r\n");
	es_write_utf8_string("   -attributes-width <width>\r\n");
	es_write_utf8_string("   -file-list-filename-width <width>\r\n");
	es_write_utf8_string("   -run-count-width <width>\r\n");
	es_write_utf8_string("   -date-run-width <width>\r\n");
	es_write_utf8_string("   -date-recently-changed-width <width>, -rc-width <width>\r\n");
	es_write_utf8_string("        Set the column width 0-200.\r\n");
	es_write_utf8_string("\r\n");
	es_write_utf8_string("   -no-digit-grouping\r\n");
	es_write_utf8_string("        Don't group numbers with commas.\r\n");
	es_write_utf8_string("   -size-leading-zero\r\n");
	es_write_utf8_string("   -run-count-leading-zero\r\n");
	es_write_utf8_string("        Format the number with leading zeros, use with -no-digit-grouping.\r\n");
	es_write_utf8_string("   -double-quote\r\n");
	es_write_utf8_string("        Wrap paths and filenames with double quotes.\r\n");
	es_write_utf8_string("\r\n");
	es_write_utf8_string("\r\n");
	es_write_utf8_string("Export options\r\n");
	es_write_utf8_string("   -export-csv <out.csv>\r\n");
	es_write_utf8_string("   -export-efu <out.efu>\r\n");
	es_write_utf8_string("   -export-txt <out.txt>\r\n");
	es_write_utf8_string("   -export-m3u <out.m3u>\r\n");
	es_write_utf8_string("   -export-m3u8 <out.m3u8>\r\n");
	es_write_utf8_string("   -export-tsv <out.txt>\r\n");
	es_write_utf8_string("        Export to a file using the specified layout.\r\n");
	es_write_utf8_string("   -no-header\r\n");
	es_write_utf8_string("        Do not output a column header for CSV, EFU and TSV files.\r\n");
	es_write_utf8_string("   -utf8-bom\r\n");
	es_write_utf8_string("        Store a UTF-8 byte order mark at the start of the exported file.\r\n");
	es_write_utf8_string("\r\n");
	es_write_utf8_string("\r\n");
	es_write_utf8_string("General options\r\n");
	es_write_utf8_string("   -h, -help\r\n");
	es_write_utf8_string("        Display this help.\r\n");
	es_write_utf8_string("\r\n");
	es_write_utf8_string("   -instance <name>\r\n");
	es_write_utf8_string("        Connect to the unique Everything instance name.\r\n");
	es_write_utf8_string("   -ipc1, -ipc2\r\n");
	es_write_utf8_string("        Use IPC version 1 or 2.\r\n");
	es_write_utf8_string("   -pause, -more\r\n");
	es_write_utf8_string("        Pause after each page of output.\r\n");
	es_write_utf8_string("   -hide-empty-search-results\r\n");
	es_write_utf8_string("        Don't show any results when there is no search.\r\n");
	es_write_utf8_string("   -empty-search-help\r\n");
	es_write_utf8_string("        Show help when no search is specified.\r\n");
	es_write_utf8_string("   -timeout <milliseconds>\r\n");
	es_write_utf8_string("        Timeout after the specified number of milliseconds to wait for\r\n");
	es_write_utf8_string("        the Everything database to load before sending a query.\r\n");
	es_write_utf8_string("        \r\n");
	es_write_utf8_string("   -set-run-count <filename> <count>\r\n");
	es_write_utf8_string("        Set the run count for the specified filename.\r\n");
	es_write_utf8_string("   -inc-run-count <filename>\r\n");
	es_write_utf8_string("        Increment the run count for the specified filename by one.\r\n");
	es_write_utf8_string("   -get-run-count <filename>\r\n");
	es_write_utf8_string("        Display the run count for the specified filename.\r\n");
	es_write_utf8_string("   -get-result-count\r\n");
	es_write_utf8_string("        Display the result count for the specified search.\r\n");
	es_write_utf8_string("   -get-total-size\r\n");
	es_write_utf8_string("        Display the total result size for the specified search.\r\n");
	es_write_utf8_string("   -save-settings, -clear-settings\r\n");
	es_write_utf8_string("        Save or clear settings.\r\n");
	es_write_utf8_string("   -version\r\n");
	es_write_utf8_string("        Display ES major.minor.revision.build version and exit.\r\n");
	es_write_utf8_string("   -get-everything-version\r\n");
	es_write_utf8_string("        Display Everything major.minor.revision.build version and exit.\r\n");
	es_write_utf8_string("   -exit\r\n");
	es_write_utf8_string("        Exit Everything.\r\n");
	es_write_utf8_string("        Returns after Everything process closes.\r\n");
	es_write_utf8_string("   -save-db\r\n");
	es_write_utf8_string("        Save the Everything database to disk.\r\n");
	es_write_utf8_string("        Returns after saving completes.\r\n");
	es_write_utf8_string("   -reindex\r\n");
	es_write_utf8_string("        Force Everything to reindex.\r\n");
	es_write_utf8_string("        Returns after indexing completes.\r\n");
	es_write_utf8_string("   -no-result-error\r\n");
	es_write_utf8_string("        Set the error level if no results are found.\r\n");
	es_write_utf8_string("   -get-folder-size <filename>\r\n");
	es_write_utf8_string("        Display the total folder size for the specified filename.\r\n");
	es_write_utf8_string("\r\n");
	es_write_utf8_string("\r\n");
	es_write_utf8_string("Notes \r\n");
	es_write_utf8_string("    Internal -'s in options can be omitted, eg: -nodigitgrouping\r\n");
	es_write_utf8_string("    Switches can also start with a /\r\n");
	es_write_utf8_string("    Use double quotes to escape spaces and switches.\r\n");
	es_write_utf8_string("    Switches can be disabled by prefixing them with no-, eg: -no-size.\r\n");
	es_write_utf8_string("    Use a ^ prefix or wrap with double quotes (\") to escape \\ & | > < ^\r\n");
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
	array_t local_column_color_array;
	array_t local_column_width_array;
	array_t local_column_array;
	
	wchar_buf_init(&argv_wcbuf);
	wchar_buf_init(&search_wcbuf);
	wchar_buf_init(&filter_wcbuf);
	wchar_buf_init(&instance_name_wcbuf);
	pool_init(&local_column_color_pool);
	pool_init(&local_column_width_pool);
	pool_init(&local_column_pool);
	array_init(&local_column_color_array);
	array_init(&local_column_width_array);
	array_init(&local_column_array);
	
	get_folder_size_filename = NULL;
	es_instance_name_wcbuf = &instance_name_wcbuf;
	es_search_wcbuf = &search_wcbuf;
	
	column_color_pool = &local_column_color_pool;
	column_width_pool = &local_column_width_pool;
	column_pool = &local_column_pool;
	column_color_array = &local_column_color_array;
	column_width_array = &local_column_width_array;
	column_array = &local_column_array;
	
/*
printf("PROPERTY %u\n",es_property_id_from_name("date-modified"));
printf("PROPERTY %u\n",es_property_id_from_name("datemodified"));
printf("PROPERTY %u\n",es_property_id_from_name("dm"));
return 0;
*/	

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
				
				es_cibuf_attributes = csbi.wAttributes;
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
		es_get_argv(&argv_wcbuf);
	}
	
	if (es_command_line)
	{
		es_get_argv(&argv_wcbuf);
		
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
					es_help();
					
					goto exit;
				}
			}
*/
			for(;;)
			{
				int basic_property_name_i;
				
				if (es_check_option_utf8_string(argv_wcbuf.buf,"set-run-count"))
				{
					es_expect_command_argv(&argv_wcbuf);
				
					es_run_history_size = sizeof(EVERYTHING_IPC_RUN_HISTORY);
					es_run_history_size = safe_size_add(es_run_history_size,safe_size_mul_sizeof_wchar(safe_size_add_one(argv_wcbuf.length_in_wchars)));

					es_run_history_data = mem_alloc(es_run_history_size);

					wchar_string_copy_wchar_string_n((wchar_t *)(((EVERYTHING_IPC_RUN_HISTORY *)es_run_history_data)+1),argv_wcbuf.buf,argv_wcbuf.length_in_wchars);
					
					es_expect_command_argv(&argv_wcbuf);
					
					((EVERYTHING_IPC_RUN_HISTORY *)es_run_history_data)->run_count = wchar_string_to_int(argv_wcbuf.buf);
					es_run_history_command = EVERYTHING_IPC_COPYDATA_SET_RUN_COUNTW;
				}
				else			
				if (es_check_option_utf8_string(argv_wcbuf.buf,"inc-run-count"))
				{
					es_expect_command_argv(&argv_wcbuf);
					
					es_run_history_size = (wchar_string_get_length_in_wchars(argv_wcbuf.buf) + 1) * sizeof(wchar_t);
					es_run_history_data = wchar_string_alloc_wchar_string_n(argv_wcbuf.buf,argv_wcbuf.length_in_wchars);
					es_run_history_command = EVERYTHING_IPC_COPYDATA_INC_RUN_COUNTW;
				}
				else			
				if (es_check_option_utf8_string(argv_wcbuf.buf,"get-run-count"))
				{
					es_expect_command_argv(&argv_wcbuf);

					es_run_history_size = (wchar_string_get_length_in_wchars(argv_wcbuf.buf) + 1) * sizeof(wchar_t);
					es_run_history_data = wchar_string_alloc_wchar_string_n(argv_wcbuf.buf,argv_wcbuf.length_in_wchars);
					es_run_history_command = EVERYTHING_IPC_COPYDATA_GET_RUN_COUNTW;
				}
				else
				if (es_check_option_utf8_string(argv_wcbuf.buf,"get-folder-size"))
				{
					es_expect_command_argv(&argv_wcbuf);
					
					get_folder_size_filename = wchar_string_alloc_wchar_string_n(argv_wcbuf.buf,argv_wcbuf.length_in_wchars);
				}
				else
				if ((es_check_option_utf8_string(argv_wcbuf.buf,"r")) || (es_check_option_utf8_string(argv_wcbuf.buf,"regex")))
				{
					es_expect_argv(&argv_wcbuf);
					
					if (search_wcbuf.length_in_wchars)
					{
						wchar_buf_cat_wchar(&search_wcbuf,' ');
					}
							
					wchar_buf_cat_utf8_string(&search_wcbuf,"regex:");
					wchar_buf_cat_wchar_string_n(&search_wcbuf,argv_wcbuf.buf,argv_wcbuf.length_in_wchars);
				}
				else
				if ((es_check_option_utf8_string(argv_wcbuf.buf,"i")) || (es_check_option_utf8_string(argv_wcbuf.buf,"case")))
				{
					es_match_case = 1;
				}
				else
				if ((es_check_option_utf8_string(argv_wcbuf.buf,"no-i")) || (es_check_option_utf8_string(argv_wcbuf.buf,"no-case")))
				{
					es_match_case = 0;
				}
				else
				if ((es_check_option_utf8_string(argv_wcbuf.buf,"a")) || (es_check_option_utf8_string(argv_wcbuf.buf,"diacritics")))
				{
					es_match_diacritics = 1;
				}
				else
				if ((es_check_option_utf8_string(argv_wcbuf.buf,"no-a")) || (es_check_option_utf8_string(argv_wcbuf.buf,"no-diacritics")))
				{
					es_match_diacritics = 0;
				}
				else
				if (es_check_option_utf8_string(argv_wcbuf.buf,"instance"))
				{
					es_expect_command_argv(&argv_wcbuf);
					
					if (argv_wcbuf.length_in_wchars)
					{
						wchar_buf_copy_wchar_string_n(&instance_name_wcbuf,argv_wcbuf.buf,argv_wcbuf.length_in_wchars);
					}
				}
				else
				if ((es_check_option_utf8_string(argv_wcbuf.buf,"exit")) || (es_check_option_utf8_string(argv_wcbuf.buf,"quit")))
				{
					es_exit_everything = 1;
				}
				else
				if ((es_check_option_utf8_string(argv_wcbuf.buf,"re-index")) || (es_check_option_utf8_string(argv_wcbuf.buf,"re-build")) || (es_check_option_utf8_string(argv_wcbuf.buf,"update")))
				{
					es_reindex = 1;
				}
				else
				if (es_check_option_utf8_string(argv_wcbuf.buf,"save-db"))
				{
					es_save_db = 1;
				}
				else
				if (es_check_option_utf8_string(argv_wcbuf.buf,"highlight-color"))
				{
					es_expect_command_argv(&argv_wcbuf);
					
					es_highlight_color = wchar_string_to_int(argv_wcbuf.buf);
				}
				else
				if (es_check_option_utf8_string(argv_wcbuf.buf,"highlight"))
				{
					es_highlight = 1;
				}
				else			
				if (es_check_option_utf8_string(argv_wcbuf.buf,"no-highlight"))
				{
					es_highlight = 0;
				}
				else
				if (es_check_option_utf8_string(argv_wcbuf.buf,"m3u"))
				{
					es_export_type = ES_EXPORT_TYPE_M3U;
				}
				else				
				if (es_check_option_utf8_string(argv_wcbuf.buf,"export-m3u"))
				{
					es_expect_command_argv(&argv_wcbuf);
					
					es_export_file = os_create_file(argv_wcbuf.buf);
					if (es_export_file != INVALID_HANDLE_VALUE)
					{
						es_export_type = ES_EXPORT_TYPE_M3U;
					}
					else
					{
						es_fatal(ES_ERROR_CREATE_FILE);
					}
				}
				else					
				if (es_check_option_utf8_string(argv_wcbuf.buf,"m3u8"))
				{
					es_export_type = ES_EXPORT_TYPE_M3U8;
				}
				else				
				if (es_check_option_utf8_string(argv_wcbuf.buf,"export-m3u8"))
				{
					es_expect_command_argv(&argv_wcbuf);
					
					es_export_file = os_create_file(argv_wcbuf.buf);
					if (es_export_file != INVALID_HANDLE_VALUE)
					{
						es_export_type = ES_EXPORT_TYPE_M3U8;
					}
					else
					{
						es_fatal(ES_ERROR_CREATE_FILE);
					}
				}
				else		
				if (es_check_option_utf8_string(argv_wcbuf.buf,"csv"))
				{
					es_export_type = ES_EXPORT_TYPE_CSV;
				}
				else				
				if (es_check_option_utf8_string(argv_wcbuf.buf,"tsv"))
				{
					es_export_type = ES_EXPORT_TYPE_TSV;
				}
				else				
				if (es_check_option_utf8_string(argv_wcbuf.buf,"export-csv"))
				{
					es_expect_command_argv(&argv_wcbuf);
					
					es_export_file = os_create_file(argv_wcbuf.buf);
					if (es_export_file != INVALID_HANDLE_VALUE)
					{
						es_export_type = ES_EXPORT_TYPE_CSV;
					}
					else
					{
						es_fatal(ES_ERROR_CREATE_FILE);
					}
				}
				else		
				if (es_check_option_utf8_string(argv_wcbuf.buf,"export-tsv"))
				{
					es_expect_command_argv(&argv_wcbuf);
					
					es_export_file = os_create_file(argv_wcbuf.buf);
					if (es_export_file != INVALID_HANDLE_VALUE)
					{
						es_export_type = ES_EXPORT_TYPE_TSV;
					}
					else
					{
						es_fatal(ES_ERROR_CREATE_FILE);
					}
				}
				else		
				if (es_check_option_utf8_string(argv_wcbuf.buf,"efu"))
				{
					es_export_type = ES_EXPORT_TYPE_EFU;
				}
				else		
				if (es_check_option_utf8_string(argv_wcbuf.buf,"export-efu"))
				{
					es_expect_command_argv(&argv_wcbuf);
					
					es_export_file = os_create_file(argv_wcbuf.buf);
					if (es_export_file != INVALID_HANDLE_VALUE)
					{
						es_export_type = ES_EXPORT_TYPE_EFU;
					}
					else
					{
						es_fatal(ES_ERROR_CREATE_FILE);
					}
				}
				else		
				if (es_check_option_utf8_string(argv_wcbuf.buf,"txt"))
				{
					es_export_type = ES_EXPORT_TYPE_TXT;
				}
				else				
				if (es_check_option_utf8_string(argv_wcbuf.buf,"export-txt"))
				{
					es_expect_command_argv(&argv_wcbuf);
					
					es_export_file = os_create_file(argv_wcbuf.buf);
					if (es_export_file != INVALID_HANDLE_VALUE)
					{
						es_export_type = ES_EXPORT_TYPE_TXT;
					}
					else
					{
						es_fatal(ES_ERROR_CREATE_FILE);
					}
				}
				else
				if (es_check_option_utf8_string(argv_wcbuf.buf,"cp"))
				{
					es_expect_command_argv(&argv_wcbuf);
					
					es_cp = wchar_string_to_int(argv_wcbuf.buf);
				}
				else
				if ((es_check_option_utf8_string(argv_wcbuf.buf,"w")) || (es_check_option_utf8_string(argv_wcbuf.buf,"ww")) || (es_check_option_utf8_string(argv_wcbuf.buf,"whole-word")) || (es_check_option_utf8_string(argv_wcbuf.buf,"whole-words")))
				{
					es_match_whole_word = 1;
				}
				else
				if ((es_check_option_utf8_string(argv_wcbuf.buf,"no-w")) || (es_check_option_utf8_string(argv_wcbuf.buf,"no-ww")) || (es_check_option_utf8_string(argv_wcbuf.buf,"no-whole-word")) || (es_check_option_utf8_string(argv_wcbuf.buf,"no-whole-words")))
				{
					es_match_whole_word = 0;
				}
				else
				if ((es_check_option_utf8_string(argv_wcbuf.buf,"p")) || (es_check_option_utf8_string(argv_wcbuf.buf,"match-path")))
				{
					es_match_path = 1;
				}
				else
				if (es_check_option_utf8_string(argv_wcbuf.buf,"no-p"))
				{
					es_match_path = 0;
				}
				else
				if ((es_check_option_utf8_string(argv_wcbuf.buf,"file-name-width")) || (es_check_option_utf8_string(argv_wcbuf.buf,"file-name-wide")))
				{
					es_expect_command_argv(&argv_wcbuf);

					column_width_set(EVERYTHING3_PROPERTY_ID_FULL_PATH,wchar_string_to_int(argv_wcbuf.buf));
				}
				else
				if ((es_check_option_utf8_string(argv_wcbuf.buf,"name-width")) || (es_check_option_utf8_string(argv_wcbuf.buf,"name-wide")))
				{
					es_expect_command_argv(&argv_wcbuf);
					
					column_width_set(EVERYTHING3_PROPERTY_ID_NAME,wchar_string_to_int(argv_wcbuf.buf));
				}
				else
				if ((es_check_option_utf8_string(argv_wcbuf.buf,"path-width")) || (es_check_option_utf8_string(argv_wcbuf.buf,"path-wide")))
				{
					es_expect_command_argv(&argv_wcbuf);
					
					column_width_set(EVERYTHING3_PROPERTY_ID_PATH,wchar_string_to_int(argv_wcbuf.buf));
				}
				else
				if ((es_check_option_utf8_string(argv_wcbuf.buf,"extension-width")) || (es_check_option_utf8_string(argv_wcbuf.buf,"extension-wide")) || (es_check_option_utf8_string(argv_wcbuf.buf,"ext-width")) || (es_check_option_utf8_string(argv_wcbuf.buf,"ext-wide")))
				{
					es_expect_command_argv(&argv_wcbuf);
					
					column_width_set(EVERYTHING3_PROPERTY_ID_EXTENSION,wchar_string_to_int(argv_wcbuf.buf));
				}
				else
				if ((es_check_option_utf8_string(argv_wcbuf.buf,"size-width")) || (es_check_option_utf8_string(argv_wcbuf.buf,"size-wide")))
				{
					es_expect_command_argv(&argv_wcbuf);
					
					column_width_set(EVERYTHING3_PROPERTY_ID_SIZE,wchar_string_to_int(argv_wcbuf.buf));
				}
				else
				if ((es_check_option_utf8_string(argv_wcbuf.buf,"date-created-width")) || (es_check_option_utf8_string(argv_wcbuf.buf,"date-created-wide")) || (es_check_option_utf8_string(argv_wcbuf.buf,"dc-width")) || (es_check_option_utf8_string(argv_wcbuf.buf,"dc-wide")))
				{
					es_expect_command_argv(&argv_wcbuf);
					
					column_width_set(EVERYTHING3_PROPERTY_ID_DATE_CREATED,wchar_string_to_int(argv_wcbuf.buf));
				}
				else
				if ((es_check_option_utf8_string(argv_wcbuf.buf,"date-modified-width")) || (es_check_option_utf8_string(argv_wcbuf.buf,"date-modified-wide")) || (es_check_option_utf8_string(argv_wcbuf.buf,"dm-width")) || (es_check_option_utf8_string(argv_wcbuf.buf,"dm-wide")))
				{
					es_expect_command_argv(&argv_wcbuf);
					
					column_width_set(EVERYTHING3_PROPERTY_ID_DATE_MODIFIED,wchar_string_to_int(argv_wcbuf.buf));
				}
				else
				if ((es_check_option_utf8_string(argv_wcbuf.buf,"date-accessed-width")) || (es_check_option_utf8_string(argv_wcbuf.buf,"date-accessed-wide")) || (es_check_option_utf8_string(argv_wcbuf.buf,"da-width")) || (es_check_option_utf8_string(argv_wcbuf.buf,"da-wide")))
				{
					es_expect_command_argv(&argv_wcbuf);
					
					column_width_set(EVERYTHING3_PROPERTY_ID_DATE_ACCESSED,wchar_string_to_int(argv_wcbuf.buf));
				}
				else
				if ((es_check_option_utf8_string(argv_wcbuf.buf,"attributes-width")) || (es_check_option_utf8_string(argv_wcbuf.buf,"attributes-wide")))
				{
					es_expect_command_argv(&argv_wcbuf);
					
					column_width_set(EVERYTHING3_PROPERTY_ID_ATTRIBUTES,wchar_string_to_int(argv_wcbuf.buf));
				}
				else
				if ((es_check_option_utf8_string(argv_wcbuf.buf,"file-list-file-name-width")) || (es_check_option_utf8_string(argv_wcbuf.buf,"file-list-file-name-wide")))
				{
					es_expect_command_argv(&argv_wcbuf);
					
					column_width_set(EVERYTHING3_PROPERTY_ID_FILE_LIST_FILENAME,wchar_string_to_int(argv_wcbuf.buf));
				}
				else
				if ((es_check_option_utf8_string(argv_wcbuf.buf,"run-count-width")) || (es_check_option_utf8_string(argv_wcbuf.buf,"run-count-wide")))
				{
					es_expect_command_argv(&argv_wcbuf);
					
					column_width_set(EVERYTHING3_PROPERTY_ID_RUN_COUNT,wchar_string_to_int(argv_wcbuf.buf));
				}
				else
				if ((es_check_option_utf8_string(argv_wcbuf.buf,"date-run-width")) || (es_check_option_utf8_string(argv_wcbuf.buf,"date-run-wide")))
				{
					es_expect_command_argv(&argv_wcbuf);
					
					column_width_set(EVERYTHING3_PROPERTY_ID_DATE_RUN,wchar_string_to_int(argv_wcbuf.buf));
				}
				else
				if ((es_check_option_utf8_string(argv_wcbuf.buf,"date-recently-changed-width")) || (es_check_option_utf8_string(argv_wcbuf.buf,"date-recently-changed-wide")) || (es_check_option_utf8_string(argv_wcbuf.buf,"rc-width")) || (es_check_option_utf8_string(argv_wcbuf.buf,"rc-wide")))
				{
					es_expect_command_argv(&argv_wcbuf);
					
					column_width_set(EVERYTHING3_PROPERTY_ID_DATE_RECENTLY_CHANGED,wchar_string_to_int(argv_wcbuf.buf));
				}
				else
				if (es_check_option_utf8_string(argv_wcbuf.buf,"size-leading-zero"))
				{
					es_size_leading_zero = 1;
				}
				else
				if (es_check_option_utf8_string(argv_wcbuf.buf,"no-size-leading-zero"))
				{
					es_size_leading_zero = 0;
				}
				else
				if (es_check_option_utf8_string(argv_wcbuf.buf,"run-count-leading-zero"))
				{
					es_run_count_leading_zero = 1;
				}
				else
				if (es_check_option_utf8_string(argv_wcbuf.buf,"no-run-count-leading-zero"))
				{
					es_run_count_leading_zero = 0;
				}
				else
				if (es_check_option_utf8_string(argv_wcbuf.buf,"no-digit-grouping"))
				{
					es_digit_grouping = 0;
				}
				else
				if (es_check_option_utf8_string(argv_wcbuf.buf,"digit-grouping"))
				{
					es_digit_grouping = 1;
				}
				else
				if (es_check_option_utf8_string(argv_wcbuf.buf,"size-format"))
				{
					es_expect_command_argv(&argv_wcbuf);
					
					es_size_format = wchar_string_to_int(argv_wcbuf.buf);
				}
				else
				if (es_check_option_utf8_string(argv_wcbuf.buf,"date-format"))
				{
					es_expect_command_argv(&argv_wcbuf);

					es_date_format = wchar_string_to_int(argv_wcbuf.buf);
				}
				else			
				if ((es_check_option_utf8_string(argv_wcbuf.buf,"pause")) || (es_check_option_utf8_string(argv_wcbuf.buf,"more")))
				{
					es_pause = 1;
				}
				else
				if ((es_check_option_utf8_string(argv_wcbuf.buf,"no-pause")) || (es_check_option_utf8_string(argv_wcbuf.buf,"no-more")))
				{
					es_pause = 0;
				}
				else
				if (es_check_option_utf8_string(argv_wcbuf.buf,"empty-search-help"))
				{
					es_empty_search_help = 1;
					es_hide_empty_search_results = 0;
				}
				else
				if (es_check_option_utf8_string(argv_wcbuf.buf,"no-empty-search-help"))
				{
					es_empty_search_help = 0;
				}
				else
				if (es_check_option_utf8_string(argv_wcbuf.buf,"hide-empty-search-results"))
				{
					es_hide_empty_search_results = 1;
					es_empty_search_help = 0;
				}
				else
				if (es_check_option_utf8_string(argv_wcbuf.buf,"no-hide-empty-search-results"))
				{
					es_hide_empty_search_results = 0;
				}
				else
				if (es_check_option_utf8_string(argv_wcbuf.buf,"save-settings"))
				{
					es_save = 1;
				}
				else
				if (es_check_option_utf8_string(argv_wcbuf.buf,"clear-settings"))
				{
					wchar_buf_t ini_filename_wcbuf;

					wchar_buf_init(&ini_filename_wcbuf);
					
					if (config_get_filename(0,0,&ini_filename_wcbuf))
					{
						DeleteFile(ini_filename_wcbuf.buf);

						es_write_utf8_string("Settings saved.\r\n");
					}
					
					wchar_buf_kill(&ini_filename_wcbuf);
					
					goto exit;
				}
				else
				if (es_check_option_utf8_string(argv_wcbuf.buf,"path"))
				{
					wchar_buf_t path_wcbuf;

					// make sure we process this before es_check_column_param
					// otherwise es_check_column_param will eat -path.
					wchar_buf_init(&path_wcbuf);

					es_expect_command_argv(&argv_wcbuf);
					
					// relative path.
					os_get_full_path_name(argv_wcbuf.buf,&path_wcbuf);
		
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
				if (es_check_option_utf8_string(argv_wcbuf.buf,"parent-path"))
				{
					wchar_buf_t path_wcbuf;

					wchar_buf_init(&path_wcbuf);
				
					es_expect_command_argv(&argv_wcbuf);
					
					// relative path.
					os_get_full_path_name(argv_wcbuf.buf,&path_wcbuf);
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
				if (es_check_option_utf8_string(argv_wcbuf.buf,"parent"))
				{
					wchar_buf_t path_wcbuf;

					wchar_buf_init(&path_wcbuf);
					
					es_expect_command_argv(&argv_wcbuf);
					
					// relative path.
					os_get_full_path_name(argv_wcbuf.buf,&path_wcbuf);
					
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
				if (es_check_color_param(argv_wcbuf.buf,&basic_property_name_i))
				{
					es_expect_command_argv(&argv_wcbuf);
					
					column_color_set(property_basic_name_to_id_array[basic_property_name_i].id,wchar_string_to_int(argv_wcbuf.buf));
				}
				else
				if (es_check_column_param(argv_wcbuf.buf))
				{
				}
				else
				if (es_check_option_utf8_string(argv_wcbuf.buf,"sort-ascending"))
				{
					es_sort_ascending = 1;
				}
				else
				if (es_check_option_utf8_string(argv_wcbuf.buf,"sort-descending"))
				{
					es_sort_ascending = -1;
				}
				else
				if (es_check_option_utf8_string(argv_wcbuf.buf,"sort"))
				{	
					wchar_buf_t sort_name_wcbuf;
					int basic_property_name_i;
					
					wchar_buf_init(&sort_name_wcbuf);
					es_expect_command_argv(&argv_wcbuf);

					for(basic_property_name_i=0;basic_property_name_i<PROPERTY_BASIC_COUNT;basic_property_name_i++)
					{
						if (es_check_param_ascii(argv_wcbuf.buf,property_basic_name_to_id_array[basic_property_name_i].name))
						{
							es_sort_property_id = property_basic_name_to_id_array[basic_property_name_i].id;

							break;
						}
						else
						{
							wchar_buf_printf(&sort_name_wcbuf,"%s-ascending",property_basic_name_to_id_array[basic_property_name_i].name);

							if (es_check_option_wchar_string_name(argv_wcbuf.buf,sort_name_wcbuf.buf))
							{
								es_sort_property_id = property_basic_name_to_id_array[basic_property_name_i].id;
								es_sort_ascending = 1;

								break;
							}
							else
							{
								wchar_buf_printf(&sort_name_wcbuf,"%s-descending",property_basic_name_to_id_array[basic_property_name_i].name);
								
								if (es_check_option_wchar_string_name(argv_wcbuf.buf,sort_name_wcbuf.buf))
								{
									es_sort_property_id = property_basic_name_to_id_array[basic_property_name_i].id;
									es_sort_ascending = -1;

									break;
								}
							}
						}
					}
					
					if (basic_property_name_i == PROPERTY_BASIC_COUNT)
					{
						es_fatal(ES_ERROR_EXPECTED_SWITCH_PARAMETER);
					}

					wchar_buf_kill(&sort_name_wcbuf);
				}
				else
				if (es_check_option_utf8_string(argv_wcbuf.buf,"ipc1"))
				{	
					es_ipc_version = ES_IPC_VERSION_FLAG_IPC1; 
				}
				else
				if (es_check_option_utf8_string(argv_wcbuf.buf,"ipc2"))
				{	
					es_ipc_version = ES_IPC_VERSION_FLAG_IPC2; 
				}
				else
				if (es_check_option_utf8_string(argv_wcbuf.buf,"ipc3"))
				{	
					es_ipc_version = ES_IPC_VERSION_FLAG_IPC3; 
				}
				else
				if (es_check_option_utf8_string(argv_wcbuf.buf,"header"))
				{	
					es_header = 1; 
				}
				else
				if (es_check_option_utf8_string(argv_wcbuf.buf,"no-header"))
				{	
					es_header = 0; 
				}
				else
				if (es_check_option_utf8_string(argv_wcbuf.buf,"double-quote"))
				{	
					es_double_quote = 1; 
					es_csv_double_quote = 1;
				}
				else
				if (es_check_option_utf8_string(argv_wcbuf.buf,"no-double-quote"))
				{	
					es_double_quote = 0; 
					es_csv_double_quote = 0;
				}
				else
				if (es_check_option_utf8_string(argv_wcbuf.buf,"version"))
				{	
					es_write_utf8_string(VERSION_TEXT);
					es_write_utf8_string("\r\n");

					goto exit;
				}
				else
				if (es_check_option_utf8_string(argv_wcbuf.buf,"get-everything-version"))
				{
					es_get_everything_version = 1;
				}
				else
				if (es_check_option_utf8_string(argv_wcbuf.buf,"utf8-bom"))
				{
					es_utf8_bom = 1;
				}
				else
				if (es_check_sorts(argv_wcbuf.buf))
				{
				}
				else
				if (es_check_option_utf8_string(argv_wcbuf.buf,"on"))
				{	
					es_sort_property_id = EVERYTHING3_PROPERTY_ID_NAME;
					es_sort_ascending = 1;
				}
				else
				if (es_check_option_utf8_string(argv_wcbuf.buf,"o-n"))
				{	
					es_sort_property_id = EVERYTHING3_PROPERTY_ID_NAME;
					es_sort_ascending = -1;
				}
				else
				if (es_check_option_utf8_string(argv_wcbuf.buf,"os"))
				{	
					es_sort_property_id = EVERYTHING3_PROPERTY_ID_SIZE;
					es_sort_ascending = 1;
				}
				else
				if (es_check_option_utf8_string(argv_wcbuf.buf,"o-s"))
				{	
					es_sort_property_id = EVERYTHING3_PROPERTY_ID_SIZE;
					es_sort_ascending = -1;
				}
				else
				if (es_check_option_utf8_string(argv_wcbuf.buf,"oe"))
				{	
					es_sort_property_id = EVERYTHING3_PROPERTY_ID_EXTENSION;
					es_sort_ascending = 1;
				}
				else
				if (es_check_option_utf8_string(argv_wcbuf.buf,"o-e"))
				{	
					es_sort_property_id = EVERYTHING3_PROPERTY_ID_EXTENSION;
					es_sort_ascending = -1;
				}
				else
				if (es_check_option_utf8_string(argv_wcbuf.buf,"od"))
				{	
					es_sort_property_id = EVERYTHING3_PROPERTY_ID_DATE_MODIFIED;
					es_sort_ascending = 1;
				}
				else
				if (es_check_option_utf8_string(argv_wcbuf.buf,"o-d"))
				{	
					es_sort_property_id = EVERYTHING3_PROPERTY_ID_DATE_MODIFIED;
					es_sort_ascending = -1;
				}
				else
				if (es_check_option_utf8_string(argv_wcbuf.buf,"s"))
				{
					es_sort_property_id = EVERYTHING3_PROPERTY_ID_PATH;
				}
				else
				if ((es_check_option_utf8_string(argv_wcbuf.buf,"n")) || (es_check_option_utf8_string(argv_wcbuf.buf,"max-results")))
				{
					es_expect_command_argv(&argv_wcbuf);
					
					es_max_results = wchar_string_to_int(argv_wcbuf.buf);
				}
				else
				if ((es_check_option_utf8_string(argv_wcbuf.buf,"o")) || (es_check_option_utf8_string(argv_wcbuf.buf,"offset")))
				{
					es_expect_command_argv(&argv_wcbuf);
					
					es_offset = wchar_string_to_int(argv_wcbuf.buf);
				}
				else
				if (es_check_option_utf8_string(argv_wcbuf.buf,"time-out"))
				{
					es_expect_command_argv(&argv_wcbuf);
					
					es_timeout = (DWORD)wchar_string_to_int(argv_wcbuf.buf);
				}
				else
				if (es_check_option_utf8_string(argv_wcbuf.buf,"no-time-out"))
				{
					es_timeout = 0;
				}
				else
				if (es_check_option_utf8_string(argv_wcbuf.buf,"ad"))
				{	
					// add folder:
					es_append_filter(&filter_wcbuf,L"folder:");
				}
				else
				if (es_check_option_utf8_string(argv_wcbuf.buf,"a-d"))
				{	
					// add folder:
					es_append_filter(&filter_wcbuf,L"file:");
				}
				else
				if (es_check_option_utf8_string(argv_wcbuf.buf,"get-result-count"))
				{
					es_get_result_count = 1;
				}
				else
				if (es_check_option_utf8_string(argv_wcbuf.buf,"get-total-size"))
				{
					es_get_total_size = 1;
				}
				else
				if ((es_check_option_utf8_string(argv_wcbuf.buf,"no-result-error")) || (es_check_option_utf8_string(argv_wcbuf.buf,"no-results-error")) || (es_check_option_utf8_string(argv_wcbuf.buf,"error-on-no-results")))
				{
					es_no_result_error = 1;
				}
				else
				if ((es_check_option_utf8_string(argv_wcbuf.buf,"q")) || (es_check_option_utf8_string(argv_wcbuf.buf,"search")))
				{
					// this removes quotes from the search string.
					es_expect_command_argv(&argv_wcbuf);
							
					if (search_wcbuf.length_in_wchars)
					{
						wchar_buf_cat_wchar(&search_wcbuf,' ');
					}

					wchar_buf_cat_wchar_string(&search_wcbuf,argv_wcbuf.buf);
				}
				else
				if ((es_check_option_utf8_string(argv_wcbuf.buf,"q*")) || (es_check_option_utf8_string(argv_wcbuf.buf,"search*")))
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
			/*	if (es_check_option_utf8_string(argv_wcbuf.buf,"%"))
				{	
					// this would conflict with powershell..
					for(;;)
					{
						const wchar_t *s;

						command_line = es_get_argv(command_line);
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
			*/	if (((argv_wcbuf.buf[0] == '-') || (argv_wcbuf.buf[0] == '/')) && (argv_wcbuf.buf[1] == 'a') && (argv_wcbuf.buf[2]))
				{
					const wchar_t *p;
					wchar_buf_t attrib_wcbuf;
					wchar_buf_t notattrib_wcbuf;

					wchar_buf_init(&attrib_wcbuf);
					wchar_buf_init(&notattrib_wcbuf);
					p = argv_wcbuf.buf + 2;
					
					// TODO handle unknown a switches.
					while(*p)
					{
						if ((*p == '-') && (p[1]))
						{
							wchar_buf_cat_wchar(&notattrib_wcbuf,unicode_ascii_to_lower(p[1]));
							
							p += 2;
						}
						else
						{
							wchar_buf_cat_wchar(&attrib_wcbuf,unicode_ascii_to_lower(*p));
							
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
				if ((es_check_option_utf8_string(argv_wcbuf.buf,"?")) || (es_check_option_utf8_string(argv_wcbuf.buf,"help")) || (es_check_option_utf8_string(argv_wcbuf.buf,"h")))
				{
					// user requested help
					es_help();
					
					goto exit;
				}
				else
				if ((argv_wcbuf.buf[0] == '-') && (!es_is_literal_switch(argv_wcbuf.buf)))
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

				es_get_argv(&argv_wcbuf);
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
			es_write_utf8_string("Settings saved.\r\n");
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
		es_everything_hwnd = es_find_ipc_window();
		
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
				
			es_write_dword(major);
			es_write_utf8_string(".");
			es_write_dword(minor);
			es_write_utf8_string(".");
			es_write_dword(revision);
			es_write_utf8_string(".");
			es_write_dword(build);
			es_write_utf8_string("\r\n");
		}
		else
		{
			es_fatal(ES_ERROR_IPC);
		}
		
		perform_search = 0;
	}
	
	// reindex?
	if (es_reindex)
	{
		es_everything_hwnd = es_find_ipc_window();
		
		if (es_everything_hwnd)
		{
			SendMessage(es_everything_hwnd,EVERYTHING_WM_IPC,EVERYTHING_IPC_REBUILD_DB,0);
			
			// poll until db is available.
			es_wait_for_db_loaded();
		}
		else
		{
			es_fatal(ES_ERROR_IPC);
		}

		perform_search = 0;
	}
	
	// run history command
	if (es_run_history_command)
	{
		es_do_run_history_command();

		perform_search = 0;
	}
	
	if (get_folder_size_filename)
	{
		es_get_folder_size(get_folder_size_filename);
		
		mem_free(get_folder_size_filename);

		perform_search = 0;
	}
	
	// save db
	// do this after a reindex.
	if (es_save_db)
	{
		es_everything_hwnd = es_find_ipc_window();
		
		if (es_everything_hwnd)
		{
			SendMessage(es_everything_hwnd,EVERYTHING_WM_IPC,EVERYTHING_IPC_SAVE_DB,0);
			
			// wait until not busy..
			es_wait_for_db_not_busy();
		}
		else
		{
			es_fatal(ES_ERROR_IPC);
		}

		perform_search = 0;
	}
	
	// Exit Everything?
	if (es_exit_everything)
	{
		es_everything_hwnd = es_find_ipc_window();
		
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
					es_fatal(ES_ERROR_SEND_MESSAGE);
				}
			}
			else
			{
				es_fatal(ES_ERROR_SEND_MESSAGE);
			}
		}
		else
		{
			es_fatal(ES_ERROR_IPC);
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
				es_help();
				
				goto exit;
			}

			if (es_hide_empty_search_results)
			{
				goto exit;
			}
		}
		
		// add filename column
		if (!column_find(EVERYTHING3_PROPERTY_ID_FULL_PATH))
		{
			if (!column_find(EVERYTHING3_PROPERTY_ID_NAME))
			{
				if (!column_find(EVERYTHING3_PROPERTY_ID_PATH))
				{
					column_add(EVERYTHING3_PROPERTY_ID_FULL_PATH);
				}
			}
		}

		// apply highlighting to columns.
		// just the standard properties.
		// TODO: users will need to call -highlight-column <property-name> to highlight other columns.
		if (es_highlight)
		{
			column_t *column;
			
			column = column_order_start;
			
			while(column)
			{
				switch(column->property_id)
				{
					case EVERYTHING3_PROPERTY_ID_FULL_PATH:
					case EVERYTHING3_PROPERTY_ID_NAME:
					case EVERYTHING3_PROPERTY_ID_PATH:
						column->flags |= COLUMN_FLAG_HIGHLIGHT;
						break;
				}
				
				column = column->order_next;
			}
		}
				
		// write export headers
		// write header
		if (es_export_type)
		{
			if (es_utf8_bom)
			{
				if (es_export_file != INVALID_HANDLE_VALUE)
				{
					BYTE bom[3];
					DWORD numwritten;
					
					// 0xEF,0xBB,0xBF.
					bom[0] = 0xEF;
					bom[1] = 0xBB;
					bom[2] = 0xBF;
					
					WriteFile(es_export_file,bom,3,&numwritten,0);
				}
			}
		
			// disable pause
			es_pause = 0;
		
			// remove highlighting.
			
			{
				column_t *column;
				
				column = column_order_start;
				
				while(column)
				{
					if (column->flags & COLUMN_FLAG_HIGHLIGHT)
					{
						column->flags &= (~COLUMN_FLAG_HIGHLIGHT);
					}

					column = column->order_next;
				}
			}
			
			if (es_export_file != INVALID_HANDLE_VALUE)
			{
				es_export_buf = mem_alloc(ES_EXPORT_BUF_SIZE);
				es_export_p = es_export_buf;
				es_export_remaining = ES_EXPORT_BUF_SIZE;
			}
			
			if ((es_export_type == ES_EXPORT_TYPE_CSV) || (es_export_type == ES_EXPORT_TYPE_TSV))
			{
//TODO: we have to output header later, as it might change with ipc3 or ipc1.
				if (es_header)
				{
					const column_t **column_p;
					SIZE_T column_run;
					int done_first;
//					es_buf_t name_cbuf;
//					es_buf_init(&name_cbuf);
					
					done_first = 0;
					
					column_p = (const column_t **)local_column_array.indexes;
					column_run = local_column_array.count;
					
					while(column_run)
					{
						if (done_first)
						{
							es_fwrite_utf8_string((es_export_type == ES_EXPORT_TYPE_CSV) ? "," : "\t");
						}
						else
						{
							done_first = 1;
						}

//TODO:
//						es_column_get_name();
//						es_fwrite_wchar_string(es_column_names[es_columns[columni]]);

						column_p++;
						column_run--;
					}

					es_fwrite_utf8_string("\r\n");
				}
			}
			else
			if (es_export_type == ES_EXPORT_TYPE_EFU)
			{
				int was_size_column;
				int was_date_modified_column;
				int was_date_created_column;
				int was_attributes_column;

				was_size_column = 0;
				was_date_modified_column = 0;
				was_date_created_column = 0;
				was_attributes_column = 0;
							
				{
					column_t *column;
					
					column = column_order_start;
					
					while(column)
					{
						switch(column->property_id)
						{
							case EVERYTHING3_PROPERTY_ID_SIZE:
								was_size_column = 1;
								break;
								
							case EVERYTHING3_PROPERTY_ID_DATE_MODIFIED:
								was_date_modified_column = 1;
								break;
								
							case EVERYTHING3_PROPERTY_ID_DATE_CREATED:
								was_date_created_column = 1;
								break;
								
							case EVERYTHING3_PROPERTY_ID_ATTRIBUTES:
								was_attributes_column = 1;
								break;
						}
						
						column = column->order_next;
					}
				}
				
				// reset columns and force Filename,Size,Date Modified,Date Created,Attributes.
				column_clear_all();

				column_add(EVERYTHING3_PROPERTY_ID_FULL_PATH);
				
				if (was_size_column)
				{
					column_add(EVERYTHING3_PROPERTY_ID_SIZE);
				}

				if (was_date_modified_column)
				{
					column_add(EVERYTHING3_PROPERTY_ID_DATE_MODIFIED);
				}

				if (was_date_created_column)
				{
					column_add(EVERYTHING3_PROPERTY_ID_DATE_CREATED);
				}

				if (was_attributes_column)
				{
					column_add(EVERYTHING3_PROPERTY_ID_ATTRIBUTES);
				}

				if (es_header)
				{
					es_fwrite_utf8_string("Filename,Size,Date Modified,Date Created,Attributes\r\n");
				}
			}
			else
			if ((es_export_type == ES_EXPORT_TYPE_TXT) || (es_export_type == ES_EXPORT_TYPE_M3U) || (es_export_type == ES_EXPORT_TYPE_M3U8))
			{
				if (es_export_type == ES_EXPORT_TYPE_M3U)		
				{
					es_cp = CP_ACP;
				}

				// reset columns and force Filename.
				column_clear_all();
				column_add(EVERYTHING3_PROPERTY_ID_FULL_PATH);
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

		if (es_ipc_version & ES_IPC_VERSION_FLAG_IPC3)
		{
			if (es_sendquery3()) 
			{
				// success
				// don't try other versions.
				// we dont need a message loop, exit..
				goto exit;
			}
		}

		es_everything_hwnd = es_find_ipc_window();
		if (es_everything_hwnd)
		{
			if (es_export_type == ES_EXPORT_TYPE_EFU)
			{
				// get indexed file info column for exporting.
				if (SendMessage(es_everything_hwnd,EVERYTHING_WM_IPC,EVERYTHING_IPC_IS_FILE_INFO_INDEXED,EVERYTHING_IPC_FILE_INFO_FILE_SIZE))
				{
					column_add(EVERYTHING3_PROPERTY_ID_SIZE);
				}
				
				if (SendMessage(es_everything_hwnd,EVERYTHING_WM_IPC,EVERYTHING_IPC_IS_FILE_INFO_INDEXED,EVERYTHING_IPC_FILE_INFO_DATE_MODIFIED))
				{
					column_add(EVERYTHING3_PROPERTY_ID_DATE_MODIFIED);
				}
				
				if (SendMessage(es_everything_hwnd,EVERYTHING_WM_IPC,EVERYTHING_IPC_IS_FILE_INFO_INDEXED,EVERYTHING_IPC_FILE_INFO_DATE_CREATED))
				{
					column_add(EVERYTHING3_PROPERTY_ID_DATE_CREATED);
				}
				
				if (SendMessage(es_everything_hwnd,EVERYTHING_WM_IPC,EVERYTHING_IPC_IS_FILE_INFO_INDEXED,EVERYTHING_IPC_FILE_INFO_ATTRIBUTES))
				{
					column_add(EVERYTHING3_PROPERTY_ID_ATTRIBUTES);
				}
			}
	
			if (es_ipc_version & ES_IPC_VERSION_FLAG_IPC2)
			{
				if (es_sendquery2()) 
				{
					// success
					// don't try version 1.
					goto query_sent;
				}
			}

			if (es_ipc_version & ES_IPC_VERSION_FLAG_IPC1)
			{
				if (es_sendquery()) 
				{
					// success
					// don't try other versions.
					goto query_sent;
				}
			}

			es_fatal(ES_ERROR_SEND_MESSAGE);
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

	column_clear_all();
	es_column_color_clear_all();
	es_column_width_clear_all();

	if (es_run_history_data)
	{
		mem_free(es_run_history_data);
	}

	es_flush();
	
	if (es_export_buf)
	{
		mem_free(es_export_buf);
	}
	
	if (es_export_file != INVALID_HANDLE_VALUE)
	{
		CloseHandle(es_export_file);
	}
	
	if (es_ret != ES_ERROR_SUCCESS)
	{
		es_fatal(es_ret);
	}

	array_kill(&local_column_array);
	array_kill(&local_column_width_array);
	array_kill(&local_column_color_array);
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
static void es_get_window_classname(wchar_buf_t *wcbuf)
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
HWND es_find_ipc_window(void)
{
	DWORD tickstart;
	DWORD tick;
	wchar_buf_t window_class_wcbuf;
	HWND ret;

	wchar_buf_init(&window_class_wcbuf);
	
	tickstart = GetTickCount();

	es_get_window_classname(&window_class_wcbuf);

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
			es_fatal(ES_ERROR_IPC);
		}
		
		// try again..
		Sleep(10);
		
		tick = GetTickCount();
		
		if (tick - tickstart > es_timeout)
		{
			// the everything window was not found.
			// we can optionally RegisterWindowMessage("EVERYTHING_IPC_CREATED") and 
			// wait for Everything to post this message to all top level windows when its up and running.
			es_fatal(ES_ERROR_IPC);
		}
	}
	
	wchar_buf_kill(&window_class_wcbuf);

	return ret;
}

// find the Everything IPC window
static void es_wait_for_db_loaded(void)
{
	DWORD tickstart;
	wchar_buf_t window_class_wcbuf;

	wchar_buf_init(&window_class_wcbuf);
	
	tickstart = GetTickCount();
	
	es_get_window_classname(&window_class_wcbuf);
	
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
				es_fatal(ES_ERROR_IPC);
			}
		}
	}
	
	wchar_buf_kill(&window_class_wcbuf);
}

// find the Everything IPC window
static void es_wait_for_db_not_busy(void)
{
	DWORD tickstart;
	wchar_buf_t window_class_wcbuf;

	wchar_buf_init(&window_class_wcbuf);
	
	tickstart = GetTickCount();
	
	es_get_window_classname(&window_class_wcbuf);
	
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
				es_fatal(ES_ERROR_IPC);
			}
		}
	}
	
	wchar_buf_kill(&window_class_wcbuf);
}

// check if param is 
// check if s matches param.
int es_check_option_wchar_string(const wchar_t *argv,const wchar_t *s)
{
	const wchar_t *argv_p;
	
	argv_p = argv;
	
	if ((*argv_p == '-') || (*argv_p == '/'))
	{
		argv_p++;
		
		// allow double -
		if (*argv_p == '-')
		{
			argv_p++;
		}
		
		return es_check_option_wchar_string_name(argv_p,s);
	}
	
	return 0;
}

//TODO: optimize.
int es_check_option_utf8_string(const wchar_t *argv,const ES_UTF8 *s)
{
	int ret;
	wchar_buf_t wcbuf;

	wchar_buf_init(&wcbuf);

	wchar_buf_copy_utf8_string(&wcbuf,s);
	
	ret = es_check_option_wchar_string(argv,wcbuf.buf);
	
	wchar_buf_kill(&wcbuf);
	
	return ret;
}

int es_check_option_wchar_string_name(const wchar_t *param,const wchar_t *s)
{
	while(*s)
	{
		if (*s == '-')
		{
			if (*param == '-')
			{
				param++;
			}
		
			s++;
		}
		else
		{
			if (*param != *s)
			{
				return 0;
			}
		
			param++;
			s++;
		}
	}
	
	if (*param)
	{
		return 0;
	}
	
	return 1;
}

int es_check_param_ascii(const wchar_t *s,const char *search)
{
	const wchar_t *p1;
	const char *p2;
	
	p1 = s;
	p2 = search;
	
	while(*p2)
	{
		if (*p1 == '-')
		{
			if (*p2 == '-')
			{
				p2++;
			}
			
			p1++;
			continue;
		}

		if (*p1 != *p2)
		{
			return 0;
		}
	
		p1++;
		p2++;
	}
	
	if (*p1)
	{
		return 0;
	}
	
	return 1;
}

// TODO: FIXME: unaligned access...
void *es_get_column_data(EVERYTHING_IPC_LIST2 *list,int index,DWORD property_id,DWORD property_highlight)
{	
	char *p;
	EVERYTHING_IPC_ITEM2 *items;
	
	items = (EVERYTHING_IPC_ITEM2 *)(list + 1);
	
	p = ((char *)list) + items[index].data_offset;

	if (list->request_flags & EVERYTHING_IPC_QUERY2_REQUEST_NAME)
	{
		DWORD len;

		if ((property_id == EVERYTHING3_PROPERTY_ID_NAME) && (!property_highlight))
		{
			return p;
		}
		
		len = *(DWORD *)p;
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
		
		len = *(DWORD *)p;
		p += sizeof(DWORD);
		
		p += (len + 1) * sizeof(wchar_t);
	}
	
	if (list->request_flags & EVERYTHING_IPC_QUERY2_REQUEST_FULL_PATH_AND_NAME)
	{
		DWORD len;
		
		if ((property_id == EVERYTHING3_PROPERTY_ID_FULL_PATH) && (!property_highlight))
		{
			return p;
		}
		
		len = *(DWORD *)p;
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
		
		len = *(DWORD *)p;
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
		
		if (property_id == EVERYTHING3_PROPERTY_ID_FILE_LIST_FILENAME)	
		{
			return p;
		}
		
		len = *(DWORD *)p;
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
		
		len = *(DWORD *)p;
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
		
		len = *(DWORD *)p;
		p += sizeof(DWORD);
		
		p += (len + 1) * sizeof(wchar_t);
	}
	
	if (list->request_flags & EVERYTHING_IPC_QUERY2_REQUEST_HIGHLIGHTED_FULL_PATH_AND_NAME)
	{
		DWORD len;
		
		if ((property_id == EVERYTHING3_PROPERTY_ID_FULL_PATH) && (property_highlight))
		{
			return p;
		}
		
		len = *(DWORD *)p;
		p += sizeof(DWORD);
		
		p += (len + 1) * sizeof(wchar_t);
	}			
	
	return 0;
}

void es_format_dir(wchar_buf_t *wcbuf)
{
	wchar_buf_copy_utf8_string(wcbuf,"<DIR>");
}

void es_format_size(ES_UINT64 size,wchar_buf_t *wcbuf)
{
	wchar_buf_empty(wcbuf);
	
	if (size != 0xffffffffffffffffI64)
	{
		if (es_size_format == 0)
		{
			// auto size.
			if (size < 1000)
			{
				wchar_buf_print_UINT64(wcbuf,size);
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
					es_format_number(size/100,wcbuf);
					wchar_buf_cat_utf8_string(wcbuf,suffix);				
				}
			}
		}
		else
		if (es_size_format == 2)
		{
			es_format_number(((size) + 1023) / 1024,wcbuf);
			wchar_buf_cat_utf8_string(wcbuf," KB");
		}
		else
		if (es_size_format == 3)
		{
			es_format_number(((size) + 1048575) / 1048576,wcbuf);
			wchar_buf_cat_utf8_string(wcbuf," MB");
		}
		else
		if (es_size_format == 4)
		{
			es_format_number(((size) + 1073741823) / 1073741824,wcbuf);
			wchar_buf_cat_utf8_string(wcbuf," GB");
		}
		else
		{
			es_format_number(size,wcbuf);
		}
	}

//	es_format_leading_space(buf,es_size_width,es_size_leading_zero);
}

void es_format_leading_space(wchar_t *buf,int size,int ch)
{
	SIZE_T len;
	
	len = wchar_string_get_length_in_wchars(buf);

	if (es_digit_grouping)
	{
		ch = ' ';
	}

	if (len <= INT_MAX)
	{
		if ((int)len < size)
		{
			int i;
			
			MoveMemory(buf+(size-(int)len),buf,((int)len + 1) * sizeof(wchar_t));
			
			for(i=0;i<size-(int)len;i++)
			{
				buf[i] = ch;
			}
		}
	}
}

int es_filetime_to_localtime(SYSTEMTIME *localst,ES_UINT64 ft)
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

void es_format_attributes(DWORD attributes,wchar_buf_t *wcbuf)
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

//	es_space_to_width(buf,es_attributes_width);
}

void es_format_run_count(DWORD run_count,wchar_buf_t *wcbuf)
{
	es_format_number(run_count,wcbuf);

//	es_format_leading_space(buf,es_run_count_width,es_run_count_leading_zero);
}

void es_format_filetime(ES_UINT64 filetime,wchar_buf_t *wcbuf)
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
				
				es_filetime_to_localtime(&st,filetime);
				
				switch(dmyformat)
				{
					case 0: val1 = st.wMonth; val2 = st.wDay; val3 = st.wYear; break; // Month-Day-Year
					default: val1 = st.wDay; val2 = st.wMonth; val3 = st.wYear; break; // Day-Month-Year
					case 2: val1 = st.wYear; val2 = st.wMonth; val3 = st.wDay; break; // Year-Month-Day
				}
				
				wchar_buf_printf(wcbuf,"%02d/%02d/%02d %02d:%02d",val1,val2,val3,st.wHour,st.wMinute);
//TODO:
	//seconds		wsprintf(buf,L"%02d/%02d/%02d %02d:%02d:%02d",val1,val2,val3,st.wHour,st.wMinute,st.wSecond);
				break;
			}
				
			case 1: // ISO-8601
			{
				SYSTEMTIME st;
				es_filetime_to_localtime(&st,filetime);
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

void es_format_number(ES_UINT64 number,wchar_buf_t *wcbuf)
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
				if (es_digit_grouping)
				{
					*--d = ',';
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
	
	wchar_buf_copy_utf8_string(wcbuf,d);
}

void es_space_to_width(wchar_t *buf,int wide)
{
	SIZE_T len;
	
	len = wchar_string_get_length_in_wchars(buf);
	
	if (len <= INT_MAX)
	{
		if ((int)len < wide)
		{
			int i;
			
			for(i=0;i<wide-(int)len;i++)
			{
				buf[i+(int)len] = ' ';
			}
			
			buf[i+len] = 0;
		}
	}
}

void es_get_argv(wchar_buf_t *wcbuf)
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

void es_expect_argv(wchar_buf_t *wcbuf)
{
	es_get_argv(wcbuf);
	
	if (!es_command_line)
	{
		es_fatal(ES_ERROR_EXPECTED_SWITCH_PARAMETER);
	}
}

// like es_get_argv, but we remove double quotes.
void es_get_command_argv(wchar_buf_t *wcbuf)
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

void es_expect_command_argv(wchar_buf_t *wcbuf)
{
	es_get_command_argv(wcbuf);
	
	if (!es_command_line)
	{
		es_fatal(ES_ERROR_EXPECTED_SWITCH_PARAMETER);
	}
}

int es_is_valid_key(INPUT_RECORD *ir)
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
            return 0;
    }
    
    return 1;
}

static BOOL _es_save_settings_with_filename(const wchar_t *filename)
{
	BOOL ret;
	HANDLE file_handle;
	
	ret = FALSE;
			
	file_handle = os_create_file(filename);
	if (file_handle != INVALID_HANDLE_VALUE)
	{
		os_write_file_utf8_string(file_handle,"[ES]\r\n");
		
//TODO: use canonical name
		config_write_int(file_handle,"sort_property_id",es_sort_property_id);
		config_write_int(file_handle,"sort_ascending",es_sort_ascending);
		config_write_string(file_handle,"instance",es_instance_name_wcbuf->buf);
		config_write_int(file_handle,"highlight_color",es_highlight_color);
		config_write_int(file_handle,"highlight",es_highlight);
		config_write_int(file_handle,"match_whole_word",es_match_whole_word);
		config_write_int(file_handle,"match_path",es_match_path);
		config_write_int(file_handle,"match_case",es_match_case);
		config_write_int(file_handle,"match_diacritics",es_match_diacritics);

//TODO: use canonical name
/*
		{
			wchar_t columnbuf[ES_WSTRING_SIZE];
			SIZE_T columni;
			
			*columnbuf = 0;
			
			for(columni=0;columni<es_numcolumns;columni++)
			{
				if (columni)
				{
					wchar_string_cat_wchar_string(columnbuf,L",");
				}
				
				wchar_string_cat_wchar_string_UINT64(columnbuf,es_columns[columni]);
			}

			config_write_string("columns",columnbuf);
		}
*/
		config_write_int(file_handle,"size_leading_zero",es_size_leading_zero);
		config_write_int(file_handle,"run_count_leading_zero",es_run_count_leading_zero);
		config_write_int(file_handle,"digit_grouping",es_digit_grouping);
		config_write_int(file_handle,"offset",es_offset);
		config_write_int(file_handle,"max_results",es_max_results);
		config_write_int(file_handle,"timeout",es_timeout);

//TODO: use canonical name
/*
		{
			wchar_t colorbuf[ES_WSTRING_SIZE];
			int columni;
			
			*colorbuf = 0;
			
			for(columni=0;columni<ES_COLUMN_TOTAL;columni++)
			{
				if (columni)
				{
					wchar_string_cat_wchar_string(colorbuf,L",");
				}
				
				if (es_column_color_is_valid[columni])
				{
					wchar_string_cat_wchar_string_UINT64(colorbuf,es_column_color[columni]);
				}
			}

			config_write_string("column_colors",colorbuf,filename);
		}
*/
		config_write_int(file_handle,"size_format",es_size_format);
		config_write_int(file_handle,"date_format",es_date_format);
		config_write_int(file_handle,"pause",es_pause);
		config_write_int(file_handle,"empty_search_help",es_empty_search_help);
		config_write_int(file_handle,"hide_empty_search_results",es_hide_empty_search_results);
		config_write_int(file_handle,"utf8_bom",es_utf8_bom);
		
//TODO: use canonical name
/*
		{
			wchar_t widthbuf[ES_WSTRING_SIZE];
			int columni;
			
			*widthbuf = 0;
			
			for(columni=0;columni<ES_COLUMN_TOTAL;columni++)
			{
				if (columni)
				{
					wchar_string_cat_wchar_string(widthbuf,L",");
				}
				
				wchar_string_cat_wchar_string_UINT64(widthbuf,es_column_widths[columni]);
			}

			config_write_string("column_widths",widthbuf,filename);
		}
		*/
		
		ret = TRUE;
		
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
		//TODO: use canonical sort name
		es_sort_property_id = config_read_int(&ini,"sort_property_id",es_sort_property_id);
		es_sort_ascending = config_read_int(&ini,"sort_ascending",es_sort_ascending);
		config_read_string(&ini,"instance",es_instance_name_wcbuf);
		es_highlight_color = config_read_int(&ini,"highlight_color",es_highlight_color);
		es_highlight = config_read_int(&ini,"highlight",es_highlight);
		es_match_whole_word = config_read_int(&ini,"match_whole_word",es_match_whole_word);
		es_match_path = config_read_int(&ini,"match_path",es_match_path);
		es_match_case = config_read_int(&ini,"match_case",es_match_case);
		es_match_diacritics = config_read_int(&ini,"match_diacritics",es_match_diacritics);

//TODO: use canonical name
/*
		{
			wchar_t columnbuf[ES_WSTRING_SIZE];
			wchar_t *p;
			
			*columnbuf = 0;
			
			config_read_string("columns",columnbuf,&ini);
			
			p = columnbuf;
			
			while(*p)
			{
				const wchar_t *start;
				int columntype;
				
				start = p;
				
				while(*p)
				{
					if (*p == ',')
					{
						*p++ = 0;
						
						break;
					}

					p++;
				}
				
				if (*start)
				{
					columntype = wchar_string_to_int(start);
					
					if ((columntype >= 0) && (columntype < ES_COLUMN_TOTAL))
					{
					#error property_id
//						column_add(columntype);
					}
				}
			}
		}*/

		es_size_leading_zero = config_read_int(&ini,"size_leading_zero",es_size_leading_zero);
		es_run_count_leading_zero = config_read_int(&ini,"run_count_leading_zero",es_run_count_leading_zero);
		es_digit_grouping = config_read_int(&ini,"digit_grouping",es_digit_grouping);
		es_offset = config_read_int(&ini,"offset",es_offset);
		es_max_results = config_read_int(&ini,"max_results",es_max_results);
		es_timeout = config_read_int(&ini,"timeout",es_timeout);

//TODO: use canonical name
/*
		{
			wchar_t colorbuf[ES_WSTRING_SIZE];
			wchar_t *p;
			int column_index;
			
			*colorbuf = 0;
			
			config_read_string("column_colors",colorbuf,&ini);
			
			p = colorbuf;
			column_index = 0;
			
			while(*p)
			{
				const wchar_t *start;
				int color;
				
				start = p;
				
				while(*p)
				{
					if (*p == ',')
					{
						*p++ = 0;
						
						break;
					}

					p++;
				}
				
				if (*start)
				{
					color = wchar_string_to_int(start);
					
					es_column_color[column_index] = color;
					es_column_color_is_valid[column_index] = 1;
				}
				
				column_index++;
			}
		}
*/

		es_size_format = config_read_int(&ini,"size_format",es_size_format);
		es_date_format = config_read_int(&ini,"date_format",es_date_format);
		es_pause = config_read_int(&ini,"pause",es_pause);
		es_empty_search_help = config_read_int(&ini,"empty_search_help",es_empty_search_help);
		es_hide_empty_search_results = config_read_int(&ini,"hide_empty_search_results",es_hide_empty_search_results);
		es_utf8_bom = config_read_int(&ini,"utf8_bom",es_utf8_bom);
		
//TODO: use canonical name
/*
		{
			wchar_t widthbuf[ES_WSTRING_SIZE];
			wchar_t *p;
			int column_index;
			
			*widthbuf = 0;
			
			config_read_string("column_widths",widthbuf,&ini);
			
			p = widthbuf;
			column_index = 0;
			
			while(*p)
			{
				const wchar_t *start;
				int width;
				
				start = p;
				
				while(*p)
				{
					if (*p == ',')
					{
						*p++ = 0;
						
						break;
					}

					p++;
				}
				
				if (*start)
				{
					width = wchar_string_to_int(start);
					
					// sanity.
					if (width < 0) 
					{
						width = 0;
					}
					
					if (width > 200) 
					{
						width = 200;
					}						
					
					es_column_widths[column_index] = width;
				}
				
				column_index++;
			}
		}	
		*/

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

void es_append_filter(wchar_buf_t *wcbuf,const wchar_t *filter)
{
	if (wcbuf->length_in_wchars)
	{
		wchar_buf_cat_wchar(wcbuf,' ');
	}
	
	wchar_buf_cat_wchar_string(wcbuf,filter);
}

void es_do_run_history_command(void)
{	
	es_everything_hwnd = es_find_ipc_window();
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
				es_write_dword(run_count);
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
		es_fatal(ES_ERROR_IPC);
	}
}

// checks for -sort-size, -sort-size-ascending and -sort-size-descending
// depreciated.
static BOOL es_check_sorts(const wchar_t *argv)
{
	BOOL ret;
	wchar_buf_t sort_name_wcbuf;
	int basic_property_name_i;
	
	ret = FALSE;
	wchar_buf_init(&sort_name_wcbuf);
	
	for(basic_property_name_i=0;basic_property_name_i<PROPERTY_BASIC_COUNT;basic_property_name_i++)
	{
		wchar_buf_printf(&sort_name_wcbuf,"sort-%s-ascending",property_basic_name_to_id_array[basic_property_name_i].name);
		
		if (es_check_option_wchar_string(argv,sort_name_wcbuf.buf))
		{
			es_sort_property_id = property_basic_name_to_id_array[basic_property_name_i].id;
			es_sort_ascending = 1;

			ret = TRUE;
			break;
		}
		
		wchar_buf_printf(&sort_name_wcbuf,"sort-%s-descending",property_basic_name_to_id_array[basic_property_name_i].name);
		
		if (es_check_option_wchar_string(argv,sort_name_wcbuf.buf))
		{
			es_sort_property_id = property_basic_name_to_id_array[basic_property_name_i].id;
			es_sort_ascending = -1;

			ret = TRUE;
			break;
		}
		
		wchar_buf_printf(&sort_name_wcbuf,"sort-%s",property_basic_name_to_id_array[basic_property_name_i].name);
		
		if (es_check_option_wchar_string(argv,sort_name_wcbuf.buf))
		{
			es_sort_property_id = property_basic_name_to_id_array[basic_property_name_i].id;

			ret = TRUE;
			break;
		}
	}
	
	wchar_buf_kill(&sort_name_wcbuf);

	return ret;
}

static int es_is_literal_switch(const wchar_t *s)
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
		return 0;
	}
	
	return 1;
}

static int _es_is_unbalanced_quotes(const wchar_t *s)
{
	const wchar_t *p;
	int in_quote;
	
	p = s;
	in_quote = 0;
	
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

// instance_name must be non-NULL.
static void es_get_pipe_name(wchar_buf_t *wcbuf)
{
	wchar_buf_copy_utf8_string(wcbuf,"\\\\.\\PIPE\\Everything IPC");

	if (es_instance_name_wcbuf->length_in_wchars)
	{
		wchar_buf_cat_utf8_string(wcbuf," (");
		wchar_buf_cat_wchar_string_n(wcbuf,es_instance_name_wcbuf->buf,es_instance_name_wcbuf->length_in_wchars);
		wchar_buf_cat_utf8_string(wcbuf,")");
	}
}

// fill in a buffer with a VLQ value and progress the buffer pointer.
static BYTE *es_copy_len_vlq(BYTE *buf,SIZE_T value)
{
	BYTE *d;
	
	d = buf;
	
	if (value < 0xff)
	{
		if (buf)
		{
			*d++ = (BYTE)value;
		}
		else
		{
			d++;
		}
		
		return d;
	}
	
	value -= 0xff;

	if (buf)
	{
		*d++ = 0xff;
	}
	else
	{
		d++;
	}
	
	if (value < 0xffff)
	{
		if (buf)
		{
			*d++ = (BYTE)(value >> 8);
			*d++ = (BYTE)(value);
		}
		else
		{
			d += 2;
		}
		
		return d;
	}
	
	value -= 0xffff;

	if (buf)
	{
		*d++ = 0xff;
		*d++ = 0xff;
	}
	else
	{
		d += 2;
	}
	
	if (value < 0xffffffff)
	{
		if (buf)
		{
			*d++ = (BYTE)(value >> 24);
			*d++ = (BYTE)(value >> 16);
			*d++ = (BYTE)(value >> 8);
			*d++ = (BYTE)(value);
		}
		else
		{
			d += 4;
		}
		
		return d;
	}
	
	value -= 0xffffffff;

	if (buf)
	{
		*d++ = 0xff;
		*d++ = 0xff;
		*d++ = 0xff;
		*d++ = 0xff;
	}
	else
	{
		d += 4;
	}
	
	// value cannot be larger than or equal to 0xffffffffffffffff
#if SIZE_MAX == 0xFFFFFFFFFFFFFFFFUI64

	if (buf)
	{
		*d++ = (BYTE)(value >> 56);
		*d++ = (BYTE)(value >> 48);
		*d++ = (BYTE)(value >> 40);
		*d++ = (BYTE)(value >> 32);
		*d++ = (BYTE)(value >> 24);
		*d++ = (BYTE)(value >> 16);
		*d++ = (BYTE)(value >> 8);
		*d++ = (BYTE)(value);
	}
	else
	{
		d += 8;
	}
	

#elif SIZE_MAX == 0xFFFFFFFF

	if (buf)
	{
		*d++ = 0x00;
		*d++ = 0x00;
		*d++ = 0x00;
		*d++ = 0x00;
		*d++ = 0xff;
		*d++ = 0xff;
		*d++ = 0xff;
		*d++ = 0xff;
	}
	else
	{
		d += 8;
	}
	
#else
	#error unknown SIZE_MAX
#endif

	return d;
}

static BYTE *es_copy_dword(BYTE *buf,DWORD value)
{
	return os_copy_memory(buf,&value,sizeof(DWORD));
}

static BYTE *es_copy_uint64(BYTE *buf,ES_UINT64 value)
{
	return os_copy_memory(buf,&value,sizeof(ES_UINT64));
}

static BYTE *es_copy_size_t(BYTE *buf,SIZE_T value)
{
#if SIZE_MAX == 0xFFFFFFFFFFFFFFFFUI64
	
	return es_copy_uint64(buf,(ES_UINT64)value);
	
#elif SIZE_MAX == 0xFFFFFFFF

	return es_copy_dword(buf,(DWORD)value);
	
#else
	#error unknown SIZE_MAX
#endif

}

static BOOL es_write_pipe_data(HANDLE h,const void *in_data,SIZE_T in_size)
{
	const BYTE *p;
	SIZE_T run;
	
	p = in_data;
	run = in_size;
	
	while(run)
	{
		DWORD chunk_size;
		DWORD num_written;
		
		if (run <= 65536)
		{
			chunk_size = (DWORD)run;
		}
		else
		{
			chunk_size = 65536;
		}
		
		if (WriteFile(h,p,chunk_size,&num_written,NULL))
		{
			if (num_written)
			{
				p += num_written;
				run -= num_written;
			}
			else
			{
				return FALSE;
			}
		}
		else
		{
			return FALSE;
		}
	}

	return TRUE;
}

// send some data to the IPC pipe.
// returns TRUE if successful.
// returns FALSE on error. Call GetLastError() for more information.
static BOOL es_write_pipe_message(HANDLE h,DWORD code,const void *in_data,SIZE_T in_size)
{
	// make sure the in_size is sane.
	if (in_size <= ES_DWORD_MAX)
	{
		_everything3_message_t send_message;
		
		send_message.code = code;
		send_message.size = (DWORD)in_size;
		
		if (es_write_pipe_data(h,&send_message,sizeof(_everything3_message_t)))
		{
			if (es_write_pipe_data(h,in_data,(DWORD)in_size))
			{
				return TRUE;
			}
		}
	}

	return FALSE;
}
// Read some data from the IPC Pipe.
static void _everything3_stream_read_data(_everything3_stream_t *stream,void *data,SIZE_T size)
{
	BYTE *d;
	SIZE_T run;
	
	d = (BYTE *)data;
	run = size;
	
	while(run)
	{
		SIZE_T chunk_size;
		
		if (!stream->avail)
		{
			_everything3_message_t recv_header;
			
			if (stream->got_last)
			{
				// read passed EOF
				os_zero_memory(d,run);
				
				stream->is_error = 1;
			
				return;
			}
			
			if (!es_read_pipe(stream->pipe_handle,&recv_header,sizeof(_everything3_message_t)))
			{
				// read header failed.
				os_zero_memory(d,run);
				
				stream->is_error = 1;
			
				return;
			}

			// last chunk?
			if (recv_header.code == _EVERYTHING3_RESPONSE_OK_MORE_DATA)
			{
			}
			else
			if (recv_header.code == _EVERYTHING3_RESPONSE_OK)
			{
				stream->got_last = 1;
			}
			else
			{
				os_zero_memory(d,run);
				
				stream->is_error = 1;
				
				return;
			}
			
			if (recv_header.size)			
			{
//TODO: dont reallocate.
				if (stream->buf)
				{
					mem_free(stream->buf);
				}
			
				stream->buf = mem_alloc(recv_header.size);
				if (!stream->buf)
				{
					os_zero_memory(d,run);
					
					stream->is_error = 1;
					
					break;
				}
						
				if (!es_read_pipe(stream->pipe_handle,stream->buf,recv_header.size))
				{
					os_zero_memory(d,run);
					
					stream->is_error = 1;

					break;
				}
				
				stream->p = stream->buf;
				stream->avail = recv_header.size;
			}
		}
		
		// stream->avail can be zero if we received a zero-sized data message.
		
		chunk_size = run;
		if (chunk_size > stream->avail)
		{
			chunk_size = stream->avail;
		}
		
		CopyMemory(d,stream->p,chunk_size);
		
		stream->p += chunk_size;
		stream->avail -= chunk_size;
		
		d += chunk_size;
		run -= chunk_size;
	}
}

static BYTE _everything3_stream_read_byte(_everything3_stream_t *stream)
{
	BYTE value;
	
	_everything3_stream_read_data(stream,&value,sizeof(BYTE));	
	
	return value;
}

static WORD _everything3_stream_read_word(_everything3_stream_t *stream)
{
	WORD value;
	
	_everything3_stream_read_data(stream,&value,sizeof(WORD));	
	
	return value;
}

static DWORD _everything3_stream_read_dword(_everything3_stream_t *stream)
{
	DWORD value;
	
	_everything3_stream_read_data(stream,&value,sizeof(DWORD));	
	
	return value;
}

static ES_UINT64 _everything3_stream_read_uint64(_everything3_stream_t *stream)
{
	ES_UINT64 value;
	
	_everything3_stream_read_data(stream,&value,sizeof(ES_UINT64));	
	
	return value;
}

// get a SIZE_T, where the size can differ to Everything.
// we set the error code if the value would overflow. (we are 32bit and Everything is 64bit and the value is > 0xffffffff)
static SIZE_T _everything3_stream_read_size_t(_everything3_stream_t *stream)
{
	SIZE_T ret;
	
	ret = SIZE_MAX;
	
	if (stream->is_64bit)
	{
		ES_UINT64 uint64_value;
		
		uint64_value = _everything3_stream_read_uint64(stream);	
					
#if SIZE_MAX == 0xFFFFFFFFFFFFFFFFUI64

		ret = uint64_value;

#elif SIZE_MAX == 0xFFFFFFFF

		if (uint64_value <= SIZE_MAX)
		{
			ret = (SIZE_T)uint64_value;
		}
		else
		{
			stream->is_error = 1;
		}
		
#else

		#error unknown SIZE_MAX
		
#endif		
	}
	else
	{
		ret = _everything3_stream_read_dword(stream);	
	}

	return ret;
}

// read a variable length quantity.
// Doesn't have to be too efficient as the data will follow immediately.
// Sets the error code if the length would overflow (32bit dll, 64bit Everything, len > 0xffffffff )
static SIZE_T _everything3_stream_read_len_vlq(_everything3_stream_t *stream)
{
	BYTE byte_value;
	WORD word_value;
	DWORD dword_value;
	SIZE_T start;
	
	start = 0;

	// BYTE
	byte_value = _everything3_stream_read_byte(stream);
	
	if (byte_value < 0xff)
	{
		return byte_value;
	}

	// WORD
	start = safe_size_add(start,0xff);
	
	word_value = _everything3_stream_read_word(stream);
	
	if (word_value < 0xffff)
	{
		return safe_size_add(start,word_value);
	}	
	
	// DWORD
	start = safe_size_add(start,0xffff);

	dword_value = _everything3_stream_read_dword(stream);
	
	if (dword_value < 0xffffffff)
	{
		return safe_size_add(start,dword_value);
	}

#if SIZE_MAX == 0xFFFFFFFFFFFFFFFFUI64

	{
		ES_UINT64 uint64_value;
		
		// UINT64
		start = safe_size_add(start,0xffffffff);

		uint64_value = _everything3_stream_read_uint64(stream);

		if (uint64_value < 0xFFFFFFFFFFFFFFFFUI64)
		{
			return safe_size_add(start,uint64_value);
		}

		stream->is_error = 1;
		return ES_UINT64_MAX;
	}
	
#elif SIZE_MAX == 0xffffffffui32
		
	stream->is_error = 1;
	return ES_DWORD_MAX;

#else

	#error unknown SIZE_MAX

#endif
}

// Receive some data from the IPC pipe.
// returns TRUE if successful.
// returns FALSE on error. Call GetLastError() for more information.
static BOOL es_read_pipe(HANDLE pipe_handle,void *buf,SIZE_T buf_size)
{
	BYTE *recv_p;
	SIZE_T recv_run;
	
	recv_p = (BYTE *)buf;
	recv_run = buf_size;

	while(recv_run)
	{
		DWORD chunk_size;
		DWORD numread;
		
		if (recv_run <= 65536)
		{
			chunk_size = (DWORD)recv_run;
		}
		else
		{
			chunk_size = 65536;
		}
		
		if (ReadFile(pipe_handle,recv_p,chunk_size,&numread,NULL))
		{
			if (numread)
			{
				recv_p += numread;
				recv_run -= numread;
				
				// continue..
			}
			else
			{
				return FALSE;
			}
		}
		else
		{
			return FALSE;
		}
	}
	
	return TRUE;
}

static BOOL es_skip_pipe(HANDLE pipe_handle,SIZE_T buf_size)
{
	BYTE buf[256];
	SIZE_T run;
	
	run = buf_size;
	
	while(run)
	{
		SIZE_T read_size;
		
		read_size = run;
		if (read_size > 256)
		{
			read_size = 256;
		}
		
		if (!es_read_pipe(pipe_handle,buf,read_size))
		{
			return 0;
		}
		
		run -= read_size;
	}
	
	return 1;
}

static BOOL es_column_is_right_aligned(DWORD property_id)
{
	if (property_format_to_right_align[property_get_format(property_id)])
	{
		return TRUE;
	}
	
	return FALSE;
}

static void es_write_utf8_string(const ES_UTF8 *text)
{
	wchar_buf_t wcbuf;

	wchar_buf_init(&wcbuf);

	wchar_buf_copy_utf8_string(&wcbuf,text);
	
	es_write_wchar_string(wcbuf.buf);

	wchar_buf_kill(&wcbuf);
}

static BOOL es_check_color_param(const wchar_t *argv,int *out_basic_property_name_i)
{
	BOOL ret;
	wchar_buf_t sort_name_wcbuf;
	int basic_property_name_i;

	ret = FALSE;
	wchar_buf_init(&sort_name_wcbuf);
	
	for(basic_property_name_i=0;basic_property_name_i<PROPERTY_BASIC_COUNT;basic_property_name_i++)
	{
		wchar_buf_copy_utf8_string(&sort_name_wcbuf,property_basic_name_to_id_array[basic_property_name_i].name);
		wchar_buf_cat_utf8_string(&sort_name_wcbuf,"-color");

		if (es_check_option_wchar_string(argv,sort_name_wcbuf.buf))
		{
			*out_basic_property_name_i = basic_property_name_i;
			
			ret = TRUE;
			break;
		}
	}
	
	wchar_buf_kill(&sort_name_wcbuf);
	
	return ret;
}

static BOOL es_check_column_param(const wchar_t *argv)
{
	BOOL ret;
	wchar_buf_t sort_name_wcbuf;
	int basic_property_name_i;
	
	ret = FALSE;
	wchar_buf_init(&sort_name_wcbuf);
	
	for(basic_property_name_i=0;basic_property_name_i<PROPERTY_BASIC_COUNT;basic_property_name_i++)
	{
		wchar_buf_copy_utf8_string(&sort_name_wcbuf,property_basic_name_to_id_array[basic_property_name_i].name);
		
		if (es_check_option_wchar_string(argv,sort_name_wcbuf.buf))
		{
			column_add(property_basic_name_to_id_array[basic_property_name_i].id);
			
			ret = TRUE;
			break;
		}

		wchar_buf_printf(&sort_name_wcbuf,"no-%s",property_basic_name_to_id_array[basic_property_name_i].name);
		
		if (es_check_option_wchar_string(argv,sort_name_wcbuf.buf))
		{
			column_remove(property_basic_name_to_id_array[basic_property_name_i].id);
			
			ret = TRUE;
			break;
		}
	}
	
	wchar_buf_kill(&sort_name_wcbuf);

	return ret;
}

//static int es_resolve_sort_ascending(DWORD property_id,int sort_ascending)

static int es_get_ipc_sort_type_from_property_id(DWORD property_id,int sort_ascending)
{
	if (!sort_ascending)
	{
	//TODO:
		// resolve sort_ascending..
	}
//TODO: use a lookup table
	switch(property_id)
	{
		case EVERYTHING3_PROPERTY_ID_NAME:
			return sort_ascending ? EVERYTHING_IPC_SORT_NAME_ASCENDING : EVERYTHING_IPC_SORT_NAME_DESCENDING;

		case EVERYTHING3_PROPERTY_ID_PATH:
			return sort_ascending ? EVERYTHING_IPC_SORT_PATH_ASCENDING : EVERYTHING_IPC_SORT_PATH_DESCENDING;

		case EVERYTHING3_PROPERTY_ID_SIZE:
			return sort_ascending ? EVERYTHING_IPC_SORT_SIZE_ASCENDING : EVERYTHING_IPC_SORT_SIZE_DESCENDING;

		case EVERYTHING3_PROPERTY_ID_EXTENSION:
			return sort_ascending ? EVERYTHING_IPC_SORT_EXTENSION_ASCENDING : EVERYTHING_IPC_SORT_EXTENSION_DESCENDING;

		case EVERYTHING3_PROPERTY_ID_TYPE:
			return sort_ascending ? EVERYTHING_IPC_SORT_TYPE_NAME_ASCENDING : EVERYTHING_IPC_SORT_TYPE_NAME_DESCENDING;

		case EVERYTHING3_PROPERTY_ID_DATE_CREATED:
			return sort_ascending ? EVERYTHING_IPC_SORT_DATE_CREATED_ASCENDING : EVERYTHING_IPC_SORT_DATE_CREATED_DESCENDING;

		case EVERYTHING3_PROPERTY_ID_DATE_MODIFIED:
			return sort_ascending ? EVERYTHING_IPC_SORT_DATE_MODIFIED_ASCENDING : EVERYTHING_IPC_SORT_DATE_MODIFIED_DESCENDING;

		case EVERYTHING3_PROPERTY_ID_ATTRIBUTES:
			return sort_ascending ? EVERYTHING_IPC_SORT_ATTRIBUTES_ASCENDING : EVERYTHING_IPC_SORT_ATTRIBUTES_DESCENDING;

		case EVERYTHING3_PROPERTY_ID_FILE_LIST_FILENAME:
			return sort_ascending ? EVERYTHING_IPC_SORT_FILE_LIST_FILENAME_ASCENDING : EVERYTHING_IPC_SORT_FILE_LIST_FILENAME_DESCENDING;

		case EVERYTHING3_PROPERTY_ID_RUN_COUNT:
			return sort_ascending ? EVERYTHING_IPC_SORT_RUN_COUNT_ASCENDING : EVERYTHING_IPC_SORT_RUN_COUNT_DESCENDING;

		case EVERYTHING3_PROPERTY_ID_DATE_RECENTLY_CHANGED:
			return sort_ascending ? EVERYTHING_IPC_SORT_DATE_RECENTLY_CHANGED_ASCENDING : EVERYTHING_IPC_SORT_DATE_RECENTLY_CHANGED_DESCENDING;

		case EVERYTHING3_PROPERTY_ID_DATE_ACCESSED:
			return sort_ascending ? EVERYTHING_IPC_SORT_DATE_ACCESSED_ASCENDING : EVERYTHING_IPC_SORT_DATE_ACCESSED_DESCENDING;

		case EVERYTHING3_PROPERTY_ID_DATE_RUN:
			return sort_ascending ? EVERYTHING_IPC_SORT_DATE_RUN_ASCENDING : EVERYTHING_IPC_SORT_DATE_RUN_DESCENDING;
	}
	
	return EVERYTHING_IPC_SORT_NAME_ASCENDING;
}

static void es_column_color_clear_all(void)
{
	array_empty(column_color_array);
	pool_empty(column_color_pool);
}

static void es_column_width_clear_all(void)
{
	array_empty(column_width_array);
	pool_empty(column_width_pool);
}

static HANDLE es_connect_ipc_pipe(void)
{
	wchar_buf_t pipe_name_wcbuf;
	HANDLE pipe_handle;
	
	wchar_buf_init(&pipe_name_wcbuf);

	es_get_pipe_name(&pipe_name_wcbuf);
		
	pipe_handle = CreateFile(pipe_name_wcbuf.buf,GENERIC_READ|GENERIC_WRITE,0,0,OPEN_EXISTING,FILE_FLAG_OVERLAPPED,0);
	
	wchar_buf_kill(&pipe_name_wcbuf);
	
	return pipe_handle;
}

static int es_pipe_ioctrl(HANDLE pipe_handle,int command,const void *in_buf,SIZE_T in_size,void *out_buf,SIZE_T out_size,SIZE_T *out_numread)
{
	if (es_write_pipe_message(pipe_handle,command,in_buf,in_size))
	{
		_everything3_message_t recv_header;
		BYTE *out_d;
		SIZE_T out_run;
		
		out_d = out_buf;
		out_run = out_size;
		
		for(;;)
		{
			int is_more;
			DWORD read_size;
			
			if (!es_read_pipe(pipe_handle,&recv_header,sizeof(_everything3_message_t)))
			{
				break;
			}
			
			is_more = 0;
			
			if (recv_header.code == _EVERYTHING3_RESPONSE_OK_MORE_DATA)
			{
				is_more = 1;
			}
			else
			if (recv_header.code == _EVERYTHING3_RESPONSE_OK)
			{
			}
			else
			{
				break;
			}
			
			read_size = recv_header.size;
			
			if (out_run <= ES_DWORD_MAX)
			{
				if (read_size > (DWORD)out_run)
				{
					read_size = (DWORD)out_run;
				}
			}
				
			if (read_size)
			{
				if (!es_read_pipe(pipe_handle,out_d,read_size))
				{
					break;
				}
			}
			
			// skip overflow.
			if (!es_skip_pipe(pipe_handle,recv_header.size - read_size))
			{
				break;
			}
			
			out_d += read_size;
			out_run -= read_size;
			
			if (!is_more)
			{
				*out_numread = out_d - (BYTE *)out_buf;
				
				return 1;
			}
		}
	}
	
	return 0;
}

static void es_get_folder_size(const wchar_t *filename)
{
	HANDLE pipe_handle;
	
	pipe_handle = es_connect_ipc_pipe();
	if (pipe_handle != INVALID_HANDLE_VALUE)
	{
		utf8_buf_t filename_cbuf;
		ES_UINT64 folder_size;
		SIZE_T numread;
		
		utf8_buf_init(&filename_cbuf);

		utf8_buf_copy_wchar_string(&filename_cbuf,filename);
		
		if (es_pipe_ioctrl(pipe_handle,_EVERYTHING3_COMMAND_GET_FOLDER_SIZE,filename_cbuf.buf,filename_cbuf.length_in_bytes,&folder_size,sizeof(ES_UINT64),&numread))
		{
			if (numread == sizeof(ES_UINT64))
			{
				if (folder_size != ES_UINT64_MAX)
				{
					es_write_UINT64(folder_size);
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
				es_fatal(ES_ERROR_SEND_MESSAGE);
			}
		}
		else
		{
			// bad ioctrl
			es_fatal(ES_ERROR_SEND_MESSAGE);
		}

		utf8_buf_kill(&filename_cbuf);

		CloseHandle(pipe_handle);
	}
	else
	{
		// no IPC
		es_fatal(ES_ERROR_IPC);
	}
}

// only create the reply window once.
static void es_get_reply_window(void)
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
			wcex.lpfnWndProc = es_window_proc;
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
				es_pChangeWindowMessageFilterEx = (BOOL (WINAPI *)(HWND hWnd,UINT message,DWORD action,_ES_CHANGEFILTERSTRUCT *pChangeFilterStruct))GetProcAddress(user32_hdll,"ChangeWindowMessageFilterEx");

				if (es_pChangeWindowMessageFilterEx)
				{
					es_pChangeWindowMessageFilterEx(es_reply_hwnd,WM_COPYDATA,_ES_MSGFLT_ALLOW,0);
				}
			}
		}
	}
}

